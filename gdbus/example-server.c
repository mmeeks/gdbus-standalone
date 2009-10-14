
#include <gdbus/gdbus.h>
#include <stdlib.h>

/* ---------------------------------------------------------------------------------------------------- */

static GDBusNodeInfo *introspection_data = NULL;

/* Introspection data for the service we are exporting */
static const gchar introspection_xml[] =
  "<node>"
  "  <interface name='org.gtk.GDBus.TestInterface'>"
  "    <method name='HelloWorld'>"
  "      <arg type='s' name='greeting' direction='in'/>"
  "      <arg type='s' name='response' direction='out'/>"
  "    </method>"
  "    <signal name='VelocityChanged'>"
  "      <arg type='d' name='speed_in_mph'/>"
  "      <arg type='s' name='speed_as_string'/>"
  "    </signal>"
  "    <property type='s' name='FluxCapicitorName' access='read'/>"
  "    <property type='s' name='Title' access='readwrite'/>"
  "  </interface>"
  "</node>";

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
                                                       &introspection_data->interfaces[0],
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

  /* We are lazy here - we don't want to manually provide
   * the introspection data structures - so we just build
   * them from XML.
   */
  introspection_data = g_dbus_node_info_new_for_xml (introspection_xml, NULL);
  g_assert (introspection_data != NULL);

  owner_id = g_bus_own_name (G_BUS_TYPE_SESSION,
                             "org.gtk.GDBus.TestServer",
                             G_BUS_NAME_OWNER_FLAGS_NONE,
                             on_name_acquired,
                             on_name_lost,
                             NULL);

  loop = g_main_loop_new (NULL, FALSE);
  g_main_loop_run (loop);

  g_bus_unown_name (owner_id);

  g_dbus_node_info_free (introspection_data);

  return 0;
}

