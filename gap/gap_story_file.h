/*  gap_story_file.h
 *
 *  This module handles GAP storyboard file
 *  parsing of storyboard level1 files (load informations into a list)
 *  and (re)write storyboard files from the list (back to storyboard file)
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

/* revision history:
 * version 2.3.0;   2006/04/14  new features: overlap, flip, mask definitions
 * version 1.3.25b; 2004/01/23  hof: created
 */

#ifndef _GAP_STORY_FILE_H
#define _GAP_STORY_FILE_H

#include "libgimp/gimp.h"
#include "gap_lib.h"
#include "gap_story_syntax.h"
#include "gap_story_render_types.h"

/* transition attribute types
 * (values are used as index for look-up tables)
 */
#define GAP_STB_ATT_TYPES_ARRAY_MAX 7
#define GAP_STB_ATT_TYPE_OPACITY  0
#define GAP_STB_ATT_TYPE_MOVE_X   1
#define GAP_STB_ATT_TYPE_MOVE_Y   2
#define GAP_STB_ATT_TYPE_ZOOM_X   3
#define GAP_STB_ATT_TYPE_ZOOM_Y   4
#define GAP_STB_ATT_TYPE_ROTATE   5
#define GAP_STB_ATT_TYPE_MOVEPATH 6


#define GAP_STB_MASK_SECTION_NAME  "Masks"
#define GAP_STB_MAX_FRAMENR 99999999



#define GAP_GIMPRC_VIDEO_STORYBOARD_MULTIPROCESSOR_ENABLE      "video-storyboard-multiprocessor-enable"
#define GAP_GIMPRC_VIDEO_STORYBOARD_PREVIEW_RENDER_FULL_SIZE   "video-storyboard-preview-render-full-size"
#define GAP_GIMPRC_VIDEO_STORYBOARD_MAX_OPEN_VIDEOFILES        "video-storyboard-max-open-videofiles"
#define GAP_GIMPRC_VIDEO_STORYBOARD_FCACHE_SIZE_PER_VIDEOFILE  "video-storyboard-fcache-size-per-videofile"
#define GAP_GIMPRC_VIDEO_STORYBOARD_RESOURCE_LOG_INTERVAL      "video-storyboard-resource-log-interval"
#define GAP_GIMPRC_VIDEO_ENCODER_FFMPEG_MULTIPROCESSOR_ENABLE  "video-enoder-ffmpeg-multiprocessor-enable"

/* GapStoryRecordType enum values are superset of GapLibAinfoType
 * from the sourcefile gap_lib.h
 */
  typedef enum
  {
     GAP_STBREC_VID_SILENCE
    ,GAP_STBREC_VID_COLOR
    ,GAP_STBREC_VID_IMAGE
    ,GAP_STBREC_VID_ANIMIMAGE
    ,GAP_STBREC_VID_FRAMES
    ,GAP_STBREC_VID_MOVIE
    ,GAP_STBREC_VID_COMMENT
    ,GAP_STBREC_VID_UNKNOWN

    ,GAP_STBREC_AUD_SILENCE
    ,GAP_STBREC_AUD_SOUND
    ,GAP_STBREC_AUD_MOVIE

    ,GAP_STBREC_ATT_TRANSITION
    ,GAP_STBREC_VID_SECTION
    ,GAP_STBREC_VID_BLACKSECTION
  } GapStoryRecordType;


  typedef enum
  {
    GAP_STB_TARGET_URILIST,
    GAP_STB_TARGET_UTF8_STRING,
    GAP_STB_TARGET_STRING,
    GAP_STB_TARGET_TEXT,
    GAP_STB_TARGET_COMPOUND_TEXT,
    GAP_STB_TARGET_STORYBOARD_ELEM
  } GapStoryDndTargets;

  typedef enum
  {
     GAP_STB_PM_NORMAL
    ,GAP_STB_PM_PINGPONG
  } GapStoryVideoPlaymode;

  typedef enum
  {
     GAP_STB_MASTER_TYPE_UNDEFINED
    ,GAP_STB_MASTER_TYPE_STORYBOARD
    ,GAP_STB_MASTER_TYPE_CLIPLIST
  } GapStoryMasterType;


  /* The GapStoryElem is a common used structure for
   * all type of video clips, audio clips, video attributes
   * and mask definitions.
   * mask definitions are handled as video clips using the reserved track number
   * GAP_STB_MASK_TRACK_NUMBER.
   */
  typedef struct GapStoryElem {
    gint32                 story_id;
    gint32                 story_orig_id;
    gboolean               selected;
    GapStoryRecordType     record_type;
    GapStoryVideoPlaymode  playmode;
    gint32                 track;

    char  *orig_filename;   /* full filename use for IMAGE and MOVIE Files
                             * and SECTIONS (for section_name)
                             */
    char  *orig_src_line;   /* without \n, used to store header, comment and unknown lines */

                            /* basename + ext are used for FRAME range elements only */
    gchar *basename;        /* path+filename (without number part and without extension */
    gchar *ext;             /* extenson ".xcf" ".jpg" ... including the dot */
    gint32     seltrack;    /* selected videotrack in a videofile (for GAP_FRN_MOVIE) */
    gint32     exact_seek;  /* 0 fast seek, 1 exact seek (for GAP_FRN_MOVIE) */
    gdouble    delace;      /* 0.0 no deinterlace, 1.0-1.99 odd 2.0-2.99 even rows  (for GAP_FRN_MOVIE) */

    gint32     flip_request;  /* 0 none, 1 flip horizontal, 2 flip vertical, 3 flip both */
    char   *mask_name;        /* optional reference to a layer mask
                               * if track == GAP_STB_MASK_TRACK_NUMBER this atribute
                               * is the mandatory definition of the mask_name.
                               */
    char   *colormask_file;   /* optional reference to a colormask parameter file
                               * relevant for ancor mode GAP_MSK_ANCHOR_CLIPCOLOR
                               * where mask is applied as colormask
                               */
    gdouble mask_stepsize;
    GapStoryMaskAnchormode  mask_anchor;
    gboolean  mask_disable;

    gchar *preferred_decoder;
    gchar *filtermacro_file;
    gint32 fmac_total_steps;
    gint32 fmac_accel;

    gint32 from_frame;
    gint32 to_frame;
    gint32 nloop;          /* 1 play one time */

    gint32 nframes;        /* if playmode == normal
                            * then frames = nloop * (ABS(from_frame - to_frame) + 1);
                            * else frames = (nloop * 2 * ABS(from_frame - to_frame)) + 1;
                            */

    gdouble step_density;  /* 1.0 for normal stepsize
                            * 2.0 use every 2.nd frame (double speed at same framerate)
                            * 0.5 use each frame twice (half speed at same framerate)
                            */
    gint32 file_line_nr;   /* line Number in the storyboard file */

    /* members for level2 VID Record types */
    gdouble  vid_wait_untiltime_sec;
    gdouble  color_red;
    gdouble  color_green;
    gdouble  color_blue;
    gdouble  color_alpha;

    /* members for attribute Record types */
    gboolean att_keep_proportions;
    gboolean att_fit_width;
    gboolean att_fit_height;

    /* members for transition attribute Record type */
    gboolean att_arr_enable[GAP_STB_ATT_TYPES_ARRAY_MAX];
    gdouble  att_arr_value_from[GAP_STB_ATT_TYPES_ARRAY_MAX];
    gdouble  att_arr_value_to[GAP_STB_ATT_TYPES_ARRAY_MAX];
    gint32   att_arr_value_dur[GAP_STB_ATT_TYPES_ARRAY_MAX];        /* number of frames to change from -> to value */
    gint32   att_arr_value_accel[GAP_STB_ATT_TYPES_ARRAY_MAX];      /* acceleration characteristics */
    gint32   att_overlap;  /* number of overlapping frames (value > 0 will generate a shadow track) */
    gchar   *att_movepath_file_xml;

    /* new members for Audio Record types */
    char     *aud_filename;
    gint32   aud_seltrack;          /* selected audiotrack in a videofile (for GAP_AUT_MOVIE) */
    gdouble  aud_wait_untiltime_sec;
    gdouble  aud_play_from_sec;
    gdouble  aud_play_to_sec;
    gdouble  aud_volume_start;
    gdouble  aud_volume;
    gdouble  aud_volume_end;
    gdouble  aud_fade_in_sec;
    gdouble  aud_fade_out_sec;
    gdouble  aud_min_play_sec;  /* for optimzed audio extract from videofiles */
    gdouble  aud_max_play_sec;
    gdouble  aud_framerate;     /* framerate that is used to convert audio unit frame <-> secs */

    struct GapStoryElem  *comment;
    struct GapStoryElem  *next;
  } GapStoryElem;

  typedef struct GapStorySection
  {
     GapStoryElem    *stb_elem;
     gchar           *section_name;  /* null refers to the main section */
     gint32          current_vtrack;

     gint32          section_id;  /* unique ID, NOT persistent */
     gint32          version;     /* numer of changes while editing, NOT persistent */

     void            *next;
  } GapStorySection;


  typedef struct GapStoryEditSettings
  {
     gchar           *section_name;  /* null refers to the main section */
     gint32          track;
     gint32          page;
  } GapStoryEditSettings;

  typedef struct GapStoryFrameNumberMappingElem {
    gint32 mapped_frame_number;
    gint32 orig_frame_number;

    struct GapStoryFrameNumberMappingElem *next;

  } GapStoryFrameNumberMappingElem;


  typedef struct GapStoryFrameNumberMap {
    gint32 total_frames_selected;
    GapStoryFrameNumberMappingElem *map_list;
  } GapStoryFrameNumberMap;

  typedef struct GapStoryBoard {
     GapStorySection *active_section;   /* reference pointer to active section (dont free this) */
     GapStorySection *mask_section;     /* reference pointer to mask section (dont free this) */
     GapStorySection *stb_section;      /* root of section list */

     gchar         *storyboardfile;
     GapStoryMasterType master_type;
     gint32         master_width;
     gint32         master_height;
     gdouble        master_framerate;
     gboolean       master_vtrack1_is_toplayer;  /* default = true; */

     gdouble        master_aspect_ratio;
     gint32         master_aspect_width;
     gint32         master_aspect_height;

     gint32         master_samplerate;
     gdouble        master_volume;

     gint32         layout_cols;
     gint32         layout_rows;
     gint32         layout_thumbsize;

     gchar         *preferred_decoder;

     /* for error handling while parsing */
     gchar         *errtext;
     gchar         *errline;
     gint32         errline_nr;
     gchar         *warntext;
     gchar         *warnline;
     gint32         warnline_nr;
     gint32         curr_nr;
     gchar         *currline;      /* dont g_free this one ! */
     gint32         count_unprintable_chars;

     /* for composite vide playback */
     gint32         stb_parttype;
     gint32         stb_unique_id;

     /* selection mapping is not relevant for rendering
      * but is used at playback of composite video
      * where frame ranges of the selected clips
      * are represented by a mapping
      * and the player only picks frame numbers via mapping
      */
     GapStoryFrameNumberMap *mapping;


     gboolean       unsaved_changes;
     GapStoryEditSettings *edit_settings;

     gchar         *master_insert_alpha_format;
     gchar         *master_insert_area_format;
 }  GapStoryBoard;


  typedef struct GapStoryLocateRet {
     GapStoryElem  *stb_elem;
     gint32        ret_framenr;
     gboolean      locate_ok;
  }  GapStoryLocateRet;

  typedef struct GapStoryCalcAttr
  {
    gint32  width;
    gint32  height;
    gint32  x_offs;
    gint32  y_offs;
    gdouble opacity;
    gdouble rotate;

    gint32  visible_width;
    gint32  visible_height;
  } GapStoryCalcAttr;

  typedef struct GapStoryVideoFileRef
  {
     gchar           *videofile;         /* full filename */
     gchar           *userdata;
     gchar           *preferred_decoder;
     gint32          seltrack;
     gint32          max_ref_framenr;

     void            *next;
  } GapStoryVideoFileRef;


void                gap_story_debug_fprint_list(FILE *fp, GapStoryBoard *stb);
void                gap_story_debug_fprint_elem(FILE *fp, GapStoryElem *stb_elem);
void                gap_story_debug_print_list(GapStoryBoard *stb);
void                gap_story_debug_print_elem(GapStoryElem *stb_elem);


GapStoryBoard *     gap_story_new_story_board(const char *filename);
GapStoryBoard *     gap_story_parse(const gchar *filename);
void                gap_story_elem_calculate_nframes(GapStoryElem *stb_elem);
GapStoryLocateRet * gap_story_locate_framenr(GapStoryBoard *stb
                         , gint32 in_framenr
                         , gint32 in_track);
GapStoryLocateRet * gap_story_locate_expanded_framenr(GapStorySection  *section
                         , gint32 in_framenr
                         , gint32 in_track);

void                gap_story_lists_merge(GapStoryBoard *stb_dst
                         , GapStoryBoard *stb_src
                         , gint32 story_id
                         , gboolean insert_after
                         , gint32   dst_vtrack);
gint32              gap_story_find_last_selected_in_track(GapStorySection *section, gint32 track_nr);
GapStoryElem *      gap_story_elem_find_by_story_id(GapStoryBoard *stb, gint32 story_id);
GapStoryElem *      gap_story_elem_find_by_story_orig_id(GapStoryBoard *stb, gint32 story_orig_id);


gboolean            gap_story_save(GapStoryBoard *stb, const char *filename);
GapStoryElem *      gap_story_new_elem(GapStoryRecordType record_type);
long                gap_story_upd_elem_from_filename(GapStoryElem *stb_elem,  const char *filename);
gboolean            gap_story_filename_is_videofile_by_ext(const char *filename);
gboolean            gap_story_filename_is_videofile(const char *filename);
void                gap_story_elem_free(GapStoryElem **stb_elem);
void                gap_story_free_stb_section(GapStorySection *stb_section);
void                gap_story_free_selection_mapping(GapStoryFrameNumberMap *mapping);
void                gap_story_free_storyboard(GapStoryBoard **stb_ptr);


GapStoryElem *      gap_story_new_mask_elem(GapStoryRecordType record_type);

GapStorySection *   gap_story_new_section();
GapStorySection *   gap_story_find_first_referable_subsection(GapStoryBoard *stb_dst);
GapStoryElem *      gap_story_elem_find_in_section_by_story_id(GapStorySection *section, gint32 story_id);
GapStorySection *   gap_story_find_section_by_story_id(GapStoryBoard *stb, gint32 story_id);
GapStorySection *   gap_story_find_section_by_stb_elem(GapStoryBoard *stb, GapStoryElem      *stb_elem);
GapStorySection *   gap_story_find_section_by_name(GapStoryBoard *stb, const char *section_name);
GapStorySection *   gap_story_find_main_section(GapStoryBoard *stb);
GapStorySection *   gap_story_create_or_find_section_by_name(GapStoryBoard *stb, const char *section_name);
gboolean            gap_story_remove_section(GapStoryBoard *stb, GapStorySection *del_section);
gchar *             gap_story_generate_new_unique_section_name(GapStoryBoard    *stb);



void                gap_story_list_append_elem(GapStoryBoard *stb, GapStoryElem *stb_elem);
void                gap_story_list_append_elem_at_section(GapStoryBoard *stb
                        , GapStoryElem *stb_elem
                        , GapStorySection *active_section);

gint32              gap_story_count_total_frames_in_section(GapStorySection  *section);
gint32              gap_story_get_framenr_by_story_id(GapStorySection  *section, gint32 story_id, gint32 in_track);
gint32              gap_story_get_expanded_framenr_by_story_id(GapStorySection *section, gint32 story_id, gint32 in_track);
char *              gap_story_get_filename_from_elem(GapStoryElem *stb_elem);
char *              gap_story_get_filename_from_elem_nr(GapStoryElem *stb_elem, gint32 in_framenr);
char *              gap_story_get_filename_from_elem_nr_anim(GapStoryElem *stb_elem, gint32 in_framenr);
GapStoryElem *      gap_story_fetch_nth_active_elem(GapStoryBoard *stb
                                                     , gint32 seq_nr
                                                     , gint32 in_track
                                                     );
GapAnimInfo *       gap_story_fake_ainfo_from_stb(GapStoryBoard *stb_ptr, gint32 in_track);

GapStoryElem *      gap_story_elem_duplicate(GapStoryElem *stb_elem);
void                gap_story_elem_copy(GapStoryElem *stb_elem_dst, GapStoryElem *stb_elem_src);

GapStoryElem *      gap_story_find_mask_definition_by_name(GapStoryBoard *stb_ptr, const char *mask_name);
GapStoryElem *      gap_story_find_mask_reference_by_name(GapStoryBoard *stb_ptr, const char *mask_name);

void                gap_story_enable_hidden_maskdefinitions(GapStoryBoard *stb_ptr);
GapStoryBoard *     gap_story_duplicate_full(GapStoryBoard *stb_ptr);
GapStoryBoard *     gap_story_duplicate_active_and_mask_section(GapStoryBoard *stb_ptr);
GapStoryBoard *     gap_story_duplicate_vtrack(GapStoryBoard *stb_ptr, gint32 in_vtrack);
GapStoryBoard *     gap_story_duplicate_sel_only(GapStoryBoard *stb_ptr, gint32 in_vtrack);
GapStoryBoard *     gap_story_duplicate_one_elem_and_masks(GapStoryBoard *stb_ptr
                      , GapStorySection *active_section, gint32 story_id);
GapStoryBoard *     gap_story_duplicate_one_elem(GapStoryBoard *stb_ptr
                      , GapStorySection *active_section, gint32 story_id);
GapStoryBoard *     gap_story_board_duplicate_distinct_sorted(GapStoryBoard  *stb_dup, GapStoryBoard *stb_ptr);
void                gap_story_copy_sub_sections(GapStoryBoard *stb_src, GapStoryBoard *stb_dst);



void                gap_story_set_properties_like_sample_storyboard (GapStoryBoard *stb
                         , GapStoryBoard *stb_sample);
void                gap_story_remove_sel_elems(GapStoryBoard *stb);

gint32              gap_story_count_active_elements(GapStoryBoard *stb_ptr, gint32 in_track);
void                gap_story_get_master_pixelsize(GapStoryBoard *stb_ptr
                         ,gint32 *width
                         ,gint32 *height);
gdouble             gap_story_get_master_size_respecting_aspect(GapStoryBoard *stb_ptr
                         ,gint32 *width
                         ,gint32 *height);
gdouble             gap_story_adjust_size_respecting_aspect(GapStoryBoard *stb_ptr
                         ,gint32 *width
                         ,gint32 *height);
void                gap_story_selection_all_set(GapStoryBoard *stb, gboolean sel_state);
void                gap_story_selection_by_story_id(GapStoryBoard *stb, gboolean sel_state, gint32 story_id);
void                gap_story_selection_from_ref_list_orig_ids(GapStoryBoard *stb, gboolean sel_state, GapStoryBoard *stb_ref);

const char *        gap_story_get_preferred_decoder(GapStoryBoard *stb, GapStoryElem *stb_elem);

void                gap_story_set_aud_movie_min_max(GapStoryBoard *stb);
gboolean            gap_story_elem_is_audio(GapStoryElem *stb_elem);
gboolean            gap_story_elem_is_video(GapStoryElem *stb_elem);
gboolean            gap_story_elem_is_video_relevant(GapStoryElem *stb_elem);
gboolean            gap_story_elem_is_same_resource(GapStoryElem *stb_elem, GapStoryElem *stb_elem_ref);
GapStoryElem *      gap_story_elem_find_by_same_resource(GapStoryBoard *stb_ptr, GapStoryElem *stb_elem_ref);



void                gap_story_del_audio_track(GapStoryBoard *stb, gint aud_track);
gboolean            gap_story_gen_otone_audio(GapStoryBoard *stb
                         ,gint vid_track
                         ,gint aud_track
                         ,gint aud_seltrack
                         ,gboolean replace_existing_aud_track
                         ,gdouble *first_non_matching_framerate
                         );
gdouble             gap_story_get_default_attribute(gint att_typ_idx);
void                gap_story_file_calculate_render_attributes(GapStoryCalcAttr *result_attr
                         , gint32 view_vid_width
                         , gint32 view_vid_height
                         , gint32 vid_width
                         , gint32 vid_height
                         , gint32 frame_width
                         , gint32 frame_height
                         , gboolean keep_proportions
                         , gboolean fit_width
                         , gboolean fit_height
                         , gdouble rotate
                         , gdouble opacity
                         , gdouble scale_x
                         , gdouble scale_y
                         , gdouble move_x
                         , gdouble move_y
                         );

gboolean          gap_story_update_mask_name_references(GapStoryBoard *stb_ptr
                         , const char *mask_name_new
                         , const char *mask_name_old
                         );
char *            gap_story_generate_unique_maskname(GapStoryBoard *stb_ptr);
GapStoryElem *    gap_story_find_maskdef_equal_to_ref_elem(GapStoryBoard *stb_ptr, GapStoryElem *stb_ref_elem);
gint32            gap_story_get_current_vtrack (GapStoryBoard *stb, GapStorySection *section);
void              gap_story_set_current_vtrack (GapStoryBoard *stb, GapStorySection *section
                         , gint32 current_vtrack);


gint32            gap_story_get_mapped_master_frame_number(GapStoryFrameNumberMap *mapping
                         , gint32 frame_number);
GapStoryFrameNumberMap * gap_story_create_new_mapping_from_selection(GapStorySection *active_section
                         , gint32 vtrack);
GapStoryFrameNumberMap * gap_story_create_new_mapping_by_story_id(GapStorySection *active_section
                         , gint32 vtrack, gint32 story_id);
void              gap_story_debug_print_mapping(GapStoryFrameNumberMap *mapping);

void              gap_story_free_GapStoryVideoFileRef(GapStoryVideoFileRef *vref_list);
GapStoryVideoFileRef * p_new_GapStoryVideoFileRef(const char *videofile
                          , gint32 seltrack
                          , const char *preferred_decoder
                          , gint32          max_ref_framenr);
GapStoryVideoFileRef * gap_story_get_video_file_ref_list(GapStoryBoard *stb);
char *                 gap_story_build_basename(const char *filename);

void                   gap_story_transform_rotate_layer(gint32 image_id, gint32 layer_id, gdouble rotate);
gboolean               gap_story_checkForAtLeatOneClipWithScalingDisabled(GapStoryBoard *stb_ptr);
gboolean               gap_story_isMultiprocessorSupportEnabled(void);

#endif
