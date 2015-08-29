/* gap_mov_dialog.h
 * 2014.05.07 hof (Wolfgang Hofer)
 *
 * GAP ... Gimp Animation Plugins
 *
 * Dialog Window for gap_mov
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
#ifndef _GAP_MOV_DIALOG_H
#define _GAP_MOV_DIALOG_H

/* revision history:
 * gimp    2.8.14;  2015/08/24  hof: support merge down postprocessing
 * gimp    2.8.10;  2014/05/07  hof: support unlimited number of controlpoints.
 * gimp    1.3.20d; 2003/10/14  hof: added bluebox stuff
 * gimp    1.3.20d; 2003/10/05  hof: added tweenindex (twix)
 * gimp    1.3.20c; 2003/09/29  hof: new features: perspective transformation, tween_layer and trace_layer
 *                                   changed opacity, rotation and resize from int to gdouble
 * gimp    1.3.14a; 2003/05/24  hof: moved render procedures to module gap_mov_render
 * gimp    1.1.29b; 2000/11/19  hof: new feature: FRAME based Stepmodes,
 *                                   increased controlpoint Limit GAP_MOV_MAX_POINT (from 256 -> 1024)
 * gimp    1.1.20a; 2000/04/25  hof: support for keyframes, anim_preview
 * version 0.96.02; 1998.07.25  hof: added clip_to_img
 *                   1997.11.06 hof: created
 */

#include "gap_bluebox.h"

#define GAP_MOV_KEEP_SRC_PAINTMODE 4444

#define GAP_MOV_INT_VERSION 3

#define   GAP_MOVPATH_XML_FILENAME_MAX_LENGTH     1024
#define   GAP_MOVEPATH_GIMPRC_LOG_RENDER_PARAMS "video-move-path-log-render-params"
#define   GAP_MOVEPATH_GIMPRC_ROTATE_THRESHOLD  "video-move-path-rotate-threshold"
#define   GAP_MOVEPATH_DEFAULT_ROTATE_THRESHOLD  0.015

typedef enum
{
  GAP_STEP_LOOP      = 0,
  GAP_STEP_LOOP_REV  = 1,
  GAP_STEP_ONCE      = 2,
  GAP_STEP_ONCE_REV  = 3,
  GAP_STEP_PING_PONG = 4,
  GAP_STEP_NONE      = 5,
  GAP_STEP_FRAME_LOOP      = 100,
  GAP_STEP_FRAME_LOOP_REV  = 101,
  GAP_STEP_FRAME_ONCE      = 102,
  GAP_STEP_FRAME_ONCE_REV  = 103,
  GAP_STEP_FRAME_PING_PONG = 104,
  GAP_STEP_FRAME_NONE      = 105
} GapMovStepMode;

#define GAP_STEP_FRAME      GAP_STEP_FRAME_LOOP

typedef enum
{
  GAP_HANDLE_LEFT_TOP  = 0,
  GAP_HANDLE_LEFT_BOT  = 1,
  GAP_HANDLE_RIGHT_TOP = 2,
  GAP_HANDLE_RIGHT_BOT = 3,
  GAP_HANDLE_CENTER    = 4
} GapMovHandle;

typedef enum
{
  GAP_APV_EXACT      = 0,
  GAP_APV_ONE_FRAME  = 1,
  GAP_APV_QUICK      = 2
} GapMovApvMode;


typedef enum
{
  GAP_MOV_SEL_IGNORE          = 0,
  GAP_MOV_SEL_INITIAL         = 1,
  GAP_MOV_SEL_FRAME_SPECIFIC  = 2
} GapMovSelMode;

typedef enum
{
  GAP_MPP_MODE_KEEP        = 0,
  GAP_MPP_MODE_MERGE_DOWN  = 1,
  GAP_MPP_MODE_DELETE      = 2
} GapMovMergePostProcessingMode;

typedef enum
{
  GAP_MPP_TARGET_NEW_LAYER                        = 0,
  GAP_MPP_TARGET_LOWER_LAYER                      = 1,
  GAP_MPP_TARGET_LOWER_LAYERS_MASK_REPLACE_BLACK  = 2,
  GAP_MPP_TARGET_LOWER_LAYERS_MASK_REPLACE_WHITE  = 3,
  GAP_MPP_TARGET_LOWER_LAYERS_MASK_ADD            = 4
} GapMovMergePostProcessingTargetMode;

typedef struct {
        gint32  dst_frame_nr;        /* current destination frame_nr */
        gint32  src_layer_parent_id; /* 0 for toplevel layer that are not member of a layer group */
        gint32  src_layer_idx;       /* index of current layer within group or image (used for multilayer stepmodes) */
        gint32  src_frame_idx;       /* current frame number (used for source frame stepmodes) */
        gdouble src_layer_idx_dbl;
        gdouble src_frame_idx_dbl;
        gint32 *src_layers;          /* array of layer id's  in hte same layer group or image (used for multilayer stepmodes) */
        gint32  src_last_layer_idx;  /* index of last layer 0 upto n-1 within group or image (used for multilayer stepmodes) */
        gdouble currX,  currY;
        gint    l_handleX;
        gint    l_handleY;

        gdouble currOpacity;
        gdouble currWidth;
        gdouble currHeight;
        gdouble currRotation;

        gdouble currTTLX;     /*  transform x top left */
        gdouble currTTLY;     /*  transform y top left */
        gdouble currTTRX;     /*  transform x top right */
        gdouble currTTRY;     /*  transform y top right */
        gdouble currTBLX;     /*  transform x bot left */
        gdouble currTBLY;     /*  transform y bot left */
        gdouble currTBRX;     /*  transform x bot right */
        gdouble currTBRY;     /*  transform y bot right */

        gdouble currSelFeatherRadius;

        /* acceleration characteristics */
        gint accPosition;
        gint accOpacity;
        gint accSize;
        gint accRotation;
        gint accPerspective;
        gint accSelFeatherRadius;

        gboolean isSingleFrame;      /* TRUE when processing in single frame mode */
        gboolean keep_proportions;
	gboolean fit_width;
	gboolean fit_height;
        gint32   singleMovObjLayerId;
        gint32   singleMovObjImageId;
        
        gint32   processedLayerId;   /* id of the layer that was processed in the current render step */

} GapMovCurrent;


typedef struct {
        gint    p_x, p_y;   /* +- path koordinates */
        gdouble opacity;    /* 0.0  upto 100.0% */
        gdouble w_resize;   /* width resize 10 upto 300% */
        gdouble h_resize;   /* height resize 10 upto 300% */
        gdouble rotation;   /* rotatation +- degrees */

        gint    keyframe_abs;
        gint    keyframe;

        /* 4-point transform distortion (perspective) */
        gdouble ttlx;     /* 0.0 upto 10.0 transform x top left */
        gdouble ttly;     /* 0.0 upto 10.0 transform y top left */
        gdouble ttrx;     /* 0.0 upto 10.0 transform x top right */
        gdouble ttry;     /* 0.0 upto 10.0 transform y top right */
        gdouble tblx;     /* 0.0 upto 10.0 transform x bot left */
        gdouble tbly;     /* 0.0 upto 10.0 transform y bot left */
        gdouble tbrx;     /* 0.0 upto 10.0 transform x bot right */
        gdouble tbry;     /* 0.0 upto 10.0 transform y bot right */

        /* feather radius for selection handling */
        gdouble sel_feather_radius;

        /* acceleration characteristics */
	gint accPosition;
	gint accOpacity;
	gint accSize;
	gint accRotation;
	gint accPerspective;
	gint accSelFeatherRadius;

} GapMovPoint;

#define GAP_MOV_POINT_INITIAL_SIZE 1024
#define GAP_MOV_POINT_EXPAND_SIZE 512

/*
 * Notes:
 * - The source image MUST NOT be one of the Frames
 *   of the destination Animation !!
 */

typedef struct {
        gint32   version;
        gint32   recordedObjWidth;       /* witdh of the moving object layer (at recording time of the move path settings) */
        gint32   recordedObjHeight;      /* height of the moving object layer (at recording time of the move path settings) */
        gint32   recordedFrameWidth;     /* witdh of the frame (at recording time of the move path settings) */
        gint32   recordedFrameHeight;    /* height of the frame (at recording time of the move path settings) */
        gint32   total_frames;
        gint32   src_layerstack;
        gchar   *src_filename;


        gint32         src_image_id;   /* source image */
        gint32         src_layer_id;   /* id of layer (to begin with) */
        GapMovHandle   src_handle;
        GapMovStepMode src_stepmode;
        GapMovSelMode  src_selmode;
        int            src_paintmode;
        gint           src_force_visible;      /* TRUE FALSE */
        gint           src_apply_bluebox;      /* TRUE FALSE */
        gint           clip_to_img;            /* TRUE FALSE */

        gint32         tmpsel_image_id;     /* a temporary image to save the selection of the source image or frame */
        gint32         tmpsel_channel_id;   /* the selection */

        gdouble        step_speed_factor;
        gdouble        tween_opacity_initial;
        gdouble        tween_opacity_desc;
        gdouble        trace_opacity_initial;
        gdouble        trace_opacity_desc;
        gint           tracelayer_enable;
        gint           tween_steps;     /* 0 == no virtual tweens between frames */

        gint    point_idx;           /* current point index starting at 0 */
        gint    point_idx_max;       /* index of last valid point starting at 0 */
        gint    point_table_size;    /* number of currently allocetd GapMovPoint elementgs in point table */
        GapMovPoint *point;       /* dynamic allocated point table 
                                   * note that this is not a list, but a table 
                                   * that is reallocated when inital size 
                                   * GAP_MOV_POINT_INITIAL_SIZE is not big enough
                                   */

        gint    dst_range_start;  /* use current frame as default */
        gint    dst_range_end;
        gint    dst_layerstack;

        /* for dialog only */
        gint32  dst_image_id;      /* frame image */
        gint32  tmp_image_id;      /* temp. flattened preview image */
        gint32  tmp_alt_image_id;  /* temp. preview image (preloaded preview frame) */
        gint32  tmp_alt_framenr;   /* framenr of the tmp_alt_image_id */

        /* for generating animated preview only */
        GapMovApvMode  apv_mode;
        gint32   apv_src_frame;     /* image_id of the (already scaled) baseframe
                                     * or -1 in exact mode.
                                     * (exact mode uses unscaled original frames)
                                     */
        gint32   apv_mlayer_image;  /* destination image_id for the animated preview
                                     * -1 if we are not in anim_preview mode
                                     */
        gchar   *apv_gap_paste_buff;  /* Optional PasteBuffer (to store preview frames)
                                       * "/tmp/gimp_video_paste_buffer/gap_pasteframe_"
                                       * NULL if we do not copy frames to a paste_buffer
                                       */

        gdouble  apv_framerate;
        gdouble  apv_scalex;
        gdouble  apv_scaley;

        /* for FRAME based stepmodes */
        gint32   cache_src_image_id;    /* id of the source image (from where cache image was copied) */
        gint32   cache_tmp_image_id;    /* id of a cached flattened copy of the src image */
        gint32   cache_tmp_layer_id;    /* the only visible layer in the cached image */
        gint32   cache_frame_number;
        GapAnimInfo *cache_ainfo_ptr;

        /* for tween and trace layer processing */
        gint32 tween_image_id;   /* -1 if no tweening */
        gint32 tween_layer_id;   /* -1 if no tweening */
        gint32 trace_image_id;   /* -1 if no tracing */
        gint32 trace_layer_id;   /* -1 if no tracing */
        gint   twix;             /* current tweenindex, 0 is used for the real frame, processing order: tween_steps .... 2, 1, 0 */

        /* for the bluebox filter */
        GapBlueboxGlobalParams *bbp;
        GapBlueboxGlobalParams *bbp_pv;

        gdouble  rotate_threshold;

        gchar  *dst_group_name_path_string;
        gchar  *dst_group_name_delimiter;
        
        /* for merge down postprocessing options */
        GapMovMergePostProcessingMode  mergeModeRenderedObject;
        GapMovMergePostProcessingMode  mergeModeRenderedTweenLayer;
        GapMovMergePostProcessingMode  mergeModeRenderedTraceLayer;
        GapMovMergePostProcessingTargetMode  mergeTarget;


} GapMovValues;

typedef struct {
        gint32       drawable_id;      /* the layer in the frame image to be processed (e.g. moved and transofrmed) */
        gint32       frame_phase;      /* current frame number starting at 1 */
        gint32       total_frames;
        gboolean     keep_proportions;
	gboolean     fit_width;
	gboolean     fit_height;
        gchar        xml_paramfile[GAP_MOVPATH_XML_FILENAME_MAX_LENGTH];
} GapMovSingleFrame;


typedef struct {
        GapAnimInfo  *dst_ainfo_ptr;        /* destination frames */
        GapMovSingleFrame *singleFramePtr;  /* NULL when processing multiple frames */
        GapMovValues *val_ptr;
} GapMovData;



typedef struct {
    /* IN */
   gint  pointIndexToQuery;
   gint  startOfSegmentIndexToQuery;
   gint  endOfSegmentIndexToQuery;
   gint  tweenCount;

   /* OUT */
   gint    segmentNumber;
   gdouble pathSegmentLengthInPixels;
   gdouble maxSpeedInPixelsPerFrame;
   gdouble minSpeedInPixelsPerFrame;
} GapMovQuery;


typedef void (*t_close_movepath_edit_callback_fptr)(gpointer callback_data);

long  gap_mov_dlg_move_dialog (GapMovData *mov_ptr);
gint  gap_mov_dlg_move_dialog_singleframe(GapMovSingleFrame *singleFramePtr);

GtkWidget * gap_mov_dlg_edit_movepath_dialog (gint32 frame_image_id, gint32 drawable_id
   , const char *xml_paramfile
   , GapAnimInfo *ainfo_ptr
   , GapMovValues *pvals
   , t_close_movepath_edit_callback_fptr close_fptr
   , gpointer callback_data
   , gint32 nframes
   );


#endif
