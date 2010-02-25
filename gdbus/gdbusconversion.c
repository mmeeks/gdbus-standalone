/* GDBus - GLib D-Bus Library
 *
 * Copyright © 2007, 2008  Ryan Lortie
 * Copyright © 2009 Codethink Limited
 * Copyright © 2008-2009 Red Hat, Inc.
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
 * Author: Ryan Lortie <desrt@desrt.ca>
 *         David Zeuthen <davidz@redhat.com>
 */

#include "config.h"

#include <stdlib.h>

#include <glib/gi18n.h>

#include "gdbusconversion.h"
#include "gdbuserror.h"
#include "gdbusenums.h"
#include "gdbusprivate.h"

static gboolean
dconf_dbus_from_gv (DBusMessageIter  *iter,
                    GVariant         *value,
                    GError          **error)
{
  switch (g_variant_get_type_class (value))
    {
     case G_VARIANT_CLASS_BOOLEAN:
      {
        dbus_bool_t v = g_variant_get_boolean (value);
        dbus_message_iter_append_basic (iter, DBUS_TYPE_BOOLEAN, &v);
        break;
      }

     case G_VARIANT_CLASS_BYTE:
      {
        guint8 v = g_variant_get_byte (value);
        dbus_message_iter_append_basic (iter, DBUS_TYPE_BYTE, &v);
        break;
      }

     case G_VARIANT_CLASS_INT16:
      {
        gint16 v = g_variant_get_int16 (value);
        dbus_message_iter_append_basic (iter, DBUS_TYPE_INT16, &v);
        break;
      }

     case G_VARIANT_CLASS_UINT16:
      {
        guint16 v = g_variant_get_uint16 (value);
        dbus_message_iter_append_basic (iter, DBUS_TYPE_UINT16, &v);
        break;
      }

     case G_VARIANT_CLASS_INT32:
      {
        gint32 v = g_variant_get_int32 (value);
        dbus_message_iter_append_basic (iter, DBUS_TYPE_INT32, &v);
        break;
      }

     case G_VARIANT_CLASS_UINT32:
      {
        guint32 v = g_variant_get_uint32 (value);
        dbus_message_iter_append_basic (iter, DBUS_TYPE_UINT32, &v);
        break;
      }

     case G_VARIANT_CLASS_INT64:
      {
        gint64 v = g_variant_get_int64 (value);
        dbus_message_iter_append_basic (iter, DBUS_TYPE_INT64, &v);
        break;
      }

     case G_VARIANT_CLASS_UINT64:
      {
        guint64 v = g_variant_get_uint64 (value);
        dbus_message_iter_append_basic (iter, DBUS_TYPE_UINT64, &v);
        break;
      }

     case G_VARIANT_CLASS_DOUBLE:
      {
        gdouble v = g_variant_get_double (value);
        dbus_message_iter_append_basic (iter, DBUS_TYPE_DOUBLE, &v);
        break;
      }

     case G_VARIANT_CLASS_STRING:
      {
        const gchar *v = g_variant_get_string (value, NULL);
        dbus_message_iter_append_basic (iter, DBUS_TYPE_STRING, &v);
        break;
      }

     case G_VARIANT_CLASS_OBJECT_PATH:
      {
        const gchar *v = g_variant_get_string (value, NULL);
        dbus_message_iter_append_basic (iter, DBUS_TYPE_OBJECT_PATH, &v);
        break;
      }

     case G_VARIANT_CLASS_SIGNATURE:
      {
        const gchar *v = g_variant_get_string (value, NULL);
        dbus_message_iter_append_basic (iter, DBUS_TYPE_SIGNATURE, &v);
        break;
      }

     case G_VARIANT_CLASS_VARIANT:
      {
        DBusMessageIter sub;
        GVariant *child;

        child = g_variant_get_child_value (value, 0);
        dbus_message_iter_open_container (iter, DBUS_TYPE_VARIANT,
                                          g_variant_get_type_string (child),
                                          &sub);
        if (!dconf_dbus_from_gv (&sub, child, error))
          {
            g_variant_unref (child);
            goto fail;
          }
        dbus_message_iter_close_container (iter, &sub);
        g_variant_unref (child);
        break;
      }

     case G_VARIANT_CLASS_ARRAY:
      {
        DBusMessageIter dbus_iter;
        const gchar *type_string;
        GVariantIter gv_iter;
        GVariant *item;

        type_string = g_variant_get_type_string (value);
        type_string++; /* skip the 'a' */

        dbus_message_iter_open_container (iter, DBUS_TYPE_ARRAY,
                                          type_string, &dbus_iter);
        g_variant_iter_init (&gv_iter, value);

        while ((item = g_variant_iter_next_value (&gv_iter)))
          {
            if (!dconf_dbus_from_gv (&dbus_iter, item, error))
              {
                goto fail;
              }
          }

        dbus_message_iter_close_container (iter, &dbus_iter);
        break;
      }

     case G_VARIANT_CLASS_TUPLE:
      {
        DBusMessageIter dbus_iter;
        GVariantIter gv_iter;
        GVariant *item;

        dbus_message_iter_open_container (iter, DBUS_TYPE_STRUCT,
                                          NULL, &dbus_iter);
        g_variant_iter_init (&gv_iter, value);

        while ((item = g_variant_iter_next_value (&gv_iter)))
          {
            if (!dconf_dbus_from_gv (&dbus_iter, item, error))
              goto fail;
          }

        dbus_message_iter_close_container (iter, &dbus_iter);
        break;
      }

     case G_VARIANT_CLASS_DICT_ENTRY:
      {
        DBusMessageIter dbus_iter;
        GVariant *key, *val;

        dbus_message_iter_open_container (iter, DBUS_TYPE_DICT_ENTRY,
                                          NULL, &dbus_iter);
        key = g_variant_get_child_value (value, 0);
        if (!dconf_dbus_from_gv (&dbus_iter, key, error))
          {
            g_variant_unref (key);
            goto fail;
          }
        g_variant_unref (key);

        val = g_variant_get_child_value (value, 1);
        if (!dconf_dbus_from_gv (&dbus_iter, val, error))
          {
            g_variant_unref (val);
            goto fail;
          }
        g_variant_unref (val);

        dbus_message_iter_close_container (iter, &dbus_iter);
        break;
      }

     default:
       g_set_error (error,
                    G_DBUS_ERROR,
                    G_DBUS_ERROR_CONVERSION_FAILED,
                    _("Error serializing GVariant with type-string `%s' to a D-Bus message"),
                    g_variant_get_type_string (value));
       goto fail;
    }

  return TRUE;

 fail:
  return FALSE;
}

static GVariant *
dconf_dbus_to_gv (DBusMessageIter  *iter,
                  GError          **error)
{
  gint arg_type;

  arg_type = dbus_message_iter_get_arg_type (iter);

  switch (dbus_message_iter_get_arg_type (iter))
    {
     case DBUS_TYPE_BOOLEAN:
      {
        dbus_bool_t value;
        dbus_message_iter_get_basic (iter, &value);
        return g_variant_new_boolean (value);
      }

     case DBUS_TYPE_BYTE:
      {
        guchar value;
        dbus_message_iter_get_basic (iter, &value);
        return g_variant_new_byte (value);
      }

     case DBUS_TYPE_INT16:
      {
        gint16 value;
        dbus_message_iter_get_basic (iter, &value);
        return g_variant_new_int16 (value);
      }

     case DBUS_TYPE_UINT16:
      {
        guint16 value;
        dbus_message_iter_get_basic (iter, &value);
        return g_variant_new_uint16 (value);
      }

     case DBUS_TYPE_INT32:
      {
        gint32 value;
        dbus_message_iter_get_basic (iter, &value);
        return g_variant_new_int32 (value);
      }

     case DBUS_TYPE_UINT32:
      {
        guint32 value;
        dbus_message_iter_get_basic (iter, &value);
        return g_variant_new_uint32 (value);
      }

     case DBUS_TYPE_INT64:
      {
        gint64 value;
        dbus_message_iter_get_basic (iter, &value);
        return g_variant_new_int64 (value);
      }

     case DBUS_TYPE_UINT64:
      {
        guint64 value;
        dbus_message_iter_get_basic (iter, &value);
        return g_variant_new_uint64 (value);
      }

     case DBUS_TYPE_DOUBLE:
      {
        gdouble value;
        dbus_message_iter_get_basic (iter, &value);
        return g_variant_new_double (value);
      }

     case DBUS_TYPE_STRING:
      {
       const gchar *value;
       dbus_message_iter_get_basic (iter, &value);
       return g_variant_new_string (value);
      }

     case DBUS_TYPE_OBJECT_PATH:
      {
       const gchar *value;
       dbus_message_iter_get_basic (iter, &value);
       return g_variant_new_object_path (value);
      }

     case DBUS_TYPE_SIGNATURE:
      {
       const gchar *value;
       dbus_message_iter_get_basic (iter, &value);
       return g_variant_new_signature (value);
      }

     case DBUS_TYPE_VARIANT:
       {
        GVariantBuilder *builder;
        GVariantClass class;
        DBusMessageIter sub;
        char *type;
        GVariant *val;

        dbus_message_iter_recurse (iter, &sub);
        class = dbus_message_iter_get_arg_type (iter);
        type = dbus_message_iter_get_signature (&sub);
//        builder = g_variant_builder_new (class, G_VARIANT_TYPE (type));
        builder = g_variant_builder_new (G_VARIANT_TYPE (type));
        dbus_free (type);

        while (dbus_message_iter_get_arg_type (&sub))
          {
            val = dconf_dbus_to_gv (&sub, error);
            if (val == NULL)
              {
                g_variant_builder_cancel (builder);
                goto fail;
              }
            g_variant_builder_add_value (builder, val);
            dbus_message_iter_next (&sub);
          }

        return g_variant_builder_end (builder);
       }

     case DBUS_TYPE_ARRAY:
     case DBUS_TYPE_STRUCT:
     case DBUS_TYPE_DICT_ENTRY:
      {
        GVariantBuilder *builder;
        GVariantClass class;
        DBusMessageIter sub;
        char *type;
        GVariant *val;

        dbus_message_iter_recurse (iter, &sub);
        class = dbus_message_iter_get_arg_type (iter);
        type = dbus_message_iter_get_signature (iter);
//        builder = g_variant_builder_new (class, G_VARIANT_TYPE (type));
        builder = g_variant_builder_new (G_VARIANT_TYPE (type));
        dbus_free (type);

        while (dbus_message_iter_get_arg_type (&sub))
          {
            val = dconf_dbus_to_gv (&sub, error);
            if (val == NULL)
              {
                g_variant_builder_cancel (builder);
                goto fail;
              }
            g_variant_builder_add_value (builder, val);
            dbus_message_iter_next (&sub);
          }

        return g_variant_builder_end (builder);
      }

     default:
       g_set_error (error,
                    G_DBUS_ERROR,
                    G_DBUS_ERROR_CONVERSION_FAILED,
                    _("Error serializing D-Bus message to GVariant. Unsupported arg type `%c' (%d)"),
                    arg_type,
                    arg_type);
      goto fail;
    }

  g_assert_not_reached ();

 fail:
  return NULL;
}

/**
 * _g_dbus_dbus_1_to_gvariant:
 * @message: A #DBusMessage
 * @error: Return location for error or %NULL.
 *
 * If @message is an error message (cf. DBUS_MESSAGE_TYPE_ERROR), sets
 * @error with the contents of the error using
 * g_dbus_error_set_dbus_error().
 *
 * Otherwise build a #GVariant with the message (this never fails).
 *
 * Returns: A #GVariant or %NULL if @error is set.
 **/
GVariant *
_g_dbus_dbus_1_to_gvariant (DBusMessage  *message,
                            GError      **error)
{
  DBusMessageIter iter;
  GVariantBuilder *builder;
  guint n;
  GVariant *result;
  DBusError dbus_error;

  g_assert (message != NULL);

  result = NULL;

  dbus_error_init (&dbus_error);
  if (dbus_set_error_from_message (&dbus_error, message))
    {
      g_dbus_error_set_dbus_error (error,
                                   dbus_error.name,
                                   dbus_error.message,
                                   NULL);
      dbus_error_free (&dbus_error);
      goto out;
    }

  dbus_message_iter_init (message, &iter);

  builder = g_variant_builder_new (G_VARIANT_TYPE_TUPLE);
  n = 0;
  while (dbus_message_iter_get_arg_type (&iter) != DBUS_TYPE_INVALID)
    {
      GVariant *item;

      item = dconf_dbus_to_gv (&iter, error);
      if (item == NULL)
        {
          g_variant_builder_cancel (builder);
          g_prefix_error (error,
                          _("Error decoding out-arg %d: "),
                          n);
          goto out;
        }
      g_variant_builder_add_value (builder, item);
      dbus_message_iter_next (&iter);
    }

  result = g_variant_builder_end (builder);

 out:

  return result;
}

gboolean
_g_dbus_gvariant_to_dbus_1 (DBusMessage  *message,
                            GVariant     *value,
                            GError      **error)
{
  gboolean ret;
  guint n;

  ret = FALSE;

  if (value != NULL)
    {
      DBusMessageIter iter;
      GVariantIter gv_iter;
      GVariant *item;

      dbus_message_iter_init_append (message, &iter);

      g_variant_iter_init (&gv_iter, value);
      n = 0;
      while ((item = g_variant_iter_next_value (&gv_iter)))
        {
          if (!dconf_dbus_from_gv (&iter, item, error))
            {
              g_prefix_error (error,
                              _("Error encoding in-arg %d: "),
                              n);
              goto out;
            }
          n++;
        }
    }

  ret = TRUE;

 out:
  return ret;
}

