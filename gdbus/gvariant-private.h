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

#ifndef __G_VARIANT_PRIVATE_H__
#define __G_VARIANT_PRIVATE_H__

#include "gvarianttypeinfo.h"
#include "gvariant.h"

/* gvariant-core.c */
GVariant *                      g_variant_new_tree                      (const GVariantType  *type,
                                                                         GVariant           **children,
                                                                         gsize                n_children,
                                                                         gboolean             trusted);
void                            g_variant_assert_invariant              (GVariant            *value);
gboolean                        g_variant_is_trusted                    (GVariant            *value);
void                            g_variant_dump_data                     (GVariant            *value);
GVariant *                      g_variant_load_fixed                    (const GVariantType  *type,
                                                                         gconstpointer        data,
                                                                         gsize                n_items);
gboolean                        g_variant_iter_should_free              (GVariantIter        *iter);
GVariant *                      g_variant_deep_copy                     (GVariant            *value);

/* do not use -- only for test cases */
gboolean                        g_variant_is_normal_                    (GVariant            *value);

#endif /* __G_VARIANT_PRIVATE_H__ */
