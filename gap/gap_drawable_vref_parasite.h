/* gap_drawable_vref_parasite.h
 *
 *  This module handles gap specific video reference drawable parasites.
 *  Such parasites are typically used to identify extracted video file frames
 *  for usage in filtermacro as persistent drawable_id references at recording time of filtermacros.
 *
 *  The gap player does attach such vref drawable parasites (as temporary parasites)
 *  when extracting a videoframe at original size (by click on the preview)
 *
 * Copyright (C) 2008 Wolfgang Hofer <hof@gimp.org>
 *
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

#ifndef _GAP_DRAWABLE_VREF_PARASITE_H
#define _GAP_DRAWABLE_VREF_PARASITE_H

#include "libgimp/gimp.h"
#include "libgimp/gimp.h"
#include "gap_lib_common_defs.h"


#define GAP_DRAWABLE_VIDEOFILE_PARASITE_NAME   "gap-video-file"
#define GAP_DRAWABLE_VIDEOPARAMS_PARASITE_NAME "gap-video-param"

typedef struct GapDrawableVideoParasite {
  char   preferred_decoder[16];
  gint32 framenr;
  gint32 seltrack;
} GapDrawableVideoParasite;

typedef struct GapDrawableVideoRef { /* nickname:  dvref */
  char                      *videofile;
  GapDrawableVideoParasite   para;
} GapDrawableVideoRef;


GapDrawableVideoRef *   gap_dvref_get_drawable_video_reference_via_parasite(gint32 drawable_id);
void                    gap_dvref_assign_videoref_parasites(GapDrawableVideoRef *dvref, gint32 layer_id);

void                    gap_dvref_free(GapDrawableVideoRef **dvref);
void                    gap_dvref_debug_print_GapDrawableVideoRef(GapDrawableVideoRef *dvref);

#endif
