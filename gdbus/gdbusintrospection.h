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

#ifndef __G_DBUS_INTROSPECTION_H__
#define __G_DBUS_INTROSPECTION_H__

#include <gdbus/gdbustypes.h>

G_BEGIN_DECLS

/**
 * GDBusAnnotationInfo:
 * @key: The name of the annotation, e.g. <literal>org.freedesktop.DBus.Deprecated</literal>
 * @value: The value of the annotation.
 * @annotations: A pointer to an array of annotations for the annotation or %NULL if there are no annotations.
 *
 * Information about an annotation.
 *
 * By convention, an array of annotations is always terminated by an element
 * where @key is %NULL.
 */
struct _GDBusAnnotationInfo
{
  const gchar                *key;
  const gchar                *value;
  const GDBusAnnotationInfo  *annotations;
};

/**
 * GDBusArgInfo:
 * @name: Name of the argument, e.g. @unix_user_id.
 * @signature: D-Bus signature of the argument (a single complete type).
 * @annotations: A pointer to an array of annotations for the argument or %NULL if there are no annotations.
 *
 * Information about an argument for a method or a signal.
 */
struct _GDBusArgInfo
{
  const gchar                *name;
  const gchar                *signature;
  const GDBusAnnotationInfo  *annotations;
};

/**
 * GDBusMethodInfo:
 * @name: The name of the D-Bus method, e.g. @RequestName.
 * @in_signature: The combined D-Bus signature of all arguments passed to the method (@in_num_args complete types).
 * @in_num_args: Number of arguments passed to the method.
 * @in_args: A pointer to an array of @in_num_args #GDBusArgInfo structures or %NULL if @in_num_args is 0.
 * @out_signature: The combined D-Bus signature of all arguments the method returns (@out_num_args complete types).
 * @out_num_args: Number of arguments the method returns.
 * @out_args: A pointer to an array of @out_num_args #GDBusArgInfo structures or %NULL if @out_num_args is 0.
 * @annotations: A pointer to an array of annotations for the method or %NULL if there are no annotations.
 *
 * Information about a method on an D-Bus interface.
 */
struct _GDBusMethodInfo
{
  const gchar                *name;

  const gchar                *in_signature;
  guint                       in_num_args;
  const GDBusArgInfo         *in_args;

  const gchar                *out_signature;
  guint                       out_num_args;
  const GDBusArgInfo         *out_args;

  const GDBusAnnotationInfo  *annotations;
};

/**
 * GDBusSignalInfo:
 * @name: The name of the D-Bus signal, e.g. @NameOwnerChanged.
 * @signature: The combined D-Bus signature of all arguments of the signal (@num_args complete types).
 * @num_args: Number of arguments of the signal.
 * @args: A pointer to an array of @num_args #GDBusArgInfo structures or %NULL if @num_args is 0.
 * @annotations: A pointer to an array of annotations for the signal or %NULL if there are no annotations.
 *
 * Information about a signal on a D-Bus interface.
 */
struct _GDBusSignalInfo
{
  const gchar                *name;

  const gchar                *signature;
  guint                       num_args;
  const GDBusArgInfo         *args;

  const GDBusAnnotationInfo  *annotations;
};

/**
 * GDBusPropertyInfo:
 * @name: The name of the D-Bus property, e.g. @SupportedFilesystems.
 * @signature: The D-Bus signature of the property (a single complete type).
 * @flags: Access control flags for the property.
 * @annotations: A pointer to an array of annotations for the property or %NULL if there are no annotations.
 *
 * Information about a D-Bus property on a D-Bus interface.
 */
struct _GDBusPropertyInfo
{
  const gchar                *name;
  const gchar                *signature;
  GDBusPropertyInfoFlags      flags;
  const GDBusAnnotationInfo  *annotations;
};

/**
 * GDBusInterfaceInfo:
 * @name: The name of the D-Bus interface, e.g. <literal>org.freedesktop.DBus.Properties</literal>.
 * @num_methods: Number of methods on the interface.
 * @methods: A pointer to an array of @num_methods #GDBusMethodInfo structures or %NULL if @num_methods is 0.
 * @num_signals: Number of signals on the interface.
 * @signals: A pointer to an array of @num_signals #GDBusSignalInfo structures or %NULL if @num_signals is 0.
 * @num_properties: Number of properties on the interface.
 * @properties: A pointer to an array of @num_properties #GDBusPropertyInfo structures or %NULL if @num_properties is 0.
 * @annotations: A pointer to an array of annotations for the interface or %NULL if there are no annotations.
 *
 * Information about a D-Bus interface.
 */
struct _GDBusInterfaceInfo
{
  const gchar                *name;

  guint                       num_methods;
  const GDBusMethodInfo      *methods;

  guint                       num_signals;
  const GDBusSignalInfo      *signals;

  guint                       num_properties;
  const GDBusPropertyInfo    *properties;

  const GDBusAnnotationInfo  *annotations;
};

/**
 * GDBusNodeInfo:
 * @path: The path of the node or %NULL if omitted. Note that this may be a relative path. See the D-Bus specification for more details.
 * @num_interfaces: Number of interfaces of the node.
 * @interfaces: A pointer to an array of @num_interfaces #GDBusInterfaceInfo structures or %NULL if @num_interfaces is 0.
 * @num_nodes: Number of child nodes.
 * @nodes: A pointer to an array of @num_nodes #GDBusNodeInfo structures or %NULL if @num_nodes is 0.
 * @annotations: A pointer to an array of annotations for the node or %NULL if there are no annotations.
 *
 * Information about nodes in a remote object hierarchy.
 */
struct _GDBusNodeInfo
{
  const gchar                *path;

  guint                       num_interfaces;
  const GDBusInterfaceInfo   *interfaces;

  guint                       num_nodes;
  const GDBusNodeInfo        *nodes;

  const GDBusAnnotationInfo  *annotations;
};

const gchar              *g_dbus_annotation_info_lookup          (const GDBusAnnotationInfo *annotations,
                                                                  const gchar               *name);
const GDBusMethodInfo    *g_dbus_interface_info_lookup_method    (const GDBusInterfaceInfo  *interface_info,
                                                                  const gchar               *name);
const GDBusSignalInfo    *g_dbus_interface_info_lookup_signal    (const GDBusInterfaceInfo  *interface_info,
                                                                  const gchar               *name);
const GDBusPropertyInfo  *g_dbus_interface_info_lookup_property  (const GDBusInterfaceInfo  *interface_info,
                                                                  const gchar               *name);
void                      g_dbus_interface_info_generate_xml     (const GDBusInterfaceInfo  *interface_info,
                                                                  guint                      indent,
                                                                  GString                   *string_builder);

GDBusNodeInfo            *g_dbus_node_info_new_for_xml           (const gchar               *xml_data,
                                                                  GError                   **error);
const GDBusInterfaceInfo *g_dbus_node_info_lookup_interface      (const GDBusNodeInfo       *node_info,
                                                                  const gchar               *name);
void                      g_dbus_node_info_free                  (GDBusNodeInfo             *node_info);
void                      g_dbus_node_info_generate_xml          (const GDBusNodeInfo       *node_info,
                                                                  guint                      indent,
                                                                  GString                   *string_builder);

G_END_DECLS

#endif /* __G_DBUS_INTROSPECTION_H__ */
