/*
 * Copyright © 2007, 2008 Ryan Lortie
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

/**
 * SECTION: gvariant
 * @title: GVariant
 * @short_description: a general purpose variant datatype
 * @see_also: GVariantType
 *
 * #GVariant is a variant datatype; it stores a value along with
 * information about the type of that value.  The range of possible
 * values is determined by the type.  The range of possible types is
 * exactly those types that may be sent over DBus.
 *
 * #GVariant instances always have a type and a value (which are given
 * at construction time).  The type and value of a #GVariant instance
 * can never change other than by the #GVariant itself being
 * destroyed.  A #GVariant can not contain a pointer.
 *
 * Facilities exist for serialising the value of a #GVariant into a
 * byte sequence.  A #GVariant can be sent over the bus or be saved to
 * disk.  Additionally, #GVariant is used as the basis of the
 * #GSettings persistent storage system.
 **/

/*
 * This file is organised into 6 sections
 *
 * SECTION 1: structure declaration, condition constants
 * SECTION 2: allocation/free functions
 * SECTION 3: condition enabling functions
 * SECTION 4: condition machinery
 * SECTION 5: other internal functions
 * SECTION 6: user-visible functions
 */

#include "gvariant-private.h"
#include "gvariant-serialiser.h"

#include <string.h>
#include <glib.h>
#include "gbitlock.h"

/* == SECTION 1: structure declaration, condition constants ============== */
/**
 * GVariant:
 *
 * #GVariant is an opaque data structure and can only be accessed
 * using the following functions.
 **/
struct _GVariant
{
  union
  {
    struct
    {
      GVariant *source;
      guint8 *data;
    } serialised;

    struct
    {
      GVariant **children;
      gsize n_children;
    } tree;

    struct
    {
      GDestroyNotify callback;
      gpointer user_data;
    } notify;
  } contents;

  gsize size;
  GVariantTypeInfo *type;
  gboolean floating;
  gint state;
  gint ref_count;
};

#define CONDITION_NONE                           0
#define CONDITION_SOURCE_NATIVE         0x00000001
#define CONDITION_BECAME_NATIVE         0x00000002
#define CONDITION_NATIVE                0x00000004

#define CONDITION_SOURCE_TRUSTED        0x00000010
#define CONDITION_BECAME_TRUSTED        0x00000020
#define CONDITION_TRUSTED               0x00000040

#define CONDITION_FIXED_SIZE            0x00000100
#define CONDITION_SIZE_KNOWN            0x00000200
#define CONDITION_SIZE_VALID            0x00000400

#define CONDITION_SERIALISED            0x00001000
#define CONDITION_INDEPENDENT           0x00002000
#define CONDITION_RECONSTRUCTED         0x00004000

#define CONDITION_NOTIFY                0x00010000
#define CONDITION_LOCKED                0x80000000

static const char * /* debugging only */
g_variant_state_to_string (guint state)
{
  GString *string;

  string = g_string_new (NULL);

#define add(cond,name) if (state & cond) g_string_append (string, name ", ");
  add (CONDITION_SOURCE_NATIVE,  "source native");
  add (CONDITION_BECAME_NATIVE,  "became native");
  add (CONDITION_NATIVE,         "native");
  add (CONDITION_SOURCE_TRUSTED, "source trusted");
  add (CONDITION_BECAME_TRUSTED, "became trusted");
  add (CONDITION_TRUSTED,        "trusted");
  add (CONDITION_FIXED_SIZE,     "fixed-size");
  add (CONDITION_SIZE_KNOWN,     "size known");
  add (CONDITION_SIZE_VALID,     "size valid");
  add (CONDITION_INDEPENDENT,    "independent");
  add (CONDITION_SERIALISED,     "serialised");
  add (CONDITION_RECONSTRUCTED,  "reconstructed");
  add (CONDITION_NOTIFY,         "notify");
#undef add

  g_string_truncate (string, string->len - 2);
  return g_string_free (string, FALSE);
}

/* == SECTION 2: allocation/free functions =============================== */
static GVariant *
g_variant_alloc (GVariantTypeInfo *type,
                 guint             initial_state)
{
  GVariant *new;

  new = g_slice_new (GVariant);
  new->ref_count = 1;
  new->type = type;
  new->floating = TRUE;
  new->state = initial_state & ~CONDITION_LOCKED;

  return new;
}

static void
g_variant_free (GVariant *value)
{
  /* free the type info */
  if (value->type)
    g_variant_type_info_unref (value->type);

  /* free the data */
  if (value->state & CONDITION_NOTIFY)
    value->contents.notify.callback (value->contents.notify.user_data);
  else if (value->state & CONDITION_SERIALISED)
    {
      if (value->state & CONDITION_INDEPENDENT)
        g_slice_free1 (value->size, value->contents.serialised.data);
      else
        g_variant_unref (value->contents.serialised.source);

      if (value->state & CONDITION_RECONSTRUCTED)
        g_variant_unref (value->contents.serialised.source);
    }
  else
    {
      GVariant **children;
      gsize n_children;
      gsize i;

      children = value->contents.tree.children;
      n_children = value->contents.tree.n_children;

      for (i = 0; i < n_children; i++)
        g_variant_unref (children[i]);

      g_slice_free1 (sizeof (GVariant *) * n_children, children);
    }

  /* free the structure itself */
  g_slice_free (GVariant, value);
}

static void
g_variant_lock (GVariant *value)
{
  g_bit_lock (&value->state, 31);
}

static void
g_variant_unlock (GVariant *value)
{
  g_bit_unlock (&value->state, 31);
}

/* == SECTION 3: condition enabling functions ============================ */
static void g_variant_fill_gvs (GVariantSerialised *, gpointer);

static gboolean
g_variant_enable_size_known (GVariant *value)
{
  GVariant **children;
  gsize n_children;

  children = value->contents.tree.children;
  n_children = value->contents.tree.n_children;
  value->size = g_variant_serialiser_needed_size (value->type,
                                                  &g_variant_fill_gvs,
                                                  (gpointer *) children,
                                                  n_children);

  return TRUE;
}

static gboolean
g_variant_enable_serialised (GVariant *value)
{
  GVariantSerialised gvs;
  GVariant **children;
  gsize n_children;
  gsize i;

  children = value->contents.tree.children;
  n_children = value->contents.tree.n_children;

  gvs.type_info = value->type;
  gvs.size = value->size;
  gvs.data = g_slice_alloc (gvs.size);

  g_variant_serialiser_serialise (gvs, &g_variant_fill_gvs,
                                  (gpointer *) children, n_children);

  value->contents.serialised.source = NULL;
  value->contents.serialised.data = gvs.data;

  for (i = 0; i < n_children; i++)
    g_variant_unref (children[i]);
  g_slice_free1 (sizeof (GVariant *) * n_children, children);

  return TRUE;
}

static gboolean
g_variant_enable_source_native (GVariant *value)
{
  return (value->contents.serialised.source->state & CONDITION_NATIVE) != 0;
}

static gboolean
g_variant_enable_became_native (GVariant *value)
{
  GVariantSerialised gvs;

  gvs.type_info = value->type;
  gvs.size = value->size;
  gvs.data = value->contents.serialised.data;

  g_variant_serialised_byteswap (gvs);

  return TRUE;
}

static gboolean
g_variant_enable_source_trusted (GVariant *value)
{
  return (value->contents.serialised.source->state & CONDITION_TRUSTED) != 0;
}

static gboolean
g_variant_enable_became_trusted (GVariant *value)
{
  GVariantSerialised gvs;

  gvs.type_info = value->type;
  gvs.data = value->contents.serialised.data;
  gvs.size = value->size;

  return g_variant_serialised_is_normal (gvs);
}

static gboolean
g_variant_enable_reconstructed (GVariant *value)
{
  GVariant *old, *new;

  old = g_variant_alloc (g_variant_type_info_ref (value->type),
                         value->state & ~CONDITION_LOCKED);
  value->contents.serialised.source = g_variant_ref_sink (old);
  old->contents.serialised.source = NULL;
  old->contents.serialised.data = value->contents.serialised.data;
  old->size = value->size;

  new = g_variant_deep_copy (old);
  g_variant_flatten (new);

  /* steal the data from new.  this is very evil. */
  new->state &= ~CONDITION_INDEPENDENT;
  new->contents.serialised.source = value;
  value->contents.serialised.data = new->contents.serialised.data;
  value->size = new->size;

  g_variant_unref (new);

  return TRUE;
}

static gboolean
g_variant_enable_independent (GVariant *value)
{
  GVariant *source;
  gpointer  new;

  source = value->contents.serialised.source;
  g_assert (source->state & CONDITION_INDEPENDENT);

  new = g_slice_alloc (value->size);
  memcpy (new, value->contents.serialised.data, value->size);

  /* barrier to ensure byteswap is not in progress */
  g_variant_lock (source);
  g_variant_unlock (source);

  /* rare: check if the source became native while we were copying */
  if (source->state & CONDITION_NATIVE)
    {
      /* our data is probably half-and-half */
      g_slice_free1 (value->size, new);

      return FALSE;
    }

  value->contents.serialised.source = NULL;
  value->contents.serialised.data = new;
  g_variant_unref (source);

  return TRUE;
}

static gboolean
g_variant_enable_fixed_size (GVariant *value)
{
  gsize fixed_size;

  g_variant_type_info_query (value->type, NULL, &fixed_size);

  return fixed_size != 0;
}

/* == SECTION 4: condition machinery ===================================== */
struct precondition_clause
{
  guint required;
  guint forbidden;
};

struct condition
{
  guint    condition;
  guint    implies;
  guint    forbids;
  guint    absence_implies;
  void     (*assert_invariant) (GVariant *);
  gboolean (*enable) (GVariant *);
  struct precondition_clause precondition[4];
};

struct condition condition_table[] =
{
  { CONDITION_SOURCE_NATIVE,
    CONDITION_NATIVE | CONDITION_SERIALISED,
    CONDITION_INDEPENDENT | CONDITION_BECAME_NATIVE |
    CONDITION_RECONSTRUCTED,
    CONDITION_NONE,
    NULL, g_variant_enable_source_native,
    { { CONDITION_NONE, CONDITION_NATIVE | CONDITION_INDEPENDENT    } } },

  { CONDITION_BECAME_NATIVE,
    CONDITION_NATIVE | CONDITION_INDEPENDENT | CONDITION_SERIALISED,
    CONDITION_SOURCE_NATIVE | CONDITION_RECONSTRUCTED,
    CONDITION_NONE,
    NULL, g_variant_enable_became_native,
    { { CONDITION_INDEPENDENT | CONDITION_SIZE_VALID,
        CONDITION_NATIVE                                            } } },

  { CONDITION_NATIVE,
    CONDITION_NONE,
    CONDITION_NONE,
    CONDITION_SERIALISED,
    NULL, NULL,
    { { CONDITION_SOURCE_NATIVE                                     },
      { CONDITION_BECAME_NATIVE                                     },
      { CONDITION_RECONSTRUCTED                                     } } },

  { CONDITION_SOURCE_TRUSTED,
    CONDITION_TRUSTED | CONDITION_SERIALISED,
    CONDITION_RECONSTRUCTED,
    CONDITION_NONE,
    NULL, g_variant_enable_source_trusted,
    { { CONDITION_NONE, CONDITION_TRUSTED | CONDITION_INDEPENDENT   } } },

  { CONDITION_BECAME_TRUSTED,
    CONDITION_TRUSTED | CONDITION_SERIALISED,
    CONDITION_SOURCE_TRUSTED | CONDITION_RECONSTRUCTED,
    CONDITION_NONE,
    NULL, g_variant_enable_became_trusted,
    { { CONDITION_SERIALISED, CONDITION_TRUSTED                     } } },

  { CONDITION_TRUSTED,
    CONDITION_NONE,
    CONDITION_NONE,
    CONDITION_NONE,
    NULL, NULL,
    { { CONDITION_SOURCE_TRUSTED                                    },
      { CONDITION_BECAME_TRUSTED                                    },
      { CONDITION_RECONSTRUCTED                                     } } },

  { CONDITION_SERIALISED,
    CONDITION_SIZE_KNOWN,
    CONDITION_NONE,
    CONDITION_NATIVE | CONDITION_INDEPENDENT,
    NULL, g_variant_enable_serialised,
    { { CONDITION_SIZE_KNOWN                                        } } },

  { CONDITION_SIZE_KNOWN,
    CONDITION_NONE,
    CONDITION_NOTIFY,
    CONDITION_NONE,
    NULL, g_variant_enable_size_known,
    { { CONDITION_NONE, CONDITION_SIZE_KNOWN                        } } },

  { CONDITION_SIZE_VALID,
    CONDITION_SIZE_KNOWN,
    CONDITION_NONE,
    CONDITION_NONE,
    NULL, NULL,
    { { CONDITION_SIZE_KNOWN | CONDITION_FIXED_SIZE                 },
      { CONDITION_SIZE_KNOWN | CONDITION_TRUSTED                    },
      { CONDITION_SIZE_KNOWN | CONDITION_NATIVE                     } } },

  { CONDITION_INDEPENDENT,
    CONDITION_NONE,
    CONDITION_NONE,
    CONDITION_SERIALISED,
    NULL, g_variant_enable_independent,
    { { CONDITION_SERIALISED, CONDITION_NATIVE                      } } },

  { CONDITION_RECONSTRUCTED,
    CONDITION_TRUSTED | CONDITION_NATIVE | CONDITION_INDEPENDENT,
    CONDITION_BECAME_NATIVE | CONDITION_BECAME_TRUSTED,
    CONDITION_NONE,
    NULL, g_variant_enable_reconstructed,
    { { CONDITION_SERIALISED,
        CONDITION_NATIVE | CONDITION_TRUSTED | CONDITION_FIXED_SIZE } } },

  { CONDITION_FIXED_SIZE,
    CONDITION_NONE,
    CONDITION_NONE,
    CONDITION_NONE,
    NULL, g_variant_enable_fixed_size,
    { { CONDITION_NONE, CONDITION_FIXED_SIZE                        } } },

  { CONDITION_NOTIFY,
    CONDITION_NONE,
    CONDITION_NOTIFY - 1,
    CONDITION_NONE,
    NULL, NULL,
    { } },

  { }
};

static gboolean
g_variant_state_is_valid (guint state)
{
  struct condition *c;

  for (c = condition_table; c->condition; c++)
    {
      if (state & c->condition)
        {
          if (~state & c->implies)
            return FALSE;

          if (state & c->forbids)
            return FALSE;
        }
      else
        {
          if (~state & c->absence_implies)
            return FALSE;
        }
    }

  return TRUE;
}

/*
 * g_variant_assert_invariant:
 * @value: a #GVariant instance to check
 *
 * This function asserts the class invariant on a #GVariant instance.
 * Any detected problems will result in an assertion failure.
 *
 * This function is potentially very slow.
 *
 * This function never fails.
 */
void
g_variant_assert_invariant (GVariant *value)
{
  if (value->state & CONDITION_NOTIFY)
    return;

  g_variant_lock (value);

  g_assert_cmpint (value->ref_count, >, 0);

  if G_UNLIKELY (!g_variant_state_is_valid (value->state & ~CONDITION_LOCKED))
    g_critical ("instance %p in invalid state: %s",
                value, g_variant_state_to_string (value->state));

  if (value->state & CONDITION_SERIALISED)
    {
      if (value->state & CONDITION_RECONSTRUCTED)
        g_variant_assert_invariant (value->contents.serialised.source);

      if (!(value->state & CONDITION_INDEPENDENT))
        g_variant_assert_invariant (value->contents.serialised.source);
    }
  else
    {
      gsize i;

      for (i = 0; i < value->contents.tree.n_children; i++)
        {
          g_assert_cmpint (value->state & CONDITION_TRUSTED, <=,
                           value->contents.tree.children[i]->state &
                           CONDITION_TRUSTED);

          g_assert (!value->contents.tree.children[i]->floating);
          g_variant_assert_invariant (value->contents.tree.children[i]);
        }
    }

  g_variant_unlock (value);
}

/* how many bits are in 'reqd' but not 'have'?
 */
static int
bits_missing (guint have,
              guint reqd)
{
  guint count = 0;

  reqd &= ~have;
  while (reqd && ++count)
    reqd &= reqd - 1;

  return count;
}

static gboolean
g_variant_try_unlocked (GVariant *value,
                        guint     conditions)
{
  struct precondition_clause *p;
  struct condition *c;
  int max_missing;

  /* attempt to enable each missing condition */
  for (c = condition_table; c->condition; c++)
    if ((value->state & c->condition) < (conditions & c->condition))
      {
        /* prefer preconditon clauses with the fewest false terms */
        for (max_missing = 0; max_missing < 10; max_missing++)
          for (p = c->precondition; p->required || p->forbidden; p++)
            if (!(value->state & p->forbidden) &&
                bits_missing (value->state, p->required) < max_missing &&
                max_missing && g_variant_try_unlocked (value, p->required))
              goto attempt_enable;

        return FALSE;

       attempt_enable:
        if (c->enable && !c->enable (value))
          return FALSE;

        value->state |= c->condition;
      }

  if (~value->state & conditions)
    g_error ("was %x %x", value->state, conditions);

  g_assert (!(~value->state & conditions));

  return TRUE;
}

static gboolean
g_variant_try_enabling_conditions (GVariant *value,
                                   guint     conditions)
{
  gboolean success;

  g_variant_assert_invariant (value);

  if ((value->state & conditions) == conditions)
    return TRUE;

  g_variant_lock (value);
  success = g_variant_try_unlocked (value, conditions);
  g_variant_unlock (value);
  g_variant_assert_invariant (value);

  return success;
}

static void
g_variant_require_conditions (GVariant *value,
                              guint     condition_set)
{
  if G_UNLIKELY (!g_variant_try_enabling_conditions (value, condition_set))
    g_error ("instance %p unable to enable '%s' from '%s'\n",
             value, g_variant_state_to_string (condition_set),
             g_variant_state_to_string (value->state));
}

static gboolean
g_variant_forbid_conditions (GVariant *value,
                             guint     condition_set)
{
  g_variant_assert_invariant (value);

  if (value->state & condition_set)
    return FALSE;

  g_variant_lock (value);

  if (value->state & condition_set)
    {
      g_variant_unlock (value);
      return FALSE;
    }

  return TRUE;
}

GVariant *
g_variant_load_fixed (const GVariantType *type,
                      gconstpointer       data,
                      gsize               n_items)
{
  GVariantTypeInfo *info;
  gsize fixed_size;
  GVariant *new;

  info = g_variant_type_info_get (type);
  if (g_variant_type_is_array (type))
    {
      g_variant_type_info_query_element (info, NULL, &fixed_size);
      fixed_size *= n_items;
    }
  else
    g_variant_type_info_query (info, NULL, &fixed_size);
  g_assert (fixed_size);

  /* TODO: can't be trusted yet since there may be non-zero
   *       padding between the elements.  can fix this with
   *       some sort of intelligent zero-inserting memdup.
   */
  new = g_variant_alloc (info,
                         CONDITION_INDEPENDENT | CONDITION_NATIVE |
                         CONDITION_SERIALISED | CONDITION_SIZE_KNOWN);
  new->contents.serialised.source = NULL;
  new->contents.serialised.data = g_slice_alloc (fixed_size);
  memcpy (new->contents.serialised.data, data, fixed_size);
  new->size = fixed_size;

  g_variant_assert_invariant (new);

  return new;
}

/* == SECTION 5: other internal functions ================================ */
static GVariantSerialised
g_variant_get_gvs (GVariant  *value,
                   GVariant **source)
{
  GVariantSerialised gvs = { value->type };

  g_assert (value->state & CONDITION_SERIALISED);
  g_variant_require_conditions (value, CONDITION_SIZE_VALID);

  if (g_variant_forbid_conditions (value, CONDITION_INDEPENDENT))
    {
      /* dependent */
      gvs.data = value->contents.serialised.data;

      if (source)
        *source = g_variant_ref (value->contents.serialised.source);

      g_variant_unlock (value);
    }
  else
    {
      /* independent */
      gvs.data = value->contents.serialised.data;

      if (source)
        *source = g_variant_ref (value);

      g_variant_unlock (value);
    }

  gvs.size = value->size;

  return gvs;
}

/*
 * g_variant_fill_gvs:
 * @serialised: the #GVariantSerialised to fill
 * @data: our #GVariant instance
 *
 * Utility function used as a callback from the serialiser to get
 * information about a given #GVariant instance (in @data).
 */
static void
g_variant_fill_gvs (GVariantSerialised *serialised,
                    gpointer            data)
{
  GVariant *value = data;

  g_variant_assert_invariant (value);

  g_variant_require_conditions (value, CONDITION_SIZE_VALID);

  if (serialised->type_info == NULL)
    serialised->type_info = value->type;

  if (serialised->size == 0)
    serialised->size = value->size;

  g_assert (serialised->type_info == value->type);
  g_assert (serialised->size == value->size);

  if (serialised->data && serialised->size)
    g_variant_store (value, serialised->data);
}

/*
 * g_variant_get_zeros:
 * @size: a size, in bytes
 * @returns: a new reference to a #GVariant
 *
 * Creates a #GVariant with no type that contains at least @size bytes
 * worth of zeros.  The instance will live forever.
 *
 * This is required for the case where deserialisation of a fixed-size
 * value from a non-fixed-sized container fails.  The fixed-sized
 * value needs to have zero-filled data (in case this data is
 * requested).  This data must also outlive the child since the parent
 * from which that child was taken may have been flattened (in which
 * case the user expects the child's data to live as long as the
 * parent).
 *
 * There are less-permanent ways of doing this (ie: somehow
 * associating the zeros with the parent) but this way is easy and it
 * works fine for now.
 */
static GVariant *
g_variant_get_zeros (gsize size)
{
  static GStaticMutex lock = G_STATIC_MUTEX_INIT;
  static GSList *zeros;
  GVariant *value;

  /* there's actually no sense in storing the entire linked list since
   * we will always use the first item in the list, but it prevents
   * valgrind from complaining.
   */

  if (size < 4096)
    size = 4096;

  g_static_mutex_lock (&lock);
  if (zeros)
    {
      value = zeros->data;

      if (value->size < size)
        value = NULL;
    }
  else
    value = NULL;

  if (value == NULL)
    {
      size--;
      size |= size >> 1; size |= size >> 2; size |= size >> 4;
      size |= size >> 8; size |= size >> 16;
      size |= size >> 16; size |= size >> 16;
      size++;

      value = g_variant_alloc (NULL,
                               CONDITION_SERIALISED | CONDITION_SIZE_KNOWN |
                               CONDITION_NATIVE | CONDITION_TRUSTED |
                               CONDITION_INDEPENDENT | CONDITION_SIZE_VALID);
      value->contents.serialised.source = NULL;
      value->contents.serialised.data = g_malloc0 (size);
      value->size = size;

      zeros = g_slist_prepend (zeros, g_variant_ref_sink (value));
    }
  g_static_mutex_unlock (&lock);

  return g_variant_ref (value);
}

/*
 * g_variant_apply_flags:
 * @value: a fresh #GVariant instance
 * @flags: various load flags
 *
 * This function is the common code used to apply flags (normalise,
 * byteswap, etc) to fresh #GVariant instances created using one of
 * the load functions.
 */
static GVariant *
g_variant_apply_flags (GVariant      *value,
                       GVariantFlags  flags)
{
  guint16 byte_order = flags;

  if (byte_order == 0)
    byte_order = G_BYTE_ORDER;

  g_assert (byte_order == G_LITTLE_ENDIAN ||
            byte_order == G_BIG_ENDIAN);

  if (byte_order == G_BYTE_ORDER)
    value->state |= CONDITION_NATIVE;

  else if (~flags & G_VARIANT_LAZY_BYTESWAP)
    g_variant_require_conditions (value, CONDITION_NATIVE);

  g_variant_assert_invariant (value);

  return value;
}

gboolean
g_variant_is_trusted (GVariant *value)
{
  return !!(value->state & CONDITION_TRUSTED);
}

gboolean
g_variant_is_normal_ (GVariant *value)
{
  return g_variant_serialised_is_normal (g_variant_get_gvs (value, NULL));
}

/* == SECTION 6: user-visibile functions ================================= */
/**
 * g_variant_get_type:
 * @value: a #GVariant
 * @returns: a #GVariantType
 *
 * Determines the type of @value.
 *
 * The return value is valid for the lifetime of @value and must not
 * be freed.
 */
const GVariantType *
g_variant_get_type (GVariant *value)
{
  g_return_val_if_fail (value != NULL, NULL);
  g_variant_assert_invariant (value);

  return G_VARIANT_TYPE (g_variant_type_info_get_type_string (value->type));
}

/**
 * g_variant_n_children:
 * @value: a container #GVariant
 * @returns: the number of children in the container
 *
 * Determines the number of children in a container #GVariant
 * instance.  This includes variants, maybes, arrays, tuples and
 * dictionary entries.  It is an error to call this function on any
 * other type of #GVariant.
 *
 * For variants, the return value is always 1.  For maybes, it is
 * always zero or one.  For arrays, it is the length of the array.
 * For tuples it is the number of tuple items (which depends
 * only on the type).  For dictionary entries, it is always 2.
 *
 * This function never fails.
 * TS
 **/
gsize
g_variant_n_children (GVariant *value)
{
  gsize n_children;

  g_return_val_if_fail (value != NULL, 0);
  g_variant_assert_invariant (value);

  if (g_variant_forbid_conditions (value, CONDITION_SERIALISED))
    {
      n_children = value->contents.tree.n_children;
      g_variant_unlock (value);
    }
  else
    {
      GVariantSerialised gvs;
      GVariant *source;

      gvs = g_variant_get_gvs (value, &source);
      n_children = g_variant_serialised_n_children (gvs);
      g_variant_unref (source);
    }

  return n_children;
}

/**
 * g_variant_get_child_value:
 * @value: a container #GVariant
 * @index: the index of the child to fetch
 * @returns: the child at the specified index
 *
 * Reads a child item out of a container #GVariant instance.  This
 * includes variants, maybes, arrays, tuple and dictionary
 * entries.  It is an error to call this function on any other type of
 * #GVariant.
 *
 * It is an error if @index is greater than the number of child items
 * in the container.  See g_variant_n_children().
 *
 * This function never fails.
 **/
GVariant *
g_variant_get_child_value (GVariant *value,
                           gsize     index)
{
  GVariant *child;

  g_return_val_if_fail (value != NULL, NULL);
  g_variant_assert_invariant (value);

  if (g_variant_forbid_conditions (value, CONDITION_SERIALISED))
    {
      if G_UNLIKELY (index >= value->contents.tree.n_children)
        g_error ("Attempt to access item %" G_GSIZE_FORMAT
                 " in a container with only %" G_GSIZE_FORMAT
                 " items", index, value->contents.tree.n_children);

      child = g_variant_ref (value->contents.tree.children[index]);
      g_variant_unlock (value);
    }
  else
    {
      GVariantSerialised gvs;
      GVariant *source;

      gvs = g_variant_get_gvs (value, &source);
      gvs = g_variant_serialised_get_child (gvs, index);

      child = g_variant_alloc (gvs.type_info,
                               CONDITION_SERIALISED | CONDITION_SIZE_KNOWN);
      child->type = gvs.type_info;
      child->contents.serialised.source = source;
      child->contents.serialised.data = gvs.data;
      child->size = gvs.size;
      child->floating = FALSE;

      if (gvs.data == NULL)
        {
          /* not actually using source data -- release it */
          g_variant_unref (child->contents.serialised.source);
          child->contents.serialised.source = NULL;

          if (gvs.size)
            {
              GVariant *zeros;
              gpointer data;

              g_assert (!((source->state | value->state)
                          & CONDITION_TRUSTED));

              zeros = g_variant_get_zeros (gvs.size);
              data = zeros->contents.serialised.data;

              child->contents.serialised.source = zeros;
              child->contents.serialised.data = data;

              child->state |= CONDITION_FIXED_SIZE | CONDITION_TRUSTED;
            }
          else
            child->state |= CONDITION_INDEPENDENT;

          child->state |= CONDITION_NATIVE | CONDITION_SIZE_VALID;
        }

      /* inherit 'native' and 'trusted' attributes */
      child->state |= (source->state | value->state) &
                      (CONDITION_NATIVE | CONDITION_TRUSTED);
    }

  g_variant_assert_invariant (child);

  return child;
}

/**
 * g_variant_get_size:
 * @value: a #GVariant instance
 * @returns: the serialised size of @value
 *
 * Determines the number of bytes that would be required to store
 * @value with g_variant_store().
 *
 * In the case that @value is already in serialised form or the size
 * has already been calculated (ie: this function has been called
 * before) then this function is O(1).  Otherwise, the size is
 * calculated, an operation which is approximately O(n) in the number
 * of values involved.
 *
 * This function never fails.
 **/
gsize
g_variant_get_size (GVariant *value)
{
  g_return_val_if_fail (value != NULL, 0);

  g_variant_require_conditions (value, CONDITION_SIZE_VALID);

  return value->size;
}

/**
 * g_variant_get_data:
 * @value: a #GVariant instance
 * @returns: the serialised form of @value
 *
 * Returns a pointer to the serialised form of a #GVariant instance.
 * The returned data is in machine native byte order but may not be in
 * fully-normalised form if read from an untrusted source.  The
 * returned data must not be freed; it remains valid for as long as
 * @value exists.
 *
 * In the case that @value is already in serialised form, this
 * function is O(1).  If the value is not already in serialised form,
 * serialisation occurs implicitly and is approximately O(n) in the
 * size of the result.
 *
 * This function never fails.
 **/
gconstpointer
g_variant_get_data (GVariant *value)
{
  g_return_val_if_fail (value != NULL, NULL);

  g_variant_require_conditions (value, CONDITION_NATIVE | CONDITION_SERIALISED);

  return value->contents.serialised.data;
}

/**
 * g_variant_store:
 * @value: the #GVariant to store
 * @data: the location to store the serialised data at
 *
 * Stores the serialised form of @variant at @data.  @data should be
 * serialised enough.  See g_variant_get_size().
 *
 * The stored data is in machine native byte order but may not be in
 * fully-normalised form if read from an untrusted source.  See
 * g_variant_normalise() for a solution.
 *
 * This function is approximately O(n) in the size of @data.
 *
 * This function never fails.
 **/
void
g_variant_store (GVariant *value,
                 gpointer  data)
{
  g_return_if_fail (value != NULL);

  g_variant_assert_invariant (value);

  g_variant_require_conditions (value, CONDITION_SIZE_VALID | CONDITION_NATIVE);

  if (g_variant_forbid_conditions (value, CONDITION_SERIALISED))
    {
      GVariantSerialised gvs;
      GVariant **children;
      gsize n_children;

      gvs.type_info = value->type;
      gvs.data = data;
      gvs.size = value->size;

      children = value->contents.tree.children;
      n_children = value->contents.tree.n_children;

      /* XXX we hold the lock for an awful long time here... */
      g_variant_serialiser_serialise (gvs,
                                      &g_variant_fill_gvs,
                                      (gpointer *) children,
                                      n_children);
      g_variant_unlock (value);
    }
  else
    {
      GVariantSerialised gvs;
      GVariant *source;

      gvs = g_variant_get_gvs (value, &source);
      memcpy (data, gvs.data, gvs.size);
      g_variant_unref (source);
  }
}

/**
 * g_variant_get_fixed:
 * @value: a #GVariant
 * @size: the size of @value
 * @returns: a pointer to the fixed-sized data
 *
 * Gets a pointer to the data of a fixed sized #GVariant instance.
 * This pointer can be treated as a pointer to the equivalent C
 * stucture type and accessed directly.  The data is in machine byte
 * order.
 *
 * @size must be equal to the fixed size of the type of @value.  It
 * isn't used for anything, but serves as a sanity check to ensure the
 * user of this function will be able to make sense of the data they
 * receive a pointer to.
 *
 * This function may return %NULL if @size is zero.
 **/
gconstpointer
g_variant_get_fixed (GVariant *value,
                     gsize     size)
{
  gsize fixed_size;

  g_return_val_if_fail (value != NULL, NULL);

  g_variant_assert_invariant (value);

  g_variant_type_info_query (value->type, NULL, &fixed_size);
  g_assert (fixed_size);

  g_assert_cmpint (size, ==, fixed_size);

  return g_variant_get_data (value);
}

/**
 * g_variant_get_fixed_array:
 * @value: an array #GVariant
 * @elem_size: the size of one array element
 * @length: a pointer to the length of the array, or %NULL
 * @returns: a pointer to the array data
 *
 * Gets a pointer to the data of an array of fixed sized #GVariant
 * instances.  This pointer can be treated as a pointer to an array of
 * the equivalent C type and accessed directly.  The data is
 * in machine byte order.
 *
 * @elem_size must be equal to the fixed size of the element type of
 * @value.  It isn't used for anything, but serves as a sanity check
 * to ensure the user of this function will be able to make sense of
 * the data they receive a pointer to.
 *
 * @length is set equal to the number of items in the array, so that
 * the size of the memory region returned is @elem_size times @length.
 *
 * This function may return %NULL if either @elem_size or @length is
 * zero.
 */
gconstpointer
g_variant_get_fixed_array (GVariant *value,
                           gsize     elem_size,
                           gsize    *length)
{
  gsize fixed_elem_size;

  g_return_val_if_fail (value != NULL, NULL);

  g_variant_assert_invariant (value);

  /* unsupported: maybes are treated as arrays of size zero or one */
  g_variant_type_info_query_element (value->type, NULL, &fixed_elem_size);
  g_assert (fixed_elem_size);

  g_assert_cmpint (elem_size, ==, fixed_elem_size);

  if (length != NULL)
    *length = g_variant_n_children (value);

  return g_variant_get_data (value);
}

/**
 * g_variant_unref:
 * @value: a #GVariant
 *
 * Decreases the reference count of @variant.  When its reference
 * count drops to 0, the memory used by the variant is freed.
 **/
void
g_variant_unref (GVariant *value)
{
  g_return_if_fail (value != NULL);

  g_variant_assert_invariant (value);

  if (g_atomic_int_dec_and_test (&value->ref_count))
    g_variant_free (value);
}

/**
 * g_variant_ref:
 * @value: a #GVariant
 * @returns: the same @variant
 *
 * Increases the reference count of @variant.
 **/
GVariant *
g_variant_ref (GVariant *value)
{
  g_return_val_if_fail (value != NULL, NULL);

  g_variant_assert_invariant (value);

  g_atomic_int_inc (&value->ref_count);

  return value;
}

/**
 * g_variant_ref_sink:
 * @value: a #GVariant
 * @returns: the same @variant
 *
 * If @value is floating, mark it as no longer floating.  If it is not
 * floating, increase its reference count.
 **/
GVariant *
g_variant_ref_sink (GVariant *value)
{
  g_return_val_if_fail (value != NULL, NULL);

  g_variant_assert_invariant (value);

  g_variant_ref (value);
  if (g_atomic_int_compare_and_exchange (&value->floating, 1, 0))
    g_variant_unref (value);

  return value;
}

/* private */
GVariant *
g_variant_new_tree (const GVariantType  *type,
                    GVariant           **children,
                    gsize                n_children,
                    gboolean             trusted)
{
  GVariant *new;

  new = g_variant_alloc (g_variant_type_info_get (type),
                         CONDITION_INDEPENDENT | CONDITION_NATIVE);
  new->contents.tree.children = children;
  new->contents.tree.n_children = n_children;
  new->size = 0;

  if (trusted)
    new->state |= CONDITION_TRUSTED;

  g_variant_assert_invariant (new);

  return new;
}

/**
 * g_variant_from_slice:
 * @type: the #GVariantType of the new variant
 * @slice: a pointer to a GSlice-allocated region
 * @size: the size of @slice
 * @flags: zero or more #GVariantFlags
 * @returns: a new #GVariant instance
 *
 * Creates a #GVariant instance from a memory slice.  Ownership of the
 * memory slice is assumed.  This function allows efficiently creating
 * #GVariant instances with data that is, for example, read over a
 * socket.
 *
 * If @type is %NULL then @data is assumed to have the type
 * %G_VARIANT_TYPE_VARIANT and the return value is the value extracted
 * from that variant.
 *
 * This function never fails.
 **/
GVariant *
g_variant_from_slice (const GVariantType *type,
                      gpointer            slice,
                      gsize               size,
                      GVariantFlags       flags)
{
  GVariant *new;

  g_return_val_if_fail (slice != NULL || size == 0, NULL);

  if (type == NULL)
    {
      GVariant *variant;

      variant = g_variant_from_slice (G_VARIANT_TYPE_VARIANT,
                                      slice, size, flags);
      new = g_variant_get_variant (variant);
      g_variant_unref (variant);

      return new;
    }
  else
    {
      new = g_variant_alloc (g_variant_type_info_get (type),
                             CONDITION_SERIALISED | CONDITION_INDEPENDENT |
                             CONDITION_SIZE_KNOWN);

      new->contents.serialised.source = NULL;
      new->contents.serialised.data = slice;
      new->floating = FALSE;
      new->size = size;

      return g_variant_apply_flags (new, flags);
    }
}

/**
 * g_variant_from_data:
 * @type: the #GVariantType of the new variant
 * @data: a pointer to the serialised data
 * @size: the size of @data
 * @flags: zero or more #GVariantFlags
 * @notify: a function to call when @data is no longer needed
 * @user_data: a #gpointer to pass to @notify
 * @returns: a new #GVariant instance
 *
 * Creates a #GVariant instance from serialised data.  The data is not
 * copied.  When the data is no longer required (which may be before
 * or after the return value is freed) @notify is called.  @notify may
 * even be called before this function returns.
 *
 * If @type is %NULL then @data is assumed to have the type
 * %G_VARIANT_TYPE_VARIANT and the return value is the value extracted
 * from that variant.
 *
 * This function never fails.
 **/
GVariant *
g_variant_from_data (const GVariantType *type,
                          gconstpointer       data,
                          gsize               size,
                          GVariantFlags       flags,
                          GDestroyNotify      notify,
                          gpointer            user_data)
{
  GVariant *new;

  g_return_val_if_fail (data != NULL || size == 0, NULL);

  if (type == NULL)
    {
      GVariant *variant;

      variant = g_variant_from_data (G_VARIANT_TYPE_VARIANT,
                                     data, size, flags, notify, user_data);
      new = g_variant_get_variant (variant);
      g_variant_unref (variant);

      return new;
    }
  else
    {
      GVariant *marker;

      marker = g_variant_alloc (NULL, CONDITION_NOTIFY);
      marker->contents.notify.callback = notify;
      marker->contents.notify.user_data = user_data;

      new = g_variant_alloc (g_variant_type_info_get (type),
                             CONDITION_SERIALISED | CONDITION_SIZE_KNOWN);
      new->contents.serialised.source = marker;
      new->contents.serialised.data = (gpointer) data;
      new->floating = FALSE;
      new->size = size;

      return g_variant_apply_flags (new, flags);
    }
}

/**
 * g_variant_load:
 * @type: the #GVariantType of the new variant, or %NULL
 * @data: the serialised #GVariant data to load
 * @size: the size of @data
 * @flags: zero or more #GVariantFlags
 * @returns: a new #GVariant instance
 *
 * Creates a new #GVariant instance.  @data is copied.  For a more
 * efficient way to create #GVariant instances, see
 * g_variant_from_slice() or g_variant_from_data().
 *
 * If @type is non-%NULL then it specifies the type of the
 * #GVariant contained in the serialise data.  If @type is
 * %NULL then the serialised data is assumed to have type
 * "v" and instead of returning the variant itself, the
 * contents of the variant is returned.  This provides a
 * simple way to store data along with its type.
 *
 * This function is O(n) in the size of @data.
 *
 * This function never fails.
 **/
GVariant *
g_variant_load (const GVariantType *type,
                gconstpointer       data,
                gsize               size,
                GVariantFlags       flags)
{
  GVariant *new;

  g_return_val_if_fail (data != NULL || size == 0, NULL);

  if (type == NULL)
    {
      GVariant *variant;

      variant = g_variant_load (G_VARIANT_TYPE_VARIANT, data, size, flags);
      new = g_variant_get_variant (variant);
      g_variant_unref (variant);

      return new;
    }
  else
    {
      gpointer slice;

      slice = g_slice_alloc (size);
      memcpy (slice, data, size);

      return g_variant_from_slice (type, slice, size, flags);
    }
}

/**
 * g_variant_classify:
 * @value: a #GVariant
 * @returns: the #GVariantClass of @value
 *
 * Returns the type class of @value.  This function is equivalent to
 * calling g_variant_get_type() followed by
 * g_variant_type_get_class().
 **/
GVariantClass
g_variant_classify (GVariant *value)
{
  g_return_val_if_fail (value != NULL, 0);
  return g_variant_type_info_get_type_char (value->type);
}

#include <glib-object.h>

GType
g_variant_get_gtype (void)
{
  static GType type_id = 0;

  if (!type_id)
    type_id = g_boxed_type_register_static (g_intern_static_string ("GVariant"),
                                            (GBoxedCopyFunc) g_variant_ref,
                                            (GBoxedFreeFunc) g_variant_unref);

  return type_id;
}
