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

#ifndef __SESSION_BUS_H__
#define __SESSION_BUS_H__

#include <gdbus/gdbus.h>

G_BEGIN_DECLS

const gchar *session_bus_up_with_address       (const gchar *given_address);
void         session_bus_down_with_address     (const gchar *address);
const gchar *session_bus_get_temporary_address (void);
const gchar *session_bus_up                    (void);
void         session_bus_down                  (void);

G_END_DECLS

#endif /* __SESSION_BUS_H__ */
