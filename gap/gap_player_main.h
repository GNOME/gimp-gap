/*  gap_player_main.h
 *
 *  This module handles GAP video playback
 *  based on thumbnail files
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
 * version 1.3.26d; 2004/01/28  hof: mtrace_mode
 * version 1.3.20d; 2003/10/06  hof: new gpp struct members for resize behaviour
 * version 1.3.19a; 2003/09/07  hof: audiosupport (based on wavplay, for UNIX only),
 * version 1.3.15a; 2003/06/21  hof: created
 */

#ifndef _GAP_PLAYER_MAIN_H
#define _GAP_PLAYER_MAIN_H

#include "libgimp/gimp.h"
#include "gap_lib.h"
#include <gtk/gtk.h>
#include <libgimp/gimp.h>
#include <libgimp/gimpui.h>
#include "gap_pview_da.h"
#include "gap_story_file.h"
#include "gap_player_cache.h"
#include "gap_story_render_types.h"
#include "gap_drawable_vref_parasite.h"

#ifdef GAP_ENABLE_VIDEOAPI_SUPPORT
#include "gap_vid_api.h"
#else
#ifndef GAP_STUBTYPE_GVA_HANDLE
typedef gpointer t_GVA_Handle;
#define GAP_STUBTYPE_GVA_HANDLE
#endif
#endif

#define MAX_AUDIOFILE_LEN 1024

typedef enum {
    GAP_PLAYER_MTRACE_OFF
   ,GAP_PLAYER_MTRACE_IMG_SIZE
   ,GAP_PLAYER_MTRACE_PV_SIZE
  } GapPlayerMtraceType;

typedef struct GapPlayerAddClip {
  gpointer       user_data_ptr;   /* sgpp */
  GapAnimInfo   *ainfo_ptr;
  gint32         range_from;
  gint32         range_to;
  } GapPlayerAddClip;

/* Function Typedefs */
typedef  void           (*GapPlayerSetRangeFptr)(GapPlayerAddClip *plac_ptr);

#define GAP_PLAYER_DONT_FORCE_ASPECT 0.0

typedef struct GapPlayerMainGlobalParams {
  gboolean     standalone_mode;
  GimpRunMode  run_mode;
  gint32       image_id;
  gchar        *imagename;
  gint32       imagewidth;
  gint32       imageheight;
  gdouble      aspect_ratio;  /* 0.0 use original size, else force aspect  */

  GapAnimInfo *ainfo_ptr;
  GapStoryBoard *stb_ptr;
  t_GVA_Handle  *gvahand;
  gchar         *gva_videofile;

  gint32                mtrace_image_id;    /* multilayer image trace image id */ 
  GapPlayerMtraceType   mtrace_mode;        /* Init GAP_PLAYER_MTRACE_OFF */
  
  GapPlayerSetRangeFptr fptr_set_range;     /* procedure to callback at set range */
  gpointer              user_data_ptr;      /* userdata for the callback procedure */ 
  
  gboolean   autostart;
  gboolean   caller_range_linked;  /* propagate range selection immediate to the caller */
  gboolean   use_thumbnails;
  gboolean   exact_timing;      /* TRUE: allow drop frames fro exact timing, FALSE: disable drop */
  gboolean   play_is_active;
  gboolean   play_selection_only;
  gboolean   play_loop;
  gboolean   play_pingpong;
  gboolean   play_backward;
  gboolean   request_cancel_video_api;
  gboolean   cancel_video_api;
  gboolean   gva_lock;

  gint32     play_timertag;

  gint32   begin_frame;
  gint32   end_frame;
  gint32   play_current_framenr;
  gint32   pb_stepsize;
  gdouble  speed;             /* playback speed fps */
  gdouble  original_speed;    /* playback speed fps */
  gdouble  prev_speed;        /* previous playback speed fps */
  gint32   pv_pixelsize;      /* 32 upto 512 */
  gint32   pv_width;
  gint32   pv_height;
  
  /* lockflags */  
  gboolean in_feedback;
  gboolean in_timer_playback;
  gboolean in_resize;         /* for disable resize while initial startup */

  gint32   go_job_framenr;
  gint32   go_timertag;
  gint32   go_base_framenr;
  gint32   go_base;
  gint32   pingpong_count;
  
  /* GUI widget pointers */
  GapPView   *pv_ptr;
  GtkWidget *shell_window;  
  GtkWidget *docking_container;    /* NULL if not docked, or vbox to contain player */
  GtkWidget *frame_with_name;  
  GtkObject *from_spinbutton_adj;
  GtkObject *to_spinbutton_adj;
  GtkWidget *framenr_scale;  
  GtkObject *framenr_spinbutton_adj;
  GtkObject *speed_spinbutton_adj;
  GtkObject *size_spinbutton_adj;
  GtkWidget *from_button;
  GtkWidget *to_button;

  GtkWidget *progress_bar;
  GtkWidget *status_label;
  GtkWidget *timepos_label;
  GtkWidget *resize_box;
  GtkWidget *size_spinbutton;

  GtkWidget *use_thumb_checkbutton;
  GtkWidget *exact_timing_checkbutton;
  GtkWidget *pinpong_checkbutton;
  GtkWidget *selonly_checkbutton;
  GtkWidget *loop_checkbutton;

  GTimer    *gtimer;
  gdouble   cycle_time_secs;
  gdouble   rest_secs;
  gdouble   delay_secs;
  gdouble   framecnt;
  gint32    seltrack;
  gdouble   delace;
  gchar    *preferred_decoder;
  gboolean  force_open_as_video;
  gboolean  have_progress_bar;
  gchar    *progress_bar_idle_txt;
  
  gint32    resize_handler_id;
  gint32    old_resize_width;
  gint32    old_resize_height;
  gboolean  startup;
  gint32    shell_initial_width;
  gint32    shell_initial_height;
  
  /* audio stuff */
  gboolean  audio_enable;
  gint32    audio_resync;        /* force audio brak for n frames and sync restart */
  gchar     audio_filename[MAX_AUDIOFILE_LEN];
  gchar     audio_wavfile_tmp[MAX_AUDIOFILE_LEN];
  gint32    audio_frame_offset;
  guint32   audio_samplerate;
  guint32   audio_required_samplerate;
  guint32   audio_bits;
  guint32   audio_channels;
  guint32   audio_samples;  
  gint32    audio_status;
  gdouble   audio_volume;   /* 0.0 upto 1.0 */
  gint32    audio_tmp_samplerate;
  gint32    audio_tmp_samples;
  gboolean  audio_tmp_resample;
  gboolean  audio_tmp_dialog_is_open;
  
  GtkWidget *audio_filename_entry;
  GtkWidget *audio_offset_time_label;
  GtkWidget *audio_total_time_label;
  GtkWidget *audio_total_frames_label;
  GtkWidget *audio_samples_label;
  GtkWidget *audio_samplerate_label;
  GtkWidget *audio_bits_label;
  GtkWidget *audio_channels_label;
  GtkObject *audio_volume_spinbutton_adj;
  GtkObject *audio_frame_offset_spinbutton_adj;
  GtkWidget *audio_filesel;
  GtkWidget *audio_table;
  GtkWidget *audio_status_label;
  GtkWidget *video_total_time_label;
  GtkWidget *video_total_frames_label;
  gboolean   vindex_creation_is_running;
  GtkWidget *play_n_stop_hbox;
  GtkWidget *cancel_vindex_button;

  const char *help_id;
  
  gboolean    onion_delete;
  
  /* player cache settings */
  GtkObject *cache_size_spinbutton_adj;
  GtkWidget *label_current_cache_values;
  GtkWidget *progress_bar_cache_usage;

  gint32     max_player_cache;    /* max bytesize to use for caching frames 
                                   * (at pview widget size) 
                                   * a value of 0 turns caching OFF
                                   */
  GapPlayerCacheCompressionType cache_compression;
  gdouble                       cache_jpeg_quality;
  
  /* layout options */
  gboolean show_go_buttons;
  gboolean show_position_scale;
  GtkWidget *show_go_buttons_checkbutton;
  GtkWidget *show_position_scale_checkbutton;
  GtkWidget *gobutton_hbox;
  GtkWidget *frame_scale_hbox;
  
  /* flags to trigger built in transformations */
  gint32 flip_request;
  gint32 flip_status;
  gint32 stb_in_track;
  
  /* for playback of storyboard composite video */
  gint32 stb_parttype;
  gint32 stb_unique_id;
  GapStoryRenderVidHandle *stb_comp_vidhand;

  /* audio otone extract stuff */
  gboolean  audio_auto_offset_by_framenr;
  gint32    audio_otone_atrack;

  GtkWidget *audio_auto_offset_by_framenr_checkbutton;
  GtkWidget *audio_otone_extract_button;
  GtkWidget *audio_otone_atrack_spinbutton;
  GtkObject *audio_otone_atrack_spinbutton_adj;

  GtkWidget *progress_bar_audio;
  GtkWidget *audio_enable_checkbutton;

  GapDrawableVideoRef  *dvref_ptr;

  gboolean     enableDetailTracking;
  gulong       cache_ntiles;                 /* gimp tile cache size for the player process */
  GtkObject   *cache_ntiles_spinbutton_adj;
  GtkWidget   *detail_tracking_checkbutton;

  gboolean     enableAutoSkipMissingFrames;
  GtkWidget   *autoSkipMissingFrames_checkbutton;
  
} GapPlayerMainGlobalParams;

#define GAP_PLAYER_MAIN_DEFAULT_CACHE_NTILES   200

#define GAP_PLAYER_MAIN_AUSTAT_UNCHECKED       -1
#define GAP_PLAYER_MAIN_AUSTAT_NONE             0
#define GAP_PLAYER_MAIN_AUSTAT_SERVER_STARTED   1
#define GAP_PLAYER_MAIN_AUSTAT_FILENAME_SET     2
#define GAP_PLAYER_MAIN_AUSTAT_PLAYING          3

#define GAP_PLAYER_MAIN_MIN_SAMPLERATE   1000 
#define GAP_PLAYER_MAIN_MAX_SAMPLERATE   48000

#endif
