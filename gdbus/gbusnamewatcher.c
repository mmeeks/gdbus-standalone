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
#include "gdbusenumtypes.h"
#include "gdbusconnection.h"
#include "gdbusconnection-lowlevel.h"
#include "gdbuserror.h"
#include "gdbusprivate.h"

#include "gdbusalias.h"

/**
 * SECTION:gbusnamewatcher
 * @short_description: Watch names on a bus
 * @include: gdbus/gdbus.h
 *
 * <para><note>
 * This class is rarely used directly. If you are writing an application, it is often
 * easier to use the g_bus_own_name() or g_bus_watch_name() APIs.
 * </note></para>
 * #GBusNameWatcher is a utility class for watching names on a message
 * bus.
 */

struct _GBusNameWatcherPrivate
{
  gchar *name;
  GBusType bus_type;

  GDBusConnection *connection;

  gchar *name_owner;

  gboolean is_initialized;

  guint name_owner_changed_subscription_id;
};

enum
{
  PROP_0,
  PROP_NAME,
  PROP_FLAGS,
  PROP_BUS_TYPE,
  PROP_NAME_OWNER,
  PROP_IS_INITIALIZED,
  PROP_CONNECTION,
};

enum
{
  NAME_APPEARED_SIGNAL,
  NAME_VANISHED_SIGNAL,
  INITIALIZED_SIGNAL,
  LAST_SIGNAL,
};

G_LOCK_DEFINE_STATIC (watcher_lock);

static GObject *g_bus_name_watcher_constructor (GType                  type,
                                                guint                  n_construct_properties,
                                                GObjectConstructParam *construct_properties);
static void g_bus_name_watcher_constructed (GObject *object);

static GBusNameWatcher *singletons_get_watcher (GDBusConnection *connection, const gchar *name);
static void singletons_add_watcher (GBusNameWatcher *watcher, GDBusConnection *connection, const gchar *name);
static void singletons_remove_watcher (GBusNameWatcher *watcher);

static guint signals[LAST_SIGNAL] = { 0 };

static void on_connection_opened (GDBusConnection *connection,
                                  gpointer         user_data);

static void on_connection_closed (GDBusConnection *connection,
                                  gpointer         user_data);

static void on_connection_initialized (GDBusConnection *connection,
                                       gpointer         user_data);

G_DEFINE_TYPE (GBusNameWatcher, g_bus_name_watcher, G_TYPE_OBJECT);

static void
g_bus_name_watcher_dispose (GObject *object)
{
  GBusNameWatcher *watcher = G_BUS_NAME_WATCHER (object);

  G_LOCK (watcher_lock);
  singletons_remove_watcher (watcher);
  G_UNLOCK (watcher_lock);

  if (G_OBJECT_CLASS (g_bus_name_watcher_parent_class)->dispose != NULL)
    G_OBJECT_CLASS (g_bus_name_watcher_parent_class)->dispose (object);
}

static void
g_bus_name_watcher_finalize (GObject *object)
{
  GBusNameWatcher *watcher = G_BUS_NAME_WATCHER (object);

  if (watcher->priv->connection != NULL)
    {
      g_signal_handlers_disconnect_by_func (watcher->priv->connection, on_connection_opened, watcher);
      g_signal_handlers_disconnect_by_func (watcher->priv->connection, on_connection_closed, watcher);
      g_signal_handlers_disconnect_by_func (watcher->priv->connection, on_connection_initialized, watcher);

      if (watcher->priv->name_owner_changed_subscription_id > 0)
        g_dbus_connection_dbus_1_signal_unsubscribe (watcher->priv->connection,
                                                     watcher->priv->name_owner_changed_subscription_id);

      g_object_unref (watcher->priv->connection);
    }
  g_free (watcher->priv->name);
  g_free (watcher->priv->name_owner);

  if (G_OBJECT_CLASS (g_bus_name_watcher_parent_class)->finalize != NULL)
    G_OBJECT_CLASS (g_bus_name_watcher_parent_class)->finalize (object);
}

static void
g_bus_name_watcher_get_property (GObject    *object,
                               guint       prop_id,
                               GValue     *value,
                               GParamSpec *pspec)
{
  GBusNameWatcher *watcher = G_BUS_NAME_WATCHER (object);

  switch (prop_id)
    {
    case PROP_NAME:
      g_value_set_string (value, g_bus_name_watcher_get_name (watcher));
      break;

    case PROP_BUS_TYPE:
      g_value_set_enum (value, g_bus_name_watcher_get_bus_type (watcher));
      break;

    case PROP_NAME_OWNER:
      g_value_set_string (value, g_bus_name_watcher_get_name_owner (watcher));
      break;

    case PROP_IS_INITIALIZED:
      g_value_set_boolean (value, g_bus_name_watcher_get_is_initialized (watcher));
      break;

    case PROP_CONNECTION:
      g_value_set_object (value, g_bus_name_watcher_get_connection (watcher));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
g_bus_name_watcher_set_property (GObject      *object,
                               guint         prop_id,
                               const GValue *value,
                               GParamSpec   *pspec)
{
  GBusNameWatcher *watcher = G_BUS_NAME_WATCHER (object);

  switch (prop_id)
    {
    case PROP_NAME:
      watcher->priv->name = g_value_dup_string (value);
      break;

    case PROP_BUS_TYPE:
      watcher->priv->bus_type = g_value_get_enum (value);
      break;

    case PROP_CONNECTION:
      watcher->priv->connection = g_value_dup_object (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

/* ---------------------------------------------------------------------------------------------------- */

static void
on_name_owner_changed (GDBusConnection *connection,
                       DBusMessage     *message,
                       gpointer         user_data)
{
  GBusNameWatcher *watcher = G_BUS_NAME_WATCHER (user_data);
  const gchar *name;
  const gchar *old_owner;
  const gchar *new_owner;
  DBusError dbus_error;

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
  if (g_strcmp0 (name, watcher->priv->name) != 0)
    goto out;

  if ((old_owner != NULL && strlen (old_owner) > 0) && watcher->priv->name_owner != NULL)
    {
      g_free (watcher->priv->name_owner);
      watcher->priv->name_owner = NULL;
      g_object_notify (G_OBJECT (watcher), "name-owner");
      g_signal_emit (watcher, signals[NAME_VANISHED_SIGNAL], 0);
    }

  if (new_owner != NULL && strlen (new_owner) > 0)
    {
      g_warn_if_fail (watcher->priv->name_owner == NULL);
      g_free (watcher->priv->name_owner);
      watcher->priv->name_owner = g_strdup (new_owner);
      g_object_notify (G_OBJECT (watcher), "name-owner");
      g_signal_emit (watcher, signals[NAME_APPEARED_SIGNAL], 0);
    }

 out:
  ;
}

static void
get_name_owner_cb (GDBusConnection *connection,
                   GAsyncResult    *res,
                   gpointer         user_data)
{
  GBusNameWatcher *watcher = G_BUS_NAME_WATCHER (user_data);
  DBusMessage *reply;
  DBusError dbus_error;
  const gchar *name_owner;
  gchar *old_name_owner;
  gboolean old_is_initialized;

  old_name_owner = g_strdup (watcher->priv->name_owner);
  old_is_initialized = watcher->priv->is_initialized;

  reply = g_dbus_connection_send_dbus_1_message_with_reply_finish (connection,
                                                                   res,
                                                                   NULL);
  if (reply == NULL)
    goto out;

  dbus_error_init (&dbus_error);
  if (!dbus_set_error_from_message (&dbus_error, reply))
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
  else
    {
      g_free (watcher->priv->name_owner);
      watcher->priv->name_owner = g_strdup (name_owner);
      if (g_strcmp0 (watcher->priv->name_owner, old_name_owner) != 0)
        {
          g_object_notify (G_OBJECT (watcher), "name-owner");
          g_signal_emit (watcher, signals[NAME_APPEARED_SIGNAL], 0);
        }
    }

 out:
  /* we're now initialized */
  watcher->priv->is_initialized = TRUE;
  if (old_is_initialized != watcher->priv->is_initialized)
    {
      g_object_notify (G_OBJECT (watcher), "is-initialized");
      g_signal_emit (watcher, signals[INITIALIZED_SIGNAL], 0);
    }
  if (reply != NULL)
    dbus_message_unref (reply);
  g_object_unref (watcher);
  g_free (old_name_owner);
}

static void
on_connection_initialized (GDBusConnection *connection,
                           gpointer         user_data)
{
  GBusNameWatcher *watcher = G_BUS_NAME_WATCHER (user_data);

  /* This can only fire once */
  g_assert (!watcher->priv->is_initialized);

  if (g_dbus_connection_get_is_open (connection))
    {
      /* if the connection is now open then on_connection_opened() will fire
       * and we'll try to get the name owner from there
       */
    }
  else
    {
      /* connection failed to open so we are now initialized */
      watcher->priv->is_initialized = TRUE;
      g_object_notify (G_OBJECT (watcher), "is-initialized");
      g_signal_emit (watcher, signals[INITIALIZED_SIGNAL], 0);
    }
}

/* note that this is also called from g_bus_name_watcher_constructed() if the connection is open */
static void
on_connection_opened (GDBusConnection *connection,
                      gpointer         user_data)
{
  GBusNameWatcher *watcher = G_BUS_NAME_WATCHER (user_data);
  DBusMessage *message;

  /* invariants */
  g_assert (g_dbus_connection_get_is_open (watcher->priv->connection));
  g_assert (watcher->priv->name_owner == NULL);
  g_assert (watcher->priv->name_owner_changed_subscription_id == 0);

  watcher->priv->name_owner_changed_subscription_id =
    g_dbus_connection_dbus_1_signal_subscribe (watcher->priv->connection,
                                               DBUS_SERVICE_DBUS,
                                               DBUS_INTERFACE_DBUS,
                                               "NameOwnerChanged",
                                               DBUS_PATH_DBUS,
                                               watcher->priv->name,
                                               on_name_owner_changed,
                                               watcher,
                                               NULL);

  if ((message = dbus_message_new_method_call (DBUS_SERVICE_DBUS,
                                               DBUS_PATH_DBUS,
                                               DBUS_INTERFACE_DBUS,
                                               "GetNameOwner")) == NULL)
    _g_dbus_oom ();
  if (!dbus_message_append_args (message,
                                 DBUS_TYPE_STRING, &watcher->priv->name,
                                 DBUS_TYPE_INVALID))
    _g_dbus_oom ();
  g_dbus_connection_send_dbus_1_message_with_reply (watcher->priv->connection,
                                                    message,
                                                    -1,
                                                    NULL,
                                                    (GAsyncReadyCallback) get_name_owner_cb,
                                                    g_object_ref (watcher));
  dbus_message_unref (message);
}

static void
on_connection_closed (GDBusConnection *connection,
                      gpointer         user_data)
{
  GBusNameWatcher *watcher = G_BUS_NAME_WATCHER (user_data);

  if (watcher->priv->name_owner_changed_subscription_id != 0)
    {
      g_dbus_connection_dbus_1_signal_unsubscribe (watcher->priv->connection,
                                                   watcher->priv->name_owner_changed_subscription_id);
      watcher->priv->name_owner_changed_subscription_id = 0;
    }

  /* if we currently have a name owner then nuke it */
  if (watcher->priv->name_owner != NULL)
    {
      g_free (watcher->priv->name_owner);
      watcher->priv->name_owner = NULL;
      g_object_notify (G_OBJECT (watcher), "name-owner");
      g_signal_emit (watcher, signals[NAME_VANISHED_SIGNAL], 0);
    }
}

static void
g_bus_name_watcher_class_init (GBusNameWatcherClass *klass)
{
  GObjectClass *gobject_class;

  g_type_class_add_private (klass, sizeof (GBusNameWatcherPrivate));

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->constructor  = g_bus_name_watcher_constructor;
  gobject_class->dispose      = g_bus_name_watcher_dispose;
  gobject_class->finalize     = g_bus_name_watcher_finalize;
  gobject_class->constructed  = g_bus_name_watcher_constructed;
  gobject_class->get_property = g_bus_name_watcher_get_property;
  gobject_class->set_property = g_bus_name_watcher_set_property;

  /**
   * GBusNameWatcher:name:
   *
   * The well-known or unique name to watch.
   */
  g_object_class_install_property (gobject_class,
                                   PROP_NAME,
                                   g_param_spec_string ("name",
                                                        _("name"),
                                                        _("The name to watch"),
                                                        NULL,
                                                        G_PARAM_READABLE |
                                                        G_PARAM_WRITABLE |
                                                        G_PARAM_CONSTRUCT_ONLY |
                                                        G_PARAM_STATIC_NAME |
                                                        G_PARAM_STATIC_BLURB |
                                                        G_PARAM_STATIC_NICK));

  /**
   * GBusNameWatcher:name-owner:
   *
   * The unique name of the owner of the name being watched or %NULL
   * if there is no name owner.
   */
  g_object_class_install_property (gobject_class,
                                   PROP_NAME_OWNER,
                                   g_param_spec_boolean ("name-owner",
                                                         _("name-owner"),
                                                         _("The unique name of the owner of the name being watched"),
                                                         FALSE,
                                                         G_PARAM_READABLE |
                                                         G_PARAM_STATIC_NAME |
                                                         G_PARAM_STATIC_BLURB |
                                                         G_PARAM_STATIC_NICK));

  /**
   * GBusNameWatcher:is-initialized:
   *
   * %TRUE if the name watcher object is initialized.
   */
  g_object_class_install_property (gobject_class,
                                   PROP_IS_INITIALIZED,
                                   g_param_spec_boolean ("is-initialized",
                                                         _("is-initialized"),
                                                         _("Whether the object is initialized"),
                                                         FALSE,
                                                         G_PARAM_READABLE |
                                                         G_PARAM_STATIC_NAME |
                                                         G_PARAM_STATIC_BLURB |
                                                         G_PARAM_STATIC_NICK));

  /**
   * GBusWatcher:bus-type:
   *
   * When constructing, set to the type of the message bus the watcher
   * object is for. It will be ignored if #GBusNameOnwer:connection is
   * also set upon construction.
   *
   * When reading, the actual type (never #G_BUS_TYPE_STARTER) of the message
   * bus the watcher object is for or #G_TYPE_BUS_NONE if the watcher object is
   * not for a known message bus type.
   */
  g_object_class_install_property (gobject_class,
                                   PROP_BUS_TYPE,
                                   g_param_spec_enum ("bus-type",
                                                      _("bus-type"),
                                                      _("The type of message bus the watcher object is for"),
                                                      G_TYPE_BUS_TYPE,
                                                      G_BUS_TYPE_NONE,
                                                      G_PARAM_READABLE |
                                                      G_PARAM_WRITABLE |
                                                      G_PARAM_CONSTRUCT_ONLY |
                                                      G_PARAM_STATIC_NAME |
                                                      G_PARAM_STATIC_BLURB |
                                                      G_PARAM_STATIC_NICK));

  /**
   * GBusNameWatcher:connection:
   *
   * The #GMessageBusConnection used for watching the name.
   */
  g_object_class_install_property (gobject_class,
                                   PROP_CONNECTION,
                                   g_param_spec_object ("connection",
                                                        _("connection"),
                                                        _("The connection the name will be watched on"),
                                                        G_TYPE_DBUS_CONNECTION,
                                                        G_PARAM_READABLE |
                                                        G_PARAM_WRITABLE |
                                                        G_PARAM_CONSTRUCT_ONLY |
                                                        G_PARAM_STATIC_NAME |
                                                        G_PARAM_STATIC_BLURB |
                                                        G_PARAM_STATIC_NICK));

  /**
   * GBusNameWatcher::name-vanished:
   * @watcher: The #GBusNameWatcher emitting the signal.
   *
   * Emitted when the name being watched vanishes.
   **/
  signals[NAME_VANISHED_SIGNAL] = g_signal_new ("name-vanished",
                                                G_TYPE_BUS_NAME_WATCHER,
                                                G_SIGNAL_RUN_LAST,
                                                G_STRUCT_OFFSET (GBusNameWatcherClass, name_vanished),
                                                NULL,
                                                NULL,
                                                g_cclosure_marshal_VOID__VOID,
                                                G_TYPE_NONE,
                                                0);

  /**
   * GBusNameWatcher::name-appeared:
   * @watcher: The #GBusNameWatcher emitting the signal.
   *
   * Emitted when the name being watched appears.
   **/
  signals[NAME_APPEARED_SIGNAL] = g_signal_new ("name-appeared",
                                                G_TYPE_BUS_NAME_WATCHER,
                                                G_SIGNAL_RUN_LAST,
                                                G_STRUCT_OFFSET (GBusNameWatcherClass, name_appeared),
                                                NULL,
                                                NULL,
                                                g_cclosure_marshal_VOID__VOID,
                                                G_TYPE_NONE,
                                                0);

  /**
   * GBusNameWatcher::initialized:
   * @watcher: The #GBusNameWatcher emitting the signal.
   *
   * Emitted when @watcher is initialized.
   **/
  signals[INITIALIZED_SIGNAL] = g_signal_new ("initialized",
                                              G_TYPE_BUS_NAME_WATCHER,
                                              G_SIGNAL_RUN_LAST,
                                              G_STRUCT_OFFSET (GBusNameWatcherClass, initialized),
                                              NULL,
                                              NULL,
                                              g_cclosure_marshal_VOID__VOID,
                                              G_TYPE_NONE,
                                              0);
}

static void
g_bus_name_watcher_init (GBusNameWatcher *watcher)
{
  watcher->priv = G_TYPE_INSTANCE_GET_PRIVATE (watcher, G_TYPE_BUS_NAME_WATCHER, GBusNameWatcherPrivate);
  watcher->priv->is_initialized = FALSE;
}

/* ---------------------------------------------------------------------------------------------------- */
/* routines used for singleton handling */

/* maps from connection to GHashTable (that maps from names to GBusNameWatcher) */
static GHashTable *map_connection_to_map_of_watched_names_to_watcher = NULL;

/* the following singletons_* functions must be called with watcher_lock held */

/* called from _constructor() before creating a new object - result is not reffed */
static GBusNameWatcher *
singletons_get_watcher (GDBusConnection *connection, const gchar *name)
{
  GBusNameWatcher *ret;
  GHashTable *map_of_watched_names_to_watcher;

  g_return_val_if_fail (connection != NULL, NULL);
  g_return_val_if_fail (name != NULL, NULL);

  ret = NULL;

  if (map_connection_to_map_of_watched_names_to_watcher == NULL)
    goto out;

  map_of_watched_names_to_watcher = g_hash_table_lookup (map_connection_to_map_of_watched_names_to_watcher,
                                                         connection);
  if (map_of_watched_names_to_watcher == NULL)
    goto out;

  ret = g_hash_table_lookup (map_of_watched_names_to_watcher,
                             name);
 out:
  return ret;
}

/* called from _constructor() */
static void
singletons_add_watcher (GBusNameWatcher *watcher, GDBusConnection *connection, const gchar *name)
{
  GHashTable *map_of_watched_names_to_watcher;

  g_return_if_fail (watcher != NULL);

  if (map_connection_to_map_of_watched_names_to_watcher == NULL)
    map_connection_to_map_of_watched_names_to_watcher = g_hash_table_new (g_direct_hash, g_direct_equal);

  map_of_watched_names_to_watcher = g_hash_table_lookup (map_connection_to_map_of_watched_names_to_watcher,
                                                     connection);
  if (map_of_watched_names_to_watcher == NULL)
    {
      map_of_watched_names_to_watcher = g_hash_table_new_full (g_str_hash,
                                                           g_str_equal,
                                                           g_free,
                                                           NULL);
      g_hash_table_insert (map_connection_to_map_of_watched_names_to_watcher,
                           connection,
                           map_of_watched_names_to_watcher);
    }

  g_hash_table_insert (map_of_watched_names_to_watcher,
                       g_strdup (name),
                       watcher);
}

/* called from dispose() - ie. can be called multiple times so don't assert anything */
static void
singletons_remove_watcher (GBusNameWatcher *watcher)
{
  GHashTable *map_of_watched_names_to_watcher;

  if (map_connection_to_map_of_watched_names_to_watcher == NULL)
    goto out;

  map_of_watched_names_to_watcher = g_hash_table_lookup (map_connection_to_map_of_watched_names_to_watcher,
                                                         watcher->priv->connection);
  if (map_of_watched_names_to_watcher == NULL)
    goto out;

  if (!g_hash_table_remove (map_of_watched_names_to_watcher,
                            watcher->priv->name))
    goto out;

  /* clean up as needed */
  if (g_hash_table_size (map_of_watched_names_to_watcher) == 0)
    {
      g_hash_table_remove (map_connection_to_map_of_watched_names_to_watcher, watcher->priv->connection);
      g_hash_table_destroy (map_of_watched_names_to_watcher);

      if (g_hash_table_size (map_connection_to_map_of_watched_names_to_watcher) == 0)
        {
          g_hash_table_destroy (map_connection_to_map_of_watched_names_to_watcher);
          map_connection_to_map_of_watched_names_to_watcher = NULL;
        }
    }

 out:
  ;
}

/* ---------------------------------------------------------------------------------------------------- */

static void
g_bus_name_watcher_constructed (GObject *object)
{
  GBusNameWatcher *watcher = G_BUS_NAME_WATCHER (object);

  if (watcher->priv->connection == NULL)
    {
      g_assert (watcher->priv->bus_type != G_BUS_TYPE_NONE);
      watcher->priv->connection = g_dbus_connection_bus_get (watcher->priv->bus_type);
      /* readjust e.g. G_BUS_TYPE_STARTER */
      watcher->priv->bus_type = g_dbus_connection_get_bus_type (watcher->priv->connection);
    }

  g_signal_connect (watcher->priv->connection, "opened", G_CALLBACK (on_connection_opened), watcher);
  g_signal_connect (watcher->priv->connection, "closed", G_CALLBACK (on_connection_closed), watcher);
  g_signal_connect (watcher->priv->connection, "initialized", G_CALLBACK (on_connection_initialized), watcher);
  if (g_dbus_connection_get_is_open (watcher->priv->connection))
    {
      /* if the connection is already open, then do RequestName right away */
      on_connection_opened (watcher->priv->connection, watcher);
    }
  else
    {
      if (!g_dbus_connection_get_is_initialized (watcher->priv->connection))
        {
          /* if the connection is not initialized, we are not initialized until we know if it could
           * be opened
           */
        }
      else
        {
          /* connection is not open but it is initialized so we are now initialized (no need to emit
           * signals since no-one has a reference to us)
           */
          watcher->priv->is_initialized = TRUE;
        }
    }

  if (G_OBJECT_CLASS (g_bus_name_watcher_parent_class)->constructed != NULL)
    G_OBJECT_CLASS (g_bus_name_watcher_parent_class)->constructed (object);
}

/* ---------------------------------------------------------------------------------------------------- */

static GObject *
g_bus_name_watcher_constructor (GType                  type,
                              guint                  n_construct_properties,
                              GObjectConstructParam *construct_properties)
{
  GObject *object;
  GDBusConnection *connection;
  const gchar *name;
  guint n;

  G_LOCK (watcher_lock);

  connection = NULL;
  name = NULL;
  object = NULL;

  for (n = 0; n < n_construct_properties; n++)
    {
      if (g_strcmp0 (construct_properties[n].pspec->name, "bus-type") == 0)
        {
          GBusType bus_type;
          bus_type = g_value_get_enum (construct_properties[n].value);
          if (bus_type != G_BUS_TYPE_NONE)
            connection = g_dbus_connection_bus_get (bus_type);
        }
      else if (g_strcmp0 (construct_properties[n].pspec->name, "connection") == 0)
        {
          GDBusConnection *given_connection;
          given_connection = g_value_get_object (construct_properties[n].value);
          if (given_connection != NULL)
            {
              if (connection != NULL)
                g_object_unref (connection);
              connection = g_object_ref (given_connection);
            }
        }
      else if (g_strcmp0 (construct_properties[n].pspec->name, "name") == 0)
        {
          name = g_value_get_string (construct_properties[n].value);
        }
    }

  g_assert (connection != NULL);
  g_assert (name != NULL);

  object = G_OBJECT (singletons_get_watcher (connection, name));
  if (object != NULL)
    {
      g_object_ref (object);
      goto out;
    }

  object = G_OBJECT_CLASS (g_bus_name_watcher_parent_class)->constructor (type,
                                                                        n_construct_properties,
                                                                        construct_properties);

  singletons_add_watcher (G_BUS_NAME_WATCHER (object), connection, name);

 out:

  g_object_unref (connection);
  G_UNLOCK (watcher_lock);
  return object;
}

/* ---------------------------------------------------------------------------------------------------- */

/**
 * g_bus_name_watcher_new:
 * @bus_type: The type of bus to watch the name on
 * @name: The name to watch.
 *
 * Get a new #GBusNameWatcher object for watching @name on the message
 * bus specified by @bus_type.
 *
 * Since watching a name is an asynchronous operation, the returned
 * object is not guaranteed to return correct information until it has
 * been initialized.
 * Use g_bus_name_watcher_get_is_initialized() to check whether the object
 * is initialized and connect to the #GBusNameWatcher::initialized signal or
 * listen for notifications on the #GBusNameWatcher:is-initialized property
 * to get informed when it is.
 *
 * Note that the returned object may shared with other callers, e.g.
 * if two separate parts of a process calls this function with the
 * same @bus_type and @name, they will share the same object. As such,
 * the object may already have been initialized when it is returned.
 *
 * #GBusNameOwner deals gracefully with the underlying connection
 * closing and opening by checking the owner of the name when the
 * connection is back up.
 *
 * Returns: A #GBusNameWatcher object. Free with g_object_unref().
 **/
GBusNameWatcher *
g_bus_name_watcher_new (GBusType               bus_type,
                        const gchar           *name)
{
  return G_BUS_NAME_WATCHER (g_object_new (G_TYPE_BUS_NAME_WATCHER,
                                           "bus-type", bus_type,
                                           "name", name,
                                           NULL));
}

/**
 * g_bus_name_watcher_new_for_connection:
 * @connection: A #GDBusConnection.
 * @name: The name to watch.
 *
 * Like g_bus_name_watcher_new() but allows you to pass in a
 * #GDBusConnection instead of a #GBusType.
 *
 * Returns: A #GBusNameWatcher object. Free with g_object_unref().
 **/
GBusNameWatcher *
g_bus_name_watcher_new_for_connection (GDBusConnection       *connection,
                                       const gchar           *name)
{
  return G_BUS_NAME_WATCHER (g_object_new (G_TYPE_BUS_NAME_WATCHER,
                                           "connection", connection,
                                           "name", name,
                                           NULL));
}

/* ---------------------------------------------------------------------------------------------------- */

/**
 * g_bus_name_watcher_get_is_initialized:
 * @watcher: A #GBusNameWatcher.
 *
 * Gets whether @watcher is initialized.
 *
 * You can track changes to this value by connecting to the
 * #GBusNameWatcher::initialized signal or by listening to changes on
 * the #GBusNameWatcher:is-initialized property.
 *
 * Returns: %TRUE if @watcher is initialized, %FALSE otherwise.
 **/
gboolean
g_bus_name_watcher_get_is_initialized (GBusNameWatcher *watcher)
{
  g_return_val_if_fail (G_IS_BUS_NAME_WATCHER (watcher), FALSE);

  return watcher->priv->is_initialized;
}

/**
 * g_bus_name_watcher_get_name_owner:
 * @watcher: A #GBusNameWatcher.
 *
 * Gets the unique name of the owner of the name being watched.
 *
 * You can track changes to this value by listening to the
 * #GBusNameWatcher::name-appeared and #GBusNameWatcher::name-vanished signals
 * or by listning to changes on the #GBusNameWatcher:name-owner property.
 *
 * Returns: The unique name of the owner of the name being watched or
 * %NULL if no-one owns the name.
 **/
const gchar *
g_bus_name_watcher_get_name_owner (GBusNameWatcher *watcher)
{
  g_return_val_if_fail (G_IS_BUS_NAME_WATCHER (watcher), FALSE);

  return watcher->priv->name_owner;
}

/**
 * g_bus_name_watcher_get_name:
 * @watcher: A #GBusNameWatcher.
 *
 * Gets the name being watched.
 *
 * Returns: The name being watched. Do not free this string,
 * it is owned by @watcher.
 **/
const gchar *
g_bus_name_watcher_get_name (GBusNameWatcher *watcher)
{
  g_return_val_if_fail (G_IS_BUS_NAME_WATCHER (watcher), NULL);

  return watcher->priv->name;
}

/**
 * g_bus_name_watcher_get_bus_type:
 * @watcher: A #GBusNameWatcher.
 *
 * Gets the type (never #G_BUS_TYPE_STARTER) of the message bus the
 * watcher object is for or #G_TYPE_BUS_NONE if the watcher object is not
 * for a known message bus type.
 *
 * Returns: A #GBusType.
 **/
GBusType
g_bus_name_watcher_get_bus_type (GBusNameWatcher *watcher)
{
  g_return_val_if_fail (G_IS_BUS_NAME_WATCHER (watcher), 0);

  return watcher->priv->bus_type;
}

/**
 * g_bus_name_watcher_get_connection:
 * @watcher: A #GBusNameWatcher.
 *
 * Gets the #GDBusConnection used for @watcher.
 *
 * Returns: A #GDBusConnection object owned by @watcher. Do not unref.
 **/
GDBusConnection *
g_bus_name_watcher_get_connection (GBusNameWatcher *watcher)
{
  g_return_val_if_fail (G_IS_BUS_NAME_WATCHER (watcher), NULL);

  return watcher->priv->connection;
}

#define __G_BUS_NAME_WATCHER_C__
#include "gdbusaliasdef.c"
