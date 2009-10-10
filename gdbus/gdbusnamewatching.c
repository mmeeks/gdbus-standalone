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

#include "gdbusnamewatching.h"
#include "gdbuserror.h"
#include "gdbusprivate.h"
#include "gdbusconnection.h"
#include "gdbusconnection-lowlevel.h"


#include "gdbusalias.h"

/**
 * SECTION:gdbusnamewatching
 * @title: Watching Bus Names
 * @short_description: Simple API for watching bus names
 * @include: gdbus/gdbus.h
 *
 * Convenience API for watching bus names.
 *
 * <example id="gdbus-watching-names"><title>Simple application watching a name</title><programlisting><xi:include xmlns:xi="http://www.w3.org/2001/XInclude" parse="text" href="../../../../gdbus/example-watch-name.c"><xi:fallback>FIXME: MISSING XINCLUDE CONTENT</xi:fallback></xi:include></programlisting></example>
 */

G_LOCK_DEFINE_STATIC (lock);

/* ---------------------------------------------------------------------------------------------------- */

typedef enum
{
  PREVIOUS_CALL_NONE = 0,
  PREVIOUS_CALL_APPEARED,
  PREVIOUS_CALL_VANISHED,
} PreviousCall;

typedef struct
{
  volatile gint             ref_count;
  guint                     id;
  gchar                    *name;
  gchar                    *name_owner;
  GBusNameAppearedCallback  name_appeared_handler;
  GBusNameVanishedCallback  name_vanished_handler;
  gpointer                  user_data;

  GDBusConnection          *connection;
  gulong                    disconnected_signal_handler_id;
  guint                     name_owner_changed_subscription_id;

  PreviousCall              previous_call;

  gboolean                  cancelled;
  gboolean                  initialized;
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
          if (client->name_owner_changed_subscription_id > 0)
            g_dbus_connection_dbus_1_signal_unsubscribe (client->connection, client->name_owner_changed_subscription_id);
          if (client->disconnected_signal_handler_id > 0)
            g_signal_handler_disconnect (client->connection, client->disconnected_signal_handler_id);
          g_object_unref (client->connection);
        }
      g_free (client->name);
      g_free (client->name_owner);
      g_free (client);
    }
}

/* ---------------------------------------------------------------------------------------------------- */

static void
call_appeared_handler (Client *client)
{
  if (client->previous_call != PREVIOUS_CALL_APPEARED)
    {
      client->previous_call = PREVIOUS_CALL_APPEARED;
      if (!client->cancelled && client->name_appeared_handler != NULL)
        {
          client->name_appeared_handler (client->connection,
                                         client->name,
                                         client->name_owner,
                                         client->user_data);
        }
    }
}

static void
call_vanished_handler (Client  *client,
                       gboolean ignore_cancelled)
{
  if (client->previous_call != PREVIOUS_CALL_VANISHED)
    {
      client->previous_call = PREVIOUS_CALL_VANISHED;
      if (((!client->cancelled) || ignore_cancelled) && client->name_vanished_handler != NULL)
        {
          client->name_vanished_handler (client->connection,
                                         client->name,
                                         client->user_data);
        }
    }
}

/* ---------------------------------------------------------------------------------------------------- */

static void
on_connection_disconnected (GDBusConnection *connection,
                            gpointer         user_data)
{
  Client *client = user_data;

  if (client->name_owner_changed_subscription_id > 0)
    g_dbus_connection_dbus_1_signal_unsubscribe (client->connection, client->name_owner_changed_subscription_id);
  if (client->disconnected_signal_handler_id > 0)
    g_signal_handler_disconnect (client->connection, client->disconnected_signal_handler_id);
  g_object_unref (client->connection);
  client->disconnected_signal_handler_id = 0;
  client->name_owner_changed_subscription_id = 0;
  client->connection = NULL;

  call_vanished_handler (client, FALSE);
}

/* ---------------------------------------------------------------------------------------------------- */

static void
on_name_owner_changed (GDBusConnection *connection,
                       DBusMessage     *message,
                       gpointer         user_data)
{
  Client *client = user_data;
  const gchar *name;
  const gchar *old_owner;
  const gchar *new_owner;
  DBusError dbus_error;

  if (!client->initialized)
    goto out;

  /* extract parameters */
  dbus_error_init (&dbus_error);
  if (!dbus_message_get_args (message,
                              &dbus_error,
                              DBUS_TYPE_STRING, &name,
                              DBUS_TYPE_STRING, &old_owner,
                              DBUS_TYPE_STRING, &new_owner,
                              DBUS_TYPE_INVALID))
    {
      g_warning ("Error extracting parameters for NameOwnerChanged signal: %s: %s",
                 dbus_error.name, dbus_error.message);
      dbus_error_free (&dbus_error);
      goto out;
    }

  /* we only care about a specific name */
  if (g_strcmp0 (name, client->name) != 0)
    goto out;

  if ((old_owner != NULL && strlen (old_owner) > 0) && client->name_owner != NULL)
    {
      g_free (client->name_owner);
      client->name_owner = NULL;
      call_vanished_handler (client, FALSE);
    }

  if (new_owner != NULL && strlen (new_owner) > 0)
    {
      g_warn_if_fail (client->name_owner == NULL);
      g_free (client->name_owner);
      client->name_owner = g_strdup (new_owner);
      call_appeared_handler (client);
    }

 out:
  ;
}

/* ---------------------------------------------------------------------------------------------------- */

static void
get_name_owner_cb (GObject      *source_object,
                   GAsyncResult *res,
                   gpointer      user_data)
{
  Client *client = user_data;
  DBusMessage *reply;
  DBusError dbus_error;
  const char *name_owner;

  name_owner = NULL;
  reply = NULL;

  reply = g_dbus_connection_send_dbus_1_message_with_reply_finish (client->connection,
                                                                   res,
                                                                   NULL);
  dbus_error_init (&dbus_error);
  if (reply != NULL && !dbus_set_error_from_message (&dbus_error, reply))
    {
      dbus_message_get_args (reply,
                             &dbus_error,
                             DBUS_TYPE_STRING, &name_owner,
                             DBUS_TYPE_INVALID);
    }
  if (dbus_error_is_set (&dbus_error))
    {
      dbus_error_free (&dbus_error);
    }

  if (name_owner != NULL)
    {
      g_warn_if_fail (client->name_owner == NULL);
      client->name_owner = g_strdup (name_owner);
      call_appeared_handler (client);
    }
  else
    {
      call_vanished_handler (client, FALSE);
    }

  client->initialized = TRUE;

  if (reply != NULL)
    dbus_message_unref (reply);
  client_unref (client);
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

  /* start listening to NameOwnerChanged messages immediately */
  client->name_owner_changed_subscription_id =
    g_dbus_connection_dbus_1_signal_subscribe (client->connection,
                                               DBUS_SERVICE_DBUS,
                                               DBUS_INTERFACE_DBUS,
                                               "NameOwnerChanged",
                                               DBUS_PATH_DBUS,
                                               client->name,
                                               on_name_owner_changed,
                                               client,
                                               NULL);

  /* check owner */
  if ((message = dbus_message_new_method_call (DBUS_SERVICE_DBUS,
                                               DBUS_PATH_DBUS,
                                               DBUS_INTERFACE_DBUS,
                                               "GetNameOwner")) == NULL)
    _g_dbus_oom ();
  if (!dbus_message_append_args (message,
                                 DBUS_TYPE_STRING, &client->name,
                                 DBUS_TYPE_INVALID))
    _g_dbus_oom ();
  g_dbus_connection_send_dbus_1_message_with_reply (client->connection,
                                                    message,
                                                    -1,
                                                    NULL,
                                                    (GAsyncReadyCallback) get_name_owner_cb,
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
      call_vanished_handler (client, FALSE);
      goto out;
    }

  has_connection (client);

 out:
  client_unref (client);
}

/* ---------------------------------------------------------------------------------------------------- */

/**
 * g_bus_watch_name:
 * @bus_type: The type of bus to watch a name on (can't be #G_BUS_TYPE_NONE).
 * @name: The name (well-known or unique) to watch.
 * @name_appeared_handler: Handler to invoke when @name is known to exist.
 * @name_vanished_handler: Handler to invoke when @name is known to not exist.
 * @user_data: User data to pass to handlers.
 *
 * Starts watching @name on the bus specified by @bus_type and calls
 * @name_appeared_handler and @name_vanished_handler when the name is
 * known to have a owner respectively known to lose its owner.
 *
 * You are guaranteed that one of the handlers will be invoked (on the
 * main thread) after calling this function. When you are done
 * watching the name, just call g_bus_unwatch_name() with the watcher
 * id this function returns.
 *
 * If the name vanishes or appears (for example the application owning
 * the name could restart), the handlers are also invoked. If the
 * #GDBusConnection that is used for watching the name disconnects, then
 * @name_vanished_handler is invoked since it is no longer
 * possible to access the name.
 *
 * Another guarantee is that invocations of @name_appeared_handler
 * and @name_vanished_handler are guaranteed to alternate; that
 * is, if @name_appeared_handler is invoked then you are
 * guaranteed that the next time one of the handlers is invoked, it
 * will be @name_vanished_handler. The reverse is also true.
 *
 * This behavior makes it very simple to write applications that wants
 * to take action when a certain name exists, see <xref
 * linkend="gdbus-watching-names"/>. Basically, the application
 * should create object proxies in @name_appeared_handler and destroy
 * them again (if any) in @name_vanished_handler.
 *
 * Returns: An identifier (never 0) that an be used with
 * g_bus_unwatch_name() to stop watching the name.
 **/
guint
g_bus_watch_name (GBusType                  bus_type,
                  const gchar              *name,
                  GBusNameAppearedCallback  name_appeared_handler,
                  GBusNameVanishedCallback  name_vanished_handler,
                  gpointer                  user_data)
{
  Client *client;

  g_return_val_if_fail (bus_type != G_BUS_TYPE_NONE, 0);
  g_return_val_if_fail (name != NULL, 0);
  g_return_val_if_fail (name_appeared_handler != NULL, 0);
  g_return_val_if_fail (name_vanished_handler != NULL, 0);

  G_LOCK (lock);

  client = g_new0 (Client, 1);
  client->ref_count = 1;
  client->id = next_global_id++; /* TODO: uh oh, handle overflow */
  client->name = g_strdup (name);
  client->name_appeared_handler = name_appeared_handler;
  client->name_vanished_handler = name_vanished_handler;
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
 * g_bus_unwatch_name:
 * @watcher_id: An identifier obtained from g_bus_watch_name()
 *
 * Stops watching a name.
 *
 * If the name being watched currently has an owner the name (e.g. @name_appeared_handler
 * was the last handler to be invoked), then @name_vanished_handler will be invoked
 * before this function returns.
 **/
void
g_bus_unwatch_name (guint watcher_id)
{
  Client *client;

  client = NULL;

  G_LOCK (lock);
  if (watcher_id == 0 ||
      map_id_to_client == NULL ||
      (client = g_hash_table_lookup (map_id_to_client, GUINT_TO_POINTER (watcher_id))) == NULL)
    {
      g_warning ("Invalid id %d passed to g_bus_unwatch_name()", watcher_id);
      goto out;
    }

  client->cancelled = TRUE;
  g_warn_if_fail (g_hash_table_remove (map_id_to_client, GUINT_TO_POINTER (watcher_id)));

 out:
  G_UNLOCK (lock);

  /* do callback without holding lock */
  if (client != NULL)
    {
      call_vanished_handler (client, TRUE);
      client_unref (client);
    }
}

#define __G_DBUS_NAME_WATCHING_C__
#include "gdbusaliasdef.c"
