/* gap_story_render_types.h
 *
 *  types for the GAP storyboard rendering processor.
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


#ifndef GAP_STORY_RENDER_TYPES_H
#define GAP_STORY_RENDER_TYPES_H

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


#define GAP_STB_MAX_VID_TRACKS 20
#define GAP_STB_MAX_AUD_TRACKS 99
#define GAP_STB_MAX_VID_INTERNAL_TRACKS (GAP_STB_MAX_VID_TRACKS * 2)
#define GAP_STB_MASK_TRACK_NUMBER 0

/* the hidden mask track is reserved for internal use
 * (implicite copy of corresponding mask definitions
 * use this track number)
 */
#define GAP_STB_HIDDEN_MASK_TRACK_NUMBER -777

typedef enum
{
   GAP_MSK_ANCHOR_CLIP
  ,GAP_MSK_ANCHOR_MASTER
  ,GAP_MSK_ANCHOR_XCOLOR   /* apply to clip as colormask */
} GapStoryMaskAnchormode;

typedef enum
{
   GAP_FRN_SILENCE
  ,GAP_FRN_COLOR
  ,GAP_FRN_IMAGE
  ,GAP_FRN_ANIMIMAGE
  ,GAP_FRN_FRAMES
  ,GAP_FRN_MOVIE
  ,GAP_FRN_SECTION
} GapStoryRenderFrameType;


typedef enum
{
   GAP_AUT_SILENCE
  ,GAP_AUT_AUDIOFILE
  ,GAP_AUT_MOVIE
} GapStoryRenderAudioType;


typedef struct GapStoryRenderImageCacheElem
{
   gint32  image_id;
   char   *filename;
   void *next;
} GapStoryRenderImageCacheElem;

typedef struct GapStoryRenderImageCache
{
  GapStoryRenderImageCacheElem *ic_list;
  gint32            max_img_cache;  /* number of images to hold in the cache */
} GapStoryRenderImageCache;



typedef struct GapStoryRenderAudioCacheElem
{
   gint32  audio_id;
   char   *filename;
   guchar *aud_data;   /* full audiodata (including header) loaded in memory */
   gint32 aud_bytelength;

   gint32 segment_startoffset;
   gint32 segment_bytelength;

   void *next;
} GapStoryRenderAudioCacheElem;

typedef struct GapStoryRenderAudioCache
{
  GapStoryRenderAudioCacheElem *ac_list;
  gint32 nextval_audio_id;
  gint32            max_aud_cache;  /* number of images to hold in the cache */
} GapStoryRenderAudioCache;


/* forward declaration for GapStoryRenderMaskDefElem */
struct GapStoryRenderMaskDefElem;  /* nick: maskdef_elem */

typedef struct GapStoryRenderFrameRangeElem   /* nick: frn_elem */
{
   GapStoryRenderFrameType  frn_type;
   gint32  track;
   gint32  last_master_frame_access;
   char   *basename;
   char   *ext;
   t_GVA_Handle *gvahand;     /* API handle for videofile (for GAP_FRN_MOVIE) */
   gint32        seltrack;    /* selected videotrack in a videofile (for GAP_FRN_MOVIE) */
   gint32        exact_seek;  /* 0 fast seek, 1 exact seek (for GAP_FRN_MOVIE) */
   gdouble       delace;      /* 0.0 no deinterlace, 1.0-1.99 odd 2.0-2.99 even rows  (for GAP_FRN_MOVIE) */
   char   *filtermacro_file;
   char   *filtermacro_file_to;  /* additional macro with 2nd parameterset(s) for varying apply */
   gint32  fmac_total_steps;     /* total steps for varying filtermacro apply */
   gint32  fmac_accel;           /* acceleration characteristic for filtermacro apply with varying values */

   gdouble  frame_from;       /* internal frame number that is 1.st of range (float due to internal clip splitting) */
   gdouble  frame_to;         /* internal frame number that is the last handled frame of the range */
   gint32  frames_to_handle;
   gint32  delta;               /* +1 or -1 */
   gdouble step_density;        /* 1==normal stepsize 1:1   0.5 == each frame twice, 2.0 only every 2nd frame */
   gdouble red_f;
   gdouble green_f;
   gdouble blue_f;
   gdouble alpha_f;

   gboolean keep_proportions;
   gboolean fit_width;
   gboolean fit_height;

   gdouble  mask_framecount;   /* 1 or progress offset for splitted elements */
   gint32   flip_request;      /* 0 none, 1 flip horizontal, 2 flip vertical, 3 flip both */
   char    *mask_name;          /* optional reference to a layer mask */
   gdouble  mask_stepsize;
   GapStoryMaskAnchormode  mask_anchor;
   char   *colormask_file;    /* optional reference to a colormask parameter file
                               * relevant for ancor mode GAP_MSK_ANCHOR_CLIPCOLOR
                               * where mask is applied as colormask
                               */


   gdouble  wait_untiltime_sec;
   gint32   wait_untilframes;

   gdouble rotate_from;        /* -36000.0 upto 3600.0 degree */
   gdouble rotate_to;          /*  -36000.0 upto 3600.0 degree */
   gint32  rotate_dur;         /* number of frames to change from -> to value */

   gdouble opacity_from;       /* 0.0 upto 1.0 */
   gdouble opacity_to;         /* 0.0 upto 1.0 */
   gint32  opacity_dur;        /* number of frames to change from -> to value */

   gdouble scale_x_from;       /* 0.0 upto 10.0  where 1.0 is fit video size */
   gdouble scale_x_to;         /* 0.0 upto 10.0  where 1.0 is fit video size */
   gint32  scale_x_dur;        /* number of frames to change from -> to value */

   gdouble scale_y_from;       /* 0.0 upto 10.0  where 1.0 is fit video size */
   gdouble scale_y_to;         /* 0.0 upto 10.0  where 1.0 is fit video size */
   gint32  scale_y_dur;        /* number of frames to change from -> to value */

   gdouble move_x_from;        /* -1.0 upto 1.0 where 0 is center and -1.0 left outside */
   gdouble move_x_to;          /* -1.0 upto 1.0 where 0 is center and -1.0 left outside */
   gint32  move_x_dur;         /* number of frames to change from -> to value */

   gdouble move_y_from;        /* -1.0 upto 1.0 where 0 is center and -1.0 up outside */
   gdouble move_y_to;          /* -1.0 upto 1.0 where 0 is center and -1.0 up outside */
   gint32  move_y_dur;         /* number of frames to change from -> to value */

   gint    opacity_accel;        /* acceleration characteristic for opacity transformation */
   gint32  opacity_frames_done;  /* already processed frames since begin of transition */
   gint    move_x_accel;
   gint32  move_x_frames_done;
   gint    move_y_accel;
   gint32  move_y_frames_done;
   gint    scale_x_accel;
   gint32  scale_x_frames_done;
   gint    scale_y_accel;
   gint32  scale_y_frames_done;
   gint    rotate_accel;        /* acceleration characteristic for opacity transformation */
   gint32  rotate_frames_done;  /* already processed frames since begin of transition */

   gdouble movepath_from;        /* 1.0 to path length in frames */
   gdouble movepath_to;          /* 1.0 to path length in frames */
   gint32  movepath_frames_done;
   gint32  movepath_dur;         /* number of frames to change from -> to value */
   char   *movepath_file_xml;    /* XML parameter file for movepath transitions */

   void   *next;
} GapStoryRenderFrameRangeElem;  /* used for storyboard processing */


typedef struct GapStoryRenderAudioRangeElem
{
   GapStoryRenderAudioType  aud_type;
   gint32    track;
   char     *audiofile;
   char     *tmp_audiofile;
   t_GVA_Handle *gvahand;           /* API handle for videofile (for GAP_AUT_MOVIE) */
   gint32        seltrack;          /* selected audiotrack in a videofile (for GAP_AUT_MOVIE) */
   gint32   samplerate;             /* samples per sec */
   gint32   channels;               /* 1 mono, 2 stereo */
   gint32   bytes_per_sample;
   gint32   samples;
   gdouble  max_playtime_sec;

   gdouble  wait_untiltime_sec;
   gint32   wait_until_samples;

   gdouble  range_playtime_sec;
   gdouble  play_from_sec;
   gdouble  play_to_sec;
   gdouble  volume_start;
   gdouble  volume;
   gdouble  volume_end;
   gdouble  fade_in_sec;
   gdouble  fade_out_sec;

   gint32 audio_id;               /* audio cache id */
   GapStoryRenderAudioCacheElem *ac_elem;
   guchar *aud_data;
   gint32 aud_bytelength;
   gint32 range_samples;          /* number of samples in the selected range (sample has upto 4 bytes) */
   gint32 fade_in_samples;
   gint32 fade_out_samples;
   gint32 byteoffset_rangestart;
   gint32 byteoffset_data;

   void   *next;
} GapStoryRenderAudioRangeElem;  /* used for storyboard processing */


typedef struct GapStoryRenderVTrackAttrElem
{
   gint32  frame_count;        /* current total number of frames (until now) in this track */
   gint32  overlap_count;      /* how much frames to overlap (ignored in shadow tracks) */
   gdouble mask_framecount;    /* local mask frame progress. reset to 1 for each new clip
                                * that is added via input from storyboard file parsing
                                * but not for internally generated splitted elements
                                */

   gboolean keep_proportions;
   gboolean fit_width;
   gboolean fit_height;

   gdouble rotate_from;       /* rotation in degree */
   gdouble rotate_to;         /* rotation in degree */
   gint32  rotate_dur;        /* number of frames to change from -> to value */

   gdouble opacity_from;       /* 0.0 upto 1.0 */
   gdouble opacity_to;         /* 0.0 upto 1.0 */
   gint32  opacity_dur;        /* number of frames to change from -> to value */

   gdouble scale_x_from;       /* 0.0 upto 10.0  where 1.0 is fit video size */
   gdouble scale_x_to;         /* 0.0 upto 10.0  where 1.0 is fit video size */
   gint32  scale_x_dur;        /* number of frames to change from -> to value */

   gdouble scale_y_from;       /* 0.0 upto 10.0  where 1.0 is fit video size */
   gdouble scale_y_to;         /* 0.0 upto 10.0  where 1.0 is fit video size */
   gint32  scale_y_dur;        /* number of frames to change from -> to value */

   gdouble move_x_from;        /* -1.0 upto 1.0 where 0 is center and -1.0 left outside */
   gdouble move_x_to;          /* -1.0 upto 1.0 where 0 is center and -1.0 left outside */
   gint32  move_x_dur;         /* number of frames to change from -> to value */

   gdouble move_y_from;        /* -1.0 upto 1.0 where 0 is center and -1.0 up outside */
   gdouble move_y_to;          /* -1.0 upto 1.0 where 0 is center and -1.0 up outside */
   gint32  move_y_dur;         /* number of frames to change from -> to value */

   gint    rotate_accel;        /* acceleration characteristic for opacity transformation */
   gint32  rotate_frames_done;  /* already processed frames since begin of transition */
   gint    opacity_accel;        /* acceleration characteristic for opacity transformation */
   gint32  opacity_frames_done;  /* already processed frames since begin of transition */
   gint    move_x_accel;
   gint32  move_x_frames_done;
   gint    move_y_accel;
   gint32  move_y_frames_done;
   gint    scale_x_accel;
   gint32  scale_x_frames_done;
   gint    scale_y_accel;
   gint32  scale_y_frames_done;

   gdouble movepath_from;        /* 1.0 to path length in frames */
   gdouble movepath_to;          /* 1.0 to path length in frames */
   gint32  movepath_frames_done;
   gint32  movepath_dur;         /* number of frames to change from -> to value */
   char   *movepath_file_xml;    /* (dont g_free this) XML parameter file for movepath transitions */

} GapStoryRenderVTrackAttrElem;  /* Video track attributes used for storyboard processing */

typedef struct GapStoryRenderVTrackArray
{
  GapStoryRenderVTrackAttrElem attr[GAP_STB_MAX_VID_INTERNAL_TRACKS];
  gint32 max_tracknum;
} GapStoryRenderVTrackArray;  /* used for storyboard processing */


typedef struct GapStoryRenderErrors
{
  char   *errtext;       /* NULL==no error */
  char   *errline;       /* NULL, or copy of the line that has the 1. error */
  gint32  errline_nr;    /* line number where 1.st error occurred */
  char   *warntext;      /* NULL==no error */
  char   *warnline;      /* NULL, or copy of the line that has the 1. error */
  gint32  warnline_nr;   /* line number where 1.st error occurred */
  gint32  curr_nr;       /* current line nr while parsing */
  char   *currline;      /* pointer to currently parsed line (do not free this) */
} GapStoryRenderErrors;  /* used for storyboard processing */



typedef struct GapStoryRenderSection
{
  GapStoryRenderFrameRangeElem    *frn_list;
  GapStoryRenderAudioRangeElem    *aud_list;
  gchar                           *section_name;  /* null refers to the main section */
  void                            *next;
} GapStoryRenderSection;



typedef struct GapStoryRenderVidHandle  /* nick: vidhand */
{
  GapStoryRenderSection           *section_list;
  GapStoryRenderSection           *parsing_section;
  GapStoryRenderFrameRangeElem    *frn_list;
  GapStoryRenderAudioRangeElem    *aud_list;
  GapStoryRenderErrors            *sterr;
  char                         *preferred_decoder;

  char                         *master_insert_alpha_format;    /* Format for alpha channel inserting */
  gboolean                      master_insert_alpha_format_has_videobasename;
  gboolean                      master_insert_alpha_format_has_framenumber;

  char                         *master_insert_area_format;    /* Format for logo replacement */
  gboolean                      master_insert_area_format_has_videobasename;
  gboolean                      master_insert_area_format_has_framenumber;

  /* master video settings found in the storyboard file */
  gdouble  master_framerate;
  gint32   master_width;
  gint32   master_height;
  gint32   master_samplerate;
  gdouble  master_volume;

  char    *util_sox;
  char    *util_sox_options;

  gboolean ignore_audio;
  gboolean ignore_video;
  gboolean create_audio_tmp_files;

  gboolean   cancel_operation;   /* not supported yet */
  gdouble    *progress;
  gdouble    dummy_progress;     /* progress points to dummy_progress if no
                                  * external progress_ptr was specified at
                                  * opening of the video handle.
                                  */
  gboolean   do_gimp_progress;   /* pass this to GVA gvahand->do_gimp_progress (to show video seek progress) */
  gchar      *status_msg;
  gint32     status_msg_len;

  struct GapStoryRenderMaskDefElem *maskdef_elem;  /* list of mask definitions */
  gboolean   is_mask_handle;

  gint32     ffetch_user_id;

  gint32     minVidTrack;                /* lowest video track number of all elemnents in the current frn_list */
  gint32     maxVidTrack;                /* highest video track number of all elemnents in the current frn_list */

  gboolean      isLogResourceUsage;      /* triggers logging of resources (open videohandles an cached images) */
  gint32        resourceLogInterval;
  gboolean      isMultithreadEnabled;    /* triggers prefetch of videoframes via thread pool parallel processing */
  
} GapStoryRenderVidHandle;  /* used for storyboard processing */



/* wrapper struct for mask element definitions
 * adds video handle for mask access
 */
typedef struct GapStoryRenderMaskDefElem  /* nick: maskdef_elem */
{
  char  *mask_name;      /* identifier key string */
  gint32 record_type;
  gint32 frame_count;
  gint32 flip_request;  /* 0 none, 1 flip horizontal, 2 flip vertical, 3 flip both */


  GapStoryRenderVidHandle *mask_vidhand;

  struct GapStoryRenderMaskDefElem *next;
} GapStoryRenderMaskDefElem;


#endif        /* GAP_STORY_RENDER_TYPES_H */
