/*  gap_story_syntax.h
 *
 *  This module handles GAP storyboard file syntax list for
 *  named parameters parsing support of storyboard level1 files
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
 * version 2.1.0a;     2004/04/24  hof: created
 */

#ifndef _GAP_STORY_SYNTAX_H
#define _GAP_STORY_SYNTAX_H

#include "libgimp/gimp.h"


/* record keynames in Storyboard files */
#define GAP_STBKEY_STB_HEADER               "STORYBOARDFILE"
#define GAP_STBKEY_CLIP_HEADER              "CLIPLISTFILE"
#define GAP_STBKEY_LAYOUT_SIZE              "LAYOUT_SIZE"
#define GAP_STBKEY_EDIT_SETTINGS            "EDIT_SETTINGS"

#define GAP_STBKEY_VID_MASTER_SIZE          "VID_MASTER_SIZE"
#define GAP_STBKEY_VID_MASTER_FRAMERATE     "VID_MASTER_FRAMERATE"
#define GAP_STBKEY_VID_MASTER_FRAME_ASPECT  "VID_MASTER_FRAME_ASPECT"
#define GAP_STBKEY_VID_MASTER_LAYERSTACK    "VID_MASTER_LAYERSTACK"
#define GAP_STBKEY_VID_PREFERRED_DECODER    "VID_PREFERRED_DECODER"
#define GAP_STBKEY_VID_MASTER_INSERT_ALPHA  "VID_MASTER_INSERT_ALPHA"
#define GAP_STBKEY_VID_MASTER_INSERT_AREA   "VID_MASTER_INSERT_AREA"

#define GAP_STBKEY_MAIN_SECTION             "MAIN_SECTION"
#define GAP_STBKEY_SUB_SECTION              "SUB_SECTION"

#define GAP_STBKEY_VID_PLAY_SECTION         "VID_PLAY_SECTION"
#define GAP_STBKEY_VID_PLAY_BLACKSECTION    "VID_PLAY_BLACKSECTION"
#define GAP_STBKEY_VID_PLAY_MOVIE           "VID_PLAY_MOVIE"
#define GAP_STBKEY_VID_PLAY_FRAMES          "VID_PLAY_FRAMES"
#define GAP_STBKEY_VID_PLAY_ANIMIMAGE       "VID_PLAY_ANIMIMAGE"
#define GAP_STBKEY_VID_PLAY_IMAGE           "VID_PLAY_IMAGE"
#define GAP_STBKEY_VID_PLAY_COLOR           "VID_PLAY_COLOR"
#define GAP_STBKEY_VID_SILENCE              "VID_SILENCE"
#define GAP_STBKEY_VID_ROTATE               "VID_ROTATE"
#define GAP_STBKEY_VID_OPACITY              "VID_OPACITY"
#define GAP_STBKEY_VID_ZOOM_X               "VID_ZOOM_X"
#define GAP_STBKEY_VID_ZOOM_Y               "VID_ZOOM_Y"
#define GAP_STBKEY_VID_MOVE_X               "VID_MOVE_X"
#define GAP_STBKEY_VID_MOVE_Y               "VID_MOVE_Y"
#define GAP_STBKEY_VID_FIT_SIZE             "VID_FIT_SIZE"
#define GAP_STBKEY_VID_OVERLAP              "VID_OVERLAP"
#define GAP_STBKEY_VID_MOVEPATH             "VID_MOVEPATH"

#define GAP_STBKEY_MASK_IMAGE               "MASK_IMAGE"
#define GAP_STBKEY_MASK_ANIMIMAGE           "MASK_ANIMIMAGE"
#define GAP_STBKEY_MASK_FRAMES              "MASK_FRAMES"
#define GAP_STBKEY_MASK_MOVIE               "MASK_MOVIE"
#define GAP_STBKEY_MASK_BLACKSECTION        "MASK_BLACKSECTION"


#define GAP_STBKEY_AUD_MASTER_VOLUME        "AUD_MASTER_VOLUME"
#define GAP_STBKEY_AUD_MASTER_SAMPLERATE    "AUD_MASTER_SAMPLERATE"
#define GAP_STBKEY_AUD_PLAY_SOUND           "AUD_PLAY_SOUND"
#define GAP_STBKEY_AUD_PLAY_MOVIE           "AUD_PLAY_MOVIE"
#define GAP_STBKEY_AUD_SILENCE              "AUD_SILENCE"

#define GAP_STB_VTRACK1_TOP_TRUE  "TOP"
#define GAP_STB_VTRACK1_TOP_FALSE "BACKGROUND"

typedef struct GapStbSyntaxParnames
{
  const char* parname[30];
  gint   tabsize;
} GapStbSyntaxParnames;


gint           gap_stb_syntax_get_parname_idx(const char *record_key
                 ,const char *parname
                 );

char*          gap_stb_syntax_get_parname(const char *record_key
                 ,gint par_idx
                 );
void           gap_stb_syntax_get_parname_tab(const char *record_key
                 ,GapStbSyntaxParnames *par_tab
                 );

#endif
