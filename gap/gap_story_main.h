/*  gap_story_main.h
 *
 *  This module handles GAP storyboard level1 editing
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

/* revision history:
 * version 1.3.25a; 2004/01/23  hof: created
 */

#ifndef _GAP_STORY_MAIN_H
#define _GAP_STORY_MAIN_H

#include "libgimp/gimp.h"
#include "gap_lib.h"
#include <gtk/gtk.h>
#include <libgimp/gimp.h>
#include <libgimp/gimpui.h>
#include "gap_pview_da.h"
#include "gap_story_file.h"
#include "gap_story_undo_types.h"
#include "gap_player_main.h"
#include "gap_mov_dialog.h"

#define GAP_STORY_PLUG_IN_PROC            "plug_in_gap_storyboard_edit"
#define GAP_STORYBOARD_EDIT_HELP_ID       "plug-in-gap-storyboard-edit"
#define GAP_STORY_PLUG_IN_PROC_CREATION   "plug_in_gap_storyboard_create_and_edit"

#define GAP_STORY_MAX_STORYFILENAME_LEN 2048
#define GAP_STORY_MAX_CLIP_WIDGETS 2000
#define GAP_STORY_DEFAULT_FRAMERATE 25.0
#define GAP_STORY_DEFAULT_SAMPLERATE 44100

#ifdef GAP_ENABLE_VIDEOAPI_SUPPORT
#include "gap_vid_api.h"
#else
#ifndef GAP_STUBTYPE_GVA_HANDLE
typedef gpointer t_GVA_Handle;
#define GAP_STUBTYPE_GVA_HANDLE
#endif
#endif

#define GAP_GIMPRC_ENABLE_STORYBOARD_DEBUG_FEATURES "video-enable-storyboard-debug-features"
#define GAP_VIDEO_STORYBOARD_PROCESSOR_FAIL_ON_IMAGE_LOAD_ERRORS "video-storyboard-processor-fail-on-image-load-errors"


#define GAP_STB_ATT_GFX_ARRAY_MAX 2

/* max flip request and delace modes (for dimensions of radio button tables) */
#define GAP_MAX_FLIP_REQUEST  4
#define GAP_MAX_DELACE_MODES  5

typedef enum
  {
     GAP_STB_EDMO_SEQUENCE_NUMBER
    ,GAP_STB_EDMO_FRAME_NUMBER
    ,GAP_STB_EDMO_TIMECODE
    ,GAP_STB_EDMO_SRC_FRAME_NUMBER_AND_COUNT
  } GapStoryElemDisplayMode;

typedef enum
  {
     GAP_STB_CLIPTARGET_CLIPLIST_APPEND
    ,GAP_STB_CLIPTARGET_STORYBOARD_APPEND
  } GapStoryClipTargetEnum;

typedef enum
  {
     GAP_STB_VLIST_MOVIE
    ,GAP_STB_VLIST_SECTION
    ,GAP_STB_VLIST_ANIM_IMAGE
  } GapStoryVthumbEnum;

typedef enum
  {
     GAP_VTHUMB_PREFETCH_NOT_ACTIVE
    ,GAP_VTHUMB_PREFETCH_IN_PROGRESS
    ,GAP_VTHUMB_PREFETCH_RESTART_REQUEST
    ,GAP_VTHUMB_PREFETCH_CANCEL_REQUEST
    ,GAP_VTHUMB_PREFETCH_PENDING
  } GapVThumbPrefetchProgressMode;

/* video list element describes base resource that has
 * a unique video_id and requires (much) more than one thumbnail
 * per resource.
 *
 * currently supported video resource types are defined via GapStoryVthumbEnum.
 *
 * The storyboard dialog has a global video thumbnail list that keeps
 * all non-persistent thumbnails in memory, connected to the video list elem
 * by theis video_id.
 *
 * Note that other resources that are based on single images / frames
 * use persistent thumbnails (according to the same open thumbnail standard
 * as supported by the gimp)
 */
typedef struct GapStoryVTResurceElem {
  GapStoryVthumbEnum vt_type;
  gint32 section_id;
  gint32 version;

  gchar *video_filename;   /* filename of the video or anim image,
                            * or section_name
                            */
  gint32 seltrack;         /* not relevant for
                            * GAP_STB_VLIST_SECTION and
                            * GAP_STB_VLIST_ANIM_IMAGE
                            */
  gint32 video_id;         /* uniqie video resource id */
  gint32 total_frames;     /* total frames of the video resource */


  void *next;
}  GapStoryVTResurceElem;

/* video thumbnail elemnt is used in the storyboard dialog
 * (and clip properties dialog)
 */
typedef struct GapVThumbElem {
  guchar *th_data;
  gint32 th_width;
  gint32 th_height;
  gint32 th_bpp;
  gint32 framenr;
  gint32 video_id;

  void  *next;
}  GapVThumbElem;


typedef struct GapStbPropWidget  /* nickname: pw */
{
  GapStoryElem  *stb_elem_bck;     /* backup for use at reset button pressed */
  GapStoryElem  *stb_elem_refptr;  /* never g_free this one ! */
  GapStoryBoard *stb_refptr;       /* never g_free this one ! */
  void  *sgpp;               /* never g_free this one ! */
  void  *tabw;               /* never g_free this one ! (pointer to parent GapStbTabWidgets) */

  gint32   go_job_framenr;
  gboolean go_render_all_request;
  gboolean go_recreate_request;
  gint32   go_timertag;
  gboolean scene_detection_busy;
  gboolean close_flag;

  gdouble     delace_threshold;
  gint32      delace_mode;
  gint32      flip_request;
  GapStoryMaskAnchormode  mask_anchor;

  GapPView   *pv_ptr;            /* for gap preview rendering of clip icon based on drawing_area */
  GapPView   *mask_pv_ptr;       /* for gap preview rendering of mask icon based on drawing_area */
  GtkWidget  *mask_pv_container; /* holds the layermask preview (hidden when not relevant) */
  GapPView   *typ_icon_pv_ptr;   /* for display of the clip or mask type */
  GtkWidget  *pw_prop_dialog;
  GtkWidget  *pw_filesel;
  GtkWidget  *pw_filename_entry;
  GtkWidget  *master_table;
  GtkWidget  *cliptype_label;
  GtkWidget  *dur_frames_label;
  GtkWidget  *dur_time_label;
  GtkWidget  *pingpong_toggle;
  GtkWidget  *comment_entry;
  GtkWidget  *pw_fmac_filesel;
  GtkWidget  *fmac_entry;
  GtkWidget  *pw_colormask_file_filesel;
  GtkWidget  *colormask_file_entry;
  GtkWidget  *colormask_file_filesel_button;
  GtkWidget  *colormask_file_label;
  GtkObject  *pw_spinbutton_from_adj;
  GtkObject  *pw_spinbutton_to_adj;
  GtkObject  *pw_spinbutton_loops_adj;
  GtkObject  *pw_spinbutton_seltrack_adj;
  GtkObject  *pw_spinbutton_delace_adj;
  GtkWidget  *pw_spinbutton_delace;
  GtkObject  *pw_spinbutton_step_density_adj;
  GtkWidget  *pw_framenr_label;
  GtkWidget  *pw_frametime_label;

  GtkWidget  *pw_delace_mode_radio_button_arr[GAP_MAX_DELACE_MODES];
  GtkWidget  *pw_flip_request_radio_button_arr[GAP_MAX_FLIP_REQUEST];

  /* for mask handling */
  GtkWidget  *pw_mask_definition_name_label;
  gboolean   is_mask_definition;
  GtkWidget  *pw_mask_name_entry;      /* relevant to enter mask definition */
  GtkWidget  *mask_name_combo;         /* selected mask_name reference */
  gint32 mask_name_combo_elem_count;
  GtkWidget  *mask_anchor_label;
  GtkWidget  *pingpong_label;
  GtkWidget  *pw_mask_enable_toggle;
  GtkWidget  *pw_mask_anchor_radio_button_arr[3];
  GtkObject  *pw_spinbutton_mask_stepsize_adj;

  /* for filermacro handling */
  GtkObject  *pw_spinbutton_fmac_accel_adj;
  GtkWidget  *pw_spinbutton_fmac_accel;
  GtkWidget  *pw_spinbutton_fmac_steps;
  GtkObject  *pw_spinbutton_fmac_steps_adj;
  GtkWidget  *pw_label_alternate_fmac2;
  GtkWidget  *pw_label_alternate_fmac_file;
  GtkWidget  *pw_hbox_fmac2;


  struct GapStbPropWidget *next;
} GapStbPropWidget;


typedef struct GapStbAttrLayerInfo  /* nickname: linfo */
{
  gboolean               layer_is_fake;
  GapStoryRecordType     layer_record_type;
  gint32                 layer_local_framenr;
  gint32                 layer_seltrack;
  gchar                 *layer_filename;

} GapStbAttrLayerInfo;


typedef struct GapStbSecpropWidget  /* nickname: spw */
{
  GapStorySection  *section_refptr;   /* never g_free this one ! */
  GapStoryBoard    *stb_refptr;       /* never g_free this one ! */
  void  *sgpp;               /* never g_free this one ! */
  void  *tabw;               /* never g_free this one ! (pointer to parent GapStbTabWidgets) */


  GapPView   *typ_icon_pv_ptr;   /* for display of the section type */
  GtkWidget  *spw_prop_dialog;
  GtkWidget  *spw_section_name_entry;
  GtkWidget  *master_table;
  GtkWidget  *cliptype_label;
  GtkWidget  *dur_frames_label;
  GtkWidget  *dur_time_label;
  GtkWidget  *spw_info_text_label;
  GtkWidget  *spw_delete_button;

} GapStbSecpropWidget;


/* for graphical display of transition attributes */
typedef struct GapStbAttGfx
{
  gint32      image_id;

  gint32      orig_layer_id;   /* invisible frame at original image size */
  gint32      opre_layer_id;   /* invisible prefetch frame at original image size */

  gint32      deco_layer_id;   /* decor layer (always on top of stack) */
  gint32      curr_layer_id;   /* copy of orig_layer_id, after transformations */
  gint32      pref_layer_id;   /* copy of opre_layer_id, after transformations (visible if overlapping is active) */
  gint32      base_layer_id;   /* decor layer (always BG) */
  gboolean    auto_update;

  /* information about the orig layer */
  GapStbAttrLayerInfo  orig_info;
  GapStbAttrLayerInfo  opre_info;

  GapPView   *pv_ptr;
  GtkWidget  *auto_update_toggle;
  GtkWidget  *framenr_label;
  GtkWidget  *frametime_label;

} GapStbAttGfx;

/* widgets for one transition attribute */
typedef struct GapStbAttRow
{
  GtkWidget  *enable_toggle;
  GtkObject  *spinbutton_from_adj;
  GtkObject  *spinbutton_to_adj;
  GtkObject  *spinbutton_dur_adj;
  GtkObject  *spinbutton_accel_adj;
  GtkWidget  *dur_time_label;

  GtkWidget  *spinbutton_from;
  GtkWidget  *spinbutton_to;
  GtkWidget  *spinbutton_dur;
  GtkWidget  *spinbutton_accel;

  GtkWidget  *button_from;
  GtkWidget  *button_to;
  GtkWidget  *button_dur;

} GapStbAttRow;

typedef struct GapStbAttrWidget  /* nickname: attw */
{
  GapStoryElem  *stb_elem_bck;     /* backup for use at reset button pressed */
  GapStoryElem  *stb_elem_refptr;  /* never g_free this one ! */
  GapStoryBoard *stb_refptr;       /* never g_free this one ! */
  void  *sgpp;               /* never g_free this one ! */
  void  *tabw;               /* never g_free this one ! (pointer to parent GapStbTabWidgets) */

  gint32   go_timertag;
  gboolean timer_full_update_request;
  gboolean close_flag;

  GtkWidget  *attw_prop_dialog;
  GtkWidget  *master_table;

  GtkWidget  *fit_width_toggle;
  GtkWidget  *fit_height_toggle;
  GtkWidget  *keep_proportions_toggle;

  GapStbAttRow  att_rows[GAP_STB_ATT_TYPES_ARRAY_MAX];
  GapStbAttGfx  gfx_tab[GAP_STB_ATT_GFX_ARRAY_MAX];   /* 0 .. from, 1 .. to */

  GtkObject  *spinbutton_overlap_dur_adj;
  GtkWidget  *spinbutton_overlap_dur;
  GtkWidget  *button_overlap_dur;

  GtkWidget   *comment_entry;

  GtkWidget   *movepath_edit_button;
  GtkWidget   *movepath_file_entry;
  GtkWidget   *movepath_filesel;
  gboolean     movepath_file_xml_is_valid;
  GtkWidget   *movepath_edit_dialog;
  GapAnimInfo *ainfo_ptr;
  GapMovValues *pvals;
  gint32       movepath_frame_image_id;
  gint32       movepath_obj_image_id;
  gint32       movepath_obj_layer_id;

  struct GapStbAttrWidget *next;
} GapStbAttrWidget;



typedef struct GapStbFrameWidget  /* nickname: fw */
{
  GtkWidget *event_box;
  GtkWidget *vbox;
  GtkWidget *vbox2;
  GtkWidget *hbox;
  GtkWidget *key_label;      /* for the 6-digit framenumber */
  GtkWidget *val_label;
  GapPView   *pv_ptr;            /* for gap preview rendering based on drawing_area */

  GapStoryElem  *stb_elem_refptr;  /* never g_free this one ! */
  GapStoryBoard *stb_refptr;       /* never g_free this one ! */

  gint32    seq_nr;          /* position in the storyboard list (starting at 1) */
  char      *frame_filename;

  void  *sgpp;               /* never g_free this one ! */
  void  *tabw;               /* never g_free this one ! (pointer to parent GapStbTabWidgets) */
} GapStbFrameWidget;


typedef struct GapStbTabWidgets  /* nickname: tabw */
{
  GapStoryMasterType type;
  GapStbFrameWidget  **fw_tab;
  gint32 fw_tab_size;
  gint32 mount_col;
  gint32 mount_row;
  gint32 cols;
  gint32 rows;
  gint32 thumbsize;
  gint32 thumb_width;
  gint32 thumb_height;
  gint32 rowpage;
  gint32 vtrack;

  gint32    story_id_at_prev_paste;

  gboolean  master_dlg_open;
  gboolean  otone_dlg_open;

  GtkWidget *mount_table;
  GtkWidget *fw_gtk_table;
  GtkWidget *frame_with_name;
  GtkWidget *total_rows_label;
  GtkObject *rowpage_spinbutton_adj;
  GtkObject *rowpage_vscale_adj;
  GtkWidget *rowpage_vscale;

  GtkWidget *vtrack_spinbutton;
  GtkObject *vtrack_spinbutton_adj;

  GtkWidget *filesel;
  GtkWidget *filename_entry;
  char      *filename_refptr;   /* never g_free this one ! */
  gint32     filename_maxlen;
  GtkWidget *load_button;
  GtkWidget *save_button;
  GtkWidget *play_button;

  GtkWidget *undo_button;
  GtkWidget *redo_button;
  GtkWidget *edit_cut_button;
  GtkWidget *edit_copy_button;
  GtkWidget *edit_paste_button;

  GtkWidget *new_clip_button;


  GtkWidget *sections_combo;
  gint32     sections_combo_elem_count; /* current number element in the combo box */

  GapStbPropWidget       *pw;
  GapStbSecpropWidget    *spw;
  GapStbAttrWidget       *attw;
  GapStoryElemDisplayMode edmode;

  GapStoryUndoElem  *undo_stack_list;
  GapStoryUndoElem  *undo_stack_ptr;
  gdouble            undo_stack_group_counter;

  void  *sgpp;               /* never g_free this one ! */

} GapStbTabWidgets;


typedef struct GapStbMainGlobalParams  /* nickname: sgpp */
{
  GimpRunMode  run_mode;
  gboolean     initialized;   /* FALSE at startup */
  gboolean     run;
  gint32       image_id;

  gchar        storyboard_filename[GAP_STORY_MAX_STORYFILENAME_LEN];
  gchar        cliplist_filename[GAP_STORY_MAX_STORYFILENAME_LEN];

  GapStoryBoard *stb;
  GapStoryBoard *cll;
  GapStoryBoard *curr_selection;
  GapPlayerMainGlobalParams *plp;

  /* GUI widget pointers */
  GapStbTabWidgets  *stb_widgets;
  GapStbTabWidgets  *cll_widgets;

  GapStoryVTResurceElem *video_list;
  GapVThumbElem     *vthumb_list;
  t_GVA_Handle      *gvahand;
  gchar             *gva_videofile;
  GtkWidget         *progress_bar_master;
  GtkWidget         *progress_bar_sub;
  gboolean           gva_lock;
  gboolean           cancel_video_api;
  gboolean           auto_vthumb;
  gboolean           auto_vthumb_refresh_canceled;
  gboolean           in_player_call;
  gboolean           arr_dlg_open;
  gboolean           force_stb_aspect;
  GapStoryClipTargetEnum           clip_target;
  GapVThumbPrefetchProgressMode    vthumb_prefetch_in_progress;

  /* layout values
   * those values are used for LAST_VALUES runmode at startup only
   * rendering uses the values in the tabw structures
   */
  gint32                  stb_max_open_videofile;
  gint32                  stb_fcache_size_per_videofile;
  gint32                  ffetch_max_img_cache_elements;
  gint32                  stb_resource_log_linterval;
  gboolean                stb_isMultithreadEnabled;
  gboolean                stb_preview_render_full_size;
  gboolean                render_prop_dlg_open;
  gboolean                win_prop_dlg_open;
  GapStoryElemDisplayMode cll_edmode;
  gint32                  cll_cols;
  gint32                  cll_rows;
  gint32                  cll_thumbsize;
  GapStoryElemDisplayMode stb_edmode;
  gint32                  stb_cols;
  gint32                  stb_rows;
  gint32                  stb_thumbsize;
  /* end layout values */

  GtkWidget *shell_window;
  GtkWidget *player_frame;

  GtkWidget *menu_item_win_vthumbs;

  GtkWidget *menu_item_stb_save;
  GtkWidget *menu_item_stb_save_as;
  GtkWidget *menu_item_stb_add_clip;
  GtkWidget *menu_item_stb_add_section_clip;
  GtkWidget *menu_item_stb_playback;
  GtkWidget *menu_item_stb_properties;
  GtkWidget *menu_item_stb_att_properties;
  GtkWidget *menu_item_stb_audio_otone;
  GtkWidget *menu_item_stb_encode;
  GtkWidget *menu_item_stb_undo;
  GtkWidget *menu_item_stb_redo;
  GtkWidget *menu_item_stb_close;

  GtkWidget *menu_item_cll_save;
  GtkWidget *menu_item_cll_save_as;
  GtkWidget *menu_item_cll_add_clip;
  GtkWidget *menu_item_cll_add_section_clip;
  GtkWidget *menu_item_cll_playback;
  GtkWidget *menu_item_cll_properties;
  GtkWidget *menu_item_cll_att_properties;
  GtkWidget *menu_item_cll_audio_otone;
  GtkWidget *menu_item_cll_encode;
  GtkWidget *menu_item_cll_undo;
  GtkWidget *menu_item_cll_redo;
  GtkWidget *menu_item_cll_close;

  GdkPixbuf *dnd_pixbuf;
} GapStbMainGlobalParams;


typedef struct GapStbCreationParams  /* nickname: scrp */
{
  gchar         storyboard_filename[GAP_STORY_MAX_STORYFILENAME_LEN];
  gint32        vid_width;
  gint32        vid_height;
  gdouble       framerate;

  gdouble       aspect_ratio;
  gint32        aspect_width;
  gint32        aspect_height;

  gdouble       samplerate;   // from audio track or default 44100 if no audio available...
  gchar         preferred_decoder[GAP_STORY_MAX_STORYFILENAME_LEN];

  /* Clip parameters */
  gint32        record_type_int;


  gchar         filename[GAP_STORY_MAX_STORYFILENAME_LEN];
  gint32        from_frame;
  gint32        to_frame;
  gint32        vidtrack;
  gdouble       delace_threshold;
  gint32        delace_mode;
  gint32        exact_seek;
  gint32        nloop;


} GapStbCreationParams;

#endif
