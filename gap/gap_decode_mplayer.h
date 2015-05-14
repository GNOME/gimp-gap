/* gap_decode_mplayer.h
 * 2004.12.06 hof (Wolfgang Hofer)
 *
 * GAP ... Gimp Animation Plugins
 *         Call mplayer
 *         To split any mplayer supported video into
 *         video frames (single images on disk)
 *         Audio Tracks can also be extracted.
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
 * gimp-gap    2.1;     2004/12/06  hof: created
 */

#ifndef _GAP_MPLAYER_H
#define _GAP_MPLAYER_H

#include "libgimp/gimp.h"

#define GAP_MPLAYER_HELP_ID              "plug-in-gap-mplayer-decode"
#define GAP_MPLAYER_PLUGIN_NAME          "plug_in_gap_mplayer_decode"
#define GAP_MPLAYER_PLUGIN_NAME_TOOLBOX  "plug_in_gap_mplayer_decode_toolbox"

typedef enum
{ 
   MPENC_XCF     /* no direct support by mplayer, have to convert */
 , MPENC_PNG
 , MPENC_JPEG
} GapMPlayerFormats;


typedef struct GapMPlayerParams    /* nick: gpp */
{
   GimpRunMode run_mode;
   gint32    start_hour;
   gint32    start_minute;
   gint32    start_second;
   gint32    number_of_frames;
   
   char                video_filename[1024];
   char                audio_filename[1024];
   char                basename[1024];
   GapMPlayerFormats   img_format;
   gint32              vtrack;          /* 0 no video, 1 upto n: track number */
   gint32              atrack;          /* 0 no audio, 1 upto n: track number */
   gint32              jpg_quality;     /* 0 upto 100 (100 is best) */
   gint32              jpg_optimize;    /* 0 upto 100 */
   gint32              jpg_smooth;      /* 0 upto 100 */
   gboolean            jpg_progressive;
   gboolean            jpg_baseline;
   gint32              png_compression;  /* 0 upto 9 (9 max_compression) */
   
   gboolean            silent;
   gboolean            autoload;
   gboolean            run_mplayer_asynchron;
   
   gchar              *mplayer_prog;
   gchar              *mplayer_working_dir;

   gboolean            use_old_mplayer1_syntax;
} GapMPlayerParams;       



gint32   gap_mplayer_decode(GapMPlayerParams *gpp);

#endif


