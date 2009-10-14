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

#include "config.h"

#include <stdlib.h>

#include <glib/gi18n.h>

#include "gdbusintrospection.h"

/**
 * SECTION:gdbusintrospection
 * @title: Introspection XML
 * @short_description: Parse and Generate Introspection XML
 * @include: gdbus/gdbus.h
 *
 * Various data structures and convenience routines to parse and
 * generate D-Bus introspection XML.
 */

/* ---------------------------------------------------------------------------------------------------- */

typedef struct
{
  /* stuff we are currently collecting */
  GArray *args;
  GArray *out_args;
  GArray *methods;
  GArray *signals;
  GArray *properties;
  GArray *interfaces;
  GArray *nodes;
  GArray *annotations;

  /* A list of GArray's containing annotations */
  GSList *annotations_stack;

  /* A list of GArray's containing interfaces */
  GSList *interfaces_stack;

  /* A list of GArray's containing nodes */
  GSList *nodes_stack;

  /* Whether the direction was "in" for last parsed arg */
  gboolean last_arg_was_in;

  /* Number of args currently being collected; used for assigning
   * names to args without a "name" attribute
   */
  guint num_args;

} ParseData;

/* ---------------------------------------------------------------------------------------------------- */

static void g_dbus_annotation_info_free (GDBusAnnotationInfo *info);

static void
g_dbus_introspector_free_annotation_array (GDBusAnnotationInfo *annotations)
{
  guint n;
  for (n = 0; annotations != NULL && annotations[n].key != NULL; n++)
    g_dbus_annotation_info_free (&(annotations[n]));
  g_free (annotations);
}

static void
g_dbus_annotation_info_free (GDBusAnnotationInfo *info)
{
  g_free ((gpointer) info->key);
  g_free ((gpointer) info->value);
  g_dbus_introspector_free_annotation_array ((GDBusAnnotationInfo *) info->annotations);
}

static void
g_dbus_arg_info_free (GDBusArgInfo *info)
{
  g_free ((gpointer) info->name);
  g_free ((gpointer) info->signature);
  g_dbus_introspector_free_annotation_array ((GDBusAnnotationInfo *) info->annotations);
}

static void
g_dbus_method_info_free (GDBusMethodInfo *info)
{
  guint n;

  g_free ((gpointer) info->name);

  g_free ((gpointer) info->in_signature);
  for (n = 0; n < info->in_num_args; n++)
    g_dbus_arg_info_free ((GDBusArgInfo *) &(info->in_args[n]));
  g_free ((gpointer) info->in_args);

  g_free ((gpointer) info->out_signature);
  for (n = 0; n < info->out_num_args; n++)
    g_dbus_arg_info_free ((GDBusArgInfo *) &(info->out_args[n]));
  g_free ((gpointer) info->out_args);

  g_dbus_introspector_free_annotation_array ((GDBusAnnotationInfo *) info->annotations);
}

static void
g_dbus_signal_info_free (GDBusSignalInfo *info)
{
  guint n;

  g_free ((gpointer) info->name);
  g_free ((gpointer) info->g_name);

  g_free ((gpointer) info->signature);
  for (n = 0; n < info->num_args; n++)
    g_dbus_arg_info_free ((GDBusArgInfo *) &(info->args[n]));
  g_free ((gpointer) info->args);

  g_dbus_introspector_free_annotation_array ((GDBusAnnotationInfo *) info->annotations);
}

static void
g_dbus_property_info_free (GDBusPropertyInfo *info)
{
  g_free ((gpointer) info->name);
  g_free ((gpointer) info->g_name);
  g_free ((gpointer) info->signature);
  g_dbus_introspector_free_annotation_array ((GDBusAnnotationInfo *) info->annotations);
}

static void
g_dbus_interface_info_free (GDBusInterfaceInfo *info)
{
  guint n;

  g_free ((gpointer) info->name);

  for (n = 0; n < info->num_methods; n++)
    g_dbus_method_info_free ((GDBusMethodInfo *) &(info->methods[n]));
  g_free ((gpointer) info->methods);

  for (n = 0; n < info->num_signals; n++)
    g_dbus_signal_info_free ((GDBusSignalInfo *) &(info->signals[n]));
  g_free ((gpointer) info->signals);

  for (n = 0; n < info->num_properties; n++)
    g_dbus_property_info_free ((GDBusPropertyInfo *) &(info->properties[n]));
  g_free ((gpointer) info->properties);

  g_dbus_introspector_free_annotation_array ((GDBusAnnotationInfo *) info->annotations);
}

/**
 * g_dbus_node_info_free:
 * @node_info: A #GDBusNodeInfo.
 *
 * Frees @node_info. This is a deep free, all nodes of @node_info and
 * its children will be freed as well.
 */
void
g_dbus_node_info_free (GDBusNodeInfo *node_info)
{
  guint n;

  g_free ((gpointer) node_info->path);

  for (n = 0; n < node_info->num_interfaces; n++)
    g_dbus_interface_info_free ((GDBusInterfaceInfo *) &(node_info->interfaces[n]));
  g_free ((gpointer) node_info->interfaces);

  for (n = 0; n < node_info->num_nodes; n++)
    g_dbus_node_info_free ((GDBusNodeInfo *) &(node_info->nodes[n]));
  g_free ((gpointer) node_info->nodes);

  g_dbus_introspector_free_annotation_array ((GDBusAnnotationInfo *) node_info->annotations);
}

/* ---------------------------------------------------------------------------------------------------- */

static void
g_dbus_annotation_info_set (ParseData                      *data,
                                        GDBusAnnotationInfo *info,
                                        const gchar                    *key,
                                        const gchar                    *value,
                                        GDBusAnnotationInfo *embedded_annotations)
{
  if (key != NULL)
    info->key = g_strdup (key);

  if (value != NULL)
    info->value = g_strdup (value);

  if (embedded_annotations != NULL)
    info->annotations = embedded_annotations;
}

static void
g_dbus_arg_info_set (ParseData           *data,
                     GDBusArgInfo        *info,
                     const gchar         *name,
                     const gchar         *signature,
                     GDBusAnnotationInfo *annotations)
{
  /* name may be NULL - TODO: compute name? */
  if (name != NULL)
    info->name = g_strdup (name);

  if (signature != NULL)
    info->signature = g_strdup (signature);

  if (annotations != NULL)
    info->annotations = annotations;
}

static gchar *
compute_signature (GDBusArgInfo  *args,
                   guint          num_args)
{
  GString *s;
  guint n;

  s = g_string_new ("");
  for (n = 0; n < num_args; n++)
    g_string_append (s, args[n].signature);

  return g_string_free (s, FALSE);
}

static void
g_dbus_method_info_set (ParseData           *data,
                        GDBusMethodInfo     *info,
                        const gchar         *name,
                        guint                in_num_args,
                        GDBusArgInfo        *in_args,
                        guint                out_num_args,
                        GDBusArgInfo        *out_args,
                        GDBusAnnotationInfo *annotations)
{
  if (name != NULL)
    info->name = g_strdup (name);

  if (in_num_args != 0)
    {
      info->in_num_args = in_num_args;
      info->in_args = in_args;
    }
  g_free ((gpointer) info->in_signature);
  info->in_signature = compute_signature (in_args, in_num_args);

  if (out_num_args != 0)
    {
      info->out_num_args = out_num_args;
      info->out_args = out_args;
    }
  g_free ((gpointer) info->out_signature);
  info->out_signature = compute_signature (out_args, out_num_args);

  if (annotations != NULL)
    info->annotations = annotations;
}

static void
g_dbus_signal_info_set (ParseData           *data,
                        GDBusSignalInfo     *info,
                        const gchar         *name,
                        guint                num_args,
                        GDBusArgInfo        *args,
                        GDBusAnnotationInfo *annotations)
{
  if (name != NULL)
    info->name = g_strdup (name);

  if (num_args != 0)
    {
      info->num_args = num_args;
      info->args = args;
    }
  g_free ((gpointer) info->signature);
  info->signature = compute_signature (args, num_args);

  if (annotations != NULL)
    {
      info->annotations = annotations;
    }
}

static void
g_dbus_property_info_set (ParseData              *data,
                          GDBusPropertyInfo      *info,
                          const gchar            *name,
                          const gchar            *signature,
                          GDBusPropertyInfoFlags  flags,
                          GDBusAnnotationInfo    *annotations)
{
  if (name != NULL)
    info->name = g_strdup (name);

  if (flags != G_DBUS_PROPERTY_INFO_FLAGS_NONE)
    info->flags = flags;

  if (signature != NULL)
    {
      info->signature = g_strdup (signature);
    }

  if (annotations != NULL)
    {
      info->annotations = annotations;
    }
}

static void
g_dbus_interface_info_set (ParseData           *data,
                           GDBusInterfaceInfo  *info,
                           const gchar         *name,
                           guint                num_methods,
                           GDBusMethodInfo     *methods,
                           guint                num_signals,
                           GDBusSignalInfo     *signals,
                           guint                num_properties,
                           GDBusPropertyInfo   *properties,
                           GDBusAnnotationInfo *annotations)
{
  if (name != NULL)
    {
      info->name = g_strdup (name);
    }

  if (num_methods != 0)
    {
      info->num_methods    = num_methods;
      info->methods        = methods;
    }

  if (num_signals != 0)
    {
      info->num_signals    = num_signals;
      info->signals        = signals;
    }

  if (num_properties != 0)
    {
      info->num_properties = num_properties;
      info->properties     = properties;
    }

  if (annotations != NULL)
    {
      info->annotations = annotations;
    }
}

static void
g_dbus_node_info_set (ParseData           *data,
                      GDBusNodeInfo       *info,
                      const gchar         *path,
                      guint                num_interfaces,
                      GDBusInterfaceInfo  *interfaces,
                      guint                num_nodes,
                      GDBusNodeInfo       *nodes,
                      GDBusAnnotationInfo *annotations)
{
  if (path != NULL)
    {
      info->path = g_strdup (path);
      /* TODO: relative / absolute path snafu */
    }

  if (num_interfaces != 0)
    {
      info->num_interfaces = num_interfaces;
      info->interfaces     = interfaces;
    }

  if (num_nodes != 0)
    {
      info->num_nodes      = num_nodes;
      info->nodes          = nodes;
    }

  if (annotations != NULL)
    {
      info->annotations = annotations;
    }

}

/* ---------------------------------------------------------------------------------------------------- */

static void
g_dbus_annotation_info_generate_xml (const GDBusAnnotationInfo  *info,
                                     guint                       indent,
                                     GString                    *string_builder)
{
  guint n;

  g_string_append_printf (string_builder, "%*s<annotation name=\"%s\" value=\"%s\"",
                          indent, "",
                          info->key,
                          info->value);

  if (info->annotations == NULL)
    {
      g_string_append (string_builder, "/>\n");
    }
  else
    {
      g_string_append (string_builder, ">\n");

      for (n = 0; info->annotations != NULL && info->annotations[n].key != NULL; n++)
        g_dbus_annotation_info_generate_xml (&(info->annotations[n]),
                                             indent + 2,
                                             string_builder);

      g_string_append_printf (string_builder, "%*s</annotation>\n",
                              indent, "");
    }

}

static void
g_dbus_arg_info_generate_xml (const GDBusArgInfo  *info,
                              guint                indent,
                              const gchar         *extra_attributes,
                              GString             *string_builder)
{
  guint n;

  g_string_append_printf (string_builder, "%*s<arg type=\"%s\"",
                          indent, "",
                          info->signature);

  if (info->name != NULL)
    g_string_append_printf (string_builder, " name=\"%s\"", info->name);

  if (extra_attributes != NULL)
    g_string_append_printf (string_builder, " %s", extra_attributes);

  if (info->annotations == NULL)
    {
      g_string_append (string_builder, "/>\n");
    }
  else
    {
      g_string_append (string_builder, ">\n");

      for (n = 0; info->annotations != NULL && info->annotations[n].key != NULL; n++)
        g_dbus_annotation_info_generate_xml (&(info->annotations[n]),
                                             indent + 2,
                                             string_builder);

      g_string_append_printf (string_builder, "%*s</arg>\n", indent, "");
    }

}

static void
g_dbus_method_info_generate_xml (const GDBusMethodInfo  *info,
                                 guint                   indent,
                                 GString                *string_builder)
{
  guint n;

  g_string_append_printf (string_builder, "%*s<method name=\"%s\"",
                          indent, "",
                          info->name);

  if (info->annotations == NULL && info->in_num_args == 0 && info->out_num_args == 0)
    {
      g_string_append (string_builder, "/>\n");
    }
  else
    {
      g_string_append (string_builder, ">\n");

      for (n = 0; info->annotations != NULL && info->annotations[n].key != NULL; n++)
        g_dbus_annotation_info_generate_xml (&(info->annotations[n]),
                                             indent + 2,
                                             string_builder);

      for (n = 0; n < info->in_num_args; n++)
        g_dbus_arg_info_generate_xml (&(info->in_args[n]),
                                      indent + 2,
                                      "direction=\"in\"",
                                      string_builder);

      for (n = 0; n < info->out_num_args; n++)
        g_dbus_arg_info_generate_xml (&(info->out_args[n]),
                                      indent + 2,
                                      "direction=\"out\"",
                                      string_builder);

      g_string_append_printf (string_builder, "%*s</method>\n", indent, "");
    }
}

static void
g_dbus_signal_info_generate_xml (const GDBusSignalInfo  *info,
                                 guint                   indent,
                                 GString                *string_builder)
{
  guint n;

  g_string_append_printf (string_builder, "%*s<signal name=\"%s\"",
                          indent, "",
                          info->name);

  if (info->annotations == NULL && info->num_args == 0)
    {
      g_string_append (string_builder, "/>\n");
    }
  else
    {
      g_string_append (string_builder, ">\n");

      for (n = 0; info->annotations != NULL && info->annotations[n].key != NULL; n++)
        g_dbus_annotation_info_generate_xml (&(info->annotations[n]),
                                             indent + 2,
                                             string_builder);

      for (n = 0; n < info->num_args; n++)
        g_dbus_arg_info_generate_xml (&(info->args[n]),
                                      indent + 2,
                                      NULL,
                                      string_builder);

      g_string_append_printf (string_builder, "%*s</signal>\n", indent, "");
    }
}

static void
g_dbus_property_info_generate_xml (const GDBusPropertyInfo  *info,
                                   guint                     indent,
                                   GString                  *string_builder)
{
  guint n;
  const gchar *access_string;

  if ((info->flags & G_DBUS_PROPERTY_INFO_FLAGS_READABLE) &&
      (info->flags & G_DBUS_PROPERTY_INFO_FLAGS_WRITABLE))
    {
      access_string = "readwrite";
    }
  else if (info->flags & G_DBUS_PROPERTY_INFO_FLAGS_READABLE)
    {
      access_string = "read";
    }
  else if (info->flags & G_DBUS_PROPERTY_INFO_FLAGS_WRITABLE)
    {
      access_string = "write";
    }
  else
    {
      g_assert_not_reached ();
    }

  g_string_append_printf (string_builder, "%*s<property type=\"%s\" name=\"%s\" access=\"%s\"",
                          indent, "",
                          info->signature,
                          info->name,
                          access_string);

  if (info->annotations == NULL)
    {
      g_string_append (string_builder, "/>\n");
    }
  else
    {
      g_string_append (string_builder, ">\n");

      for (n = 0; info->annotations != NULL && info->annotations[n].key != NULL; n++)
        g_dbus_annotation_info_generate_xml (&(info->annotations[n]),
                                               indent + 2,
                                               string_builder);

      g_string_append_printf (string_builder, "%*s</property>\n", indent, "");
    }

}

/**
 * g_dbus_interface_info_generate_xml:
 * @interface_info: A #GDBusNodeInfo
 * @indent: Indentation level.
 * @string_builder: A #GString to to append XML data to.
 *
 * Appends an XML representation of @interface_info (and its children) to @string_builder.
 *
 * This function is typically used for generating introspection XML
 * documents at run-time for handling the
 * <literal>org.freedesktop.DBus.Introspectable.Introspect</literal>
 * method.
 */
void
g_dbus_interface_info_generate_xml (const GDBusInterfaceInfo  *interface_info,
                                    guint                      indent,
                                    GString                   *string_builder)
{
  guint n;

  g_string_append_printf (string_builder, "%*s<interface name=\"%s\">\n",
                          indent, "",
                          interface_info->name);

  for (n = 0; interface_info->annotations != NULL && interface_info->annotations[n].key != NULL; n++)
    g_dbus_annotation_info_generate_xml (&(interface_info->annotations[n]),
                                         indent + 2,
                                         string_builder);

  for (n = 0; n < interface_info->num_methods; n++)
    g_dbus_method_info_generate_xml (&(interface_info->methods[n]),
                                     indent + 2,
                                     string_builder);

  for (n = 0; n < interface_info->num_signals; n++)
    g_dbus_signal_info_generate_xml (&(interface_info->signals[n]),
                                     indent + 2,
                                     string_builder);

  for (n = 0; n < interface_info->num_properties; n++)
    g_dbus_property_info_generate_xml (&(interface_info->properties[n]),
                                       indent + 2,
                                       string_builder);

  g_string_append_printf (string_builder, "%*s</interface>\n", indent, "");
}

/**
 * g_dbus_node_info_generate_xml:
 * @node_info: A #GDBusNodeInfo.
 * @indent: Indentation level.
 * @string_builder: A #GString to to append XML data to.
 *
 * Appends an XML representation of @node_info (and its children) to @string_builder.
 *
 * This function is typically used for generating introspection XML documents at run-time for
 * handling the <literal>org.freedesktop.DBus.Introspectable.Introspect</literal> method.
 */
void
g_dbus_node_info_generate_xml (const GDBusNodeInfo  *node_info,
                               guint                 indent,
                               GString              *string_builder)
{
  guint n;

  g_string_append_printf (string_builder, "%*s<node", indent, "");
  if (node_info->path != NULL)
    g_string_append_printf (string_builder, " name=\"%s\"", node_info->path);

  if (node_info->num_interfaces == 0 && node_info->num_nodes == 0 && node_info->annotations == NULL)
    {
      g_string_append (string_builder, "/>\n");
    }
  else
    {
      g_string_append (string_builder, ">\n");

      for (n = 0; node_info->annotations != NULL && node_info->annotations[n].key != NULL; n++)
        g_dbus_annotation_info_generate_xml (&(node_info->annotations[n]),
                                             indent + 2,
                                             string_builder);

      for (n = 0; n < node_info->num_interfaces; n++)
        g_dbus_interface_info_generate_xml (&(node_info->interfaces[n]),
                                            indent + 2,
                                            string_builder);

      for (n = 0; n < node_info->num_nodes; n++)
        g_dbus_node_info_generate_xml (&(node_info->nodes[n]),
                                       indent + 2,
                                       string_builder);

      g_string_append_printf (string_builder, "%*s</node>\n", indent, "");
    }
}

/* ---------------------------------------------------------------------------------------------------- */

static GDBusAnnotationInfo *
parse_data_steal_annotations (ParseData *data, guint *out_num_elements)
{
  GDBusAnnotationInfo *ret;
  if (out_num_elements != NULL)
    *out_num_elements = data->annotations->len;
  if (data->annotations == NULL)
    ret = NULL;
  else
    ret = (GDBusAnnotationInfo *) g_array_free (data->annotations, FALSE);
  data->annotations = g_array_new (FALSE, TRUE, sizeof (GDBusAnnotationInfo));
  return ret;
}

static GDBusArgInfo *
parse_data_steal_args (ParseData *data, guint *out_num_elements)
{
  GDBusArgInfo *ret;
  if (out_num_elements != NULL)
    *out_num_elements = data->args->len;
  if (data->args == NULL)
    ret = NULL;
  else
    ret = (GDBusArgInfo *) g_array_free (data->args, FALSE);
  data->args = g_array_new (FALSE, TRUE, sizeof (GDBusArgInfo));
  return ret;
}

static GDBusArgInfo *
parse_data_steal_out_args (ParseData *data, guint *out_num_elements)
{
  GDBusArgInfo *ret;
  if (out_num_elements != NULL)
    *out_num_elements = data->out_args->len;
  if (data->out_args == NULL)
    ret = NULL;
  else
    ret = (GDBusArgInfo *) g_array_free (data->out_args, FALSE);
  data->out_args = g_array_new (FALSE, TRUE, sizeof (GDBusArgInfo));
  return ret;
}

static GDBusMethodInfo *
parse_data_steal_methods (ParseData *data, guint *out_num_elements)
{
  GDBusMethodInfo *ret;
  if (out_num_elements != NULL)
    *out_num_elements = data->methods->len;
  if (data->methods == NULL)
    ret = NULL;
  else
    ret = (GDBusMethodInfo *) g_array_free (data->methods, FALSE);
  data->methods = g_array_new (FALSE, TRUE, sizeof (GDBusMethodInfo));
  return ret;
}

static GDBusSignalInfo *
parse_data_steal_signals (ParseData *data, guint *out_num_elements)
{
  GDBusSignalInfo *ret;
  if (out_num_elements != NULL)
    *out_num_elements = data->signals->len;
  if (data->signals == NULL)
    ret = NULL;
  else
    ret = (GDBusSignalInfo *) g_array_free (data->signals, FALSE);
  data->signals = g_array_new (FALSE, TRUE, sizeof (GDBusSignalInfo));
  return ret;
}

static GDBusPropertyInfo *
parse_data_steal_properties (ParseData *data, guint *out_num_elements)
{
  GDBusPropertyInfo *ret;
  if (out_num_elements != NULL)
    *out_num_elements = data->properties->len;
  if (data->properties == NULL)
    ret = NULL;
  else
    ret = (GDBusPropertyInfo *) g_array_free (data->properties, FALSE);
  data->properties = g_array_new (FALSE, TRUE, sizeof (GDBusPropertyInfo));
  return ret;
}

static GDBusInterfaceInfo *
parse_data_steal_interfaces (ParseData *data, guint *out_num_elements)
{
  GDBusInterfaceInfo *ret;
  if (out_num_elements != NULL)
    *out_num_elements = data->interfaces->len;
  if (data->interfaces == NULL)
    ret = NULL;
  else
    ret = (GDBusInterfaceInfo *) g_array_free (data->interfaces, FALSE);
  data->interfaces = g_array_new (FALSE, TRUE, sizeof (GDBusInterfaceInfo));
  return ret;
}

static GDBusNodeInfo *
parse_data_steal_nodes (ParseData *data, guint *out_num_elements)
{
  GDBusNodeInfo *ret;
  if (out_num_elements != NULL)
    *out_num_elements = data->nodes->len;
  if (data->nodes == NULL)
    ret = NULL;
  else
    ret = (GDBusNodeInfo *) g_array_free (data->nodes, FALSE);
  data->nodes = g_array_new (FALSE, TRUE, sizeof (GDBusNodeInfo));
  return ret;
}

/* ---------------------------------------------------------------------------------------------------- */

static void
parse_data_free_annotations (ParseData *data)
{
  guint n;
  if (data->annotations == NULL)
    return;
  for (n = 0; n < data->annotations->len; n++)
    g_dbus_annotation_info_free (&g_array_index (data->annotations, GDBusAnnotationInfo, n));
  g_array_free (data->annotations, TRUE);
  data->annotations = NULL;
}

static void
parse_data_free_args (ParseData *data)
{
  guint n;
  if (data->args == NULL)
    return;
  for (n = 0; n < data->args->len; n++)
    g_dbus_arg_info_free (&g_array_index (data->args, GDBusArgInfo, n));
  g_array_free (data->args, TRUE);
  data->args = NULL;
}

static void
parse_data_free_out_args (ParseData *data)
{
  guint n;
  if (data->out_args == NULL)
    return;
  for (n = 0; n < data->out_args->len; n++)
    g_dbus_arg_info_free (&g_array_index (data->out_args, GDBusArgInfo, n));
  g_array_free (data->out_args, TRUE);
  data->out_args = NULL;
}

static void
parse_data_free_methods (ParseData *data)
{
  guint n;
  if (data->methods == NULL)
    return;
  for (n = 0; n < data->methods->len; n++)
    g_dbus_method_info_free (&g_array_index (data->methods, GDBusMethodInfo, n));
  g_array_free (data->methods, TRUE);
  data->methods = NULL;
}

static void
parse_data_free_signals (ParseData *data)
{
  guint n;
  if (data->signals == NULL)
    return;
  for (n = 0; n < data->signals->len; n++)
    g_dbus_signal_info_free (&g_array_index (data->signals, GDBusSignalInfo, n));
  g_array_free (data->signals, TRUE);
  data->signals = NULL;
}

static void
parse_data_free_properties (ParseData *data)
{
  guint n;
  if (data->properties == NULL)
    return;
  for (n = 0; n < data->properties->len; n++)
    g_dbus_property_info_free (&g_array_index (data->properties, GDBusPropertyInfo, n));
  g_array_free (data->properties, TRUE);
  data->properties = NULL;
}

static void
parse_data_free_interfaces (ParseData *data)
{
  guint n;
  if (data->interfaces == NULL)
    return;
  for (n = 0; n < data->interfaces->len; n++)
    g_dbus_interface_info_free (&g_array_index (data->interfaces, GDBusInterfaceInfo, n));
  g_array_free (data->interfaces, TRUE);
  data->interfaces = NULL;
}

static void
parse_data_free_nodes (ParseData *data)
{
  guint n;
  if (data->nodes == NULL)
    return;
  for (n = 0; n < data->nodes->len; n++)
    g_dbus_node_info_free (&g_array_index (data->nodes, GDBusNodeInfo, n));
  g_array_free (data->nodes, TRUE);
  data->nodes = NULL;
}

/* ---------------------------------------------------------------------------------------------------- */

static GDBusAnnotationInfo *
parse_data_get_annotation (ParseData *data, gboolean create_new)
{
  if (create_new)
    g_array_set_size (data->annotations, data->annotations->len + 1);
  return &g_array_index (data->annotations, GDBusAnnotationInfo, data->annotations->len - 1);
}

static GDBusArgInfo *
parse_data_get_arg (ParseData *data, gboolean create_new)
{
  if (create_new)
    g_array_set_size (data->args, data->args->len + 1);
  return &g_array_index (data->args, GDBusArgInfo, data->args->len - 1);
}

static GDBusArgInfo *
parse_data_get_out_arg (ParseData *data, gboolean create_new)
{
  if (create_new)
    g_array_set_size (data->out_args, data->out_args->len + 1);
  return &g_array_index (data->out_args, GDBusArgInfo, data->out_args->len - 1);
}

static GDBusMethodInfo *
parse_data_get_method (ParseData *data, gboolean create_new)
{
  if (create_new)
    g_array_set_size (data->methods, data->methods->len + 1);
  return &g_array_index (data->methods, GDBusMethodInfo, data->methods->len - 1);
}

static GDBusSignalInfo *
parse_data_get_signal (ParseData *data, gboolean create_new)
{
  if (create_new)
    g_array_set_size (data->signals, data->signals->len + 1);
  return &g_array_index (data->signals, GDBusSignalInfo, data->signals->len - 1);
}

static GDBusPropertyInfo *
parse_data_get_property (ParseData *data, gboolean create_new)
{
  if (create_new)
    g_array_set_size (data->properties, data->properties->len + 1);
  return &g_array_index (data->properties, GDBusPropertyInfo, data->properties->len - 1);
}

static GDBusInterfaceInfo *
parse_data_get_interface (ParseData *data, gboolean create_new)
{
  if (create_new)
    g_array_set_size (data->interfaces, data->interfaces->len + 1);
  return &g_array_index (data->interfaces, GDBusInterfaceInfo, data->interfaces->len - 1);
}

static GDBusNodeInfo *
parse_data_get_node (ParseData *data, gboolean create_new)
{
  if (create_new)
    g_array_set_size (data->nodes, data->nodes->len + 1);
  return &g_array_index (data->nodes, GDBusNodeInfo, data->nodes->len - 1);
}

/* ---------------------------------------------------------------------------------------------------- */

static ParseData *
parse_data_new (void)
{
  ParseData *data;

  data = g_new0 (ParseData, 1);

  /* initialize arrays */
  parse_data_steal_annotations (data, NULL);
  parse_data_steal_args (data, NULL);
  parse_data_steal_out_args (data, NULL);
  parse_data_steal_methods (data, NULL);
  parse_data_steal_signals (data, NULL);
  parse_data_steal_properties (data, NULL);
  parse_data_steal_interfaces (data, NULL);
  parse_data_steal_nodes (data, NULL);

  return data;
}

static void
parse_data_free (ParseData *data)
{
  GSList *l;

  /* free stack of annotation arrays */
  for (l = data->annotations_stack; l != NULL; l = l->next)
    {
      GArray *annotations = l->data;
      guint n;

      for (n = 0; n < annotations->len; n++)
        g_dbus_annotation_info_free (&g_array_index (annotations, GDBusAnnotationInfo, n));
      g_array_free (annotations, TRUE);
    }
  g_slist_free (data->annotations_stack);

  /* free stack of interface arrays */
  for (l = data->interfaces_stack; l != NULL; l = l->next)
    {
      GArray *interfaces = l->data;
      guint n;

      for (n = 0; n < interfaces->len; n++)
        g_dbus_interface_info_free (&g_array_index (interfaces, GDBusInterfaceInfo, n));
      g_array_free (interfaces, TRUE);
    }
  g_slist_free (data->interfaces_stack);

  /* free stack of node arrays */
  for (l = data->nodes_stack; l != NULL; l = l->next)
    {
      GArray *nodes = l->data;
      guint n;

      for (n = 0; n < nodes->len; n++)
        g_dbus_node_info_free (&g_array_index (nodes, GDBusNodeInfo, n));
      g_array_free (nodes, TRUE);
    }
  g_slist_free (data->nodes_stack);

  /* free arrays (data->annotations, data->interfaces and data->nodes have been freed above) */
  parse_data_free_args (data);
  parse_data_free_out_args (data);
  parse_data_free_methods (data);
  parse_data_free_signals (data);
  parse_data_free_properties (data);

  g_free (data);
}

/* ---------------------------------------------------------------------------------------------------- */

static void
parser_start_element (GMarkupParseContext *context,
                      const gchar         *element_name,
                      const gchar        **attribute_names,
                      const gchar        **attribute_values,
                      gpointer             user_data,
                      GError             **error)
{
  ParseData *data = user_data;
  GSList *stack;
  const gchar *name;
  const gchar *type;
  const gchar *access;
  const gchar *direction;
  const gchar *value;

  name = NULL;
  type = NULL;
  access = NULL;
  direction = NULL;
  value = NULL;

  stack = (GSList *) g_markup_parse_context_get_element_stack (context);

  /* ---------------------------------------------------------------------------------------------------- */
  if (strcmp (element_name, "node") == 0)
    {
      if (!(g_slist_length (stack) >= 1 || strcmp (stack->next->data, "node") != 0))
        {
          g_set_error_literal (error,
                               G_MARKUP_ERROR,
                               G_MARKUP_ERROR_INVALID_CONTENT,
                               "<node> elements can only be top-level or embedded in other <node> elements");
          goto out;
        }

      if (!g_markup_collect_attributes (element_name,
                                        attribute_names,
                                        attribute_values,
                                        error,
                                        G_MARKUP_COLLECT_STRING | G_MARKUP_COLLECT_OPTIONAL, "name", &name,
                                        /* some hand-written introspection XML documents use this */
                                        G_MARKUP_COLLECT_STRING | G_MARKUP_COLLECT_OPTIONAL, "xmlns:doc", NULL,
                                        G_MARKUP_COLLECT_INVALID))
        goto out;

      g_dbus_node_info_set (data,
                            parse_data_get_node (data, TRUE),
                            name,
                            0, NULL,
                            0, NULL,
                            NULL);

      /* push the currently retrieved interfaces and nodes on the stack and prepare new arrays */
      data->interfaces_stack = g_slist_prepend (data->interfaces_stack, data->interfaces);
      data->interfaces = NULL;
      parse_data_steal_interfaces (data, NULL);

      data->nodes_stack = g_slist_prepend (data->nodes_stack, data->nodes);
      data->nodes = NULL;
      parse_data_steal_nodes (data, NULL);

    }
  /* ---------------------------------------------------------------------------------------------------- */
  else if (strcmp (element_name, "interface") == 0)
    {
      if (g_slist_length (stack) < 2 || strcmp (stack->next->data, "node") != 0)
        {
          g_set_error_literal (error,
                               G_MARKUP_ERROR,
                               G_MARKUP_ERROR_INVALID_CONTENT,
                               "<interface> elements can only be embedded in <node> elements");
          goto out;
        }

      if (!g_markup_collect_attributes (element_name,
                                        attribute_names,
                                        attribute_values,
                                        error,
                                        G_MARKUP_COLLECT_STRING, "name", &name,
                                        G_MARKUP_COLLECT_INVALID))
        goto out;

      g_dbus_interface_info_set (data,
                                 parse_data_get_interface (data, TRUE),
                                 name,
                                 0, NULL,
                                 0, NULL,
                                 0, NULL,
                                 NULL);

    }
  /* ---------------------------------------------------------------------------------------------------- */
  else if (strcmp (element_name, "method") == 0)
    {
      if (g_slist_length (stack) < 2 || strcmp (stack->next->data, "interface") != 0)
        {
          g_set_error_literal (error,
                               G_MARKUP_ERROR,
                               G_MARKUP_ERROR_INVALID_CONTENT,
                               "<method> elements can only be embedded in <interface> elements");
          goto out;
        }

      if (!g_markup_collect_attributes (element_name,
                                        attribute_names,
                                        attribute_values,
                                        error,
                                        G_MARKUP_COLLECT_STRING, "name", &name,
                                        G_MARKUP_COLLECT_INVALID))
        goto out;

      g_dbus_method_info_set (data,
                              parse_data_get_method (data, TRUE),
                              name,
                              0, NULL,
                              0, NULL,
                              NULL);

      data->num_args = 0;

    }
  /* ---------------------------------------------------------------------------------------------------- */
  else if (strcmp (element_name, "signal") == 0)
    {
      if (g_slist_length (stack) < 2 || strcmp (stack->next->data, "interface") != 0)
        {
          g_set_error_literal (error,
                               G_MARKUP_ERROR,
                               G_MARKUP_ERROR_INVALID_CONTENT,
                               "<signal> elements can only be embedded in <interface> elements");
          goto out;
        }

      if (!g_markup_collect_attributes (element_name,
                                        attribute_names,
                                        attribute_values,
                                        error,
                                        G_MARKUP_COLLECT_STRING, "name", &name,
                                        G_MARKUP_COLLECT_INVALID))
        goto out;

      g_dbus_signal_info_set (data,
                              parse_data_get_signal (data, TRUE),
                              name,
                              0, NULL,
                              NULL);

      data->num_args = 0;

    }
  /* ---------------------------------------------------------------------------------------------------- */
  else if (strcmp (element_name, "property") == 0)
    {
      GDBusPropertyInfoFlags flags;

      if (g_slist_length (stack) < 2 || strcmp (stack->next->data, "interface") != 0)
        {
          g_set_error_literal (error,
                               G_MARKUP_ERROR,
                               G_MARKUP_ERROR_INVALID_CONTENT,
                               "<property> elements can only be embedded in <interface> elements");
          goto out;
        }

      if (!g_markup_collect_attributes (element_name,
                                        attribute_names,
                                        attribute_values,
                                        error,
                                        G_MARKUP_COLLECT_STRING, "name", &name,
                                        G_MARKUP_COLLECT_STRING, "type", &type,
                                        G_MARKUP_COLLECT_STRING, "access", &access,
                                        G_MARKUP_COLLECT_INVALID))
        goto out;

      if (strcmp (access, "read") == 0)
        flags = G_DBUS_PROPERTY_INFO_FLAGS_READABLE;
      else if (strcmp (access, "write") == 0)
        flags = G_DBUS_PROPERTY_INFO_FLAGS_WRITABLE;
      else if (strcmp (access, "readwrite") == 0)
        flags = G_DBUS_PROPERTY_INFO_FLAGS_READABLE | G_DBUS_PROPERTY_INFO_FLAGS_WRITABLE;
      else
        {
          g_set_error (error,
                       G_MARKUP_ERROR,
                       G_MARKUP_ERROR_INVALID_CONTENT,
                       "Unknown value '%s' of access attribute for element <property>",
                       access);
          goto out;
        }

      g_dbus_property_info_set (data,
                                parse_data_get_property (data, TRUE),
                                name,
                                type,
                                flags,
                                NULL);

    }
  /* ---------------------------------------------------------------------------------------------------- */
  else if (strcmp (element_name, "arg") == 0)
    {
      gboolean is_in;
      gchar *name_to_use;

      if (g_slist_length (stack) < 2 ||
          (strcmp (stack->next->data, "method") != 0 &&
           strcmp (stack->next->data, "signal") != 0))
        {
          g_set_error_literal (error,
                               G_MARKUP_ERROR,
                               G_MARKUP_ERROR_INVALID_CONTENT,
                               "<arg> elements can only be embedded in <method> or <signal> elements");
          goto out;
        }

      if (!g_markup_collect_attributes (element_name,
                                        attribute_names,
                                        attribute_values,
                                        error,
                                        G_MARKUP_COLLECT_STRING | G_MARKUP_COLLECT_OPTIONAL, "name", &name,
                                        G_MARKUP_COLLECT_STRING | G_MARKUP_COLLECT_OPTIONAL, "direction", &direction,
                                        G_MARKUP_COLLECT_STRING, "type", &type,
                                        G_MARKUP_COLLECT_INVALID))
        goto out;

      is_in = FALSE;
      if (direction != NULL)
        {
          if (strcmp (direction, "in") == 0)
            is_in = TRUE;
          else if (strcmp (direction, "out") == 0)
            is_in = FALSE;
          else
            {
              g_set_error (error,
                           G_MARKUP_ERROR,
                           G_MARKUP_ERROR_INVALID_CONTENT,
                           "Unknown value '%s' of direction attribute",
                           direction);
              goto out;
            }
        }

      if (is_in && strcmp (stack->next->data, "signal") == 0)
        {
          g_set_error_literal (error,
                               G_MARKUP_ERROR,
                               G_MARKUP_ERROR_INVALID_CONTENT,
                               "Only direction 'out' is allowed for <arg> elements embedded in <signal>");
          goto out;
        }

      if (name == NULL)
        name_to_use = g_strdup_printf ("arg_%d", data->num_args);
      else
        name_to_use = g_strdup (name);
      data->num_args++;

      if (is_in)
        {
          g_dbus_arg_info_set (data,
                               parse_data_get_arg (data, TRUE),
                               name_to_use,
                               type,
                               NULL);
          data->last_arg_was_in = TRUE;
        }
      else
        {
          g_dbus_arg_info_set (data,
                               parse_data_get_out_arg (data, TRUE),
                               name_to_use,
                               type,
                               NULL);
          data->last_arg_was_in = FALSE;

        }

      g_free (name_to_use);
    }
  /* ---------------------------------------------------------------------------------------------------- */
  else if (strcmp (element_name, "annotation") == 0)
    {
      if (g_slist_length (stack) < 2 ||
          (strcmp (stack->next->data, "node") != 0 &&
           strcmp (stack->next->data, "interface") != 0 &&
           strcmp (stack->next->data, "signal") != 0 &&
           strcmp (stack->next->data, "method") != 0 &&
           strcmp (stack->next->data, "property") != 0 &&
           strcmp (stack->next->data, "arg") != 0 &&
           strcmp (stack->next->data, "annotation") != 0))
        {
          g_set_error_literal (error,
                               G_MARKUP_ERROR,
                               G_MARKUP_ERROR_INVALID_CONTENT,
                               "<annotation> elements can only be embedded in <node>, <interface>, <signal>, <method>, <property>, <arg> or <annotation> elements");
          goto out;
        }

      if (!g_markup_collect_attributes (element_name,
                                        attribute_names,
                                        attribute_values,
                                        error,
                                        G_MARKUP_COLLECT_STRING, "name", &name,
                                        G_MARKUP_COLLECT_STRING, "value", &value,
                                        G_MARKUP_COLLECT_INVALID))
        goto out;

      g_dbus_annotation_info_set (data,
                                  parse_data_get_annotation (data, TRUE),
                                  name,
                                  value,
                                  NULL);
    }
  /* ---------------------------------------------------------------------------------------------------- */
  else
    {
      /* don't bail on unknown elements; just ignore them */
    }
  /* ---------------------------------------------------------------------------------------------------- */

  /* push the currently retrieved annotations on the stack and prepare a new one */
  data->annotations_stack = g_slist_prepend (data->annotations_stack, data->annotations);
  data->annotations = NULL;
  parse_data_steal_annotations (data, NULL);

 out:
  ;
}

/* ---------------------------------------------------------------------------------------------------- */

static GDBusAnnotationInfo *
steal_annotations (ParseData *data)
{
  GDBusAnnotationInfo *annotations;
  guint num_annotations;

  if (data->annotations->len == 0)
    {
      annotations = parse_data_steal_annotations (data, &num_annotations);
      g_free (annotations);
      annotations = NULL;
    }
  else
    {
      /* NULL terminate */
      g_dbus_annotation_info_set (data,
                                  parse_data_get_annotation (data, TRUE),
                                  NULL,
                                  NULL,
                                  NULL);
      annotations = parse_data_steal_annotations (data, &num_annotations);
    }
  return annotations;
}


static void
parser_end_element (GMarkupParseContext *context,
                    const gchar         *element_name,
                    gpointer             user_data,
                    GError             **error)
{
  ParseData *data = user_data;
  gboolean have_popped_annotations;

  have_popped_annotations = FALSE;

  if (strcmp (element_name, "node") == 0)
    {
      guint num_nodes;
      guint num_interfaces;
      GDBusNodeInfo *nodes;
      GDBusInterfaceInfo *interfaces;

      nodes = parse_data_steal_nodes (data, &num_nodes);
      interfaces = parse_data_steal_interfaces (data, &num_interfaces);

      /* destroy the nodes, interfaces for scope we're exiting and and pop the nodes, interfaces from the
       * scope we're reentering
       */
      parse_data_free_interfaces (data);
      data->interfaces = (GArray *) data->interfaces_stack->data;
      data->interfaces_stack = g_slist_remove (data->interfaces_stack, data->interfaces_stack->data);

      parse_data_free_nodes (data);
      data->nodes = (GArray *) data->nodes_stack->data;
      data->nodes_stack = g_slist_remove (data->nodes_stack, data->nodes_stack->data);

      g_dbus_node_info_set (data,
                            parse_data_get_node (data, FALSE),
                            NULL,
                            num_interfaces,
                            interfaces,
                            num_nodes,
                            nodes,
                            steal_annotations (data));

    }
  else if (strcmp (element_name, "interface") == 0)
    {
      guint num_methods;
      guint num_signals;
      guint num_properties;
      GDBusMethodInfo *methods;
      GDBusSignalInfo *signals;
      GDBusPropertyInfo *properties;

      methods    = parse_data_steal_methods    (data, &num_methods);
      signals    = parse_data_steal_signals    (data, &num_signals);
      properties = parse_data_steal_properties (data, &num_properties);

      g_dbus_interface_info_set (data,
                                 parse_data_get_interface (data, FALSE),
                                 NULL,
                                 num_methods,
                                 methods,
                                 num_signals,
                                 signals,
                                 num_properties,
                                 properties,
                                 steal_annotations (data));

    }
  else if (strcmp (element_name, "method") == 0)
    {
      guint in_num_args;
      guint out_num_args;
      GDBusArgInfo *in_args;
      GDBusArgInfo *out_args;

      in_args  = parse_data_steal_args     (data, &in_num_args);
      out_args = parse_data_steal_out_args (data, &out_num_args);

      g_dbus_method_info_set (data,
                              parse_data_get_method (data, FALSE),
                              NULL,
                              in_num_args,
                              in_args,
                              out_num_args,
                              out_args,
                              steal_annotations (data));
    }
  else if (strcmp (element_name, "signal") == 0)
    {
      guint num_args;
      GDBusArgInfo *args;

      args = parse_data_steal_out_args (data, &num_args);

      g_dbus_signal_info_set (data,
                              parse_data_get_signal (data, FALSE),
                              NULL,
                              num_args,
                              args,
                              steal_annotations (data));
    }
  else if (strcmp (element_name, "property") == 0)
    {
      g_dbus_property_info_set (data,
                                parse_data_get_property (data, FALSE),
                                NULL,
                                NULL,
                                G_DBUS_PROPERTY_INFO_FLAGS_NONE,
                                steal_annotations (data));
    }
  else if (strcmp (element_name, "arg") == 0)
    {
      g_dbus_arg_info_set (data,
                           data->last_arg_was_in ? parse_data_get_arg (data, FALSE) : parse_data_get_out_arg (data, FALSE),
                           NULL,
                           NULL,
                           steal_annotations (data));
    }
  else if (strcmp (element_name, "annotation") == 0)
    {
      GDBusAnnotationInfo *embedded_annotations;

      embedded_annotations = steal_annotations (data);

      /* destroy the annotations for scope we're exiting and and pop the annotations from the scope we're reentering */
      parse_data_free_annotations (data);
      data->annotations = (GArray *) data->annotations_stack->data;
      data->annotations_stack = g_slist_remove (data->annotations_stack, data->annotations_stack->data);

      have_popped_annotations = TRUE;

      g_dbus_annotation_info_set (data,
                                  parse_data_get_annotation (data, FALSE),
                                  NULL,
                                  NULL,
                                  embedded_annotations);
    }
  else
    {
      /* don't bail on unknown elements; just ignore them */
    }

  if (!have_popped_annotations)
    {
      /* destroy the annotations for scope we're exiting and and pop the annotations from the scope we're reentering */
      parse_data_free_annotations (data);
      data->annotations = (GArray *) data->annotations_stack->data;
      data->annotations_stack = g_slist_remove (data->annotations_stack, data->annotations_stack->data);
    }
}

/* ---------------------------------------------------------------------------------------------------- */

static void
parser_error (GMarkupParseContext *context,
              GError              *error,
              gpointer             user_data)
{
  gint line_number;
  gint char_number;

  g_markup_parse_context_get_position (context, &line_number, &char_number);

  g_prefix_error (&error, "%d:%d: ",
                  line_number,
                  char_number);
}

/* ---------------------------------------------------------------------------------------------------- */

/**
 * g_dbus_node_info_new_for_xml:
 * @xml_data: Valid D-Bus introspection XML.
 * @error: Return location for error.
 *
 * Parses @xml_data and returns a #GDBusNodeInfo representing the data.
 *
 * Returns: A #GDBusNodeInfo structure or %NULL if @error is set. Free
 * with g_dbus_node_info_free().
 */
GDBusNodeInfo *
g_dbus_node_info_new_for_xml (const gchar  *xml_data,
                              GError      **error)
{
  GDBusNodeInfo *ret;
  GMarkupParseContext *context;
  GMarkupParser *parser;
  guint num_nodes;
  ParseData *data;

  ret = NULL;
  parser = NULL;
  context = NULL;

  parser = g_new0 (GMarkupParser, 1);
  parser->start_element = parser_start_element;
  parser->end_element   = parser_end_element;
  parser->error         = parser_error;

  data = parse_data_new ();
  context = g_markup_parse_context_new (parser,
                                        0,
                                        data,
                                        (GDestroyNotify) parse_data_free);

  if (!g_markup_parse_context_parse (context,
                                     xml_data,
                                     strlen (xml_data),
                                     error))
    goto out;

  ret = parse_data_steal_nodes (data, &num_nodes);

  if (num_nodes != 1)
    {
      guint n;

      g_set_error (error,
                   G_MARKUP_ERROR,
                   G_MARKUP_ERROR_INVALID_CONTENT,
                   "Expected a single node in introspection XML, found %d",
                   num_nodes);

      /* clean up */
      for (n = 0; n < num_nodes; n++)
        {
          for (n = 0; n < num_nodes; n++)
            g_dbus_node_info_free (&(ret[n]));
        }
      g_free (ret);
      ret = NULL;
    }

 out:
  if (parser != NULL)
    g_free (parser);
  if (context != NULL)
    g_markup_parse_context_free (context);

  return ret;
}

/* ---------------------------------------------------------------------------------------------------- */

/**
 * g_dbus_annotation_info_lookup:
 * @annotations: An array of annotations.
 * @name: The name of the annotation to look up.
 *
 * Looks up the value of an annotation.
 *
 * Returns: The value or %NULL if not found. Do not free, it is owned by @annotations.
 **/
const gchar *
g_dbus_annotation_info_lookup (const GDBusAnnotationInfo *annotations,
                               const gchar               *name)
{
  guint n;
  const gchar *ret;

  ret = NULL;

  for (n = 0; annotations != NULL && annotations[n].key != NULL; n++)
    {
      if (g_strcmp0 (annotations[n].key, name) == 0)
        {
          ret = annotations[n].value;
          goto out;
        }
    }

 out:
  return ret;
}

/* ---------------------------------------------------------------------------------------------------- */

/**
 * g_dbus_interface_info_lookup_method_for_name:
 * @interface_info: A #GDBusInterfaceInfo.
 * @name: A D-Bus method name (typically in CamelCase)
 *
 * Looks up information about a method.
 *
 * Returns: A #GDBusMethodInfo or %NULL if not found. Do not free, it is owned by @interface_info.
 **/
const GDBusMethodInfo *
g_dbus_interface_info_lookup_method_for_name (const GDBusInterfaceInfo *interface_info,
                                              const gchar              *name)
{
  guint n;
  const GDBusMethodInfo *result;

  for (n = 0; n < interface_info->num_methods; n++)
    {
      const GDBusMethodInfo *i = interface_info->methods + n;

      if (g_strcmp0 (i->name, name) == 0)
        {
          result = i;
          goto out;
        }
    }

  result = NULL;

 out:
  return result;
}

/* ---------------------------------------------------------------------------------------------------- */

/**
 * g_dbus_interface_info_lookup_signal_for_name:
 * @interface_info: A #GDBusInterfaceInfo.
 * @name: A D-Bus signal name (typically in CamelCase)
 *
 * Looks up information about a signal.
 *
 * Returns: A #GDBusSignalInfo or %NULL if not found. Do not free, it is owned by @interface_info.
 **/
const GDBusSignalInfo *
g_dbus_interface_info_lookup_signal_for_name   (const GDBusInterfaceInfo *interface_info,
                                                const gchar              *name)
{
  guint n;
  const GDBusSignalInfo *result;

  for (n = 0; n < interface_info->num_signals; n++)
    {
      const GDBusSignalInfo *i = interface_info->signals + n;

      if (g_strcmp0 (i->name, name) == 0)
        {
          result = i;
          goto out;
        }
    }

  result = NULL;

 out:
  return result;
}

/**
 * g_dbus_interface_info_lookup_signal_for_g_name:
 * @interface_info: A #GDBusInterfaceInfo.
 * @g_name: A GObject signal name (lower case with hyphens)
 *
 * Looks up information about a signal.
 *
 * Returns: A #GDBusSignalInfo or %NULL if not found. Do not free, it is owned by @interface_info.
 **/
const GDBusSignalInfo *
g_dbus_interface_info_lookup_signal_for_g_name   (const GDBusInterfaceInfo *interface_info,
                                                  const gchar              *g_name)
{
  guint n;
  const GDBusSignalInfo *result;

  for (n = 0; n < interface_info->num_signals; n++)
    {
      const GDBusSignalInfo *i = interface_info->signals + n;

      if (g_strcmp0 (i->g_name, g_name) == 0)
        {
          result = i;
          goto out;
        }
    }

  result = NULL;

 out:
  return result;
}

/* ---------------------------------------------------------------------------------------------------- */

/**
 * g_dbus_interface_info_lookup_property_for_name:
 * @interface_info: A #GDBusInterfaceInfo.
 * @name: A D-Bus property name (typically in CamelCase).
 *
 * Looks up information about a property.
 *
 * Returns: A #GDBusPropertyInfo or %NULL if not found. Do not free, it is owned by @interface_info.
 **/
const GDBusPropertyInfo *
g_dbus_interface_info_lookup_property_for_name (const GDBusInterfaceInfo *interface_info,
                                                const gchar              *name)
{
  guint n;
  const GDBusPropertyInfo *result;

  for (n = 0; n < interface_info->num_properties; n++)
    {
      const GDBusPropertyInfo *i = interface_info->properties + n;

      if (g_strcmp0 (i->name, name) == 0)
        {
          result = i;
          goto out;
        }
    }

  result = NULL;

 out:
  return result;
}

/**
 * g_dbus_interface_info_lookup_property_for_g_name:
 * @interface_info: A #GDBusInterfaceInfo.
 * @g_name: A GObject property name (e.g. lower case with hyphens).
 *
 * Looks up information about a property.
 *
 * Returns: A #GDBusPropertyInfo or %NULL if not found. Do not free, it is owned by @interface_info.
 **/
const GDBusPropertyInfo *
g_dbus_interface_info_lookup_property_for_g_name (const GDBusInterfaceInfo *interface_info,
                                                  const gchar              *g_name)
{
  guint n;
  const GDBusPropertyInfo *result;

  for (n = 0; n < interface_info->num_properties; n++)
    {
      const GDBusPropertyInfo *i = interface_info->properties + n;

      if (g_strcmp0 (i->g_name, g_name) == 0)
        {
          result = i;
          goto out;
        }
    }

  result = NULL;

 out:
  return result;
}

/* ---------------------------------------------------------------------------------------------------- */

/**
 * g_dbus_node_info_lookup_interface_for_name:
 * @node_info: A #GDBusNodeInfo.
 * @name: A D-Bus interface name.
 *
 * Looks up information about an interface.
 *
 * Returns: A #GDBusInterfaceInfo or %NULL if not found. Do not free, it is owned by @node_info.
 **/
const GDBusInterfaceInfo *
g_dbus_node_info_lookup_interface_for_name (const GDBusNodeInfo *node_info,
                                            const gchar         *name)
{
  guint n;
  const GDBusInterfaceInfo *result;

  for (n = 0; n < node_info->num_interfaces; n++)
    {
      const GDBusInterfaceInfo *i = node_info->interfaces + n;

      if (g_strcmp0 (i->name, name) == 0)
        {
          result = i;
          goto out;
        }
    }

  result = NULL;

 out:
  return result;
}
