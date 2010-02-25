/*
 * Copyright © 2007, 2008 Ryan Lortie
 * Copyright © 2009 Codethink Limited
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
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifndef __G_VARIANT_H__
#define __G_VARIANT_H__

#include "gvarianttype.h"
#include <glib/gstring.h>
#include <glib/gmarkup.h>
#include <glib/gerror.h>
#include <glib-object.h>
#include <stdarg.h>

typedef struct _GVariant        GVariant;
typedef struct _GVariantIter    GVariantIter;
typedef struct _GVariantBuilder GVariantBuilder;
typedef enum   _GVariantClass   GVariantClass;

/* compatibility bits yanked from other pieces of glib */
#ifndef GSIZE_FROM_LE
#  define GSIZE_FROM_LE(val)      (GSIZE_TO_LE (val))
#  define GSSIZE_FROM_LE(val)     (GSSIZE_TO_LE (val))
// tad of a hack for now, but safe I hope
#  if GLIB_SIZEOF_SIZE_T == 4
#    define GSIZE_TO_LE(val)   ((gsize) GUINT32_TO_LE (val))
#  elif GLIB_SIZEOF_SIZE_T == 8
#    define GSIZE_TO_LE(val)   ((gsize) GUINT64_TO_LE (val))
#  else
#    error "Weirdo architecture - needs an updated glib"
#  endif
#endif
#ifndef G_TYPE_VARIANT
#  define        G_TYPE_VARIANT (g_variant_get_gtype ())
GType   g_variant_get_gtype     (void)  G_GNUC_CONST;
#endif

enum _GVariantClass
{
  G_VARIANT_CLASS_BOOLEAN       = 'b',
  G_VARIANT_CLASS_BYTE          = 'y',
  G_VARIANT_CLASS_INT16         = 'n',
  G_VARIANT_CLASS_UINT16        = 'q',
  G_VARIANT_CLASS_INT32         = 'i',
  G_VARIANT_CLASS_UINT32        = 'u',
  G_VARIANT_CLASS_INT64         = 'x',
  G_VARIANT_CLASS_UINT64        = 't',
  G_VARIANT_CLASS_HANDLE        = 'h',
  G_VARIANT_CLASS_DOUBLE        = 'd',
  G_VARIANT_CLASS_STRING        = 's',
  G_VARIANT_CLASS_OBJECT_PATH   = 'o',
  G_VARIANT_CLASS_SIGNATURE     = 'g',
  G_VARIANT_CLASS_VARIANT       = 'v',
  G_VARIANT_CLASS_MAYBE         = 'm',
  G_VARIANT_CLASS_ARRAY         = 'a',
  G_VARIANT_CLASS_TUPLE         = '(',
  G_VARIANT_CLASS_DICT_ENTRY    = '{'
};

struct _GVariantIter
{
  gpointer priv[8];
};

G_BEGIN_DECLS

GVariant *                      g_variant_ref                           (GVariant             *value);
GVariant *                      g_variant_ref_sink                      (GVariant             *value);
void                            g_variant_unref                         (GVariant             *value);
void                            g_variant_flatten                       (GVariant             *value);

const GVariantType *            g_variant_get_type                      (GVariant             *value);
const gchar *                   g_variant_get_type_string               (GVariant             *value);
gboolean                        g_variant_is_basic                      (GVariant             *value);
gboolean                        g_variant_is_container                  (GVariant             *value);
gboolean                        g_variant_has_type                      (GVariant             *value,
                                                                         const GVariantType   *pattern);

/* varargs construct/deconstruct */
GVariant *                      g_variant_new                           (const gchar          *format_string,
                                                                         ...);
void                            g_variant_get                           (GVariant             *value,
                                                                         const gchar          *format_string,
                                                                         ...);

gboolean                        g_variant_format_string_scan            (const gchar          *string,
                                                                         const gchar          *limit,
                                                                         const gchar         **endptr);
GVariantType *                  g_variant_format_string_scan_type       (const gchar          *string,
                                                                         const gchar          *limit,
                                                                         const gchar         **endptr);
GVariant *                      g_variant_new_va                        (gpointer              must_be_null,
                                                                         const gchar          *format_string,
                                                                         const gchar         **endptr,
                                                                         va_list              *app);
void                            g_variant_get_va                        (GVariant             *value,
                                                                         gpointer              must_be_null,
                                                                         const gchar          *format_string,
                                                                         const gchar         **endptr,
                                                                         va_list              *app);

/* constructors */
GVariant *                      g_variant_new_boolean                   (gboolean              boolean);
GVariant *                      g_variant_new_byte                      (guint8                byte);
GVariant *                      g_variant_new_uint16                    (guint16               uint16);
GVariant *                      g_variant_new_int16                     (gint16                int16);
GVariant *                      g_variant_new_uint32                    (guint32               uint32);
GVariant *                      g_variant_new_int32                     (gint32                int32);
GVariant *                      g_variant_new_uint64                    (guint64               uint64);
GVariant *                      g_variant_new_int64                     (gint64                int64);
GVariant *                      g_variant_new_double                    (gdouble               floating);
GVariant *                      g_variant_new_string                    (const gchar          *string);
GVariant *                      g_variant_new_object_path               (const gchar          *string);
gboolean                        g_variant_is_object_path                (const gchar          *string);
GVariant *                      g_variant_new_signature                 (const gchar          *string);
gboolean                        g_variant_is_signature                  (const gchar          *string);
GVariant *                      g_variant_new_variant                   (GVariant             *value);
GVariant *                      g_variant_new_handle                    (gint32                handle);
GVariant *                      g_variant_new_strv                      (const gchar * const  *strv,
                                                                         gint                  length);

/* deconstructors */
gboolean                        g_variant_get_boolean                   (GVariant             *value);
guint8                          g_variant_get_byte                      (GVariant             *value);
guint16                         g_variant_get_uint16                    (GVariant             *value);
gint16                          g_variant_get_int16                     (GVariant             *value);
guint32                         g_variant_get_uint32                    (GVariant             *value);
gint32                          g_variant_get_int32                     (GVariant             *value);
guint64                         g_variant_get_uint64                    (GVariant             *value);
gint64                          g_variant_get_int64                     (GVariant             *value);
gdouble                         g_variant_get_double                    (GVariant             *value);
const gchar *                   g_variant_get_string                    (GVariant             *value,
                                                                         gsize                *length);
gchar *                         g_variant_dup_string                    (GVariant             *value,
                                                                         gsize                *length);
const gchar **                  g_variant_get_strv                      (GVariant             *value,
                                                                         gint                 *length);
gchar **                        g_variant_dup_strv                      (GVariant             *value,
                                                                         gint                 *length);
GVariant *                      g_variant_get_variant                   (GVariant             *value);
gint32                          g_variant_get_handle                    (GVariant             *value);
gconstpointer                   g_variant_get_fixed                     (GVariant             *value,
                                                                         gsize                 size);
gconstpointer                   g_variant_get_fixed_array               (GVariant             *value,
                                                                         gsize                 elem_size,
                                                                         gsize                *length);
gsize                           g_variant_n_children                    (GVariant             *value);
GVariant *                      g_variant_get_child_value               (GVariant             *value,
                                                                         gsize                 index);
void                            g_variant_get_child                     (GVariant             *value,
                                                                         gint                  index,
                                                                         const gchar          *format_string,
                                                                         ...);
GVariant *                      g_variant_lookup_value                  (GVariant             *dictionary,
                                                                         const gchar          *key);
gboolean                        g_variant_lookup                        (GVariant             *dictionary,
                                                                         const gchar          *key,
                                                                         const gchar          *format_string,
                                                                         ...);

/* GVariantIter */
gsize                           g_variant_iter_init                     (GVariantIter         *iter,
                                                                         GVariant             *value);
GVariant *                      g_variant_iter_next_value               (GVariantIter         *iter);
void                            g_variant_iter_cancel                   (GVariantIter         *iter);
gboolean                        g_variant_iter_was_cancelled            (GVariantIter         *iter);
gboolean                        g_variant_iter_next                     (GVariantIter         *iter,
                                                                         const gchar          *format_string,
                                                                         ...);

/* GVariantBuilder */
void                            g_variant_builder_add_value             (GVariantBuilder      *builder,
                                                                         GVariant             *value);
void                            g_variant_builder_add                   (GVariantBuilder      *builder,
                                                                         const gchar          *format_string,
                                                                         ...);
GVariantBuilder *               g_variant_builder_open                  (GVariantBuilder      *parent,
                                                                         const GVariantType   *type);
GVariantBuilder *               g_variant_builder_close                 (GVariantBuilder      *child);
gboolean                        g_variant_builder_check_add             (GVariantBuilder      *builder,
                                                                         const GVariantType   *type,
                                                                         GError              **error);
gboolean                        g_variant_builder_check_end             (GVariantBuilder      *builder,
                                                                         GError              **error);
GVariantBuilder *               g_variant_builder_new                   (const GVariantType   *type);
GVariant *                      g_variant_builder_end                   (GVariantBuilder      *builder);
void                            g_variant_builder_cancel                (GVariantBuilder      *builder);

#define G_VARIANT_BUILDER_ERROR \
    (g_quark_from_static_string ("g-variant-builder-error-quark"))

typedef enum
{
  G_VARIANT_BUILDER_ERROR_TOO_MANY,
  G_VARIANT_BUILDER_ERROR_TOO_FEW,
  G_VARIANT_BUILDER_ERROR_INFER,
  G_VARIANT_BUILDER_ERROR_TYPE
} GVariantBuilderError;

/* text printing/parsing */
typedef struct
{
  const gchar *start;
  const gchar *end;
  gchar *error;
} GVariantParseError;

gchar *                         g_variant_print                         (GVariant             *value,
                                                                         gboolean              type_annotate);
GString *                       g_variant_print_string                  (GVariant             *value,
                                                                         GString              *string,
                                                                         gboolean              type_annotate);
GVariant *                      g_variant_parse                         (const gchar          *text,
                                                                         gint                  text_length,
                                                                         const GVariantType   *type,
                                                                         GError              **error);
GVariant *                      g_variant_parse_full                    (const gchar          *text,
                                                                         const gchar          *limit,
                                                                         const gchar         **endptr,
                                                                         const GVariantType   *type,
                                                                         GVariantParseError   *error);
GVariant *                      g_variant_new_parsed                    (const gchar          *format,
                                                                         ...);
GVariant *                      g_variant_new_parsed_va                 (const gchar          *format,
                                                                         va_list              *app);

/* markup printing/parsing */
gchar *                         g_variant_markup_print                  (GVariant             *value,
                                                                         gboolean              newlines,
                                                                         gint                  indentation,
                                                                         gint                  tabstop);
GString *                       g_variant_markup_print_string           (GVariant             *value,
                                                                         GString              *string,
                                                                         gboolean              newlines,
                                                                         gint                  indentation,
                                                                         gint                  tabstop);
void                            g_variant_markup_subparser_start        (GMarkupParseContext  *context,
                                                                         const GVariantType   *type);
GVariant *                      g_variant_markup_subparser_end          (GMarkupParseContext  *context,
                                                                         GError              **error);
GMarkupParseContext *           g_variant_markup_parse_context_new      (GMarkupParseFlags     flags,
                                                                         const GVariantType   *type);
GVariant *                      g_variant_markup_parse_context_end      (GMarkupParseContext  *context,
                                                                         GError              **error);
GVariant *                      g_variant_markup_parse                  (const gchar          *text,
                                                                         gssize                text_len,
                                                                         const GVariantType   *type,
                                                                         GError              **error);

/* load/store serialised format */
typedef enum
{
  G_VARIANT_LITTLE_ENDIAN       = G_LITTLE_ENDIAN,
  G_VARIANT_BIG_ENDIAN          = G_BIG_ENDIAN,
  G_VARIANT_TRUSTED             = 0x00010000,
  G_VARIANT_LAZY_BYTESWAP       = 0x00020000,
} GVariantFlags;

GVariant *                      g_variant_load                          (const GVariantType  *type,
                                                                         gconstpointer        data,
                                                                         gsize                size,
                                                                         GVariantFlags        flags);
GVariant *                      g_variant_from_slice                    (const GVariantType  *type,
                                                                         gpointer             slice,
                                                                         gsize                size,
                                                                         GVariantFlags        flags);
GVariant *                      g_variant_from_data                     (const GVariantType  *type,
                                                                         gconstpointer        data,
                                                                         gsize                size,
                                                                         GVariantFlags        flags,
                                                                         GDestroyNotify       notify,
                                                                         gpointer             user_data);
GVariant *                      g_variant_from_file                     (const GVariantType  *type,
                                                                         const gchar         *filename,
                                                                         GVariantFlags        flags,
                                                                         GError              **error);

void                            g_variant_store                         (GVariant            *value,
                                                                         gpointer             data);
gconstpointer                   g_variant_get_data                      (GVariant            *value);
gsize                           g_variant_get_size                      (GVariant            *value);

#define G_VARIANT_JUST ((gboolean *) "truetrue")

GVariantClass                   g_variant_classify                      (GVariant            *value);

#define g_variant_get_type_class g_variant_classify

G_END_DECLS

#endif /* __G_VARIANT_H__ */
