/* gap_image.h
 * 2003.10.09 hof (Wolfgang Hofer)
 *
 * GAP ... Gimp Animation Plugins
 *
 * This Module contains Image specific Procedures
 *
 */
/* The GIMP -- an image manipulation program
 * Copyright (C) 1995 Spencer Kimball and Peter Mattis
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see
 * <http://www.gnu.org/licenses/>.
 */

/* revision history:
 * version 1.3.20d; 2003.10.14   hof: created
 */

#ifndef _GAP_IMAGE_H
#define _GAP_IMAGE_H


#include "config.h"

/* SYTEM (UNIX) includes */
#include <stdio.h>
#include <stdlib.h>

/* GIMP includes */
#include "gtk/gtk.h"
#include "libgimp/gimp.h"

void      gap_image_delete_immediate (gint32 image_id);
gint32    gap_image_merge_visible_layers(gint32 image_id, GimpMergeType mergemode);
void      gap_image_prevent_empty_image(gint32 image_id);
gint32    gap_image_new_with_layer_of_samesize(gint32 old_image_id, gint32 *layer_id);
gint32    gap_image_new_of_samesize(gint32 old_image_id);
gboolean  gap_image_is_alive(gint32 image_id);
gint32    gap_image_get_any_layer(gint32 image_id);

gint32    gap_image_merge_to_specified_layer(gint32 ref_layer_id, GimpMergeType mergemode);
gboolean  gap_image_set_selection_from_selection_or_drawable(gint32 image_id, gint32 ref_drawable_id
                              , gboolean force_from_drawable);
void      gap_image_remove_invisble_layers(gint32 image_id);
void      gap_image_remove_all_guides(gint32 image_id);
void      gap_image_limit_layers(gint32 image_id, gint keepTopLayers,  gint keepBgLayers);

gint32    gap_image_create_unicolor_image(gint32 *layer_id, gint32 width , gint32 height
                       , gdouble r_f, gdouble g_f, gdouble b_f, gdouble a_f);


#endif

