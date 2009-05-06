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

#ifndef __G_BUS_NAME_OWNER_H__
#define __G_BUS_NAME_OWNER_H__

#include <gdbus/gdbustypes.h>

G_BEGIN_DECLS

#define G_TYPE_BUS_NAME_OWNER         (g_bus_name_owner_get_type ())
#define G_BUS_NAME_OWNER(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), G_TYPE_BUS_NAME_OWNER, GBusNameOwner))
#define G_BUS_NAME_OWNER_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), G_TYPE_BUS_NAME_OWNER, GBusNameOwnerClass))
#define G_BUS_NAME_OWNER_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), G_TYPE_BUS_NAME_OWNER, GBusNameOwnerClass))
#define G_IS_BUS_NAME_OWNER(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), G_TYPE_BUS_NAME_OWNER))
#define G_IS_BUS_NAME_OWNER_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), G_TYPE_BUS_NAME_OWNER))

typedef struct _GBusNameOwnerClass   GBusNameOwnerClass;
typedef struct _GBusNameOwnerPrivate GBusNameOwnerPrivate;

/**
 * GBusNameOwner:
 *
 * The #GBusNameOwner structure contains only private data and
 * should only be accessed using the provided API.
 */
struct _GBusNameOwner
{
  /*< private >*/
  GObject parent_instance;
  GBusNameOwnerPrivate *priv;
};

/**
 * GBusNameOwnerClass:
 * @name_acquired: Signal class handler for the #GBusNameOwner::name-acquired signal.
 * @name_lost: Signal class handler for the #GBusNameOwner::name-lost signal.
 * @initialized: Signal class handler for the #GBusNameOwner::initialized signal.
 *
 * Class structure for #GBusNameOwner.
 */
struct _GBusNameOwnerClass
{
  /*< private >*/
  GObjectClass parent_class;

  /*< public >*/

  /* Signals */
  void (*name_acquired) (GBusNameOwner *owner);
  void (*name_lost)     (GBusNameOwner *owner);
  void (*initialized)   (GBusNameOwner *owner);

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

GType               g_bus_name_owner_get_type                  (void) G_GNUC_CONST;
GBusNameOwner      *g_bus_name_owner_new                       (GBusType               bus_type,
                                                                const gchar           *name,
                                                                GBusNameOwnerFlags     flags);
GBusNameOwner      *g_bus_name_owner_new_for_connection        (GDBusConnection       *connection,
                                                                const gchar           *name,
                                                                GBusNameOwnerFlags     flags);
gboolean            g_bus_name_owner_get_owns_name             (GBusNameOwner         *owner);
const gchar        *g_bus_name_owner_get_name                  (GBusNameOwner         *owner);
GBusType            g_bus_name_owner_get_bus_type              (GBusNameOwner         *owner);
GBusNameOwnerFlags  g_bus_name_owner_get_flags                 (GBusNameOwner         *owner);
gboolean            g_bus_name_owner_get_is_initialized        (GBusNameOwner         *owner);
GDBusConnection    *g_bus_name_owner_get_connection            (GBusNameOwner         *owner);

G_END_DECLS

#endif /* __G_BUS_NAME_OWNER_H__ */
