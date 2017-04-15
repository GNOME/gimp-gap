/*  gap_vin.h
 *
 *  This module handles GAP video info files (_vin.gap)
 *  _vin.gap files store global informations about an animation
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
 * version 2.1.0a;  2004/06/03  hof: added onionskin setting ref_mode
 * version 1.3.16c; 2003/07/09  hof: support onionskin settings in video_info files
 * version 1.3.14a; 2003/05/24  hof: created (splitted off from gap_pdb_calls module)
 */

#ifndef _GAP_VIN_H
#define _GAP_VIN_H

#include "libgimp/gimp.h"
#include "gap_val_file.h"

#define GAP_ACTIVE_LAYER_TRACKING_OFF          0
#define GAP_ACTIVE_LAYER_TRACKING_BY_NAME      1
#define GAP_ACTIVE_LAYER_TRACKING_BY_STACKPOS  2

typedef struct GapVinVideoInfo {
  gdouble     framerate;    /* playback rate in frames per second */
  gint32      timezoom;

  gint32      active_layer_tracking;

  /* stuff for onionskin layers */
  gboolean onionskin_auto_enable;     /* master switch for onionskin load/save triggers */
  gboolean auto_replace_after_load;
  gboolean auto_delete_before_save;
   
   
  gint32  num_olayers;      /* Number of Onion Layers  1 .. 10 Default: 1 */
  gint32  ref_mode;         /* 0: NORMAL, 1:BIDRIECTIONAL_SINGLE 2:BIDRIECTIONAL_DOUBLE */
  gint32  ref_delta;        /* Reference Frame Delta:  +- 1 ... n  Default: -1 */
  gint32  ref_cycle;        /* Reference is Cycle   : TRUE/FALSE   Default: TRUE
                             *    TRUE .. last frame has frame 0 as next frame
                             */
  gint32  stack_pos;        /* Place OnionLayer(s) on Stackposition 0..n Default: 1 */
  gint32  stack_top;        /* TRUE Stack Position is relative from TOP
                             * FALSE Stack Position is relative from Bottom (Default: FALSE) */
  gdouble opacity;          /* OnionOpacity: 0.0..100.0%  Default: 50 % */
  gdouble opacity_delta;    /* OnionOpacityDelta: 0..100%  Default: 80 %
                             * (2nd Layer has 80% of 50%)
                             */
  gint32  ignore_botlayers; /* Ignore N Bottom Sourcelayers Default: 1
                             *  (0 .. Onion Layer is built from all Src Layers)
                             *  (2 .. Layers are ignored,  Background and next layer)
                             */
  gint32  select_mode;       /* Mode how to identify a layer: -1 Pattern off,  0-3 by layername 0=equal, 1=prefix, 2=suffix, 3=contains */
  gint32  select_case;
  gint32  select_invert;
  gchar   select_string[512];

  gboolean asc_opacity;    /* TRUE: the far neighbour frames have higher opacity
                            * FALSE: near neighbour frames have higher opacity (DEFAULT)
                            */
  gint32  layermask_mode;  /* onionskin layermask creation 0: NONE, 1:SELECTION, 2:BLACK, 3:WHITE */
  gint32  active_mode;     /* 0 keep active layer, 1 set oinon layer active, 2 set onion layermask active */

} GapVinVideoInfo;



char *gap_vin_alloc_name(char *basename);
int   gap_vin_set_common(GapVinVideoInfo *vin_ptr, char *basename);
int   gap_vin_set_common_onion(GapVinVideoInfo *vin_ptr, char *basename);
GapVinVideoInfo *gap_vin_get_all(char *basename);

#endif
