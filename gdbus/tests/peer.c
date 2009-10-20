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

static gchar *test_address = NULL;
static GMainLoop *loop = NULL;

/* ---------------------------------------------------------------------------------------------------- */
/* Test that peer-to-peer connections work */
/* ---------------------------------------------------------------------------------------------------- */


typedef struct
{
  gboolean accept_connection;
  gint num_connection_attempts;
  GPtrArray *current_connections;
  guint num_method_calls;
} PeerData;

static const GDBusArgInfo test_interface_hello_peer_method_in_args[] =
{
  {"greeting", "s", NULL}
};

static const GDBusArgInfo test_interface_hello_peer_method_out_args[] =
{
  {"response", "s", NULL}
};

static const GDBusMethodInfo test_interface_method_info[] =
{
  {
    "HelloPeer",
    "s", 1, test_interface_hello_peer_method_in_args,
    "s", 1, test_interface_hello_peer_method_out_args,
    NULL
  },
  {
    "EmitSignal",
    "", 0, NULL,
    "", 0, NULL,
    NULL
  }
};

static const GDBusArgInfo test_interface_peer_signal_args[] =
{
  {"a_string", "s", NULL}
};

static const GDBusSignalInfo test_interface_signal_info[] =
{
  {
    "PeerSignal",
    "s", 1, test_interface_peer_signal_args,
    NULL
  }
};

static const GDBusPropertyInfo test_interface_property_info[] =
{
  {
    "PeerProperty",
    "s", G_DBUS_PROPERTY_INFO_FLAGS_READABLE,
    NULL
  }
};

static const GDBusInterfaceInfo test_interface_introspection_data =
{
  "org.gtk.GDBus.PeerTestInterface",
  2, test_interface_method_info,
  1, test_interface_signal_info,
  1, test_interface_property_info,
  NULL,
};

static void
test_interface_method_call (GDBusConnection       *connection,
                            gpointer               user_data,
                            const gchar           *sender,
                            const gchar           *object_path,
                            const gchar           *interface_name,
                            const gchar           *method_name,
                            GVariant              *parameters,
                            GDBusMethodInvocation *invocation)
{
  PeerData *data = user_data;

  g_assert_cmpstr (object_path, ==, "/org/gtk/GDBus/PeerTestObject");
  g_assert_cmpstr (interface_name, ==, "org.gtk.GDBus.PeerTestInterface");

  if (g_strcmp0 (method_name, "HelloPeer") == 0)
    {
      const gchar *greeting;
      gchar *response;

      g_variant_get (parameters, "(s)", &greeting);

      response = g_strdup_printf ("You greeted me with '%s'.",
                                  greeting);
      g_dbus_method_invocation_return_value (invocation,
                                             g_variant_new ("(s)", response));
      g_free (response);
    }
  else if (g_strcmp0 (method_name, "EmitSignal") == 0)
    {
      GError *error;

      error = NULL;
      g_dbus_connection_emit_signal (connection,
                                     NULL,
                                     "/org/gtk/GDBus/PeerTestObject",
                                     "org.gtk.GDBus.PeerTestInterface",
                                     "PeerSignal",
                                     NULL,
                                     &error);
      g_assert_no_error (error);
      g_dbus_method_invocation_return_value (invocation, NULL);
    }
  else
    {
      g_assert_not_reached ();
    }

  data->num_method_calls++;
}

static GVariant *
test_interface_get_property (GDBusConnection  *connection,
                             gpointer          user_data,
                             const gchar      *sender,
                             const gchar      *object_path,
                             const gchar      *interface_name,
                             const gchar      *property_name,
                             GError          **error)
{
  g_assert_cmpstr (object_path, ==, "/org/gtk/GDBus/PeerTestObject");
  g_assert_cmpstr (interface_name, ==, "org.gtk.GDBus.PeerTestInterface");
  g_assert_cmpstr (property_name, ==, "PeerProperty");

  return g_variant_new_string ("ThePropertyValue");
}


static const GDBusInterfaceVTable test_interface_vtable =
{
  test_interface_method_call,
  test_interface_get_property,
  NULL  /* set_property */
};

static void
on_new_connection (GDBusServer      *server,
                   GDBusConnection  *connection,
                   gpointer          user_data)
{
  PeerData *data = user_data;

  data->num_connection_attempts++;

  if (data->accept_connection)
    {
      GError *error;
      guint reg_id;

      g_ptr_array_add (data->current_connections, g_object_ref (connection));

      /* export object on the newly established connection */
      error = NULL;
      reg_id = g_dbus_connection_register_object (connection,
                                                  "/org/gtk/GDBus/PeerTestObject",
                                                  "org.gtk.GDBus.PeerTestInterface",
                                                  &test_interface_introspection_data,
                                                  &test_interface_vtable,
                                                  data,
                                                  NULL, /* GDestroyNotify for data */
                                                  &error);
      g_assert_no_error (error);
      g_assert (reg_id > 0);
    }
  else
    {
      /* don't ref the connection */
    }

  g_main_loop_quit (loop);
}

static void
new_proxy_cb (GObject      *source_object,
              GAsyncResult *res,
              gpointer      user_data)
{
  GDBusProxy **proxy = user_data;
  GError *error;

  error = NULL;
  *proxy = g_dbus_proxy_new_finish (res, &error);
  g_assert_no_error (error);
  g_assert (*proxy != NULL);

  g_main_loop_quit (loop);
}

static void
hello_peer_cb (GDBusProxy   *proxy,
               GAsyncResult *res,
               gpointer      user_data)
{
  GError *error;
  GVariant *result;
  const gchar *s;

  error = NULL;
  result = g_dbus_proxy_invoke_method_finish (proxy, res, &error);
  g_assert_no_error (error);
  g_assert (result != NULL);
  g_variant_get (result, "(s)", &s);
  g_assert_cmpstr (s, ==, "You greeted me with 'Hey Peer!'.");
  g_variant_unref (result);

  g_main_loop_quit (loop);
}

static void
on_proxy_signal_received (GDBusProxy *proxy,
                          gchar      *sender_name,
                          gchar      *signal_name,
                          GVariant   *parameters,
                          gpointer    user_data)
{
  g_assert (sender_name == NULL);
  g_assert_cmpstr (signal_name, ==, "PeerSignal");
  g_main_loop_quit (loop);
}

static void
test_peer (void)
{
  GDBusServer *server;
  GDBusConnection *c;
  GDBusConnection *c2;
  GDBusProxy *proxy;
  GError *error;
  PeerData data;
  GVariant *value;

  error = NULL;
  data.num_connection_attempts = 0;
  data.current_connections = g_ptr_array_new_with_free_func (g_object_unref);
  data.num_method_calls = 0;

  /* first try to connect when there is no server */
  c = g_dbus_connection_new_sync (test_address,
                                  NULL,
                                  &error);
  g_assert_error (error, G_DBUS_ERROR, G_DBUS_ERROR_FILE_NOT_FOUND);
  g_assert (!g_dbus_error_is_remote_error (error));
  g_clear_error (&error);
  g_assert (c == NULL);

  /* bring up a server */
  server = g_dbus_server_new (test_address, &error);
  g_assert_no_error (error);
  g_assert (server != NULL);

  g_signal_connect (server,
                    "new-connection",
                    G_CALLBACK (on_new_connection),
                    &data);

  /* bring up a connection and accept it */
  data.accept_connection = TRUE;
  c = g_dbus_connection_new_sync (test_address,
                                  NULL,
                                  &error);
  g_assert_no_error (error);
  g_assert (c != NULL);
  g_main_loop_run (loop);
  g_assert_cmpint (data.current_connections->len, ==, 1);
  g_assert_cmpint (data.num_connection_attempts, ==, 1);
  g_assert (g_dbus_connection_get_bus_type (c) == G_BUS_TYPE_NONE);
  g_assert (g_dbus_connection_get_unique_name (c) == NULL);
  g_assert_cmpstr (g_dbus_connection_get_address (c), ==, test_address);

  /* check that we create a proxy, read properties, receive signals and invoke the HelloPeer() method
   *
   * Need to do this async to avoid deadlock.
   */
  proxy = NULL;
  g_dbus_proxy_new (c,
                    G_TYPE_DBUS_PROXY,
                    G_DBUS_PROXY_FLAGS_NONE,
                    NULL, /* bus_name */
                    "/org/gtk/GDBus/PeerTestObject",
                    "org.gtk.GDBus.PeerTestInterface",
                    NULL, /* GCancellable */
                    (GAsyncReadyCallback) new_proxy_cb,
                    &proxy);
  g_main_loop_run (loop);
  g_assert (proxy != NULL);
  value = g_dbus_proxy_get_cached_property (proxy, "PeerProperty", &error);
  g_assert_cmpstr (g_variant_get_string (value, NULL), ==, "ThePropertyValue");

  /* try invoking a method - again, async */
  g_dbus_proxy_invoke_method (proxy,
                              "HelloPeer",
                              g_variant_new ("(s)", "Hey Peer!"),
                              -1,
                              NULL,  /* GCancellable */
                              (GAsyncReadyCallback) hello_peer_cb,
                              NULL); /* user_data */
  g_main_loop_run (loop);
  g_assert_cmpint (data.num_method_calls, ==, 1);

  /* make the other peer emit a signal - catch it */
  g_signal_connect (proxy,
                    "g-signal",
                    G_CALLBACK (on_proxy_signal_received),
                    NULL);
  g_dbus_proxy_invoke_method (proxy,
                              "EmitSignal",
                              NULL,  /* no arguments */
                              -1,
                              NULL,  /* GCancellable */
                              NULL,  /* GAsyncReadyCallback - we don't care about the result */
                              NULL); /* user_data */
  g_main_loop_run (loop);
  g_object_unref (proxy);
  g_assert_cmpint (data.num_method_calls, ==, 2);

  /* bring up a connection - don't accept it
   *
   * Note that the client will get the connection - but will be disconnected immediately
   */
  data.accept_connection = FALSE;
  c2 = g_dbus_connection_new_sync (test_address,
                                   NULL,
                                   &error);
  g_assert_no_error (error);
  g_assert (c2 != NULL);
  g_assert (!g_dbus_connection_get_is_disconnected (c2));
  g_main_loop_run (loop);
  g_assert_cmpint (data.current_connections->len, ==, 1);
  g_assert_cmpint (data.num_connection_attempts, ==, 2);
  _g_assert_signal_received (c2, "disconnected");
  g_assert (g_dbus_connection_get_is_disconnected (c2));
  g_object_unref (c2);

  /* bring up a connection - accept it.. then disconnect from the client side - check
   * that the server side gets the disconnect signal.
   */
  data.accept_connection = TRUE;
  c2 = g_dbus_connection_new_sync (test_address,
                                   NULL,
                                   &error);
  g_assert_no_error (error);
  g_assert (c2 != NULL);
  g_assert (!g_dbus_connection_get_is_disconnected (c2));
  g_main_loop_run (loop);
  g_assert_cmpint (data.current_connections->len, ==, 2);
  g_assert_cmpint (data.num_connection_attempts, ==, 3);
  g_assert (!g_dbus_connection_get_is_disconnected (G_DBUS_CONNECTION (data.current_connections->pdata[1])));
  g_object_unref (c2);
  _g_assert_signal_received (G_DBUS_CONNECTION (data.current_connections->pdata[1]), "disconnected");
  g_assert (g_dbus_connection_get_is_disconnected (G_DBUS_CONNECTION (data.current_connections->pdata[1])));
  g_ptr_array_set_size (data.current_connections, 1); /* remove disconnected connection object */

  /* unref the server and stop listening for new connections
   *
   * This won't bring down the established connections - check that c is still connected
   * by invoking a method
   */
  g_object_unref (server);
  g_dbus_proxy_invoke_method (proxy,
                              "HelloPeer",
                              g_variant_new ("(s)", "Hey Peer!"),
                              -1,
                              NULL,  /* GCancellable */
                              (GAsyncReadyCallback) hello_peer_cb,
                              NULL); /* user_data */
  g_main_loop_run (loop);
  g_assert_cmpint (data.num_method_calls, ==, 3);

  /* now disconnect from the server side - check that the client side gets the signal */
  g_assert_cmpint (data.current_connections->len, ==, 1);
  g_assert (G_DBUS_CONNECTION (data.current_connections->pdata[0]) != c);
  g_dbus_connection_disconnect (G_DBUS_CONNECTION (data.current_connections->pdata[0]));
  g_assert (!g_dbus_connection_get_is_disconnected (c));
  _g_assert_signal_received (c, "disconnected");
  g_assert (g_dbus_connection_get_is_disconnected (c));
  g_object_unref (c);

  g_ptr_array_unref (data.current_connections);
}

/* ---------------------------------------------------------------------------------------------------- */

int
main (int   argc,
      char *argv[])
{
  gint ret;

  g_type_init ();
  g_thread_init (NULL);
  g_test_init (&argc, &argv, NULL);

  test_address = g_strdup_printf ("unix:path=/tmp/gdbus-test-pid-%d", getpid ());

  /* all the tests rely on a shared main loop */
  loop = g_main_loop_new (NULL, FALSE);

  g_test_add_func ("/gdbus/peer-to-peer", test_peer);

  ret = g_test_run();

  g_free (test_address);
  g_main_loop_unref (loop);

  return ret;
}
