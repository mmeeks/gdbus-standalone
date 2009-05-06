/* GDBus - GLib D-Bus Library
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

#if !defined (G_DBUS_COMPILATION)
#error "gdbusprivate.h is a private header file."
#endif

#ifndef __G_DBUS_PRIVATE_H__
#define __G_DBUS_PRIVATE_H__

#include <gdbus/gdbustypes.h>
#include <stdarg.h>

G_BEGIN_DECLS

void _g_dbus_oom (void);

/**
 * _GDBusSignatureVarargForeachCallback:
 * @arg_num: The argument number currently being processed.
 * @signature: Signature for single complete D-Bus type.
 * @type: The #GType corresponding to @signature.
 * @va_args: A #va_list you can use va_arg() on to extract the value.
 * @user_data: User data passed to _gdbus_signature_vararg_foreach().
 *
 * Callback function used in _gdbus_signature_vararg_foreach().
 *
 * Returns: %TRUE to short-circuit iteration, %FALSE to continue iterating.
 */
typedef gboolean (*_GDBusSignatureVarargForeachCallback) (guint        arg_num,
                                                          const gchar *signature,
                                                          GType        type,
                                                          va_list      va_args,
                                                          gpointer     user_data);

gboolean _gdbus_signature_vararg_foreach (const gchar                         *signature,
                                          GType                                first_type,
                                          va_list                              va_args,
                                          _GDBusSignatureVarargForeachCallback callback,
                                          gpointer                             user_data);

GDBusStructure *_g_dbus_structure_new_for_values (const gchar *signature,
                                                  guint        num_elements,
                                                  GValue      *elements);

const GValue   *_g_dbus_structure_get_gvalue_for_element     (GDBusStructure  *structure,
                                                              guint            element);

const GValue *_g_dbus_variant_get_gvalue (GDBusVariant *variant);

GDBusVariant *_g_dbus_variant_new_for_gvalue (const GValue *value, const gchar *signature);

gboolean _g_strv_equal (gchar **strv1,
                        gchar **strv2);

gboolean _g_array_equal (GArray *array1,
                         GArray *array2);

gboolean _g_ptr_array_equal (GPtrArray *array1,
                             GPtrArray *array2,
                             const gchar *element_signature);

gboolean _g_hash_table_equal (GHashTable *hash1,
                              GHashTable *hash2,
                              const gchar *key_signature,
                              const gchar *value_signature);

gboolean _g_dbus_gvalue_equal (const GValue *value1,
                               const GValue *value2,
                               const gchar  *signature);

GType _g_dbus_signature_get_gtype (const gchar *signature);

G_END_DECLS

#endif /* __G_DBUS_PRIVATE_H__ */
