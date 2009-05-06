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

#ifndef __G_DBUS_MAINLOOP_LOWLEVEL_H__
#define __G_DBUS_MAINLOOP_LOWLEVEL_H__

#include <gdbus/gdbustypes.h>
#include <dbus/dbus.h>

G_BEGIN_DECLS

void g_dbus_integrate_dbus_1_connection   (DBusConnection *connection,
                                           GMainContext   *context);
void g_dbus_unintegrate_dbus_1_connection (DBusConnection *connection);

void g_dbus_integrate_dbus_1_server       (DBusServer   *server,
                                           GMainContext *context);
void g_dbus_unintegrate_dbus_1_server     (DBusServer   *server);

G_END_DECLS

#endif /* __G_DBUS_MAINLOOP_LOWLEVEL_H__ */
