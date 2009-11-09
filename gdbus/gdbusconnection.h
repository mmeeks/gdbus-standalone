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

#ifndef __G_DBUS_CONNECTION_H__
#define __G_DBUS_CONNECTION_H__

#include <gdbus/gdbustypes.h>

G_BEGIN_DECLS

#define G_TYPE_DBUS_CONNECTION         (g_dbus_connection_get_type ())
#define G_DBUS_CONNECTION(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), G_TYPE_DBUS_CONNECTION, GDBusConnection))
#define G_DBUS_CONNECTION_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), G_TYPE_DBUS_CONNECTION, GDBusConnectionClass))
#define G_DBUS_CONNECTION_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), G_TYPE_DBUS_CONNECTION, GDBusConnectionClass))
#define G_IS_DBUS_CONNECTION(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), G_TYPE_DBUS_CONNECTION))
#define G_IS_DBUS_CONNECTION_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), G_TYPE_DBUS_CONNECTION))

typedef struct _GDBusConnectionClass   GDBusConnectionClass;
typedef struct _GDBusConnectionPrivate GDBusConnectionPrivate;

/**
 * GDBusConnection:
 *
 * The #GDBusConnection structure contains only private data and
 * should only be accessed using the provided API.
 */
struct _GDBusConnection
{
  /*< private >*/
  GObject parent_instance;
  GDBusConnectionPrivate *priv;
};

/**
 * GDBusConnectionClass:
 * @disconnected: Signal class handler for the #GDBusConnection::disconnected signal.
 *
 * Class structure for #GDBusConnection.
 */
struct _GDBusConnectionClass
{
  /*< private >*/
  GObjectClass parent_class;

  /*< public >*/

  /* Signals */
  void (*disconnected) (GDBusConnection *connection);

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

GType            g_dbus_connection_get_type                   (void) G_GNUC_CONST;

void             g_dbus_connection_new                        (const gchar         *address,
                                                               GCancellable        *cancellable,
                                                               GAsyncReadyCallback  callback,
                                                               gpointer             user_data);
GDBusConnection *g_dbus_connection_new_finish                 (GAsyncResult        *res,
                                                               GError             **error);
GDBusConnection *g_dbus_connection_new_sync                   (const gchar         *address,
                                                               GCancellable       *cancellable,
                                                               GError            **error);

void             g_dbus_connection_bus_get                    (GBusType             bus_type,
                                                               GCancellable        *cancellable,
                                                               GAsyncReadyCallback  callback,
                                                               gpointer             user_data);
GDBusConnection *g_dbus_connection_bus_get_finish             (GAsyncResult        *res,
                                                               GError             **error);
GDBusConnection *g_dbus_connection_bus_get_sync               (GBusType            bus_type,
                                                               GCancellable       *cancellable,
                                                               GError            **error);

void             g_dbus_connection_bus_get_private            (GBusType             bus_type,
                                                               GCancellable        *cancellable,
                                                               GAsyncReadyCallback  callback,
                                                               gpointer             user_data);
GDBusConnection *g_dbus_connection_bus_get_private_finish     (GAsyncResult        *res,
                                                               GError             **error);
GDBusConnection *g_dbus_connection_bus_get_private_sync       (GBusType            bus_type,
                                                               GCancellable       *cancellable,
                                                               GError            **error);
GBusType         g_dbus_connection_get_bus_type               (GDBusConnection    *connection);
const gchar     *g_dbus_connection_get_address                (GDBusConnection    *connection);
const gchar     *g_dbus_connection_get_unique_name            (GDBusConnection    *connection);
gboolean         g_dbus_connection_get_is_private             (GDBusConnection    *connection);
gboolean         g_dbus_connection_get_is_disconnected        (GDBusConnection    *connection);
void             g_dbus_connection_set_exit_on_disconnect     (GDBusConnection    *connection,
                                                               gboolean            exit_on_disconnect);
void             g_dbus_connection_disconnect                 (GDBusConnection    *connection);

/**
 * GDBusInterfaceMethodCallFunc:
 * @connection: A #GDBusConnection.
 * @user_data: The @user_data #gpointer passed to g_dbus_connection_register_object().
 * @sender: The unique bus name of the remote caller.
 * @object_path: The object path that the method was invoked on.
 * @interface_name: The D-Bus interface name the method was invoked on.
 * @method_name: The name of the method that was invoked.
 * @parameters: A #GVariant tuple with parameters.
 * @invocation: A #GDBusMethodInvocation object that can be used to return a value or error.
 *
 * The type of the @method_call function in #GDBusInterfaceVTable.
 */
typedef void (*GDBusInterfaceMethodCallFunc) (GDBusConnection       *connection,
                                              gpointer               user_data,
                                              const gchar           *sender,
                                              const gchar           *object_path,
                                              const gchar           *interface_name,
                                              const gchar           *method_name,
                                              GVariant              *parameters,
                                              GDBusMethodInvocation *invocation);

/**
 * GDBusInterfaceGetPropertyFunc:
 * @connection: A #GDBusConnection.
 * @user_data: The @user_data #gpointer passed to g_dbus_connection_register_object().
 * @sender: The unique bus name of the remote caller.
 * @object_path: The object path that the method was invoked on.
 * @interface_name: The D-Bus interface name for the property.
 * @property_name: The name of the property to get the value of.
 * @error: Return location for error.
 *
 * The type of the @get_property function in #GDBusInterfaceVTable.
 *
 * Returns: A newly-allocated #GVariant with the value for @property_name or %NULL if @error is set.
 */
typedef GVariant *(*GDBusInterfaceGetPropertyFunc) (GDBusConnection       *connection,
                                                    gpointer               user_data,
                                                    const gchar           *sender,
                                                    const gchar           *object_path,
                                                    const gchar           *interface_name,
                                                    const gchar           *property_name,
                                                    GError               **error);

/**
 * GDBusInterfaceSetPropertyFunc:
 * @connection: A #GDBusConnection.
 * @user_data: The @user_data #gpointer passed to g_dbus_connection_register_object().
 * @sender: The unique bus name of the remote caller.
 * @object_path: The object path that the method was invoked on.
 * @interface_name: The D-Bus interface name for the property.
 * @property_name: The name of the property to get the value of.
 * @value: The value to set the property to.
 * @error: Return location for error.
 *
 * The type of the @set_property function in #GDBusInterfaceVTable.
 *
 * Returns: %TRUE if the property was set to @value, %FALSE if @error is set.
 */
typedef gboolean  (*GDBusInterfaceSetPropertyFunc) (GDBusConnection       *connection,
                                                    gpointer               user_data,
                                                    const gchar           *sender,
                                                    const gchar           *object_path,
                                                    const gchar           *interface_name,
                                                    const gchar           *property_name,
                                                    GVariant              *value,
                                                    GError               **error);

/**
 * GDBusInterfaceVTable:
 * @method_call: Function for handling incoming method calls.
 * @get_property: Function for getting a property.
 * @set_property: Function for setting a property.
 *
 * Virtual table for handling properties and method calls for a D-Bus
 * interface.
 *
 * If you want to handle getting/setting D-Bus properties asynchronously, simply
 * register an object with the <literal>org.freedesktop.DBus.Properties</literal>
 * D-Bus interface using g_dbus_connection_register_object().
 */
struct _GDBusInterfaceVTable
{
  GDBusInterfaceMethodCallFunc  method_call;
  GDBusInterfaceGetPropertyFunc get_property;
  GDBusInterfaceSetPropertyFunc set_property;

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

guint            g_dbus_connection_register_object            (GDBusConnection            *connection,
                                                               const gchar                *object_path,
                                                               const gchar                *interface_name,
                                                               const GDBusInterfaceInfo   *introspection_data,
                                                               const GDBusInterfaceVTable *vtable,
                                                               gpointer                    user_data,
                                                               GDestroyNotify              user_data_free_func,
                                                               GError                    **error);
gboolean         g_dbus_connection_unregister_object          (GDBusConnection            *connection,
                                                               guint                       registration_id);

/**
 * GDBusSubtreeEnumerateFunc:
 * @connection: A #GDBusConnection.
 * @user_data: The @user_data #gpointer passed to g_dbus_connection_register_subtree().
 * @sender: The unique bus name of the remote caller.
 * @object_path: The object path that was registered with g_dbus_connection_register_subtree().
 *
 * The type of the @enumerate function in #GDBusSubtreeVTable.
 *
 * Returns: A newly allocated array of strings for node names that are children of @object_path.
 */
typedef gchar** (*GDBusSubtreeEnumerateFunc) (GDBusConnection       *connection,
                                              gpointer               user_data,
                                              const gchar           *sender,
                                              const gchar           *object_path);

/**
 * GDBusSubtreeIntrospectFunc:
 * @connection: A #GDBusConnection.
 * @user_data: The @user_data #gpointer passed to g_dbus_connection_register_subtree().
 * @sender: The unique bus name of the remote caller.
 * @object_path: The object path that was registered with g_dbus_connection_register_subtree().
 * @node: A node that is a child of @object_path (relative to @object_path) or <quote>/</quote> for the root of the subtree.
 *
 * The type of the @introspect function in #GDBusSubtreeVTable.
 *
 * Returns: A newly-allocated #GPtrArray with pointers to #GDBusInterfaceInfo describing
 * the interfaces implemented by @node.
 */
typedef GPtrArray *(*GDBusSubtreeIntrospectFunc) (GDBusConnection       *connection,
                                                  gpointer               user_data,
                                                  const gchar           *sender,
                                                  const gchar           *object_path,
                                                  const gchar           *node);

/**
 * GDBusSubtreeDispatchFunc:
 * @connection: A #GDBusConnection.
 * @user_data: The @user_data #gpointer passed to g_dbus_connection_register_subtree().
 * @sender: The unique bus name of the remote caller.
 * @object_path: The object path that was registered with g_dbus_connection_register_subtree().
 * @interface_name: The D-Bus interface name that the method call or property access is for.
 * @node: A node that is a child of @object_path (relative to @object_path) or <quote>/</quote> for the root of the subtree.
 * @out_user_data: Return location for user data to pass to functions in the returned #GDBusInterfaceVTable (never %NULL).
 *
 * The type of the @dispatch function in #GDBusSubtreeVtable.
 *
 * Returns: A #GDBusInterfaceVTable or %NULL if you don't want to handle the methods.
 */
typedef const GDBusInterfaceVTable * (*GDBusSubtreeDispatchFunc) (GDBusConnection             *connection,
                                                                  gpointer                     user_data,
                                                                  const gchar                 *sender,
                                                                  const gchar                 *object_path,
                                                                  const gchar                 *interface_name,
                                                                  const gchar                 *node,
                                                                  gpointer                    *out_user_data);

/**
 * GDBusSubtreeVTable:
 * @enumerate: Function for enumerating child nodes.
 * @introspect: Function for introspecting a child node.
 * @dispatch: Function for dispatching a remote call on a child node.
 *
 * Virtual table for handling subtrees registered with g_dbus_connection_register_subtree().
 */
struct _GDBusSubtreeVTable
{
  GDBusSubtreeEnumerateFunc  enumerate;
  GDBusSubtreeIntrospectFunc introspect;
  GDBusSubtreeDispatchFunc   dispatch;

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

guint            g_dbus_connection_register_subtree           (GDBusConnection            *connection,
                                                               const gchar                *object_path,
                                                               const GDBusSubtreeVTable   *vtable,
                                                               gboolean                    is_dynamic,
                                                               gpointer                    user_data,
                                                               GDestroyNotify              user_data_free_func,
                                                               GError                    **error);
gboolean         g_dbus_connection_unregister_subtree         (GDBusConnection            *connection,
                                                               guint                       registration_id);

gboolean  g_dbus_connection_emit_signal                       (GDBusConnection    *connection,
                                                               const gchar        *destination_bus_name,
                                                               const gchar        *object_path,
                                                               const gchar        *interface_name,
                                                               const gchar        *signal_name,
                                                               GVariant           *parameters,
                                                               GError            **error);
void      g_dbus_connection_invoke_method                     (GDBusConnection    *connection,
                                                               const gchar        *bus_name,
                                                               const gchar        *object_path,
                                                               const gchar        *interface_name,
                                                               const gchar        *method_name,
                                                               GVariant           *parameters,
                                                               gint                timeout_msec,
                                                               GCancellable       *cancellable,
                                                               GAsyncReadyCallback callback,
                                                               gpointer            user_data);
GVariant *g_dbus_connection_invoke_method_finish              (GDBusConnection    *connection,
                                                               GAsyncResult       *res,
                                                               GError            **error);
GVariant *g_dbus_connection_invoke_method_sync                (GDBusConnection    *connection,
                                                               const gchar        *bus_name,
                                                               const gchar        *object_path,
                                                               const gchar        *interface_name,
                                                               const gchar        *method_name,
                                                               GVariant           *parameters,
                                                               gint                timeout_msec,
                                                               GCancellable       *cancellable,
                                                               GError            **error);

/**
 * GDBusSignalCallback:
 * @connection: A #GDBusConnection.
 * @sender_name: The unique bus name of the sender of the signal.
 * @object_path: The object path that the signal was emitted on.
 * @interface_name: The name of the signal.
 * @signal_name: The name of the signal.
 * @parameters: A #GVariant tuple with parameters for the signal.
 * @user_data: User data passed when subscribing to the signal.
 *
 * Signature for callback function used in g_dbus_connection_signal_subscribe().
 */
typedef void (*GDBusSignalCallback) (GDBusConnection  *connection,
                                     const gchar      *sender_name,
                                     const gchar      *object_path,
                                     const gchar      *interface_name,
                                     const gchar      *signal_name,
                                     GVariant         *parameters,
                                     gpointer          user_data);

guint            g_dbus_connection_signal_subscribe           (GDBusConnection     *connection,
                                                               const gchar         *sender,
                                                               const gchar         *interface_name,
                                                               const gchar         *member,
                                                               const gchar         *object_path,
                                                               const gchar         *arg0,
                                                               GDBusSignalCallback  callback,
                                                               gpointer             user_data,
                                                               GDestroyNotify      user_data_free_func);
void             g_dbus_connection_signal_unsubscribe         (GDBusConnection     *connection,
                                                               guint                subscription_id);

G_END_DECLS

#endif /* __G_DBUS_CONNECTION_H__ */
