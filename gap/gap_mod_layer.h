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
 * along with this program; if not, see
 * <http://www.gnu.org/licenses/>.
 */

/* revision history:
 * gimp   1.3.24a;   2004/01/17  hof: added Layermask Handling (the GAP_MOD_ACM_LMASK_xxx modes)
 * version 0.98.00   1998.11.27  hof: - use new module gap_pdb_calls.h
 * version 0.97.00              hof: - created module (as extract gap_fileter_foreach)
 */

#ifndef _GAP_MOD_LAYER_H
#define _GAP_MOD_LAYER_H

#include <libgimp/gimp.h>
#include <libgimp/gimpui.h>
#include "gap_libgapbase.h"

#define MAX_LAYERNAME 128

/* action_mode values */
#define  GAP_MOD_ACM_SET_VISIBLE    0
#define  GAP_MOD_ACM_SET_INVISIBLE  1
#define  GAP_MOD_ACM_SET_LINKED     2
#define  GAP_MOD_ACM_SET_UNLINKED   3
#define  GAP_MOD_ACM_RAISE          4
#define  GAP_MOD_ACM_LOWER          5
#define  GAP_MOD_ACM_MERGE_EXPAND   6
#define  GAP_MOD_ACM_MERGE_IMG      7
#define  GAP_MOD_ACM_MERGE_BG       8
#define  GAP_MOD_ACM_APPLY_FILTER   9
#define  GAP_MOD_ACM_DUPLICATE     10
#define  GAP_MOD_ACM_DELETE        11
#define  GAP_MOD_ACM_RENAME        12

#define  GAP_MOD_ACM_SEL_REPLACE   13
#define  GAP_MOD_ACM_SEL_ADD       14
#define  GAP_MOD_ACM_SEL_SUBTRACT  15
#define  GAP_MOD_ACM_SEL_INTERSECT 16
#define  GAP_MOD_ACM_SEL_NONE      17
#define  GAP_MOD_ACM_SEL_ALL       18
#define  GAP_MOD_ACM_SEL_INVERT    19
#define  GAP_MOD_ACM_SEL_SAVE      20
#define  GAP_MOD_ACM_SEL_LOAD      21
#define  GAP_MOD_ACM_SEL_DELETE    22

#define  GAP_MOD_ACM_ADD_ALPHA        23
#define  GAP_MOD_ACM_LMASK_WHITE      24
#define  GAP_MOD_ACM_LMASK_BLACK      25
#define  GAP_MOD_ACM_LMASK_ALPHA      26
#define  GAP_MOD_ACM_LMASK_TALPHA     27
#define  GAP_MOD_ACM_LMASK_SEL        28
#define  GAP_MOD_ACM_LMASK_BWCOPY     29
#define  GAP_MOD_ACM_LMASK_DELETE     30
#define  GAP_MOD_ACM_LMASK_APPLY      31

#define  GAP_MOD_ACM_LMASK_COPY_FROM_UPPER_LMASK   32
#define  GAP_MOD_ACM_LMASK_COPY_FROM_LOWER_LMASK   33

#define  GAP_MOD_ACM_LMASK_INVERT                  34

#define  GAP_MOD_ACM_SET_MODE_NORMAL               35
#define  GAP_MOD_ACM_SET_MODE_DISSOLVE             36
#define  GAP_MOD_ACM_SET_MODE_MULTIPLY             37
#define  GAP_MOD_ACM_SET_MODE_DIVIDE               38
#define  GAP_MOD_ACM_SET_MODE_SCREEN               39
#define  GAP_MOD_ACM_SET_MODE_OVERLAY              40
#define  GAP_MOD_ACM_SET_MODE_DIFFERENCE           41
#define  GAP_MOD_ACM_SET_MODE_ADDITION             42
#define  GAP_MOD_ACM_SET_MODE_SUBTRACT             43
#define  GAP_MOD_ACM_SET_MODE_DARKEN_ONLY          44
#define  GAP_MOD_ACM_SET_MODE_LIGHTEN_ONLY         45
#define  GAP_MOD_ACM_SET_MODE_DODGE                46
#define  GAP_MOD_ACM_SET_MODE_BURN                 47
#define  GAP_MOD_ACM_SET_MODE_HARDLIGHT            48
#define  GAP_MOD_ACM_SET_MODE_SOFTLIGHT            49
#define  GAP_MOD_ACM_SET_MODE_COLOR_ERASE          50
#define  GAP_MOD_ACM_SET_MODE_GRAIN_EXTRACT_MODE   51
#define  GAP_MOD_ACM_SET_MODE_GRAIN_MERGE_MODE     52
#define  GAP_MOD_ACM_SET_MODE_HUE_MODE             53
#define  GAP_MOD_ACM_SET_MODE_SATURATION_MODE      54
#define  GAP_MOD_ACM_SET_MODE_COLOR_MODE           55
#define  GAP_MOD_ACM_SET_MODE_VALUE_MODE           56

#define  GAP_MOD_ACM_APPLY_FILTER_ON_LAYERMASK     57
#define  GAP_MOD_ACM_SEL_ALPHA                     58
#define  GAP_MOD_ACM_RESIZE_TO_IMG                 59

#define  GAP_MOD_ACM_CREATE_LAYER_FROM_OPACITY     60
#define  GAP_MOD_ACM_CREATE_LAYER_FROM_LMASK       61
#define  GAP_MOD_ACM_CREATE_LAYER_FROM_ALPHA       62

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

gint32  gap_mod_frames_modify(GapAnimInfo *ainfo_ptr,
                   long range_from, long range_to,
                   gint32 action_mode, gint32 sel_mode,
                   gint32 sel_case, gint32 sel_invert,
                   char *sel_pattern, char *new_layername,
                   GtkWidget *progress_bar,
                   gboolean *run_flag);

#endif
