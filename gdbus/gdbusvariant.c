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

#include "gdbusvariant.h"
#include "gdbusenumtypes.h"
#include "gdbusconnection.h"
#include "gdbuserror.h"
#include "gdbusstructure.h"
#include "gdbusprivate.h"

#include "gdbusalias.h"

/**
 * SECTION:gdbusvariant
 * @short_description: Holds a D-Bus signature and a value
 * @include: gdbus/gdbus.h
 *
 * #GDBusVariant is a polymorphic type that holds a D-Bus signature and a value.
 */

struct _GDBusVariantPrivate
{
  gchar *signature;
  GValue value;
};

G_DEFINE_TYPE (GDBusVariant, g_dbus_variant, G_TYPE_OBJECT);

static void
g_dbus_variant_finalize (GObject *object)
{
  GDBusVariant *variant = G_DBUS_VARIANT (object);

  if (variant->priv->signature != NULL)
    g_value_unset (&variant->priv->value);
  g_free (variant->priv->signature);

  if (G_OBJECT_CLASS (g_dbus_variant_parent_class)->finalize != NULL)
    G_OBJECT_CLASS (g_dbus_variant_parent_class)->finalize (object);
}

static void
g_dbus_variant_class_init (GDBusVariantClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->finalize     = g_dbus_variant_finalize;

  /* We avoid using GObject properties since
   *
   *  - performance is a concern
   *  - this is for the C object mapping only
   */

  g_type_class_add_private (klass, sizeof (GDBusVariantPrivate));
}

static void
g_dbus_variant_init (GDBusVariant *variant)
{
  variant->priv = G_TYPE_INSTANCE_GET_PRIVATE (variant, G_TYPE_DBUS_VARIANT, GDBusVariantPrivate);
}

const GValue *
_g_dbus_variant_get_gvalue (GDBusVariant *variant)
{
  return &variant->priv->value;
}

/**
 * g_dbus_variant_get_variant_signature:
 * @variant: A #GDBusVariant
 *
 * Gets the signature for @variant.
 *
 * Returns: The signature for @variant or %NULL if @variant is
 * unset. Do not free this string, it is owned by @variant.
 **/
const gchar *
g_dbus_variant_get_variant_signature (GDBusVariant *variant)
{
  g_return_val_if_fail (G_IS_DBUS_VARIANT (variant), NULL);
  return variant->priv->signature;
}

/* ---------------------------------------------------------------------------------------------------- */

/**
 * g_dbus_variant_equal:
 * @variant1: The first #GDBusVariant.
 * @variant2: The second #GDBusVariant.
 *
 * Checks if @variant1 holds the same type and value as @variant2.
 *
 * Returns: %TRUE if @variant1 and @variant2 are equal, %FALSE otherwise.
 **/
gboolean
g_dbus_variant_equal (GDBusVariant    *variant1,
                      GDBusVariant    *variant2)
{
  g_return_val_if_fail (G_IS_DBUS_VARIANT (variant1), FALSE);
  g_return_val_if_fail (G_IS_DBUS_VARIANT (variant2), FALSE);

  if (g_strcmp0 (variant1->priv->signature, variant2->priv->signature) != 0)
    return FALSE;

  if (variant1->priv->signature == NULL)
    return TRUE;

  return _g_dbus_gvalue_equal (&variant1->priv->value,
                               &variant2->priv->value,
                               variant1->priv->signature);
}

/* ---------------------------------------------------------------------------------------------------- */

static void
set_signature (GDBusVariant *variant,
               const gchar  *signature)
{
  if (signature == NULL)
    {
      if (variant->priv->signature != NULL)
        g_value_unset (&variant->priv->value);
      variant->priv->signature = NULL;
      g_free (variant->priv->signature);
    }
  else
    {
      g_free (variant->priv->signature);
      variant->priv->signature = g_strdup (signature);
    }
}

static void
set_signature_for_array (GDBusVariant *variant,
                         const gchar  *elem_signature)
{
  if (elem_signature == NULL)
    {
      if (variant->priv->signature != NULL)
        g_value_unset (&variant->priv->value);
      variant->priv->signature = NULL;
      g_free (variant->priv->signature);
    }
  else
    {
      g_free (variant->priv->signature);
      variant->priv->signature = g_strdup_printf ("a%s", elem_signature);
    }
}

static void
set_signature_for_hash_table (GDBusVariant *variant,
                              const gchar  *key_signature,
                              const gchar  *value_signature)
{
  if (key_signature == NULL || value_signature == NULL)
    {
      if (variant->priv->signature != NULL)
        g_value_unset (&variant->priv->value);
      variant->priv->signature = NULL;
      g_free (variant->priv->signature);
    }
  else
    {
      g_free (variant->priv->signature);
      variant->priv->signature = g_strdup_printf ("a{%s%s}", key_signature, value_signature);
    }
}


/* ---------------------------------------------------------------------------------------------------- */

/**
 * g_dbus_variant_new:
 *
 * Creates a new unset variant.
 *
 * Returns: A new #GDBusVariant. Free with g_object_unref().
 **/
GDBusVariant *
g_dbus_variant_new (void)
{
  GDBusVariant *variant;
  variant = G_DBUS_VARIANT (g_object_new (G_TYPE_DBUS_VARIANT, NULL));
  return variant;
}

GDBusVariant *
_g_dbus_variant_new_for_gvalue (const GValue *value, const gchar *signature)
{
  GDBusVariant *variant;
  variant = g_dbus_variant_new ();
  g_value_init (&variant->priv->value, G_VALUE_TYPE (value));
  g_value_copy (value, &variant->priv->value);
  set_signature (variant, signature);
  return variant;
}

/**
 * g_dbus_variant_new_for_string:
 * @value: A string.
 *
 * Creates a new variant that holds a copy of @value.
 *
 * Returns: A new #GDBusVariant. Free with g_object_unref().
 **/
GDBusVariant *
g_dbus_variant_new_for_string (const gchar *value)
{
  GDBusVariant *variant;
  variant = g_dbus_variant_new ();
  g_dbus_variant_set_string (variant, value);
  return variant;
}

/**
 * g_dbus_variant_new_for_object_path:
 * @value: An object path.
 *
 * Creates a new variant that holds a copy of @value.
 *
 * Returns: A new #GDBusVariant. Free with g_object_unref().
 **/
GDBusVariant *
g_dbus_variant_new_for_object_path (const gchar *value)
{
  GDBusVariant *variant;
  variant = g_dbus_variant_new ();
  g_dbus_variant_set_object_path (variant, value);
  return variant;
}

/**
 * g_dbus_variant_new_for_signature:
 * @value: A D-Bus signature.
 *
 * Creates a new variant that holds a copy of @value.
 *
 * Returns: A new #GDBusVariant. Free with g_object_unref().
 **/
GDBusVariant *
g_dbus_variant_new_for_signature (const gchar *value)
{
  GDBusVariant *variant;
  variant = g_dbus_variant_new ();
  g_dbus_variant_set_signature (variant, value);
  return variant;
}

/**
 * g_dbus_variant_new_for_string_array:
 * @value: A string array.
 *
 * Creates a new variant that holds a copy of @value.
 *
 * Returns: A new #GDBusVariant. Free with g_object_unref().
 **/
GDBusVariant *
g_dbus_variant_new_for_string_array (gchar **value)
{
  GDBusVariant *variant;
  variant = g_dbus_variant_new ();
  g_dbus_variant_set_string_array (variant, value);
  return variant;
}

/**
 * g_dbus_variant_new_for_object_path_array:
 * @value: An object path array.
 *
 * Creates a new variant that holds a copy of @value.
 *
 * Returns: A new #GDBusVariant. Free with g_object_unref().
 **/
GDBusVariant *
g_dbus_variant_new_for_object_path_array (gchar **value)
{
  GDBusVariant *variant;
  variant = g_dbus_variant_new ();
  g_dbus_variant_set_object_path_array (variant, value);
  return variant;
}

/**
 * g_dbus_variant_new_for_signature_array:
 * @value: A signature array.
 *
 * Creates a new variant that holds a copy of @value.
 *
 * Returns: A new #GDBusVariant. Free with g_object_unref().
 **/
GDBusVariant *
g_dbus_variant_new_for_signature_array (gchar **value)
{
  GDBusVariant *variant;
  variant = g_dbus_variant_new ();
  g_dbus_variant_set_signature_array (variant, value);
  return variant;
}

/**
 * g_dbus_variant_new_for_byte:
 * @value: A #guchar.
 *
 * Creates a new variant that holds a #guchar equal to @value.
 *
 * Returns: A new #GDBusVariant. Free with g_object_unref().
 **/
GDBusVariant *
g_dbus_variant_new_for_byte (guchar value)
{
  GDBusVariant *variant;
  variant = g_dbus_variant_new ();
  g_dbus_variant_set_byte (variant, value);
  return variant;
}

/**
 * g_dbus_variant_new_for_int16:
 * @value: A #gint16.
 *
 * Creates a new variant that holds a #gint16 equal to @value.
 *
 * Returns: A new #GDBusVariant. Free with g_object_unref().
 **/
GDBusVariant *
g_dbus_variant_new_for_int16 (gint16 value)
{
  GDBusVariant *variant;
  variant = g_dbus_variant_new ();
  g_dbus_variant_set_int16 (variant, value);
  return variant;
}

/**
 * g_dbus_variant_new_for_uint16:
 * @value: A #guint16.
 *
 * Creates a new variant that holds a #guint16 equal to @value.
 *
 * Returns: A new #GDBusVariant. Free with g_object_unref().
 **/
GDBusVariant *
g_dbus_variant_new_for_uint16 (guint16 value)
{
  GDBusVariant *variant;
  variant = g_dbus_variant_new ();
  g_dbus_variant_set_uint16 (variant, value);
  return variant;
}

/**
 * g_dbus_variant_new_for_int:
 * @value: A #gint.
 *
 * Creates a new variant that holds a #gint equal to @value.
 *
 * Returns: A new #GDBusVariant. Free with g_object_unref().
 **/
GDBusVariant *
g_dbus_variant_new_for_int (gint value)
{
  GDBusVariant *variant;
  variant = g_dbus_variant_new ();
  g_dbus_variant_set_int (variant, value);
  return variant;
}

/**
 * g_dbus_variant_new_for_uint:
 * @value: A #guint.
 *
 * Creates a new variant that holds a #guint equal to @value.
 *
 * Returns: A new #GDBusVariant. Free with g_object_unref().
 **/
GDBusVariant *
g_dbus_variant_new_for_uint (guint value)
{
  GDBusVariant *variant;
  variant = g_dbus_variant_new ();
  g_dbus_variant_set_uint (variant, value);
  return variant;
}

/**
 * g_dbus_variant_new_for_int64:
 * @value: A #gint64.
 *
 * Creates a new variant that holds a #gint64 equal to @value.
 *
 * Returns: A new #GDBusVariant. Free with g_object_unref().
 **/
GDBusVariant *
g_dbus_variant_new_for_int64 (gint64 value)
{
  GDBusVariant *variant;
  variant = g_dbus_variant_new ();
  g_dbus_variant_set_int64 (variant, value);
  return variant;
}

/**
 * g_dbus_variant_new_for_uint64:
 * @value: A #guint64.
 *
 * Creates a new variant that holds a #guint64 equal to @value.
 *
 * Returns: A new #GDBusVariant. Free with g_object_unref().
 **/
GDBusVariant *
g_dbus_variant_new_for_uint64 (guint64 value)
{
  GDBusVariant *variant;
  variant = g_dbus_variant_new ();
  g_dbus_variant_set_uint64 (variant, value);
  return variant;
}

/**
 * g_dbus_variant_new_for_boolean:
 * @value: A #gboolean.
 *
 * Creates a new variant that holds a #gboolean equal to @value.
 *
 * Returns: A new #GDBusVariant. Free with g_object_unref().
 **/
GDBusVariant *
g_dbus_variant_new_for_boolean (gboolean value)
{
  GDBusVariant *variant;
  variant = g_dbus_variant_new ();
  g_dbus_variant_set_boolean (variant, value);
  return variant;
}

/**
 * g_dbus_variant_new_for_double:
 * @value: A #gdouble.
 *
 * Creates a new variant that holds a #gdouble equal to @value.
 *
 * Returns: A new #GDBusVariant. Free with g_object_unref().
 **/
GDBusVariant *
g_dbus_variant_new_for_double (gdouble value)
{
  GDBusVariant *variant;
  variant = g_dbus_variant_new ();
  g_dbus_variant_set_double (variant, value);
  return variant;
}

/**
 * g_dbus_variant_new_for_array:
 * @array: A #GArray.
 * @element_signature: D-Bus signature of the elements stored in @array.
 *
 * Creates a new variant that holds a reference to @array.
 *
 * Returns: A new #GDBusVariant. Free with g_object_unref().
 **/
GDBusVariant *
g_dbus_variant_new_for_array (GArray      *array,
                              const gchar *element_signature)
{
  GDBusVariant *variant;
  variant = g_dbus_variant_new ();
  g_dbus_variant_set_array (variant, array, element_signature);
  return variant;
}

/**
 * g_dbus_variant_new_for_ptr_array:
 * @array: A #GPtrArray.
 * @element_signature: D-Bus signature of the elements stored in @array.
 *
 * Creates a new variant that holds a reference to @array.
 *
 * Returns: A new #GDBusVariant. Free with g_object_unref().
 **/
GDBusVariant *
g_dbus_variant_new_for_ptr_array (GPtrArray   *array,
                                  const gchar *element_signature)
{
  GDBusVariant *variant;
  variant = g_dbus_variant_new ();
  g_dbus_variant_set_ptr_array (variant, array, element_signature);
  return variant;
}

/**
 * g_dbus_variant_new_for_hash_table:
 * @hash_table: A #GHashTable.
 * @key_signature: D-Bus signature of the keys stored in @hash_table.
 * @value_signature: D-Bus signature of the values stored in @hash_table.
 *
 * Creates a new variant that holds a reference to @hash_table.
 *
 * Returns: A new #GDBusVariant. Free with g_object_unref().
 **/
GDBusVariant *
g_dbus_variant_new_for_hash_table (GHashTable   *hash_table,
                                   const gchar  *key_signature,
                                   const gchar  *value_signature)
{
  GDBusVariant *variant;
  variant = g_dbus_variant_new ();
  g_dbus_variant_set_hash_table (variant, hash_table, key_signature, value_signature);
  return variant;
}

/**
 * g_dbus_variant_new_for_structure:
 * @structure: A #GDBusStructure.
 *
 * Creates a new variant that holds a reference to @structure.
 *
 * Returns: A new #GDBusVariant. Free with g_object_unref().
 **/
GDBusVariant *
g_dbus_variant_new_for_structure (GDBusStructure *structure)
{
  GDBusVariant *variant;
  variant = g_dbus_variant_new ();
  g_dbus_variant_set_structure (variant, structure);
  return variant;
}

/**
 * g_dbus_variant_new_for_variant:
 * @variant: A #GDBusVariant.
 *
 * Creates a new variant that holds a reference to @variant.
 *
 * Returns: A new #GDBusVariant. Free with g_object_unref().
 **/
GDBusVariant *
g_dbus_variant_new_for_variant (GDBusVariant *variant)
{
  GDBusVariant *_variant;
  _variant = g_dbus_variant_new ();
  g_dbus_variant_set_variant (_variant, variant);
  return _variant;
}

/* ---------------------------------------------------------------------------------------------------- */

/**
 * g_dbus_variant_set_string:
 * @variant: A #GDBusVariant.
 * @value: A string.
 *
 * Makes @variant hold a copy of @value.
 **/
void
g_dbus_variant_set_string (GDBusVariant *variant,
                           const gchar  *value)
{
  g_return_if_fail (G_IS_DBUS_VARIANT (variant));
  if (variant->priv->signature != NULL)
    g_value_unset (&variant->priv->value);
  g_value_init (&variant->priv->value, G_TYPE_STRING);
  g_value_set_string (&variant->priv->value, value);
  set_signature (variant, "s");
}

/**
 * g_dbus_variant_set_object_path:
 * @variant: A #GDBusVariant.
 * @value: An object path.
 *
  * Makes @variant hold a copy of @value.
 **/
void
g_dbus_variant_set_object_path (GDBusVariant *variant,
                                const gchar  *value)
{
  g_return_if_fail (G_IS_DBUS_VARIANT (variant));
  if (variant->priv->signature != NULL)
    g_value_unset (&variant->priv->value);
  g_value_init (&variant->priv->value, G_TYPE_STRING); //TODO:o G_DBUS_TYPE_OBJECT_PATH);
  g_value_set_string (&variant->priv->value, value);
  set_signature (variant, "o");
}

/**
 * g_dbus_variant_set_signature:
 * @variant: A #GDBusVariant.
 * @value: A signature.
 *
  * Makes @variant hold a copy of @value.
 **/
void
g_dbus_variant_set_signature (GDBusVariant *variant,
                              const gchar  *value)
{
  g_return_if_fail (G_IS_DBUS_VARIANT (variant));
  if (variant->priv->signature != NULL)
    g_value_unset (&variant->priv->value);
  g_value_init (&variant->priv->value, G_TYPE_STRING); //TODO:g G_DBUS_TYPE_OBJECT_PATH);
  g_value_set_string (&variant->priv->value, value);
  set_signature (variant, "g");
}

/**
 * g_dbus_variant_set_string_array:
 * @variant: A #GDBusVariant.
 * @value: A %NULL-terminated string array.
 *
  * Makes @variant hold a copy of @value.
 **/
void
g_dbus_variant_set_string_array (GDBusVariant  *variant,
                                 gchar        **value)
{
  g_return_if_fail (G_IS_DBUS_VARIANT (variant));
  if (variant->priv->signature != NULL)
    g_value_unset (&variant->priv->value);
  g_value_init (&variant->priv->value, G_TYPE_STRV);
  g_value_set_boxed (&variant->priv->value, value);
  set_signature (variant, "as");
}

/**
 * g_dbus_variant_set_object_path_array:
 * @variant: A #GDBusVariant.
 * @value: A %NULL-terminated object path array.
 *
  * Makes @variant hold a copy of @value.
 **/
void
g_dbus_variant_set_object_path_array (GDBusVariant  *variant,
                                      gchar        **value)
{
  g_return_if_fail (G_IS_DBUS_VARIANT (variant));
  if (variant->priv->signature != NULL)
    g_value_unset (&variant->priv->value);
  g_value_init (&variant->priv->value, G_TYPE_STRV); //TODO:o G_DBUS_TYPE_OBJECT_PATH);
  g_value_set_boxed (&variant->priv->value, value);
  set_signature (variant, "ao");
}

/**
 * g_dbus_variant_set_signature_array:
 * @variant: A #GDBusVariant.
 * @value: A %NULL-terminated signature array.
 *
  * Makes @variant hold a copy of @value.
 **/
void
g_dbus_variant_set_signature_array (GDBusVariant  *variant,
                                    gchar        **value)
{
  g_return_if_fail (G_IS_DBUS_VARIANT (variant));
  if (variant->priv->signature != NULL)
    g_value_unset (&variant->priv->value);
  g_value_init (&variant->priv->value, G_TYPE_STRV); //TODO:g G_DBUS_TYPE_SIGNATURE);
  g_value_set_boxed (&variant->priv->value, value);
  set_signature (variant, "ag");
}

/**
 * g_dbus_variant_set_byte:
 * @variant: A #GDBusVariant.
 * @value: A #guchar.
 *
  * Makes @variant hold a #guchar equal to @value.
 **/
void
g_dbus_variant_set_byte (GDBusVariant *variant,
                         guchar        value)
{
  g_return_if_fail (G_IS_DBUS_VARIANT (variant));
  if (variant->priv->signature != NULL)
    g_value_unset (&variant->priv->value);
  g_value_init (&variant->priv->value, G_TYPE_UCHAR);
  g_value_set_uchar (&variant->priv->value, value);
  set_signature (variant, "y");
}

/**
 * g_dbus_variant_set_int16:
 * @variant: A #GDBusVariant.
 * @value: A #gint16.
 *
  * Makes @variant hold a #gint16 equal to @value.
 **/
void
g_dbus_variant_set_int16 (GDBusVariant *variant,
                          gint16        value)
{
  g_return_if_fail (G_IS_DBUS_VARIANT (variant));
  if (variant->priv->signature != NULL)
    g_value_unset (&variant->priv->value);
  g_value_init (&variant->priv->value, G_TYPE_INT); //TODO:16 DBUS_TYPE_INT16);
  g_value_set_int (&variant->priv->value, value);
  set_signature (variant, "n");
}

/**
 * g_dbus_variant_set_uint16:
 * @variant: A #GDBusVariant.
 * @value: A #guint16.
 *
  * Makes @variant hold a #guint64 equal to @value.
 **/
void
g_dbus_variant_set_uint16 (GDBusVariant *variant,
                           guint16       value)
{
  g_return_if_fail (G_IS_DBUS_VARIANT (variant));
  if (variant->priv->signature != NULL)
    g_value_unset (&variant->priv->value);
  g_value_init (&variant->priv->value, G_TYPE_UINT); //TODO:16 G_DBUS_TYPE_UINT16);
  g_value_set_uint (&variant->priv->value, value);
  set_signature (variant, "q");
}

/**
 * g_dbus_variant_set_int:
 * @variant: A #GDBusVariant.
 * @value: A #gint.
 *
  * Makes @variant hold a #gint equal to @value.
 **/
void
g_dbus_variant_set_int (GDBusVariant *variant,
                          gint        value)
{
  g_return_if_fail (G_IS_DBUS_VARIANT (variant));
  if (variant->priv->signature != NULL)
    g_value_unset (&variant->priv->value);
  g_value_init (&variant->priv->value, G_TYPE_INT);
  g_value_set_int (&variant->priv->value, value);
  set_signature (variant, "i");
}

/**
 * g_dbus_variant_set_uint:
 * @variant: A #GDBusVariant.
 * @value: A #guint.
 *
  * Makes @variant hold a #guint equal to @value.
 **/
void
g_dbus_variant_set_uint (GDBusVariant *variant,
                           guint       value)
{
  g_return_if_fail (G_IS_DBUS_VARIANT (variant));
  if (variant->priv->signature != NULL)
    g_value_unset (&variant->priv->value);
  g_value_init (&variant->priv->value, G_TYPE_UINT);
  g_value_set_uint (&variant->priv->value, value);
  set_signature (variant, "u");
}

/**
 * g_dbus_variant_set_int64:
 * @variant: A #GDBusVariant.
 * @value: A #gint64.
 *
  * Makes @variant hold a #gint64 equal to @value.
 **/
void
g_dbus_variant_set_int64 (GDBusVariant *variant,
                          gint64        value)
{
  g_return_if_fail (G_IS_DBUS_VARIANT (variant));
  if (variant->priv->signature != NULL)
    g_value_unset (&variant->priv->value);
  g_value_init (&variant->priv->value, G_TYPE_INT64);
  g_value_set_int64 (&variant->priv->value, value);
  set_signature (variant, "x");
}

/**
 * g_dbus_variant_set_uint64:
 * @variant: A #GDBusVariant.
 * @value: A #guint64.
 *
  * Makes @variant hold a #guint64 equal to @value.
 **/
void
g_dbus_variant_set_uint64 (GDBusVariant *variant,
                           guint64       value)
{
  g_return_if_fail (G_IS_DBUS_VARIANT (variant));
  if (variant->priv->signature != NULL)
    g_value_unset (&variant->priv->value);
  g_value_init (&variant->priv->value, G_TYPE_UINT64);
  g_value_set_uint64 (&variant->priv->value, value);
  set_signature (variant, "t");
}

/**
 * g_dbus_variant_set_boolean:
 * @variant: A #GDBusVariant.
 * @value: A #gboolean.
 *
  * Makes @variant hold a #gboolean equal to @value.
 **/
void
g_dbus_variant_set_boolean (GDBusVariant *variant,
                            gboolean      value)
{
  g_return_if_fail (G_IS_DBUS_VARIANT (variant));
  if (variant->priv->signature != NULL)
    g_value_unset (&variant->priv->value);
  g_value_init (&variant->priv->value, G_TYPE_BOOLEAN);
  g_value_set_boolean (&variant->priv->value, value);
  set_signature (variant, "b");
}

/**
 * g_dbus_variant_set_double:
 * @variant: A #GDBusVariant.
 * @value: A #gdouble.
 *
  * Makes @variant hold a #gdouble equal to @value.
 **/
void
g_dbus_variant_set_double (GDBusVariant *variant,
                           gdouble       value)
{
  g_return_if_fail (G_IS_DBUS_VARIANT (variant));
  if (variant->priv->signature != NULL)
    g_value_unset (&variant->priv->value);
  g_value_init (&variant->priv->value, G_TYPE_DOUBLE);
  g_value_set_double (&variant->priv->value, value);
  set_signature (variant, "d");
}

/**
 * g_dbus_variant_set_array:
 * @variant: A #GDBusVariant.
 * @array: A #GArray.
 * @element_signature: D-Bus signature of the elements stored in @array.
 *
  * Makes @variant hold a reference to @array.
 **/
void
g_dbus_variant_set_array (GDBusVariant   *variant,
                          GArray         *array,
                          const gchar    *element_signature)
{
  g_return_if_fail (G_IS_DBUS_VARIANT (variant));
  if (variant->priv->signature != NULL)
    g_value_unset (&variant->priv->value);
  g_value_init (&variant->priv->value, G_TYPE_ARRAY);
  g_value_set_boxed (&variant->priv->value, array);
  set_signature_for_array (variant, element_signature);
}

/**
 * g_dbus_variant_set_ptr_array:
 * @variant: A #GDBusVariant.
 * @array: A #GPtrArray.
 * @element_signature: D-Bus signature of the elements stored in @array.
 *
  * Makes @variant hold a reference to @array.
 **/
void
g_dbus_variant_set_ptr_array (GDBusVariant   *variant,
                              GPtrArray      *array,
                              const gchar    *element_signature)
{
  g_return_if_fail (G_IS_DBUS_VARIANT (variant));
  if (variant->priv->signature != NULL)
    g_value_unset (&variant->priv->value);
  g_value_init (&variant->priv->value, G_TYPE_PTR_ARRAY);
  g_value_set_boxed (&variant->priv->value, array);
  set_signature_for_array (variant, element_signature);
}

/**
 * g_dbus_variant_set_hash_table:
 * @variant: A #GDBusVariant.
 * @hash_table: A #GHashTable.
 * @key_signature: D-Bus signature of the keys stored in @hash_table.
 * @value_signature: D-Bus signature of the values stored in @hash_table.
 *
  * Makes @variant hold a reference to @hash_table.
 **/
void
g_dbus_variant_set_hash_table (GDBusVariant *variant,
                               GHashTable   *hash_table,
                               const gchar  *key_signature,
                               const gchar  *value_signature)
{
  g_return_if_fail (G_IS_DBUS_VARIANT (variant));
  if (variant->priv->signature != NULL)
    g_value_unset (&variant->priv->value);
  g_value_init (&variant->priv->value, G_TYPE_HASH_TABLE);
  g_value_set_boxed (&variant->priv->value, hash_table);
  set_signature_for_hash_table (variant,
                                key_signature,
                                value_signature);
}

/**
 * g_dbus_variant_set_structure:
 * @variant: A #GDBusVariant.
 * @structure: A #GDBusStructure.
 *
 * Makes @variant hold a reference to @structure.
 **/
void
g_dbus_variant_set_structure (GDBusVariant   *variant,
                              GDBusStructure *structure)
{
  g_return_if_fail (G_IS_DBUS_VARIANT (variant));
  if (variant->priv->signature != NULL)
    g_value_unset (&variant->priv->value);
  g_value_init (&variant->priv->value, G_TYPE_DBUS_STRUCTURE);
  g_value_set_object (&variant->priv->value, structure);
  set_signature (variant, g_dbus_structure_get_signature (structure));
}

/**
 * g_dbus_variant_set_variant:
 * @variant: A #GDBusVariant.
 * @other_variant: A #GDBusVariant.
 *
 * Makes @variant hold a reference to @other_variant.
 **/
void
g_dbus_variant_set_variant (GDBusVariant   *variant,
                            GDBusVariant   *other_variant)
{
  g_return_if_fail (G_IS_DBUS_VARIANT (variant));
  if (variant->priv->signature != NULL)
    g_value_unset (&variant->priv->value);
  g_value_init (&variant->priv->value, G_TYPE_DBUS_VARIANT);
  g_value_set_object (&variant->priv->value, other_variant);
  set_signature (variant, "v");
}

/* ---------------------------------------------------------------------------------------------------- */

/**
 * g_dbus_variant_get_string:
 * @variant: A #GDBusVariant.
 *
 * Gets the string stored in @variant.
 *
 * Returns: A string owned by @variant. Do not free.
 **/
const gchar *
g_dbus_variant_get_string (GDBusVariant *variant)
{
  g_return_val_if_fail (G_IS_DBUS_VARIANT (variant) && g_dbus_variant_is_string (variant), NULL);
  return g_value_get_string (&variant->priv->value);
}

/**
 * g_dbus_variant_get_object_path:
 * @variant: A #GDBusVariant.
 *
 * Gets the object path stored in @variant.
 *
 * Returns: An object path owned by @variant. Do not free.
 **/
const gchar *
g_dbus_variant_get_object_path (GDBusVariant *variant)
{
  g_return_val_if_fail (G_IS_DBUS_VARIANT (variant) && g_dbus_variant_is_object_path (variant), NULL);
  return g_value_get_string (&variant->priv->value); // TODO:o
}

/**
 * g_dbus_variant_get_signature:
 * @variant: A #GDBusVariant.
 *
 * Gets the signature stored in @variant.
 *
 * Returns: A signature owned by @variant. Do not free.
 **/
const gchar *
g_dbus_variant_get_signature (GDBusVariant *variant)
{
  g_return_val_if_fail (G_IS_DBUS_VARIANT (variant) && g_dbus_variant_is_signature (variant), NULL);
  return g_value_get_string (&variant->priv->value); // TODO:g
}

/**
 * g_dbus_variant_get_string_array:
 * @variant: A #GDBusVariant.
 *
 * Gets the string array stored in @variant.
 *
 * Returns: A %NULL-terminated string array signature owned by @variant. Do not free.
 **/
gchar **
g_dbus_variant_get_string_array (GDBusVariant *variant)
{
  g_return_val_if_fail (G_IS_DBUS_VARIANT (variant) && g_dbus_variant_is_string_array (variant), NULL);
  return g_value_get_boxed (&variant->priv->value);
}

/**
 * g_dbus_variant_get_object_path_array:
 * @variant: A #GDBusVariant.
 *
 * Gets the object path array stored in @variant.
 *
 * Returns: A %NULL-terminated object path array owned by @variant. Do not free.
 **/
gchar **
g_dbus_variant_get_object_path_array (GDBusVariant *variant)
{
  g_return_val_if_fail (G_IS_DBUS_VARIANT (variant) && g_dbus_variant_is_object_path_array (variant), NULL);
  return g_value_get_boxed (&variant->priv->value);
}

/**
 * g_dbus_variant_get_signature_array:
 * @variant: A #GDBusVariant.
 *
 * Gets the signature array stored in @variant.
 *
 * Returns: A %NULL-terminated signature array owned by @variant. Do not free.
 **/
gchar **
g_dbus_variant_get_signature_array (GDBusVariant *variant)
{
  g_return_val_if_fail (G_IS_DBUS_VARIANT (variant) && g_dbus_variant_is_signature_array (variant), NULL);
  return g_value_get_boxed (&variant->priv->value);
}

/**
 * g_dbus_variant_get_byte:
 * @variant: A #GDBusVariant.
 *
 * Gets the #guchar stored in @variant.
 *
 * Returns: A #guchar.
 **/
guchar
g_dbus_variant_get_byte (GDBusVariant *variant)
{
  g_return_val_if_fail (G_IS_DBUS_VARIANT (variant) && g_dbus_variant_is_byte (variant), 0);
  return g_value_get_uchar (&variant->priv->value);
}

/**
 * g_dbus_variant_get_int16:
 * @variant: A #GDBusVariant.
 *
 * Gets the #gint16 stored in @variant.
 *
 * Returns: A #gint16.
 **/
gint16
g_dbus_variant_get_int16 (GDBusVariant *variant)
{
  g_return_val_if_fail (G_IS_DBUS_VARIANT (variant) && g_dbus_variant_is_int16 (variant), 0);
  return g_value_get_int (&variant->priv->value); // TODO:16
}

/**
 * g_dbus_variant_get_uint16:
 * @variant: A #GDBusVariant.
 *
 * Gets the #guint16 stored in @variant.
 *
 * Returns: A #guint16.
 **/
guint16
g_dbus_variant_get_uint16 (GDBusVariant *variant)
{
  g_return_val_if_fail (G_IS_DBUS_VARIANT (variant) && g_dbus_variant_is_uint16 (variant), 0);
  return g_value_get_uint (&variant->priv->value); // TODO:16
}

/**
 * g_dbus_variant_get_int:
 * @variant: A #GDBusVariant.
 *
 * Gets the #gint stored in @variant.
 *
 * Returns: A #gint.
 **/
gint
g_dbus_variant_get_int (GDBusVariant *variant)
{
  g_return_val_if_fail (G_IS_DBUS_VARIANT (variant) && g_dbus_variant_is_int (variant), 0);
  return g_value_get_int (&variant->priv->value);
}

/**
 * g_dbus_variant_get_uint:
 * @variant: A #GDBusVariant.
 *
 * Gets the #guint stored in @variant.
 *
 * Returns: A #guint.
 **/
guint
g_dbus_variant_get_uint (GDBusVariant *variant)
{
  g_return_val_if_fail (G_IS_DBUS_VARIANT (variant) && g_dbus_variant_is_uint (variant), 0);
  return g_value_get_uint (&variant->priv->value);
}

/**
 * g_dbus_variant_get_int64:
 * @variant: A #GDBusVariant.
 *
 * Gets the #gint64 stored in @variant.
 *
 * Returns: A #gint64.
 **/
gint64
g_dbus_variant_get_int64 (GDBusVariant *variant)
{
  g_return_val_if_fail (G_IS_DBUS_VARIANT (variant) && g_dbus_variant_is_int64 (variant), 0);
  return g_value_get_int64 (&variant->priv->value);
}

/**
 * g_dbus_variant_get_uint64:
 * @variant: A #GDBusVariant.
 *
 * Gets the #guint64 stored in @variant.
 *
 * Returns: A #guint64.
 **/
guint64
g_dbus_variant_get_uint64 (GDBusVariant *variant)
{
  g_return_val_if_fail (G_IS_DBUS_VARIANT (variant) && g_dbus_variant_is_uint64 (variant), 0);
  return g_value_get_uint64 (&variant->priv->value);
}

/**
 * g_dbus_variant_get_boolean:
 * @variant: A #GDBusVariant.
 *
 * Gets the #gboolean stored in @variant.
 *
 * Returns: A #gboolean.
 **/
gboolean
g_dbus_variant_get_boolean (GDBusVariant *variant)
{
  g_return_val_if_fail (G_IS_DBUS_VARIANT (variant) && g_dbus_variant_is_boolean (variant), FALSE);
  return g_value_get_boolean (&variant->priv->value);
}

/**
 * g_dbus_variant_get_double:
 * @variant: A #GDBusVariant.
 *
 * Gets the #gdouble stored in @variant.
 *
 * Returns: A #gdouble.
 **/
gdouble
g_dbus_variant_get_double (GDBusVariant *variant)
{
  g_return_val_if_fail (G_IS_DBUS_VARIANT (variant) && g_dbus_variant_is_double (variant), 0.0);
  return g_value_get_double (&variant->priv->value);
}

/**
 * g_dbus_variant_get_array:
 * @variant: A #GDBusVariant.
 *
 * Gets the array stored in @variant.
 *
 * Returns: A #GArray owned by @variant. Do not free.
 **/
GArray *
g_dbus_variant_get_array (GDBusVariant *variant)
{
  g_return_val_if_fail (G_IS_DBUS_VARIANT (variant) && g_dbus_variant_is_array (variant), NULL);
  return g_value_get_boxed (&variant->priv->value);
}

/**
 * g_dbus_variant_get_ptr_array:
 * @variant: A #GDBusVariant.
 *
 * Gets the pointer array stored in @variant.
 *
 * Returns: A #GPtrArray owned by @variant. Do not free.
 **/
GPtrArray *
g_dbus_variant_get_ptr_array (GDBusVariant *variant)
{
  g_return_val_if_fail (G_IS_DBUS_VARIANT (variant) && g_dbus_variant_is_ptr_array (variant), NULL);
  return g_value_get_boxed (&variant->priv->value);
}

/**
 * g_dbus_variant_get_hash_table:
 * @variant: A #GDBusVariant.
 *
 * Gets the hash table stored in @variant.
 *
 * Returns: A #GHashTable owned by @variant. Do not free.
 **/
GHashTable  *
g_dbus_variant_get_hash_table (GDBusVariant *variant)
{
  g_return_val_if_fail (G_IS_DBUS_VARIANT (variant) && g_dbus_variant_is_hash_table (variant), NULL);
  return g_value_get_boxed (&variant->priv->value);
}

/**
 * g_dbus_variant_get_structure:
 * @variant: A #GDBusVariant.
 *
 * Gets the structure stored in @variant.
 *
 * Returns: A #GDBusStructure owned by @variant. Do not free.
 **/
GDBusStructure *
g_dbus_variant_get_structure (GDBusVariant *variant)
{
  g_return_val_if_fail (G_IS_DBUS_VARIANT (variant) && g_dbus_variant_is_structure (variant), NULL);
  return g_value_get_object (&variant->priv->value);
}

/**
 * g_dbus_variant_get_variant:
 * @variant: A #GDBusVariant.
 *
 * Gets the variant stored in @variant.
 *
 * Returns: A #GDBusVariant owned by @variant. Do not free.
 **/
GDBusVariant *
g_dbus_variant_get_variant (GDBusVariant *variant)
{
  g_return_val_if_fail (G_IS_DBUS_VARIANT (variant) && g_dbus_variant_is_variant (variant), NULL);
  return g_value_get_object (&variant->priv->value);
}

/* ---------------------------------------------------------------------------------------------------- */

/**
 * g_dbus_variant_is_unset:
 * @variant: A #GDBusVariant.
 *
 * Checks if @variant holds a value.
 *
 * Returns: %TRUE if @variant does not hold a value, %FALSE is @variant holds a value.
 **/
gboolean
g_dbus_variant_is_unset (GDBusVariant *variant)
{
  g_return_val_if_fail (G_IS_DBUS_VARIANT (variant), FALSE);
  return variant->priv->signature != NULL;
}

/**
 * g_dbus_variant_is_string:
 * @variant: A #GDBusVariant.
 *
 * Checks if @variant holds a string.
 *
 * Returns: %TRUE only if @variant holds a string.
 **/
gboolean
g_dbus_variant_is_string (GDBusVariant *variant)
{
  g_return_val_if_fail (G_IS_DBUS_VARIANT (variant), FALSE);
  return
    variant->priv->signature != NULL &&
    variant->priv->signature[0] == 's';
}

/**
 * g_dbus_variant_is_object_path:
 * @variant: A #GDBusVariant.
 *
 * Checks if @variant holds an object path.
 *
 * Returns: %TRUE only if @variant holds an object path.
 **/
gboolean
g_dbus_variant_is_object_path (GDBusVariant *variant)
{
  g_return_val_if_fail (G_IS_DBUS_VARIANT (variant), FALSE);
  return
    variant->priv->signature != NULL &&
    variant->priv->signature[0] == 'o';
}

/**
 * g_dbus_variant_is_signature
 * @variant: A #GDBusVariant.
 *
 * Checks if @variant holds an signature.
 *
 * Returns: %TRUE only if @variant holds an signature.
 **/
gboolean
g_dbus_variant_is_signature (GDBusVariant *variant)
{
  g_return_val_if_fail (G_IS_DBUS_VARIANT (variant), FALSE);
  return
    variant->priv->signature != NULL &&
    variant->priv->signature[0] == 'g';
}

/**
 * g_dbus_variant_is_string_array:
 * @variant: A #GDBusVariant.
 *
 * Checks if @variant holds a string array.
 *
 * Returns: %TRUE only if @variant holds a string array.
 **/
gboolean
g_dbus_variant_is_string_array (GDBusVariant *variant)
{
  g_return_val_if_fail (G_IS_DBUS_VARIANT (variant), FALSE);
  return
    variant->priv->signature != NULL &&
    variant->priv->signature[0] == 'a' &&
    variant->priv->signature[1] == 's';
}

/**
 * g_dbus_variant_is_object_path_array:
 * @variant: A #GDBusVariant.
 *
 * Checks if @variant holds an object path array.
 *
 * Returns: %TRUE only if @variant holds an object path array.
 **/
gboolean
g_dbus_variant_is_object_path_array (GDBusVariant *variant)
{
  g_return_val_if_fail (G_IS_DBUS_VARIANT (variant), FALSE);
  return
    variant->priv->signature != NULL &&
    variant->priv->signature[0] == 'a' &&
    variant->priv->signature[1] == 'o';
}

/**
 * g_dbus_variant_is_signature_array:
 * @variant: A #GDBusVariant.
 *
 * Checks if @variant holds a signature array.
 *
 * Returns: %TRUE only if @variant holds a signature array.
 **/
gboolean
g_dbus_variant_is_signature_array (GDBusVariant *variant)
{
  g_return_val_if_fail (G_IS_DBUS_VARIANT (variant), FALSE);
  return
    variant->priv->signature != NULL &&
    variant->priv->signature[0] == 'a' &&
    variant->priv->signature[1] == 'g';
}

/**
 * g_dbus_variant_is_byte:
 * @variant: A #GDBusVariant.
 *
 * Checks if @variant holds a #guchar.
 *
 * Returns: %TRUE only if @variant holds a #guchar.
 **/
gboolean
g_dbus_variant_is_byte (GDBusVariant *variant)
{
  g_return_val_if_fail (G_IS_DBUS_VARIANT (variant), FALSE);
  return
    variant->priv->signature != NULL &&
    variant->priv->signature[0] == 'y';
}

/**
 * g_dbus_variant_is_int16:
 * @variant: A #GDBusVariant.
 *
 * Checks if @variant holds a #gint16.
 *
 * Returns: %TRUE only if @variant holds a #gint16.
 **/
gboolean
g_dbus_variant_is_int16 (GDBusVariant *variant)
{
  g_return_val_if_fail (G_IS_DBUS_VARIANT (variant), FALSE);
  return
    variant->priv->signature != NULL &&
    variant->priv->signature[0] == 'n';
}

/**
 * g_dbus_variant_is_uint16:
 * @variant: A #GDBusVariant.
 *
 * Checks if @variant holds a #guint16.
 *
 * Returns: %TRUE only if @variant holds a #guint16.
 **/
gboolean
g_dbus_variant_is_uint16 (GDBusVariant *variant)
{
  g_return_val_if_fail (G_IS_DBUS_VARIANT (variant), FALSE);
  return
    variant->priv->signature != NULL &&
    variant->priv->signature[0] == 'q';
}

/**
 * g_dbus_variant_is_int:
 * @variant: A #GDBusVariant.
 *
 * Checks if @variant holds a #gint.
 *
 * Returns: %TRUE only if @variant holds a #gint.
 **/
gboolean
g_dbus_variant_is_int (GDBusVariant *variant)
{
  g_return_val_if_fail (G_IS_DBUS_VARIANT (variant), FALSE);
  return
    variant->priv->signature != NULL &&
    variant->priv->signature[0] == 'i';
}

/**
 * g_dbus_variant_is_uint:
 * @variant: A #GDBusVariant.
 *
 * Checks if @variant holds a #guint.
 *
 * Returns: %TRUE only if @variant holds a #guint.
 **/
gboolean
g_dbus_variant_is_uint (GDBusVariant *variant)
{
  g_return_val_if_fail (G_IS_DBUS_VARIANT (variant), FALSE);
  return
    variant->priv->signature != NULL &&
    variant->priv->signature[0] == 'u';
}

/**
 * g_dbus_variant_is_int64:
 * @variant: A #GDBusVariant.
 *
 * Checks if @variant holds a #gint64.
 *
 * Returns: %TRUE only if @variant holds a #gint64.
 **/
gboolean
g_dbus_variant_is_int64 (GDBusVariant *variant)
{
  g_return_val_if_fail (G_IS_DBUS_VARIANT (variant), FALSE);
  return
    variant->priv->signature != NULL &&
    variant->priv->signature[0] == 'x';
}

/**
 * g_dbus_variant_is_uint64:
 * @variant: A #GDBusVariant.
 *
 * Checks if @variant holds a #guint64.
 *
 * Returns: %TRUE only if @variant holds a #guint64.
 **/
gboolean
g_dbus_variant_is_uint64 (GDBusVariant *variant)
{
  g_return_val_if_fail (G_IS_DBUS_VARIANT (variant), FALSE);
  return
    variant->priv->signature != NULL &&
    variant->priv->signature[0] == 't';
}

/**
 * g_dbus_variant_is_boolean:
 * @variant: A #GDBusVariant.
 *
 * Checks if @variant holds a #gboolean.
 *
 * Returns: %TRUE only if @variant holds a #gboolean.
 **/
gboolean
g_dbus_variant_is_boolean (GDBusVariant *variant)
{
  g_return_val_if_fail (G_IS_DBUS_VARIANT (variant), FALSE);
  return
    variant->priv->signature != NULL &&
    variant->priv->signature[0] == 'b';
}

/**
 * g_dbus_variant_is_double:
 * @variant: A #GDBusVariant.
 *
 * Checks if @variant holds a #gdouble.
 *
 * Returns: %TRUE only if @variant holds a #gdouble.
 **/
gboolean
g_dbus_variant_is_double (GDBusVariant *variant)
{
  g_return_val_if_fail (G_IS_DBUS_VARIANT (variant), FALSE);
  return
    variant->priv->signature != NULL &&
    variant->priv->signature[0] == 'd';
}

/**
 * g_dbus_variant_is_array:
 * @variant: A #GDBusVariant.
 *
 * Checks if @variant holds an array.
 *
 * Returns: %TRUE only if @variant holds an array.
 **/
gboolean
g_dbus_variant_is_array (GDBusVariant *variant)
{
  g_return_val_if_fail (G_IS_DBUS_VARIANT (variant), FALSE);
  return
    variant->priv->signature != NULL &&
    variant->priv->signature[0] == 'a' &&
    (variant->priv->signature[1] == 'y' ||
     variant->priv->signature[1] == 'n' ||
     variant->priv->signature[1] == 'q' ||
     variant->priv->signature[1] == 'i' ||
     variant->priv->signature[1] == 'x' ||
     variant->priv->signature[1] == 't' ||
     variant->priv->signature[1] == 'd' ||
     variant->priv->signature[1] == 'b');
}

/**
 * g_dbus_variant_is_ptr_array:
 * @variant: A #GDBusVariant.
 *
 * Checks if @variant holds a pointer array.
 *
 * Returns: %TRUE only if @variant holds a pointer array.
 **/
gboolean
g_dbus_variant_is_ptr_array (GDBusVariant *variant)
{
  g_return_val_if_fail (G_IS_DBUS_VARIANT (variant), FALSE);
  return
    variant->priv->signature != NULL &&
    variant->priv->signature[0] == 'a' &&
    (variant->priv->signature[1] == 'a' ||
     variant->priv->signature[1] == 'v' ||
     variant->priv->signature[1] == '(');
}

/**
 * g_dbus_variant_is_hash_table:
 * @variant: A #GDBusVariant.
 *
 * Checks if @variant holds a hash table.
 *
 * Returns: %TRUE only if @variant holds a hash table.
 **/
gboolean
g_dbus_variant_is_hash_table (GDBusVariant *variant)
{
  g_return_val_if_fail (G_IS_DBUS_VARIANT (variant), FALSE);
  return
    variant->priv->signature != NULL &&
    variant->priv->signature[0] == 'a' &&
    variant->priv->signature[1] == '{';
}

/**
 * g_dbus_variant_is_structure:
 * @variant: A #GDBusVariant.
 *
 * Checks if @variant holds a structure.
 *
 * Returns: %TRUE only if @variant holds a structure.
 **/
gboolean
g_dbus_variant_is_structure (GDBusVariant *variant)
{
  g_return_val_if_fail (G_IS_DBUS_VARIANT (variant), FALSE);
  return
    variant->priv->signature != NULL &&
    variant->priv->signature[0] == '(';
}

/**
 * g_dbus_variant_is_variant:
 * @variant: A #GDBusVariant.
 *
 * Checks if @variant holds a variant.
 *
 * Returns: %TRUE only if @variant holds a structure.
 **/
gboolean
g_dbus_variant_is_variant (GDBusVariant *variant)
{
  g_return_val_if_fail (G_IS_DBUS_VARIANT (variant), FALSE);
  return
    variant->priv->signature != NULL &&
    variant->priv->signature[0] == 'v';
}

/* ---------------------------------------------------------------------------------------------------- */

#define __G_DBUS_VARIANT_C__
#include "gdbusaliasdef.c"
