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

#ifndef __G_DBUS_CONNECTION_H__
#define __G_DBUS_CONNECTION_H__

#include <gdbus/gdbustypes.h>

G_BEGIN_DECLS

#define G_TYPE_DBUS_CONNECTION         (g_dbus_connection_get_type ())
#define G_DBUS_CONNECTION(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), G_TYPE_DBUS_CONNECTION, GDBusConnection))
#define G_DBUS_CONNECTION_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), G_TYPE_DBUS_CONNECTION, GDBusConnectionClass))
#define G_DBUS_CONNECTION_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), G_TYPE_DBUS_CONNECTION, GDBusConnectionClass))
#define G_IS_DBUS_CONNECTION(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), G_TYPE_DBUS_CONNECTION))
#define G_IS_DBUS_CONNECTION_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), G_TYPE_DBUS_CONNECTION))

typedef struct _GDBusConnectionClass   GDBusConnectionClass;
typedef struct _GDBusConnectionPrivate GDBusConnectionPrivate;

/**
 * GDBusConnection:
 *
 * The #GDBusConnection structure contains only private data and
 * should only be accessed using the provided API.
 */
struct _GDBusConnection
{
  /*< private >*/
  GObject parent_instance;
  GDBusConnectionPrivate *priv;
};

/**
 * GDBusConnectionClass:
 * @opened: Signal class handler for the #GDBusConnection::opened signal.
 * @closed: Signal class handler for the #GDBusConnection::closed signal.
 * @initialized: Signal class handler for the #GDBusConnection::initialized signal.
 *
 * Class structure for #GDBusConnection.
 */
struct _GDBusConnectionClass
{
  /*< private >*/
  GObjectClass parent_class;

  /*< public >*/

  /* Signals */
  void (*opened)      (GDBusConnection *connection);
  void (*closed)      (GDBusConnection *connection);
  void (*initialized) (GDBusConnection *connection);

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

GType            g_dbus_connection_get_type                   (void) G_GNUC_CONST;
GDBusConnection *g_dbus_connection_bus_get                    (GBusType            bus_type);
GDBusConnection *g_dbus_connection_bus_get_private            (GBusType            bus_type);
GBusType         g_dbus_connection_get_bus_type               (GDBusConnection    *connection);
gboolean         g_dbus_connection_get_is_open                (GDBusConnection    *connection);
gboolean         g_dbus_connection_get_is_initialized         (GDBusConnection    *connection);
gboolean         g_dbus_connection_get_is_private             (GDBusConnection    *connection);
const gchar     *g_dbus_connection_get_unique_name            (GDBusConnection    *connection);
gboolean         g_dbus_connection_get_exit_on_close          (GDBusConnection    *connection);
void             g_dbus_connection_set_exit_on_close          (GDBusConnection    *connection,
                                                               gboolean            exit_on_close);

/* The following is only for the C object mapping and should not be bound to other languages */

/**
 * GDBusSignalCallback:
 * @connection: A #GDBusConnection.
 * @name: The name of the signal.
 * @signature: The signature of the signal.
 * @args: A #GPtrArray containing one #GDBusVariant for each argument of the signal.
 * @user_data: User data passed when subscribing to the signal.
 *
 * Signature for callback function used in g_dbus_connection_signal_subscribe().
 */
typedef void (*GDBusSignalCallback) (GDBusConnection  *connection,
                                     const gchar      *name,
                                     const gchar      *signature,
                                     GPtrArray        *args,
                                     gpointer          user_data);

guint            g_dbus_connection_signal_subscribe           (GDBusConnection     *connection,
                                                               const gchar         *sender,
                                                               const gchar         *interface_name,
                                                               const gchar         *member,
                                                               const gchar         *object_path,
                                                               const gchar         *arg0,
                                                               GDBusSignalCallback  callback,
                                                               gpointer             user_data,
                                                               GDestroyNotify      user_data_free_func);
void             g_dbus_connection_signal_unsubscribe         (GDBusConnection     *connection,
                                                               guint                subscription_id);

G_END_DECLS

#endif /* __G_DBUS_CONNECTION_H__ */
