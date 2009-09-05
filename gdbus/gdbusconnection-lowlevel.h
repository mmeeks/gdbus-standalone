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

#if !defined (__G_DBUS_G_DBUS_LOWLEVEL_H_INSIDE__) && !defined (G_DBUS_COMPILATION)
#error "Only <gdbus/gdbus-lowlevel.h> can be included directly."
#endif

#ifndef __G_DBUS_CONNECTION_LOWLEVEL_H__
#define __G_DBUS_CONNECTION_LOWLEVEL_H__

#include <gdbus/gdbus-lowlevel.h>
#include <dbus/dbus.h>

G_BEGIN_DECLS

/**
 * GDBusSignalCallback1:
 * @connection: A #GDBusConnection.
 * @message: A #DBusMessage.
 * @user_data: User data passed when subscribing to the signal.
 *
 * Signature for callback function used in g_dbus_connection_dbus_1_signal_subscribe().
 */
typedef void (*GDBusSignalCallback1) (GDBusConnection  *connection,
                                      DBusMessage      *message,
                                      gpointer          user_data);

DBusConnection  *g_dbus_connection_get_dbus_1_connection                    (GDBusConnection    *connection);
void             g_dbus_connection_send_dbus_1_message                      (GDBusConnection    *connection,
                                                                             DBusMessage        *message);
void             g_dbus_connection_send_dbus_1_message_with_reply           (GDBusConnection    *connection,
                                                                             DBusMessage        *message,
                                                                             gint                timeout_msec,
                                                                             GCancellable       *cancellable,
                                                                             GAsyncReadyCallback callback,
                                                                             gpointer            user_data);
DBusMessage     *g_dbus_connection_send_dbus_1_message_with_reply_finish    (GDBusConnection    *connection,
                                                                             GAsyncResult       *res,
                                                                             GError            **error);
DBusMessage     *g_dbus_connection_send_dbus_1_message_with_reply_sync      (GDBusConnection    *connection,
                                                                             DBusMessage        *message,
                                                                             gint                timeout_msec,
                                                                             GCancellable       *cancellable,
                                                                             GError            **error);
guint            g_dbus_connection_dbus_1_signal_subscribe                  (GDBusConnection    *connection,
                                                                             const gchar        *sender,
                                                                             const gchar        *interface_name,
                                                                             const gchar        *member,
                                                                             const gchar        *object_path,
                                                                             const gchar        *arg0,
                                                                             GDBusSignalCallback1 callback,
                                                                             gpointer            user_data,
                                                                             GDestroyNotify      user_data_free_func);
void             g_dbus_connection_dbus_1_signal_unsubscribe                (GDBusConnection    *connection,
                                                                             guint               subscription_id);

G_END_DECLS

#endif /* __G_DBUS_CONNECTION_LOWLEVEL_H__ */
