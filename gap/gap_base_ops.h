/* gap_base_ops.h
 * 2003.05.24 hof (Wolfgang Hofer)
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
 * 1.3.16b; 2003/07/03   hof: added gap_density
 * 1.3.14a  2003/05/24   hof: created (module was splitted off from gap_lib)
 */

#ifndef _GAP_BASE_OPS_H
#define _GAP_BASE_OPS_H

#include "libgimp/gimp.h"

/* Video menu basic fuctions */

gint32 gap_base_next(GimpRunMode run_mode, gint32 image_id);
gint32 gap_base_prev(GimpRunMode run_mode, gint32 image_id);
gint32 gap_base_first(GimpRunMode run_mode, gint32 image_id);
gint32 gap_base_last(GimpRunMode run_mode, gint32 image_id);
gint32 gap_base_goto(GimpRunMode run_mode, gint32 image_id, int nr);

gint32 gap_base_density(GimpRunMode run_mode, gint32 image_id, long range_from, long range_to, gdouble density_factor, gboolean density_grow);
gint32 gap_base_dup(GimpRunMode run_mode, gint32 image_id, int nr, long range_from, long range_to);
gint32 gap_base_del(GimpRunMode run_mode, gint32 image_id, int nr);
gint32 gap_base_exchg(GimpRunMode run_mode, gint32 image_id, int nr);
gint32 gap_base_shift(GimpRunMode run_mode, gint32 image_id, int nr, long range_from, long range_to);
gint32 gap_base_reverse(GimpRunMode run_mode, gint32 image_id, long range_from, long range_to);
gint32 gap_base_renumber(GimpRunMode run_mode, gint32 image_id,
            long start_frame_nr, long digits);

#endif


