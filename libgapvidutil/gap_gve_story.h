/* gap_gve_story.h
 *
 *  GAP common encoder tool procedures
 *  for storyboard processing and rendering.
 *
 */

/*
 * 2006.06.25  hof  - moved functionality to gap/gap_story_render_processor
 * 2005.01.12  hof  - audio processing has new flag: create_audio_tmp_files
 *                  - no more checks and constraints for valid video/audio ranges
 *                    adressing illegal range delivers black frames (or audio silence)
 * 2004.07.24  hof  - added step_density parameter
 * 2003.05.29  hof  - gap_gve_story_fetch_composite_image_or_chunk (for dont_recode_flag)
 * 2003.04.16  hof  - Support deinterlace and exact_seek features for VID_PLAY_MOVIE
 * 2003.03.08  hof  - buffered audio processing
 * 2003.02.04  hof  - added Videofile (MOVIE) support
 * 2002.11.16  hof  - created procedures for storyboard file processing
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


#ifndef GAP_GVE_STORY_H
#define GAP_GVE_STORY_H

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
#include "gap_story_render_processor.h"
#include "gap_frame_fetcher.h"


/* --------------------------*/
/* Data Type DLARATIONS      */
/* --------------------------*/
/* old type names are still supported via defines */

#define GapGveStoryFrameType            GapStoryRenderFrameType
#define GapGveStoryAudioType            GapStoryRenderAudioType
#define GapGveStoryImageCacheElem       GapStoryRenderImageCacheElem
#define GapGveStoryImageCache           GapStoryRenderImageCache
#define GapGveStoryAudioCacheElem       GapStoryRenderAudioCacheElem
#define GapGveStoryAudioCache           GapStoryRenderAudioCache
#define GapGveStoryFrameRangeElem       GapStoryRenderFrameRangeElem
#define GapGveStoryAudioRangeElem       GapStoryRenderAudioRangeElem
#define GapGveStoryVTrackAttrElem       GapStoryRenderVTrackAttrElem
#define GapGveStoryVTrackArray          GapStoryRenderVTrackArray
#define GapGveStoryErrors               GapStoryRenderErrors
#define GapGveStoryVidHandle            GapStoryRenderVidHandle


/* --------------------------*/
/* PROCEDURE DECLARATIONS    */
/* --------------------------*/
/* old procedure names are still supported via defines */

#define gap_gve_story_debug_print_framerange_list           gap_story_render_debug_print_framerange_list
#define gap_gve_story_debug_print_audiorange_list           gap_story_render_debug_print_audiorange_list
#define gap_gve_story_drop_image_cache                      gap_frame_fetch_drop_resources
#define gap_gve_story_remove_tmp_audiofiles                 gap_story_render_remove_tmp_audiofiles
#define gap_gve_story_drop_audio_cache                      gap_story_render_drop_audio_cache
#define gap_gve_story_fetch_composite_image                 gap_story_render_fetch_composite_image
#define gap_gve_story_fetch_composite_image_or_chunk        gap_story_render_fetch_composite_image_or_chunk
#define gap_gve_story_open_vid_handle                       gap_story_render_open_vid_handle
#define gap_gve_story_open_extended_video_handle            gap_story_render_open_extended_video_handle
#define gap_gve_story_close_vid_handle                      gap_story_render_close_vid_handle
#define gap_gve_story_set_audio_resampling_program          gap_story_render_set_audio_resampling_program
#define gap_gve_story_calc_audio_playtime                   gap_story_render_calc_audio_playtime
#define gap_gve_story_create_composite_audiofile            gap_story_render_create_composite_audiofile


#endif        /* GAP_GVE_STORY_H */
