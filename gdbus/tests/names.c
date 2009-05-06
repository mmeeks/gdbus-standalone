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

/* all tests rely on a shared mainloop */
static GMainLoop *loop = NULL;

/* ---------------------------------------------------------------------------------------------------- */
/* Test that GBusNameOwner works correctly */
/* ---------------------------------------------------------------------------------------------------- */

static void
test_bus_name_owner (void)
{
  GBusNameOwner *o;
  GBusNameOwner *o2;
  GDBusConnection *pc;

  /**
   *  Try to own a name when there is no bus.
   *
   *  The owner should not be initialized until we figure out there is no bus to connect to.
   */
  o = g_bus_name_owner_new (G_BUS_TYPE_SESSION,
                            "org.gtk.GDBus.Name1",
                            G_BUS_NAME_OWNER_FLAGS_NONE);
  g_assert (!g_bus_name_owner_get_is_initialized (o));
  g_assert (!g_bus_name_owner_get_owns_name (o));
  _g_assert_property_notify (o, "is-initialized");
  g_assert ( g_bus_name_owner_get_is_initialized (o));
  g_assert (!g_bus_name_owner_get_owns_name (o));
  g_object_unref (o);

  /**
   *  Try to own a name if we do have a bus instance.
   *
   *  The owner should not be initialized until we managed to acquire the name.
   */
  session_bus_up ();
  o = g_bus_name_owner_new (G_BUS_TYPE_SESSION,
                            "org.gtk.GDBus.Name1",
                            G_BUS_NAME_OWNER_FLAGS_NONE);
  g_assert (!g_bus_name_owner_get_is_initialized (o));
  g_assert (!g_bus_name_owner_get_owns_name (o));
  _g_assert_property_notify (o, "is-initialized");
  g_assert ( g_bus_name_owner_get_is_initialized (o));
  g_assert ( g_bus_name_owner_get_owns_name (o));
  g_dbus_connection_set_exit_on_close (g_bus_name_owner_get_connection (o), FALSE);

  /**
   * try to own the same name again... this should not fail, we should get the same object
   * because of singleton handling..
   */
  o2 = g_bus_name_owner_new (G_BUS_TYPE_SESSION,
                             "org.gtk.GDBus.Name1",
                             G_BUS_NAME_OWNER_FLAGS_NONE);
  g_assert (o == o2);
  g_object_unref (o2);

  /**
   * Now bring the bus down and check that we lose the name.
   */
  session_bus_down ();
  _g_assert_signal_received (o, "name-lost");
  g_assert (!g_bus_name_owner_get_owns_name (o));


  /**
   * Bring the bus back up and check that we own the name - this checks that GBusNameOwner
   * tries to reaquire the name on reconnects.
   */
  session_bus_up ();
  _g_assert_signal_received (o, "name-acquired");
  g_assert (g_bus_name_owner_get_owns_name (o));

  /**
   * Create a private connection and use that to acquire the name.
   *
   * This should fail because o already owns the name and
   * %G_BUS_NAME_OWNER_FLAGS_ALLOW_REPLACEMENT was not specified.
   */
  pc = g_dbus_connection_bus_get_private (G_BUS_TYPE_SESSION);
  g_assert (pc != g_bus_name_owner_get_connection (o));
  g_dbus_connection_set_exit_on_close (pc, FALSE);
  _g_assert_signal_received (pc, "opened");
  o2 = g_bus_name_owner_new_for_connection (pc,
                                            "org.gtk.GDBus.Name1",
                                            G_BUS_NAME_OWNER_FLAGS_ALLOW_REPLACEMENT);
  g_assert (!g_bus_name_owner_get_is_initialized (o2));
  g_assert (!g_bus_name_owner_get_owns_name (o2));
  _g_assert_property_notify (o2, "is-initialized");
  g_assert ( g_bus_name_owner_get_is_initialized (o2));
  g_assert (!g_bus_name_owner_get_owns_name (o2));

  /**
   * Now make o stop owning the name. This should result in o2 acquiring the name.
   */
  g_object_unref (o);
  _g_assert_signal_received (o2, "name-acquired");
  g_assert ( g_bus_name_owner_get_owns_name (o2));

  /**
   * Now try to acquire the name from a non-private connection without
   * specifying %G_BUS_NAME_OWNER_FLAGS_REPLACE.
   *
   * This should fail because o2 already owns the name and we didn't
   * want to replace it.
   */
  o = g_bus_name_owner_new (G_BUS_TYPE_SESSION,
                            "org.gtk.GDBus.Name1",
                            G_BUS_NAME_OWNER_FLAGS_NONE);
  g_assert (!g_bus_name_owner_get_is_initialized (o));
  g_assert (!g_bus_name_owner_get_owns_name (o));
  _g_assert_property_notify (o, "is-initialized");
  g_assert ( g_bus_name_owner_get_is_initialized (o));
  g_assert (!g_bus_name_owner_get_owns_name (o));
  g_object_unref (o);

  /**
   * Now try with specifying %G_BUS_NAME_OWNER_FLAGS_REPLACE.
   *
   * This will make o2 lose the name since we specified replacement
   * and o2 allows replacement.
   */
  o = g_bus_name_owner_new (G_BUS_TYPE_SESSION,
                            "org.gtk.GDBus.Name1",
                            G_BUS_NAME_OWNER_FLAGS_REPLACE);
  g_assert (!g_bus_name_owner_get_is_initialized (o));
  g_assert (!g_bus_name_owner_get_owns_name (o));
  _g_assert_property_notify (o, "is-initialized");
  g_assert ( g_bus_name_owner_get_is_initialized (o));
  g_assert ( g_bus_name_owner_get_owns_name (o));
  /* unfortunately it varies whether o2 gets NameLost before o gets NameAcquired... */
  if (g_bus_name_owner_get_owns_name (o2))
    _g_assert_signal_received (o2, "name-lost");
  g_assert (!g_bus_name_owner_get_owns_name (o2));

  /**
   * Now, give up the name again.. o2 should get it back.
   */
  g_object_unref (o);
  _g_assert_signal_received (o2, "name-acquired");
  g_assert ( g_bus_name_owner_get_owns_name (o2));

  /* Clean up */
  g_object_unref (o2);
  g_object_unref (pc);
  session_bus_down ();
}

/* ---------------------------------------------------------------------------------------------------- */
/* Test that GBusNameWatcher works correctly */
/* ---------------------------------------------------------------------------------------------------- */

static void
test_bus_name_watcher (void)
{
  GBusNameWatcher *w;
  GBusNameOwner *o;

  /**
   *  Try to watch a name when there is no bus.
   *
   *  The watcher should not be initialized until we figure out there is no bus to connect to.
   */
  w = g_bus_name_watcher_new (G_BUS_TYPE_SESSION,
                              "org.gtk.GDBus.Name1");
  g_assert (!g_bus_name_watcher_get_is_initialized (w));
  g_assert (g_bus_name_watcher_get_name_owner (w) == NULL);
  _g_assert_property_notify (w, "is-initialized");
  g_assert ( g_bus_name_watcher_get_is_initialized (w));
  g_assert (g_bus_name_watcher_get_name_owner (w) == NULL);
  g_object_unref (w);

  /**
   *  Try to watch a name if we do have a bus instance.
   *
   *  The watcher should not be initialized until connected.
   */
  session_bus_up ();
  w = g_bus_name_watcher_new (G_BUS_TYPE_SESSION,
                              "org.gtk.GDBus.Name1");
  g_assert (!g_bus_name_watcher_get_is_initialized (w));
  g_assert (g_bus_name_watcher_get_name_owner (w) == NULL);
  _g_assert_property_notify (w, "is-initialized");
  g_assert ( g_bus_name_watcher_get_is_initialized (w));
  g_assert (g_bus_name_watcher_get_name_owner (w) == NULL);
  g_assert (g_dbus_connection_get_is_open (g_bus_name_watcher_get_connection (w)));
  g_object_unref (w);

  /**
   * Now own a name and then create a new watcher.
   */
  /* own the name */
  o = g_bus_name_owner_new (G_BUS_TYPE_SESSION,
                            "org.gtk.GDBus.Name1",
                            G_BUS_NAME_OWNER_FLAGS_NONE);
  g_assert (!g_bus_name_owner_get_is_initialized (o));
  g_assert (!g_bus_name_owner_get_owns_name (o));
  _g_assert_property_notify (o, "is-initialized");
  g_assert ( g_bus_name_owner_get_is_initialized (o));
  g_assert ( g_bus_name_owner_get_owns_name (o));
  g_dbus_connection_set_exit_on_close (g_bus_name_owner_get_connection (o), FALSE);
  /* create watcher */
  w = g_bus_name_watcher_new (G_BUS_TYPE_SESSION,
                              "org.gtk.GDBus.Name1");
  g_assert (!g_bus_name_watcher_get_is_initialized (w));
  g_assert (g_bus_name_watcher_get_name_owner (w) == NULL);
  _g_assert_property_notify (w, "is-initialized");
  g_assert ( g_bus_name_watcher_get_is_initialized (w));
  g_assert (g_bus_name_watcher_get_name_owner (w) != NULL);
  g_object_unref (w);
  g_object_unref (o);

  /**
   * Now create the watcher and then own the name.
   *
   * The watcher should emit #GBusNameWatcher::name-appeared.
   */
  /* create watcher */
  w = g_bus_name_watcher_new (G_BUS_TYPE_SESSION,
                              "org.gtk.GDBus.Name1");
  g_assert (!g_bus_name_watcher_get_is_initialized (w));
  g_assert (g_bus_name_watcher_get_name_owner (w) == NULL);
  _g_assert_property_notify (w, "is-initialized");
  g_assert ( g_bus_name_watcher_get_is_initialized (w));
  g_assert (g_bus_name_watcher_get_name_owner (w) == NULL);
  /* own the name */
  o = g_bus_name_owner_new (G_BUS_TYPE_SESSION,
                            "org.gtk.GDBus.Name1",
                            G_BUS_NAME_OWNER_FLAGS_NONE);
  g_dbus_connection_set_exit_on_close (g_bus_name_owner_get_connection (o), FALSE);
  g_assert (!g_bus_name_owner_get_is_initialized (o));
  g_assert (!g_bus_name_owner_get_owns_name (o));
  /* wait until watcher has noticed */
  _g_assert_signal_received (w, "name-appeared");
  g_assert (g_bus_name_watcher_get_name_owner (w) != NULL);

  /**
   * Now destroy the owner.
   *
   * The watcher should emit #GBusNameWatcher::name-vanished.
   */
  g_object_unref (o);
  _g_assert_signal_received (w, "name-vanished");
  g_assert (g_bus_name_watcher_get_name_owner (w) == NULL);

  /* cleanup */
  g_object_unref (w);
  session_bus_down ();
}

/* ---------------------------------------------------------------------------------------------------- */
/* Test that g_bus_own_name() works correctly */
/* ---------------------------------------------------------------------------------------------------- */

typedef struct
{
  guint num_acquired;
  guint num_lost;
} OwnNameData;

static void
name_acquired_handler (GDBusConnection *connection,
                       const gchar     *name,
                       gpointer         user_data)
{
  OwnNameData *data = user_data;
  g_dbus_connection_set_exit_on_close (connection, FALSE);
  data->num_acquired += 1;
  g_main_loop_quit (loop);
}

static void
name_lost_handler (GDBusConnection *connection,
                   const gchar     *name,
                   gpointer         user_data)
{
  OwnNameData *data = user_data;
  g_dbus_connection_set_exit_on_close (connection, FALSE);
  data->num_lost += 1;
  g_main_loop_quit (loop);
}

static void
test_bus_own_name (void)
{
  guint id;
  OwnNameData data;

  /**
   * First check that name_lost_handler() is invoked if there is no bus.
   *
   * Also make sure name_lost_handler() isn't invoked when unowning the name.
   */
  data.num_acquired = 0;
  data.num_lost = 0;
  id = g_bus_own_name (G_BUS_TYPE_SESSION,
                       "org.gtk.GDBus.Name1",
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
  id = g_bus_own_name (G_BUS_TYPE_SESSION,
                       "org.gtk.GDBus.Name1",
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
   * Stop owning the name - this should trigger name_lost_handler() because of this
   * guarantee
   *
   *   If currently owning the name (e.g. @name_acquired_handler was the
   *   last handler to be invoked), then @name_lost_handler will be invoked
   *   before this function returns.
   *
   * in g_bus_unown_name().
   */
  g_bus_unown_name (id);
  g_assert_cmpint (data.num_acquired, ==, 1);
  g_assert_cmpint (data.num_lost,     ==, 1);

  /**
   * Own the name again. Then nuke the bus and bring it back up, checking
   * that the name_lost_handler() and name_acquired() handlers are invoked
   * as appropriate.
   */
  data.num_acquired = 0;
  data.num_lost = 0;
  id = g_bus_own_name (G_BUS_TYPE_SESSION,
                       "org.gtk.GDBus.Name1",
                       G_BUS_NAME_OWNER_FLAGS_NONE,
                       name_acquired_handler,
                       name_lost_handler,
                       &data);
  g_assert_cmpint (data.num_acquired, ==, 0);
  g_assert_cmpint (data.num_lost,     ==, 0);
  g_main_loop_run (loop);
  g_assert_cmpint (data.num_acquired, ==, 1);
  g_assert_cmpint (data.num_lost,     ==, 0);
  /* nuke the bus */
  session_bus_down ();
  g_main_loop_run (loop);
  g_assert_cmpint (data.num_acquired, ==, 1);
  g_assert_cmpint (data.num_lost,     ==, 1);
  /* bring the bus back up */
  session_bus_up ();
  g_main_loop_run (loop);
  g_assert_cmpint (data.num_acquired, ==, 2);
  g_assert_cmpint (data.num_lost,     ==, 1);
  /* nuke the bus */
  session_bus_down ();
  g_main_loop_run (loop);
  g_assert_cmpint (data.num_acquired, ==, 2);
  g_assert_cmpint (data.num_lost,     ==, 2);

  /* cleanup */
  g_bus_unown_name (id);
  g_assert_cmpint (data.num_acquired, ==, 2);
  g_assert_cmpint (data.num_lost,     ==, 2);
}

/* ---------------------------------------------------------------------------------------------------- */
/* Test that g_bus_watch_name() works correctly */
/* ---------------------------------------------------------------------------------------------------- */

typedef struct
{
  guint num_appeared;
  guint num_vanished;
} WatchNameData;

static void
name_appeared_handler (GDBusConnection *connection,
                       const gchar     *name,
                       const gchar     *name_owner,
                       gpointer         user_data)
{
  WatchNameData *data = user_data;
  g_dbus_connection_set_exit_on_close (connection, FALSE);
  data->num_appeared += 1;
  g_main_loop_quit (loop);
}

static void
name_vanished_handler (GDBusConnection *connection,
                       const gchar     *name,
                       gpointer         user_data)
{
  WatchNameData *data = user_data;
  g_dbus_connection_set_exit_on_close (connection, FALSE);
  data->num_vanished += 1;
  g_main_loop_quit (loop);
}

static void
test_bus_watch_name (void)
{
  WatchNameData data;
  GBusNameOwner *o;
  guint id;

  /**
   * First check that name_vanished_handler() is invoked if there is no bus.
   *
   * Also make sure name_vanished_handler() isn't invoked when unwatching the name.
   */
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
  g_bus_unwatch_name (id);
  g_assert_cmpint (data.num_appeared, ==, 0);
  g_assert_cmpint (data.num_vanished, ==, 1);

  /**
   * Now bring up a bus, own a name, and then start watching it.
   */
  session_bus_up ();
  /* own the name */
  o = g_bus_name_owner_new (G_BUS_TYPE_SESSION,
                            "org.gtk.GDBus.Name1",
                            G_BUS_NAME_OWNER_FLAGS_NONE);
  g_dbus_connection_set_exit_on_close (g_bus_name_owner_get_connection (o), FALSE);
  g_assert (!g_bus_name_owner_get_owns_name (o));
  _g_assert_signal_received (o, "name-acquired");
  g_assert ( g_bus_name_owner_get_owns_name (o));
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
  g_object_unref (o);

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
  o = g_bus_name_owner_new (G_BUS_TYPE_SESSION,
                            "org.gtk.GDBus.Name1",
                            G_BUS_NAME_OWNER_FLAGS_NONE);
  g_dbus_connection_set_exit_on_close (g_bus_name_owner_get_connection (o), FALSE);
  g_assert (!g_bus_name_owner_get_owns_name (o));
  g_main_loop_run (loop);
  g_assert_cmpint (data.num_appeared, ==, 1);
  g_assert_cmpint (data.num_vanished, ==, 1);
  /* there's a race here, owner might not know that it has owned the name yet.. so.. */
  if (!g_bus_name_owner_get_owns_name (o))
    _g_assert_signal_received (o, "name-acquired");
  g_assert ( g_bus_name_owner_get_owns_name (o));

  /**
   * Nuke the bus and check that the name vanishes. Then bring the bus back up
   * and check that the name appears.
   */
  session_bus_down ();
  g_main_loop_run (loop);
  g_assert_cmpint (data.num_appeared, ==, 1);
  g_assert_cmpint (data.num_vanished, ==, 2);
  /* again, work around possible races */
  if (g_bus_name_owner_get_owns_name (o))
    _g_assert_signal_received (o, "name-lost");
  g_assert (!g_bus_name_owner_get_owns_name (o));
  session_bus_up ();
  g_main_loop_run (loop);
  g_assert_cmpint (data.num_appeared, ==, 2);
  g_assert_cmpint (data.num_vanished, ==, 2);
  /* again, work around possible races */
  if (!g_bus_name_owner_get_owns_name (o))
    _g_assert_signal_received (o, "name-acquired");
  g_assert ( g_bus_name_owner_get_owns_name (o));
  g_bus_unwatch_name (id);
  g_assert_cmpint (data.num_appeared, ==, 2);
  g_assert_cmpint (data.num_vanished, ==, 3);
  g_object_unref (o);

  session_bus_down ();

}

/* ---------------------------------------------------------------------------------------------------- */

int
main (int   argc,
      char *argv[])
{
  g_type_init ();
  g_test_init (&argc, &argv, NULL);

  /* all the tests rely on a shared main loop */
  loop = g_main_loop_new (NULL, FALSE);

  /* all the tests use a session bus with a well-known address that we can bring up and down
   * using session_bus_up() and session_bus_down().
   */
  g_unsetenv ("DISPLAY");
  g_setenv ("DBUS_SESSION_BUS_ADDRESS", session_bus_get_temporary_address (), TRUE);

  g_test_add_func ("/gdbus/bus-name-owner", test_bus_name_owner);
  g_test_add_func ("/gdbus/bus-name-watcher", test_bus_name_watcher);
  g_test_add_func ("/gdbus/bus-own-name", test_bus_own_name);
  g_test_add_func ("/gdbus/bus-watch-name", test_bus_watch_name);
  return g_test_run();
}
