/* gap_mov_render.h
 * 1997.11.06 hof (Wolfgang Hofer)
 *
 * GAP ... Gimp Animation Plugins
 *
 * Render utility Procedures for GAP MovePath
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
#ifndef _GAP_MOV_RENDER_H
#define _GAP_MOV_RENDER_H

/* revision history:
 * gimp    1.3.21b; 2003/10/22  hof: gap_mov_render_create_or_replace_tempsel_image new parameter all_empty
 * gimp    1.3.20d; 2003/10/15  hof: sourcecode cleanup
 * gimp    1.3.14a; 2003/05/24  hof: created (splitted off from gap_mov_dialog)
 */


#include "gap_mov_dialog.h"

gint   gap_mov_render_render(gint32 image_id, GapMovValues *val_ptr, GapMovCurrent *cur_ptr);
gint   gap_mov_render_fetch_src_frame_autoskip(GapMovValues *pvals,  gint32 wanted_frame_nr
            , gint32 *but_got_frame_nr, long stepsize);
gint   gap_mov_render_fetch_src_frame(GapMovValues *pvals,  gint32 wanted_frame_nr);
void   gap_mov_render_create_or_replace_tempsel_image(gint32 channel_id, GapMovValues *val_ptr, gboolean all_empty);


#endif
