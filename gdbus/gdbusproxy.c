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

#include "gdbusproxy.h"
#include "gdbusenumtypes.h"
#include "gdbusconnection.h"
#include "gdbusconnection-lowlevel.h"
#include "gdbusctypemapping-lowlevel.h"
#include "gdbuserror.h"
#include "gdbusstructure.h"
#include "gdbusvariant.h"
#include "gbusnamewatcher.h"
#include "gdbus-marshal.h"
#include "gdbusprivate.h"

#include "gdbusalias.h"

/**
 * SECTION:gdbusproxy
 * @short_description: Base class for proxies
 * @include: gdbus/gdbus.h
 *
 * #GDBusProxy is a base class used for proxies to access a D-Bus
 * interface on a remote object.
 */

struct _GDBusProxyPrivate
{
  GDBusConnection *connection;
  GDBusProxyFlags flags;
  gchar *name;
  gchar *object_path;
  gchar *interface_name;
  gboolean properties_available;
  guint property_loading_pending_call_id;
  GHashTable *properties;

  GBusNameWatcher *watcher;

  gulong watcher_initialized_signal_handler_id;
  gulong watcher_name_appeared_signal_handler_id;
  gulong watcher_name_vanished_signal_handler_id;

  guint properties_changed_subscriber_id;
  guint signals_subscriber_id;
};

enum
{
  PROP_0,
  PROP_G_DBUS_PROXY_CONNECTION,
  PROP_G_DBUS_PROXY_NAME,
  PROP_G_DBUS_PROXY_NAME_OWNER,
  PROP_G_DBUS_PROXY_FLAGS,
  PROP_G_DBUS_PROXY_OBJECT_PATH,
  PROP_G_DBUS_PROXY_INTERFACE_NAME,
  PROP_G_DBUS_PROXY_PROPERTIES_AVAILABLE,
};

enum
{
  PROPERTIES_AVAILABLE_CHANGED_SIGNAL,
  PROPERTIES_CHANGED_SIGNAL,
  SIGNAL_SIGNAL, /* (!) */
  LAST_SIGNAL,
};

static void g_dbus_proxy_constructed (GObject *object);

guint signals[LAST_SIGNAL] = {0};

G_DEFINE_TYPE (GDBusProxy, g_dbus_proxy, G_TYPE_OBJECT);

static void
g_dbus_proxy_finalize (GObject *object)
{
  GDBusProxy *proxy = G_DBUS_PROXY (object);

  if (proxy->priv->watcher_initialized_signal_handler_id > 0)
    g_signal_handler_disconnect (proxy->priv->watcher, proxy->priv->watcher_initialized_signal_handler_id);
  if (proxy->priv->watcher_name_appeared_signal_handler_id > 0)
    g_signal_handler_disconnect (proxy->priv->watcher, proxy->priv->watcher_name_appeared_signal_handler_id);
  if (proxy->priv->watcher_name_vanished_signal_handler_id > 0)
    g_signal_handler_disconnect (proxy->priv->watcher, proxy->priv->watcher_name_vanished_signal_handler_id);
  g_object_unref (proxy->priv->watcher);

  if (proxy->priv->properties_changed_subscriber_id > 0)
    {
      g_dbus_connection_signal_unsubscribe (proxy->priv->connection,
                                            proxy->priv->properties_changed_subscriber_id);
    }

  if (proxy->priv->signals_subscriber_id > 0)
    {
      g_dbus_connection_signal_unsubscribe (proxy->priv->connection,
                                            proxy->priv->signals_subscriber_id);
    }

  g_object_unref (proxy->priv->connection);
  g_free (proxy->priv->name);
  g_free (proxy->priv->object_path);
  g_free (proxy->priv->interface_name);
  if (proxy->priv->properties != NULL)
    g_hash_table_unref (proxy->priv->properties);

  if (G_OBJECT_CLASS (g_dbus_proxy_parent_class)->finalize != NULL)
    G_OBJECT_CLASS (g_dbus_proxy_parent_class)->finalize (object);
}

static void
g_dbus_proxy_get_property (GObject    *object,
                           guint       prop_id,
                           GValue     *value,
                           GParamSpec *pspec)
{
  GDBusProxy *proxy = G_DBUS_PROXY (object);

  switch (prop_id)
    {
    case PROP_G_DBUS_PROXY_CONNECTION:
      g_value_set_object (value, proxy->priv->connection);
      break;

    case PROP_G_DBUS_PROXY_FLAGS:
      g_value_set_flags (value, proxy->priv->flags);
      break;

    case PROP_G_DBUS_PROXY_NAME:
      g_value_set_string (value, proxy->priv->name);
      break;

    case PROP_G_DBUS_PROXY_NAME_OWNER:
      g_value_set_string (value, g_dbus_proxy_get_name_owner (proxy));
      break;

    case PROP_G_DBUS_PROXY_OBJECT_PATH:
      g_value_set_string (value, proxy->priv->object_path);
      break;

    case PROP_G_DBUS_PROXY_INTERFACE_NAME:
      g_value_set_string (value, proxy->priv->interface_name);
      break;

    case PROP_G_DBUS_PROXY_PROPERTIES_AVAILABLE:
      g_value_set_boolean (value, proxy->priv->properties_available);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
g_dbus_proxy_set_property (GObject      *object,
                           guint         prop_id,
                           const GValue *value,
                           GParamSpec   *pspec)
{
  GDBusProxy *proxy = G_DBUS_PROXY (object);

  switch (prop_id)
    {
    case PROP_G_DBUS_PROXY_CONNECTION:
      proxy->priv->connection = g_value_dup_object (value);
      break;

    case PROP_G_DBUS_PROXY_FLAGS:
      proxy->priv->flags = g_value_get_flags (value);
      break;

    case PROP_G_DBUS_PROXY_NAME:
      proxy->priv->name = g_value_dup_string (value);
      break;

    case PROP_G_DBUS_PROXY_OBJECT_PATH:
      proxy->priv->object_path = g_value_dup_string (value);
      break;

    case PROP_G_DBUS_PROXY_INTERFACE_NAME:
      proxy->priv->interface_name = g_value_dup_string (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
g_dbus_proxy_class_init (GDBusProxyClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->finalize     = g_dbus_proxy_finalize;
  gobject_class->set_property = g_dbus_proxy_set_property;
  gobject_class->get_property = g_dbus_proxy_get_property;
  gobject_class->constructed  = g_dbus_proxy_constructed;

  /* all property names are prefixed to avoid collisions with D-Bus property names */

  /**
   * GDBusProxy:g-dbus-proxy-connection:
   *
   * The @GDBusConnection the proxy is for.
   */
  g_object_class_install_property (gobject_class,
                                   PROP_G_DBUS_PROXY_CONNECTION,
                                   g_param_spec_object ("g-dbus-proxy-connection",
                                                        _("g-dbus-proxy-connection"),
                                                        _("The connection the proxy is for"),
                                                        G_TYPE_DBUS_CONNECTION,
                                                        G_PARAM_READABLE |
                                                        G_PARAM_WRITABLE |
                                                        G_PARAM_CONSTRUCT_ONLY |
                                                        G_PARAM_STATIC_NAME |
                                                        G_PARAM_STATIC_BLURB |
                                                        G_PARAM_STATIC_NICK));

  /**
   * GDBusProxy:g-dbus-proxy-flags:
   *
   * Flags from the #GDBusProxyFlags enumeration.
   */
  g_object_class_install_property (gobject_class,
                                   PROP_G_DBUS_PROXY_FLAGS,
                                   g_param_spec_flags ("g-dbus-proxy-flags",
                                                       _("g-dbus-proxy-flags"),
                                                       _("Flags for the proxy"),
                                                       G_TYPE_DBUS_PROXY_FLAGS,
                                                       G_DBUS_PROXY_FLAGS_NONE,
                                                       G_PARAM_READABLE |
                                                       G_PARAM_WRITABLE |
                                                       G_PARAM_CONSTRUCT_ONLY |
                                                       G_PARAM_STATIC_NAME |
                                                       G_PARAM_STATIC_BLURB |
                                                       G_PARAM_STATIC_NICK));

  /**
   * GDBusProxy:g-dbus-proxy-name:
   *
   * The name the proxy is for.
   */
  g_object_class_install_property (gobject_class,
                                   PROP_G_DBUS_PROXY_NAME,
                                   g_param_spec_string ("g-dbus-proxy-name",
                                                        _("g-dbus-proxy-name"),
                                                        _("The name the proxy is for"),
                                                        NULL,
                                                        G_PARAM_READABLE |
                                                        G_PARAM_WRITABLE |
                                                        G_PARAM_CONSTRUCT_ONLY |
                                                        G_PARAM_STATIC_NAME |
                                                        G_PARAM_STATIC_BLURB |
                                                        G_PARAM_STATIC_NICK));

  /**
   * GDBusProxy:g-dbus-proxy-name-owner:
   *
   * The unique name of the owner owning the name the proxy is for or
   * %NULL if there is no such owner.
   */
  g_object_class_install_property (gobject_class,
                                   PROP_G_DBUS_PROXY_NAME_OWNER,
                                   g_param_spec_string ("g-dbus-proxy-name-owner",
                                                        _("g-dbus-proxy-name-owner"),
                                                        _("The owner of the name the proxy is for"),
                                                        NULL,
                                                        G_PARAM_READABLE |
                                                        G_PARAM_STATIC_NAME |
                                                        G_PARAM_STATIC_BLURB |
                                                        G_PARAM_STATIC_NICK));

  /**
   * GDBusProxy:g-dbus-proxy-object-path:
   *
   * The object path the proxy is for.
   */
  g_object_class_install_property (gobject_class,
                                   PROP_G_DBUS_PROXY_OBJECT_PATH,
                                   g_param_spec_string ("g-dbus-proxy-object-path",
                                                        _("g-dbus-proxy-object-path"),
                                                        _("The object path the proxy is for"),
                                                        NULL,
                                                        G_PARAM_READABLE |
                                                        G_PARAM_WRITABLE |
                                                        G_PARAM_CONSTRUCT_ONLY |
                                                        G_PARAM_STATIC_NAME |
                                                        G_PARAM_STATIC_BLURB |
                                                        G_PARAM_STATIC_NICK));

  /**
   * GDBusProxy:g-dbus-proxy-interface-name:
   *
   * The D-Bus interface name the proxy is for.
   */
  g_object_class_install_property (gobject_class,
                                   PROP_G_DBUS_PROXY_INTERFACE_NAME,
                                   g_param_spec_string ("g-dbus-proxy-interface-name",
                                                        _("g-dbus-proxy-interface-name"),
                                                        _("The D-Bus interface name the proxy is for"),
                                                        NULL,
                                                        G_PARAM_READABLE |
                                                        G_PARAM_WRITABLE |
                                                        G_PARAM_CONSTRUCT_ONLY |
                                                        G_PARAM_STATIC_NAME |
                                                        G_PARAM_STATIC_BLURB |
                                                        G_PARAM_STATIC_NICK));

  /**
   * GDBusProxy:g-dbus-proxy-properties-available:
   *
   * Whether D-Bus properties has been acquired.
   */
  g_object_class_install_property (gobject_class,
                                   PROP_G_DBUS_PROXY_PROPERTIES_AVAILABLE,
                                   g_param_spec_boolean ("g-dbus-proxy-properties-available",
                                                         _("g-dbus-proxy-properties-available"),
                                                         _("Whether D-Bus properties has been acquired"),
                                                         FALSE,
                                                         G_PARAM_READABLE |
                                                         G_PARAM_STATIC_NAME |
                                                         G_PARAM_STATIC_BLURB |
                                                         G_PARAM_STATIC_NICK));

  /**
   * GDBusProxy::g-dbus-proxy-properties-available-changed:
   * @proxy: The #GDBusProxy emitting the signal.
   * @properties_available: Whether properties are available.
   *
   * Emitted when properties are either available or unavailable on @proxy.
   **/
  signals[PROPERTIES_AVAILABLE_CHANGED_SIGNAL] = g_signal_new ("g-dbus-proxy-properties-available-changed",
                                                               G_TYPE_DBUS_PROXY,
                                                               G_SIGNAL_RUN_LAST,
                                                               G_STRUCT_OFFSET (GDBusProxyClass, properties_available_changed),
                                                               NULL,
                                                               NULL,
                                                               g_cclosure_marshal_VOID__BOOLEAN,
                                                               G_TYPE_NONE,
                                                               1,
                                                               G_TYPE_BOOLEAN);

  /**
   * GDBusProxy::g-dbus-proxy-properties-changed:
   * @proxy: The #GDBusProxy emitting the signal.
   * @changed_properties: A #GHashTable containing the properties that changed.
   *
   * Emitted when one or more D-Bus properties on @proxy changes. The cached properties
   * are already replaced when this signal fires.
   **/
  signals[PROPERTIES_CHANGED_SIGNAL] = g_signal_new ("g-dbus-proxy-properties-changed",
                                                     G_TYPE_DBUS_PROXY,
                                                     G_SIGNAL_RUN_LAST,
                                                     G_STRUCT_OFFSET (GDBusProxyClass, properties_changed),
                                                     NULL,
                                                     NULL,
                                                     g_cclosure_marshal_VOID__BOXED,
                                                     G_TYPE_NONE,
                                                     1,
                                                     G_TYPE_HASH_TABLE);

  /**
   * GDBusProxy::g-dbus-proxy-signal:
   * @proxy: The #GDBusProxy emitting the signal.
   * @name: The name of the signal.
   * @signature: The D-Bus signature of the signal.
   * @args: A #GPtrArray containing one #GDBusVariant for each argument of the signal.
   *
   * Emitted when a signal from the remote object and interface that @proxy is for, has been received.
   **/
  signals[SIGNAL_SIGNAL] = g_signal_new ("g-dbus-proxy-signal",
                                         G_TYPE_DBUS_PROXY,
                                         G_SIGNAL_RUN_LAST,
                                         G_STRUCT_OFFSET (GDBusProxyClass, signal),
                                         NULL,
                                         NULL,
                                         _gdbus_marshal_VOID__STRING_STRING_BOXED,
                                         G_TYPE_NONE,
                                         3,
                                         G_TYPE_STRING,
                                         G_TYPE_STRING,
                                         G_TYPE_PTR_ARRAY);


  g_type_class_add_private (klass, sizeof (GDBusProxyPrivate));
}

static void
g_dbus_proxy_init (GDBusProxy *proxy)
{
  proxy->priv = G_TYPE_INSTANCE_GET_PRIVATE (proxy, G_TYPE_DBUS_PROXY, GDBusProxyPrivate);
}

/* ---------------------------------------------------------------------------------------------------- */

/**
 * g_dbus_proxy_get_cached_property:
 * @proxy: A #GDBusProxy.
 * @property_name: Property name.
 * @error: Return location for error.
 *
 * Looks up the value for a property from the cache. This call does no blocking IO.
 *
 * Normally you will not need to modify the returned variant since it is updated automatically
 * in response to <literal>org.freedesktop.DBus.Properties.PropertiesChanged</literal>
 * D-Bus signals (which also causes #GDBusProxy::g-dbus-proxy-properties-changed to be emitted).
 * However, for properties for which said D-Bus signal is not emitted, you
 * can catch other signals and modify the returned variant accordingly (remember to emit
 * #GDBusProxy::g-dbus-proxy-properties-changed yourself). This should be done in a subclass
 * of #GDBusProxy since multiple different call sites may share the same underlying
 * #GDBusProxy object.
 *
 * Returns: A reference to the #GDBusVariant object that holds the value for @property_name or
 * %NULL if @error is set. Free the reference with g_object_unref().
 **/
GDBusVariant *
g_dbus_proxy_get_cached_property (GDBusProxy          *proxy,
                                  const gchar         *property_name,
                                  GError             **error)
{
  GDBusVariant *variant;

  g_return_val_if_fail (G_IS_DBUS_PROXY (proxy), NULL);
  g_return_val_if_fail (property_name != NULL, NULL);

  variant = NULL;

  if (proxy->priv->flags & G_DBUS_PROXY_FLAGS_DO_NOT_LOAD_PROPERTIES)
    {
      g_set_error (error,
                   G_DBUS_ERROR,
                   G_DBUS_ERROR_FAILED,
                   _("Properties are not available (proxy created with G_DBUS_PROXY_FLAGS_DO_NOT_LOAD_PROPERTIES)"));
      goto out;
    }

  if (!proxy->priv->properties_available)
    {
      g_set_error (error,
                   G_DBUS_ERROR,
                   G_DBUS_ERROR_FAILED,
                   _("Properties are not available"));
      goto out;
    }

  variant = g_hash_table_lookup (proxy->priv->properties, property_name);
  if (variant == NULL)
    {
      g_set_error (error,
                   G_DBUS_ERROR,
                   G_DBUS_ERROR_FAILED,
                   _("No property with name %s"),
                   property_name);
      goto out;
    }

  g_object_ref (variant);

 out:

  return variant;
}

/* ---------------------------------------------------------------------------------------------------- */

static void
get_all_cb (GDBusProxy   *proxy,
            GAsyncResult *res,
            gpointer      user_data)
{
  GError *error;

  proxy->priv->property_loading_pending_call_id = 0;

  /* check for invariants */
  g_warn_if_fail (proxy->priv->properties == NULL);
  g_warn_if_fail (!proxy->priv->properties_available);

  error = NULL;
  if (!g_dbus_proxy_invoke_method_finish (proxy,
                                          "a{sv}",
                                          res,
                                          &error,
                                          G_TYPE_HASH_TABLE, &proxy->priv->properties,
                                          G_TYPE_INVALID))
    {
      /* this can happen if the remote object does not have any properties */
      g_error_free (error);
      proxy->priv->properties = NULL;
    }
  else
    {
      proxy->priv->properties_available = TRUE;
      g_object_notify (G_OBJECT (proxy), "g-dbus-proxy-properties-available");
      g_signal_emit (proxy, signals[PROPERTIES_AVAILABLE_CHANGED_SIGNAL], 0, proxy->priv->properties_available);
    }
}

static void
on_signal_received (GDBusConnection  *connection,
                    const gchar      *name,
                    const gchar      *signature,
                    GPtrArray        *args,
                    gpointer          user_data)
{
  GDBusProxy *proxy = G_DBUS_PROXY (user_data);

  g_signal_emit (proxy,
                 signals[SIGNAL_SIGNAL],
                 0,
                 name,
                 signature,
                 args);
}

/* ---------------------------------------------------------------------------------------------------- */

static void
on_properties_changed (GDBusConnection  *connection,
                       const gchar      *name,
                       const gchar      *signature,
                       GPtrArray        *args,
                       gpointer          user_data)
{
  GDBusProxy *proxy = G_DBUS_PROXY (user_data);
  GError *error;
  const gchar *interface_name;
  GHashTable *changed_properties;
  GHashTableIter hash_iter;
  gchar *property_name;
  GDBusVariant *property_value;

  error = NULL;

  /* Ignore this signal if properties are not yet available
   *
   * (can happen in the window between subscribing to PropertiesChanged() and until
   *  org.freedesktop.DBus.Properties.GetAll() returns)
   */
  if (!proxy->priv->properties_available)
    goto out;

  if (args->len != 2)
    {
      g_warning ("Incorrect number of values in PropertiesChanged() signal");
      goto out;
    }

  if (!g_dbus_variant_is_string (G_DBUS_VARIANT (args->pdata[0])))
    {
      g_warning ("Expected a string for argument 1 of PropertiesChanged()");
      goto out;
    }
  interface_name = g_dbus_variant_get_string (G_DBUS_VARIANT (args->pdata[0]));

  if (g_strcmp0 (interface_name, proxy->priv->interface_name) != 0)
    {
      g_warning ("Expected PropertiesChanged() for interface %s but got the signal for interface %s",
                 proxy->priv->interface_name,
                 interface_name);
      goto out;
    }

  if (!g_dbus_variant_is_hash_table (G_DBUS_VARIANT (args->pdata[1])))
    {
      g_warning ("Expected a hash table for argument 2 of PropertiesChanged()");
      goto out;
    }
  changed_properties = g_dbus_variant_get_hash_table (G_DBUS_VARIANT (args->pdata[1]));

  /* update local cache */
  g_hash_table_iter_init (&hash_iter, changed_properties);
  while (g_hash_table_iter_next (&hash_iter,
                                 (gpointer) &property_name,
                                 (gpointer) &property_value))
    {
      g_hash_table_insert (proxy->priv->properties,
                           g_strdup (property_name),
                           g_object_ref (property_value));
    }

  /* emit signal */
  g_signal_emit (proxy, signals[PROPERTIES_CHANGED_SIGNAL], 0, changed_properties);

 out:
  ;
}

/* ---------------------------------------------------------------------------------------------------- */

static void
on_watcher_name_appeared (GBusNameWatcher *watcher,
                          gpointer         user_data)
{
  GDBusProxy *proxy = G_DBUS_PROXY (user_data);

  if (!(proxy->priv->flags & G_DBUS_PROXY_FLAGS_DO_NOT_LOAD_PROPERTIES))
    {
      /* check for invariants */
      g_warn_if_fail (proxy->priv->properties == NULL);
      g_warn_if_fail (!proxy->priv->properties_available);
      g_warn_if_fail (proxy->priv->properties_changed_subscriber_id == 0);

      /* subscribe to PropertiesChanged() */
      proxy->priv->properties_changed_subscriber_id =
        g_dbus_connection_signal_subscribe (proxy->priv->connection,
                                            g_bus_name_watcher_get_name_owner (proxy->priv->watcher),
                                            "org.freedesktop.DBus.Properties",
                                            "PropertiesChanged",
                                            proxy->priv->object_path,
                                            proxy->priv->interface_name,
                                            on_properties_changed,
                                            proxy,
                                            NULL);

      /* start loading properties */
      proxy->priv->property_loading_pending_call_id =
        g_dbus_proxy_invoke_method (proxy,
                                    "org.freedesktop.DBus.Properties.GetAll",
                                    "s",
                                    -1,
                                    NULL,
                                    (GAsyncReadyCallback) get_all_cb,
                                    NULL,
                                    G_TYPE_STRING,
                                    proxy->priv->interface_name,
                                    G_TYPE_INVALID);
    }

  if (!(proxy->priv->flags & G_DBUS_PROXY_FLAGS_DO_NOT_CONNECT_SIGNALS))
    {
      /* check for invariants */
      g_warn_if_fail (proxy->priv->signals_subscriber_id == 0);

      /* subscribe to all signals for the object */
      proxy->priv->signals_subscriber_id =
        g_dbus_connection_signal_subscribe (proxy->priv->connection,
                                            g_bus_name_watcher_get_name_owner (proxy->priv->watcher),
                                            proxy->priv->interface_name,
                                            NULL,                        /* member */
                                            proxy->priv->object_path,
                                            NULL,                        /* arg0 */
                                            on_signal_received,
                                            proxy,
                                            NULL);
    }

  g_object_notify (G_OBJECT (proxy), "g-dbus-proxy-name-owner");
}

/* ---------------------------------------------------------------------------------------------------- */

static void
on_watcher_name_vanished (GBusNameWatcher *watcher,
                          gpointer         user_data)
{
  GDBusProxy *proxy = G_DBUS_PROXY (user_data);

  g_object_notify (G_OBJECT (proxy), "g-dbus-proxy-name-owner");

  if (!(proxy->priv->flags & G_DBUS_PROXY_FLAGS_DO_NOT_LOAD_PROPERTIES))
    {
      gboolean old_properties_available;

      if (proxy->priv->properties_changed_subscriber_id > 0)
        {
          g_dbus_connection_dbus_1_signal_unsubscribe (proxy->priv->connection,
                                                       proxy->priv->properties_changed_subscriber_id);
          proxy->priv->properties_changed_subscriber_id = 0;
        }

      old_properties_available = proxy->priv->properties_available;
      proxy->priv->properties_available = FALSE;
      if (proxy->priv->properties != NULL)
        {
          g_hash_table_unref (proxy->priv->properties);
          proxy->priv->properties = NULL;
        }
      if (old_properties_available != proxy->priv->properties_available)
        {
          g_object_notify (G_OBJECT (proxy), "g-dbus-proxy-properties-available");
          g_signal_emit (proxy, signals[PROPERTIES_AVAILABLE_CHANGED_SIGNAL], 0, proxy->priv->properties_available);

          /* cancel loading properties if applicable */
          if (proxy->priv->property_loading_pending_call_id > 0)
            {
              g_dbus_connection_send_dbus_1_message_cancel (proxy->priv->connection,
                                                            proxy->priv->property_loading_pending_call_id);
              proxy->priv->property_loading_pending_call_id = 0;
            }
        }

    }

  if (!(proxy->priv->flags & G_DBUS_PROXY_FLAGS_DO_NOT_CONNECT_SIGNALS))
    {
      if (proxy->priv->signals_subscriber_id > 0)
        {
          g_dbus_connection_dbus_1_signal_unsubscribe (proxy->priv->connection,
                                                       proxy->priv->signals_subscriber_id);
          proxy->priv->signals_subscriber_id = 0;
        }
    }
}

/* ---------------------------------------------------------------------------------------------------- */

static void
on_watcher_initialized (GBusNameWatcher *watcher,
                        gpointer         user_data)
{
  GDBusProxy *proxy = G_DBUS_PROXY (user_data);
  const gchar *name_owner;

  proxy->priv->watcher_name_appeared_signal_handler_id = g_signal_connect (proxy->priv->watcher,
                                                                           "name-appeared",
                                                                           G_CALLBACK (on_watcher_name_appeared),
                                                                           proxy);

  proxy->priv->watcher_name_vanished_signal_handler_id = g_signal_connect (proxy->priv->watcher,
                                                                           "name-vanished",
                                                                           G_CALLBACK (on_watcher_name_vanished),
                                                                           proxy);

  name_owner = g_bus_name_watcher_get_name_owner (proxy->priv->watcher);
  if (name_owner != NULL)
    {
      on_watcher_name_appeared (proxy->priv->watcher, proxy);
    }
  else
    {
      /* No name owner.. just tool around until "name-appeared" fires */
    }
}

/* ---------------------------------------------------------------------------------------------------- */

static void
g_dbus_proxy_constructed (GObject *object)
{
  GDBusProxy *proxy = G_DBUS_PROXY (object);

  proxy->priv->watcher = g_bus_name_watcher_new_for_connection (proxy->priv->connection, proxy->priv->name);
  if (g_bus_name_watcher_get_is_initialized (proxy->priv->watcher))
    on_watcher_initialized (proxy->priv->watcher, proxy);
  else
    proxy->priv->watcher_initialized_signal_handler_id = g_signal_connect (proxy->priv->watcher,
                                                                           "initialized",
                                                                           G_CALLBACK (on_watcher_initialized),
                                                                           proxy);

  if (G_OBJECT_CLASS (g_dbus_proxy_parent_class)->constructed != NULL)
    G_OBJECT_CLASS (g_dbus_proxy_parent_class)->constructed (object);
}

/* ---------------------------------------------------------------------------------------------------- */

/**
 * g_dbus_proxy_new:
 * @connection: A #GDBusConnection.
 * @flags: Flags used when constructing the proxy.
 * @name: A bus name.
 * @object_path: An object path.
 * @interface_name: A D-Bus interface name.
 *
 * Creates a proxy for accessing @interface_name on the remote object at @object_path
 * owned by @name at @connection and starts loading D-Bus properties unless the
 * #G_DBUS_PROXY_FLAGS_DO_NOT_LOAD_PROPERTIES flag is used.
 *
 * Use g_dbus_proxy_get_properties_available() to check whether properties
 * are available and connect to the #GDBusProxy::g-dbus-proxy-properties-available-changed
 * signal or listen for notifications on the #GDBusProxy:g-dbus-proxy-properties-available
 * property to get informed when properties are available. Properties are reloaded whenever
 * a new owner acquires @name.
 *
 * If the #G_DBUS_PROXY_FLAGS_DO_NOT_CONNECT_SIGNALS flag is not set, also sets up
 * match rules for signals. Connect to the #GDBusProxy::g-dbus-proxy-signal signal
 * to handle signals from the remote object.
 *
 * Use g_dbus_proxy_get_name_owner() to check the current owner of the proxy and
 * watch for notifications on #GDBusProxy:g-dbus-proxy-name-owner for changes. If
 * there is no owner, method calls may activate the name depending on whether
 * the name supports activation.
 *
 * Returns: A #GDBusProxy. Free with g_object_unref().
 **/
GDBusProxy *
g_dbus_proxy_new (GDBusConnection     *connection,
                  GDBusProxyFlags      flags,
                  const gchar         *name,
                  const gchar         *object_path,
                  const gchar         *interface_name)
{
  GDBusProxy *proxy;

  /* TODO: maybe do singleton stuff like we do for watchers and owners. Tricky though,
   *       since the normal use case is to instantiate a subclass.
   */

  g_return_val_if_fail (G_IS_DBUS_CONNECTION (connection), NULL);
  g_return_val_if_fail (name != NULL, NULL);
  g_return_val_if_fail (object_path != NULL, NULL);
  g_return_val_if_fail (interface_name, NULL);

  proxy = G_DBUS_PROXY (g_object_new (G_TYPE_DBUS_PROXY,
                                      "g-dbus-proxy-flags", flags,
                                      "g-dbus-proxy-name", name,
                                      "g-dbus-proxy-connection", connection,
                                      "g-dbus-proxy-object-path", object_path,
                                      "g-dbus-proxy-interface-name", interface_name,
                                      NULL));
  return proxy;
}

/* ---------------------------------------------------------------------------------------------------- */

/**
 * g_dbus_proxy_get_connection:
 * @proxy: A #GDBusProxy.
 *
 * Gets the connection @proxy is for.
 *
 * Returns: A #GDBusConnection owned by @proxy. Do not free.
 **/
GDBusConnection *
g_dbus_proxy_get_connection (GDBusProxy *proxy)
{
  g_return_val_if_fail (G_IS_DBUS_PROXY (proxy), NULL);
  return proxy->priv->connection;
}

/**
 * g_dbus_proxy_get_flags:
 * @proxy: A #GDBusProxy.
 *
 * Gets the flags that @proxy was constructed with.
 *
 * Returns: Flags from the #GDBusProxyFlags enumeration.
 **/
GDBusProxyFlags
g_dbus_proxy_get_flags (GDBusProxy *proxy)
{
  g_return_val_if_fail (G_IS_DBUS_PROXY (proxy), 0);
  return proxy->priv->flags;
}

/**
 * g_dbus_proxy_get_name:
 * @proxy: A #GDBusProxy.
 *
 * Gets the bus name @proxy is for.
 *
 * Returns: A string owned by @proxy. Do not free.
 **/
const gchar *
g_dbus_proxy_get_name (GDBusProxy *proxy)
{
  g_return_val_if_fail (G_IS_DBUS_PROXY (proxy), NULL);
  return proxy->priv->name;
}

/**
 * g_dbus_proxy_get_name_owner:
 * @proxy: A #GDBusProxy.
 *
 * Gets the unique name of the owner owning the name @proxy is for. If there
 * is no owner of the name, this function returns %NULL. You can watch the
 * property #GDBusProxy:g-dbus-proxy-name-owner for notifications to track
 * changes.
 *
 * Returns: A string owned by @proxy. Do not free.
 **/
const gchar *
g_dbus_proxy_get_name_owner (GDBusProxy *proxy)
{
  g_return_val_if_fail (G_IS_DBUS_PROXY (proxy), NULL);
  return g_bus_name_watcher_get_name_owner (proxy->priv->watcher);
}

/**
 * g_dbus_proxy_get_object_path:
 * @proxy: A #GDBusProxy.
 *
 * Gets the object path @proxy is for.
 *
 * Returns: A string owned by @proxy. Do not free.
 **/
const gchar *
g_dbus_proxy_get_object_path (GDBusProxy *proxy)
{
  g_return_val_if_fail (G_IS_DBUS_PROXY (proxy), NULL);
  return proxy->priv->object_path;
}

/**
 * g_dbus_proxy_get_interface_name:
 * @proxy: A #GDBusProxy.
 *
 * Gets the D-Bus interface name @proxy is for.
 *
 * Returns: A string owned by @proxy. Do not free.
 **/
const gchar *
g_dbus_proxy_get_interface_name (GDBusProxy *proxy)
{
  g_return_val_if_fail (G_IS_DBUS_PROXY (proxy), NULL);
  return proxy->priv->interface_name;
}

/**
 * g_dbus_proxy_get_properties_available:
 * @proxy: A #GDBusProxy.
 *
 * Gets whether properties has been available.
 *
 * To track changes, listen for notifications on the
 * #GDBusProxy:g-dbus-proxy-properties-available property.
 *
 * Returns: %TRUE if properties are available, %FALSE otherwise.
 **/
gboolean
g_dbus_proxy_get_properties_available (GDBusProxy *proxy)
{
  g_return_val_if_fail (G_IS_DBUS_PROXY (proxy), FALSE);
  return proxy->priv->properties_available;
}

/* ---------------------------------------------------------------------------------------------------- */

static gboolean
extract_values_cb (guint        arg_num,
                   const gchar *signature,
                   GType        type,
                   va_list      va_args,
                   gpointer     user_data)
{
  DBusMessageIter *iter = user_data;
  GValue value = {0};
  GValue value2 = {0};
  GValue *value_to_extract;
  GError *error;
  gboolean ret;
  gchar *error_str;

  ret = TRUE;

  //g_debug ("arg %d: %s", arg_num, g_type_name (type));

  error = NULL;
  if (!g_dbus_c_type_mapping_get_value_from_iter (iter, &value, &error))
    {
      g_warning ("Error extracting value for arg %d: %s",
                 arg_num,
                 error->message);
      g_error_free (error);
      goto out;
    }
  value_to_extract = &value;

  /* coerce if needed */
  if (!g_type_is_a (type, G_VALUE_TYPE (&value)))
    {
      /* try and see if a transformation routine is available */
      g_value_init (&value2, type);
      if (!g_value_transform (&value, &value2))
        {
          g_warning ("GType '%s' is incompatible with D-Bus signature '%s' (expected GType '%s') "
                     "and no transformation is available",
                     g_type_name (type),
                     signature,
                     g_type_name (G_VALUE_TYPE (&value)));
          g_value_unset (&value);
          goto out;
        }
      //g_debug ("Transformed from %s to %s", g_type_name (G_VALUE_TYPE (&value)), g_type_name (type));
      value_to_extract = &value2;
      g_value_unset (&value);
    }

  error_str = NULL;
  G_VALUE_LCOPY (value_to_extract, va_args, G_VALUE_NOCOPY_CONTENTS, &error_str);
  if (error != NULL)
    {
      g_warning ("Error copying value: %s", error_str);
      g_free (error_str);
      g_value_unset (value_to_extract);
      goto out;
    }

  dbus_message_iter_next (iter);

  ret = FALSE;

 out:
  return ret;
}

/* ---------------------------------------------------------------------------------------------------- */

static gboolean
append_values_cb (guint        arg_num,
                  const gchar *signature,
                  GType        type,
                  va_list      va_args,
                  gpointer     user_data)
{
  DBusMessageIter *iter = user_data;
  GValue value = {0};
  GValue value2 = {0};
  GValue *value_to_append;
  GError *error;
  gboolean ret;
  gchar *error_str;
  GType expected_type;

  ret = TRUE;

  //g_debug ("arg %d: %s", arg_num, g_type_name (type));

  g_value_init (&value, type);
  error_str = NULL;
  G_VALUE_COLLECT (&value, va_args, G_VALUE_NOCOPY_CONTENTS, &error_str);
  if (error_str != NULL)
    {
      g_warning ("Error collecting value: %s", error_str);
      g_free (error_str);
      g_value_unset (&value);
      goto out;
    }
  value_to_append = &value;

  /* coerce if needed */
  expected_type = _g_dbus_signature_get_gtype (signature);
  if (!g_type_is_a (type, expected_type))
    {
      /* try and see if a transformation routine is available */
      g_value_init (&value2, expected_type);
      if (!g_value_transform (&value, &value2))
        {
          g_warning ("GType '%s' is incompatible with D-Bus signature '%s' (expected GType '%s') "
                     "and no transformation is available",
                     g_type_name (type),
                     signature,
                     g_type_name (expected_type));
          g_value_unset (&value);
          goto out;
        }
      //g_debug ("Transformed from %s to %s", g_type_name (type), g_type_name (expected_type));
      value_to_append = &value2;
      g_value_unset (&value);
    }

  error = NULL;
  if (!g_dbus_c_type_mapping_append_value_to_iter (iter, signature, value_to_append, &error))
    {
      g_warning ("Error appending value for arg %d: %s",
                 arg_num,
                 error->message);
      g_error_free (error);
      g_value_unset (value_to_append);
      goto out;
    }

  g_value_unset (value_to_append);

  ret = FALSE;

 out:
  return ret;
}

/* ---------------------------------------------------------------------------------------------------- */

static void
invoke_method_cb (GDBusConnection *connection,
                  GAsyncResult    *result,
                  gpointer         user_data)
{
  GSimpleAsyncResult *simple = G_SIMPLE_ASYNC_RESULT (user_data);
  DBusMessage *reply;
  GError *error;

  error = NULL;
  reply = g_dbus_connection_send_dbus_1_message_with_reply_finish (connection,
                                                                   result,
                                                                   &error);

  if (reply == NULL)
    {
      g_simple_async_result_set_from_error (simple, error);
      g_error_free (error);
    }
  else
    {
      g_simple_async_result_set_op_res_gpointer (simple,
                                                 reply,
                                                 (GDestroyNotify) dbus_message_unref);
    }

  g_simple_async_result_complete (simple);
  g_object_unref (simple);
}

static guint
g_dbus_proxy_invoke_method_valist (GDBusProxy          *proxy,
                                   const gchar         *name,
                                   const gchar         *signature,
                                   guint                timeout_msec,
                                   GCancellable        *cancellable,
                                   GAsyncReadyCallback  callback,
                                   gpointer             user_data,
                                   GType                first_in_arg_type,
                                   va_list              va_args)
{
  DBusMessage *message;
  DBusMessageIter iter;
  GSimpleAsyncResult *simple;
  guint pending_call_id;

  pending_call_id = 0;

  simple = g_simple_async_result_new (G_OBJECT (proxy),
                                      callback,
                                      user_data,
                                      g_dbus_proxy_invoke_method);

  if (strchr (name, '.'))
    {
      gchar *buf;
      gchar *p;
      buf = g_strdup (name);
      p = strrchr (buf, '.');
      *p = '\0';
      if ((message = dbus_message_new_method_call (proxy->priv->name,
                                                   proxy->priv->object_path,
                                                   buf,
                                                   p + 1)) == NULL)
        _g_dbus_oom ();
      g_free (buf);
    }
  else
    {
      if ((message = dbus_message_new_method_call (proxy->priv->name,
                                                   proxy->priv->object_path,
                                                   proxy->priv->interface_name,
                                                   name)) == NULL)
        _g_dbus_oom ();
    }

  /* append args to message */
  dbus_message_iter_init_append (message, &iter);
  if (_gdbus_signature_vararg_foreach (signature,
                                       first_in_arg_type,
                                       va_args,
                                       append_values_cb,
                                       &iter))
    goto out;

  pending_call_id = g_dbus_connection_send_dbus_1_message_with_reply (proxy->priv->connection,
                                                                      message,
                                                                      timeout_msec,
                                                                      cancellable,
                                                                      (GAsyncReadyCallback) invoke_method_cb,
                                                                      simple);
  dbus_message_unref (message);

 out:
  return pending_call_id;
}

static gboolean
g_dbus_proxy_invoke_method_finish_valist (GDBusProxy          *proxy,
                                          const gchar         *signature,
                                          GAsyncResult        *res,
                                          GError             **error,
                                          GType                first_out_arg_type,
                                          va_list              va_args)
{
  GSimpleAsyncResult *simple = G_SIMPLE_ASYNC_RESULT (res);
  DBusMessage *reply;
  DBusMessageIter reply_iter;
  DBusError dbus_error;
  gboolean ret;

  g_warn_if_fail (g_simple_async_result_get_source_tag (simple) == g_dbus_proxy_invoke_method);

  ret = FALSE;
  if (g_simple_async_result_propagate_error (simple, error))
    goto out;

  reply = g_simple_async_result_get_op_res_gpointer (simple);
  dbus_error_init (&dbus_error);

  /* Translate to GError if the reply is an error */
  if (dbus_set_error_from_message (&dbus_error, reply))
    {
      /* TODO: hmm, gotta get caller to pass error_types or... better to rethink GDBusError helpers */
      g_dbus_error_set_dbus_error (error,
                                   &dbus_error,
                                   NULL,
                                   NULL);
      dbus_error_free (&dbus_error);
      goto out;
    }

  /* extract values from message */
  dbus_message_iter_init (reply, &reply_iter);
  if (_gdbus_signature_vararg_foreach (signature,
                                       first_out_arg_type,
                                       va_args,
                                       extract_values_cb,
                                       &reply_iter))
    {
      g_set_error (error,
                   G_DBUS_ERROR,
                   G_DBUS_ERROR_FAILED,
                   _("Error extracting values from D-Bus message. This is a bug in the application"));
      ret = FALSE;
      goto out;
    }

  ret = TRUE;

 out:
  return ret;
}

/**
 * g_dbus_proxy_invoke_method:
 * @proxy: A #GDBusProxy.
 * @name: The name of the method to invoke.
 * @signature: The D-Bus signature for all arguments being passed to the method.
 * @timeout_msec: Timeout in milliseconds or -1 for default timeout.
 * @cancellable: A #GCancellable or %NULL.
 * @callback: The callback function to invoke when the reply is ready or %NULL.
 * @user_data: User data to pass to @callback.
 * @first_in_arg_type: The #GType of the first argument to pass or #G_TYPE_INVALID if there are no arguments.
 * @...: The value of the first in argument, followed by zero or more (type, value) pairs, followed by #G_TYPE_INVALID.
 *
 * Invokes the method @name on the interface and remote object represented by @proxy. This
 * is an asynchronous operation and when the reply is ready @callback will be invoked (on
 * the main thread) and you can get the result by calling g_dbus_proxy_invoke_method_finish().
 *
 * If @name contains any dots, then @name is split into interface and
 * method name parts. This allows using @proxy for invoking methods on
 * other interfaces.
 *
 * Note that @signature and and the supplied (type, value) pairs must match as described in
 * chapter TODO_SECTION_EXPLAINING_DBUS_TO_GTYPE_OBJECT_MAPPING.
 *
 * Returns: An pending call id (never 0) for the message that can be used with
 * g_dbus_proxy_invoke_method_cancel() or g_dbus_proxy_invoke_method_block().
 **/
guint
g_dbus_proxy_invoke_method (GDBusProxy          *proxy,
                            const gchar         *name,
                            const gchar         *signature,
                            guint                timeout_msec,
                            GCancellable        *cancellable,
                            GAsyncReadyCallback  callback,
                            gpointer             user_data,
                            GType                first_in_arg_type,
                            ...)
{
  va_list va_args;
  guint pending_call_id;

  va_start (va_args, first_in_arg_type);
  pending_call_id = g_dbus_proxy_invoke_method_valist (proxy,
                                                       name,
                                                       signature,
                                                       timeout_msec,
                                                       cancellable,
                                                       callback,
                                                       user_data,
                                                       first_in_arg_type,
                                                       va_args);
  va_end (va_args);

  return pending_call_id;
}

/**
 * g_dbus_proxy_invoke_method_block:
 * @proxy: A #GDBusProxy.
 * @pending_call_id: A pending call id obtained from g_dbus_proxy_invoke_method().
 *
 * Blocks until the pending call specified by @pending_call_id is done. This will
 * not block in the main loop. You are guaranteed that the #GAsyncReadyCallback passed to
 * g_dbus_proxy_invoke_method() is invoked before this function returns.
 **/
void
g_dbus_proxy_invoke_method_block (GDBusProxy *proxy,
                                  guint       pending_call_id)
{
  g_return_if_fail (G_IS_DBUS_PROXY (proxy));
  g_dbus_connection_send_dbus_1_message_block (proxy->priv->connection,
                                               pending_call_id);
}

/**
 * g_dbus_proxy_invoke_method_cancel:
 * @proxy: A #GDBusProxy.
 * @pending_call_id: A pending call id obtained from g_dbus_proxy_invoke_method().
 *
 * Cancels the pending call specified by @pending_call_id.
 *
 * The #GAsyncReadyCallback passed to g_dbus_proxy_invoke_method() will
 * be invoked in idle (e.g. after this function returns).
 **/
void
g_dbus_proxy_invoke_method_cancel (GDBusProxy *proxy,
                                   guint       pending_call_id)
{
  g_return_if_fail (G_IS_DBUS_PROXY (proxy));
  g_dbus_connection_send_dbus_1_message_cancel (proxy->priv->connection,
                                                pending_call_id);
}


/**
 * g_dbus_proxy_invoke_method_finish:
 * @proxy: A #GDBusProxy.
 * @signature: The D-Bus signature for all return values.
 * @res: A #GAsyncResult obtained from the #GAsyncReadyCallback function passed to g_dbus_proxy_invoke_method().
 * @error: Return location for error or %NULL.
 * @first_out_arg_type: The #GType of the first return value or #G_TYPE_INVALID if there are no return values.
 * @...: The location to store of the first return value, followed by zero or more (type, value) pairs,
 * followed by #G_TYPE_INVALID.
 *
 * Finishes invoking a method on @proxy initiated with g_dbus_proxy_invoke_method().
 *
 * Note that @signature and and the supplied (type, return value) pairs must match as described in
 * chapter TODO_SECTION_EXPLAINING_DBUS_TO_GTYPE_OBJECT_MAPPING.
 *
 * Returns: %TRUE if the method invocation succeeded, %FALSE is @error is set.
 **/
gboolean
g_dbus_proxy_invoke_method_finish (GDBusProxy          *proxy,
                                   const gchar         *signature,
                                   GAsyncResult        *res,
                                   GError             **error,
                                   GType                first_out_arg_type,
                                   ...)
{
  gboolean ret;
  va_list va_args;

  g_return_val_if_fail (G_IS_DBUS_PROXY (proxy), FALSE);
  /* TODO: other checks */

  va_start (va_args, first_out_arg_type);
  ret = g_dbus_proxy_invoke_method_finish_valist (proxy,
                                                  signature,
                                                  res,
                                                  error,
                                                  first_out_arg_type,
                                                  va_args);
  va_end (va_args);

  return ret;
}

static void
invoke_method_sync_cb (GDBusConnection *connection,
                       GAsyncResult    *res,
                       gpointer         user_data)
{
  GAsyncResult **out_res = user_data;
  g_assert (out_res != NULL);
  *out_res = g_object_ref (res);
}

/**
 * g_dbus_proxy_invoke_method_sync:
 * @proxy: A #GDBusProxy.
 * @name: The name of the method to invoke.
 * @in_signature: The D-Bus signature for all arguments being passed to the method.
 * @out_signature: The D-Bus signature for all return values.
 * @timeout_msec: Timeout in milliseconds or -1 for default timeout.
 * @cancellable: A #GCancellable or %NULL.
 * @error: Return location for @error or %NULL.
 * @first_in_arg_type: The #GType of the first argument to pass or #G_TYPE_INVALID if there are no arguments.
 * @...: The value of the first argument to pass (if any), followed by zero or more (type, value) pairs,
 * followed by #G_TYPE_INVALID. Then one or more pairs for the #GType and return location for the return
 * values, then terminated by #G_TYPE_INVALID.
 *
 * Synchronously invokes the method @name on the interface and remote
 * object represented by @proxy.
 *
 * If @name contains any dots, then @name is split into interface and
 * method name parts. This allows using @proxy for invoking methods on
 * other interfaces.
 *
 * Returns: %TRUE if the call succeeded, %FALSE if @error is set.
 **/
gboolean
g_dbus_proxy_invoke_method_sync (GDBusProxy          *proxy,
                                 const gchar         *name,
                                 const gchar         *in_signature,
                                 const gchar         *out_signature,
                                 guint                timeout_msec,
                                 GCancellable        *cancellable,
                                 GError             **error,
                                 GType                first_in_arg_type,
                                 ...)
{
  va_list va_args;
  guint pending_call_id;
  GAsyncResult *res;
  GType first_out_arg_type;
  gboolean ret;

  g_return_val_if_fail (G_IS_DBUS_PROXY (proxy), FALSE);
  /* TODO: other checks */

  ret = FALSE;

  va_start (va_args, first_in_arg_type);
  pending_call_id = g_dbus_proxy_invoke_method_valist (proxy,
                                                       name,
                                                       in_signature,
                                                       timeout_msec,
                                                       cancellable,
                                                       (GAsyncReadyCallback) invoke_method_sync_cb,
                                                       &res,
                                                       first_in_arg_type,
                                                       va_args);
  /* check if caller messed up */
  if (pending_call_id == 0)
    {
      g_set_error (error,
                   G_DBUS_ERROR,
                   G_DBUS_ERROR_FAILED,
                   _("Error appending values to D-Bus message. This is a bug in the application"));
      goto out;
    }

  g_assert (va_arg (va_args, GType) == G_TYPE_INVALID);
  first_out_arg_type = va_arg (va_args, GType);

  res = NULL;
  g_dbus_proxy_invoke_method_block (proxy, pending_call_id);

  g_assert (res != NULL);

  ret = g_dbus_proxy_invoke_method_finish_valist (proxy,
                                                  out_signature,
                                                  res,
                                                  error,
                                                  first_out_arg_type,
                                                  va_args);
  g_object_unref (res);

 out:
  va_end (va_args);

  return ret;
}

#define __G_DBUS_PROXY_C__
#include "gdbusaliasdef.c"
