/*
 * Copyright © 2007, 2008 Ryan Lortie
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

#include "gvariant-private.h"

#include <glib/gtestutils.h>
#include <glib/gmessages.h>
#include <glib/gstrfuncs.h>
#include <string.h>

/**
 * g_variant_format_string_scan:
 * @string: a string that may be prefixed with a format string
 * @limit: a pointer to the end of @string
 * @endptr: location to store the end pointer, or %NULL
 * @returns: %TRUE if there was a valid format string
 *
 * Checks the string pointed to by @string for starting with a properly
 * formed #GVariant varargs format string.  If no valid format string is
 * found then %FALSE is returned.
 *
 * If @string does start with a valid format string and @endptr is
 * non-%NULL then it is updated to point to the first character after
 * the format string.  If @endptr is %NULL and there is any character
 * following the format string then this call returns %FALSE.  Another
 * way of saying this is that if @endptr is %NULL then @string is
 * checked to contain exactly one valid format string and nothing else.
 *
 * If @limit is non-%NULL then @limit (and any charater after it) will
 * not be accessed and the effect is otherwise equivalent to if the
 * character at @limit were nul.
 *
 * All valid #GVariantType strings are also valid format strings.  See
 * g_variant_type_string_is_valid().
 *
 * Additionally, any type string contained in the format string may be
 * prefixed with a '@' character.  Nested '@' characters may not
 * appear.
 *
 * Additionally, any fixed-width type may be prefixed with a '&'
 * character.  No wildcard type is a fixed-width type.  Like '@', '&'
 * characters may not be nested.
 *
 * Additionally, any array of fixed-width types may be prefixed with a
 * '&'.
 *
 * Additionally, '&s', '^as' and '^a&s' may appear.
 *
 * No '@', '&' or '^' character, however, may appear as part of an array
 * type.
 *
 * Currently, there are no other permissible format strings.  Others
 * may be added in the future.
 *
 * For an explanation of what these strings mean, see g_variant_new()
 * and g_variant_get().
 **/
gboolean
g_variant_format_string_scan (const gchar  *string,
                              const gchar  *limit,
                              const gchar **endptr)
{
  const gchar *start;

  if (string == limit || *string == '\0')
    return FALSE;

  switch (*string++)
    {
    case '(':
      while (string == limit || *string != ')')
        if (!g_variant_format_string_scan (string, limit, &string))
          return FALSE;

      string++;
      break;

    case '{':
      if (string != limit && (*string == '@' || *string == '&'))
        string++;

      if (string == limit || *string == '\0' ||                    /* { */
          !strchr ("bynqihuxtdsog?", *string++) ||                  /* key */
          !g_variant_format_string_scan (string, limit, &string) ||/* value */
          string == limit || *string++ != '}')                     /* } */
        return FALSE;

      break;

    case 'm':
      return g_variant_format_string_scan (string, limit, endptr); /* tcall */

    case 'a':
    case '@':
      return g_variant_type_string_scan (string, limit, endptr);   /* tcall */

    case '&':
      start = string;

      if (!g_variant_type_string_scan (string, limit, &string))
        return FALSE;

      if (start + 1 == string && *start == 's')
        break;

      if (*start == 'a')
        start++;

      while (start != string)
        if (!strchr ("bynqihuxtd(){}", *start++))
          return FALSE;

      break;

    case '^':
      if (string == limit || *string++ != 'a')
        return FALSE;

      if (string != limit && *string == '&')
        string++;

      if (string == limit || *string++ != 's')
        return FALSE;

      break;

    case 'b': case 'y': case 'n': case 'q': case 'i': case 'u':
    case 'x': case 't': case 'd': case 's': case 'o': case 'g':
    case 'v': case '*': case '?': case 'r': case 'h':
      break;

    default:
      return FALSE;
  }

  if (endptr != NULL)
    *endptr = string;
  else
    {
      if (string != limit && *string != '\0')
        return FALSE;
    }

  return TRUE;
}

/**
 * g_variant_format_string_scan_type:
 * @string: a string that may be prefixed with a format string
 * @limit: a pointer to the end of @string
 * @endptr: location to store the end pointer, or %NULL
 * @returns: a #GVariantType if there was a valid format string
 *
 * If @string starts with a valid format string then this function will
 * return the type that the format string corresponds to.  Otherwise
 * this function returns %NULL.
 *
 * The returned type must be freed by the caller.
 *
 * This function is otherwise exactly like
 * g_variant_format_string_scan().
 **/
GVariantType *
g_variant_format_string_scan_type (const gchar  *string,
                                   const gchar  *limit,
                                   const gchar **endptr)
{
  const gchar *my_end;
  gchar *dest;
  gchar *new;

  if (endptr == NULL)
    endptr = &my_end;

  if (!g_variant_format_string_scan (string, limit, endptr))
    return NULL;

  dest = new = g_malloc (*endptr - string + 1);
  while (string != *endptr)
    {
      if (*string != '@' && *string != '&' && *string != '^')
        *dest++ = *string;
      string++;
    }
  *dest = '\0';

  return (GVariantType *) G_VARIANT_TYPE (new);
}

static GVariant *
g_variant_valist_new (const gchar **format_string,
                      va_list      *app)
{
  switch (**format_string)
  {
    case 'b':
      (*format_string)++;
      return g_variant_new_boolean (va_arg (*app, gboolean));

    case 'y':
      (*format_string)++;
      return g_variant_new_byte (va_arg (*app, guint));

    case 'n':
      (*format_string)++;
      return g_variant_new_int16 (va_arg (*app, gint));

    case 'q':
      (*format_string)++;
      return g_variant_new_uint16 (va_arg (*app, guint));

    case 'i':
      (*format_string)++;
      return g_variant_new_int32 (va_arg (*app, gint));

    case 'h':
      (*format_string)++;
      return g_variant_new_handle (va_arg (*app, gint));

    case 'u':
      (*format_string)++;
      return g_variant_new_uint32 (va_arg (*app, guint));

    case 'x':
      (*format_string)++;
      return g_variant_new_int64 (va_arg (*app, gint64));

    case 't':
      (*format_string)++;
      return g_variant_new_uint64 (va_arg (*app, guint64));

    case 'd':
      (*format_string)++;
      return g_variant_new_double (va_arg (*app, gdouble));

    case 's':
      (*format_string)++;
      return g_variant_new_string (va_arg (*app, const gchar *));

    case 'o':
      (*format_string)++;
      return g_variant_new_object_path (va_arg (*app, const gchar *));

    case 'g':
      (*format_string)++;
      return g_variant_new_signature (va_arg (*app, const gchar *));

    case 'v':
      (*format_string)++;
      return g_variant_new_variant (va_arg (*app, GVariant *));

    case '@':
    case '*':
    case '?':
    case 'r':
      g_variant_format_string_scan (*format_string, NULL, format_string);
      return va_arg (*app, GVariant *);

    case '^':
      {
        const gchar *string = (*format_string + 1);

        if (string[0] == 'a' &&
            (string[1] == 's' ||
             (string[1] == '&' && string[2] == 's')))
          {
            const gchar * const *ptr;

            *format_string += 3 + (string[1] == '&');
            ptr = va_arg (*app, const gchar * const *);

            return g_variant_new_strv (ptr, -1);
          }

        g_error ("Currently, only ^as and ^a&s are supported");
      }

    case '&':
      {
        const gchar *string = (*format_string + 1);
        gconstpointer ptr;
        gint n_items = 0;

        ptr = va_arg (*app, gconstpointer);

        switch (*string++)
          {
          case 's': /* '&s' */
            /* for building, just the same as normal 's' */
            *format_string += 2;
            return g_variant_new_string (ptr);

          case 'a':
            n_items = va_arg (*app, gint);

            if (n_items < 0)
              g_error ("size of -1 can only be specified for string arrays");

            /* fall through */

          default:
            {
              GVariantType *type;
              GVariant *value;

              type = g_variant_format_string_scan_type (*format_string,
                                                        NULL, format_string);
              g_assert (g_variant_type_is_definite (type));

              value = g_variant_load_fixed (type, ptr, n_items);
              g_variant_type_free (type);

              return value;
            }
          }
      }

    case 'a':
      g_variant_format_string_scan (*format_string, NULL, format_string);
      return g_variant_builder_end (va_arg (*app, GVariantBuilder *));

    case 'm':
      {
        GVariantBuilder *builder;
        const gchar *string;
        GVariantType *type;
        GVariant *value;

        string = (*format_string) + 1;
        type = g_variant_format_string_scan_type (*format_string,
                                                  NULL, format_string);
        builder = g_variant_builder_new (type);
        g_variant_type_free (type);

        switch (*string)
        {
          case 's':
            if ((string = va_arg (*app, const gchar *)))
              g_variant_builder_add_value (builder,
                                           g_variant_new_string (string));
            break;

          case 'o':
            if ((string = va_arg (*app, const gchar *)))
              g_variant_builder_add_value (builder,
                                           g_variant_new_object_path (string));
            break;

          case 'g':
            if ((string = va_arg (*app, const gchar *)))
              g_variant_builder_add_value (builder,
                                           g_variant_new_signature (string));
            break;

          case '@':
          case '*':
          case '?':
          case 'r':
            if ((value = va_arg (*app, GVariant *)))
              g_variant_builder_add_value (builder, value);
            break;

          case 'v':
            if ((value = va_arg (*app, GVariant *)))
              g_variant_builder_add_value (builder,
                                           g_variant_new_variant (value));
            break;

          case '&':
            {
              gconstpointer ptr;

              if ((ptr = va_arg (*app, gconstpointer)))
                {
                  GVariantType *type;
                  gsize n_items;

                  type = g_variant_format_string_scan_type (string,
                                                            NULL, NULL);
                  g_assert (g_variant_type_is_definite (type));

                  if (g_variant_type_is_array (type))
                    n_items = va_arg (*app, gsize);
                  else
                    n_items = 0;

                  g_variant_builder_add_value (builder,
                                               g_variant_load_fixed (type,
                                                                     ptr,
                                                                     n_items));
                  g_variant_type_free (type);
                }
              break;
            }

          default:
            {
              gboolean *just;

              just = va_arg (*app, gboolean *);

              if (just != NULL)
                {
                  /* non-NULL, so consume the arguments */
                  value = g_variant_valist_new (&string, app);
                  g_assert (string == *format_string);

                  g_variant_ref_sink (value);

                  if (*just)
                    /* only put in the maybe if just was TRUE */
                    g_variant_builder_add_value (builder, value);

                  g_variant_unref (value);
                }
            }
        }

        return g_variant_builder_end (builder);
      }

    case '(':
    case '{':
      {
        GVariantBuilder *builder;

        if (**format_string == '(')
          builder = g_variant_builder_new (G_VARIANT_TYPE_TUPLE);
        else
          builder = g_variant_builder_new (G_VARIANT_TYPE_DICT_ENTRY);

        (*format_string)++;                                          /* '(' */
        while (**format_string != ')' && **format_string != '}')
          {
            GVariant *value;

            value = g_variant_valist_new (format_string, app);
            g_variant_builder_add_value (builder, value);
          }
        (*format_string)++;                                          /* ')' */

        return g_variant_builder_end (builder);
      }

    default:
      g_assert_not_reached ();
  }
}

static void
g_variant_valist_get (GVariant     *value,
                      gboolean      free,
                      const gchar **format_string,
                      va_list      *app)
{
  switch ((*format_string)[0])
  {
    case 'b':
      {
        gboolean *ptr = va_arg (*app, gboolean *);

        if (ptr)
          {
            if (value)
              *ptr = g_variant_get_boolean (value);
            else
              *ptr = FALSE;
          }

        (*format_string)++;
        return;
      }

    case 'y':
      {
        guchar *ptr = va_arg (*app, guchar *);

        if (ptr)
          {
            if (value)
              *ptr = g_variant_get_byte (value);
            else
              *ptr = '\0';
          }

        (*format_string)++;
        return;
      }

    case 'n':
      {
        gint16 *ptr = va_arg (*app, gint16 *);

        if (ptr)
          {
            if (value)
              *ptr = g_variant_get_int16 (value);
            else
              *ptr = 0;
          }

        (*format_string)++;
        return;
      }

    case 'q':
      {
        guint16 *ptr = va_arg (*app, guint16 *);

        if (ptr)
          {
            if (value)
              *ptr = g_variant_get_uint16 (value);
            else
              *ptr = 0;
          }

        (*format_string)++;
        return;
      }

    case 'i':
      {
        gint32 *ptr = va_arg (*app, gint32 *);

        if (ptr)
          {
            if (value)
              *ptr = g_variant_get_int32 (value);
            else
              *ptr = 0;
          }

        (*format_string)++;
        return;
      }

    case 'h':
      {
        gint32 *ptr = va_arg (*app, gint32 *);

        if (ptr)
          {
            if (value)
              *ptr = g_variant_get_handle (value);
            else
              *ptr = 0;
          }

        (*format_string)++;
        return;
      }

    case 'u':
      {
        guint32 *ptr = va_arg (*app, guint32 *);

        if (ptr)
          {
            if (value)
              *ptr = g_variant_get_uint32 (value);
            else
              *ptr = 0;
          }

        (*format_string)++;
        return;
      }

    case 'x':
      {
        gint64 *ptr = va_arg (*app, gint64 *);

        if (ptr)
          {
            if (value)
              *ptr = g_variant_get_int64 (value);
            else
              *ptr = 0;
          }

        (*format_string)++;
        return;
      }

    case 't':
      {
        guint64 *ptr = va_arg (*app, guint64 *);

        if (ptr)
          {
            if (value)
              *ptr = g_variant_get_uint64 (value);
            else
              *ptr = 0;
          }

        (*format_string)++;
        return;
      }

    case 'd':
      {
        gdouble *ptr = va_arg (*app, gdouble *);

        if (ptr)
          {
            if (value)
              *ptr = g_variant_get_double (value);
            else
              *ptr = 0.;
          }

        (*format_string)++;
        return;
      }

    case 's':
    case 'o':
    case 'g':
      {
        gchar **ptr = va_arg (*app, gchar **);

        if (ptr)
          {
            if (free && *ptr)
              g_free (*ptr);

            if (value)
              *ptr = g_variant_dup_string (value, NULL);
            else
              *ptr = NULL;
          }

        (*format_string)++;
        return;
      }

    case 'v':
      {
        GVariant **ptr = va_arg (*app, GVariant **);

        if (ptr)
          {
            if (free && *ptr)
              g_variant_unref (*ptr);

            if (value)
              *ptr = g_variant_get_variant (value);
            else
              *ptr = NULL;
          }

        (*format_string)++;
        return;
      }

    case '@':
    case '*':
    case '?':
    case 'r':
      {
        GVariant **ptr = va_arg (*app, GVariant **);

        if (ptr)
          {
            if (free && *ptr)
              g_variant_unref (*ptr);

            if (value)
              *ptr = g_variant_ref (value);
            else
              *ptr = NULL;
          }

        g_variant_format_string_scan (*format_string, NULL, format_string);
        return;
      }

    case 'a':
      {
        GVariantIter *ptr = va_arg (*app, GVariantIter *);

        if (ptr)
          {
            if (free)
              g_variant_iter_cancel (ptr);

            if (value)
              g_variant_iter_init (ptr, value);
            else
              {
                memset (ptr, 0, sizeof (GVariantIter));
                g_variant_iter_cancel (ptr);
              }
          }

        g_variant_format_string_scan (*format_string, NULL, format_string);
        return;
      }

    case '^':
      {
        const gchar *string = (*format_string + 1);

        if (string[0] == 'a')
          {
            gpointer *ptr;

            ptr = va_arg (*app, gpointer *);

            if (string[1] == 's')
              {
                *format_string += 3;

                if (free && *ptr)
                  g_strfreev (*ptr);

                if (value)
                  *ptr = g_variant_dup_strv (value, NULL);
                else
                  *ptr = NULL;

                return;
              }

            if (string[1] == '&' && string[2] == 's')
              {
                *format_string += 4;

                if (free && *ptr)
                  g_free (*ptr);

                if (value)
                  *ptr = g_variant_get_strv (value, NULL);
                else
                  *ptr = NULL;

                return;
              }
          }

        g_error ("Currently, only ^as and ^a&s are supported");
      }


    case '&':
      {
        gconstpointer *ptr;

        ptr = va_arg (*app, gconstpointer *);

        if ((*format_string)[1] == 's') /* '&s' */
          {
            /* cannot just get_data() like below.
             * might not be nul-terminated */
            if (ptr)
              {
                if (value)
                  *ptr = g_variant_get_string (value, NULL);
                else
                  *ptr = NULL;
              }

            (*format_string) += 2;
            return;
          }

        if ((*format_string)[1] == 'a') /* '&a..' */
          {
            gint *n_items = va_arg (*app, gint *);

            if (n_items)
              {
                if (value)
                  *n_items = g_variant_n_children (value);
                else
                  *n_items = 0;
              }
          }

        if (ptr)
          {
            if (value)
              *ptr = g_variant_get_data (value);
            else
              *ptr = NULL;
          }

        g_variant_format_string_scan (*format_string, NULL, format_string);
        return;
      }

    case 'm':
      {
        GVariant *just;

        if (value && g_variant_n_children (value))
          just = g_variant_get_child_value (value, 0);
        else
          just = NULL;

        /* skip the 'm', check the next character */
        if (!strchr ("sogv@*?r&", *++(*format_string)))
          {
            gboolean *ptr = va_arg (*app, gboolean *);

            if (ptr)
              {
                /* only free the args if *ptr was TRUE from last time.
                 * else, last iteration was 'None' -> nothing to free. */
                g_variant_valist_get (just, free && *ptr,
                                      format_string, app);
                *ptr = just != NULL;
              }
            else
              g_variant_format_string_scan (*format_string,
                                            NULL, format_string);
          }
        else
          g_variant_valist_get (just, free, format_string, app);

        if (just)
          g_variant_unref (just);
        return;
      }

    case '(':
    case '{':
      {
        GVariantIter iter;
        GVariant *child;
        char end_char;

        if (**format_string == '(')
          end_char = ')';
        else
          end_char = '}';

        if (value)
          g_variant_iter_init (&iter, value);

        (*format_string)++;                                          /* '(' */
        while (**format_string != ')' && **format_string != '}')
          {
            if (value)
              {
                child = g_variant_iter_next_value (&iter);
                g_assert (child != NULL);
              }
            else
              child = NULL;

            g_variant_valist_get (child, free, format_string, app);
          }
        (*format_string)++;                                          /* ')' */

        if (value)
          {
            child = g_variant_iter_next_value (&iter);
            g_assert (child == NULL);
          }

        return;
      }

    default:
      g_assert_not_reached ();
  }
}

/**
 * g_variant_new:
 * @format_string: a #GVariant format string
 * @...: arguments, as per @format_string
 * @returns: a new floating #GVariant instance
 *
 * Creates a new #GVariant instance.
 *
 * Think of this function as an analogue to g_strdup_printf().
 *
 * The type of the created instance and the arguments that are
 * expected by this function are determined by @format_string.  In the
 * most simple case, @format_string is exactly equal to a definite
 * #GVariantType type string and the result is of that type.  All
 * exceptions to this case are explicitly mentioned below.
 *
 * The arguments that this function collects are determined by
 * scanning @format_string from start to end.  Brackets do not impact
 * the collection of arguments (except when following 'a', 'm' or '@';
 * see below).  Each other character that is encountered will result
 * in an argument being collected.
 *
 * Arguments for the base types are expected as follows:
 * <informaltable>
 *   <tgroup cols='3'>
 *     <colspec align='center'/>
 *     <colspec align='center'/>
 *     <colspec align='left'/>
 *     <thead>
 *       <row>
 *       <entry>Character</entry>
 *       <entry>Argument Type</entry>
 *       <entry>Notes</entry>
 *       </row>
 *     </thead>
 *     <tbody>
 *       <row>
 *         <entry><literal>b</literal></entry>
 *         <entry>#gboolean</entry>
 *       </row>
 *       <row>
 *         <entry><literal>y</literal></entry>
 *         <entry>#guchar</entry>
 *       </row>
 *       <row>
 *         <entry><literal>n</literal></entry>
 *         <entry>#gint16</entry>
 *       </row>
 *       <row>
 *         <entry><literal>q</literal></entry>
 *         <entry>#guint16</entry>
 *       </row>
 *       <row>
 *         <entry><literal>i</literal></entry>
 *         <entry>#gint32</entry>
 *       </row>
 *       <row>
 *         <entry><literal>u</literal></entry>
 *         <entry>#guint32</entry>
 *       </row>
 *       <row>
 *         <entry><literal>x</literal></entry>
 *           <entry>#gint64</entry>
 *       </row>
 *       <row>
 *         <entry><literal>t</literal></entry>
 *         <entry>#guint64</entry>
 *       </row>
 *       <row>
 *         <entry><literal>d</literal></entry>
 *         <entry>#gdouble</entry>
 *       </row>
 *       <row>
 *         <entry><literal>s</literal></entry>
 *         <entry>const #gchar *</entry>
 *         <entry>must be non-%NULL</entry>
 *       </row>
 *       <row>
 *         <entry><literal>o</literal></entry>
 *         <entry>const #gchar *</entry>
 *         <entry>must be non-%NULL and a valid DBus object path</entry>
 *       </row>
 *       <row>
 *         <entry><literal>g</literal></entry>
 *         <entry>const #gchar *</entry>
 *         <entry>must be non-%NULL and a valid DBus signature string</entry>
 *       </row>
 *     </tbody>
 *   </tgroup>
 * </informaltable>
 *
 * If a 'v' character is encountered in @format_string then a
 * (#GVariant *) is collected which must be non-%NULL and must point
 * to a valid #GVariant instance.
 *
 * If an array type is encountered in @format_string, a
 * (#GVariantBuilder *) is collected and has g_variant_builder_end()
 * called on it.  The type of the array has no impact on argument
 * collection but is checked against the type of the array and can be
 * used to infer the type of an empty array.  As a special exception,
 * if %NULL is given then an empty array of the given type will be
 * used.
 *
 * If a maybe type is encountered in @format_string, then the expected
 * arguments vary depending on the type.
 *
 * <informaltable>
 *   <tgroup cols='3'>
 *     <colspec align='center'/>
 *     <colspec align='center'/>
 *     <colspec align='left'/>
 *     <thead>
 *       <row>
 *       <entry>Format string</entry>
 *       <entry>Argument Type</entry>
 *       <entry>Notes</entry>
 *       </row>
 *     </thead>
 *     <tbody>
 *       <row>
 *         <entry>ms</entry>
 *         <entry>const #gchar *</entry>
 *         <entry>use %NULL for Nothing</entry>
 *       </row>
 *       <row>
 *         <entry>mo</entry>
 *         <entry>const #gchar *</entry>
 *         <entry>use %NULL for Nothing</entry>
 *       </row>
 *       <row>
 *         <entry>mg</entry>
 *         <entry>const #gchar *</entry>
 *         <entry>use %NULL for Nothing</entry>
 *       </row>
 *       <row>
 *         <entry>mv</entry>
 *         <entry>#GVariant *</entry>
 *         <entry>use %NULL for Nothing</entry>
 *       </row>
 *       <row>
 *         <entry>m@*</entry>
 *         <entry>#GVariant *</entry>
 *         <entry>use %NULL for Nothing</entry>
 *       </row>
 *       <row>
 *         <entry>otherwise</entry>
 *         <entry>#gboolean</entry>
 *         <entry>
 *           use %FALSE for Nothing.  If %TRUE is given then the
 *           arguments will be collected, after the #gboolean, exactly
 *           as they would be if there were no 'm'.
 *         </entry>
 *       </row>
 *     </tbody>
 *   </tgroup>
 * </informaltable>
 *
 * If a '@' character is encountered in @format_string, followed by a
 * type string, then a (#GVariant *) is collected which must be
 * non-%NULL and must point to a valid #GVariant instance.  This
 * #GVariant is inserted directly at the given position.  The given
 * #GVariant must match the provided format string.
 *
 * '*' means exactly the same as '@*'. 'r' means exactly the same as
 * '@r'.  '?' means exactly the same as '@?'.
 *
 * The first character of the format string must not be '*' '?' '@' or
 * 'r'; in essence, a new #GVariant must always be constructed by this
 * function (and not merely passed through it unmodified).
 *
 * Please note that the syntax of the format string is very likely to
 * be extended in the future.
 **/
GVariant *
g_variant_new (const gchar *format_string,
               ...)
{
  GVariant *value;
  va_list ap;

  g_assert (format_string != NULL);
  g_assert (strchr ("*?@r", format_string[0]) == NULL);

  va_start (ap, format_string);
  value = g_variant_new_va (NULL, format_string, NULL, &ap);
  va_end (ap);

  return value;
}

/**
 * g_variant_new_va:
 * @must_be_null: %NULL (for future expansion)
 * @format_string: a string that is prefixed with a format string
 * @endptr: location to store the end pointer, or %NULL
 * @app: a pointer to a #va_list
 * @returns: a new, usually floating, #GVariant
 *
 * This function is intended to be used by libraries based on
 * #GVariant that want to provide g_variant_new()-like functionality
 * to their users.
 *
 * The API is more general than g_variant_new() to allow a wider range
 * of possible uses.

 * @format_string must still point to a valid format string, but it only
 * need to be nul-terminated if @endptr is %NULL.  If @endptr is
 * non-%NULL then it is updated to point to the first character past the
 * end of the format string.
 *
 * @app is a pointer to a #va_list.  The arguments, according to
 * @format_string, are collected from this #va_list and the list is left
 * pointing to the argument following the last.
 *
 * These two generalisations allow mixing of multiple calls to
 * g_variant_new_va() and g_variant_get_va() within a single actual
 * varargs call by the user.
 *
 * The return value will be floating if it was a newly created GVariant
 * instance (for example, if the format string was "(ii)").  In the case
 * that the format_string was '*', '?', 'r', or a format starting with
 * '@' then the collected #GVariant pointer will be returned unmodified,
 * without adding any additional references.
 *
 * In order to behave correctly in all cases it is necessary for the
 * calling function to g_variant_ref_sink() the return result before
 * returning control to the user that originally provided the pointer.
 * At this point, the caller will have their own full reference to the
 * result.  This can also be done by adding the result to a container,
 * or by passing it to another g_variant_new() call.
 **/
GVariant *
g_variant_new_va (gpointer      must_be_null,
                  const gchar  *format_string,
                  const gchar **endptr,
                  va_list      *app)
{
  GVariant *value;

  g_return_val_if_fail (format_string != NULL, NULL);
  g_return_val_if_fail (must_be_null == NULL, NULL);
  g_return_val_if_fail (app != NULL, NULL);

  value = g_variant_valist_new (&format_string, app);

  if (endptr != NULL)
    *endptr = format_string;
  else
    g_assert (*format_string == '\0');

  g_variant_flatten (value);

  return value;
}

/**
 * g_variant_get:
 * @value: a #GVariant instance
 * @format_string: a #GVariant format string
 * @...: arguments, as per @format_string
 *
 * Deconstructs a #GVariant instance.
 *
 * Think of this function as an analogue to scanf().
 *
 * The arguments that are expected by this function are entirely
 * determined by @format_string.  @format_string also restricts the
 * permissible types of @value.  It is an error to give a value with
 * an incompatible type.
 *
 * In the most simple case, @format_string is exactly equal to a
 * definite #GVariantType type string and @value must have that type.
 * All exceptions to this case are explicitly mentioned below.
 *
 * The arguments that this function collects are determined by
 * scanning @format_string from start to end.  Brackets do not impact
 * the collection of arguments (except when following 'a', 'm' or '@';
 * see below).  Each other character that is encountered will result
 * in an argument being collected.
 *
 * All arguments are pointer types.  Any pointer may be %NULL and in
 * that case, collection will be supressed for that position (ie: you
 * can use a %NULL pointer if you don't care about the value of a
 * particular child of a complex value).
 *
 * Arguments for the base types are expected as follows:
 * <informaltable>
 *   <tgroup cols='2'>
 *     <colspec align='center'/>
 *     <colspec align='center'/>
 *     <thead>
 *       <row>
 *         <entry>Character</entry>
 *         <entry>Argument Type</entry>
 *       </row>
 *     </thead>
 *     <tbody>
 *       <row>
 *         <entry><literal>b</literal></entry>
 *         <entry>#gboolean*</entry>
 *       </row>
 *       <row>
 *         <entry><literal>y</literal></entry>
 *         <entry>#guchar*</entry>
 *       </row>
 *       <row>
 *         <entry><literal>n</literal></entry>
 *         <entry>#gint16*</entry>
 *       </row>
 *       <row>
 *         <entry><literal>q</literal></entry>
 *         <entry>#guint16*</entry>
 *       </row>
 *       <row>
 *         <entry><literal>i</literal></entry>
 *         <entry>#gint32*</entry>
 *       </row>
 *       <row>
 *         <entry><literal>u</literal></entry>
 *         <entry>#guint32*</entry>
 *       </row>
 *       <row>
 *         <entry><literal>x</literal></entry>
 *         <entry>#gint64*</entry>
 *       </row>
 *       <row>
 *         <entry><literal>t</literal></entry>
 *         <entry>#guint64*</entry>
 *       </row>
 *       <row>
 *         <entry><literal>d</literal></entry>
 *         <entry>#gdouble*</entry>
 *       </row>
 *       <row>
 *         <entry><literal>s</literal></entry>
 *         <entry>const #gchar *</entry>
 *       </row>
 *       <row>
 *         <entry><literal>o</literal></entry>
 *         <entry>const #gchar *</entry>
 *       </row>
 *       <row>
 *         <entry><literal>g</literal></entry>
 *         <entry>const #gchar *</entry>
 *       </row>
 *     </tbody>
 *   </tgroup>
 * </informaltable>
 *
 * If a 'v' character is encountered in @format_string then a
 * (#GVariant **) is collected.  If this is non-%NULL then it is set
 * to a new reference to a #GVariant.  You must call g_variant_unref()
 * on it later.
 *
 * If an array type is encountered in @format_string, a
 * (#GVariantIter *) is collected.  If this is non-%NULL then the
 * #GVariantIter at which it points will be initialised for iterating
 * over the array.  The array must be iterated (or the iter cancelled)
 * in order to avoid leaking memory.
 *
 * If a maybe type is encountered in @format_string, then the expected
 * arguments vary depending on the type.
 *
 * <informaltable>
 *   <tgroup cols='3'>
 *     <colspec align='center'/>
 *     <colspec align='center'/>
 *     <colspec align='left'/>
 *     <thead>
 *       <row>
 *       <entry>Format string</entry>
 *       <entry>Argument Type</entry>
 *       <entry>Notes</entry>
 *       </row>
 *     </thead>
 *     <tbody>
 *       <row>
 *         <entry>ms</entry>
 *         <entry>const #gchar *</entry>
 *         <entry>set to %NULL in case of Nothing</entry>
 *       </row>
 *       <row>
 *         <entry>mo</entry>
 *         <entry>const #gchar *</entry>
 *         <entry>set to %NULL in case of Nothing</entry>
 *       </row>
 *       <row>
 *         <entry>mg</entry>
 *         <entry>const #gchar *</entry>
 *         <entry>set to %NULL in case of Nothing</entry>
 *       </row>
 *       <row>
 *         <entry>mv</entry>
 *         <entry>#GVariant *</entry>
 *         <entry>set to %NULL in case of Nothing</entry>
 *       </row>
 *       <row>
 *         <entry>m@*</entry>
 *         <entry>#GVariant *</entry>
 *         <entry>set to %NULL in case of Nothing</entry>
 *       </row>
 *       <row>
 *         <entry>otherwise</entry>
 *         <entry>#gboolean*</entry>
 *         <entry>
 *           If %NULL is given then the entire value is ignored.
 *           Else, the #gboolean is set to %TRUE to indicate that the
 *           maybe is non-Nothing or %FALSE to indicate Nothing.  In
 *           the %TRUE case, the normal arguments (as per the format
 *           string) are collected and handled normally.  In the
 *           %FALSE case, the normal arguments are collected and
 *           ignored.  This allows the same set of arguments to be
 *           given without knowing the content of the value ahead of
 *           time.
 *         </entry>
 *       </row>
 *     </tbody>
 *   </tgroup>
 * </informaltable>
 *
 * If a '@' character is encountered in @format_string, followed by a
 * type string, then a (#GVariant **) is collected.  If this is
 * non-%NULL then it is set to a new reference to a #GVariant the
 * corresponds to the value at the current position.  You must call
 * g_variant_unref() on it later.  The returned value will match the
 * type string provided.
 *
 * '*' means exactly the same as '@*'. 'r' means exactly the same as
 * '@r'.  '?' means exactly the same as '@?'.
 *
 * Please note that the syntax of the format string is very likely to
 * be extended in the future.
 **/
void
g_variant_get (GVariant    *value,
               const gchar *format_string,
               ...)
{
  va_list ap;

  va_start (ap, format_string);
  g_variant_get_va (value, NULL, format_string, NULL, &ap);
  va_end (ap);
}

/**
 * g_variant_get_va:
 * @value: a #GVariant
 * @must_be_null: %NULL (for future expansion)
 * @format_string: a string that is prefixed with a format string
 * @endptr: location to store the end pointer, or %NULL
 * @app: a pointer to a #va_list
 *
 * This function is intended to be used by libraries based on #GVariant
 * that want to provide g_variant_get()-like functionality to their
 * users.
 *
 * The API is more general than g_variant_get() to allow a wider range
 * of possible uses.
 *
 * @format_string must still point to a valid format string, but it only
 * need to be nul-terminated if @endptr is %NULL.  If @endptr is
 * non-%NULL then it is updated to point to the first character past the
 * end of the format string.
 *
 * @app is a pointer to a #va_list.  The arguments, according to
 * @format_string, are collected from this #va_list and the list is left
 * pointing to the argument following the last.
 *
 * These two generalisations allow mixing of multiple calls to
 * g_variant_new_va() and g_variant_get_va() within a single actual
 * varargs call by the user.
 **/
void
g_variant_get_va (GVariant     *value,
                  gpointer      must_be_null,
                  const gchar  *format_string,
                  const gchar **endptr,
                  va_list      *app)
{
  g_return_if_fail (format_string != NULL);
  g_return_if_fail (must_be_null == NULL);
  g_return_if_fail (value != NULL);
  g_return_if_fail (app != NULL);

  {
    GVariantType *type;

    type = g_variant_format_string_scan_type (format_string, NULL, endptr);
    g_return_if_fail (type != NULL);
    g_return_if_fail (g_variant_has_type (value, type));
    g_variant_type_free (type);
  }

  g_variant_flatten (value);
  g_variant_valist_get (value, FALSE, &format_string, app);
}

/**
 * g_variant_iter_next:
 * @iter: a #GVariantIter
 * @format_string: a format string
 * @...: arguments, as per @format_string
 * @returns: %TRUE if a child was fetched or %FALSE if not
 *
 * Retreives the next child value from @iter and deconstructs it
 * according to @format_string.  This call is sort of like calling
 * g_variant_iter_next() and g_variant_get().
 *
 * This function does something else, though: on all but the first
 * call (including on the last call, which returns %FALSE) the values
 * allocated by the previous call will be freed.  This allows you to
 * iterate without ever freeing anything yourself.  In the case of
 * #GVariant * arguments, they are unref'd and in the case of
 * #GVariantIter arguments, they are cancelled.
 *
 * Note that strings are not freed since (as with g_variant_get())
 * they are constant pointers to internal #GVariant data.
 *
 * This function might be used as follows:
 *
 * <programlisting>
 * {
 *   const gchar *key, *value;
 *   GVariantIter iter;
 *   ...
 *
 *   while (g_variant_iter_next (iter, "{ss}", &key, &value))
 *     printf ("dict['%s'] = '%s'\n", key, value);
 * }
 * </programlisting>
 **/
gboolean
g_variant_iter_next (GVariantIter *iter,
                     const gchar  *format_string,
                     ...)
{
  gboolean free_args;
  GVariant *next;
  va_list ap;

  free_args = g_variant_iter_should_free (iter);
  next = g_variant_iter_next_value (iter);
  /* g_assert (free_args || next != NULL);
   * XXX this fails on empty iters */

  if (next)
    g_variant_flatten (next);

  va_start (ap, format_string);
  g_variant_valist_get (next, free_args, &format_string, &ap);
  g_assert (*format_string == '\0');
  va_end (ap);

  return next != NULL;
}

/**
 * g_variant_builder_add:
 * @builder: a #GVariantBuilder
 * @format_string: a #GVariant varargs format string
 * @...: arguments, as per @format_string
 *
 * Adds to a #GVariantBuilder.
 *
 * This call is a convenience wrapper that is exactly equivalent to
 * calling g_variant_new() followed by g_variant_builder_add_value().
 *
 * This function might be used as follows:
 *
 * <programlisting>
 * GVariant *
 * make_pointless_dictionary (void)
 * {
 *   GVariantBuilder *builder;
 *   int i;
 *
 *   builder = g_variant_builder_new (G_VARIANT_TYPE_CLASS_ARRAY,
 *                                    NULL);
 *   for (i = 0; i < 16; i++)
 *     {
 *       char buf[3];
 *
 *       sprintf (buf, "%d", i);
 *       g_variant_builder_add (builder, "{is}", i, buf);
 *     }
 *
 *   return g_variant_builder_end (builder);
 * }
 * </programlisting>
 **/
void
g_variant_builder_add (GVariantBuilder *builder,
                       const gchar     *format_string,
                       ...)
{
  GVariant *variant;
  va_list ap;

  va_start (ap, format_string);
  variant = g_variant_new_va (NULL, format_string, NULL, &ap);
  va_end (ap);

  g_variant_builder_add_value (builder, variant);
}

/**
 * g_variant_get_child:
 * @value: a container #GVariant
 * @index: the index of the child to deconstruct
 * @format_string: a #GVariant format string
 * @...: arguments, as per @format_string
 *
 * Reads a child item out of a container #GVariant instance and
 * deconstructs it according to @format_string.  This call is
 * essentially a combination of g_variant_get_child_value() and
 * g_variant_get().
 **/
void
g_variant_get_child (GVariant    *value,
                     gint         index,
                     const gchar *format_string,
                     ...)
{
  GVariant *child;
  va_list ap;

  g_variant_flatten (value);
  child = g_variant_get_child_value (value, index);

  va_start (ap, format_string);
  g_variant_get_va (child, NULL, format_string, NULL, &ap);
  va_end (ap);

  g_variant_unref (child);
}

/**
 * g_variant_lookup:
 * @dictionary: a #GVariant dictionary, keyed by strings
 * @key: a string to lookup in the dictionary
 * @format_string: a #GVariant format string, or %NULL
 * @...: arguments, as per @format_string
 * @returns: %TRUE if @key was found, else %FALSE
 *
 * Looks up @key in @dictionary and deconstructs it according to
 * @format_string.  This call is essentially a combination of
 * g_variant_lookup_value() and g_variant_get().
 *
 * If @key is not found, then no deconstruction occurs (ie: the argument
 * list is left untouched) and %FALSE is returned.  If @key is found
 * then %TRUE is returned.
 *
 * As a special case, if @format_string is %NULL then the lookup occurs
 * but no deconstruction is preformed.  This is useful for checking (via
 * the return value) if a key is in the dictionary or not.
 **/
gboolean
g_variant_lookup (GVariant    *dictionary,
                  const gchar *key,
                  const gchar *format_string,
                  ...)
{
  GVariant *child;
  va_list ap;

  if (format_string)
    g_variant_flatten (dictionary);

  child = g_variant_lookup_value (dictionary, key);

  if (child == NULL)
    return FALSE;

  if (format_string)
    {
      va_start (ap, format_string);
      g_variant_get_va (child, NULL, format_string, NULL, &ap);
      va_end (ap);
    }

  g_variant_unref (child);

  return TRUE;
}
