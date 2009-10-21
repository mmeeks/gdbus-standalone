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
  GDestroyNotify            user_data_free_func;
  GMainContext             *main_context;

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
            g_dbus_connection_signal_unsubscribe (client->connection, client->name_owner_changed_subscription_id);
          if (client->disconnected_signal_handler_id > 0)
            g_signal_handler_disconnect (client->connection, client->disconnected_signal_handler_id);
          g_object_unref (client->connection);
        }
      g_free (client->name);
      g_free (client->name_owner);
      if (client->main_context != NULL)
        g_main_context_unref (client->main_context);
      if (client->user_data_free_func != NULL)
        client->user_data_free_func (client->user_data);
      g_free (client);
    }
}

static gboolean
schedule_unref_in_idle_cb (gpointer data)
{
  Client *client = data;
  client_unref (client);
  return FALSE;
}

static void
schedule_unref_in_idle (Client *client)
{
  GSource *idle_source;

  idle_source = g_idle_source_new ();
  g_source_set_priority (idle_source, G_PRIORITY_HIGH);
  g_source_set_callback (idle_source,
                         schedule_unref_in_idle_cb,
                         client_ref (client),
                         (GDestroyNotify) client_unref);
  g_source_attach (idle_source, client->main_context);
  g_source_unref (idle_source);
}

/* ---------------------------------------------------------------------------------------------------- */

typedef struct
{
  Client *client;

  /* keep this separate because client->connection may
   * be set to NULL after scheduling the call
   */
  GDBusConnection *connection;

  /* ditto */
  gchar *name_owner;

  /* set to TRUE to call appeared */
  gboolean call_appeared;
} CallHandlerData;

static void
call_handler_data_free (CallHandlerData *data)
{
  if (data->connection != NULL)
    g_object_unref (data->connection);
  g_free (data->name_owner);
  client_unref (data->client);
  g_free (data);
}

static gboolean
call_in_idle_cb (gpointer _data)
{
  CallHandlerData *data = _data;

  if (data->call_appeared)
    {
      if (data->client->name_appeared_handler != NULL)
        {
          data->client->name_appeared_handler (data->connection,
                                               data->client->name,
                                               data->name_owner,
                                               data->client->user_data);
        }
    }
  else
    {
      if (data->client->name_vanished_handler != NULL)
        {
          data->client->name_vanished_handler (data->connection,
                                               data->client->name,
                                               data->client->user_data);
        }
    }

  return FALSE;
}

static void
schedule_call_in_idle (Client *client,
                       gboolean call_appeared)
{
  CallHandlerData *data;
  GSource *idle_source;

  data = g_new0 (CallHandlerData, 1);
  data->client = client_ref (client);
  data->connection = client->connection != NULL ? g_object_ref (client->connection) : NULL;
  data->name_owner = g_strdup (client->name_owner);
  data->call_appeared = call_appeared;

  idle_source = g_idle_source_new ();
  g_source_set_priority (idle_source, G_PRIORITY_HIGH);
  g_source_set_callback (idle_source,
                         call_in_idle_cb,
                         data,
                         (GDestroyNotify) call_handler_data_free);
  g_source_attach (idle_source, client->main_context);
  g_source_unref (idle_source);
}

static void
call_appeared_handler (Client *client)
{
  if (client->previous_call != PREVIOUS_CALL_APPEARED)
    {
      client->previous_call = PREVIOUS_CALL_APPEARED;
      if (!client->cancelled && client->name_appeared_handler != NULL)
        {
          schedule_call_in_idle (client, TRUE);
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
          schedule_call_in_idle (client, FALSE);
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
    g_dbus_connection_signal_unsubscribe (client->connection, client->name_owner_changed_subscription_id);
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
                       const gchar      *sender_name,
                       const gchar      *object_path,
                       const gchar      *interface_name,
                       const gchar      *signal_name,
                       GVariant         *parameters,
                       gpointer          user_data)
{
  Client *client = user_data;
  const gchar *name;
  const gchar *old_owner;
  const gchar *new_owner;

  if (!client->initialized)
    goto out;

  if (g_strcmp0 (object_path, "/org/freedesktop/DBus") != 0 ||
      g_strcmp0 (interface_name, "org.freedesktop.DBus") != 0 ||
      g_strcmp0 (sender_name, "org.freedesktop.DBus") != 0)
    goto out;

  g_variant_get (parameters,
                 "(sss)",
                 &name,
                 &old_owner,
                 &new_owner);

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
  GVariant *result;
  const char *name_owner;

  name_owner = NULL;
  result = NULL;

  result = g_dbus_connection_invoke_method_finish (client->connection,
                                                   res,
                                                   NULL);
  if (result != NULL)
    {
      g_variant_get (result, "(s)", &name_owner);
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

  if (result != NULL)
    g_variant_unref (result);
  client_unref (client);
}

/* ---------------------------------------------------------------------------------------------------- */

static void
has_connection (Client *client)
{
  /* listen for disconnection */
  client->disconnected_signal_handler_id = g_signal_connect (client->connection,
                                                             "disconnected",
                                                             G_CALLBACK (on_connection_disconnected),
                                                             client);

  /* start listening to NameOwnerChanged messages immediately */
  client->name_owner_changed_subscription_id = g_dbus_connection_signal_subscribe (client->connection,
                                                                                   "org.freedesktop.DBus",  /* name */
                                                                                   "org.freedesktop.DBus",  /* if */
                                                                                   "NameOwnerChanged",      /* signal */
                                                                                   "/org/freedesktop/DBus", /* path */
                                                                                   client->name,
                                                                                   on_name_owner_changed,
                                                                                   client,
                                                                                   NULL);

  /* check owner */
  g_dbus_connection_invoke_method (client->connection,
                                   "org.freedesktop.DBus",  /* bus name */
                                   "/org/freedesktop/DBus", /* object path */
                                   "org.freedesktop.DBus",  /* interface name */
                                   "GetNameOwner",          /* method name */
                                   g_variant_new ("(s)", client->name),
                                   -1,
                                   NULL,
                                   (GAsyncReadyCallback) get_name_owner_cb,
                                   client_ref (client));
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
 * @user_data_free_func: Function for freeing @user_data or %NULL.
 *
 * Starts watching @name on the bus specified by @bus_type and calls
 * @name_appeared_handler and @name_vanished_handler when the name is
 * known to have a owner respectively known to lose its
 * owner. Callbacks will be invoked in the <link
 * linkend="g-main-context-push-thread-default">thread-default main
 * loop</link> of the thread you are calling this function from.
 *
 * You are guaranteed that one of the handlers will be invoked after
 * calling this function. When you are done watching the name, just
 * call g_bus_unwatch_name() with the watcher id this function
 * returns.
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
                  gpointer                  user_data,
                  GDestroyNotify            user_data_free_func)
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
  client->user_data_free_func = user_data_free_func;
  client->main_context = g_main_context_get_thread_default ();
  if (client->main_context != NULL)
    g_main_context_ref (client->main_context);

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
 * was the last handler to be invoked), then @name_vanished_handler will be invoked.
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
      schedule_unref_in_idle (client);
    }
}
