/* gap_morph_exec.h
 * 2004.02.12 hof (Wolfgang Hofer)
 * layer morphing worker procedures
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
 * along with this program; if not, write to the Free Software
 * <http://www.gnu.org/licenses/>.
 */

/* revision history:
 * gimp    1.3.26a; 2004/02/12  hof: created
 */

#ifndef _GAP_MORPH_EXEC_H
#define _GAP_MORPH_EXEC_H

#include "libgimp/gimp.h"
#include "gap_morph_main.h"
#include "gap_morph_dialog.h"
#include "gap_libgimpgap.h"

void     gap_morph_exec_free_workpoint_list(GapMorphWorkPoint **wp_list);
gboolean gap_moprh_exec_save_workpointfile(const char *filename
                                 , GapMorphGUIParams *mgup
                                 );
GapMorphWorkPoint * gap_moprh_exec_load_workpointfile(const char *filename
                                 , GapMorphGUIParams *mgup
                                 );
                                 
gint32  gap_morph_exec_find_dst_drawable(gint32 image_id, gint32 layer_id);
void    gap_morph_exec_get_warp_pick_koords(GapMorphWorkPoint *wp_list
                                      , gint32        in_x
                                      , gint32        in_y
                                      , gdouble       scale_x
                                      , gdouble       scale_y
                                      , gboolean      use_quality_wp_selection
                                      , gboolean      use_gravity
                                      , gdouble       gravity_intensity
                                      , gdouble       affect_radius
                                      , gdouble *pick_x
                                      , gdouble *pick_y
                                      );
gint32  gap_morph_execute(GapMorphGlobalParams *mgpp);
gint32  gap_morph_render_one_tween(GapMorphGlobalParams *mgpp);
gint32  gap_morph_render_frame_tweens(GapAnimInfo *ainfo_ptr, GapMorphGlobalParams *mgpp, gboolean *cancelFlagPtr);

#endif


