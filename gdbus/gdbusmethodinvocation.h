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

#ifndef __G_DBUS_METHOD_INVOCATION_H__
#define __G_DBUS_METHOD_INVOCATION_H__

#include <gdbus/gdbustypes.h>

G_BEGIN_DECLS

#define G_TYPE_DBUS_METHOD_INVOCATION         (g_dbus_method_invocation_get_type ())
#define G_DBUS_METHOD_INVOCATION(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), G_TYPE_DBUS_METHOD_INVOCATION, GDBusMethodInvocation))
#define G_DBUS_METHOD_INVOCATION_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), G_TYPE_DBUS_METHOD_INVOCATION, GDBusMethodInvocationClass))
#define G_DBUS_METHOD_INVOCATION_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), G_TYPE_DBUS_METHOD_INVOCATION, GDBusMethodInvocationClass))
#define G_IS_DBUS_METHOD_INVOCATION(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), G_TYPE_DBUS_METHOD_INVOCATION))
#define G_IS_DBUS_METHOD_INVOCATION_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), G_TYPE_DBUS_METHOD_INVOCATION))

typedef struct _GDBusMethodInvocationClass   GDBusMethodInvocationClass;
typedef struct _GDBusMethodInvocationPrivate GDBusMethodInvocationPrivate;

/**
 * GDBusMethodInvocation:
 *
 * The #GDBusMethodInvocation structure contains only private data and
 * should only be accessed using the provided API.
 */
struct _GDBusMethodInvocation
{
  /*< private >*/
  GObject parent_instance;
  GDBusMethodInvocationPrivate *priv;
};

/**
 * GDBusMethodInvocationClass:
 *
 * Class structure for #GDBusMethodInvocation.
 */
struct _GDBusMethodInvocationClass
{
  /*< private >*/
  GObjectClass parent_class;

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

GType                  g_dbus_method_invocation_get_type           (void) G_GNUC_CONST;
GDBusMethodInvocation *g_dbus_method_invocation_new                (const gchar           *sender,
                                                                    const gchar           *object_path,
                                                                    const gchar           *interface_name,
                                                                    const gchar           *method_name,
                                                                    GDBusConnection       *connection,
                                                                    GObject               *object,
                                                                    GVariant              *parameters);
const gchar           *g_dbus_method_invocation_get_sender         (GDBusMethodInvocation *invocation);
const gchar           *g_dbus_method_invocation_get_object_path    (GDBusMethodInvocation *invocation);
const gchar           *g_dbus_method_invocation_get_interface_name (GDBusMethodInvocation *invocation);
const gchar           *g_dbus_method_invocation_get_method_name    (GDBusMethodInvocation *invocation);
GDBusConnection       *g_dbus_method_invocation_get_connection     (GDBusMethodInvocation *invocation);
GObject               *g_dbus_method_invocation_get_object         (GDBusMethodInvocation *invocation);
GVariant              *g_dbus_method_invocation_get_parameters     (GDBusMethodInvocation *invocation);


G_END_DECLS

#endif /* __G_DBUS_METHOD_INVOCATION_H__ */
