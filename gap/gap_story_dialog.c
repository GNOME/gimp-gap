/*  gap_story_dialog.c
 *
 *  This module handles GAP storyboard dialog editor
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
 * version 2.2.1;   2006/02/07  hof: support drag&drop  destination for image/videofilenames
 * version 2.1.0a;  2005/01/22  hof: copy/cut/paste handling via keys (ctrl-c, ctrl-x, ctrl-v)
 * version 2.1.0a;  2004/12/05  hof: added global layout properties dialog
 * version 1.3.27a; 2004/03/15  hof: video thumbnails are kept in memory
 *                                   for the startframes in all MOVIE clips
 *                                   (common list for both storyboard and cliplist)
 *                                   Need define GAP_ENABLE_VIDEOAPI_SUPPORT
 *                                   to handle thumbnails for cliptype GAP_STBREC_VID_MOVIE
 * version 1.3.25a; 2004/02/22  hof: created
 */


#include "config.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <glib/gstdio.h>

#include <gtk/gtk.h>
#include <libgimp/gimp.h>
#include <libgimp/gimpui.h>
#include <gdk/gdkkeysyms.h>

#include "gap_libgapbase.h"
#include "gap_story_main.h"
#include "gap_story_undo.h"
#include "gap_story_dialog.h"
#include "gap_story_file.h"
#include "gap_player_dialog.h"
#include "gap_pdb_calls.h"
#include "gap_pview_da.h"
#include "gap_stock.h"
#include "gap_lib.h"
#include "gap_image.h"
#include "gap_vin.h"
#include "gap_timeconv.h"
#include "gap_thumbnail.h"
#include "gap_arr_dialog.h"
#include "gap_story_att_trans_dlg.h"
#include "gap_story_vthumb.h"
#include "gap_story_section_properties.h"
#include "gap_frame_fetcher.h"

#include "images/gap-stock-pixbufs.h"

#include "gap-intl.h"

#define GAP_STB_PLAYER_HELP_ID    "plug-in-gap-storyboard-player"
#define GAP_STB_MASTER_PROP_DLG_HELP_ID  "plug-in-gap-storyboard-master-prop"
#define GAP_STB_GEN_OTONE_DLG_HELP_ID    "plug-in-gap-storyboard-gen-otone"


extern int gap_debug;  /* 1 == print debug infos , 0 dont print debug infos */

#define STB_THSIZE_LARGE  88
#define STB_THSIZE_MEDIUM 66
#define STB_THSIZE_SMALL  44


/* layout constraint check values */
#define STB_THSIZE_MAX   120
#define STB_THSIZE_MIN    33
#define CLL_MIN_COL        4
#define CLL_MAX_COL       15
#define CLL_MIN_ROW        1
#define CLL_MAX_ROW       10

#define STB_MIN_COL       10
#define STB_MAX_COL       30
#define STB_MIN_ROW        1
#define STB_MAX_ROW        5


#define STB_THIDX_LARGE  0
#define STB_THIDX_MEDIUM 1
#define STB_THIDX_SMALL  2
#define GAP_STORY_FW_PTR   "gap_story_fw_ptr"
#define GAP_STORY_TABW_PTR "gap_story_tabw_ptr"


/* render parameters constraint check values */

#define MIN_OPEN_VIDEOFILES 2
#define MAX_OPEN_VIDEOFILES 100
#define DEFAULT_OPEN_VIDEOFILES 12

#define MIN_FCACHE_PER_VIDEO 2
#define MAX_FCACHE_PER_VIDEO 250
#define DEFAULT_FCACHE_PER_VIDEO 36

#define MIN_IMG_CACHE_ELEMENTS 1
#define MAX_IMG_CACHE_ELEMENTS 2000
#define DEFAULT_IMG_CACHE_ELEMENTS 18

#define MIN_RESOURCE_LOG_INTERVAL 0
#define MAX_RESOURCE_LOG_INTERVAL 100000
#define DEFAULT_RESOURCE_LOG_INTERVAL 0



/* the file GAP_DEBUG_STORYBOARD_CONFIG_FILE
 * is a debug configuration intended for development and test
 * if this file exists at start of the storyboard plug-in
 * it has an additional menu item that triggers debug output of internal
 * memory structures to stdout. (or logifile)
 * the content of this file selects what to print by symbolic names
 * and optionally specifies the name of a logfile (default print goes to stdout)
 * (see procedure p_is_debug_feature_item_enabled for details)
 */
#define GAP_DEBUG_STORYBOARD_CONFIG_FILE "GAP_DEBUG_STORYBOARD"

typedef void (*GapStbMenuCallbackFptr)(GtkWidget *widget, GapStbMainGlobalParams *sgpp);

static char *   p_get_gapdebug_storyboard_config_file();
static gboolean p_is_debug_menu_enabled(void);
static gboolean p_is_debug_feature_item_enabled(const char *debug_item);
static FILE *   p_debug_feature_open_optional_logfile();
static void     p_get_begin_and_end_for_single_clip_playback(gint32 *begin_frame, gint32 *end_frame, GapStoryElem *stb_elem);


static void     on_stb_elem_drag_begin (GtkWidget        *widget,
                    GdkDragContext   *context,
                    GapStbFrameWidget *fw);
static void     on_stb_elem_drag_get (GtkWidget        *widget,
                    GdkDragContext   *context,
                    GtkSelectionData *selection_data,
                    guint             info,
                    guint             time,
                    GapStbFrameWidget *fw);
//static void     on_stb_elem_drag_delete (GtkWidget        *widget,
//                    GdkDragContext   *context,
//                    GapStbFrameWidget *fw);
static void     on_clip_elements_dropped (GtkWidget        *widget,
                   GdkDragContext   *context,
                   gint              x,
                   gint              y,
                   GtkSelectionData *selection_data,
                   guint             info,
                   guint             time);
static GapStoryBoard * p_get_or_auto_create_storyboard (GapStbMainGlobalParams *sgpp, GapStbTabWidgets *tabw , gboolean *auto_created);
static GapStoryBoard * p_dnd_selection_to_stb_list(const char *selection_data, gint32 vtrack);
static GapStoryElem *  p_check_and_convert_uri_to_stb_elem(const char *uri, GapStoryElem *prev_elem, gint32 vtrack);
static gboolean        p_check_and_merge_range_if_possible(GapStoryElem *elem, GapStoryElem *prev_elem);
static GapStoryElem *  p_create_stb_elem_from_filename (const char *filename, gint32 vtrack);
static void            p_frame_widget_init_dnd(GapStbFrameWidget *fw);
static void            p_widget_init_dnd_drop(GtkWidget *widget, GapStbTabWidgets *tabw);



static gint32   p_thumbsize_to_index (gint32 thumbsize);
static gint32   p_index_to_thumbsize (gint32 thindex);

static GapStoryBoard    *  p_tabw_get_stb_ptr (GapStbTabWidgets *tabw);
static void                p_set_layout_preset(GapStbMainGlobalParams *sgpp);
static GapStbTabWidgets *  p_new_stb_tab_widgets(GapStbMainGlobalParams *sgpp
                                                , GapStoryMasterType type);
static void     p_render_all_frame_widgets (GapStbTabWidgets *tabw);

static void     p_frame_widget_init_empty (GapStbTabWidgets *tabw, GapStbFrameWidget *fw);
static void     p_frame_widget_render (GapStbFrameWidget *fw);
static void     p_frame_widget_destroy(GapStbFrameWidget *fw);

static gint32   p_get_max_rowpage(gint32 act_elems, gint32 cols, gint rows);

static void     p_story_set_range_cb(GapPlayerAddClip *plac_ptr);

static void     gap_story_fw_abstract_properties_dialog (GapStbFrameWidget *fw);

static void     p_story_call_player(GapStbMainGlobalParams *sgpp
                   , GapStoryBoard *stb
                   , char *imagename
                   , gint32 imagewidth
                   , gint32 imageheight
                   , gdouble aspect_ratio
                   , gint32 image_id
                   , gint32 begin_frame
                   , gint32 end_frame
                   , gboolean play_all
                   , gint32 seltrack
                   , gdouble delace
                   , const char *preferred_decoder
                   , gint32 flip_request
                   , gint32 flip_status
                   , gint32 stb_in_track
                   , gboolean stb_composite
                   );
static void     p_call_master_encoder(GapStbMainGlobalParams *sgpp
                   , GapStoryBoard *stb
                   , const char *stb_filename
                   );


static void     p_restore_edit_settings (GapStbTabWidgets *tabw, GapStoryBoard *stb);
static void     p_tabw_process_redo(GapStbTabWidgets *tabw);
static void     p_tabw_process_undo(GapStbTabWidgets *tabw);
static void     p_tabw_replace_storyboard_and_widgets(GapStbTabWidgets *tabw, GapStoryBoard *stb);

static gboolean p_storyboard_reload (GapStbMainGlobalParams *sgpp);
static gboolean p_cliplist_reload (GapStbMainGlobalParams *sgpp);

static void     p_player_cll_mode_cb (GtkWidget *w, GdkEventButton  *bevent, GapStbMainGlobalParams *sgpp);
static void     p_player_stb_mode_cb (GtkWidget *w, GdkEventButton  *bevent, GapStbMainGlobalParams *sgpp);
static void     p_player_img_mode_cb (GtkWidget *w, GapStbMainGlobalParams *sgpp);
static void     p_cancel_button_cb (GtkWidget *w, GapStbMainGlobalParams *sgpp);

static void     p_tabw_gen_otone_dialog(GapStbTabWidgets *tabw);

static gint32   p_parse_int(const char *buff);
static void     p_parse_aspect_width_and_height(const char *buff, gint32 *aspect_width, gint32 *aspect_height);
static void     p_tabw_master_prop_dialog(GapStbTabWidgets *tabw, gboolean new_flag);
static void     p_tabw_add_elem (GapStbTabWidgets *tabw, GapStbMainGlobalParams *sgpp, GapStoryBoard *stb_dst);
static void     p_tabw_add_section_elem (GapStbTabWidgets *tabw, GapStbMainGlobalParams *sgpp, GapStoryBoard *stb_dst);
static void     p_tabw_add_att_elem (GapStbTabWidgets *tabw, GapStbMainGlobalParams *sgpp, GapStoryBoard *stb_dst);
static void     p_tabw_new_clip_cb (GtkWidget *w, GdkEventButton  *bevent, GapStbTabWidgets *tabw);


static void     p_filesel_tabw_close_cb ( GtkWidget *widget, GapStbTabWidgets *tabw);
static gboolean p_filesel_tabw_ok (GtkWidget *widget, GapStbTabWidgets *tabw);
static void     p_filesel_tabw_load_ok_cb (GtkWidget *widget, GapStbTabWidgets *tabw);
static void     p_filesel_tabw_save_ok_cb ( GtkWidget *widget, GapStbTabWidgets *tabw);
static void     p_tabw_load_file_cb ( GtkWidget *w, GapStbTabWidgets *tabw);
static void     p_tabw_file_save_as_cb ( GtkWidget *w, GapStbTabWidgets *tabw);
static void     p_tabw_filename_entry_cb(GtkWidget *widget, GapStbTabWidgets *tabw);
static void     p_tabw_set_rowpage_and_refresh(GapStbTabWidgets *tabw, gint32 rowpage);
static void     p_rowpage_spinbutton_cb(GtkEditable     *editable,
                                     GapStbTabWidgets *tabw);
static void     p_vtrack_spinbutton_cb(GtkEditable     *editable,
                                     GapStbTabWidgets *tabw);
static void     p_update_vscale_page(GapStbTabWidgets *tabw);

static void     p_section_combo_changed_cb(GtkWidget     *widget,
                                     GapStbTabWidgets *tabw);
static void     p_vscale_changed_cb(GtkObject *adj, GapStbTabWidgets *tabw);
static void     p_cliptarget_togglebutton_toggled_cb (GtkToggleButton *togglebutton
                       , GapStoryClipTargetEnum *clip_target_ptr);
static void     p_single_clip_playback(GapStbFrameWidget *fw);
static gboolean p_frame_widget_preview_events_cb (GtkWidget *widget,
                             GdkEvent  *event,
                             GapStbFrameWidget *fw);
static void     p_tabw_destroy_properties_dlg (GapStbTabWidgets *tabw, gboolean destroy_all);
static void     p_tabw_destroy_attw_dlg (GapStbTabWidgets *tabw, gboolean destroy_all);
static void     p_tabw_destroy_all_popup_dlg (GapStbTabWidgets *tabw);
static void     p_tabw_section_properties_cut_cb (GtkWidget *w, GapStbTabWidgets *tabw);
static void     p_tabw_undo_cb (GtkWidget *w, GapStbTabWidgets *tabw);
static void     p_tabw_redo_cb (GtkWidget *w, GapStbTabWidgets *tabw);
static void     p_tabw_edit_cut_cb (GtkWidget *w, GapStbTabWidgets *tabw);
static void     p_tabw_edit_copy_cb (GtkWidget *w, GapStbTabWidgets *tabw);
static void     p_tabw_edit_paste_cb (GtkWidget *w, GapStbTabWidgets *tabw);
static void     p_tabw_edit_paste_at_story_id (GapStbMainGlobalParams  *sgpp
                    , GapStoryBoard *stb_dst
                    , GapStbTabWidgets *tabw
                    , gint32 story_id
                    , gboolean insert_after);

static void     p_tabw_update_selection(GapStbTabWidgets  *tabw);
static void     p_selection_add(GapStbFrameWidget *fw);
static void     p_selection_extend(GapStbFrameWidget *fw);
static void     p_selection_replace(GapStbFrameWidget *fw);

static gboolean p_cut_copy_sensitive (GapStoryBoard *stb);
static gboolean p_paste_sensitive (GapStbMainGlobalParams *sgpp
                   ,GapStoryBoard *stb
                   );
static void     p_tabw_sensibility (GapStbMainGlobalParams *sgpp
                   ,GapStoryBoard *stb
                   ,GapStbTabWidgets *tabw);

static void     p_tabw_set_undo_sensitivity(GapStbTabWidgets *tabw
                   , GtkWidget *menu_item);
static void     p_tabw_set_redo_sensitivity(GapStbTabWidgets *tabw
                   , GtkWidget *menu_item);
static void     p_undo_redo_sensibility (GapStbMainGlobalParams *sgpp);

static void     p_widget_sensibility (GapStbMainGlobalParams *sgpp);


static gboolean p_close_one_or_both_lists(GapStbMainGlobalParams *sgpp
                         ,gboolean check_cliplist
                         ,gboolean check_storyboard
                         );
static gboolean p_story_save_with_edit_settings(GapStbTabWidgets *tabw
                         ,GapStoryBoard *stb
                         , const char *filename);

static void     p_menu_win_quit_cb (GtkWidget *widget, GapStbMainGlobalParams *sgpp);
static void     p_menu_win_help_cb (GtkWidget *widget, GapStbMainGlobalParams *sgpp);
static void     p_menu_win_properties_cb (GtkWidget *widget, GapStbMainGlobalParams *sgpp);
static void     p_menu_win_vthumbs_toggle_cb (GtkWidget *widget, GapStbMainGlobalParams *sgpp);
static void     p_menu_win_debug_log_to_stdout_cb (GtkWidget *widget, GapStbMainGlobalParams *sgpp);
static void     p_menu_win_render_properties_cb (GtkWidget *widget, GapStbMainGlobalParams *sgpp);

static void     p_menu_cll_new_cb (GtkWidget *widget, GapStbMainGlobalParams *sgpp);
static void     p_menu_cll_open_cb (GtkWidget *widget, GapStbMainGlobalParams *sgpp);
static void     p_menu_cll_save_cb (GtkWidget *widget, GapStbMainGlobalParams *sgpp);
static void     p_menu_cll_save_cb_as (GtkWidget *widget, GapStbMainGlobalParams *sgpp);
static void     p_menu_cll_playback_cb (GtkWidget *widget, GapStbMainGlobalParams *sgpp);
static void     p_menu_cll_properties_cb (GtkWidget *widget, GapStbMainGlobalParams *sgpp);
static void     p_menu_cll_add_elem_cb (GtkWidget *widget, GapStbMainGlobalParams *sgpp);
static void     p_menu_cll_add_section_elem_cb (GtkWidget *widget, GapStbMainGlobalParams *sgpp);
static void     p_menu_cll_add_att_elem_cb (GtkWidget *widget, GapStbMainGlobalParams *sgpp);
static void     p_menu_cll_toggle_edmode (GtkWidget *widget, GapStbMainGlobalParams *sgpp);
static void     p_menu_cll_audio_otone_cb (GtkWidget *widget, GapStbMainGlobalParams *sgpp);
static void     p_menu_cll_encode_cb (GtkWidget *widget, GapStbMainGlobalParams *sgpp);
static void     p_menu_cll_undo_cb (GtkWidget *widget, GapStbMainGlobalParams *sgpp);
static void     p_menu_cll_redo_cb (GtkWidget *widget, GapStbMainGlobalParams *sgpp);
static void     p_menu_cll_close_cb (GtkWidget *widget, GapStbMainGlobalParams *sgpp);

static void     p_menu_stb_new_cb (GtkWidget *widget, GapStbMainGlobalParams *sgpp);
static void     p_menu_stb_open_cb (GtkWidget *widget, GapStbMainGlobalParams *sgpp);
static void     p_menu_stb_save_cb (GtkWidget *widget, GapStbMainGlobalParams *sgpp);
static void     p_menu_stb_save_cb_as (GtkWidget *widget, GapStbMainGlobalParams *sgpp);
static void     p_menu_stb_playback_cb (GtkWidget *widget, GapStbMainGlobalParams *sgpp);
static void     p_menu_stb_properties_cb (GtkWidget *widget, GapStbMainGlobalParams *sgpp);
static void     p_menu_stb_add_elem_cb (GtkWidget *widget, GapStbMainGlobalParams *sgpp);
static void     p_menu_stb_add_section_elem_cb (GtkWidget *widget, GapStbMainGlobalParams *sgpp);
static void     p_menu_stb_add_att_elem_cb (GtkWidget *widget, GapStbMainGlobalParams *sgpp);
static void     p_menu_stb_toggle_edmode (GtkWidget *widget, GapStbMainGlobalParams *sgpp);
static void     p_menu_stb_audio_otone_cb (GtkWidget *widget, GapStbMainGlobalParams *sgpp);
static void     p_menu_stb_encode_cb (GtkWidget *widget, GapStbMainGlobalParams *sgpp);
static void     p_menu_stb_undo_cb (GtkWidget *widget, GapStbMainGlobalParams *sgpp);
static void     p_menu_stb_redo_cb (GtkWidget *widget, GapStbMainGlobalParams *sgpp);
static void     p_menu_stb_close_cb (GtkWidget *widget, GapStbMainGlobalParams *sgpp);


static GtkWidget*  p_make_menu_bar_item(GtkWidget *menu_bar, gchar *label);
static GtkWidget*  p_make_item_with_image(GtkWidget *parent, const gchar *stock_id,
                     GapStbMenuCallbackFptr fptr, GapStbMainGlobalParams *sgpp);
static GtkWidget*  p_make_item_with_label(GtkWidget *parent, gchar *label,
                     GapStbMenuCallbackFptr fptr, GapStbMainGlobalParams *sgpp);
static GtkWidget*  p_make_check_item_with_label(GtkWidget *parent, gchar *label,
                     GapStbMenuCallbackFptr fptr, GapStbMainGlobalParams *sgpp
                     ,gboolean initial_value);

static void        p_make_menu_global(GapStbMainGlobalParams *sgpp, GtkWidget *menu_bar);
static void        p_make_menu_cliplist(GapStbMainGlobalParams *sgpp, GtkWidget *menu_bar);
static void        p_make_menu_storyboard(GapStbMainGlobalParams *sgpp, GtkWidget *menu_bar);
static void        p_make_menu_help(GapStbMainGlobalParams *sgpp, GtkWidget *menu_bar);

static gboolean    p_check_unsaved_changes (GapStbMainGlobalParams *sgpp
                           , gboolean check_cliplist
                           , gboolean check_storyboard
                           );
static void        p_tabw_update_frame_label_and_rowpage_limits (GapStbTabWidgets *tabw
                           , GapStbMainGlobalParams *sgpp
                           );

static void     p_prefetch_vthumbs (GapStbMainGlobalParams *sgpp
                   , GapStoryBoard *stb
                   );

static void     p_optimized_prefetch_vthumbs_worker (GapStbMainGlobalParams *sgpp);
static void     p_optimized_prefetch_vthumbs (GapStbMainGlobalParams *sgpp);

static void     p_tabw_set_focus_to_first_fw(GapStbTabWidgets *tabw);
static gboolean p_tabw_key_press_event_cb ( GtkWidget *widget
                    , GdkEvent  *event
                    , GapStbTabWidgets *tabw
                    );
static gboolean p_tabw_scroll_event_cb ( GtkWidget *widget
                    , GdkEventScroll *sevent
                    , GapStbTabWidgets *tabw
                    );
static void     story_dlg_response (GtkWidget *widget,
                    gint       response_id,
                    GapStbMainGlobalParams *sgpp);
static void     story_dlg_destroy (GtkWidget *widget,
                    GapStbMainGlobalParams *sgpp);


static void     p_recreate_tab_widgets(GapStoryBoard *stb
                       ,GapStbTabWidgets *tabw
                       ,gint32 mount_col
                       ,gint32 mount_row
                       ,GapStbMainGlobalParams *sgpp
                      );

static GapStorySection * p_find_section_by_active_index(GapStoryBoard *stb
                            , gint active_index);
static gint     p_find_combo_index_of_section(GapStoryBoard *stb
                           , GapStorySection *target_section);
static void     p_activate_section_by_combo_index(gint active_index
                       , GapStbTabWidgets *tabw);
static void     p_set_strings_for_section_combo(GapStbTabWidgets *tabw);


static GtkWidget *  p_create_button_bar(GapStbTabWidgets *tabw
                   ,gint32 mount_col
                   ,gint32 mount_row
                   ,GapStbMainGlobalParams *sgpp
                   ,gint32 mount_vs_col
                   ,gint32 mount_vs_row
                   );

static gint32  p_get_gimprc_preview_size(const char *gimprc_option_name);
static void    p_save_gimprc_preview_size(const char *gimprc_option_name, gint32 preview_size);
static void    p_save_gimprc_int_value(const char *gimprc_option_name, gint32 value);
static void    p_save_gimprc_gboolean_value(const char *gimprc_option_name, gboolean value);
static void    p_save_gimprc_layout_settings(GapStbMainGlobalParams *sgpp);
static void    p_get_gimprc_layout_settings(GapStbMainGlobalParams *sgpp);
static void    p_save_gimprc_render_settings(GapStbMainGlobalParams *sgpp);
static void    p_get_gimprc_render_settings(GapStbMainGlobalParams *sgpp);


static void    p_reset_progress_bars(GapStbMainGlobalParams *sgpp);
static void    p_storyboard_init_from_creation_params( GapStbTabWidgets *tabw
                   , GapStbMainGlobalParams *sgpp
                   , GapStoryBoard *stb
                   , GapStbCreationParams  *scrp
                   );
static void    p_call_external_image_viewer(GapStbFrameWidget *fw);


/* ---------------------------------------
 * p_get_gapdebug_storyboard_config_file
 * ---------------------------------------
 */
static char *
p_get_gapdebug_storyboard_config_file()
{
  char *filename;

  filename = g_build_filename(gimp_directory(), GAP_DEBUG_STORYBOARD_CONFIG_FILE, NULL);
  return (filename);
}

/* ---------------------------------------
 * p_is_debug_menu_enabled
 * ---------------------------------------
 * print the list of video elements
 * to stdout (typical used for logging and debug purpose)
 */
static gboolean
p_is_debug_menu_enabled(void)
{
  gboolean enable;
  gboolean enableStoryboardDebugFeatures;
  char *filename;

  enable = FALSE;
  enableStoryboardDebugFeatures =
          gap_base_get_gimprc_gboolean_value(GAP_GIMPRC_ENABLE_STORYBOARD_DEBUG_FEATURES, FALSE);

  if((gap_debug)
  || (enableStoryboardDebugFeatures)
  || (g_file_test(GAP_DEBUG_STORYBOARD_CONFIG_FILE, G_FILE_TEST_EXISTS)))
  {
    return(TRUE);
  }

  filename = p_get_gapdebug_storyboard_config_file();
  if(filename)
  {
    if(g_file_test(filename, G_FILE_TEST_EXISTS))
    {
      enable = TRUE;
    }
    g_free(filename);
  }
  return(enable);

}  /* end p_is_debug_menu_enabled */

/* ---------------------------------------
 * p_is_debug_feature_item_enabled
 * ---------------------------------------
 * return TRUE if the specified debug_item name
 * is enabled (e.g. is present as text line in the debug configuration file)
 */
static gboolean
p_is_debug_feature_item_enabled(const char *debug_item)
{
  gboolean enable;
  char *filename;

  enable = FALSE;
  filename = p_get_gapdebug_storyboard_config_file();
  if(filename)
  {
    if(gap_debug)
    {
        printf("debug feature filename:'%s'\n", filename);
    }
    if(g_file_test(filename, G_FILE_TEST_EXISTS))
    {
      FILE *l_fp;
      char         l_buf[400];

      printf("debug_item:'%s'", debug_item);

      l_fp = g_fopen(filename, "r");
      if(l_fp)
      {
        while (NULL != fgets (l_buf, sizeof(l_buf)-1, l_fp))
        {
          l_buf[sizeof(l_buf)-1] = '\0';
          gap_file_chop_trailingspace_and_nl(&l_buf[0]);

          if(gap_debug)
          {
            printf("  CFG:'%s'\n", l_buf);
          }

          if (strcmp(l_buf, debug_item) == 0)
          {
            enable = TRUE;
            break;
          }
        }

        fclose(l_fp);
        if(enable)
        {
          printf(" IS ENABLED\n");
        }
        else
        {
          printf(" is disabled\n");
        }
      }

    }
    g_free(filename);
  }


  return(enable);

}  /* end p_is_debug_feature_item_enabled */


/* ---------------------------------------
 * p_debug_feature_open_optional_logfile
 * ---------------------------------------
 * check for presence of a "logfile=" item int the debug configuration file.
 * if present, then open the logfilename (starts immediate after the "=" character)
 * for write append (or create if not yet exist) and return the file handle pointer.
 * ELSE return NULL.
 */
static FILE *
p_debug_feature_open_optional_logfile()
{
  FILE *loggingFp;
  char *filename;

  loggingFp = NULL;
  filename = p_get_gapdebug_storyboard_config_file();
  if(filename)
  {
    if(g_file_test(filename, G_FILE_TEST_EXISTS))
    {
      FILE *l_fp;
      char         l_buf[400];

      l_fp = g_fopen(filename, "r");
      if(l_fp)
      {
        while (NULL != fgets (l_buf, sizeof(l_buf)-1, l_fp))
        {
          l_buf[sizeof(l_buf)-1] = '\0';
          gap_file_chop_trailingspace_and_nl(&l_buf[0]);

          if(gap_debug)
          {
            printf("  CFG:'%s'\n", l_buf);
          }

          if (strncmp(l_buf, "logfile=", strlen("logfile=")) ==0)
          {
            char *logfileName;

            logfileName = &l_buf[strlen("logfile=")];
            loggingFp = g_fopen(logfileName, "a");
            break;
          }
        }

        fclose(l_fp);
      }

    }
    g_free(filename);
  }


  return(loggingFp);

}  /* end p_debug_feature_open_optional_logfile */


/* ---------------------------------------------
 * p_get_begin_and_end_for_single_clip_playback
 * ---------------------------------------------
 */
static void
p_get_begin_and_end_for_single_clip_playback(gint32 *begin_frame, gint32 *end_frame, GapStoryElem *stb_elem)
{
  *begin_frame = stb_elem->from_frame;
  *end_frame = stb_elem->to_frame;

  if(stb_elem->record_type == GAP_STBREC_VID_IMAGE)
  {
    char *l_basename;
    long l_number;

    l_basename = gap_lib_alloc_basename(stb_elem->orig_filename, &l_number);
    g_free(l_basename);
    *begin_frame = l_number;
    *end_frame = l_number;

  }
}    /* end p_get_begin_and_end_for_single_clip_playback */




/* -----------------------------
 * p_thumbsize_to_index
 * -----------------------------
 */
static gint32
p_thumbsize_to_index (gint32 thumbsize)
{
  if(thumbsize >= STB_THSIZE_LARGE)
  {
    return (STB_THIDX_LARGE);
  }

  if(thumbsize >= STB_THSIZE_MEDIUM)
  {
    return (STB_THIDX_MEDIUM);
  }

  return (STB_THIDX_SMALL);
}  /* end p_thumbsize_to_index */


/* -----------------------------
 * p_index_to_thumbsize
 * -----------------------------
 */
static gint32
p_index_to_thumbsize (gint32 thindex)
{
  if(thindex == STB_THIDX_LARGE)
  {
    return (STB_THSIZE_LARGE);
  }

  if(thindex == STB_THIDX_MEDIUM)
  {
    return (STB_THSIZE_MEDIUM);
  }

  return (STB_THSIZE_SMALL);
}  /* end p_index_to_thumbsize */


/* -------------------------------------
 * gap_story_dlg_tabw_update_frame_label
 * -------------------------------------
 */
void
gap_story_dlg_tabw_update_frame_label (GapStbTabWidgets *tabw
                           , GapStbMainGlobalParams *sgpp
                           )
{
  if((tabw) && (sgpp))
  {
    p_tabw_update_frame_label_and_rowpage_limits(tabw, sgpp);
  }
}  /* end gap_story_dlg_tabw_update_frame_label */

/* -----------------------------
 * gap_story_dlg_pw_render_all
 * -----------------------------
 */
void
gap_story_dlg_pw_render_all(GapStbPropWidget *pw, gboolean recreate)
{
  if(gap_debug)
  {
    printf("gap_story_dlg_pw_render_all recreate:%d\n", (int)recreate);
  }

  if(pw == NULL) { return; }
  if(pw->tabw == NULL) { return; }

  if(recreate)
  {
    GapStbTabWidgets *tabw;

    tabw = (GapStbTabWidgets *)pw->tabw;

    /* refresh storyboard layout and thumbnail list widgets */
    p_recreate_tab_widgets( pw->stb_refptr
                           ,tabw
                           ,tabw->mount_col
                           ,tabw->mount_row
                           ,pw->sgpp
                           );
  }
  p_render_all_frame_widgets(pw->tabw);
  p_tabw_update_frame_label_and_rowpage_limits(pw->tabw, pw->sgpp);
}  /* end gap_story_dlg_pw_render_all */

/* --------------------------------------
 * gap_story_dlg_attw_render_all
 * --------------------------------------
 */
void
gap_story_dlg_attw_render_all(GapStbAttrWidget *attw)
{
  if(attw == NULL) { return; }
  if(attw->tabw == NULL) { return; }

  p_render_all_frame_widgets(attw->tabw);
  p_tabw_update_frame_label_and_rowpage_limits(attw->tabw, attw->sgpp);
}  /* end gap_story_dlg_attw_render_all */

/* -----------------------------
 * p_tabw_get_stb_ptr
 * -----------------------------
 */
static GapStoryBoard *
p_tabw_get_stb_ptr (GapStbTabWidgets *tabw)
{
  GapStoryBoard *l_stb;
  GapStbMainGlobalParams *sgpp;

  if(tabw == NULL) { return(NULL); }

  sgpp = (GapStbMainGlobalParams *)tabw->sgpp;
  if(sgpp == NULL)  { return(NULL); }

  l_stb = sgpp->stb;
  if(tabw->type == GAP_STB_MASTER_TYPE_CLIPLIST)
  {
    l_stb = sgpp->cll;
  }
  return(l_stb);

}  /* end p_tabw_get_stb_ptr */

/* ------------------------------
 * gap_story_dlg_tabw_get_stb_ptr
 * ------------------------------
 */
GapStoryBoard *
gap_story_dlg_tabw_get_stb_ptr (GapStbTabWidgets *tabw)
{
  return(p_tabw_get_stb_ptr(tabw));
}  /* end gap_story_dlg_tabw_get_stb_ptr */


/* -----------------------------
 * p_new_stb_tab_widgets
 * -----------------------------
 */
static GapStbTabWidgets *
p_new_stb_tab_widgets(GapStbMainGlobalParams *sgpp, GapStoryMasterType type)
{
  GapStbTabWidgets *tabw;

  tabw = g_new(GapStbTabWidgets, 1);
  if(tabw)
  {
    tabw->sgpp = sgpp;
    tabw->type = type;
    if(type == GAP_STB_MASTER_TYPE_STORYBOARD)
    {
      sgpp->stb_widgets = tabw;
      p_set_layout_preset(sgpp);
      tabw->filename_refptr = sgpp->storyboard_filename;
      tabw->filename_maxlen = sizeof(sgpp->storyboard_filename);
    }
    else
    {
      sgpp->cll_widgets = tabw;
      p_set_layout_preset(sgpp);
      tabw->filename_refptr = sgpp->cliplist_filename;
      tabw->filename_maxlen = sizeof(sgpp->cliplist_filename);
    }
    tabw->filesel = NULL;
    tabw->filename_entry = NULL;
    tabw->load_button = NULL;
    tabw->save_button = NULL;
    tabw->play_button = NULL;
    tabw->undo_button = NULL;
    tabw->redo_button = NULL;
    tabw->edit_paste_button = NULL;
    tabw->edit_cut_button = NULL;
    tabw->edit_copy_button = NULL;
    tabw->new_clip_button = NULL;
    tabw->vtrack_spinbutton = NULL;

    tabw->thumb_width = tabw->thumbsize;
    tabw->thumb_height = tabw->thumbsize;
    tabw->rowpage = 1;
    tabw->vtrack = 1;

    tabw->mount_table = NULL;
    tabw->frame_with_name = NULL;
    tabw->fw_gtk_table = NULL;
    tabw->rowpage_spinbutton_adj = NULL;
    tabw->rowpage_vscale_adj = NULL;
    tabw->total_rows_label = NULL;

    tabw->pw = NULL;
    tabw->spw = NULL;
    tabw->attw = NULL;
    tabw->fw_tab = NULL;
    tabw->fw_tab_size = 0;  /* start up with empty fw table of 0 elements */
    tabw->master_dlg_open = FALSE;
    tabw->otone_dlg_open = FALSE;
    tabw->story_id_at_prev_paste = -1;

    tabw->undo_stack_list = NULL;
    tabw->undo_stack_ptr = NULL;
    tabw->undo_stack_group_counter = 0.0;

    tabw->sgpp = sgpp;

  }
  return(tabw);
}  /* end p_new_stb_tab_widgets */


/* -----------------------------
 * p_set_layout_preset
 * -----------------------------
 */
static void
p_set_layout_preset(GapStbMainGlobalParams *sgpp)
{
  GapStbTabWidgets *tabw;

  if(sgpp)
  {
    tabw = sgpp->stb_widgets;
    if(tabw)
    {
      tabw->cols = sgpp->stb_cols;
      tabw->rows = sgpp->stb_rows;
      tabw->thumbsize = sgpp->stb_thumbsize;
      tabw->edmode = GAP_STB_EDMO_FRAME_NUMBER;
      if(tabw->thumbsize <= STB_THSIZE_SMALL)
      {
        /* for smaller thumbnail sizes show only the sequence number
         * (there is no space for the framenumber
         */
        tabw->edmode = GAP_STB_EDMO_SEQUENCE_NUMBER;
      }
    }

    tabw = sgpp->cll_widgets;
    if(tabw)
    {
      tabw->cols = sgpp->cll_cols;
      tabw->rows = sgpp->cll_rows;
      tabw->thumbsize = sgpp->cll_thumbsize;
      tabw->edmode = GAP_STB_EDMO_SEQUENCE_NUMBER;
    }
  }
}  /* end p_set_layout_preset */


/* -----------------------------
 * p_render_all_frame_widgets
 * -----------------------------
 * assign frame_filename and stb_elem_refptr
 * to all frame widgets
 * according to the current rowpage setting (scroll).
 */
static void
p_render_all_frame_widgets (GapStbTabWidgets *tabw)
{
  GapStoryBoard *l_stb;
  GapStbMainGlobalParams *sgpp_ptr;
  GapStbFrameWidget *fw;
  gint32 ii;
  gint32 seq_nr;
  gint32 l_act_elems = 0;


  if(gap_debug)
  {
    printf("(1) START p_render_all_frame_widgets\n");
  }

  if(tabw == NULL)
  {
    return;
  }

  sgpp_ptr = (GapStbMainGlobalParams *)tabw->sgpp;
  if(sgpp_ptr)
  {
    sgpp_ptr->auto_vthumb_refresh_canceled = FALSE;

    if(gap_debug)
    {
      printf("(1.1)  p_render_all_frame_widgets : vthumb_prefetch_in_progress (0 NOTACTIVE, 1 PROGRESS, 2 RESTART) value:%d\n"
        ,(int)sgpp_ptr->vthumb_prefetch_in_progress
        );
    }

    if(sgpp_ptr->vthumb_prefetch_in_progress == GAP_VTHUMB_PREFETCH_IN_PROGRESS)
    {
      sgpp_ptr->vthumb_prefetch_in_progress = GAP_VTHUMB_PREFETCH_RESTART_REQUEST;
    }
  }
  l_stb = p_tabw_get_stb_ptr(tabw);

  if(l_stb)
  {
    l_act_elems = gap_story_count_active_elements(l_stb, tabw->vtrack);
  }


  /* init buffers for the frame widgets */
  seq_nr = (tabw->rowpage -1) * tabw->cols;
  for(ii=0; ii < tabw->fw_tab_size; ii++)
  {
    seq_nr++;

    if(gap_debug)
    {
      printf("(2) p_render_all_frame_widgets act_elems: %d ii:%d  seq_nr: %d\n"
            , (int)l_act_elems
            , (int)ii
            , (int)seq_nr
            );
    }

    fw = tabw->fw_tab[ii];

    if(gap_debug)
    {
      printf("(3) p_render_all_frame_widgets fw: %d\n", (int)fw );
      printf("(4) p_render_all_frame_widgets fw->frame_filename: %d\n", (int)&fw->frame_filename );
    }

    if(fw->frame_filename)
    {
      g_free(fw->frame_filename);
      fw->frame_filename = NULL;
    }
    fw->tabw = tabw;
    fw->stb_refptr = l_stb;
    fw->stb_elem_refptr = gap_story_fetch_nth_active_elem(l_stb, seq_nr, tabw->vtrack);
    if(fw->stb_elem_refptr)
    {
      fw->frame_filename = gap_story_get_filename_from_elem(fw->stb_elem_refptr);
      fw->seq_nr = seq_nr;

      p_frame_widget_render(fw);
      if(sgpp_ptr)
      {
       if(sgpp_ptr->cancel_video_api)
       {
         /* if user did press cancel during videoseek or videoindex creation
          * we set the flag auto_vthumb_refresh_canceled TRUE
          * this Flag stays TRUE until the next start of this refresh procedure.
          * All further video items waiting for refresh are then rendered
          * with the default icon (instead of accessing the videofile) in this refresh cycle.
          * otherwise the user has to cancel each video item separately
          */
         sgpp_ptr->auto_vthumb_refresh_canceled = TRUE;
       }
      }
    }

  }

  /* render selection state for all displayed framewidgets */
  p_tabw_update_selection(tabw);
}  /* end p_render_all_frame_widgets */


/* ---------------------------------
 * p_frame_widget_init_empty
 * ---------------------------------
 * this procedure does initialize
 * the passed frame_widget, by creating all
 * the gtk_widget stuff to build up the frame_widget
 */
static void
p_frame_widget_init_empty (GapStbTabWidgets *tabw, GapStbFrameWidget *fw)
{
  GtkWidget *alignment;
  gint32     l_check_size;

  if(gap_debug) printf("p_frame_widget_init_empty START\n");

  /* create all sub-widgets for the given frame widget */
  fw->pv_ptr        = NULL;
  fw->seq_nr = 0;
  fw->frame_filename = NULL;
  fw->stb_elem_refptr = NULL;
  fw->stb_refptr = NULL;
  fw->tabw = tabw;
  fw->sgpp = tabw->sgpp;

  fw->vbox = gtk_vbox_new (FALSE, 0);
  GTK_WIDGET_SET_FLAGS(fw->vbox, GTK_CAN_FOCUS);

  /* the vox2  */
  fw->vbox2 = gtk_vbox_new (FALSE, 0);

  /* Create an EventBox (container for labels and the preview drawing_area)
   * used to handle selection events
   */
  fw->event_box = gtk_event_box_new ();

  gtk_container_add (GTK_CONTAINER (fw->event_box), fw->vbox2);
  gtk_widget_set_events (fw->event_box, GDK_BUTTON_PRESS_MASK);

  gtk_box_pack_start (GTK_BOX (fw->vbox), fw->event_box, FALSE, FALSE, 0);


  /*  The frame preview  */
  alignment = gtk_alignment_new (0.5, 0.5, 0.0, 0.0);
  gtk_box_pack_start (GTK_BOX (fw->vbox2), alignment, FALSE, FALSE, 1);
  gtk_widget_show (alignment);

  l_check_size = tabw->thumbsize / 16;
  fw->pv_ptr = gap_pview_new( tabw->thumb_width + 0
                            , tabw->thumb_height + 0
                            , l_check_size
                            , NULL   /* no aspect_frame is used */
                            );
  gtk_widget_set_events (fw->pv_ptr->da_widget, GDK_EXPOSURE_MASK);
  gtk_container_add (GTK_CONTAINER (alignment), fw->pv_ptr->da_widget);
  gtk_widget_show (fw->pv_ptr->da_widget);
  g_signal_connect (G_OBJECT (fw->pv_ptr->da_widget), "event",
                    G_CALLBACK (p_frame_widget_preview_events_cb),
                    fw);

  /* the hbox  */
  fw->hbox = gtk_hbox_new (FALSE, 1);
  gtk_box_pack_start (GTK_BOX (fw->vbox2), fw->hbox, FALSE, FALSE, 2);

  /*  the key label */
  fw->key_label = gtk_label_new ("key");
  gtk_box_pack_start (GTK_BOX (fw->hbox), fw->key_label, FALSE, FALSE, 0);
  gtk_widget_show (fw->key_label);

  /*  the frame timing label */
  fw->val_label = gtk_label_new ("val");
  gtk_box_pack_start (GTK_BOX (fw->hbox), fw->val_label, FALSE, FALSE, 2);

  if(tabw->thumbsize >= STB_THSIZE_LARGE)
  {
    gimp_label_set_attributes (GTK_LABEL (fw->key_label)
                              ,PANGO_ATTR_SCALE,  PANGO_SCALE_MEDIUM
                              ,PANGO_ATTR_WEIGHT, PANGO_WEIGHT_BOLD
                              ,-1);
    gimp_label_set_attributes (GTK_LABEL (fw->val_label)
                              ,PANGO_ATTR_SCALE,  PANGO_SCALE_MEDIUM
                              ,-1);
  }
  else
  {
    if(tabw->thumbsize >= STB_THSIZE_MEDIUM)
    {
      gimp_label_set_attributes (GTK_LABEL (fw->key_label)
                                ,PANGO_ATTR_SCALE,  PANGO_SCALE_SMALL
                                ,PANGO_ATTR_WEIGHT, PANGO_WEIGHT_BOLD
                                ,-1);
      gimp_label_set_attributes (GTK_LABEL (fw->val_label)
                                ,PANGO_ATTR_SCALE,  PANGO_SCALE_SMALL
                                ,-1);
    }
    else
    {
      gimp_label_set_attributes (GTK_LABEL (fw->key_label)
                                ,PANGO_ATTR_SCALE,  PANGO_SCALE_X_SMALL
                                ,PANGO_ATTR_WEIGHT, PANGO_WEIGHT_BOLD
                                ,-1);
      gimp_label_set_attributes (GTK_LABEL (fw->val_label)
                                ,PANGO_ATTR_SCALE,  PANGO_SCALE_X_SMALL
                                ,-1);
    }
  }

  gtk_widget_show (fw->key_label);
  gtk_widget_show (fw->val_label);



  gtk_widget_show (fw->hbox);
  gtk_widget_show (fw->vbox2);
  gtk_widget_show (fw->event_box);
  gtk_widget_show (fw->vbox);

  p_frame_widget_init_dnd(fw);

  g_signal_connect (G_OBJECT (fw->event_box), "button_press_event",
                    G_CALLBACK (p_frame_widget_preview_events_cb),
                    fw);

  /* drag and drop also connects a signal handler on "button_press_event"
   * therefore we connect a handler that runs after this handler
   * (regardless if the event was hadled there or not)
   */
  g_signal_connect_after (G_OBJECT (fw->pv_ptr->da_widget), "button_press_event",
                    G_CALLBACK (p_frame_widget_preview_events_cb),
                    fw);
  if(gap_debug) printf("p_frame_widget_init_empty END\n");

}  /* end p_frame_widget_init_empty */


/* ----------------------------------
 * gap_story_dlg_render_default_icon
 * ----------------------------------
 * render a default icon,
 * depending on the track and record_type of the
 * story board elem.
 * if stb_elem is passed as NULL pointer then render
 * the icon for undefined mask reference.
 */
void
gap_story_dlg_render_default_icon(GapStoryElem *stb_elem, GapPView   *pv_ptr)
{
  GdkPixbuf *pixbuf;

  if(pv_ptr == NULL)
  {
    return;
  }

  pixbuf = NULL;
  if(stb_elem)
  {
    if(stb_elem->track == GAP_STB_MASK_TRACK_NUMBER)
    {
      switch(stb_elem->record_type)
      {
       case GAP_STBREC_VID_MOVIE:
         pixbuf = gdk_pixbuf_new_from_inline (-1, gap_story_icon_mask_movie, FALSE, NULL);
         break;
       case GAP_STBREC_VID_FRAMES:
         pixbuf = gdk_pixbuf_new_from_inline (-1, gap_story_icon_mask_frames, FALSE, NULL);
         break;
       case GAP_STBREC_VID_ANIMIMAGE:
         pixbuf = gdk_pixbuf_new_from_inline (-1, gap_story_icon_mask_animimage, FALSE, NULL);
         break;
       case GAP_STBREC_VID_IMAGE:
         pixbuf = gdk_pixbuf_new_from_inline (-1, gap_story_icon_mask_image, FALSE, NULL);
         break;
       case GAP_STBREC_VID_COLOR:
         pixbuf = gdk_pixbuf_new_from_inline (-1, gap_story_icon_color, FALSE, NULL);
         break;
       case GAP_STBREC_ATT_TRANSITION:
         pixbuf = gdk_pixbuf_new_from_inline (-1, gap_story_icon_transition_attr, FALSE, NULL);
         break;
       default:
         pixbuf = gdk_pixbuf_new_from_inline (-1, gap_story_icon_default, FALSE, NULL);
         break;
      }
    }
    else
    {
      switch(stb_elem->record_type)
      {
       case GAP_STBREC_VID_MOVIE:
         pixbuf = gdk_pixbuf_new_from_inline (-1, gap_story_icon_movie, FALSE, NULL);
         break;
       case GAP_STBREC_VID_FRAMES:
         pixbuf = gdk_pixbuf_new_from_inline (-1, gap_story_icon_frames, FALSE, NULL);
         break;
       case GAP_STBREC_VID_ANIMIMAGE:
         pixbuf = gdk_pixbuf_new_from_inline (-1, gap_story_icon_animimage, FALSE, NULL);
         break;
       case GAP_STBREC_VID_IMAGE:
         pixbuf = gdk_pixbuf_new_from_inline (-1, gap_story_icon_image, FALSE, NULL);
         break;
       case GAP_STBREC_VID_COLOR:
         pixbuf = gdk_pixbuf_new_from_inline (-1, gap_story_icon_color, FALSE, NULL);
         break;
       case GAP_STBREC_ATT_TRANSITION:
         pixbuf = gdk_pixbuf_new_from_inline (-1, gap_story_icon_transition_attr, FALSE, NULL);
         break;
       case GAP_STBREC_VID_SECTION:
         pixbuf = gdk_pixbuf_new_from_inline (-1, gap_story_icon_section, FALSE, NULL);
         break;
       case GAP_STBREC_VID_BLACKSECTION:
         pixbuf = gdk_pixbuf_new_from_inline (-1, gap_story_icon_blacksection, FALSE, NULL);
         break;
       default:
         pixbuf = gdk_pixbuf_new_from_inline (-1, gap_story_icon_default, FALSE, NULL);
         break;
      }
    }
  }
  else
  {
         pixbuf = gdk_pixbuf_new_from_inline (-1, gap_story_icon_mask_undefined, FALSE, NULL);
  }

  if(pixbuf)
  {
    gap_pview_render_from_pixbuf (pv_ptr, pixbuf);
    g_object_unref(pixbuf);
    return;
  }

  gap_pview_render_default_icon(pv_ptr);

}  /* end gap_story_dlg_render_default_icon */

/* ---------------------------------
 * p_frame_widget_render
 * ---------------------------------
 * 1.) fetch thumbnal pixbuf data,
 * 2.) if no thumbnail available
 *     try to load full image (and create the thumbnail for next usage)
 * 3.) if neither thumbnail nor image could be fetched
 *        render a default icon
 *
 */
static void
p_frame_widget_render (GapStbFrameWidget *fw)
{
   gint32  l_th_width;
   gint32  l_th_height;
   gint32  l_th_bpp;
   GdkPixbuf *pixbuf;

    if(fw->stb_elem_refptr)
    {
      if(fw->stb_elem_refptr->track == GAP_STB_MASK_TRACK_NUMBER)
      {
        fw->pv_ptr->desaturate_request = TRUE;
      }
    }

   if((fw->key_label) && (fw->val_label))
   {
     char *l_txt;
     gint32 l_framenr;
     GapStbTabWidgets *tabw;
     char  txt_buf[100];

     tabw = (GapStbTabWidgets *)fw->tabw;

     l_txt = g_strdup_printf("%d.", (int)fw->seq_nr);
     gtk_label_set_text ( GTK_LABEL(fw->key_label), l_txt);
     g_free(l_txt);

     if(tabw->vtrack == GAP_STB_MASK_TRACK_NUMBER)
     {
       /* in the mask track always show the mask_name
        * (because framenumber or timecode are not relevant for mask definitions)
        */
       if(fw->stb_elem_refptr)
       {
         if(fw->stb_elem_refptr->mask_name)
         {
           if(strlen(fw->stb_elem_refptr->mask_name) > 8)
           {
             l_txt = g_strdup_printf("%.6s..", fw->stb_elem_refptr->mask_name);
             gtk_label_set_text ( GTK_LABEL(fw->val_label), l_txt);
             g_free(l_txt);
           }
           else
           {
             gtk_label_set_text ( GTK_LABEL(fw->val_label), fw->stb_elem_refptr->mask_name);
           }
         }
       }
     }
     else
     {
       switch(tabw->edmode)
       {
        case GAP_STB_EDMO_SRC_FRAME_NUMBER_AND_COUNT:
           if((fw->stb_refptr) && (fw->stb_elem_refptr))
           {
             gint l_src_framenr;
             gint l_frame_count;
             
             l_src_framenr = fw->stb_elem_refptr->from_frame;
             l_frame_count = fw->stb_elem_refptr->nframes;
             l_txt = g_strdup_printf(":%d#%d", (int)l_src_framenr, (int)l_frame_count);
             gtk_label_set_text ( GTK_LABEL(fw->val_label), l_txt);
             g_free(l_txt);
             break;
           }
        case GAP_STB_EDMO_FRAME_NUMBER:
           if(fw->stb_refptr)
           {
             l_framenr = gap_story_get_framenr_by_story_id(fw->stb_refptr->active_section
                                                          ,fw->stb_elem_refptr->story_id
                                                          ,tabw->vtrack
                                                          );
             l_txt = g_strdup_printf("%06d", (int)l_framenr);
             gtk_label_set_text ( GTK_LABEL(fw->val_label), l_txt);
             g_free(l_txt);
             break;
           }
         case GAP_STB_EDMO_TIMECODE:
           if(fw->stb_refptr)
           {
             gdouble l_framerate;

             l_framerate = fw->stb_refptr->master_framerate;
             if(l_framerate < 1)
             {
               l_framerate = GAP_STORY_DEFAULT_FRAMERATE;
             }
             l_framenr = gap_story_get_framenr_by_story_id(fw->stb_refptr->active_section
                                                          ,fw->stb_elem_refptr->story_id
                                                          ,tabw->vtrack
                                                          );
             gap_timeconv_framenr_to_timestr(l_framenr
                               , (gdouble)l_framerate
                               , txt_buf
                               , sizeof(txt_buf)
                               );
             gtk_label_set_text ( GTK_LABEL(fw->val_label), txt_buf);
             break;
           }
         default:
           if(fw->stb_elem_refptr)
           {
             g_snprintf(txt_buf, sizeof(txt_buf), "#:%d", (int)fw->stb_elem_refptr->nframes);
             gtk_label_set_text ( GTK_LABEL(fw->val_label), txt_buf);
           }
           else
           {
             gtk_label_set_text ( GTK_LABEL(fw->val_label), " ");
           }
           break;
       }
     }
   }

   /* init preferred width and height
    * (as hint for the thumbnail loader to decide
    *  if thumbnail is to fetch from normal or large thumbnail directory
    *  just for the case when both sizes are available)
    */
   l_th_width = 128;
   l_th_height = 128;

   if((fw->stb_elem_refptr->record_type == GAP_STBREC_VID_MOVIE)
   || (fw->stb_elem_refptr->record_type == GAP_STBREC_VID_ANIMIMAGE)
   || (fw->stb_elem_refptr->record_type == GAP_STBREC_VID_SECTION))
   {
     guchar *l_th_data;

     l_th_data = gap_story_vthumb_fetch_thdata(fw->sgpp
                  ,fw->stb_refptr
                  ,fw->stb_elem_refptr
                  ,fw->stb_elem_refptr->from_frame
                  ,fw->stb_elem_refptr->seltrack
                  ,gap_story_get_preferred_decoder(fw->stb_refptr, fw->stb_elem_refptr)
                  ,&l_th_bpp
                  ,&l_th_width
                  ,&l_th_height
                  );

     if(l_th_data)
     {
       gboolean l_th_data_was_grabbed;

       l_th_data_was_grabbed = gap_pview_render_f_from_buf (fw->pv_ptr
                    , l_th_data
                    , l_th_width
                    , l_th_height
                    , l_th_bpp
                    , TRUE         /* allow_grab_src_data */
                    , fw->stb_elem_refptr->flip_request
                    , GAP_STB_FLIP_NONE  /* flip_status */
                    );
       if(!l_th_data_was_grabbed)
       {
         /* the gap_pview_render_f_from_buf procedure can grab the l_th_data
          * instead of making a private copy for later use on repaint demands.
          * if such a grab happened it returns TRUE.
          * (this is done for optimal performance reasons)
          * in such a case the caller must NOT free the src_data (l_th_data) !!!
          */
          g_free(l_th_data);
       }
       l_th_data = NULL;
       return;
     }

   }

   if(fw->frame_filename == NULL)
   {
     /* no filename available, use default icon */
     gap_story_dlg_render_default_icon(fw->stb_elem_refptr, fw->pv_ptr);
     return;
   }

   pixbuf = gap_thumb_file_load_pixbuf_thumbnail(fw->frame_filename
                                    , &l_th_width, &l_th_height
                                    , &l_th_bpp
                                    );
   if(pixbuf)
   {
     gap_pview_render_f_from_pixbuf (fw->pv_ptr
                                  , pixbuf
                                  , fw->stb_elem_refptr->flip_request
                                  , GAP_STB_FLIP_NONE  /* flip_status */
                                  );
     g_object_unref(pixbuf);
   }
   else
   {
      gint32  l_image_id;

      l_image_id = -1;

      /* skip attempt to render thubnail from full image in case cliptype is a movie
       * (attempts to load movie filetyes unsupported by the gimp core do result in annoying
       * GIMP error messages since GIMP-2.6.x, and do not make sense anyway.)
       */
      if((fw->stb_elem_refptr->record_type != GAP_STBREC_VID_MOVIE)
      && (fw->stb_elem_refptr->record_type != GAP_STBREC_VID_SECTION))
      {
        if(gap_debug)
        {
          printf("p_frame_widget_render: call gap_lib_load_image to get THUMBNAIL for:%s\n", fw->frame_filename);
        }
        l_image_id = gap_lib_load_image(fw->frame_filename);
      }

      if (l_image_id < 0)
      {
        /* could not read the image
         */
        if(gap_debug)
        {
          printf("p_frame_widget_render: fetch failed, using DEFAULT_ICON\n");
        }
        gap_story_dlg_render_default_icon(fw->stb_elem_refptr, fw->pv_ptr);
      }
      else
      {
        /* there is no need for undo on this scratch image
         * so we turn undo off for performance reasons
         */
        gimp_image_undo_disable (l_image_id);
        gap_pview_render_f_from_image (fw->pv_ptr
                                    , l_image_id
                                    , fw->stb_elem_refptr->flip_request
                                    , GAP_STB_FLIP_NONE  /* flip_status */
                                    );

        /* create thumbnail (to speed up acces next time) */
        gap_thumb_cond_gimp_file_save_thumbnail(l_image_id, fw->frame_filename);

        gimp_image_delete(l_image_id);
      }
   }

}       /* end p_frame_widget_render */


/* ---------------------------------
 * p_frame_widget_destroy
 * ---------------------------------
 */
static void
p_frame_widget_destroy(GapStbFrameWidget *fw)
{
  gap_pview_reset(fw->pv_ptr);

  if(fw->frame_filename) { g_free(fw->frame_filename); }
  fw->frame_filename = NULL;
  fw->stb_elem_refptr = NULL;
  fw->stb_refptr = NULL;
  fw->tabw = NULL;

  gtk_widget_destroy(fw->vbox);

}  /* end p_frame_widget_destroy */



/* -----------------------------
 * p_get_max_rowpage
 * -----------------------------
 */
static gint32
p_get_max_rowpage(gint32 act_elems, gint32 cols, gint rows)
{
  gint32 l_max_rowpage;

  if(cols < 1)
  {
    l_max_rowpage = (MAX(act_elems, 1) -1);
  }
  else
  {
    l_max_rowpage = ((MAX(act_elems, 1) -1) / cols);
  }
  l_max_rowpage =  1 + (l_max_rowpage - (MAX(rows,1) -1));
  return(MAX(l_max_rowpage,1));

}  /* end p_get_max_rowpage */



/* -----------------------------
 * p_story_set_range_cb
 * -----------------------------
 */
static void
p_story_set_range_cb(GapPlayerAddClip *plac_ptr)
{
  if(gap_debug)
  {
    printf("p_story_set_range_cb:\n");
    printf("  FROM : %d\n", (int)plac_ptr->range_from);
    printf("  TO : %d\n", (int)plac_ptr->range_to);
  }
  if(plac_ptr->ainfo_ptr)
  {
    GapStbMainGlobalParams  *sgpp;
    GapStbTabWidgets *tabw;
    GapStbTabWidgets *tabw_1;
    GapStbTabWidgets *tabw_2;
    GapStoryBoard *stb_dst;
    GapStoryBoard *stb_1;
    GapStoryBoard *stb_2;
    GapStoryElem *stb_elem;

    if(gap_debug)
    {
      printf("  basename  :%s:\n", plac_ptr->ainfo_ptr->basename);
      printf("  extension :%s:\n", plac_ptr->ainfo_ptr->extension);
      printf("  frame_cnt :%d\n",  (int)plac_ptr->ainfo_ptr->frame_cnt);
      printf("  old_filename :%s\n",  plac_ptr->ainfo_ptr->old_filename);
    }

    sgpp = (GapStbMainGlobalParams *)plac_ptr->user_data_ptr;
    if(sgpp)
    {
      stb_1 = (GapStoryBoard *)sgpp->cll;
      stb_2 = (GapStoryBoard *)sgpp->stb;
      tabw_1 = sgpp->cll_widgets;
      tabw_2 = sgpp->stb_widgets;
      if(sgpp->clip_target == GAP_STB_CLIPTARGET_STORYBOARD_APPEND)
      {
        stb_1 = (GapStoryBoard *)sgpp->stb;
        stb_2 = (GapStoryBoard *)sgpp->cll;
        tabw_1 = sgpp->stb_widgets;
        tabw_2 = sgpp->cll_widgets;
      }
      stb_dst = stb_1;
      tabw = tabw_1;
      if(stb_dst == NULL)
      {
        stb_dst = stb_2;
        tabw = tabw_2;
      }

      if(stb_dst)
      {
        stb_elem = gap_story_new_elem(GAP_STBREC_VID_UNKNOWN);
        if(stb_elem)
        {
          gap_stb_undo_group_begin(tabw);
          gap_stb_undo_push(tabw, GAP_STB_FEATURE_CREATE_CLIP);

          gap_story_upd_elem_from_filename(stb_elem, plac_ptr->ainfo_ptr->old_filename);
          stb_elem->track = tabw->vtrack;
          stb_elem->from_frame = plac_ptr->range_from;
          stb_elem->to_frame   = plac_ptr->range_to;
          gap_story_elem_calculate_nframes(stb_elem);
          if(plac_ptr->ainfo_ptr)
          {
            if(plac_ptr->ainfo_ptr->ainfo_type == GAP_AINFO_MOVIE)
            {
              stb_elem->record_type = GAP_STBREC_VID_MOVIE;
              stb_elem->seltrack    = plac_ptr->ainfo_ptr->seltrack;
              stb_elem->exact_seek  = 0;
              stb_elem->delace      = plac_ptr->ainfo_ptr->delace;
              //stb_elem->density   = plac_ptr->ainfo_ptr->density;

            }
          }

          gap_story_list_append_elem(stb_dst, stb_elem);

          /* refresh storyboard layout and thumbnail list widgets */
          p_recreate_tab_widgets( stb_dst
                                 ,tabw
                                 ,tabw->mount_col
                                 ,tabw->mount_row
                                 ,sgpp
                                 );
          p_render_all_frame_widgets(tabw);
          gap_stb_undo_group_end(tabw);
        }
      }
    }

  }

}  /* end p_story_set_range_cb */


/* ---------------------------------------
 * gap_story_fw_abstract_properties_dialog
 * ---------------------------------------
 */
static void
gap_story_fw_abstract_properties_dialog (GapStbFrameWidget *fw)
{
  if(fw)
  {
    GapStbMainGlobalParams *sgpp;

    sgpp = fw->sgpp;
    if (sgpp == NULL)
    {
      return;
    }

    if(fw->stb_elem_refptr)
    {
      if(gap_story_elem_is_video(fw->stb_elem_refptr))
      {
        gap_story_fw_properties_dialog(fw);
        return;
      }
      if (fw->stb_elem_refptr->record_type == GAP_STBREC_ATT_TRANSITION)
      {
        gap_story_att_fw_properties_dialog(fw);
        return;
      }
    }
  }
}  /* end gap_story_fw_abstract_properties_dialog */


/* -----------------------------
 * p_story_call_player
 * -----------------------------
 * IN: stb       Pointer to a stroyboard list structure
 * IN: imagename of one frame to playback in normal mode (NULL for storyboard playback)
 * IN: image_id  of one frame to playback in normal mode (use -1 for storyboard playback)
 *
 * IN: begin_frame   use -1 to start play from  1.st frame
 * IN: end_frame     use -1 to start play until last frame
 * IN: stb_composite true: force composite video playback (all tracks involved)
 *                   false: play clips of the selected stb_in_track
 *                          without calling the storyboard render processing.
 *
 * IN: stb_in_track  specify video track number (start with 1)
 *                   for composite video playback the stb_in_track is only relevant
 *                   if playback shall include only selected clips. In this case
 *                   the selection mapping is built based on the selected clips
 *                   in the stb_in_track.
 *
 * stb_mode:    stb != NULL, imagename == NULL, image_id == -1
 * imagemode:   stb == NULL, imagename != NULL, image_id == -1
 * normalmode:  stb == NULL, imagename == NULL, image_id >= 0
 *
 * Call the Player
 * If it is the 1.st call or the player window has closed since last call
 *    create the player widget
 * else
 *    reset the player widget
 *
 * Notes for the stb_mode:
 *   The player always gets a duplicate of the current storyboard,
 *   where the relevant clips are placed in the main section.
 *
 *   The duplicate will contain only selected elements in case where
 *   the caller has requested this via play_all == FALSE.
 *
 *   If the active_section in the specified original storyboard (stb)
 *   is NOT its main section, the active section of the original
 *   is copied as main section to the duplicate for playback.
 *   (additional the mask_section is also included in the duplicate)
 *
 *   playback is limited to simple clips of the current video track
 *   in case a valid (positive) vtrack number is specified.
 *   (this allows quick direct frame fetch operations without
 *    the use of the storyboard render processor engine,
 *    VID_PLAY_SECTION elements are rendered as black frames in this
 *    playback mode)
 *
 *   a negative vtrack number (-1) triggers composite video playback
 *   where the storyboard render processor engine is used.
 */
void
p_story_call_player(GapStbMainGlobalParams *sgpp
                   , GapStoryBoard *stb
                   , char *imagename
                   , gint32 imagewidth
                   , gint32 imageheight
                   , gdouble aspect_ratio
                   , gint32 image_id
                   , gint32 begin_frame
                   , gint32 end_frame
                   , gboolean play_all
                   , gint32 seltrack
                   , gdouble delace
                   , const char *preferred_decoder
                   , gint32 flip_request
                   , gint32 flip_status
                   , gint32 stb_in_track
                   , gboolean stb_composite
                   )
{
  GapStoryBoard *stb_dup;
  gint32 player_stb_in_track;

  if(sgpp->in_player_call)
  {
    /* this procedure is already active, and locked against
     * calls while busy
     */
    return;
  }
  if(sgpp->vthumb_prefetch_in_progress != GAP_VTHUMB_PREFETCH_NOT_ACTIVE)
  {
    /* refuse player call while vthumb prefetch is busy
     */
    g_message(_("playback was blocked (video file access is busy)"));
    return;
  }


  sgpp->in_player_call = TRUE;
  gap_story_vthumb_close_videofile(sgpp);

  player_stb_in_track = stb_in_track;

  stb_dup = NULL;
  if(stb)
  {
    if (stb_composite)
    {
      player_stb_in_track = -1;  /* force composite playback in the player */
    }

    if(gap_debug)
    {
       printf("\n\n\n\n\n\n\n\n\np_story_call_player:"
         " play_all:%d stb_composite:%d stb_in_track:%d begin:%d end:%d\n"
         ,(int)play_all
         ,(int)stb_composite
         ,(int)stb_in_track
         ,(int)begin_frame
         ,(int)end_frame
         );

//       printf("The original storyboard list:\n");

//       gap_story_debug_print_list(stb);
//       printf("==========###########========\n\n END of The original storyboard list:\n");
//       fflush(stdout);
    }
    /* make a copy of the storyboard list
     * for the internal use in the player
     * (note that section clip playback is only supported
     *  if the main section is active and composite video playback is selected)
     */
    if(play_all)
    {
      if(stb->active_section == gap_story_find_main_section(stb))
      {
        stb_dup = gap_story_duplicate_full(stb);
      }
      else
      {
        stb_dup = gap_story_duplicate_active_and_mask_section(stb);
      }
    }
    else
    {
      if ((stb_composite) || (stb_in_track < 0))
      {
        stb_dup = gap_story_duplicate_active_and_mask_section(stb);
        if(stb->active_section == gap_story_find_main_section(stb))
        {
           /* for composite video playback of "only selected elements" in
            * the main section, we have to copy implicitly
            * all referenced sub sections.
            * (but we simply add all available subsection)
            */
           gap_story_copy_sub_sections(stb, stb_dup);
        }

        /* create selection mapping from source storyboard stb
         * and attach the mapping to the duplicate.
         * Note that the duplicate must contain all clips of the active section
         * (and not only the selected clips), so that the mapping
         * can be used there too.
         */
        stb_dup->mapping =
          gap_story_create_new_mapping_from_selection(
               stb->active_section
             , MAX(1, stb_in_track));

        if(gap_debug)
        {
          gap_story_debug_print_mapping(stb_dup->mapping);
          fflush(stdout);
        }

      }
      else
      {
        /* here we handle storyboard playback of selected clips
         * in the current videotrack of the active_section.
         * (the player will not use the render processor,
         * and will be much faster in this mode.
         * playback of sub section references is not supported in this case.)
         */
        stb_dup = gap_story_duplicate_sel_only(stb, stb_in_track);
        gap_story_enable_hidden_maskdefinitions(stb_dup);
      }


    }


    if(stb_dup)
    {
      GapStorySection *dup_main_section;

      if(gap_debug)
      {
         printf("p_story_call_player: The duplicated storyboard list:");
         gap_story_debug_print_list(stb_dup);
         printf("==========###########========\n\n END of The duplicated storyboard list:\n");
         fflush(stdout);
      }

      if(TRUE == p_is_debug_feature_item_enabled("save_stb_duplicate_on_call_player"))
      {
        /* debug feature: save the storyboard duplicate that will be passed
         * to the player.
         */
        gap_story_save(stb_dup, "STB_DUPLICATE_on_call_player.txt");
      }


      dup_main_section = gap_story_find_main_section(stb_dup);
      if(dup_main_section->stb_elem == NULL)
      {
        /* there was no selection, now try to playback the
         * whole storyboard
         */
        gap_story_free_storyboard(&stb_dup);
        if(stb->active_section == gap_story_find_main_section(stb))
        {
          stb_dup = gap_story_duplicate_full(stb);
        }
        else
        {
          stb_dup = gap_story_duplicate_active_and_mask_section(stb);
        }


      }
    }
    if(stb_dup == NULL)
    {
      return;  /* in case of errors: NO playback possible */
      sgpp->in_player_call = FALSE;
    }

    if(sgpp->stb_preview_render_full_size != TRUE)
    {
      /* if the storboard duplicate (stb_dup) includes at least one unscalable clip
       * (where either with or height is fixed to original size)
       * then we must force slow internal rendering at full size.
       */
      if(gap_story_checkForAtLeatOneClipWithScalingDisabled(stb_dup) == TRUE)
      {
        if(gap_debug)
        {
          printf("Storyboard includes non-scaleable clip force rendering at full size!\n");
        }
      }
      else
      {
        gint32 previewWidth;
        gint32 previewHeight;

        /* pick small size for preview rendering at composite video playback via storyboard processor
         * purpose when posible for performance reasons.
         */
        if(sgpp->plp)
        {
          previewWidth = sgpp->plp->pv_width;
          previewHeight = sgpp->plp->pv_height;
        }
        else
        {
          /* in case player was not yet called (e.g this ist the very 1st player call)
           * the preview size is not yet known.
           * Assume a preview width of 320 pixel in that case
           */
          previewWidth = 320;
          previewHeight = previewWidth * (stb_dup->master_height / MAX(stb_dup->master_width,1));
        }

        stb_dup->master_width = MIN(stb_dup->master_width, previewWidth);
        stb_dup->master_height = MIN(stb_dup->master_height, previewHeight);
      }
    }

  }

  if(sgpp->plp)
  {
    if(sgpp->plp->stb_ptr)
    {
      /* free the (old duplicate) of the storyboard list
       * that was used in the previous
       * player call
       */
      gap_story_free_storyboard(&sgpp->plp->stb_ptr);
    }

    if((sgpp->plp->shell_window == NULL)
    && (sgpp->plp->docking_container == NULL))
    {
      /*if(gap_debug)*/ printf("Player shell has gone, force Reopen now\n");

      gap_player_dlg_cleanup(sgpp->plp);
      g_free(sgpp->plp);
      sgpp->plp = NULL;
    }
  }

  if(sgpp->plp == NULL)
  {
    if(gap_debug)
    {
      printf("p_story_call_player: 1.st start\n");
    }

    /* 1. START mode */
    sgpp->plp = (GapPlayerMainGlobalParams *)g_malloc0(sizeof(GapPlayerMainGlobalParams));

    if(sgpp->plp)
    {
      sgpp->plp->standalone_mode = FALSE;  /* player acts as widget and does not call gtk_main_quit */
      sgpp->plp->docking_container = sgpp->player_frame;

      sgpp->plp->help_id = GAP_STB_PLAYER_HELP_ID;
      if(sgpp->plp->docking_container)
      {
        /* the player widget is created without Help button
         * when it is part of the storyboard window
         */
        sgpp->plp->help_id = NULL;
      }

      sgpp->plp->autostart = TRUE;
      sgpp->plp->caller_range_linked = FALSE;
      sgpp->plp->use_thumbnails = FALSE;
      sgpp->plp->exact_timing = FALSE;
      sgpp->plp->play_selection_only = TRUE;
      sgpp->plp->play_loop = FALSE;
      sgpp->plp->play_pingpong = FALSE;
      sgpp->plp->play_backward = FALSE;

      sgpp->plp->stb_ptr = stb_dup;
      sgpp->plp->stb_in_track = player_stb_in_track;
      sgpp->plp->image_id = image_id;
      sgpp->plp->imagename = NULL;
      if(imagename)
      {
        sgpp->plp->imagename = g_strdup(imagename);
      }
      sgpp->plp->imagewidth = imagewidth;
      sgpp->plp->imageheight = imageheight;
      sgpp->plp->aspect_ratio = aspect_ratio;

      sgpp->plp->play_current_framenr = 0;
      sgpp->plp->begin_frame = begin_frame;
      sgpp->plp->end_frame = end_frame;

      sgpp->plp->fptr_set_range = p_story_set_range_cb;
      sgpp->plp->user_data_ptr = sgpp;
      sgpp->plp->seltrack = seltrack;
      sgpp->plp->delace = delace;
      sgpp->plp->preferred_decoder = g_strdup(preferred_decoder);
      sgpp->plp->force_open_as_video = FALSE;   /* FALSE: try video open only for known videofile extensions */
      sgpp->plp->have_progress_bar = TRUE;
      sgpp->plp->flip_request = flip_request;
      sgpp->plp->flip_status = flip_status;

      gap_player_dlg_create(sgpp->plp);

    }
  }
  else
  {
    if(gap_debug)
    {
      printf("p_story_call_player: RE start\n");
    }

    sgpp->plp->aspect_ratio = aspect_ratio;

    /* RESTART mode */
    gap_player_dlg_restart(sgpp->plp
                      , TRUE               /* gboolean autostart */
                      , image_id
                      , imagename
                      , imagewidth
                      , imageheight
                      , stb_dup
                      , begin_frame
                      , end_frame
                      , TRUE              /* gboolean play_selection_only */
                      , seltrack
                      , delace
                      , preferred_decoder
                      , FALSE
                      , flip_request
                      , flip_status
                      , player_stb_in_track
                      );

  }

  sgpp->in_player_call = FALSE;
}  /* end p_story_call_player */



/* -----------------------------
 * p_call_master_encoder
 * -----------------------------
 * INTERACTIVE call of the GAP master videoencoder dialog plug-in
 */
static void
p_call_master_encoder(GapStbMainGlobalParams *sgpp
                     , GapStoryBoard *stb
                     , const char *stb_filename
                   )
{
#define GAP_PLUG_IN_MASTER_ENCODER  "plug_in_gap_vid_encode_master"
  GimpParam* l_params;
  gint       l_retvals;
  gint       l_rc;
  gint32     dummy_layer_id;

  gint32 vid_width;
  gint32 vid_height;

  if(gap_debug)
  {
    printf("p_call_master_encoder\n");
  }

  if((sgpp == NULL)
  || (stb == NULL))
  {
    return;
  }

  l_rc = -1;
  gap_story_get_master_size_respecting_aspect(stb, &vid_width, &vid_height);

  gtk_window_iconify(sgpp->shell_window);
  while (gtk_events_pending ())
  {
    gtk_main_iteration ();
  }

  dummy_layer_id = gap_image_get_any_layer(sgpp->image_id);
  /* generic call of GAP master video encoder plugin */
  l_params = gimp_run_procedure (GAP_PLUG_IN_MASTER_ENCODER,
                     &l_retvals,
                     GIMP_PDB_INT32,  GIMP_RUN_INTERACTIVE,
                     GIMP_PDB_IMAGE,  sgpp->image_id,  /* pass the image where invoked from */
                     GIMP_PDB_DRAWABLE, dummy_layer_id,
                     GIMP_PDB_STRING, "video_output.avi",
                     GIMP_PDB_INT32,  1,            /* range_from */
                     GIMP_PDB_INT32,  2,            /* range_to */
                     GIMP_PDB_INT32,  vid_width,
                     GIMP_PDB_INT32,  vid_height,
                     GIMP_PDB_INT32,  1,            /* 1 VID_FMT_PAL,  2 VID_FMT_NTSC */
                     GIMP_PDB_FLOAT,  MAX(stb->master_framerate, 1.0),
                     GIMP_PDB_INT32,  44100,        /* samplerate */
                     GIMP_PDB_STRING, NULL,         /* audfile l_16bit_wav_file */
                     GIMP_PDB_STRING, NULL,         /* vid_enc_plugin */
                     GIMP_PDB_STRING, NULL,         /* filtermacro_file */
                     GIMP_PDB_STRING, stb_filename, /* storyboard_file */
                     GIMP_PDB_INT32,  2,            /* GAP_RNGTYPE_STORYBOARD input_mode */
                     GIMP_PDB_END);
  switch(l_params[0].data.d_status)
  {
    case GIMP_PDB_SUCCESS:
      l_rc = 0;
      break;
    case GIMP_PDB_CANCEL:
      break;
    default:
      printf("gap_story_dialog:p_call_master_encoder\n"
             "PDB calling error %s\n"
             "status: %d\n"
            , GAP_PLUG_IN_MASTER_ENCODER
            , (int)l_params[0].data.d_status
            );
      break;
  }
  g_free(l_params);
  gtk_window_deiconify(sgpp->shell_window);

}  /* end p_call_master_encoder */

/* -----------------------------
 * p_restore_edit_settings
 * -----------------------------
 */
static void
p_restore_edit_settings (GapStbTabWidgets *tabw, GapStoryBoard *stb)
{
  if(stb == NULL)
  {
    return;
  }
  if (stb->edit_settings)
  {
    GapStorySection *section;
    gint32 l_rowpage;
    gint32 l_page;

    section =
      gap_story_find_section_by_name(stb, stb->edit_settings->section_name);
    if (section == NULL)
    {
      section = gap_story_find_main_section(stb);
    }

    stb->active_section = section;
    tabw->vtrack = CLAMP(stb->edit_settings->track, 0, GAP_STB_MAX_VID_TRACKS);
    l_rowpage = 1;
    l_page = MAX(1, stb->edit_settings->page);

    /* restore scroll position, recalculate the page
     * (that reprensents the scroll position)
     * if(stb->layout_cols > 0)       { tabw->cols = stb->layout_cols; }
     * if(stb->layout_rows > 0)       { tabw->rows = stb->layout_rows; }
     * if(stb->layout_thumbsize > 0)  { tabw->thumbsize = stb->layout_thumbsize; }
     */
    if (l_page != 1)
    {
      gint32 l_max_rowpage = 1;
      gint32 l_act_elems = 0;

      if ((tabw->cols != stb->layout_cols)
      && (tabw->cols > 1)
      && (stb->layout_cols > 1))
      {
        gdouble new_page;

        /* convert the page to current column layout */
        new_page = (gdouble)(l_page -1) * (gdouble)stb->layout_cols / (gdouble)tabw->cols;
        l_page = 1 + new_page;
      }

      l_act_elems = gap_story_count_active_elements(stb, tabw->vtrack );
      l_max_rowpage = p_get_max_rowpage(l_act_elems, tabw->cols, tabw->rows);

      l_rowpage = CLAMP(l_page, 1, l_max_rowpage);
    }
    tabw->rowpage = l_rowpage;


    gtk_adjustment_set_value(GTK_ADJUSTMENT(tabw->vtrack_spinbutton_adj)
                               , tabw->vtrack);


  }

}  /* end p_restore_edit_settings */


/* -----------------------------
 * p_tabw_process_undo
 * -----------------------------
 */
static void
p_tabw_process_undo(GapStbTabWidgets *tabw)
{
  GapStoryBoard *stb;
  GapStoryBoard *old_stb;

  old_stb = p_tabw_get_stb_ptr(tabw);
  if (old_stb)
  {
    p_tabw_destroy_all_popup_dlg(tabw);
  }
  stb = gap_stb_undo_pop(tabw);
  if (stb)
  {
     p_tabw_replace_storyboard_and_widgets(tabw, stb);
  }
}  /* end p_tabw_process_undo */

/* -----------------------------
 * p_tabw_process_redo
 * -----------------------------
 */
static void
p_tabw_process_redo(GapStbTabWidgets *tabw)
{
  GapStoryBoard *stb;
  GapStoryBoard *old_stb;

  old_stb = p_tabw_get_stb_ptr(tabw);
  if (old_stb)
  {
    p_tabw_destroy_all_popup_dlg(tabw);
  }

  stb = gap_stb_undo_redo(tabw);
  if (stb)
  {
     p_tabw_replace_storyboard_and_widgets(tabw, stb);
  }
}  /* end p_tabw_process_redo */




/* ---------------------------------------
 * p_tabw_replace_storyboard_and_widgets
 * ---------------------------------------
 * replace the storyboard (if there is one) and
 * rebuild all the widgets.
 * further close all pop-up dialogs related to the old existing
 * storyboard.
 * This procedure is typically called at load, undo and redo
 * features.
 */
static void
p_tabw_replace_storyboard_and_widgets(GapStbTabWidgets *tabw, GapStoryBoard *stb)
{
  GapStoryBoard *old_stb;
  GapStbMainGlobalParams *sgpp;
  gint32 target_rowpage;

  if ((tabw == NULL) || (stb == NULL))
  {
    return;
  }

  sgpp = (GapStbMainGlobalParams *)tabw->sgpp;
  if(sgpp == NULL)  { return; }

  old_stb = p_tabw_get_stb_ptr(tabw);
  if (old_stb)
  {
    p_tabw_destroy_all_popup_dlg(tabw);
  }

  if(tabw == sgpp->cll_widgets)
  {
    /* drop the old cliplist structure */
    gap_story_free_storyboard(&sgpp->cll);
    sgpp->cll = stb;
    sgpp->cll->master_type = GAP_STB_MASTER_TYPE_CLIPLIST;
  }
  else
  {
    if(tabw == sgpp->stb_widgets)
    {
      /* drop the old storyboard structure */
      gap_story_free_storyboard(&sgpp->stb);
      sgpp->stb = stb;
      sgpp->stb->master_type = GAP_STB_MASTER_TYPE_STORYBOARD;
    }
  }


  p_restore_edit_settings(tabw, stb);
  target_rowpage = tabw->rowpage;

  /* refresh storyboard layout and thumbnail list widgets */
  p_recreate_tab_widgets( stb
                         ,tabw
                         ,tabw->mount_col
                         ,tabw->mount_row
                         ,sgpp
                         );
  p_render_all_frame_widgets(tabw);

  /* update the combo_box to reflect active_section */
  p_set_strings_for_section_combo(tabw);

  /* update vscale and corresponding rowpage spinbutton */
  p_update_vscale_page(tabw);

  if(gap_debug)
  {
    printf("p_tabw_replace_storyboard_and_widgets:\n"
           "  >> target rowpage:%d tabw->rowpage:%d rowpage_spinbutton:%.4f\n"
           ,(int)target_rowpage
           ,(int)tabw->rowpage
           ,(float)GTK_ADJUSTMENT(tabw->rowpage_spinbutton_adj)->value
           );
  }

  /* update the hidden rowpage_spinbutton widget
   * to target_rowpage value.
   */
  {
    gint32 rowpage_spinbutton_value;

    rowpage_spinbutton_value = GTK_ADJUSTMENT(tabw->rowpage_spinbutton_adj)->value;
    if(rowpage_spinbutton_value != target_rowpage)
    {
      gtk_adjustment_set_value(GTK_ADJUSTMENT(tabw->rowpage_spinbutton_adj)
                              , (gdouble)target_rowpage);
    }

  }


  gap_story_dlg_tabw_undo_redo_sensitivity(tabw);

}  /* end p_tabw_replace_storyboard_and_widgets */



/* -----------------------------
 * p_storyboard_reload
 * -----------------------------
 * return TRUE on successful reload
 */
static gboolean
p_storyboard_reload (GapStbMainGlobalParams *sgpp)
{
  gboolean l_ret;

  l_ret = FALSE;

  if(sgpp)
  {
    GapStoryBoard *stb;

    /* load the new storyboard structure from file */
    stb = gap_story_parse(sgpp->storyboard_filename);
    if(stb)
    {
      if(stb->errtext != NULL)
      {
        g_message(_("** ERROR: Storyboard parser reported:\n%s\n"), stb->errtext);
        gap_story_free_storyboard(&stb);
      }
      else
      {
        p_tabw_replace_storyboard_and_widgets(sgpp->stb_widgets, stb);
        gap_stb_undo_destroy_undo_stack(sgpp->stb_widgets);

        l_ret = TRUE;
      }
    }

  }
  return (l_ret);
}  /* end p_storyboard_reload */

/* -----------------------------
 * p_cliplist_reload
 * -----------------------------
 * return TRUE on successful reload
 */
static gboolean
p_cliplist_reload (GapStbMainGlobalParams *sgpp)
{
  gboolean l_ret;

  l_ret = FALSE;

  if(sgpp)
  {
    GapStoryBoard *stb;

    /* load the new storyboard structure from file */
    stb = gap_story_parse(sgpp->cliplist_filename);
    if(stb)
    {
      if(stb->errtext != NULL)
      {
        g_message(_("** ERROR: Storyboard parser reported:\n%s\n"), stb->errtext);
        gap_story_free_storyboard(&stb);
      }
      else
      {
        p_tabw_replace_storyboard_and_widgets(sgpp->cll_widgets, stb);
        gap_stb_undo_destroy_undo_stack(sgpp->cll_widgets);

        l_ret = TRUE;
      }
    }

  }
  return (l_ret);
}  /* end p_cliplist_reload */



/*
 * DIALOG and callback stuff
 */

/* -----------------------------
 * p_player_cll_mode_cb
 * -----------------------------
 */
static void
p_player_cll_mode_cb (GtkWidget *w
                     , GdkEventButton  *bevent
                     , GapStbMainGlobalParams *sgpp)
{
  gint32 imagewidth;
  gint32 imageheight;
  gdouble aspect_ratio;
  gint32 stb_in_track;
  gboolean play_all;
  gboolean stb_composite;
  GapStbTabWidgets  *tabw;


  if(sgpp->cll)
  {
    play_all = FALSE;
    stb_composite = FALSE;

    tabw = sgpp->cll_widgets;

    stb_in_track = 1;
    if(tabw)
    {
      stb_in_track = tabw->vtrack;
    }
    if(bevent)
    {
      if(bevent->state & GDK_SHIFT_MASK)
      {
        play_all = TRUE;
      }
      if(bevent->state & GDK_CONTROL_MASK)
      {
        stb_composite = TRUE;
      }
    }

    if(sgpp->force_stb_aspect)
    {
      aspect_ratio = gap_story_get_master_size_respecting_aspect(sgpp->cll
                                         , &imagewidth
                                         , &imageheight);
    }
    else
    {
      aspect_ratio = GAP_PLAYER_DONT_FORCE_ASPECT;
      gap_story_get_master_pixelsize(sgpp->cll
                                         , &imagewidth
                                         , &imageheight);
    }
    p_story_call_player(sgpp
                         ,sgpp->cll
                         ,NULL      /* no imagename */
                         ,imagewidth
                         ,imageheight
                         ,aspect_ratio
                         ,-1        /* image_id (unused in storyboard playback mode) */
                         ,-1        /* play from start */
                         ,-1        /* play until end */
                         ,play_all
                         ,1         /* seltrack */
                         ,0.0       /* delace */
                         ,gap_story_get_preferred_decoder(sgpp->cll, NULL)
                         ,GAP_STB_FLIP_NONE
                         ,GAP_STB_FLIP_NONE
                         ,stb_in_track
                         ,stb_composite
                         );
  }
}  /* end p_player_cll_mode_cb */

/* -----------------------------
 * p_player_stb_mode_cb
 * -----------------------------
 */
static void
p_player_stb_mode_cb (GtkWidget *w
                     , GdkEventButton  *bevent
                     , GapStbMainGlobalParams *sgpp)
{
  gint32 imagewidth;
  gint32 imageheight;
  gdouble aspect_ratio;
  gint32 stb_in_track;
  gboolean play_all;
  gboolean stb_composite;
  GapStbTabWidgets  *tabw;

  if(sgpp->stb)
  {
    play_all = FALSE;
    stb_composite = FALSE;

    tabw = sgpp->stb_widgets;

    stb_in_track = 1;
    if(tabw)
    {
      stb_in_track = tabw->vtrack;
    }
    if(bevent)
    {
      if(bevent->state & GDK_SHIFT_MASK)
      {
        play_all = TRUE;
      }
      if(bevent->state & GDK_CONTROL_MASK)
      {
        stb_composite = TRUE;
      }
    }

    if(sgpp->force_stb_aspect)
    {
      aspect_ratio = gap_story_get_master_size_respecting_aspect(sgpp->stb
                                         , &imagewidth
                                         , &imageheight);
    }
    else
    {
      aspect_ratio = GAP_PLAYER_DONT_FORCE_ASPECT;
      gap_story_get_master_pixelsize(sgpp->stb
                                         , &imagewidth
                                         , &imageheight);
    }

    p_story_call_player(sgpp
                         ,sgpp->stb
                         ,NULL      /* no imagename */
                         ,imagewidth
                         ,imageheight
                         ,aspect_ratio
                         ,-1        /* image_id (unused in storyboard playback mode) */
                         ,-1        /* play from start */
                         ,-1        /* play until end */
                         ,play_all
                         ,1         /* seltrack */
                         ,0.0       /* delace */
                         ,gap_story_get_preferred_decoder(sgpp->stb, NULL)
                         ,GAP_STB_FLIP_NONE
                         ,GAP_STB_FLIP_NONE
                         ,stb_in_track
                         ,stb_composite
                         );
  }
}  /* end p_player_stb_mode_cb */


/* -----------------------------
 * p_player_img_mode_cb
 * -----------------------------
 * this is used to init the player window
 * with image where the storyboard was invoked from.
 */
static void
p_player_img_mode_cb (GtkWidget *w,
                      GapStbMainGlobalParams *sgpp)
{
  gint32 imagewidth;
  gint32 imageheight;



  imagewidth = gimp_image_width(sgpp->image_id);
  imageheight = gimp_image_height(sgpp->image_id);

  if(gap_debug)
  {
    printf("p_player_img_mode_cb: init player from image:id:%d\n"
       , sgpp->image_id
       );
  }

  p_story_call_player(sgpp
                     ,NULL              /* Play Normal mode without storyboard */
                     ,NULL              /* no imagename mode */
                     ,imagewidth
                     ,imageheight
                     ,GAP_PLAYER_DONT_FORCE_ASPECT
                     ,sgpp->image_id
                     ,-1                /* play from start */
                     ,-1                /* play until end */
                     ,FALSE      /* play all */
                     ,1         /* seltrack */
                     ,0.0       /* delace */
                     ,"libavformat"
                     ,GAP_STB_FLIP_NONE
                     ,GAP_STB_FLIP_NONE
                     ,1                /* stb_in_track (not relevant here) */
                     ,FALSE            /* stb_composite (not relevant here) */
                     );
}  /* end p_player_img_mode_cb */



/* -----------------------------
 * p_cancel_button_cb
 * -----------------------------
 */
static void
p_cancel_button_cb (GtkWidget *w,
                    GapStbMainGlobalParams *sgpp)
{

  if(gap_debug)
  {
    if(w != NULL)
    {
      printf("VIDEO-CANCEL BUTTON clicked\n");
    }
    else
    {
      printf("VIDEO-CANCEL occurred implicitly\n");
    }
  }

  if(sgpp)
  {
    sgpp->cancel_video_api = TRUE;
    sgpp->auto_vthumb_refresh_canceled = TRUE;

    p_reset_progress_bars(sgpp);

    if(sgpp->vthumb_prefetch_in_progress == GAP_VTHUMB_PREFETCH_NOT_ACTIVE)
    {
      return;
    }

    sgpp->vthumb_prefetch_in_progress = GAP_VTHUMB_PREFETCH_NOT_ACTIVE;
    if(sgpp->menu_item_win_vthumbs != NULL)
    {
      /* implicitly switch off auto_vthumb  (by setting the relevant menu item FALSE)
       */
      gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(sgpp->menu_item_win_vthumbs), FALSE);
    }
  }

}  /* end p_cancel_button_cb */



/* -----------------------------------
 * p_tabw_add_elem
 * -----------------------------------
 */
static void
p_tabw_add_elem (GapStbTabWidgets *tabw, GapStbMainGlobalParams *sgpp, GapStoryBoard *stb_dst)
{
  GapStoryElem *stb_elem;
  if(gap_debug) printf("p_tabw_add_elem\n");

  if(sgpp == NULL)    { return; }
  if(tabw == NULL)    { return; }
  if(stb_dst == NULL) { return; }

  stb_elem = gap_story_new_elem(GAP_STBREC_VID_IMAGE);
  /* stb_elem->orig_filename = g_strdup("empty.xcf"); */
  stb_elem->track = tabw->vtrack;
  stb_elem->from_frame = 1;
  stb_elem->to_frame = 1;
  stb_elem->nloop = 1;
  stb_elem->nframes = 1;
  if(stb_elem)
  {
    gap_stb_undo_group_begin(tabw);
    gap_stb_undo_push(tabw, GAP_STB_FEATURE_CREATE_CLIP);

    gap_story_list_append_elem(stb_dst, stb_elem);

    /* refresh storyboard layout and thumbnail list widgets */
    p_recreate_tab_widgets( stb_dst
                                 ,tabw
                                 ,tabw->mount_col
                                 ,tabw->mount_row
                                 ,sgpp
                                 );
    p_render_all_frame_widgets(tabw);
    gap_story_stb_elem_properties_dialog(tabw, stb_elem, stb_dst);
    gap_stb_undo_group_end(tabw);
  }

  p_tabw_update_frame_label_and_rowpage_limits(tabw, sgpp);
}  /* end p_tabw_add_elem */


/* -----------------------------------
 * p_tabw_add_section_elem
 * -----------------------------------
 */
static void
p_tabw_add_section_elem (GapStbTabWidgets *tabw, GapStbMainGlobalParams *sgpp, GapStoryBoard *stb_dst)
{
  GapStoryElem *stb_elem;
  GapStorySection *referable_section;
  gint32 l_nframes;

  if(gap_debug) printf("p_tabw_add_section_elem\n");

  if(sgpp == NULL)    { return; }
  if(tabw == NULL)    { return; }
  if(stb_dst == NULL) { return; }
  if(stb_dst->active_section == NULL) { return; }

  referable_section = gap_story_find_first_referable_subsection(stb_dst);
  if(referable_section == NULL) { return; }

  l_nframes = gap_story_count_total_frames_in_section(referable_section);

  stb_elem = gap_story_new_elem(GAP_STBREC_VID_SECTION);
  stb_elem->orig_filename = g_strdup(referable_section->section_name);
  stb_elem->track = tabw->vtrack;
  stb_elem->from_frame = 1;
  stb_elem->to_frame = l_nframes;
  stb_elem->nloop = 1;
  stb_elem->nframes = l_nframes;
  if(stb_elem)
  {
    gap_stb_undo_group_begin(tabw);
    gap_stb_undo_push(tabw, GAP_STB_FEATURE_CREATE_SECTION_CLIP);

    gap_story_list_append_elem(stb_dst, stb_elem);

    /* refresh storyboard layout and thumbnail list widgets */
    p_recreate_tab_widgets( stb_dst
                                 ,tabw
                                 ,tabw->mount_col
                                 ,tabw->mount_row
                                 ,sgpp
                                 );
    p_render_all_frame_widgets(tabw);
    gap_story_stb_elem_properties_dialog(tabw, stb_elem, stb_dst);
    gap_stb_undo_group_end(tabw);
  }

  p_tabw_update_frame_label_and_rowpage_limits(tabw, sgpp);
}  /* end p_tabw_add_section_elem */


/* -----------------------------------
 * p_tabw_add_att_elem
 * -----------------------------------
 */
static void
p_tabw_add_att_elem (GapStbTabWidgets *tabw, GapStbMainGlobalParams *sgpp, GapStoryBoard *stb_dst)
{
  GapStoryElem *stb_elem;
  if(gap_debug) printf("p_tabw_add_att_elem\n");

  if(sgpp == NULL)    { return; }
  if(tabw == NULL)    { return; }
  if(stb_dst == NULL) { return; }

  /* initial settings for creating a new attribute transition element */
  stb_elem = gap_story_new_elem(GAP_STBREC_ATT_TRANSITION);
  stb_elem->track = tabw->vtrack;
  stb_elem->att_keep_proportions = FALSE;
  stb_elem->att_fit_width = TRUE;
  stb_elem->att_fit_height = TRUE;
  {
    gint ii;
    for(ii=0; ii < GAP_STB_ATT_TYPES_ARRAY_MAX; ii++)
    {
      gdouble l_from;
      gdouble l_to;
      gboolean l_enable;

      l_from = 0.0;
      l_to = 0.0;
      l_enable = FALSE;

      if ((ii == GAP_STB_ATT_TYPE_ZOOM_X)
      ||  (ii == GAP_STB_ATT_TYPE_ZOOM_Y))
      {
        l_from = 2.0;   /* from double size */
        l_to = 1.0;     /* to 1:1 original size */
        l_enable = FALSE;
      }

      if ((ii == GAP_STB_ATT_TYPE_MOVE_X)
      ||  (ii == GAP_STB_ATT_TYPE_MOVE_Y))
      {
        l_from = -1.0;    /* from completely left/top outside */
        l_to = 0.0;       /* to centered position */
        l_enable = FALSE;
      }
      if (ii == GAP_STB_ATT_TYPE_OPACITY)
      {
        l_from = 1.0;   /* from fully opaque */
        l_to = 0.0;     /* to fully transparent */
        l_enable = TRUE;
      }

      stb_elem->att_arr_enable[ii]     = l_enable;
      stb_elem->att_arr_value_from[ii] = l_from;
      stb_elem->att_arr_value_to[ii]   = l_to;
      stb_elem->att_arr_value_dur[ii]  = 20;
    }
  }

  if(stb_elem)
  {
    gap_stb_undo_group_begin(tabw);
    gap_stb_undo_push(tabw, GAP_STB_FEATURE_CREATE_TRANSITION);

    gap_story_list_append_elem(stb_dst, stb_elem);

    /* refresh storyboard layout and thumbnail list widgets */
    p_recreate_tab_widgets( stb_dst
                                 ,tabw
                                 ,tabw->mount_col
                                 ,tabw->mount_row
                                 ,sgpp
                                 );
    p_render_all_frame_widgets(tabw);
    gap_story_att_stb_elem_properties_dialog(tabw, stb_elem, stb_dst);
    gap_stb_undo_group_end(tabw);
  }

  p_tabw_update_frame_label_and_rowpage_limits(tabw, sgpp);
}  /* end p_tabw_add_att_elem */


/* -----------------------------
 * p_tabw_new_clip_cb
 * -----------------------------
 * callback for the new clip button
 * SHIFT: create transition (disabled in mask_section)
 * CTRL:  create section clip (enabled in MAIN section, disabled in all other sections)
 * standard clips can be create in all section types (without any modifier key)
 */
static void
p_tabw_new_clip_cb (GtkWidget *w
                     , GdkEventButton  *bevent
                     , GapStbTabWidgets *tabw)
{
  GapStoryBoard  *stb_dst;

  if (tabw == NULL) { return; }
  if (tabw->sgpp == NULL) { return; }

  stb_dst = p_tabw_get_stb_ptr(tabw);

  if(stb_dst != NULL)
  {
    if(bevent)
    {
      if(bevent->state & GDK_SHIFT_MASK)
      {
        if (stb_dst->active_section != stb_dst->mask_section)
        {
          p_tabw_add_att_elem (tabw, tabw->sgpp, stb_dst);
        }
        return;
      }
      if(bevent->state & GDK_CONTROL_MASK)
      {
        if (stb_dst->active_section == gap_story_find_main_section(stb_dst))
        {
          p_tabw_add_section_elem (tabw, tabw->sgpp, stb_dst);
        }
        return;
      }
    }

    p_tabw_add_elem (tabw, tabw->sgpp, stb_dst);

  }
}  /* end p_tabw_new_clip_cb */


/* -----------------------------
 * p_filesel_tabw_close_cb
 * -----------------------------
 */
static void
p_filesel_tabw_close_cb ( GtkWidget *widget
                        , GapStbTabWidgets *tabw)
{
  if(tabw->filesel == NULL) return;

  gtk_widget_destroy(GTK_WIDGET(tabw->filesel));
  tabw->filesel = NULL;   /* now filesel_story is closed */

}  /* end p_filesel_tabw_close_cb */


/* --------------------------------
 * p_filesel_tabw_ok
 * --------------------------------
 */
static gboolean
p_filesel_tabw_ok (GtkWidget *widget
                   ,GapStbTabWidgets *tabw)
{
  const gchar *filename;
  gboolean     rc;

  rc = FALSE;
  if(tabw == NULL) return (rc);
  if(tabw->filesel == NULL) return (rc);

  filename = gtk_file_selection_get_filename (GTK_FILE_SELECTION (tabw->filesel));
  if(filename)
  {
    if(tabw->filename_refptr)
    {
      g_snprintf(tabw->filename_refptr
              , tabw->filename_maxlen
              , "%s"
              , filename
              );
      gtk_entry_set_text(GTK_ENTRY(tabw->filename_entry), filename);
    }
    rc = TRUE;
  }

  gtk_widget_destroy(GTK_WIDGET(tabw->filesel));
  tabw->filesel = NULL;

  return(rc);
}  /* end p_filesel_tabw_ok */

/* --------------------------------
 * p_filesel_tabw_load_ok_cb
 * --------------------------------
 */
static void
p_filesel_tabw_load_ok_cb (GtkWidget *widget
                   ,GapStbTabWidgets *tabw)
{
  GapStbMainGlobalParams *sgpp;

  sgpp = (GapStbMainGlobalParams *)tabw->sgpp;
  sgpp->auto_vthumb_refresh_canceled = FALSE;
  if(p_filesel_tabw_ok(widget, tabw))
  {
    if(sgpp->auto_vthumb)
    {
      /* pending vthumb prefetch suspends video file access
       * at vthumb fetching.
       * The access shall be done only once and for all vthumbs after load is done.
       * (when p_optimized_prefetch_vthumbs is called)
       */
      sgpp->vthumb_prefetch_in_progress = GAP_VTHUMB_PREFETCH_PENDING;
    }

    /* (re)load storyboard from new filename */
    if(tabw->type == GAP_STB_MASTER_TYPE_STORYBOARD)
    {
      p_storyboard_reload(sgpp);
    }
    else
    {
      p_cliplist_reload(sgpp);
    }

    if(sgpp->auto_vthumb)
    {
      /* video thumbnail creation */
      p_optimized_prefetch_vthumbs(sgpp);
    }
    else
    {
      sgpp->vthumb_prefetch_in_progress = GAP_VTHUMB_PREFETCH_NOT_ACTIVE;
    }



  }
}  /* end p_filesel_tabw_load_ok_cb */

/* --------------------------------
 * p_filesel_tabw_save_ok_cb
 * --------------------------------
 */
static void
p_filesel_tabw_save_ok_cb ( GtkWidget *widget
                          , GapStbTabWidgets *tabw)
{
  gboolean l_wr_permission;

  if(tabw)
  {
    if(p_filesel_tabw_ok(widget, tabw))
    {
      l_wr_permission = gap_arr_overwrite_file_dialog(tabw->filename_refptr);

      if(l_wr_permission)
      {
        if(tabw->type == GAP_STB_MASTER_TYPE_STORYBOARD)
        {
          /* save storyboard filename */
          p_menu_stb_save_cb(widget, tabw->sgpp);
        }
        else
        {
          /* save cliplist filename */
          p_menu_cll_save_cb(widget, tabw->sgpp);
        }
      }
    }
  }
}  /* end p_filesel_tabw_save_ok_cb */


/* -----------------------------
 * p_tabw_load_file_cb
 * -----------------------------
 */
static void
p_tabw_load_file_cb ( GtkWidget *w
                    , GapStbTabWidgets *tabw)
{
  GapStbMainGlobalParams *sgpp;
  GtkWidget *filesel = NULL;
  gboolean   drop_unsaved_changes = TRUE;

  if(tabw->filesel != NULL)
  {
     gtk_window_present(GTK_WINDOW(tabw->filesel));
     return;   /* filesel is already open */
  }
  sgpp = (GapStbMainGlobalParams *)tabw->sgpp;
  if(sgpp == NULL) { return; }

  if(sgpp->vthumb_prefetch_in_progress != GAP_VTHUMB_PREFETCH_NOT_ACTIVE)
  {
    /* cancel */
    sgpp->cancel_video_api = TRUE;
    sgpp->auto_vthumb_refresh_canceled = TRUE;
  }

  if(tabw->type == GAP_STB_MASTER_TYPE_STORYBOARD)
  {
    drop_unsaved_changes = p_check_unsaved_changes(tabw->sgpp
                           , FALSE  /* check_cliplist */
                           , TRUE   /* check_storyboard */
                           );
    if(drop_unsaved_changes)
    {
        p_tabw_destroy_all_popup_dlg(tabw);
        /* drop the old storyboard structure */
        gap_story_free_storyboard(&sgpp->stb);
        p_recreate_tab_widgets( sgpp->stb
                           ,sgpp->stb_widgets
                           ,sgpp->stb_widgets->mount_col
                           ,sgpp->stb_widgets->mount_row
                           ,sgpp
                           );
    }
    else
    {
      return;
    }
  }
  else
  {
    drop_unsaved_changes = p_check_unsaved_changes(tabw->sgpp
                           , TRUE   /* check_cliplist */
                           , FALSE  /* check_storyboard */
                           );
    if(drop_unsaved_changes)
    {
        p_tabw_destroy_all_popup_dlg(tabw);
        /* drop the old cliplist structure */
        gap_story_free_storyboard(&sgpp->cll);
        p_recreate_tab_widgets( sgpp->cll
                           ,sgpp->cll_widgets
                           ,sgpp->cll_widgets->mount_col
                           ,sgpp->cll_widgets->mount_row
                           ,sgpp
                           );
    }
    else
    {
      return;
    }
  }



  if(tabw->type == GAP_STB_MASTER_TYPE_STORYBOARD)
  {
    filesel = gtk_file_selection_new ( _("Load Storyboard"));
  }
  else
  {
    filesel = gtk_file_selection_new ( _("Load Cliplist"));
  }
  tabw->filesel = filesel;

  gtk_window_set_position (GTK_WINDOW (filesel), GTK_WIN_POS_MOUSE);
  g_signal_connect (GTK_FILE_SELECTION (filesel)->ok_button,
                    "clicked", (GtkSignalFunc) p_filesel_tabw_load_ok_cb,
                    tabw);
  g_signal_connect (GTK_FILE_SELECTION (filesel)->cancel_button,
                    "clicked", (GtkSignalFunc) p_filesel_tabw_close_cb,
                    tabw);
  g_signal_connect (filesel, "destroy",
                    (GtkSignalFunc) p_filesel_tabw_close_cb,
                    tabw);
  gtk_file_selection_set_filename (GTK_FILE_SELECTION (filesel),
                                     tabw->filename_refptr);
  gtk_widget_show (filesel);

}  /* end p_tabw_load_file_cb */



/* -----------------------------
 * p_tabw_file_save_as_cb
 * -----------------------------
 */
static void
p_tabw_file_save_as_cb ( GtkWidget *w
                       , GapStbTabWidgets *tabw)
{
  GtkWidget *filesel;

  if(tabw->filesel != NULL)
  {
     gtk_window_present(GTK_WINDOW(tabw->filesel));
     return;   /* filesel is already open */
  }

  filesel = gtk_file_selection_new ( _("Save Storyboard file"));
  tabw->filesel = filesel;

  gtk_window_set_position (GTK_WINDOW (filesel), GTK_WIN_POS_MOUSE);
  g_signal_connect (GTK_FILE_SELECTION (filesel)->ok_button,
                    "clicked", (GtkSignalFunc) p_filesel_tabw_save_ok_cb,
                    tabw);
  g_signal_connect (GTK_FILE_SELECTION (filesel)->cancel_button,
                    "clicked", (GtkSignalFunc) p_filesel_tabw_close_cb,
                    tabw);
  g_signal_connect (filesel, "destroy",
                    (GtkSignalFunc) p_filesel_tabw_close_cb,
                    tabw);
  gtk_file_selection_set_filename (GTK_FILE_SELECTION (filesel),
                                     tabw->filename_refptr);
  gtk_widget_show (filesel);

}  /* end p_tabw_file_save_as_cb */


/* ---------------------------------
 * p_tabw_filename_entry_cb
 * ---------------------------------
 */
static void
p_tabw_filename_entry_cb(GtkWidget *widget, GapStbTabWidgets *tabw)
{
  if(tabw)
  {
    if(tabw->filename_refptr)
    {
      g_snprintf(tabw->filename_refptr
            , tabw->filename_maxlen
            , "%s"
            , gtk_entry_get_text(GTK_ENTRY(widget))
            );
    }
  }
}  /* end p_tabw_filename_entry_cb */



/* ------------------------------
 * p_tabw_set_rowpage_and_refresh
 * ------------------------------
 */
static void
p_tabw_set_rowpage_and_refresh(GapStbTabWidgets *tabw, gint32 rowpage)
{
  GapStoryBoard *stb_dst;

  tabw->rowpage = rowpage;
  stb_dst = p_tabw_get_stb_ptr(tabw);

  if(stb_dst)
  {
    /* refresh storyboard layout and thumbnail list widgets */
    p_recreate_tab_widgets( stb_dst
                          ,tabw
                          ,tabw->mount_col
                          ,tabw->mount_row
                          ,tabw->sgpp
                          );
    p_render_all_frame_widgets(tabw);
  }
}  /* end p_tabw_set_rowpage_and_refresh */


/* -----------------------------
 * p_rowpage_spinbutton_cb
 * -----------------------------
 */
static void
p_rowpage_spinbutton_cb( GtkEditable     *editable
                       , GapStbTabWidgets *tabw)
{
  gint32 rowpage;

  if(tabw == NULL) { return; }

  rowpage = (gint32)GTK_ADJUSTMENT(tabw->rowpage_spinbutton_adj)->value;
  if(rowpage != tabw->rowpage)
  {
    p_tabw_set_rowpage_and_refresh(tabw, rowpage);
  }
  if(rowpage != (gint32)GTK_ADJUSTMENT(tabw->rowpage_vscale_adj)->value)
  {
    gtk_adjustment_set_value(GTK_ADJUSTMENT(tabw->rowpage_vscale_adj), (gdouble)rowpage);
  }

}  /* end p_rowpage_spinbutton_cb */


/* -----------------------------
 * p_update_vscale_page
 * -----------------------------
 */
static void
p_update_vscale_page(GapStbTabWidgets *tabw)
{
  if(tabw)
  {
    if(tabw->rowpage_vscale_adj)
    {
      gtk_adjustment_set_value(GTK_ADJUSTMENT(tabw->rowpage_vscale_adj)
        , (gdouble)tabw->rowpage);
    }
  }
}  /* end p_update_vscale_page */

/* -----------------------------
 * p_section_combo_changed_cb
 * -----------------------------
 */
static void
p_section_combo_changed_cb( GtkWidget     *widget
                       , GapStbTabWidgets *tabw)
{
  gint active_index;

  if(tabw == NULL) { return; }
  if(tabw->sections_combo == NULL) {return;}

  active_index = gtk_combo_box_get_active(GTK_COMBO_BOX (tabw->sections_combo));

  if(gap_debug)
  {
    printf("SECTION COMBO index set to: %d\n", (int)active_index);
  }

  p_activate_section_by_combo_index(active_index, tabw);

  if(tabw->spw != NULL)
  {
    GapStoryBoard *stb_dst;

    stb_dst = p_tabw_get_stb_ptr(tabw);
    if (stb_dst)
    {
      gap_story_spw_switch_to_section(tabw->spw, stb_dst);
    }
  }

}  /* end p_section_combo_changed_cb */

/* -----------------------------
 * p_vtrack_spinbutton_cb
 * -----------------------------
 */
static void
p_vtrack_spinbutton_cb( GtkEditable     *editable
                       , GapStbTabWidgets *tabw)
{
  gint32 vtrack;

  if(tabw == NULL) { return; }

  vtrack = (gint32)GTK_ADJUSTMENT(tabw->vtrack_spinbutton_adj)->value;
  if(vtrack != tabw->vtrack)
  {
    GapStoryBoard *stb_dst;

    tabw->vtrack = vtrack;
    stb_dst = p_tabw_get_stb_ptr(tabw);

    if(stb_dst)
    {
      tabw->rowpage = 1;
      gtk_adjustment_set_value(GTK_ADJUSTMENT(tabw->rowpage_spinbutton_adj), 1.0);

      gap_story_set_current_vtrack (stb_dst
                                   , stb_dst->active_section
                                   , vtrack);

      /* refresh storyboard layout and thumbnail list widgets */
      p_recreate_tab_widgets( stb_dst
                            ,tabw
                            ,tabw->mount_col
                            ,tabw->mount_row
                            ,tabw->sgpp
                            );
      p_render_all_frame_widgets(tabw);
    }
  }

}  /* end p_vtrack_spinbutton_cb */


/* ------------------------------
 * p_vscale_changed_cb
 * ------------------------------
 */
static void
p_vscale_changed_cb(GtkObject *adj, GapStbTabWidgets *tabw)
{
  gint32  value;

  if(tabw == NULL) { return;}

  if((adj) && (tabw->rowpage_spinbutton_adj))
  {
    value = (gint32)GTK_ADJUSTMENT(adj)->value;
    if(value != tabw->rowpage)
    {
      if(gap_debug)
      {
        printf("p_vscale_changed_cb new rowpage value:%d\n",
           (int)value
           );
      }

      /* set value in the rowpage spinbutton
       * (this may fire another callback for update of tabw->rowpage;
       *  but only in case where the tabw->rowpage_spinbutton widget is NOT hidden !)
       */
      gtk_adjustment_set_value(GTK_ADJUSTMENT(tabw->rowpage_spinbutton_adj), (gdouble)value);

      /* if the tabw->rowpage_spinbutton widget is hidden
       * the tabw->rowpage does NOT change to desired value.
       * in this case we must trigger the update explicitely.
       */
      if (value != tabw->rowpage)
      {
        p_tabw_set_rowpage_and_refresh(tabw, value);
      }

    }
  }

}  /* end p_vscale_changed_cb */


/* ----------------------------------------
 * p_cliptarget_togglebutton_toggled_cb
 * ----------------------------------------
 */
static void
p_cliptarget_togglebutton_toggled_cb (GtkToggleButton *togglebutton
                       , GapStoryClipTargetEnum *clip_target_ptr)
{
 GapStbMainGlobalParams *sgpp;

 if(gap_debug)
 {
   printf("CB: p_cliptarget_togglebutton_toggled_cb: %d\n", (int)togglebutton);
 }

 if(clip_target_ptr)
 {
    if (togglebutton->active)
    {
       *clip_target_ptr = GAP_STB_CLIPTARGET_STORYBOARD_APPEND;
    }
    else
    {
       *clip_target_ptr = GAP_STB_CLIPTARGET_CLIPLIST_APPEND;
    }
 }

 sgpp = (GapStbMainGlobalParams *)g_object_get_data(G_OBJECT(togglebutton), "sgpp");
 if(sgpp)
 {
    return;  /* */
 }
}  /* end p_cliptarget_togglebutton_toggled_cb */


/* ---------------------------------
 * gap_story_pw_single_clip_playback
 * ---------------------------------
 */
void
gap_story_pw_single_clip_playback(GapStbPropWidget *pw)
{
  gchar *imagename;
  GapStbTabWidgets *tabw;
  GapStbMainGlobalParams *sgpp;
  gdouble aspect_ratio;

  tabw = (GapStbTabWidgets *)pw->tabw;
  sgpp = pw->sgpp;

  if(gap_debug)
  {
    printf("CALLING Player from clip properties\n");
  }

  if((tabw) && (sgpp))
  {
    gint32 imagewidth;
    gint32 imageheight;
    gint32 l_begin_frame;
    gint32 l_end_frame;

    imagewidth = tabw->thumb_width;
    imageheight = tabw->thumb_height;

    aspect_ratio = GAP_PLAYER_DONT_FORCE_ASPECT;

    if((sgpp->force_stb_aspect) && (pw->stb_refptr))
    {
      aspect_ratio = gap_story_adjust_size_respecting_aspect(pw->stb_refptr
               , &imagewidth, &imageheight);
    }


    p_get_begin_and_end_for_single_clip_playback(&l_begin_frame, &l_end_frame, pw->stb_elem_refptr);
    imagename = gap_story_get_filename_from_elem(pw->stb_elem_refptr);
    p_story_call_player(pw->sgpp
                     ,NULL             /* No storyboard pointer */
                     ,imagename
                     ,imagewidth
                     ,imageheight
                     ,aspect_ratio
                     ,-1            /* image_id (unused in imagename based playback mode) */
                     ,l_begin_frame      /* play from */
                     ,l_end_frame        /* play until */
                     ,TRUE      /* play all */
                     ,pw->stb_elem_refptr->seltrack
                     ,pw->stb_elem_refptr->delace
                     ,gap_story_get_preferred_decoder(pw->stb_refptr, pw->stb_elem_refptr)
                     ,pw->stb_elem_refptr->flip_request
                     ,GAP_STB_FLIP_NONE
                     ,tabw->vtrack             /* stb_in_track (not relevant here) */
                     ,FALSE                    /* stb_composite (not relevant here) */
                     );
    g_free(imagename);
  }
}  /* end gap_story_pw_single_clip_playback */


/* ---------------------------------
 * gap_story_pw_composite_playback
 * ---------------------------------
 */
void
gap_story_pw_composite_playback(GapStbPropWidget *pw)
{
  GapStbTabWidgets *tabw;
  GapStbMainGlobalParams *sgpp;

  tabw = (GapStbTabWidgets *)pw->tabw;
  sgpp = pw->sgpp;

  if((tabw) && (pw->sgpp))
  {
    GapStoryBoard *stb_duptrack;
    gint32 imagewidth;
    gint32 imageheight;
    gdouble aspect_ratio;
    gint32 begin_frame;
    gint32 end_frame;


    stb_duptrack = gap_story_duplicate_vtrack(pw->stb_refptr
                                          , pw->stb_elem_refptr->track);
    gap_story_copy_sub_sections(pw->stb_refptr, stb_duptrack);

    if(gap_debug)
    {
      printf("gap_story_pw_composite_playback: START\n");
      gap_story_debug_print_list(stb_duptrack);
    }

    if(sgpp->force_stb_aspect)
    {
      aspect_ratio = gap_story_get_master_size_respecting_aspect(stb_duptrack
         , &imagewidth, &imageheight);
    }
    else
    {
      aspect_ratio = GAP_PLAYER_DONT_FORCE_ASPECT;
      gap_story_get_master_pixelsize(stb_duptrack
                                         , &imagewidth
                                         , &imageheight);
    }

    begin_frame = gap_story_get_framenr_by_story_id(pw->stb_refptr->active_section
                       , pw->stb_elem_refptr->story_id
                       , pw->stb_elem_refptr->track);
    end_frame = begin_frame + (pw->stb_elem_refptr->nframes -1);

    p_story_call_player(pw->sgpp
                         ,stb_duptrack
                         ,NULL      /* no imagename */
                         ,imagewidth
                         ,imageheight
                         ,aspect_ratio
                         ,-1        /* image_id (unused in storyboard playback mode) */
                         ,begin_frame
                         ,end_frame
                         ,TRUE      /* play_all */
                         ,1         /* seltrack */
                         ,0.0       /* delace */
                         ,gap_story_get_preferred_decoder(stb_duptrack, NULL)
                         ,GAP_STB_FLIP_NONE
                         ,GAP_STB_FLIP_NONE
                         ,-1        /* stb_in_track -1 for composite video */
                         ,TRUE            /* stb_composite */
                         );
  }
}  /* end gap_story_pw_composite_playback */


/* ---------------------------------
 * gap_story_attw_range_playback
 * ---------------------------------
 * playback on a duplicate storyboard reduced to
 * the relevant video track.
 * playback is initiated for the specified range.
 */
void
gap_story_attw_range_playback(GapStbAttrWidget *attw, gint32 begin_frame, gint32 end_frame)
{
  GapStbTabWidgets *tabw;
  GapStbMainGlobalParams *sgpp;

  tabw = (GapStbTabWidgets *)attw->tabw;
  sgpp = attw->sgpp;

  if((tabw) && (attw->sgpp))
  {
    GapStoryBoard *stb_duptrack;
    gint32 imagewidth;
    gint32 imageheight;
    gdouble aspect_ratio;

    stb_duptrack = gap_story_duplicate_vtrack(attw->stb_refptr
                                          , attw->stb_elem_refptr->track);

    if(sgpp->force_stb_aspect)
    {
      aspect_ratio = gap_story_get_master_size_respecting_aspect(stb_duptrack
                                         , &imagewidth
                                         , &imageheight);
    }
    else
    {
      aspect_ratio = GAP_PLAYER_DONT_FORCE_ASPECT;
      gap_story_get_master_pixelsize(stb_duptrack
                                         , &imagewidth
                                         , &imageheight);
    }

    p_story_call_player(attw->sgpp
                         ,stb_duptrack
                         ,NULL      /* no imagename */
                         ,imagewidth
                         ,imageheight
                         ,aspect_ratio
                         ,-1        /* image_id (unused in storyboard playback mode) */
                         ,begin_frame
                         ,end_frame
                         ,TRUE      /* play_all */
                         ,1         /* seltrack */
                         ,0.0       /* delace */
                         ,gap_story_get_preferred_decoder(stb_duptrack, NULL)
                         ,GAP_STB_FLIP_NONE
                         ,GAP_STB_FLIP_NONE
                         ,-1        /* stb_in_track -1 for composite video */
                         ,TRUE            /* stb_composite */
                         );
  }
}  /* end gap_story_attw_range_playback */


/* ---------------------------------
 * p_single_clip_playback
 * ---------------------------------
 */
static void
p_single_clip_playback(GapStbFrameWidget *fw)
{
  gchar *imagename;
  GapStbTabWidgets *tabw;
  GapStbMainGlobalParams *sgpp;

  tabw = (GapStbTabWidgets *)fw->tabw;
  sgpp = fw->sgpp;


  if((tabw) && (sgpp))
  {
    if(fw->stb_elem_refptr)
    {
      if(gap_story_elem_is_video(fw->stb_elem_refptr))
      {
        gint32 imagewidth;
        gint32 imageheight;
        gdouble aspect_ratio;

        if(gap_debug)
        {
          printf("CALLING Player from single clip\n");
        }

        imagewidth = tabw->thumb_width;
        imageheight = tabw->thumb_height;
        aspect_ratio = GAP_PLAYER_DONT_FORCE_ASPECT;

        if((sgpp->force_stb_aspect) && (fw->stb_refptr))
        {
          aspect_ratio = gap_story_adjust_size_respecting_aspect(fw->stb_refptr
                   , &imagewidth, &imageheight);
        }

        if((fw->stb_elem_refptr->record_type == GAP_STBREC_VID_SECTION)
        || (fw->stb_elem_refptr->record_type == GAP_STBREC_VID_BLACKSECTION))
        {
          gint32 stb_in_track;

          /* select the section clip
           * (because section playback is only possible in composite
           * mode (using the storyboard render processor)
           * an composite playback operates on mapping for selected clips)
           */
          p_selection_replace(fw);

          stb_in_track = tabw->vtrack;
          p_story_call_player(sgpp
                               ,fw->stb_refptr
                               ,NULL      /* no imagename */
                               ,imagewidth
                               ,imageheight
                               ,aspect_ratio
                               ,-1        /* image_id (unused in storyboard playback mode) */
                               ,-1        /* play from start */
                               ,-1        /* play until end */
                               ,FALSE     /* play_all flag */
                               ,1         /* seltrack */
                               ,0.0       /* delace */
                               ,gap_story_get_preferred_decoder(fw->stb_refptr, NULL)
                               ,GAP_STB_FLIP_NONE
                               ,GAP_STB_FLIP_NONE
                               ,stb_in_track
                               ,TRUE    /* stb_composite */
                               );
        }
        else
        {
          gint32 l_begin_frame;
          gint32 l_end_frame;

          p_get_begin_and_end_for_single_clip_playback(&l_begin_frame, &l_end_frame, fw->stb_elem_refptr);
          imagename = gap_story_get_filename_from_elem(fw->stb_elem_refptr);

          if(gap_debug)
          {
            printf("CALLING Player from single clip imagename:%s\n  from:%d (%d) to:%d (%d) type:%d\n  orig_filename:%s\n\n"
                , imagename
                ,(int)fw->stb_elem_refptr->from_frame
                ,(int)l_begin_frame
                ,(int)fw->stb_elem_refptr->to_frame
                ,(int)l_end_frame
                ,(int)fw->stb_elem_refptr->record_type
                , fw->stb_elem_refptr->orig_filename
                );
          }
          p_story_call_player(fw->sgpp
                           ,NULL             /* No storyboard pointer */
                           ,imagename
                           ,imagewidth
                           ,imageheight
                           ,aspect_ratio
                           ,-1            /* image_id (unused in imagename based playback mode) */
                           ,l_begin_frame      /* play from */
                           ,l_end_frame        /* play until */
                           ,TRUE                                 /* play all */
                           ,fw->stb_elem_refptr->seltrack
                           ,fw->stb_elem_refptr->delace
                           ,gap_story_get_preferred_decoder(fw->stb_refptr, fw->stb_elem_refptr)
                           ,fw->stb_elem_refptr->flip_request
                           ,GAP_STB_FLIP_NONE
                           ,tabw->vtrack           /* stb_in_track (not relevant here) */
                           ,FALSE                  /* stb_composite (not relevant here) */
                           );
          g_free(imagename);
        }


      }
    }
  }


}  /* end p_single_clip_playback */


/* ---------------------------------
 * p_frame_widget_preview_events_cb
 * ---------------------------------
 * handles events for all frame_widgets (in the dyn_table)
 * - Expose of pview drawing_area
 * - Doubleclick (goto)
 * - Selections (Singleclick, and key-modifiers)
 */
static gboolean
p_frame_widget_preview_events_cb (GtkWidget *widget,
                             GdkEvent  *event,
                             GapStbFrameWidget *fw)
{
  GdkEventExpose *eevent;
  GdkEventButton *bevent;

  if(fw == NULL)
  {
      return FALSE;
  }

  if ((fw->stb_elem_refptr == NULL)
  ||  (fw->stb_refptr == NULL))
  {
    /* the frame_widget is not initialized or it is just a dummy, no action needed */
    return FALSE;
  }

  switch (event->type)
  {
    case GDK_2BUTTON_PRESS:
      if(gap_debug) printf("p_frame_widget_preview_events_cb GDK_2BUTTON_PRESS (doubleclick)\n");

      if (event->button.state & GDK_CONTROL_MASK)
      {
        /* additional call external image viewer */
        p_call_external_image_viewer(fw);
      }
      p_single_clip_playback(fw);
      gtk_widget_grab_focus (fw->vbox);
      return TRUE;

    case GDK_BUTTON_PRESS:
      bevent = (GdkEventButton *) event;

      gtk_widget_grab_focus (fw->vbox);
      if(gap_debug) printf("p_frame_widget_preview_events_cb GDK_BUTTON_PRESS button:%d seq_nr:%d widget:%d  da_wgt:%d\n"
                              , (int)bevent->button
                              , (int)fw->seq_nr
                              , (int)widget
                              , (int)fw->pv_ptr->da_widget
                              );
      if(fw->stb_elem_refptr == NULL)
      {
        /* perform no actions on dummy entries in the dyn_table */
        return TRUE;
      }

      if (bevent->button == 3)
      {
          gap_story_fw_abstract_properties_dialog(fw);
          return TRUE;
      }

      /*  handle selctions, according to modifier keys */
      if (event->button.state & GDK_CONTROL_MASK)
      {
         if(gap_debug) printf("p_frame_widget_preview_events_cb SELECTION GDK_CONTROL_MASK (ctrl modifier)\n");
         p_selection_add(fw);
      }
      else if (event->button.state & GDK_SHIFT_MASK)
      {
         if(gap_debug) printf("p_frame_widget_preview_events_cb SELECTION GDK_SHIFT_MASK  (shift modifier)\n");
         p_selection_extend(fw);
      }
      else if (event->button.state & GDK_MOD1_MASK)
      {
         if(gap_debug) printf("p_frame_widget_preview_events_cb SELECTION GDK_MOD1_MASK  (alt modifier)\n");
         p_selection_add(fw);
      }
      else
      {
         if(gap_debug) printf("p_frame_widget_preview_events_cb SELECTION (no modifier)\n");
         p_selection_replace(fw);
      }

      break;

    case GDK_EXPOSE:
      if(gap_debug) printf("p_frame_widget_preview_events_cb GDK_EXPOSE seq_nr:%d widget:%d  da_wgt:%d\n"
                              , (int)fw->seq_nr
                              , (int)widget
                              , (int)fw->pv_ptr->da_widget
                              );

      eevent = (GdkEventExpose *) event;

      if(widget == fw->pv_ptr->da_widget)
      {
        //navi_preview_extents();
        //navi_frame_widget_time_label_update(fw);
        gap_pview_repaint(fw->pv_ptr);
        gdk_flush ();
      }

      break;

    default:
      break;
  }

  return FALSE;
}       /* end  p_frame_widget_preview_events_cb */



/* --------------------------------------------
 * gap_story_dlg_pw_update_mask_references
 * --------------------------------------------
 * this procedure updates mask name references
 * and rendering of layermasks in all open
 * clip property dialogs
 */
void
gap_story_dlg_pw_update_mask_references(GapStbTabWidgets *tabw)
{
  GapStbPropWidget *pw;
  GapStoryBoard    *stb;

  if(tabw==NULL) { return; }
  stb = p_tabw_get_stb_ptr(tabw);
  if(stb==NULL)  { return; }

  for(pw = tabw->pw; pw != NULL; pw = (GapStbPropWidget *)pw->next)
  {
    if ((pw->is_mask_definition) || (pw->pw_prop_dialog == NULL))
    {
      continue;
    }

    if(pw->stb_elem_refptr)
    {
      if(pw->stb_elem_refptr->mask_name)
      {
        if(gap_debug)
        {
          printf("\n\n\n ## gap_story_dlg_pw_update_mask_references\n");
          gap_story_debug_print_elem(pw->stb_elem_refptr);
        }
        gap_story_pw_trigger_refresh_properties(pw);
      }
    }
  }
}  /* end gap_story_dlg_pw_update_mask_references */


/* ---------------------------------
 * gap_story_dlg_spw_section_refresh
 * ---------------------------------
 * set specified section as new active section,
 * refresh the section combo box.
 * (this causes an implicit refresh in case
 * where the active_section has changed)
 */
void
gap_story_dlg_spw_section_refresh(GapStbSecpropWidget *spw, GapStorySection *target_section)
{
  GapStoryBoard    *stb;
  GapStbTabWidgets *tabw;
  gint target_active_index;

  if(gap_debug)
  {
    printf("gap_story_dlg_spw_refresh START\n");
  }

  if(spw == NULL) { return; }
  if(spw->tabw == NULL) { return; }

  tabw = spw->tabw;

  stb = spw->stb_refptr;

  if(stb == NULL) { return; }

  /* rebuild strings in the combo box
   * (The section properties dialog my have deleted a section
   * or added a new section)
   */
  p_set_strings_for_section_combo(spw->tabw);

  target_active_index = p_find_combo_index_of_section(stb, target_section);

  if (gap_debug)
  {
    if(target_section)
    {
      printf("\ngap_story_dlg_spw_section_refresh\n  target_section: %d (id:%d)\n"
         , (int)target_section
         , (int)target_section->section_id
         );
      if(target_section->section_name)
      {
        printf("  target_section->section_name: %s\n", target_section->section_name);
      }
    }
    printf("  target_active_index: %d\n", target_active_index);
  }


  if(tabw->sections_combo != NULL)
  {
    gtk_combo_box_set_active(GTK_COMBO_BOX (tabw->sections_combo)
      , target_active_index);
  }

}  /* end gap_story_dlg_spw_section_refresh */


/* -----------------------------------
 * p_tabw_destroy_properties_dlg
 * -----------------------------------
 * destroy the open property dialog(s) attached to tabw
 * destroy_all == FALSE: destroy only the invalid elents
 *                       (that are no members of the tabw->xxx list)
 * destroy_all == TRUE:  destroy all elements
 */
static void
p_tabw_destroy_properties_dlg (GapStbTabWidgets *tabw, gboolean destroy_all)
{
  GapStbPropWidget *pw;
  GapStbPropWidget *pw_prev;
  GapStbPropWidget *pw_next;
  GapStoryBoard    *stb;

  if(tabw==NULL) { return; }
  stb = p_tabw_get_stb_ptr(tabw);
  if(stb==NULL)  { return; }

  pw_prev = NULL;
  for(pw = tabw->pw; pw != NULL; pw = pw_next)
  {
    pw_next = (GapStbPropWidget *)pw->next;

    if(pw->stb_elem_refptr)
    {
      gboolean destroy_elem;

      destroy_elem = TRUE;
      if(!destroy_all)
      {
        GapStoryElem *stb_elem;
        GapStorySection *active_section;

        active_section = stb->active_section;

        if (active_section != NULL)
        {
          for(stb_elem = active_section->stb_elem; stb_elem != NULL;  stb_elem = stb_elem->next)
          {
            if(stb_elem == pw->stb_elem_refptr)
            {
              destroy_elem = FALSE;
              break;
            }
          }
        }
      }

      if(destroy_elem)
      {
        if(pw->stb_elem_bck)
        {
          gap_story_elem_free(&pw->stb_elem_bck);
        }

        if(pw->pw_prop_dialog)
        {
          gtk_widget_destroy(pw->pw_prop_dialog);
        }
        if(pw_prev)
        {
          pw_prev->next = pw_next;
        }
        else
        {
          tabw->pw = pw_next;
        }
        g_free(pw);
      }
      else
      {
        pw_prev = pw;
      }
    }

  }
}  /* end p_tabw_destroy_properties_dlg */


/* -----------------------------------
 * p_tabw_destroy_attw_dlg
 * -----------------------------------
 * destroy the open attribute dialog(s) attached to tabw
 * destroy_all == FALSE: destroy only the invalid elents
 *                       (that are no members of the tabw->xxx list)
 * destroy_all == TRUE:  destroy all elements
 */
static void
p_tabw_destroy_attw_dlg (GapStbTabWidgets *tabw, gboolean destroy_all)
{
  GapStbAttrWidget *attw;
  GapStbAttrWidget *attw_prev;
  GapStbAttrWidget *attw_next;
  GapStoryBoard    *stb;

  if(tabw==NULL) { return; }
  stb = p_tabw_get_stb_ptr(tabw);
  if(stb==NULL)  { return; }

  attw_prev = NULL;
  for(attw = tabw->attw; attw != NULL; attw = attw_next)
  {
    attw_next = (GapStbAttrWidget *)attw->next;

    if(attw->stb_elem_refptr)
    {
      gboolean destroy_elem;

      destroy_elem = TRUE;
      if(!destroy_all)
      {
        GapStoryElem *stb_elem;
        GapStorySection *active_section;

        active_section = stb->active_section;

        if (active_section != NULL)
        {
          for(stb_elem = active_section->stb_elem; stb_elem != NULL;  stb_elem = stb_elem->next)
          {
            if(stb_elem == attw->stb_elem_refptr)
            {
              destroy_elem = FALSE;
              break;
            }
          }
        }
      }

      if(destroy_elem)
      {
        if(attw->stb_elem_bck)
        {
          gap_story_elem_free(&attw->stb_elem_bck);
        }

        if(attw->attw_prop_dialog)
        {
          if(attw->movepath_edit_dialog != NULL)
          {
            /* force close of the movepath edit dialog that is still open */
            gtk_widget_destroy(attw->movepath_edit_dialog);
            attw->movepath_edit_dialog = NULL;
          }
          gtk_widget_destroy(attw->attw_prop_dialog);
        }
        if(attw_prev)
        {
          attw_prev->next = attw_next;
        }
        else
        {
          tabw->attw = attw_next;
        }
        g_free(attw);
      }
      else
      {
        attw_prev = attw;
      }
    }

  }
}  /* end p_tabw_destroy_attw_dlg */



/* -----------------------------------
 * p_tabw_destroy_all_popup_dlg
 * -----------------------------------
 * destroy all open pop up dialog(s) attached to tabw
 */
static void
p_tabw_destroy_all_popup_dlg (GapStbTabWidgets *tabw)
{
  GapStoryBoard    *stb;

  if(tabw==NULL) { return; }
  stb = p_tabw_get_stb_ptr(tabw);
  if(stb==NULL)  { return; }

  p_tabw_destroy_properties_dlg(tabw, TRUE /* destory all */);
  p_tabw_destroy_attw_dlg(tabw, TRUE /* destory all */);

  if (tabw->spw != NULL)
  {
    if (tabw->spw->spw_prop_dialog)
    {
      gtk_widget_destroy(tabw->spw->spw_prop_dialog);
      tabw->spw->spw_prop_dialog = NULL;
    }
  }

}  /* end p_tabw_destroy_all_popup_dlg */


/* ---------------------------------
 * p_tabw_section_properties_cut_cb
 * ---------------------------------
 */
static void
p_tabw_section_properties_cut_cb ( GtkWidget *w
                    , GapStbTabWidgets *tabw)
{
  GapStbMainGlobalParams  *sgpp;
  GapStoryBoard *stb_dst;

  if(gap_debug) printf("p_tabw_section_properties_cut_cb\n");

  sgpp = (GapStbMainGlobalParams  *)tabw->sgpp;
  if(sgpp == NULL) { return; }


  stb_dst = p_tabw_get_stb_ptr(tabw);
  if(stb_dst)
  {
    gap_story_spw_properties_dialog (stb_dst, tabw);

  }

}  /* end p_tabw_section_properties_cut_cb */

/* ---------------------------------
 * p_tabw_undo_cb
 * ---------------------------------
 */
static void
p_tabw_undo_cb ( GtkWidget *w
                    , GapStbTabWidgets *tabw)
{
  if(gap_debug) printf("p_tabw_undo_cb\n");

  p_tabw_process_undo(tabw);

}  /* end p_tabw_undo_cb */


/* ---------------------------------
 * p_tabw_redo_cb
 * ---------------------------------
 */
static void
p_tabw_redo_cb ( GtkWidget *w
                    , GapStbTabWidgets *tabw)
{
  if(gap_debug) printf("p_tabw_redo_cb\n");

  p_tabw_process_redo(tabw);

}  /* end p_tabw_redo_cb */

/* ---------------------------------
 * p_tabw_edit_cut_cb
 * ---------------------------------
 */
static void
p_tabw_edit_cut_cb ( GtkWidget *w
                    , GapStbTabWidgets *tabw)
{
  GapStbMainGlobalParams  *sgpp;
  GapStoryBoard *stb_dst;

  if(gap_debug) printf("p_tabw_edit_cut_cb\n");

  sgpp = (GapStbMainGlobalParams  *)tabw->sgpp;
  if(sgpp == NULL) { return; }


  if(sgpp->curr_selection)
  {
     gap_story_free_storyboard(&sgpp->curr_selection);
  }

  stb_dst = p_tabw_get_stb_ptr(tabw);
  if(stb_dst)
  {
    gap_stb_undo_group_begin(tabw);
    gap_stb_undo_push(tabw, GAP_STB_FEATURE_EDIT_CUT);

    sgpp->curr_selection = gap_story_duplicate_sel_only(stb_dst, tabw->vtrack);
    gap_story_remove_sel_elems(stb_dst);

    p_tabw_destroy_properties_dlg (tabw, FALSE /* DONT destroy_all*/ );
    p_tabw_destroy_attw_dlg (tabw, FALSE /* DONT destroy_all*/ );

    /* refresh storyboard layout and thumbnail list widgets */
    p_recreate_tab_widgets( stb_dst
                         ,tabw
                         ,tabw->mount_col
                         ,tabw->mount_row
                         ,sgpp
                         );
    p_render_all_frame_widgets(tabw);
    p_widget_sensibility(sgpp);
    gap_stb_undo_group_end(tabw);
  }


  gap_story_dlg_pw_update_mask_references(tabw);

}  /* end p_tabw_edit_cut_cb */

/* ---------------------------------
 * p_tabw_edit_copy_cb
 * ---------------------------------
 * replace the current selection storyboard
 * with a copy of all selected element in the
 * storyboard referred by tabw.
 */
static void
p_tabw_edit_copy_cb ( GtkWidget *w
                    , GapStbTabWidgets *tabw)
{
  GapStbMainGlobalParams  *sgpp;
  GapStoryBoard *stb_dst;

  if(gap_debug) printf("p_tabw_edit_copy_cb\n");

  sgpp = (GapStbMainGlobalParams  *)tabw->sgpp;
  if(sgpp == NULL) { return; }


  if(sgpp->curr_selection)
  {
     gap_story_free_storyboard(&sgpp->curr_selection);
  }

  stb_dst = p_tabw_get_stb_ptr(tabw);
  if(stb_dst)
  {
    sgpp->curr_selection = gap_story_duplicate_sel_only(stb_dst, tabw->vtrack);
  }

  p_widget_sensibility(sgpp);

}  /* end p_tabw_edit_copy_cb */

/* ---------------------------------
 * p_tabw_edit_paste_cb
 * ---------------------------------
 * paste after the last selected element
 * (or append at end of list if there was no selection)
 */
static void
p_tabw_edit_paste_cb ( GtkWidget *w
                    , GapStbTabWidgets *tabw)
{
  GapStbMainGlobalParams  *sgpp;
  GapStoryBoard *stb_dst;
  gint32         story_id;

  if(gap_debug) printf("p_tabw_edit_paste_cb\n");
  sgpp = (GapStbMainGlobalParams  *)tabw->sgpp;
  if(sgpp == NULL) { return; }

  stb_dst = p_tabw_get_stb_ptr(tabw);
  if(stb_dst)
  {
    gap_stb_undo_group_begin(tabw);
    gap_stb_undo_push(tabw, GAP_STB_FEATURE_EDIT_PASTE);

    story_id = gap_story_find_last_selected_in_track(stb_dst->active_section, tabw->vtrack);
    if(story_id < 0)
    {
      story_id = tabw->story_id_at_prev_paste;
    }
    else
    {
      tabw->story_id_at_prev_paste = story_id;
    }

    p_tabw_edit_paste_at_story_id(sgpp
                               ,stb_dst
                               ,tabw
                               ,story_id
                               ,TRUE      /* insert_after == TRUE */
                               );
    gap_stb_undo_group_end(tabw);
  }

}  /* end p_tabw_edit_paste_cb */


/* ---------------------------------
 * p_tabw_edit_paste_at_story_id
 * ---------------------------------
 */
static void
p_tabw_edit_paste_at_story_id (GapStbMainGlobalParams  *sgpp
                    , GapStoryBoard *stb_dst
                    , GapStbTabWidgets *tabw
                    , gint32 story_id
                    , gboolean insert_after)
{
  GapStoryBoard *stb_sel_dup;

  if(gap_debug) printf("p_tabw_edit_paste_cb\n");

  if(sgpp == NULL) { return; }
  if(sgpp->curr_selection == NULL) { return; }

  stb_sel_dup = gap_story_duplicate_full(sgpp->curr_selection);
  if(stb_sel_dup)
  {
    /* clear all selections of the destination list before pasting into it */
    gap_story_selection_all_set(stb_dst, FALSE);

    gap_story_lists_merge(stb_dst
                      , stb_sel_dup
                      , story_id
                      , insert_after
                      , tabw->vtrack
                      );
    gap_story_free_storyboard(&stb_sel_dup);
  }

  // printf("p_tabw_edit_paste_at_story_id: TODO: check if pasted elem is off screen and scroll if TRUE\n");


  /* refresh storyboard layout and thumbnail list widgets */
  p_recreate_tab_widgets( stb_dst
                         ,tabw
                         ,tabw->mount_col
                         ,tabw->mount_row
                         ,sgpp
                         );
  p_render_all_frame_widgets(tabw);

}  /* end p_tabw_edit_paste_at_story_id */

/* ---------------------------------
 * p_tabw_update_selection
 * ---------------------------------
 */
static void
p_tabw_update_selection(GapStbTabWidgets  *tabw)
{
  GapStbFrameWidget *fw;
  GapStbMainGlobalParams  *sgpp;
  GdkColor      *bg_color;
  GdkColor      *fg_color;
  gint ii;

  if(tabw == NULL) { return; }

  sgpp = (GapStbMainGlobalParams  *)tabw->sgpp;
  if(sgpp == NULL) { return; }

  for(ii=0; ii < tabw->fw_tab_size; ii++)
  {
    fw = tabw->fw_tab[ii];

    bg_color = &sgpp->shell_window->style->bg[GTK_STATE_NORMAL];
    fg_color = &sgpp->shell_window->style->fg[GTK_STATE_NORMAL];
    if(fw->stb_elem_refptr)
    {
      if(fw->stb_elem_refptr->selected)
      {
         bg_color = &sgpp->shell_window->style->bg[GTK_STATE_SELECTED];
         fg_color = &sgpp->shell_window->style->fg[GTK_STATE_SELECTED];
      }
    }

    if(gap_debug) printf("p_tabw_update_selection: GTK_STYLE_SET_BACKGROUND bg_color: %d\n", (int)bg_color);

    /* Note: Gtk does not know about selcted items, since selections are handled
     * external by gap_navigator_dialog code.
     * using SELECTED or NORMAL color from the shell window to paint this private selection
     * (must use always the NORMAL state here, because for Gtk the event_box is never selected)
     */
    gtk_widget_modify_bg(fw->event_box, GTK_STATE_NORMAL, bg_color);
    gtk_widget_modify_fg(fw->key_label, GTK_STATE_NORMAL, fg_color);
    gtk_widget_modify_fg(fw->val_label, GTK_STATE_NORMAL, fg_color);
  }

}  /* end p_tabw_update_selection */

/* ---------------------------------
 * p_selection_add
 * ---------------------------------
 */
static void
p_selection_add(GapStbFrameWidget *fw)
{
  GapStbTabWidgets  *tabw;
  GapStbMainGlobalParams *sgpp;

  tabw = (GapStbTabWidgets  *)fw->tabw;
  if(tabw == NULL)   { return; }

  sgpp = (GapStbMainGlobalParams *)tabw->sgpp;
  if(sgpp == NULL)   { return; }

  /* clear all selected flags in the other list */
  if(tabw->type == GAP_STB_MASTER_TYPE_STORYBOARD)
  {
    gap_story_selection_all_set(sgpp->cll, FALSE);
  }
  else
  {
    gap_story_selection_all_set(sgpp->stb, FALSE);
  }

  if(fw->stb_elem_refptr)
  {
      if(fw->stb_elem_refptr->selected)
      {
         fw->stb_elem_refptr->selected = FALSE;
      }
      else
      {
         fw->stb_elem_refptr->selected = TRUE;
      }
  }

  /* update both list */
  p_tabw_update_selection(sgpp->cll_widgets);
  p_tabw_update_selection(sgpp->stb_widgets);

  p_widget_sensibility(sgpp);
}  /* end p_selection_add */

/* ---------------------------------
 * p_selection_extend
 * ---------------------------------
 * select elements in the current track that are
 * between the current picked element, the first and the last
 * already selected element in the same track in the active section.
 * (if no selected elements exist, then select only
 * the current picked element)
 */
static void
p_selection_extend(GapStbFrameWidget *fw)
{
  GapStbTabWidgets  *tabw;
  GapStbMainGlobalParams *sgpp;
  GapStoryBoard *stb;
  GapStoryElem *stb_elem;
  gint32 story_id;
  gint32 last_sel_story_id;
  gboolean sel_state;

  tabw = (GapStbTabWidgets  *)fw->tabw;
  if(tabw == NULL)   { return; }

  sgpp = (GapStbMainGlobalParams *)tabw->sgpp;
  if(sgpp == NULL)   { return; }

  /* clear all selected flags in the other list */
  if(tabw->type == GAP_STB_MASTER_TYPE_STORYBOARD)
  {
    gap_story_selection_all_set(sgpp->cll, FALSE);
  }
  else
  {
    gap_story_selection_all_set(sgpp->stb, FALSE);
  }

  stb = p_tabw_get_stb_ptr(tabw);
  if(stb == NULL)   { return; }

  if(fw->stb_elem_refptr == NULL)   { return; }

  last_sel_story_id = gap_story_find_last_selected_in_track(stb->active_section, tabw->vtrack);

  fw->stb_elem_refptr->selected = TRUE;      /* always select the clicked element */
  story_id = fw->stb_elem_refptr->story_id;

  sel_state = FALSE;

  if((last_sel_story_id >= 0)
  && (story_id != last_sel_story_id)
  && (stb->active_section != NULL))
  {
    for(stb_elem = stb->active_section->stb_elem; stb_elem != NULL;  stb_elem = stb_elem->next)
    {
      if((stb_elem->track != tabw->vtrack))
      {
        /* ignore elements of other tracks */
        continue;
      }
      if((stb_elem->story_id == last_sel_story_id))
      {
        if(story_id == -1)
        {
            break;
        }
        sel_state = TRUE;
        last_sel_story_id = -1;
      }
      if((stb_elem->story_id == story_id))
      {
        if(last_sel_story_id == -1)
        {
          break;
        }
        sel_state = TRUE;
        story_id = -1;
      }
      if(stb_elem->selected)
      {
        /* set selected beginning at the 1.st already seleected elem
         * and all further elements until element with story_id
         */
        sel_state = TRUE;
      }
      stb_elem->selected = sel_state;
    }
  }

  /* update both list */
  p_tabw_update_selection(sgpp->cll_widgets);
  p_tabw_update_selection(sgpp->stb_widgets);
  p_widget_sensibility(sgpp);
}  /* end p_selection_extend */

/* ---------------------------------
 * p_selection_replace
 * ---------------------------------
 */
static void
p_selection_replace(GapStbFrameWidget *fw)
{
  GapStbTabWidgets  *tabw;
  GapStbMainGlobalParams *sgpp;
  gboolean   old_selection_state;

  old_selection_state = FALSE;
  if(fw->stb_elem_refptr)
  {
    old_selection_state = fw->stb_elem_refptr->selected;
  }

  tabw = (GapStbTabWidgets  *)fw->tabw;
  if(tabw == NULL)   { return; }

  sgpp = (GapStbMainGlobalParams *)tabw->sgpp;
  if(sgpp == NULL)   { return; }

  /* clear all selected flags in both lists */
  gap_story_selection_all_set(sgpp->stb, FALSE);
  gap_story_selection_all_set(sgpp->cll, FALSE);

  if(fw->stb_elem_refptr)
  {
    fw->stb_elem_refptr->selected = old_selection_state;
  }

  p_selection_add(fw);

}  /* end p_selection_replace */



/* -----------------------------
 * p_cut_copy_sensitive
 * -----------------------------
 */
static gboolean
p_cut_copy_sensitive (GapStoryBoard *stb)
{
  gboolean l_sensitive_cc;

  l_sensitive_cc = FALSE;

  if(stb)
  {
    GapStoryElem      *stb_elem;
    GapStorySection   *section;

    /* check if there is at least one selected element */
    for(section = stb->stb_section; section != NULL; section = section->next)
    {
      for(stb_elem = section->stb_elem; stb_elem != NULL;  stb_elem = stb_elem->next)
      {
        if(stb_elem->selected)
        {
          l_sensitive_cc = TRUE;
          break;
        }
      }
    }
  }

  return (l_sensitive_cc);

}  /* end p_cut_copy_sensitive */


/* -----------------------------
 * p_paste_sensitive
 * -----------------------------
 */
static gboolean
p_paste_sensitive (GapStbMainGlobalParams *sgpp
                   ,GapStoryBoard *stb
                   )
{
  if(stb)
  {
    /* check if there is at least one elem in the paste buffer */
    if(sgpp->curr_selection)
    {
      GapStorySection *curr_selection_main_section;

      curr_selection_main_section = gap_story_find_main_section(sgpp->curr_selection);

      if(curr_selection_main_section)
      {
        if(curr_selection_main_section->stb_elem)
        {
          return(TRUE);
        }
      }
    }
  }
  return(FALSE);
}  /* end p_paste_sensitive */


/* -----------------------------
 * p_tabw_sensibility
 * -----------------------------
 */
static void
p_tabw_sensibility (GapStbMainGlobalParams *sgpp
                   ,GapStoryBoard *stb
                   ,GapStbTabWidgets *tabw)
{
  gboolean l_sensitive;
  gboolean l_sensitive_cc;
  gboolean l_sensitive_new;
  gdouble  l_lower_limit;
  gdouble  l_upper_limit;

  if(sgpp == NULL) { return; }
  if(tabw == NULL) { return; }
  if(sgpp->initialized == FALSE) { return; }

  l_sensitive_new = FALSE;
  l_sensitive = FALSE;
  if(stb)
  {
    l_sensitive_new = TRUE;
    if(stb->active_section)
    {
      if(stb->active_section->stb_elem)
      {
        l_sensitive  = TRUE;
      }
    }
  }

  if(tabw->save_button)  gtk_widget_set_sensitive(tabw->save_button, l_sensitive);
  if(tabw->play_button)  gtk_widget_set_sensitive(tabw->play_button, l_sensitive);

  l_sensitive  = p_paste_sensitive(sgpp, stb);
  l_sensitive_cc = p_cut_copy_sensitive(stb);

  if(tabw->edit_paste_button) gtk_widget_set_sensitive(tabw->edit_paste_button, l_sensitive);

  if(tabw->edit_cut_button) gtk_widget_set_sensitive(tabw->edit_cut_button, l_sensitive_cc);
  if(tabw->edit_copy_button) gtk_widget_set_sensitive(tabw->edit_copy_button, l_sensitive_cc);
  if(tabw->new_clip_button) gtk_widget_set_sensitive(tabw->new_clip_button, l_sensitive_new);


  /* handle video track sensitivity (not sensitive if stb is not present e.g. is NULL)  */
  l_sensitive = FALSE;
  l_lower_limit = 0;
  l_upper_limit = GAP_STB_MAX_VID_TRACKS -1;
  if(stb)
  {
    if (stb->active_section != stb->mask_section)
    {
      l_sensitive = TRUE;
      l_lower_limit = 1;
    }
  }

  if ((tabw->vtrack_spinbutton) && (tabw->vtrack_spinbutton_adj))
  {
    gtk_widget_set_sensitive(tabw->vtrack_spinbutton, l_sensitive);
    GTK_ADJUSTMENT(tabw->vtrack_spinbutton_adj)->lower = l_lower_limit;
    GTK_ADJUSTMENT(tabw->vtrack_spinbutton_adj)->upper = l_upper_limit;
  }

}  /* end p_tabw_sensibility */

/* ---------------------------------------
 * p_tabw_set_undo_sensitivity
 * ---------------------------------------
 */
static void
p_tabw_set_undo_sensitivity(GapStbTabWidgets *tabw, GtkWidget *menu_item)
{
  gboolean l_sensitive;
  const char *undo_feature;
  char *tooltip_help;

  l_sensitive = FALSE;
  tooltip_help = NULL;
  undo_feature = gap_stb_undo_get_undo_feature(tabw);
  if (undo_feature != NULL)
  {
    tooltip_help = g_strdup_printf(_("UNDO %s"), undo_feature);
    l_sensitive = TRUE;
  }
  else
  {
    tooltip_help = g_strdup(_("UNDO"));
  }

  if (menu_item)
  {
    gtk_widget_set_sensitive(menu_item, l_sensitive);
  }
  if(tabw->undo_button)
  {
    gtk_widget_set_sensitive(tabw->undo_button, l_sensitive);
    gimp_help_set_help_data (tabw->undo_button, tooltip_help,  NULL);
  }

  g_free(tooltip_help);

}  /* end p_tabw_set_undo_sensitivity */


/* ---------------------------------------
 * p_tabw_set_redo_sensitivity
 * ---------------------------------------
 */
static void
p_tabw_set_redo_sensitivity(GapStbTabWidgets *tabw, GtkWidget *menu_item)
{
  gboolean l_sensitive;
  const char *redo_feature;
  char *tooltip_help;

  l_sensitive = FALSE;
  tooltip_help = NULL;
  redo_feature = gap_stb_undo_get_redo_feature(tabw);
  if (redo_feature != NULL)
  {
    tooltip_help = g_strdup_printf(_("REDO %s"), redo_feature);
    l_sensitive = TRUE;
  }
  else
  {
    tooltip_help = g_strdup(_("REDO"));
  }

  if (menu_item)
  {
    gtk_widget_set_sensitive(menu_item, l_sensitive);
  }

  if(tabw->redo_button)
  {
    gtk_widget_set_sensitive(tabw->redo_button, l_sensitive);
    gimp_help_set_help_data (tabw->redo_button, tooltip_help,  NULL);
  }

  g_free(tooltip_help);
}  /* end p_tabw_set_redo_sensitivity */



/* ---------------------------------------
 * p_undo_redo_sensibility
 * ---------------------------------------
 * handle sensitivity of undo and redo widgets
 * for both cliplist and storyboard.
 */
static void
p_undo_redo_sensibility (GapStbMainGlobalParams *sgpp)
{
  p_tabw_set_undo_sensitivity(sgpp->cll_widgets, sgpp->menu_item_cll_undo);
  p_tabw_set_redo_sensitivity(sgpp->cll_widgets, sgpp->menu_item_cll_redo);

  p_tabw_set_undo_sensitivity(sgpp->stb_widgets, sgpp->menu_item_stb_undo);
  p_tabw_set_redo_sensitivity(sgpp->stb_widgets, sgpp->menu_item_stb_redo);

}  /* end p_undo_redo_sensibility */


/* ----------------------------------------
 * gap_story_dlg_tabw_undo_redo_sensitivity
 * ----------------------------------------
 * handle sensitivity of undo and redo widgets
 * for both cliplist and storyboard.
 */
void
gap_story_dlg_tabw_undo_redo_sensitivity(GapStbTabWidgets *tabw)
{
  if(tabw != NULL)
  {
    if(tabw->sgpp != NULL)
    {
      p_undo_redo_sensibility(tabw->sgpp);
    }
  }
}  /* end gap_story_dlg_tabw_undo_redo_sensitivity */


/* -----------------------------
 * p_widget_sensibility
 * -----------------------------
 */
static void
p_widget_sensibility (GapStbMainGlobalParams *sgpp)
{
  gboolean l_sensitive;
  gboolean l_sensitive_add;
  gboolean l_sensitive_add_section;
  gboolean l_sensitive_encode;
  gboolean l_sensitive_att;

  if(gap_debug) printf("p_widget_sensibility\n");

  if(sgpp == NULL) { return; }
  if(sgpp->initialized == FALSE) { return; }

  l_sensitive  = FALSE;
  l_sensitive_add = FALSE;
  l_sensitive_encode = FALSE;
  if(sgpp->cll)
  {
    l_sensitive_add = TRUE;
    if(sgpp->cll->active_section)
    {
      if(sgpp->cll->active_section->stb_elem)
      {
        l_sensitive  = TRUE;
        if(!sgpp->cll->unsaved_changes)
        {
          /* encoder call only available when saved */
          l_sensitive_encode = TRUE;
        }
      }
    }

  }
  l_sensitive_att = l_sensitive_add;
  if(sgpp->cll_widgets->vtrack == GAP_STB_MASK_TRACK_NUMBER)
  {
    l_sensitive_att = FALSE;
  }

  if(sgpp->menu_item_cll_save)         gtk_widget_set_sensitive(sgpp->menu_item_cll_save, l_sensitive);
  if(sgpp->menu_item_cll_save_as)      gtk_widget_set_sensitive(sgpp->menu_item_cll_save_as, l_sensitive);
  if(sgpp->menu_item_cll_playback)     gtk_widget_set_sensitive(sgpp->menu_item_cll_playback, l_sensitive);
  if(sgpp->menu_item_cll_properties)   gtk_widget_set_sensitive(sgpp->menu_item_cll_properties, l_sensitive);
  if(sgpp->menu_item_cll_audio_otone)  gtk_widget_set_sensitive(sgpp->menu_item_cll_audio_otone, l_sensitive);
  if(sgpp->menu_item_cll_close)        gtk_widget_set_sensitive(sgpp->menu_item_cll_close, l_sensitive);

  if(sgpp->menu_item_cll_encode)       gtk_widget_set_sensitive(sgpp->menu_item_cll_encode, l_sensitive_encode);

  if(sgpp->menu_item_cll_add_clip)       gtk_widget_set_sensitive(sgpp->menu_item_cll_add_clip, l_sensitive_add);
  if(sgpp->menu_item_cll_att_properties) gtk_widget_set_sensitive(sgpp->menu_item_cll_att_properties, l_sensitive_att);


  if(sgpp->menu_item_cll_add_section_clip)
  {
    l_sensitive_add_section = l_sensitive_add;

    if(NULL == (gap_story_find_first_referable_subsection(sgpp->cll)))
    {
      l_sensitive_add_section = FALSE;
    }
    gtk_widget_set_sensitive(sgpp->menu_item_cll_add_section_clip, l_sensitive_add_section);
  }

  if(sgpp->cll_widgets)
  {
    p_tabw_sensibility(sgpp, sgpp->cll, sgpp->cll_widgets);
  }


  l_sensitive  = FALSE;
  l_sensitive_add = FALSE;
  l_sensitive_encode = FALSE;
  if(sgpp->stb)
  {
    l_sensitive_add = TRUE;
    if(sgpp->stb->active_section)
    {
      if(sgpp->stb->active_section->stb_elem)
      {
        l_sensitive  = TRUE;
        if(!sgpp->stb->unsaved_changes)
        {
          /* encoder call only available when saved */
          l_sensitive_encode = TRUE;
        }
      }
    }
  }
  l_sensitive_att = l_sensitive_add;
  if(sgpp->stb_widgets->vtrack == GAP_STB_MASK_TRACK_NUMBER)
  {
    l_sensitive_att = FALSE;
  }
  if(sgpp->menu_item_stb_save)         gtk_widget_set_sensitive(sgpp->menu_item_stb_save, l_sensitive);
  if(sgpp->menu_item_stb_save_as)      gtk_widget_set_sensitive(sgpp->menu_item_stb_save_as, l_sensitive);
  if(sgpp->menu_item_stb_playback)     gtk_widget_set_sensitive(sgpp->menu_item_stb_playback, l_sensitive);
  if(sgpp->menu_item_stb_properties)   gtk_widget_set_sensitive(sgpp->menu_item_stb_properties, l_sensitive);
  if(sgpp->menu_item_stb_audio_otone)  gtk_widget_set_sensitive(sgpp->menu_item_stb_audio_otone, l_sensitive);
  if(sgpp->menu_item_stb_close)        gtk_widget_set_sensitive(sgpp->menu_item_stb_close, l_sensitive);
  if(sgpp->menu_item_stb_encode)       gtk_widget_set_sensitive(sgpp->menu_item_stb_encode, l_sensitive_encode);

  if(sgpp->menu_item_stb_add_clip)       gtk_widget_set_sensitive(sgpp->menu_item_stb_add_clip, l_sensitive_add);
  if(sgpp->menu_item_stb_att_properties) gtk_widget_set_sensitive(sgpp->menu_item_stb_att_properties, l_sensitive_att);


  if(sgpp->menu_item_stb_add_section_clip)
  {
    l_sensitive_add_section = l_sensitive_add;

    if(NULL == (gap_story_find_first_referable_subsection(sgpp->stb)))
    {
      l_sensitive_add_section = FALSE;
    }

    gtk_widget_set_sensitive(sgpp->menu_item_stb_add_section_clip, l_sensitive_add_section);
  }

  if(sgpp->stb_widgets)
  {
    p_tabw_sensibility(sgpp, sgpp->stb, sgpp->stb_widgets);
  }

  p_undo_redo_sensibility(sgpp);

  gap_base_check_tooltips(NULL);

}  /* end p_widget_sensibility */


/* -----------------------------
 * p_close_one_or_both_lists
 * -----------------------------
 * check for unsaved changes and ask for close permission if there are such changes.
 *
 * return: FALSE if there were unsaved changes
 *               AND the user has REFUSED permission to close
 *               (by pressing the CANCEL Button)
 *
 * return: TRUE  if the close was performed.
 */
static gboolean
p_close_one_or_both_lists(GapStbMainGlobalParams *sgpp
                         ,gboolean check_cliplist
                         ,gboolean check_storyboard
                         )
{
  gboolean  l_rc;

  l_rc = TRUE;
  if(sgpp)
  {
    l_rc = p_check_unsaved_changes(sgpp
                              , check_cliplist
                              , check_storyboard
                              );
    if(l_rc)
    {
      if(check_cliplist)
      {
        p_tabw_destroy_all_popup_dlg (sgpp->cll_widgets);
        /* drop the old cliplist structure */
        gap_story_free_storyboard(&sgpp->cll);
        p_recreate_tab_widgets( sgpp->cll
                           ,sgpp->cll_widgets
                           ,sgpp->cll_widgets->mount_col
                           ,sgpp->cll_widgets->mount_row
                           ,sgpp
                           );
      }

      if(check_storyboard)
      {
        p_tabw_destroy_all_popup_dlg (sgpp->stb_widgets);
        /* drop the old storyboard structure */
        gap_story_free_storyboard(&sgpp->stb);
        p_recreate_tab_widgets( sgpp->stb
                           ,sgpp->stb_widgets
                           ,sgpp->stb_widgets->mount_col
                           ,sgpp->stb_widgets->mount_row
                           ,sgpp
                           );
      }

    }
  }
  return(l_rc);
}  /* end p_close_one_or_both_lists */


/* -----------------------------------
 * gap_story_dlg_update_edit_settings
 * -----------------------------------
 */
void
gap_story_dlg_update_edit_settings(GapStoryBoard *stb, GapStbTabWidgets *tabw)
{
  if (stb->edit_settings)
  {
    if (stb->edit_settings->section_name != NULL)
    {
      g_free(stb->edit_settings->section_name);
      stb->edit_settings->section_name = NULL;
    }
    if (stb->active_section != NULL)
    {
      if (stb->active_section->section_name != NULL)
      {
        stb->edit_settings->section_name =
          g_strdup(stb->active_section->section_name);
      }
    }

    if(tabw != NULL)
    {
      stb->edit_settings->track = tabw->vtrack;
      stb->edit_settings->page = tabw->rowpage;

      stb->layout_cols = tabw->cols;
      stb->layout_rows = tabw->rows;
      stb->layout_thumbsize = tabw->thumbsize;
    }

  }
}  /* end gap_story_dlg_update_edit_settings */


/* -------------------------------
 * p_story_save_with_edit_settings
 * -------------------------------
 * saves the specified storyboard with
 * edit settings from the table widgets (tabw)
 * this includes the active section
 * the currently edited video track number and scroll position.
 */
static gboolean
p_story_save_with_edit_settings(GapStbTabWidgets *tabw,
  GapStoryBoard *stb, const char *filename)
{
  gap_stb_undo_stack_set_unsaved_changes(tabw);
  gap_story_dlg_update_edit_settings(stb, tabw);
  return (gap_story_save(stb, filename));
}  /* end p_story_save_with_edit_settings */



/* -----------------------------
 * p_menu_win_quit_cb
 * -----------------------------
 */
static void
p_menu_win_quit_cb (GtkWidget *widget, GapStbMainGlobalParams *sgpp)
{
  if(gap_debug) printf("p_menu_win_quit_cb\n");

  if(p_close_one_or_both_lists(sgpp
                              , TRUE  /* check_cliplist */
                              , TRUE  /* check_storyboard */
                              ))
  {
    story_dlg_response(widget, GTK_RESPONSE_CANCEL, sgpp);
  }

}  /* end p_menu_win_quit_cb */

/* -----------------------------
 * p_menu_win_help_cb
 * -----------------------------
 */
static void
p_menu_win_help_cb (GtkWidget *widget, GapStbMainGlobalParams *sgpp)
{
  if(gap_debug) printf("p_menu_win_help_cb\n");

  if(sgpp)
  {
    gimp_standard_help_func(GAP_STORYBOARD_EDIT_HELP_ID, sgpp->shell_window);
  }

}  /* end p_menu_win_help_cb */

/* -----------------------------
 * p_menu_win_properties_cb
 * -----------------------------
 */
static void
p_menu_win_properties_cb (GtkWidget *widget, GapStbMainGlobalParams *sgpp)
{
  static GapArrArg  argv[10];
  static char *radio_args[3];
  gint l_ii;
  gint l_cll_cols_idx;
  gint l_cll_rows_idx;
  gint l_cll_thumbsize_idx;
  gint l_stb_cols_idx;
  gint l_stb_rows_idx;
  gint l_stb_thumbsize_idx;
  gint l_stb_aspect_idx;
  gboolean l_rc;

  gboolean old_force_stb_aspect;




  if(gap_debug) printf("p_menu_win_properties_cb\n");

  if(sgpp->win_prop_dlg_open)
  {
    g_message(_("Global Layout Properties dialog already open"));
    return;
  }

  sgpp->win_prop_dlg_open = TRUE;
  old_force_stb_aspect = sgpp->force_stb_aspect;

  radio_args[STB_THIDX_LARGE]  = _("large");
  radio_args[STB_THIDX_MEDIUM] = _("medium");
  radio_args[STB_THIDX_SMALL]  = _("small");

  /* Cliplist layout settings */
  l_ii = 0;
  gap_arr_arg_init(&argv[l_ii], GAP_ARR_WGT_LABEL_LEFT);
  argv[l_ii].label_txt = _("Cliplist Layout:");

  l_ii++;
  l_cll_thumbsize_idx = l_ii;
  gap_arr_arg_init(&argv[l_ii], GAP_ARR_WGT_RADIO);
  argv[l_ii].label_txt = _("Thumbnail Size:");
  argv[l_ii].help_txt  = _("Thumbnail size in the cliplist");
  argv[l_ii].radio_argc  =  G_N_ELEMENTS(radio_args);
  argv[l_ii].radio_argv = radio_args;
  argv[l_ii].radio_ret  = p_thumbsize_to_index(sgpp->cll_thumbsize);
  argv[l_ii].has_default = TRUE;
  argv[l_ii].radio_default = STB_THIDX_MEDIUM;

  l_ii++; l_cll_cols_idx = l_ii;
  gap_arr_arg_init(&argv[l_ii], GAP_ARR_WGT_INT);
  argv[l_ii].constraint = TRUE;
  argv[l_ii].label_txt = _("Columns:");
  argv[l_ii].help_txt  = _("columns in the cliplist");
  argv[l_ii].int_min   = (gint)CLL_MIN_COL;
  argv[l_ii].int_max   = (gint)CLL_MAX_COL;
  argv[l_ii].int_ret   = (gint)sgpp->cll_cols;
  argv[l_ii].has_default = TRUE;
  argv[l_ii].int_default = (gint)6;

  l_ii++; l_cll_rows_idx = l_ii;
  gap_arr_arg_init(&argv[l_ii], GAP_ARR_WGT_INT);
  argv[l_ii].constraint = TRUE;
  argv[l_ii].label_txt = _("Rows:");
  argv[l_ii].help_txt  = _("rows in the cliplist");
  argv[l_ii].int_min   = (gint)CLL_MIN_ROW;
  argv[l_ii].int_max   = (gint)CLL_MAX_ROW;
  argv[l_ii].int_ret   = (gint)sgpp->cll_rows;
  argv[l_ii].has_default = TRUE;
  argv[l_ii].int_default = (gint)6;



  /* Storyboard layout settings */
  l_ii++;
  gap_arr_arg_init(&argv[l_ii], GAP_ARR_WGT_LABEL_LEFT);
  argv[l_ii].label_txt = _("Storyboard Layout:");

  l_ii++;
  l_stb_thumbsize_idx = l_ii;
  gap_arr_arg_init(&argv[l_ii], GAP_ARR_WGT_RADIO);
  argv[l_ii].label_txt = _("Thumbnail Size:");
  argv[l_ii].help_txt  = _("Thumbnail size in the storyboard list");
  argv[l_ii].radio_argc  =  G_N_ELEMENTS(radio_args);
  argv[l_ii].radio_argv = radio_args;
  argv[l_ii].radio_ret  = p_thumbsize_to_index(sgpp->stb_thumbsize);
  argv[l_ii].has_default = TRUE;
  argv[l_ii].radio_default = STB_THIDX_MEDIUM;

  l_ii++; l_stb_cols_idx = l_ii;
  gap_arr_arg_init(&argv[l_ii], GAP_ARR_WGT_INT);
  argv[l_ii].constraint = TRUE;
  argv[l_ii].label_txt = _("Columns:");
  argv[l_ii].help_txt  = _("columns in the storyboard list");
  argv[l_ii].int_min   = (gint)STB_MIN_COL;
  argv[l_ii].int_max   = (gint)STB_MAX_COL;
  argv[l_ii].int_ret   = (gint)sgpp->stb_cols;
  argv[l_ii].has_default = TRUE;
  argv[l_ii].int_default = (gint)15;

  l_ii++; l_stb_rows_idx = l_ii;
  gap_arr_arg_init(&argv[l_ii], GAP_ARR_WGT_INT);
  argv[l_ii].constraint = TRUE;
  argv[l_ii].label_txt = _("Rows:");
  argv[l_ii].help_txt  = _("rows in the storyboard list");
  argv[l_ii].int_min   = (gint)STB_MIN_ROW;
  argv[l_ii].int_max   = (gint)STB_MAX_ROW;
  argv[l_ii].int_ret   = (gint)sgpp->stb_rows;
  argv[l_ii].has_default = TRUE;
  argv[l_ii].int_default = (gint)2;


  l_ii++; l_stb_aspect_idx = l_ii;
  gap_arr_arg_init(&argv[l_ii], GAP_ARR_WGT_TOGGLE);
  argv[l_ii].constraint = TRUE;
  argv[l_ii].label_txt = _("Force Aspect:");
  argv[l_ii].help_txt  = _("ON: player shows clips transformed to aspect setting from the Storyboard properties."
                           "OFF: player shows clips according to their original pixel sizes");
  argv[l_ii].int_ret   = (gint)sgpp->force_stb_aspect;
  argv[l_ii].has_default = TRUE;
  argv[l_ii].int_default = (gint)1;

  /* the reset to default button */
  l_ii++;
  gap_arr_arg_init(&argv[l_ii], GAP_ARR_WGT_DEFAULT_BUTTON);
  argv[l_ii].label_txt = _("Default");
  argv[l_ii].help_txt  = _("Use the standard built in layout settings");

  l_rc = gap_arr_ok_cancel_dialog( _("Global Layout Properties")
                                 , _("Settings")
                                 ,G_N_ELEMENTS(argv), argv
                                 );
  sgpp->win_prop_dlg_open = FALSE;

  if(l_rc == TRUE)
  {
    gint32 cll_thumbsize;
    gint32 stb_thumbsize;

    cll_thumbsize = p_index_to_thumbsize(argv[l_cll_thumbsize_idx].radio_ret);
    stb_thumbsize = p_index_to_thumbsize(argv[l_stb_thumbsize_idx].radio_ret);

    sgpp->force_stb_aspect = (argv[l_stb_aspect_idx].int_ret != 0);

    if((sgpp->cll_thumbsize  != cll_thumbsize)
    || (sgpp->stb_thumbsize  != stb_thumbsize)
    || (sgpp->cll_cols != argv[l_cll_cols_idx].int_ret)
    || (sgpp->stb_cols != argv[l_stb_cols_idx].int_ret)
    || (sgpp->cll_rows != argv[l_cll_rows_idx].int_ret)
    || (sgpp->stb_rows != argv[l_stb_rows_idx].int_ret)
    )
    {
      GapStbTabWidgets *tabw;
      GapStoryBoard    *stb_dst;
      gint min_width;
      gint min_height;

      /* apply the new layout settings */
      sgpp->cll_thumbsize  = cll_thumbsize;
      sgpp->stb_thumbsize  = stb_thumbsize;
      sgpp->cll_cols = argv[l_cll_cols_idx].int_ret;
      sgpp->stb_cols = argv[l_stb_cols_idx].int_ret;
      sgpp->cll_rows = argv[l_cll_rows_idx].int_ret;
      sgpp->stb_rows = argv[l_stb_rows_idx].int_ret;

      p_set_layout_preset(sgpp);
      p_save_gimprc_layout_settings(sgpp);

      /* refresh */
      if(sgpp->stb_widgets)
      {
        stb_dst = (GapStoryBoard *)sgpp->stb;
        tabw = sgpp->stb_widgets;

        if((stb_dst) && (tabw))
        {
          /* refresh storyboard layout and thumbnail list widgets */
          p_recreate_tab_widgets( stb_dst
                                 ,tabw
                                 ,tabw->mount_col
                                 ,tabw->mount_row
                                 ,sgpp
                                 );
          p_render_all_frame_widgets(sgpp->stb_widgets);
        }
      }
      if(sgpp->cll_widgets)
      {
        stb_dst = (GapStoryBoard *)sgpp->cll;
        tabw = sgpp->cll_widgets;

        if((stb_dst) && (tabw))
        {
          /* refresh storyboard layout and thumbnail list widgets */
          p_recreate_tab_widgets( stb_dst
                                 ,tabw
                                 ,tabw->mount_col
                                 ,tabw->mount_row
                                 ,sgpp
                                 );
          p_render_all_frame_widgets(sgpp->cll_widgets);
        }
      }

      /* shrink shell window to minimal size
       * note: this does not shrink downto 320 x 200 (is just a dummy size)
       * but shrinks as much as possible.
       */
      min_width = 320;
      min_height = 200;
      gtk_window_resize (GTK_WINDOW(sgpp->shell_window), min_width, min_height);

    }
    else
    {
        if (old_force_stb_aspect != sgpp->force_stb_aspect)
        {
          p_save_gimprc_layout_settings(sgpp);
        }

    }
  }
}  /* end p_menu_win_properties_cb */



/* -----------------------------
 * p_menu_win_vthumbs_toggle_cb
 * -----------------------------
 */
static void
p_menu_win_vthumbs_toggle_cb (GtkWidget *widget, GapStbMainGlobalParams *sgpp)
{
  if(gap_debug)
  {
    printf("p_menu_win_vthumbs_toggle_cb\n");
  }
  if(sgpp)
  {
    if(sgpp->menu_item_win_vthumbs)
    {
#ifndef GAP_ENABLE_VIDEOAPI_SUPPORT
      g_message(_("GIMP-GAP is compiled without videoapi support. "
                  "Therefore thumbnails for videoframes can not be displayed."));
      return;
#else
      sgpp->auto_vthumb = gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(sgpp->menu_item_win_vthumbs));

      sgpp->auto_vthumb_refresh_canceled = FALSE;
      sgpp->cancel_video_api = FALSE;
      if(sgpp->auto_vthumb)
      {
          p_optimized_prefetch_vthumbs(sgpp);
      }
#endif
    }
  }

}  /* end p_menu_win_vthumbs_toggle_cb */


/* ---------------------------------
 * p_menu_win_debug_log_to_stdout_cb
 * ---------------------------------
 * callback to handle logging of the configured debug output.
 * per default logging goes to stdout
 * (unless the logfilename is configured in the GAP_DEBUG_STORYBOARD_CONFIG_FILE
 *  via item "logfile=")
 */
static void
p_menu_win_debug_log_to_stdout_cb (GtkWidget *widget, GapStbMainGlobalParams *sgpp)
{
  if(sgpp)
  {
    gboolean selection_found;
    FILE *loggingFp;
    FILE *fpLog;

    selection_found = FALSE;

    loggingFp = p_debug_feature_open_optional_logfile();
    if (loggingFp != NULL)
    {
      fpLog = loggingFp;
    }
    else
    {
      fpLog = stdout;
    }


    fprintf(fpLog, "\n\nDEBUG LOG_MENU START\n");
    if(TRUE == p_is_debug_feature_item_enabled("dump_vthumb_list"))
    {
      selection_found = TRUE;
      fprintf(fpLog, "\ndump_vthumb_list enabled\n");
      gap_story_vthumb_debug_fprint_videolist(fpLog, sgpp->video_list, sgpp->vthumb_list);
    }
    if(TRUE == p_is_debug_feature_item_enabled("dump_cliplist"))
    {
      selection_found = TRUE;
      fprintf(fpLog, "\ndump_cliplist enabled\n");
      gap_story_debug_fprint_list(fpLog, sgpp->cll);
    }
    if(TRUE == p_is_debug_feature_item_enabled("dump_undostack_cliplist"))
    {
      selection_found = TRUE;
      fprintf(fpLog, "\ndump_undostack_cliplist enabled\n");
      gap_stb_undo_debug_fprint_stack(fpLog, sgpp->cll_widgets);
    }
    if(TRUE == p_is_debug_feature_item_enabled("dump_stroryboard"))
    {
      selection_found = TRUE;
      fprintf(fpLog, "\ndump_stroryboard enabled\n");
      gap_story_debug_fprint_list(fpLog, sgpp->stb);
    }
    if(TRUE == p_is_debug_feature_item_enabled("dump_undostack_stroryboard"))
    {
      selection_found = TRUE;
      fprintf(fpLog, "\ndump_undostack_stroryboard enabled\n");
      gap_stb_undo_debug_fprint_stack(fpLog, sgpp->stb_widgets);
    }

    if (selection_found != TRUE)
    {
       fprintf(fpLog, "INFO p_menu_win_debug_log_to_stdout_cb:"
              " The file: %s does not exist or does not contain"
              " any valid selction what to print for debug purpose.\n"
              , GAP_DEBUG_STORYBOARD_CONFIG_FILE);
    }

    fprintf(fpLog, "\n\nDEBUG LOG_MENU END\n");
    fflush(fpLog);
    if (loggingFp != NULL)
    {
      fclose(loggingFp);
      loggingFp = NULL;
    }
  }
}  /* end p_menu_win_debug_log_to_stdout_cb */

/* -------------------------------
 * p_menu_win_render_properties_cb
 * -------------------------------
 */
static void
p_menu_win_render_properties_cb (GtkWidget *widget, GapStbMainGlobalParams *sgpp)
{
  static GapArrArg  argv[8];
  gint l_ii;

  gint l_stb_max_open_videofile_idx;
  gint l_stb_fcache_size_per_videofile_idx;
  gint l_ffetch_max_img_cache_elements_idx;
  gint l_stb_resource_log_linterval_idx;
  gint l_stb_preview_render_full_size_idx;
  gint l_stb_isMultithreadEnabled_idx;
  gint l_stb_isMultithreadFfEncoderEnabled_idx;

  gint multithreadDefaultFlag;
  gboolean l_rc;


  if(gap_debug) printf("p_menu_win_render_properties_cb\n");

  if(sgpp->render_prop_dlg_open)
  {
    g_message(_("Global Render Properties dialog already open"));
    return;
  }

  sgpp->render_prop_dlg_open = TRUE;

  l_ii = 0;
  l_stb_max_open_videofile_idx = l_ii;
  gap_arr_arg_init(&argv[l_ii], GAP_ARR_WGT_INT);
  argv[l_ii].constraint = TRUE;
  argv[l_ii].label_txt = _("Max open Videofiles:");
  argv[l_ii].help_txt  = _("Maximum number of videofiles to be opened at same time while storyboard rendering");
  argv[l_ii].int_min   = (gint)MIN_OPEN_VIDEOFILES;
  argv[l_ii].int_max   = (gint)MAX_OPEN_VIDEOFILES;
  argv[l_ii].int_ret   = (gint)sgpp->stb_max_open_videofile;
  argv[l_ii].has_default = TRUE;
  argv[l_ii].int_default = (gint)DEFAULT_OPEN_VIDEOFILES;

  l_ii++; l_stb_fcache_size_per_videofile_idx = l_ii;
  gap_arr_arg_init(&argv[l_ii], GAP_ARR_WGT_INT);
  argv[l_ii].constraint = TRUE;
  argv[l_ii].label_txt = _("Framecache / open video:");
  argv[l_ii].help_txt  = _("Maximum number of frames to be cached per open video "
                           "(only relevant when video clips are rendered backwards)");
  argv[l_ii].int_min   = (gint)MIN_FCACHE_PER_VIDEO;
  argv[l_ii].int_max   = (gint)MAX_FCACHE_PER_VIDEO;
  argv[l_ii].int_ret   = (gint)sgpp->stb_fcache_size_per_videofile;
  argv[l_ii].has_default = TRUE;
  argv[l_ii].int_default = (gint)DEFAULT_FCACHE_PER_VIDEO;



  l_ii++; l_ffetch_max_img_cache_elements_idx = l_ii;
  gap_arr_arg_init(&argv[l_ii], GAP_ARR_WGT_INT);
  argv[l_ii].constraint = TRUE;
  argv[l_ii].label_txt = _("Max Image cache:");
  argv[l_ii].help_txt  = _("Maximum number of images to keep cached "
                           "(in memory as gimp image without display) "
                           "while storyboard rendering");
  argv[l_ii].int_min   = (gint)MIN_IMG_CACHE_ELEMENTS;
  argv[l_ii].int_max   = (gint)MAX_IMG_CACHE_ELEMENTS;
  argv[l_ii].int_ret   = (gint)sgpp->ffetch_max_img_cache_elements;
  argv[l_ii].has_default = TRUE;
  argv[l_ii].int_default = (gint)DEFAULT_IMG_CACHE_ELEMENTS;

  l_ii++; l_stb_resource_log_linterval_idx = l_ii;
  gap_arr_arg_init(&argv[l_ii], GAP_ARR_WGT_INT);
  argv[l_ii].constraint = TRUE;
  argv[l_ii].label_txt = _("Resource Loginterval:");
  argv[l_ii].help_txt  = _("Value 0 turns off resource logging to stdout. "
                           "Value n logs current resource usage afte each n processed frames to stdout "
                           "(this includes the list of cached images and opened videofiles)");
  argv[l_ii].int_min   = (gint)MIN_RESOURCE_LOG_INTERVAL;
  argv[l_ii].int_max   = (gint)MAX_RESOURCE_LOG_INTERVAL;
  argv[l_ii].int_ret   = (gint)sgpp->stb_resource_log_linterval;
  argv[l_ii].has_default = TRUE;
  argv[l_ii].int_default = (gint)DEFAULT_RESOURCE_LOG_INTERVAL;


  l_ii++; l_stb_preview_render_full_size_idx = l_ii;
  gap_arr_arg_init(&argv[l_ii], GAP_ARR_WGT_TOGGLE);
  argv[l_ii].constraint = TRUE;
  argv[l_ii].label_txt = _("Render preview at full size:");
  argv[l_ii].help_txt  = _("ON: Rendering of composite frames for preview purpose is done (slow) at full size."
                           "OFF: storyboard rendering for preview purpose is done at small size where possible."
                           "(typically faster but disables extraction of a composite frame at original size "
                           "via click into the player preview)");
  argv[l_ii].int_ret   = (gint)sgpp->stb_preview_render_full_size;
  argv[l_ii].has_default = TRUE;
  argv[l_ii].int_default = (gint)0;


  /* the default for multiprocessing support is derived from
   * gimprc "num-processors" configuration. In case gimp is configured to use
   * only one processor the default is NO otherwise it is YES.
   */
  multithreadDefaultFlag = 0; /* FALSE */
  if (gap_base_get_gimprc_int_value("num-processors"
                               , 1  /* default */
                               , 1  /* min */
                               , 32 /* max */
                               ) > 1)
  {
    multithreadDefaultFlag = 1; /* TRUE */
  }

  l_ii++; l_stb_isMultithreadEnabled_idx = l_ii;
  gap_arr_arg_init(&argv[l_ii], GAP_ARR_WGT_TOGGLE);
  argv[l_ii].constraint = TRUE;
  argv[l_ii].label_txt = _("Multiprocessor Storyboard Support:");
  argv[l_ii].help_txt  = _("ON: Rendering of composite storyboard frames uses more than one processor. "
                           "(reading frames from videoclips is done by parallel running prefetch processing) "
                           "OFF: Rendering of composite frames uses only one processor.");
  argv[l_ii].int_ret   = (gint)sgpp->stb_isMultithreadEnabled;
  argv[l_ii].has_default = TRUE;
  argv[l_ii].int_default = (gint)multithreadDefaultFlag;


  l_ii++; l_stb_isMultithreadFfEncoderEnabled_idx = l_ii;
  gap_arr_arg_init(&argv[l_ii], GAP_ARR_WGT_TOGGLE);
  argv[l_ii].constraint = TRUE;
  argv[l_ii].label_txt = _("Multiprocessor Encoder Support:");
  argv[l_ii].help_txt  = _("ON: Video encoders shall use more than one processor where implemented. "
                           "The ffmpeg based video encoder implementation supports parallel processing. "
                           "OFF: Video encoders use only one processor.");
  argv[l_ii].int_ret   = (gint)gap_base_get_gimprc_gboolean_value(
                              GAP_GIMPRC_VIDEO_ENCODER_FFMPEG_MULTIPROCESSOR_ENABLE
                             , multithreadDefaultFlag  /* default */
                             );
  argv[l_ii].has_default = TRUE;
  argv[l_ii].int_default = (gint)multithreadDefaultFlag;

  /* the reset to default button */
  l_ii++;
  gap_arr_arg_init(&argv[l_ii], GAP_ARR_WGT_DEFAULT_BUTTON);
  argv[l_ii].label_txt = _("Default");
  argv[l_ii].help_txt  = _("Use the standard built in storyboard render settings");

  l_rc = gap_arr_ok_cancel_dialog( _("Global Storyboard Render Properties")
                                 , _("Render Settings")
                                 ,G_N_ELEMENTS(argv), argv
                                 );
  sgpp->render_prop_dlg_open = FALSE;

  if(l_rc == TRUE)
  {
    gboolean isFFMpegEncoderMultiprocessorSupport;
 
    sgpp->stb_max_open_videofile         = argv[l_stb_max_open_videofile_idx].int_ret;
    sgpp->stb_fcache_size_per_videofile  = argv[l_stb_fcache_size_per_videofile_idx].int_ret;
    sgpp->ffetch_max_img_cache_elements  = argv[l_ffetch_max_img_cache_elements_idx].int_ret;
    sgpp->stb_resource_log_linterval     = argv[l_stb_resource_log_linterval_idx].int_ret;
    sgpp->stb_preview_render_full_size   = (argv[l_stb_preview_render_full_size_idx].int_ret != 0);
    sgpp->stb_isMultithreadEnabled       = (argv[l_stb_isMultithreadEnabled_idx].int_ret != 0);
    isFFMpegEncoderMultiprocessorSupport = (argv[l_stb_isMultithreadFfEncoderEnabled_idx].int_ret != 0);

    p_save_gimprc_render_settings(sgpp);
    p_save_gimprc_gboolean_value(GAP_GIMPRC_VIDEO_ENCODER_FFMPEG_MULTIPROCESSOR_ENABLE
                              , isFFMpegEncoderMultiprocessorSupport
                              );

  }
}  /* end p_menu_win_render_properties_cb  */


/* -----------------------------
 * p_menu_cll_new_cb
 * -----------------------------
 */
static void
p_menu_cll_new_cb (GtkWidget *widget, GapStbMainGlobalParams *sgpp)
{
  if(gap_debug) printf("p_menu_cll_new_cb\n");

  if(p_close_one_or_both_lists(sgpp
                           , TRUE   /* check_cliplist */
                           , FALSE  /* check_storyboard */
                           ))
  {
    sgpp->cll = gap_story_new_story_board(NULL);
    sgpp->cll->master_type = GAP_STB_MASTER_TYPE_CLIPLIST;
    p_tabw_master_prop_dialog(sgpp->cll_widgets, TRUE /* new_flag */);

  }
}  /* end p_menu_cll_new_cb */

/* -----------------------------
 * p_menu_cll_open_cb
 * -----------------------------
 */
static void
p_menu_cll_open_cb (GtkWidget *widget, GapStbMainGlobalParams *sgpp)
{
  if(gap_debug) printf("p_menu_cll_open_cb\n");
  if(sgpp->cll_widgets)
  {
      p_tabw_load_file_cb (widget, sgpp->cll_widgets);
  }

}  /* end p_menu_cll_open_cb */



/* -----------------------------
 * p_menu_cll_save_cb
 * -----------------------------
 */
static void
p_menu_cll_save_cb (GtkWidget *widget, GapStbMainGlobalParams *sgpp)
{
  gboolean l_save_ok;
  gint l_errno;

  if(gap_debug) printf("p_menu_cll_save_cb\n");

  if(sgpp->cll == NULL)          { return; }
  if(sgpp->cll->errtext != NULL)
  {
    printf("p_menu_cll_save_cb: ** INTERNAL ERROR save of corrupted storyboard canceled\n");
    return;
  }

  /* replace the internal copy of the name in the GapStoryBoard struct */
  if(sgpp->cll->storyboardfile)
  {
    g_free(sgpp->cll->storyboardfile);
  }
  sgpp->cll->storyboardfile = g_strdup(sgpp->cliplist_filename);

  /* save cliplist as new filename */
  l_save_ok = p_story_save_with_edit_settings(sgpp->cll_widgets
                         , sgpp->cll
                         , sgpp->cliplist_filename);
  l_errno = errno;

  if(!l_save_ok)
  {
     g_message (_("Failed to write cliplistfile\n"
                  "filename: '%s':\n%s")
               ,  sgpp->cliplist_filename, g_strerror (l_errno));
  }

  p_tabw_update_frame_label_and_rowpage_limits(sgpp->cll_widgets, sgpp);
}  /* end p_menu_cll_save_cb */

/* -----------------------------
 * p_menu_cll_save_cb_as
 * -----------------------------
 */
static void
p_menu_cll_save_cb_as (GtkWidget *widget, GapStbMainGlobalParams *sgpp)
{
  if(gap_debug) printf("p_menu_cll_save_cb_as\n");
  p_tabw_file_save_as_cb(widget, sgpp->cll_widgets);
  p_tabw_update_frame_label_and_rowpage_limits(sgpp->cll_widgets, sgpp);
}  /* end p_menu_cll_save_cb_as */

/* -----------------------------
 * p_menu_cll_playback_cb
 * -----------------------------
 */
static void
p_menu_cll_playback_cb (GtkWidget *widget, GapStbMainGlobalParams *sgpp)
{
  if(gap_debug) printf("p_menu_cll_playback_cb\n");
  p_player_cll_mode_cb(widget, NULL, sgpp);
}  /* end p_menu_cll_playback_cb */

/* -----------------------------
 * p_menu_cll_properties_cb
 * -----------------------------
 */
static void
p_menu_cll_properties_cb (GtkWidget *widget, GapStbMainGlobalParams *sgpp)
{
  if(gap_debug) printf("p_menu_cll_properties_cb\n");
  p_tabw_master_prop_dialog(sgpp->cll_widgets, FALSE);
}  /* end p_menu_cll_properties_cb */

/* -----------------------------
 * p_menu_cll_add_elem_cb
 * -----------------------------
 */
static void
p_menu_cll_add_elem_cb (GtkWidget *widget, GapStbMainGlobalParams *sgpp)
{
  if(gap_debug) printf("p_menu_cll_add_elem_cb\n");

  if(sgpp == NULL) { return; }
  p_tabw_add_elem(sgpp->cll_widgets, sgpp, sgpp->cll);
}  /* end p_menu_cll_add_elem_cb */

/* -----------------------------
 * p_menu_cll_add_section_elem_cb
 * -----------------------------
 */
static void
p_menu_cll_add_section_elem_cb (GtkWidget *widget, GapStbMainGlobalParams *sgpp)
{
  if(gap_debug) printf("p_menu_cll_add_section_elem_cb\n");

  if(sgpp == NULL) { return; }
  p_tabw_add_section_elem(sgpp->cll_widgets, sgpp, sgpp->cll);
}  /* end p_menu_cll_add_section_elem_cb */

/* -----------------------------
 * p_menu_cll_add_att_elem_cb
 * -----------------------------
 */
static void
p_menu_cll_add_att_elem_cb (GtkWidget *widget, GapStbMainGlobalParams *sgpp)
{
  if(gap_debug) printf("p_menu_cll_add_att_elem_cb\n");

  if(sgpp == NULL) { return; }
  p_tabw_add_att_elem(sgpp->cll_widgets, sgpp, sgpp->cll);
}  /* end p_menu_cll_add_att_elem_cb */


/* -----------------------------
 * p_menu_cll_toggle_edmode
 * -----------------------------
 */
static void
p_menu_cll_toggle_edmode (GtkWidget *widget, GapStbMainGlobalParams *sgpp)
{
  if(gap_debug) printf("p_menu_cll_close_cb\n");

  if(sgpp->cll_widgets)
  {
    switch(sgpp->cll_widgets->edmode)
    {
      case GAP_STB_EDMO_SEQUENCE_NUMBER:
        sgpp->cll_widgets->edmode = GAP_STB_EDMO_FRAME_NUMBER;
        break;
      case GAP_STB_EDMO_FRAME_NUMBER:
        sgpp->cll_widgets->edmode = GAP_STB_EDMO_TIMECODE;
        break;
      case GAP_STB_EDMO_TIMECODE:
        sgpp->cll_widgets->edmode = GAP_STB_EDMO_SRC_FRAME_NUMBER_AND_COUNT;
        break;
      default:
        sgpp->cll_widgets->edmode = GAP_STB_EDMO_SEQUENCE_NUMBER;
        break;
    }
    p_render_all_frame_widgets(sgpp->cll_widgets);
  }
}  /* end p_menu_cll_toggle_edmode */


/* -----------------------------
 * p_menu_cll_audio_otone_cb
 * -----------------------------
 */
static void
p_menu_cll_audio_otone_cb (GtkWidget *widget, GapStbMainGlobalParams *sgpp)
{
  if(gap_debug) printf("p_menu_cll_audio_otone_cb\n");
  p_tabw_gen_otone_dialog(sgpp->cll_widgets);
}  /* end p_menu_cll_audio_otone_cb */

/* -----------------------------
 * p_menu_cll_encode_cb
 * -----------------------------
 */
static void
p_menu_cll_encode_cb (GtkWidget *widget, GapStbMainGlobalParams *sgpp)
{
  if(gap_debug) printf("p_menu_cll_encode_cb\n");

  p_call_master_encoder(sgpp, sgpp->cll, sgpp->cliplist_filename);
}  /* end p_menu_cll_encode_cb */


/* -----------------------------
 * p_menu_cll_undo_cb
 * -----------------------------
 */
static void
p_menu_cll_undo_cb (GtkWidget *widget, GapStbMainGlobalParams *sgpp)
{
  if(gap_debug) printf("p_menu_cll_undo_cb\n");

  p_tabw_process_undo(sgpp->cll_widgets);

}  /* end p_menu_cll_undo_cb */

/* -----------------------------
 * p_menu_cll_redo_cb
 * -----------------------------
 */
static void
p_menu_cll_redo_cb (GtkWidget *widget, GapStbMainGlobalParams *sgpp)
{
  if(gap_debug) printf("p_menu_cll_redo_cb\n");

  p_tabw_process_redo(sgpp->cll_widgets);

}  /* end p_menu_cll_redo_cb */


/* -----------------------------
 * p_menu_cll_close_cb
 * -----------------------------
 */
static void
p_menu_cll_close_cb (GtkWidget *widget, GapStbMainGlobalParams *sgpp)
{
  if(gap_debug) printf("p_menu_cll_close_cb\n");

  p_close_one_or_both_lists(sgpp
                           , TRUE   /* check_cliplist */
                           , FALSE  /* check_storyboard */
                           );
}  /* end p_menu_cll_close_cb */










/* -----------------------------
 * p_menu_stb_new_cb
 * -----------------------------
 */
static void
p_menu_stb_new_cb (GtkWidget *widget, GapStbMainGlobalParams *sgpp)
{
  if(gap_debug) printf("p_menu_stb_new_cb\n");

  if(p_close_one_or_both_lists(sgpp
                           , FALSE  /* check_cliplist */
                           , TRUE   /* check_storyboard */
                           ))
  {
    sgpp->stb = gap_story_new_story_board(NULL);
    sgpp->stb->master_type = GAP_STB_MASTER_TYPE_STORYBOARD;
    p_tabw_master_prop_dialog(sgpp->stb_widgets, TRUE /* new_flag */);
  }
}  /* end p_menu_stb_new_cb */


/* -----------------------------
 * p_menu_stb_open_cb
 * -----------------------------
 */
static void
p_menu_stb_open_cb (GtkWidget *widget, GapStbMainGlobalParams *sgpp)
{
  if(gap_debug) printf("p_menu_stb_open_cb\n");
  if(sgpp->stb_widgets)
  {
      p_tabw_load_file_cb (widget, sgpp->stb_widgets);
  }

}  /* end p_menu_stb_open_cb */



/* -----------------------------
 * p_menu_stb_save_cb
 * -----------------------------
 */
static void
p_menu_stb_save_cb (GtkWidget *widget, GapStbMainGlobalParams *sgpp)
{
  gboolean l_save_ok;
  gint l_errno;

  if(gap_debug) printf("p_menu_stb_save_cb\n");

  if(sgpp->stb == NULL)          { return; }
  if(sgpp->stb->errtext != NULL)
  {
    printf("p_menu_stb_save_cb: ** INTERNAL ERROR save of corrupted storyboard canceled\n");
    return;
  }

  /* replace the internal copy of the name in the GapStoryBoard struct */
  if(sgpp->stb->storyboardfile)
  {
    g_free(sgpp->stb->storyboardfile);
  }
  sgpp->stb->storyboardfile = g_strdup(sgpp->storyboard_filename);

  /* save storyboard as new filename */
  l_save_ok = p_story_save_with_edit_settings(sgpp->stb_widgets
                   , sgpp->stb
                   , sgpp->storyboard_filename
                   );
  l_errno = errno;

  if(!l_save_ok)
  {
     g_message (_("Failed to write storyboardfile\n"
                  "filename: '%s':\n%s")
               ,  sgpp->storyboard_filename, g_strerror (l_errno));
  }

  p_tabw_update_frame_label_and_rowpage_limits(sgpp->stb_widgets, sgpp);
}  /* end p_menu_stb_save_cb */

/* -----------------------------
 * p_menu_stb_save_cb_as
 * -----------------------------
 */
static void
p_menu_stb_save_cb_as (GtkWidget *widget, GapStbMainGlobalParams *sgpp)
{
  if(gap_debug) printf("p_menu_stb_save_cb_as\n");
  p_tabw_file_save_as_cb(widget, sgpp->stb_widgets);
  p_tabw_update_frame_label_and_rowpage_limits(sgpp->stb_widgets, sgpp);
}  /* end p_menu_stb_save_cb_as */

/* -----------------------------
 * p_menu_stb_playback_cb
 * -----------------------------
 */
static void
p_menu_stb_playback_cb (GtkWidget *widget, GapStbMainGlobalParams *sgpp)
{
  if(gap_debug) printf("p_menu_stb_playback_cb\n");
  p_player_stb_mode_cb(widget, NULL, sgpp);
}  /* end p_menu_stb_playback_cb */

/* -----------------------------
 * p_menu_stb_properties_cb
 * -----------------------------
 */
static void
p_menu_stb_properties_cb (GtkWidget *widget, GapStbMainGlobalParams *sgpp)
{
  if(gap_debug) printf("p_menu_stb_properties_cb\n");
  p_tabw_master_prop_dialog(sgpp->stb_widgets, FALSE);
}  /* end p_menu_stb_properties_cb */

/* -----------------------------
 * p_menu_stb_add_elem_cb
 * -----------------------------
 */
static void
p_menu_stb_add_elem_cb (GtkWidget *widget, GapStbMainGlobalParams *sgpp)
{
  if(gap_debug) printf("p_menu_stb_add_elem_cb\n");

  if(sgpp == NULL) { return; }
  p_tabw_add_elem(sgpp->stb_widgets, sgpp, sgpp->stb);
}  /* end p_menu_stb_add_elem_cb */

/* -----------------------------
 * p_menu_stb_add_section_elem_cb
 * -----------------------------
 */
static void
p_menu_stb_add_section_elem_cb (GtkWidget *widget, GapStbMainGlobalParams *sgpp)
{
  if(gap_debug) printf("p_menu_stb_add_elem_cb\n");

  if(sgpp == NULL) { return; }
  p_tabw_add_section_elem(sgpp->stb_widgets, sgpp, sgpp->stb);
}  /* end p_menu_stb_add_section_elem_cb */


/* -----------------------------
 * p_menu_stb_add_att_elem_cb
 * -----------------------------
 */
static void
p_menu_stb_add_att_elem_cb (GtkWidget *widget, GapStbMainGlobalParams *sgpp)
{
  if(gap_debug) printf("p_menu_stb_add_att_elem_cb\n");

  if(sgpp == NULL) { return; }
  p_tabw_add_att_elem(sgpp->stb_widgets, sgpp, sgpp->stb);
}  /* end p_menu_stb_add_att_elem_cb */

/* -----------------------------
 * p_menu_stb_toggle_edmode
 * -----------------------------
 */
static void
p_menu_stb_toggle_edmode (GtkWidget *widget, GapStbMainGlobalParams *sgpp)
{
  if(gap_debug) printf("p_menu_stb_close_cb\n");

  if(sgpp->stb_widgets)
  {
    switch(sgpp->stb_widgets->edmode)
    {
      case GAP_STB_EDMO_SEQUENCE_NUMBER:
        sgpp->stb_widgets->edmode = GAP_STB_EDMO_FRAME_NUMBER;
        break;
      case GAP_STB_EDMO_FRAME_NUMBER:
        sgpp->stb_widgets->edmode = GAP_STB_EDMO_TIMECODE;
        break;
      case GAP_STB_EDMO_TIMECODE:
        sgpp->stb_widgets->edmode = GAP_STB_EDMO_SRC_FRAME_NUMBER_AND_COUNT;
        break;
      default:
        sgpp->stb_widgets->edmode = GAP_STB_EDMO_SEQUENCE_NUMBER;
        break;
    }
    p_render_all_frame_widgets(sgpp->stb_widgets);
  }
}  /* end p_menu_stb_toggle_edmode */


/* -----------------------------
 * p_menu_stb_audio_otone_cb
 * -----------------------------
 */
static void
p_menu_stb_audio_otone_cb (GtkWidget *widget, GapStbMainGlobalParams *sgpp)
{
  if(gap_debug) printf("p_menu_stb_audio_otone_cb\n");
  p_tabw_gen_otone_dialog(sgpp->stb_widgets);
}  /* end p_menu_stb_audio_otone_cb */

/* -----------------------------
 * p_menu_stb_encode_cb
 * -----------------------------
 */
static void
p_menu_stb_encode_cb (GtkWidget *widget, GapStbMainGlobalParams *sgpp)
{
  if(gap_debug) printf("p_menu_stb_encode_cb\n");

  p_call_master_encoder(sgpp, sgpp->stb, sgpp->storyboard_filename);
}  /* end p_menu_stb_encode_cb */


/* -----------------------------
 * p_menu_stb_undo_cb
 * -----------------------------
 */
static void
p_menu_stb_undo_cb (GtkWidget *widget, GapStbMainGlobalParams *sgpp)
{
  if(gap_debug) printf("p_menu_stb_undo_cb\n");

  p_tabw_process_undo(sgpp->stb_widgets);

}  /* end p_menu_stb_undo_cb */

/* -----------------------------
 * p_menu_stb_redo_cb
 * -----------------------------
 */
static void
p_menu_stb_redo_cb (GtkWidget *widget, GapStbMainGlobalParams *sgpp)
{
  if(gap_debug) printf("p_menu_stb_undo_cb\n");

  p_tabw_process_redo(sgpp->stb_widgets);

}  /* end p_menu_stb_redo_cb */


/* -----------------------------
 * p_menu_stb_close_cb
 * -----------------------------
 */
static void
p_menu_stb_close_cb (GtkWidget *widget, GapStbMainGlobalParams *sgpp)
{
  if(gap_debug) printf("p_menu_stb_close_cb\n");

  p_close_one_or_both_lists(sgpp
                           , FALSE  /* check_cliplist */
                           , TRUE   /* check_storyboard */
                           );

}  /* end p_menu_stb_close_cb */



/* -----------------------------
 * p_make_menu_bar_item
 * -----------------------------
 */
static GtkWidget*
p_make_menu_bar_item(GtkWidget *menu_bar, gchar *label)
{
   GtkWidget *menu = gtk_menu_new();
   GtkWidget *item = gtk_menu_item_new_with_mnemonic(label);
   GtkWidget *tearoff = gtk_tearoff_menu_item_new();

   gtk_menu_shell_insert(GTK_MENU_SHELL(menu), tearoff, 0);
   gtk_widget_show(tearoff);
   gtk_widget_show(item);

   gtk_menu_item_set_submenu(GTK_MENU_ITEM(item), menu);
   gtk_menu_shell_append(GTK_MENU_SHELL(menu_bar), item);
//   gtk_menu_set_accel_group(GTK_MENU(menu), accelerator_group);

   return menu;
}  /* end p_make_menu_bar_item */



/* -----------------------------
 * p_make_item_with_image
 * -----------------------------
 */
static GtkWidget*
p_make_item_with_image(GtkWidget *parent, const gchar *stock_id,
                     GapStbMenuCallbackFptr fptr, GapStbMainGlobalParams *sgpp)
{
   GtkWidget *item;

   item = gtk_image_menu_item_new_from_stock(stock_id
                                            , NULL /* accelerator_group */
                                            );

   gtk_menu_shell_append(GTK_MENU_SHELL(parent), item);
   gtk_widget_show(item);
   g_signal_connect(item, "activate", G_CALLBACK(fptr), sgpp);
   return item;
}  /* end p_make_item_with_image */


/* -----------------------------
 * p_make_item_with_label
 * -----------------------------
 */
static GtkWidget*
p_make_item_with_label(GtkWidget *parent, gchar *label,
                     GapStbMenuCallbackFptr fptr, GapStbMainGlobalParams *sgpp)
{
   GtkWidget *item;

   item = gtk_menu_item_new_with_mnemonic(label);

   gtk_menu_shell_append(GTK_MENU_SHELL(parent), item);
   gtk_widget_show(item);
   g_signal_connect(item, "activate", G_CALLBACK(fptr), sgpp);
   return item;
}  /* end p_make_item_with_label */


/* -----------------------------
 * p_make_check_item_with_label
 * -----------------------------
 */
static GtkWidget*
p_make_check_item_with_label(GtkWidget *parent, gchar *label
                     , GapStbMenuCallbackFptr fptr
                     , GapStbMainGlobalParams *sgpp
                     , gboolean initial_value
                     )
{
   GtkWidget *item;

   item = gtk_check_menu_item_new_with_mnemonic(label);
   gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(item), initial_value);

   gtk_menu_shell_append(GTK_MENU_SHELL(parent), item);
   gtk_widget_show(item);
   g_signal_connect(item, "activate", G_CALLBACK(fptr), sgpp);
   return item;
}  /* end p_make_check_item_with_label */

/* -----------------------------
 * p_make_menu_global
 * -----------------------------
 */
static void
p_make_menu_global(GapStbMainGlobalParams *sgpp, GtkWidget *menu_bar)
{
   GtkWidget *file_menu = p_make_menu_bar_item(menu_bar, _("Global"));

   p_make_item_with_label(file_menu, _("Properties")
                          , p_menu_win_properties_cb
                          , sgpp
                          );

   p_make_item_with_label(file_menu, _("Render Settings")
                          , p_menu_win_render_properties_cb
                          , sgpp
                          );


   sgpp->menu_item_win_vthumbs =
   p_make_check_item_with_label(file_menu, _("Video thumbnails")
                          , p_menu_win_vthumbs_toggle_cb
                          , sgpp
                          , sgpp->auto_vthumb
                          );

   if(p_is_debug_menu_enabled() == TRUE)
   {
     p_make_item_with_label(file_menu, _("DEBUG: log to stdout")
                          , p_menu_win_debug_log_to_stdout_cb
                          , sgpp
                          );
   }

   p_make_item_with_image(file_menu, GTK_STOCK_QUIT
                          , p_menu_win_quit_cb
                          , sgpp
                          );

}  /* end p_make_menu_global */

/* -----------------------------
 * p_make_menu_help
 * -----------------------------
 */
static void
p_make_menu_help(GapStbMainGlobalParams *sgpp, GtkWidget *menu_bar)
{
   GtkWidget *file_menu = p_make_menu_bar_item(menu_bar, _("Help"));

   p_make_item_with_image(file_menu, GTK_STOCK_HELP
                          , p_menu_win_help_cb
                          , sgpp
                          );
}  /* end p_make_menu_help */

/* -----------------------------
 * p_make_menu_cliplist
 * -----------------------------
 */
static void
p_make_menu_cliplist(GapStbMainGlobalParams *sgpp, GtkWidget *menu_bar)
{
   GtkWidget *file_menu = p_make_menu_bar_item(menu_bar, _("Cliplist"));

   p_make_item_with_image(file_menu, GTK_STOCK_NEW
                          , p_menu_cll_new_cb
                          , sgpp
                          );

   p_make_item_with_image(file_menu, GTK_STOCK_OPEN
                          , p_menu_cll_open_cb
                          , sgpp
                          );
   sgpp->menu_item_cll_save =
   p_make_item_with_image(file_menu, GTK_STOCK_SAVE
                          , p_menu_cll_save_cb
                          , sgpp
                          );

   sgpp->menu_item_cll_save_as =
   p_make_item_with_image(file_menu, GTK_STOCK_SAVE_AS
                          , p_menu_cll_save_cb_as
                          , sgpp
                          );

   sgpp->menu_item_cll_playback =
   p_make_item_with_label(file_menu, _("Playback")
                          , p_menu_cll_playback_cb
                          , sgpp
                          );

   sgpp->menu_item_cll_properties =
   p_make_item_with_label(file_menu, _("Properties")
                          , p_menu_cll_properties_cb
                          , sgpp
                          );

   sgpp->menu_item_cll_add_clip =
   p_make_item_with_label(file_menu, _("Create Clip")
                          , p_menu_cll_add_elem_cb
                          , sgpp
                          );

   sgpp->menu_item_cll_add_section_clip =
   p_make_item_with_label(file_menu, _("Create Section Clip")
                          , p_menu_cll_add_section_elem_cb
                          , sgpp
                          );

   sgpp->menu_item_cll_att_properties =
   p_make_item_with_label(file_menu, _("Create Transition")
                          , p_menu_cll_add_att_elem_cb
                          , sgpp
                          );

   p_make_item_with_label(file_menu, _("Toggle Unit")
                          , p_menu_cll_toggle_edmode
                          , sgpp
                          );

   sgpp->menu_item_cll_audio_otone =
   p_make_item_with_label(file_menu, _("Add Original Audio Track")
                          , p_menu_cll_audio_otone_cb
                          , sgpp
                          );

   sgpp->menu_item_cll_encode =
   p_make_item_with_label(file_menu, _("Encode")
                          , p_menu_cll_encode_cb
                          , sgpp
                          );

   sgpp->menu_item_cll_undo =
   p_make_item_with_image(file_menu, GTK_STOCK_UNDO
                          , p_menu_cll_undo_cb
                          , sgpp
                          );

   sgpp->menu_item_cll_redo =
   p_make_item_with_image(file_menu, GTK_STOCK_REDO
                          , p_menu_cll_redo_cb
                          , sgpp
                          );


   sgpp->menu_item_cll_close =
   p_make_item_with_image(file_menu, GTK_STOCK_CLOSE
                          , p_menu_cll_close_cb
                          , sgpp
                          );

}  /* end p_make_menu_cliplist */



/* -----------------------------
 * p_make_menu_storyboard
 * -----------------------------
 */
static void
p_make_menu_storyboard(GapStbMainGlobalParams *sgpp, GtkWidget *menu_bar)
{
   GtkWidget *file_menu = p_make_menu_bar_item(menu_bar, _("_Storyboard"));

   p_make_item_with_image(file_menu, GTK_STOCK_NEW
                          , p_menu_stb_new_cb
                          , sgpp
                          );

   p_make_item_with_image(file_menu, GTK_STOCK_OPEN
                          , p_menu_stb_open_cb
                          , sgpp
                          );
   sgpp->menu_item_stb_save =
   p_make_item_with_image(file_menu, GTK_STOCK_SAVE
                          , p_menu_stb_save_cb
                          , sgpp
                          );

   sgpp->menu_item_stb_save_as =
   p_make_item_with_image(file_menu, GTK_STOCK_SAVE_AS
                          , p_menu_stb_save_cb_as
                          , sgpp
                          );

   sgpp->menu_item_stb_playback =
   p_make_item_with_label(file_menu, _("Playback")
                          , p_menu_stb_playback_cb
                          , sgpp
                          );

   sgpp->menu_item_stb_properties =
   p_make_item_with_label(file_menu, _("Properties")
                          , p_menu_stb_properties_cb
                          , sgpp
                          );

   sgpp->menu_item_stb_add_clip =
   p_make_item_with_label(file_menu, _("Create Clip")
                          , p_menu_stb_add_elem_cb
                          , sgpp
                          );

   sgpp->menu_item_stb_add_section_clip =
   p_make_item_with_label(file_menu, _("Create Section Clip")
                          , p_menu_stb_add_section_elem_cb
                          , sgpp
                          );

   sgpp->menu_item_stb_att_properties =
   p_make_item_with_label(file_menu, _("Create Transition")
                          , p_menu_stb_add_att_elem_cb
                          , sgpp
                          );

   p_make_item_with_label(file_menu, _("Toggle Unit")
                          , p_menu_stb_toggle_edmode
                          , sgpp
                          );

   sgpp->menu_item_stb_audio_otone =
   p_make_item_with_label(file_menu, _("Add Original Audio Track")
                          , p_menu_stb_audio_otone_cb
                          , sgpp
                          );

   sgpp->menu_item_stb_encode =
   p_make_item_with_label(file_menu, _("Encode")
                          , p_menu_stb_encode_cb
                          , sgpp
                          );

   sgpp->menu_item_stb_undo =
   p_make_item_with_image(file_menu, GTK_STOCK_UNDO
                          , p_menu_stb_undo_cb
                          , sgpp
                          );

   sgpp->menu_item_stb_redo =
   p_make_item_with_image(file_menu, GTK_STOCK_REDO
                          , p_menu_stb_redo_cb
                          , sgpp
                          );

   sgpp->menu_item_stb_close =
   p_make_item_with_image(file_menu, GTK_STOCK_CLOSE
                          , p_menu_stb_close_cb
                          , sgpp
                          );

}  /* end p_make_menu_storyboard */



/* -----------------------------
 * p_check_unsaved_changes
 * -----------------------------
 */
static gboolean
p_check_unsaved_changes (GapStbMainGlobalParams *sgpp
                           , gboolean check_cliplist
                           , gboolean check_storyboard
                           )
{
  static  GapArrButtonArg  l_argv[2];
  static  GapArrArg  argv[2];

  if(sgpp)
  {
    gint    l_rc;
    gchar  *l_cll_msg;
    gchar  *l_stb_msg;
    gint    l_ii;

    l_cll_msg = NULL;
    l_stb_msg = NULL;
    l_ii = 0;
    if((check_cliplist) && (sgpp->cll))
    {
      if(sgpp->cll->unsaved_changes)
      {
        l_cll_msg = g_strdup_printf(_("Unsaved cliplist changes '%s'")
                               , sgpp->cliplist_filename);
        gap_arr_arg_init(&argv[l_ii], GAP_ARR_WGT_LABEL);
        argv[l_ii].label_txt = l_cll_msg;
        l_ii++;
      }
    }

    if((check_storyboard) && (sgpp->stb))
    {
      if(sgpp->stb->unsaved_changes)
      {
        l_stb_msg = g_strdup_printf(_("Unsaved storyboard changes '%s'")
                                , sgpp->storyboard_filename);
        gap_arr_arg_init(&argv[l_ii], GAP_ARR_WGT_LABEL);
        argv[l_ii].label_txt = l_stb_msg;
        l_ii++;
      }
    }

    if(l_ii > 0)
    {
      if((check_storyboard) && (check_cliplist))
      {
        l_argv[1].but_txt  = _("Quit Storyboard");
      }
      else
      {
        l_argv[1].but_txt  = _("Continue");
      }

      l_argv[1].but_val  = 0;

      l_argv[0].but_txt  = GTK_STOCK_CANCEL;
      l_argv[0].but_val  = -1;


      l_rc = gap_arr_std_dialog ( _("Storyboard unsaved changes"),
                                    _("Storyboard unsaved changes"),
                                     l_ii, argv,
                                     2, l_argv, -1);

      if(l_cll_msg)  { g_free(l_cll_msg); }
      if(l_stb_msg)  { g_free(l_stb_msg); }

      if(l_rc < 0)
      {
        return (FALSE);   /* CANCEL was pressed, dont overwrite */
      }
    }
  }
  return (TRUE);

}  /* end p_check_unsaved_changes */


/* -------------------------------------------
 * p_tabw_update_frame_label_and_rowpage_limits
 * --------------------------------------------
 * display frame label,
 * if valid storyboard available add the filename
 * and show the string "(modified)" if there are unsaved changes
 *
 * then calculate max_rowpage
 * and set upper limit for the tabw->rowpage_spinbutton_adj widget
 */
static void
p_tabw_update_frame_label_and_rowpage_limits (GapStbTabWidgets *tabw, GapStbMainGlobalParams *sgpp)
{
  gint32 l_max_rowpage;
  GapStoryBoard *stb_dst;

  if(tabw == NULL) { return; }
  if(sgpp == NULL) { return; }

  if((tabw->frame_with_name)
  && (tabw->filename_refptr))
  {
    gchar *l_hdr_txt = NULL;
    gchar *l_fil_txt = NULL;
    gchar *l_mod_txt = NULL;
    gchar *l_txt = NULL;
    gint   l_max_chars;

    l_fil_txt = g_strdup(" ");
    l_mod_txt = g_strdup(" ");
    l_max_chars = 70;

    if(tabw->type == GAP_STB_MASTER_TYPE_STORYBOARD)
    {
      l_hdr_txt = g_strdup(_("Storyboard:"));

      /* the max_chars for the cliplist header text
       * is just a guess how much characters
       * would fit when the default font is used.
       * correct calculation would be more complex.
       */
      l_max_chars = 160;
      if(sgpp->stb)
      {
        g_free(l_fil_txt);
        l_fil_txt = g_strdup(tabw->filename_refptr);
        if(sgpp->stb->unsaved_changes)
        {
          g_free(l_mod_txt);
          l_mod_txt = g_strdup(_("(modified)"));
        }
      }
    }
    else
    {
      l_hdr_txt = g_strdup(_("Cliplist:"));

      /* the max_chars for the cliplist is just a guess
       * dependent on thumbnail size and columns in the layout.
       * the layout space is limited here
       * because the extra space is requred for the player window.
       * correct calculation would be complex
       * and would have to respect current fontsize, NLS settings,
       * and something more ...
       */
      l_max_chars = 65;
      if(sgpp->cll_cols <= 4)
      {
        l_max_chars = 55;
      }

      if(sgpp->cll)
      {
        g_free(l_fil_txt);
        l_fil_txt = g_strdup(tabw->filename_refptr);
        if(sgpp->cll->unsaved_changes)
        {
          g_free(l_mod_txt);
          l_mod_txt = g_strdup(_("(modified)"));
        }
      }
    }
    l_txt = gap_base_shorten_filename(l_hdr_txt  /* prefix */
                        ,l_fil_txt              /* filenamepart */
                        ,l_mod_txt              /* suffix */
                        ,l_max_chars
                        );
    gtk_frame_set_label (GTK_FRAME (tabw->frame_with_name), l_txt);

    g_free(l_hdr_txt);
    g_free(l_fil_txt);
    g_free(l_mod_txt);
    g_free(l_txt);

  }


  l_max_rowpage = 1;
  stb_dst = p_tabw_get_stb_ptr(tabw);
  if(stb_dst)
  {
    gint32 l_act_elems = 0;

    l_act_elems = gap_story_count_active_elements(stb_dst, tabw->vtrack );
    l_max_rowpage = p_get_max_rowpage(l_act_elems, tabw->cols, tabw->rows);
  }
  if(tabw->rowpage_spinbutton_adj)
  {
    GTK_ADJUSTMENT(tabw->rowpage_spinbutton_adj)->lower = 1;
    GTK_ADJUSTMENT(tabw->rowpage_spinbutton_adj)->upper = l_max_rowpage;
    if(GTK_ADJUSTMENT(tabw->rowpage_spinbutton_adj)->value > l_max_rowpage)
    {
      gtk_adjustment_set_value(GTK_ADJUSTMENT(tabw->rowpage_spinbutton_adj), (gdouble)l_max_rowpage);
    }
  }
  if(tabw->rowpage_vscale_adj)
  {
    gdouble upper_limit;
    gdouble lower_limit;
    gdouble page_increment;
    gdouble page_size;

    page_size = (gdouble)tabw->rows;
    page_increment = (gdouble)(page_size / 2);
    lower_limit = 1.0;
    upper_limit = l_max_rowpage + page_size;

    if(gap_debug)
    {
      printf("\n######rowpage_vscale_adj : %d  tabw->rows:%d\n", (int)l_max_rowpage ,(int)tabw->rows);
      printf("lower_limit %f\n", (float)lower_limit );
      printf("upper_limit %f\n", (float)upper_limit );
      printf("page_size %f\n", (float) page_size);
      printf("page_increment %f\n", (float)page_increment );
      printf("value          %f\n", (float)GTK_ADJUSTMENT(tabw->rowpage_vscale_adj)->value );
    }

    GTK_ADJUSTMENT(tabw->rowpage_vscale_adj)->lower = lower_limit;
    GTK_ADJUSTMENT(tabw->rowpage_vscale_adj)->upper = upper_limit;
    GTK_ADJUSTMENT(tabw->rowpage_vscale_adj)->page_increment = page_increment;
    GTK_ADJUSTMENT(tabw->rowpage_vscale_adj)->page_size = page_size;
    if(GTK_ADJUSTMENT(tabw->rowpage_vscale_adj)->value > l_max_rowpage)
    {
      gtk_adjustment_set_value(GTK_ADJUSTMENT(tabw->rowpage_vscale_adj), (gdouble)l_max_rowpage);
    }

    if(lower_limit+page_size >= upper_limit)
    {
      gtk_widget_hide(tabw->rowpage_vscale);
    }
    else
    {
      gtk_widget_show(tabw->rowpage_vscale);
      gtk_widget_queue_draw(tabw->rowpage_vscale);
    }

  }

  if(tabw->total_rows_label)
  {
    char max_rowpage_txt[40];

    g_snprintf(max_rowpage_txt, sizeof(max_rowpage_txt), "%d", (int)l_max_rowpage);
    gtk_label_set_text(GTK_LABEL(tabw->total_rows_label), max_rowpage_txt);
  }

  p_widget_sensibility(sgpp);

}  /* end p_tabw_update_frame_label_and_rowpage_limits */









/* ------------------------------
 * gap_story_dlg_fetch_videoframe
 * ------------------------------
 * return thdata pointer or NULL
 */
guchar *
gap_story_dlg_fetch_videoframe(GapStbMainGlobalParams *sgpp
                   , const char *gva_videofile
                   , gint32 framenumber
                   , gint32 seltrack
                   , const char *preferred_decoder
                   , gdouble delace
                   , gint32 *th_bpp
                   , gint32 *th_width
                   , gint32 *th_height
                   , gboolean do_scale
                   )
{
  guchar *th_data;

  th_data = NULL;
#ifdef GAP_ENABLE_VIDEOAPI_SUPPORT

  if(sgpp->gva_lock)
  {
    return(NULL);
  }
  sgpp->gva_lock = TRUE;

  if((sgpp->gva_videofile) && (sgpp->gvahand))
  {
    if((strcmp(sgpp->gva_videofile, gva_videofile) != 0)
    ||(seltrack != (sgpp->gvahand->vid_track+1)))
    {
//printf(" VIDFETCH (--) CALL gap_story_vthumb_close_videofile\n");
//printf(" VIDFETCH (--) sgpp->gva_videofile: %s\n", sgpp->gva_videofile);
//printf(" VIDFETCH (--) sgpp->gvahand->vid_track: %d\n", (int)sgpp->gvahand->vid_track);
//printf(" VIDFETCH (--) gva_videofile: %s\n", gva_videofile);
//printf(" VIDFETCH (--) seltrack: %d\n", (int)seltrack);
      gap_story_vthumb_close_videofile(sgpp);
    }
  }

//printf(" VIDFETCH (-0) sgpp->gvahand: %d  framenumber:%06d\n", (int)sgpp->gvahand, (int)framenumber);
  if(sgpp->gvahand == NULL)
  {
    gap_story_vthumb_open_videofile(sgpp, gva_videofile, seltrack, preferred_decoder);
  }

  if(sgpp->gvahand)
  {
     gint32 l_deinterlace;
     gdouble l_threshold;

     if(sgpp->progress_bar_sub)
     {
       t_GVA_DecoderElem *dec_elem;
       char *videoseek_msg;

       videoseek_msg = NULL;
       dec_elem = (t_GVA_DecoderElem *)sgpp->gvahand->dec_elem;
       if(dec_elem->decoder_name)
       {
         videoseek_msg = g_strdup_printf(_("Videoseek (decoder: %s)"), dec_elem->decoder_name);
       }
       else
       {
         videoseek_msg = g_strdup_printf(_("Videoseek"));
       }

       gtk_progress_bar_set_text(GTK_PROGRESS_BAR(sgpp->progress_bar_sub), videoseek_msg);
       g_free(videoseek_msg);
     }

     if(do_scale == TRUE)
     {
       gint32 vthumb_size;

       vthumb_size = 256;
       if(sgpp->gvahand->width > sgpp->gvahand->height)
       {
         *th_width = vthumb_size;
         *th_height = (sgpp->gvahand->height * vthumb_size) / sgpp->gvahand->width;
       }
       else
       {
         *th_height = vthumb_size;
         *th_width = (sgpp->gvahand->width * vthumb_size) / sgpp->gvahand->height;
       }

     }
     else
     {
         *th_width = sgpp->gvahand->width;
         *th_height = sgpp->gvahand->height;
     }
     *th_bpp = sgpp->gvahand->frame_bpp;

     /* split delace value: integer part is deinterlace mode, rest is threshold */
     l_deinterlace = delace;
     l_threshold = delace - (gdouble)l_deinterlace;

     sgpp->cancel_video_api = FALSE;

//printf(" VIDFETCH (-6) current_seek_nr:%d current_frame_nr:%d\n", (int)sgpp->gvahand->current_seek_nr  ,(int)sgpp->gvahand->current_frame_nr );
     /* fetch the wanted framenr  */
     th_data = GVA_fetch_frame_to_buffer(sgpp->gvahand
                , do_scale
                , FALSE           /* isBackwards */
                , framenumber
                , l_deinterlace
                , l_threshold
                , th_bpp
                , th_width
                , th_height
                );
//printf(" VIDFETCH (-7) current_seek_nr:%d current_frame_nr:%d\n", (int)sgpp->gvahand->current_seek_nr  ,(int)sgpp->gvahand->current_frame_nr );
     if(sgpp->progress_bar_sub)
     {
       gtk_progress_bar_set_text(GTK_PROGRESS_BAR(sgpp->progress_bar_sub), " ");
       gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(sgpp->progress_bar_sub), 0.0);
     }
  }

  if(th_data)
  {
    /* throw away frame data because the fetch was cancelled
     * (an empty display is better than trash or the wrong frame)
     */
    if(sgpp->cancel_video_api)
    {
      g_free(th_data);
      th_data = NULL;
    }
  }

  sgpp->gva_lock = FALSE;

#endif
  return (th_data);
}  /* end gap_story_dlg_fetch_videoframe */


/* -----------------------------
 * p_prefetch_vthumbs
 * -----------------------------
 * this procedure does a prefetch of all
 * videoframes that are start frame in a storyboard_element
 * with type GAP_STBREC_VID_MOVIE
 * the fetch causes creation of all the vthumbs
 */
static void
p_prefetch_vthumbs (GapStbMainGlobalParams *sgpp, GapStoryBoard *stb)
{
  GapStorySection  *section;
  GapStoryElem     *stb_elem;
  GapVThumbElem    *vthumb;
  gint32         l_total;
  gint32         l_count;


  if(stb == NULL)
  {
    return;
  }

  if(gap_debug)
  {
    printf("p_prefetch_vthumbs : stb: %d\n"
       ,(int)stb
       );
  }

  l_total = 0;
  for(section = stb->stb_section; section != NULL; section = section->next)
  {
    for(stb_elem = section->stb_elem; stb_elem != NULL;  stb_elem = stb_elem->next)
    {
      if(stb_elem->record_type == GAP_STBREC_VID_MOVIE)
      {
        l_total++;
      }
    }
  }

  l_count = 0;
  for(section = stb->stb_section; section != NULL; section = section->next)
  {
    for(stb_elem = section->stb_elem; stb_elem != NULL;  stb_elem = stb_elem->next)
    {
      if(stb_elem->record_type == GAP_STBREC_VID_MOVIE)
      {
        gchar  *msg;

        if((sgpp->cancel_video_api) || (sgpp->auto_vthumb_refresh_canceled == TRUE) || (sgpp->auto_vthumb != TRUE))
        {
          if(sgpp->progress_bar_master)
          {
            gtk_progress_bar_set_text(GTK_PROGRESS_BAR(sgpp->progress_bar_master),
                                     _("video thumbnail cancelled"));
            gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(sgpp->progress_bar_master), 0);
          }
          if(gap_debug)
          {
            printf("p_prefetch_vthumbs : (1) video thumbnail cancelled: cancel_video_api: %d auto_vthumb_refresh_canceled:%d auto_vthumb:%d\n"
               ,(int)sgpp->cancel_video_api
               ,(int)sgpp->auto_vthumb_refresh_canceled
               ,(int)sgpp->auto_vthumb
               );
          }
          return;
        }

        l_count++;

        vthumb = gap_story_vthumb_elem_fetch(sgpp
                    ,stb
                    ,stb_elem
                    ,stb_elem->from_frame
                    ,stb_elem->seltrack
                    ,gap_story_get_preferred_decoder(stb, stb_elem)
                    );
        if(sgpp->progress_bar_master)
        {

           msg = g_strdup_printf(_("Fetching video thumbnail for clip: %d (out of %d)")
                                ,(int)l_count
                                ,(int)l_total
                                );
           gtk_progress_bar_set_text(GTK_PROGRESS_BAR(sgpp->progress_bar_master), msg);
           gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(sgpp->progress_bar_master)
                                        ,(gdouble)l_count / (gdouble)l_total
                                        );

           gap_story_vthumb_g_main_context_iteration(sgpp);

           /* processing of the g_main_context_iteration may have changed objects
            * in the storyboard or cliplist
            */
           if (sgpp->vthumb_prefetch_in_progress != GAP_VTHUMB_PREFETCH_IN_PROGRESS)
           {
             if(gap_debug)
             {
               printf("p_prefetch_vthumbs: prefetch PROGRESS interrupt occured\n");
             }

             /* stop current prefetch of (possibly outdated) storyboard
              */
             p_reset_progress_bars(sgpp);
             return;
           }


        }
      }
    }
  }

  if(gap_debug)
  {
    printf("p_prefetch_vthumbs : Loop done\n");
  }
  p_reset_progress_bars(sgpp);

}  /* end p_prefetch_vthumbs */


/* ---------------------------------
 * p_optimized_prefetch_vthumbs_worker
 * ---------------------------------
 * this procedure does a prefetch of all
 * videoframes that are start frame in a storyboard_element
 * with type GAP_STBREC_VID_MOVIE
 * the fetch causes creation of all the vthumbs for both storyboard and cliplist
 */
static void
p_optimized_prefetch_vthumbs_worker (GapStbMainGlobalParams *sgpp)
{
  GapStoryBoard *stb;

  stb = NULL;
  if((sgpp->stb_widgets) && (sgpp->stb))
  {
    stb = gap_story_board_duplicate_distinct_sorted(stb, sgpp->stb);
  }

  if((sgpp->cll_widgets) && (sgpp->cll))
  {
    stb = gap_story_board_duplicate_distinct_sorted(stb, sgpp->cll);
  }

  if(stb == NULL)
  {
    return;
  }

  /* gap_story_save(stb, "zz_STB_distinct_sort.txt"); */ /* for test only */

  /* operate on a copy that includes elements of both (stb + cll) storyboards
   * this merged copy has groups of same resources sorted by start frame numbers
   * (to mimimize both video open operations and frame seek times)
   */
  p_prefetch_vthumbs(sgpp, stb);
  gap_story_free_storyboard(&stb);
}  /* end p_optimized_prefetch_vthumbs_worker */


/* ---------------------------------
 * p_optimized_prefetch_both_vthumbs
 * ---------------------------------
 * this procedure does a prefetch of all
 * videoframes that are start frame in a storyboard_element
 * with type GAP_STBREC_VID_MOVIE
 * the fetch causes creation of all the vthumbs for both storyboard and cliplist
 */
static void
p_optimized_prefetch_vthumbs (GapStbMainGlobalParams *sgpp)
{
  static gboolean recreateRequired = FALSE;
  static gboolean refreshRequired = FALSE;


  if(gap_debug)
  {
    printf("p_optimized_prefetch_vthumbs\n");
  }
  if(sgpp)
  {

    sgpp->auto_vthumb_refresh_canceled = FALSE;
    sgpp->cancel_video_api = FALSE;
    if(sgpp->auto_vthumb)
    {
        gboolean option_restart;

        refreshRequired = TRUE;
        recreateRequired = FALSE;


        sgpp->vthumb_prefetch_in_progress = GAP_VTHUMB_PREFETCH_IN_PROGRESS;
        while(TRUE)
        {
          p_optimized_prefetch_vthumbs_worker(sgpp);
          /*
           * - one option is to render default icon, and restart the prefetch
           *   via GAP_VTHUMB_PREFETCH_RESTART_REQUEST
           *   (because the storyboard may have changed since prefetch was started
           *    note that prefetch will be very quick for all clips where vthumb is already present
           *    from the cancelled previous prefetch cycle)
           *    (currently this attempt leads to crashes that i could not locate yet)
           * - the other (currently implemented) option is to cancel prefetch and implicitly turn off auto_vthumb mode
           */
          option_restart = TRUE;


          if(gap_debug)
          {
            printf("p_optimized_prefetch_vthumbs  vthumb_prefetch_in_progress"
                   " (0 NOTACTIVE, 1 PROGRESS, 2 RESTART, 3 CANCEL) value:%d\n"
              ,(int)sgpp->vthumb_prefetch_in_progress
              );
          }

          if(sgpp->vthumb_prefetch_in_progress == GAP_VTHUMB_PREFETCH_RESTART_REQUEST)
          {
            recreateRequired = TRUE;
//             if(option_restart == FALSE)
//             {
//               if(sgpp->progress_bar_master)
//               {
//                 gtk_progress_bar_set_text(GTK_PROGRESS_BAR(sgpp->progress_bar_master),
//                                          _("video thumbnail cancelled"));
//                 gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(sgpp->progress_bar_master), 0);
//               }
//               p_cancel_button_cb(NULL, sgpp);
//               sgpp->vthumb_prefetch_in_progress = GAP_VTHUMB_PREFETCH_NOT_ACTIVE;
//             }
//             else
            {
              sgpp->vthumb_prefetch_in_progress = GAP_VTHUMB_PREFETCH_IN_PROGRESS;
              printf("performing GAP_VTHUMB_PREFETCH_RESTART_REQUEST\n");
            }
          }
          else
          {
            /* regular end (all vthumbs prefeteched OK) */
            sgpp->vthumb_prefetch_in_progress = GAP_VTHUMB_PREFETCH_NOT_ACTIVE;
            break;
          }
        }


    }

    if(refreshRequired == TRUE)
    {
      if(sgpp->stb_widgets)
      {
        if(recreateRequired)
        {
          gap_story_dlg_recreate_tab_widgets(sgpp->stb_widgets, sgpp);
        }
        else
        {
          p_render_all_frame_widgets(sgpp->stb_widgets);
        }
      }
      if(sgpp->cll_widgets)
      {
        if(recreateRequired)
        {
          gap_story_dlg_recreate_tab_widgets(sgpp->cll_widgets, sgpp);
        }
        else
        {
          p_render_all_frame_widgets(sgpp->cll_widgets);
        }
      }
      refreshRequired = FALSE;
      recreateRequired = FALSE;
    }
  }

}  /* end p_optimized_prefetch_vthumbs */



/* -----------------------------------
 * p_tabw_set_focus_to_first_fw
 * -----------------------------------
 */
static void
p_tabw_set_focus_to_first_fw(GapStbTabWidgets *tabw)
{
  GapStbFrameWidget *fw;

  if(tabw == NULL) { return; }

  if(tabw->fw_tab_size > 0)
  {
    fw = tabw->fw_tab[0];
    gtk_widget_grab_focus (fw->vbox);
  }

}  /* end p_tabw_set_focus_to_first_fw */

/* -----------------------------------
 * p_tabw_key_press_event_cb
 * -----------------------------------
 * handling of ctrl-c, ctrl-x, ctrl-v, ctrl-z keys
 * pressed when focus is somewhere in the tabw frame_with_name.
 */
static gboolean
p_tabw_key_press_event_cb ( GtkWidget *widget
                    , GdkEvent  *event
                    , GapStbTabWidgets *tabw
                    )
{
  gint keyval;
  gboolean sensitive;
  gboolean ctrl_modifier;
  GapStbMainGlobalParams *sgpp;
  GapStoryBoard *stb;
  GdkEventKey   *kevent;

  if(gap_debug) printf("p_tabw_key_press_event_cb : tabw:%d\n", (int)tabw);

  if(tabw == NULL)
  {
    return (FALSE);
  }
  sgpp = (GapStbMainGlobalParams *)tabw->sgpp;
  stb = p_tabw_get_stb_ptr(tabw);

  ctrl_modifier = FALSE;

  switch (event->type)
  {
    case GDK_KEY_PRESS:
      kevent =  (GdkEventKey *) event;

      if(kevent->state & GDK_CONTROL_MASK)
      {
        ctrl_modifier = TRUE;
      }
      keyval = (gint)kevent->keyval;
      if(gap_debug)
      {
        printf("KEY Press : val:%d  ctrl_modifier:%d\n", (int)keyval, (int)ctrl_modifier);
      }
      switch (keyval)
      {
        case GDK_C:
        case GDK_c:
          if(ctrl_modifier)
          {
            if(gap_debug) printf("GDK_C: copy\n");
            sensitive = p_cut_copy_sensitive(stb);
            if(sensitive)
            {
              p_tabw_edit_copy_cb(NULL, tabw);
            }
            return(TRUE);
          }
          break;
        case GDK_V:
        case GDK_v:
          if(ctrl_modifier)
          {
            if(gap_debug) printf("GDK_V: paste\n");
            sensitive = p_paste_sensitive(sgpp, stb);
            if(sensitive)
            {
              p_tabw_edit_paste_cb(NULL, tabw);

              /* after paste focus is lost
               * force setting focus at 1st widget in the fw_tab
               * to catch the release event of the v-key
               */
              p_tabw_set_focus_to_first_fw(tabw);
            }
            return(TRUE);
          }
          break;
        case GDK_X:
        case GDK_x:
          if(ctrl_modifier)
          {
            if(gap_debug) printf("GDK_X: cut\n");
            sensitive = p_cut_copy_sensitive(stb);
            if(sensitive)
            {
              p_tabw_edit_cut_cb(NULL, tabw);
            }
            return(TRUE);
          }
          break;
        case GDK_Z:
        case GDK_z:
          if(ctrl_modifier)
          {
            if(gap_debug) printf("GDK_Z: undo\n");

            /* undo not implemented yet */
            return(TRUE);
          }
          break;
      }
      break;

    default:
      /*  do nothing  */
      break;
  }

  return FALSE;
}  /* end p_tabw_key_press_event_cb */

/* -----------------------------------
 * p_tabw_scroll_event_cb
 * -----------------------------------
 * handling of ctrl-c, ctrl-x, ctrl-v, ctrl-z keys
 * pressed when focus is somewhere in the tabw frame_with_name.
 */
static gboolean
p_tabw_scroll_event_cb ( GtkWidget *widget
                    , GdkEventScroll *sevent
                    , GapStbTabWidgets *tabw
                    )
{
  gint32  value;


  if(tabw == NULL) { return FALSE;}

  value = (gint32)GTK_ADJUSTMENT(tabw->rowpage_spinbutton_adj)->value;

  if((sevent->direction == GDK_SCROLL_UP)
  || (sevent->direction == GDK_SCROLL_RIGHT))
  {
    value--;
  }
  else
  {
    value++;
  }

  /* set value in the rowpage spinbutton
   * (this fires another callback for update of tabw->rowpage;)
   */
  gtk_adjustment_set_value(GTK_ADJUSTMENT(tabw->rowpage_spinbutton_adj), (gdouble)value);

  return FALSE;
}  /* end p_tabw_scroll_event_cb */

/* ---------------------------------
 * story_dlg_response
 * ---------------------------------
 */
static void
story_dlg_response (GtkWidget *widget,
                 gint       response_id,
                 GapStbMainGlobalParams *sgpp)
{
  GtkWidget *dialog;

  switch (response_id)
  {
    case GTK_RESPONSE_OK:
      if(sgpp)
      {
        if (GTK_WIDGET_VISIBLE (sgpp->shell_window))
          gtk_widget_hide (sgpp->shell_window);

        sgpp->run = TRUE;
      }

    default:
      dialog = NULL;
      if(sgpp)
      {
        gap_story_vthumb_close_videofile(sgpp);
        dialog = sgpp->shell_window;
        if(dialog)
        {
          sgpp->shell_window = NULL;
          gtk_widget_destroy (dialog);
        }
      }
      gtk_main_quit ();
      break;
  }
}  /* end story_dlg_response */

/* ---------------------------------
 * story_dlg_destroy
 * ---------------------------------
 */
static void
story_dlg_destroy (GtkWidget *widget,
                 GapStbMainGlobalParams *sgpp_1)
{
  GtkWidget *dialog;
  GapStbMainGlobalParams *sgpp;

  dialog = NULL;

  if(sgpp_1 != NULL)
  {
    sgpp = sgpp_1;
  }
  else
  {
    sgpp = (GapStbMainGlobalParams *)g_object_get_data(G_OBJECT(widget), "sgpp");
  }


  if(sgpp)
  {
    gap_story_vthumb_close_videofile(sgpp);
    dialog = sgpp->shell_window;
    if(dialog)
    {
      sgpp->shell_window = NULL;
      gtk_widget_destroy (dialog);
    }
  }

  gtk_main_quit ();

}  /* end story_dlg_destroy */



/* -----------------------------
 * p_recreate_tab_widgets
 * -----------------------------
 * destroy old fw_table and all the attached frame_widgets if they exist.
 * then newly create that stuff with empty frame_widgets.
 * the caller should invoke the p_frame_widgets_render procedure afterwards.
 */
static void
p_recreate_tab_widgets(GapStoryBoard *stb
                       ,GapStbTabWidgets *tabw
                       ,gint32 mount_col
                       ,gint32 mount_row
                       ,GapStbMainGlobalParams *sgpp
                      )
{
  if(gap_debug) printf("p_recreate_tab_widgets:START stb:%d tabw:%d\n", (int)stb ,(int)tabw);

  if(tabw)
  {
    GtkWidget *table;
    gint col;
    gint row;
    gint ii;
    gint32 l_max_ii = 0;
    gint32 l_act_elems = 0;

//printf("p_recreate_tab_widgets:(2)\n");

    if(tabw->mount_table == NULL)
    {
      printf("Internal error: tabw->mount_table is NULL\n");
      return;
    }

    tabw->sgpp = sgpp;

    if(tabw->fw_tab)
    {
      /* destroy the old table of frame widgets */
      for(ii=0; ii < tabw->fw_tab_size; ii++)
      {
        p_frame_widget_destroy(tabw->fw_tab[ii]);
        g_free(tabw->fw_tab[ii]);
      }
      g_free(tabw->fw_tab);
      tabw->fw_tab = NULL;
    }

    if(tabw->fw_gtk_table)
    {
      /* destroy the old table widget */
      gtk_widget_destroy(tabw->fw_gtk_table);
      tabw->fw_gtk_table = NULL;
    }

    tabw->mount_col = mount_col;
    tabw->mount_row = mount_row;

    if(stb)
    {
      gint32 l_width = 1;
      gint32 l_height = 1;

      gap_story_get_master_size_respecting_aspect(stb
                               ,&l_width
                               ,&l_height);
      if(l_width > l_height)
      {
        tabw->thumb_width = tabw->thumbsize;
        tabw->thumb_height = tabw->thumbsize * l_height / l_width;
      }
      else
      {
        tabw->thumb_width = tabw->thumbsize * l_width / l_height;
        tabw->thumb_height = tabw->thumbsize;
      }

      l_act_elems = gap_story_count_active_elements(stb, tabw->vtrack);
    }

    if(gap_debug) printf("p_recreate_tab_widgets: rows:%d cols:%d\n", (int)tabw->rows ,(int)tabw->cols );

    table = gtk_table_new (tabw->rows, tabw->cols, TRUE);
    gtk_widget_show (table);
    tabw->fw_gtk_table = table;
    gtk_table_set_row_spacings (GTK_TABLE (table), 2);
    gtk_table_set_col_spacings (GTK_TABLE (table), 2);

    gtk_table_attach (GTK_TABLE (tabw->mount_table)
                      , tabw->fw_gtk_table
                      , tabw->mount_col, tabw->mount_col+1
                      , tabw->mount_row, tabw->mount_row+1
                      , (GtkAttachOptions) (GTK_FILL | GTK_EXPAND)
                      , (GtkAttachOptions) (GTK_FILL | GTK_EXPAND)
                      , 0, 0);


    /* allocate as much frame widgets as can be displayed
     * in the table
     */
    tabw->fw_tab_size = MIN((tabw->rows * tabw->cols), l_act_elems);
    tabw->fw_tab = g_malloc0(sizeof(GapStbFrameWidget *) * tabw->fw_tab_size);

    /* init buffers for the frame widgets */
    for(ii=0; ii < tabw->fw_tab_size; ii++)
    {
      tabw->fw_tab[ii] = g_new(GapStbFrameWidget, 1);
      p_frame_widget_init_empty(tabw, tabw->fw_tab[ii]);
    }

    ii = 0;
    l_max_ii = l_act_elems - ((tabw->rowpage -1) * MAX(tabw->cols,1));
    for(row=0; row < tabw->rows; row++)
    {
      for(col=0; col < tabw->cols; col++)
      {
         if(ii < l_max_ii)
         {
           GapStbFrameWidget *fw;

           fw = tabw->fw_tab[ii];
           gtk_table_attach (GTK_TABLE (tabw->fw_gtk_table)
                        , fw->vbox
                        , col, col+1
                        , row, row+1
                        , (GtkAttachOptions) (GTK_FILL | GTK_EXPAND)
                        , (GtkAttachOptions) (GTK_FILL | GTK_EXPAND)
                        , 0, 0);
         }
         else
         {
           char *l_txt;
           GtkWidget *label;

           /* dummy label */
           l_txt = g_strdup_printf(" ");

           label = gtk_label_new (l_txt);
           gtk_widget_show (label);


           gtk_table_attach (GTK_TABLE (tabw->fw_gtk_table)
                        , label                          // tabw->fw_tab[ii].vbox
                        , col, col+1
                        , row, row+1
                        , (GtkAttachOptions) (GTK_FILL | GTK_EXPAND)
                        , (GtkAttachOptions) (GTK_FILL | GTK_EXPAND)
                        , 0, 0);

           g_free(l_txt);
         }
         ii++;
      }
    }

    /* display frame label (if valid storyboard available add the filename)
     * and set upper limit for the tabw->rowpage_spinbutton_adj widget
     */
    p_tabw_update_frame_label_and_rowpage_limits(tabw, sgpp);

  }
}  /* end p_recreate_tab_widgets */


/* ----------------------------------
 * gap_story_dlg_recreate_tab_widgets
 * ----------------------------------
 */
void
gap_story_dlg_recreate_tab_widgets(GapStbTabWidgets *tabw
                                  ,GapStbMainGlobalParams *sgpp
                                  )
{
  GapStoryBoard *stb_dst;

  if(tabw)
  {
    stb_dst = p_tabw_get_stb_ptr(tabw);

    /* clear selections when scene detection adds new clips */
    gap_story_selection_all_set(stb_dst, FALSE);

    /* refresh storyboard layout and thumbnail list widgets */
    p_recreate_tab_widgets( stb_dst
                           ,tabw
                           ,tabw->mount_col
                           ,tabw->mount_row
                           ,sgpp
                           );
    p_render_all_frame_widgets(tabw);
  }
}  /* end gap_story_dlg_recreate_tab_widgets */


/* ---------------------------------
 * p_find_section_by_active_index
 * ---------------------------------
 * return the section corresponding to the specified active_index
 *        or NULL in case there is no such section available in
 *        the specified storyboard.
 */
static GapStorySection *
p_find_section_by_active_index(GapStoryBoard *stb, gint active_index)
{
  GapStorySection *section;
  gint ii;

  section = NULL;
  switch(active_index)
  {
    case 0:
      section = gap_story_find_main_section(stb);
      break;
    case 1:
      section = stb->mask_section;
      break;
    default:
      ii = 0;
      for(section = stb->stb_section; section != NULL; section = section->next)
      {
        if (ii == active_index)
        {
          return (section);
        }
        ii++;
      }
      break;
  }
  return (section);

}  /* end p_find_section_by_active_index */


/* ---------------------------------
 * p_find_combo_index_of_section
 * ---------------------------------
 * return the combo box index corresponding to the specified section
 *        (if section not found return 0 that also refers
 *         to the MAIN section)
 */
static gint
p_find_combo_index_of_section(GapStoryBoard *stb, GapStorySection *target_section)
{
  GapStorySection *main_section;
  GapStorySection *section;
  gint index;

  if(target_section == stb->mask_section)
  {
    return (1);
  }
  main_section = gap_story_find_main_section(stb);
  if(target_section == main_section)
  {
    return (0);
  }

  index = 2;
  for(section = stb->stb_section; section != NULL; section = section->next)
  {
    if((section == main_section) || (section == stb->mask_section))
    {
      continue;
    }
    if (section->section_id == target_section->section_id)
    {
      return (index);
    }
    index++;
  }

  return (0);

}  /* end p_find_combo_index_of_section */

/* ---------------------------------
 * p_activate_section_by_combo_index
 * ---------------------------------
 * select the active section by
 * its index in the combo box.
 */
static void
p_activate_section_by_combo_index(gint target_index
                       , GapStbTabWidgets *tabw)
{
  GapStoryBoard *stb_dst;

  if(tabw == NULL) { return; }
  if(tabw->sections_combo == NULL) {return;}

  stb_dst = p_tabw_get_stb_ptr(tabw);

  if (stb_dst)
  {
     GapStorySection *section;

     section = p_find_section_by_active_index(stb_dst, target_index);

     if (gap_debug)
     {
       if (section == NULL)
       {
         printf("p_activate_section_by_combo_index: %d  NULL section not found\n"
                ,(int)target_index
                );
       }
       else
       {
         printf("p_activate_section_by_combo_index: %d active_section: %d section:%d "
            ,(int)target_index
            , (int)section
            , (int)stb_dst->active_section
            );
         if (section->section_name == NULL)
         {
           printf("section_name: NULL (e.g. MAIN)\n");
         }
         else
         {
           printf("section_name: %s\n", section->section_name);
         }
       }
     }


     if ((section != stb_dst->active_section)
     && (section != NULL))
     {
       gint32 current_vtrack;

       current_vtrack = gap_story_get_current_vtrack(stb_dst, section);

       stb_dst->active_section = section;

       p_tabw_sensibility(tabw->sgpp, stb_dst, tabw);

       /* fake -99 as current video track to force always refresh
        * in p_vtrack_spinbutton_cb
        */
       tabw->vtrack = -99;

       gtk_adjustment_set_value(GTK_ADJUSTMENT(tabw->vtrack_spinbutton_adj)
                               , current_vtrack);

       if (tabw->vtrack != current_vtrack)
       {
         /* the callback p_vtrack_spinbutton_cb is normally triggered automatically
          * if the current vtrack number is set to the another number.
          * this explicit call is for the case where the track number
          * did not change. (but same track number in another section requires
          * always a refresh, that is triggered here)
          */
         p_vtrack_spinbutton_cb(NULL, tabw);
       }
     }
     else
     {
       p_tabw_sensibility(tabw->sgpp, stb_dst, tabw);
     }
  }

}  /* end p_activate_section_by_combo_index */

/* ---------------------------------
 * p_set_strings_for_section_combo
 * ---------------------------------
 * replace all entries in the combo box for section  names.
 * Note: at least the MAIN and Mask section are always present
 * in the combo box, even if they are empty or not present in
 * the storyboard (that is refered by tabw).
 */
static void
p_set_strings_for_section_combo(GapStbTabWidgets *tabw)
{
  GapStoryBoard *stb;
  GapStorySection *section;
  gint index_of_active_item;
  gint index;
  gint remove_position;

  if(tabw == NULL) {return;}
  if(tabw->sections_combo == NULL) {return;}

  /* remove all already existing entries */
  remove_position = 0;
  while(tabw->sections_combo_elem_count > 0)
  {
    gtk_combo_box_remove_text(GTK_COMBO_BOX (tabw->sections_combo), remove_position);
    tabw->sections_combo_elem_count--;
  }

  index_of_active_item = 0;
  tabw->sections_combo_elem_count = 0;

  /* always add the entries for main and mask section */
  gtk_combo_box_append_text(GTK_COMBO_BOX (tabw->sections_combo)
                                    ,_("MAIN"));
  tabw->sections_combo_elem_count++;
  gtk_combo_box_append_text(GTK_COMBO_BOX (tabw->sections_combo)
                                    ,_("Masks"));
  tabw->sections_combo_elem_count++;

  /* add all other subsection names if there are any present in the storyboard */
  stb = p_tabw_get_stb_ptr (tabw);
  if(stb)
  {
    index = 2;
    for(section = stb->stb_section; section != NULL; section = section->next)
    {
       if (section->section_name == NULL)
       {
         if (section == stb->active_section)
         {
           index_of_active_item = 0;
         }
       }
       else
       {
          if(section == stb->mask_section)
          {
            if (section == stb->active_section)
            {
              index_of_active_item = 1;
            }
          }
          else
          {
            gchar *l_txt;

            if(section == stb->active_section)
            {
              index_of_active_item = index;
            }

            l_txt = gap_base_shorten_filename(NULL  /* prefix */
                        ,section->section_name     /* filenamepart */
                        ,NULL                      /* suffix */
                        ,12                        /* max_chars */
                        );


            gtk_combo_box_append_text(GTK_COMBO_BOX (tabw->sections_combo)
                                    , l_txt);
            g_free(l_txt);
            tabw->sections_combo_elem_count++;
            index++;
          }
       }
    }
  }

  gtk_combo_box_set_active(GTK_COMBO_BOX (tabw->sections_combo)
      ,index_of_active_item);
}  /* end p_make_func_menu_item */

/* -----------------------------
 * p_create_button_bar
 * -----------------------------
 * create button bar (cut,copy,paste,play and rowpage spinbutton)
 * and rowpage vscale widgets
 */
static GtkWidget *
p_create_button_bar(GapStbTabWidgets *tabw
                   ,gint32 mount_col
                   ,gint32 mount_row
                   ,GapStbMainGlobalParams *sgpp
                   ,gint32 mount_vs_col
                   ,gint32 mount_vs_row
                   )
{
  /* rowpage spinbutton and labels */
  GtkWidget *hbox;
  GtkWidget *hbox2;
  GtkWidget *entry;
  GtkWidget *label;
  GtkWidget *button;
  GtkWidget *vscale;
  GtkWidget *spinbutton;
  GtkObject *spinbutton_adj;
  GtkObject *adj;

  /* the hbox */
  hbox = gtk_hbox_new (FALSE, 2);
  gtk_widget_show (hbox);



  gtk_table_attach (GTK_TABLE (tabw->mount_table)
                  , hbox
                  , mount_col, mount_col+1
                  , mount_row, mount_row+1
                  , (GtkAttachOptions) (GTK_FILL | GTK_EXPAND)
                  , (GtkAttachOptions) (0)
                  , 0, 0);


  /* the hbox2 */
  hbox2 = gtk_hbox_new (FALSE, 2);
  gtk_container_set_border_width (GTK_CONTAINER (hbox2), 2);
  gtk_box_pack_start (GTK_BOX (hbox), hbox2, FALSE, FALSE, 0);
  gtk_widget_show (hbox2);


  /* with_load_and_save button */
  {
    /*  The Load button  */
    button = gap_gtk_button_new_from_stock_icon (GTK_STOCK_OPEN );
    gtk_box_pack_start (GTK_BOX (hbox2), button, FALSE, FALSE, 0);
    g_signal_connect (G_OBJECT (button), "clicked",
                      G_CALLBACK (p_tabw_load_file_cb),
                      tabw);
    if(tabw->type == GAP_STB_MASTER_TYPE_STORYBOARD)
    {
      gimp_help_set_help_data (button,
                             _("Load storyboard file"),
                             NULL);
    }
    else
    {
      gimp_help_set_help_data (button,
                             _("Load cliplist file"),
                             NULL);
    }
    gtk_widget_show (button);

    /*  The Save button  */
    button = gap_gtk_button_new_from_stock_icon (GTK_STOCK_SAVE );
    tabw->save_button = button;
    gtk_box_pack_start (GTK_BOX (hbox2), button, FALSE, FALSE, 0);
    if(tabw->type == GAP_STB_MASTER_TYPE_STORYBOARD)
    {
      g_signal_connect (G_OBJECT (button), "clicked",
                    G_CALLBACK (p_menu_stb_save_cb),
                    tabw->sgpp);
      gimp_help_set_help_data (button,
                             _("Save storyboard to file"),
                             NULL);
    }
    else
    {
      g_signal_connect (G_OBJECT (button), "clicked",
                    G_CALLBACK (p_menu_cll_save_cb),
                    tabw->sgpp);
      gimp_help_set_help_data (button,
                             _("Save cliplist to file"),
                             NULL);
    }
    gtk_widget_show (button);
  }

  /* Section Combo Box */
  {
    GtkWidget *combo;

    combo = gtk_combo_box_new_text();
    tabw->sections_combo_elem_count = 0;
    tabw->sections_combo = combo;
    gtk_box_pack_start (GTK_BOX (hbox2), combo, FALSE, FALSE, 2);
    gtk_widget_show (combo);

    p_set_strings_for_section_combo(tabw);

    g_signal_connect (G_OBJECT (combo), "changed",
                    G_CALLBACK (p_section_combo_changed_cb),
                    tabw);
  }

  /*  The Section Properties button  */
  button = gap_gtk_button_new_from_stock_icon (GTK_STOCK_PROPERTIES );
  //tabw->edit_story_properties_button = button;
  gtk_box_pack_start (GTK_BOX (hbox2), button, FALSE, FALSE, 0);
  g_signal_connect (G_OBJECT (button), "clicked",
                  G_CALLBACK (p_tabw_section_properties_cut_cb),
                  tabw);
  gimp_help_set_help_data (button,
                           _("Show Section properties window"),
                           NULL);
  gtk_widget_show (button);

  /* Track label */
  label = gtk_label_new (_("Track:"));
  gtk_box_pack_start (GTK_BOX (hbox2), label, FALSE, FALSE, 2);
  gtk_widget_show (label);

  /* video track spinbutton */
  spinbutton_adj = gtk_adjustment_new ( tabw->vtrack
                                       , 0   /* min */
                                       , GAP_STB_MAX_VID_TRACKS -1   /* max */
                                       , 1, 10, 0);
  spinbutton = gtk_spin_button_new (GTK_ADJUSTMENT (spinbutton_adj), 1, 0);
  tabw->vtrack_spinbutton_adj = spinbutton_adj;
  tabw->vtrack_spinbutton = spinbutton;
  gtk_widget_show (spinbutton);
  gtk_box_pack_start (GTK_BOX (hbox2), spinbutton, FALSE, FALSE, 2);

  gimp_help_set_help_data (spinbutton, _("Video Track Number (0 refers to mask definition track)"), NULL);
  g_signal_connect (G_OBJECT (spinbutton), "value_changed",
                  G_CALLBACK (p_vtrack_spinbutton_cb),
                  tabw);



  /*  The Undo button  */
  button = gap_gtk_button_new_from_stock_icon (GTK_STOCK_UNDO );
  tabw->undo_button = button;
  gtk_box_pack_start (GTK_BOX (hbox2), button, FALSE, FALSE, 0);
  g_signal_connect (G_OBJECT (button), "clicked",
                  G_CALLBACK (p_tabw_undo_cb),
                  tabw);
  gimp_help_set_help_data (button, _("UNDO"),  NULL);
  gtk_widget_show (button);

  /*  The Redo button  */
  button = gap_gtk_button_new_from_stock_icon (GTK_STOCK_REDO );
  tabw->redo_button = button;
  gtk_box_pack_start (GTK_BOX (hbox2), button, FALSE, FALSE, 0);
  g_signal_connect (G_OBJECT (button), "clicked",
                  G_CALLBACK (p_tabw_redo_cb),
                  tabw);
  gimp_help_set_help_data (button, _("REDO"),  NULL);
  gtk_widget_show (button);


  /*  The Cut button  */
  button = gap_gtk_button_new_from_stock_icon (GTK_STOCK_CUT );
  tabw->edit_cut_button = button;
  gtk_box_pack_start (GTK_BOX (hbox2), button, FALSE, FALSE, 0);
  g_signal_connect (G_OBJECT (button), "clicked",
                  G_CALLBACK (p_tabw_edit_cut_cb),
                  tabw);
  gimp_help_set_help_data (button,
                           _("Cut a clip"),
                           NULL);
  gtk_widget_show (button);

  /*  The Copy button  */
  button = gap_gtk_button_new_from_stock_icon (GTK_STOCK_COPY );
  tabw->edit_copy_button = button;
  gtk_box_pack_start (GTK_BOX (hbox2), button, FALSE, FALSE, 0);
  g_signal_connect (G_OBJECT (button), "clicked",
                  G_CALLBACK (p_tabw_edit_copy_cb),
                  tabw);
  gimp_help_set_help_data (button,
                           _("Copy a clip"),
                           NULL);
  gtk_widget_show (button);

  /*  The Paste button  */
  button = gap_gtk_button_new_from_stock_icon (GTK_STOCK_PASTE );
  tabw->edit_paste_button = button;
  gtk_box_pack_start (GTK_BOX (hbox2), button, FALSE, FALSE, 0);
  g_signal_connect (G_OBJECT (button), "clicked",
                  G_CALLBACK (p_tabw_edit_paste_cb),
                  tabw);
  gimp_help_set_help_data (button,
                           _("Paste a clip after last (selected) element"),
                           NULL);
  gtk_widget_show (button);

  /*  The New Clip button  */
  button = gap_gtk_button_new_from_stock_icon (GTK_STOCK_NEW );
  tabw->new_clip_button = button;
  gtk_box_pack_start (GTK_BOX (hbox2), button, FALSE, FALSE, 0);
  g_signal_connect (G_OBJECT (button), "button_press_event",
                  G_CALLBACK (p_tabw_new_clip_cb),
                  tabw);
  gimp_help_set_help_data (button,
                           _("Create new clip\n"
                             "(SHIFT create transition\n"
                             "CTRL create section clip)"),
                           NULL);
  gtk_widget_show (button);




  /*  The Play Button */
  button = gap_gtk_button_new_from_stock_icon (GAP_STOCK_PLAY);
  tabw->play_button = button;
  gtk_box_pack_start (GTK_BOX (hbox2), button, FALSE, FALSE, 0);
  gimp_help_set_help_data (button,
                           _("Play selected clips\n"
                             "SHIFT: Playback all clips of current track.\n"
                             "CTRL: Play composite video (all tracks)"),
                           NULL);
  if(tabw->type == GAP_STB_MASTER_TYPE_STORYBOARD)
  {
    g_signal_connect (G_OBJECT (button), "button_press_event",
                    G_CALLBACK (p_player_stb_mode_cb),
                    sgpp);
  }
  else
  {
    g_signal_connect (G_OBJECT (button), "button_press_event",
                    G_CALLBACK (p_player_cll_mode_cb),
                    sgpp);
  }
  gtk_widget_show (button);

  /*  The filename entry */
  entry = gtk_entry_new();
  gtk_box_pack_start (GTK_BOX (hbox), entry, TRUE, TRUE, 0);
  gtk_entry_set_text(GTK_ENTRY(entry), tabw->filename_refptr);
  g_signal_connect(G_OBJECT(entry), "changed",
                 G_CALLBACK (p_tabw_filename_entry_cb),
                 tabw);
  tabw->filename_entry = entry;
  /*  gtk_widget_show (entry); */



  /* Row label */
  label = gtk_label_new (_("Row:"));
  gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 2);
  gtk_widget_hide (label);

  /* rowpage spinbutton */
  spinbutton_adj = gtk_adjustment_new ( tabw->rowpage
                                       , 1   /* min */
                                       , 1   /* max */
                                       , 1, 10, 0);
  tabw->rowpage_spinbutton_adj = spinbutton_adj;
  spinbutton = gtk_spin_button_new (GTK_ADJUSTMENT (spinbutton_adj), 1, 0);
  gtk_widget_hide (spinbutton);
  gtk_box_pack_start (GTK_BOX (hbox), spinbutton, FALSE, FALSE, 2);

  gimp_help_set_help_data (spinbutton, _("Top rownumber"), NULL);
  g_signal_connect (G_OBJECT (spinbutton), "value_changed",
                  G_CALLBACK (p_rowpage_spinbutton_cb),
                  tabw);

  /* of label */
  label = gtk_label_new (_("of:"));
  gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 2);
  gtk_widget_hide (label);

  /* total rows label */
  label = gtk_label_new ("1");
  gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 2);
  gtk_widget_show (label);
  tabw->total_rows_label = label;


  /* rowpage vscale */
  adj = gtk_adjustment_new ( tabw->rowpage
                                       , 1   /* min */
                                       , 1   /* max */
                                       , 1, 10, 0);
  vscale = gtk_vscrollbar_new (GTK_ADJUSTMENT(adj));

  gtk_range_set_update_policy (GTK_RANGE (vscale), GTK_UPDATE_DELAYED); /* GTK_UPDATE_CONTINUOUS */

  gtk_table_attach (GTK_TABLE (tabw->mount_table)
                  , vscale
                  , mount_vs_col, mount_vs_col+1
                  , mount_vs_row, mount_vs_row+1
                  , (GtkAttachOptions) (0)
                  , (GtkAttachOptions) (GTK_FILL)
                  , 0, 0);
  g_signal_connect (adj, "value_changed",
                    (GtkSignalFunc)p_vscale_changed_cb,
                    tabw);
  /* startup with invisble vscale */
  gtk_widget_hide (vscale);

  tabw->rowpage_vscale_adj = adj;
  tabw->rowpage_vscale = vscale;

  /* setup the hbox that contains the button bar
   * as drop destination.
   * (for inserting clips at end of storyboard, and for
   * enable drop destination in case when th storyboard is empty
   * and has not frame widget that can act as DND target).
   */
  p_widget_init_dnd_drop(hbox, tabw);

  return(hbox);
}  /* end p_create_button_bar */

/* --------------------------------
 * gap_gtk_button_new_from_stock_icon
 * --------------------------------
 * create a  button from stock, using only the icon
 * (the text assotiated with the stock id is not rendered,
 * to keep the button width small)
 */
GtkWidget * gap_gtk_button_new_from_stock_icon(const char *stock_id)
{
  GtkWidget *button;
  GtkWidget *image;
  image = gtk_image_new_from_stock (stock_id,
                                    GTK_ICON_SIZE_BUTTON);

  gtk_widget_show (image);
  button = gtk_button_new();
  gtk_container_add (GTK_CONTAINER (button), image);
  return(button);

}  /* end gap_gtk_button_new_from_stock_icon */




/* ---------------------------------
 * p_get_gimprc_preview_size
 * ---------------------------------
 */
static gint32
p_get_gimprc_preview_size(const char *gimprc_option_name)
{
  char *value_string;
  gint32 preview_size;

  preview_size = STB_THSIZE_MEDIUM;  /* default preview size if nothing is configured */
  value_string = gimp_gimprc_query(gimprc_option_name);

  if(value_string)
  {
     if (strcmp (value_string, "small") == 0)
     {
        preview_size = STB_THSIZE_SMALL;
     }
     else if (strcmp (value_string, "medium") == 0)
     {
        preview_size = STB_THSIZE_MEDIUM;
     }
      else if (strcmp (value_string, "large") == 0)
     {
        preview_size = STB_THSIZE_LARGE;
     }
     else
     {
        preview_size = atol(value_string);
     }

     g_free(value_string);
  }


  /* limit to useable values (in case gimrc value was specified as integer number) */
  if (preview_size < STB_THSIZE_MIN)
  {
    preview_size = STB_THSIZE_MIN;
  }
  if (preview_size > STB_THSIZE_MAX)
  {
    preview_size = STB_THSIZE_MAX;
  }

  return (preview_size);
}  /* p_get_gimprc_preview_size */


/* ---------------------------------
 * p_save_gimprc_preview_size
 * ---------------------------------
 */
static void
p_save_gimprc_preview_size(const char *gimprc_option_name, gint32 preview_size)
{
  char *value_string;

  value_string = NULL;

  switch(preview_size)
  {
    case STB_THSIZE_SMALL:
      value_string = g_strdup_printf("small");
      break;
    case STB_THSIZE_MEDIUM:
      value_string = g_strdup_printf("medium");
      break;
    case STB_THSIZE_LARGE:
      value_string = g_strdup_printf("large");
      break;
    default:
      value_string = g_strdup_printf("%d", preview_size);
      break;

  }
  gimp_gimprc_set(gimprc_option_name, value_string);
  g_free(value_string);

}  /* p_save_gimprc_preview_size */

/* ---------------------------------
 * p_save_gimprc_int_value
 * ---------------------------------
 */
static void
p_save_gimprc_int_value(const char *gimprc_option_name, gint32 value)
{
  char *value_string;

  value_string = g_strdup_printf("%d", (int)value);
  gimp_gimprc_set(gimprc_option_name, value_string);
  g_free(value_string);

}  /* p_save_gimprc_int_value */

/* ---------------------------------
 * p_save_gimprc_gboolean_value
 * ---------------------------------
 */
static void
p_save_gimprc_gboolean_value(const char *gimprc_option_name, gboolean value)
{
  if(value)
  {
    gimp_gimprc_set(gimprc_option_name, "yes");
  }
  else
  {
    gimp_gimprc_set(gimprc_option_name, "no");
  }

}  /* p_save_gimprc_gboolean_value */

/* ---------------------------------
 * p_save_gimprc_layout_settings
 * ---------------------------------
 */
static void
p_save_gimprc_layout_settings(GapStbMainGlobalParams *sgpp)
{
  p_save_gimprc_preview_size("video-cliplist-thumbnail_size", sgpp->cll_thumbsize);
  p_save_gimprc_int_value("video-cliplist-columns", sgpp->cll_cols);
  p_save_gimprc_int_value("video-cliplist-rows", sgpp->cll_rows);

  p_save_gimprc_preview_size("video-storyboard-thumbnail_size", sgpp->stb_thumbsize);
  p_save_gimprc_int_value("video-storyboard-columns", sgpp->stb_cols);
  p_save_gimprc_int_value("video-storyboard-rows", sgpp->stb_rows);

  p_save_gimprc_gboolean_value("video-storyboard-force-aspect-playback", sgpp->force_stb_aspect);


}  /* end p_save_gimprc_layout_settings */


/* ---------------------------------
 * p_get_gimprc_layout_settings
 * ---------------------------------
 */
static void
p_get_gimprc_layout_settings(GapStbMainGlobalParams *sgpp)
{
  sgpp->cll_thumbsize = p_get_gimprc_preview_size("video-cliplist-thumbnail_size");
  sgpp->cll_cols = gap_base_get_gimprc_int_value("video-cliplist-columns"
                    , sgpp->cll_cols
                    , CLL_MIN_COL
                    , CLL_MAX_COL
                    );
  sgpp->cll_rows = gap_base_get_gimprc_int_value("video-cliplist-rows"
                    , sgpp->cll_rows
                    , CLL_MIN_ROW
                    , CLL_MAX_ROW
                    );

  sgpp->stb_thumbsize = p_get_gimprc_preview_size("video-storyboard-thumbnail_size");
  sgpp->stb_cols = gap_base_get_gimprc_int_value("video-storyboard-columns"
                    , sgpp->stb_cols
                    , STB_MIN_COL
                    , STB_MAX_COL
                    );
  sgpp->stb_rows = gap_base_get_gimprc_int_value("video-storyboard-rows"
                    , sgpp->stb_rows
                    , STB_MIN_ROW
                    , STB_MAX_ROW
                    );

  sgpp->force_stb_aspect = gap_base_get_gimprc_gboolean_value("video-storyboard-force-aspect-playback"
                    , sgpp->force_stb_aspect
                    );

}  /* end p_get_gimprc_layout_settings */


/* ---------------------------------
 * p_save_gimprc_render_settings
 * ---------------------------------
 * Save global storyboard render settings as gimprc parameters.
 */
static void
p_save_gimprc_render_settings(GapStbMainGlobalParams *sgpp)
{
  p_save_gimprc_int_value("gap_ffetch_max_img_cache_elements", sgpp->ffetch_max_img_cache_elements);
  p_save_gimprc_int_value(GAP_GIMPRC_VIDEO_STORYBOARD_MAX_OPEN_VIDEOFILES, sgpp->stb_max_open_videofile);
  p_save_gimprc_int_value(GAP_GIMPRC_VIDEO_STORYBOARD_FCACHE_SIZE_PER_VIDEOFILE, sgpp->stb_fcache_size_per_videofile);
  p_save_gimprc_int_value(GAP_GIMPRC_VIDEO_STORYBOARD_RESOURCE_LOG_INTERVAL, sgpp->stb_resource_log_linterval);
  p_save_gimprc_gboolean_value(GAP_GIMPRC_VIDEO_STORYBOARD_PREVIEW_RENDER_FULL_SIZE, sgpp->stb_preview_render_full_size);
  p_save_gimprc_gboolean_value(GAP_GIMPRC_VIDEO_STORYBOARD_MULTIPROCESSOR_ENABLE, sgpp->stb_isMultithreadEnabled);

}  /* end p_save_gimprc_render_settings */


/* ---------------------------------
 * p_get_gimprc_render_settings
 * ---------------------------------
 */
static void
p_get_gimprc_render_settings(GapStbMainGlobalParams *sgpp)
{

  sgpp->ffetch_max_img_cache_elements  = gap_base_get_gimprc_int_value("gap_ffetch_max_img_cache_elements"
                    ,DEFAULT_IMG_CACHE_ELEMENTS
                    ,MIN_IMG_CACHE_ELEMENTS
                    ,MAX_IMG_CACHE_ELEMENTS
                    );
  sgpp->stb_max_open_videofile  = gap_base_get_gimprc_int_value(GAP_GIMPRC_VIDEO_STORYBOARD_MAX_OPEN_VIDEOFILES
                    ,DEFAULT_OPEN_VIDEOFILES
                    ,MIN_OPEN_VIDEOFILES
                    ,MAX_OPEN_VIDEOFILES
                    );

  sgpp->stb_fcache_size_per_videofile  = gap_base_get_gimprc_int_value(GAP_GIMPRC_VIDEO_STORYBOARD_FCACHE_SIZE_PER_VIDEOFILE
                    ,DEFAULT_FCACHE_PER_VIDEO
                    ,MIN_FCACHE_PER_VIDEO
                    ,MAX_FCACHE_PER_VIDEO
                    );

  sgpp->stb_resource_log_linterval  = gap_base_get_gimprc_int_value(GAP_GIMPRC_VIDEO_STORYBOARD_RESOURCE_LOG_INTERVAL
                    ,0  /* default */
                    ,0  /* min */
                    ,100000  /* max */
                    );


  sgpp->stb_preview_render_full_size  = gap_base_get_gimprc_gboolean_value(
                    GAP_GIMPRC_VIDEO_STORYBOARD_PREVIEW_RENDER_FULL_SIZE
                    ,FALSE  /* default */
                    );

  sgpp->stb_isMultithreadEnabled = gap_story_isMultiprocessorSupportEnabled();

}  /* end p_get_gimprc_render_settings */

/* ---------------------------------
 * p_reset_progress_bars
 * ---------------------------------
 */
static void
p_reset_progress_bars(GapStbMainGlobalParams *sgpp)
{
  if(sgpp)
  {
    if(sgpp->progress_bar_master)
    {
      gtk_progress_bar_set_text(GTK_PROGRESS_BAR(sgpp->progress_bar_master), "");
      gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(sgpp->progress_bar_master), 0);
    }
    if(sgpp->progress_bar_sub)
    {
      gtk_progress_bar_set_text(GTK_PROGRESS_BAR(sgpp->progress_bar_sub), "");
      gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(sgpp->progress_bar_sub), 0);
    }
  }
}  /* end p_reset_progress_bars */


/* --------------------------------------
 * p_storyboard_init_from_creation_params
 * --------------------------------------
 * creates an inital storyboard that includes one inital clip
 * from the specified GapStbCreationParams.
 */
static void
p_storyboard_init_from_creation_params(
     GapStbTabWidgets *tabw
   , GapStbMainGlobalParams *sgpp
   , GapStoryBoard *stb
   , GapStbCreationParams  *scrp
   )
{
    GapStoryRecordType record_type;

    stb->master_width = scrp->vid_width;
    stb->master_height = scrp->vid_height;
    stb->master_framerate = scrp->framerate;

    stb->master_volume = 1.0;
    stb->master_samplerate = scrp->samplerate;

    stb->master_aspect_ratio = scrp->aspect_ratio;  /* 0.0 for none */
    stb->master_aspect_width = scrp->aspect_width;
    stb->master_aspect_height = scrp->aspect_height;


    if(gap_debug)
    {
      printf("p_storyboard_init_from_creation_params: storyboard_filename:%s\n"
        ,scrp->storyboard_filename
        );
    }
    
    stb->storyboardfile = NULL;
    if(scrp->storyboard_filename[0] != '\0')
    {
      /* replace the internal copy of the name in the GapStoryBoard struct */
      stb->storyboardfile = g_strdup(scrp->storyboard_filename);
      if(tabw->filename_refptr)
      {
        g_snprintf(tabw->filename_refptr
              , tabw->filename_maxlen
              , "%s"
              , scrp->storyboard_filename
              );
      }
    }




    if (scrp->preferred_decoder[0] != '\0')
    {
      stb->preferred_decoder = g_strdup(scrp->preferred_decoder);
    }

     /* refresh storyboard layout and thumbnail list widgets */
     p_recreate_tab_widgets( stb
                            ,tabw
                            ,tabw->mount_col
                            ,tabw->mount_row
                            ,sgpp
                            );
     p_render_all_frame_widgets(tabw);
     p_widget_sensibility(sgpp);

     /* creation of one inital clip */

     switch (scrp->record_type_int)
     {
       case 0:
         record_type = GAP_STBREC_VID_MOVIE;
         break;
       case 1:
         record_type = GAP_STBREC_VID_IMAGE;
         break;
       case 2:
         record_type = GAP_STBREC_VID_FRAMES;
         break;
       case 3:
         record_type = GAP_STBREC_VID_ANIMIMAGE;
         break;
       default:
         record_type = GAP_STBREC_VID_UNKNOWN;
         break;
     }

     if (record_type != GAP_STBREC_VID_UNKNOWN)
     {
       GapStoryElem      *stb_elem;

       stb_elem = gap_story_new_elem(record_type);
       if(stb_elem)
       {
         stb_elem->orig_filename = g_strdup(scrp->filename);
         stb_elem->track = tabw->vtrack;
         stb_elem->from_frame = scrp->from_frame;
         stb_elem->to_frame = scrp->to_frame;
         stb_elem->nloop = scrp->nloop;
         stb_elem->nframes = (1 + abs(scrp->to_frame - scrp->from_frame) ) * scrp->nloop;

         stb_elem->seltrack     = scrp->vidtrack;
         stb_elem->exact_seek   = scrp->exact_seek;
         stb_elem->delace       = 0.0;
         if (scrp->delace_mode != 0)
         {
           stb_elem->delace = scrp->delace_mode + CLAMP(scrp->delace_threshold, 0.0, 0.999999);
         }

         if (scrp->preferred_decoder[0] != '\0')
         {
           stb_elem->preferred_decoder = g_strdup(scrp->preferred_decoder);
         }



         gap_stb_undo_group_begin(tabw);
         gap_stb_undo_push(tabw, GAP_STB_FEATURE_CREATE_CLIP);

         gap_story_list_append_elem(stb, stb_elem);

         /* refresh storyboard layout and thumbnail list widgets */
         p_recreate_tab_widgets( stb
                                 ,tabw
                                 ,tabw->mount_col
                                 ,tabw->mount_row
                                 ,sgpp
                                 );
         p_render_all_frame_widgets(tabw);
         // gap_story_stb_elem_properties_dialog(tabw, stb_elem, stb);
         gap_stb_undo_group_end(tabw);
       }

       p_tabw_update_frame_label_and_rowpage_limits(tabw, sgpp);

     }


}  /* end p_storyboard_new_from_creation_params */



/* -----------------------------
 * gap_storyboard_dialog
 * -----------------------------
 */
void
gap_storyboard_dialog(GapStbMainGlobalParams *sgpp, GapStbCreationParams  *scrp)
{
  GtkWidget *dialog;
  GtkWidget  *vbox;
  GtkWidget  *hbox;
  GtkWidget  *stb_frame;
  GtkWidget  *stb_vbox;

  GtkWidget  *clp_frame;
  GtkWidget  *clp_vbox;
  GtkWidget  *player_frame;
  GtkWidget  *table;
  GtkWidget  *menu_bar;
  GtkWidget  *frame_event_box;
  GtkWidget  *hbox_stb_frame;

  /* Init UI  */
  gimp_ui_init ("storyboard", FALSE);
  gap_stock_init();

  /*  The Storyboard dialog  */
  sgpp->run = FALSE;
  sgpp->curr_selection = NULL;

  sgpp->video_list = NULL;
  sgpp->vthumb_list = NULL;
  sgpp->gvahand = NULL;
  sgpp->gva_videofile = NULL;
  sgpp->progress_bar_master = NULL;
  sgpp->progress_bar_sub = NULL;
  sgpp->gva_lock = FALSE;
  sgpp->vthumb_prefetch_in_progress = GAP_VTHUMB_PREFETCH_NOT_ACTIVE;
  sgpp->auto_vthumb = FALSE;
  sgpp->auto_vthumb = FALSE;
  sgpp->auto_vthumb_refresh_canceled = FALSE;

  sgpp->cll_widgets = NULL;
  sgpp->menu_item_cll_save = NULL;
  sgpp->menu_item_cll_save_as = NULL;
  sgpp->menu_item_cll_add_clip = NULL;
  sgpp->menu_item_cll_add_section_clip = NULL;
  sgpp->menu_item_cll_att_properties = NULL;
  sgpp->menu_item_cll_playback = NULL;
  sgpp->menu_item_cll_properties = NULL;
  sgpp->menu_item_cll_undo = NULL;
  sgpp->menu_item_cll_redo = NULL;
  sgpp->menu_item_cll_close = NULL;

  sgpp->stb_widgets = NULL;
  sgpp->menu_item_stb_save = NULL;
  sgpp->menu_item_stb_save_as = NULL;
  sgpp->menu_item_stb_add_clip = NULL;
  sgpp->menu_item_stb_add_section_clip = NULL;
  sgpp->menu_item_stb_att_properties = NULL;
  sgpp->menu_item_stb_playback = NULL;
  sgpp->menu_item_stb_properties = NULL;
  sgpp->menu_item_stb_undo = NULL;
  sgpp->menu_item_stb_redo = NULL;
  sgpp->menu_item_stb_close = NULL;

  sgpp->win_prop_dlg_open = FALSE;
  sgpp->render_prop_dlg_open = FALSE;
  sgpp->dnd_pixbuf = NULL;

  /* get layout settings from gimprc
   * (keeping initial values if no layout settings available in gimprc)
   */
  p_get_gimprc_layout_settings(sgpp);
  /* get render settings from gimprc
   * (using default values if no layout settings available in gimprc)
   */
  p_get_gimprc_render_settings(sgpp);

  /*  The dialog and main vbox  */
  /* the help_id is passed as NULL to avoid creation of the HELP button
   * (the Help Button would be the only button in the action area and results
   *  in creating an extra row
   *  additional note: the Storyboard dialog provides
   *  Help via Menu-Item
   */
// // NO longer use gimp_dialog_new because the window gets no minimize widget when created this way !
// //
//   dialog = gimp_dialog_new (_("Storyboard"), "storyboard",
//                                NULL, 0,
//                                gimp_standard_help_func, NULL, /* GAP_STORYBOARD_EDIT_HELP_ID */
//                                NULL);
//   g_signal_connect (G_OBJECT (dialog), "response",
//                     G_CALLBACK (story_dlg_response),
//                     sgpp);
//   /* the vbox */
//   vbox = gtk_vbox_new (FALSE, 0);
//   // gtk_container_set_border_width (GTK_CONTAINER (vbox), 2);
//   gtk_box_pack_start (GTK_BOX (GTK_DIALOG (dialog)->vbox), vbox,
//                       TRUE, TRUE, 0);
//   gtk_widget_show (vbox);

 {
   gimp_ui_init ("storyboard", TRUE);

   dialog = gtk_window_new (GTK_WINDOW_TOPLEVEL);
   gtk_window_set_title (GTK_WINDOW (dialog), _("Storyboard"));
   gtk_window_set_role (GTK_WINDOW (dialog), "storybord-editor");

   g_object_set_data (G_OBJECT (dialog), "sgpp"
                          , (gpointer)sgpp);

   g_signal_connect (dialog, "destroy",
                     G_CALLBACK (story_dlg_destroy),
                     sgpp);

   gimp_help_connect (dialog, gimp_standard_help_func, GAP_STORY_PLUG_IN_PROC, NULL);

   /* the vbox */
   vbox = gtk_vbox_new (FALSE, 0);
   gtk_container_add (GTK_CONTAINER (dialog), vbox);
   gtk_widget_show (vbox);
 }


  sgpp->shell_window = dialog;



  /* the menu_bar */
  menu_bar = gtk_menu_bar_new();
  gtk_box_pack_start(GTK_BOX(vbox), menu_bar, FALSE, TRUE, 0);

  p_make_menu_global(sgpp, menu_bar);
  p_make_menu_cliplist(sgpp, menu_bar);
  p_make_menu_storyboard(sgpp, menu_bar);
  p_make_menu_help(sgpp, menu_bar);

  gtk_widget_show(menu_bar);

  /* the hbox */
  hbox = gtk_hbox_new (FALSE, 2);
  gtk_container_set_border_width (GTK_CONTAINER (hbox), 2);
  gtk_box_pack_start (GTK_BOX (vbox), hbox, TRUE, TRUE, 0);
  gtk_widget_show (hbox);


  /* XXXXXXXXXXX Start of the CLIPLIST widgets  XXXXXXXXXXXX */


  /* the frame_event_box (to catch key events) */
  frame_event_box = gtk_event_box_new ();
  gtk_widget_show (frame_event_box);
  gtk_widget_set_events (frame_event_box
                        , GDK_KEY_PRESS_MASK | GDK_KEY_RELEASE_MASK);

  /* the clp_frame */
  clp_frame = gimp_frame_new ( _("Cliplist") );
  gtk_frame_set_shadow_type (GTK_FRAME (clp_frame) ,GTK_SHADOW_ETCHED_IN);
  gtk_container_add (GTK_CONTAINER (frame_event_box ), clp_frame);

  gtk_box_pack_start (GTK_BOX (hbox), frame_event_box, TRUE, TRUE, 0);
  gtk_widget_show (clp_frame);

  /* the clp_vbox */
  clp_vbox = gtk_vbox_new (FALSE, 2);
  gtk_container_set_border_width (GTK_CONTAINER (clp_vbox), 2);
  gtk_container_add (GTK_CONTAINER (clp_frame), clp_vbox);
  gtk_widget_show (clp_vbox);

  /* mount_table for the cliplist table */
  table = gtk_table_new (2, 2, FALSE);
  gtk_widget_show (table);
  gtk_box_pack_start (GTK_BOX (clp_vbox), table, TRUE, TRUE, 0);

  /* init Table widgets (tabw struct for cliplist) */
  sgpp->cll_widgets = p_new_stb_tab_widgets(sgpp, GAP_STB_MASTER_TYPE_CLIPLIST);
  sgpp->cll_widgets->mount_table = table;
  sgpp->cll_widgets->frame_with_name = clp_frame;

  g_signal_connect (G_OBJECT (frame_event_box), "key_press_event",
                    G_CALLBACK (p_tabw_key_press_event_cb),
                    sgpp->cll_widgets);
  g_signal_connect (G_OBJECT (frame_event_box), "scroll_event",
                    G_CALLBACK (p_tabw_scroll_event_cb),
                    sgpp->cll_widgets);


  /* create CLIPLIST Table widgets */
  p_recreate_tab_widgets( sgpp->cll
                         ,sgpp->cll_widgets
                         ,0    /* mount_col */
                         ,0    /* mount_row */
                         ,sgpp
                         );

  /* button bar */
  p_create_button_bar(sgpp->cll_widgets
                         ,0    /* mount_col */
                         ,1    /* mount_row */
                         ,sgpp
                         ,1    /* mount_vs_col */
                         ,0    /* mount_vs_row */
                         );


  /* XXXXXXXXXXX Player Frame  XXXXXXXXXXXX */
  /* the player_frame */
  player_frame = gimp_frame_new ( _("Playback") );
  gtk_frame_set_shadow_type (GTK_FRAME (player_frame) ,GTK_SHADOW_ETCHED_IN);
  gtk_box_pack_start (GTK_BOX (hbox), player_frame, TRUE, TRUE, 0);
  gtk_widget_show (player_frame);
  sgpp->player_frame = player_frame;

  /* XXXXXXXXXXX Start of the STORYBOARD widgets  XXXXXXXXXXXX */


  /* the frame_event_box (to catch key events) */
  frame_event_box = gtk_event_box_new ();
  gtk_widget_show (frame_event_box);
  gtk_widget_set_events (frame_event_box
                        , GDK_KEY_PRESS_MASK | GDK_KEY_RELEASE_MASK);

  /* the stb_frame */
  stb_frame = gimp_frame_new ( _("Storyboard") );
  gtk_frame_set_shadow_type (GTK_FRAME (stb_frame) ,GTK_SHADOW_ETCHED_IN);
  gtk_container_add (GTK_CONTAINER (frame_event_box ), stb_frame);

  gtk_box_pack_start (GTK_BOX (vbox), frame_event_box, TRUE, TRUE, 0);
  gtk_widget_show (stb_frame);

  /* the stb_vbox */
  stb_vbox = gtk_vbox_new (FALSE, 2);
  gtk_container_set_border_width (GTK_CONTAINER (stb_vbox), 2);
  gtk_container_add (GTK_CONTAINER (stb_frame), stb_vbox);
  gtk_widget_show (stb_vbox);


  /* mount_table for the storyboard table */
  table = gtk_table_new (2, 2, FALSE);
  gtk_widget_show (table);
  gtk_box_pack_start (GTK_BOX (stb_vbox), table, TRUE, TRUE, 0);

  /* init Table widgets (tabw struct for storyboard) */
  sgpp->stb_widgets = p_new_stb_tab_widgets(sgpp, GAP_STB_MASTER_TYPE_STORYBOARD);
  sgpp->stb_widgets->mount_table = table;
  sgpp->stb_widgets->frame_with_name = stb_frame;
  g_signal_connect (G_OBJECT (frame_event_box), "key_press_event",
                    G_CALLBACK (p_tabw_key_press_event_cb),
                    sgpp->stb_widgets);
  g_signal_connect (G_OBJECT (frame_event_box), "scroll_event",
                    G_CALLBACK (p_tabw_scroll_event_cb),
                    sgpp->stb_widgets);


  /* create STORYBOARD Table widgets */
  p_recreate_tab_widgets( sgpp->stb
                         ,sgpp->stb_widgets
                         ,0    /* mount_col */
                         ,0    /* mount_row */
                         ,sgpp
                         );
  /* button bar */
  hbox_stb_frame = p_create_button_bar(sgpp->stb_widgets
                         ,0    /* mount_col */
                         ,1    /* mount_row */
                         ,sgpp
                         ,1    /* mount_vs_col */
                         ,0    /* mount_vs_row */
                         );

  /* The clip target togglebutton  */
  {
    GtkWidget *togglebutton;
    gboolean initial_state;
    GtkWidget *image;

    image = gtk_image_new_from_stock (GAP_STOCK_RANGE_START, GTK_ICON_SIZE_BUTTON);
    gtk_widget_show (image);

    togglebutton = gtk_toggle_button_new ();
    gtk_container_add (GTK_CONTAINER (togglebutton), image);
    gtk_widget_show (togglebutton);
    gtk_box_pack_start (GTK_BOX (hbox_stb_frame), togglebutton, FALSE, FALSE, 0);

    initial_state = FALSE;
    if(sgpp->clip_target == GAP_STB_CLIPTARGET_STORYBOARD_APPEND)
    {
      initial_state = TRUE;
    }
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (togglebutton), initial_state);
    g_object_set_data (G_OBJECT (togglebutton), "sgpp"
                          , (gpointer)sgpp);
    g_signal_connect (G_OBJECT (togglebutton), "toggled"
                   , G_CALLBACK (p_cliptarget_togglebutton_toggled_cb)
                   , &sgpp->clip_target);
    gimp_help_set_help_data (togglebutton
                   , _("ON: clip target is storyboard (for clips created from playback range).")
                   , NULL);
  }

  /*  The Video ProgressBar */
  {
    GtkWidget *button;
    GtkWidget *progress_bar;
    GtkWidget *vbox_progress;

    vbox_progress = gtk_vbox_new (FALSE, 0);
    gtk_widget_show (vbox_progress);

    progress_bar = gtk_progress_bar_new ();
    sgpp->progress_bar_master = progress_bar;
    gtk_progress_bar_set_text(GTK_PROGRESS_BAR(progress_bar), " ");
    gtk_widget_show (progress_bar);
    gtk_box_pack_start (GTK_BOX (vbox_progress), progress_bar, TRUE, TRUE, 0);

    progress_bar = gtk_progress_bar_new ();
    sgpp->progress_bar_sub = progress_bar;
    gtk_progress_bar_set_text(GTK_PROGRESS_BAR(progress_bar), " ");
    gtk_widget_show (progress_bar);
    gtk_box_pack_start (GTK_BOX (vbox_progress), progress_bar, TRUE, TRUE, 0);


    gtk_box_pack_start (GTK_BOX (hbox_stb_frame), vbox_progress, TRUE, TRUE, 0);



    button = gtk_button_new_with_label (_("Cancel"));
    gtk_box_pack_start (GTK_BOX (hbox_stb_frame), button, FALSE, FALSE, 0);
    gtk_widget_show (button);
    gimp_help_set_help_data (button
                   , _("Cancel video access if in progress and disable automatic video thumbnails")
                   , NULL);
    g_signal_connect (G_OBJECT (button), "clicked",
                    G_CALLBACK (p_cancel_button_cb),
                    sgpp);
  }


  sgpp->initialized = TRUE;
  p_widget_sensibility(sgpp);
  gtk_widget_show (dialog);

  /* init player window */
  p_player_img_mode_cb(NULL, sgpp);

  {
    gint32 ffetch_user_id;

    /* register for frame fetcher resources (image cache)
     */
    ffetch_user_id = gap_frame_fetch_register_user("gap_storyboard_dialog");

    if (scrp != NULL)
    {
      /* create an initial cliplist according to creation parameters */
      sgpp->cll = gap_story_new_story_board(NULL);
      sgpp->cll->master_type = GAP_STB_MASTER_TYPE_CLIPLIST;
      p_storyboard_init_from_creation_params(sgpp->cll_widgets, sgpp, sgpp->cll, scrp);
    }

    gtk_main ();
    gdk_flush ();

    /* unregister (shall drop cached resources of the frame fetcher) */
    gap_frame_fetch_unregister_user(ffetch_user_id);
  }

}  /* end gap_storyboard_dialog */



/* -----------------------------------
 * p_tabw_gen_otone_dialog
 * ------------------------------------
 */
static void
p_tabw_gen_otone_dialog(GapStbTabWidgets *tabw)
{
  GapArrArg  argv[7];
  gint   l_ii;
  gint   l_ii_aud_seltrack;
  gint   l_ii_aud_track;
  gint   l_ii_replace;
  GapStbMainGlobalParams *sgpp;
  GapStoryBoard *stb_dst;
  gboolean l_rc;

  stb_dst = p_tabw_get_stb_ptr (tabw);
  if(stb_dst == NULL) { return; }
  if(tabw->otone_dlg_open)  { return; }
  sgpp = (GapStbMainGlobalParams *)tabw->sgpp;

  tabw->otone_dlg_open  = TRUE;

  l_ii = 0;
  gap_arr_arg_init(&argv[l_ii], GAP_ARR_WGT_LABEL_LEFT);
  argv[l_ii].label_txt = _("Generate original tone audio track "
                           "for all video clips in the storyboard");

  l_ii++; l_ii_aud_seltrack = l_ii;
  gap_arr_arg_init(&argv[l_ii], GAP_ARR_WGT_INT);
  argv[l_ii].constraint = TRUE;
  argv[l_ii].label_txt = _("Input Audiotrack:");
  argv[l_ii].help_txt  = _("select input audiotrack in the videofile(s).");
  argv[l_ii].int_min   = (gint)1;
  argv[l_ii].int_max   = (gint)99;
  argv[l_ii].int_ret   = (gint)1;
  argv[l_ii].has_default = TRUE;
  argv[l_ii].int_default = (gint)1;


  l_ii++; l_ii_aud_track = l_ii;
  gap_arr_arg_init(&argv[l_ii], GAP_ARR_WGT_INT);
  argv[l_ii].constraint = TRUE;
  argv[l_ii].label_txt = _("Output Audiotrack:");
  argv[l_ii].help_txt  = _("output audiotrack to be generated in the storyboard file. "
                           "The generated storyboard audiotrack will be a list of references "
                           "to the audioparts in the input videos, corresponding to all "
                           "used video clip references.");
  argv[l_ii].int_min   = (gint)1;
  argv[l_ii].int_max   = (gint)GAP_STB_MAX_AUD_TRACKS;
  argv[l_ii].int_ret   = (gint)1;
  argv[l_ii].has_default = TRUE;
  argv[l_ii].int_default = (gint)1;

  l_ii++; l_ii_replace = l_ii;
  gap_arr_arg_init(&argv[l_ii], GAP_ARR_WGT_TOGGLE);
  argv[l_ii].label_txt = _("Replace Audiotrack:");
  argv[l_ii].help_txt  = _("ON: Allow replacing of already existing audio clip references "
                        " in the storyboard");
  argv[l_ii].int_ret   = FALSE;
  argv[l_ii].has_default = TRUE;
  argv[l_ii].int_default = FALSE;


  l_ii++;
  gap_arr_arg_init(&argv[l_ii], GAP_ARR_WGT_DEFAULT_BUTTON);
  argv[l_ii].label_txt =  _("Reset");
  argv[l_ii].help_txt  = _("Reset parameters to default values");

  l_ii++;
  gap_arr_arg_init(&argv[l_ii], GAP_ARR_WGT_HELP_BUTTON);
  argv[l_ii].help_id = GAP_STB_GEN_OTONE_DLG_HELP_ID;

  l_ii++;

  l_rc = gap_arr_ok_cancel_dialog( _("Generate Original Tone Audio")
                                 , _("Settings")
                                 , l_ii
                                 , argv);

  if((l_rc) && (sgpp->shell_window))
  {
     gint     aud_seltrack;
     gint     aud_track;
     gint     vid_track;
     gboolean replace_existing_aud_track;
     gboolean l_ok;
     gdouble  l_first_non_matching_framerate;

     aud_seltrack = (gint32)(argv[l_ii_aud_seltrack].int_ret);
     aud_track    = (gint32)(argv[l_ii_aud_track].int_ret);
     vid_track    = tabw->vtrack;
     replace_existing_aud_track = (gint32)(argv[l_ii_replace].int_ret);

     gap_stb_undo_group_begin(tabw);
     gap_stb_undo_push(tabw, GAP_STB_FEATURE_GENERATE_OTONE);

     /* start otone generator procedure */
     l_ok = gap_story_gen_otone_audio(stb_dst
                         ,vid_track
                         ,aud_track
                         ,aud_seltrack
                         ,replace_existing_aud_track
                         ,&l_first_non_matching_framerate
                         );
     gap_stb_undo_group_end(tabw);

     if(!l_ok)
     {
       g_message(_("Original tone track was not created.\n"
                   "The storyboard %s\n"
                   "has already audio clip references at track %d.\n"
                   "Use another track number or allow replace at next try.")
                ,tabw->filename_refptr
                ,(int)aud_track
                );
     }
     else
     {
       if(l_first_non_matching_framerate != 0.0)
       {
         g_message(_("Original tone track was created with warnings.\n"
                   "The storyboard %s\n"
                   "has movie clips with framerate %.4f. "
                   "that is different from the master framerate %.4f.\n"
                   "The generated audio is NOT synchronized with the video.")
                ,tabw->filename_refptr
                ,(float)l_first_non_matching_framerate
                ,(float)stb_dst->master_framerate
                );
       }
     }

     /* update to reflect the modified status */
     p_tabw_update_frame_label_and_rowpage_limits (tabw, sgpp);

  }
  tabw->otone_dlg_open  = FALSE;

}  /* end p_tabw_gen_otone_dialog */



/* -----------------------------------
 * p_parse_int
 * ------------------------------------
 */
static gint32
p_parse_int(const char *buff)
{
  gint32 value;

  value  = 0;
  if(buff == NULL)
  {
  }

  while(buff != NULL)
  {
     if (*buff == '\0')
     {
       break;
     }
     if ((*buff != ' ') || (*buff != '\t') || (*buff != '\n'))
     {
       value = atol(buff);
       break;
     }
     buff++;
  }

  return (value);

}  /* end p_parse_int */


/* -----------------------------------
 * p_parse_aspect_width_and_height
 * ------------------------------------
 */
static void
p_parse_aspect_width_and_height(const char *buff, gint32 *aspect_width, gint32 *aspect_height)
{
  *aspect_width = 0;
  *aspect_height = 0;
  const char *h_ptr;

  if(buff == NULL)
  {
    return;
  }

  *aspect_width = p_parse_int(buff);

  h_ptr = buff;
  while(h_ptr != NULL)
  {
     if (*h_ptr == ':')
     {
       h_ptr++;
       *aspect_height = p_parse_int(h_ptr);
       return;
     }
     if (*h_ptr == '\0')
     {
       break;
     }
     h_ptr++;
  }

}  /* end p_parse_aspect_width_and_height */

/* -----------------------------------
 * p_tabw_master_prop_dialog
 * -----------------------------------
 */
static void
p_tabw_master_prop_dialog(GapStbTabWidgets *tabw, gboolean new_flag)
{
  GapArrArg  argv[13];
  static char *radio_args[4]  = { N_("automatic"), "libmpeg3", "libavformat", "quicktime4linux" };
  static char *radio_aspect_args[4]  = { N_("none"), "4:3", "16:9", "3:2"};
  gint   l_ii;
  gint   l_decoder_idx;
  gint   l_aspect_idx;
  gint   l_ii_width;
  gint   l_ii_height;
  gint   l_ii_framerate;
  gint   l_ii_vtrack1_is_toplayer;
  gint   l_ii_preferred_decoder;
  gint   l_ii_samplerate;
  gint   l_ii_volume;
  gint   l_ii_aspect;
  GapStbMainGlobalParams *sgpp;
  GapStoryBoard *stb_dst;
  gint32   l_master_width;
  gint32   l_master_height;
  gchar    buf_preferred_decoder[60];
  gchar    buf_aspect_string[40];
  gchar   *label_txt;
  gchar    l_master_insert_alpha_format[GAP_STORY_MAX_STORYFILENAME_LEN];
  gchar    l_master_insert_area_format[GAP_STORY_MAX_STORYFILENAME_LEN];

  gboolean l_rc;

  stb_dst = p_tabw_get_stb_ptr (tabw);
  if(stb_dst == NULL)
  {
    return;
  }
  if(tabw->master_dlg_open)
  {
    return;
  }
  sgpp = (GapStbMainGlobalParams *)tabw->sgpp;

  tabw->master_dlg_open  = TRUE;

  gap_story_get_master_pixelsize(stb_dst, &l_master_width, &l_master_height);

  label_txt = NULL;
  l_ii = 0;
  if(new_flag)
  {
    gap_arr_arg_init(&argv[l_ii], GAP_ARR_WGT_FILESEL);
    argv[l_ii].label_txt = _("Name:");
    argv[l_ii].entry_width = 250;       /* pixel */
    if(tabw == sgpp->stb_widgets)
    {
      argv[l_ii].help_txt  = _("Name of the Storyboardfile");
      argv[l_ii].text_buf_len = sizeof(sgpp->storyboard_filename);
      argv[l_ii].text_buf_ret = &sgpp->storyboard_filename[0];
    }
    else
    {
      argv[l_ii].help_txt  = _("Name of the Cliplistfile");
      argv[l_ii].text_buf_len = sizeof(sgpp->cliplist_filename);
      argv[l_ii].text_buf_ret = &sgpp->cliplist_filename[0];
    }
  }
  else
  {
    gap_arr_arg_init(&argv[l_ii], GAP_ARR_WGT_LABEL_LEFT);
    if(tabw == sgpp->stb_widgets)
    {
      label_txt = g_strdup_printf(_("Name: %s"), sgpp->storyboard_filename);
    }
    else
    {
      label_txt = g_strdup_printf(_("Name: %s"), sgpp->cliplist_filename);
    }
    argv[l_ii].label_txt = label_txt;
  }

  l_ii++; l_ii_width = l_ii;
  gap_arr_arg_init(&argv[l_ii], GAP_ARR_WGT_INT_PAIR);
  argv[l_ii].constraint = TRUE;
  argv[l_ii].label_txt = _("Width:");
  argv[l_ii].help_txt  = _("Master width of the resulting video in pixels");
  argv[l_ii].int_min   = (gint)1;
  argv[l_ii].int_max   = (gint)9999;
  argv[l_ii].int_ret   = (gint)l_master_width;
  argv[l_ii].has_default = TRUE;
  argv[l_ii].int_default = (gint)l_master_width;

  l_ii++; l_ii_height = l_ii;
  gap_arr_arg_init(&argv[l_ii], GAP_ARR_WGT_INT_PAIR);
  argv[l_ii].constraint = TRUE;
  argv[l_ii].label_txt = _("Height:");
  argv[l_ii].help_txt  = _("Master height of the resulting video in pixels)");
  argv[l_ii].int_min   = (gint)1;
  argv[l_ii].int_max   = (gint)9999;
  argv[l_ii].int_ret   = (gint)l_master_height;
  argv[l_ii].has_default = TRUE;
  argv[l_ii].int_default = (gint)l_master_height;


  l_ii++; l_ii_framerate = l_ii;
  gap_arr_arg_init(&argv[l_ii], GAP_ARR_WGT_FLT_PAIR);
  argv[l_ii].constraint = TRUE;
  argv[l_ii].label_txt = _("Framerate:");
  argv[l_ii].help_txt  = _("Framerate in frames/sec.");
  argv[l_ii].flt_min   = 0.0;
  argv[l_ii].flt_max   = 999;
  argv[l_ii].flt_step  = 0.1;
  argv[l_ii].pagestep  = 1.0;
  argv[l_ii].flt_ret   = GAP_STORY_DEFAULT_FRAMERATE;
  if(stb_dst->master_framerate > 0)
  {
    argv[l_ii].flt_ret   = stb_dst->master_framerate;
  }
  argv[l_ii].has_default = TRUE;
  argv[l_ii].flt_default = argv[l_ii].flt_ret;


  /* the toggle that controls layerstack order of videotracks */
  l_ii++; l_ii_vtrack1_is_toplayer = l_ii;
  gap_arr_arg_init(&argv[l_ii], GAP_ARR_WGT_TOGGLE);
  argv[l_ii].constraint = TRUE;
  argv[l_ii].label_txt = _("Track 1 on top:");
  argv[l_ii].help_txt  = _("ON: video track1 is Foregrond (on top). "
                           "OFF: video track 1 is on Background.");
  argv[l_ii].int_ret   = (gint)stb_dst->master_vtrack1_is_toplayer;
  argv[l_ii].has_default = TRUE;
  argv[l_ii].int_default = (gint)1;




  /* the aspect option entry widget */
  l_ii++; l_ii_aspect = l_ii;
  buf_aspect_string[0] = '\0';
  l_decoder_idx = 0;

  l_aspect_idx = 0;
  if((stb_dst->master_aspect_width == 4)
  && (stb_dst->master_aspect_height == 3))
  {
    l_aspect_idx = 1;
  }
  if((stb_dst->master_aspect_width == 16)
  && (stb_dst->master_aspect_height == 9))
  {
    l_aspect_idx = 2;
  }

  if((stb_dst->master_aspect_width != 0)
  && (stb_dst->master_aspect_height != 0))
  {
     g_snprintf(buf_aspect_string
               , sizeof(buf_aspect_string)
               , "%d:%d"
               , (int)stb_dst->master_aspect_width
               , (int)stb_dst->master_aspect_height
               );
  }

  gap_arr_arg_init(&argv[l_ii], GAP_ARR_WGT_OPT_ENTRY);
  argv[l_ii].label_txt = _("Aspect:");
  argv[l_ii].help_txt  = _("Select video frame aspect ratio; "
                       "enter a string like \"4:3\" or \"16:9\" to specify the aspect. "
                       "Enter none or leave empty if no special aspect shall be used "
                       "(in this case video frames use the master pixelsize 1:1 "
                       "for displaying video frames).");
  argv[l_ii].radio_argc  = G_N_ELEMENTS(radio_aspect_args);
  argv[l_ii].radio_argv = radio_aspect_args;
  argv[l_ii].radio_ret  = l_aspect_idx;
  argv[l_ii].has_default = TRUE;
  argv[l_ii].radio_default = 1;
  argv[l_ii].text_buf_ret = buf_aspect_string;
  argv[l_ii].text_buf_len = sizeof(buf_aspect_string);
  argv[l_ii].text_buf_default =  "4:3";

  /* the preferred decoder */
  l_ii++; l_ii_preferred_decoder = l_ii;
  buf_preferred_decoder[0] = '\0';
  l_decoder_idx = 0;
  if(stb_dst->preferred_decoder)
  {
    if(*stb_dst->preferred_decoder != '\0')
    {
      guint jj;

      g_snprintf(buf_preferred_decoder
               , sizeof(buf_preferred_decoder)
               , "%s"
               , stb_dst->preferred_decoder
               );
      for(jj=0; jj < G_N_ELEMENTS(radio_args);jj++)
      {
        if(strcmp(radio_args[jj], buf_preferred_decoder) == 0)
        {
          l_decoder_idx = jj;
          break;
        }
      }

    }

  }
  gap_arr_arg_init(&argv[l_ii], GAP_ARR_WGT_OPT_ENTRY);
  argv[l_ii].label_txt = _("Decoder:");
  argv[l_ii].help_txt  = _("Select preferred video decoder library, "
                       "or leave empty for automatic selection."
                       "The decoder setting is only relevant if "
                       "videoclips are used (but not for frames "
                       "that are imagefiles)");
  argv[l_ii].radio_argc  = G_N_ELEMENTS(radio_args);
  argv[l_ii].radio_argv = radio_args;
  argv[l_ii].radio_ret  = l_decoder_idx;
  argv[l_ii].has_default = TRUE;
  argv[l_ii].radio_default = l_decoder_idx;
  argv[l_ii].text_buf_ret = buf_preferred_decoder;
  argv[l_ii].text_buf_len = sizeof(buf_preferred_decoder);
  argv[l_ii].text_buf_default = g_strdup(buf_preferred_decoder);

  l_ii++; l_ii_samplerate = l_ii;
  gap_arr_arg_init(&argv[l_ii], GAP_ARR_WGT_INT_PAIR);
  argv[l_ii].constraint = TRUE;
  argv[l_ii].label_txt = _("Samplerate:");
  argv[l_ii].help_txt  = _("Master audio samplerate for the resulting video in samples/sec.");

  argv[l_ii].int_min   = 0;
  argv[l_ii].int_max   = 48000;
  argv[l_ii].int_ret   = GAP_STORY_DEFAULT_SAMPLERATE;
  if(stb_dst->master_samplerate > 0)
  {
    argv[l_ii].int_ret   = stb_dst->master_samplerate;
  }
  argv[l_ii].has_default = TRUE;
  argv[l_ii].int_default = argv[l_ii].int_ret;

  l_ii++; l_ii_volume = l_ii;
  gap_arr_arg_init(&argv[l_ii], GAP_ARR_WGT_FLT_PAIR);
  argv[l_ii].constraint = TRUE;
  argv[l_ii].label_txt = _("Volume:");
  argv[l_ii].help_txt  = _("Master audio volume, where 1.0 keeps original volume");
  argv[l_ii].flt_min   = 0.0;
  argv[l_ii].flt_max   = 5.0;
  argv[l_ii].flt_step  = 0.1;
  argv[l_ii].pagestep  = 1.0;
  argv[l_ii].flt_ret   = stb_dst->master_volume;
  if(stb_dst->master_volume < 0.0)
  {
    argv[l_ii].flt_ret = 1.0;
  }
  argv[l_ii].has_default = TRUE;
  argv[l_ii].flt_default = argv[l_ii].flt_ret;




  l_ii++;
  l_master_insert_alpha_format[0] = '\0';
  if (stb_dst->master_insert_alpha_format)
  {
    g_snprintf(&l_master_insert_alpha_format[0], sizeof(l_master_insert_alpha_format), "%s"
            , stb_dst->master_insert_alpha_format
            );
  }
  gap_arr_arg_init(&argv[l_ii], GAP_ARR_WGT_FILESEL);
  argv[l_ii].label_txt = _("Ext. Transparency Format:");
  argv[l_ii].entry_width = 250;       /* pixel */
  argv[l_ii].help_txt  = _("Format string to provide external tranparency in movie clips. "
                           "(e.g automatic alpha channel insertation)"
                           "this string shall contain \%s as placeholder for the basename of a videoclip and "
                           "optional \%06d as placeholder for the framenumber.");
  argv[l_ii].text_buf_len = sizeof(l_master_insert_alpha_format);
  argv[l_ii].text_buf_ret = &l_master_insert_alpha_format[0];




  l_ii++;
  l_master_insert_area_format[0] = '\0';
  if (stb_dst->master_insert_area_format)
  {
    g_snprintf(&l_master_insert_area_format[0], sizeof(l_master_insert_area_format), "%s"
            , stb_dst->master_insert_area_format
            );
  }
  gap_arr_arg_init(&argv[l_ii], GAP_ARR_WGT_FILESEL);
  argv[l_ii].label_txt = _("AreaFormat:");
  argv[l_ii].entry_width = 250;       /* pixel */
  argv[l_ii].help_txt  = _("Format string for area replacement in movie clips. (e.g automatic logo insert)"
                           "this string shall contain \%s as placeholder for the basename of a videoclip and "
                           "optional \%06d as placeholder for the framenumber.");
  argv[l_ii].text_buf_len = sizeof(l_master_insert_area_format);
  argv[l_ii].text_buf_ret = &l_master_insert_area_format[0];


  l_ii++;
  gap_arr_arg_init(&argv[l_ii], GAP_ARR_WGT_DEFAULT_BUTTON);
  argv[l_ii].label_txt =  _("Reset");                /* should use GIMP_STOCK_RESET if possible */
  argv[l_ii].help_txt  = _("Reset parameters to inital values");


  l_ii++;
  gap_arr_arg_init(&argv[l_ii], GAP_ARR_WGT_HELP_BUTTON);
  argv[l_ii].help_id = GAP_STB_MASTER_PROP_DLG_HELP_ID;



  l_ii++;

  l_rc = gap_arr_ok_cancel_dialog( _("Master Properties"),
                                 _("Settings"),
                                  G_N_ELEMENTS(argv), argv);
  if(label_txt)
  {
    g_free(label_txt);
  }
  if((l_rc) && (sgpp->shell_window))
  {
     GapStoryBoard *stb_dup;

     gap_stb_undo_push(tabw, GAP_STB_FEATURE_PROPERTIES_MASTER);

     stb_dup = gap_story_duplicate_active_and_mask_section(stb_dst);


     stb_dst->master_width = (gint32)(argv[l_ii_width].int_ret);
     stb_dst->master_height = (gint32)(argv[l_ii_height].int_ret);
     stb_dst->master_framerate = (gdouble)(argv[l_ii_framerate].flt_ret);
     stb_dst->master_vtrack1_is_toplayer = (argv[l_ii_vtrack1_is_toplayer].int_ret != 0);

     p_parse_aspect_width_and_height(buf_aspect_string
                                   , &stb_dst->master_aspect_width
                                   , &stb_dst->master_aspect_height
                                   );

     stb_dst->master_aspect_ratio = 0.0;
     if(stb_dst->master_aspect_height > 0)
     {
        stb_dst->master_aspect_ratio = (gdouble)stb_dst->master_aspect_width
                                     / (gdouble)stb_dst->master_aspect_height;
     }

     if(gap_debug)
     {
       printf("master_aspect_width: %d\n", (int)stb_dst->master_aspect_width );
       printf("master_aspect_height: %d\n", (int)stb_dst->master_aspect_height );
     }

     stb_dst->master_samplerate = (gint32)(argv[l_ii_samplerate].int_ret);
     stb_dst->master_volume = (gint32)(argv[l_ii_volume].flt_ret);
     if(*buf_preferred_decoder)
     {
       if(stb_dst->preferred_decoder)
       {
         if(strcmp(stb_dst->preferred_decoder, buf_preferred_decoder) != 0)
         {
           stb_dst->unsaved_changes = TRUE;
           g_free(stb_dst->preferred_decoder);
           stb_dst->preferred_decoder = g_strdup(buf_preferred_decoder);

           /* close the videohandle (if open)
            * this ensures that the newly selected decoder
            * can be used at the next videofile access attempt
            */
           gap_story_vthumb_close_videofile(sgpp);
         }
       }
       else
       {
           stb_dst->unsaved_changes = TRUE;
           stb_dst->preferred_decoder = g_strdup(buf_preferred_decoder);
           gap_story_vthumb_close_videofile(sgpp);
       }
     }
     else
     {
       if(stb_dst->preferred_decoder)
       {
         stb_dst->unsaved_changes = TRUE;
         g_free(stb_dst->preferred_decoder);
       }
       stb_dst->preferred_decoder = NULL;
     }


     if (l_master_insert_alpha_format[0] != '\0')
     {
       if (stb_dst->master_insert_alpha_format)
       {
         if (strcmp(stb_dst->master_insert_alpha_format, &l_master_insert_alpha_format[0]) != 0)
         {
           stb_dst->unsaved_changes = TRUE;
         }
         g_free(stb_dst->master_insert_alpha_format);
       }
       else
       {
           stb_dst->unsaved_changes = TRUE;
       }
       stb_dst->master_insert_alpha_format = g_strdup(&l_master_insert_alpha_format[0]);
     }
     else
     {
       if (stb_dst->master_insert_alpha_format)
       {
         stb_dst->unsaved_changes = TRUE;
         g_free(stb_dst->master_insert_alpha_format);
         stb_dst->master_insert_alpha_format = NULL;
       }
     }



     if (l_master_insert_area_format[0] != '\0')
     {
       if (stb_dst->master_insert_area_format)
       {
         if (strcmp(stb_dst->master_insert_area_format, &l_master_insert_area_format[0]) != 0)
         {
           stb_dst->unsaved_changes = TRUE;
         }
         g_free(stb_dst->master_insert_area_format);
       }
       else
       {
           stb_dst->unsaved_changes = TRUE;
       }
       stb_dst->master_insert_area_format = g_strdup(&l_master_insert_area_format[0]);
     }
     else
     {
       if (stb_dst->master_insert_area_format)
       {
         stb_dst->unsaved_changes = TRUE;
         g_free(stb_dst->master_insert_area_format);
         stb_dst->master_insert_area_format = NULL;
       }
     }



     /* check for further changes of master properties */
     if((stb_dst->master_width        != stb_dup->master_width)
     || (stb_dst->master_height       != stb_dup->master_height)
     || (stb_dst->master_framerate    != stb_dup->master_framerate)
     || (stb_dst->master_aspect_ratio != stb_dup->master_aspect_ratio)
     || (stb_dst->master_samplerate   != stb_dup->master_samplerate)
     || (stb_dst->master_volume       != stb_dup->master_volume)
     || (stb_dst->master_vtrack1_is_toplayer != stb_dup->master_vtrack1_is_toplayer)
     )
     {
       stb_dst->unsaved_changes = TRUE;
     }
     gap_story_free_storyboard(&stb_dup);

     /* refresh storyboard layout and thumbnail list widgets */
     p_recreate_tab_widgets( stb_dst
                                 ,tabw
                                 ,tabw->mount_col
                                 ,tabw->mount_row
                                 ,sgpp
                                 );
     p_render_all_frame_widgets(tabw);
     p_widget_sensibility(sgpp);

  }
  tabw->master_dlg_open  = FALSE;

}  /* end p_tabw_master_prop_dialog */



/* ---------------------------------
 * on_stb_elem_drag_begin
 * ---------------------------------
 */
static void
on_stb_elem_drag_begin (GtkWidget        *widget,
                    GdkDragContext   *context,
                    GapStbFrameWidget *fw)
{
  GapStbMainGlobalParams  *sgpp;
  GapStbTabWidgets *tabw;

  if(gap_debug)
  {
    printf("on_stb_elem_drag_begin FW:%d\n", (int)fw);
  }
  if (fw == NULL)                  { return; }
  if(fw->stb_elem_refptr == NULL)  { return; }

  tabw = (GapStbTabWidgets  *)fw->tabw;
  if(tabw == NULL)   { return; }

  sgpp = (GapStbMainGlobalParams *)tabw->sgpp;
  if(sgpp == NULL)   { return; }

  if(fw->pv_ptr)
  {
    if(sgpp->dnd_pixbuf)
    {
      g_object_unref(sgpp->dnd_pixbuf);
    }

    /* get the icon from (a duplicate of the) repaint buffer */
    sgpp->dnd_pixbuf = gap_pview_get_repaint_pixbuf(fw->pv_ptr);
    if(sgpp->dnd_pixbuf)
    {
      gtk_drag_set_icon_pixbuf(context
        , sgpp->dnd_pixbuf
        , -7   /* hot_x */
        , -7   /* hot_y */
        );

    }
  }
}  /* end on_stb_elem_drag_begin */



/* ---------------------------------
 * on_stb_elem_drag_get
 * ---------------------------------
 */
static void
on_stb_elem_drag_get (GtkWidget        *widget,
                    GdkDragContext   *context,
                    GtkSelectionData *selection_data,
                    guint             info,
                    guint             time,
                    GapStbFrameWidget *fw)
{
  if(gap_debug)
  {
    printf("on_stb_elem_drag_get FW:%d, info:%d\n", (int)fw, (int)info);
  }
  if (fw)
  {
      if (info == GAP_STB_TARGET_STORYBOARD_ELEM)
      {
          gtk_selection_data_set (selection_data,
                                  selection_data->target, 8,
                                  (void *)&fw, sizeof(GapStbFrameWidget *));
      }
  }
}  /* end on_stb_elem_drag_get */


/* ---------------------------------
 * on_stb_elem_drag_delete
 * ---------------------------------
 */
//static void
//on_stb_elem_drag_delete (GtkWidget        *widget,
//                    GdkDragContext   *context,
//                    GapStbFrameWidget *fw)
//{
//printf("on_stb_elem_drag_delete FW:%d\n", (int)fw);
//}  /* end on_stb_elem_drag_delete */


/* ---------------------------------
 * on_clip_elements_dropped
 * ---------------------------------
 * signal handler called when uri list
 * was dropped onto a frame widget
 *   or another widget that can act as drop destination.
 * (the thumbnail icon in a storyboard)
 * Note:
 * this implementation replaces the current selection,
 * with the elements received from the DND drop operation.
 */
static void
on_clip_elements_dropped (GtkWidget        *widget,
                   GdkDragContext   *context,
                   gint              x,
                   gint              y,
                   GtkSelectionData *selection_data,
                   guint             info,
                   guint             time)
{
  GapStoryBoard *stb_dst;
  GapStoryBoard *stb_bck_selection;
  GapStbTabWidgets *tabw;
  GapStbMainGlobalParams  *sgpp;
  GapStbFrameWidget *fw;
  GapStbFrameWidget *fw_drop;
  GapStoryBoard *stb_src;
  GapStbTabWidgets *tabw_src;
  gint32   dest_story_id;
  gboolean insert_after;
  gboolean auto_created;

  if(gap_debug)
  {
    printf("  ** >> DROP handler(COPY:%d MOVE:%d )  context->action:%d  x:%d y:%d\n"
      , (int)GDK_ACTION_COPY
      , (int)GDK_ACTION_MOVE
      , (int)context->action
      , (int)x
      , (int)y
      );
  }
  fw_drop = NULL;
  stb_src = NULL;
  tabw_src = NULL;
  insert_after = TRUE;
  auto_created = FALSE;
  dest_story_id = -2;   /* unknown id for insert after last elem */

  fw = g_object_get_data (G_OBJECT (widget)
                        , GAP_STORY_FW_PTR);

  /* check if we were invoked from a frame widget */
  if(fw != NULL)
  {
     if(fw->pv_ptr)
     {
       if(x < (fw->pv_ptr->pv_width / 2))
       {
         /* insert before fw if drop in the left part of the thumbnail preview */
         insert_after = FALSE;
       }
     }
    if(fw->stb_elem_refptr == NULL)
    {
      return;
    }
    dest_story_id = fw->stb_elem_refptr->story_id;
    tabw = (GapStbTabWidgets  *)fw->tabw;
  }
  else
  {
    /* check if we were invoked from another DND destination widget */
    tabw = (GapStbTabWidgets  *)g_object_get_data (G_OBJECT (widget)
                        , GAP_STORY_TABW_PTR);
  }


  if(tabw == NULL)   { return; }

  sgpp = (GapStbMainGlobalParams *)tabw->sgpp;
  if(sgpp == NULL)   { return; }


  /* find out the destination story board
   * (or create a new one if drop destination has none)
   */
  stb_dst = p_get_or_auto_create_storyboard(sgpp, tabw, &auto_created);
  if(stb_dst == NULL)
  {
    return;
  }

  /* save existing selection */
  stb_bck_selection = sgpp->curr_selection;

  switch (info)
  {
    case GAP_STB_TARGET_URILIST:
      /* clear all selected flags in the destination list */
      gap_story_selection_all_set(stb_dst, FALSE);

      /* replace the current selection with elements we got from DND as uri list */
      sgpp->curr_selection = p_dnd_selection_to_stb_list((const char *)selection_data->data
                               , tabw->vtrack);
      break;
    case GAP_STB_TARGET_STORYBOARD_ELEM:
      {
        GapStbFrameWidget **fw_drop_ptr;

        fw_drop_ptr = (GapStbFrameWidget **)selection_data->data;
        fw_drop = *fw_drop_ptr;
        if(gap_debug)
        {
          printf("on_clip_elements_dropped FW_DROP:%d\n", (int)fw_drop);
        }
        if (fw_drop == NULL)
        {
          return;
        }
        tabw_src = (GapStbTabWidgets *)fw_drop->tabw;
        if ((tabw_src == NULL)
        || ((tabw_src != sgpp->cll_widgets) && (tabw_src != sgpp->stb_widgets)))
        {
          /* if tabw of the droped frame widget
           * is not equal to one of stb_widgets or cll_widgets
           * assume that the sender was another application
           * which is not supported for drop type GAP_STB_TARGET_STORYBOARD_ELEM.
           */
          return;
        }
        stb_src = p_tabw_get_stb_ptr(tabw_src);
        if (auto_created)
        {
          /* clone properties from the source storyboard
           * because the stb_dst was created in this DND drop action
           */
          gap_story_set_properties_like_sample_storyboard (stb_dst, stb_src);
        }

        /* select the storyboard elem referred by fw_drop */
        if(fw_drop->stb_elem_refptr)
        {
          fw_drop->stb_elem_refptr->selected = TRUE;
        }
        p_tabw_edit_copy_cb(widget, tabw_src);

      }
      break;
    default:
      return;
  }

  if(sgpp->curr_selection != NULL)
  {

     if (gap_debug)
     {
       gap_story_debug_print_list(sgpp->curr_selection);
     }

     gap_stb_undo_group_begin(tabw);
     gap_stb_undo_push(tabw, GAP_STB_FEATURE_DND_PASTE);

     /* insert into destination list via standard paste operation */
     p_tabw_edit_paste_at_story_id (sgpp
                    , stb_dst
                    , tabw
                    , dest_story_id
                    , insert_after
                    );
    gap_stb_undo_group_end(tabw);

    if ((info == GAP_STB_TARGET_STORYBOARD_ELEM)
    && (context->action == GDK_ACTION_MOVE)
    && (tabw_src != NULL)
    && (stb_src != NULL))
    {
        /* normally delete should be done in the handler for the "drag_data_delete"
         * handler of the source widget, but this handler was NEVER called
         * in my tests.
         */
      gap_stb_undo_group_begin(tabw_src);
      gap_stb_undo_push(tabw_src, GAP_STB_FEATURE_DND_CUT);

      gap_story_selection_from_ref_list_orig_ids(
             stb_src
           , TRUE  /* sel_state */
           , sgpp->curr_selection
           );
      p_tabw_edit_cut_cb(widget, tabw_src);

      gap_stb_undo_group_end(tabw_src);
    }
  }

  if (info == GAP_STB_TARGET_URILIST)
  {
    /* restore selection as it was before the DND of uri-list caused paste */
    sgpp->curr_selection = stb_bck_selection;
  }

  if (gap_debug)
  {
    printf("  ** << DROP handler end\n\n");
  }
}  /* end on_clip_elements_dropped */


/* ---------------------------------
 * p_get_or_auto_create_storyboard
 * ---------------------------------
 * the flag auto_created is set to TRUE if auto creation was done.
 */
static GapStoryBoard *
p_get_or_auto_create_storyboard (GapStbMainGlobalParams *sgpp
  , GapStbTabWidgets *tabw
  , gboolean *auto_created)
{
  GapStoryBoard *l_stb;

  *auto_created   = FALSE;
  if(gap_debug) printf("p_get_or_auto_create_storyboard\n");
  if(tabw == NULL)   { return (NULL); }
  if(sgpp == NULL)   { return (NULL); }

  l_stb = p_tabw_get_stb_ptr(tabw);
  if(gap_debug)
  {
    printf("(STB_tabw:%d CLL_tabw:%d) TABW: %d\n"
      , (int)sgpp->stb_widgets
      , (int)sgpp->cll_widgets
      , (int)tabw
      );
    printf("(stb:%d cll:%d)stb_dst: %d\n"
      , (int)sgpp->stb
      , (int)sgpp->cll
      , (int)l_stb
      );
  }

  if(l_stb == NULL)
  {
    /* auto create a default storyboard or cliplist */
    *auto_created   = TRUE;
    l_stb = gap_story_new_story_board(_("STORY_new.txt"));
    l_stb->master_type = tabw->type;
    if(tabw->type == GAP_STB_MASTER_TYPE_CLIPLIST)
    {
      sgpp->cll = l_stb;
      g_snprintf(sgpp->cliplist_filename, sizeof(sgpp->cliplist_filename)
        , "%s", l_stb->storyboardfile);
      tabw->filename_refptr = sgpp->cliplist_filename;
      tabw->filename_maxlen = sizeof(sgpp->cliplist_filename);
    }
    else
    {
      sgpp->stb = l_stb;;
      g_snprintf(sgpp->storyboard_filename, sizeof(sgpp->storyboard_filename)
        , "%s", l_stb->storyboardfile);
      tabw->filename_refptr = sgpp->storyboard_filename;
      tabw->filename_maxlen = sizeof(sgpp->storyboard_filename);
    }


    /* refresh storyboard layout and thumbnail list widgets */
    p_recreate_tab_widgets( l_stb
                           ,tabw
                           ,tabw->mount_col
                           ,tabw->mount_row
                           ,sgpp
                           );
    p_render_all_frame_widgets(tabw);
    p_tabw_update_frame_label_and_rowpage_limits(tabw, sgpp);
    //p_widget_sensibility(sgpp);
  }

  return(l_stb);

}  /* end p_get_or_auto_create_storyboard */



/* ---------------------------------
 * p_dnd_selection_to_stb_list
 * ---------------------------------
 */
static GapStoryBoard *
p_dnd_selection_to_stb_list(const char *uri_list, gint32 vtrack)
{
  const gchar *p, *q;
  GapStoryElem  *list_root;
  GapStoryElem  *prev_elem;
  GapStoryBoard *stb_dnd;

  prev_elem = NULL;
  list_root = NULL;
  stb_dnd = NULL;

  g_return_val_if_fail (uri_list != NULL, NULL);

  p = uri_list;
  /* We don't actually try to validate the URI according to RFC
   * 2396, or even check for allowed characters - we just ignore
   * comments and trim whitespace off the ends.  We also
   * allow LF delimination as well as the specified CRLF.
   *
   * We do allow comments like specified in RFC 2483.
   */
  while (p)
  {
      if (*p != '#')
      {
          while (g_ascii_isspace(*p))
          {
            p++;
          }
          q = p;
          while (*q && (*q != '\n') && (*q != '\r'))
          {
            q++;
          }

          if (q > p)
          {
              q--;
              while (q > p && g_ascii_isspace (*q))
              {
                q--;
              }
              if (q > p)
              {
                gchar *uri;
                GapStoryElem *elem;

                uri = g_strndup (p, q - p + 1);
                elem = p_check_and_convert_uri_to_stb_elem(uri, prev_elem, vtrack);
                if (elem)
                {
                  if(list_root == NULL)
                  {
                    list_root = elem;
                  }
                  else
                  {
                    prev_elem->next = elem;
                  }
                  prev_elem = elem;

                }
                g_free(uri);
              }
          }
      }
      p = strchr (p, '\n');
      if (p)
      {
        p++;
      }
  }
  if(list_root)
  {
    GapStorySection *stb_dnd_main_section;

    stb_dnd = gap_story_new_story_board("StoryboardDND");
    stb_dnd_main_section = gap_story_find_main_section(stb_dnd);
    stb_dnd_main_section->stb_elem = list_root;
  }
  return (stb_dnd);
}  /* end p_dnd_selection_to_stb_list */


/* -----------------------------------
 * p_check_and_convert_uri_to_stb_elem
 * -----------------------------------
 * check if the specified uri refers to
 * an existing local file and convert
 * to storyboard element.
 * return the converted elem
 * return NULL in case where conversion was not possible
 *             or in case the element could be merged into prev_elem
 */
static GapStoryElem *
p_check_and_convert_uri_to_stb_elem(const char *uri, GapStoryElem *prev_elem, gint32 vtrack)
{
  GapStoryElem *elem;
  gchar *filename;
  gchar *hostname;
  const gchar *this_hostname;
  gboolean is_localfile;
  GError *error = NULL;

  elem = NULL;
  hostname = NULL;
  filename = g_filename_from_uri (uri, &hostname, &error);

  if(gap_debug)
  {
    printf("DND: p_check_and_convert_uri_to_stb_elem\nURI:%s\nfilename:%s\n"
      ,uri
      ,filename
      );
  }


  if (filename == NULL)
  {
      g_warning ("Error getting dropped filename: %s\n",
                 error->message);
      g_error_free (error);
      return (NULL);
  }

  this_hostname = g_get_host_name();

  is_localfile = FALSE;
  if (hostname == NULL)
  {
    is_localfile = TRUE;
  }
  else
  {
    if ((strcmp (hostname, this_hostname) == 0)
    ||  (strcmp (hostname, "localhost") == 0))
    {
      is_localfile = TRUE;
    }
  }


  if (is_localfile)
  {
    if(g_file_test(filename, G_FILE_TEST_EXISTS))
    {
      elem = p_create_stb_elem_from_filename(filename, vtrack);
      if (p_check_and_merge_range_if_possible(elem, prev_elem) == TRUE)
      {
         /* elem could be merged as extended range of prev_elem
          * dont need elem any longer in this case.
          */
         gap_story_elem_free(&elem);
         return (NULL);
      }
    }
  }
  return (elem);

}  /* end p_check_and_convert_uri_to_stb_elem */


/* -----------------------------------
 * p_check_and_merge_range_if_possible
 * -----------------------------------
 * check if both prev_elem and framename
 * are from type GAP_STBREC_VID_FRAMES
 * AND if elem is a frame image
 * representing the the next frame
 * in sequence of the prev_elem.
 * if this is the case, then add the frame to prev_elem
 * by incrementing the to_frame atribut by 1 and return TRUE.
 * otherwise return FALSE.
 */
static gboolean
p_check_and_merge_range_if_possible(GapStoryElem *elem, GapStoryElem *prev_elem)
{
  if (prev_elem == NULL)
  {
    return (FALSE);
  }

  if(elem->record_type != GAP_STBREC_VID_FRAMES)
  {
    return (FALSE);
  }


  if (prev_elem->record_type != GAP_STBREC_VID_FRAMES)
  {
    return (FALSE);
  }

  if ((strcmp (prev_elem->basename, prev_elem->basename) == 0)
  &&  (strcmp (prev_elem->ext, prev_elem->ext) == 0)
  &&  (elem->from_frame == prev_elem->to_frame +1))
  {
     /* extend prv_elem range to include the elem range */
     prev_elem->to_frame = elem->to_frame;
     return (TRUE);
  }

  return (FALSE);

}  /* end p_check_and_merge_range_if_possible */


/* -----------------------------------
 * p_create_stb_elem_from_filename
 * -----------------------------------
 */
static GapStoryElem *
p_create_stb_elem_from_filename (const char *filename, gint32 vtrack)
{
  GapStoryElem *stb_elem;
  long current_frame;
  if(gap_debug)
  {
    printf("p_create_stb_elem_from_filename  file:%s\n", filename);
  }

  stb_elem = gap_story_new_elem(GAP_STBREC_VID_IMAGE);
  stb_elem->orig_filename = g_strdup(filename);
  stb_elem->track = vtrack;
  stb_elem->from_frame = 1;
  stb_elem->to_frame = 1;
  stb_elem->nloop = 1;
  stb_elem->nframes = 1;
  current_frame = gap_story_upd_elem_from_filename(stb_elem, filename);
  if (current_frame > 0)
  {
    stb_elem->from_frame = current_frame;
    stb_elem->to_frame = current_frame;
  }

  if(gap_debug)
  {
    printf("p_create_stb_elem_from_filename  current_frame:%d\n"
       , (int)current_frame);
    gap_story_debug_print_elem(stb_elem);
  }

  return (stb_elem);
}  /* end p_create_stb_elem_from_filename */


/* ---------------------------------
 * p_frame_widget_init_dnd
 * ---------------------------------
 * accept dropping of uri (filenames) from
 * any applications.
 *  TODO: this drop should be restricted to GDK_ACTION_COPY
 *  but if we remove the GDK_ACTION_MOVE in the gtk_drag_dest_set
 *  call, move would be disabled too for the application internal
 *  drop-move of typ GAP_STB_TARGET_STORYBOARD_ELEM
 * drag & drop support for storyboard elem
 * only local in the same application.
 */
static void
p_frame_widget_init_dnd(GapStbFrameWidget *fw)
{
  GtkWidget *widget;
  static const GtkTargetEntry drop_types[] = {
    { "text/uri-list", 0, GAP_STB_TARGET_URILIST},
    { "application/gimp-gap-story-elem", GTK_TARGET_SAME_APP, GAP_STB_TARGET_STORYBOARD_ELEM}
  };
  static const gint n_drop_types = sizeof(drop_types)/sizeof(drop_types[0]);

  static const GtkTargetEntry drag_types[] = {
    { "application/gimp-gap-story-elem", GTK_TARGET_SAME_APP, GAP_STB_TARGET_STORYBOARD_ELEM}
  };
  static const gint n_drag_types = sizeof(drag_types)/sizeof(drag_types[0]);

  widget = fw->pv_ptr->da_widget;
  g_object_set_data (G_OBJECT (widget), GAP_STORY_FW_PTR, (gpointer)fw);

  gtk_drag_dest_set (GTK_WIDGET (widget),
                     GTK_DEST_DEFAULT_ALL,
                     drop_types, n_drop_types,
                     GDK_ACTION_COPY | GDK_ACTION_MOVE);
  g_signal_connect (widget, "drag_data_received",
                    G_CALLBACK (on_clip_elements_dropped), NULL);


  gtk_drag_source_set (GTK_WIDGET (widget),
                       GDK_BUTTON1_MASK | GDK_BUTTON2_MASK,
                       drag_types, n_drag_types,
                       GDK_ACTION_COPY | GDK_ACTION_MOVE);

  g_signal_connect (widget, "drag_data_get",
                    G_CALLBACK (on_stb_elem_drag_get), fw);

  g_signal_connect (widget, "drag_begin",
                    G_CALLBACK (on_stb_elem_drag_begin), fw);

  /* the "drag_data_delete" event handler was never called in may test
   * therefore was commented out, and workaround was
   * implemented in the drop handler on_clip_elements_dropped
   */
  //g_signal_connect (widget, "drag_data_delete",
  //                 G_CALLBACK (on_stb_elem_drag_delete), fw);

  if(gap_debug) printf("INIT FW:%d    fw->tabw:%d\n", (int)fw, (int)fw->tabw);

}  /* end p_frame_widget_init_dnd */


/* ---------------------------------
 * p_widget_init_dnd_drop
 * ---------------------------------
 * accept dropping of uri (filenames) from
 * any applications.
 */
static void
p_widget_init_dnd_drop(GtkWidget *widget, GapStbTabWidgets *tabw)
{
  static const GtkTargetEntry drop_types[] = {
    { "text/uri-list", 0, GAP_STB_TARGET_URILIST},
    { "application/gimp-gap-story-elem", GTK_TARGET_SAME_APP, GAP_STB_TARGET_STORYBOARD_ELEM}
  };
  static const gint n_drop_types = sizeof(drop_types)/sizeof(drop_types[0]);

  g_object_set_data (G_OBJECT (widget), GAP_STORY_TABW_PTR, (gpointer)tabw);
  gtk_drag_dest_set (GTK_WIDGET (widget),
                     GTK_DEST_DEFAULT_ALL,
                     drop_types, n_drop_types,
                     GDK_ACTION_COPY | GDK_ACTION_MOVE);
  g_signal_connect (widget, "drag_data_received",
                    G_CALLBACK (on_clip_elements_dropped), NULL);


}  /* end p_widget_init_dnd_drop */


/* ---------------------------------
 * p_get_external_image_viewer
 * ---------------------------------
 * TODO: configuration via environment AND/OR gimprc
 */
static char *
p_get_external_image_viewer()
{
  static char *defaultExternalImageViewer = "eog";
  char *value_string;

  value_string = gimp_gimprc_query("video_external_image_viewer");
  if(value_string)
  {
     return(value_string);
  }

  return g_strdup(defaultExternalImageViewer);

}  /* end p_get_external_image_viewer */


/* ---------------------------------
 * p_frame_widget_preview_events_cb
 * ---------------------------------
 * handles events for all frame_widgets (in the dyn_table)
 * - Expose of pview drawing_area
 */
static void
p_call_external_image_viewer(GapStbFrameWidget *fw)
{
  gchar *imagename;
  GapStbTabWidgets *tabw;
  GapStbMainGlobalParams *sgpp;

  tabw = (GapStbTabWidgets *)fw->tabw;
  sgpp = fw->sgpp;

  if((tabw) && (sgpp))
  {
    if(fw->stb_elem_refptr)
    {
      if(gap_story_elem_is_video(fw->stb_elem_refptr))
      {
        if((fw->stb_elem_refptr->record_type == GAP_STBREC_VID_SECTION)
        || (fw->stb_elem_refptr->record_type == GAP_STBREC_VID_BLACKSECTION))
        {
          return;
        }
        else
        {
          char *externalImageViewerCall;
          char *externalImageViewer;


          imagename = gap_story_get_filename_from_elem(fw->stb_elem_refptr);
          externalImageViewer = p_get_external_image_viewer();
          externalImageViewerCall = g_strdup_printf("%s %s &"
             ,p_get_external_image_viewer()
             ,imagename
             );
          system(externalImageViewerCall);

          g_free(externalImageViewerCall);
          g_free(imagename);
        }


      }
    }
  }

}  /* end p_call_external_image_viewer */
