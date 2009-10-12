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

#define G_DBUS_I_UNDERSTAND_THAT_ABI_AND_API_IS_UNSTABLE
#include <gdbus/gdbus-lowlevel.h>

#include "tests.h"

/* all tests rely on a global connection */
static GDBusConnection *c = NULL;

/* all tests rely on a shared mainloop */
static GMainLoop *loop = NULL;

/* ---------------------------------------------------------------------------------------------------- */
/* Ensure that signal and method replies are delivered in the right thread */
/* ---------------------------------------------------------------------------------------------------- */

typedef struct {
  GThread *thread;
  GMainLoop *thread_loop;
  guint signal_count;
} DeliveryData;

static void
msg_cb_expect_success (GDBusConnection *connection,
                       GAsyncResult    *res,
                       gpointer         user_data)
{
  GError *error;
  DBusMessage *reply;
  DeliveryData *data = user_data;

  error = NULL;
  reply = g_dbus_connection_send_dbus_1_message_with_reply_finish (connection,
                                                                   res,
                                                                   &error);
  g_assert_no_error (error);
  g_assert (reply != NULL);
  dbus_message_unref (reply);

  g_assert (g_thread_self () == data->thread);

  g_main_loop_quit (data->thread_loop);
}

static void
msg_cb_expect_error_cancelled (GDBusConnection *connection,
                               GAsyncResult    *res,
                               gpointer         user_data)
{
  GError *error;
  DBusMessage *reply;
  DeliveryData *data = user_data;

  error = NULL;
  reply = g_dbus_connection_send_dbus_1_message_with_reply_finish (connection,
                                                                   res,
                                                                   &error);
  g_assert_error (error, G_DBUS_ERROR, G_DBUS_ERROR_CANCELLED);
  g_error_free (error);
  g_assert (reply == NULL);

  g_assert (g_thread_self () == data->thread);

  g_main_loop_quit (data->thread_loop);
}

static void
signal_handler (GDBusConnection *connection,
                DBusMessage     *message,
                gpointer         user_data)
{
  DeliveryData *data = user_data;

  g_assert (g_thread_self () == data->thread);

  data->signal_count++;

  g_main_loop_quit (data->thread_loop);
}

static gpointer
test_delivery_in_thread_func (gpointer _data)
{
  GMainLoop *thread_loop;
  GMainContext *thread_context;
  DeliveryData data;
  DBusMessage *m;
  GCancellable *ca;
  guint subscription_id;
  GDBusConnection *priv_c;
  GError *error;

  error = NULL;

  thread_context = g_main_context_new ();
  thread_loop = g_main_loop_new (thread_context, FALSE);
  g_main_context_push_thread_default (thread_context);

  data.thread = g_thread_self ();
  data.thread_loop = thread_loop;
  data.signal_count = 0;

  /* ---------------------------------------------------------------------------------------------------- */

  /* Use the GetId() method on the message bus for testing */
  m = dbus_message_new_method_call (DBUS_SERVICE_DBUS,
                                    DBUS_PATH_DBUS,
                                    DBUS_INTERFACE_DBUS,
                                    "GetId");

  /**
   * Check that we get a reply to the GetId() method call.
   */
  g_dbus_connection_send_dbus_1_message_with_reply (c,
                                                    m,
                                                    -1,
                                                    NULL,
                                                    (GAsyncReadyCallback) msg_cb_expect_success,
                                                    &data);
  g_main_loop_run (thread_loop);

  /**
   * Check that we never actually send a message if the GCancellable is already cancelled - i.e.
   * we should get #G_DBUS_ERROR_CANCELLED instead of #G_DBUS_ERROR_FAILED even when the actual
   * connection is not up.
   */
  ca = g_cancellable_new ();
  g_cancellable_cancel (ca);
  g_dbus_connection_send_dbus_1_message_with_reply (c,
                                                    m,
                                                    -1,
                                                    ca,
                                                    (GAsyncReadyCallback) msg_cb_expect_error_cancelled,
                                                    &data);
  g_main_loop_run (thread_loop);
  g_object_unref (ca);

  /**
   * Check that cancellation works when the message is already in flight.
   */
  ca = g_cancellable_new ();
  g_dbus_connection_send_dbus_1_message_with_reply (c,
                                                    m,
                                                    -1,
                                                    ca,
                                                    (GAsyncReadyCallback) msg_cb_expect_error_cancelled,
                                                    &data);
  g_cancellable_cancel (ca);
  g_main_loop_run (thread_loop);
  g_object_unref (ca);

  /**
   * Check that signals are delivered to the correct thread.
   *
   * First we subscribe to the signal, the we create a a private connection. This should
   * cause a NameOwnerChanged message. Then we tear it down. This should cause another
   * NameOwnerChanged message.
   */
  subscription_id = g_dbus_connection_dbus_1_signal_subscribe (c,
                                                               "org.freedesktop.DBus",  /* sender */
                                                               "org.freedesktop.DBus",  /* interface */
                                                               "NameOwnerChanged",      /* member */
                                                               "/org/freedesktop/DBus", /* path */
                                                               NULL,
                                                               signal_handler,
                                                               &data,
                                                               NULL);
  g_assert (subscription_id != 0);
  g_assert (data.signal_count == 0);

  priv_c = g_dbus_connection_bus_get_private_sync (G_BUS_TYPE_SESSION, NULL, &error);
  g_assert_no_error (error);
  g_assert (priv_c != NULL);

  g_main_loop_run (thread_loop);
  g_assert (data.signal_count == 1);

  g_object_unref (priv_c);

  g_main_loop_run (thread_loop);
  g_assert (data.signal_count == 2);

  g_dbus_connection_dbus_1_signal_unsubscribe (c, subscription_id);

  /* ---------------------------------------------------------------------------------------------------- */

  g_main_context_pop_thread_default (thread_context);
  g_main_loop_unref (thread_loop);
  g_main_context_unref (thread_context);

  dbus_message_unref (m);

  g_main_loop_quit (loop);

  return NULL;
}

static void
test_delivery_in_thread (void)
{
  GError *error;
  GThread *thread;

  error = NULL;
  thread = g_thread_create (test_delivery_in_thread_func,
                            NULL,
                            TRUE,
                            &error);
  g_assert_no_error (error);
  g_assert (thread != NULL);

  /* run the event loop - it is needed to dispatch D-Bus messages */
  g_main_loop_run (loop);

  g_thread_join (thread);
}

/* ---------------------------------------------------------------------------------------------------- */

int
main (int   argc,
      char *argv[])
{
  GError *error;
  gint ret;

  g_type_init ();
  g_thread_init (NULL);
  g_test_init (&argc, &argv, NULL);

  /* all the tests rely on a shared main loop */
  loop = g_main_loop_new (NULL, FALSE);

  /* all the tests use a session bus with a well-known address that we can bring up and down
   * using session_bus_up() and session_bus_down().
   */
  g_unsetenv ("DISPLAY");
  g_setenv ("DBUS_SESSION_BUS_ADDRESS", session_bus_get_temporary_address (), TRUE);

  session_bus_up ();

  /* TODO: wait a bit for the bus to come up.. ideally session_bus_up() won't return
   * until one can connect to the bus but that's not how things work right now
   */
  usleep (500 * 1000);

  /* Create the connection in the main thread */
  error = NULL;
  c = g_dbus_connection_bus_get_sync (G_BUS_TYPE_SESSION, NULL, &error);
  g_assert_no_error (error);
  g_assert (c != NULL);

  g_test_add_func ("/gdbus/delivery-in-thread", test_delivery_in_thread);

  ret = g_test_run();

  g_object_unref (c);

  /* tear down bus */
  session_bus_down ();

  return ret;
}