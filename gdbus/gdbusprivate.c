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
#include <string.h>

#include <glib/gi18n.h>

#include <dbus/dbus.h>

#include "gdbustypes.h"
#include "gdbusprivate.h"
#include "gdbusvariant.h"
#include "gdbusstructure.h"

void
_g_dbus_oom (void)
{
  /* TODO: print stack trace etc. */
  g_error ("OOM from libdbus");
  abort ();
}

/* ---------------------------------------------------------------------------------------------------- */

/**
 * _gdbus_signature_vararg_foreach:
 * @signature: A D-Bus signature of zero or more single complete types.
 * @first_type: First #GType to match up.
 * @va_args: The value of the first in argument, followed by zero or more (type, value) pairs, followed
 * by #G_TYPE_INVALID.
 * @callback: Callback function.
 * @user_data: User data to pass to @callback.
 *
 * Splits up @signature into single complete types, matches each single complete type with
 * the #GType<!-- -->s and values from @first_type, @va_args and invokes @callback for each
 * matching pair.
 *
 * Returns: %TRUE if @callback short-circuited the matching, %FALSE otherwise.
 */
gboolean
_gdbus_signature_vararg_foreach (const gchar                         *signature,
                                 GType                                first_type,
                                 va_list                              va_args,
                                 _GDBusSignatureVarargForeachCallback callback,
                                 gpointer                             user_data)
{
  DBusSignatureIter iter;
  gchar *element_signature;
  gboolean ret;
  GType gtype;
  guint arg_num;

  g_assert (dbus_signature_validate (signature, NULL));

  ret = FALSE;

  if (strlen (signature) == 0)
      goto out;

  g_assert (first_type != G_TYPE_INVALID);

  dbus_signature_iter_init (&iter, signature);

  gtype = first_type;
  arg_num = 0;
  do
    {
      if (arg_num > 0)
        gtype = va_arg (va_args, GType);

      element_signature = dbus_signature_iter_get_signature (&iter);

      ret = callback (arg_num,
                      element_signature,
                      gtype,
                      va_args,
                      user_data);
      dbus_free (element_signature);

      if (ret)
        goto out;

      arg_num++;
    }
  while (dbus_signature_iter_next (&iter));

 out:
  return ret;
}

/* ---------------------------------------------------------------------------------------------------- */

gboolean
_g_strv_equal (gchar **strv1,
               gchar **strv2)
{
  guint n;

  if (strv1 == NULL && strv2 == NULL)
    return TRUE;
  if (strv1 == NULL || strv2 == NULL)
    return FALSE;

  for (n = 0; strv1[n] != NULL && strv2[n] != NULL; n++)
    {
      if (g_strcmp0 (strv1[n], strv2[n]) != 0)
        return FALSE;
    }
  if (strv1[n] != NULL || strv2[n] != NULL)
    return FALSE;

  return TRUE;
}

gboolean
_g_array_equal (GArray *array1,
                GArray *array2)
{
  if (array1 == NULL && array2 == NULL)
    return TRUE;
  if (array1 == NULL || array2 == NULL)
    return FALSE;

  if (array1->len != array2->len)
    return FALSE;

  if (array1->len == 0)
    return TRUE;

  if (g_array_get_element_size (array1) != g_array_get_element_size (array2))
    return FALSE;

  return memcmp (array1->data, array2->data, g_array_get_element_size (array1) * array1->len) == 0;
}

static gboolean
values_equal (gpointer     value1,
              gpointer     value2,
              const gchar *signature)
{
  gboolean ret;

  ret = FALSE;
  switch (signature[0])
    {
    case DBUS_TYPE_BYTE:
    case DBUS_TYPE_BOOLEAN:
    case DBUS_TYPE_INT16:
    case DBUS_TYPE_UINT16:
    case DBUS_TYPE_INT32:
    case DBUS_TYPE_UINT32:
      ret = (value1 == value2);
      break;

    case DBUS_TYPE_OBJECT_PATH:
    case DBUS_TYPE_SIGNATURE:
    case DBUS_TYPE_STRING:
      ret = (strcmp (value1, value2) == 0);
      break;

    case DBUS_TYPE_INT64:
    case DBUS_TYPE_UINT64:
      ret = (*((const gint64*) value1) == *((const gint64*) value2));
      break;

    case DBUS_TYPE_DOUBLE:
      ret = (*((const gdouble*) value1) == *((const gdouble*) value2));
      break;

    case DBUS_TYPE_VARIANT:
      ret = g_dbus_variant_equal (value1, value2);
      break;

    case DBUS_STRUCT_BEGIN_CHAR:
      ret = g_dbus_structure_equal (value1, value2);
      break;

    case DBUS_TYPE_ARRAY:
      switch (signature[1])
        {
        case DBUS_TYPE_BYTE:
        case DBUS_TYPE_BOOLEAN:
        case DBUS_TYPE_INT16:
        case DBUS_TYPE_UINT16:
        case DBUS_TYPE_INT32:
        case DBUS_TYPE_UINT32:
        case DBUS_TYPE_INT64:
        case DBUS_TYPE_UINT64:
        case DBUS_TYPE_DOUBLE:
          ret = _g_array_equal (value1, value2);
          break;

        case DBUS_TYPE_OBJECT_PATH:
        case DBUS_TYPE_SIGNATURE:
        case DBUS_TYPE_STRING:
          ret = _g_strv_equal (value1, value2);
          break;

        case DBUS_STRUCT_BEGIN_CHAR:
        case DBUS_TYPE_VARIANT:
          ret = _g_ptr_array_equal (value1, value2, signature + 1);
          break;

        case DBUS_DICT_ENTRY_BEGIN_CHAR:
          {
            char key_sig[2];
            char *val_sig;

            /* keys are guaranteed by the D-Bus spec to be primitive types */
            key_sig[0] = signature[2];
            key_sig[1] = '\0';
            val_sig = g_strdup (signature + 3);
            val_sig[strlen (val_sig) - 1] = '\0';

            ret = _g_hash_table_equal (value1, value2, key_sig, val_sig);
            g_free (val_sig);
          }
          break;

        default:
          g_assert_not_reached ();
          break;
        }
      break;

    default:
      g_assert_not_reached ();
      break;
    }

  return ret;
}

gboolean
_g_ptr_array_equal (GPtrArray *array1,
                    GPtrArray *array2,
                    const gchar *element_signature)
{
  gboolean ret;
  guint n;

  if (array1 == NULL && array2 == NULL)
    return TRUE;
  if (array1 == NULL || array2 == NULL)
    return FALSE;

  if (array1->len != array2->len)
    return FALSE;

  if (array1->len == 0)
    return TRUE;

  ret = FALSE;

  for (n = 0; n < array1->len; n++)
    {
      gpointer value1 = array1->pdata[n];
      gpointer value2 = array2->pdata[n];

      if (!values_equal (value1, value2, element_signature))
        goto out;

    }

  ret = TRUE;

 out:
  return ret;
}

gboolean
_g_hash_table_equal (GHashTable *hash1,
                     GHashTable *hash2,
                     const gchar *key_signature,
                     const gchar *value_signature)
{
  gboolean ret;
  GHashTableIter iter;
  gpointer key1;
  gpointer value1;
  gpointer value2;

  if (hash1 == NULL && hash2 == NULL)
    return TRUE;
  if (hash1 == NULL || hash2 == NULL)
    return FALSE;

  if (g_hash_table_size (hash1) != g_hash_table_size (hash2))
    return FALSE;

  if (g_hash_table_size (hash1) == 0)
    return TRUE;

  ret = FALSE;

  g_hash_table_iter_init (&iter, hash1);
  while (g_hash_table_iter_next (&iter, &key1, &value1))
    {
      /* this takes care of comparing the keys (relying on the hashes being set up correctly) */
      if (!g_hash_table_lookup_extended (hash2, key1, NULL, &value2))
        goto out;

      if (!values_equal (value1, value2, value_signature))
        goto out;
    }

  ret = TRUE;

 out:
  return ret;
}

/* ---------------------------------------------------------------------------------------------------- */

/* this equal func only works for GValue instances used by GDBus */
gboolean
_g_dbus_gvalue_equal (const GValue *value1,
                      const GValue *value2,
                      const gchar  *signature)
{
  GType type;
  gboolean ret;

  if (value1 == NULL && value2 == NULL)
    return TRUE;
  if (value1 == NULL || value2 == NULL)
    return FALSE;

  if (G_VALUE_TYPE (value1) != G_VALUE_TYPE (value2))
    return FALSE;

  ret = FALSE;
  type = G_VALUE_TYPE (value1);
  switch (type)
    {
    case G_TYPE_UCHAR:
      ret = (g_value_get_uchar (value1) == g_value_get_uchar (value2));
      break;

    case G_TYPE_BOOLEAN:
      ret = (g_value_get_boolean (value1) == g_value_get_boolean (value2));
      break;

    case G_TYPE_INT: // TODO:16
      ret = (g_value_get_int (value1) == g_value_get_int (value2));
      break;

    case G_TYPE_UINT:
      ret = (g_value_get_uint (value1) == g_value_get_uint (value2));
      break;

    case G_TYPE_INT64:
      ret = (g_value_get_int64 (value1) == g_value_get_int64 (value2));
      break;

    case G_TYPE_UINT64:
      ret = (g_value_get_uint64 (value1) == g_value_get_uint64 (value2));
      break;

    case G_TYPE_DOUBLE:
      ret = (g_value_get_double (value1) == g_value_get_double (value2));
      break;

    case G_TYPE_STRING: // TODO:o TODO:g
      ret = (g_strcmp0 (g_value_get_string (value1), g_value_get_string (value2)) == 0);
      break;

    default:
      if (type == G_TYPE_STRV) // TODO:ao TODO:ag
        ret = _g_strv_equal (g_value_get_boxed (value1),
                             g_value_get_boxed (value2));
      else if (type == G_TYPE_ARRAY)
        ret = _g_array_equal (g_value_get_boxed (value1),
                              g_value_get_boxed (value2));
      else if (type == G_TYPE_PTR_ARRAY)
        ret = _g_ptr_array_equal (g_value_get_boxed (value1),
                                  g_value_get_boxed (value2),
                                  signature + 1);
      else if (type == G_TYPE_HASH_TABLE)
        {
          char key_sig[2];
          char *val_sig;

          /* keys are guaranteed by the D-Bus spec to be primitive types */
          key_sig[0] = signature[2];
          key_sig[1] = '\0';
          val_sig = g_strdup (signature + 3);
          val_sig[strlen (val_sig) - 1] = '\0';

          ret = _g_hash_table_equal (g_value_get_boxed (value1),
                                     g_value_get_boxed (value2),
                                     key_sig,
                                     val_sig);
          g_free (val_sig);
        }
      else if (type == G_TYPE_DBUS_STRUCTURE)
        {
          ret = g_dbus_structure_equal (g_value_get_object (value1),
                                        g_value_get_object (value2));
        }
      else
        g_assert_not_reached ();
      break;
    }

  return ret;
}

/**
 * _g_dbus_signature_get_gtype:
 * @signature: A D-Bus signature
 *
 * Returns the #GType to use for @signature.
 *
 * Returns: A #GType.
 **/
GType
_g_dbus_signature_get_gtype (const gchar *signature)
{
  GType ret;

  ret = G_TYPE_INVALID;

  switch (signature[0])
    {
    case DBUS_TYPE_BYTE:
      ret = G_TYPE_UCHAR;
      break;

    case DBUS_TYPE_BOOLEAN:
      ret = G_TYPE_BOOLEAN;
      break;

    case DBUS_TYPE_INT16:
      ret = G_TYPE_INT; // TODO:16
      break;

    case DBUS_TYPE_UINT16:
      ret = G_TYPE_UINT; // TODO:16
      break;

    case DBUS_TYPE_INT32:
      ret = G_TYPE_INT;
      break;

    case DBUS_TYPE_UINT32:
      ret = G_TYPE_UINT;
      break;

    case DBUS_TYPE_INT64:
      ret = G_TYPE_INT64;
      break;

    case DBUS_TYPE_UINT64:
      ret = G_TYPE_UINT64;
      break;

    case DBUS_TYPE_DOUBLE:
      ret = G_TYPE_DOUBLE;
      break;

    case DBUS_TYPE_VARIANT:
      ret = G_TYPE_DBUS_VARIANT;
      break;

    case DBUS_STRUCT_BEGIN_CHAR:
      ret = G_TYPE_DBUS_STRUCTURE;
      break;

    case DBUS_TYPE_OBJECT_PATH: // TODO:o
    case DBUS_TYPE_SIGNATURE: // TODO:g
    case DBUS_TYPE_STRING:
      ret = G_TYPE_STRING;
      break;

    case DBUS_TYPE_ARRAY:
      switch (signature[1])
        {
        case DBUS_TYPE_BYTE:
        case DBUS_TYPE_BOOLEAN:
        case DBUS_TYPE_INT16:
        case DBUS_TYPE_UINT16:
        case DBUS_TYPE_INT32:
        case DBUS_TYPE_UINT32:
        case DBUS_TYPE_INT64:
        case DBUS_TYPE_UINT64:
        case DBUS_TYPE_DOUBLE:
          ret = G_TYPE_ARRAY;
          break;

        case DBUS_TYPE_OBJECT_PATH: // TODO:ao
        case DBUS_TYPE_SIGNATURE: // TODO:ag
        case DBUS_TYPE_STRING:
          ret = G_TYPE_STRV;
          break;

        case DBUS_STRUCT_BEGIN_CHAR:
        case DBUS_TYPE_VARIANT:
        case DBUS_TYPE_ARRAY:
          ret = G_TYPE_PTR_ARRAY;
          break;

        case DBUS_DICT_ENTRY_BEGIN_CHAR:
          ret = G_TYPE_HASH_TABLE;
          break;

        default:
          g_warning ("Failed determining GType for D-Bus array with signature '%s'", signature);
          g_assert_not_reached ();
          break;
        }
      break;

    default:
      g_warning ("Failed determining GType for D-Bus signature '%s'", signature);
      g_assert_not_reached ();
      break;
    }

  return ret;
}
