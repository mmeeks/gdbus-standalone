
#include <gdbus/gdbus.h>
#include <stdlib.h>

/* ---------------------------------------------------------------------------------------------------- */

/* Introspection data */

static const GDBusArgInfo method_hello_world_in_args[] =
{
  { "greeting", "s", NULL }
};

static const GDBusArgInfo method_hello_world_out_args[] =
{
  { "response", "s", NULL }
};

static const GDBusMethodInfo test_method_info[] =
{
  {
    "HelloWorld",
    "s", 1, method_hello_world_in_args,
    "s", 1, method_hello_world_out_args,
    NULL
  }
};

/* --- */

static const GDBusArgInfo signal_velocity_changed_args[] =
{
  { "speed_in_mph", "d", NULL },
  { "speed_as_string", "s", NULL }
};

static const GDBusSignalInfo test_signal_info[] =
{
  {
    "VelocityChanged", "velocity-changed",
    "ds", 2, signal_velocity_changed_args,
    NULL
  }
};

/* --- */

static const GDBusPropertyInfo test_property_info[] =
{
  {
    "FluxCapicitorName", "flux-capicitor-name",
    "s",
    G_DBUS_PROPERTY_INFO_FLAGS_READABLE,
    NULL
  },
  {
    "Title", "title",
    "s",
    G_DBUS_PROPERTY_INFO_FLAGS_READABLE | G_DBUS_PROPERTY_INFO_FLAGS_WRITABLE,
    NULL
  }
};

/* --- */

static const GDBusInterfaceInfo test_interface_info =
{
  "org.gtk.GDBus.TestInterface",
  1, test_method_info,
  1, test_signal_info,
  2, test_property_info,
  NULL,
};

/* ---------------------------------------------------------------------------------------------------- */

static void
test_handle_method_call (GDBusConnection       *connection,
                         GObject               *object,
                         const gchar           *sender,
                         const gchar           *object_path,
                         const gchar           *method_name,
                         GVariant              *parameters,
                         GDBusMethodInvocation *invocation)
{
  if (g_strcmp0 (method_name, "HelloWorld") == 0)
    {
      const gchar *greeting;

      g_variant_get (parameters, "(s)", &greeting);

      if (g_strcmp0 (greeting, "Return Unregistered") == 0)
        {
          g_dbus_method_invocation_return_error (invocation,
                                                 G_IO_ERROR,
                                                 G_IO_ERROR_FAILED_HANDLED,
                                                 "As requested, here's a GError not registered (G_IO_ERROR_FAILED_HANDLED)");
        }
      else if (g_strcmp0 (greeting, "Return Registered") == 0)
        {
          g_dbus_method_invocation_return_error (invocation,
                                                 G_DBUS_ERROR,
                                                 G_DBUS_ERROR_MATCH_RULE_NOT_FOUND,
                                                 "As requested, here's a GError that is registered (G_DBUS_ERROR_MATCH_RULE_NOT_FOUND)");
        }
      else if (g_strcmp0 (greeting, "Return Raw") == 0)
        {
          g_dbus_method_invocation_return_dbus_error (invocation,
                                                      "org.gtk.GDBus.SomeErrorName",
                                                      "As requested, here's a raw D-Bus error");
        }
      else
        {
          gchar *response;
          response = g_strdup_printf ("You greeted me with '%s'. Thanks!", greeting);
          g_dbus_method_invocation_return_value (invocation,
                                                 g_variant_new ("(s)", response));
          g_free (response);
        }
    }
}

/* for now */
static const GDBusInterfaceVTable test_interface_vtable =
{
  test_handle_method_call
};

/* ---------------------------------------------------------------------------------------------------- */

static void
on_name_acquired (GDBusConnection *connection,
                  const gchar     *name,
                  gpointer         user_data)
{
  guint registration_id;

  registration_id = g_dbus_connection_register_object (connection,
                                                       NULL,
                                                       "/org/gtk/GDBus/TestObject",
                                                       "org.gtk.GDBus.TestInterface",
                                                       &test_interface_info,
                                                       &test_interface_vtable,
                                                       NULL,
                                                       NULL,
                                                       NULL);
  g_assert (registration_id > 0);
}

static void
on_name_lost (GDBusConnection *connection,
              const gchar     *name,
              gpointer         user_data)
{
  exit (1);
}

int
main (int argc, char *argv[])
{
  guint owner_id;
  GMainLoop *loop;

  g_type_init ();

  owner_id = g_bus_own_name (G_BUS_TYPE_SESSION,
                             "org.gtk.GDBus.TestServer",
                             G_BUS_NAME_OWNER_FLAGS_NONE,
                             on_name_acquired,
                             on_name_lost,
                             NULL);

  loop = g_main_loop_new (NULL, FALSE);
  g_main_loop_run (loop);

  g_bus_unown_name (owner_id);

  return 0;
}

