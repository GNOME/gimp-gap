/* gap_layer_copy.h
 * 1997.11.06 hof (Wolfgang Hofer)
 *      gap_layer_copy_to_dest_image 
 *        can copy layers from a drawable in another image.
 *
 * returns the id of the new layer
 *      and the offests of the original within the source image
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
 * version 2.1.0a  2004.04.11   hof: added gap_layer_clear_to_color
 * version 1.3.26a 2004.01.28   hof: added gap_layer_copy_from_buffer
 * version 1.3.21c 2003.11.02   hof: added gap_layer_copy_to_image
 * version 1.3.5a  2002.04.20   hof: use gimp_layer_new_from_drawable (API cleanup, requries gimp.1.3.6)
 *                                   removed channel_copy
 * version 0.98.00 1998.11.26   hof: added channel copy
 * version 0.90.00;             hof: 1.st (pre) release
 */

#ifndef _GAP_LAYER_COPY_H
#define _GAP_LAYER_COPY_H

/* SYTEM (UNIX) includes */ 
#include <stdio.h>
#include <stdlib.h>

/* GIMP includes */
#include "gtk/gtk.h"
#include "libgimp/gimp.h"

gint32 gap_layer_copy_to_dest_image (gint32 dst_image_id,
                        gint32 src_layer_id,
                        gdouble    opacity, /* 0.0 upto 100.0 */
                        GimpLayerModeEffects mode,
                        gint *src_offset_x,
                        gint *src_offset_y );
gint32 gap_layer_copy_to_image (gint32 dst_image_id,
                                gint32 src_layer_id);

gboolean gap_layer_copy_content (gint32 dst_drawable_id, gint32 src_drawable_id);
gboolean gap_layer_copy_picked_channel (gint32 dst_drawable_id,  guint dst_channel_pick
                              , gint32 src_drawable_id, guint src_channel_pick
                              , gboolean shadow);

gint32 gap_layer_new_from_buffer(gint32 dst_image_id
                                , gint32 width
                                , gint32 height
                                , gint32 bpp
                                , guchar *data
                                );
void   gap_layer_clear_to_color(gint32 layer_id
                             ,gdouble red
                             ,gdouble green
                             ,gdouble blue
                             ,gdouble alpha
                             );

gint32 gap_layer_flip(gint32 layer_id, gint32 flip_request);



void   gap_layer_copy_paste_drawable(gint32 image_id, gint32 dst_drawable_id, gint32 src_drawable_id);

gint32 gap_layer_get_stackposition(gint32 image_id, gint32 ref_layer_id);
gint32 gap_layer_get_id_by_stackposition(gint32 image_id, gint32 stackposition);


gint32  gap_layer_make_duplicate(gint32 src_layer_id, gint32 image_id
                                , const char *name_prefix, const char *name_suffix);

gint32  gap_layer_create_layer_from_layermask(gint32 src_layer_id
                               , gint32 image_id
                               , const char *name_prefix, const char *name_suffix);

gint32  gap_layer_create_layer_from_alpha(gint32 src_layer_id, gint32 image_id
                               , const char *name_prefix, const char *name_suffix
                               , gboolean applyExistingLayermask, gboolean useTransferAlpha);

gint32  gap_layer_find_by_name(gint32 image_id, const char *name);

gint32  gap_layer_new_same_size_and_offsets(gint32 image_id, gint32 origLayerId, gboolean imageSize);

void    gap_layer_resize_to_selection(gint32 selImageId, gint32 layerId);

#endif
