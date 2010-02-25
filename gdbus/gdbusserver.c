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

#include "gdbusconnection.h"
#include "gdbuserror.h"
#include "gdbusenumtypes.h"
#include "gdbusserver.h"
#include "gdbusprivate.h"

/**
 * SECTION:gdbusserver
 * @short_description: Helper for accepting peer-to-peer connections
 * @include: gdbus/gdbus.h
 *
 * TODO
 */

struct _GDBusServerPrivate
{
  /* construct properties */
  gchar *address;

  DBusServer *dbus_1_server;
};

enum
{
  NEW_CONNECTION_SIGNAL,
  LAST_SIGNAL
};

enum
{
  PROP_0,
  PROP_ADDRESS
};

static guint signals[LAST_SIGNAL] = { 0 };

static void on_new_dbus_1_connection (DBusServer     *server,
                                      DBusConnection *new_connection,
                                      void           *user_data);

static void initable_iface_init       (GInitableIface *initable_iface);

G_DEFINE_TYPE_WITH_CODE (GDBusServer, g_dbus_server, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (G_TYPE_INITABLE, initable_iface_init)
                         );

static void
g_dbus_server_finalize (GObject *object)
{
  GDBusServer *server = G_DBUS_SERVER (object);

  if (server->priv->dbus_1_server != NULL)
    {
      _g_dbus_unintegrate_dbus_1_server (server->priv->dbus_1_server);
      dbus_server_disconnect (server->priv->dbus_1_server);
      dbus_server_unref (server->priv->dbus_1_server);
    }

  if (G_OBJECT_CLASS (g_dbus_server_parent_class)->finalize != NULL)
    G_OBJECT_CLASS (g_dbus_server_parent_class)->finalize (object);
}

static void
g_dbus_server_get_property (GObject    *object,
                                guint       prop_id,
                                GValue     *value,
                                GParamSpec *pspec)
{
  GDBusServer *server = G_DBUS_SERVER (object);

  switch (prop_id)
    {
    case PROP_ADDRESS:
      g_value_set_string (value, g_dbus_server_get_address (server));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
g_dbus_server_set_property (GObject      *object,
                            guint         prop_id,
                            const GValue *value,
                            GParamSpec   *pspec)
{
  GDBusServer *server = G_DBUS_SERVER (object);

  switch (prop_id)
    {
    case PROP_ADDRESS:
      server->priv->address = g_value_dup_string (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}


static void
g_dbus_server_init (GDBusServer *server)
{
  server->priv = G_TYPE_INSTANCE_GET_PRIVATE (server, G_TYPE_DBUS_SERVER, GDBusServerPrivate);
}

static void
g_dbus_server_class_init (GDBusServerClass *klass)
{
  GObjectClass *gobject_class;

  g_type_class_add_private (klass, sizeof (GDBusServerPrivate));

  gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->finalize     = g_dbus_server_finalize;
  gobject_class->set_property = g_dbus_server_set_property;
  gobject_class->get_property = g_dbus_server_get_property;

  /**
   * GDBusServer:address:
   *
   * The address to listen on. See the D-Bus specification for details
   * on address formats.
   *
   * If there are multiple semicolon-separated address entries in
   * @address, tries each one and listens on the first one that works.
   */
  g_object_class_install_property (gobject_class,
                                   PROP_ADDRESS,
                                   g_param_spec_string ("address",
                                                        _("Address"),
                                                        _("The address to listen on"),
                                                        NULL,
                                                        G_PARAM_READABLE |
                                                        G_PARAM_WRITABLE |
                                                        G_PARAM_CONSTRUCT_ONLY |
                                                        G_PARAM_STATIC_NAME |
                                                        G_PARAM_STATIC_BLURB |
                                                        G_PARAM_STATIC_NICK));


  /**
   * GDBusServer::new-connection:
   * @server: The #GDBusServer emitting the signal.
   * @connection: The new connection.
   *
   * Emitted when there is a new connection. If you wish to keep the
   * connection open, take a reference to @connection. Connect to
   * #GDBusConnection::disconnected to find out when the client
   * disconnects. Use g_dbus_connection_disconnect() to disconnect the
   * client.
   *
   * Note that the returned @connection object will have
   * #GDBusConnection:exit-on-disconnect set to %FALSE.
   */
  signals[NEW_CONNECTION_SIGNAL] = g_signal_new ("new-connection",
                                                 G_TYPE_DBUS_SERVER,
                                                 G_SIGNAL_RUN_LAST,
                                                 G_STRUCT_OFFSET (GDBusServerClass, new_connection),
                                                 NULL,
                                                 NULL,
                                                 g_cclosure_marshal_VOID__OBJECT,
                                                 G_TYPE_NONE,
                                                 1,
                                                 G_TYPE_DBUS_CONNECTION);
}

/* ---------------------------------------------------------------------------------------------------- */

static gboolean
initable_init (GInitable       *initable,
               GCancellable    *cancellable,
               GError         **error)
{
  GDBusServer *server = G_DBUS_SERVER (initable);
  DBusError dbus_error;
  gboolean ret;

  ret = FALSE;

  dbus_connection_set_change_sigpipe (TRUE);

  dbus_error_init (&dbus_error);
  server->priv->dbus_1_server = dbus_server_listen (server->priv->address,
                                                    &dbus_error);
  if (server->priv->dbus_1_server != NULL)
    {
      _g_dbus_integrate_dbus_1_server (server->priv->dbus_1_server, NULL);

      dbus_server_set_new_connection_function (server->priv->dbus_1_server,
                                               on_new_dbus_1_connection,
                                               server,
                                               NULL);

      ret = TRUE;
    }
  else
    {
      if (error != NULL)
        {
          g_dbus_error_set_dbus_error (error,
                                       dbus_error.name,
                                       dbus_error.message,
                                       NULL);
          /* this is a locally generated error so strip the remote part */
          g_dbus_error_strip_remote_error (*error);
        }
      dbus_error_free (&dbus_error);
    }

  return ret;
}

static void
initable_iface_init (GInitableIface *initable_iface)
{
  initable_iface->init = initable_init;
}

/* ---------------------------------------------------------------------------------------------------- */

/**
 * g_dbus_server_new:
 * @address: The address to listen on. See the D-Bus specification for details on address formats.
 * @error: Return location for error or %NULL.
 *
 * Listens for new connections on @address. Whenever there's a new
 * connection the #GDBusServer::new-connection signal is emitted in
 * the thread where the returned #GDBusServer was constructed.
 *
 * If there are multiple semicolon-separated address entries in
 * @address, tries each one and listens on the first one that works.
 *
 * Returns: A #GDBusServer or %NULL if @error is set. Free with
 * g_object_unref().
 */
GDBusServer *
g_dbus_server_new (const gchar   *address,
                   GError       **error)
{
  GInitable *initable;

  initable = g_initable_new (G_TYPE_DBUS_SERVER,
                             NULL, /* GCancellable */
                             error,
                             "address", address,
                             NULL);

  if (initable != NULL)
    return G_DBUS_SERVER (initable);
  else
    return NULL;
}

/* ---------------------------------------------------------------------------------------------------- */

/**
 * g_dbus_server_get_address:
 * @server: A #GDBusServer.
 *
 * Gets the address that was passed when @server was constructed.
 *
 * Returns: The address that @server listens on. See the D-Bus
 * specification for details on address formats. Do not free, this
 * string is owned by @server.
 */
const gchar *
g_dbus_server_get_address (GDBusServer *server)
{
  g_return_val_if_fail (G_IS_DBUS_SERVER (server), NULL);
  return server->priv->address;
}

/* ---------------------------------------------------------------------------------------------------- */

static void
on_new_dbus_1_connection (DBusServer     *_server,
                          DBusConnection *new_connection,
                          void           *user_data)
{
  GDBusServer *server = G_DBUS_SERVER (user_data);
  GDBusConnection *connection;

  /* create a new connection */
  connection = _g_dbus_connection_new_for_dbus_1_connection (new_connection);

  /* give up ownership of connection */
  dbus_connection_unref (new_connection);

  g_signal_emit (server, signals[NEW_CONNECTION_SIGNAL], 0, connection);

  /* if the signal receivers didn't ref, we'll close the connection here */
  g_object_unref (connection);
}

extern void _g_futex_thread_init(void);

/* a hack to initialize threads */
void g_dbus_threads_init (void)
{
  /* ugly - but we need a working 'bitlock' */
  _g_futex_thread_init ();
  dbus_threads_init_default ();
}
