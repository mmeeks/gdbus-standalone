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

#if !defined (__G_DBUS_G_DBUS_H_INSIDE__) && !defined (G_DBUS_COMPILATION)
#error "Only <gdbus/gdbus.h> can be included directly."
#endif

#ifndef __G_DBUS_PROXY_H__
#define __G_DBUS_PROXY_H__

#include <gdbus/gdbustypes.h>

G_BEGIN_DECLS

#define G_TYPE_DBUS_PROXY         (g_dbus_proxy_get_type ())
#define G_DBUS_PROXY(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), G_TYPE_DBUS_PROXY, GDBusProxy))
#define G_DBUS_PROXY_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), G_TYPE_DBUS_PROXY, GDBusProxyClass))
#define G_DBUS_PROXY_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), G_TYPE_DBUS_PROXY, GDBusProxyClass))
#define G_IS_DBUS_PROXY(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), G_TYPE_DBUS_PROXY))
#define G_IS_DBUS_PROXY_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), G_TYPE_DBUS_PROXY))

typedef struct _GDBusProxyClass   GDBusProxyClass;
typedef struct _GDBusProxyPrivate GDBusProxyPrivate;

/**
 * GDBusProxy:
 *
 * The #GDBusProxy structure contains only private data and
 * should only be accessed using the provided API.
 */
struct _GDBusProxy
{
  /*< private >*/
  GObject parent_instance;
  GDBusProxyPrivate *priv;
};

/**
 * GDBusProxyClass:
 * @properties_changed: Signal class handler for the #GDBusProxy::g-dbus-proxy-properties-changed signal.
 * @signal: Signal class handler for the #GDBusProxy::g-dbus-proxy-signal signal.
 *
 * Class structure for #GDBusProxy.
 */
struct _GDBusProxyClass
{
  /*< private >*/
  GObjectClass parent_class;

  /*< public >*/
  void (*properties_changed)           (GDBusProxy   *proxy,
                                        GHashTable   *changed_properties);
  void (*signal)                       (GDBusProxy   *proxy,
                                        const gchar  *signal_name,
                                        const gchar  *signature,
                                        GPtrArray    *args);

  /*< private >*/
  /* Padding for future expansion */
  void (*_g_reserved1) (void);
  void (*_g_reserved2) (void);
  void (*_g_reserved3) (void);
  void (*_g_reserved4) (void);
  void (*_g_reserved5) (void);
  void (*_g_reserved6) (void);
  void (*_g_reserved7) (void);
  void (*_g_reserved8) (void);
};

GType            g_dbus_proxy_get_type                    (void) G_GNUC_CONST;
void             g_dbus_proxy_new                         (GDBusConnection     *connection,
                                                           GDBusProxyFlags      flags,
                                                           const gchar         *unique_bus_name,
                                                           const gchar         *object_path,
                                                           const gchar         *interface_name,
                                                           GCancellable        *cancellable,
                                                           GAsyncReadyCallback  callback,
                                                           gpointer             user_data);
GDBusProxy      *g_dbus_proxy_new_finish                  (GAsyncResult        *res,
                                                           GError             **error);
GDBusProxy      *g_dbus_proxy_new_sync                    (GDBusConnection     *connection,
                                                           GDBusProxyFlags      flags,
                                                           const gchar         *unique_bus_name,
                                                           const gchar         *object_path,
                                                           const gchar         *interface_name,
                                                           GCancellable        *cancellable,
                                                           GError             **error);
GDBusConnection *g_dbus_proxy_get_connection              (GDBusProxy          *proxy);
GDBusProxyFlags  g_dbus_proxy_get_flags                   (GDBusProxy          *proxy);
const gchar     *g_dbus_proxy_get_unique_bus_name         (GDBusProxy          *proxy);
const gchar     *g_dbus_proxy_get_object_path             (GDBusProxy          *proxy);
const gchar     *g_dbus_proxy_get_interface_name          (GDBusProxy          *proxy);
void             g_dbus_proxy_invoke_method               (GDBusProxy          *proxy,
                                                           const gchar         *method_name,
                                                           const gchar         *signature,
                                                           guint                timeout_msec,
                                                           GCancellable        *cancellable,
                                                           GAsyncReadyCallback  callback,
                                                           gpointer             user_data,
                                                           GType                first_in_arg_type,
                                                           ...);
gboolean         g_dbus_proxy_invoke_method_finish        (GDBusProxy          *proxy,
                                                           const gchar         *signature,
                                                           GAsyncResult        *res,
                                                           GError             **error,
                                                           GType                first_out_arg_type,
                                                           ...);
gboolean         g_dbus_proxy_invoke_method_sync          (GDBusProxy          *proxy,
                                                           const gchar         *method_name,
                                                           const gchar         *in_signature,
                                                           const gchar         *out_signature,
                                                           guint                timeout_msec,
                                                           GCancellable        *cancellable,
                                                           GError             **error,
                                                           GType                first_in_arg_type,
                                                           ...);
GDBusVariant    *g_dbus_proxy_get_cached_property         (GDBusProxy          *proxy,
                                                           const gchar         *property_name,
                                                           GError             **error);

G_END_DECLS

#endif /* __G_DBUS_PROXY_H__ */
