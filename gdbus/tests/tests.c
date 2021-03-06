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

/* ---------------------------------------------------------------------------------------------------- */

typedef struct
{
  GMainLoop *loop;
  gboolean   timed_out;
} PropertyNotifyData;

static void
on_property_notify (GObject    *object,
                    GParamSpec *pspec,
                    gpointer    user_data)
{
  PropertyNotifyData *data = user_data;
  g_main_loop_quit (data->loop);
}

static gboolean
on_property_notify_timeout (gpointer user_data)
{
  PropertyNotifyData *data = user_data;
  data->timed_out = TRUE;
  g_main_loop_quit (data->loop);
  return TRUE;
}

gboolean
_g_assert_property_notify_run (gpointer     object,
                               const gchar *property_name)
{
  gchar *s;
  gulong handler_id;
  guint timeout_id;
  PropertyNotifyData data;

  data.loop = g_main_loop_new (NULL, FALSE);
  data.timed_out = FALSE;
  s = g_strdup_printf ("notify::%s", property_name);
  handler_id = g_signal_connect (object,
                                 s,
                                 G_CALLBACK (on_property_notify),
                                 &data);
  g_free (s);
  timeout_id = g_timeout_add (5 * 1000,
                              on_property_notify_timeout,
                              &data);
  g_main_loop_run (data.loop);
  g_signal_handler_disconnect (object, handler_id);
  g_source_remove (timeout_id);
  g_main_loop_unref (data.loop);

  return data.timed_out;
}

/* ---------------------------------------------------------------------------------------------------- */

typedef struct
{
  GMainLoop *loop;
  gboolean   timed_out;
} SignalReceivedData;

static void
on_signal_received (gpointer user_data)
{
  SignalReceivedData *data = user_data;
  g_main_loop_quit (data->loop);
}

static gboolean
on_signal_received_timeout (gpointer user_data)
{
  SignalReceivedData *data = user_data;
  data->timed_out = TRUE;
  g_main_loop_quit (data->loop);
  return TRUE;
}

gboolean
_g_assert_signal_received_run (gpointer     object,
                               const gchar *signal_name)
{
  gulong handler_id;
  guint timeout_id;
  SignalReceivedData data;

  data.loop = g_main_loop_new (NULL, FALSE);
  data.timed_out = FALSE;
  handler_id = g_signal_connect_swapped (object,
                                         signal_name,
                                         G_CALLBACK (on_signal_received),
                                         &data);
  timeout_id = g_timeout_add (5 * 1000,
                              on_signal_received_timeout,
                              &data);
  g_main_loop_run (data.loop);
  g_signal_handler_disconnect (object, handler_id);
  g_source_remove (timeout_id);
  g_main_loop_unref (data.loop);

  return data.timed_out;
}

/* ---------------------------------------------------------------------------------------------------- */
