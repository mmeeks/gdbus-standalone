#include <gdbus/gdbus.h>

static gchar *opt_name         = NULL;
static gchar *opt_object_path  = NULL;
static gchar *opt_interface    = NULL;
static gboolean opt_system_bus = FALSE;

static GOptionEntry opt_entries[] =
{
  { "name", 'n', 0, G_OPTION_ARG_STRING, &opt_name, "Name of the remote object to watch", NULL },
  { "object-path", 'o', 0, G_OPTION_ARG_STRING, &opt_object_path, "Object path of the remote object", NULL },
  { "interface", 'i', 0, G_OPTION_ARG_STRING, &opt_interface, "D-Bus interface of remote object", NULL },
  { "system-bus", 's', 0, G_OPTION_ARG_NONE, &opt_system_bus, "Use the system-bus instead of the session-bus", NULL },
  { NULL}
};

static void
print_properties (GDBusProxy *proxy)
{
  gchar **property_names;
  guint n;

  g_print ("    properties:\n");

  property_names = g_dbus_proxy_get_cached_property_names (proxy, NULL);
  for (n = 0; property_names != NULL && property_names[n] != NULL; n++)
    {
      const gchar *key = property_names[n];
      GVariant *value;
      gchar *value_str;

      value = g_dbus_proxy_get_cached_property (proxy, key, NULL);
      value_str = g_variant_print (value, TRUE);

      g_print ("      %s -> %s\n", key, value_str);

      g_variant_unref (value);
      g_free (value_str);
    }
  g_strfreev (property_names);
}

static void
on_properties_changed (GDBusProxy *proxy,
                       GHashTable *changed_properties,
                       gpointer    user_data)
{
  print_properties (proxy);
}

static void
on_proxy_appeared (GDBusConnection *connection,
                   const gchar     *name,
                   const gchar     *name_owner,
                   GDBusProxy      *proxy,
                   gpointer         user_data)
{
  g_print ("+++ Acquired proxy object for remote object owned by %s\n"
           "    bus:          %s\n"
           "    name:         %s\n"
           "    object path:  %s\n"
           "    interface:    %s\n",
           name_owner,
           opt_system_bus ? "System Bus" : "Session Bus",
           opt_name,
           opt_object_path,
           opt_interface);

  print_properties (proxy);

  g_signal_connect (proxy,
                    "g-properties-changed",
                    G_CALLBACK (on_properties_changed),
                    NULL);
}

static void
on_proxy_vanished (GDBusConnection *connection,
                   const gchar     *name,
                   gpointer         user_data)
{
  g_print ("--- Cannot create proxy object for\n"
           "    bus:          %s\n"
           "    name:         %s\n"
           "    object path:  %s\n"
           "    interface:    %s\n",
           opt_system_bus ? "System Bus" : "Session Bus",
           opt_name,
           opt_object_path,
           opt_interface);
}

int
main (int argc, char *argv[])
{
  guint watcher_id;
  GMainLoop *loop;
  GOptionContext *opt_context;
  GError *error;

  g_type_init ();

  opt_context = g_option_context_new ("g_bus_watch_proxy() example");
  g_option_context_set_summary (opt_context,
                                "Example: to watch the manager object of DeviceKit-disks daemon, use:\n"
                                "\n"
                                "  ./example-watch-proxy -n org.freedesktop.DeviceKit.Disks  \\\n"
                                "                        -o /org/freedesktop/DeviceKit/Disks \\\n"
                                "                        -i org.freedesktop.DeviceKit.Disks  \\\n"
                                "                        --system-bus");
  g_option_context_add_main_entries (opt_context, opt_entries, NULL);
  error = NULL;
  if (!g_option_context_parse (opt_context, &argc, &argv, &error))
    {
      g_printerr ("Error parsing options: %s", error->message);
      goto out;
    }
  if (opt_name == NULL || opt_object_path == NULL || opt_interface == NULL)
    {
      g_printerr ("Incorrect usage, try --help.\n");
      goto out;
    }

  watcher_id = g_bus_watch_proxy (opt_system_bus ? G_BUS_TYPE_SYSTEM : G_BUS_TYPE_SESSION,
                                  opt_name,
                                  opt_object_path,
                                  opt_interface,
                                  G_TYPE_DBUS_PROXY,
                                  G_DBUS_PROXY_FLAGS_NONE,
                                  on_proxy_appeared,
                                  on_proxy_vanished,
                                  NULL,
                                  NULL);

  loop = g_main_loop_new (NULL, FALSE);
  g_main_loop_run (loop);

  g_bus_unwatch_proxy (watcher_id);

 out:
  g_option_context_free (opt_context);
  g_free (opt_name);
  g_free (opt_object_path);
  g_free (opt_interface);

  return 0;
}
