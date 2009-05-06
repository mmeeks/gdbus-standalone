/* GDBus - GLib D-Bus Library
 *
 * Copyright (C) 2008-2009 Red Hat, Inc.
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
 * You should have received a copy of the GNU Lesser General
 * Public License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place, Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * Author: David Zeuthen <davidz@redhat.com>
 */

#include "config.h"

#include <stdlib.h>
#include <glib/gi18n.h>
#include <gobject/gvaluecollector.h>

#include "gdbus-lowlevel.h"
#include "gdbusstructure.h"
#include "gdbusenumtypes.h"
#include "gdbusconnection.h"
#include "gdbuserror.h"
#include "gdbusprivate.h"

#include "gdbusalias.h"

/**
 * SECTION:gdbusstructure
 * @short_description: Holds a set of values
 * @include: gdbus/gdbus.h
 *
 * #GDBusStructure is a type that holds a D-Bus structure.
 */

struct _GDBusStructurePrivate
{
  gchar  *signature;
  guint   num_elements;
  GValue *elements;
  gchar **element_signatures;
};

static gboolean construct_cb (guint        arg_num,
                              const gchar *signature,
                              GType        type,
                              va_list      va_args,
                              gpointer     user_data);

G_DEFINE_TYPE (GDBusStructure, g_dbus_structure, G_TYPE_OBJECT);

static void
g_dbus_structure_finalize (GObject *object)
{
  GDBusStructure *structure = G_DBUS_STRUCTURE (object);
  guint n;

  g_free (structure->priv->signature);
  for (n = 0; n < structure->priv->num_elements; n++)
    {
      g_value_unset (&structure->priv->elements[n]);
      dbus_free (structure->priv->element_signatures[n]); /* allocated by libdbus */
    }
  g_free (structure->priv->element_signatures);

  if (G_OBJECT_CLASS (g_dbus_structure_parent_class)->finalize != NULL)
    G_OBJECT_CLASS (g_dbus_structure_parent_class)->finalize (object);
}

static void
g_dbus_structure_class_init (GDBusStructureClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->finalize     = g_dbus_structure_finalize;

  /* We avoid using GObject properties since
   *
   *  - performance is a concern
   *  - this is for the C object mapping only
   */

  g_type_class_add_private (klass, sizeof (GDBusStructurePrivate));
}

static void
g_dbus_structure_init (GDBusStructure *structure)
{
  structure->priv = G_TYPE_INSTANCE_GET_PRIVATE (structure, G_TYPE_DBUS_STRUCTURE, GDBusStructurePrivate);
}

/* ---------------------------------------------------------------------------------------------------- */

/**
 * g_dbus_structure_get_num_elements:
 * @structure: A #GDBusStructure.
 *
 * Gets number of elements in @structure.
 *
 * Returns: Number of elements in @structure.
 **/
guint
g_dbus_structure_get_num_elements (GDBusStructure *structure)
{
  g_return_val_if_fail (G_IS_DBUS_STRUCTURE (structure), 0);
  return structure->priv->num_elements;
}

/**
 * g_dbus_structure_get_signature:
 * @structure: A #GDBusStructure.
 *
 * Gets the signature for @structure.
 *
 * Returns: A D-Bus signature. Do not free this string, it
 * is owned by @structure.
 **/
const gchar *
g_dbus_structure_get_signature (GDBusStructure *structure)
{
  g_return_val_if_fail (G_IS_DBUS_STRUCTURE (structure), NULL);
  return structure->priv->signature;
}

/**
 * g_dbus_structure_get_signature_for_element:
 * @structure: A #GDBusStructure.
 * @element: Element number.
 *
 * Gets the signature for an @element in @structure.
 *
 * Returns: A D-Bus signature. Do not free this string, it
 * is owned by @structure.
 **/
const gchar *
g_dbus_structure_get_signature_for_element (GDBusStructure *structure,
                                            guint           element)
{
  g_return_val_if_fail (G_IS_DBUS_STRUCTURE (structure), NULL);
  g_return_val_if_fail (element < structure->priv->num_elements, NULL);
  return structure->priv->element_signatures[element];
}

const GValue *
_g_dbus_structure_get_gvalue_for_element (GDBusStructure *structure,
                                          guint           element)
{
  g_return_val_if_fail (G_IS_DBUS_STRUCTURE (structure), NULL);
  g_return_val_if_fail (element < structure->priv->num_elements, NULL);
  return &structure->priv->elements[element];
}

/**
 * g_dbus_structure_equal:
 * @structure1: The first #GDBusStructure.
 * @structure2: The second #GDBusStructure.
 *
 * Checks if @structure1 holds the same types and values as @structure2.
 *
 * Returns: %TRUE if @structure1 and @structure2 are equal, %FALSE otherwise.
 **/
gboolean
g_dbus_structure_equal (GDBusStructure  *structure1,
                        GDBusStructure  *structure2)
{
  gboolean ret;
  guint n;

  g_return_val_if_fail (G_IS_DBUS_STRUCTURE (structure1), FALSE);
  g_return_val_if_fail (G_IS_DBUS_STRUCTURE (structure2), FALSE);

  if (g_strcmp0 (structure1->priv->signature, structure2->priv->signature) != 0)
    return FALSE;

  ret = FALSE;
  for (n = 0; n < structure1->priv->num_elements; n++)
    {
      if (!_g_dbus_gvalue_equal (&structure1->priv->elements[n],
                                 &structure2->priv->elements[n],
                                 structure1->priv->element_signatures[n]))
        goto out;
    }

  ret = TRUE;

 out:
  return ret;
}

/* ---------------------------------------------------------------------------------------------------- */

/**
 * g_dbus_structure_new:
 * @signature: The signature of the structure.
 * @first_element_type: The #GType of the element.
 * @...: The value of the first element, followed by zero or more (type, value) pairs, followed by #G_TYPE_INVALID.
 *
 * Creates a new D-Bus structure with @signature.
 *
 * Note that @signature and and the supplied (type, value) pairs must match as described in
 * chapter TODO_SECTION_EXPLAINING_DBUS_TO_GTYPE_OBJECT_MAPPING.
 *
 * Returns: A new #GDBusStructure, free with g_object_unref().
 **/
GDBusStructure *
g_dbus_structure_new (const gchar *signature,
                      GType        first_element_type,
                      ...)
{
  GDBusStructure *s;
  va_list va_args;
  GArray *a;
  guint n;
  gchar *signature_with_parenthesis;

  s = NULL;
  a = g_array_new (FALSE,
                   TRUE,
                   sizeof (GValue));

  signature_with_parenthesis = g_strdup (signature + 1);
  signature_with_parenthesis[strlen (signature_with_parenthesis) - 1] = '\0';

  va_start (va_args, first_element_type);
  if (_gdbus_signature_vararg_foreach (signature_with_parenthesis,
                                       first_element_type,
                                       va_args,
                                       construct_cb,
                                       a))
    {
      for (n = 0; n < a->len; n++)
        g_value_unset (&(g_array_index (a, GValue, n)));
      g_array_free (a, TRUE);
      goto out;
    }

  s = _g_dbus_structure_new_for_values (signature,
                                        a->len,
                                        (GValue *) a->data);

  g_array_free (a, FALSE);

 out:
  va_end (va_args);
  g_free (signature_with_parenthesis);
  return s;
}

static gboolean
construct_cb (guint        arg_num,
              const gchar *signature,
              GType        type,
              va_list      va_args,
              gpointer     user_data)
{
  GArray *a = user_data;
  GValue *value;
  gboolean ret;
  gchar *error_str;

  ret = TRUE;

  g_array_set_size (a, a->len + 1);
  value = &g_array_index (a, GValue, a->len - 1);

  g_value_init (value, type);

  error_str = NULL;
  G_VALUE_COLLECT (value, va_args, 0, &error_str);
  if (error_str != NULL)
    {
      g_warning ("Error collecting value: %s", error_str);
      g_free (error_str);
      goto error;
    }

  /* TODO: check if value is compatible with signature */

  //g_debug ("arg_num %d: %s", arg_num, g_strdup_value_contents (value));

  ret = FALSE;

 error:
  return ret;
}

/* ---------------------------------------------------------------------------------------------------- */

GDBusStructure *
_g_dbus_structure_new_for_values (const gchar *signature,
                                  guint        num_elements,
                                  GValue      *elements)
{
  GDBusStructure *s;
  DBusSignatureIter iter;
  GPtrArray *p;
  gchar *signature_with_parenthesis;

  s = G_DBUS_STRUCTURE (g_object_new (G_TYPE_DBUS_STRUCTURE, NULL));

  signature_with_parenthesis = g_strdup (signature + 1);
  signature_with_parenthesis[strlen (signature_with_parenthesis) - 1] = '\0';

  s->priv->signature = g_strdup (signature);
  s->priv->num_elements = num_elements;
  s->priv->elements = elements; /* we steal the elements */

  /* TODO: when constructing structures we already parse the signature so maybe use
   *       that to avoid the overhead of parsing it again
   */
  p = g_ptr_array_new ();
  dbus_signature_iter_init (&iter, signature_with_parenthesis);
  do
    {
      g_ptr_array_add (p, dbus_signature_iter_get_signature (&iter));
    }
  while (dbus_signature_iter_next (&iter));

  g_assert (num_elements == p->len);

  s->priv->element_signatures = (gchar **) p->pdata;

  g_ptr_array_free (p, FALSE);

  g_free (signature_with_parenthesis);

  return s;
}

/* ---------------------------------------------------------------------------------------------------- */

static void
g_dbus_structure_set_element_valist (GDBusStructure *structure,
                                     guint           first_element_number,
                                     va_list         var_args)
{
  guint elem_number;

  g_return_if_fail (G_IS_DBUS_STRUCTURE (structure));

  elem_number = first_element_number;
  while (elem_number != (guint) -1)
    {
      gchar *error;

      if (elem_number >= structure->priv->num_elements)
        {
          g_warning ("%s: elem number %u is out of bounds", G_STRFUNC, elem_number);
          break;
        }

      G_VALUE_COLLECT (&(structure->priv->elements[elem_number]), var_args, 0, &error);
      if (error != NULL)
        {
          g_warning ("%s: %s", G_STRFUNC, error);
          g_free (error);
          break;
        }

      elem_number = va_arg (var_args, guint);
    }
}

/**
 * g_dbus_structure_set_element:
 * @structure: A #GDBusStructure.
 * @first_element_number: Element number to set.
 * @...: First element to set, followed optionally by
 * more element number / return location pairs, followed by -1.
 *
 * Sets element values in a #GDBusStructure. Similar to g_object_set().
 **/
void
g_dbus_structure_set_element (GDBusStructure *structure,
                              guint           first_element_number,
                              ...)
{
  va_list var_args;

  g_return_if_fail (G_IS_DBUS_STRUCTURE (structure));

  va_start (var_args, first_element_number);
  g_dbus_structure_set_element_valist (structure, first_element_number, var_args);
  va_end (var_args);
}

/* ---------------------------------------------------------------------------------------------------- */

static void
g_dbus_structure_get_element_valist (GDBusStructure *structure,
                                     guint           first_element_number,
                                     va_list         var_args)
{
  guint elem_number;

  g_return_if_fail (G_IS_DBUS_STRUCTURE (structure));

  elem_number = first_element_number;
  while (elem_number != (guint) -1)
    {
      gchar *error;

      if (elem_number >= structure->priv->num_elements)
        {
          g_warning ("%s: elem number %u is out of bounds", G_STRFUNC, elem_number);
          break;
        }

      G_VALUE_LCOPY (&(structure->priv->elements[elem_number]), var_args, G_VALUE_NOCOPY_CONTENTS, &error);
      if (error != NULL)
        {
          g_warning ("%s: %s", G_STRFUNC, error);
          g_free (error);
          break;
        }

      elem_number = va_arg (var_args, guint);
    }
}

/**
 * g_dbus_structure_get_element:
 * @structure: A #GDBusStructure.
 * @first_element_number: Element number to get.
 * @...: Return location for the first element, followed optionally by
 * more element number / return location pairs, followed by -1.
 *
 * Gets element values in a #GDBusStructure, similar to g_object_get(). The returned
 * values should not be freed; @structure owns the reference.
 **/
void
g_dbus_structure_get_element (GDBusStructure *structure,
                              guint           first_element_number,
                              ...)
{
  va_list var_args;

  g_return_if_fail (G_IS_DBUS_STRUCTURE (structure));

  va_start (var_args, first_element_number);
  g_dbus_structure_get_element_valist (structure, first_element_number, var_args);
  va_end (var_args);
}

/* ---------------------------------------------------------------------------------------------------- */

#define __G_DBUS_STRUCTURE_C__
#include "gdbusaliasdef.c"
