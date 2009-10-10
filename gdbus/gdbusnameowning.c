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

#include "gdbusnameowning.h"
#include "gdbuserror.h"
#include "gdbusprivate.h"
#include "gdbusconnection.h"
#include "gdbusconnection-lowlevel.h"

#include "gdbusalias.h"

/**
 * SECTION:gdbusnameowning
 * @title: Owning Bus Names
 * @short_description: Simple API for owning bus names
 * @include: gdbus/gdbus.h
 *
 * Convenience API for owning bus names.
 *
 * <example id="gdbus-owning-names"><title>Simple application owning a name</title><programlisting><xi:include xmlns:xi="http://www.w3.org/2001/XInclude" parse="text" href="../../../../gdbus/example-own-name.c"><xi:fallback>FIXME: MISSING XINCLUDE CONTENT</xi:fallback></xi:include></programlisting></example>
 */

G_LOCK_DEFINE_STATIC (lock);

/* ---------------------------------------------------------------------------------------------------- */

typedef enum
{
  PREVIOUS_CALL_NONE = 0,
  PREVIOUS_CALL_ACQUIRED,
  PREVIOUS_CALL_LOST,
} PreviousCall;

typedef struct
{
  volatile gint             ref_count;
  guint                     id;
  GBusNameOwnerFlags        flags;
  gchar                    *name;
  GBusNameAcquiredCallback  name_acquired_handler;
  GBusNameLostCallback      name_lost_handler;
  gpointer                  user_data;

  PreviousCall              previous_call;

  GDBusConnection          *connection;
  gulong                    disconnected_signal_handler_id;
  guint                     name_acquired_subscription_id;
  guint                     name_lost_subscription_id;

  gboolean                  cancelled;

  gboolean                  needs_release;
} Client;

static guint next_global_id = 1;
static GHashTable *map_id_to_client = NULL;


static Client *
client_ref (Client *client)
{
  g_atomic_int_inc (&client->ref_count);
  return client;
}

static void
client_unref (Client *client)
{
  if (g_atomic_int_dec_and_test (&client->ref_count))
    {
      if (client->connection != NULL)
        {
          if (client->disconnected_signal_handler_id > 0)
            g_signal_handler_disconnect (client->connection, client->disconnected_signal_handler_id);
          if (client->name_acquired_subscription_id > 0)
            g_dbus_connection_dbus_1_signal_unsubscribe (client->connection, client->name_acquired_subscription_id);
          if (client->name_lost_subscription_id > 0)
            g_dbus_connection_dbus_1_signal_unsubscribe (client->connection, client->name_lost_subscription_id);
          g_object_unref (client->connection);
        }
      g_free (client->name);
      g_free (client);
    }
}

/* ---------------------------------------------------------------------------------------------------- */

static void
call_acquired_handler (Client *client)
{
  if (client->previous_call != PREVIOUS_CALL_ACQUIRED)
    {
      client->previous_call = PREVIOUS_CALL_ACQUIRED;
      if (!client->cancelled && client->name_acquired_handler != NULL)
        {
          client->name_acquired_handler (client->connection,
                                         client->name,
                                         client->user_data);
        }
    }
}

static void
call_lost_handler (Client  *client,
                   gboolean ignore_cancelled)
{
  if (client->previous_call != PREVIOUS_CALL_LOST)
    {
      client->previous_call = PREVIOUS_CALL_LOST;
      if (((!client->cancelled) || ignore_cancelled) && client->name_lost_handler != NULL)
        {
          client->name_lost_handler (client->connection,
                                     client->name,
                                     client->user_data);
        }
    }
}

/* ---------------------------------------------------------------------------------------------------- */

static void
on_name_lost_or_acquired (GDBusConnection *connection,
                          DBusMessage     *message,
                          gpointer         user_data)
{
  Client *client = user_data;
  DBusError dbus_error;
  const gchar *name;

  dbus_error_init (&dbus_error);

  if (dbus_message_is_signal (message,
                              DBUS_INTERFACE_DBUS,
                              "NameLost") &&
      g_strcmp0 (dbus_message_get_sender (message), DBUS_SERVICE_DBUS) == 0 &&
      g_strcmp0 (dbus_message_get_path (message), DBUS_PATH_DBUS) == 0)
    {
      if (dbus_message_get_args (message,
                                 &dbus_error,
                                 DBUS_TYPE_STRING, &name,
                                 DBUS_TYPE_INVALID))
        {
          if (g_strcmp0 (name, client->name) == 0)
            {
              call_lost_handler (client, FALSE);
            }
        }
      else
        {
          g_warning ("Error extracting name for NameLost signal: %s: %s", dbus_error.name, dbus_error.message);
          dbus_error_free (&dbus_error);
        }
    }
  else if (dbus_message_is_signal (message,
                                   DBUS_INTERFACE_DBUS,
                                   "NameAcquired") &&
           g_strcmp0 (dbus_message_get_sender (message), DBUS_SERVICE_DBUS) == 0 &&
           g_strcmp0 (dbus_message_get_path (message), DBUS_PATH_DBUS) == 0)
    {
      if (dbus_message_get_args (message,
                                 &dbus_error,
                                 DBUS_TYPE_STRING, &name,
                                 DBUS_TYPE_INVALID))
        {
          if (g_strcmp0 (name, client->name) == 0)
            {
              call_acquired_handler (client);
            }
        }
      else
        {
          g_warning ("Error extracting name for NameAcquired signal: %s: %s", dbus_error.name, dbus_error.message);
          dbus_error_free (&dbus_error);
        }
    }
  else
    {
      g_warning ("Unexpected message in on_name_lost_or_acquired()");
    }
}

/* ---------------------------------------------------------------------------------------------------- */

static void
request_name_cb (GObject      *source_object,
                 GAsyncResult *res,
                 gpointer      user_data)
{
  Client *client = user_data;
  DBusMessage *reply;
  DBusError dbus_error;
  dbus_uint32_t request_name_reply;

  request_name_reply = 0;
  reply = NULL;

  reply = g_dbus_connection_send_dbus_1_message_with_reply_finish (client->connection,
                                                                   res,
                                                                   NULL);
  dbus_error_init (&dbus_error);
  if (!dbus_set_error_from_message (&dbus_error, reply))
    {
      dbus_message_get_args (reply,
                             &dbus_error,
                             DBUS_TYPE_UINT32, &request_name_reply,
                             DBUS_TYPE_INVALID);
    }
  if (dbus_error_is_set (&dbus_error))
    {
      dbus_error_free (&dbus_error);
    }
  else
    {
      gboolean subscribe;

      subscribe = FALSE;

      switch (request_name_reply)
        {
        case DBUS_REQUEST_NAME_REPLY_PRIMARY_OWNER:
          /* We got the name - now listen for NameLost and NameAcquired */
          call_acquired_handler (client);
          subscribe = TRUE;
          client->needs_release = TRUE;
          break;

        case DBUS_REQUEST_NAME_REPLY_IN_QUEUE:
          /* Waiting in line - listen for NameLost and NameAcquired */
          call_lost_handler (client, FALSE);
          subscribe = TRUE;
          client->needs_release = TRUE;
          break;

        case DBUS_REQUEST_NAME_REPLY_EXISTS:
        case DBUS_REQUEST_NAME_REPLY_ALREADY_OWNER:
          /* Some other part of the process is already owning the name */
          call_lost_handler (client, FALSE);
          break;
        }

      if (subscribe)
        {
          /* start listening to NameLost and NameAcquired messages */
          client->name_lost_subscription_id =
            g_dbus_connection_dbus_1_signal_subscribe (client->connection,
                                                       DBUS_SERVICE_DBUS,
                                                       DBUS_INTERFACE_DBUS,
                                                       "NameLost",
                                                       DBUS_PATH_DBUS,
                                                       client->name,
                                                       on_name_lost_or_acquired,
                                                       client,
                                                       NULL);
          client->name_acquired_subscription_id =
            g_dbus_connection_dbus_1_signal_subscribe (client->connection,
                                                       DBUS_SERVICE_DBUS,
                                                       DBUS_INTERFACE_DBUS,
                                                       "NameAcquired",
                                                       DBUS_PATH_DBUS,
                                                       client->name,
                                                       on_name_lost_or_acquired,
                                                       client,
                                                       NULL);
        }
    }

  if (reply != NULL)
    dbus_message_unref (reply);
  client_unref (client);
}

/* ---------------------------------------------------------------------------------------------------- */

static void
on_connection_disconnected (GDBusConnection *connection,
                            gpointer         user_data)
{
  Client *client = user_data;

  if (client->disconnected_signal_handler_id > 0)
    g_signal_handler_disconnect (client->connection, client->disconnected_signal_handler_id);
  if (client->name_acquired_subscription_id > 0)
    g_dbus_connection_dbus_1_signal_unsubscribe (client->connection, client->name_acquired_subscription_id);
  if (client->name_lost_subscription_id > 0)
    g_dbus_connection_dbus_1_signal_unsubscribe (client->connection, client->name_lost_subscription_id);
  g_object_unref (client->connection);
  client->disconnected_signal_handler_id = 0;
  client->name_acquired_subscription_id = 0;
  client->name_lost_subscription_id = 0;
  client->connection = NULL;

  call_lost_handler (client, FALSE);
}

/* ---------------------------------------------------------------------------------------------------- */

static void
has_connection (Client *client)
{
  DBusMessage *message;

  /* listen for disconnection */
  client->disconnected_signal_handler_id = g_signal_connect (client->connection,
                                                             "disconnected",
                                                             G_CALLBACK (on_connection_disconnected),
                                                             client);

  /* attempt to acquire the name */
  if ((message = dbus_message_new_method_call (DBUS_SERVICE_DBUS,
                                               DBUS_PATH_DBUS,
                                               DBUS_INTERFACE_DBUS,
                                               "RequestName")) == NULL)
    _g_dbus_oom ();
  if (!dbus_message_append_args (message,
                                 DBUS_TYPE_STRING, &client->name,
                                 DBUS_TYPE_UINT32, &client->flags,
                                 DBUS_TYPE_INVALID))
    _g_dbus_oom ();
  g_dbus_connection_send_dbus_1_message_with_reply (client->connection,
                                                    message,
                                                    -1,
                                                    NULL,
                                                    (GAsyncReadyCallback) request_name_cb,
                                                    client_ref (client));
  dbus_message_unref (message);
}


static void
connection_get_cb (GObject      *source_object,
                   GAsyncResult *res,
                   gpointer      user_data)
{
  Client *client = user_data;

  client->connection = g_dbus_connection_bus_get_finish (res, NULL);
  if (client->connection == NULL)
    {
      call_lost_handler (client, FALSE);
      goto out;
    }

  has_connection (client);

 out:
  client_unref (client);
}

/* ---------------------------------------------------------------------------------------------------- */

/**
 * g_bus_own_name_on_connection:
 * @connection: A #GDBusConnection that has not been disconnected.
 * @name: The well-known name to own.
 * @flags: A set of flags from the #GBusNameOwnerFlags enumeration.
 * @name_acquired_handler: Handler to invoke when @name is acquired.
 * @name_lost_handler: Handler to invoke when @name is lost.
 * @user_data: User data to pass to handlers.
 *
 * Like g_bus_own_name() but takes a #GDBusConnection instead of a
 * #GBusType.
 *
 * Returns: An identifier (never 0) that an be used with
 * g_bus_unown_name() to stop owning the name.
 **/
guint
g_bus_own_name_on_connection (GDBusConnection          *connection,
                              const gchar              *name,
                              GBusNameOwnerFlags        flags,
                              GBusNameAcquiredCallback  name_acquired_handler,
                              GBusNameLostCallback      name_lost_handler,
                              gpointer                  user_data)
{
  Client *client;

  g_return_val_if_fail (connection != NULL, 0);
  g_return_val_if_fail (!g_dbus_connection_get_is_disconnected (connection), 0);
  g_return_val_if_fail (name != NULL, 0);
  //g_return_val_if_fail (TODO_is_well_known_name (), 0);
  g_return_val_if_fail (name_acquired_handler != NULL, 0);
  g_return_val_if_fail (name_lost_handler != NULL, 0);

  G_LOCK (lock);

  client = g_new0 (Client, 1);
  client->ref_count = 1;
  client->id = next_global_id++; /* TODO: uh oh, handle overflow */
  client->name = g_strdup (name);
  client->flags = flags;
  client->name_acquired_handler = name_acquired_handler;
  client->name_lost_handler = name_lost_handler;
  client->user_data = user_data;
  client->connection = g_object_ref (connection);

  if (map_id_to_client == NULL)
    {
      map_id_to_client = g_hash_table_new (g_direct_hash, g_direct_equal);
    }
  g_hash_table_insert (map_id_to_client,
                       GUINT_TO_POINTER (client->id),
                       client);

  G_UNLOCK (lock);

  has_connection (client);

  return client->id;
}

/**
 * g_bus_own_name:
 * @bus_type: The type of bus to own a name on (can't be #G_BUS_TYPE_NONE).
 * @name: The well-known name to own.
 * @flags: A set of flags from the #GBusNameOwnerFlags enumeration.
 * @name_acquired_handler: Handler to invoke when @name is acquired.
 * @name_lost_handler: Handler to invoke when @name is lost.
 * @user_data: User data to pass to handlers.
 *
 * Starts acquiring @name on the bus specified by @bus_type and calls
 * @name_acquired_handler and @name_lost_handler when the name is
 * acquired respectively lost.
 *
 * You are guaranteed that one of the handlers will be invoked (on the
 * main thread) after calling this function. When you are done owning
 * the name, just call g_bus_unown_name() with the owner id this
 * function returns.
 *
 * If the name is acquired or lost (for example another application
 * could acquire the name if you allow replacement), the handlers are
 * also invoked. If the #GDBusConnection that is used for attempting
 * to own the name disconnects, then @name_lost_handler is invoked since
 * it is no longer possible for other processes to access the
 * process.
 *
 * You cannot use g_bus_own_name() several times (unless interleaved
 * with calls to g_bus_unown_name()) - only the first call will work.
 *
 * Another guarantee is that invocations of @name_acquired_handler
 * and @name_lost_handler are guaranteed to alternate; that
 * is, if @name_acquired_handler is invoked then you are
 * guaranteed that the next time one of the handlers is invoked, it
 * will be @name_lost_handler. The reverse is also true.
 *
 * This behavior makes it very simple to write applications that wants
 * to own names, see <xref linkend="gdbus-owning-names"/>. Simply
 * register objects to be exported in @name_acquired_handler and
 * unregister the objects (if any) in @name_lost_handler.
 *
 * Returns: An identifier (never 0) that an be used with
 * g_bus_unown_name() to stop owning the name.
 **/
guint
g_bus_own_name (GBusType                  bus_type,
                const gchar              *name,
                GBusNameOwnerFlags        flags,
                GBusNameAcquiredCallback  name_acquired_handler,
                GBusNameLostCallback      name_lost_handler,
                gpointer                  user_data)
{
  Client *client;

  g_return_val_if_fail (bus_type != G_BUS_TYPE_NONE, 0);
  g_return_val_if_fail (name != NULL, 0);
  //g_return_val_if_fail (TODO_is_well_known_name (), 0);
  g_return_val_if_fail (name_acquired_handler != NULL, 0);
  g_return_val_if_fail (name_lost_handler != NULL, 0);

  G_LOCK (lock);

  client = g_new0 (Client, 1);
  client->ref_count = 1;
  client->id = next_global_id++; /* TODO: uh oh, handle overflow */
  client->name = g_strdup (name);
  client->flags = flags;
  client->name_acquired_handler = name_acquired_handler;
  client->name_lost_handler = name_lost_handler;
  client->user_data = user_data;

  if (map_id_to_client == NULL)
    {
      map_id_to_client = g_hash_table_new (g_direct_hash, g_direct_equal);
    }
  g_hash_table_insert (map_id_to_client,
                       GUINT_TO_POINTER (client->id),
                       client);

  g_dbus_connection_bus_get (bus_type,
                             NULL,
                             connection_get_cb,
                             client_ref (client));
  G_UNLOCK (lock);

  return client->id;
}

/**
 * g_bus_unown_name:
 * @owner_id: An identifier obtained from g_bus_own_name()
 *
 * Stops owning a name.
 *
 * If currently owning the name (e.g. @name_acquired_handler was the
 * last handler to be invoked), then @name_lost_handler will be invoked
 * before this function returns.
 **/
void
g_bus_unown_name (guint owner_id)
{
  Client *client;

  client = NULL;

  G_LOCK (lock);
  if (owner_id == 0 || map_id_to_client == NULL ||
      (client = g_hash_table_lookup (map_id_to_client, GUINT_TO_POINTER (owner_id))) == NULL)
    {
      g_warning ("Invalid id %d passed to g_bus_unown_name()", owner_id);
      goto out;
    }

  client->cancelled = TRUE;
  g_warn_if_fail (g_hash_table_remove (map_id_to_client, GUINT_TO_POINTER (owner_id)));

 out:
  G_UNLOCK (lock);

  /* do callback without holding lock */
  if (client != NULL)
    {
      /* Release the name if needed */
      if (client->needs_release && client->connection != NULL)
        {
          DBusMessage *message;
          DBusMessage *reply;
          DBusError dbus_error;
          dbus_uint32_t release_name_reply;

          if ((message = dbus_message_new_method_call (DBUS_SERVICE_DBUS,
                                                       DBUS_PATH_DBUS,
                                                       DBUS_INTERFACE_DBUS,
                                                       "ReleaseName")) == NULL)
            _g_dbus_oom ();
          if (!dbus_message_append_args (message,
                                         DBUS_TYPE_STRING, &client->name,
                                         DBUS_TYPE_INVALID))
            _g_dbus_oom ();

          /* TODO: it kinda sucks having to do a sync call to release the name - but if
           * we don't, then a subsequent grab of the name will make the bus daemon return
           * IN_QUEUE which will trigger name_lost().
           *
           * I believe this is a bug in the bus daemon.
           */
          reply = g_dbus_connection_send_dbus_1_message_with_reply_sync (client->connection,
                                                                         message,
                                                                         -1,
                                                                         NULL,
                                                                         NULL);
          dbus_error_init (&dbus_error);
          if (!dbus_set_error_from_message (&dbus_error, reply))
            {
              dbus_message_get_args (reply,
                                     &dbus_error,
                                     DBUS_TYPE_UINT32, &release_name_reply,
                                     DBUS_TYPE_INVALID);
            }
          if (dbus_error_is_set (&dbus_error))
            {
              g_warning ("Error releasing name %s: %s: %s", client->name, dbus_error.name, dbus_error.message);
              dbus_error_free (&dbus_error);
            }
          else
            {
              if (release_name_reply != DBUS_RELEASE_NAME_REPLY_RELEASED)
                {
                  g_warning ("Unexpected reply %d when releasing name %s", release_name_reply, client->name);
                }
            }
          dbus_message_unref (reply);
          dbus_message_unref (message);


          call_lost_handler (client, TRUE);

          if (client->disconnected_signal_handler_id > 0)
            g_signal_handler_disconnect (client->connection, client->disconnected_signal_handler_id);
          if (client->name_acquired_subscription_id > 0)
            g_dbus_connection_dbus_1_signal_unsubscribe (client->connection, client->name_acquired_subscription_id);
          if (client->name_lost_subscription_id > 0)
            g_dbus_connection_dbus_1_signal_unsubscribe (client->connection, client->name_lost_subscription_id);
          g_object_unref (client->connection);
          client->disconnected_signal_handler_id = 0;
          client->name_acquired_subscription_id = 0;
          client->name_lost_subscription_id = 0;
          client->connection = NULL;
        }
      else
        {
          call_lost_handler (client, TRUE);
        }

      client_unref (client);
    }
}

#define __G_DBUS_NAME_OWNING_C__
#include "gdbusaliasdef.c"
