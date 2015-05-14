/* gap_story_render_audio.h
 *
 *  GAP storyboard rendering processor.
 *
 */

/*
 * 2006.06.25  hof  - created (moved stuff from the former gap_gve_story modules to this  new module)
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

#ifndef GAP_STORY_RENDER_AUDIO_H
#define GAP_STORY_RENDER_AUDIO_H

#include "libgimp/gimp.h"

#ifdef GAP_ENABLE_VIDEOAPI_SUPPORT
#include "gap_vid_api.h"
#else
#ifndef GAP_STUBTYPE_GVA_HANDLE
typedef gpointer t_GVA_Handle;
#define GAP_STUBTYPE_GVA_HANDLE
#endif
#endif

#include "gap_story_file.h"
#include "gap_lib_common_defs.h"
#include "gap_story_render_processor.h"

void      gap_story_render_audio_calculate_playtime(GapStoryRenderVidHandle *vidhand, gdouble *aud_total_sec);

gboolean  gap_story_render_audio_create_composite_audiofile(GapStoryRenderVidHandle *vidhand
                            , char *comp_audiofile
                            );


GapStoryRenderAudioRangeElem * gap_story_render_audio_new_audiorange_element(GapStoryRenderAudioType  aud_type
                      ,gint32 track
                      ,const char *audiofile
                      ,gint32  master_samplerate
                      ,gdouble play_from_sec
                      ,gdouble play_to_sec
                      ,gdouble volume_start
                      ,gdouble volume
                      ,gdouble volume_end
                      ,gdouble fade_in_sec
                      ,gdouble fade_out_sec
                      ,char *util_sox
                      ,char *util_sox_options
                      ,const char *storyboard_file  /* IN: NULL if no storyboard file is used */
                      ,const char *preferred_decoder
                      ,GapStoryRenderAudioRangeElem *known_aud_list /* NULL or list of already known ranges */
                      ,GapStoryRenderErrors *sterr           /* element to store Error/Warning report */
                      ,gint32 seltrack      /* IN: select audiotrack number 1 upto 99 for GAP_AUT_MOVIE */
                      ,gboolean create_audio_tmp_files
                      ,gdouble min_play_sec
                      ,gdouble max_play_sec
                      ,GapStoryRenderVidHandle *vidhand  /* for progress */
                      );

void     gap_story_render_audio_add_aud_list(GapStoryRenderVidHandle *vidhand, GapStoryRenderAudioRangeElem *aud_elem);


#endif        /* GAP_STORY_RENDER_AUDIO_H */

