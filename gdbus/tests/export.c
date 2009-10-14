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

#include <dbus/dbus.h>

#include "tests.h"

/* all tests rely on a shared mainloop */
static GMainLoop *loop = NULL;

/* ---------------------------------------------------------------------------------------------------- */
/* Test that we can export objects */
/* ---------------------------------------------------------------------------------------------------- */

static const GDBusMethodInfo foo_method_info[] =
{
  {
    "Method1",
    "", 0, NULL,
    "", 0, NULL,
    NULL
  },
  {
    "Method2",
    "", 0, NULL,
    "", 0, NULL,
    NULL
  }
};

static const GDBusSignalInfo foo_signal_info[] =
{
  {
    "SignalAlpha",
    "", 0, NULL,
    NULL
  }
};

static const GDBusPropertyInfo foo_property_info[] =
{
  {
    "PropertyUno",
    "s",
    G_DBUS_PROPERTY_INFO_FLAGS_READABLE,
    NULL
  }
};

static const GDBusInterfaceInfo foo_interface_info =
{
  "org.example.Foo",
  2,
  foo_method_info,
  1,
  foo_signal_info,
  1,
  foo_property_info,
  NULL,
};

/* for now */
static const GDBusInterfaceVTable foo_interface_vtable = {0};

/* -------------------- */

static const GDBusMethodInfo bar_method_info[] =
{
  {
    "MethodA",
    "", 0, NULL,
    "", 0, NULL,
    NULL
  },
  {
    "MethodB",
    "", 0, NULL,
    "", 0, NULL,
    NULL
  }
};

static const GDBusSignalInfo bar_signal_info[] =
{
  {
    "SignalMars",
    "", 0, NULL,
    NULL
  }
};

static const GDBusPropertyInfo bar_property_info[] =
{
  {
    "PropertyDuo",
    "s",
    G_DBUS_PROPERTY_INFO_FLAGS_READABLE,
    NULL
  }
};

static const GDBusInterfaceInfo bar_interface_info =
{
  "org.example.Bar",
  2,
  bar_method_info,
  1,
  bar_signal_info,
  1,
  bar_property_info,
  NULL,
};

/* for now */
static const GDBusInterfaceVTable bar_interface_vtable = {0};

/* ---------------------------------------------------------------------------------------------------- */

static void
introspect_callback (GDBusConnection *connection,
                     GAsyncResult    *res,
                     gpointer         user_data)
{
  const gchar *s;
  gchar **xml_data = user_data;
  GVariant *result;
  GError *error;

  error = NULL;
  result = g_dbus_connection_invoke_method_with_reply_finish (connection,
                                                              res,
                                                              &error);
  g_assert_no_error (error);
  g_assert (result != NULL);
  g_variant_get (result, "(s)", &s);
  *xml_data = g_strdup (s);
  g_variant_unref (result);

  g_main_loop_quit (loop);
}

static gchar **
get_nodes_at (GDBusConnection  *c,
              const gchar      *object_path)
{
  GError *error;
  GDBusProxy *proxy;
  gchar *xml_data;
  GPtrArray *p;
  GDBusNodeInfo *node_info;
  guint n;

  error = NULL;
  proxy = g_dbus_proxy_new_sync (c,
                                 G_DBUS_PROXY_FLAGS_DO_NOT_LOAD_PROPERTIES |
                                 G_DBUS_PROXY_FLAGS_DO_NOT_CONNECT_SIGNALS,
                                 g_dbus_connection_get_unique_name (c),
                                 object_path,
                                 "org.freedesktop.DBus.Introspectable",
                                 NULL,
                                 &error);
  g_assert_no_error (error);
  g_assert (proxy != NULL);

  /* do this async to avoid libdbus-1 deadlocks */
  xml_data = NULL;
  g_dbus_connection_invoke_method_with_reply (g_dbus_proxy_get_connection (proxy),
                                              g_dbus_proxy_get_unique_bus_name (proxy),
                                              g_dbus_proxy_get_object_path (proxy),
                                              "org.freedesktop.DBus.Introspectable",
                                              "Introspect",
                                              NULL,
                                              -1,
                                              NULL,
                                              (GAsyncReadyCallback) introspect_callback,
                                              &xml_data);
  g_main_loop_run (loop);
  g_assert (xml_data != NULL);

  node_info = g_dbus_node_info_new_for_xml (xml_data, &error);
  g_assert_no_error (error);
  g_assert (node_info != NULL);

  p = g_ptr_array_new ();
  for (n = 0; n < node_info->num_nodes; n++)
    {
      const GDBusNodeInfo *sub_node_info = node_info->nodes + n;
      g_ptr_array_add (p, g_strdup (sub_node_info->path));
    }
  g_ptr_array_add (p, NULL);

  g_object_unref (proxy);
  g_free (xml_data);
  g_dbus_node_info_free (node_info);

  return (gchar **) g_ptr_array_free (p, FALSE);
}

typedef struct
{
  guint num_unregistered_calls;

} ObjectRegistrationData;

static void
on_object_unregistered (gpointer user_data)
{
  ObjectRegistrationData *data = user_data;

  data->num_unregistered_calls++;
}

static gboolean
_g_strv_has_string (const gchar* const * haystack,
                    const gchar *needle)
{
  guint n;

  for (n = 0; haystack != NULL && haystack[n] != NULL; n++)
    {
      if (g_strcmp0 (haystack[n], needle) == 0)
        return TRUE;
    }
  return FALSE;
}

static DBusHandlerResult
dc_message_func (DBusConnection *connection,
                 DBusMessage    *message,
                 void           *user_data)
{
  return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
}

static void
test_object_registration (void)
{
  GDBusConnection *c;
  GError *error;
  GObject *boss;
  GObject *worker1;
  GObject *worker2;
  GObject *intern1;
  GObject *intern2;
  ObjectRegistrationData data;
  gchar **nodes;
  guint registration_id;
  guint null_registration_id;
  guint num_successful_registrations;
  DBusConnection *dc;
  DBusError dbus_error;
  DBusObjectPathVTable dc_obj_vtable =
    {
      NULL,
      dc_message_func,
      NULL,
      NULL,
      NULL,
      NULL
    };

  data.num_unregistered_calls = 0;
  num_successful_registrations = 0;

  error = NULL;
  c = g_dbus_connection_bus_get_sync (G_BUS_TYPE_SESSION, NULL, &error);
  g_assert_no_error (error);
  g_assert (c != NULL);

  boss = g_object_new (G_TYPE_OBJECT, NULL);
  registration_id = g_dbus_connection_register_object (c,
                                                       boss,
                                                       "/foo/boss",
                                                       foo_interface_info.name,
                                                       &foo_interface_info,
                                                       &foo_interface_vtable,
                                                       on_object_unregistered,
                                                       &data,
                                                       &error);
  g_assert_no_error (error);
  g_assert (registration_id > 0);
  num_successful_registrations++;

  registration_id = g_dbus_connection_register_object (c,
                                                       boss,
                                                       "/foo/boss",
                                                       bar_interface_info.name,
                                                       &bar_interface_info,
                                                       &bar_interface_vtable,
                                                       on_object_unregistered,
                                                       &data,
                                                       &error);
  g_assert_no_error (error);
  g_assert (registration_id > 0);
  num_successful_registrations++;

  worker1 = g_object_new (G_TYPE_OBJECT, NULL);
  registration_id = g_dbus_connection_register_object (c,
                                                       worker1,
                                                       "/foo/boss/worker1",
                                                       foo_interface_info.name,
                                                       &foo_interface_info,
                                                       &foo_interface_vtable,
                                                       on_object_unregistered,
                                                       &data,
                                                       &error);
  g_assert_no_error (error);
  g_assert (registration_id > 0);
  num_successful_registrations++;

  worker2 = g_object_new (G_TYPE_OBJECT, NULL);
  registration_id = g_dbus_connection_register_object (c,
                                                       worker2,
                                                       "/foo/boss/worker2",
                                                       bar_interface_info.name,
                                                       &bar_interface_info,
                                                       &bar_interface_vtable,
                                                       on_object_unregistered,
                                                       &data,
                                                       &error);
  g_assert_no_error (error);
  g_assert (registration_id > 0);
  num_successful_registrations++;

  intern1 = g_object_new (G_TYPE_OBJECT, NULL);
  registration_id = g_dbus_connection_register_object (c,
                                                       worker1,
                                                       "/foo/boss/interns/intern1",
                                                       foo_interface_info.name,
                                                       &foo_interface_info,
                                                       &foo_interface_vtable,
                                                       on_object_unregistered,
                                                       &data,
                                                       &error);
  g_assert_no_error (error);
  g_assert (registration_id > 0);
  num_successful_registrations++;

  /* Now check we get an error if trying to register a path already registered by another D-Bus binding
   *
   * To check this we need to pretend, for a while, that we're another binding.
   */
  dbus_error_init (&dbus_error);
  dc = dbus_bus_get (DBUS_BUS_SESSION, &dbus_error);
  g_assert (!dbus_error_is_set (&dbus_error));
  g_assert (dc != NULL);
  g_assert (dbus_connection_try_register_object_path (dc,
                                                      "/foo/boss/interns/other_intern",
                                                      &dc_obj_vtable,
                                                      NULL,
                                                      &dbus_error));
  intern2 = g_object_new (G_TYPE_OBJECT, NULL);
  registration_id = g_dbus_connection_register_object (c,
                                                       intern2,
                                                       "/foo/boss/interns/other_intern",
                                                       bar_interface_info.name,
                                                       &bar_interface_info,
                                                       &bar_interface_vtable,
                                                       on_object_unregistered,
                                                       &data,
                                                       &error);
  g_assert_error (error, G_DBUS_ERROR, G_DBUS_ERROR_OBJECT_PATH_IN_USE);
  g_error_free (error);
  error = NULL;
  g_assert (registration_id == 0);

  /* ... and try again at another path */
  registration_id = g_dbus_connection_register_object (c,
                                                       intern2,
                                                       "/foo/boss/interns/intern2",
                                                       bar_interface_info.name,
                                                       &bar_interface_info,
                                                       &bar_interface_vtable,
                                                       on_object_unregistered,
                                                       &data,
                                                       &error);
  g_assert_no_error (error);
  g_assert (registration_id > 0);
  num_successful_registrations++;

  /* register at the same path/interface - this should fail */
  registration_id = g_dbus_connection_register_object (c,
                                                       intern2,
                                                       "/foo/boss/interns/intern2",
                                                       bar_interface_info.name,
                                                       &bar_interface_info,
                                                       &bar_interface_vtable,
                                                       on_object_unregistered,
                                                       &data,
                                                       &error);
  g_assert_error (error, G_DBUS_ERROR, G_DBUS_ERROR_OBJECT_PATH_IN_USE);
  g_error_free (error);
  error = NULL;
  g_assert (registration_id == 0);

  /* register at different interface - shouldn't fail */
  registration_id = g_dbus_connection_register_object (c,
                                                       intern2,
                                                       "/foo/boss/interns/intern2",
                                                       foo_interface_info.name,
                                                       &foo_interface_info,
                                                       &foo_interface_vtable,
                                                       on_object_unregistered,
                                                       &data,
                                                       &error);
  g_assert_no_error (error);
  g_assert (registration_id > 0);
  num_successful_registrations++;

  /* finalize object - should cause two unregistration calls - one for each interface */
  g_object_unref (intern2);
  intern2 = NULL;
  g_assert_cmpint (data.num_unregistered_calls, ==, 2);

  /* register it again */
  intern2 = g_object_new (G_TYPE_OBJECT, NULL);
  registration_id = g_dbus_connection_register_object (c,
                                                       intern2,
                                                       "/foo/boss/interns/intern2",
                                                       bar_interface_info.name,
                                                       &bar_interface_info,
                                                       &bar_interface_vtable,
                                                       on_object_unregistered,
                                                       &data,
                                                       &error);
  g_assert_no_error (error);
  g_assert (registration_id > 0);
  num_successful_registrations++;

  /* unregister it via the id */
  g_assert (g_dbus_connection_unregister_object (c, registration_id));
  g_assert_cmpint (data.num_unregistered_calls, ==, 3);

  /* register it back */
  registration_id = g_dbus_connection_register_object (c,
                                                       intern2,
                                                       "/foo/boss/interns/intern2",
                                                       bar_interface_info.name,
                                                       &bar_interface_info,
                                                       &bar_interface_vtable,
                                                       on_object_unregistered,
                                                       &data,
                                                       &error);
  g_assert_no_error (error);
  g_assert (registration_id > 0);
  num_successful_registrations++;

  /* check we can pass NULL for @object, @introspection_data and @vtable */
  null_registration_id = g_dbus_connection_register_object (c,
                                                            NULL,
                                                            "/foo/boss/interns/intern3",
                                                            bar_interface_info.name,
                                                            NULL,
                                                            NULL,
                                                            on_object_unregistered,
                                                            &data,
                                                            &error);
  g_assert_no_error (error);
  g_assert (null_registration_id > 0);
  num_successful_registrations++;

  /* now check that the object hierarachy is properly generated
   */
  nodes = get_nodes_at (c, "/");
  g_assert (nodes != NULL);
  g_assert_cmpint (g_strv_length (nodes), ==, 1);
  g_assert_cmpstr (nodes[0], ==, "foo");
  g_strfreev (nodes);
  nodes = get_nodes_at (c, "/foo");
  g_assert (nodes != NULL);
  g_assert_cmpint (g_strv_length (nodes), ==, 1);
  g_assert_cmpstr (nodes[0], ==, "boss");
  g_strfreev (nodes);
  nodes = get_nodes_at (c, "/foo/boss");
  g_assert (nodes != NULL);
  g_assert_cmpint (g_strv_length (nodes), ==, 3);
  g_assert (_g_strv_has_string ((const gchar* const *) nodes, "worker1"));
  g_assert (_g_strv_has_string ((const gchar* const *) nodes, "worker2"));
  g_assert (_g_strv_has_string ((const gchar* const *) nodes, "interns"));
  g_strfreev (nodes);
  nodes = get_nodes_at (c, "/foo/boss/interns");
  g_assert (nodes != NULL);
  g_assert_cmpint (g_strv_length (nodes), ==, 4);
  g_assert (_g_strv_has_string ((const gchar* const *) nodes, "intern1"));
  g_assert (_g_strv_has_string ((const gchar* const *) nodes, "intern2"));
  g_assert (_g_strv_has_string ((const gchar* const *) nodes, "intern3"));
  g_assert (_g_strv_has_string ((const gchar* const *) nodes, "other_intern")); /* from the other "binding" */
  g_strfreev (nodes);

  g_object_unref (boss);
  g_object_unref (worker1);
  g_object_unref (worker2);
  g_object_unref (intern1);
  g_object_unref (intern2);
  /* it's -1 because one of our registrations didn't pass an object */
  g_assert_cmpint (data.num_unregistered_calls, ==, num_successful_registrations - 1);

  /* Shutdown the other "binding" */
  g_assert (dbus_connection_unregister_object_path (dc, "/foo/boss/interns/other_intern"));
  dbus_connection_unref (dc);

  /* we have one remaining registration pending */
  g_assert (g_dbus_connection_unregister_object (c, null_registration_id));

  /* check that we no longer export any objects - TODO: it looks like there's a bug in
   * libdbus-1 here (it still reports the '/foo' object) so disable the test for now
   */
#if 0
  nodes = get_nodes_at (c, "/");
  g_assert (nodes != NULL);
  g_assert_cmpint (g_strv_length (nodes), ==, 0);
  g_strfreev (nodes);
#endif

  g_object_unref (c);
}


/* ---------------------------------------------------------------------------------------------------- */

int
main (int   argc,
      char *argv[])
{
  gint ret;

  g_type_init ();
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

  g_test_add_func ("/gdbus/object-registration", test_object_registration);
  /* TODO: check that we spit out correct introspection data */
  /* TODO: check that registering a whole subtree works */

  ret = g_test_run();

  /* tear down bus */
  session_bus_down ();

  return ret;
}
