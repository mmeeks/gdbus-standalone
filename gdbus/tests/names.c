/* GLib testing framework examples and tests
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

#include <gdbus/gdbus.h>
#include <unistd.h>

#include "tests.h"
#define G_DBUS_I_UNDERSTAND_THAT_ABI_AND_API_IS_UNSTABLE
#include <gdbus/gdbus-lowlevel.h>

/* all tests rely on a shared mainloop */
static GMainLoop *loop;

/* ---------------------------------------------------------------------------------------------------- */
/* Test that g_bus_own_name() works correctly */
/* ---------------------------------------------------------------------------------------------------- */

typedef struct
{
  GMainLoop *loop;
  gboolean expect_null_connection;
  guint num_acquired;
  guint num_lost;
} OwnNameData;

static void
name_acquired_handler (GDBusConnection *connection,
                       const gchar     *name,
                       gpointer         user_data)
{
  OwnNameData *data = user_data;
  g_dbus_connection_set_exit_on_disconnect (connection, FALSE);
  data->num_acquired += 1;
  g_main_loop_quit (loop);
}

static void
name_lost_handler (GDBusConnection *connection,
                   const gchar     *name,
                   gpointer         user_data)
{
  OwnNameData *data = user_data;
  if (data->expect_null_connection)
    {
      g_assert (connection == NULL);
    }
  else
    {
      g_assert (connection != NULL);
      g_dbus_connection_set_exit_on_disconnect (connection, FALSE);
    }
  data->num_lost += 1;
  g_main_loop_quit (loop);
}

static void
test_bus_own_name (void)
{
  guint id;
  guint id2;
  OwnNameData data;
  OwnNameData data2;
  const gchar *name;
  GDBusConnection *c;
  DBusMessage *m;
  DBusMessage *r;
  DBusError dbus_error;
  GError *error;
  dbus_bool_t name_has_owner_reply;
  GDBusConnection *c2;

  error = NULL;
  name = "org.gtk.GDBus.Name1";

  /**
   * First check that name_lost_handler() is invoked if there is no bus.
   *
   * Also make sure name_lost_handler() isn't invoked when unowning the name.
   */
  data.num_acquired = 0;
  data.num_lost = 0;
  data.expect_null_connection = TRUE;
  id = g_bus_own_name (G_BUS_TYPE_SESSION,
                       name,
                       G_BUS_NAME_OWNER_FLAGS_NONE,
                       name_acquired_handler,
                       name_lost_handler,
                       &data);
  g_assert_cmpint (data.num_acquired, ==, 0);
  g_assert_cmpint (data.num_lost,     ==, 0);
  g_main_loop_run (loop);
  g_assert_cmpint (data.num_acquired, ==, 0);
  g_assert_cmpint (data.num_lost,     ==, 1);
  g_bus_unown_name (id);
  g_assert_cmpint (data.num_acquired, ==, 0);
  g_assert_cmpint (data.num_lost,     ==, 1);

  /**
   * Bring up a bus, then own a name and check name_acquired_handler() is invoked.
   */
  session_bus_up ();
  data.num_acquired = 0;
  data.num_lost = 0;
  data.expect_null_connection = FALSE;
  id = g_bus_own_name (G_BUS_TYPE_SESSION,
                       name,
                       G_BUS_NAME_OWNER_FLAGS_NONE,
                       name_acquired_handler,
                       name_lost_handler,
                       &data);
  g_assert_cmpint (data.num_acquired, ==, 0);
  g_assert_cmpint (data.num_lost,     ==, 0);
  g_main_loop_run (loop);
  g_assert_cmpint (data.num_acquired, ==, 1);
  g_assert_cmpint (data.num_lost,     ==, 0);

  /**
   * Check that the name was actually acquired.
   */
  c = g_dbus_connection_bus_get_sync (G_BUS_TYPE_SESSION, NULL, NULL);
  g_assert (c != NULL);
  g_assert (!g_dbus_connection_get_is_disconnected (c));
  m = dbus_message_new_method_call (DBUS_SERVICE_DBUS,
                                    DBUS_PATH_DBUS,
                                    DBUS_INTERFACE_DBUS,
                                    "NameHasOwner");
  dbus_message_append_args (m,
                            DBUS_TYPE_STRING, &name,
                            DBUS_TYPE_INVALID);
  r = g_dbus_connection_send_dbus_1_message_with_reply_sync (c,
                                                             m,
                                                             -1,
                                                             NULL,
                                                             &error);
  g_assert_no_error (error);
  g_assert (r != NULL);
  dbus_error_init (&dbus_error);
  dbus_message_get_args (r,
                         &dbus_error,
                         DBUS_TYPE_BOOLEAN, &name_has_owner_reply,
                         DBUS_TYPE_INVALID);
  g_assert (!dbus_error_is_set (&dbus_error));
  g_assert (name_has_owner_reply);
  dbus_message_unref (m);
  dbus_message_unref (r);

  /**
   * Stop owning the name - this should trigger name_lost_handler() because of this
   * guarantee
   *
   *   If currently owning the name (e.g. @name_acquired_handler was the
   *   last handler to be invoked), then @name_lost_handler will be invoked
   *   before this function returns.
   *
   * in g_bus_unown_name().
   */
  data.expect_null_connection = FALSE;
  g_bus_unown_name (id);
  g_assert_cmpint (data.num_acquired, ==, 1);
  g_assert_cmpint (data.num_lost,     ==, 1);

  /**
   * Check that the name was actually relased.
   */
  m = dbus_message_new_method_call (DBUS_SERVICE_DBUS,
                                    DBUS_PATH_DBUS,
                                    DBUS_INTERFACE_DBUS,
                                    "NameHasOwner");
  dbus_message_append_args (m,
                            DBUS_TYPE_STRING, &name,
                            DBUS_TYPE_INVALID);
  r = g_dbus_connection_send_dbus_1_message_with_reply_sync (c,
                                                             m,
                                                             -1,
                                                             NULL,
                                                             &error);
  g_assert_no_error (error);
  g_assert (r != NULL);
  dbus_error_init (&dbus_error);
  dbus_message_get_args (r,
                         &dbus_error,
                         DBUS_TYPE_BOOLEAN, &name_has_owner_reply,
                         DBUS_TYPE_INVALID);
  g_assert (!dbus_error_is_set (&dbus_error));
  g_assert (!name_has_owner_reply);
  dbus_message_unref (m);
  dbus_message_unref (r);

  /**
   * Own the name again.
   */
  data.num_acquired = 0;
  data.num_lost = 0;
  data.expect_null_connection = FALSE;
  id = g_bus_own_name (G_BUS_TYPE_SESSION,
                       name,
                       G_BUS_NAME_OWNER_FLAGS_NONE,
                       name_acquired_handler,
                       name_lost_handler,
                       &data);
  g_assert_cmpint (data.num_acquired, ==, 0);
  g_assert_cmpint (data.num_lost,     ==, 0);
  g_main_loop_run (loop);
  g_assert_cmpint (data.num_acquired, ==, 1);
  g_assert_cmpint (data.num_lost,     ==, 0);

  /**
   * Try owning the name with another object on the same connection  - this should
   * fail because we already own the name.
   */
  data2.num_acquired = 0;
  data2.num_lost = 0;
  data2.expect_null_connection = FALSE;
  id2 = g_bus_own_name (G_BUS_TYPE_SESSION,
                        name,
                        G_BUS_NAME_OWNER_FLAGS_NONE,
                        name_acquired_handler,
                        name_lost_handler,
                        &data2);
  g_assert_cmpint (data2.num_acquired, ==, 0);
  g_assert_cmpint (data2.num_lost,     ==, 0);
  g_main_loop_run (loop);
  g_assert_cmpint (data2.num_acquired, ==, 0);
  g_assert_cmpint (data2.num_lost,     ==, 1);
  g_bus_unown_name (id2);
  g_assert_cmpint (data2.num_acquired, ==, 0);
  g_assert_cmpint (data2.num_lost,     ==, 1);

  /**
   * Create a secondary (e.g. private) connection and try owning the name on that
   * connection. This should fail both with and without _REPLACE because we
   * didn't specify ALLOW_REPLACEMENT.
   */
  c2 = g_dbus_connection_bus_get_private_sync (G_BUS_TYPE_SESSION, NULL, NULL);
  g_assert (c2 != NULL);
  g_assert (!g_dbus_connection_get_is_disconnected (c2));
  /* first without _REPLACE */
  data2.num_acquired = 0;
  data2.num_lost = 0;
  data2.expect_null_connection = FALSE;
  id2 = g_bus_own_name_on_connection (c2,
                                      name,
                                      G_BUS_NAME_OWNER_FLAGS_NONE,
                                      name_acquired_handler,
                                      name_lost_handler,
                                      &data2);
  g_assert_cmpint (data2.num_acquired, ==, 0);
  g_assert_cmpint (data2.num_lost,     ==, 0);
  g_main_loop_run (loop);
  g_assert_cmpint (data2.num_acquired, ==, 0);
  g_assert_cmpint (data2.num_lost,     ==, 1);
  g_bus_unown_name (id2);
  g_assert_cmpint (data2.num_acquired, ==, 0);
  g_assert_cmpint (data2.num_lost,     ==, 1);
  /* then with _REPLACE */
  data2.num_acquired = 0;
  data2.num_lost = 0;
  data2.expect_null_connection = FALSE;
  id2 = g_bus_own_name_on_connection (c2,
                                      name,
                                      G_BUS_NAME_OWNER_FLAGS_REPLACE,
                                      name_acquired_handler,
                                      name_lost_handler,
                                      &data2);
  g_assert_cmpint (data2.num_acquired, ==, 0);
  g_assert_cmpint (data2.num_lost,     ==, 0);
  g_main_loop_run (loop);
  g_assert_cmpint (data2.num_acquired, ==, 0);
  g_assert_cmpint (data2.num_lost,     ==, 1);
  g_bus_unown_name (id2);
  g_assert_cmpint (data2.num_acquired, ==, 0);
  g_assert_cmpint (data2.num_lost,     ==, 1);

  /**
   * Stop owning the name and grab it again with _ALLOW_REPLACEMENT.
   */
  data.expect_null_connection = FALSE;
  g_bus_unown_name (id);
  g_assert_cmpint (data.num_acquired, ==, 1);
  g_assert_cmpint (data.num_lost,     ==, 1);
  /* grab it again */
  data.num_acquired = 0;
  data.num_lost = 0;
  data.expect_null_connection = FALSE;
  id = g_bus_own_name (G_BUS_TYPE_SESSION,
                       name,
                       G_BUS_NAME_OWNER_FLAGS_ALLOW_REPLACEMENT,
                       name_acquired_handler,
                       name_lost_handler,
                       &data);
  g_assert_cmpint (data.num_acquired, ==, 0);
  g_assert_cmpint (data.num_lost,     ==, 0);
  g_main_loop_run (loop);
  g_assert_cmpint (data.num_acquired, ==, 1);
  g_assert_cmpint (data.num_lost,     ==, 0);

  /**
   * Now try to grab the name from the secondary connection.
   *
   */
  /* first without _REPLACE - this won't make us acquire the name */
  data2.num_acquired = 0;
  data2.num_lost = 0;
  data2.expect_null_connection = FALSE;
  id2 = g_bus_own_name_on_connection (c2,
                                      name,
                                      G_BUS_NAME_OWNER_FLAGS_NONE,
                                      name_acquired_handler,
                                      name_lost_handler,
                                      &data2);
  g_assert_cmpint (data2.num_acquired, ==, 0);
  g_assert_cmpint (data2.num_lost,     ==, 0);
  g_main_loop_run (loop);
  g_assert_cmpint (data2.num_acquired, ==, 0);
  g_assert_cmpint (data2.num_lost,     ==, 1);
  g_bus_unown_name (id2);
  g_assert_cmpint (data2.num_acquired, ==, 0);
  g_assert_cmpint (data2.num_lost,     ==, 1);
  /* then with _REPLACE - here we should acquire the name - e.g. owner should lose it
   * and owner2 should acquire it  */
  data2.num_acquired = 0;
  data2.num_lost = 0;
  data2.expect_null_connection = FALSE;
  id2 = g_bus_own_name_on_connection (c2,
                                      name,
                                      G_BUS_NAME_OWNER_FLAGS_REPLACE,
                                      name_acquired_handler,
                                      name_lost_handler,
                                      &data2);
  g_assert_cmpint (data.num_acquired, ==, 1);
  g_assert_cmpint (data.num_lost,     ==, 0);
  g_assert_cmpint (data2.num_acquired, ==, 0);
  g_assert_cmpint (data2.num_lost,     ==, 0);
  /* wait for handlers for both owner and owner2 to fire */
  g_main_loop_run (loop);
  g_main_loop_run (loop);
  g_assert_cmpint (data.num_acquired, ==, 1);
  g_assert_cmpint (data.num_lost,     ==, 1);
  g_assert_cmpint (data2.num_acquired, ==, 1);
  g_assert_cmpint (data2.num_lost,     ==, 0);
  /* ok, make owner2 release the name - then wait for owner to automagically reacquire it */
  g_bus_unown_name (id2);
  g_assert_cmpint (data2.num_acquired, ==, 1);
  g_assert_cmpint (data2.num_lost,     ==, 1);
  g_main_loop_run (loop);
  g_assert_cmpint (data.num_acquired, ==, 2);
  g_assert_cmpint (data.num_lost,     ==, 1);

  /**
   * Finally, nuke the bus and check name_lost_handler() is invoked.
   *
   */
  data.expect_null_connection = TRUE;
  session_bus_down ();
  g_main_loop_run (loop);
  g_assert_cmpint (data.num_acquired, ==, 2);
  g_assert_cmpint (data.num_lost,     ==, 2);
  g_bus_unown_name (id);

  g_object_unref (c);
  g_object_unref (c2);
}

/* ---------------------------------------------------------------------------------------------------- */
/* Test that g_bus_watch_name() works correctly */
/* ---------------------------------------------------------------------------------------------------- */

typedef struct
{
  gboolean expect_null_connection;
  guint num_acquired;
  guint num_lost;
  guint num_appeared;
  guint num_vanished;
} WatchNameData;

static void
w_name_acquired_handler (GDBusConnection *connection,
                         const gchar     *name,
                         gpointer         user_data)
{
  WatchNameData *data = user_data;
  data->num_acquired += 1;
  g_main_loop_quit (loop);
}

static void
w_name_lost_handler (GDBusConnection *connection,
                     const gchar     *name,
                     gpointer         user_data)
{
  WatchNameData *data = user_data;
  data->num_lost += 1;
  g_main_loop_quit (loop);
}

static void
name_appeared_handler (GDBusConnection *connection,
                       const gchar     *name,
                       const gchar     *name_owner,
                       gpointer         user_data)
{
  WatchNameData *data = user_data;
  if (data->expect_null_connection)
    {
      g_assert (connection == NULL);
    }
  else
    {
      g_assert (connection != NULL);
      g_dbus_connection_set_exit_on_disconnect (connection, FALSE);
    }
  data->num_appeared += 1;
  g_main_loop_quit (loop);
}

static void
name_vanished_handler (GDBusConnection *connection,
                       const gchar     *name,
                       gpointer         user_data)
{
  WatchNameData *data = user_data;
  if (data->expect_null_connection)
    {
      g_assert (connection == NULL);
    }
  else
    {
      g_assert (connection != NULL);
      g_dbus_connection_set_exit_on_disconnect (connection, FALSE);
    }
  data->num_vanished += 1;
  g_main_loop_quit (loop);
}

static void
test_bus_watch_name (void)
{
  WatchNameData data;
  guint id;
  guint owner_id;

  /**
   * First check that name_vanished_handler() is invoked if there is no bus.
   *
   * Also make sure name_vanished_handler() isn't invoked when unwatching the name.
   */
  data.num_appeared = 0;
  data.num_vanished = 0;
  data.expect_null_connection = TRUE;
  id = g_bus_watch_name (G_BUS_TYPE_SESSION,
                         "org.gtk.GDBus.Name1",
                         name_appeared_handler,
                         name_vanished_handler,
                         &data);
  g_assert_cmpint (data.num_appeared, ==, 0);
  g_assert_cmpint (data.num_vanished, ==, 0);
  g_main_loop_run (loop);
  g_assert_cmpint (data.num_appeared, ==, 0);
  g_assert_cmpint (data.num_vanished, ==, 1);
  g_bus_unwatch_name (id);
  g_assert_cmpint (data.num_appeared, ==, 0);
  g_assert_cmpint (data.num_vanished, ==, 1);

  /**
   * Now bring up a bus, own a name, and then start watching it.
   */
  session_bus_up ();
  /* own the name */
  data.num_acquired = 0;
  data.num_lost = 0;
  data.expect_null_connection = FALSE;
  owner_id = g_bus_own_name (G_BUS_TYPE_SESSION,
                             "org.gtk.GDBus.Name1",
                             G_BUS_NAME_OWNER_FLAGS_NONE,
                             w_name_acquired_handler,
                             w_name_lost_handler,
                             &data);
  g_main_loop_run (loop);
  g_assert_cmpint (data.num_acquired, ==, 1);
  g_assert_cmpint (data.num_lost,     ==, 0);
  /* now watch the name */
  data.num_appeared = 0;
  data.num_vanished = 0;
  id = g_bus_watch_name (G_BUS_TYPE_SESSION,
                         "org.gtk.GDBus.Name1",
                         name_appeared_handler,
                         name_vanished_handler,
                         &data);
  g_assert_cmpint (data.num_appeared, ==, 0);
  g_assert_cmpint (data.num_vanished, ==, 0);
  g_main_loop_run (loop);
  g_assert_cmpint (data.num_appeared, ==, 1);
  g_assert_cmpint (data.num_vanished, ==, 0);

  /**
   * Unwatch the name - this should trigger name_vanished_handler() because of this
   * guarantee
   *
   *   If the name being watched currently has an owner the name (e.g. @name_appeared_handler
   *   was the last handler to be invoked), then @name_vanished_handler will be invoked
   *   before this function returns.
   *
   * in g_bus_unwatch_name().
   */
  g_bus_unwatch_name (id);
  g_assert_cmpint (data.num_appeared, ==, 1);
  g_assert_cmpint (data.num_vanished, ==, 1);

  /* unown the name */
  g_bus_unown_name (owner_id);
  g_assert_cmpint (data.num_acquired, ==, 1);
  g_assert_cmpint (data.num_lost,     ==, 1);

  /**
   * Create a watcher and then make a name be owned.
   *
   * This should trigger name_appeared_handler() ...
   */
  /* watch the name */
  data.num_appeared = 0;
  data.num_vanished = 0;
  id = g_bus_watch_name (G_BUS_TYPE_SESSION,
                         "org.gtk.GDBus.Name1",
                         name_appeared_handler,
                         name_vanished_handler,
                         &data);
  g_assert_cmpint (data.num_appeared, ==, 0);
  g_assert_cmpint (data.num_vanished, ==, 0);
  g_main_loop_run (loop);
  g_assert_cmpint (data.num_appeared, ==, 0);
  g_assert_cmpint (data.num_vanished, ==, 1);

  /* own the name */
  data.num_acquired = 0;
  data.num_lost = 0;
  data.expect_null_connection = FALSE;
  owner_id = g_bus_own_name (G_BUS_TYPE_SESSION,
                             "org.gtk.GDBus.Name1",
                             G_BUS_NAME_OWNER_FLAGS_NONE,
                             w_name_acquired_handler,
                             w_name_lost_handler,
                             &data);
  g_main_loop_run (loop);
  g_main_loop_run (loop);
  g_assert_cmpint (data.num_acquired, ==, 1);
  g_assert_cmpint (data.num_lost,     ==, 0);
  g_assert_cmpint (data.num_appeared, ==, 1);
  g_assert_cmpint (data.num_vanished, ==, 1);

  /**
   * Nuke the bus and check that the name vanishes and is lost.
   */
  data.expect_null_connection = TRUE;
  session_bus_down ();
  g_main_loop_run (loop);
  g_assert_cmpint (data.num_lost,     ==, 1);
  g_assert_cmpint (data.num_vanished, ==, 2);

  g_bus_unwatch_name (id);
  g_bus_unown_name (owner_id);

}

/* ---------------------------------------------------------------------------------------------------- */

int
main (int   argc,
      char *argv[])
{
  gint ret;

  g_type_init ();
  g_test_init (&argc, &argv, NULL);

  loop = g_main_loop_new (NULL, FALSE);

  /* all the tests use a session bus with a well-known address that we can bring up and down
   * using session_bus_up() and session_bus_down().
   */
  g_unsetenv ("DISPLAY");
  g_setenv ("DBUS_SESSION_BUS_ADDRESS", session_bus_get_temporary_address (), TRUE);

  g_test_add_func ("/gdbus/bus-own-name", test_bus_own_name);
  g_test_add_func ("/gdbus/bus-watch-name", test_bus_watch_name);

  ret = g_test_run();

  g_main_loop_unref (loop);

  return ret;
}
