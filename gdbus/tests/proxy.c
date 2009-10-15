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
#include <string.h>

#include "tests.h"

/* all tests rely on a shared mainloop */
static GMainLoop *loop = NULL;

/* ---------------------------------------------------------------------------------------------------- */
/* Test that the method aspects of GDBusProxy works */
/* ---------------------------------------------------------------------------------------------------- */

static void
test_methods (GDBusConnection *connection,
              const gchar     *name,
              const gchar     *name_owner,
              GDBusProxy      *proxy)
{
  GVariant *result;
  GError *error;
  const gchar *str;
  gchar *dbus_error_name;

  /* check that we can invoke a method */
  error = NULL;
  result = g_dbus_proxy_invoke_method_sync (proxy,
                                            "HelloWorld",
                                            g_variant_new ("(s)", "Hey"),
                                            -1,
                                            NULL,
                                            &error);
  g_assert_no_error (error);
  g_assert (result != NULL);
  g_assert_cmpstr (g_variant_get_type_string (result), ==, "(s)");
  g_variant_get (result, "(s)", &str);
  g_assert_cmpstr (str, ==, "You greeted me with 'Hey'. Thanks!");
  g_variant_unref (result);

  /* Check that we can completely recover the returned error */
  result = g_dbus_proxy_invoke_method_sync (proxy,
                                            "HelloWorld",
                                            g_variant_new ("(s)", "Yo"),
                                            -1,
                                            NULL,
                                            &error);
  g_assert_error (error, G_DBUS_ERROR, G_DBUS_ERROR_REMOTE_EXCEPTION);
  g_assert (result == NULL);
  dbus_error_name = g_dbus_error_get_dbus_error_name (error);
  g_assert_cmpstr (dbus_error_name, ==, "com.example.TestException");
  g_free (dbus_error_name);
  g_assert (g_dbus_error_strip (error));
  g_assert_cmpstr (error->message, ==, "Yo is not a proper greeting");
  g_clear_error (&error);

  /* Check that we get a timeout if the method handling is taking longer than timeout */
  error = NULL;
  result = g_dbus_proxy_invoke_method_sync (proxy,
                                            "Sleep",
                                            g_variant_new ("(i)", 500 /* msec */),
                                            100 /* msec */,
                                            NULL,
                                            &error);
  g_assert_error (error, G_DBUS_ERROR, G_DBUS_ERROR_NO_REPLY);
  g_assert (result == NULL);
  g_clear_error (&error);
}

/* ---------------------------------------------------------------------------------------------------- */
/* Test that the property aspects of GDBusProxy works */
/* ---------------------------------------------------------------------------------------------------- */

static void
test_properties (GDBusConnection *connection,
                 const gchar     *name,
                 const gchar     *name_owner,
                 GDBusProxy      *proxy)
{
  GError *error;
  GVariant *variant;
  GVariant *variant2;
  GVariant *result;

  error = NULL;

  /**
   * Check that we can read cached properties.
   *
   * No need to test all properties - GVariant has already been tested
   */
  variant = g_dbus_proxy_get_cached_property (proxy, "y", &error);
  g_assert_no_error (error);
  g_assert (variant != NULL);
  g_assert_cmpint (g_variant_get_byte (variant), ==, 1);
  g_variant_unref (variant);
  variant = g_dbus_proxy_get_cached_property (proxy, "o", &error);
  g_assert_no_error (error);
  g_assert (variant != NULL);
  g_assert_cmpstr (g_variant_get_string (variant, NULL), ==, "/some/path");
  g_variant_unref (variant);

  /**
   * Now ask the service to change a property and check that #GDBusProxy::g-dbus-proxy-property-changed
   * is received. Also check that the cache is updated.
   */
  variant2 = g_variant_new_byte (42);
  result = g_dbus_proxy_invoke_method_sync (proxy,
                                            "FrobSetProperty",
                                            g_variant_new ("(sv)",
                                                           "y",
                                                           variant2),
                                            -1,
                                            NULL,
                                            &error);
  g_assert_no_error (error);
  g_assert (result != NULL);
  g_assert_cmpstr (g_variant_get_type_string (result), ==, "()");
  g_variant_unref (result);
  _g_assert_signal_received (proxy, "g-dbus-proxy-properties-changed");
  variant = g_dbus_proxy_get_cached_property (proxy, "y", &error);
  g_assert_no_error (error);
  g_assert (variant != NULL);
  g_assert_cmpint (g_variant_get_byte (variant), ==, 42);
  g_variant_unref (variant);
}

/* ---------------------------------------------------------------------------------------------------- */
/* Test that the signal aspects of GDBusProxy works */
/* ---------------------------------------------------------------------------------------------------- */

static void
test_proxy_signals_on_signal (GDBusProxy  *proxy,
                              const gchar *sender_name,
                              const gchar *signal_name,
                              GVariant    *parameters,
                              gpointer     user_data)
{
  GString *s = user_data;

  g_assert_cmpstr (signal_name, ==, "TestSignal");
  g_assert_cmpstr (g_variant_get_type_string (parameters), ==, "(sov)");

  g_variant_print_string (parameters, s, TRUE);
}

typedef struct
{
  GMainLoop *internal_loop;
  GString *s;
} TestSignalData;

static void
test_proxy_signals_on_emit_signal_cb (GDBusProxy   *proxy,
                                      GAsyncResult *res,
                                      gpointer      user_data)
{
  TestSignalData *data = user_data;
  GError *error;
  GVariant *result;

  error = NULL;
  result = g_dbus_proxy_invoke_method_finish (proxy,
                                              res,
                                              &error);
  g_assert_no_error (error);
  g_assert (result != NULL);
  g_assert_cmpstr (g_variant_get_type_string (result), ==, "()");
  g_variant_unref (result);

  /* check that the signal was recieved before we got the method result */
  g_assert (strlen (data->s->str) > 0);

  /* break out of the loop */
  g_main_loop_quit (data->internal_loop);
}

static void
test_signals (GDBusConnection *connection,
              const gchar     *name,
              const gchar     *name_owner,
              GDBusProxy      *proxy)
{
  GError *error;
  GString *s;
  gulong signal_handler_id;
  TestSignalData data;
  GVariant *result;

  error = NULL;

  /**
   * Ask the service to emit a signal and check that we receive it.
   *
   * Note that blocking calls don't block in the mainloop so wait for the signal (which
   * is dispatched before the method reply)
   */
  s = g_string_new (NULL);
  signal_handler_id = g_signal_connect (proxy,
                                        "g-dbus-proxy-signal",
                                        G_CALLBACK (test_proxy_signals_on_signal),
                                        s);

  result = g_dbus_proxy_invoke_method_sync (proxy,
                                            "EmitSignal",
                                            g_variant_new ("(so)",
                                                           "Accept the next proposition you hear",
                                                           "/some/path"),
                                            -1,
                                            NULL,
                                            &error);
  g_assert_no_error (error);
  g_assert (result != NULL);
  g_assert_cmpstr (g_variant_get_type_string (result), ==, "()");
  g_variant_unref (result);
  /* check that we haven't received the signal just yet */
  g_assert (strlen (s->str) == 0);
  /* and now wait for the signal */
  _g_assert_signal_received (proxy, "g-dbus-proxy-signal");
  g_assert_cmpstr (s->str,
                   ==,
                   "(\"Accept the next proposition you hear .. in bed!\", objectpath \"/some/path/in/bed\", <\"a variant\">)");
  g_signal_handler_disconnect (proxy, signal_handler_id);
  g_string_free (s, TRUE);

  /**
   * Now do this async to check the signal is received before the method returns.
   */
  s = g_string_new (NULL);
  data.internal_loop = g_main_loop_new (NULL, FALSE);
  data.s = s;
  signal_handler_id = g_signal_connect (proxy,
                                        "g-dbus-proxy-signal",
                                        G_CALLBACK (test_proxy_signals_on_signal),
                                        s);
  g_dbus_proxy_invoke_method (proxy,
                              "EmitSignal",
                              g_variant_new ("(so)",
                                             "You will make a great programmer",
                                             "/some/other/path"),
                              -1,
                              NULL,
                              (GAsyncReadyCallback) test_proxy_signals_on_emit_signal_cb,
                              &data);
  g_main_loop_run (data.internal_loop);
  g_main_loop_unref (data.internal_loop);
  g_assert_cmpstr (s->str,
                   ==,
                   "(\"You will make a great programmer .. in bed!\", objectpath \"/some/other/path/in/bed\", <\"a variant\">)");
  g_signal_handler_disconnect (proxy, signal_handler_id);
  g_string_free (s, TRUE);
}

/* ---------------------------------------------------------------------------------------------------- */

static void
on_proxy_appeared (GDBusConnection *connection,
                   const gchar     *name,
                   const gchar     *name_owner,
                   GDBusProxy      *proxy,
                   gpointer         user_data)
{
  test_methods (connection, name, name_owner, proxy);
  test_properties (connection, name, name_owner, proxy);
  test_signals (connection, name, name_owner, proxy);

  g_main_loop_quit (loop);
}

static void
on_proxy_vanished (GDBusConnection *connection,
                   const gchar     *name,
                   gpointer         user_data)
{
}

static void
test_proxy (void)
{
  guint watcher_id;

  session_bus_up ();

  /* TODO: wait a bit for the bus to come up.. ideally session_bus_up() won't return
   * until one can connect to the bus but that's not how things work right now
   */
  usleep (500 * 1000);

  watcher_id = g_bus_watch_proxy (G_BUS_TYPE_SESSION,
                                  "com.example.TestService",
                                  "/com/example/TestObject",
                                  "com.example.Frob",
                                  G_TYPE_DBUS_PROXY,
                                  G_DBUS_PROXY_FLAGS_NONE,
                                  on_proxy_appeared,
                                  on_proxy_vanished,
                                  NULL);

  /* this is safe; testserver will exit once the bus goes away */
  g_assert (g_spawn_command_line_async ("./testserver.py", NULL));

  g_main_loop_run (loop);

  g_bus_unwatch_proxy (watcher_id);

  /* tear down bus */
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

  g_test_add_func ("/gdbus/proxy", test_proxy);
  return g_test_run();
}
