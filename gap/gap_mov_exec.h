/* gap_mov_exec.h
 * 1997.11.01 hof (Wolfgang Hofer)
 *
 * GAP ... Gimp Animation Plugins
 *
 * basic anim functions
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
 * gimp    1.3.20c; 2003/09/23  hof: changed startangle from int to gdouble
 * gimp    1.3.5a;  2002/04/20  hof: API cleanup
 * gimp    1.1.29b; 2000/11/25  hof: NONINTEACTIV PDB interface for Movepath
 * gimp    1.1.20a; 2000/04/25  hof: support for keyframes, anim_preview
 * 0.96.00; 1998/06/27   hof: added gap animation sizechange plugins
 *                            (moved range_ops to separate .h file)
 * 0.94.01; 1998/04/27   hof: added flatten_mode to plugin: gap_range_to_multilayer
 * 0.90.00;              hof: 1.st (pre) release
 */

#ifndef _GAP_MOV_EXEC_H
#define _GAP_MOV_EXEC_H

#include "libgimp/gimp.h"
#include "gap_mov_dialog.h"

void    gap_mov_exec_set_iteration_relevant_src_layers(GapMovCurrent *cur_ptr, gint32 src_layer_id, gint32 src_image_id);
gint32  gap_mov_exec_move_path(GimpRunMode run_mode, gint32 image_id, GapMovValues *pvals, gchar *pointfile, gint rotation_follow, gdouble startangle);
gint32  gap_mov_exec_anim_preview(GapMovValues *pvals_orig, GapAnimInfo *ainfo_ptr, gint preview_frame_nr);
gint32  gap_mov_exec_move_path_singleframe(GimpRunMode run_mode, gint32 image_id
              , GapMovValues *pvals, GapMovSingleFrame *singleFramePtr);


gchar  *gap_mov_exec_chk_keyframes(GapMovValues *pvals);
gint    gap_mov_exec_conv_keyframe_to_rel(gint abs_keyframe, GapMovValues *pvals);
gint    gap_mov_exec_conv_keyframe_to_abs(gint rel_keyframe, GapMovValues *pvals);
gint    gap_mov_exec_gap_save_pointfile(char *filename, GapMovValues *pvals);
gint    gap_mov_exec_gap_load_pointfile(char *filename, GapMovValues *pvals);
void    gap_mov_exec_calculate_rotate_follow(GapMovValues *pvals, gdouble startangle);
void    gap_mov_exec_set_handle_offsets(GapMovValues *val_ptr, GapMovCurrent *cur_ptr);
void    gap_mov_exec_query(GapMovValues *val_ptr, GapAnimInfo *ainfo_ptr, GapMovQuery *mov_query);
gdouble gap_mov_exec_get_default_rotate_threshold();

GapMovValues *gap_mov_exec_new_GapMovValues();
void gap_mov_exec_free_GapMovValues(GapMovValues *pvals);
void gap_mov_exec_copy_GapMovValues(GapMovValues *dstValues, GapMovValues *srcValues);
void gap_mov_exec_copy_xml_GapMovValues(GapMovValues *dstValues, GapMovValues *srcValues);

gboolean  gap_mov_exec_check_valid_xml_paramfile(const char *filename);

/* ------------------------------
 * gap_mov_exec_dim_point_table
 * ------------------------------
 * (re) allocate point table when actual size is smaller than specified num_points
 * e.g. this procedure does never shrink an already allocated point table,
 * but just makes sure it can hold up to num_points.
 */
void gap_mov_exec_dim_point_table(GapMovValues *pvals, gint num_points);

/* ---------------------------------------------
 * gap_mov_exec_move_path_singleframe_directcall
 * ---------------------------------------------
 * this procedure renders one frame of a movepath sequence.
 * it is typically called by the storyboard processor.
 * return the processed layer id
 */
gint32  gap_mov_exec_move_path_singleframe_directcall(gint32 frame_image_id
          , gint32 drawable_id
          , gboolean keep_proportions
          , gboolean fit_width
          , gboolean fit_height
          , gint32 frame_phase
          , const char *xml_paramfile
          );

#endif


