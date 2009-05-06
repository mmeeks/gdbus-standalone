#include <gdbus/gdbus.h>

static void
on_name_appeared (GDBusConnection *connection,
                  const gchar     *name,
                  const gchar     *name_owner,
                  gpointer         user_data)
{
  g_print ("Name %s on the session bus is owned by %s\n", name, name_owner);
}

static void
on_name_vanished (GDBusConnection *connection,
                  const gchar     *name,
                  gpointer         user_data)
{
  g_print ("Name %s does not exists on the session bus\n", name);
}

int
main (int argc, char *argv[])
{
  guint watcher_id;
  GMainLoop *loop;
  gchar *opt_name;
  GOptionContext *opt_context;
  GError *error;
  GOptionEntry opt_entries[] =
    {
      { "name", 'n', 0, G_OPTION_ARG_STRING, &opt_name, "Name to watch", NULL },
      { NULL}
    };

  g_type_init ();

  error = NULL;
  opt_name = NULL;
  opt_context = g_option_context_new ("g_bus_watch_name() example");
  g_option_context_add_main_entries (opt_context, opt_entries, NULL);
  if (!g_option_context_parse (opt_context, &argc, &argv, &error))
    {
      g_printerr ("Error parsing options: %s", error->message);
      return 1;
    }
  if (opt_name == NULL)
    {
      g_printerr ("Incorrect usage, try --help.\n");
      return 1;
    }

  g_type_init ();

  watcher_id = g_bus_watch_name (G_BUS_TYPE_SESSION,
                                 opt_name,
                                 on_name_appeared,
                                 on_name_vanished,
                                 NULL);

  loop = g_main_loop_new (NULL, FALSE);
  g_main_loop_run (loop);

  g_bus_unwatch_name (watcher_id);

  return 0;
}
