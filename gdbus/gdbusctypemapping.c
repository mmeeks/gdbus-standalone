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

#include "gdbuserror.h"
#include "gdbusctypemapping-lowlevel.h"
#include "gdbusprivate.h"

#include "gdbusalias.h"

/**
 * SECTION:gdbusctypemapping
 * @title: Mapping GType to D-Bus
 * @short_description: Encode and decode D-Bus messages
 * @include: gdbus/gdbus-lowlevel.h
 * @stability: Unstable
 *
 * Functions for mapping D-Bus messages to the GLib type system.
 */

static guint
get_element_size (gchar dbus_type_code)
{
  guint ret;

  switch (dbus_type_code)
    {
    case DBUS_TYPE_BYTE:
      ret = 1;
      break;
    case DBUS_TYPE_BOOLEAN:
      ret = sizeof (gboolean);
      break;
    case DBUS_TYPE_INT16:
    case DBUS_TYPE_UINT16:
      ret = sizeof (gint16);
      break;
    case DBUS_TYPE_INT32:
    case DBUS_TYPE_UINT32:
      ret = sizeof (gint);
      break;
    case DBUS_TYPE_INT64:
    case DBUS_TYPE_UINT64:
      ret = sizeof (gint64);
      break;
    case DBUS_TYPE_DOUBLE:
      ret = sizeof (gdouble);
      break;
    default:
      g_assert_not_reached ();
    }

  return ret;
}

/* ---------------------------------------------------------------------------------------------------- */

/**
 * g_dbus_c_type_mapping_get_value_from_iter:
 * @iter: A #DBusMessageIter
 * @out_value: Return location for result (must be initialized to zeroes).
 * @error: Return location for error or %NULL.
 *
 * Constructs a #GValue for the single complete type pointed to by @iter. Note that @iter is not
 * advanced to the next position.
 *
 * Returns: %TRUE on success, %FALSE if @error is set.
 **/
gboolean
g_dbus_c_type_mapping_get_value_from_iter (DBusMessageIter   *iter,
                                           GValue            *out_value,
                                           GError           **error)
{
  int arg_type;
  int array_arg_type;
  dbus_bool_t bool_val;
  const char *str_val;
  guchar uint8_val;
  dbus_int16_t int16_val;
  dbus_uint16_t uint16_val;
  dbus_int32_t int32_val;
  dbus_uint32_t uint32_val;
  dbus_int64_t int64_val;
  dbus_uint64_t uint64_val;
  double double_val;
  gboolean ret;

  /* TODO: - fill in @error instead of using g_assert_not_reached() */

  g_return_val_if_fail (iter != NULL, FALSE);
  g_return_val_if_fail (out_value != NULL, FALSE);

  ret = FALSE;

  arg_type = dbus_message_iter_get_arg_type (iter);

  switch (arg_type)
    {
    case DBUS_TYPE_STRING:
      g_value_init (out_value, G_TYPE_STRING);
      dbus_message_iter_get_basic (iter, &str_val);
      g_value_set_string (out_value, str_val);
      break;

    case DBUS_TYPE_OBJECT_PATH:
      g_value_init (out_value, G_TYPE_STRING); // TODO:o
      dbus_message_iter_get_basic (iter, &str_val);
      g_value_set_string (out_value, str_val);
      break;

    case DBUS_TYPE_SIGNATURE:
      g_value_init (out_value, G_TYPE_STRING); // TODO:g
      dbus_message_iter_get_basic (iter, &str_val);
      g_value_set_string (out_value, str_val);
      break;

    case DBUS_TYPE_BOOLEAN:
      g_value_init (out_value, G_TYPE_BOOLEAN);
      dbus_message_iter_get_basic (iter, &bool_val);
      g_value_set_boolean (out_value, bool_val);
      break;

    case DBUS_TYPE_BYTE:
      g_value_init (out_value, G_TYPE_UCHAR);
      dbus_message_iter_get_basic (iter, &uint8_val);
      g_value_set_uchar (out_value, uint8_val);
      break;

    case DBUS_TYPE_INT16:
      g_value_init (out_value, G_TYPE_INT); // TODO:16
      dbus_message_iter_get_basic (iter, &int16_val);
      g_value_set_int (out_value, int16_val);
      break;

    case DBUS_TYPE_UINT16:
      g_value_init (out_value, G_TYPE_UINT); // TODO:16
      dbus_message_iter_get_basic (iter, &uint16_val);
      g_value_set_uint (out_value, uint16_val);
      break;

    case DBUS_TYPE_INT32:
      g_value_init (out_value, G_TYPE_INT);
      dbus_message_iter_get_basic (iter, &int32_val);
      g_value_set_int (out_value, int32_val);
      break;

    case DBUS_TYPE_UINT32:
      g_value_init (out_value, G_TYPE_UINT);
      dbus_message_iter_get_basic (iter, &uint32_val);
      g_value_set_uint (out_value, uint32_val);
      break;

    case DBUS_TYPE_INT64:
      g_value_init (out_value, G_TYPE_INT64);
      dbus_message_iter_get_basic (iter, &int64_val);
      g_value_set_int64 (out_value, int64_val);
      break;

    case DBUS_TYPE_UINT64:
      g_value_init (out_value, G_TYPE_UINT64);
      dbus_message_iter_get_basic (iter, &uint64_val);
      g_value_set_uint64 (out_value, uint64_val);
      break;

    case DBUS_TYPE_DOUBLE:
      g_value_init (out_value, G_TYPE_DOUBLE);
      dbus_message_iter_get_basic (iter, &double_val);
      g_value_set_double (out_value, double_val);
      break;

    case DBUS_TYPE_STRUCT:
      {
        DBusMessageIter struct_iter;
        char *struct_signature;
        GDBusStructure *structure;
        GArray *a;

        struct_signature = dbus_message_iter_get_signature (iter);

        a = g_array_new (FALSE,
                         TRUE,
                         sizeof (GValue));

        dbus_message_iter_recurse (iter, &struct_iter);

        /* now collect all the elements in the structure as GValue objects */
        while (dbus_message_iter_get_arg_type (&struct_iter) != DBUS_TYPE_INVALID)
          {
            GValue *value;

            g_array_set_size (a, a->len + 1);
            value = &g_array_index (a, GValue, a->len - 1);

            /* doing the recursive dance! */
            if (!g_dbus_c_type_mapping_get_value_from_iter (&struct_iter, value, error))
              goto out;

            dbus_message_iter_next (&struct_iter);
          }

        /* takes ownership of elems */
        structure = _g_dbus_structure_new_for_values (struct_signature,
                                                      a->len,
                                                      (GValue *) a->data);
        g_assert (structure != NULL);

        g_array_free (a, FALSE);

        g_value_init (out_value, G_TYPE_DBUS_STRUCTURE);
        g_value_take_object (out_value, structure);

        dbus_free (struct_signature);
      }
      break;

    case DBUS_TYPE_ARRAY:
      array_arg_type = dbus_message_iter_get_element_type (iter);
      if (array_arg_type == DBUS_TYPE_STRING ||
          array_arg_type == DBUS_TYPE_OBJECT_PATH ||
          array_arg_type == DBUS_TYPE_SIGNATURE)
        {
          GPtrArray *p;
          DBusMessageIter array_iter;
          GType boxed_type;

          if (array_arg_type == DBUS_TYPE_STRING)
            boxed_type = G_TYPE_STRV;
          else if (array_arg_type == DBUS_TYPE_OBJECT_PATH)
            boxed_type = G_TYPE_STRV; // TODO:o EGG_DBUS_TYPE_OBJECT_PATH_ARRAY;
          else if (array_arg_type == DBUS_TYPE_SIGNATURE)
            boxed_type = G_TYPE_STRV; // TODO:g EGG_DBUS_TYPE_SIGNATURE_ARRAY;
          else
            g_assert_not_reached ();

          dbus_message_iter_recurse (iter, &array_iter);
          p = g_ptr_array_new ();
          while (dbus_message_iter_get_arg_type (&array_iter) != DBUS_TYPE_INVALID)
            {
              dbus_message_iter_get_basic (&array_iter, &str_val);
              g_ptr_array_add (p, (void *) g_strdup (str_val));
              dbus_message_iter_next (&array_iter);
            }
          g_ptr_array_add (p, NULL);
          g_value_init (out_value, boxed_type);
          g_value_take_boxed (out_value, p->pdata);
          g_ptr_array_free (p, FALSE);
        }
      else if (array_arg_type == DBUS_TYPE_BYTE ||
               array_arg_type == DBUS_TYPE_INT16 ||
               array_arg_type == DBUS_TYPE_UINT16 ||
               array_arg_type == DBUS_TYPE_INT32 ||
               array_arg_type == DBUS_TYPE_UINT32 ||
               array_arg_type == DBUS_TYPE_INT64 ||
               array_arg_type == DBUS_TYPE_UINT64 ||
               array_arg_type == DBUS_TYPE_BOOLEAN ||
               array_arg_type == DBUS_TYPE_DOUBLE)
        {
          GArray *a;
          DBusMessageIter array_iter;
          gconstpointer data;
          gint num_items;

          dbus_message_iter_recurse (iter, &array_iter);
          dbus_message_iter_get_fixed_array (&array_iter,
                                             &data,
                                             (int*) &num_items);
          a = g_array_sized_new (FALSE, FALSE, get_element_size (array_arg_type), num_items);
          g_array_append_vals (a, data, num_items);
          g_value_init (out_value, G_TYPE_ARRAY);
          g_value_take_boxed (out_value, a);
        }
      else if (array_arg_type == DBUS_TYPE_STRUCT)
        {
          DBusMessageIter array_iter;
          char *struct_signature;
          GPtrArray *p;

          p = g_ptr_array_new_with_free_func (g_object_unref);

          dbus_message_iter_recurse (iter, &array_iter);

          struct_signature = dbus_message_iter_get_signature (&array_iter);

          /* now collect all the elements in the structure.
           */
          while (dbus_message_iter_get_arg_type (&array_iter) != DBUS_TYPE_INVALID)
            {
              GValue elem_value = {0};

              /* recurse */
              if (!g_dbus_c_type_mapping_get_value_from_iter (&array_iter, &elem_value, error))
                {
                  dbus_free (struct_signature);
                  g_ptr_array_unref (p);
                  goto out;
                }

              g_ptr_array_add (p, g_value_get_object (&elem_value));

              dbus_message_iter_next (&array_iter);
            }

          g_value_init (out_value, G_TYPE_PTR_ARRAY);
          g_value_take_boxed (out_value, p);

          dbus_free (struct_signature);
        }
      else if (array_arg_type == DBUS_TYPE_DICT_ENTRY)
        {
          DBusMessageIter array_iter;
          GHashTable *hash;
          char key_sig[2];
          char *val_sig;
          char *array_sig;
          GHashFunc hash_func;
          GEqualFunc equal_func;
          GDestroyNotify key_free_func;
          GDestroyNotify val_free_func;

          dbus_message_iter_recurse (iter, &array_iter);

          array_sig = dbus_message_iter_get_signature (&array_iter);

          /* keys are guaranteed by the D-Bus spec to be primitive types */
          key_sig[0] = array_sig[1];
          key_sig[1] = '\0';
          val_sig = g_strdup (array_sig + 2);
          val_sig[strlen (val_sig) - 1] = '\0';

          /* set up the hash table */

          switch (key_sig[0])
            {
            case DBUS_TYPE_BOOLEAN:
            case DBUS_TYPE_BYTE:
            case DBUS_TYPE_INT16:
            case DBUS_TYPE_UINT16:
            case DBUS_TYPE_INT32:
            case DBUS_TYPE_UINT32:
              key_free_func = NULL;
              hash_func = g_direct_hash;
              equal_func = g_direct_equal;
              break;

            case DBUS_TYPE_INT64:
            case DBUS_TYPE_UINT64:
              hash_func = g_int64_hash;
              equal_func = g_int64_equal;
              key_free_func = g_free;
              break;

            case DBUS_TYPE_DOUBLE:
              hash_func = g_double_hash;
              equal_func = g_double_equal;
              key_free_func = g_free;
              break;

            case DBUS_TYPE_STRING:
            case DBUS_TYPE_OBJECT_PATH:
            case DBUS_TYPE_SIGNATURE:
              hash_func = g_str_hash;
              equal_func = g_str_equal;
              key_free_func = g_free;
              break;

            default:
              g_assert_not_reached ();
              break;
            }

          switch (val_sig[0])
            {
            case DBUS_TYPE_BOOLEAN:
            case DBUS_TYPE_BYTE:
            case DBUS_TYPE_INT16:
            case DBUS_TYPE_UINT16:
            case DBUS_TYPE_INT32:
            case DBUS_TYPE_UINT32:
              val_free_func = NULL;
              break;

            case DBUS_TYPE_INT64:
            case DBUS_TYPE_UINT64:
            case DBUS_TYPE_DOUBLE:
            case DBUS_TYPE_STRING:
            case DBUS_TYPE_OBJECT_PATH:
            case DBUS_TYPE_SIGNATURE:
              val_free_func = g_free;
              break;

            case DBUS_TYPE_ARRAY:
              switch (val_sig[1])
                {
                case DBUS_TYPE_STRING:
                case DBUS_TYPE_OBJECT_PATH:
                case DBUS_TYPE_SIGNATURE:
                  val_free_func = (GDestroyNotify) g_strfreev;
                  break;

                case DBUS_TYPE_BOOLEAN:
                case DBUS_TYPE_BYTE:
                case DBUS_TYPE_INT16:
                case DBUS_TYPE_UINT16:
                case DBUS_TYPE_INT32:
                case DBUS_TYPE_UINT32:
                case DBUS_TYPE_INT64:
                case DBUS_TYPE_UINT64:
                case DBUS_TYPE_DOUBLE:
                  val_free_func = (GDestroyNotify) g_array_unref;
                  break;

                case DBUS_STRUCT_BEGIN_CHAR:
                case DBUS_DICT_ENTRY_BEGIN_CHAR:
                  val_free_func = (GDestroyNotify) g_ptr_array_unref;
                  break;

                case DBUS_TYPE_VARIANT:
                  val_free_func = (GDestroyNotify) g_ptr_array_unref;
                  break;

                case DBUS_TYPE_ARRAY:
                  val_free_func = (GDestroyNotify) g_ptr_array_unref;
                  break;

                default:
                  g_assert_not_reached ();
                  break;
                }
              break;

            case DBUS_STRUCT_BEGIN_CHAR:
              val_free_func = g_object_unref;
              break;

            case DBUS_TYPE_VARIANT:
              val_free_func = g_object_unref;
              break;

            default:
              g_assert_not_reached ();
              break;
            }

          hash = g_hash_table_new_full (hash_func,
                                        equal_func,
                                        key_free_func,
                                        val_free_func);

          while (dbus_message_iter_get_arg_type (&array_iter) != DBUS_TYPE_INVALID)
            {
              DBusMessageIter hash_iter;
              gpointer key, value;
              const char *str_val;

              dbus_message_iter_recurse (&array_iter, &hash_iter);

              switch (key_sig[0])
                {
                case DBUS_TYPE_BOOLEAN:
                  dbus_message_iter_get_basic (&hash_iter, &bool_val);
                  key = GINT_TO_POINTER (bool_val);
                  break;

                case DBUS_TYPE_BYTE:
                  dbus_message_iter_get_basic (&hash_iter, &uint8_val);
                  key = GINT_TO_POINTER (uint8_val);
                  break;

                case DBUS_TYPE_INT16:
                  dbus_message_iter_get_basic (&hash_iter, &int16_val);
                  key = GINT_TO_POINTER (int16_val);
                  break;

                case DBUS_TYPE_UINT16:
                  dbus_message_iter_get_basic (&hash_iter, &uint16_val);
                  key = GINT_TO_POINTER (uint16_val);
                  break;

                case DBUS_TYPE_INT32:
                  dbus_message_iter_get_basic (&hash_iter, &int32_val);
                  key = GINT_TO_POINTER (int32_val);
                  break;

                case DBUS_TYPE_UINT32:
                  dbus_message_iter_get_basic (&hash_iter, &uint32_val);
                  key = GINT_TO_POINTER (uint32_val);
                  break;

                case DBUS_TYPE_INT64:
                  dbus_message_iter_get_basic (&hash_iter, &int64_val);
                  key = g_memdup (&int64_val, sizeof (gint64));
                  break;

                case DBUS_TYPE_UINT64:
                  dbus_message_iter_get_basic (&hash_iter, &uint64_val);
                  key = g_memdup (&uint64_val, sizeof (guint64));
                  break;

                case DBUS_TYPE_DOUBLE:
                  dbus_message_iter_get_basic (&hash_iter, &double_val);
                  key = g_memdup (&double_val, sizeof (gdouble));
                  break;

                case DBUS_TYPE_STRING:
                  dbus_message_iter_get_basic (&hash_iter, &str_val);
                  key = g_strdup (str_val);
                  break;

                case DBUS_TYPE_OBJECT_PATH:
                  dbus_message_iter_get_basic (&hash_iter, &str_val);
                  key = g_strdup (str_val);
                  break;

                case DBUS_TYPE_SIGNATURE:
                  dbus_message_iter_get_basic (&hash_iter, &str_val);
                  key = g_strdup (str_val);
                  break;

                default:
                  g_assert_not_reached ();
                  break;
                }

              dbus_message_iter_next (&hash_iter);

              switch (val_sig[0])
                {
                case DBUS_TYPE_BOOLEAN:
                  dbus_message_iter_get_basic (&hash_iter, &bool_val);
                  value = GINT_TO_POINTER (bool_val);
                  break;

                case DBUS_TYPE_BYTE:
                  dbus_message_iter_get_basic (&hash_iter, &uint8_val);
                  value = GINT_TO_POINTER (uint8_val);
                  break;

                case DBUS_TYPE_INT16:
                  dbus_message_iter_get_basic (&hash_iter, &int16_val);
                  value = GINT_TO_POINTER (int16_val);
                  break;

                case DBUS_TYPE_UINT16:
                  dbus_message_iter_get_basic (&hash_iter, &uint16_val);
                  value = GINT_TO_POINTER (uint16_val);
                  break;

                case DBUS_TYPE_INT32:
                  dbus_message_iter_get_basic (&hash_iter, &int32_val);
                  value = GINT_TO_POINTER (int32_val);
                  break;

                case DBUS_TYPE_INT64:
                  dbus_message_iter_get_basic (&hash_iter, &int64_val);
                  value = g_memdup (&int64_val, sizeof (gint64));
                  break;

                case DBUS_TYPE_UINT64:
                  dbus_message_iter_get_basic (&hash_iter, &uint64_val);
                  value = g_memdup (&uint64_val, sizeof (guint64));
                  break;

                case DBUS_TYPE_DOUBLE:
                  dbus_message_iter_get_basic (&hash_iter, &double_val);
                  value = g_memdup (&double_val, sizeof (gdouble));
                  break;

                case DBUS_TYPE_UINT32:
                  dbus_message_iter_get_basic (&hash_iter, &uint32_val);
                  value = GINT_TO_POINTER (uint32_val);
                  break;

                case DBUS_TYPE_STRING:
                  dbus_message_iter_get_basic (&hash_iter, &str_val);
                  value = g_strdup (str_val);
                  break;

                case DBUS_TYPE_OBJECT_PATH:
                  dbus_message_iter_get_basic (&hash_iter, &str_val);
                  value = g_strdup (str_val);
                  break;

                case DBUS_TYPE_SIGNATURE:
                  dbus_message_iter_get_basic (&hash_iter, &str_val);
                  value = g_strdup (str_val);
                  break;

                case DBUS_TYPE_ARRAY:
                  {
                    GValue array_val = {0};
                    /* recurse */
                    if (!g_dbus_c_type_mapping_get_value_from_iter (&hash_iter, &array_val, error))
                      goto out;
                    switch (val_sig[1])
                      {
                      case DBUS_TYPE_BOOLEAN:
                      case DBUS_TYPE_BYTE:
                      case DBUS_TYPE_INT16:
                      case DBUS_TYPE_UINT16:
                      case DBUS_TYPE_INT32:
                      case DBUS_TYPE_UINT32:
                      case DBUS_TYPE_INT64:
                      case DBUS_TYPE_UINT64:
                      case DBUS_TYPE_DOUBLE:
                      case DBUS_STRUCT_BEGIN_CHAR:
                      case DBUS_DICT_ENTRY_BEGIN_CHAR:
                      case DBUS_TYPE_STRING:
                      case DBUS_TYPE_OBJECT_PATH:
                      case DBUS_TYPE_SIGNATURE:
                      case DBUS_TYPE_VARIANT:
                      case DBUS_TYPE_ARRAY:
                        value = g_value_get_boxed (&array_val);
                        break;

                      default:
                        g_assert_not_reached ();
                        break;
                      }
                  }
                  break;

                case DBUS_STRUCT_BEGIN_CHAR:
                  {
                    GValue object_val = {0};
                    /* recurse */
                    if (!g_dbus_c_type_mapping_get_value_from_iter (&hash_iter, &object_val, error))
                      goto out;
                    value = g_value_get_object (&object_val);
                  }
                  break;

                case DBUS_TYPE_VARIANT:
                  {
                    GValue object_val = {0};
                    /* recurse */
                    if (!g_dbus_c_type_mapping_get_value_from_iter (&hash_iter, &object_val, error))
                      goto out;
                    value = g_value_get_object (&object_val);
                  }
                  break;

                default:
                  g_assert_not_reached ();
                  break;
                }

              g_hash_table_insert (hash, key, value);
              dbus_message_iter_next (&array_iter);
            }

          g_value_init (out_value, G_TYPE_HASH_TABLE);
          g_value_take_boxed (out_value, hash);

          dbus_free (array_sig);
          g_free (val_sig);

        }
      else if (array_arg_type == DBUS_TYPE_ARRAY)
        {
          GPtrArray *p;
          DBusMessageIter array_iter;
          GDestroyNotify elem_free_func;

          /* handling array of arrays, e.g.
           *
           * - aas:    GPtrArray of GStrv (gchar **)
           * - aao:    GPtrArray of GStrv (gchar **) TODO:o
           * - aao:    GPtrArray of GStrv (gchar **) TODO:g
           * - aa{ss}: GPtrArray of GHashTable
           * - aai:    GPtrArray of GArray
           * - aa(ii): GPtrArray of GPtrArray containg GObject-derived type
           * - aaas:   GPtrArray of GPtrArray of GStrv (gchar **)
           *
           */

          dbus_message_iter_recurse (iter, &array_iter);

          elem_free_func = NULL;

          p = g_ptr_array_new ();
          while (dbus_message_iter_get_arg_type (&array_iter) != DBUS_TYPE_INVALID)
            {
              GValue elem_val = {0};

              /* recurse */
              if (!g_dbus_c_type_mapping_get_value_from_iter (&array_iter, &elem_val, error))
                goto out;

              if (G_VALUE_TYPE (&elem_val) == G_TYPE_STRV)
                {
                  g_ptr_array_add (p, g_value_get_boxed (&elem_val));
                  elem_free_func = (GDestroyNotify) g_strfreev;
                }
              else if (G_VALUE_TYPE (&elem_val) == G_TYPE_ARRAY)
                {
                  g_ptr_array_add (p, g_value_get_boxed (&elem_val));
                  elem_free_func = (GDestroyNotify) g_array_unref;
                }
              else if (G_VALUE_TYPE (&elem_val) == G_TYPE_PTR_ARRAY)
                {
                  g_ptr_array_add (p, g_value_get_boxed (&elem_val));
                  elem_free_func = (GDestroyNotify) g_ptr_array_unref;
                }
              else if (G_VALUE_TYPE (&elem_val) == G_TYPE_HASH_TABLE)
                {
                  g_ptr_array_add (p, g_value_get_boxed (&elem_val));
                  elem_free_func = (GDestroyNotify) g_hash_table_unref;
                }
              else
                {
                  g_assert_not_reached ();
                }

              dbus_message_iter_next (&array_iter);
            } /* for all elements in array */
          g_ptr_array_set_free_func (p, elem_free_func);
          g_value_init (out_value, G_TYPE_PTR_ARRAY);
          g_value_take_boxed (out_value, p);
        }
      else if (array_arg_type == DBUS_TYPE_VARIANT)
        {
          GPtrArray *p;
          DBusMessageIter array_iter;
          char *elem_signature;

          /* array of variants */

          dbus_message_iter_recurse (iter, &array_iter);

          elem_signature = dbus_message_iter_get_signature (&array_iter);

          p = g_ptr_array_new_with_free_func (g_object_unref);
          while (dbus_message_iter_get_arg_type (&array_iter) != DBUS_TYPE_INVALID)
            {
              GValue elem_val = {0};

              /* recurse */
              if (!g_dbus_c_type_mapping_get_value_from_iter (&array_iter, &elem_val, error))
                goto out;

              g_ptr_array_add (p, g_value_get_object (&elem_val));

              dbus_message_iter_next (&array_iter);
            } /* for all elements in array */

          g_value_init (out_value, G_TYPE_PTR_ARRAY);
          g_value_take_boxed (out_value, p);

          dbus_free (elem_signature);
        }
      else
        {
          char *signature;
          signature = dbus_message_iter_get_signature (iter);
          g_set_error (error,
                       G_DBUS_ERROR,
                       G_DBUS_ERROR_FAILED,
                       "Cannot decode message with array of signature %s", signature);
          g_free (signature);
          goto out;
        }
      break;

    case DBUS_TYPE_VARIANT:
      {
        GValue variant_val = {0};
        GDBusVariant *variant;
        DBusMessageIter variant_iter;
        gchar *variant_signature;

        dbus_message_iter_recurse (iter, &variant_iter);

        variant_signature = dbus_message_iter_get_signature (&variant_iter);

        /* recurse */
        if (!g_dbus_c_type_mapping_get_value_from_iter (&variant_iter, &variant_val, error))
          goto out;

        variant = _g_dbus_variant_new_for_gvalue (&variant_val, variant_signature);

        g_value_init (out_value, G_TYPE_DBUS_VARIANT);
        g_value_take_object (out_value, variant);

        dbus_free (variant_signature);
      }
      break;

    default:
      {
        char *signature;
        signature = dbus_message_iter_get_signature (iter);
        g_set_error (error,
                     G_DBUS_ERROR,
                     G_DBUS_ERROR_FAILED,
                     "Cannot decode message with signature %s", signature);
        dbus_free (signature);
        goto out;
      }
      break;
    }

  ret = TRUE;

 out:

  return ret;
}

/* ---------------------------------------------------------------------------------------------------- */

/**
 * g_dbus_c_type_mapping_append_value_to_iter:
 * @iter: A #DBusMessageIter.
 * @signature: A D-Bus signature corresponding to @value.
 * @value: A #GValue with the value to set
 * @error: Return location for error or %NULL.
 *
 * Appens @value to @iter.
 *
 * Returns: %TRUE on success, %FALSE if @error is set.
 **/
gboolean
g_dbus_c_type_mapping_append_value_to_iter (DBusMessageIter   *iter,
                                            const gchar       *signature,
                                            const GValue      *value,
                                            GError           **error)
{
  gboolean ret;

  /* TODO: - fill in @error instead of using g_assert_not_reached()
   *       - validate object paths and signatures (to shield app developers
   *         from getting disconnected from the bus)
   *       - perhaps access GValue directly (trading off safety for performance)
   */

  //g_debug ("_append_value_to_iter: signature=%s -> value=%s", signature, g_strdup_value_contents (value));

  ret = FALSE;

  /* TODO: we could probably speed this up by accessing the GValue directly */
  switch (signature[0])
    {
    case DBUS_TYPE_STRING:
      {
        const char *val = g_value_get_string (value);
        dbus_message_iter_append_basic (iter, DBUS_TYPE_STRING, &val);
      }
      break;

    case DBUS_TYPE_OBJECT_PATH:
      {
        const char *val = g_value_get_string (value); // TODO:o
        dbus_message_iter_append_basic (iter, DBUS_TYPE_OBJECT_PATH, &val);
      }
      break;

    case DBUS_TYPE_SIGNATURE:
      {
        const char *val = g_value_get_string (value); // TODO:g
        dbus_message_iter_append_basic (iter, DBUS_TYPE_SIGNATURE, &val);
      }
      break;

    case DBUS_TYPE_BYTE:
      {
        guchar val = g_value_get_uchar (value);
        dbus_message_iter_append_basic (iter, DBUS_TYPE_BYTE, &val);
      }
      break;

    case DBUS_TYPE_BOOLEAN:
      {
        dbus_bool_t val = g_value_get_boolean (value);
        dbus_message_iter_append_basic (iter, DBUS_TYPE_BOOLEAN, &val);
      }
      break;

    case DBUS_TYPE_INT16:
      {
        dbus_int16_t val = g_value_get_int (value); // TODO:16
        dbus_message_iter_append_basic (iter, DBUS_TYPE_INT16, &val);
      }
      break;

    case DBUS_TYPE_UINT16:
      {
        dbus_uint16_t val = g_value_get_uint (value); // TODO:16
        dbus_message_iter_append_basic (iter, DBUS_TYPE_UINT16, &val);
      }
      break;

    case DBUS_TYPE_INT32:
      {
        dbus_int32_t val = g_value_get_int (value);
        dbus_message_iter_append_basic (iter, DBUS_TYPE_INT32, &val);
      }
      break;

    case DBUS_TYPE_UINT32:
      {
        dbus_uint32_t val;
        if (value->g_type == G_TYPE_UINT)
          {
            val = g_value_get_uint (value);
          }
        else if (G_TYPE_IS_ENUM (value->g_type))
          {
            val = g_value_get_enum (value);
          }
        else if (G_TYPE_IS_FLAGS (value->g_type))
          {
            val = g_value_get_flags (value);
          }
        dbus_message_iter_append_basic (iter, DBUS_TYPE_UINT32, &val);
      }
      break;

    case DBUS_TYPE_INT64:
      {
        dbus_int64_t val = g_value_get_int64 (value);
        dbus_message_iter_append_basic (iter, DBUS_TYPE_INT64, &val);
      }
      break;

    case DBUS_TYPE_UINT64:
      {
        dbus_uint64_t val = g_value_get_uint64 (value);
        dbus_message_iter_append_basic (iter, DBUS_TYPE_UINT64, &val);
      }
      break;

    case DBUS_TYPE_DOUBLE:
      {
        double val = g_value_get_double (value);
        dbus_message_iter_append_basic (iter, DBUS_TYPE_DOUBLE, &val);
      }
      break;

    /* ---------------------------------------------------------------------------------------------------- */
    case DBUS_TYPE_ARRAY:
      if (signature[1] == DBUS_TYPE_BYTE   ||
          signature[1] == DBUS_TYPE_INT16  ||
          signature[1] == DBUS_TYPE_UINT16 ||
          signature[1] == DBUS_TYPE_INT32  ||
          signature[1] == DBUS_TYPE_UINT32 ||
          signature[1] == DBUS_TYPE_INT64  ||
          signature[1] == DBUS_TYPE_UINT64 ||
          signature[1] == DBUS_TYPE_DOUBLE ||
          signature[1] == DBUS_TYPE_BOOLEAN)
        {
          DBusMessageIter array_iter;
          GArray *a;

          a = (GArray *) g_value_get_boxed (value);

          if (a != NULL && a->len > 0 && get_element_size (signature[1]) != g_array_get_element_size (a))
            {
              g_set_error (error,
                           G_DBUS_ERROR,
                           G_DBUS_ERROR_FAILED,
                           "GArray has element size %d but element size %d was expected",
                           get_element_size (signature[1]),
                           g_array_get_element_size (a));
              goto out;
            }

          dbus_message_iter_open_container (iter, DBUS_TYPE_ARRAY, signature + 1, &array_iter);
          dbus_message_iter_append_fixed_array (&array_iter, signature[1], &(a->data), a->len);
          dbus_message_iter_close_container (iter, &array_iter);
        }
      else if (signature[1] == DBUS_TYPE_STRING ||
               signature[1] == DBUS_TYPE_OBJECT_PATH ||
               signature[1] == DBUS_TYPE_SIGNATURE)
        {
          DBusMessageIter array_iter;
          gchar **strv;
          guint n;

          strv = (gchar **) g_value_get_boxed (value);
          dbus_message_iter_open_container (iter, DBUS_TYPE_ARRAY, signature + 1, &array_iter);
          for (n = 0; strv[n] != NULL; n++)
            dbus_message_iter_append_basic (&array_iter, signature[1], &(strv[n]));
          dbus_message_iter_close_container (iter, &array_iter);
        }
      else if (signature[1] == DBUS_STRUCT_BEGIN_CHAR)
        {
          DBusMessageIter array_iter;
          GPtrArray *p;
          guint n;

          p = g_value_get_boxed (value);
          dbus_message_iter_open_container (iter, DBUS_TYPE_ARRAY, signature + 1, &array_iter);
          for (n = 0; n < p->len; n++)
            {
              GValue val = {0};
              g_value_init (&val, G_TYPE_DBUS_STRUCTURE);
              g_value_take_object (&val, G_DBUS_STRUCTURE (p->pdata[n]));
              /* recursive dance */
              if (!g_dbus_c_type_mapping_append_value_to_iter (&array_iter, signature + 1, &val, error))
                goto out;
            }
          dbus_message_iter_close_container (iter, &array_iter);
        }
      else if (signature[1] == DBUS_DICT_ENTRY_BEGIN_CHAR)
        {
          DBusMessageIter array_iter;
          GHashTable *hash_table;
          GHashTableIter hash_iter;
          gpointer hash_key;
          gpointer hash_value;
          char *value_signature;

          value_signature = g_strdup (signature + 3);
          value_signature[strlen (value_signature) - 1] = '\0';

          hash_table = g_value_get_boxed (value);

          //g_debug ("signature='%s' value_signature='%s'", signature, value_signature);

          dbus_message_iter_open_container (iter, DBUS_TYPE_ARRAY, signature + 1, &array_iter);
          g_hash_table_iter_init (&hash_iter, hash_table);
          while (g_hash_table_iter_next (&hash_iter, &hash_key, &hash_value))
            {
              GValue val = {0};
              DBusMessageIter dict_iter;

              dbus_message_iter_open_container (&array_iter,
                                                DBUS_TYPE_DICT_ENTRY,
                                                NULL,
                                                &dict_iter);
              switch (signature[2])
                {
                case DBUS_TYPE_STRING:
                case DBUS_TYPE_OBJECT_PATH:
                case DBUS_TYPE_SIGNATURE:
                case DBUS_TYPE_BYTE:
                case DBUS_TYPE_BOOLEAN:
                case DBUS_TYPE_INT16:
                case DBUS_TYPE_UINT16:
                case DBUS_TYPE_INT32:
                case DBUS_TYPE_UINT32:
                  dbus_message_iter_append_basic (&dict_iter, signature[2], &hash_key);
                  break;

                case DBUS_TYPE_INT64:
                case DBUS_TYPE_UINT64:
                case DBUS_TYPE_DOUBLE:
                  dbus_message_iter_append_basic (&dict_iter, signature[2], hash_key);
                  break;

                default:
                  /* the D-Bus spec says the hash_key must be a basic type */
                  g_assert_not_reached ();
                  break;
                }

              //g_debug ("value_signature='%s'", value_signature);
              switch (value_signature[0])
                {
                case DBUS_TYPE_STRING:
                case DBUS_TYPE_OBJECT_PATH:
                case DBUS_TYPE_SIGNATURE:
                case DBUS_TYPE_BYTE:
                case DBUS_TYPE_BOOLEAN:
                case DBUS_TYPE_INT16:
                case DBUS_TYPE_UINT16:
                case DBUS_TYPE_INT32:
                case DBUS_TYPE_UINT32:
                  dbus_message_iter_append_basic (&dict_iter, signature[2], &hash_value);
                  break;

                case DBUS_TYPE_INT64:
                case DBUS_TYPE_UINT64:
                case DBUS_TYPE_DOUBLE:
                  dbus_message_iter_append_basic (&dict_iter, signature[2], hash_value);
                  break;

                case DBUS_STRUCT_BEGIN_CHAR:
                  g_value_init (&val, G_TYPE_DBUS_STRUCTURE);
                  g_value_take_object (&val, G_DBUS_STRUCTURE (hash_value));
                  /* recursive dance */
                  if (!g_dbus_c_type_mapping_append_value_to_iter (&dict_iter, value_signature, &val, error))
                    goto out;
                  break;

                case DBUS_TYPE_ARRAY:
                  if (value_signature[1] == DBUS_TYPE_BYTE ||
                      value_signature[1] == DBUS_TYPE_INT16 ||
                      value_signature[1] == DBUS_TYPE_UINT16 ||
                      value_signature[1] == DBUS_TYPE_INT32 ||
                      value_signature[1] == DBUS_TYPE_UINT32 ||
                      value_signature[1] == DBUS_TYPE_INT64 ||
                      value_signature[1] == DBUS_TYPE_UINT64 ||
                      value_signature[1] == DBUS_TYPE_BOOLEAN ||
                      value_signature[1] == DBUS_TYPE_DOUBLE)
                    {
                      g_value_init (&val, G_TYPE_ARRAY);
                      g_value_take_boxed (&val, hash_value);
                      /* recurse */
                      if (!g_dbus_c_type_mapping_append_value_to_iter (&dict_iter, value_signature, &val, error))
                        goto out;
                    }
                  else if (value_signature[1] == DBUS_TYPE_STRING ||
                           value_signature[1] == DBUS_TYPE_OBJECT_PATH ||
                           value_signature[1] == DBUS_TYPE_SIGNATURE)
                    {
                      GType boxed_type;

                      if (value_signature[1] == DBUS_TYPE_STRING)
                        boxed_type = G_TYPE_STRV;
                      else if (value_signature[1] == DBUS_TYPE_OBJECT_PATH)
                        boxed_type = G_TYPE_STRV; // TODO:o EGG_DBUS_TYPE_OBJECT_PATH_ARRAY;
                      else if (value_signature[1] == DBUS_TYPE_SIGNATURE)
                        boxed_type = G_TYPE_STRV; // TODO:o EGG_DBUS_TYPE_SIGNATURE_ARRAY;
                      else
                        g_assert_not_reached ();

                      g_value_init (&val, boxed_type);
                      g_value_set_static_boxed (&val, hash_value);
                      /* recurse */
                      if (!g_dbus_c_type_mapping_append_value_to_iter (&dict_iter, value_signature, &val, error))
                        goto out;
                    }
                  else if (value_signature[1] == DBUS_STRUCT_BEGIN_CHAR)
                    {
                      g_value_init (&val, G_TYPE_PTR_ARRAY);
                      g_value_take_boxed (&val, hash_value);
                      /* recurse */
                      if (!g_dbus_c_type_mapping_append_value_to_iter (&dict_iter, value_signature, &val, error))
                        goto out;
                    }
                  else if (value_signature[1] == DBUS_DICT_ENTRY_BEGIN_CHAR)
                    {
                      g_value_init (&val, G_TYPE_HASH_TABLE);
                      g_value_take_boxed (&val, hash_value);
                      /* recurse */
                      if (!g_dbus_c_type_mapping_append_value_to_iter (&dict_iter, value_signature, &val, error))
                        goto out;
                    }
                  else if (value_signature[1] == DBUS_TYPE_VARIANT)
                    {
                      g_value_init (&val, G_TYPE_PTR_ARRAY);
                      g_value_take_boxed (&val, hash_value);
                      /* recurse */
                      if (!g_dbus_c_type_mapping_append_value_to_iter (&dict_iter, value_signature, &val, error))
                        goto out;
                    }
                  else if (value_signature[1] == DBUS_TYPE_ARRAY)
                    {
                      g_value_init (&val, G_TYPE_PTR_ARRAY);
                      g_value_take_boxed (&val, hash_value);
                      /* recurse */
                      if (!g_dbus_c_type_mapping_append_value_to_iter (&dict_iter, value_signature, &val, error))
                        goto out;
                    }
                  else
                    {
                      g_assert_not_reached ();
                    }
                  break;

                case DBUS_TYPE_VARIANT:
                  g_value_init (&val, G_TYPE_DBUS_VARIANT);
                  g_value_take_object (&val, hash_value);
                  /* recurse */
                  if (!g_dbus_c_type_mapping_append_value_to_iter (&dict_iter, value_signature, &val, error))
                    goto out;
                  break;

                default:
                  g_assert_not_reached ();
                  break;
                }

              dbus_message_iter_close_container (&array_iter, &dict_iter);
            }
          dbus_message_iter_close_container (iter, &array_iter);

          g_free (value_signature);
        }
      else if (signature[1] == DBUS_TYPE_ARRAY) /* array of an array */
        {
          DBusMessageIter array_iter;
          GPtrArray *p;
          guint n;

          p = g_value_get_boxed (value);
          dbus_message_iter_open_container (iter, DBUS_TYPE_ARRAY, signature + 1, &array_iter);
          for (n = 0; n < p->len; n++)
            {
              GValue val = {0};

              g_value_init (&val, _g_dbus_signature_get_gtype (signature + 1));

              /* this can either be boxed (for GStrv, GArray) or an object (for e.g. structs, variants) */
              if (g_type_is_a (G_VALUE_TYPE (&val), G_TYPE_BOXED))
                g_value_take_boxed (&val, p->pdata[n]);
              else if (g_type_is_a (G_VALUE_TYPE (&val), G_TYPE_OBJECT))
                g_value_take_object (&val, p->pdata[n]);
              else
                g_assert_not_reached ();

              /* recursive dance */
              if (!g_dbus_c_type_mapping_append_value_to_iter (&array_iter, signature + 1, &val, error))
                goto out;
            }

          dbus_message_iter_close_container (iter, &array_iter);
        }
      else if (signature[1] == DBUS_TYPE_VARIANT ) /* array of variants */
        {
          DBusMessageIter array_iter;
          GPtrArray *p;
          guint n;

          p = g_value_get_boxed (value);
          dbus_message_iter_open_container (iter, DBUS_TYPE_ARRAY, signature + 1, &array_iter);
          for (n = 0; n < p->len; n++)
            {
              GValue val = {0};

              g_value_init (&val, G_TYPE_DBUS_VARIANT);
              g_value_take_object (&val, p->pdata[n]);

              /* recursive dance */
              if (!g_dbus_c_type_mapping_append_value_to_iter (&array_iter, signature + 1, &val, error))
                goto out;
            }

          dbus_message_iter_close_container (iter, &array_iter);
        }
      else
        {
          g_warning ("Don't know append an array with elements with with signature %s", signature + 1);
          g_assert_not_reached ();
        }
      break;

    /* ---------------------------------------------------------------------------------------------------- */
    case DBUS_STRUCT_BEGIN_CHAR:
      {
        DBusMessageIter struct_iter;
        GDBusStructure *structure;
        guint n;
        guint num_elems;

        structure = G_DBUS_STRUCTURE (g_value_get_object (value));

        num_elems = g_dbus_structure_get_num_elements (structure);

        dbus_message_iter_open_container (iter, DBUS_TYPE_STRUCT, NULL, &struct_iter);
        for (n = 0; n < num_elems; n++)
          {
            const GValue *val;
            const gchar *sig_for_elem;

            val = _g_dbus_structure_get_gvalue_for_element (structure, n);
            sig_for_elem = g_dbus_structure_get_signature_for_element (structure, n);
            /* recurse */
            if (!g_dbus_c_type_mapping_append_value_to_iter (&struct_iter, sig_for_elem, val, error))
              goto out;
          }
        dbus_message_iter_close_container (iter, &struct_iter);
      }
      break;

    /* ---------------------------------------------------------------------------------------------------- */
    case DBUS_TYPE_VARIANT:
      {
        GDBusVariant *variant;
        DBusMessageIter variant_iter;
        const gchar *variant_signature;
        GValue val = {0};

        g_value_init (&val, G_TYPE_DBUS_VARIANT);
        variant = g_value_get_object (value);

        variant_signature = g_dbus_variant_get_variant_signature (variant);

        dbus_message_iter_open_container (iter, DBUS_TYPE_VARIANT, variant_signature, &variant_iter);
        /* recurse */
        if (!g_dbus_c_type_mapping_append_value_to_iter (&variant_iter,
                                                         variant_signature,
                                                         _g_dbus_variant_get_gvalue (variant),
                                                         error))
          goto out;
        dbus_message_iter_close_container (iter, &variant_iter);
      }
      break;

    default:
      g_warning ("Don't know append a value with signature %s", signature);
      g_assert_not_reached ();
      break;
    }

  ret = TRUE;

 out:
  return ret;
}


#define __G_DBUS_C_TYPE_MAPPING_C__
#include "gdbusaliasdef.c"
