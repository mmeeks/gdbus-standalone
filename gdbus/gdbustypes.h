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

#ifndef __G_DBUS_TYPES_H__
#define __G_DBUS_TYPES_H__

#include <gdbus/gdbusenums.h>

G_BEGIN_DECLS

typedef struct _GDBusConnection       GDBusConnection;
typedef struct _GDBusProxy            GDBusProxy;

typedef struct _GDBusInterfaceVTable  GDBusInterfaceVTable;

typedef struct _GDBusAnnotationInfo   GDBusAnnotationInfo;
typedef struct _GDBusArgInfo          GDBusArgInfo;
typedef struct _GDBusMethodInfo       GDBusMethodInfo;
typedef struct _GDBusSignalInfo       GDBusSignalInfo;
typedef struct _GDBusPropertyInfo     GDBusPropertyInfo;
typedef struct _GDBusInterfaceInfo    GDBusInterfaceInfo;
typedef struct _GDBusNodeInfo         GDBusNodeInfo;

G_END_DECLS

#endif /* __G_DBUS_TYPES_H__ */
