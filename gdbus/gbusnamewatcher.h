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

#ifndef __G_BUS_NAME_WATCHER_H__
#define __G_BUS_NAME_WATCHER_H__

#include <gdbus/gdbustypes.h>

G_BEGIN_DECLS

#define G_TYPE_BUS_NAME_WATCHER         (g_bus_name_watcher_get_type ())
#define G_BUS_NAME_WATCHER(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), G_TYPE_BUS_NAME_WATCHER, GBusNameWatcher))
#define G_BUS_NAME_WATCHER_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), G_TYPE_BUS_NAME_WATCHER, GBusNameWatcherClass))
#define G_BUS_NAME_WATCHER_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), G_TYPE_BUS_NAME_WATCHER, GBusNameWatcherClass))
#define G_IS_BUS_NAME_WATCHER(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), G_TYPE_BUS_NAME_WATCHER))
#define G_IS_BUS_NAME_WATCHER_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), G_TYPE_BUS_NAME_WATCHER))

typedef struct _GBusNameWatcherClass   GBusNameWatcherClass;
typedef struct _GBusNameWatcherPrivate GBusNameWatcherPrivate;

/**
 * GBusNameWatcher:
 *
 * The #GBusNameWatcher structure contains only private data and
 * should only be accessed using the provided API.
 */
struct _GBusNameWatcher
{
  /*< private >*/
  GObject parent_instance;
  GBusNameWatcherPrivate *priv;
};

/**
 * GBusNameWatcherClass:
 * @name_appeared: Signal class handler for the #GBusNameWatcher::name-appeared signal.
 * @name_vanished: Signal class handler for the #GBusNameWatcher::name-vanished signal.
 * @initialized: Signal class handler for the #GBusNameWatcher::initialized signal.
 *
 * Class structure for #GBusNameWatcher.
 */
struct _GBusNameWatcherClass
{
  /*< private >*/
  GObjectClass parent_class;

  /*< public >*/

  /* Signals */
  void (*name_appeared) (GBusNameWatcher *watcher);
  void (*name_vanished) (GBusNameWatcher *watcher);
  void (*initialized)   (GBusNameWatcher *watcher);

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

GType               g_bus_name_watcher_get_type                  (void) G_GNUC_CONST;
GBusNameWatcher    *g_bus_name_watcher_new                       (GBusType               bus_type,
                                                                  const gchar           *name);
GBusNameWatcher    *g_bus_name_watcher_new_for_connection        (GDBusConnection       *connection,
                                                                  const gchar           *name);
const gchar        *g_bus_name_watcher_get_name_owner            (GBusNameWatcher       *watcher);
const gchar        *g_bus_name_watcher_get_name                  (GBusNameWatcher       *watcher);
GBusType            g_bus_name_watcher_get_bus_type              (GBusNameWatcher       *watcher);
gboolean            g_bus_name_watcher_get_is_initialized        (GBusNameWatcher       *watcher);
GDBusConnection    *g_bus_name_watcher_get_connection            (GBusNameWatcher       *watcher);

G_END_DECLS

#endif /* __G_BUS_NAME_WATCHER_H__ */
