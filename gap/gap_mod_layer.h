/* gap_mod_layer.h
 * 1998.10.14 hof (Wolfgang Hofer)
 *
 * GAP ... Gimp Animation Plugins
 *
 * This Module contains:
 * modify Layer (perform actions (like raise, set visible, apply filter)
 *               - foreach selected layer
 *               - in each frame of the selected framerange)
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

/* revision history:
 * version 0.98.00   1998.11.27  hof: - use new module gap_pdb_calls.h
 * version 0.97.00              hof: - created module (as extract gap_fileter_foreach)
 */

#ifndef _GAP_MOD_LAYER_H
#define _GAP_MOD_LAYER_H

#define MAX_LAYERNAME 128

/* action_mode values */
#define	 GAP_MOD_ACM_SET_VISIBLE    0
#define	 GAP_MOD_ACM_SET_INVISIBLE  1
#define	 GAP_MOD_ACM_SET_LINKED	    2
#define	 GAP_MOD_ACM_SET_UNLINKED   3
#define	 GAP_MOD_ACM_RAISE          4
#define	 GAP_MOD_ACM_LOWER          5
#define	 GAP_MOD_ACM_MERGE_EXPAND   6
#define	 GAP_MOD_ACM_MERGE_IMG      7
#define	 GAP_MOD_ACM_MERGE_BG       8
#define	 GAP_MOD_ACM_APPLY_FILTER   9
#define	 GAP_MOD_ACM_DUPLICATE     10
#define	 GAP_MOD_ACM_DELETE        11
#define	 GAP_MOD_ACM_RENAME        12

#define	 GAP_MOD_ACM_SEL_REPLACE   13
#define	 GAP_MOD_ACM_SEL_ADD       14
#define	 GAP_MOD_ACM_SEL_SUTRACT   15
#define	 GAP_MOD_ACM_SEL_INTERSECT 16
#define	 GAP_MOD_ACM_SEL_NONE      17
#define	 GAP_MOD_ACM_SEL_ALL       18
#define	 GAP_MOD_ACM_SEL_INVERT    19
#define	 GAP_MOD_ACM_SEL_SAVE      20
#define	 GAP_MOD_ACM_SEL_LOAD      21
#define	 GAP_MOD_ACM_SEL_DELETE    22

typedef struct
{
  gint32 layer_id;
  gint   visible;
  gint   selected;
}  GapModLayliElem;

GapModLayliElem *gap_mod_alloc_layli(gint32 image_id, gint32 *l_sel_cnt, gint *nlayers,
        		   gint32 sel_mode,
        		   gint32 sel_case,
			   gint32 sel_invert,
        		   char *sel_pattern );
int  gap_mod_get_1st_selected (GapModLayliElem * layli_ptr, gint nlayers);

gint gap_mod_layer(GimpRunMode run_mode, gint32 image_id,
                   gint32 range_from,  gint32 range_to,
                   gint32 action_mode, gint32 sel_mode,
                   gint32 sel_case, gint32 sel_invert,
                   char *sel_pattern, char *new_layername);

#endif
