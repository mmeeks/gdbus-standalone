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
#include "gdbus-marshal.h"
#include "gdbusprivate.h"

#include "gdbusalias.h"

/**
 * SECTION:gdbusproxy
 * @short_description: Base class for proxies
 * @include: gdbus/gdbus.h
 *
 * #GDBusProxy is a base class used for proxies to access a D-Bus
 * interface on a remote object. A #GDBusProxy can only be constructed
 * for unique name bus and does not track whether the name
 * vanishes. Use g_bus_watch_proxy() to construct #GDBusProxy proxies
 * for owners of a well-known names.
 */

struct _GDBusProxyPrivate
{
  GDBusConnection *connection;
  GDBusProxyFlags flags;
  gchar *unique_bus_name;
  gchar *object_path;
  gchar *interface_name;
  GHashTable *properties;

  guint properties_changed_subscriber_id;
  guint signals_subscriber_id;
};

enum
{
  PROP_0,
  PROP_G_DBUS_PROXY_CONNECTION,
  PROP_G_DBUS_PROXY_UNIQUE_BUS_NAME,
  PROP_G_DBUS_PROXY_FLAGS,
  PROP_G_DBUS_PROXY_OBJECT_PATH,
  PROP_G_DBUS_PROXY_INTERFACE_NAME,
};

enum
{
  PROPERTIES_CHANGED_SIGNAL,
  SIGNAL_SIGNAL, /* (!) */
  LAST_SIGNAL,
};

static void g_dbus_proxy_constructed (GObject *object);

guint signals[LAST_SIGNAL] = {0};

static void initable_iface_init       (GInitableIface *initable_iface);
static void async_initable_iface_init (GAsyncInitableIface *async_initable_iface);

G_DEFINE_TYPE_WITH_CODE (GDBusProxy, g_dbus_proxy, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (G_TYPE_INITABLE, initable_iface_init)
                         G_IMPLEMENT_INTERFACE (G_TYPE_ASYNC_INITABLE, async_initable_iface_init)
                         );

static void
g_dbus_proxy_finalize (GObject *object)
{
  GDBusProxy *proxy = G_DBUS_PROXY (object);

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
  g_free (proxy->priv->unique_bus_name);
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

    case PROP_G_DBUS_PROXY_UNIQUE_BUS_NAME:
      g_value_set_string (value, proxy->priv->unique_bus_name);
      break;

    case PROP_G_DBUS_PROXY_OBJECT_PATH:
      g_value_set_string (value, proxy->priv->object_path);
      break;

    case PROP_G_DBUS_PROXY_INTERFACE_NAME:
      g_value_set_string (value, proxy->priv->interface_name);
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

    case PROP_G_DBUS_PROXY_UNIQUE_BUS_NAME:
      proxy->priv->unique_bus_name = g_value_dup_string (value);
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

  /* Note that all property names are prefixed to avoid collisions with D-Bus property names
   * in derived classes */

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
   * GDBusProxy:g-dbus-proxy-unique-bus-name:
   *
   * The unique bus name the proxy is for.
   */
  g_object_class_install_property (gobject_class,
                                   PROP_G_DBUS_PROXY_UNIQUE_BUS_NAME,
                                   g_param_spec_string ("g-dbus-proxy-unique-bus-name",
                                                        _("g-dbus-proxy-unique-bus-name"),
                                                        _("The unique bus name the proxy is for"),
                                                        NULL,
                                                        G_PARAM_READABLE |
                                                        G_PARAM_WRITABLE |
                                                        G_PARAM_CONSTRUCT_ONLY |
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
   * @signal_name: The name of the signal.
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
 *
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
on_signal_received (GDBusConnection  *connection,
                    const gchar      *signal_name,
                    const gchar      *signature,
                    GPtrArray        *args,
                    gpointer          user_data)
{
  GDBusProxy *proxy = G_DBUS_PROXY (user_data);

  g_signal_emit (proxy,
                 signals[SIGNAL_SIGNAL],
                 0,
                 signal_name,
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

#if 0 // TODO!
  /* Ignore this signal if properties are not yet available
   *
   * (can happen in the window between subscribing to PropertiesChanged() and until
   *  org.freedesktop.DBus.Properties.GetAll() returns)
   */
  if (!proxy->priv->properties_available)
    goto out;
#endif

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
g_dbus_proxy_constructed (GObject *object)
{
  if (G_OBJECT_CLASS (g_dbus_proxy_parent_class)->constructed != NULL)
    G_OBJECT_CLASS (g_dbus_proxy_parent_class)->constructed (object);
}

/* ---------------------------------------------------------------------------------------------------- */

static void
subscribe_to_signals (GDBusProxy *proxy)
{
  if (!(proxy->priv->flags & G_DBUS_PROXY_FLAGS_DO_NOT_LOAD_PROPERTIES))
    {
      /* subscribe to PropertiesChanged() */
      proxy->priv->properties_changed_subscriber_id =
        g_dbus_connection_signal_subscribe (proxy->priv->connection,
                                            proxy->priv->unique_bus_name,
                                            "org.freedesktop.DBus.Properties",
                                            "PropertiesChanged",
                                            proxy->priv->object_path,
                                            proxy->priv->interface_name,
                                            on_properties_changed,
                                            proxy,
                                            NULL);
    }

  if (!(proxy->priv->flags & G_DBUS_PROXY_FLAGS_DO_NOT_CONNECT_SIGNALS))
    {
      /* subscribe to all signals for the object */
      proxy->priv->signals_subscriber_id =
        g_dbus_connection_signal_subscribe (proxy->priv->connection,
                                            proxy->priv->unique_bus_name,
                                            proxy->priv->interface_name,
                                            NULL,                        /* member */
                                            proxy->priv->object_path,
                                            NULL,                        /* arg0 */
                                            on_signal_received,
                                            proxy,
                                            NULL);
    }
}

/* ---------------------------------------------------------------------------------------------------- */


static gboolean
initable_init (GInitable       *initable,
               GCancellable    *cancellable,
               GError         **error)
{
  GDBusProxy *proxy = G_DBUS_PROXY (initable);
  gboolean ret;

  ret = FALSE;

  /* load all properties synchronously */
  if (!g_dbus_proxy_invoke_method_sync (proxy,
                                        "org.freedesktop.DBus.Properties.GetAll",
                                        "s",
                                        "a{sv}",
                                        -1,           /* timeout */
                                        cancellable,
                                        error,
                                        G_TYPE_STRING, proxy->priv->interface_name,
                                        G_TYPE_INVALID,
                                        G_TYPE_HASH_TABLE, &proxy->priv->properties,
                                        G_TYPE_INVALID))
    {
      goto out;
    }

  subscribe_to_signals (proxy);

  ret = TRUE;

 out:
  return ret;
}

static void
initable_iface_init (GInitableIface *initable_iface)
{
  initable_iface->init = initable_init;
}

/* ---------------------------------------------------------------------------------------------------- */

static void
async_initable_init_async (GAsyncInitable     *initable,
                           gint                io_priority,
                           GCancellable       *cancellable,
                           GAsyncReadyCallback callback,
                           gpointer            user_data)
{
  GDBusProxy *proxy = G_DBUS_PROXY (initable);

  /* load all properties asynchronously */
  g_dbus_proxy_invoke_method (proxy,
                              "org.freedesktop.DBus.Properties.GetAll",
                              "s",
                              -1,           /* timeout */
                              cancellable,
                              callback,
                              user_data,
                              G_TYPE_STRING, proxy->priv->interface_name,
                              G_TYPE_INVALID);
}

static gboolean
async_initable_init_finish (GAsyncInitable  *initable,
                            GAsyncResult    *res,
                            GError         **error)
{
  GDBusProxy *proxy = G_DBUS_PROXY (initable);
  gboolean ret;

  ret = FALSE;

  if (!g_dbus_proxy_invoke_method_finish (proxy,
                                          "a{sv}",
                                          res,
                                          error,
                                          G_TYPE_HASH_TABLE, &proxy->priv->properties,
                                          G_TYPE_INVALID))
    goto out;

  subscribe_to_signals (proxy);

  ret = TRUE;

 out:
  return ret;
}

static void
async_initable_iface_init (GAsyncInitableIface *async_initable_iface)
{
  async_initable_iface->init_async = async_initable_init_async;
  async_initable_iface->init_finish = async_initable_init_finish;
}

/* ---------------------------------------------------------------------------------------------------- */

/**
 * g_dbus_proxy_new:
 * @connection: A #GDBusConnection.
 * @flags: Flags used when constructing the proxy.
 * @unique_bus_name: A unique bus name.
 * @object_path: An object path.
 * @interface_name: A D-Bus interface name.
 * @cancellable: A #GCancellable or %NULL.
 * @callback: Callback function to invoke when the proxy is ready.
 * @user_data: User data to pass to @callback.
 *
 * Creates a proxy for accessing @interface_name on the remote object at @object_path
 * owned by @unique_bus_name at @connection and asynchronously loads D-Bus properties unless the
 * #G_DBUS_PROXY_FLAGS_DO_NOT_LOAD_PROPERTIES flag is used. Connect to the
 * #GDBusProxy::g-dbus-proxy-properties-changed signal to get notified about property changes.
 *
 * If the #G_DBUS_PROXY_FLAGS_DO_NOT_CONNECT_SIGNALS flag is not set, also sets up
 * match rules for signals. Connect to the #GDBusProxy::g-dbus-proxy-signal signal
 * to handle signals from the remote object.
 *
 * This is a failable asynchronous constructor - when the proxy is
 * ready, @callback will be invoked and you can use
 * g_dbus_proxy_new_finish() to get the result.
 *
 * See g_dbus_proxy_new_sync() and for a synchronous version of this constructor.
 **/
void
g_dbus_proxy_new (GDBusConnection     *connection,
                  GDBusProxyFlags      flags,
                  const gchar         *unique_bus_name,
                  const gchar         *object_path,
                  const gchar         *interface_name,
                  GCancellable        *cancellable,
                  GAsyncReadyCallback  callback,
                  gpointer             user_data)
{
  g_return_if_fail (G_IS_DBUS_CONNECTION (connection));
  g_return_if_fail (unique_bus_name != NULL); /* TODO: check that it is unique */
  g_return_if_fail (object_path != NULL);
  g_return_if_fail (interface_name);

  g_async_initable_new_async (G_TYPE_DBUS_PROXY,
                              G_PRIORITY_DEFAULT,
                              cancellable,
                              callback,
                              user_data,
                              "g-dbus-proxy-flags", flags,
                              "g-dbus-proxy-unique-bus-name", unique_bus_name,
                              "g-dbus-proxy-connection", connection,
                              "g-dbus-proxy-object-path", object_path,
                              "g-dbus-proxy-interface-name", interface_name,
                              NULL);
}

/**
 * g_dbus_proxy_new_finish:
 * @res: A #GAsyncResult obtained from the #GAsyncReadyCallback function passed to g_dbus_proxy_new().
 * @error: Return location for error or %NULL.
 *
 * Finishes creating a #GDBusProxy.
 *
 * Returns: A #GDBusProxy or %NULL if @error is set. Free with g_object_unref().
 **/
GDBusProxy *
g_dbus_proxy_new_finish (GAsyncResult  *res,
                         GError       **error)
{
  GObject *object;
  GObject *source_object;

  source_object = g_async_result_get_source_object (res);
  g_assert (source_object != NULL);

  object = g_async_initable_new_finish (G_ASYNC_INITABLE (source_object),
                                        res,
                                        error);
  g_object_unref (source_object);

  if (object != NULL)
    return G_DBUS_PROXY (object);
  else
    return NULL;
}


/* ---------------------------------------------------------------------------------------------------- */

/**
 * g_dbus_proxy_new_sync:
 * @connection: A #GDBusConnection.
 * @flags: Flags used when constructing the proxy.
 * @unique_bus_name: A unique bus name.
 * @object_path: An object path.
 * @interface_name: A D-Bus interface name.
 * @cancellable: A #GCancellable or %NULL.
 * @error: Return location for error or %NULL.
 *
 * Creates a proxy for accessing @interface_name on the remote object at @object_path
 * owned by @unique_bus_name at @connection and synchronously loads D-Bus properties unless the
 * #G_DBUS_PROXY_FLAGS_DO_NOT_LOAD_PROPERTIES flag is used.
 *
 * If the #G_DBUS_PROXY_FLAGS_DO_NOT_CONNECT_SIGNALS flag is not set, also sets up
 * match rules for signals. Connect to the #GDBusProxy::g-dbus-proxy-signal signal
 * to handle signals from the remote object.
 *
 * This is a synchronous failable constructor. See g_dbus_proxy_new()
 * and g_dbus_proxy_new_finish() for the asynchronous version.
 *
 * Returns: A #GDBusProxy or %NULL if error is set. Free with g_object_unref().
 **/
GDBusProxy *
g_dbus_proxy_new_sync (GDBusConnection     *connection,
                       GDBusProxyFlags      flags,
                       const gchar         *unique_bus_name,
                       const gchar         *object_path,
                       const gchar         *interface_name,
                       GCancellable        *cancellable,
                       GError             **error)
{
  GInitable *initable;

  g_return_val_if_fail (G_IS_DBUS_CONNECTION (connection), NULL);
  g_return_val_if_fail (unique_bus_name != NULL, NULL); /* TODO: check that it is unique */
  g_return_val_if_fail (object_path != NULL, NULL);
  g_return_val_if_fail (interface_name, NULL);

  initable = g_initable_new (G_TYPE_DBUS_PROXY,
                             cancellable,
                             error,
                             "g-dbus-proxy-flags", flags,
                             "g-dbus-proxy-unique-bus-name", unique_bus_name,
                             "g-dbus-proxy-connection", connection,
                             "g-dbus-proxy-object-path", object_path,
                             "g-dbus-proxy-interface-name", interface_name,
                             NULL);
  if (initable != NULL)
    return G_DBUS_PROXY (initable);
  else
    return NULL;
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
 * g_dbus_proxy_get_unique_bus_name:
 * @proxy: A #GDBusProxy.
 *
 * Gets the unique bus name @proxy is for.
 *
 * Returns: A string owned by @proxy. Do not free.
 **/
const gchar *
g_dbus_proxy_get_unique_bus_name (GDBusProxy *proxy)
{
  g_return_val_if_fail (G_IS_DBUS_PROXY (proxy), NULL);
  return proxy->priv->unique_bus_name;
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

  g_simple_async_result_complete_in_idle (simple);
  g_object_unref (simple);
}

static void
g_dbus_proxy_invoke_method_valist (GDBusProxy          *proxy,
                                   const gchar         *method_name,
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

  simple = g_simple_async_result_new (G_OBJECT (proxy),
                                      callback,
                                      user_data,
                                      g_dbus_proxy_invoke_method);

  if (strchr (method_name, '.'))
    {
      gchar *buf;
      gchar *p;
      buf = g_strdup (method_name);
      p = strrchr (buf, '.');
      *p = '\0';
      if ((message = dbus_message_new_method_call (proxy->priv->unique_bus_name,
                                                   proxy->priv->object_path,
                                                   buf,
                                                   p + 1)) == NULL)
        _g_dbus_oom ();
      g_free (buf);
    }
  else
    {
      if ((message = dbus_message_new_method_call (proxy->priv->unique_bus_name,
                                                   proxy->priv->object_path,
                                                   proxy->priv->interface_name,
                                                   method_name)) == NULL)
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

  g_dbus_connection_send_dbus_1_message_with_reply (proxy->priv->connection,
                                                    message,
                                                    timeout_msec,
                                                    cancellable,
                                                    (GAsyncReadyCallback) invoke_method_cb,
                                                    simple);
  dbus_message_unref (message);

 out:
  ;
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

static gboolean
g_dbus_proxy_invoke_method_sync_valist (GDBusProxy          *proxy,
                                        const gchar         *method_name,
                                        const gchar         *in_signature,
                                        const gchar         *out_signature,
                                        guint                timeout_msec,
                                        GCancellable        *cancellable,
                                        GError             **error,
                                        GType                first_in_arg_type,
                                        va_list              va_args)
{
  DBusMessage *message;
  DBusMessage *reply;
  DBusMessageIter iter;
  DBusMessageIter reply_iter;
  DBusError dbus_error;
  GType first_out_arg_type;
  gboolean ret;

  message = NULL;
  reply = NULL;
  ret = FALSE;

  if (strchr (method_name, '.'))
    {
      gchar *buf;
      gchar *p;
      buf = g_strdup (method_name);
      p = strrchr (buf, '.');
      *p = '\0';
      if ((message = dbus_message_new_method_call (proxy->priv->unique_bus_name,
                                                   proxy->priv->object_path,
                                                   buf,
                                                   p + 1)) == NULL)
        _g_dbus_oom ();
      g_free (buf);
    }
  else
    {
      if ((message = dbus_message_new_method_call (proxy->priv->unique_bus_name,
                                                   proxy->priv->object_path,
                                                   proxy->priv->interface_name,
                                                   method_name)) == NULL)
        _g_dbus_oom ();
    }

  /* append args to message */
  dbus_message_iter_init_append (message, &iter);
  if (_gdbus_signature_vararg_foreach (in_signature,
                                       first_in_arg_type,
                                       va_args,
                                       append_values_cb,
                                       &iter))
    goto out;

  reply = g_dbus_connection_send_dbus_1_message_with_reply_sync (proxy->priv->connection,
                                                                 message,
                                                                 timeout_msec,
                                                                 cancellable,
                                                                 error);
  if (reply == NULL)
    goto out;

  /* Translate to GError if the reply is an error */
  dbus_error_init (&dbus_error);
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

  g_assert (va_arg (va_args, GType) == G_TYPE_INVALID);

  first_out_arg_type = va_arg (va_args, GType);
  if (strlen (out_signature) == 0)
    {
      g_assert (first_out_arg_type == G_TYPE_INVALID);
    }
  else
    {
      /* extract values from message */
      dbus_message_iter_init (reply, &reply_iter);
      if (_gdbus_signature_vararg_foreach (out_signature,
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

      g_assert (va_arg (va_args, GType) == G_TYPE_INVALID);
    }

  ret = TRUE;

 out:
  if (message != NULL)
    dbus_message_unref (message);
  if (reply != NULL)
    dbus_message_unref (reply);

  return ret;
}

/**
 * g_dbus_proxy_invoke_method:
 * @proxy: A #GDBusProxy.
 * @method_name: The name of the method to invoke.
 * @signature: The D-Bus signature for all arguments being passed to the method.
 * @timeout_msec: Timeout in milliseconds or -1 for default timeout.
 * @cancellable: A #GCancellable or %NULL.
 * @callback: The callback function to invoke when the reply is ready or %NULL.
 * @user_data: User data to pass to @callback.
 * @first_in_arg_type: The #GType of the first argument to pass or #G_TYPE_INVALID if there are no arguments.
 * @...: The value of the first in argument, followed by zero or more (type, value) pairs, followed by #G_TYPE_INVALID.
 *
 * Invokes the method @method_name on the interface and remote object represented by @proxy. This
 * is an asynchronous operation and when the reply is ready @callback will be invoked (on
 * the main thread) and you can get the result by calling g_dbus_proxy_invoke_method_finish().
 *
 * If @method_name contains any dots, then @name is split into interface and
 * method name parts. This allows using @proxy for invoking methods on
 * other interfaces.
 *
 * Note that @signature and and the supplied (type, value) pairs must match as described in
 * chapter TODO_SECTION_EXPLAINING_DBUS_TO_GTYPE_OBJECT_MAPPING.
 **/
void
g_dbus_proxy_invoke_method (GDBusProxy          *proxy,
                            const gchar         *method_name,
                            const gchar         *signature,
                            guint                timeout_msec,
                            GCancellable        *cancellable,
                            GAsyncReadyCallback  callback,
                            gpointer             user_data,
                            GType                first_in_arg_type,
                            ...)
{
  va_list va_args;

  va_start (va_args, first_in_arg_type);
  g_dbus_proxy_invoke_method_valist (proxy,
                                     method_name,
                                     signature,
                                     timeout_msec,
                                     cancellable,
                                     callback,
                                     user_data,
                                     first_in_arg_type,
                                     va_args);
  va_end (va_args);
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

/**
 * g_dbus_proxy_invoke_method_sync:
 * @proxy: A #GDBusProxy.
 * @method_name: The name of the method to invoke.
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
 * Synchronously invokes the method @method_name on the interface and remote
 * object represented by @proxy.
 *
 * If @method_name contains any dots, then @method_name is split into interface and
 * method name parts. This allows using @proxy for invoking methods on
 * other interfaces.
 *
 * Returns: %TRUE if the call succeeded, %FALSE if @error is set.
 **/
gboolean
g_dbus_proxy_invoke_method_sync (GDBusProxy          *proxy,
                                 const gchar         *method_name,
                                 const gchar         *in_signature,
                                 const gchar         *out_signature,
                                 guint                timeout_msec,
                                 GCancellable        *cancellable,
                                 GError             **error,
                                 GType                first_in_arg_type,
                                 ...)
{
  va_list va_args;
  gboolean ret;

  g_return_val_if_fail (G_IS_DBUS_PROXY (proxy), FALSE);
  /* TODO: other checks */

  ret = FALSE;

  va_start (va_args, first_in_arg_type);
  ret = g_dbus_proxy_invoke_method_sync_valist (proxy,
                                                method_name,
                                                in_signature,
                                                out_signature,
                                                timeout_msec,
                                                cancellable,
                                                error,
                                                first_in_arg_type,
                                                va_args);
  va_end (va_args);

  return ret;
}

#define __G_DBUS_PROXY_C__
#include "gdbusaliasdef.c"
