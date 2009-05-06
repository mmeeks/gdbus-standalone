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

#if !defined (__G_DBUS_G_DBUS_H_INSIDE__) && !defined (G_DBUS_COMPILATION)
#error "Only <gdbus/gdbus.h> can be included directly."
#endif

#ifndef __G_DBUS_VARIANT_H__
#define __G_DBUS_VARIANT_H__

#include <gdbus/gdbustypes.h>

G_BEGIN_DECLS

#define G_TYPE_DBUS_VARIANT         (g_dbus_variant_get_type ())
#define G_DBUS_VARIANT(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), G_TYPE_DBUS_VARIANT, GDBusVariant))
#define G_DBUS_VARIANT_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), G_TYPE_DBUS_VARIANT, GDBusVariantClass))
#define G_DBUS_VARIANT_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), G_TYPE_DBUS_VARIANT, GDBusVariantClass))
#define G_IS_DBUS_VARIANT(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), G_TYPE_DBUS_VARIANT))
#define G_IS_DBUS_VARIANT_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), G_TYPE_DBUS_VARIANT))

typedef struct _GDBusVariantClass   GDBusVariantClass;
typedef struct _GDBusVariantPrivate GDBusVariantPrivate;

/**
 * GDBusVariant:
 *
 * The #GDBusVariant structure contains only private data and
 * should only be accessed using the provided API.
 */
struct _GDBusVariant
{
  /*< private >*/
  GObject parent_instance;
  GDBusVariantPrivate *priv;
};

/**
 * GDBusVariantClass:
 *
 * Class structure for #GDBusVariant.
 */
struct _GDBusVariantClass
{
  /*< private >*/
  GObjectClass parent_class;

  /*< public >*/

  /*< private >*/
  /* Padding for future expansion */
  void (*_g_reserved1) (void);
  void (*_g_reserved2) (void);
  void (*_g_reserved3) (void);
  void (*_g_reserved4) (void);
  void (*_g_reserved5) (void);
  void (*_g_reserved6) (void);
  void (*_g_reserved7) (void);
  void (*_g_reserved8) (void);
};

GType            g_dbus_variant_get_type                  (void) G_GNUC_CONST;
const gchar     *g_dbus_variant_get_variant_signature     (GDBusVariant    *variant);
gboolean         g_dbus_variant_equal                     (GDBusVariant    *variant1,
                                                           GDBusVariant    *variant2);
/* constructors */
GDBusVariant    *g_dbus_variant_new                       (void);
GDBusVariant    *g_dbus_variant_new_for_string            (const gchar     *value);
GDBusVariant    *g_dbus_variant_new_for_object_path       (const gchar     *value);
GDBusVariant    *g_dbus_variant_new_for_signature         (const gchar     *value);
GDBusVariant    *g_dbus_variant_new_for_string_array      (gchar          **value);
GDBusVariant    *g_dbus_variant_new_for_object_path_array (gchar          **value);
GDBusVariant    *g_dbus_variant_new_for_signature_array   (gchar          **value);
GDBusVariant    *g_dbus_variant_new_for_byte              (guchar           value);
GDBusVariant    *g_dbus_variant_new_for_int16             (gint16           value);
GDBusVariant    *g_dbus_variant_new_for_uint16            (guint16          value);
GDBusVariant    *g_dbus_variant_new_for_int               (gint             value);
GDBusVariant    *g_dbus_variant_new_for_uint              (guint            value);
GDBusVariant    *g_dbus_variant_new_for_int64             (gint64           value);
GDBusVariant    *g_dbus_variant_new_for_uint64            (guint64          value);
GDBusVariant    *g_dbus_variant_new_for_boolean           (gboolean         value);
GDBusVariant    *g_dbus_variant_new_for_double            (gdouble          value);
GDBusVariant    *g_dbus_variant_new_for_array             (GArray          *array,
                                                           const gchar     *element_signature);
GDBusVariant    *g_dbus_variant_new_for_ptr_array         (GPtrArray       *array,
                                                           const gchar     *element_signature);
GDBusVariant    *g_dbus_variant_new_for_hash_table        (GHashTable      *hash_table,
                                                           const gchar     *key_signature,
                                                           const gchar     *value_signature);
GDBusVariant    *g_dbus_variant_new_for_structure         (GDBusStructure  *structure);
GDBusVariant    *g_dbus_variant_new_for_variant           (GDBusVariant    *variant);

/* getters */
const gchar     *g_dbus_variant_get_string            (GDBusVariant   *variant);
const gchar     *g_dbus_variant_get_object_path       (GDBusVariant   *variant);
const gchar     *g_dbus_variant_get_signature         (GDBusVariant   *variant);
gchar          **g_dbus_variant_get_string_array      (GDBusVariant   *variant);
gchar          **g_dbus_variant_get_object_path_array (GDBusVariant   *variant);
gchar          **g_dbus_variant_get_signature_array   (GDBusVariant   *variant);
guchar           g_dbus_variant_get_byte              (GDBusVariant   *variant);
gint16           g_dbus_variant_get_int16             (GDBusVariant   *variant);
guint16          g_dbus_variant_get_uint16            (GDBusVariant   *variant);
gint             g_dbus_variant_get_int               (GDBusVariant   *variant);
guint            g_dbus_variant_get_uint              (GDBusVariant   *variant);
gint64           g_dbus_variant_get_int64             (GDBusVariant   *variant);
guint64          g_dbus_variant_get_uint64            (GDBusVariant   *variant);
gboolean         g_dbus_variant_get_boolean           (GDBusVariant   *variant);
gdouble          g_dbus_variant_get_double            (GDBusVariant   *variant);
GArray          *g_dbus_variant_get_array             (GDBusVariant   *variant);
GPtrArray       *g_dbus_variant_get_ptr_array         (GDBusVariant   *variant);
GHashTable      *g_dbus_variant_get_hash_table        (GDBusVariant   *variant);
GDBusStructure  *g_dbus_variant_get_structure         (GDBusVariant   *variant);
GDBusVariant    *g_dbus_variant_get_variant           (GDBusVariant   *variant);

/* setters */
void             g_dbus_variant_set_string            (GDBusVariant   *variant,
                                                       const gchar    *value);
void             g_dbus_variant_set_object_path       (GDBusVariant   *variant,
                                                       const gchar    *value);
void             g_dbus_variant_set_signature         (GDBusVariant   *variant,
                                                       const gchar    *value);
void             g_dbus_variant_set_string_array      (GDBusVariant   *variant,
                                                       gchar         **value);
void             g_dbus_variant_set_object_path_array (GDBusVariant   *variant,
                                                       gchar         **value);
void             g_dbus_variant_set_signature_array   (GDBusVariant   *variant,
                                                       gchar         **value);
void             g_dbus_variant_set_byte              (GDBusVariant   *variant,
                                                       guchar          value);
void             g_dbus_variant_set_int16             (GDBusVariant   *variant,
                                                       gint16          value);
void             g_dbus_variant_set_uint16            (GDBusVariant   *variant,
                                                       guint16         value);
void             g_dbus_variant_set_int               (GDBusVariant   *variant,
                                                       gint            value);
void             g_dbus_variant_set_uint              (GDBusVariant   *variant,
                                                       guint           value);
void             g_dbus_variant_set_int64             (GDBusVariant   *variant,
                                                       gint64          value);
void             g_dbus_variant_set_uint64            (GDBusVariant   *variant,
                                                       guint64         value);
void             g_dbus_variant_set_boolean           (GDBusVariant   *variant,
                                                       gboolean        value);
void             g_dbus_variant_set_double            (GDBusVariant   *variant,
                                                       gdouble         value);
void             g_dbus_variant_set_array             (GDBusVariant   *variant,
                                                       GArray         *array,
                                                       const gchar    *element_signature);
void             g_dbus_variant_set_ptr_array         (GDBusVariant   *variant,
                                                       GPtrArray      *array,
                                                       const gchar    *element_signature);
void             g_dbus_variant_set_hash_table        (GDBusVariant   *variant,
                                                       GHashTable     *hash_table,
                                                       const gchar    *key_signature,
                                                       const gchar    *value_signature);
void             g_dbus_variant_set_structure         (GDBusVariant   *variant,
                                                       GDBusStructure *structure);
void             g_dbus_variant_set_variant           (GDBusVariant   *variant,
                                                       GDBusVariant   *other_variant);

/* identification */
gboolean         g_dbus_variant_is_unset              (GDBusVariant     *variant);
gboolean         g_dbus_variant_is_string             (GDBusVariant     *variant);
gboolean         g_dbus_variant_is_object_path        (GDBusVariant     *variant);
gboolean         g_dbus_variant_is_signature          (GDBusVariant     *variant);
gboolean         g_dbus_variant_is_string_array       (GDBusVariant     *variant);
gboolean         g_dbus_variant_is_object_path_array  (GDBusVariant     *variant);
gboolean         g_dbus_variant_is_signature_array    (GDBusVariant     *variant);
gboolean         g_dbus_variant_is_byte               (GDBusVariant     *variant);
gboolean         g_dbus_variant_is_int16              (GDBusVariant     *variant);
gboolean         g_dbus_variant_is_uint16             (GDBusVariant     *variant);
gboolean         g_dbus_variant_is_int                (GDBusVariant     *variant);
gboolean         g_dbus_variant_is_uint               (GDBusVariant     *variant);
gboolean         g_dbus_variant_is_int64              (GDBusVariant     *variant);
gboolean         g_dbus_variant_is_uint64             (GDBusVariant     *variant);
gboolean         g_dbus_variant_is_boolean            (GDBusVariant     *variant);
gboolean         g_dbus_variant_is_double             (GDBusVariant     *variant);
gboolean         g_dbus_variant_is_array              (GDBusVariant     *variant);
gboolean         g_dbus_variant_is_ptr_array          (GDBusVariant     *variant);
gboolean         g_dbus_variant_is_hash_table         (GDBusVariant     *variant);
gboolean         g_dbus_variant_is_structure          (GDBusVariant     *variant);
gboolean         g_dbus_variant_is_variant            (GDBusVariant     *variant);

G_END_DECLS

#endif /* __G_DBUS_VARIANT_H__ */
