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
/* Connection life-cycle testing */
/* ---------------------------------------------------------------------------------------------------- */

static void
test_connection_life_cycle (void)
{
  GDBusConnection *c;
  GDBusConnection *c2;
  GError *error;

  error = NULL;

  /**
   * Check for correct behavior when no bus is present
   *
   */
  c = g_dbus_connection_bus_get_sync (G_BUS_TYPE_SESSION, NULL, &error);
  g_assert_error (error, G_DBUS_ERROR, G_DBUS_ERROR_FILE_NOT_FOUND);
  g_assert (!g_dbus_error_is_remote_error (error));
  g_assert (c == NULL);
  g_error_free (error);
  error = NULL;

  /**
   *  Check for correct behavior when a bus is present
   */
  session_bus_up ();
  /* case 1 */
  c = g_dbus_connection_bus_get_sync (G_BUS_TYPE_SESSION, NULL, &error);
  g_assert_no_error (error);
  g_assert (c != NULL);
  g_assert (!g_dbus_connection_get_is_disconnected (c));

  /**
   * Check that singleton handling work
   */
  c2 = g_dbus_connection_bus_get_sync (G_BUS_TYPE_SESSION, NULL, &error);
  g_assert_no_error (error);
  g_assert (c2 != NULL);
  g_assert (c == c2);
  g_object_unref (c2);

  /**
   * Check that private connections work
   */
  c2 = g_dbus_connection_bus_get_private_sync (G_BUS_TYPE_SESSION, NULL, &error);
  g_assert_no_error (error);
  g_assert (c2 != NULL);
  g_assert (c != c2);
  g_object_unref (c2);

  /**
   *  Check for correct behavior when the bus goes away
   *
   */
  g_dbus_connection_set_exit_on_disconnect (c, FALSE);
  session_bus_down ();
  _g_assert_signal_received (c, "disconnected");
  g_assert (g_dbus_connection_get_is_disconnected (c));
  g_object_unref (c);
}

/* ---------------------------------------------------------------------------------------------------- */
/* Test that sending and receiving messages work as expected */
/* ---------------------------------------------------------------------------------------------------- */

static void
msg_cb_expect_error_disconnected (GDBusConnection *connection,
                                  GAsyncResult    *res,
                                  gpointer         user_data)
{
  GError *error;
  GVariant *result;

  error = NULL;
  result = g_dbus_connection_invoke_method_finish (connection,
                                                   res,
                                                   &error);
  g_assert_error (error, G_DBUS_ERROR, G_DBUS_ERROR_DISCONNECTED);
  g_assert (!g_dbus_error_is_remote_error (error));
  g_error_free (error);
  g_assert (result == NULL);

  g_main_loop_quit (loop);
}

static void
msg_cb_expect_error_unknown_method (GDBusConnection *connection,
                                    GAsyncResult    *res,
                                    gpointer         user_data)
{
  GError *error;
  GVariant *result;

  error = NULL;
  result = g_dbus_connection_invoke_method_finish (connection,
                                                   res,
                                                   &error);
  g_assert_error (error, G_DBUS_ERROR, G_DBUS_ERROR_UNKNOWN_METHOD);
  g_assert (g_dbus_error_is_remote_error (error));
  g_assert (result == NULL);

  g_main_loop_quit (loop);
}

static void
msg_cb_expect_success (GDBusConnection *connection,
                       GAsyncResult    *res,
                       gpointer         user_data)
{
  GError *error;
  GVariant *result;

  error = NULL;
  result = g_dbus_connection_invoke_method_finish (connection,
                                                   res,
                                                   &error);
  g_assert_no_error (error);
  g_assert (result != NULL);
  g_variant_unref (result);

  g_main_loop_quit (loop);
}

static void
msg_cb_expect_error_cancelled (GDBusConnection *connection,
                               GAsyncResult    *res,
                               gpointer         user_data)
{
  GError *error;
  GVariant *result;

  error = NULL;
  result = g_dbus_connection_invoke_method_finish (connection,
                                                   res,
                                                   &error);
  g_assert_error (error, G_DBUS_ERROR, G_DBUS_ERROR_CANCELLED);
  g_assert (!g_dbus_error_is_remote_error (error));
  g_error_free (error);
  g_assert (result == NULL);

  g_main_loop_quit (loop);
}

/* ---------------------------------------------------------------------------------------------------- */

static void
test_connection_send (void)
{
  GDBusConnection *c;
  GCancellable *ca;

  session_bus_up ();

  /* First, get an unopened connection */
  c = g_dbus_connection_bus_get_sync (G_BUS_TYPE_SESSION, NULL, NULL);
  g_assert (c != NULL);
  g_assert (!g_dbus_connection_get_is_disconnected (c));

  /**
   * Check that we never actually send a message if the GCancellable is already cancelled - i.e.
   * we should get #G_DBUS_ERROR_CANCELLED instead of #G_DBUS_ERROR_FAILED even when the actual
   * connection is not up.
   */
  ca = g_cancellable_new ();
  g_cancellable_cancel (ca);
  g_dbus_connection_invoke_method (c,
                                   "org.freedesktop.DBus",  /* bus_name */
                                   "/org/freedesktop/DBus", /* object path */
                                   "org.freedesktop.DBus",  /* interface name */
                                   "GetId",                 /* method name */
                                   NULL,
                                   -1,
                                   ca,
                                   (GAsyncReadyCallback) msg_cb_expect_error_cancelled,
                                   NULL);
  g_main_loop_run (loop);
  g_object_unref (ca);

  /**
   * Check that we get a reply to the GetId() method call.
   */
  g_dbus_connection_invoke_method (c,
                                   "org.freedesktop.DBus",  /* bus_name */
                                   "/org/freedesktop/DBus", /* object path */
                                   "org.freedesktop.DBus",  /* interface name */
                                   "GetId",                 /* method name */
                                   NULL,
                                   -1,
                                   NULL,
                                   (GAsyncReadyCallback) msg_cb_expect_success,
                                   NULL);
  g_main_loop_run (loop);

  /**
   * Check that we get an error reply to the NonExistantMethod() method call.
   */
  g_dbus_connection_invoke_method (c,
                                   "org.freedesktop.DBus",  /* bus_name */
                                   "/org/freedesktop/DBus", /* object path */
                                   "org.freedesktop.DBus",  /* interface name */
                                   "NonExistantMethod",     /* method name */
                                   NULL,
                                   -1,
                                   NULL,
                                   (GAsyncReadyCallback) msg_cb_expect_error_unknown_method,
                                   NULL);
  g_main_loop_run (loop);

  /**
   * Check that cancellation works when the message is already in flight.
   */
  ca = g_cancellable_new ();
  g_dbus_connection_invoke_method (c,
                                   "org.freedesktop.DBus",  /* bus_name */
                                   "/org/freedesktop/DBus", /* object path */
                                   "org.freedesktop.DBus",  /* interface name */
                                   "GetId",                 /* method name */
                                   NULL,
                                   -1,
                                   ca,
                                   (GAsyncReadyCallback) msg_cb_expect_error_cancelled,
                                   NULL);
  g_cancellable_cancel (ca);
  g_main_loop_run (loop);
  g_object_unref (ca);

  /**
   * Check that we get an error when sending to a connection that is disconnected.
   */
  g_dbus_connection_set_exit_on_disconnect (c, FALSE);
  session_bus_down ();
  _g_assert_signal_received (c, "disconnected");
  g_assert (g_dbus_connection_get_is_disconnected (c));

  g_dbus_connection_invoke_method (c,
                                   "org.freedesktop.DBus",  /* bus_name */
                                   "/org/freedesktop/DBus", /* object path */
                                   "org.freedesktop.DBus",  /* interface name */
                                   "GetId",                 /* method name */
                                   NULL,
                                   -1,
                                   NULL,
                                   (GAsyncReadyCallback) msg_cb_expect_error_disconnected,
                                   NULL);
  g_main_loop_run (loop);

  g_object_unref (c);
}

/* ---------------------------------------------------------------------------------------------------- */
/* Connection signal tests */
/* ---------------------------------------------------------------------------------------------------- */

static void
test_connection_signal_handler (GDBusConnection *connection,
                                const gchar      *sender_name,
                                const gchar      *object_path,
                                const gchar      *interface_name,
                                const gchar      *signal_name,
                                GVariant         *parameters,
                                gpointer         user_data)
{
  gint *counter = user_data;
  *counter += 1;
  g_main_loop_quit (loop);
}

static gboolean
test_connection_signal_quit_mainloop (gpointer user_data)
{
  g_main_loop_quit (loop);
  return FALSE;
}

static void
test_connection_signals (void)
{
  GDBusConnection *c1;
  GDBusConnection *c2;
  GDBusConnection *c3;
  guint s1;
  guint s2;
  guint s3;
  gint count_s1;
  gint count_s2;
  gint count_name_owner_changed;
  GError *error;
  gboolean ret;

  error = NULL;

  /**
   * Bring up first separate connections
   */
  session_bus_up ();
  /* if running with dbus-monitor, it claims the name :1.0 - so if we don't run with the monitor
   * emulate this
   */
  if (g_getenv ("G_DBUS_MONITOR") == NULL)
    {
      c1 = g_dbus_connection_bus_get_private_sync (G_BUS_TYPE_SESSION, NULL, NULL);
      g_assert (c1 != NULL);
      g_assert (!g_dbus_connection_get_is_disconnected (c1));
      g_object_unref (c1);
    }
  c1 = g_dbus_connection_bus_get_sync (G_BUS_TYPE_SESSION, NULL, NULL);
  g_assert (c1 != NULL);
  g_assert (!g_dbus_connection_get_is_disconnected (c1));
  g_assert_cmpstr (g_dbus_connection_get_unique_name (c1), ==, ":1.1");

  /**
   * Install two signal handlers for the first connection
   *
   *  - Listen to the signal "Foo" from :1.2 (e.g. c2)
   *  - Listen to the signal "Foo" from anyone (e.g. both c2 and c3)
   *
   * and then count how many times this signal handler was invoked.
   */
  s1 = g_dbus_connection_signal_subscribe (c1,
                                           ":1.2",
                                           "org.gtk.GDBus.ExampleInterface",
                                           "Foo",
                                           "/org/gtk/GDBus/ExampleInterface",
                                           NULL,
                                           test_connection_signal_handler,
                                           &count_s1,
                                           NULL);
  s2 = g_dbus_connection_signal_subscribe (c1,
                                           NULL, /* match any sender */
                                           "org.gtk.GDBus.ExampleInterface",
                                           "Foo",
                                           "/org/gtk/GDBus/ExampleInterface",
                                           NULL,
                                           test_connection_signal_handler,
                                           &count_s2,
                                           NULL);
  s3 = g_dbus_connection_signal_subscribe (c1,
                                           "org.freedesktop.DBus",  /* sender */
                                           "org.freedesktop.DBus",  /* interface */
                                           "NameOwnerChanged",      /* member */
                                           "/org/freedesktop/DBus", /* path */
                                           NULL,
                                           test_connection_signal_handler,
                                           &count_name_owner_changed,
                                           NULL);
  g_assert (s1 != 0);
  g_assert (s2 != 0);
  g_assert (s3 != 0);

  count_s1 = 0;
  count_s2 = 0;
  count_name_owner_changed = 0;

  /**
   * Bring up two other connections
   */
  c2 = g_dbus_connection_bus_get_private_sync (G_BUS_TYPE_SESSION, NULL, NULL);
  g_assert (c2 != NULL);
  g_assert (!g_dbus_connection_get_is_disconnected (c2));
  g_assert_cmpstr (g_dbus_connection_get_unique_name (c2), ==, ":1.2");
  c3 = g_dbus_connection_bus_get_private_sync (G_BUS_TYPE_SESSION, NULL, NULL);
  g_assert (c3 != NULL);
  g_assert (!g_dbus_connection_get_is_disconnected (c3));
  g_assert_cmpstr (g_dbus_connection_get_unique_name (c3), ==, ":1.3");

  /**
   * Make c2 emit "Foo" - we should catch it twice
   */
  ret = g_dbus_connection_emit_signal (c2,
                                       NULL, /* destination bus name */
                                       "/org/gtk/GDBus/ExampleInterface",
                                       "org.gtk.GDBus.ExampleInterface",
                                       "Foo",
                                       NULL,
                                       &error);
  g_assert_no_error (error);
  g_assert (ret);
  while (!(count_s1 == 1 && count_s2 == 1))
    g_main_loop_run (loop);
  g_assert_cmpint (count_s1, ==, 1);
  g_assert_cmpint (count_s2, ==, 1);

  /**
   * Make c3 emit "Foo" - we should catch it only once
   */
  ret = g_dbus_connection_emit_signal (c3,
                                       NULL, /* destination bus name */
                                       "/org/gtk/GDBus/ExampleInterface",
                                       "org.gtk.GDBus.ExampleInterface",
                                       "Foo",
                                       NULL,
                                       &error);
  g_assert_no_error (error);
  g_assert (ret);
  while (!(count_s1 == 1 && count_s2 == 2))
    g_main_loop_run (loop);
  g_assert_cmpint (count_s1, ==, 1);
  g_assert_cmpint (count_s2, ==, 2);

  /**
   * Tool around in the mainloop to avoid race conditions and also to check the
   * total amount of NameOwnerChanged signals
   */
  g_timeout_add (500, test_connection_signal_quit_mainloop, NULL);
  g_main_loop_run (loop);
  g_assert_cmpint (count_s1, ==, 1);
  g_assert_cmpint (count_s2, ==, 2);
  g_assert_cmpint (count_name_owner_changed, ==, 2);

  /**
   * Now bring down the session bus and check we get the :disconnected signal from each connection.
   */
  session_bus_down ();
  g_dbus_connection_set_exit_on_disconnect (c1, FALSE);
  g_dbus_connection_set_exit_on_disconnect (c2, FALSE);
  g_dbus_connection_set_exit_on_disconnect (c3, FALSE);
  if (!g_dbus_connection_get_is_disconnected (c1))
    _g_assert_signal_received (c1, "disconnected");
  if (!g_dbus_connection_get_is_disconnected (c2))
    _g_assert_signal_received (c2, "disconnected");
  if (!g_dbus_connection_get_is_disconnected (c3))
    _g_assert_signal_received (c3, "disconnected");

  g_dbus_connection_signal_unsubscribe (c1, s1);
  g_dbus_connection_signal_unsubscribe (c1, s2);
  g_dbus_connection_signal_unsubscribe (c1, s3);
  g_object_unref (c1);
  g_object_unref (c2);
  g_object_unref (c3);
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

  g_test_add_func ("/gdbus/connection-life-cycle", test_connection_life_cycle);
  g_test_add_func ("/gdbus/connection-send", test_connection_send);
  g_test_add_func ("/gdbus/connection-signals", test_connection_signals);
  return g_test_run();
}
