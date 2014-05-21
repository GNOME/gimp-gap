/* gap_story_render_processor.h
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


#ifndef GAP_STORY_RENDER_PROCESSOR_H
#define GAP_STORY_RENDER_PROCESSOR_H

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
#include "gap_story_render_types.h"

/* G_DIR_SEPARATOR (is defined in glib.h if you have glib-1.2.0 or later) */
#ifdef G_OS_WIN32

/* Filenames in WIN/DOS Style */
#ifndef G_DIR_SEPARATOR
#define G_DIR_SEPARATOR '\\'
#endif
#define DIR_ROOT ':'

#else  /* !G_OS_WIN32 */

/* Filenames in UNIX Style */
#ifndef G_DIR_SEPARATOR
#define G_DIR_SEPARATOR '/'
#endif
#define DIR_ROOT '/'

#endif /* !G_OS_WIN32 */



#define GAP_STB_RENDER_GVA_FRAMES_TO_KEEP_CACHED 36


#define GAP_VID_ENC_SAVE_MULTILAYER   "GAP_VID_ENC_SAVE_MULTILAYER"
#define GAP_VID_ENC_SAVE_FLAT         "GAP_VID_ENC_SAVE_FLAT"
#define GAP_VID_ENC_MONITOR           "GAP_VID_ENC_MONITOR"

#define GAP_VID_CHCHK_FLAG_SIZE               1
#define GAP_VID_CHCHK_FLAG_MPEG_INTEGRITY     2
#define GAP_VID_CHCHK_FLAG_JPG                4
#define GAP_VID_CHCHK_FLAG_VCODEC_NAME        8
#define GAP_VID_CHCHK_FLAG_FULL_FRAME        16
#define GAP_VID_CHCHK_FLAG_PNG               32

/* codec name list element
 */
typedef struct GapCodecNameElem {
  gchar *codec_name;
  gint32 video_id;
  
  void  *next;
}  GapCodecNameElem;

typedef enum GapStoryFetchResultEnum
{
    GAP_STORY_FETCH_RESULT_IS_IMAGE              = 1
,   GAP_STORY_FETCH_RESULT_IS_RAW_RGB888         = 2
,   GAP_STORY_FETCH_RESULT_IS_COMPRESSED_CHUNK   = 3
,   GAP_STORY_FETCH_RESULT_IS_ERROR              = -1
} GapStoryFetchResultEnum;


/* The GapStoryFetchResult represents the fetched composite video frame
 * Depending on the flags dont_recode_flag and enable_rgb888_flag
 * the delivered frame can have one of the types
 * that are defined in GapStoryFetchResultEnum.
 *
 * Note that the caller of the fetch procedure can already provide
 * allocated memory for the buffers  raw_rgb_data and video_frame_chunk_data.
 * (in this case the caler is responsible to allocate the buffers large enough
 * to hold one uncompressed frame in rgb888 colormodel representation)
 *
 * in case raw_rgb_data or video_frame_chunk_data is NULL the buffer is automatically
 * allocated in correct size when needed. (this is done by the fetch procedures 
 * GVA_search_fcache_and_get_frame_as_gimp_layer_or_rgb888
 * gap_story_render_fetch_composite_image_or_chunk )
 */
typedef struct GapStoryFetchResult {
  GapStoryFetchResultEnum resultEnum;
  
  /* GAP_STORY_FETCH_RESULT_IS_IMAGE */
  gint32    layer_id;        /* Id of the only layer in the composite image */
  gint32    image_id;        /* output: Id of the only layer in the composite image */
  
  /* GAP_STORY_FETCH_RESULT_IS_RAW_RGB888 */
  unsigned  char    *raw_rgb_data;          /* raw data RGB888 at full size height * width * 3 */
  
  
  /* GAP_STORY_FETCH_RESULT_IS_COMPRESSED_CHUNK */
  gboolean  force_keyframe;                 /* the calling encoder should encode an I-Frame when true */
  gint32    video_frame_chunk_size;         /* total size of frame (may include a videoformat specific frameheader) */
  gint32    video_frame_chunk_hdr_size;     /* size of videoformat specific frameheader (0 if has no hdr) */
  unsigned  char *video_frame_chunk_data;   /* copy of the already compressed video frame from source video */

} GapStoryFetchResult;


/* --------------------------*/
/* PROCEDURE DECLARATIONS    */
/* --------------------------*/

void     gap_story_render_debug_print_framerange_list(GapStoryRenderFrameRangeElem *frn_list
                             , gint32 track                    /* -1 show all tracks */
                             );
void     gap_story_render_debug_print_audiorange_list(GapStoryRenderAudioRangeElem *aud_list
                             , gint32 track                    /* -1 show all tracks */
                             );


void     gap_story_render_drop_image_cache(void);

void     gap_story_render_remove_tmp_audiofiles(GapStoryRenderVidHandle *vidhand);
void     gap_story_render_drop_audio_cache(void);



/* ----------------------------------------------------
 * gap_story_render_fetch_composite_image (simple API)
 * ----------------------------------------------------
 * fetch composite VIDEO Image at a given master_frame_nr
 * within a storyboard framerange list.
 *
 * the returned image is flattend RGB and scaled to
 * desired video framesize.
 *
 *  it is a merged result of all video tracks,
 *
 *  frames at master_frame_nr were loaded
 *  for all video tracks and added to the composite image
 *   (track 0 on top, track N on bottom
 *    of the layerstack)
 *  opacity, scaling and move (decenter) attributes
 *  were set to according to current Video Attributes.
 *
 * an (optional) filtermacro_file is performed on the
 * composite image.
 *
 * (simple animations without a storyboard file
 *  are represented by a short storyboard framerange list that has
 *  just one element entry at track 1).
 *
 * return image_id of resulting image and the flattened resulting layer_id
 */
gint32   gap_story_render_fetch_composite_image(GapStoryRenderVidHandle *vidhand
                    , gint32 master_frame_nr  /* starts at 1 */
                    , gint32  vid_width       /* desired Video Width in pixels */
                    , gint32  vid_height      /* desired Video Height in pixels */
                    , char *filtermacro_file  /* NULL if no filtermacro is used */
                    , gint32 *layer_id        /* output: Id of the only layer in the composite image */
                 );



/* ------------------------------------------------------------------------
 * gap_story_render_fetch_composite_image_or_buffer_or_chunk (extended API)
 * ------------------------------------------------------------------------
 *
 * fetch composite VIDEO frame at a given master_frame_nr
 * within a storyboard framerange list.
 *
 * on success the result can be delivered in one of those types:
 *   GAP_STORY_FETCH_RESULT_IS_IMAGE
 *   GAP_STORY_FETCH_RESULT_IS_RAW_RGB888
 *   GAP_STORY_FETCH_RESULT_IS_COMPRESSED_CHUNK
 *
 * The delivered data type depends on the flags:
 *   dont_recode_flag
 *   enable_rgb888_flag
 *
 * In case all of those flags are FALSE, the caller can always expect
 * a gimp image (GAP_STORY_FETCH_RESULT_IS_IMAGE) as result on success.
 *
 * Encoders that can handle RGB888 colormdel can set the enable_rgb888_flag
 *
 *   If the enable_rgb888_flag is TRUE and the refered frame can be copied
 *   without render transitions from only one input video clip
 *   then the render engine is bypassed, and the result will be of type 
 *   GAP_STORY_FETCH_RESULT_IS_RAW_RGB888 for this frame.
 *   (this speeds up encoding of simple 1:1 copied video clip frames
 *   because the converting from rgb88 to gimp drawable and back to rgb88
 *   can be skipped in this special case)
 *   
 *
 * Encoders that support lossless video cut can set the dont_recode_flag.
 *
 *   if the dont_recode_flag is TRUE, the render engine is also bypassed where
 *   a direct fetch of the (already compressed) Frame chunk from an input videofile
 *   is possible for the master_frame_nr.
 *   (in case there are any transitions or mix with other input channels
 *   or in case the input is not an mpeg encoded video file it is not possible to 
 *   make a lossless copy of the input frame data)
 *
 *   Restriction: current implementation provided lossless cut only for MPEG1 and MPEG2
 *
 *
 * the compressed fetch depends on following conditions:
 * - dont_recode_flag == TRUE
 * - there is only 1 videoinput track at this master_frame_nr
 * - the videodecoder must support a read_video_chunk procedure
 *   (libmpeg3 has this support, for the libavformat the support is available vie the gap video api)
 *    TODO: for future releases should also check for the same vcodec_name)
 * - the videoframe must match 1:1 in size
 * - there are no transformations (opacity, offsets ....)
 * - there are no filtermacros to perform on the fetched frame
 *
 * check_flags:
 *   force checks if corresponding bit value is set. Supportet Bit values are:
 *      GAP_VID_CHCHK_FLAG_SIZE               check if width and height are equal
 *      GAP_VID_CHCHK_FLAG_MPEG_INTEGRITY     checks for MPEG P an B frames if the sequence of fetched frames
 *                                                   also includes the refered I frame (before or after the current
 *                                                   handled frame)
 *      GAP_VID_CHCHK_FLAG_JPG                check if fetched cunk is a jpeg encoded frame.
 *                                                  (typical for MPEG I frames)
 *      GAP_VID_CHCHK_FLAG_VCODEC_NAME        check for a compatible vcodec_name
 *
 *
 * The resulting frame is deliverd into the GapStoryFetchResult struct.
 *
 *   Note that the caller of the fetch procedure can already provide
 *   allocated memory for the buffers  raw_rgb_data and video_frame_chunk_data.
 *   (in this case the caler is responsible to allocate the buffers large enough
 *   to hold one uncompressed frame in rgb888 colormodel representation)
 *
 *   in case raw_rgb_data or video_frame_chunk_data is NULL the buffer is automatically
 *   allocated in correct size when needed.
 */
void  gap_story_render_fetch_composite_image_or_buffer_or_chunk(GapStoryRenderVidHandle *vidhand
                    , gint32 master_frame_nr  /* starts at 1 */
                    , gint32  vid_width       /* desired Video Width in pixels */
                    , gint32  vid_height      /* desired Video Height in pixels */
                    , char *filtermacro_file  /* NULL if no filtermacro is used */
                    , gboolean dont_recode_flag                /* IN: TRUE try to fetch comressed chunk if possible */
                    , gboolean enable_rgb888_flag              /* IN: TRUE deliver result already converted to rgb buffer */
                    , GapCodecNameElem *vcodec_list            /* IN: list of video_codec names that are compatible to the calling encoder program */
                    , gint32 video_frame_chunk_maxsize         /* IN: sizelimit (larger chunks are not fetched) */
                    , gdouble master_framerate
                    , gint32  max_master_frame_nr              /* the number of frames that will be encoded in total */
                    , gint32  check_flags                      /* IN: combination of GAP_VID_CHCHK_FLAG_* flag values */
                    , GapStoryFetchResult *gapStoryFetchResult
                 );

//////////////////////
gboolean gap_story_render_fetch_composite_image_or_chunk(GapStoryRenderVidHandle *vidhand  //// DEPRECATED
                    , gint32 master_frame_nr  /* starts at 1 */
                    , gint32  vid_width       /* desired Video Width in pixels */
                    , gint32  vid_height      /* desired Video Height in pixels */
                    , char *filtermacro_file  /* NULL if no filtermacro is used */
                    , gint32 *layer_id        /* output: Id of the only layer in the composite image */

                    , gint32 *image_id        /* output: Id of the only layer in the composite image */
                    , gboolean dont_recode_flag                /* IN: TRUE try to fetch comressed chunk if possible */
                    , GapCodecNameElem *vcodec_list            /* IN: list of video_codec names that are compatible to the calling encoder program */
                    , gboolean *force_keyframe                 /* OUT: the calling encoder should encode an I-Frame */
                    , unsigned char *video_frame_chunk_data    /* OUT: copy of the already compressed video frame from source video */
                    , gint32 *video_frame_chunk_size           /* OUT: total size of frame (may include a videoformat specific frameheader) */
                    , gint32 video_frame_chunk_maxsize         /* IN: sizelimit (larger chunks are not fetched) */
                    , gdouble master_framerate
                    , gint32  max_master_frame_nr   /* the number of frames that will be encoded in total */
                    
                    
                    , gint32 *video_frame_chunk_hdr_size       /* OUT: size of videoformat specific frameheader (0 if has no hdr) */
                    , gint32 check_flags                       /* IN: combination of GAP_VID_CHCHK_FLAG_* flag values */
                 );



GapStoryRenderVidHandle *  gap_story_render_open_vid_handle_from_stb(
                           GapStoryBoard *stb_ptr
                          ,gint32 *frame_count   /* output total frame_count , or 0 on failure */
                          );

GapStoryRenderVidHandle *  gap_story_render_open_vid_handle(
                           GapLibTypeInputRange input_mode
                          ,gint32 image_id 
                          ,const char *storyboard_file
                          ,const char *basename
                          ,const char *ext
                          ,gint32  frame_from
                          ,gint32  frame_to
                          ,gint32 *frame_count   /* output total frame_count , or 0 on failure */
                          );

GapStoryRenderVidHandle *  gap_story_render_open_extended_video_handle(
                           gboolean ignore_audio
                          ,gboolean igore_video
                          ,gboolean create_audio_tmp_files
                          ,gdouble  *progress_ptr
                          ,char *status_msg
                          ,gint32 status_msg_len
                          ,GapLibTypeInputRange input_mode
                          ,const char *imagename
                          ,const char *storyboard_file
                          ,const char *basename
                          ,const char *ext
                          ,gint32  frame_from
                          ,gint32  frame_to
                          ,gint32 *frame_count   /* output total frame_count , or 0 on failure */
                          ,char *util_sox
                          ,char *util_sox_options
                          );

void    gap_story_render_close_vid_handle(GapStoryRenderVidHandle *vidhand);
void    gap_story_render_set_audio_resampling_program(
                           GapStoryRenderVidHandle *vidhand
                           , char *util_sox
                           , char *util_sox_options
                           );
void      gap_story_render_set_stb_error(GapStoryRenderErrors *sterr, char *errtext);
void      gap_story_render_set_stb_warning(GapStoryRenderErrors *sterr, char *errtext);
void      gap_story_render_calc_audio_playtime(GapStoryRenderVidHandle *vidhand, gdouble *aud_total_sec);
gboolean  gap_story_render_create_composite_audiofile(GapStoryRenderVidHandle *vidhand
                            , char *comp_audiofile
                            );
guchar *  gap_story_convert_layer_to_RGB_thdata(gint32 l_layer_id, gint32 *RAW_size
                            , gint32 *th_bpp
                            , gint32 *th_width
                            , gint32 *th_height
                            );
guchar *  gap_story_render_fetch_composite_vthumb(GapStoryRenderVidHandle *stb_comp_vidhand
                            , gint32 framenumber
                            , gint32 width, gint32 height
                            );

gint32    gap_story_render_transform_with_movepath_processing(gint32 comp_image_id
                         , gint32 tmp_image_id  /* must contain layer_id */
                         , gint32 layer_id
                         , gboolean keep_proportions
                         , gboolean fit_width
                         , gboolean fit_height
                         , gdouble rotate     /* rotation in degree */
                         , gdouble opacity    /* 0.0 upto 1.0 */
                         , gdouble scale_x    /* 0.0 upto 10.0 where 1.0 = 1:1 */
                         , gdouble scale_y    /* 0.0 upto 10.0 where 1.0 = 1:1 */
                         , gdouble move_x     /* -1.0 upto +1.0 where 0 = no move, -1 is left outside */
                         , gdouble move_y     /* -1.0 upto +1.0 where 0 = no move, -1 is top outside */
                         , const char *movepath_file_xml
                         , gdouble movepath_framePhase
                         );

#endif        /* GAP_STORY_RENDER_PROCESSOR_H */
