/*  gap_filter.h
 *
 * GAP ... Gimp Animation Plugins
 *
 * This Module contains:
 *   Headers for gap_filter_*.c (animated filter apply to all imagelayers)
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

#ifndef _GAP_FILTER_H
#define _GAP_FILTER_H

#include "libgimp/gimp.h"
#include "gap_filter_pdb.h"

/* GIMP-GAP Suffix strings to identify iterator procedures.
 * The iterator procedures have same name as the corresponding plug in
 * extended by GAP_ITERATOR_SUFFIX or GAP_ITERATOR_SUFFIX
 *
 * Note: GIMP-2.4 changed naming style for pdb names from
 * underscore to minus character. 
 * The GIMP-2.4 core uses
 * canonicalized names that converts all characters that are no letter and no digit
 * to minus character.
 * 
 */
#define GAP_ITERATOR_SUFFIX "-Iterator"
#define GAP_ITERATOR_ALT_SUFFIX "-Iterator-ALT"

/* GIMP-GAP suffixes for start and end value buffers
 * (stored via gimp_set_data)
 * those buffers are named like the corresponding plug in
 * extended by suffixes GAP_ITER_FROM_SUFFIX and GAP_ITER_TO_SUFFIX.
 *
 */
#define GAP_ITER_FROM_SUFFIX "-ITER-FROM"
#define GAP_ITER_TO_SUFFIX "-ITER-TO"

/* ------------------------
 * gap_filter_foreach.h
 * ------------------------
 */

gint gap_proc_anim_apply(GimpRunMode run_mode, gint32 image_id, char *l_plugin_name
    , gint32 accelCharacteristic);


/* ------------------------
 * gap_filter_iterators.h
 * ------------------------
 */

/* Hacked Iterators for some existing Plugins */

gint gap_run_iterators_ALT(const char *name, GimpRunMode run_mode, gint32 total_steps, gdouble current_step, gint32 len_struct);
void gap_query_iterators_ALT();

/* ------------------------
 * gap_filter_codegen.h
 * ------------------------
 */
 
void gap_codegen_remove_codegen_files();
gint gap_codegen_gen_code_iter_ALT   (char  *proc_name);
gint gap_codegen_gen_forward_iter_ALT(char  *proc_name);
gint gap_codegen_gen_tab_iter_ALT    (char  *proc_name);
gint gap_codegen_gen_code_iterator       (char  *proc_name);



#endif
