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

#ifndef __G_DBUS_NAME_OWNING_H__
#define __G_DBUS_NAME_OWNING_H__

#include <gdbus/gdbustypes.h>

G_BEGIN_DECLS

/**
 * GBusNameAcquiredCallback:
 * @connection: The #GDBusConnection on which to acquired the name.
 * @name: The name being owned.
 * @user_data: User data passed to g_bus_own_name().
 *
 * Invoked when the name is acquired.
 */
typedef void (*GBusNameAcquiredCallback) (GDBusConnection *connection,
                                          const gchar     *name,
                                          gpointer         user_data);

/**
 * GBusNameLostCallback:
 * @connection: The #GDBusConnection on which to acquire the name.
 * @name: The name being owned.
 * @user_data: User data passed to g_bus_own_name().
 *
 * Invoked when the name is lost.
 */
typedef void (*GBusNameLostCallback) (GDBusConnection *connection,
                                      const gchar     *name,
                                      gpointer         user_data);

guint g_bus_own_name   (GBusType                  bus_type,
                        const gchar              *name,
                        GBusNameOwnerFlags        flags,
                        GBusNameAcquiredCallback  name_acquired_handler,
                        GBusNameLostCallback      name_lost_handler,
                        gpointer                  user_data);
void  g_bus_unown_name (guint                     owner_id);

G_END_DECLS

#endif /* __G_DBUS_NAME_OWNING_H__ */
