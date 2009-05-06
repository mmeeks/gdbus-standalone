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

#ifndef __G_DBUS_STRUCTURE_H__
#define __G_DBUS_STRUCTURE_H__

#include <gdbus/gdbustypes.h>

G_BEGIN_DECLS

#define G_TYPE_DBUS_STRUCTURE         (g_dbus_structure_get_type ())
#define G_DBUS_STRUCTURE(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), G_TYPE_DBUS_STRUCTURE, GDBusStructure))
#define G_DBUS_STRUCTURE_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), G_TYPE_DBUS_STRUCTURE, GDBusStructureClass))
#define G_DBUS_STRUCTURE_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), G_TYPE_DBUS_STRUCTURE, GDBusStructureClass))
#define G_IS_DBUS_STRUCTURE(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), G_TYPE_DBUS_STRUCTURE))
#define G_IS_DBUS_STRUCTURE_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), G_TYPE_DBUS_STRUCTURE))

typedef struct _GDBusStructureClass   GDBusStructureClass;
typedef struct _GDBusStructurePrivate GDBusStructurePrivate;

/**
 * GDBusStructure:
 *
 * The #GDBusStructure structure contains only private data and
 * should only be accessed using the provided API.
 */
struct _GDBusStructure
{
  /*< private >*/
  GObject parent_instance;
  GDBusStructurePrivate *priv;
};

/**
 * GDBusStructureClass:
 *
 * Class structure for #GDBusStructure.
 */
struct _GDBusStructureClass
{
  /*< private >*/
  GObjectClass parent_class;

  /*< public >*/

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

GType           g_dbus_structure_get_type                   (void) G_GNUC_CONST;
gboolean        g_dbus_structure_equal                      (GDBusStructure  *structure1,
                                                             GDBusStructure  *structure2);
GDBusStructure *g_dbus_structure_new                        (const gchar     *signature,
                                                             GType            first_element_type,
                                                             ...);
const gchar    *g_dbus_structure_get_signature              (GDBusStructure  *structure);
guint           g_dbus_structure_get_num_elements           (GDBusStructure  *structure);
const gchar    *g_dbus_structure_get_signature_for_element  (GDBusStructure  *structure,
                                                             guint            element);
void            g_dbus_structure_get_element                (GDBusStructure  *structure,
                                                             guint            first_element_number,
                                                             ...);
void            g_dbus_structure_set_element                (GDBusStructure  *structure,
                                                             guint            first_element_number,
                                                             ...);

G_END_DECLS

#endif /* __G_DBUS_STRUCTURE_H__ */
