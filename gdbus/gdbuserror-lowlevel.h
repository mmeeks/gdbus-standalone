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

#ifndef __G_DBUS_ERROR_LOWLEVEL_H__
#define __G_DBUS_ERROR_LOWLEVEL_H__

#include <gdbus/gdbus-lowlevel.h>

G_BEGIN_DECLS

/* Map DBusError to GError (only intended for object mappings) */

GError *g_dbus_error_new_for_dbus_error_valist (DBusError   *dbus_error,
                                                GType       *error_types,
                                                const gchar *prepend_format,
                                                va_list      va_args);

GError *g_dbus_error_new_for_dbus_error (DBusError   *dbus_error,
                                         GType       *error_types,
                                         const gchar *prepend_format,
                                         ...);

void g_dbus_error_set_dbus_error (GError      **error,
                                  DBusError    *dbus_error,
                                  GType        *error_types,
                                  const gchar  *prepend_format,
                                  ...);

void g_dbus_error_set_dbus_error_valist (GError      **error,
                                         DBusError    *dbus_error,
                                         GType        *error_types,
                                         const gchar  *prepend_format,
                                         va_list       va_args);

/* Map GError to DBusError (only intended for object mappings) */

void g_dbus_error_new_for_gerror (GError    *error,
                                  DBusError *dbus_error);

G_END_DECLS

#endif /* __G_DBUS_ERROR_LOWLEVEL_H__ */
