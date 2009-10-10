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

#include "tests.h"

/* all tests rely on a shared mainloop */
static GMainLoop *loop = NULL;

/* ---------------------------------------------------------------------------------------------------- */
/* Test marshalling on GDBusProxy (e.g. D-Bus <-> GType mapping) */
/* ---------------------------------------------------------------------------------------------------- */

static void
test_marshalling (GDBusConnection *connection,
                  const gchar     *name,
                  const gchar     *name_owner,
                  GDBusProxy      *proxy)
{
  GDBusProxy *frob;
  GError     *error;
  gboolean   ret;
  guint      n;
  gchar      v_byte;
  gboolean   v_boolean;
  gint       v_int16;
  guint      v_uint16;
  gint       v_int32;
  guint      v_uint32;
  gint64     v_int64;
  guint64    v_uint64;
  gdouble    v_double;
  gchar     *v_string;
  gchar     *v_object_path;
  gchar     *v_signature;
  GArray *ay;
  GArray *ab;
  GArray *an;
  GArray *aq;
  GArray *ai;
  GArray *au;
  GArray *ax;
  GArray *at;
  GArray *ad;
  GArray *v_ay;
  GArray *v_ab;
  GArray *v_an;
  GArray *v_aq;
  GArray *v_ai;
  GArray *v_au;
  GArray *v_ax;
  GArray *v_at;
  GArray *v_ad;
  gchar *as[] = {"bla", "baz", NULL};
  gchar *ao[] = {"/foo", "/bar", NULL};
  gchar *ag[] = {"ass", "git", NULL};
  gchar **v_as;
  gchar **v_ao;
  gchar **v_ag;
  GHashTable *hyy;
  GHashTable *hbb;
  GHashTable *hnn;
  GHashTable *hqq;
  GHashTable *hii;
  GHashTable *huu;
  GHashTable *hxx;
  GHashTable *htt;
  GHashTable *hdd;
  GHashTable *hss;
  GHashTable *hoo;
  GHashTable *hgg;
  gint64 v_int64_a = 16;
  gint64 v_int64_b = 17;
  guint64 v_uint64_a = 32;
  guint64 v_uint64_b = 33;
  gdouble v_double_a = 1.2;
  gdouble v_double_b = 2.1;
  GHashTable *v_hyy;
  GHashTable *v_hbb;
  GHashTable *v_hnn;
  GHashTable *v_hqq;
  GHashTable *v_hii;
  GHashTable *v_huu;
  GHashTable *v_hxx;
  GHashTable *v_htt;
  GHashTable *v_hdd;
  GHashTable *v_hss;
  GHashTable *v_hoo;
  GHashTable *v_hgg;
  GDBusStructure *pair;
  GDBusStructure *complex_struct;
  GDBusStructure *v_pair;
  GDBusStructure *v_complex_struct;
  GDBusStructure *other_struct;
  GPtrArray *array_of_pairs;
  GPtrArray *v_array_of_pairs;
  GDBusVariant *variant;
  GDBusVariant *v_variant;
  gchar *v_str0;
  gchar *v_str1;
  gchar *v_str2;
  gchar *v_str3;
  gchar *v_str4;
  gchar *v_str5;
  gchar *v_str6;
  gchar *v_str7;
  gchar *v_str8;

  error = NULL;

  frob = g_dbus_proxy_new_sync (connection,
                                G_DBUS_PROXY_FLAGS_NONE,
                                name_owner,
                                "/com/example/TestObject",
                                "com.example.Frob",
                                NULL,
                                &error);
  g_assert_no_error (error);
  g_assert (frob != NULL);

  ret = g_dbus_proxy_invoke_method_sync (frob,
                                         "HelloWorld",
                                         "s", "s",
                                         -1, NULL, &error,
                                         G_TYPE_STRING, "Hello World!", G_TYPE_INVALID,
                                         G_TYPE_STRING, &v_string, G_TYPE_INVALID);
  g_assert_no_error (error);
  g_assert (ret);
  g_assert_cmpstr (v_string, ==, "You greeted me with 'Hello World!'. Thanks!");
  g_free (v_string);

  /* test marshalling/demarshalling of primitive types */
  ret = g_dbus_proxy_invoke_method_sync (frob,
                                         "TestPrimitiveTypes",
                                         "ybnqiuxtdsog", "ybnqiuxtdsog",
                                         -1, NULL, &error,
                                         /* in values */
                                         G_TYPE_UCHAR, 1,
                                         G_TYPE_BOOLEAN, TRUE,
                                         G_TYPE_INT, 2,                  /* gint16 */
                                         G_TYPE_UINT, 3,                 /* guint16 */
                                         G_TYPE_INT, 4,
                                         G_TYPE_UINT, 5,
                                         G_TYPE_INT64, (gint64) 6,
                                         G_TYPE_UINT64, (guint64) 7,
                                         G_TYPE_DOUBLE, 8.1,
                                         G_TYPE_STRING, "a string",
                                         G_TYPE_STRING, "/foo",          /* an object path */
                                         G_TYPE_STRING, "ass",           /* a signature */
                                         G_TYPE_INVALID,
                                         /* out values */
                                         G_TYPE_UCHAR, &v_byte,
                                         G_TYPE_BOOLEAN, &v_boolean,
                                         G_TYPE_INT, &v_int16,           /* gint16 */
                                         G_TYPE_UINT, &v_uint16,         /* guint16 */
                                         G_TYPE_INT, &v_int32,
                                         G_TYPE_UINT, &v_uint32,
                                         G_TYPE_INT64, &v_int64,
                                         G_TYPE_UINT64, &v_uint64,
                                         G_TYPE_DOUBLE, &v_double,
                                         G_TYPE_STRING, &v_string,
                                         G_TYPE_STRING, &v_object_path,  /* an object path */
                                         G_TYPE_STRING, &v_signature,    /* a signature */
                                         G_TYPE_INVALID);
  g_assert_no_error (error);
  g_assert (ret);
  g_assert_cmpint   (v_byte, ==, 1 + 1);
  g_assert          (!v_boolean);
  g_assert_cmpint   (v_int16, ==, 2 + 1);
  g_assert_cmpint   (v_uint16, ==, 3 + 1);
  g_assert_cmpint   (v_int32, ==, 4 + 1);
  g_assert_cmpint   (v_uint32, ==, 5 + 1);
  g_assert_cmpint   (v_int64, ==, 6 + 1);
  g_assert_cmpint   (v_uint64, ==, 7 + 1);
  g_assert_cmpfloat (v_double, ==, -8.1 + 0.123);
  g_assert_cmpstr   (v_string, ==, "a stringa string");
  g_assert_cmpstr   (v_object_path, ==, "/foo/modified");
  g_assert_cmpstr   (v_signature, ==, "assass");
  g_free (v_string);
  g_free (v_object_path);
  g_free (v_signature);

  /* same test, but check that type coercion works */
  ret = g_dbus_proxy_invoke_method_sync (frob,
                                         "TestPrimitiveTypes",
                                         "ybnqiuxtdsog", "ybnqiuxtdsog",
                                         -1, NULL, &error,
                                         /* in values */
                                         G_TYPE_INT, 1,
                                         G_TYPE_INT, TRUE,
                                         G_TYPE_INT, 2,                  /* gint16 */
                                         G_TYPE_INT, 3,                 /* guint16 */
                                         G_TYPE_INT, 4,
                                         G_TYPE_INT, 5,
                                         G_TYPE_INT, 6,
                                         G_TYPE_INT, 7,
                                         G_TYPE_INT, 8,
                                         G_TYPE_STRING, "a string",
                                         G_TYPE_STRING, "/foo",          /* an object path */
                                         G_TYPE_STRING, "ass",           /* a signature */
                                         G_TYPE_INVALID,
                                         /* out values */
                                         G_TYPE_STRING, &v_str0,
                                         G_TYPE_STRING, &v_str1,
                                         G_TYPE_STRING, &v_str2,
                                         G_TYPE_STRING, &v_str3,
                                         G_TYPE_STRING, &v_str4,
                                         G_TYPE_STRING, &v_str5,
                                         G_TYPE_STRING, &v_str6,
                                         G_TYPE_STRING, &v_str7,
                                         G_TYPE_STRING, &v_str8,
                                         G_TYPE_STRING, &v_string,
                                         G_TYPE_STRING, &v_object_path,  /* an object path */
                                         G_TYPE_STRING, &v_signature,    /* a signature */
                                         G_TYPE_INVALID);
  g_assert_no_error (error);
  g_assert (ret);
  g_assert_cmpstr   (v_str0, ==, "2");
  g_assert_cmpstr   (v_str1, ==, "FALSE");
  g_assert_cmpstr   (v_str2, ==, "3");
  g_assert_cmpstr   (v_str3, ==, "4");
  g_assert_cmpstr   (v_str4, ==, "5");
  g_assert_cmpstr   (v_str5, ==, "6");
  g_assert_cmpstr   (v_str6, ==, "7");
  g_assert_cmpstr   (v_str7, ==, "8");
  g_assert_cmpstr   (v_str8, ==, "-7.877000");
  g_assert_cmpstr   (v_string, ==, "a stringa string");
  g_assert_cmpstr   (v_object_path, ==, "/foo/modified");
  g_assert_cmpstr   (v_signature, ==, "assass");
  g_free (v_str0);
  g_free (v_str1);
  g_free (v_str2);
  g_free (v_str3);
  g_free (v_str4);
  g_free (v_str5);
  g_free (v_str6);
  g_free (v_str7);
  g_free (v_str8);
  g_free (v_string);
  g_free (v_object_path);
  g_free (v_signature);

  /* test marshalling/demarhalling of arrays of primitive types */
  ay = g_array_new (FALSE, FALSE, sizeof (guchar)); g_array_set_size (ay, 1);     ((guchar *) ay->data)[0] = 1;
  ab = g_array_new (FALSE, FALSE, sizeof (gboolean)); g_array_set_size (ab, 1); ((gboolean *) ab->data)[0] = TRUE;
  an = g_array_new (FALSE, FALSE, sizeof (gint16)); g_array_set_size (an, 1);     ((gint16 *) an->data)[0] = 3;
  aq = g_array_new (FALSE, FALSE, sizeof (guint16)); g_array_set_size (aq, 1);   ((guint16 *) aq->data)[0] = 4;
  ai = g_array_new (FALSE, FALSE, sizeof (gint)); g_array_set_size (ai, 1);         ((gint *) ai->data)[0] = 5;
  au = g_array_new (FALSE, FALSE, sizeof (guint)); g_array_set_size (au, 1);       ((guint *) au->data)[0] = 6;
  ax = g_array_new (FALSE, FALSE, sizeof (gint64)); g_array_set_size (ax, 1);     ((gint64 *) ax->data)[0] = 7;
  at = g_array_new (FALSE, FALSE, sizeof (guint64)); g_array_set_size (at, 1);   ((guint64 *) at->data)[0] = 8;
  ad = g_array_new (FALSE, FALSE, sizeof (gdouble)); g_array_set_size (ad, 1);   ((gdouble *) ad->data)[0] = 8.2;
  ret = g_dbus_proxy_invoke_method_sync (frob,
                                         "TestArrayOfPrimitiveTypes",
                                         "ayabanaqaiauaxatad", "ayabanaqaiauaxatad",
                                         -1, NULL, &error,
                                         /* in values */
                                         G_TYPE_ARRAY, ay,
                                         G_TYPE_ARRAY, ab,
                                         G_TYPE_ARRAY, an,
                                         G_TYPE_ARRAY, aq,
                                         G_TYPE_ARRAY, ai,
                                         G_TYPE_ARRAY, au,
                                         G_TYPE_ARRAY, ax,
                                         G_TYPE_ARRAY, at,
                                         G_TYPE_ARRAY, ad,
                                         G_TYPE_INVALID,
                                         /* out values */
                                         G_TYPE_ARRAY, &v_ay,
                                         G_TYPE_ARRAY, &v_ab,
                                         G_TYPE_ARRAY, &v_an,
                                         G_TYPE_ARRAY, &v_aq,
                                         G_TYPE_ARRAY, &v_ai,
                                         G_TYPE_ARRAY, &v_au,
                                         G_TYPE_ARRAY, &v_ax,
                                         G_TYPE_ARRAY, &v_at,
                                         G_TYPE_ARRAY, &v_ad,
                                         G_TYPE_INVALID);
  g_assert_no_error (error);
  g_assert (ret);
#define CHECK_ARRAY(array, type, val) do {g_assert (array->len == 2 && g_array_get_element_size (array) == sizeof (type) && g_array_index (array, type, 0) == val && g_array_index (array, type, 1) == val);} while (FALSE)
  CHECK_ARRAY (v_ay, guchar,   1);
  CHECK_ARRAY (v_ab, gboolean, TRUE);
  CHECK_ARRAY (v_an, guint16,  3);
  CHECK_ARRAY (v_aq, gint16,   4);
  CHECK_ARRAY (v_ai, gint,     5);
  CHECK_ARRAY (v_au, guint,    6);
  CHECK_ARRAY (v_ax, gint64,   7);
  CHECK_ARRAY (v_at, guint64,  8);
  CHECK_ARRAY (v_ad, gdouble,  8.2);
  g_array_unref (v_ay);
  g_array_unref (v_ab);
  g_array_unref (v_an);
  g_array_unref (v_aq);
  g_array_unref (v_ai);
  g_array_unref (v_au);
  g_array_unref (v_ax);
  g_array_unref (v_at);
  g_array_unref (v_ad);

  /* test marshalling/demarshalling of array of string(ish) types */
  ret = g_dbus_proxy_invoke_method_sync (frob,
                                         "TestArrayOfStringTypes",
                                         "asaoag", "asaoag",
                                         -1, NULL, &error,
                                         /* in values */
                                         G_TYPE_STRV, as,
                                         G_TYPE_STRV, ao,
                                         G_TYPE_STRV, ag,
                                         G_TYPE_INVALID,
                                         /* out values */
                                         G_TYPE_STRV, &v_as,
                                         G_TYPE_STRV, &v_ao,
                                         G_TYPE_STRV, &v_ag,
                                         G_TYPE_INVALID);
  g_assert_no_error (error);
  g_assert (ret);
  g_assert_cmpint (g_strv_length (v_as), ==, 4);
  g_assert_cmpint (g_strv_length (v_ao), ==, 4);
  g_assert_cmpint (g_strv_length (v_ag), ==, 4);
  g_assert_cmpstr (v_as[0], ==, "bla");
  g_assert_cmpstr (v_as[2], ==, "bla");
  g_assert_cmpstr (v_as[1], ==, "baz");
  g_assert_cmpstr (v_as[3], ==, "baz");
  g_assert_cmpstr (v_ao[0], ==, "/foo");
  g_assert_cmpstr (v_ao[2], ==, "/foo");
  g_assert_cmpstr (v_ao[1], ==, "/bar");
  g_assert_cmpstr (v_ao[3], ==, "/bar");
  g_assert_cmpstr (v_ag[0], ==, "ass");
  g_assert_cmpstr (v_ag[2], ==, "ass");
  g_assert_cmpstr (v_ag[1], ==, "git");
  g_assert_cmpstr (v_ag[3], ==, "git");
  g_strfreev (v_as);
  g_strfreev (v_ao);
  g_strfreev (v_ag);

  /* test marshalling/demarshalling of hash tables with primitive types */
  hyy = g_hash_table_new (g_direct_hash, g_direct_equal);
  hbb = g_hash_table_new (g_direct_hash, g_direct_equal);
  hnn = g_hash_table_new (g_direct_hash, g_direct_equal);
  hqq = g_hash_table_new (g_direct_hash, g_direct_equal);
  hii = g_hash_table_new (g_direct_hash, g_direct_equal);
  huu = g_hash_table_new (g_direct_hash, g_direct_equal);
  hxx = g_hash_table_new (g_int64_hash, g_int64_equal);
  htt = g_hash_table_new (g_int64_hash, g_int64_equal);
  hdd = g_hash_table_new (g_double_hash, g_double_equal);
  hss = g_hash_table_new (g_str_hash, g_str_equal);
  hoo = g_hash_table_new (g_str_hash, g_str_equal);
  hgg = g_hash_table_new (g_str_hash, g_str_equal);
  g_hash_table_insert (hyy, GINT_TO_POINTER (1), GINT_TO_POINTER (1));
  g_hash_table_insert (hbb, GINT_TO_POINTER (TRUE), GINT_TO_POINTER (FALSE));
  g_hash_table_insert (hnn, GINT_TO_POINTER (2), GINT_TO_POINTER (2));
  g_hash_table_insert (hqq, GINT_TO_POINTER (3), GINT_TO_POINTER (3));
  g_hash_table_insert (hii, GINT_TO_POINTER (4), GINT_TO_POINTER (4));
  g_hash_table_insert (huu, GINT_TO_POINTER (5), GINT_TO_POINTER (5));
  g_hash_table_insert (hxx, &v_int64_a, &v_int64_b);
  g_hash_table_insert (htt, &v_uint64_a, &v_uint64_b);
  g_hash_table_insert (hdd, &v_double_a, &v_double_b);
  g_hash_table_insert (hss, "foo", "bar");
  g_hash_table_insert (hss, "frak", "frack");
  g_hash_table_insert (hoo, "/foo", "/bar");
  g_hash_table_insert (hgg, "g", "sad");
  ret = g_dbus_proxy_invoke_method_sync (frob,
                                         "TestHashTables",
                                         "a{yy}a{bb}a{nn}a{qq}a{ii}a{uu}a{xx}a{tt}a{dd}a{ss}a{oo}a{gg}",
                                         "a{yy}a{bb}a{nn}a{qq}a{ii}a{uu}a{xx}a{tt}a{dd}a{ss}a{oo}a{gg}",
                                         -1, NULL, &error,
                                         /* in values */
                                         G_TYPE_HASH_TABLE, hyy,
                                         G_TYPE_HASH_TABLE, hbb,
                                         G_TYPE_HASH_TABLE, hnn,
                                         G_TYPE_HASH_TABLE, hqq,
                                         G_TYPE_HASH_TABLE, hii,
                                         G_TYPE_HASH_TABLE, huu,
                                         G_TYPE_HASH_TABLE, hxx,
                                         G_TYPE_HASH_TABLE, htt,
                                         G_TYPE_HASH_TABLE, hdd,
                                         G_TYPE_HASH_TABLE, hss,
                                         G_TYPE_HASH_TABLE, hoo,
                                         G_TYPE_HASH_TABLE, hgg,
                                         G_TYPE_INVALID,
                                         /* out values */
                                         G_TYPE_HASH_TABLE, &v_hyy,
                                         G_TYPE_HASH_TABLE, &v_hbb,
                                         G_TYPE_HASH_TABLE, &v_hnn,
                                         G_TYPE_HASH_TABLE, &v_hqq,
                                         G_TYPE_HASH_TABLE, &v_hii,
                                         G_TYPE_HASH_TABLE, &v_huu,
                                         G_TYPE_HASH_TABLE, &v_hxx,
                                         G_TYPE_HASH_TABLE, &v_htt,
                                         G_TYPE_HASH_TABLE, &v_hdd,
                                         G_TYPE_HASH_TABLE, &v_hss,
                                         G_TYPE_HASH_TABLE, &v_hoo,
                                         G_TYPE_HASH_TABLE, &v_hgg,
                                         G_TYPE_INVALID);
  g_assert_no_error (error);
  g_assert (ret);
  g_assert_cmpint (GPOINTER_TO_INT (g_hash_table_lookup (v_hyy, GINT_TO_POINTER (1 * 2))), ==, 1 * 3);
  g_assert_cmpint (GPOINTER_TO_INT (g_hash_table_lookup (v_hbb, GINT_TO_POINTER (TRUE))),  ==, TRUE);
  g_assert_cmpint (GPOINTER_TO_INT (g_hash_table_lookup (v_hnn, GINT_TO_POINTER (2 * 2))), ==, 2 * 3);
  g_assert_cmpint (GPOINTER_TO_INT (g_hash_table_lookup (v_hqq, GINT_TO_POINTER (3 * 2))), ==, 3 * 3);
  g_assert_cmpint (GPOINTER_TO_INT (g_hash_table_lookup (v_hii, GINT_TO_POINTER (4 * 2))), ==, 4 * 3);
  g_assert_cmpint (GPOINTER_TO_INT (g_hash_table_lookup (v_huu, GINT_TO_POINTER (5 * 2))), ==, 5 * 3);
  v_int64_a += 2; v_int64_b += 1;
  g_assert_cmpint (*((gint64 *) g_hash_table_lookup (v_hxx, &v_int64_a)), ==, v_int64_b);
  v_uint64_a += 2; v_uint64_b += 1;
  g_assert_cmpint (*((guint64 *) g_hash_table_lookup (v_htt, &v_uint64_a)), ==, v_uint64_b);
  v_double_a += 2.5; v_double_b += 5.0;
  g_assert_cmpint (*((gdouble *) g_hash_table_lookup (v_hdd, &v_double_a)), ==, v_double_b);
  g_assert_cmpstr (g_hash_table_lookup (v_hss, "foomod"), ==, "barbar");
  g_assert_cmpstr (g_hash_table_lookup (v_hss, "frakmod"), ==, "frackfrack");
  g_assert_cmpstr (g_hash_table_lookup (v_hoo, "/foo/mod"), ==, "/bar/mod2");
  g_assert_cmpstr (g_hash_table_lookup (v_hgg, "gassgit"), ==, "sadsad");
  g_hash_table_unref (v_hyy);
  g_hash_table_unref (v_hbb);
  g_hash_table_unref (v_hnn);
  g_hash_table_unref (v_hqq);
  g_hash_table_unref (v_hii);
  g_hash_table_unref (v_huu);
  g_hash_table_unref (v_hxx);
  g_hash_table_unref (v_htt);
  g_hash_table_unref (v_hdd);
  g_hash_table_unref (v_hss);
  g_hash_table_unref (v_hoo);
  g_hash_table_unref (v_hgg);

  /* test marshalling/demarshalling of structs */
  pair = g_dbus_structure_new ("(ii)",
                               G_TYPE_INT, 42,
                               G_TYPE_INT, 43,
                               G_TYPE_INVALID);
  complex_struct = g_dbus_structure_new ("(s(ii)aya{ss})",
                                         G_TYPE_STRING, "The time is right to make new friends",
                                         G_TYPE_DBUS_STRUCTURE, pair,
                                         G_TYPE_ARRAY, ay,
                                         G_TYPE_HASH_TABLE, hss,
                                         G_TYPE_INVALID);
  ret = g_dbus_proxy_invoke_method_sync (frob,
                                         "TestStructureTypes",
                                         "(ii)(s(ii)aya{ss})", "(ii)(s(ii)aya{ss})",
                                         -1, NULL, &error,
                                         /* in values */
                                         G_TYPE_DBUS_STRUCTURE, pair,
                                         G_TYPE_DBUS_STRUCTURE, complex_struct,
                                         G_TYPE_INVALID,
                                         /* out values */
                                         G_TYPE_DBUS_STRUCTURE, &v_pair,
                                         G_TYPE_DBUS_STRUCTURE, &v_complex_struct,
                                         G_TYPE_INVALID);
  g_assert_no_error (error);
  g_assert (ret);
  g_dbus_structure_get_element (v_pair,
                                0, &v_int32,
                                1, &v_uint32,
                                -1);
  g_assert_cmpint (v_int32, ==, 42 + 1);
  g_assert_cmpint (v_uint32, ==, 43 + 1);
  g_dbus_structure_get_element (v_complex_struct,
                                0, &v_string,
                                1, &other_struct,
                                2, &v_ay,
                                3, &v_hss,
                                -1);
  g_assert_cmpstr (v_string, ==, "The time is right to make new friends ... in bed!"); /* lame chinese fortune joke */
  g_dbus_structure_get_element (other_struct,
                                0, &v_int32,
                                1, &v_uint32,
                                -1);
  g_assert_cmpint (v_int32, ==, 42 + 2);
  g_assert_cmpint (v_uint32, ==, 43 + 2);
  CHECK_ARRAY (v_ay, guchar,   1);
  g_assert_cmpstr (g_hash_table_lookup (v_hss, "foo"), ==, "bar ... in bed!");
  g_assert_cmpstr (g_hash_table_lookup (v_hss, "frak"), ==, "frack ... in bed!");
  g_assert (ret);
  g_object_unref (v_pair);
  g_object_unref (v_complex_struct);

  /* test marshalling/demarshalling of complex arrays */
  GPtrArray *array_of_array_of_pairs;
  GPtrArray *v_array_of_array_of_pairs;
  GPtrArray *array_of_array_of_strings;
  GPtrArray *v_array_of_array_of_strings;
  GPtrArray *array_of_hashes;
  GPtrArray *v_array_of_hashes;
  GPtrArray *array_of_ay;
  GPtrArray *v_array_of_ay;
  GPtrArray *av;
  GPtrArray *v_av;
  GPtrArray *aav;
  GPtrArray *v_aav;
  array_of_pairs = g_ptr_array_new ();
  g_ptr_array_add (array_of_pairs, pair);
  g_ptr_array_add (array_of_pairs, pair);
  array_of_array_of_pairs = g_ptr_array_new ();
  g_ptr_array_add (array_of_array_of_pairs, array_of_pairs);
  g_ptr_array_add (array_of_array_of_pairs, array_of_pairs);
  g_ptr_array_add (array_of_array_of_pairs, array_of_pairs);
  array_of_array_of_strings = g_ptr_array_new ();
  g_ptr_array_add (array_of_array_of_strings, as);
  array_of_hashes = g_ptr_array_new ();
  g_ptr_array_add (array_of_hashes, hss);
  g_ptr_array_add (array_of_hashes, hss);
  g_ptr_array_add (array_of_hashes, hss);
  g_ptr_array_add (array_of_hashes, hss);
  array_of_ay = g_ptr_array_new ();
  g_ptr_array_add (array_of_ay, ay);
  g_ptr_array_add (array_of_ay, ay);
  g_ptr_array_add (array_of_ay, ay);
  GDBusVariant *variant1;
  GDBusVariant *variant2;
  variant1 = g_dbus_variant_new_for_signature ("assgit");
  variant2 = g_dbus_variant_new_for_structure (pair);
  av = g_ptr_array_new ();
  g_ptr_array_add (av, variant1);
  g_ptr_array_add (av, variant2);
  g_ptr_array_add (av, variant1);
  aav = g_ptr_array_new ();
  g_ptr_array_add (aav, av);
  g_ptr_array_add (aav, av);
  g_ptr_array_add (aav, av);
  g_ptr_array_add (aav, av);
  g_ptr_array_add (aav, av);
  ret = g_dbus_proxy_invoke_method_sync (frob,
                                         "TestComplexArrays",
                                         "a(ii)aa(ii)aasaa{ss}aayavaav", "a(ii)aa(ii)aasaa{ss}aayavaav",
                                         -1, NULL, &error,
                                         /* in values */
                                         G_TYPE_PTR_ARRAY, array_of_pairs,
                                         G_TYPE_PTR_ARRAY, array_of_array_of_pairs,
                                         G_TYPE_PTR_ARRAY, array_of_array_of_strings,
                                         G_TYPE_PTR_ARRAY, array_of_hashes,
                                         G_TYPE_PTR_ARRAY, array_of_ay,
                                         G_TYPE_PTR_ARRAY, av,
                                         G_TYPE_PTR_ARRAY, aav,
                                         G_TYPE_INVALID,
                                         /* out values */
                                         G_TYPE_PTR_ARRAY, &v_array_of_pairs,
                                         G_TYPE_PTR_ARRAY, &v_array_of_array_of_pairs,
                                         G_TYPE_PTR_ARRAY, &v_array_of_array_of_strings,
                                         G_TYPE_PTR_ARRAY, &v_array_of_hashes,
                                         G_TYPE_PTR_ARRAY, &v_array_of_ay,
                                         G_TYPE_PTR_ARRAY, &v_av,
                                         G_TYPE_PTR_ARRAY, &v_aav,
                                         G_TYPE_INVALID);
  g_assert_no_error (error);
  g_assert (ret);
  g_assert_cmpint (v_array_of_pairs->len, ==, 4);
  g_assert_cmpint (v_array_of_array_of_pairs->len, ==, 6);
  g_assert_cmpint (v_array_of_array_of_strings->len, ==, 2);
  g_assert_cmpint (v_array_of_hashes->len, ==, 8);
  g_assert_cmpint (v_array_of_ay->len, ==, 6);
  g_assert_cmpint (v_av->len, ==, 6);
  g_assert_cmpint (v_aav->len, ==, 10);
  g_ptr_array_unref (v_array_of_pairs);
  g_ptr_array_unref (v_array_of_array_of_pairs);
  g_ptr_array_unref (v_array_of_array_of_strings);
  g_ptr_array_unref (v_array_of_hashes);
  g_ptr_array_unref (v_array_of_ay);
  g_ptr_array_unref (v_av);
  g_ptr_array_unref (v_aav);

  /* test hash tables with non-primitive types */
  GHashTable *h_str_to_pair;
  GHashTable *v_h_str_to_pair;
  GHashTable *h_str_to_variant;
  GHashTable *v_h_str_to_variant;
  GHashTable *h_str_to_av;
  GHashTable *v_h_str_to_av;
  GHashTable *h_str_to_aav;
  GHashTable *v_h_str_to_aav;
  GHashTable *h_str_to_array_of_pairs;
  GHashTable *v_h_str_to_array_of_pairs;
  GHashTable *hash_of_hashes;
  GHashTable *v_hash_of_hashes;
  h_str_to_pair = g_hash_table_new (g_str_hash, g_str_equal);
  g_hash_table_insert (h_str_to_pair, "first", pair);
  g_hash_table_insert (h_str_to_pair, "second", pair);
  h_str_to_variant = g_hash_table_new (g_str_hash, g_str_equal);
  g_hash_table_insert (h_str_to_variant, "1st", variant1);
  g_hash_table_insert (h_str_to_variant, "2nd", variant2);
  h_str_to_av = g_hash_table_new (g_str_hash, g_str_equal);
  g_hash_table_insert (h_str_to_av, "1", av);
  g_hash_table_insert (h_str_to_av, "2", av);
  h_str_to_aav = g_hash_table_new (g_str_hash, g_str_equal);
  g_hash_table_insert (h_str_to_aav, "1", aav);
  g_hash_table_insert (h_str_to_aav, "2", aav);
  h_str_to_array_of_pairs = g_hash_table_new (g_str_hash, g_str_equal);
  g_hash_table_insert (h_str_to_array_of_pairs, "1", array_of_pairs);
  g_hash_table_insert (h_str_to_array_of_pairs, "2", array_of_pairs);
  hash_of_hashes = g_hash_table_new (g_str_hash, g_str_equal);
  g_hash_table_insert (hash_of_hashes, "1", hss);
  g_hash_table_insert (hash_of_hashes, "2", hss);
  ret = g_dbus_proxy_invoke_method_sync (frob,
                                         "TestComplexHashTables",
                                         "a{s(ii)}a{sv}a{sav}a{saav}a{sa(ii)}a{sa{ss}}",
                                         "a{s(ii)}a{sv}a{sav}a{saav}a{sa(ii)}a{sa{ss}}",
                                         -1, NULL, &error,
                                         /* in values */
                                         G_TYPE_HASH_TABLE, h_str_to_pair,
                                         G_TYPE_HASH_TABLE, h_str_to_variant,
                                         G_TYPE_HASH_TABLE, h_str_to_av,
                                         G_TYPE_HASH_TABLE, h_str_to_aav,
                                         G_TYPE_HASH_TABLE, h_str_to_array_of_pairs,
                                         G_TYPE_HASH_TABLE, hash_of_hashes,
                                         G_TYPE_INVALID,
                                         /* out values */
                                         G_TYPE_HASH_TABLE, &v_h_str_to_pair,
                                         G_TYPE_HASH_TABLE, &v_h_str_to_variant,
                                         G_TYPE_HASH_TABLE, &v_h_str_to_av,
                                         G_TYPE_HASH_TABLE, &v_h_str_to_aav,
                                         G_TYPE_HASH_TABLE, &v_h_str_to_array_of_pairs,
                                         G_TYPE_HASH_TABLE, &v_hash_of_hashes,
                                         G_TYPE_INVALID);
  g_assert_no_error (error);
  g_assert (ret);
  g_assert ((v_pair = g_hash_table_lookup (v_h_str_to_pair, "first_baz")) != NULL);
  g_assert (g_dbus_structure_equal (pair, v_pair));
  g_assert ((v_pair = g_hash_table_lookup (v_h_str_to_pair, "second_baz")) != NULL);
  g_assert (g_dbus_structure_equal (pair, v_pair));
  g_hash_table_unref (v_h_str_to_pair);
  g_assert ((v_variant = g_hash_table_lookup (v_h_str_to_variant, "1st_baz")) != NULL);
  g_assert_cmpstr (g_dbus_variant_get_signature (v_variant), ==, "assgit");
  g_assert ((v_variant = g_hash_table_lookup (v_h_str_to_variant, "2nd_baz")) != NULL);
  g_assert (g_dbus_structure_equal (pair, g_dbus_variant_get_structure (v_variant)));
  g_hash_table_unref (v_h_str_to_variant);
  g_assert_cmpint (g_hash_table_size (h_str_to_av), ==, g_hash_table_size (v_h_str_to_av));
  g_hash_table_unref (v_h_str_to_av);
  g_assert_cmpint (g_hash_table_size (h_str_to_aav), ==, g_hash_table_size (v_h_str_to_aav));
  g_hash_table_unref (v_h_str_to_aav);
  g_assert_cmpint (g_hash_table_size (h_str_to_array_of_pairs), ==, g_hash_table_size (v_h_str_to_array_of_pairs));
  g_hash_table_unref (v_h_str_to_array_of_pairs);
  g_assert_cmpint (g_hash_table_size (hash_of_hashes), ==, g_hash_table_size (v_hash_of_hashes));
  g_hash_table_unref (v_hash_of_hashes);

  g_hash_table_unref (h_str_to_av);
  g_hash_table_unref (h_str_to_aav);
  g_hash_table_unref (h_str_to_array_of_pairs);
  g_hash_table_unref (hash_of_hashes);

  /* test marshalling/demarshalling of variants (this also tests various _equal() funcs) */
  for (n = 0; ; n++)
    {
      variant = NULL;
      switch (n)
        {
        case 0:
          variant = g_dbus_variant_new_for_string ("frak");
          break;
        case 1:
          variant = g_dbus_variant_new_for_object_path ("/blah");
          break;
        case 2:
          variant = g_dbus_variant_new_for_signature ("assgit");
          break;
        case 3:
          variant = g_dbus_variant_new_for_string_array (as);
          break;
        case 4:
          variant = g_dbus_variant_new_for_object_path_array (ao);
          break;
        case 5:
          variant = g_dbus_variant_new_for_signature_array (ag);
          break;
        case 6:
          variant = g_dbus_variant_new_for_byte (1);
          break;
        case 7:
          variant = g_dbus_variant_new_for_int16 (2);
          break;
        case 8:
          variant = g_dbus_variant_new_for_uint16 (3);
          break;
        case 9:
          variant = g_dbus_variant_new_for_int (4);
          break;
        case 10:
          variant = g_dbus_variant_new_for_uint (5);
          break;
        case 11:
          variant = g_dbus_variant_new_for_int64 (6);
          break;
        case 12:
          variant = g_dbus_variant_new_for_uint64 (7);
          break;
        case 13:
          variant = g_dbus_variant_new_for_boolean (TRUE);
          break;
        case 14:
          variant = g_dbus_variant_new_for_double (8.1);
          break;
        case 15:
          variant = g_dbus_variant_new_for_array (ai, "i");
          break;
        case 16:
          variant = g_dbus_variant_new_for_hash_table (hii, "i", "i");
          break;
        case 17:
          variant = g_dbus_variant_new_for_hash_table (hoo, "o", "o");
          break;
        case 18:
          variant = g_dbus_variant_new_for_structure (complex_struct);
          break;
        case 19:
          variant = g_dbus_variant_new_for_ptr_array (array_of_pairs, "(ii)");
          break;

        default:
          break;
        }

      if (variant == NULL)
        break;

      /* first make the service returns the identical variant - this also tests
       * that all the _equal() functions work as intended
       */
      ret = g_dbus_proxy_invoke_method_sync (frob,
                                             "TestVariant",
                                             "vb", "v",
                                             -1, NULL, &error,
                                             /* in values */
                                             G_TYPE_DBUS_VARIANT, variant,
                                             G_TYPE_BOOLEAN, FALSE,
                                             G_TYPE_INVALID,
                                             /* out values */
                                             G_TYPE_DBUS_VARIANT, &v_variant,
                                             G_TYPE_INVALID);
      g_assert_no_error (error);
      g_assert (ret);
      g_assert (g_dbus_variant_equal (variant, v_variant));
      g_object_unref (v_variant);

      /* this make the service modify the variant */
      ret = g_dbus_proxy_invoke_method_sync (frob,
                                             "TestVariant",
                                             "vb", "v",
                                             -1, NULL, &error,
                                             /* in values */
                                             G_TYPE_DBUS_VARIANT, variant,
                                             G_TYPE_BOOLEAN, TRUE,
                                             G_TYPE_INVALID,
                                             /* out values */
                                             G_TYPE_DBUS_VARIANT, &v_variant,
                                             G_TYPE_INVALID);
      g_assert (!g_dbus_variant_equal (variant, v_variant));
      g_assert_no_error (error);
      g_assert (ret);
      switch (n)
        {
        case 0:
          g_assert_cmpstr (g_dbus_variant_get_string (v_variant), ==, "frakfrak");
          break;
        case 1:
          g_assert_cmpstr (g_dbus_variant_get_object_path (v_variant), ==, "/blah/blah");
          break;
        case 2:
          g_assert_cmpstr (g_dbus_variant_get_signature (v_variant), ==, "assgitassgit");
          break;
        case 3:
          v_as = g_dbus_variant_get_string_array (v_variant);
          g_assert_cmpint (g_strv_length (v_as), ==, 4);
          g_assert_cmpstr (v_as[0], ==, "bla");
          g_assert_cmpstr (v_as[2], ==, "bla");
          g_assert_cmpstr (v_as[1], ==, "baz");
          g_assert_cmpstr (v_as[3], ==, "baz");
          break;
        case 4:
          v_ao = g_dbus_variant_get_object_path_array (v_variant);
          g_assert_cmpint (g_strv_length (v_ao), ==, 4);
          g_assert_cmpstr (v_ao[0], ==, "/foo");
          g_assert_cmpstr (v_ao[2], ==, "/foo");
          g_assert_cmpstr (v_ao[1], ==, "/bar");
          g_assert_cmpstr (v_ao[3], ==, "/bar");
          break;
        case 5:
          v_ag = g_dbus_variant_get_signature_array (v_variant);
          g_assert_cmpint (g_strv_length (v_ag), ==, 4);
          g_assert_cmpstr (v_ag[0], ==, "ass");
          g_assert_cmpstr (v_ag[2], ==, "ass");
          g_assert_cmpstr (v_ag[1], ==, "git");
          g_assert_cmpstr (v_ag[3], ==, "git");
          break;
        case 6:
          g_assert_cmpint (g_dbus_variant_get_byte (v_variant), ==, g_dbus_variant_get_byte (variant) * 2);
          break;
        case 7:
          g_assert_cmpint (g_dbus_variant_get_int16 (v_variant), ==, g_dbus_variant_get_int16 (variant) * 2);
          break;
        case 8:
          g_assert_cmpint (g_dbus_variant_get_uint16 (v_variant), ==, g_dbus_variant_get_uint16 (variant) * 2);
          break;
        case 9:
          g_assert_cmpint (g_dbus_variant_get_int (v_variant), ==, g_dbus_variant_get_int (variant) * 2);
          break;
        case 10:
          g_assert_cmpint (g_dbus_variant_get_uint (v_variant), ==, g_dbus_variant_get_uint (variant) * 2);
          break;
        case 11:
          g_assert_cmpint (g_dbus_variant_get_int64 (v_variant), ==, g_dbus_variant_get_int64 (variant) * 2);
          break;
        case 12:
          g_assert_cmpint (g_dbus_variant_get_uint64 (v_variant), ==, g_dbus_variant_get_uint64 (variant) * 2);
          break;
        case 13:
          g_assert (!g_dbus_variant_get_boolean (v_variant));
          break;
        case 14:
          g_assert_cmpfloat (g_dbus_variant_get_double (v_variant), ==, g_dbus_variant_get_double (variant) * 2);
          break;
        case 15:
          v_ai = g_dbus_variant_get_array (v_variant);
          CHECK_ARRAY (v_ai, gint,     5);
          break;
        case 16:
          v_hii = g_dbus_variant_get_hash_table (v_variant);
          g_assert_cmpint (GPOINTER_TO_INT (g_hash_table_lookup (v_hii, GINT_TO_POINTER (4))), ==, 4 * 2);
          break;
        case 17:
          v_hoo = g_dbus_variant_get_hash_table (v_variant);
          g_assert_cmpstr (g_hash_table_lookup (v_hoo, "/foo"), ==, "/bar/bar");
          break;
        case 18:
          other_struct = g_dbus_variant_get_structure (v_variant);
          g_dbus_structure_get_element (other_struct,
                                        0, &v_string,
                                        1, &v_int16,
                                        -1);
          g_assert_cmpstr (v_string, ==, "other struct");
          g_assert_cmpint (v_int16, ==, 100);
          break;
        case 19:
          g_assert_cmpint (g_dbus_variant_get_ptr_array (v_variant)->len, ==, array_of_pairs->len * 2);
          break;
        default:
          g_assert_not_reached ();
          break;
        }
      g_object_unref (v_variant);

      g_object_unref (variant);
    }

  /* free all objects constructed for testing */
  g_array_unref (ay);
  g_array_unref (ab);
  g_array_unref (an);
  g_array_unref (aq);
  g_array_unref (ai);
  g_array_unref (au);
  g_array_unref (ax);
  g_array_unref (at);
  g_array_unref (ad);
  g_hash_table_unref (hyy);
  g_hash_table_unref (hbb);
  g_hash_table_unref (hnn);
  g_hash_table_unref (hqq);
  g_hash_table_unref (hii);
  g_hash_table_unref (huu);
  g_hash_table_unref (hxx);
  g_hash_table_unref (htt);
  g_hash_table_unref (hdd);
  g_hash_table_unref (hss);
  g_hash_table_unref (hoo);
  g_hash_table_unref (hgg);
  g_object_unref (pair);
  g_object_unref (complex_struct);
  g_ptr_array_unref (array_of_pairs);
  g_ptr_array_unref (array_of_array_of_pairs);
  g_hash_table_unref (h_str_to_pair);
  g_hash_table_unref (h_str_to_variant);
  g_object_unref (variant1);
  g_object_unref (variant2);
  g_ptr_array_unref (array_of_array_of_strings);
  g_ptr_array_unref (array_of_hashes);
  g_ptr_array_unref (array_of_ay);
  g_ptr_array_unref (av);
  g_ptr_array_unref (aav);

  g_object_unref (frob);
#undef CHECK_ARRAY
}


/* ---------------------------------------------------------------------------------------------------- */
/* Test that the property aspects of GDBusProxy works */
/* ---------------------------------------------------------------------------------------------------- */

static void
test_properties (GDBusConnection *connection,
                 const gchar     *name,
                 const gchar     *name_owner,
                 GDBusProxy      *proxy)
{
  GError *error;
  GDBusVariant *variant;
  GDBusVariant *variant2;
  gboolean ret;

  error = NULL;

  /**
   * Check that we can read cached properties.
   *
   * No need to test all properties - we've already tested GDBusVariant
   */
  variant = g_dbus_proxy_get_cached_property (proxy, "y", &error);
  g_assert_no_error (error);
  g_assert (variant != NULL);
  g_assert_cmpint (g_dbus_variant_get_byte (variant), ==, 1);
  g_object_unref (variant);
  variant = g_dbus_proxy_get_cached_property (proxy, "o", &error);
  g_assert_no_error (error);
  g_assert (variant != NULL);
  g_assert_cmpstr (g_dbus_variant_get_object_path (variant), ==, "/some/path");
  g_object_unref (variant);

  /**
   * Now ask the service to change a property and check that #GDBusProxy::g-dbus-proxy-property-changed
   * is received. Also check that the cache is updated.
   */
  variant2 = g_dbus_variant_new_for_byte (42);
  ret = g_dbus_proxy_invoke_method_sync (proxy, "FrobSetProperty", "sv", "",
                                         -1, NULL, &error,
                                         G_TYPE_STRING, "y",
                                         G_TYPE_DBUS_VARIANT, variant2,
                                         G_TYPE_INVALID,
                                         G_TYPE_INVALID);
  g_assert_no_error (error);
  g_assert (ret);
  g_object_unref (variant2);
  _g_assert_signal_received (proxy, "g-dbus-proxy-properties-changed");
  variant = g_dbus_proxy_get_cached_property (proxy, "y", &error);
  g_assert_no_error (error);
  g_assert (variant != NULL);
  g_assert_cmpint (g_dbus_variant_get_byte (variant), ==, 42);
  g_object_unref (variant);
}

/* ---------------------------------------------------------------------------------------------------- */
/* Test that the signal aspects of GDBusProxy works */
/* ---------------------------------------------------------------------------------------------------- */

static void
test_proxy_signals_on_signal (GDBusProxy  *proxy,
                              const gchar *signal_name,
                              const gchar *signature,
                              GPtrArray   *args,
                              gpointer     user_data)
{
  GString *s = user_data;
  GDBusVariant *variant;

  g_assert_cmpstr (signature, ==, "sov");
  g_assert_cmpint (args->len, ==, 3);
  g_assert (g_dbus_variant_is_string (G_DBUS_VARIANT (args->pdata[0])));
  g_assert (g_dbus_variant_is_object_path (G_DBUS_VARIANT (args->pdata[1])));
  g_assert (g_dbus_variant_is_variant (G_DBUS_VARIANT (args->pdata[2])));

  g_string_append (s, signal_name);
  g_string_append_c (s, ':');
  g_string_append (s, g_dbus_variant_get_string (G_DBUS_VARIANT (args->pdata[0])));
  g_string_append_c (s, ',');
  g_string_append (s, g_dbus_variant_get_object_path (G_DBUS_VARIANT (args->pdata[1])));
  g_string_append_c (s, ',');
  variant = g_dbus_variant_get_variant (G_DBUS_VARIANT (args->pdata[2]));
  g_assert (g_dbus_variant_is_string (variant));
  g_string_append (s, g_dbus_variant_get_string (variant));
}

typedef struct
{
  GMainLoop *internal_loop;
  GString *s;
} TestSignalData;

static void
test_proxy_signals_on_emit_signal_cb (GDBusProxy   *proxy,
                                      GAsyncResult *res,
                                      gpointer      user_data)
{
  TestSignalData *data = user_data;
  GError *error;
  gboolean ret;

  error = NULL;
  ret = g_dbus_proxy_invoke_method_finish (proxy,
                                           "",
                                           res,
                                           &error,
                                           G_TYPE_INVALID);
  g_assert_no_error (error);
  g_assert (ret);

  /* check that the signal was recieved before we got the method result */
  g_assert (strlen (data->s->str) > 0);

  /* break out of the loop */
  g_main_loop_quit (data->internal_loop);
}

static void
test_signals (GDBusConnection *connection,
              const gchar     *name,
              const gchar     *name_owner,
              GDBusProxy      *proxy)
{
  GError *error;
  gboolean ret;
  GString *s;
  gulong signal_handler_id;
  TestSignalData data;

  error = NULL;

  /**
   * Ask the service to emit a signal and check that we receive it.
   *
   * Note that blocking calls don't block in the mainloop so wait for the signal (which
   * is dispatched before the method reply)
   */
  s = g_string_new (NULL);
  signal_handler_id = g_signal_connect (proxy,
                                        "g-dbus-proxy-signal",
                                        G_CALLBACK (test_proxy_signals_on_signal),
                                        s);
  ret = g_dbus_proxy_invoke_method_sync (proxy, "EmitSignal", "so", "",
                                         -1, NULL, &error,
                                         G_TYPE_STRING, "Accept the next proposition you hear",
                                         G_TYPE_STRING, "/some/path",
                                         G_TYPE_INVALID,
                                         G_TYPE_INVALID);
  g_assert_no_error (error);
  g_assert (ret);
  /* check that we haven't received the signal just yet */
  g_assert (strlen (s->str) == 0);
  /* and now wait for the signal */
  _g_assert_signal_received (proxy, "g-dbus-proxy-signal");
  g_assert_cmpstr (s->str,
                   ==,
                   "TestSignal:Accept the next proposition you hear .. in bed!,/some/path/in/bed,a variant");
  g_signal_handler_disconnect (proxy, signal_handler_id);
  g_string_free (s, TRUE);

  /**
   * Now do this async to check the signal is received before the method returns.
   */
  s = g_string_new (NULL);
  data.internal_loop = g_main_loop_new (NULL, FALSE);
  data.s = s;
  signal_handler_id = g_signal_connect (proxy,
                                        "g-dbus-proxy-signal",
                                        G_CALLBACK (test_proxy_signals_on_signal),
                                        s);
  g_dbus_proxy_invoke_method (proxy, "EmitSignal", "so",
                              -1, NULL,
                              (GAsyncReadyCallback) test_proxy_signals_on_emit_signal_cb,
                              &data,
                              G_TYPE_STRING, "You will make a great programmer",
                              G_TYPE_STRING, "/some/other/path",
                              G_TYPE_INVALID);
  g_main_loop_run (data.internal_loop);
  g_main_loop_unref (data.internal_loop);
  g_assert_cmpstr (s->str,
                   ==,
                   "TestSignal:You will make a great programmer .. in bed!,/some/other/path/in/bed,a variant");
  g_signal_handler_disconnect (proxy, signal_handler_id);
  g_string_free (s, TRUE);
}

/* ---------------------------------------------------------------------------------------------------- */

static void
on_proxy_appeared (GDBusConnection *connection,
                   const gchar     *name,
                   const gchar     *name_owner,
                   GDBusProxy      *proxy,
                   gpointer         user_data)
{
  test_marshalling (connection, name, name_owner, proxy);
  test_properties (connection, name, name_owner, proxy);
  test_signals (connection, name, name_owner, proxy);

  g_main_loop_quit (loop);
}

static void
on_proxy_vanished (GDBusConnection *connection,
                   const gchar     *name,
                   gpointer         user_data)
{
}

static void
test_proxy (void)
{
  guint watcher_id;

  session_bus_up ();

  /* TODO: wait a bit for the bus to come up.. ideally session_bus_up() won't return
   * until one can connect to the bus but that's not how things work right now
   */
  usleep (500 * 1000);

  watcher_id = g_bus_watch_proxy (G_BUS_TYPE_SESSION,
                                  "com.example.TestService",
                                  "/com/example/TestObject",
                                  "com.example.Frob",
                                  G_TYPE_DBUS_PROXY,
                                  G_DBUS_PROXY_FLAGS_NONE,
                                  on_proxy_appeared,
                                  on_proxy_vanished,
                                  NULL);

  /* this is safe; testserver will exit once the bus goes away */
  g_assert (g_spawn_command_line_async ("./testserver.py", NULL));

  g_main_loop_run (loop);

  g_bus_unwatch_proxy (watcher_id);

  /* tear down bus */
  session_bus_down ();
}

/* ---------------------------------------------------------------------------------------------------- */

int
main (int   argc,
      char *argv[])
{
  g_type_init ();
  g_test_init (&argc, &argv, NULL);

  /* all the tests rely on a shared main loop */
  loop = g_main_loop_new (NULL, FALSE);

  /* all the tests use a session bus with a well-known address that we can bring up and down
   * using session_bus_up() and session_bus_down().
   */
  g_unsetenv ("DISPLAY");
  g_setenv ("DBUS_SESSION_BUS_ADDRESS", session_bus_get_temporary_address (), TRUE);

  g_test_add_func ("/gdbus/proxy", test_proxy);
  return g_test_run();
}
