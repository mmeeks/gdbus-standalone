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

#include "gbusnameowner.h"
#include "gdbusnameowning.h"
#include "gdbuserror.h"
#include "gdbusprivate.h"

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
  GBusNameAcquiredCallback  name_acquired_handler;
  GBusNameLostCallback      name_lost_handler;
  gpointer                  user_data;
  GBusNameOwner            *owner;

  gulong                    name_acquired_signal_handler_id;
  gulong                    name_lost_signal_handler_id;

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
      if (client->name_acquired_signal_handler_id > 0)
        g_signal_handler_disconnect (client->owner, client->name_acquired_signal_handler_id);
      if (client->name_lost_signal_handler_id > 0)
        g_signal_handler_disconnect (client->owner, client->name_lost_signal_handler_id);
      if (client->is_initialized_notify_id > 0)
        g_signal_handler_disconnect (client->owner, client->is_initialized_notify_id);
      if (client->idle_id > 0)
        g_source_remove (client->idle_id);
      g_object_unref (client->owner);
      g_free (client);
    }
}

static void
on_name_acquired (GBusNameOwner *owner,
                  gpointer         user_data)
{
  Client *client = user_data;
  g_warn_if_fail (client->previous_call == PREVIOUS_CALL_NONE || client->previous_call == PREVIOUS_CALL_LOST);
  client->previous_call = PREVIOUS_CALL_ACQUIRED;
  if (client->name_acquired_handler != NULL)
    {
      client->name_acquired_handler (g_bus_name_owner_get_connection (client->owner),
                                     g_bus_name_owner_get_name (client->owner),
                                     client->user_data);
    }
}

static void
on_name_lost (GBusNameOwner *owner,
                  gpointer         user_data)
{
  Client *client = user_data;
  g_warn_if_fail (client->previous_call == PREVIOUS_CALL_NONE || client->previous_call == PREVIOUS_CALL_ACQUIRED);
  client->previous_call = PREVIOUS_CALL_LOST;
  if (client->name_lost_handler != NULL)
    {
      client->name_lost_handler (g_bus_name_owner_get_connection (client->owner),
                                 g_bus_name_owner_get_name (client->owner),
                                 client->user_data);
    }
}

/* must only be called with lock held */
static void
client_connect_signals (Client *client)
{
  client->name_acquired_signal_handler_id = g_signal_connect (client->owner,
                                                              "name-acquired",
                                                              G_CALLBACK (on_name_acquired),
                                                              client);
  client->name_lost_signal_handler_id = g_signal_connect (client->owner,
                                                              "name-lost",
                                                              G_CALLBACK (on_name_lost),
                                                              client);
}
/* must be called without lock held */
static void
client_do_initial_callbacks (Client *client)
{
  gboolean owns_name;

  /* and then report if the name is owned */
  owns_name = g_bus_name_owner_get_owns_name (client->owner);
  if (owns_name)
    {
      g_warn_if_fail (client->previous_call == PREVIOUS_CALL_NONE);
      client->previous_call = PREVIOUS_CALL_ACQUIRED;
      if (client->name_acquired_handler != NULL)
        {
          client->name_acquired_handler (g_bus_name_owner_get_connection (client->owner),
                                         g_bus_name_owner_get_name (client->owner),
                                         client->user_data);
        }
    }
  else
    {
      g_warn_if_fail (client->previous_call == PREVIOUS_CALL_NONE);
      client->previous_call = PREVIOUS_CALL_LOST;
      if (client->name_lost_handler != NULL)
        {
          client->name_lost_handler (g_bus_name_owner_get_connection (client->owner),
                                         g_bus_name_owner_get_name (client->owner),
                                         client->user_data);
        }
    }
}

static void
on_is_initialized_notify_cb (GBusNameOwner *owner,
                             GParamSpec    *pspec,
                             gpointer       user_data)
{
  Client *client = user_data;
  G_LOCK (lock);
  /* disconnect from signal */
  if (client->cancelled)
    goto out;
  g_signal_handler_disconnect (client->owner, client->is_initialized_notify_id);
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
 * to own the name is closed, then @name_lost_handler is invoked since
 * it is no longer possible for other processes to access the
 * process. Similarly, if the connection is opened again, the name
 * will be requested and @name_acquired_handler is invoked if it was
 * possible to acquire the name.
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
  client->name_acquired_handler = name_acquired_handler;
  client->name_lost_handler = name_lost_handler;
  client->user_data = user_data;
  client->owner = g_bus_name_owner_new (bus_type, name, flags);
  if (!g_bus_name_owner_get_is_initialized (client->owner))
    {
      /* wait for :is_initialized to change before reporting anything */
      client->is_initialized_notify_id = g_signal_connect (client->owner,
                                                           "notify::is-initialized",
                                                           G_CALLBACK (on_is_initialized_notify_cb),
                                                           client_ref (client));
    }
  else
    {
      /* is initialized, do callback in idle */
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
  if (owner_id == 0 ||
      map_id_to_client == NULL ||
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
      if (client->previous_call == PREVIOUS_CALL_ACQUIRED)
        {
          if (client->name_lost_handler != NULL)
            client->name_lost_handler (g_bus_name_owner_get_connection (client->owner),
                                       g_bus_name_owner_get_name (client->owner),
                                       client->user_data);
        }
      client_unref (client);
    }
}

#define __G_DBUS_NAME_OWNING_C__
#include "gdbusaliasdef.c"
