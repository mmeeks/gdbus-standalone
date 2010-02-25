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

#include <string.h>
#include <glib.h>

#include "gvariant-private.h"

/**
 * GVariantIter:
 *
 * An opaque structure type used to iterate over a container #GVariant
 * instance.
 *
 * The iter must be initialised with a call to g_variant_iter_init()
 * before using it.  After that, g_variant_iter_next() will return the
 * child values, in order.
 *
 * The iter may maintain a reference to the container #GVariant until
 * g_variant_iter_next() returns %NULL.  For this reason, it is
 * essential that you call g_variant_iter_next() until %NULL is
 * returned.  If you want to abort iterating part way through then use
 * g_variant_iter_cancel().
 */
typedef struct
{
  GVariant *value;
  GVariant *child;
  gsize length;
  gsize offset;
  gboolean cancelled;
} GVariantIterReal;

/**
 * g_variant_iter_init:
 * @iter: a #GVariantIter
 * @value: a container #GVariant instance
 * @returns: the number of items in the container
 *
 * Initialises the fields of a #GVariantIter and perpare to iterate
 * over the contents of @value.
 *
 * @iter is allowed to be completely uninitialised prior to this call;
 * it does not, for example, have to be cleared to zeros.  For this
 * reason, if @iter was already being used, you should first cancel it
 * with g_variant_iter_cancel().
 *
 * After this call, @iter holds a reference to @value.  The reference
 * will be automatically dropped once all values have been iterated
 * over or manually by calling g_variant_iter_cancel().
 *
 * This function returns the number of times that
 * g_variant_iter_next() will return non-%NULL.  You're not expected to
 * use this value, but it's there incase you wanted to know.
 **/
gsize
g_variant_iter_init (GVariantIter *iter,
                     GVariant     *value)
{
  GVariantIterReal *real = (GVariantIterReal *) iter;

  g_return_val_if_fail (iter != NULL, 0);
  g_return_val_if_fail (value != NULL, 0);

  g_assert (sizeof (GVariantIterReal) <= sizeof (GVariantIter));

  real->cancelled = FALSE;
  real->length = g_variant_n_children (value);
  real->offset = 0;
  real->child = NULL;

  if (real->length)
    real->value = g_variant_ref (value);
  else
    real->value = NULL;

  return real->length;
}

/**
 * g_variant_iter_next_value:
 * @iter: a #GVariantIter
 * @returns: a #GVariant for the next child
 *
 * Retreives the next child value from @iter.  In the event that no
 * more child values exist, %NULL is returned and @iter drops its
 * reference to the value that it was created with.
 *
 * The return value of this function is internally cached by the
 * @iter, so you don't have to unref it when you're done.  For this
 * reason, thought, it is important to ensure that you call
 * g_variant_iter_next() one last time, even if you know the number of
 * items in the container.
 *
 * It is permissable to call this function on a cancelled iter, in
 * which case %NULL is returned and nothing else happens.
 **/
GVariant *
g_variant_iter_next_value (GVariantIter *iter)
{
  GVariantIterReal *real = (GVariantIterReal *) iter;

  g_return_val_if_fail (iter != NULL, NULL);

  if (real->child)
    {
      g_variant_unref (real->child);
      real->child = NULL;
    }

  if (real->value == NULL)
    return NULL;

  real->child = g_variant_get_child_value (real->value, real->offset++);

  if (real->offset == real->length)
    {
      g_variant_unref (real->value);
      real->value = NULL;
    }

  return real->child;
}

/**
 * g_variant_iter_cancel:
 * @iter: a #GVariantIter
 *
 * Causes @iter to drop its reference to the container that it was
 * created with.  You need to call this on an iter if, for some
 * reason, you stopped iterating before reading the end.
 *
 * You do not need to call this in the normal case of visiting all of
 * the elements.  In this case, the reference will be automatically
 * dropped by g_variant_iter_next() just before it returns %NULL.
 *
 * It is permissable to call this function more than once on the same
 * iter.  It is permissable to call this function after the last
 * value.
 **/
void
g_variant_iter_cancel (GVariantIter *iter)
{
  GVariantIterReal *real = (GVariantIterReal *) iter;

  g_return_if_fail (iter != NULL);

  real->cancelled = TRUE;

  if (real->value)
    {
      g_variant_unref (real->value);
      real->value = NULL;
    }

  if (real->child)
    {
      g_variant_unref (real->child);
      real->child = NULL;
    }
}

/**
 * g_variant_iter_was_cancelled:
 * @iter: a #GVariantIter
 * @returns: %TRUE if g_variant_iter_cancel() was called
 *
 * Determines if g_variant_iter_cancel() was called on @iter.
 **/
gboolean
g_variant_iter_was_cancelled (GVariantIter *iter)
{
  GVariantIterReal *real = (GVariantIterReal *) iter;

  g_return_val_if_fail (iter != NULL, FALSE);

  return real->cancelled;
}

/* private */
gboolean
g_variant_iter_should_free (GVariantIter *iter)
{
  GVariantIterReal *real = (GVariantIterReal *) iter;

  return real->child != NULL;
}

/**
 * g_variant_has_type:
 * @value: a #GVariant instance
 * @pattern: a #GVariantType
 * @returns: %TRUE if the type of @value matches @type
 *
 * Checks if a value has a type matching the provided type.
 **/
gboolean
g_variant_has_type (GVariant           *value,
                    const GVariantType *type)
{
  g_return_val_if_fail (value != NULL, FALSE);
  g_return_val_if_fail (value != NULL, FALSE);

  return g_variant_type_is_subtype_of (g_variant_get_type (value), type);
}

/**
 * g_variant_new_boolean:
 * @boolean: a #gboolean value
 * @returns: a new boolean #GVariant instance
 *
 * Creates a new boolean #GVariant instance -- either %TRUE or %FALSE.
 **/
GVariant *
g_variant_new_boolean (gboolean boolean)
{
  guint8 byte = !!boolean;

  return g_variant_load (G_VARIANT_TYPE_BOOLEAN,
                         &byte, 1, G_VARIANT_TRUSTED);
}

/**
 * g_variant_new_byte:
 * @byte: a #guint8 value
 * @returns: a new byte #GVariant instance
 *
 * Creates a new byte #GVariant instance.
 **/
GVariant *
g_variant_new_byte (guint8 byte)
{
  return g_variant_load (G_VARIANT_TYPE_BYTE,
                         &byte, 1, G_VARIANT_TRUSTED);
}

/**
 * g_variant_new_int16:
 * @int16: a #gint16 value
 * @returns: a new int16 #GVariant instance
 *
 * Creates a new int16 #GVariant instance.
 **/
GVariant *
g_variant_new_int16 (gint16 int16)
{
  return g_variant_load (G_VARIANT_TYPE_INT16,
                         &int16, 2, G_VARIANT_TRUSTED);
}

/**
 * g_variant_new_uint16:
 * @uint16: a #guint16 value
 * @returns: a new uint16 #GVariant instance
 *
 * Creates a new uint16 #GVariant instance.
 **/
GVariant *
g_variant_new_uint16 (guint16 uint16)
{
  return g_variant_load (G_VARIANT_TYPE_UINT16,
                         &uint16, 2, G_VARIANT_TRUSTED);
}

/**
 * g_variant_new_int32:
 * @int32: a #gint32 value
 * @returns: a new int32 #GVariant instance
 *
 * Creates a new int32 #GVariant instance.
 **/
GVariant *
g_variant_new_int32 (gint32 int32)
{
  return g_variant_load (G_VARIANT_TYPE_INT32,
                         &int32, 4, G_VARIANT_TRUSTED);
}

/**
 * g_variant_new_handle:
 * @handle: a #gint32 value
 * @returns: a new handle #GVariant instance
 *
 * Creates a new int32 #GVariant instance.
 **/
GVariant *
g_variant_new_handle (gint32 handle)
{
  return g_variant_load (G_VARIANT_TYPE_HANDLE,
                         &handle, 4, G_VARIANT_TRUSTED);
}

/**
 * g_variant_new_uint32:
 * @uint32: a #guint32 value
 * @returns: a new uint32 #GVariant instance
 *
 * Creates a new uint32 #GVariant instance.
 **/
GVariant *
g_variant_new_uint32 (guint32 uint32)
{
  return g_variant_load (G_VARIANT_TYPE_UINT32,
                         &uint32, 4, G_VARIANT_TRUSTED);
}

/**
 * g_variant_new_int64:
 * @int64: a #gint64 value
 * @returns: a new int64 #GVariant instance
 *
 * Creates a new int64 #GVariant instance.
 **/
GVariant *
g_variant_new_int64 (gint64 int64)
{
  return g_variant_load (G_VARIANT_TYPE_INT64,
                         &int64, 8, G_VARIANT_TRUSTED);
}

/**
 * g_variant_new_uint64:
 * @uint64: a #guint64 value
 * @returns: a new uint64 #GVariant instance
 *
 * Creates a new uint64 #GVariant instance.
 **/
GVariant *
g_variant_new_uint64 (guint64 uint64)
{
  return g_variant_load (G_VARIANT_TYPE_UINT64,
                         &uint64, 8, G_VARIANT_TRUSTED);
}

/**
 * g_variant_new_double:
 * @floating: a #gdouble floating point value
 * @returns: a new double #GVariant instance
 *
 * Creates a new double #GVariant instance.
 **/
GVariant *
g_variant_new_double (gdouble floating)
{
  return g_variant_load (G_VARIANT_TYPE_DOUBLE,
                         &floating, 8, G_VARIANT_TRUSTED);
}

/**
 * g_variant_new_string:
 * @string: a normal C nul-terminated string
 * @returns: a new string #GVariant instance
 *
 * Creates a string #GVariant with the contents of @string.
 **/
GVariant *
g_variant_new_string (const gchar *string)
{
  return g_variant_load (G_VARIANT_TYPE_STRING,
                         string, strlen (string) + 1, G_VARIANT_TRUSTED);
}

/**
 * g_variant_new_object_path:
 * @string: a normal C nul-terminated string
 * @returns: a new object path #GVariant instance
 *
 * Creates a DBus object path #GVariant with the contents of @string.
 * @string must be a valid DBus object path.  Use
 * g_variant_is_object_path() if you're not sure.
 **/
GVariant *
g_variant_new_object_path (const gchar *string)
{
  g_return_val_if_fail (g_variant_is_object_path (string), NULL);

  return g_variant_load (G_VARIANT_TYPE_OBJECT_PATH,
                         string, strlen (string) + 1,
                         G_VARIANT_TRUSTED);
}

/**
 * g_variant_is_object_path:
 * @string: a normal C nul-terminated string
 * @returns: %TRUE if @string is a DBus object path
 *
 * Determines if a given string is a valid DBus object path.  You
 * should ensure that a string is a valid DBus object path before
 * passing it to g_variant_new_object_path().
 *
 * A valid object path starts with '/' followed by zero or more
 * sequences of characters separated by '/' characters.  Each sequence
 * must contain only the characters "[A-Z][a-z][0-9]_".  No sequence
 * (including the one following the final '/' character) may be empty.
 **/
gboolean
g_variant_is_object_path (const gchar *string)
{
  /* according to DBus specification */
  gsize i;

  g_return_val_if_fail (string != NULL, FALSE);

  /* The path must begin with an ASCII '/' (integer 47) character */
  if (string[0] != '/')
    return FALSE;

  for (i = 1; string[i]; i++)
    /* Each element must only contain the ASCII characters
     * "[A-Z][a-z][0-9]_"
     */
    if (g_ascii_isalnum (string[i]) || string[i] == '_')
      ;

    /* must consist of elements separated by slash characters. */
    else if (string[i] == '/')
      {
        /* No element may be the empty string. */
        /* Multiple '/' characters cannot occur in sequence. */
        if (string[i - 1] == '/')
          return FALSE;
      }

    else
      return FALSE;

  /* A trailing '/' character is not allowed unless the path is the
   * root path (a single '/' character).
   */
  if (i > 1 && string[i - 1] == '/')
    return FALSE;

  return TRUE;
}

/**
 * g_variant_new_signature:
 * @string: a normal C nul-terminated string
 * @returns: a new signature #GVariant instance
 *
 * Creates a DBus type signature #GVariant with the contents of
 * @string.  @string must be a valid DBus type signature.  Use
 * g_variant_is_signature() if you're not sure.
 **/
GVariant *
g_variant_new_signature (const gchar *string)
{
  g_return_val_if_fail (g_variant_is_signature (string), NULL);

  return g_variant_load (G_VARIANT_TYPE_SIGNATURE,
                         string, strlen (string) + 1,
                         G_VARIANT_TRUSTED);
}

/**
 * g_variant_is_signature:
 * @string: a normal C nul-terminated string
 * @returns: %TRUE if @string is a DBus type signature
 *
 * Determines if a given string is a valid DBus type signature.  You
 * should ensure that a string is a valid DBus object path before
 * passing it to g_variant_new_signature().
 *
 * DBus type signatures consist of zero or more definite #GVariantType
 * strings in sequence.
 **/
gboolean
g_variant_is_signature (const gchar *string)
{
  gsize first_invalid;

  g_return_val_if_fail (string != NULL, FALSE);

  /* make sure no non-definite characters appear */
  first_invalid = strspn (string, "ybnqihuxtdvmasog(){}");
  if (string[first_invalid])
    return FALSE;

  /* make sure each type string is well-formed */
  while (*string)
    if (!g_variant_type_string_scan (string, NULL, &string))
      return FALSE;

  return TRUE;
}

/**
 * g_variant_new_variant:
 * @value: a #GVariance instance
 * @returns: a new variant #GVariant instance
 *
 * Boxes @value.  The result is a #GVariant instance representing a
 * variant containing the original value.
 **/
GVariant *
g_variant_new_variant (GVariant *value)
{
  GVariant **children;

  g_return_val_if_fail (value != NULL, NULL);

  children = g_slice_new (GVariant *);
  children[0] = g_variant_ref_sink (value);

  return g_variant_new_tree (G_VARIANT_TYPE_VARIANT,
                             children, 1,
                             g_variant_is_trusted (value));
}

/**
 * g_variant_get_boolean:
 * @value: a boolean #GVariant instance
 * @returns: %TRUE or %FALSE
 *
 * Returns the boolean value of @value.
 *
 * It is an error to call this function with a @value of any type
 * other than %G_VARIANT_TYPE_BOOLEAN.
 **/
gboolean
g_variant_get_boolean (GVariant *value)
{
  guint8 byte;

  g_return_val_if_fail (value != NULL, FALSE);
  g_assert (g_variant_has_type (value, G_VARIANT_TYPE_BOOLEAN));
  g_variant_store (value, &byte);

  return byte;
}

/**
 * g_variant_get_byte:
 * @value: a byte #GVariant instance
 * @returns: a #guchar
 *
 * Returns the byte value of @value.
 *
 * It is an error to call this function with a @value of any type
 * other than %G_VARIANT_TYPE_BYTE.
 **/
guint8
g_variant_get_byte (GVariant *value)
{
  guint8 byte;

  g_return_val_if_fail (value != NULL, 0);
  g_assert (g_variant_has_type (value, G_VARIANT_TYPE_BYTE));
  g_variant_store (value, &byte);

  return byte;
}

/**
 * g_variant_get_int16:
 * @value: a int16 #GVariant instance
 * @returns: a #gint16
 *
 * Returns the 16-bit signed integer value of @value.
 *
 * It is an error to call this function with a @value of any type
 * other than %G_VARIANT_TYPE_INT16.
 **/
gint16
g_variant_get_int16 (GVariant *value)
{
  gint16 int16;

  g_return_val_if_fail (value != NULL, 0);
  g_assert (g_variant_has_type (value, G_VARIANT_TYPE_INT16));
  g_variant_store (value, &int16);

  return int16;
}

/**
 * g_variant_get_uint16:
 * @value: a uint16 #GVariant instance
 * @returns: a #guint16
 *
 * Returns the 16-bit unsigned integer value of @value.
 *
 * It is an error to call this function with a @value of any type
 * other than %G_VARIANT_TYPE_UINT16.
 **/
guint16
g_variant_get_uint16 (GVariant *value)
{
  guint16 uint16;

  g_return_val_if_fail (value != NULL, 0);
  g_assert (g_variant_has_type (value, G_VARIANT_TYPE_UINT16));
  g_variant_store (value, &uint16);

  return uint16;
}

/**
 * g_variant_get_int32:
 * @value: a int32 #GVariant instance
 * @returns: a #gint32
 *
 * Returns the 32-bit signed integer value of @value.
 *
 * It is an error to call this function with a @value of any type
 * other than %G_VARIANT_TYPE_INT32.
 **/
gint32
g_variant_get_int32 (GVariant *value)
{
  gint32 int32;

  g_return_val_if_fail (value != NULL, 0);
  g_assert (g_variant_has_type (value, G_VARIANT_TYPE_INT32));
  g_variant_store (value, &int32);

  return int32;
}

/**
 * g_variant_get_handle:
 * @value: a handle #GVariant instance
 * @returns: a #gint32
 *
 * Returns the 32-bit signed integer value of @value.
 *
 * It is an error to call this function with a @value of any type
 * other than %G_VARIANT_TYPE_HANDLE.
 **/
gint32
g_variant_get_handle (GVariant *value)
{
  gint32 int32;

  g_return_val_if_fail (value != NULL, 0);
  g_assert (g_variant_has_type (value, G_VARIANT_TYPE_HANDLE));
  g_variant_store (value, &int32);

  return int32;
}

/**
 * g_variant_get_uint32:
 * @value: a uint32 #GVariant instance
 * @returns: a #guint32
 *
 * Returns the 32-bit unsigned integer value of @value.
 *
 * It is an error to call this function with a @value of any type
 * other than %G_VARIANT_TYPE_UINT32.
 **/
guint32
g_variant_get_uint32 (GVariant *value)
{
  guint32 uint32;

  g_return_val_if_fail (value != NULL, 0);
  g_assert (g_variant_has_type (value, G_VARIANT_TYPE_UINT32));
  g_variant_store (value, &uint32);

  return uint32;
}

/**
 * g_variant_get_int64:
 * @value: a int64 #GVariant instance
 * @returns: a #gint64
 *
 * Returns the 64-bit signed integer value of @value.
 *
 * It is an error to call this function with a @value of any type
 * other than %G_VARIANT_TYPE_INT64.
 **/
gint64
g_variant_get_int64 (GVariant *value)
{
  gint64 int64;

  g_return_val_if_fail (value != NULL, 0);
  g_assert (g_variant_has_type (value, G_VARIANT_TYPE_INT64));
  g_variant_store (value, &int64);

  return int64;
}

/**
 * g_variant_get_uint64:
 * @value: a uint64 #GVariant instance
 * @returns: a #guint64
 *
 * Returns the 64-bit unsigned integer value of @value.
 *
 * It is an error to call this function with a @value of any type
 * other than %G_VARIANT_TYPE_UINT64.
 **/
guint64
g_variant_get_uint64 (GVariant *value)
{
  guint64 uint64;

  g_return_val_if_fail (value != NULL, 0);
  g_assert (g_variant_has_type (value, G_VARIANT_TYPE_UINT64));
  g_variant_store (value, &uint64);

  return uint64;
}

/**
 * g_variant_get_double:
 * @value: a double #GVariant instance
 * @returns: a #gdouble
 *
 * Returns the double precision floating point value of @value.
 *
 * It is an error to call this function with a @value of any type
 * other than %G_VARIANT_TYPE_DOUBLE.
 **/
gdouble
g_variant_get_double (GVariant *value)
{
  gdouble floating;

  g_return_val_if_fail (value != NULL, 0);
  g_assert (g_variant_has_type (value, G_VARIANT_TYPE_DOUBLE));
  g_variant_store (value, &floating);

  return floating;
}

/**
 * g_variant_get_string:
 * @value: a string #GVariant instance
 * @length: a pointer to a #gsize, to store the length
 * @returns: the constant string
 *
 * Returns the string value of a #GVariant instance with a string
 * type.  This includes the types %G_VARIANT_TYPE_STRING,
 * %G_VARIANT_TYPE_OBJECT_PATH and %G_VARIANT_TYPE_SIGNATURE.
 *
 * If @length is non-%NULL then the length of the string (in bytes) is
 * returned there.  For trusted values, this information is already
 * known.  For untrusted values, a strlen() will be performed.
 *
 * It is an error to call this function with a @value of any type
 * other than those three.
 *
 * The return value remains valid as long as @value exists.
 **/
const gchar *
g_variant_get_string (GVariant *value,
                      gsize    *length)
{
  GVariantClass class;
  gboolean valid_string;
  const gchar *string;
  gssize size;

  g_return_val_if_fail (value != NULL, NULL);
  class = g_variant_classify (value);
  g_return_val_if_fail (class == G_VARIANT_CLASS_STRING ||
                        class == G_VARIANT_CLASS_OBJECT_PATH ||
                        class == G_VARIANT_CLASS_SIGNATURE, NULL);

  string = g_variant_get_data (value);
  size = g_variant_get_size (value);

  if (g_variant_is_trusted (value))
    {
      if (length)
        *length = size - 1;

      return string;
    }

  valid_string = string != NULL && size > 0 && string[size - 1] == '\0';

  switch (class)
  {
    case G_VARIANT_CLASS_STRING:
      if (valid_string)
        break;

      if (length)
        *length = 0;

      return "";

    case G_VARIANT_CLASS_OBJECT_PATH:
      if (valid_string && g_variant_is_object_path (string))
        break;

      if (length)
        *length = 1;

      return "/";

    case G_VARIANT_CLASS_SIGNATURE:
      if (valid_string && g_variant_is_signature (string))
        break;

      if (length)
        *length = 0;

      return "";

    default:
      g_assert_not_reached ();
  }

  if (length)
    *length = strlen (string);

  return string;
}

/**
 * g_variant_dup_string:
 * @value: a string #GVariant instance
 * @length: a pointer to a #gsize, to store the length
 * @returns: a newly allocated string
 *
 * Similar to g_variant_get_string() except that instead of returning
 * a constant string, the string is duplicated.
 *
 * The return value must be freed using g_free().
 **/
gchar *
g_variant_dup_string (GVariant *value,
                      gsize    *length)
{
  GVariantClass class;
  int size;

  g_return_val_if_fail (value != NULL, NULL);
  class = g_variant_classify (value);
  g_return_val_if_fail (class == G_VARIANT_CLASS_STRING ||
                        class == G_VARIANT_CLASS_OBJECT_PATH ||
                        class == G_VARIANT_CLASS_SIGNATURE, NULL);

  size = g_variant_get_size (value);

  if (length)
    *length = size - 1;

  return g_memdup (g_variant_get_data (value), size);
}

/**
 * g_variant_get_variant:
 * @value: a variant #GVariance instance
 * @returns: the item contained in the variant
 *
 * Unboxes @value.  The result is the #GVariant instance that was
 * contained in @value.
 **/
GVariant *
g_variant_get_variant (GVariant *value)
{
  g_return_val_if_fail (value != NULL, NULL);
  g_assert (g_variant_has_type (value, G_VARIANT_TYPE_VARIANT));

  return g_variant_get_child_value (value, 0);
}

/**
 * GVariantBuilder:
 *
 * An opaque type used to build container #GVariant instances one
 * child value at a time.
 */
struct _GVariantBuilder
{
  GVariantBuilder *parent;

  GVariantClass container_class;
  GVariantType *type;
  const GVariantType *expected;
  /* expected2 is always definite.  it is set if adding a second
   * element to an array (or open sub-builder thereof).  it is
   * required to ensure the following works correctly:
   *
   *   b = new(array, "a**");
   *   sb = open(b, array, "ai");
   *   b = close (sb);
   *
   *   sb = open (b, array, "a*");   <-- valid
   *   add (sb, "u", 1234);          <-- must fail here
   *
   * the 'valid' call is valid since the "a*" is no more general than
   * the element type of "aa*" (in fact, it is exactly equal) but the
   * 'must fail' call still needs to know that it requires element
   * type 'i'.  this requires keeping track of the two types
   * separately.
   */
  const GVariantType *expected2;
  gsize min_items;
  gsize max_items;

  GVariant **children;
  gsize children_allocated;
  gsize offset;
  int has_child : 1;
  int trusted : 1;
};

/**
 * G_VARIANT_BUILDER_ERROR:
 *
 * Error domain for #GVariantBuilder. Errors in this domain will be
 * from the #GVariantBuilderError enumeration.  See #GError for
 * information on error domains.
 **/
/**
 * GVariantBuilderError:
 * @G_VARIANT_BUILDER_ERROR_TOO_MANY: too many items have been added
 * (returned by g_variant_builder_check_add())
 * @G_VARIANT_BUILDER_ERROR_TOO_FEW: too few items have been added
 * (returned by g_variant_builder_check_end())
 * @G_VARIANT_BUILDER_ERROR_INFER: unable to infer the type of an
 * array or maybe (returned by g_variant_builder_check_end())
 * @G_VARIANT_BUILDER_ERROR_TYPE: the value is of the incorrect type
 * (returned by g_variant_builder_check_add())
 *
 * Errors codes returned by g_variant_builder_check_add() and
 * g_variant_builder_check_end().
 */

static void
g_variant_builder_resize (GVariantBuilder *builder,
                          int              new_allocated)
{
  GVariant **new_children;
  int i;

  g_assert_cmpint (builder->offset, <=, new_allocated);

  new_children = g_slice_alloc (sizeof (GVariant *) * new_allocated);

  for (i = 0; i < builder->offset; i++)
    new_children[i] = builder->children[i];

  g_slice_free1 (sizeof (GVariant **) * builder->children_allocated,
                 builder->children);
  builder->children = new_children;
  builder->children_allocated = new_allocated;
}

/**
 * g_variant_builder_add_value:
 * @builder: a #GVariantBuilder
 * @value: a #GVariant
 *
 * Adds @value to @builder.
 *
 * It is an error to call this function if @builder has an outstanding
 * child.  It is an error to call this function in any case that
 * g_variant_builder_check_add() would return %FALSE.
 **/
void
g_variant_builder_add_value (GVariantBuilder *builder,
                             GVariant        *value)
{
  g_return_if_fail (builder != NULL && value != NULL);
  g_return_if_fail (g_variant_builder_check_add (builder,
                                                 g_variant_get_type (value),
                                                 NULL));

  builder->trusted &= g_variant_is_trusted (value);

  if (builder->container_class == G_VARIANT_CLASS_TUPLE ||
      builder->container_class == G_VARIANT_CLASS_DICT_ENTRY)
    {
      if (builder->expected)
        builder->expected = g_variant_type_next (builder->expected);

      if (builder->expected2)
        builder->expected2 = g_variant_type_next (builder->expected2);
    }

  if (builder->container_class == G_VARIANT_CLASS_ARRAY &&
      builder->expected2 == NULL)
    builder->expected2 = g_variant_get_type (value);

  if (builder->offset == builder->children_allocated)
    g_variant_builder_resize (builder, builder->children_allocated * 2);

  builder->children[builder->offset++] = g_variant_ref_sink (value);
}

/**
 * g_variant_builder_open:
 * @parent: a #GVariantBuilder
 * @type: a #GVariantType, or %NULL
 * @returns: a new (child) #GVariantBuilder
 *
 * Opens a subcontainer inside the given @parent.
 *
 * It is not permissible to use any other builder calls with @parent
 * until @g_variant_builder_close() is called on the return value of
 * this function.
 *
 * It is an error to call this function if @parent has an outstanding
 * child.  It is an error to call this function in any case that
 * g_variant_builder_check_add() would return %FALSE.  It is an error
 * to call this function in any case that it's an error to call
 * g_variant_builder_new().
 *
 * If @type is %NULL and @parent was given type information, that
 * information is passed down to the subcontainer and constrains what
 * values may be added to it.
 **/
GVariantBuilder *
g_variant_builder_open (GVariantBuilder    *parent,
                        const GVariantType *type)
{
  GVariantBuilder *child;

  g_return_val_if_fail (parent != NULL && type != NULL, NULL);
  g_return_val_if_fail (g_variant_builder_check_add (parent, type, NULL),
                        NULL);
  g_return_val_if_fail (!parent->has_child, NULL);

  child = g_variant_builder_new (type);

  if (parent->expected2)
    {
      if (g_variant_type_is_maybe (type) || g_variant_type_is_array (type))
        child->expected2 = g_variant_type_element (parent->expected2);

      if (g_variant_type_is_tuple (type) ||
          g_variant_type_is_dict_entry (type))
        child->expected2 = g_variant_type_first (parent->expected2);

      /* in variant case, we don't want to propagate the type */
    }

  parent->has_child = TRUE;
  child->parent = parent;

  return child;
}

/**
 * g_variant_builder_close:
 * @child: a #GVariantBuilder
 * @returns: the original parent of @child
 *
 * This function closes a builder that was created with a call to
 * g_variant_builder_open().
 *
 * It is an error to call this function on a builder that was created
 * using g_variant_builder_new().  It is an error to call this
 * function if @child has an outstanding child.  It is an error to
 * call this function in any case that g_variant_builder_check_end()
 * would return %FALSE.
 **/
GVariantBuilder *
g_variant_builder_close (GVariantBuilder *child)
{
  GVariantBuilder *parent;
  GVariant *value;

  g_return_val_if_fail (child != NULL, NULL);
  g_return_val_if_fail (child->has_child == FALSE, NULL);
  g_return_val_if_fail (child->parent != NULL, NULL);
  g_assert (child->parent->has_child);

  parent = child->parent;
  parent->has_child = FALSE;
  parent = child->parent;
  child->parent = NULL;

  value = g_variant_builder_end (child);
  g_variant_builder_add_value (parent, value);

  return parent;
}

/**
 * g_variant_builder_new:
 * @tclass: a container #GVariantClass
 * @type: a type contained in @tclass, or %NULL
 * @returns: a #GVariantBuilder
 *
 * Creates a new #GVariantBuilder.
 *
 * @tclass must be specified and must be a container type.
 *
 * If @type is given, it constrains the child values that it is
 * permissible to add.  If @tclass is not %G_VARIANT_CLASS_VARIANT
 * then @type must be contained in @tclass and will match the type of
 * the final value.  If @tclass is %G_VARIANT_CLASS_VARIANT then
 * @type must match the value that must be added to the variant.
 *
 * After the builder is created, values are added using
 * g_variant_builder_add_value().
 *
 * After all the child values are added, g_variant_builder_end() ends
 * the process.
 **/
GVariantBuilder *
g_variant_builder_new (const GVariantType *type)
{
  GVariantBuilder *builder;

  g_return_val_if_fail (type != NULL, NULL);
  g_return_val_if_fail (g_variant_type_is_container (type), NULL);

  builder = g_slice_new (GVariantBuilder);
  builder->parent = NULL;
  builder->offset = 0;
  builder->has_child = FALSE;
  builder->type = g_variant_type_copy (type);
  builder->expected = NULL;
  builder->trusted = TRUE;
  builder->expected2 = NULL;

  switch (*(const gchar *) type)
    {
    case G_VARIANT_CLASS_VARIANT:
      builder->container_class = G_VARIANT_CLASS_VARIANT;
      builder->children_allocated = 1;
      builder->expected = NULL;
      builder->min_items = 1;
      builder->max_items = 1;
      break;

    case G_VARIANT_CLASS_ARRAY:
      builder->container_class = G_VARIANT_CLASS_ARRAY;
      builder->children_allocated = 8;
      builder->expected = g_variant_type_element (builder->type);
      builder->min_items = 0;
      builder->max_items = -1;
      break;

    case G_VARIANT_CLASS_MAYBE:
      builder->container_class = G_VARIANT_CLASS_MAYBE;
      builder->children_allocated = 1;
      builder->expected = g_variant_type_element (builder->type);
      builder->min_items = 0;
      builder->max_items = 1;
      break;

    case G_VARIANT_CLASS_DICT_ENTRY:
      builder->container_class = G_VARIANT_CLASS_DICT_ENTRY;
      builder->children_allocated = 2;
      builder->expected = g_variant_type_key (builder->type);
      builder->min_items = 2;
      builder->max_items = 2;
      break;

    case 'r': /* G_VARIANT_TYPE_TUPLE was given */
      builder->container_class = G_VARIANT_CLASS_TUPLE;
      builder->children_allocated = 8;
      builder->expected = NULL;
      builder->min_items = 0;
      builder->max_items = -1;
      break;

    case G_VARIANT_CLASS_TUPLE: /* a definite tuple type was given */
      builder->container_class = G_VARIANT_CLASS_TUPLE;
      builder->children_allocated = g_variant_type_n_items (type);
      builder->expected = g_variant_type_first (builder->type);
      builder->min_items = builder->children_allocated;
      builder->max_items = builder->children_allocated;
      break;

    default:
      g_assert_not_reached ();
   }

  builder->children = g_slice_alloc (sizeof (GVariant *) *
                                     builder->children_allocated);

  return builder;
}

/**
 * g_variant_builder_end:
 * @builder: a #GVariantBuilder
 * @returns: a new, floating, #GVariant
 *
 * Ends the builder process and returns the constructed value.
 *
 * It is an error to call this function on a #GVariantBuilder created
 * by a call to g_variant_builder_open().  It is an error to call this
 * function if @builder has an outstanding child.  It is an error to
 * call this function in any case that g_variant_builder_check_end()
 * would return %FALSE.
 **/
GVariant *
g_variant_builder_end (GVariantBuilder *builder)
{
  GVariantType *my_type;
  GVariant *value;

  g_return_val_if_fail (builder != NULL, NULL);
  g_return_val_if_fail (builder->parent == NULL, NULL);
  g_return_val_if_fail (g_variant_builder_check_end (builder, NULL), NULL);

  g_variant_builder_resize (builder, builder->offset);

  if (g_variant_type_is_definite (builder->type))
    {
      my_type = g_variant_type_copy (builder->type);
    }
  else
    {
      switch (builder->container_class)
        {
        case G_VARIANT_CLASS_MAYBE:
          {
            const GVariantType *child_type;

            child_type = g_variant_get_type (builder->children[0]);
            my_type = g_variant_type_new_maybe (child_type);
          }
          break;

        case G_VARIANT_CLASS_ARRAY:
          {
            const GVariantType *child_type;

            child_type = g_variant_get_type (builder->children[0]);
            my_type = g_variant_type_new_array (child_type);
          }
          break;

        case G_VARIANT_CLASS_TUPLE:
          {
            const GVariantType **types;
            gint i;

            types = g_new (const GVariantType *, builder->offset);
            for (i = 0; i < builder->offset; i++)
              types[i] = g_variant_get_type (builder->children[i]);
            my_type = g_variant_type_new_tuple (types, i);
            g_free (types);
          }
          break;

        case G_VARIANT_CLASS_DICT_ENTRY:
          {
            const GVariantType *key_type, *value_type;

            key_type = g_variant_get_type (builder->children[0]);
            value_type = g_variant_get_type (builder->children[1]);
            my_type = g_variant_type_new_dict_entry (key_type, value_type);
          }
        break;

        case G_VARIANT_CLASS_VARIANT:
          /* 'v' is surely a definite type, so this should never be hit */
        default:
          g_assert_not_reached ();
        }
    }

  value = g_variant_new_tree (my_type, builder->children,
                              builder->offset, builder->trusted);

  g_variant_type_free (builder->type);
  g_slice_free (GVariantBuilder, builder);
  g_variant_type_free (my_type);

  return value;
}

/**
 * g_variant_builder_check_end:
 * @builder: a #GVariantBuilder
 * @error: a #GError
 * @returns: %TRUE if ending is safe
 *
 * Checks if a call to g_variant_builder_end() or
 * g_variant_builder_close() would succeed.
 *
 * It is an error to call this function if @builder has a child (ie:
 * g_variant_builder_open() has been used on @builder and
 * g_variant_builder_close() has not yet been called).
 *
 * This function checks that a sufficient number of items have been
 * added to the builder.  For dictionary entries, for example, it
 * ensures that 2 items were added.
 *
 * This function also checks that array and maybe builders that were
 * created without definite type information contain at least one item
 * (without which it would be impossible to infer the definite type).
 *
 * If some sort of error (either too few items were added or type
 * inference is not possible) prevents the builder from being ended
 * then %FALSE is returned and @error is set.
 **/
gboolean
g_variant_builder_check_end (GVariantBuilder  *builder,
                             GError          **error)
{
  g_return_val_if_fail (builder != NULL, FALSE);
  g_return_val_if_fail (builder->has_child == FALSE, FALSE);

  /* this function needs to check two things:
   *
   * 1) that we have the number of items required
   * 2) in the case of an array or maybe type, either:
   *      a) we have a definite type already
   *      b) we have an item from which to infer the type
   */

  if (builder->offset < builder->min_items)
    {
      gchar *type_str;

      type_str = g_variant_type_dup_string (builder->type);
      g_set_error (error, G_VARIANT_BUILDER_ERROR,
                   G_VARIANT_BUILDER_ERROR_TOO_FEW,
                   "this container (type '%s') must contain %"G_GSIZE_FORMAT
                   " values but only %"G_GSIZE_FORMAT "have been given",
                   type_str, builder->min_items, builder->offset);
      g_free (type_str);

      return FALSE;
    }

  if (!g_variant_type_is_definite (builder->type) &&
      (builder->container_class == G_VARIANT_CLASS_MAYBE ||
       builder->container_class == G_VARIANT_CLASS_ARRAY) &&
      builder->offset == 0)
    {
      g_set_error (error, G_VARIANT_BUILDER_ERROR,
                   G_VARIANT_BUILDER_ERROR_INFER,
                   "unable to infer type with no values present");
      return FALSE;
    }

  return TRUE;
}

/**
 * g_variant_builder_check_add:
 * @builder: a #GVariantBuilder
 * @tclass: a #GVariantClass
 * @type: a #GVariantType, or %NULL
 * @error: a #GError
 * @returns: %TRUE if adding is safe
 *
 * Does all sorts of checks to ensure that it is safe to call
 * g_variant_builder_add() or g_variant_builder_open().
 *
 * It is an error to call this function if @builder has a child (ie:
 * g_variant_builder_open() has been used on @builder and
 * g_variant_builder_close() has not yet been called).
 *
 * It is an error to call this function with an invalid @tclass
 * (including %G_VARIANT_CLASS_INVALID) or a class that's not the
 * smallest class for some definite type (for example,
 * %G_VARIANT_CLASS_ALL).
 *
 * If @type is non-%NULL this function first checks that it is a
 * member of @tclass (except, as with g_variant_builder_new(), if
 * @tclass is %G_VARIANT_CLASS_VARIANT then any @type is OK).
 *
 * The function then checks if any child of class @tclass (and type
 * @type, if given) would be suitable for adding to the builder.  If
 * @type is non-%NULL and is non-definite then all definite types
 * matching @type must be suitable for adding (ie: @type must be equal
 * to or less general than the type expected by the builder).
 *
 * In the case of an array that already has at least one item in it,
 * this function performs an additional check to ensure that @tclass
 * and @type match the items already in the array.  @type, if given,
 * need not be definite in order for this check to pass.
 *
 * Errors are flagged in the event that the builder contains too many
 * items or the addition would cause a type error.
 *
 * If @tclass is specified and is a container type and @type is not
 * given then there is no guarantee that adding a value of that class
 * would not fail.  Calling g_variant_builder_open() with that @tclass
 * (and @type as %NULL) would succeed, though.
 *
 * In the case that any error is detected @error is set and %FALSE is
 * returned.
 **/
gboolean
g_variant_builder_check_add (GVariantBuilder     *builder,
                             const GVariantType  *type,
                             GError             **error)
{
  g_return_val_if_fail (builder != NULL, FALSE);
  g_return_val_if_fail (type != NULL, FALSE);
  g_return_val_if_fail (builder->has_child == FALSE, FALSE);

  /* this function needs to check two things:
   *
   * 1) that we have not exceeded the number of allowable items
   * 2) that the incoming type matches the expected types like so:
   *
   *      expected2 <= type <= expected
   *
   * but since expected2 or expected could be %NULL, we need explicit checks:
   *
   *   type <= expected
   *   expected2 <= type
   *
   * (we already know expected2 <= expected)
   */

  if (builder->offset == builder->max_items)
    {
      gchar *type_str;

      type_str = g_variant_type_dup_string (builder->type);
      g_set_error (error, G_VARIANT_BUILDER_ERROR,
                   G_VARIANT_BUILDER_ERROR_TOO_MANY,
                   "this container (type '%s') may not contain more than"
                   " %"G_GSIZE_FORMAT " values", type_str, builder->offset);
      g_free (type_str);

      return FALSE;
    }

  /* type <= expected */
  if (builder->expected &&
      !g_variant_type_is_subtype_of (type, builder->expected))
    {
      gchar *expected_str, *type_str;

      expected_str = g_variant_type_dup_string (builder->expected);
      type_str = g_variant_type_dup_string (type);
      g_set_error (error, G_VARIANT_BUILDER_ERROR,
                   G_VARIANT_BUILDER_ERROR_TYPE,
                   "type '%s' does not match expected type '%s'",
                   type_str, expected_str);
      g_free (expected_str);
      g_free (type_str);
      return FALSE;
    }

  /* expected2 <= type */
  if (builder->expected2 &&
      !g_variant_type_is_subtype_of (builder->expected2, type))
    {
      g_set_error (error, G_VARIANT_BUILDER_ERROR,
                   G_VARIANT_BUILDER_ERROR_TYPE,
                   "all elements in an array must have the same type");
      return FALSE;
    }

  return TRUE;
}

/**
 * g_variant_builder_cancel:
 * @builder: a #GVariantBuilder
 *
 * Cancels the build process.  All memory associated with @builder is
 * freed.  If the builder was created with g_variant_builder_open()
 * then all ancestors are also freed.
 **/
void
g_variant_builder_cancel (GVariantBuilder *builder)
{
  GVariantBuilder *parent;

  g_return_if_fail (builder != NULL);

  do
    {
      gsize i;

      for (i = 0; i < builder->offset; i++)
        g_variant_unref (builder->children[i]);

      g_slice_free1 (sizeof (GVariant *) * builder->children_allocated,
                     builder->children);

      if (builder->type)
        g_variant_type_free (builder->type);

      parent = builder->parent;
      g_slice_free (GVariantBuilder, builder);
    }
  while ((builder = parent));
}

/**
 * g_variant_flatten:
 * @value: a #GVariant instance
 *
 * Flattens @value.
 *
 * This is a strange function with no direct effects but some
 * noteworthy side-effects.  Essentially, it ensures that @value is in
 * its most favourable form.  This involves ensuring that the value is
 * serialised and in machine byte order.  The investment of time now
 * can pay off by allowing shorter access times for future calls and
 * typically results in a reduction of memory consumption.
 *
 * A value received over the network or read from the disk in machine
 * byte order is already flattened.
 *
 * Some of the effects of this call are that any future accesses to
 * the data of @value (or children taken from it after flattening)
 * will occur in O(1) time.  Also, any data accessed from such a child
 * value will continue to be valid even after the child has been
 * destroyed, as long as @value still exists (since the contents of
 * the children are now serialised as part of the parent).
 **/
void
g_variant_flatten (GVariant *value)
{
  g_return_if_fail (value != NULL);
  g_variant_get_data (value);
}

/**
 * g_variant_get_type_string:
 * @value: a #GVariant
 * @returns: the type string for the type of @value
 *
 * Returns the type string of @value.  Unlike the result of calling
 * g_variant_type_peek_string(), this string is nul-terminated.  This
 * string belongs to #GVariant and must not be freed.
 **/
const gchar *
g_variant_get_type_string (GVariant *value)
{
  g_return_val_if_fail (value != NULL, NULL);
  return (const gchar *) g_variant_get_type (value);
}

/**
 * g_variant_is_basic:
 * @value: a #GVariant
 * @returns: %TRUE if @value has a basic type
 *
 * Determines if @value has a basic type.  Values with basic types may
 * be used as the keys of dictionary entries.
 *
 * This function is the exact opposite of g_variant_is_container().
 **/
gboolean
g_variant_is_basic (GVariant *value)
{
  g_return_val_if_fail (value != NULL, FALSE);
  return g_variant_type_is_basic (g_variant_get_type (value));
}

/**
 * g_variant_is_container:
 * @value: a #GVariant
 * @returns: %TRUE if @value has a basic type
 *
 * Determines if @value has a container type.  Values with container
 * types may be used with the functions g_variant_n_children() and
 * g_variant_get_child().
 *
 * This function is the exact opposite of g_variant_is_basic().
 **/
gboolean
g_variant_is_container (GVariant *value)
{
  g_return_val_if_fail (value != NULL, FALSE);
  return g_variant_type_is_container (g_variant_get_type (value));
}

#include <stdio.h>
void
g_variant_dump_data (GVariant *value)
{
  const guchar *data;
  const guchar *end;
  char row[3*16+2];
  gsize data_size;
  gsize i;

  g_return_if_fail (value != NULL);
  data_size = g_variant_get_size (value);

  g_debug ("GVariant at %p (type '%s', %" G_GSIZE_FORMAT " bytes):",
           value, g_variant_get_type_string (value), data_size);

  data = g_variant_get_data (value);
  end = data + data_size;

  i = 0;
  row[3*16+1] = '\0';
  while (data < end || (i & 15))
    {
      if ((i & 15) == (((gsize) data) & 15) && data < end)
        sprintf (&row[3 * (i & 15) + (i & 8)/8], "%02x  ", *data++);
      else
        sprintf (&row[3 * (i & 15) + (i & 8)/8], "    ");

      if ((++i & 15) == 0)
        {
          g_debug ("   %s", row);
          memset (row, 'q', 3 * 16 + 1);
        }
    }

  g_debug ("==");
}

GVariant *
g_variant_deep_copy (GVariant *value)
{
  switch (g_variant_classify (value))
  {
    case G_VARIANT_CLASS_BOOLEAN:
      return g_variant_new_boolean (g_variant_get_boolean (value));

    case G_VARIANT_CLASS_BYTE:
      return g_variant_new_byte (g_variant_get_byte (value));

    case G_VARIANT_CLASS_INT16:
      return g_variant_new_int16 (g_variant_get_int16 (value));

    case G_VARIANT_CLASS_UINT16:
      return g_variant_new_uint16 (g_variant_get_uint16 (value));

    case G_VARIANT_CLASS_INT32:
      return g_variant_new_int32 (g_variant_get_int32 (value));

    case G_VARIANT_CLASS_UINT32:
      return g_variant_new_uint32 (g_variant_get_uint32 (value));

    case G_VARIANT_CLASS_INT64:
      return g_variant_new_int64 (g_variant_get_int64 (value));

    case G_VARIANT_CLASS_UINT64:
      return g_variant_new_uint64 (g_variant_get_uint64 (value));

    case G_VARIANT_CLASS_DOUBLE:
      return g_variant_new_double (g_variant_get_double (value));

    case G_VARIANT_CLASS_STRING:
      return g_variant_new_string (g_variant_get_string (value, NULL));

    case G_VARIANT_CLASS_OBJECT_PATH:
      return g_variant_new_object_path (g_variant_get_string (value, NULL));

    case G_VARIANT_CLASS_SIGNATURE:
      return g_variant_new_signature (g_variant_get_string (value, NULL));

    case G_VARIANT_CLASS_VARIANT:
      {
        GVariant *inside, *new;

        inside = g_variant_get_variant (value);
        new = g_variant_new_variant (g_variant_deep_copy (inside));
        g_variant_unref (inside);

        return new;
      }

    case G_VARIANT_CLASS_HANDLE:
      return g_variant_new_handle (g_variant_get_handle (value));

    case G_VARIANT_CLASS_MAYBE:
    case G_VARIANT_CLASS_ARRAY:
    case G_VARIANT_CLASS_TUPLE:
    case G_VARIANT_CLASS_DICT_ENTRY:
      {
        GVariantBuilder *builder;
        GVariantIter iter;
        GVariant *child;

        builder = g_variant_builder_new (g_variant_get_type (value));
        g_variant_iter_init (&iter, value);

        while ((child = g_variant_iter_next_value (&iter)))
          g_variant_builder_add_value (builder, g_variant_deep_copy (child));

        return g_variant_builder_end (builder);
      }

    default:
      g_assert_not_reached ();
  }
}

/**
 * g_variant_new_strv:
 * @strv: an array of strings
 * @length: the length of @strv, or -1
 * @returns: a new floating #GVariant instance
 *
 * Constructs an array of strings #GVariant from the given array of
 * strings.
 *
 * If @length is not -1 then it gives the maximum length of @strv.  In
 * any case, a %NULL pointer in @strv is taken as a terminator.
 **/
GVariant *
g_variant_new_strv (const gchar * const *strv,
                    gint                 length)
{
  GVariantBuilder *builder;

  g_return_val_if_fail (strv != NULL || length == 0, NULL);

  builder = g_variant_builder_new (G_VARIANT_TYPE ("as"));
  while (length-- && *strv)
    g_variant_builder_add (builder, "s", *strv++);

  return g_variant_builder_end (builder);
}

/**
 * g_variant_get_strv:
 * @value: an array of strings #GVariant
 * @length: the length of the result, or %NULL
 * @returns: an array of constant strings
 *
 * Gets the contents of an array of strings #GVariant.  This call
 * makes a shallow copy; the return result should be released with
 * g_free(), but the individual strings must not be modified.
 *
 * If @length is non-%NULL then the number of elements in the result
 * is stored there.  In any case, the resulting array will be
 * %NULL-terminated.
 *
 * For an empty array, @length will be set to 0 and a pointer to a
 * %NULL pointer will be returned.
 **/
const gchar **
g_variant_get_strv (GVariant *value,
                    gint     *length)
{
  const gchar **result;
  gint my_length;
  gint i;

  g_return_val_if_fail (value != NULL, NULL);
  g_return_val_if_fail (g_variant_has_type (value, G_VARIANT_TYPE ("as")),
                        NULL);

  g_variant_flatten (value);

  my_length = g_variant_n_children (value);
  result = g_new (const gchar *, my_length + 1);

  if (length)
    *length = my_length;

  for (i = 0; i < my_length; i++)
    {
      GVariant *child = g_variant_get_child_value (value, i);
      result[i] = g_variant_get_string (child, NULL);
      g_variant_unref (child);
    }
  result[i] = NULL;

  return result;
}

/**
 * g_variant_dup_strv:
 * @value: an array of strings #GVariant
 * @length: the length of the result, or %NULL
 * @returns: an array of constant strings
 *
 * Gets the contents of an array of strings #GVariant.  This call
 * makes a deep copy; the return result should be released with
 * g_strfreev().
 *
 * If @length is non-%NULL then the number of elements in the result
 * is stored there.  In any case, the resulting array will be
 * %NULL-terminated.
 *
 * For an empty array, @length will be set to 0 and a pointer to a
 * %NULL pointer will be returned.
 **/
gchar **
g_variant_dup_strv (GVariant *value,
                    gint     *length)
{
  gchar **result;
  gint my_length;
  gint i;

  g_return_val_if_fail (value != NULL, NULL);
  g_return_val_if_fail (g_variant_has_type (value, G_VARIANT_TYPE ("as")),
                        NULL);

  g_variant_flatten (value);

  my_length = g_variant_n_children (value);
  result = g_new (gchar *, my_length + 1);

  if (length)
    *length = my_length;

  for (i = 0; i < my_length; i++)
    {
      GVariant *child = g_variant_get_child_value (value, i);
      result[i] = g_variant_dup_string (child, NULL);
      g_variant_unref (child);
    }
  result[i] = NULL;

  return result;
}

/**
 * g_variant_lookup_value:
 * @dictionary: a #GVariant dictionary, keyed by strings
 * @key: a string to lookup in the dictionary
 * @returns: the value corresponding to @key, or %NULL

 * Looks up @key in @dictionary.  This is essentially a convenience
 * function for dealing with the extremely common case of a dictionary
 * keyed by strings.
 *
 * In the case that the key is found, the corresponding value is
 * returned; not the dictionary entry.  If the key is not found then
 * this function returns %NULL.
 **/
GVariant *
g_variant_lookup_value (GVariant    *dictionary,
                        const gchar *key)
{
  GVariantIter iter;
  const gchar *_key;
  GVariant *value;
  GVariant *result = NULL;

  g_return_val_if_fail (dictionary != NULL, NULL);
  g_return_val_if_fail (key != NULL, NULL);

  g_variant_iter_init (&iter, dictionary);
  while (g_variant_iter_next (&iter, "{&s*}", &_key, &value))
    if (strcmp (_key, key) == 0)
      {
        result = g_variant_ref (value);
        g_variant_iter_cancel (&iter);
      }

  return result;
}

/**
 * g_variant_from_file:
 * @type: the #GVariantType of the new variant
 * @filename: the filename to load from
 * @flags: zero or more #GVariantFlags
 * @error: a pointer to a #GError, or %NULL
 * @returns: a new #GVariant instance, or %NULL
 *
 * Utility function to load a #GVariant from the contents of a file,
 * using a #GMappedFile.
 *
 * This function attempts to open @filename using #GMappedFile and then
 * calls g_variant_from_data() on the result.  As with that function,
 * @type may be %NULL.
 **/
GVariant *
g_variant_from_file (const GVariantType *type,
                     const gchar        *filename,
                     GVariantFlags       flags,
                     GError             **error)
{
  GMappedFile *mapped;
  gconstpointer data;
  gsize size;

  g_return_val_if_fail (filename != NULL, NULL);
  g_return_val_if_fail (error == NULL || *error == NULL, NULL);

  mapped = g_mapped_file_new (filename, FALSE, error);

  if (mapped == NULL)
    return NULL;

  data = g_mapped_file_get_contents (mapped);
  size = g_mapped_file_get_length (mapped);

  if (size == 0)
  /* #595535 */
    data = NULL;

  return g_variant_from_data (type, data, size, flags,
                              (GDestroyNotify) g_mapped_file_unref,
                              mapped);
}
