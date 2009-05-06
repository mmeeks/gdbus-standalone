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
#include "gdbusenumtypes.h"
#include "gdbusconnection.h"
#include "gdbusconnection-lowlevel.h"
#include "gdbuserror.h"
#include "gdbusprivate.h"

#include "gdbusalias.h"

/**
 * SECTION:gbusnameowner
 * @short_description: Own a well-known name on a bus
 * @include: gdbus/gdbus.h
 *
 * <para><note>
 * This class is rarely used directly. If you are writing an application, it is often
 * easier to use the g_bus_own_name() or g_bus_watch_name() APIs.
 * </note></para>
 * #GBusNameOwner is a utility class for owning well-known names on a
 * message bus.
 */

struct _GBusNameOwnerPrivate
{
  gchar *name;
  GBusType bus_type;
  GBusNameOwnerFlags flags;

  GDBusConnection *connection;

  gboolean owns_name;

  gboolean is_initialized;

  guint name_lost_subscription_id;
  guint name_acquired_subscription_id;
};

enum
{
  PROP_0,
  PROP_NAME,
  PROP_FLAGS,
  PROP_BUS_TYPE,
  PROP_OWNS_NAME,
  PROP_IS_INITIALIZED,
  PROP_CONNECTION,
};

enum
{
  NAME_LOST_SIGNAL,
  NAME_ACQUIRED_SIGNAL,
  INITIALIZED_SIGNAL,
  LAST_SIGNAL,
};

G_LOCK_DEFINE_STATIC (owner_lock);

static GObject *g_bus_name_owner_constructor (GType                  type,
                                              guint                  n_construct_properties,
                                              GObjectConstructParam *construct_properties);
static void g_bus_name_owner_constructed (GObject *object);

static GBusNameOwner *singletons_get_owner (GDBusConnection *connection, const gchar *name);
static void singletons_add_owner (GBusNameOwner *owner, GDBusConnection *connection, const gchar *name);
static void singletons_remove_owner (GBusNameOwner *owner);

static guint signals[LAST_SIGNAL] = { 0 };

static void on_connection_opened (GDBusConnection *connection,
                                  gpointer         user_data);

static void on_connection_closed (GDBusConnection *connection,
                                  gpointer         user_data);

static void on_connection_initialized (GDBusConnection *connection,
                                       gpointer         user_data);

static guint get_request_name_flags (GBusNameOwnerFlags flags);

G_DEFINE_TYPE (GBusNameOwner, g_bus_name_owner, G_TYPE_OBJECT);

static void
g_bus_name_owner_dispose (GObject *object)
{
  GBusNameOwner *owner = G_BUS_NAME_OWNER (object);

  G_LOCK (owner_lock);
  singletons_remove_owner (owner);
  G_UNLOCK (owner_lock);

  if (G_OBJECT_CLASS (g_bus_name_owner_parent_class)->dispose != NULL)
    G_OBJECT_CLASS (g_bus_name_owner_parent_class)->dispose (object);
}

static void
g_bus_name_owner_finalize (GObject *object)
{
  GBusNameOwner *owner = G_BUS_NAME_OWNER (object);

  if (owner->priv->connection != NULL)
    {
      /* release the name if we own it */
      if (owner->priv->owns_name)
        {
          DBusMessage *message;
          if ((message = dbus_message_new_method_call (DBUS_SERVICE_DBUS,
                                                       DBUS_PATH_DBUS,
                                                       DBUS_INTERFACE_DBUS,
                                                       "ReleaseName")) == NULL)
            _g_dbus_oom ();
          if (!dbus_message_append_args (message,
                                         DBUS_TYPE_STRING, &owner->priv->name,
                                         DBUS_TYPE_INVALID))
            _g_dbus_oom ();
          g_dbus_connection_send_dbus_1_message (owner->priv->connection, message);
          dbus_message_unref (message);
        }

      g_signal_handlers_disconnect_by_func (owner->priv->connection, on_connection_opened, owner);
      g_signal_handlers_disconnect_by_func (owner->priv->connection, on_connection_closed, owner);
      g_signal_handlers_disconnect_by_func (owner->priv->connection, on_connection_initialized, owner);

      if (owner->priv->name_lost_subscription_id > 0)
        g_dbus_connection_dbus_1_signal_unsubscribe (owner->priv->connection,
                                                     owner->priv->name_lost_subscription_id);
      if (owner->priv->name_acquired_subscription_id > 0)
        g_dbus_connection_dbus_1_signal_unsubscribe (owner->priv->connection,
                                                     owner->priv->name_acquired_subscription_id);

      g_object_unref (owner->priv->connection);
    }
  g_free (owner->priv->name);

  if (G_OBJECT_CLASS (g_bus_name_owner_parent_class)->finalize != NULL)
    G_OBJECT_CLASS (g_bus_name_owner_parent_class)->finalize (object);
}

static void
g_bus_name_owner_get_property (GObject    *object,
                               guint       prop_id,
                               GValue     *value,
                               GParamSpec *pspec)
{
  GBusNameOwner *owner = G_BUS_NAME_OWNER (object);

  switch (prop_id)
    {
    case PROP_NAME:
      g_value_set_string (value, g_bus_name_owner_get_name (owner));
      break;

    case PROP_BUS_TYPE:
      g_value_set_enum (value, g_bus_name_owner_get_bus_type (owner));
      break;

    case PROP_FLAGS:
      g_value_set_flags (value, g_bus_name_owner_get_flags (owner));
      break;

    case PROP_OWNS_NAME:
      g_value_set_boolean (value, g_bus_name_owner_get_owns_name (owner));
      break;

    case PROP_IS_INITIALIZED:
      g_value_set_boolean (value, g_bus_name_owner_get_is_initialized (owner));
      break;

    case PROP_CONNECTION:
      g_value_set_object (value, g_bus_name_owner_get_connection (owner));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
g_bus_name_owner_set_property (GObject      *object,
                               guint         prop_id,
                               const GValue *value,
                               GParamSpec   *pspec)
{
  GBusNameOwner *owner = G_BUS_NAME_OWNER (object);

  switch (prop_id)
    {
    case PROP_NAME:
      owner->priv->name = g_value_dup_string (value);
      break;

    case PROP_BUS_TYPE:
      owner->priv->bus_type = g_value_get_enum (value);
      break;

    case PROP_FLAGS:
      owner->priv->flags = g_value_get_flags (value);
      break;

    case PROP_CONNECTION:
      owner->priv->connection = g_value_dup_object (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
on_name_lost_or_acquired (GDBusConnection *connection,
                          DBusMessage     *message,
                          gpointer         user_data)
{
  GBusNameOwner *owner = G_BUS_NAME_OWNER (user_data);
  DBusError dbus_error;
  const gchar *name;
  gboolean old_owns_name;

  dbus_error_init (&dbus_error);

  old_owns_name = owner->priv->owns_name;

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
          if (g_strcmp0 (name, owner->priv->name) == 0)
            {
              owner->priv->owns_name = FALSE;
              if (owner->priv->owns_name != old_owns_name)
                {
                  g_object_notify (G_OBJECT (owner), "owns-name");
                  g_signal_emit (owner, signals[NAME_LOST_SIGNAL], 0);
                }
              else
                {
                  /* This is not unexpected.. it can happen when releasing a name only to claim it right again */
                }
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
          if (g_strcmp0 (name, owner->priv->name) == 0)
            {
              owner->priv->owns_name = TRUE;
              if (owner->priv->owns_name != old_owns_name)
                {
                  g_object_notify (G_OBJECT (owner), "owns-name");
                  g_signal_emit (owner, signals[NAME_ACQUIRED_SIGNAL], 0);
                }
              else
                {
                  /* This is not unexpected.. it can happen when processing the Reply from the RequestName() method */
                }
            }
        }
      else
        {
          g_warning ("Error extracting name for NameAcquired signal: %s: %s", dbus_error.name, dbus_error.message);
          dbus_error_free (&dbus_error);
        }
    }
}

static void
request_name_cb (GDBusConnection *connection,
                 GAsyncResult    *res,
                 gpointer         user_data)
{
  GBusNameOwner *owner = G_BUS_NAME_OWNER (user_data);
  DBusMessage *reply;
  DBusError dbus_error;
  dbus_uint32_t request_name_reply;
  gboolean old_owns_name;
  gboolean old_is_initialized;

  old_owns_name = owner->priv->owns_name;
  old_is_initialized = owner->priv->is_initialized;

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
                             DBUS_TYPE_UINT32, &request_name_reply,
                             DBUS_TYPE_INVALID);
    }
  if (dbus_error_is_set (&dbus_error))
    {
      dbus_error_free (&dbus_error);
    }
  else
    {
      if (request_name_reply == DBUS_REQUEST_NAME_REPLY_PRIMARY_OWNER)
        {
          owner->priv->owns_name = TRUE;
          if (owner->priv->owns_name != old_owns_name)
            {
              g_object_notify (G_OBJECT (owner), "owns-name");
              g_signal_emit (owner, signals[NAME_ACQUIRED_SIGNAL], 0);
            }
        }
    }

 out:
  /* we're now initialized */
  owner->priv->is_initialized = TRUE;
  if (old_is_initialized != owner->priv->is_initialized)
    {
      g_object_notify (G_OBJECT (owner), "is-initialized");
      g_signal_emit (owner, signals[INITIALIZED_SIGNAL], 0);
    }
  if (reply != NULL)
    dbus_message_unref (reply);
  g_object_unref (owner);
}

static void
on_connection_initialized (GDBusConnection *connection,
                           gpointer         user_data)
{
  GBusNameOwner *owner = G_BUS_NAME_OWNER (user_data);

  /* This can only fire a single time */
  g_assert (!owner->priv->is_initialized);

  if (g_dbus_connection_get_is_open (connection))
    {
      /* if the connection is now open then on_connection_opened() will fire
       * and we'll try to request the name from there
       */
    }
  else
    {
      /* connection failed to open so we are now initialized */
      owner->priv->is_initialized = TRUE;
      g_object_notify (G_OBJECT (owner), "is-initialized");
      g_signal_emit (owner, signals[INITIALIZED_SIGNAL], 0);
    }
}

/* note that this is also called from g_bus_name_owner_constructed() if the connection is open */
static void
on_connection_opened (GDBusConnection *connection,
                      gpointer         user_data)
{
  GBusNameOwner *owner = G_BUS_NAME_OWNER (user_data);
  DBusMessage *message;
  dbus_uint32_t flags;

  /* invariants */
  g_assert (g_dbus_connection_get_is_open (owner->priv->connection));
  g_assert (!owner->priv->owns_name);
  g_assert (owner->priv->name_lost_subscription_id == 0);
  g_assert (owner->priv->name_acquired_subscription_id == 0);

  /* listen to NameLost and NameAcquired messages */
  owner->priv->name_lost_subscription_id =
    g_dbus_connection_dbus_1_signal_subscribe (owner->priv->connection,
                                               DBUS_SERVICE_DBUS,
                                               DBUS_INTERFACE_DBUS,
                                               "NameLost",
                                               DBUS_PATH_DBUS,
                                               owner->priv->name,
                                               on_name_lost_or_acquired,
                                               owner,
                                               NULL);
  owner->priv->name_acquired_subscription_id =
    g_dbus_connection_dbus_1_signal_subscribe (owner->priv->connection,
                                               DBUS_SERVICE_DBUS,
                                               DBUS_INTERFACE_DBUS,
                                               "NameAcquired",
                                               DBUS_PATH_DBUS,
                                               owner->priv->name,
                                               on_name_lost_or_acquired,
                                               owner,
                                               NULL);

  flags = get_request_name_flags (owner->priv->flags);
  if ((message = dbus_message_new_method_call (DBUS_SERVICE_DBUS,
                                               DBUS_PATH_DBUS,
                                               DBUS_INTERFACE_DBUS,
                                               "RequestName")) == NULL)
    _g_dbus_oom ();
  if (!dbus_message_append_args (message,
                                 DBUS_TYPE_STRING, &owner->priv->name,
                                 DBUS_TYPE_UINT32, &flags,
                                 DBUS_TYPE_INVALID))
    _g_dbus_oom ();
  g_dbus_connection_send_dbus_1_message_with_reply (owner->priv->connection,
                                                    message,
                                                    -1,
                                                    NULL,
                                                    (GAsyncReadyCallback) request_name_cb,
                                                    g_object_ref (owner));
  dbus_message_unref (message);
}

static void
on_connection_closed (GDBusConnection *connection,
                      gpointer         user_data)
{
  GBusNameOwner *owner = G_BUS_NAME_OWNER (user_data);

  if (owner->priv->name_lost_subscription_id != 0)
    {
      g_dbus_connection_dbus_1_signal_unsubscribe (owner->priv->connection,
                                                   owner->priv->name_lost_subscription_id);
      owner->priv->name_lost_subscription_id = 0;
    }
  if (owner->priv->name_acquired_subscription_id != 0)
    {
      g_dbus_connection_dbus_1_signal_unsubscribe (owner->priv->connection,
                                                   owner->priv->name_acquired_subscription_id);
      owner->priv->name_acquired_subscription_id = 0;
    }

  /* if we currently own the name, well too bad, not anymore, it's up for grabs */
  if (owner->priv->owns_name)
    {
      owner->priv->owns_name = FALSE;
      g_object_notify (G_OBJECT (owner), "owns-name");
      g_signal_emit (owner, signals[NAME_LOST_SIGNAL], 0);
    }

}

static void
g_bus_name_owner_class_init (GBusNameOwnerClass *klass)
{
  GObjectClass *gobject_class;

  g_type_class_add_private (klass, sizeof (GBusNameOwnerPrivate));

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->constructor  = g_bus_name_owner_constructor;
  gobject_class->dispose      = g_bus_name_owner_dispose;
  gobject_class->finalize     = g_bus_name_owner_finalize;
  gobject_class->constructed  = g_bus_name_owner_constructed;
  gobject_class->get_property = g_bus_name_owner_get_property;
  gobject_class->set_property = g_bus_name_owner_set_property;

  /**
   * GBusNameOwner:name:
   *
   * The well-known name to own.
   */
  g_object_class_install_property (gobject_class,
                                   PROP_NAME,
                                   g_param_spec_string ("name",
                                                        _("name"),
                                                        _("The name to own"),
                                                        NULL,
                                                        G_PARAM_READABLE |
                                                        G_PARAM_WRITABLE |
                                                        G_PARAM_CONSTRUCT_ONLY |
                                                        G_PARAM_STATIC_NAME |
                                                        G_PARAM_STATIC_BLURB |
                                                        G_PARAM_STATIC_NICK));

  /**
   * GBusNameOwner:flags:
   *
   * A set of flags from the #GBusNameOwnerFlags flag enumeration
   * detailing behavior on how to own the name.
   */
  g_object_class_install_property (gobject_class,
                                   PROP_FLAGS,
                                   g_param_spec_flags ("flags",
                                                       _("flags"),
                                                       _("Flags detailing how to own the name"),
                                                       G_TYPE_BUS_NAME_OWNER_FLAGS,
                                                       G_BUS_NAME_OWNER_FLAGS_NONE,
                                                       G_PARAM_READABLE |
                                                       G_PARAM_WRITABLE |
                                                       G_PARAM_CONSTRUCT_ONLY |
                                                       G_PARAM_STATIC_NAME |
                                                       G_PARAM_STATIC_BLURB |
                                                       G_PARAM_STATIC_NICK));

  /**
   * GBusNameOwner:owns-name:
   *
   * %TRUE if the name is currently owned.
   */
  g_object_class_install_property (gobject_class,
                                   PROP_OWNS_NAME,
                                   g_param_spec_boolean ("owns-name",
                                                         _("owns-name"),
                                                         _("Whether the name is currently owned"),
                                                         FALSE,
                                                         G_PARAM_READABLE |
                                                         G_PARAM_STATIC_NAME |
                                                         G_PARAM_STATIC_BLURB |
                                                         G_PARAM_STATIC_NICK));

  /**
   * GBusNameOwner:is-initialized:
   *
   * %TRUE if the name owner object is initialized.
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
   * GBusOwner:bus-type:
   *
   * When constructing, set to the type of the message bus the owner
   * object is for. It will be ignored if #GBusNameOnwer:connection is
   * also set upon construction.
   *
   * When reading, the actual type (never #G_BUS_TYPE_STARTER) of the message
   * bus the owner object is for or #G_TYPE_BUS_NONE if the owner object is
   * not for a known message bus type.
   */
  g_object_class_install_property (gobject_class,
                                   PROP_BUS_TYPE,
                                   g_param_spec_enum ("bus-type",
                                                      _("bus-type"),
                                                      _("The type of message bus the owner object is for"),
                                                      G_TYPE_BUS_TYPE,
                                                      G_BUS_TYPE_NONE,
                                                      G_PARAM_READABLE |
                                                      G_PARAM_WRITABLE |
                                                      G_PARAM_CONSTRUCT_ONLY |
                                                      G_PARAM_STATIC_NAME |
                                                      G_PARAM_STATIC_BLURB |
                                                      G_PARAM_STATIC_NICK));

  /**
   * GBusNameOwner:connection:
   *
   * The #GMessageBusConnection that the name will be owned on.
   */
  g_object_class_install_property (gobject_class,
                                   PROP_CONNECTION,
                                   g_param_spec_object ("connection",
                                                        _("connection"),
                                                        _("The connection that the name will be owned on"),
                                                        G_TYPE_DBUS_CONNECTION,
                                                        G_PARAM_READABLE |
                                                        G_PARAM_WRITABLE |
                                                        G_PARAM_CONSTRUCT_ONLY |
                                                        G_PARAM_STATIC_NAME |
                                                        G_PARAM_STATIC_BLURB |
                                                        G_PARAM_STATIC_NICK));

  /**
   * GBusNameOwner::name-lost:
   * @owner: The #GBusNameOwner emitting the signal.
   *
   * Emitted when @owner loses ownership of #GBusNameOwner:name.
   **/
  signals[NAME_LOST_SIGNAL] = g_signal_new ("name-lost",
                                            G_TYPE_BUS_NAME_OWNER,
                                            G_SIGNAL_RUN_LAST,
                                            G_STRUCT_OFFSET (GBusNameOwnerClass, name_lost),
                                            NULL,
                                            NULL,
                                            g_cclosure_marshal_VOID__VOID,
                                            G_TYPE_NONE,
                                            0);

  /**
   * GBusNameOwner::name-acquired:
   * @owner: The #GBusNameOwner emitting the signal.
   *
   * Emitted when @owner acquires ownership of #GBusNameOwner:name.
   **/
  signals[NAME_ACQUIRED_SIGNAL] = g_signal_new ("name-acquired",
                                                G_TYPE_BUS_NAME_OWNER,
                                                G_SIGNAL_RUN_LAST,
                                                G_STRUCT_OFFSET (GBusNameOwnerClass, name_acquired),
                                                NULL,
                                                NULL,
                                                g_cclosure_marshal_VOID__VOID,
                                                G_TYPE_NONE,
                                                0);

  /**
   * GBusNameOwner::initialized:
   * @owner: The #GBusNameOwner emitting the signal.
   *
   * Emitted when @owner is initialized.
   **/
  signals[INITIALIZED_SIGNAL] = g_signal_new ("initialized",
                                              G_TYPE_BUS_NAME_OWNER,
                                              G_SIGNAL_RUN_LAST,
                                              G_STRUCT_OFFSET (GBusNameOwnerClass, initialized),
                                              NULL,
                                              NULL,
                                              g_cclosure_marshal_VOID__VOID,
                                              G_TYPE_NONE,
                                              0);
}

static void
g_bus_name_owner_init (GBusNameOwner *owner)
{
  owner->priv = G_TYPE_INSTANCE_GET_PRIVATE (owner, G_TYPE_BUS_NAME_OWNER, GBusNameOwnerPrivate);
  owner->priv->is_initialized = FALSE;
}

/* ---------------------------------------------------------------------------------------------------- */

static guint
get_request_name_flags (GBusNameOwnerFlags flags)
{
  guint request_name_flags;

  request_name_flags = 0;
  if (flags & G_BUS_NAME_OWNER_FLAGS_ALLOW_REPLACEMENT)
    request_name_flags |= DBUS_NAME_FLAG_ALLOW_REPLACEMENT;
  if (flags & G_BUS_NAME_OWNER_FLAGS_REPLACE)
    request_name_flags |= DBUS_NAME_FLAG_REPLACE_EXISTING;

  return request_name_flags;
}

/* ---------------------------------------------------------------------------------------------------- */
/* routines used for singleton handling */

/* maps from connection to GHashTable (that maps from names to GBusNameOwner) */
static GHashTable *map_connection_to_map_of_owned_names_to_owner = NULL;

/* the following singletons_* functions must be called with owner_lock held */

/* called from _constructor() before creating a new object - result is not reffed */
static GBusNameOwner *
singletons_get_owner (GDBusConnection *connection, const gchar *name)
{
  GBusNameOwner *ret;
  GHashTable *map_of_owned_names_to_owner;

  g_return_val_if_fail (connection != NULL, NULL);
  g_return_val_if_fail (name != NULL, NULL);

  ret = NULL;

  if (map_connection_to_map_of_owned_names_to_owner == NULL)
    goto out;

  map_of_owned_names_to_owner = g_hash_table_lookup (map_connection_to_map_of_owned_names_to_owner,
                                                         connection);
  if (map_of_owned_names_to_owner == NULL)
    goto out;

  ret = g_hash_table_lookup (map_of_owned_names_to_owner,
                             name);
 out:
  return ret;
}

/* called from _constructor() */
static void
singletons_add_owner (GBusNameOwner *owner, GDBusConnection *connection, const gchar *name)
{
  GHashTable *map_of_owned_names_to_owner;

  g_return_if_fail (owner != NULL);

  if (map_connection_to_map_of_owned_names_to_owner == NULL)
    map_connection_to_map_of_owned_names_to_owner = g_hash_table_new (g_direct_hash, g_direct_equal);

  map_of_owned_names_to_owner = g_hash_table_lookup (map_connection_to_map_of_owned_names_to_owner,
                                                     connection);
  if (map_of_owned_names_to_owner == NULL)
    {
      map_of_owned_names_to_owner = g_hash_table_new_full (g_str_hash,
                                                           g_str_equal,
                                                           g_free,
                                                           NULL);
      g_hash_table_insert (map_connection_to_map_of_owned_names_to_owner,
                           connection,
                           map_of_owned_names_to_owner);
    }

  g_hash_table_insert (map_of_owned_names_to_owner,
                       g_strdup (name),
                       owner);
}

/* called from dispose() - ie. can be called multiple times so don't assert anything */
static void
singletons_remove_owner (GBusNameOwner *owner)
{
  GHashTable *map_of_owned_names_to_owner;

  if (map_connection_to_map_of_owned_names_to_owner == NULL)
    goto out;

  map_of_owned_names_to_owner = g_hash_table_lookup (map_connection_to_map_of_owned_names_to_owner,
                                                         owner->priv->connection);
  if (map_of_owned_names_to_owner == NULL)
    goto out;

  if (!g_hash_table_remove (map_of_owned_names_to_owner,
                            owner->priv->name))
    goto out;

  /* clean up as needed */
  if (g_hash_table_size (map_of_owned_names_to_owner) == 0)
    {
      g_hash_table_remove (map_connection_to_map_of_owned_names_to_owner, owner->priv->connection);
      g_hash_table_destroy (map_of_owned_names_to_owner);

      if (g_hash_table_size (map_connection_to_map_of_owned_names_to_owner) == 0)
        {
          g_hash_table_destroy (map_connection_to_map_of_owned_names_to_owner);
          map_connection_to_map_of_owned_names_to_owner = NULL;
        }
    }

 out:
  ;
}

/* ---------------------------------------------------------------------------------------------------- */

static void
g_bus_name_owner_constructed (GObject *object)
{
  GBusNameOwner *owner = G_BUS_NAME_OWNER (object);

  if (owner->priv->connection == NULL)
    {
      g_assert (owner->priv->bus_type != G_BUS_TYPE_NONE);
      owner->priv->connection = g_dbus_connection_bus_get (owner->priv->bus_type);
      /* readjust e.g. G_BUS_TYPE_STARTER */
      owner->priv->bus_type = g_dbus_connection_get_bus_type (owner->priv->connection);
    }

  g_signal_connect (owner->priv->connection, "opened", G_CALLBACK (on_connection_opened), owner);
  g_signal_connect (owner->priv->connection, "closed", G_CALLBACK (on_connection_closed), owner);
  g_signal_connect (owner->priv->connection, "initialized", G_CALLBACK (on_connection_initialized), owner);
  if (g_dbus_connection_get_is_open (owner->priv->connection))
    {
      /* if the connection is already open, then do RequestName right away */
      on_connection_opened (owner->priv->connection, owner);
    }
  else
    {
      if (!g_dbus_connection_get_is_initialized (owner->priv->connection))
        {
          /* if the connection is not yet initialized, we are not until the connection is
           */
        }
      else
        {
          /* connection is not open but it is initialized so we are initialized too (no need to emit
           * signals since no-one has a reference to us)
           */
          owner->priv->is_initialized = FALSE;
        }
    }

  if (G_OBJECT_CLASS (g_bus_name_owner_parent_class)->constructed != NULL)
    G_OBJECT_CLASS (g_bus_name_owner_parent_class)->constructed (object);
}

/* ---------------------------------------------------------------------------------------------------- */

static GObject *
g_bus_name_owner_constructor (GType                  type,
                              guint                  n_construct_properties,
                              GObjectConstructParam *construct_properties)
{
  GObject *object;
  GDBusConnection *connection;
  const gchar *name;
  guint n;

  G_LOCK (owner_lock);

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

  object = G_OBJECT (singletons_get_owner (connection, name));
  if (object != NULL)
    {
      g_object_ref (object);
      goto out;
    }

  object = G_OBJECT_CLASS (g_bus_name_owner_parent_class)->constructor (type,
                                                                        n_construct_properties,
                                                                        construct_properties);

  singletons_add_owner (G_BUS_NAME_OWNER (object), connection, name);

 out:

  g_object_unref (connection);
  G_UNLOCK (owner_lock);
  return object;
}

/* ---------------------------------------------------------------------------------------------------- */

/**
 * g_bus_name_owner_new:
 * @bus_type: The type of bus to own the name on.
 * @name: A well-known name to acquire.
 * @flags: A set of flags from the #GBusNameOwnerFlags enumeration.
 *
 * Get a new #GBusNameOwner object for owning @name on the message bus
 * specified by @bus_type.
 *
 * Since owning a name is an asynchronous operation, the returned
 * object is not guaranteed to return correct information until it has
 * been initialized. Use g_bus_name_owner_get_is_initialized() to check
 * whether the object is initialized and connect to the
 * #GBusNameOwner::initialized signal or listen for notifications on
 * the #GBusNameOwner:is-initialized property to get informed when it
 * is.
 *
 * Note that the returned object may have different
 * #GBusNameOwnerFlags flags than requested with @flags. This is
 * because #GBusNameOwner objects are shared between all callers, e.g.
 * if two separate parts of a process calls this function with the
 * same @bus_type and @name, they will share the same object. This
 * also means that the object may already be initialized when it
 * is returned.
 *
 * #GBusNameOwner deals gracefully with the underlying connection
 * closing and opening by attempting to request the name when the
 * connection is back up.
 *
 * Returns: A #GBusNameOwner object. Free with g_object_unref().
 **/
GBusNameOwner *
g_bus_name_owner_new (GBusType               bus_type,
                      const gchar           *name,
                      GBusNameOwnerFlags     flags)
{
  return G_BUS_NAME_OWNER (g_object_new (G_TYPE_BUS_NAME_OWNER,
                                         "bus-type", bus_type,
                                         "name", name,
                                         "flags", flags,
                                         NULL));
}

/**
 * g_bus_name_owner_new_for_connection:
 * @connection: A #GDBusConnection.
 * @name: A well-known name to acquire.
 * @flags: A set of flags from the #GBusNameOwnerFlags enumeration.
 *
 * Like g_bus_name_owner_new() but allows you to pass in a
 * #GDBusConnection instead of a #GBusType.
 *
 * Returns: A #GBusNameOwner object. Free with g_object_unref().
 **/
GBusNameOwner *
g_bus_name_owner_new_for_connection (GDBusConnection       *connection,
                                     const gchar           *name,
                                     GBusNameOwnerFlags     flags)
{
  return G_BUS_NAME_OWNER (g_object_new (G_TYPE_BUS_NAME_OWNER,
                                         "connection", connection,
                                         "name", name,
                                         "flags", flags,
                                         NULL));
}

/* ---------------------------------------------------------------------------------------------------- */

/**
 * g_bus_name_owner_get_is_initialized:
 * @owner: A #GBusNameOwner.
 *
 * Gets whether @owner is is initialized.
 *
 * You can track changes to this value by connecting to the
 * #GBusNameOwner::initialized signal or by listening to changes on
 * the #GBusNameOwner:is-initialized property.
 *
 * Returns: %TRUE if @owner is initialized, %FALSE otherwise.
 **/
gboolean
g_bus_name_owner_get_is_initialized (GBusNameOwner *owner)
{
  g_return_val_if_fail (G_IS_BUS_NAME_OWNER (owner), FALSE);

  return owner->priv->is_initialized;
}

/**
 * g_bus_name_owner_get_owns_name:
 * @owner: A #GBusNameOwner.
 *
 * Gets whether @owner currently owns the well-known name.
 *
 * You can track changes to this value by listening to the
 * #GBusNameOwner::name-acquired and #GBusNameOwner::name-lost signals
 * or by listning to changes on the #GBusNameOwner:owns-name property.
 *
 * Returns: %TRUE if @owner owns the name it was constructed with,
 * %FALSE otherwise.
 **/
gboolean
g_bus_name_owner_get_owns_name (GBusNameOwner *owner)
{
  g_return_val_if_fail (G_IS_BUS_NAME_OWNER (owner), FALSE);

  return owner->priv->owns_name;
}

/**
 * g_bus_name_owner_get_name:
 * @owner: A #GBusNameOwner.
 *
 * Gets the well-known name that @owner is attempting to own.
 *
 * Returns: The well-known name that @owner is attempting to own. Do
 * not free this string, it is owned by @owner.
 **/
const gchar *
g_bus_name_owner_get_name (GBusNameOwner *owner)
{
  g_return_val_if_fail (G_IS_BUS_NAME_OWNER (owner), NULL);

  return owner->priv->name;
}

/**
 * g_bus_name_owner_get_bus_type:
 * @owner: A #GBusNameOwner.
 *
 * Gets the type (never #G_BUS_TYPE_STARTER) of the message bus the
 * owner object is for or #G_TYPE_BUS_NONE if the owner object is not
 * for a known message bus type.
 *
 * Returns: A #GBusType.
 **/
GBusType
g_bus_name_owner_get_bus_type (GBusNameOwner *owner)
{
  g_return_val_if_fail (G_IS_BUS_NAME_OWNER (owner), 0);

  return owner->priv->bus_type;
}

/**
 * g_bus_name_owner_get_flags:
 * @owner: A #GBusNameOwner.
 *
 * Gets the flags that @owner was constructed with.
 *
 * Returns: Flags from the #GBusNameOwnerFlags enumeration.
 **/
GBusNameOwnerFlags
g_bus_name_owner_get_flags (GBusNameOwner *owner)
{
  g_return_val_if_fail (G_IS_BUS_NAME_OWNER (owner), 0);

  return owner->priv->flags;
}

/**
 * g_bus_name_owner_get_connection:
 * @owner: A #GBusNameOwner.
 *
 * Gets the #GDBusConnection used for @owner.
 *
 * Returns: A #GDBusConnection object owned by @owner. Do not unref.
 **/
GDBusConnection *
g_bus_name_owner_get_connection (GBusNameOwner *owner)
{
  g_return_val_if_fail (G_IS_BUS_NAME_OWNER (owner), NULL);

  return owner->priv->connection;
}

#define __G_BUS_NAME_OWNER_C__
#include "gdbusaliasdef.c"
