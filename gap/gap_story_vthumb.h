/*  gap_story_vthumb.h
 *
 *  This module handles GAP storyboard dialog video thumbnail (vthumb)
 *  processing.
 *  video thumbnails are thumbnails of relevant frames (typically
 *  the first referenced framenumber in a video, section or anim image)
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
 * version 1.3.26a; 2007/10/06  hof: created
 */

#ifndef _GAP_STORY_VTHUMB_H
#define _GAP_STORY_VTHUMB_H

#include "libgimp/gimp.h"
#include "gap_story_main.h"


void     gap_story_vthumb_debug_fprint_videolist(FILE *fp, GapStoryVTResurceElem *video_list, GapVThumbElem *vthumb_list);
void     gap_story_vthumb_close_videofile(GapStbMainGlobalParams *sgpp);
void     gap_story_vthumb_open_videofile(GapStbMainGlobalParams *sgpp
                     , const char *filename
                     , gint32 seltrack
                     , const char *preferred_decoder
                     );


GapStoryVTResurceElem *  gap_story_vthumb_get_velem_movie(GapStbMainGlobalParams *sgpp
                              ,const char *video_filename
                              ,gint32 seltrack
                              ,const char *preferred_decoder
                              );
GapStoryVTResurceElem *  gap_story_vthumb_get_velem_no_movie(GapStbMainGlobalParams *sgpp
                              ,GapStoryBoard *stb
                              ,GapStoryElem *stb_elem
                              );
guchar *             gap_story_vthumb_create_generic_vthumb(GapStbMainGlobalParams *sgpp
                              , GapStoryBoard *stb
                              , GapStorySection *section
                              , GapStoryElem *stb_elem
                              , gint32 framenumber
                              , gint32 *th_bpp
                              , gint32 *th_width
                              , gint32 *th_height
                              , gboolean do_scale
                              );

GapVThumbElem *      gap_story_vthumb_add_vthumb(GapStbMainGlobalParams *sgpp
                              ,gint32 framenr
                              ,guchar *th_data
                              ,gint32 th_width
                              ,gint32 th_height
                              ,gint32 th_bpp
                              ,gint32 video_id
                              );



GapVThumbElem *     gap_story_vthumb_elem_fetch(GapStbMainGlobalParams *sgpp
                             ,GapStoryBoard *stb
                             ,GapStoryElem *stb_elem
                             ,gint32 framenr
                             ,gint32 seltrack
                             ,const char *preferred_decoder
                             );

guchar *            gap_story_vthumb_fetch_thdata(GapStbMainGlobalParams *sgpp
                            ,GapStoryBoard *stb
                            ,GapStoryElem *stb_elem
                            ,gint32 framenr
                            ,gint32 seltrack
                            ,const char *preferred_decoder
                            , gint32 *th_bpp
                            , gint32 *th_width
                            , gint32 *th_height
                            );


guchar *            gap_story_vthumb_fetch_thdata_no_store(GapStbMainGlobalParams *sgpp
                             ,GapStoryBoard *stb
                             ,GapStoryElem *stb_elem
                             ,gint32 framenr
                             ,gint32 seltrack
                             ,const char *preferred_decoder
                             , gint32   *th_bpp
                             , gint32   *th_width
                             , gint32   *th_height
                             , gboolean *file_read_flag
                             , gint32   *video_id
                             );

void               gap_story_vthumb_g_main_context_iteration(GapStbMainGlobalParams *sgpp);

#endif

