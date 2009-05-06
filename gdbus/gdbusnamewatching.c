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

#include "gbusnamewatcher.h"
#include "gdbusnamewatching.h"
#include "gdbuserror.h"
#include "gdbusprivate.h"

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
  GBusNameAppearedCallback  name_appeared_handler;
  GBusNameVanishedCallback  name_vanished_handler;
  gpointer                  user_data;
  GBusNameWatcher          *watcher;

  gulong                    name_appeared_signal_handler_id;
  gulong                    name_vanished_signal_handler_id;

  PreviousCall              previous_call;

  gulong                    is_initialized_notify_id;
  guint                     idle_id;

  gboolean                  cancelled;
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
      if (client->name_appeared_signal_handler_id > 0)
        g_signal_handler_disconnect (client->watcher, client->name_appeared_signal_handler_id);
      if (client->name_vanished_signal_handler_id > 0)
        g_signal_handler_disconnect (client->watcher, client->name_vanished_signal_handler_id);
      if (client->is_initialized_notify_id > 0)
        g_signal_handler_disconnect (client->watcher, client->is_initialized_notify_id);
      if (client->idle_id > 0)
        g_source_remove (client->idle_id);
      g_object_unref (client->watcher);
      g_free (client);
    }
}

static void
on_name_appeared (GBusNameWatcher *watcher,
                  gpointer         user_data)
{
  Client *client = user_data;
  g_warn_if_fail (client->previous_call == PREVIOUS_CALL_NONE || client->previous_call == PREVIOUS_CALL_VANISHED);
  client->previous_call = PREVIOUS_CALL_APPEARED;
  if (client->name_appeared_handler != NULL)
    {
      client->name_appeared_handler (g_bus_name_watcher_get_connection (client->watcher),
                                     g_bus_name_watcher_get_name (client->watcher),
                                     g_bus_name_watcher_get_name_owner (client->watcher),
                                     client->user_data);
    }
}

static void
on_name_vanished (GBusNameWatcher *watcher,
                  gpointer         user_data)
{
  Client *client = user_data;
  g_warn_if_fail (client->previous_call == PREVIOUS_CALL_NONE || client->previous_call == PREVIOUS_CALL_APPEARED);
  client->previous_call = PREVIOUS_CALL_VANISHED;
  if (client->name_vanished_handler != NULL)
    {
      client->name_vanished_handler (g_bus_name_watcher_get_connection (client->watcher),
                                     g_bus_name_watcher_get_name (client->watcher),
                                     client->user_data);
    }
}

/* must only be called with lock held */
static void
client_connect_signals (Client *client)
{
  client->name_appeared_signal_handler_id = g_signal_connect (client->watcher,
                                                              "name-appeared",
                                                              G_CALLBACK (on_name_appeared),
                                                              client);
  client->name_vanished_signal_handler_id = g_signal_connect (client->watcher,
                                                              "name-vanished",
                                                              G_CALLBACK (on_name_vanished),
                                                              client);
}

/* must be called without lock held */
static void
client_do_initial_callbacks (Client *client)
{
  const gchar *name_owner;

  name_owner = g_bus_name_watcher_get_name_owner (client->watcher);
  if (name_owner != NULL)
    {
      g_warn_if_fail (client->previous_call == PREVIOUS_CALL_NONE);
      client->previous_call = PREVIOUS_CALL_APPEARED;
      if (client->name_appeared_handler != NULL)
        {
          client->name_appeared_handler (g_bus_name_watcher_get_connection (client->watcher),
                                         g_bus_name_watcher_get_name (client->watcher),
                                         name_owner,
                                         client->user_data);
        }
    }
  else
    {
      g_warn_if_fail (client->previous_call == PREVIOUS_CALL_NONE);
      client->previous_call = PREVIOUS_CALL_VANISHED;
      if (client->name_vanished_handler != NULL)
        {
          client->name_vanished_handler (g_bus_name_watcher_get_connection (client->watcher),
                                         g_bus_name_watcher_get_name (client->watcher),
                                         client->user_data);
        }
    }
}

static void
on_is_initialized_notify_cb (GBusNameWatcher *watcher,
                             GParamSpec      *pspec,
                             gpointer         user_data)
{
  Client *client = user_data;
  G_LOCK (lock);
  /* disconnect from signal */
  if (client->cancelled)
    goto out;
  g_signal_handler_disconnect (client->watcher, client->is_initialized_notify_id);
  client->is_initialized_notify_id = 0;
  client_connect_signals (client);
  G_UNLOCK (lock);
  client_do_initial_callbacks (client);
  client_unref (client);
  return;

 out:
  client_unref (client);
  G_UNLOCK (lock);
}

static gboolean
do_callback_in_idle (gpointer user_data)
{
  Client *client = user_data;
  G_LOCK (lock);
  client->idle_id = 0;
  if (client->cancelled)
    goto out;
  client_connect_signals (client);
  G_UNLOCK (lock);
  client_do_initial_callbacks (client);
  client_unref (client);
  return FALSE;

 out:
  client_unref (client);
  G_UNLOCK (lock);
  return FALSE;
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
 * #GDBusConnection that is used for watching the name is closed, then
 * @name_vanished_handler is invoked since it is no longer
 * possible to access the name. Similarly, if the connection is opened
 * again, @name_appeared_handler is invoked if and when it turns
 * out someone owns the name.
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
  client->id = next_global_id++; /* TODO: uh oh, handle overflow */
  client->ref_count = 1;
  client->name_appeared_handler = name_appeared_handler;
  client->name_vanished_handler = name_vanished_handler;
  client->user_data = user_data;
  client->watcher = g_bus_name_watcher_new (bus_type, name);
  if (!g_bus_name_watcher_get_is_initialized (client->watcher))
    {
      /* wait for :is_initialized to change before reporting anything */
      client->is_initialized_notify_id = g_signal_connect (client->watcher,
                                                           "notify::is-initialized",
                                                           G_CALLBACK (on_is_initialized_notify_cb),
                                                           client_ref (client));
    }
  else
    {
      /* is already initialized, do callback in idle */
      client->idle_id = g_idle_add ((GSourceFunc) do_callback_in_idle,
                                    client);
    }

  if (map_id_to_client == NULL)
    {
      map_id_to_client = g_hash_table_new (g_direct_hash, g_direct_equal);
    }
  g_hash_table_insert (map_id_to_client,
                       GUINT_TO_POINTER (client->id),
                       client);

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
      if (client->previous_call == PREVIOUS_CALL_APPEARED)
        {
          if (client->name_vanished_handler != NULL)
            client->name_vanished_handler (g_bus_name_watcher_get_connection (client->watcher),
                                           g_bus_name_watcher_get_name (client->watcher),
                                           client->user_data);
        }
      client_unref (client);
    }
}

#define __G_DBUS_NAME_WATCHING_C__
#include "gdbusaliasdef.c"
