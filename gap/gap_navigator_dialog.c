/* gap_navigator_dialog.c
 *  by hof (Wolfgang Hofer)
 *
 * GAP ... Gimp Animation Plugins
 *
 * This Module contains the GAP Video Navigator dialog Window
 * that provides a VCR-GUI for the GAP basic navigation functions.
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

/* revision history:
 * gimp    2.1.0a;  2005/03/12  hof: added radio buttons for active layer tracking
 * gimp    2.1.0a;  2004/11/04  hof: replaced deprecated option_menu by gimp_image_combo_box_new
 * gimp    2.1.0a;  2004/06/26  hof: #144649 use NULL for the default cursor as active_cursor
 * gimp    1.3.26a; 2004/01/30  hof: navi_pviews_reset: use the procedure gap_pview_drop_repaint_buffers rather than g_free the pixmap
 * gimp    1.3.25a; 2004/01/21  hof: gap_thumb_file_load_thumbnail now returns th_data with th_bpp == 4
 *                                   (the flatten capabilities were removed)
 * gimp    1.3.24a; 2004/01/17  hof: bugfix: call of plug_in_gap_range_to_multilayer needs regionselect_mode
 *                                   use faster procedure gap_thumb_file_load_thumbnail to load thumbnaildata
 * gimp    1.3.23a; 2003/11/26  hof: follow API changes for gimp_dialog_new
 * gimp    1.3.20d; 2003/10/18  hof: sourcecode cleanup, remove close button
 * gimp    1.3.19a; 2003/09/06  hof: call plug_in_gap_videoframes_player with dummy audioparameters
 * gimp    1.3.18b; 2003/08/27  hof: fixed waiting cursor for long running ops
 * gimp    1.3.17a; 2003/07/29  hof: param types GimpPlugInInfo.run procedure
 * gimp    1.3.16c; 2003/07/12  hof: bugfixes (vscale slider reflects pagesize), del_button,dup_button sensitivity
 * gimp    1.3.16b; 2003/07/02  hof: selection highlight bugfix (using gtk_widget_modify_bg)
 * gimp    1.3.16a; 2003/06/29  hof: redesign: replaced scrolled_window by table that fits the visible window height
 *                                   and a vertical scale to manage scrolling.
 *                                   with this approach it is now possible to handle
 *                                   all frames of an animation without a limit.
 *                                   HIGHLIGHTING of the selected frame_widgets does not work yet.
 *
 *        Overview about the GAP Navigator Redesign
 *
 *        - use gap_pview_da (drawing_area based) render procedures (instead of deprecated gtk_preview)
 *        - dyn_table is a gtk_table that has 1 column, and 1 upto MAX_DYN_ROWS.
 *        - dyn_table shows only a subset of thumbnails beginning at dyn_topframenr
 *          frame_widget_tab[0] is always attached to the 1.st dyn_table row.
 *          if dyn_rows > 1
 *          then  frame_widget_tab[1] is attached to 2.nd dyn_table row and so on...
 *
 *        - with the adjustment (vscale) dyn_adj the user can set dyn_topframenr
 *          to scroll the displayed framerange.
 *          at changes of dyn_topframenr all visible thumbnails are updated as follows:
 *
 *
 *            frame_widget_tab[0]  is loaded with thumbnail [dyn_topframenr]
 *            frame_widget_tab[1]  is loaded with thumbnail [dyn_topframenr+1]
 *            ...
 *
 *            frame_widget_tab[dyn_rows-1]  is loaded with thumbnail [dyn_topframenr + (dyn_rows - 1)]
 *
 *
 *        - dyn_table grows and shrinks at user window resize operations
 *          on significant height grow events:
 *             additional rows are added to dyn_table (using gtk_table_resize)
 *             and more elements from frame_widget_tab are attached to
 *             the newly added row(s)
 *
 *          on significant height shrink events:
 *             rows are removed from dyn_table (using gtk_table_resize)
 *             detach is done by destroying the attached frame_widgets
 *             before the row is removed.
 *
 *        - Selections are handled completely by this module in the SelectedRange List
 *             this list is sorted ascending by "from" numbers (framenumber).
 *
 * gimp    1.3.15a; 2003/06/21  hof: gap_timeconv_framenr_to_timestr, use plug_in_gap_videoframes_player for playback
 *                                   ops_button_pressed_callback must have returnvalue FALSE
 *                                   (this enables other handlers -- ops_button_extended_callback -- to run afterwards
 * gimp    1.3.14b; 2003/06/03  hof: added gap_stock_init
 *                                   ops_button_box_new: now using stock buttons instead of xpm pixmap data
 * gimp    1.3.14a; 2003/05/27  hof: replaced old workaround procedure readXVThumb by legal API gimp_file_load_thumbnail
 *                                   replaced scale widgets by spinbuttons
 * gimp    1.3.12a; 2003/05/03  hof: merge into CVS-gimp-gap project
 *                                   6digit framenumbers, replace gimp_help_init by _gimp_help_init
 * gimp    1.3.5a;  2002/04/21  hof: handle changing image_id of the active_image (navi_reload_ainfo)
 * gimp    1.3.4;   2002/03/12  hof: STARTED port to gtk+2.0.0
 *                                   still needs GTK_DISABLE_DEPRECATED (bacause port is not complete)
 * version 1.1.29b; 2000.11.30   hof: new e-mail adress
 * version 1.1.20a; 2000.04.25   hof: copy/cut/paste menu
 * version 1.1.14a; 2000.01.08   hof: 1st release
 */

/* SYTEM (UNIX) includes */
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <signal.h>
#include <sys/stat.h>
#include <unistd.h>
#include <locale.h>

#include <glib/gstdio.h>

/* GIMP includes */
#include <gtk/gtk.h>
#include "config.h"
#include "gap-intl.h"
#include <libgimp/gimp.h>
#include <libgimp/gimpui.h>

#include "gap_libgapbase.h"
#include "gap_stock.h"
#include "gap_timeconv.h"

#include "gap_navi_activtable.h"
#include "gap_pview_da.h"
#include "gap_arr_dialog.h"
#include "gap_thumbnail.h"
#include "gap_image.h"


#define PLUGIN_NAME     "plug_in_gap_navigator"
#define PLUGIN_HELP_ID  "plug-in-gap-navigator"

#define NAVI_CHECK_SIZE 4
#define MAX_DYN_ROWS 400

#define KEY_NAVI_DIALOG_TO_FRONT     "plug_in_gap_navigator_to_front"

/*
 * OpsButton  code was inspired by gimp-1.2 core code,
 *   but was changed for gap_navigator_dialog use and ported to gtk+2.2
 *   switched from GtkSignalFunc to private OpsButtonFunc
 */

typedef void (*OpsButtonFunc)      (GtkWidget *widget, gpointer data);
#define OPS_BUTTON_FUNC(f)  ((OpsButtonFunc) f)

/* ---------------------------------------  start copy of gimp-1.1.14/app/ops_buttons.h */
#ifndef __OPS_BUTTONS_H__
#define __OPS_BUTTONS_H__

typedef enum
{
  OPS_BUTTON_MODIFIER_NONE,
  OPS_BUTTON_MODIFIER_SHIFT,
  OPS_BUTTON_MODIFIER_CTRL,
  OPS_BUTTON_MODIFIER_ALT,
  OPS_BUTTON_MODIFIER_SHIFT_CTRL,
  OPS_BUTTON_MODIFIER_LAST
} OpsButtonModifier;

typedef enum
{
  OPS_BUTTON_NORMAL,
  OPS_BUTTON_RADIO
} OpsButtonType;

typedef struct _OpsButton OpsButton;

struct _OpsButton
{
  const char     *stock_id;       /*  STOCK id for the stock button  */
  OpsButtonFunc   callback;       /*  callback function        */
  OpsButtonFunc  *ext_callbacks;  /*  callback functions when
                                   *  modifiers are pressed    */
  gchar          *tooltip;
  gchar          *private_tip;
  GtkWidget      *widget;         /*  the button widget        */
  gint            modifier;
};

/* Function declarations */

GtkWidget * ops_button_box_new (GtkWidget     *parent,
                                OpsButton     *ops_button,
                                OpsButtonType  ops_type);

#endif /* __OPS_BUTTONS_H__ */
/* ---------------------------------------  end copy of gimp-1.1.14/app/ops_buttons.h */



/* GAP includes */
#include "gap_lib.h"
#include "gap_pdb_calls.h"
#include "gap_vin.h"

/*  some definitions used in all dialogs  */

#define LIST_WIDTH  200
#define LIST_HEIGHT 150
#define PREVIEW_BPP 3
#define THUMBNAIL_BPP 3
#define MIX_CHANNEL(b, a, m)  (((a * m) + (b * (255 - m))) / 255)
#define PREVIEW_BG_GRAY1 80
#define PREVIEW_BG_GRAY2 180

#define NUPD_IMAGE_MENU             1
#define NUPD_THUMBFILE_TIMESTAMP    2
#define NUPD_FRAME_NR_CHANGED       4
#define NUPD_PREV_LIST              8
#define NUPD_PREV_LIST_ICONS        16
#define NUPD_ALL                   0xffffffff;


typedef struct _OpenFrameImages OpenFrameImages;
typedef struct _SelectedRange SelectedRange;


struct _OpenFrameImages{
  gint32 image_id;
  gint32 frame_nr;
  OpenFrameImages *next;
};


struct _SelectedRange {
   gint32 from;
   gint32 to;
   SelectedRange *next;
   SelectedRange *prev;
};


typedef struct FrameWidget
{
  GtkWidget *event_box;
  GtkWidget *vbox;
  GtkWidget *hbox;
  GtkWidget *number_label;      /* for the 6-digit framenumber */
  GtkWidget *time_label;
  GapPView   *pv_ptr;            /* for gap preview rendering based on drawing_area */

  gint32    image_id;
  gint32    frame_nr;
  gint       width;
  gint       height;

  /*  state information  */
  time_t    frame_timestamp;
  char      *frame_filename;
} FrameWidget;



typedef struct NaviDialog
{
  FrameWidget   frame_widget_tab[MAX_DYN_ROWS];
  gint32        fw_tab_used_count;    /* how much elements are initialized in frame_widget_tab */
  GtkWidget     *vbox;
  GtkWidget     *hbox;
  GtkWidget     *dyn_frame;           /* table with */
  GtkWidget     *dyn_table;           /* table with */
  GtkObject     *dyn_adj;
  gint32         dyn_topframenr;
  gint32         dyn_rows;
  gint32         prev_selected_framnr;
  SelectedRange *sel_range_list;
  gboolean       in_dyn_table_sizeinit;

  gint          tooltip_on;
  GtkWidget     *shell;
  GtkWidget     *preserve_trans;
  GtkWidget     *framerate_box;
  GtkWidget     *timezoom_box;
  GtkWidget     *image_combo;
  GtkWidget     *image_combo_hbox;
  GtkWidget     *ops_menu;
  GtkWidget     *copy_menu_item;
  GtkWidget     *cut_menu_item;
  GtkWidget     *pastea_menu_item;
  GtkWidget     *pasteb_menu_item;
  GtkWidget     *paster_menu_item;
  GtkWidget     *clrpaste_menu_item;
  GtkWidget     *sel_all_menu_item;
  GtkWidget     *sel_none_menu_item;
  GtkAccelGroup *accel_group;
  GtkObject     *framerate_adj;
  GtkObject     *timezoom_adj;
  GtkWidget     *framerange_number_label;
  GtkWidget     *del_button;
  GtkWidget     *dup_button;
  gint           waiting_cursor;
  GdkCursor     *cursor_wait;
  GdkCursor     *cursor_acitve;
  gint32         paste_at_frame;         /* -1: use current frame */

  gdouble ratio;
  gdouble preview_size;
  gint    image_width, image_height;
  gint    gimage_width, gimage_height;

  /* state information  */
  gint32        active_imageid;
  gint32        any_imageid;
  GapAnimInfo  *ainfo_ptr;
  GapVinVideoInfo *vin_ptr;

  int           timer;
  int           cycle_time;
  OpenFrameImages *OpenFrameImagesList;
  int              OpenFrameImagesCount;
  gint32       item_height;

  GtkWidget     *acl_trace_off_toggle;
  GtkWidget     *acl_trace_by_name_toggle;
  GtkWidget     *acl_trace_by_pos_toggle;
} NaviDialog;


/* -----------------------
 * procedure declarations
 * -----------------------
 */
static void p_set_data_to_front_request(gboolean dialog_to_front_request);
static void p_check_dialog_to_front_request(void);

int  gap_navigator(gint32 image_id);
static void navi_preview_extents (void);
static void frames_dialog_flush (void);
static void frames_dialog_update (gint32 image_id);
static void frame_widget_preview_redraw      (FrameWidget *);
static void navi_vid_copy_and_cut(gint cut_flag);

static gboolean navi_images_menu_constrain (gint32 image_id, gpointer data);
static void navi_images_menu_callback  (GtkWidget *widget, gpointer data);
static void navi_update_after_goto(void);
static void navi_ops_menu_set_sensitive(void);
static void navi_ops_buttons_set_sensitive(void);

static void navi_pviews_reset(void);
static void navi_dialog_thumb_update_callback(GtkWidget *w, gpointer   data);
static void navi_dialog_thumb_updateall_callback(GtkWidget *w, gpointer   data);
static void navi_dialog_vcr_play_callback(GtkWidget *w, gpointer   data);
static void navi_dialog_vcr_play_layeranim_callback(GtkWidget *w, gpointer   data);
static void navi_dialog_frames_duplicate_frame_callback(GtkWidget *w, gpointer   data);
static void navi_dialog_frames_delete_frame_callback(GtkWidget *w, gpointer   data);

static void navi_dialog_vcr_goto_first_callback(GtkWidget *w, gpointer   data);
static void navi_dialog_vcr_goto_prev_callback(GtkWidget *w, gpointer   data);
static void navi_dialog_vcr_goto_prevblock_callback(GtkWidget *w, gpointer   data);
static void navi_dialog_vcr_goto_next_callback(GtkWidget *w, gpointer   data);
static void navi_dialog_vcr_goto_nextblock_callback(GtkWidget *w, gpointer   data);
static void navi_dialog_vcr_goto_last_callback(GtkWidget *w, gpointer   data);

static void navi_framerate_spinbutton_update(GtkAdjustment *w, gpointer   data);
static void navi_timezoom_spinbutton_update(GtkAdjustment *w, gpointer   data);

static gboolean frame_widget_preview_events      (GtkWidget *, GdkEvent *, gpointer);

static gint navi_dialog_poll(GtkWidget *w, gpointer   data);
static void navi_dialog_update(gint32 update_flag);
static void navi_scroll_to_current_frame_nr(void);
static gboolean navi_scroll_event_cb ( GtkWidget *widget
                    , GdkEventScroll *sevent
                    , NaviDialog *naviD
                    );

static gint32          navi_get_preview_size(void);
static void            navi_frames_timing_update (void);
static void            navi_frame_widget_time_label_update(FrameWidget *fw);
static void            navi_dialog_tooltips(void);
static void            navi_set_waiting_cursor(void);
static void            navi_set_active_cursor(void);

static SelectedRange * navi_get_last_range_list(SelectedRange *sel_range_list);
static void            navi_debug_print_sel_range(void);
static void            navi_drop_sel_range_list(void);
static void            navi_drop_sel_range_list2(void);
static void            navi_drop_sel_range_elem(SelectedRange *range_item);
static SelectedRange * navi_findframe_in_sel_range(gint32 framenr);
static void            navi_add_sel_range_list(gint32 framenr_from, gint32 framenr_to);
static void            navi_sel_normal(gint32 framenr);
static void            navi_sel_ctrl(gint32 framenr);
static void            navi_sel_shift(gint32 framenr);



static void            navi_dyn_frame_size_allocate  (GtkWidget       *widget,
                                                      GtkAllocation   *allocation,
                                                      gpointer         user_data);

static void            navi_frame_widget_replace2(FrameWidget *fw);
static void            navi_frame_widget_replace(gint32 image_id, gint32 frame_nr, gint32 dyn_rowindex);
static void            navi_refresh_dyn_table(gint32 l_frame_nr);
static void            navi_dyn_adj_changed_callback(GtkWidget *wgt, gpointer data);
static void            navi_dyn_adj_set_pos(void);
static void            navi_dyn_adj_set_limits(void);
static void            navi_frame_widget_init_empty (FrameWidget *fw);
static int             navi_dyn_table_set_size(gint win_height);

static void       p_set_acl_tracking_radio_buttons(NaviDialog *naviD);
static void       p_set_acl_tracking_mode(NaviDialog *naviD, gint32 acl_tracking_mode);
static void       p_acl_tracking_off_callback(GtkWidget *widget, NaviDialog *naviD);
static void       p_acl_tracking_by_name_callback(GtkWidget *widget, NaviDialog *naviD);
static void       p_acl_tracking_by_pos_callback(GtkWidget *widget, NaviDialog *naviD);

static gint32     p_get_number_of_frames(GapAnimInfo *ainfo_ptr);


/* -----------------------
 * Local data
 * -----------------------
 */

static NaviDialog *naviD = NULL;
static gint suspend_gimage_notify = 0;
static gint32  global_old_active_imageid = -1;

/*  the ops buttons  */
static OpsButtonFunc navi_dialog_update_ext_callbacks[] =
{
  OPS_BUTTON_FUNC (navi_dialog_thumb_updateall_callback), NULL, NULL, NULL
};
static OpsButtonFunc navi_dialog_vcr_play_ext_callbacks[] =
{
  OPS_BUTTON_FUNC (navi_dialog_vcr_play_layeranim_callback), NULL, NULL, NULL
};
static OpsButtonFunc navi_dialog_vcr_goto_prev_ext_callbacks[] =
{
  OPS_BUTTON_FUNC (navi_dialog_vcr_goto_prevblock_callback), NULL, NULL, NULL
};
static OpsButtonFunc navi_dialog_vcr_goto_next_ext_callbacks[] =
{
  OPS_BUTTON_FUNC (navi_dialog_vcr_goto_nextblock_callback), NULL, NULL, NULL
};

static OpsButton frames_ops_buttons[] =
{
  { GAP_STOCK_PLAY, OPS_BUTTON_FUNC (navi_dialog_vcr_play_callback), navi_dialog_vcr_play_ext_callbacks,
    N_("Playback\n"
       "SHIFT converts the selected frames to temporary image, "
       "and does layeranimation playback on it."),
    "#playback",
    NULL, 0 },
  { GAP_STOCK_UPDATE, OPS_BUTTON_FUNC (navi_dialog_thumb_update_callback), navi_dialog_update_ext_callbacks,
    N_("Smart update thumbnails\n"
       "SHIFT forces thumbnail update for all frames"),
    "#update",
    NULL, 0 },
  { GIMP_STOCK_DUPLICATE, OPS_BUTTON_FUNC (navi_dialog_frames_duplicate_frame_callback), NULL,
    N_("Duplicate selected frames"),
    "#duplicate",
    NULL, 0 },
  { GTK_STOCK_DELETE, OPS_BUTTON_FUNC (navi_dialog_frames_delete_frame_callback), NULL,
    N_("Delete selected frames"),
    "#delete",
    NULL, 0 },
  { NULL, NULL, NULL, NULL, NULL, NULL, 0 }
};

static OpsButton vcr_ops_buttons[] =
{
  { GTK_STOCK_GOTO_FIRST, OPS_BUTTON_FUNC (navi_dialog_vcr_goto_first_callback), NULL,
    N_("Goto first frame"),
    "#goto_first",
    NULL, 0 },
  { GTK_STOCK_GO_BACK, OPS_BUTTON_FUNC (navi_dialog_vcr_goto_prev_callback), navi_dialog_vcr_goto_prev_ext_callbacks,
    N_("Goto prev frame\n"
       "SHIFT use timezoom stepsize"),
    "#goto_previous",
    NULL, 0 },
  { GTK_STOCK_GO_FORWARD, OPS_BUTTON_FUNC (navi_dialog_vcr_goto_next_callback), navi_dialog_vcr_goto_next_ext_callbacks,
    N_("Goto next frame\n"
       "SHIFT use timezoom stepsize"),
    "#goto_next",
    NULL, 0 },
  { GTK_STOCK_GOTO_LAST, OPS_BUTTON_FUNC (navi_dialog_vcr_goto_last_callback), NULL,
    N_("Goto last frame"),
    "#goto_last",
    NULL, 0 },

  { NULL, NULL, NULL, NULL, NULL, NULL, 0 }
};



/* ------------------------
 * gap DEBUG switch
 * ------------------------
 */

/* int gap_debug = 1; */    /* print debug infos */
/* int gap_debug = 0; */    /* 0: dont print debug infos */

int gap_debug = 0;


static void query(void);
static void run(const gchar *name
              , gint n_params
              , const GimpParam *param
              , gint *nreturn_vals
              , GimpParam **return_vals);

GimpPlugInInfo PLUG_IN_INFO =
{
  NULL,  /* init_proc */
  NULL,  /* quit_proc */
  query, /* query_proc */
  run,   /* run_proc */
};

/* ------------------------
 * MAIN, query & run
 * ------------------------
 */

MAIN ()

/* ---------------------------------
 * query
 * ---------------------------------
 */
static void
query ()
{
  static GimpParamDef args_navigator[] =
  {
    {GIMP_PDB_INT32, "run_mode", "Interactive"},
    {GIMP_PDB_IMAGE, "image", "(unused)"},
    {GIMP_PDB_DRAWABLE, "drawable", "(unused)"},
  };

  static GimpParamDef *return_vals = NULL;
  static int nreturn_vals = 0;

  gimp_plugin_domain_register (GETTEXT_PACKAGE, LOCALEDIR);


  gimp_install_procedure(PLUGIN_NAME,
                         "GAP video navigator dialog",
                         "",
                         "Wolfgang Hofer (hof@gimp.org)",
                         "Wolfgang Hofer",
                         GAP_VERSION_WITH_DATE,
                         N_("VCR Navigator..."),
                         "RGB*, INDEXED*, GRAY*",
                         GIMP_PLUGIN,
                         G_N_ELEMENTS (args_navigator), nreturn_vals,
                         args_navigator, return_vals);
  gimp_plugin_menu_register (PLUGIN_NAME, N_("<Image>/Video/"));
}       /* end query */


/* ---------------------------------
 * run
 * ---------------------------------
 */
static void
run(const gchar *name
   , gint n_params
   , const GimpParam *param
   , gint *nreturn_vals
   , GimpParam **return_vals)
{
  gint32       l_active_image;
  const gchar *l_env;

  static GimpParam values[2];
  GimpRunMode run_mode;
  GimpPDBStatusType status = GIMP_PDB_SUCCESS;
  pid_t  l_navid_pid;

  gint32     l_rc;

  *nreturn_vals = 1;
  *return_vals = values;
  l_rc = 0;

  INIT_I18N();

  l_env = g_getenv("GAP_DEBUG");
  if(l_env != NULL)
  {
    if((*l_env != 'n') && (*l_env != 'N')) gap_debug = 1;
  }

  run_mode = param[0].data.d_int32;
  l_active_image = -1;

  if(gap_debug) printf("\n\ngap_navigator: debug name = %s\n", name);

  l_active_image = param[1].data.d_image;
  p_set_data_to_front_request(TRUE);

  /* check for other Video navigator Dialog Process */
  if (sizeof(pid_t) == gimp_get_data_size(PLUGIN_NAME))
  {
    gimp_get_data(PLUGIN_NAME, &l_navid_pid);

    /* note that the check gap_base_is_pid_alive implementation does not work on WINDOWS
     */
#ifndef G_OS_WIN32
    if(l_navid_pid != 0)
    {
      if (gap_base_is_pid_alive(l_navid_pid))
      {
         gap_arr_msg_win(GIMP_RUN_INTERACTIVE, _("Cant open two or more video navigator windows."));
         l_rc = -1;
      }
    }
#endif
  }

  if(l_rc == 0)
  {
    /* set pid data when navigator is running */
    l_navid_pid = gap_base_getpid();
    gimp_set_data(PLUGIN_NAME, &l_navid_pid, sizeof(pid_t));
    if (strcmp (name, PLUGIN_NAME) == 0)
    {
        if (run_mode != GIMP_RUN_INTERACTIVE)
        {
            status = GIMP_PDB_CALLING_ERROR;
        }

        if (status == GIMP_PDB_SUCCESS)
        {
          l_rc = gap_navigator(l_active_image);
        }
    }
    /* set pid data to 0 when navigator stops */
    l_navid_pid = 0;
    gimp_set_data(PLUGIN_NAME, &l_navid_pid, sizeof(pid_t));
  }

  /* ---------- return handling --------- */

 if(l_rc < 0)
 {
    status = GIMP_PDB_EXECUTION_ERROR;
 }

  values[0].type = GIMP_PDB_STATUS;
  values[0].data.d_status = status;
}  /* end run */


/* --------------------------
 * navi_delete_confirm_dialog
 * --------------------------
 */
static gboolean
navi_delete_confirm_dialog(gint32 del_cnt)
{
  gchar *msg_txt;
  gboolean l_rc;

  msg_txt = g_strdup_printf(_("The selected %d frame(s) will be deleted.\n"
                    "There will be no undo for this operation\n")
                    ,(int)del_cnt
                 );
  l_rc = gap_arr_confirm_dialog(msg_txt
                         , _("Confirm Frame Delete")   /* title_txt */
                         , _("Confirm Frame Delete")   /* frame_txt */
                         );
  g_free(msg_txt);

  return (l_rc);

}  /* end navi_delete_confirm_dialog */




/* ---------------------------------
 * navi_constrain_dyn_topframenr
 * ---------------------------------
 */
static gint32
navi_constrain_dyn_topframenr(gint32 frame_nr)
{
  gint32       l_frame_nr;

  l_frame_nr = (frame_nr - naviD->ainfo_ptr->first_frame_nr) / naviD->vin_ptr->timezoom;
  l_frame_nr = (l_frame_nr * naviD->vin_ptr->timezoom)+ naviD->ainfo_ptr->first_frame_nr;

  l_frame_nr = CLAMP(l_frame_nr
                    , naviD->ainfo_ptr->first_frame_nr
                    , MAX(naviD->ainfo_ptr->first_frame_nr
                         , (1 + naviD->ainfo_ptr->last_frame_nr - naviD->dyn_rows))
                    );
  return (l_frame_nr);
}  /* end navi_constrain_dyn_topframenr */


/* ---------------------------------
 * the navigator callback procedures
 * ---------------------------------
 */

static void
edit_copy_callback (GtkWidget *w,  gpointer   client_data)
{
  if(gap_debug) printf("edit_copy_callback\n");
  navi_vid_copy_and_cut(FALSE /* cut_flag */);
}
static void
edit_cut_callback (GtkWidget *w,  gpointer   client_data)
{
  if(gap_debug) printf("edit_cut_callback\n");
  navi_vid_copy_and_cut(TRUE /* cut_flag */);
}

/* ---------------------------------
 * p_set_data_to_front_request
 * ---------------------------------
 */
static void
p_set_data_to_front_request(gboolean dialog_to_front_request)
{
  gimp_set_data(KEY_NAVI_DIALOG_TO_FRONT, &dialog_to_front_request, sizeof (gboolean));
}  /* end p_set_data_to_front_request */

/* ---------------------------------
 * p_check_dialog_to_front_request
 * ---------------------------------
 * check for request to present the (already open) naviagtor dialog
 * window (in front of screen)
 * typically used on attempt to start a 2nd instance (which is not allowed)
 */
static void
p_check_dialog_to_front_request(void)
{
  gint data_size;
  if (naviD == NULL)
  {
    return;
  }
  if (naviD->shell == NULL)
  {
    return;
  }
  if (GTK_WIDGET(naviD->shell)->window == NULL)
  {
    return;
  }

  data_size = gimp_get_data_size(KEY_NAVI_DIALOG_TO_FRONT);
  if (data_size == sizeof (gboolean))
  {
     gboolean dialog_to_front_request;
   
     gimp_get_data(KEY_NAVI_DIALOG_TO_FRONT, &dialog_to_front_request);
     if(dialog_to_front_request == TRUE)
     {
       gtk_window_present(GTK_WINDOW(naviD->shell));
       p_set_data_to_front_request(FALSE);
     }
  }

}  /* end p_check_dialog_to_front_request */


/* ---------------------------------
 * p_edit_paste_call
 * ---------------------------------
 */
static void
p_edit_paste_call(gint32 paste_mode)
{
  GimpParam          *return_vals;
  int              nreturn_vals;
  gint32           dummy_layer_id;

  if(naviD->paste_at_frame < 0)
  {
    return;  /* invalid frame_nr do not paste here */
  }

  if(gap_debug)
  {
    printf("p_edit_paste_call: paste_at_frame:%d active_image_id:%d\n"
       , (int)naviD->paste_at_frame
       , (int)naviD->active_imageid
       );
  }

  /* goto the first selected frame */
  dummy_layer_id = gap_image_get_any_layer(naviD->active_imageid);
  return_vals = gimp_run_procedure ("plug_in_gap_goto",
                               &nreturn_vals,
                               GIMP_PDB_INT32,    GIMP_RUN_NONINTERACTIVE,
                               GIMP_PDB_IMAGE,    naviD->active_imageid,
                               GIMP_PDB_DRAWABLE, dummy_layer_id,
                               GIMP_PDB_INT32,    naviD->paste_at_frame,
                               GIMP_PDB_END);
  if (return_vals[0].data.d_status != GIMP_PDB_SUCCESS)
  {
    gimp_destroy_params(return_vals, nreturn_vals);
    gap_arr_msg_win(GIMP_RUN_INTERACTIVE
             ,_("Error while positioning to frame. Video paste operation failed")
             );
    return;
  }

  naviD->active_imageid = return_vals[1].data.d_image;
  gimp_destroy_params(return_vals, nreturn_vals);


  dummy_layer_id = gap_image_get_any_layer(naviD->active_imageid);
  return_vals = gimp_run_procedure ("plug_in_gap_video_edit_paste",
                                      &nreturn_vals,
                                      GIMP_PDB_INT32,    GIMP_RUN_NONINTERACTIVE,
                                      GIMP_PDB_IMAGE,    naviD->active_imageid,
                                      GIMP_PDB_DRAWABLE, dummy_layer_id,
                                      GIMP_PDB_INT32,    paste_mode,
                                      GIMP_PDB_END);
  if (return_vals[0].data.d_status == GIMP_PDB_SUCCESS)
  {
    naviD->active_imageid = return_vals[1].data.d_image;
  }
  else
  {
    gap_arr_msg_win(GIMP_RUN_INTERACTIVE, _("Video paste operation failed"));
  }

  gimp_destroy_params(return_vals, nreturn_vals);
  navi_update_after_goto();

}  /* end p_edit_paste_call */


static void
edit_pasteb_callback (GtkWidget *w,  gpointer   client_data)
{
  p_edit_paste_call(GAP_VID_PASTE_INSERT_BEFORE);
}
static void
edit_pastea_callback (GtkWidget *w,  gpointer   client_data)
{
  p_edit_paste_call(GAP_VID_PASTE_INSERT_AFTER);
}
static void
edit_paster_callback (GtkWidget *w,  gpointer   client_data)
{
  p_edit_paste_call(GAP_VID_PASTE_REPLACE);
}

static void
edit_clrpaste_callback (GtkWidget *w,  gpointer   client_data)
{
  GimpParam          *return_vals;
  int              nreturn_vals;
  gint32           dummy_layer_id;


  if(gap_debug) printf("edit_clrpaste_callback\n");

  dummy_layer_id = gap_image_get_any_layer(naviD->active_imageid);
  return_vals = gimp_run_procedure ("plug_in_gap_video_edit_clear",
                                      &nreturn_vals,
                                      GIMP_PDB_INT32,    GIMP_RUN_NONINTERACTIVE,
                                      GIMP_PDB_IMAGE,    naviD->active_imageid,
                                      GIMP_PDB_DRAWABLE, dummy_layer_id,
                                      GIMP_PDB_END);
  gimp_destroy_params(return_vals, nreturn_vals);
}

static void
navi_sel_all_callback (GtkWidget *w,  gpointer   client_data)
{
  navi_drop_sel_range_list();
  navi_add_sel_range_list(naviD->ainfo_ptr->first_frame_nr
                         ,naviD->ainfo_ptr->last_frame_nr);
  navi_debug_print_sel_range();
  navi_ops_buttons_set_sensitive();
  navi_frames_timing_update();
}

static void
navi_sel_none_callback (GtkWidget *w,  gpointer   client_data)
{
  navi_drop_sel_range_list();
  navi_debug_print_sel_range();
  navi_ops_buttons_set_sensitive();
  navi_frames_timing_update();
}

/* ---------------------------------
 * navi_vid_copy_and_cut
 * ---------------------------------
 * perform copy (or cut) operations
 * and free the selection list
 */
void
navi_vid_copy_and_cut(gint cut_flag)
{
   SelectedRange *range_list;
   SelectedRange *range_list2;
   GimpParam          *return_vals;
   int                 nreturn_vals;
   gboolean            vid_copy_ok;
   gint32              dummy_layer_id;

  vid_copy_ok = TRUE;
  if(gap_debug) printf("navi_dialog_vid_copy_callback\n");

  if(naviD->sel_range_list)
  {
    navi_set_waiting_cursor();
    gap_vid_edit_clear();


    range_list = naviD->sel_range_list;
    while(range_list)
    {
       /* Note: process the ranges from low frame_nummers
        *       upto high frame_numbers.
        *       (the sel_range_list is sorted by ascending from-framenumbers at creation
        *        the lowest range the 1st list element in the list)
        */
       if(gap_debug) printf("Copy Range from:%d  to:%d\n"
                     ,(int)range_list->from ,(int)range_list->to );

       dummy_layer_id = gap_image_get_any_layer(naviD->active_imageid);
       return_vals = gimp_run_procedure ("plug_in_gap_video_edit_copy",
                                   &nreturn_vals,
                                   GIMP_PDB_INT32,    GIMP_RUN_NONINTERACTIVE,
                                   GIMP_PDB_IMAGE,    naviD->active_imageid,
                                   GIMP_PDB_DRAWABLE, dummy_layer_id,
                                   GIMP_PDB_INT32,    range_list->from,
                                   GIMP_PDB_INT32,    range_list->to,
                                  GIMP_PDB_END);
       if (return_vals[0].data.d_status != GIMP_PDB_SUCCESS)
       {
         vid_copy_ok = FALSE;
       }
       gimp_destroy_params(return_vals, nreturn_vals);

       if(!vid_copy_ok)
       {
         gap_arr_msg_win(GIMP_RUN_INTERACTIVE, _("Video copy (or cut) operation failed"));
         break;
       }

       range_list = range_list->next;
    }

    if((cut_flag) && (vid_copy_ok))
    {
      range_list2 = navi_get_last_range_list(naviD->sel_range_list);
      while(range_list2)
      {
         /* for delete we process the ranges from high frame_nummers
          * downto low frame_numbers.
          * (the sel_range_list is sorted by ascending from-framenumbers at creation
          *  therefore we step from the last range liste elem backto the 1st
          *  using the prev pointers in the list)
          */
         if(gap_debug) printf("Delete Range from:%d  to:%d\n"
                       ,(int)range_list2->from ,(int)range_list2->to );

         dummy_layer_id = gap_image_get_any_layer(naviD->active_imageid);
         return_vals = gimp_run_procedure ("plug_in_gap_goto",
                                      &nreturn_vals,
                                      GIMP_PDB_INT32,    GIMP_RUN_NONINTERACTIVE,
                                      GIMP_PDB_IMAGE,    naviD->active_imageid,
                                      GIMP_PDB_DRAWABLE, dummy_layer_id,
                                      GIMP_PDB_INT32,    range_list2->from,
                                      GIMP_PDB_END);
         if (return_vals[0].data.d_status == GIMP_PDB_SUCCESS)
         {
            naviD->active_imageid = return_vals[1].data.d_image;
            gimp_destroy_params(return_vals, nreturn_vals);
            dummy_layer_id = gap_image_get_any_layer(naviD->active_imageid);
            return_vals = gimp_run_procedure ("plug_in_gap_del",
                                     &nreturn_vals,
                                     GIMP_PDB_INT32,    GIMP_RUN_NONINTERACTIVE,
                                     GIMP_PDB_IMAGE,    naviD->active_imageid,
                                     GIMP_PDB_DRAWABLE, dummy_layer_id,
                                     GIMP_PDB_INT32,    1 + (range_list2->to - range_list2->from), /* number of frames to delete */
                                    GIMP_PDB_END);
            if (return_vals[0].data.d_status == GIMP_PDB_SUCCESS)
            {
              naviD->active_imageid = return_vals[1].data.d_image;
            }
            gimp_destroy_params(return_vals, nreturn_vals);
         }
         else
         {
            gimp_destroy_params(return_vals, nreturn_vals);
            gap_arr_msg_win(GIMP_RUN_INTERACTIVE, _("Video cut operation failed"));
         }

        range_list2 = range_list2->prev;
      }
    }

    /* selection is cleared after copy/cut operations */
    navi_drop_sel_range_list();

    navi_update_after_goto();
  }
}       /* end navi_vid_copy_and_cut */



/* ---------------------------------
 * navi_get_preview_size
 * ---------------------------------
 */
static gint32
navi_get_preview_size(void)
{
  char *value_string;
  gint32 preview_size;

  preview_size = 32;  /* default preview size if nothing is configured */
  value_string = gimp_gimprc_query("video-preview-size");
  if(value_string == NULL)
  {
    value_string = gimp_gimprc_query("layer-preview-size");
  }

  if(value_string)
  {
    if(gap_debug) printf("navi_get_preview_size value_str:%s:\n", value_string);

     if (strcmp (value_string, "none") == 0)
        preview_size = 0;
      else if (strcmp (value_string, "tiny") == 0)
        preview_size = 24;
      else if (strcmp (value_string, "small") == 0)
        preview_size = 32;
      else if (strcmp (value_string, "medium") == 0)
        preview_size = 48;
      else if (strcmp (value_string, "large") == 0)
        preview_size = 64;
      else if (strcmp (value_string, "extra-large") == 0)
        preview_size = 128;
      else if (strcmp (value_string, "huge") == 0)
        preview_size = 160;
      else if (strcmp (value_string, "enormous") == 0)
        preview_size = 192;
      else if (strcmp (value_string, "gigantic") == 0)
        preview_size = 256;
      else
        preview_size = atol(value_string);

    g_free(value_string);
  }
  else
  {
    if(gap_debug) printf("navi_get_preview_size value_str is NULL\n");
  }

  return (preview_size);
}  /* navi_get_preview_size */


/* ---------------------------------
 * navi_check_exist_first_and_last
 * ---------------------------------
 */
static gint
navi_check_exist_first_and_last(GapAnimInfo *ainfo_ptr)
{
  char *l_fname;

  l_fname = gap_lib_alloc_fname(ainfo_ptr->basename,
                          ainfo_ptr->last_frame_nr,
                          ainfo_ptr->extension);
  if (!gap_lib_file_exists(l_fname))
  {
     g_free(l_fname);
     return(FALSE);
  }
  g_free(l_fname);

  l_fname = gap_lib_alloc_fname(ainfo_ptr->basename,
                          ainfo_ptr->first_frame_nr,
                          ainfo_ptr->extension);
  if (!gap_lib_file_exists(l_fname))
  {
     g_free(l_fname);
     return(FALSE);
  }
  g_free(l_fname);

  l_fname = gap_lib_alloc_fname(ainfo_ptr->basename,
                          ainfo_ptr->last_frame_nr+1,
                          ainfo_ptr->extension);
  if (gap_lib_file_exists(l_fname))
  {
     g_free(l_fname);
     return(FALSE);
  }
  g_free(l_fname);

  l_fname = gap_lib_alloc_fname(ainfo_ptr->basename,
                          ainfo_ptr->first_frame_nr-1,
                          ainfo_ptr->extension);
  if (gap_lib_file_exists(l_fname))
  {
     g_free(l_fname);
     return(FALSE);
  }
  g_free(l_fname);

  return(TRUE);
}  /* end navi_check_exist_first_and_last */


/* ---------------------------------
 * navi_get_ainfo
 * ---------------------------------
 */
static GapAnimInfo *
navi_get_ainfo(gint32 image_id, GapAnimInfo *old_ainfo_ptr)
{
  GapAnimInfo *ainfo_ptr;
  ainfo_ptr = gap_lib_alloc_ainfo(image_id, GIMP_RUN_NONINTERACTIVE);
  if(ainfo_ptr)
  {
     if(old_ainfo_ptr)
     {
        if((old_ainfo_ptr->image_id == image_id)
        && (strcmp(old_ainfo_ptr->basename, ainfo_ptr->basename) == 0))
        {
           if(navi_check_exist_first_and_last(old_ainfo_ptr))
           {
              /* - image_id and name have not changed,
               * - first and last frame still exist
               *   and are still first and last frame
               * In that case we can reuse first and last frame_nr
               * without scanning the directory for frames
               */
              ainfo_ptr->first_frame_nr = old_ainfo_ptr->first_frame_nr;
              ainfo_ptr->last_frame_nr  = old_ainfo_ptr->last_frame_nr;
              ainfo_ptr->frame_cnt      = old_ainfo_ptr->frame_cnt;
              return(ainfo_ptr);
           }
        }
     }
     gap_lib_dir_ainfo(ainfo_ptr);
  }
  navi_dyn_adj_set_limits();
  return(ainfo_ptr);
}  /* navi_get_ainfo */


/* ---------------------------------
 * navi_reload_ainfo_force
 * ---------------------------------
 */
void
navi_reload_ainfo_force(gint32 image_id)
{
  GapAnimInfo *old_ainfo_ptr;
  char frame_nr_to_char[20];
  gboolean is_alive;
  
  is_alive = gap_image_is_alive(image_id);
  
  if(gap_debug)
  {
     printf("navi_reload_ainfo_force image_id:%d is_alive:%d\n"
      , (int)image_id
      , (int)is_alive
      );
  }
  
  if (is_alive != TRUE)
  {
    return;
  }
  
  old_ainfo_ptr = naviD->ainfo_ptr;
  naviD->active_imageid = image_id;
  naviD->ainfo_ptr = navi_get_ainfo(image_id, old_ainfo_ptr);

  if((strcmp(naviD->ainfo_ptr->extension, ".xcf") != 0)
  && (strcmp(naviD->ainfo_ptr->extension, ".xcfgz") != 0)
  && (global_old_active_imageid != image_id))
  {
     /* for other frameformats than xcf
      * save the frame a 1st time (to set filetype specific save paramters)
      * this also is a workaround for a BUG that causes an X11 deadlock
      * when the save dialog pops up from the double-click callback in the frames listbox
      */
     suspend_gimage_notify++;
     gap_lib_save_named_frame(image_id, naviD->ainfo_ptr->old_filename);
     suspend_gimage_notify--;
  }
  global_old_active_imageid = image_id;

  if(naviD->framerange_number_label)
  {
     g_snprintf(frame_nr_to_char, sizeof(frame_nr_to_char), "%06d - %06d"
            , (int)naviD->ainfo_ptr->first_frame_nr
            , (int)naviD->ainfo_ptr->last_frame_nr );
     gtk_label_set_text (GTK_LABEL (naviD->framerange_number_label),
                         frame_nr_to_char);
  }

  navi_dyn_adj_set_limits();

  if(old_ainfo_ptr) gap_lib_free_ainfo(&old_ainfo_ptr);

}  /* end navi_reload_ainfo_force */


/* ---------------------------------
 * navi_reload_ainfo
 * ---------------------------------
 */
void
navi_reload_ainfo(gint32 image_id)
{
  if(gap_debug) printf("navi_reload_ainfo  image_id:%d\n", (int)image_id);


  if(image_id < 0)
  {
    navi_reload_ainfo_force(naviD->any_imageid);
  }
  else
  {
    gint32 l_new_image_id;
    gint32 l_pid;

    /* check for changes in the active_image table
     * (this table is updated automatically by gap_goto and other gap procedures
     *  to inform the navigator dialog about changes of the active image_id)
     */
    l_pid = gap_base_getpid();
    l_new_image_id = gap_navat_get_active_image(image_id, l_pid);
    if(l_new_image_id >= 0)
    {
      gap_navat_set_active_image(l_new_image_id, l_pid);
      image_id = l_new_image_id;
      global_old_active_imageid = l_new_image_id;
    }
    navi_reload_ainfo_force(image_id);
  }

  if(naviD->ainfo_ptr)
  {
    if(naviD->vin_ptr) g_free(naviD->vin_ptr);
    naviD->vin_ptr = gap_vin_get_all(naviD->ainfo_ptr->basename);

    if(naviD->framerate_adj != NULL)
    {
      gtk_adjustment_set_value(GTK_ADJUSTMENT(naviD->framerate_adj), (gfloat)naviD->vin_ptr->framerate);
    }
    if(naviD->timezoom_adj != NULL)
    {
      gtk_adjustment_set_value(GTK_ADJUSTMENT(naviD->timezoom_adj), (gfloat)naviD->vin_ptr->timezoom);
    }
    p_set_acl_tracking_radio_buttons(naviD);
  }
}  /* end navi_reload_ainfo */



/* ---------------------------------
 * navi_images_menu_constrain
 * ---------------------------------
 */
static gboolean
navi_images_menu_constrain(gint32 image_id, gpointer data)
{
  if(gap_debug) printf("navi_images_menu_constrain PROCEDURE imageID:%d\n", (int)image_id);

  if(gap_lib_get_frame_nr(image_id) < 0)
  {
      return(FALSE);  /* reject images without frame number */
  }
  if(naviD)
  {
    if(naviD->active_imageid < 0)
    {
      /* if there is no valid active_imageid
       * we nominate the first one that comes along
       */
      naviD->any_imageid = image_id;
    }
  }
  return(TRUE);
} /* end  navi_images_menu_constrain */


/* ---------------------------------
 * navi_images_menu_callback
 * ---------------------------------
 */
static void
navi_images_menu_callback  (GtkWidget *widget, gpointer data)
{
  gint32 image_id;
  gint   value;

  gimp_int_combo_box_get_active (GIMP_INT_COMBO_BOX (widget), &value);
  image_id = (gint32)value;


  if(gap_debug) printf("navi_images_menu_callback PROCEDURE imageID:%d\n", (int)image_id);

  if(naviD)
  {
    if(naviD->active_imageid != image_id)
    {
      navi_reload_ainfo(image_id);
      navi_dialog_update(NUPD_FRAME_NR_CHANGED | NUPD_PREV_LIST);
      navi_drop_sel_range_list();
      navi_scroll_to_current_frame_nr();
    }

  }
}  /* end navi_images_menu_callback */


/* ---------------------------------
 * navi_set_waiting_cursor
 * ---------------------------------
 */
static void
navi_set_waiting_cursor(void)
{
  if(naviD == NULL) return;
  if(GTK_WIDGET(naviD->shell)->window == NULL) return;

  naviD->waiting_cursor = TRUE;
  gdk_window_set_cursor(GTK_WIDGET(naviD->shell)->window, naviD->cursor_wait);
  gdk_flush();

  /* g_main_context_iteration makes sure that waiting cursor is displayed */
  while(g_main_context_iteration(NULL, FALSE));

  gdk_flush();
}  /* end navi_set_waiting_cursor */

/* ---------------------------------
 * navi_set_active_cursor
 * ---------------------------------
 */
static void
navi_set_active_cursor(void)
{
  if(naviD == NULL) return;
  if(GTK_WIDGET(naviD->shell)->window == NULL) return;

  naviD->waiting_cursor = FALSE;
  gdk_window_set_cursor(GTK_WIDGET(naviD->shell)->window, naviD->cursor_acitve);
  gdk_flush();
}  /* end navi_set_active_cursor */

/* --------------------------------
 * navi_scroll_to_current_frame_nr
 * --------------------------------
 */
static void
navi_scroll_to_current_frame_nr(void)
{
  gint32 adj_intval;

  if(naviD == NULL) return;
  if(naviD->ainfo_ptr == NULL) return;
  if(naviD->vin_ptr == NULL) return;
  if(naviD->dyn_table == NULL) return;

  adj_intval = (gint32)(GTK_ADJUSTMENT(naviD->dyn_adj)->value + 0.5);

  if(gap_debug)
  {
    printf("navi_scroll_to_current_frame_nr: BEGIN timezoom:%d, item_height:%d\n"
           "  curr_frame_nr: %d  dyn_topframenr: %d  dyn_rows:%d dyn_adj: %d\n"
          , (int)naviD->vin_ptr->timezoom
          , (int)naviD->item_height
          , (int)naviD->ainfo_ptr->curr_frame_nr
          , (int)naviD->dyn_topframenr
          , (int)naviD->dyn_rows
          , (int)adj_intval
          );
  }


  if((naviD->ainfo_ptr->curr_frame_nr >= naviD->dyn_topframenr)
  && (naviD->ainfo_ptr->curr_frame_nr < naviD->dyn_topframenr + (naviD->dyn_rows * naviD->vin_ptr->timezoom)) )
  {
     navi_dyn_adj_set_pos();
     /* current frame is within in the currently displayed range */
     return;
  }

  navi_dyn_adj_set_limits();
  naviD->dyn_topframenr = navi_constrain_dyn_topframenr(naviD->ainfo_ptr->curr_frame_nr);

  /* fetch and render all thumbnail_previews in the dyn table */
  navi_refresh_dyn_table(naviD->dyn_topframenr);
  navi_dyn_adj_set_pos();

}  /* end navi_scroll_to_current_frame_nr */


/* -----------------------------------
 * navi_scroll_event_cb
 * -----------------------------------
 * handling of ctrl-c, ctrl-x, ctrl-v, ctrl-z keys
 * pressed when focus is somewhere in the tabw frame_with_name.
 */
static gboolean
navi_scroll_event_cb ( GtkWidget *widget
                    , GdkEventScroll *sevent
                    , NaviDialog *naviD
                    )
{
  gint32  value;


  if(naviD == NULL) { return FALSE;}

  value = (gint32)GTK_ADJUSTMENT(naviD->dyn_adj)->value;

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
  gtk_adjustment_set_value(GTK_ADJUSTMENT(naviD->dyn_adj), (gdouble)value);

  return FALSE;
}  /* end navi_scroll_event_cb */



/* ---------------------------------
 * navi_dialog_tooltips
 * ---------------------------------
 */
static void
navi_dialog_tooltips(void)
{
  if(naviD == NULL) { return; }

  naviD->tooltip_on = gap_base_check_tooltips(&naviD->tooltip_on);

}  /* end navi_dialog_tooltips */


/* ---------------------------------
 * navi_find_OpenFrameList
 * ---------------------------------
 */
static gint
navi_find_OpenFrameList(OpenFrameImages *search_item)
{
  OpenFrameImages *item_list;

  item_list = naviD->OpenFrameImagesList;
  while(item_list)
  {
     if((item_list->image_id == search_item->image_id)
     && (item_list->frame_nr == search_item->frame_nr))
     {
       return(TRUE);  /* item found in the list */
     }
    item_list = (OpenFrameImages *)item_list->next;
  }
  return(FALSE);
}  /* end navi_find_OpenFrameList */


/* ---------------------------------
 * navi_free_OpenFrameList
 * ---------------------------------
 */
static void
navi_free_OpenFrameList(OpenFrameImages *list)
{
  OpenFrameImages *item_list;
  OpenFrameImages *item_next;

  item_list = list;
  while(item_list)
  {
    item_next = (OpenFrameImages *)item_list->next;
    g_free(item_list);
    item_list = item_next;
  }
}  /* end navi_free_OpenFrameList */


/* ---------------------------------
 * navi_check_image_menu_changes
 * ---------------------------------
 */
static gint
navi_check_image_menu_changes()
{
  int nimages;
  gint32 *images;
  gint32  frame_nr;
  int  i;
  gint l_rc;
  int  item_count;
  OpenFrameImages *item_list;
  OpenFrameImages *new_item;

  l_rc = TRUE;
  if(naviD->OpenFrameImagesList == NULL)
  {
    l_rc = FALSE;
  }
  item_list = NULL;
  item_count = 0;
  images = gimp_image_list (&nimages);
  for (i = 0; i < nimages; i++)
  {
     frame_nr = gap_lib_get_frame_nr(images[i]);  /* check for video frame */
     if(frame_nr >= 0)
     {
        item_count++;
        new_item = g_new (OpenFrameImages, 1);
        new_item->image_id = images[i];
        new_item->frame_nr = frame_nr;
        new_item->next = item_list;
        item_list = new_item;
        if(!navi_find_OpenFrameList(new_item))
        {
          l_rc = FALSE;
        }
     }
  }
  g_free(images);

  if(item_count != naviD->OpenFrameImagesCount)
  {
    l_rc = FALSE;
  }

  if(l_rc == TRUE)
  {
    navi_free_OpenFrameList(item_list);
  }
  else
  {
    navi_free_OpenFrameList(naviD->OpenFrameImagesList);
    naviD->OpenFrameImagesCount = item_count;
    naviD->OpenFrameImagesList = item_list;
  }
  return(l_rc);
}  /* end navi_check_image_menu_changes */


/* ---------------------------------
 * navi_refresh_image_menu
 * ---------------------------------
 */
static gint
navi_refresh_image_menu()
{
  if(naviD)
  {
    if(!navi_check_image_menu_changes())
    {
      GtkWidget *old_combo;

      if(gap_debug) printf("navi_refresh_image_menu ** BEGIN REFRESH\n");
      if(naviD->OpenFrameImagesCount != 0)
      {
        gtk_widget_set_sensitive(naviD->vbox, TRUE);
      }

      old_combo = naviD->image_combo;
      naviD->image_combo = gimp_image_combo_box_new(navi_images_menu_constrain, naviD);

      if(old_combo)
      {
        /* drop the old combo box */
        gtk_widget_destroy(old_combo);
      }

      gtk_box_pack_start (GTK_BOX (naviD->image_combo_hbox)
                         , naviD->image_combo
                         , TRUE, TRUE, 0);
      gtk_widget_show (naviD->image_combo);

      gimp_int_combo_box_connect (GIMP_INT_COMBO_BOX (naviD->image_combo),
                              naviD->active_imageid,                      /* initial value */
                              G_CALLBACK (navi_images_menu_callback),
                              naviD);

      if(naviD->OpenFrameImagesCount == 0)
      {
        gtk_widget_set_sensitive(naviD->vbox, FALSE);
      }
      return(TRUE);
    }
  }
  return(FALSE);
}  /* end navi_refresh_image_menu */



/* ---------------------------------
 * navi_update_after_goto
 * ---------------------------------
 */
void
navi_update_after_goto(void)
{
   if(naviD)
   {
      navi_dialog_update(NUPD_IMAGE_MENU | NUPD_FRAME_NR_CHANGED);
      navi_scroll_to_current_frame_nr();
   }
   gimp_displays_flush();
   navi_set_active_cursor();
}  /* end navi_update_after_goto */



/* ---------------------------------
 * navi_ops_menu_set_sensitive
 * ---------------------------------
 */
static void
navi_ops_menu_set_sensitive(void)
{
  if(naviD == NULL)
  {
    return;
  }
  if(gap_vid_edit_framecount() > 0)
  {
     gtk_widget_set_sensitive(naviD->pastea_menu_item, TRUE);
     gtk_widget_set_sensitive(naviD->pasteb_menu_item, TRUE);
     gtk_widget_set_sensitive(naviD->paster_menu_item, TRUE);
     gtk_widget_set_sensitive(naviD->clrpaste_menu_item, TRUE);
  }
  else
  {
     gtk_widget_set_sensitive(naviD->pastea_menu_item, FALSE);
     gtk_widget_set_sensitive(naviD->pasteb_menu_item, FALSE);
     gtk_widget_set_sensitive(naviD->paster_menu_item, FALSE);
     gtk_widget_set_sensitive(naviD->clrpaste_menu_item, FALSE);
  }

  if(naviD->sel_range_list)
  {
     gtk_widget_set_sensitive(naviD->copy_menu_item, TRUE);
     gtk_widget_set_sensitive(naviD->cut_menu_item, TRUE);
  }
  else
  {
     gtk_widget_set_sensitive(naviD->copy_menu_item, FALSE);
     gtk_widget_set_sensitive(naviD->cut_menu_item, FALSE);
  }
}       /* end navi_ops_menu_set_sensitive */


/* ---------------------------------
 * navi_ops_buttons_set_sensitive
 * ---------------------------------
 */
static void
navi_ops_buttons_set_sensitive(void)
{
  if(naviD == NULL)
  {
    return;
  }
  if(naviD->sel_range_list)
  {
     if(naviD->del_button) gtk_widget_set_sensitive(naviD->del_button, TRUE);
     if(naviD->dup_button) gtk_widget_set_sensitive(naviD->dup_button, TRUE);
  }
  else
  {
     if(naviD->del_button) gtk_widget_set_sensitive(naviD->del_button, FALSE);
     if(naviD->dup_button) gtk_widget_set_sensitive(naviD->dup_button, FALSE);
  }
}       /* end navi_ops_buttons_set_sensitive */

/* ---------------------------------
 * navi_pviews_reset
 * ---------------------------------
 */
static void
navi_pviews_reset(void)
{
  gint l_row;

  for(l_row = 0; l_row < naviD->dyn_rows; l_row++)
  {
    FrameWidget *fw;

    fw = &naviD->frame_widget_tab[l_row];
    fw->frame_timestamp = 0;

    gap_pview_drop_repaint_buffers(fw->pv_ptr);
  }
}  /* end navi_pviews_reset */


/* ---------------------------------
 * navi_thumb_update
 * ---------------------------------
 * update thumbnailfiles on disk
 * IN: update_all TRUE force update on all frames
 */
static void
navi_thumb_update(gboolean update_all)
{
  gint32 l_frame_nr;
  gint32 l_image_id;
  gint   l_upd_flag;
  gint   l_any_upd_flag;
  char  *l_image_filename;
  static gboolean l_msg_win_alrady_open = FALSE;


  if(naviD == NULL) return;
  if(naviD->ainfo_ptr == NULL) return;

  if(l_msg_win_alrady_open)
  {
    return;
  }


  if(!gap_thumb_thumbnailsave_is_on())
  {
    l_msg_win_alrady_open = TRUE;
    gap_arr_msg_win(GIMP_RUN_INTERACTIVE
             , _("For the thumbnail update you have to select\n"
                 "a thumbnail filesize other than 'No Thumbnails'\n"
                 "in the environment section of the preferences dialog")
             );
    l_msg_win_alrady_open = FALSE;
    return;
  }

  navi_set_waiting_cursor();

  l_any_upd_flag = FALSE;
  for(l_frame_nr = naviD->ainfo_ptr->first_frame_nr;
      l_frame_nr <= naviD->ainfo_ptr->last_frame_nr;
      l_frame_nr++)
  {
     l_upd_flag = TRUE;
     l_image_filename = gap_lib_alloc_fname(naviD->ainfo_ptr->basename, l_frame_nr, naviD->ainfo_ptr->extension);

     if(!update_all)
     {
       gint32  l_th_width;
       gint32  l_th_height;
       gint32  l_th_data_count;
       gint32  l_th_bpp;
       guchar *l_raw_thumb;

       /* init preferred width and height
        * (as hint for the thumbnail loader to decide
        *  if thumbnail is to fetch from normal or large thumbnail directory
        *  just for the case when both sizes are available)
        */
       l_th_width = naviD->preview_size;
       l_th_height = naviD->preview_size;

       l_raw_thumb = NULL;
       if(TRUE == gap_thumb_file_load_thumbnail(l_image_filename
                                  , &l_th_width
                                  , &l_th_height
                                  , &l_th_data_count
                                  , &l_th_bpp
                                  , &l_raw_thumb
                                  ))

       {
         /* Thumbnail load failed, so an update is needed to create one */
         l_upd_flag = FALSE;
       }
       else
       {
         /* Thumbnail load succeeded, update is not needed */
         if(l_raw_thumb)
         {
           g_free(l_raw_thumb);
         }
       }
     }

     if(l_upd_flag)
     {
        l_any_upd_flag = TRUE;
        if(gap_debug) printf("navi_thumb_update frame_nr:%d\n", (int)l_frame_nr);
        l_image_id = gap_lib_load_image(l_image_filename);
        gap_pdb_gimp_file_save_thumbnail(l_image_id, l_image_filename);
        gimp_image_delete(l_image_id);
     }

     if(l_image_filename) g_free(l_image_filename);
  }

  if(l_any_upd_flag  )
  {
    /* forget about the previous chached thumbnials
     * (if we had no thumbnails before, the cache holds only the default icons
     *  but now we generated real thumbnails without changing the timestamp
     *  of the oroginal file)
     */
    navi_pviews_reset();

    /* fetch and render all thumbnail_previews in the dyn table */
    navi_refresh_dyn_table(naviD->dyn_topframenr);
  }
   navi_set_active_cursor();
}  /* end navi_thumb_update */



static void
navi_dialog_thumb_update_callback(GtkWidget *w, gpointer   data)
{
  if(gap_debug) printf("navi_dialog_thumb_update_callback\n");
  navi_thumb_update(FALSE);
}

static void
navi_dialog_thumb_updateall_callback(GtkWidget *w, gpointer   data)
{
  if(gap_debug) printf("navi_dialog_thumb_updateall_callback\n");
  navi_thumb_update(TRUE);
}


/* ---------------------------------
 * navi_playback
 * ---------------------------------
 */
static void
navi_playback(gboolean use_gimp_layeranimplayer)
{
   SelectedRange *range_list;
   gint32         l_from;
   gint32         l_to;

   gint32         l_new_image_id;
   GimpParam     *return_vals;
   int            nreturn_vals;
   char           l_frame_name[50];
   int            l_frame_delay;
   gint32         dummy_layer_id;


  if(gap_debug) printf("navi_dialog_vcr_play_callback\n");

  navi_set_waiting_cursor();

  strcpy(l_frame_name, "frame_[######]");
  if(naviD->vin_ptr)
  {
    if(naviD->vin_ptr->framerate > 0)
    {
       l_frame_delay = 1000 / naviD->vin_ptr->framerate;
       g_snprintf(l_frame_name, sizeof(l_frame_name), "frame_[######] (%dms)", (int)l_frame_delay);
    }
  }

  l_from = naviD->ainfo_ptr->first_frame_nr;
  l_to   = naviD->ainfo_ptr->last_frame_nr;

  range_list = naviD->sel_range_list;
  if(range_list)
  {
     l_to   = naviD->ainfo_ptr->first_frame_nr;
     l_from = naviD->ainfo_ptr->last_frame_nr;

     while(range_list)
     {
        l_from = MIN(l_from, range_list->from);
        l_to   = MAX(l_to,   range_list->to);
        range_list = range_list->next;
     }
  }

  if(!use_gimp_layeranimplayer)
  {
     /* Start GAP video frame Playback Module via PDB
      * note: the player always rund INTERACTIVE
      * but accepts calling parameters only when called in
      * GIMP_RUN_NONINTERACTIVE runmode
      */
     dummy_layer_id = gap_image_get_any_layer(naviD->active_imageid);
     return_vals = gimp_run_procedure ("plug_in_gap_videoframes_player",
                                    &nreturn_vals,
                                    GIMP_PDB_INT32,    GIMP_RUN_NONINTERACTIVE,
                                    GIMP_PDB_IMAGE,    naviD->active_imageid,
                                    GIMP_PDB_DRAWABLE, dummy_layer_id,
                                    GIMP_PDB_INT32,    l_from,
                                    GIMP_PDB_INT32,    l_to,
                                    GIMP_PDB_INT32,    TRUE,  /* autostart */
                                    GIMP_PDB_INT32,    TRUE,  /* use_thumbnails (playback using thumbnails where available) */
                                    GIMP_PDB_INT32,    TRUE,  /* exact_timing */
                                    GIMP_PDB_INT32,    TRUE,  /* play_selection_only */
                                    GIMP_PDB_INT32,    TRUE,  /* play_loop */
                                    GIMP_PDB_INT32,    FALSE, /* play_pingpong */
                                    GIMP_PDB_FLOAT,    -1.0,  /* speed is original framerate */
                                    GIMP_PDB_INT32,    -1,    /* use default width */
                                    GIMP_PDB_INT32,    -1,    /* use default height */
                                    GIMP_PDB_INT32,    FALSE, /* audio_enable (DISABLED) */
                                    GIMP_PDB_STRING,   "\0",  /* audio_filename */
                                    GIMP_PDB_INT32,    0,     /* audio_frame_offset */
                                    GIMP_PDB_FLOAT,    1.0,   /* audio_volume */
                                    GIMP_PDB_END);
     gimp_destroy_params(return_vals, nreturn_vals);
     navi_set_active_cursor();
     return;
  }


  dummy_layer_id = gap_image_get_any_layer(naviD->active_imageid);
  return_vals = gimp_run_procedure ("plug_in_gap_range_to_multilayer",
                                    &nreturn_vals,
                                    GIMP_PDB_INT32,    GIMP_RUN_NONINTERACTIVE,
                                    GIMP_PDB_IMAGE,    naviD->active_imageid,
                                    GIMP_PDB_DRAWABLE, dummy_layer_id,
                                    GIMP_PDB_INT32,    l_from,
                                    GIMP_PDB_INT32,    l_to,
                                    GIMP_PDB_INT32,    3,     /* flatten image */
                                    GIMP_PDB_INT32,    1,     /* BG_VISIBLE */
                                    GIMP_PDB_INT32,    (gint32)naviD->vin_ptr->framerate,
                                    GIMP_PDB_STRING,   l_frame_name,
                                    GIMP_PDB_INT32,    6,     /* use all visible layers */
                                    GIMP_PDB_INT32,    0,     /* ignore case */
                                    GIMP_PDB_INT32,    0,     /* normal selection (no invert) */
                                    GIMP_PDB_STRING,   "0",   /* select string (ignored) */
                                    GIMP_PDB_INT32,    0,     /* use full layersize, ignore selection */
                                   GIMP_PDB_END);

  if (return_vals[0].data.d_status == GIMP_PDB_SUCCESS)
  {
     l_new_image_id = return_vals[1].data.d_image;
     gimp_destroy_params(return_vals, nreturn_vals);

     /* TODO: here we should start a thread for the playback,
      * so the navigator is not blocked until playback exits
      */
      dummy_layer_id = gap_image_get_any_layer(l_new_image_id);
      return_vals = gimp_run_procedure ("plug_in_animationplay",
                                    &nreturn_vals,
                                    GIMP_PDB_INT32,    GIMP_RUN_NONINTERACTIVE,
                                    GIMP_PDB_IMAGE,    l_new_image_id,
                                    GIMP_PDB_DRAWABLE, dummy_layer_id,
                                   GIMP_PDB_END);
      gimp_destroy_params(return_vals, nreturn_vals);
  }
  else
  {
     gimp_destroy_params(return_vals, nreturn_vals);
  }
  navi_set_active_cursor();
}  /* end navi_playback */


static void
navi_dialog_vcr_play_callback(GtkWidget *w, gpointer   data)
{
  if(gap_debug) printf("navi_dialog_vcr_play_callback\n");
  navi_playback(FALSE /* dont use_gimp_layeranimplayer */);
}

static void
navi_dialog_vcr_play_layeranim_callback(GtkWidget *w, gpointer   data)
{
  if(gap_debug) printf("navi_dialog_vcr_play_layeranim_callback\n");
  navi_playback(TRUE /* use_gimp_layeranimplayer */);
}


/* -------------------------------------------
 * navi_dialog_frames_duplicate_frame_callback
 * -------------------------------------------
 */
static void
navi_dialog_frames_duplicate_frame_callback(GtkWidget *w, gpointer   data)
{
   SelectedRange *range_list;
   GimpParam     *return_vals;
   int            nreturn_vals;
   gint32         dummy_layer_id;


  if(gap_debug) printf("navi_dialog_frames_duplicate_frame_callback\n");

  if(naviD->sel_range_list)
  {
    navi_set_waiting_cursor();
    range_list = navi_get_last_range_list(naviD->sel_range_list);
    while(range_list)
    {
       /* Note: process the ranges from high frame_nummers
        *       downto low frame_numbers.
        *       (the sel_range_list is sorted by ascending from framenumbers
        *        therefore we step from the last element back to the 1st)
        */
       if(gap_debug) printf("Duplicate Range from:%d  to:%d\n"
                     ,(int)range_list->from ,(int)range_list->to );

       dummy_layer_id = gap_image_get_any_layer(naviD->active_imageid);
       return_vals = gimp_run_procedure ("plug_in_gap_goto",
                                    &nreturn_vals,
                                    GIMP_PDB_INT32,    GIMP_RUN_NONINTERACTIVE,
                                    GIMP_PDB_IMAGE,    naviD->active_imageid,
                                    GIMP_PDB_DRAWABLE, dummy_layer_id,
                                    GIMP_PDB_INT32,    range_list->from,
                                    GIMP_PDB_END);
       if (return_vals[0].data.d_status == GIMP_PDB_SUCCESS)
       {
          naviD->active_imageid = return_vals[1].data.d_image;
          gimp_destroy_params(return_vals, nreturn_vals);
          dummy_layer_id = gap_image_get_any_layer(naviD->active_imageid);
          return_vals = gimp_run_procedure ("plug_in_gap_dup",
                                   &nreturn_vals,
                                   GIMP_PDB_INT32,    GIMP_RUN_NONINTERACTIVE,
                                   GIMP_PDB_IMAGE,    naviD->active_imageid,
                                   GIMP_PDB_DRAWABLE, dummy_layer_id,
                                   GIMP_PDB_INT32,    1,     /* copy block 1 times */
                                   GIMP_PDB_INT32,    range_list->from,
                                   GIMP_PDB_INT32,    range_list->to,
                                  GIMP_PDB_END);
         if (return_vals[0].data.d_status == GIMP_PDB_SUCCESS)
         {
           naviD->active_imageid = return_vals[1].data.d_image;
         }
       }
       gimp_destroy_params(return_vals, nreturn_vals);
       range_list = range_list->prev;
    }
    navi_drop_sel_range_list();
    navi_update_after_goto();
  }
}       /* end navi_dialog_frames_duplicate_frame_callback */

/* ----------------------------------------
 * navi_dialog_frames_delete_frame_callback
 * ----------------------------------------
 */
static void
navi_dialog_frames_delete_frame_callback(GtkWidget *w, gpointer   data)
{
   SelectedRange *range_list;
   GimpParam     *return_vals;
   int            nreturn_vals;
   gint32         dummy_layer_id;


  if(gap_debug) printf("navi_dialog_frames_delete_frame_callback\n");

  if(naviD->sel_range_list)
  {
    gint32  del_cnt;

    del_cnt = 0;
    range_list = navi_get_last_range_list(naviD->sel_range_list);
    while(range_list)
    {
       del_cnt += (1 + (range_list->to - range_list->from));
       range_list = range_list->prev;
    }

    if(!navi_delete_confirm_dialog(del_cnt))
    {
      return;
    }
    navi_set_waiting_cursor();
    range_list = navi_get_last_range_list(naviD->sel_range_list);
    while(range_list)
    {
       /* Note: process the ranges from high frame_nummers
        *       downto low frame_numbers.
        *       (the sel_range_list is sorted by ascending from framenumbers
        *        therefore we step from the last element back to the 1st)
        */
       if(gap_debug) printf("Delete Range from:%d  to:%d\n"
                     ,(int)range_list->from ,(int)range_list->to );

       dummy_layer_id = gap_image_get_any_layer(naviD->active_imageid);
       return_vals = gimp_run_procedure ("plug_in_gap_goto",
                                    &nreturn_vals,
                                    GIMP_PDB_INT32,    GIMP_RUN_NONINTERACTIVE,
                                    GIMP_PDB_IMAGE,    naviD->active_imageid,
                                    GIMP_PDB_DRAWABLE, dummy_layer_id,  /* dummy */
                                    GIMP_PDB_INT32,    range_list->from,
                                    GIMP_PDB_END);
       if (return_vals[0].data.d_status == GIMP_PDB_SUCCESS)
       {
          naviD->active_imageid = return_vals[1].data.d_image;
          gimp_destroy_params(return_vals, nreturn_vals);
          dummy_layer_id = gap_image_get_any_layer(naviD->active_imageid);
          return_vals = gimp_run_procedure ("plug_in_gap_del",
                                   &nreturn_vals,
                                   GIMP_PDB_INT32,    GIMP_RUN_NONINTERACTIVE,
                                   GIMP_PDB_IMAGE,    naviD->active_imageid,
                                   GIMP_PDB_DRAWABLE, dummy_layer_id,
                                   GIMP_PDB_INT32,    1 + (range_list->to - range_list->from), /* number of frames to delete */
                                  GIMP_PDB_END);
          if (return_vals[0].data.d_status == GIMP_PDB_SUCCESS)
          {
            naviD->active_imageid = return_vals[1].data.d_image;
          }
       }
       gimp_destroy_params(return_vals, nreturn_vals);
       range_list = range_list->prev;
    }
    navi_drop_sel_range_list();
    navi_update_after_goto();
  }
}       /* end navi_dialog_frames_delete_frame_callback */


/* ---------------------------------
 * navi_dialog_goto_callback
 * ---------------------------------
 */
static void
navi_dialog_goto_callback(gint32 dst_framenr)
{
   GimpParam          *return_vals;
   int              nreturn_vals;
   gint32           dummy_layer_id;

   if(gap_debug) printf("navi_dialog_goto_callback\n");
   navi_set_waiting_cursor();
   dummy_layer_id = gap_image_get_any_layer(naviD->active_imageid);
   return_vals = gimp_run_procedure ("plug_in_gap_goto",
                                    &nreturn_vals,
                                    GIMP_PDB_INT32,    GIMP_RUN_NONINTERACTIVE,
                                    GIMP_PDB_IMAGE,    naviD->active_imageid,
                                    GIMP_PDB_DRAWABLE, dummy_layer_id,  /* dummy */
                                    GIMP_PDB_INT32,    dst_framenr,
                                    GIMP_PDB_END);
   if (return_vals[0].data.d_status == GIMP_PDB_SUCCESS)
   {
      naviD->active_imageid = return_vals[1].data.d_image;
   }
   gimp_destroy_params(return_vals, nreturn_vals);

   navi_update_after_goto();
}  /* end navi_dialog_goto_callback */


/* -----------------------------------
 * navi_dialog_vcr_goto_first_callback
 * -----------------------------------
 */
static void
navi_dialog_vcr_goto_first_callback(GtkWidget *w, gpointer   data)
{
   GimpParam          *return_vals;
   int              nreturn_vals;
   gint32           dummy_layer_id;

   if(gap_debug) printf("navi_dialog_vcr_goto_first_callback\n");
   navi_set_waiting_cursor();
   dummy_layer_id = gap_image_get_any_layer(naviD->active_imageid);
   return_vals = gimp_run_procedure ("plug_in_gap_first",
                                    &nreturn_vals,
                                    GIMP_PDB_INT32,    GIMP_RUN_NONINTERACTIVE,
                                    GIMP_PDB_IMAGE,    naviD->active_imageid,
                                    GIMP_PDB_DRAWABLE, dummy_layer_id,
                                    GIMP_PDB_END);
   if (return_vals[0].data.d_status == GIMP_PDB_SUCCESS)
   {
      naviD->active_imageid = return_vals[1].data.d_image;
   }
   gimp_destroy_params(return_vals, nreturn_vals);
   navi_update_after_goto();
}  /* end navi_dialog_vcr_goto_first_callback */


/* ---------------------------------
 * navi_dialog_vcr_goto_prev_callback
 * ---------------------------------
 */
static void
navi_dialog_vcr_goto_prev_callback(GtkWidget *w, gpointer   data)
{
   GimpParam          *return_vals;
   int              nreturn_vals;
   gint32           dummy_layer_id;

   if(gap_debug) printf("navi_dialog_vcr_goto_prev_callback\n");
   navi_set_waiting_cursor();
   dummy_layer_id = gap_image_get_any_layer(naviD->active_imageid);
   return_vals = gimp_run_procedure ("plug_in_gap_prev",
                                    &nreturn_vals,
                                    GIMP_PDB_INT32,    GIMP_RUN_NONINTERACTIVE,
                                    GIMP_PDB_IMAGE,    naviD->active_imageid,
                                    GIMP_PDB_DRAWABLE, dummy_layer_id,
                                    GIMP_PDB_END);
   if (return_vals[0].data.d_status == GIMP_PDB_SUCCESS)
   {
      naviD->active_imageid = return_vals[1].data.d_image;
   }
   gimp_destroy_params(return_vals, nreturn_vals);
   navi_update_after_goto();
}  /* end navi_dialog_vcr_goto_prev_callback */

/* ---------------------------------
 * navi_dialog_goto_callback
 * ---------------------------------
 */
static void
navi_dialog_vcr_goto_prevblock_callback(GtkWidget *w, gpointer   data)
{
   gint32           dst_framenr;

   if(gap_debug) printf("navi_dialog_vcr_goto_prevblock_callback\n");
   if(naviD->ainfo_ptr == NULL) navi_reload_ainfo(naviD->active_imageid);
   if(naviD->ainfo_ptr == NULL) return;
   if(naviD->vin_ptr == NULL) return;
   dst_framenr = MAX(naviD->ainfo_ptr->curr_frame_nr - naviD->vin_ptr->timezoom,
                     naviD->ainfo_ptr->first_frame_nr);

   navi_dialog_goto_callback(dst_framenr);
}  /* end navi_dialog_goto_callback */

/* ----------------------------------
 * navi_dialog_vcr_goto_next_callback
 * ----------------------------------
 */
static void
navi_dialog_vcr_goto_next_callback(GtkWidget *w, gpointer   data)
{
   GimpParam       *return_vals;
   int              nreturn_vals;
   gint32           dummy_layer_id;

   if(gap_debug) printf("navi_dialog_vcr_goto_next_callback\n");
   navi_set_waiting_cursor();
   dummy_layer_id = gap_image_get_any_layer(naviD->active_imageid);
   return_vals = gimp_run_procedure ("plug_in_gap_next",
                                    &nreturn_vals,
                                    GIMP_PDB_INT32,    GIMP_RUN_NONINTERACTIVE,
                                    GIMP_PDB_IMAGE,    naviD->active_imageid,
                                    GIMP_PDB_DRAWABLE, dummy_layer_id,
                                    GIMP_PDB_END);
   gimp_destroy_params(return_vals, nreturn_vals);
   navi_update_after_goto();
}  /* end navi_dialog_vcr_goto_next_callback */

/* ---------------------------------------
 * navi_dialog_vcr_goto_nextblock_callback
 * ---------------------------------------
 */
static void
navi_dialog_vcr_goto_nextblock_callback(GtkWidget *w, gpointer   data)
{
   gint32           dst_framenr;

   if(gap_debug) printf("navi_dialog_vcr_goto_nextblock_callback\n");
   if(naviD->ainfo_ptr == NULL) navi_reload_ainfo(naviD->active_imageid);
   if(naviD->ainfo_ptr == NULL) return;
   if(naviD->vin_ptr == NULL) return;
   dst_framenr = MIN(naviD->ainfo_ptr->curr_frame_nr + naviD->vin_ptr->timezoom,
                     naviD->ainfo_ptr->last_frame_nr);

   navi_dialog_goto_callback(dst_framenr);
}  /* end navi_dialog_vcr_goto_nextblock_callback */


/* ---------------------------------
 * navi_dialog_vcr_goto_last_callback
 * ---------------------------------
 */
static void
navi_dialog_vcr_goto_last_callback(GtkWidget *w, gpointer   data)
{
   GimpParam          *return_vals;
   int              nreturn_vals;
   gint32           dummy_layer_id;

   if(gap_debug) printf("navi_dialog_vcr_goto_last_callback\n");
   navi_set_waiting_cursor();
   dummy_layer_id = gap_image_get_any_layer(naviD->active_imageid);
   return_vals = gimp_run_procedure ("plug_in_gap_last",
                                    &nreturn_vals,
                                    GIMP_PDB_INT32,    GIMP_RUN_NONINTERACTIVE,
                                    GIMP_PDB_IMAGE,    naviD->active_imageid,
                                    GIMP_PDB_DRAWABLE, dummy_layer_id,
                                    GIMP_PDB_END);
   gimp_destroy_params(return_vals, nreturn_vals);
   navi_update_after_goto();
} /* end navi_dialog_vcr_goto_last_callback */



/* ---------------------------------
 * navi_frames_timing_update
 * ---------------------------------
 */
static void
navi_frames_timing_update (void)
{
  gint32       l_frame_nr;
  gint32       l_frame_nr_prev;
  gint32       l_count;

  l_frame_nr = naviD->dyn_topframenr;

  l_frame_nr_prev = -1;
  for(l_count = 0; l_count < naviD->dyn_rows; l_count++)
  {
     FrameWidget   *fw;

     fw = &naviD->frame_widget_tab[l_count];

     l_frame_nr = CLAMP(l_frame_nr
                    , naviD->ainfo_ptr->first_frame_nr
                    , naviD->ainfo_ptr->last_frame_nr
                    );

      if(l_frame_nr == l_frame_nr_prev)
      {
        /* make sure that dummy element has no -1 (dummy frame_nr)
         * (will display a blank label)
         */
        fw->frame_nr = -1;
      }
      navi_frame_widget_time_label_update(fw);

      l_frame_nr_prev = l_frame_nr;
      l_frame_nr += naviD->vin_ptr->timezoom;
  }
}  /* end navi_frames_timing_update */


/* ---------------------------------
 * navi_framerate_spinbutton_update
 * ---------------------------------
 */
void
navi_framerate_spinbutton_update(GtkAdjustment *adjustment,
                      gpointer       data)
{
  gdouble    framerate;

  if(naviD == NULL) return;
  if(naviD->vin_ptr == NULL) return;

  framerate = (gdouble) (adjustment->value);
  if(framerate != naviD->vin_ptr->framerate)
  {
     naviD->vin_ptr->framerate = framerate;
     if(naviD->ainfo_ptr)
     {
       /* write new framerate to video info file */
       gap_vin_set_common(naviD->vin_ptr, naviD->ainfo_ptr->basename);
     }
     navi_frames_timing_update();
  }
  if(gap_debug) printf("navi_framerate_spinbutton_update :%d\n", (int)naviD->vin_ptr->framerate);
}  /* end navi_framerate_spinbutton_update */


/* ---------------------------------
 * navi_timezoom_spinbutton_update
 * ---------------------------------
 */
void
navi_timezoom_spinbutton_update(GtkAdjustment *adjustment,
                      gpointer       data)
{
  gint    timezoom;

  if(naviD == NULL) return;
  if(naviD->vin_ptr == NULL) return;

  timezoom = (int) (adjustment->value);
  if(timezoom != naviD->vin_ptr->timezoom)
  {
    naviD->vin_ptr->timezoom = timezoom;
    if(naviD->ainfo_ptr)
    {
       /* write new timezoom to video info file */
       gap_vin_set_common(naviD->vin_ptr, naviD->ainfo_ptr->basename);
    }
    frames_dialog_flush();
    navi_dyn_adj_set_limits();
    navi_dyn_adj_set_pos();
  }
  if(gap_debug) printf("navi_timezoom_spinbutton_update :%d\n", (int)naviD->vin_ptr->timezoom);
}  /* end navi_timezoom_spinbutton_update */


/* ------------------------
 * frames_dialog_flush
 * ------------------------
 */
static void
frames_dialog_flush (void)
{
  if(gap_debug) printf("frames_dialog_flush\n");
  if(naviD)
  {
    frames_dialog_update(naviD->active_imageid);
  }
}  /* end frames_dialog_flush */


/* ------------------------
 * frames_dialog_update
 * ------------------------
 */
void
frames_dialog_update (gint32 image_id)
{
  gint         l_waiting_cursor;

  if(gap_debug) printf("frames_dialog_update image_id:%d\n", (int)image_id);

  if (! naviD)   /* || (naviD->active_imageid == image_id) */
  {
    return;
  }
  l_waiting_cursor = naviD->waiting_cursor;
  if(!naviD->waiting_cursor) navi_set_waiting_cursor();

  navi_reload_ainfo(image_id);

  /*  Make sure the gimage is not notified of this change  */
  suspend_gimage_notify++;

  /*  Find the preview extents  */
  navi_preview_extents ();

  /*  Refresh all elements in dyn_table  */
  navi_refresh_dyn_table (naviD->dyn_topframenr);

  suspend_gimage_notify--;
  if(!l_waiting_cursor) navi_set_active_cursor();
}  /* end frames_dialog_update */


/* ---------------------------------
 * navi_render_preview
 * ---------------------------------
 */
static void
navi_render_preview (FrameWidget *fw)
{
   gint32  l_th_width;
   gint32  l_th_height;
   gint32  l_th_data_count;
   gint32  l_th_bpp;
   guchar *l_th_data;
   gint32  l_thumbdata_count;


   if(gap_debug) printf("navi_render_preview START\n");



   l_th_data = NULL;
   l_th_bpp = 3;                /* l_th_bpp always 3 for thumbnail_file data, but can be 4 for the actual image */
   if(naviD == NULL)            { return; }
   if(fw == NULL)               { return; }
   if(fw->pv_ptr == NULL)       { return; }
   if(naviD->ainfo_ptr == NULL) { return; }
   if(!gap_image_is_alive(naviD->active_imageid)) {return; }

   if((fw->pv_ptr->pv_width != naviD->image_width)
   || (fw->pv_ptr->pv_height != naviD->image_height))
   {
      gap_pview_set_size(fw->pv_ptr, naviD->image_width, naviD->image_height, NAVI_CHECK_SIZE);
   }

   if(naviD->ainfo_ptr->curr_frame_nr == fw->frame_nr)
   {
     /* if this is the currently open image
      * we must get the thumbnail from memory (gimp composite view)
      * rather than reading from thumbnail file
      * to get an actual version.
      */
     if(gap_debug) printf("navi_render_preview ACTUAL image BEFORE gap_pdb_gimp_image_thumbnail w:%d h:%d,\n"
                         ,(int)fw->pv_ptr->pv_width, (int)fw->pv_ptr->pv_height);
     gap_pdb_gimp_image_thumbnail(naviD->active_imageid
                           , fw->pv_ptr->pv_width
                           , fw->pv_ptr->pv_height
                           , &l_th_width
                           , &l_th_height
                           , &l_th_bpp
                           , &l_thumbdata_count
                           , &l_th_data
                           );
     if(gap_debug) printf("navi_render_preview AFTER gap_pdb_gimp_image_thumbnail th_w:%d th_h:%d, th_bpp:%d, count:%d\n"
                         ,(int)l_th_width, (int)l_th_height, (int)l_th_bpp, (int)l_thumbdata_count);
   }
   else
   {
     GStatBuf     l_stat;
     gchar       *l_frame_filename;
     gboolean     l_can_use_cached_thumbnail;
     gboolean     l_referenced_frame_exists;

     l_can_use_cached_thumbnail = FALSE;
     l_referenced_frame_exists = FALSE;
     l_frame_filename = gap_lib_alloc_fname(naviD->ainfo_ptr->basename, fw->frame_nr, naviD->ainfo_ptr->extension);
     if(l_frame_filename)
     {
       if (0 == g_stat(l_frame_filename, &l_stat))
       {
         l_referenced_frame_exists = TRUE;
         if(fw->frame_filename)
         {
           if(gap_debug)
           {
             printf("  CHECK SIZE old pv_area_data: %ld  pv_w: %d pv_h:%d  filename:%s timestamp:%d\n"
                               , (long)fw->pv_ptr->pv_area_data
                               , (int)fw->pv_ptr->pv_width
                               , (int)fw->pv_ptr->pv_height
                               , fw->frame_filename
                               , (int)fw->frame_timestamp
                               );

             printf("  CHECK SIZE new w: %d h:%d  filename:%s timestamp:%d\n"
                               , (int)naviD->image_width
                               , (int)naviD->image_height
                               , l_frame_filename
                               , (int)l_stat.st_mtime
                               );
           }


           if((strcmp(l_frame_filename,fw->frame_filename) == 0)
           && (fw->frame_timestamp == l_stat.st_mtime)
           && (fw->pv_ptr->pv_area_data)
           && (fw->pv_ptr->pv_width == naviD->image_width)
           && (fw->pv_ptr->pv_height == naviD->image_height) )
           {
             /* there was no change of the frame file since
              * we cached the thumbnail data
              * and the cached data does match in size
              * (fw->pv_ptr->pv_area_data is the pointer to the cached thumbnaildata)
              */
              l_can_use_cached_thumbnail = TRUE;
           }
           else
           {
              /* free the old filename */
              g_free(fw->frame_filename);

              /* store te new name and mtime */
              fw->frame_filename = l_frame_filename;
              fw->frame_timestamp = l_stat.st_mtime;
           }
         }
         else
         {
            /* we are going to read thumbnaildata of this frame for the 1.st time
             * store its name and mtime for next usage
             */
            fw->frame_filename = l_frame_filename;
            fw->frame_timestamp = l_stat.st_mtime;
         }
       }
     }

     if(l_can_use_cached_thumbnail)
     {
        if(gap_debug) printf("navi_render_preview: USING CACHED THUMBNAIL\n");
        gap_pview_repaint(fw->pv_ptr);
        return;
     }
     else
     {
       /* fetch l_th_data from thumbnail_file */
       if(gap_debug) printf("navi_render_preview: fetching THUMBNAILFILE for: %s\n", l_frame_filename);
       if((l_frame_filename)
       && (l_referenced_frame_exists))
       {
         /* init preferred width and height
          * (as hint for the thumbnail loader to decide
          *  if thumbnail is to fetch from normal or large thumbnail directory
          *  just for the case when both sizes are available)
          */
         l_th_width = naviD->preview_size;
         l_th_height = naviD->preview_size;
         gap_thumb_file_load_thumbnail(l_frame_filename
                                   , &l_th_width
                                   , &l_th_height
                                   , &l_th_data_count
                                   , &l_th_bpp
                                   , &l_th_data
                                   );
       }
     }
   }

   if(l_th_data)
   {
     gboolean l_th_data_was_grabbed;

     /* fetch was successful, we can call the render procedure */
     l_th_data_was_grabbed = gap_pview_render_from_buf (fw->pv_ptr
                 , l_th_data
                 , l_th_width
                 , l_th_height
                 , l_th_bpp
                 , TRUE         /* allow_grab_src_data */
                 );

     if(l_th_data_was_grabbed)
     {
       l_th_data = NULL;
     }
   }
   else
   {
     /* fetch failed, render a default icon */
     if(gap_debug) printf("navi_render_preview: fetch failed, using DEFAULT_ICON\n");
     gap_pview_render_default_icon(fw->pv_ptr);
   }


   if(l_th_data)
   {
     g_free(l_th_data);
   }

}       /* end navi_render_preview */


/* ---------------------------------
 * frame_widget_preview_redraw
 * ---------------------------------
 */
static void
frame_widget_preview_redraw (FrameWidget *fw)
{

  if(fw == NULL) { return;}
  if(fw->pv_ptr == NULL) { return;}

  gap_pview_repaint(fw->pv_ptr);

  /*  make sure the image has been transfered completely to the pixmap before
   *  we use it again...
   */
  gdk_flush ();

}  /* end frame_widget_preview_redraw */


/* ---------------------------------
 * frame_widget_preview_events
 * ---------------------------------
 * handles events for all frame_widgets (in the dyn_table)
 * - Expose of pview drawing_area
 * - Doubleclick (goto)
 * - Selections (Singleclick, and key-modifiers)
 */
static gboolean
frame_widget_preview_events (GtkWidget *widget,
                             GdkEvent  *event,
                             gpointer  user_data)
{
  /* GdkEventExpose *eevent; */
  GdkEventButton *bevent;
  FrameWidget *fw;
  /* int sx, sy, dx, dy, w, h; */

  fw = (FrameWidget *) user_data;
  if(fw == NULL)
  {
    fw = (FrameWidget *) g_object_get_data (G_OBJECT (widget), "frame_widget");
    if(fw == NULL)
    {
      return FALSE;
    }
  }

  if (fw->frame_nr < 0)
  {
    /* the frame_widget is not initialized or it is just a dummy, no action needed */
    return FALSE;
  }

  switch (event->type)
  {
    case GDK_2BUTTON_PRESS:
      if(gap_debug) printf("frame_widget_preview_events GDK_2BUTTON_PRESS (doubleclick)\n");
      navi_dialog_goto_callback(fw->frame_nr);
      return TRUE;

    case GDK_BUTTON_PRESS:
      bevent = (GdkEventButton *) event;

      if(gap_debug) printf("frame_widget_preview_events GDK_BUTTON_PRESS button:%d frame_nr:%d widget:%ld  da_wgt:%ld\n"
                              , (int)bevent->button
                              , (int)fw->frame_nr
                              , (long)widget
                              , (long)fw->pv_ptr->da_widget
                              );
      if(fw->frame_nr < 0)
      {
        /* perform no actions on dummy entries in the dyn_table */
        return TRUE;
      }

      if (bevent->button == 3)
      {
          /* right mousebutton (3) shows the PopUp Menu */
          navi_ops_menu_set_sensitive();
          naviD->paste_at_frame = fw->frame_nr;
          gtk_menu_popup (GTK_MENU (naviD->ops_menu),
                       NULL, NULL, NULL, NULL,
                       3, bevent->time);

          return TRUE;
      }

      /*  handle selctions, according to modifier keys */
      if (event->button.state & GDK_CONTROL_MASK)
      {
         if(gap_debug) printf("frame_widget_preview_events SELECTION GDK_CONTROL_MASK (ctrl modifier)\n");
         navi_sel_ctrl(fw->frame_nr);
         navi_debug_print_sel_range();
      }
      else if (event->button.state & GDK_SHIFT_MASK)
      {
         if(gap_debug) printf("frame_widget_preview_events SELECTION GDK_SHIFT_MASK  (shift modifier)\n");
         navi_sel_shift(fw->frame_nr);
         navi_debug_print_sel_range();
      }
      else if (event->button.state & GDK_MOD1_MASK)
      {
         if(gap_debug) printf("frame_widget_preview_events SELECTION GDK_MOD1_MASK  (alt modifier)\n");
         navi_sel_ctrl(fw->frame_nr);  /* alt does same as ctrl */
         navi_debug_print_sel_range();
      }
      else
      {
         if(gap_debug) printf("frame_widget_preview_events SELECTION (no modifier)\n");
         navi_sel_normal(fw->frame_nr);
         navi_debug_print_sel_range();
      }

      navi_ops_buttons_set_sensitive();
      navi_frames_timing_update();  /* this is also for refresh of the selection state */
      break;

    case GDK_EXPOSE:
      if(gap_debug) printf("frame_widget_preview_events GDK_EXPOSE frame_nr:%d widget:%ld  da_wgt:%ld\n"
                              , (int)fw->frame_nr
                              , (long)widget
                              , (long)fw->pv_ptr->da_widget
                              );

      /* eevent = (GdkEventExpose *) event; */

      if(widget == fw->pv_ptr->da_widget)
      {
        navi_preview_extents();
        navi_frame_widget_time_label_update(fw);
        frame_widget_preview_redraw (fw);
      }

      break;

    default:
      break;
  }

  return FALSE;
}       /* end  frame_widget_preview_events */


/* ---------------------------------
 * navi_dialog_poll
 * ---------------------------------
 */
static gint
navi_dialog_poll(GtkWidget *w, gpointer   data)
{
   gint32 frame_nr;
   gint32 update_flag;
   gint32 video_preview_size;

   if(gap_debug) printf("navi_dialog_poll  TIMER POLL\n");

   if(naviD)
   {
      if(suspend_gimage_notify == 0)
      {
         update_flag = NUPD_IMAGE_MENU;
         video_preview_size = navi_get_preview_size();
         if(naviD->preview_size != video_preview_size)
         {
            naviD->preview_size = video_preview_size;
            update_flag = NUPD_ALL;
         }

         /* check and enable/disable tooltips */
         navi_dialog_tooltips ();

         /* check request to present dialog window */
         p_check_dialog_to_front_request();
         
         /* check if active image is still valid and is a frame image */
         if(gap_image_is_alive(naviD->active_imageid))
         {
           frame_nr = gap_lib_get_frame_nr(naviD->active_imageid);
         }
         else
         {
           frame_nr = -1;
         }

         if(frame_nr < 0 )
         {
           if(gap_debug)
           {
             printf("TIMER POLL active_imageid:%d is now invalid\n"
                ,(int)naviD->active_imageid
                );
           }
           
            /* no valid frame number, maybe frame was closed
             */
            naviD->active_imageid = -1;
            update_flag = NUPD_ALL;
         }
         else
         {
           if(naviD->ainfo_ptr)
           {
              update_flag = NUPD_IMAGE_MENU | NUPD_FRAME_NR_CHANGED;
           }
           else
           {
              update_flag = NUPD_ALL;
           }
         }
         navi_dialog_update(update_flag);
      }

      /* restart timer */
      if(naviD->timer >= 0)
      {
         g_source_remove(naviD->timer);
      }
      naviD->timer = g_timeout_add(naviD->cycle_time,
                                    (GtkFunction)navi_dialog_poll, NULL);
   }
   return FALSE;
}  /* end navi_dialog_poll */


/* ---------------------------------
 * navi_dialog_update
 * ---------------------------------
 */
static void
navi_dialog_update(gint32 update_flag)
{
  gint32 l_first, l_last;
  /* gint   l_image_menu_was_changed; */

  /*  l_image_menu_was_changed = FALSE; */
  if(update_flag & NUPD_IMAGE_MENU)
  {
    /* l_image_menu_was_changed = */ navi_refresh_image_menu();
  }
  if(update_flag & NUPD_FRAME_NR_CHANGED)
  {
     l_first = -1;
     l_last = -1;

     if(naviD->ainfo_ptr)
     {
       l_first = naviD->ainfo_ptr->first_frame_nr;
       l_last =  naviD->ainfo_ptr->last_frame_nr;
     }
     navi_reload_ainfo(naviD->active_imageid);
     navi_preview_extents();

     /* we must force a rebuild of the dyn_table of frame_widgets
      * if any frames were deleteted or are created
      * (outside of the navigator)
      */
     if(naviD->ainfo_ptr)
     {
        if((l_first != naviD->ainfo_ptr->first_frame_nr)
        || (l_last  != naviD->ainfo_ptr->last_frame_nr))
        {
          update_flag |= NUPD_PREV_LIST;
          navi_drop_sel_range_list();
        }
     }
  }
  if(update_flag & NUPD_PREV_LIST)
  {
     frames_dialog_flush();
  }
  else
  {
     /* SIMPLY refresh all frame_widgets in the dn_table.
      * (could limit refresh to those frame_widgets
      * where the thumbnail file timestamp changed since last access
      * and always refresh the preview of the active image.)
      */
     navi_refresh_dyn_table (naviD->dyn_topframenr);
  }
}  /* end navi_dialog_update */


/* ------------------------
 * navi_preview_extents
 * ------------------------
 */
static void
navi_preview_extents (void)
{
  gint32 width, height;
  if (!naviD)
  {
    return;
  }
  
  if (!gap_image_is_alive(naviD->active_imageid))
  {
    return;
  }

  naviD->gimage_width = gimp_image_width(naviD->active_imageid);
  naviD->gimage_height = gimp_image_height(naviD->active_imageid);

  /*  Get the image width and height variables, based on the gimage  */
  if (naviD->gimage_width > naviD->gimage_height)
  {
    naviD->ratio = (double) naviD->preview_size / (double) naviD->gimage_width;
  }
  else
  {
    naviD->ratio = (double) naviD->preview_size / (double) naviD->gimage_height;
  }

  if (naviD->preview_size > 0)
  {
    width = (int) (naviD->ratio * naviD->gimage_width);
    height = (int) (naviD->ratio * naviD->gimage_height);
    if (width < 1) width = 1;
    if (height < 1) height = 1;
  }
  else
  {
      /* defaults for the case that gimprc preferences says: use no layer previews at all */
      width = 16;
      height = 10;
  }

  if((naviD->image_width != width)
  || (naviD->image_height != height))
  {
    gint ii;

    naviD->image_width = width;
    naviD->image_height = height;

    /* set new size for all (used) frame_wiget_tab element preview drawing_areas */
    for(ii=0; ii < naviD->fw_tab_used_count; ii++)
    {
      FrameWidget *fw;

      fw = &naviD->frame_widget_tab[ii];
      if(fw->pv_ptr)
      {


        if(gap_debug) printf("navi_preview_extents pv_w: %d pv_h:%d\n", (int)fw->pv_ptr->pv_width, (int)fw->pv_ptr->pv_height);


        gap_pview_set_size(fw->pv_ptr
                       , naviD->image_width
                       , naviD->image_height
                       , NAVI_CHECK_SIZE
                       );
      }
    }
  }

  naviD->item_height = 4 + naviD->image_height;

  if(gap_debug) printf("navi_preview_extents w: %d h:%d\n", (int)naviD->image_width, (int)naviD->image_height);
}  /* end navi_preview_extents */


/* ---------------------------------
 * navi_calc_frametiming
 * ---------------------------------
 */
static void
navi_calc_frametiming(gint32 frame_nr, char *buf, gint32 sizeof_buf)
{
  gint32 first;
  gdouble framerate;

  first = frame_nr;
  if(naviD->ainfo_ptr)
  {
    first = naviD->ainfo_ptr->first_frame_nr;
  }

  framerate = 24.0;
  if(naviD->vin_ptr)
  {
    framerate = naviD->vin_ptr->framerate;
  }

  gap_timeconv_framenr_to_timestr( (frame_nr - first)
                           , framerate
                           , buf
                           , sizeof_buf
                           );
}  /* end navi_calc_frametiming */


/* ---------------------------------
 * navi_frame_widget_time_label_update
 * ---------------------------------
 */
static void
navi_frame_widget_time_label_update(FrameWidget *fw)
{
  char frame_nr_to_time[20];
  GdkColor      *bg_color;
  GdkColor      *fg_color;

  bg_color = &naviD->shell->style->bg[GTK_STATE_NORMAL];
  fg_color = &naviD->shell->style->fg[GTK_STATE_NORMAL];
  if(fw->frame_nr >= 0)
  {
    navi_calc_frametiming(fw->frame_nr, frame_nr_to_time, sizeof(frame_nr_to_time));
    if(navi_findframe_in_sel_range(fw->frame_nr))
    {
       bg_color = &naviD->shell->style->bg[GTK_STATE_SELECTED];
       fg_color = &naviD->shell->style->fg[GTK_STATE_SELECTED];
    }
  }
  else
  {
    frame_nr_to_time[0] = ' ';
    frame_nr_to_time[1] = '\0';
  }

  if(gap_debug)
  {
    printf("navi_frame_widget_time_label_update: GTK_STYLE_SET_BACKGROUND bg_color: %ld\n"
       , (long)bg_color);
  }

  /* Note: Gtk does not know about selcted items, since selections are handled
   * external by gap_navigator_dialog code.
   * using SELECTED or NORMAL color from the shell window to paint this private selection
   * (must use always the NORMAL state here, because for Gtk the event_box is never selected)
   */
  gtk_widget_modify_bg(fw->event_box, GTK_STATE_NORMAL, bg_color);
  gtk_widget_modify_fg(fw->number_label, GTK_STATE_NORMAL, fg_color);
  gtk_widget_modify_fg(fw->time_label, GTK_STATE_NORMAL, fg_color);

  gtk_label_set_text (GTK_LABEL (fw->time_label), frame_nr_to_time);
}  /* end navi_frame_widget_time_label_update */


/*  ######### SelctionRange Procedures ########## */

/* -----------------------------------------
 * navi_debug_print_sel_range
 * -----------------------------------------
 */
static void
navi_debug_print_sel_range(void)
{
  SelectedRange *range_item;

  if(gap_debug)
  {
    printf("\n SEL_RANGE  ------------------------- START\n");
    range_item = naviD->sel_range_list;
    while(range_item)
    {
      printf(" SEL_RANGE  from: %06d  to: %06d\n"
             , (int)range_item->from
             , (int)range_item->to
             );
      range_item = range_item->next;
    }
    printf(" SEL_RANGE  ------------------------- END\n\n");
  }
}  /* end navi_debug_print_sel_range */


/* ---------------------------------
 * navi_drop_sel_range_list (2)
 * ---------------------------------
 */
static void
navi_drop_sel_range_list2(void)
{
  SelectedRange *range_list;
  SelectedRange *range_item;

  range_list = naviD->sel_range_list;
  while(range_list)
  {
    range_item = range_list;
    range_list = range_list->next;

    g_free(range_item);
  }
  naviD->sel_range_list = NULL;
  navi_ops_buttons_set_sensitive();
}  /* end navi_drop_sel_range_list2 */

static void
navi_drop_sel_range_list(void)
{
  navi_drop_sel_range_list2();
  naviD->prev_selected_framnr = -1;
}  /* end navi_drop_sel_range_list */


/* ---------------------------------
 * navi_drop_sel_range_elem
 * ---------------------------------
 */
static void
navi_drop_sel_range_elem(SelectedRange *range_item)
{
  SelectedRange *range_next;
  SelectedRange *range_prev;

  range_prev = range_item->prev;
  range_next = range_item->next;

  if(range_prev)
  {
    range_prev->next = range_next;
  }
  if(range_next)
  {
    range_next->prev = range_prev;
  }

  if(range_item == naviD->sel_range_list)
  {
    naviD->sel_range_list = range_next;
  }

  g_free(range_item);

}  /* end navi_drop_sel_range_elem */


/* -----------------------------------------
 * navi_findframe_in_sel_range
 * -----------------------------------------
 * return the SelectedRange that contains the framenr
 * return NULL if framenr is not found in the selected ranges
 */
static SelectedRange *
navi_findframe_in_sel_range(gint32 framenr)
{
  SelectedRange *range_item;

  range_item = naviD->sel_range_list;
  while(range_item)
  {
    if((framenr >= range_item->from) && (framenr <= range_item->to))
    {
      return (range_item);
    }

    range_item = range_item->next;
  }
  return (NULL);

}  /* end navi_findframe_in_sel_range */


/* -----------------------------------------
 * navi_add_sel_range_list
 * -----------------------------------------
 * add a new element to the naviD->sel_range_list
 * the element is added at its place following ascending sort order
 * by framenr_from.
 */
static void
navi_add_sel_range_list(gint32 framenr_from, gint32 framenr_to)
{
  SelectedRange *range_list;
  SelectedRange *range_item;
  SelectedRange *range_prev;
  SelectedRange *range_next;

  range_prev = NULL;
  range_next = naviD->sel_range_list;
  range_list = naviD->sel_range_list;
  while(range_list)
  {
    if(framenr_from < range_list->from)
    {
      break;
    }
    range_prev = range_list;
    range_list = range_list->next;
    range_next = range_list;
  }

  range_item = g_new(SelectedRange, 1);
  range_item->from = framenr_from;
  range_item->to = framenr_to;

  if(range_prev == NULL)
  {
    /* add new element as 1.st element in the list */
    range_item->next = naviD->sel_range_list;
    range_item->prev = NULL;
    naviD->sel_range_list = range_item;
  }
  else
  {
    range_prev->next = range_item;

    range_item->next = range_next;
    range_item->prev = range_prev;
  }

  if(range_next)
  {
    range_next->prev = range_item;
  }

}  /* end navi_add_sel_range_list */


/* -----------------------------------------
 * navi_sel_normal
 * -----------------------------------------
 */
static void
navi_sel_normal(gint32 framenr)
{
  navi_drop_sel_range_list();
  navi_add_sel_range_list(framenr, framenr);
  naviD->prev_selected_framnr = framenr;
}  /* end navi_sel_normal */


/* -----------------------------------------
 * navi_sel_ctrl
 * -----------------------------------------
 */
static void
navi_sel_ctrl(gint32 framenr)
{
  SelectedRange *range_item;

  range_item = navi_findframe_in_sel_range(framenr);
  if(range_item)
  {
     /* framenr was already selected, DESELECT now */
     if((range_item->from == framenr)
     && (range_item->to == framenr))
     {
       /* completely remove the range_item */
       navi_drop_sel_range_elem(range_item);
     }
     else if (range_item->from == framenr)
     {
       /* take out the first frame from the range */
       range_item->from++;
     }
     else if (range_item->to == framenr)
     {
       /* take out the last frame from the range */
       range_item->to--;
     }
     else
     {
       gint32 old_framenr_to;

       if(gap_debug) printf(" ** SPLIT from:%d to:%d framenr:%d\n", (int)range_item->from, (int)range_item->to, (int)framenr);

       /* have to split the range (take out framenr) */
       old_framenr_to = range_item->to;
       range_item->to = framenr - 1;
       navi_add_sel_range_list(framenr +1, old_framenr_to);
     }
  }
  else
  {
     /* framenr was not SELECTED, select now */
     navi_add_sel_range_list(framenr, framenr);
  }

  naviD->prev_selected_framnr = framenr;

}  /* end navi_sel_ctrl */


/* -----------------------------------------
 * navi_sel_shift
 * -----------------------------------------
 */
static void
navi_sel_shift(gint32 framenr)
{
  navi_drop_sel_range_list2();  /* use the variant without resetting naviD->prev_selected_framnr */

  /* if there was no previous selection
   * then we use the first_frame_nr as starting point
   */
  naviD->prev_selected_framnr = MAX(naviD->prev_selected_framnr
                                   ,naviD->ainfo_ptr->first_frame_nr);


  navi_add_sel_range_list(MIN(framenr, naviD->prev_selected_framnr)
                         ,MAX(framenr, naviD->prev_selected_framnr));
}  /* end navi_sel_shift */


/* ################################################# */


/* ---------------------------------
 * navi_dyn_frame_size_allocate
 * ---------------------------------
 */
static void
navi_dyn_frame_size_allocate  (GtkWidget       *widget,
                              GtkAllocation   *allocation,
                              gpointer         user_data)
{
  if(naviD == NULL) { return; }

  if(gap_debug) printf("\n#  on_vid_preview_size_allocate: START new: w:%d h:%d \n"
                           , (int)allocation->width
                           , (int)allocation->height
                           );
  if(!naviD->in_dyn_table_sizeinit)
  {
     gint resize_flag;

     resize_flag = navi_dyn_table_set_size((gint)allocation->height);

     if(resize_flag > 0)
     {
        /* need refresh if new ros were added */
        navi_refresh_dyn_table(naviD->dyn_topframenr);
     }
  }

}  /* end navi_dyn_frame_size_allocate */


/* ---------------------------------
 * navi_get_last_range_list
 * ---------------------------------
 *
 */
static SelectedRange *
navi_get_last_range_list(SelectedRange *sel_range_list)
{
  SelectedRange *range_list;
  SelectedRange *range_item;

  /* generate prev linkpointers in the range list */
  range_item = NULL;
  range_list = sel_range_list;
  while(range_list)
  {
    range_list->prev = range_item;
    range_item = range_list;
    if(range_list->next == NULL)
    {
      break;  /* now range_list points to the last element in the list */
    }
    range_list = range_list->next;
  }
  return (range_list);

}  /* end navi_get_last_range_list */


/* --------------------------
 * navi_frame_widget_replace2
 * --------------------------
 * in frame_nr (create empty dummy widgets for illegal frame_nr -1)
 */
static void
navi_frame_widget_replace2(FrameWidget *fw)
{
  if(gap_debug) printf("navi_frame_widget_replace2 START\n");

  if(naviD == NULL)  { return; }

  if(fw->pv_ptr == NULL)
  {
    fw->pv_ptr = gap_pview_new( naviD->image_width    /* width */
                            , naviD->image_height   /* height */
                            , NAVI_CHECK_SIZE
                            , NULL   /* preview without aspect_frame */
                            );
  }
  else
  {
    if((naviD->image_width != fw->pv_ptr->pv_width)
    || (naviD->image_height != fw->pv_ptr->pv_height))
    {
       gap_pview_set_size(fw->pv_ptr
                       , naviD->image_width
                       , naviD->image_height
                       , NAVI_CHECK_SIZE
                       );

    }
  }

  fw->width  = naviD->image_width;
  fw->height = naviD->image_height;

  if(fw->frame_nr < 0)
  {
    gtk_label_set_text (GTK_LABEL (fw->number_label), " ");
    gtk_label_set_text (GTK_LABEL (fw->time_label), " ");

    gtk_widget_hide(fw->pv_ptr->da_widget);
    return;

    gap_pview_render_from_buf (fw->pv_ptr
                 , NULL   /* render black frame */
                 , naviD->image_width
                 , naviD->image_height
                 , 3             /* bpp */
                 , FALSE         /* DONT allow_grab_src_data */
                 );
  }
  else
  {
    char frame_nr_to_char[20];

    gtk_widget_show(fw->pv_ptr->da_widget);
    if(naviD->preview_size <= 0)
    {
      gap_pview_render_default_icon(fw->pv_ptr);
    }
    else
    {
      navi_render_preview(fw);
    }

    /* update the labeltexts */
    navi_frame_widget_time_label_update(fw);
    g_snprintf(frame_nr_to_char, sizeof(frame_nr_to_char), "%06d", (int)fw->frame_nr);
    gtk_label_set_text (GTK_LABEL (fw->number_label), frame_nr_to_char);
  }

}  /* end navi_frame_widget_replace2 */


/* ---------------------------------
 * navi_frame_widget_replace
 * ---------------------------------
 */
static void
navi_frame_widget_replace(gint32 image_id, gint32 frame_nr, gint32 dyn_rowindex)
{
  FrameWidget *fw;
  time_t new_timestamp;

  if((dyn_rowindex < 0)
  || (dyn_rowindex >= MAX_DYN_ROWS)
  || (naviD == NULL)
  )
  {
    return;
  }

  fw = &naviD->frame_widget_tab[dyn_rowindex];
  new_timestamp = 0;


  if(fw->frame_filename)
  {
    gchar       *l_frame_filename;

    l_frame_filename = NULL;
    if(frame_nr >= 0)
    {
      l_frame_filename = gap_lib_alloc_fname(naviD->ainfo_ptr->basename, frame_nr, naviD->ainfo_ptr->extension);
      if(l_frame_filename)
      {
        if(strcmp(l_frame_filename, fw->frame_filename) == 0)
        {
          /* filename has not changed, keep the current timestamp in that case */
          new_timestamp = fw->frame_timestamp;
        }
      }
    }
    g_free(fw->frame_filename);
    fw->frame_filename = l_frame_filename;
  }

  fw->frame_timestamp = new_timestamp;

  fw->image_id      = image_id;
  fw->frame_nr      = frame_nr;

  navi_frame_widget_replace2(fw);
} /* end navi_frame_widget_replace */



/* ---------------------------------
 * navi_refresh_dyn_table
 * ---------------------------------
 * the refresh does read pixmap data for all items in the dyn_table
 * (from thumbnail_file or gimp_image_thumbnail)
 * the labels are refreshed too.
 */
static void
navi_refresh_dyn_table(gint32 frame_nr)
{
  gint32       l_frame_nr;
  gint32       l_frame_nr_prev;
  gint32       l_count;


  if(gap_debug) printf("navi_refresh_dyn_table: START frame_nr:%d\n", (int)frame_nr);

  naviD->dyn_topframenr = navi_constrain_dyn_topframenr(frame_nr);
  l_frame_nr = naviD->dyn_topframenr;

  l_frame_nr_prev = -1;
  for(l_count = 0; l_count < naviD->dyn_rows; l_count++)
  {
      l_frame_nr = CLAMP(l_frame_nr
                    , naviD->ainfo_ptr->first_frame_nr
                    , naviD->ainfo_ptr->last_frame_nr
                    );

      if(gap_debug) printf("navi_refresh_dyn_table: l_frame_nr:%d\n", (int)l_frame_nr);

      if(l_frame_nr == l_frame_nr_prev)
      {
        /* dont repeat display of the last frame again (use -1 instead) */
        navi_frame_widget_replace (-1, -1, l_count);
      }
      else
      {
        navi_frame_widget_replace (naviD->active_imageid, l_frame_nr, l_count);
      }

      l_frame_nr_prev = l_frame_nr;
      l_frame_nr += naviD->vin_ptr->timezoom;
  }
}  /* end navi_refresh_dyn_table */

/* ---------------------------------
 * navi_dyn_adj_set_pos
 * ---------------------------------
 */
static void
navi_dyn_adj_set_pos(void)
{
  gint32  adj_intval;
  gint32  topval;

  if(naviD == NULL) { return; }


  adj_intval = GTK_ADJUSTMENT(naviD->dyn_adj)->value;

  topval = (naviD->dyn_topframenr - naviD->ainfo_ptr->first_frame_nr)
               / naviD->vin_ptr->timezoom;
  topval++;

  if(gap_debug)
  {
     printf("navi_dyn_adj_set_pos: adj_intval:%d topval:%d\n"
           , (int)adj_intval
           , (int)topval
           );
  }


  if(adj_intval != topval)
  {
    gtk_adjustment_set_value(GTK_ADJUSTMENT(naviD->dyn_adj), (gdouble)topval);
  }
}  /* end navi_dyn_adj_set_pos */


/* ---------------------------------
 * navi_dyn_adj_changed_callback
 * ---------------------------------
 */
static void
navi_dyn_adj_changed_callback(GtkWidget *wgt, gpointer data)
{
  gint32  adj_intval;
  gint32  dyn_topframenr;
  if(naviD == NULL) { return; }

  adj_intval = (gint32)(GTK_ADJUSTMENT(naviD->dyn_adj)->value + 0.5);
  if(gap_debug) printf("navi_dyn_adj_changed_callback: adj_intval:%d dyn_topframenr:%d\n", (int)adj_intval, (int)naviD->dyn_topframenr);

  GTK_ADJUSTMENT(naviD->dyn_adj)->value = adj_intval;

  dyn_topframenr = ((adj_intval -1) * naviD->vin_ptr->timezoom) + naviD->ainfo_ptr->first_frame_nr;
  dyn_topframenr = navi_constrain_dyn_topframenr(dyn_topframenr);

  if(naviD->dyn_topframenr != dyn_topframenr)
  {
    naviD->dyn_topframenr = dyn_topframenr;
    navi_refresh_dyn_table(dyn_topframenr);
  }

}  /* end navi_dyn_adj_changed_callback */


/* ---------------------------------
 * navi_dyn_adj_set_limits
 * ---------------------------------
 */
static void
navi_dyn_adj_set_limits(void)
{
  gdouble upper_limit;
  gdouble lower_limit;
  gdouble page_increment;
  gdouble page_size;
  gdouble value;
  gint32 frame_cnt_zoomed;

  if(naviD == NULL) { return; }
  if(naviD->dyn_adj == NULL) { return; }

  frame_cnt_zoomed = p_get_number_of_frames(naviD->ainfo_ptr) / naviD->vin_ptr->timezoom;
  if(naviD->vin_ptr->timezoom > 1)
  {
    /* if there is a rest add 1 to make the last frame,
     * (that should never be filtered out by timezoom) reachable
     */
    frame_cnt_zoomed++;
  }
  lower_limit = 1.0;
  upper_limit = lower_limit + frame_cnt_zoomed;
  page_size = (gdouble)naviD->dyn_rows;
  page_increment = (gdouble)((gint32)page_size);

  value = GTK_ADJUSTMENT(naviD->dyn_adj)->value;

  if(gap_debug)
  {
    printf("\n cnt_zoomed : %d  dyn_rows:%d\n", (int)frame_cnt_zoomed ,(int)naviD->dyn_rows);
    printf("lower_limit %f\n", (float)lower_limit );
    printf("upper_limit %f\n", (float)upper_limit );
    printf("page_size %f\n", (float) page_size);
    printf("page_increment %f\n", (float)page_increment );
    printf("value              %f\n", (float)value );
  }

  GTK_ADJUSTMENT(naviD->dyn_adj)->lower = lower_limit;
  GTK_ADJUSTMENT(naviD->dyn_adj)->upper = upper_limit;
  GTK_ADJUSTMENT(naviD->dyn_adj)->page_increment = page_increment;
  GTK_ADJUSTMENT(naviD->dyn_adj)->value = MIN(value, upper_limit);
  GTK_ADJUSTMENT(naviD->dyn_adj)->page_size = page_size;

}  /* end navi_dyn_adj_set_limits */


/* ---------------------------------
 * navi_frame_widget_init_empty
 * ---------------------------------
 * this procedure does initialize
 * the passed frame_widget, by creating all
 * the gtk_widget stuff to build up the frame_widget
 */
static void
navi_frame_widget_init_empty (FrameWidget *fw)
{
  GtkWidget *alignment;

  if(gap_debug) printf("navi_frame_widget_init_empty START\n");

  /* create all sub-widgets for the given frame widget */
  fw->image_id      = -1;
  fw->frame_nr      = -1;
  fw->pv_ptr        = NULL;
  fw->width         = -1;
  fw->height        = -1;
  fw->frame_timestamp = 0;
  fw->frame_filename = NULL;

  fw->vbox = gtk_vbox_new (FALSE, 1);

  /* the hbox  */
  fw->hbox = gtk_hbox_new (FALSE, 1);

  /* Create an EventBox (container for labels and the preview drawing_area)
   * used to handle selection events
   */
  fw->event_box = gtk_event_box_new ();
  gtk_container_add (GTK_CONTAINER (fw->event_box), fw->hbox);
  gtk_widget_set_events (fw->event_box, GDK_BUTTON_PRESS_MASK);
  g_signal_connect (G_OBJECT (fw->event_box), "button_press_event",
                    G_CALLBACK (frame_widget_preview_events),
                    fw);

  gtk_box_pack_start (GTK_BOX (fw->vbox), fw->event_box, FALSE, FALSE, 1);

  /*  the frame number label */
  fw->number_label = gtk_label_new ("######");
  gtk_box_pack_start (GTK_BOX (fw->hbox), fw->number_label, FALSE, FALSE, 2);
  gtk_widget_show (fw->number_label);

  /*  The frame preview  */
  alignment = gtk_alignment_new (0.5, 0.5, 0.0, 0.0);
  gtk_box_pack_start (GTK_BOX (fw->hbox), alignment, FALSE, FALSE, 2);
  gtk_widget_show (alignment);


  fw->pv_ptr = gap_pview_new( naviD->image_width + 4
                                    , naviD->image_height + 4
                                    , NAVI_CHECK_SIZE
                                    , NULL   /* no aspect_frame is used */
                                    );

  gtk_widget_set_events (fw->pv_ptr->da_widget, GDK_EXPOSURE_MASK);
  gtk_container_add (GTK_CONTAINER (alignment), fw->pv_ptr->da_widget);
  gtk_widget_show (fw->pv_ptr->da_widget);
  g_signal_connect (G_OBJECT (fw->pv_ptr->da_widget), "event",
                    G_CALLBACK (frame_widget_preview_events),
                    fw);

  /*  the frame timing label */
  fw->time_label = gtk_label_new ("##:##:###");
  gtk_box_pack_start (GTK_BOX (fw->hbox), fw->time_label, FALSE, FALSE, 2);
  gtk_widget_show (fw->time_label);


  gtk_widget_show (fw->hbox);
  gtk_widget_show (fw->event_box);
  gtk_widget_show (fw->vbox);

  if(gap_debug) printf("navi_frame_widget_init_empty END\n");

}  /* end navi_frame_widget_init_empty */



/* ---------------------------------
 * navi_dyn_table_set_size
 * ---------------------------------
 * init and attach frame_widgets from naviD->frame_widget_tab
 * to dyn_tab
 * return 0 if number of rows was not changed,
 *       -n if shrinked by n rows
 *       +n if expanded by n rows
 */
static gint
navi_dyn_table_set_size(gint win_height)
{
  gint l_row;
  gint l_new_rows;
  gint l_resize_flag;

  l_new_rows = MIN((win_height / MAX(naviD->item_height,10)), MAX_DYN_ROWS);

  if(gap_debug) printf("navi_dyn_table_set_size: START (old)fw_tab_used_count:%d (old)dyn_rows:%d (new)l_new_rows:%d\n"
                      , (int)naviD->fw_tab_used_count
                      , (int)naviD->dyn_rows
                      , (int)l_new_rows
                      );

  l_resize_flag = l_new_rows - naviD->dyn_rows;
  if(l_resize_flag < 0)
  {
      /* gtk_table_resize does not work if we want less rows
       * and the rows already have widgets attached.
       * we first have to destroy the attached widgets.
       */
     for(l_row = l_new_rows; l_row < naviD->dyn_rows; l_row++)
     {
       FrameWidget *fw;

       if(gap_debug) printf("navi_dyn_table_set_size: destroy row:%d\n", (int)l_row);

       fw = &naviD->frame_widget_tab[l_row];

       gap_pview_reset(fw->pv_ptr);
       if(fw->frame_filename) g_free(fw->frame_filename);

       gtk_widget_destroy(fw->vbox);
       navi_frame_widget_init_empty(fw);
     }
     naviD->fw_tab_used_count = MIN(l_new_rows, naviD->fw_tab_used_count);
  }

  if(l_new_rows != naviD->dyn_rows)
  {
    gtk_table_resize(GTK_TABLE(naviD->dyn_table),  l_new_rows, 1);
  }


  /* initialize as much frame_widgets as needed
   * NOTE: frame_widgets in the frame_widget_tab are initialzed
   *       only once, and are kept even if dyn_table shrinks down
   *       to less rows.
   *       (they can be reused later if dyn_table expands again)
   */
  for(l_row = naviD->fw_tab_used_count; l_row < l_new_rows; l_row++)
  {
     if(gap_debug) printf("navi_dyn_table_set_size: init row:%d\n", (int)l_row);

     navi_frame_widget_init_empty(&naviD->frame_widget_tab[l_row]);
  }
  naviD->fw_tab_used_count = l_row;

  /* attach frame_widgets as needed */
  for(l_row = naviD->dyn_rows; l_row < l_new_rows; l_row++)
  {
     if(gap_debug) printf("navi_dyn_table_set_size: attach row:%d\n", (int)l_row);

     gtk_table_attach (GTK_TABLE (naviD->dyn_table)
                      , naviD->frame_widget_tab[l_row].vbox
                      , 0, 1
                      , l_row, l_row+1
                      , (GtkAttachOptions) (GTK_FILL | GTK_EXPAND)
                      , (GtkAttachOptions) (GTK_FILL | GTK_EXPAND)
                      , 0, 0);
  }
  naviD->dyn_rows = l_new_rows;
  navi_dyn_adj_set_limits();

  if(gap_debug) printf("navi_dyn_table_set_size: END (result)fw_tab_used_count:%d (res)dyn_rows:%d flag:%d\n"
                      , (int)naviD->fw_tab_used_count
                      , (int)naviD->dyn_rows
                      , (int)l_resize_flag
                      );

  return (l_resize_flag);

} /* end navi_dyn_table_set_size */



/* ---------------------------------
 * p_set_acl_tracking_radio_buttons
 * ---------------------------------
 */
static void
p_set_acl_tracking_radio_buttons(NaviDialog *naviD)
{
  GtkWidget *radio_button;

  radio_button = naviD->acl_trace_off_toggle;
  if(naviD->vin_ptr)
  {
    switch(naviD->vin_ptr->active_layer_tracking)
    {
      case GAP_ACTIVE_LAYER_TRACKING_BY_NAME:
        radio_button = naviD->acl_trace_by_name_toggle;
        break;
      case GAP_ACTIVE_LAYER_TRACKING_BY_STACKPOS:
        radio_button = naviD->acl_trace_by_pos_toggle;
        break;
      default:
        radio_button = naviD->acl_trace_off_toggle;
        break;
    }
  }
  if(radio_button)
  {
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (radio_button), TRUE);
  }
}  /* end p_set_acl_tracking_radio_buttons */

/* ---------------------------------
 * p_set_acl_tracking_mode
 * ---------------------------------
 */
static void
p_set_acl_tracking_mode(NaviDialog *naviD, gint32 acl_tracking_mode)
{
  if(acl_tracking_mode != naviD->vin_ptr->active_layer_tracking)
  {
     naviD->vin_ptr->active_layer_tracking = acl_tracking_mode;
     if(naviD->ainfo_ptr)
     {
       /* write new framerate to video info file */
       gap_vin_set_common(naviD->vin_ptr, naviD->ainfo_ptr->basename);
     }
  }
}  /* end p_set_acl_tracking_mode */


/* ---------------------------------
 * p_acl_tracking_off_callback
 * ---------------------------------
 */
static void
p_acl_tracking_off_callback(GtkWidget *widget, NaviDialog *naviD)
{
  if((naviD) && (GTK_TOGGLE_BUTTON (widget)->active))
  {
    p_set_acl_tracking_mode(naviD, GAP_ACTIVE_LAYER_TRACKING_OFF);
  }
}  /* end p_acl_tracking_off_callback */


/* ---------------------------------
 * p_acl_tracking_by_name_callback
 * ---------------------------------
 */
static void
p_acl_tracking_by_name_callback(GtkWidget *widget, NaviDialog *naviD)
{
  if((naviD) && (GTK_TOGGLE_BUTTON (widget)->active))
  {
    p_set_acl_tracking_mode(naviD, GAP_ACTIVE_LAYER_TRACKING_BY_NAME);
  }
}  /* end p_acl_tracking_by_name_callback */


/* ---------------------------------
 * p_acl_tracking_by_pos_callback
 * ---------------------------------
 */
static void
p_acl_tracking_by_pos_callback(GtkWidget *widget, NaviDialog *naviD)
{
  if((naviD) && (GTK_TOGGLE_BUTTON (widget)->active))
  {
    p_set_acl_tracking_mode(naviD, GAP_ACTIVE_LAYER_TRACKING_BY_STACKPOS);
  }
}  /* end p_acl_tracking_by_pos_callback */


/* ---------------------------------
 * p_create_acl_tracking_widgets
 * ---------------------------------
 */
GtkWidget *
p_create_acl_tracking_widgets(NaviDialog *naviD)
{
  GtkWidget *label;
  GtkWidget *util_box;
  GtkWidget *radio_table;
  GtkWidget *radio_button;
  GSList    *radio_group = NULL;
  gint      l_idx;
  gboolean  l_radio_pressed;


  util_box = gtk_hbox_new (FALSE, 1);

  /* the active layer tracking label */
  label = gtk_label_new (_("AL-Tracking:"));
  gtk_box_pack_start (GTK_BOX (util_box), label, FALSE, FALSE, 2);
  gtk_widget_show (label);


  /* radio_table */
  radio_table = gtk_table_new (1, 3, FALSE);

  l_idx = 0;
  /* radio button active layer tracking OFF */
  radio_button = gtk_radio_button_new_with_label ( radio_group, _("OFF") );
  radio_group = gtk_radio_button_get_group ( GTK_RADIO_BUTTON (radio_button) );
  gtk_table_attach ( GTK_TABLE (radio_table), radio_button, l_idx, l_idx+1
                   , 0, 1, GTK_FILL | GTK_EXPAND, 0, 0, 0);
  naviD->acl_trace_off_toggle = radio_button;

  l_radio_pressed = TRUE;
  if(naviD->vin_ptr)
  {
    l_radio_pressed = (naviD->vin_ptr->active_layer_tracking == GAP_ACTIVE_LAYER_TRACKING_OFF);
  }
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (radio_button),
                                   l_radio_pressed);
  gimp_help_set_help_data(radio_button, _("Disable active layer tracking"), NULL);

  gtk_widget_show (radio_button);
  g_signal_connect ( G_OBJECT (radio_button), "toggled",
                     G_CALLBACK (p_acl_tracking_off_callback),
                     naviD);

  l_idx = 1;

  /* radio button thres_mode HSV */
  radio_button = gtk_radio_button_new_with_label ( radio_group, _("Name") );
  radio_group = gtk_radio_button_get_group ( GTK_RADIO_BUTTON (radio_button) );
  gtk_table_attach ( GTK_TABLE (radio_table), radio_button, l_idx, l_idx+1, 0, 1
                   , GTK_FILL | GTK_EXPAND, 0, 0, 0);
  naviD->acl_trace_by_name_toggle = radio_button;

  l_radio_pressed = FALSE;
  if(naviD->vin_ptr)
  {
    l_radio_pressed = (naviD->vin_ptr->active_layer_tracking == GAP_ACTIVE_LAYER_TRACKING_BY_NAME);
  }
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (radio_button),
                                   l_radio_pressed);
  gimp_help_set_help_data(radio_button, _("Enable tracking of the active layer by name at framechanges"), NULL);

  gtk_widget_show (radio_button);
  g_signal_connect ( G_OBJECT (radio_button), "toggled",
                     G_CALLBACK (p_acl_tracking_by_name_callback),
                     naviD);


  l_idx = 2;

  /* radio button thres_mode VAL */
  radio_button = gtk_radio_button_new_with_label ( radio_group, _("Pos") );
  radio_group = gtk_radio_button_get_group ( GTK_RADIO_BUTTON (radio_button) );
  gtk_table_attach ( GTK_TABLE (radio_table), radio_button, l_idx, l_idx+1, 0, 1
                   , GTK_FILL | GTK_EXPAND, 0, 0, 0);
  naviD->acl_trace_by_pos_toggle = radio_button;

  l_radio_pressed = FALSE;
  if(naviD->vin_ptr)
  {
    l_radio_pressed = (naviD->vin_ptr->active_layer_tracking == GAP_ACTIVE_LAYER_TRACKING_BY_STACKPOS);
  }
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (radio_button),
                                   l_radio_pressed);
  gimp_help_set_help_data(radio_button, _("Enable tracking of the active layer by stack position at framechanges"), NULL);

  gtk_widget_show (radio_button);
  g_signal_connect ( G_OBJECT (radio_button), "toggled",
                     G_CALLBACK (p_acl_tracking_by_pos_callback),
                     naviD);

  gtk_box_pack_start (GTK_BOX (util_box), radio_table, FALSE, FALSE, 2);
  gtk_widget_show (radio_table);

  return(util_box);
}  /* end p_create_acl_tracking_widgets */


/* ------------------------
 * navi_dialog_create
 * ------------------------
 */
GtkWidget *
navi_dialog_create (GtkWidget* shell, gint32 image_id)
{
  GtkWidget *vbox;
  GtkWidget *vbox2;
  GtkWidget *vscale;
  GtkWidget *util_box;
  GtkWidget *button_box;
  GtkWidget *label;
  GtkWidget *spinbutton;
  GtkWidget *menu_item;
  GtkObject *adj;
  char  *l_basename;
  char frame_nr_to_char[20];

  if(gap_debug) printf("navi_dialog_create\n");

  if (naviD)
  {
    return naviD->vbox;
  }
  l_basename = NULL;
  g_snprintf(frame_nr_to_char, sizeof(frame_nr_to_char), "0000 - 0000");
  naviD = g_new (NaviDialog, 1);
  /* init the global naviD structure */
  naviD->del_button = NULL;
  naviD->dup_button = NULL;
  naviD->framerate_adj = NULL;
  naviD->timezoom_adj = NULL;
  naviD->acl_trace_off_toggle = NULL;
  naviD->acl_trace_by_name_toggle = NULL;
  naviD->acl_trace_by_pos_toggle = NULL;
  naviD->dyn_adj = NULL;         /* disable procedure navi_dyn_adj_set_limits before this widget is created  */
  naviD->sel_range_list = NULL;  /* startup without selection */
  naviD->prev_selected_framnr = -1;
  naviD->fw_tab_used_count = 0;  /* nothing used/initialized in the frame_widget table */
  naviD->dyn_rows = 0;           /* no rows in the dyn table (are added later at 1st resize while creation) */
  naviD->waiting_cursor = FALSE;
  naviD->cursor_wait = gdk_cursor_new (GDK_WATCH);
  naviD->cursor_acitve = NULL;   /* NULL: use the default cursor */
  naviD->shell = shell;
  naviD->OpenFrameImagesList  = NULL;
  naviD->OpenFrameImagesCount  = 0;
  naviD->any_imageid = -1;
  naviD->cycle_time   = 1000;  /* polling cylcle of 1 sec */
  naviD->timer        = -1;
  naviD->active_imageid = image_id;
/*  naviD->ainfo_ptr  = navi_get_ainfo(naviD->active_imageid, NULL); */
  naviD->ainfo_ptr  = NULL;
  naviD->framerange_number_label = NULL;
  navi_reload_ainfo_force(image_id);
  if(naviD->ainfo_ptr != NULL)
  {
    g_snprintf(frame_nr_to_char, sizeof(frame_nr_to_char), "%06d - %06d"
            , (int)naviD->ainfo_ptr->first_frame_nr
            , (int)naviD->ainfo_ptr->last_frame_nr);
    l_basename = naviD->ainfo_ptr->basename;
  }
  naviD->vin_ptr  = gap_vin_get_all(l_basename);
  naviD->image_width = 0;
  naviD->image_height = 0;
  naviD->preview_size = navi_get_preview_size();

  if (naviD->preview_size > 0)
  {
      navi_preview_extents ();
  }

  /*  The main vbox  */
  naviD->vbox = gtk_event_box_new ();

  vbox = gtk_vbox_new (FALSE, 1);
  gtk_container_set_border_width (GTK_CONTAINER (vbox), 2);
  gtk_container_add (GTK_CONTAINER (naviD->vbox), vbox);

  /*  The image menu  */
  util_box = gtk_hbox_new (FALSE, 1);
  gtk_box_pack_start (GTK_BOX (vbox), util_box, FALSE, FALSE, 0);

  /*  The image combobox (is replaced at navi_refresh_image_menu) */
  naviD->image_combo = NULL;
  naviD->image_combo_hbox = util_box;
  navi_refresh_image_menu();
  gtk_widget_show (util_box);

  /*  The popup menu (copy/cut/paste) */
  naviD->ops_menu = gtk_menu_new ();

      /* menu_item copy */
      menu_item = gtk_menu_item_new_with_label (_("Copy"));
      gtk_container_add (GTK_CONTAINER (naviD->ops_menu), menu_item);

      gtk_widget_show (menu_item);
      g_signal_connect (G_OBJECT (menu_item), "activate",
                        G_CALLBACK (edit_copy_callback),
                        naviD);
      naviD->copy_menu_item = menu_item;

      /* menu_item cut */
      menu_item = gtk_menu_item_new_with_label (_("Cut"));
      gtk_container_add (GTK_CONTAINER (naviD->ops_menu), menu_item);
      gtk_widget_show (menu_item);
      g_signal_connect (G_OBJECT (menu_item), "activate",
                        G_CALLBACK (edit_cut_callback),
                        naviD);
      naviD->cut_menu_item = menu_item;

      /* menu_item paste before */
      menu_item = gtk_menu_item_new_with_label (_("Paste Before"));
      gtk_container_add (GTK_CONTAINER (naviD->ops_menu), menu_item);
      gtk_widget_show (menu_item);
      g_signal_connect (G_OBJECT (menu_item), "activate",
                        G_CALLBACK (edit_pasteb_callback),
                        naviD);
      naviD->pasteb_menu_item = menu_item;

      /* menu_item copy */
      menu_item = gtk_menu_item_new_with_label (_("Paste After"));
      gtk_container_add (GTK_CONTAINER (naviD->ops_menu), menu_item);
      gtk_widget_show (menu_item);
      g_signal_connect (G_OBJECT (menu_item), "activate",
                        G_CALLBACK (edit_pastea_callback),
                        naviD);
      naviD->pastea_menu_item = menu_item;

      /* menu_item copy */
      menu_item = gtk_menu_item_new_with_label (_("Paste Replace"));
      gtk_container_add (GTK_CONTAINER (naviD->ops_menu), menu_item);
      gtk_widget_show (menu_item);
      g_signal_connect (G_OBJECT (menu_item), "activate",
                        G_CALLBACK (edit_paster_callback),
                        naviD);
      naviD->paster_menu_item = menu_item;

      /* menu_item copy */
      menu_item = gtk_menu_item_new_with_label (_("Clear Video Buffer"));
      gtk_container_add (GTK_CONTAINER (naviD->ops_menu), menu_item);
      gtk_widget_show (menu_item);
      g_signal_connect (G_OBJECT (menu_item), "activate",
                        G_CALLBACK (edit_clrpaste_callback),
                        naviD);
      naviD->clrpaste_menu_item = menu_item;


      /* menu_item Select All */
      menu_item = gtk_menu_item_new_with_label (_("Select All"));
      gtk_container_add (GTK_CONTAINER (naviD->ops_menu), menu_item);
      gtk_widget_show (menu_item);
      g_signal_connect (G_OBJECT (menu_item), "activate",
                        G_CALLBACK (navi_sel_all_callback),
                        naviD);
      naviD->sel_all_menu_item = menu_item;

      /* menu_item Select None */
      menu_item = gtk_menu_item_new_with_label (_("Select None"));
      gtk_container_add (GTK_CONTAINER (naviD->ops_menu), menu_item);
      gtk_widget_show (menu_item);
      g_signal_connect (G_OBJECT (menu_item), "activate",
                        G_CALLBACK (navi_sel_none_callback),
                        naviD);
      naviD->sel_none_menu_item = menu_item;
  gtk_widget_show (naviD->ops_menu);


  /* radio button box for active layer tracking */
  util_box = p_create_acl_tracking_widgets(naviD);
  gtk_box_pack_start (GTK_BOX (vbox), util_box, FALSE, FALSE, 0);
  gtk_widget_show (util_box);


  /*  the Framerange label */
  util_box = gtk_hbox_new (FALSE, 1);
  gtk_box_pack_start (GTK_BOX (vbox), util_box, FALSE, FALSE, 0);

  label = gtk_label_new (_("Videoframes:"));
  gtk_box_pack_start (GTK_BOX (util_box), label, FALSE, FALSE, 2);
  gtk_widget_show (label);

  /*  the max frame number label */
  naviD->framerange_number_label = gtk_label_new (frame_nr_to_char);
  gtk_box_pack_start (GTK_BOX (util_box), naviD->framerange_number_label, FALSE, FALSE, 2);
  gtk_widget_show (naviD->framerange_number_label);
  gtk_widget_show (util_box);


  /*  framerate spinbutton  */
  naviD->framerate_box = util_box = gtk_hbox_new (FALSE, 1);
  gtk_box_pack_start (GTK_BOX (vbox), util_box, FALSE, FALSE, 0);

  label = gtk_label_new (_("Framerate:"));
  gtk_box_pack_start (GTK_BOX (util_box), label, FALSE, FALSE, 2);
  gtk_widget_show (label);

  spinbutton = gimp_spin_button_new (&adj,  /* return value (the adjustment) */
                      naviD->vin_ptr->framerate,     /* initial_val */
                      1.0,                  /* umin */
                      100.0,                /* umax */
                      1.0,                  /* sstep */
                      10.0,                /* pagestep */
                      0.0,                 /* page_size */
                      1.0,                 /* climb_rate */
                      4                    /* digits */
                      );
  naviD->framerate_adj = adj;

  gtk_box_pack_start (GTK_BOX (util_box), spinbutton, TRUE, TRUE, 0);
  g_signal_connect (G_OBJECT (naviD->framerate_adj), "value_changed",
                    G_CALLBACK (navi_framerate_spinbutton_update),
                    naviD);
  gtk_widget_show (spinbutton);

  gimp_help_set_help_data (spinbutton, _("Set framerate in frames/sec"), NULL);

  gtk_widget_show (util_box);

  /*  timezoom spinbutton  */
  naviD->timezoom_box = util_box = gtk_hbox_new (FALSE, 1);
  gtk_box_pack_start (GTK_BOX (vbox), util_box, FALSE, FALSE, 0);

  label = gtk_label_new (_("Timezoom:"));
  gtk_box_pack_start (GTK_BOX (util_box), label, FALSE, FALSE, 2);
  gtk_widget_show (label);

  spinbutton = gimp_spin_button_new (&adj,  /* return value (the adjustment) */
                      naviD->vin_ptr->framerate,     /* initial_val */
                      1.0,                  /* umin */
                      500.0,                /* umax */
                      1.0,                  /* sstep */
                      10.0,                /* pagestep */
                      0.0,                 /* page_size */
                      1.0,                 /* climb_rate */
                      0                    /* digits */
                      );

  naviD->timezoom_adj = adj;
  gtk_box_pack_start (GTK_BOX (util_box), spinbutton, TRUE, TRUE, 0);
  g_signal_connect (G_OBJECT (naviD->timezoom_adj), "value_changed",
                    G_CALLBACK (navi_timezoom_spinbutton_update),
                    naviD);
  gtk_widget_show (spinbutton);

  gimp_help_set_help_data (spinbutton, _("Show only every Nth frame"), NULL);

  gtk_widget_show (util_box);

  /*  The frame (that will contain the dyn_table)  */
  naviD->dyn_frame = gimp_frame_new (NULL);
  naviD->in_dyn_table_sizeinit = TRUE;
  g_signal_connect (G_OBJECT (naviD->dyn_frame), "size_allocate",
                      G_CALLBACK (navi_dyn_frame_size_allocate),
                      NULL);

  gtk_widget_set_size_request (naviD->dyn_frame
                              , -1                 /* LIST_WIDTH */
                              , LIST_HEIGHT        /* LIST_HEIGHT */
                              );
  gtk_box_pack_start (GTK_BOX (vbox), naviD->dyn_frame, TRUE, TRUE, 2);
  gtk_widget_show (naviD->dyn_frame);

  /* hbox for dyn_table and vscale */
  naviD->hbox = gtk_hbox_new (FALSE, 0);
  gtk_container_add (GTK_CONTAINER (naviD->dyn_frame), naviD->hbox);
  gtk_widget_show (naviD->hbox);

  /* dyn_table has 1 upto MAX_DYN_ROWS and always just 1 column */
  naviD->dyn_table = gtk_table_new (1, 1, TRUE);
  navi_dyn_table_set_size(LIST_HEIGHT);
  gtk_widget_show (naviD->dyn_table);
  gtk_box_pack_start (GTK_BOX (naviD->hbox), naviD->dyn_table, TRUE, TRUE, 2);

  /*  The vbox for vscale with arrow buttons */
  vbox2 = gtk_vbox_new (FALSE, 1);
  gtk_widget_show (vbox2);
  gtk_box_pack_start (GTK_BOX (naviD->hbox), vbox2, FALSE, FALSE, 2);

  {
    gdouble upper_max;
    gdouble initial_val;

    upper_max = p_get_number_of_frames(naviD->ainfo_ptr);
    initial_val = 1 + ((naviD->ainfo_ptr->curr_frame_nr - naviD->ainfo_ptr->first_frame_nr) / naviD->vin_ptr->timezoom);

    naviD->dyn_adj = gtk_adjustment_new (initial_val
                                         , 1
                                         , upper_max
                                         , 1.0, 1.0, 0.0
                                        );
    /* vscale = gtk_vscale_new (GTK_ADJUSTMENT(naviD->dyn_adj)); */
    vscale = gtk_vscrollbar_new (GTK_ADJUSTMENT(naviD->dyn_adj));


    gtk_range_set_update_policy (GTK_RANGE (vscale), GTK_UPDATE_DELAYED); /* GTK_UPDATE_CONTINUOUS */
    gtk_box_pack_start (GTK_BOX (vbox2), vscale, TRUE, TRUE, 0);
    g_signal_connect (naviD->dyn_adj, "value_changed",
                      G_CALLBACK (navi_dyn_adj_changed_callback),
                      naviD);
    gtk_widget_show (vscale);
  }


  /*  The ops buttons  */
  button_box = ops_button_box_new (naviD->shell,
                                   frames_ops_buttons, OPS_BUTTON_NORMAL);
  gtk_box_pack_start (GTK_BOX (vbox), button_box, FALSE, FALSE, 2);
  gtk_widget_show (button_box);


  /*  The VCR ops buttons  */
  button_box = ops_button_box_new (naviD->shell,
                                   vcr_ops_buttons, OPS_BUTTON_NORMAL);
  gtk_box_pack_start (GTK_BOX (vbox), button_box, FALSE, FALSE, 2);
  gtk_widget_show (button_box);


  /* the GimpProgressBar */
  {
    GtkWidget *progressbar;

    progressbar = gimp_progress_bar_new();
    gtk_widget_set_size_request (progressbar, 100, -1);
    gtk_widget_show (progressbar);
    gtk_box_pack_start (GTK_BOX (vbox), progressbar, FALSE, FALSE, 2);
  }

  g_signal_connect (G_OBJECT ( naviD->dyn_frame), "scroll_event",
                    G_CALLBACK (navi_scroll_event_cb),
                    naviD);

  gtk_widget_show (vbox);
  gtk_widget_show (naviD->vbox);

  if(gap_debug) printf("navi_dialog_create END\n");

  return naviD->vbox;
}  /* end navi_dialog_create */


/* ---------------------------------
 * the navigator MAIN dialog
 * ---------------------------------
 */
int  gap_navigator(gint32 image_id)
{
  GtkWidget *shell;
  GtkWidget *subshell;

  if(gap_debug) fprintf(stderr, "\nSTARTing gap_navigator_dialog\n");

  gimp_ui_init ("gap_navigator", FALSE);
  gap_stock_init();

  /*  The main shell */
  shell = gimp_dialog_new (_("Video Navigator"), "gap_navigator",
                           NULL, 0,
                           gimp_standard_help_func, PLUGIN_HELP_ID,

                           GTK_STOCK_CLOSE, GTK_RESPONSE_CLOSE,

                           NULL     /* end marker for va_args */
                           );

  /*  The subshell (toplevel vbox)  */
  subshell = gtk_vbox_new (FALSE, 1);
  gtk_container_add (GTK_CONTAINER (GTK_DIALOG (shell)->vbox), subshell);

  if(gap_debug) printf("BEFORE  navi_dialog_create\n");

  /*  The naviD dialog structure  */
  navi_dialog_create (shell, image_id);
  if(gap_debug) printf("AFTER  navi_dialog_create\n");

  gtk_box_pack_start (GTK_BOX (subshell), naviD->vbox, TRUE, TRUE, 0);

  gtk_widget_show (subshell);
  gtk_widget_show (shell);

  frames_dialog_flush();
  navi_scroll_to_current_frame_nr();

  naviD->timer = g_timeout_add (naviD->cycle_time,
                                (GtkFunction)navi_dialog_poll, NULL);


  naviD->in_dyn_table_sizeinit = FALSE;

  if(gap_debug) printf("BEFORE  gimp_dialog_run\n");

  gimp_dialog_run (GIMP_DIALOG (shell));

  if (naviD->timer >= 0)
    {
      g_source_remove(naviD->timer);
      naviD->timer = -1;
    }

  navi_pviews_reset();

  gtk_widget_destroy (shell);

  if(gap_debug) printf("END gap_navigator_dialog\n");

  return 0;
}





/* XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
 * Some Code Parts copied from gimp-1.2/app directory
 * XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
 */



/* ---------------------------------------  start copy of gimp-1.1.14/app/ops_buttons.c */
/* -- 2003.06.03  hof: changed code by using stock buttons instead of xpm pixmap data */
static gboolean ops_button_pressed_callback  (GtkWidget*, GdkEventButton*, gpointer);
static void ops_button_extended_callback (GtkWidget*, gpointer);


GtkWidget *
ops_button_box_new (GtkWidget     *parent,
                    OpsButton     *ops_button,
                    OpsButtonType  ops_type)
{
  GtkWidget *button;
  GtkWidget *image;
  GtkWidget *button_box;
  GSList    *group = NULL;

  gtk_widget_realize (parent);

  button_box = gtk_hbox_new (TRUE, 1);

  while (ops_button->stock_id)
    {
      switch (ops_type)
        {
        case OPS_BUTTON_NORMAL :
          button = gtk_button_new ();
          image = gtk_image_new_from_stock (ops_button->stock_id,
                                            GTK_ICON_SIZE_BUTTON);
          gtk_container_add (GTK_CONTAINER (button), image);
          gtk_widget_show (image);

          /* need to know del_button and dup_button
           * for setting sensitive when selection was made
           */
          if(ops_button->callback == OPS_BUTTON_FUNC(navi_dialog_frames_delete_frame_callback))
          {
            gtk_widget_set_sensitive(button, FALSE);
            naviD->del_button = button;
          }
          if(ops_button->callback == OPS_BUTTON_FUNC(navi_dialog_frames_duplicate_frame_callback))
          {
            gtk_widget_set_sensitive(button, FALSE);
            naviD->dup_button = button;
          }
          break;
        case OPS_BUTTON_RADIO :
          button = gtk_radio_button_new (group);
          group = gtk_radio_button_get_group (GTK_RADIO_BUTTON (button));
          gtk_container_set_border_width (GTK_CONTAINER (button), 0);
          gtk_toggle_button_set_mode (GTK_TOGGLE_BUTTON (button), FALSE);
          break;
        default :
          button = NULL; /*stop compiler complaints */
          g_error ("ops_button_box_new: unknown type %d\n", ops_type);
          break;
        }

      if (ops_button->ext_callbacks == NULL)
        {
          g_signal_connect_swapped (G_OBJECT (button), "clicked",
                                    G_CALLBACK (ops_button->callback),
                                    NULL);
        }
      else
        {
          gtk_widget_set_events(button, GDK_BUTTON_PRESS_MASK);
          g_signal_connect (G_OBJECT (button), "button_press_event",
                            G_CALLBACK (ops_button_pressed_callback),
                            ops_button);
          g_signal_connect (G_OBJECT (button), "clicked",
                            G_CALLBACK (ops_button_extended_callback),
                            ops_button);
        }

      gimp_help_set_help_data (button,
                               gettext (ops_button->tooltip),
                               ops_button->private_tip);

      gtk_box_pack_start (GTK_BOX (button_box), button, TRUE, TRUE, 0);

      gtk_widget_show (button);

      ops_button->widget = button;
      ops_button->modifier = OPS_BUTTON_MODIFIER_NONE;

      ops_button++;
    }

  return (button_box);
}

static gboolean
ops_button_pressed_callback (GtkWidget      *widget,
                             GdkEventButton *bevent,
                             gpointer        client_data)
{
  OpsButton *ops_button;

  if (client_data == NULL) { return FALSE; }
  ops_button = (OpsButton*)client_data;

  if (bevent->state & GDK_SHIFT_MASK)
    {
      if (bevent->state & GDK_CONTROL_MASK)
          ops_button->modifier = OPS_BUTTON_MODIFIER_SHIFT_CTRL;
      else
        ops_button->modifier = OPS_BUTTON_MODIFIER_SHIFT;
    }
  else if (bevent->state & GDK_CONTROL_MASK)
    ops_button->modifier = OPS_BUTTON_MODIFIER_CTRL;
  else if (bevent->state & GDK_MOD1_MASK)
    ops_button->modifier = OPS_BUTTON_MODIFIER_ALT;
  else
    ops_button->modifier = OPS_BUTTON_MODIFIER_NONE;

  return FALSE;
}

static void
ops_button_extended_callback (GtkWidget *widget,
                              gpointer   client_data)
{
  OpsButton *ops_button;

  g_return_if_fail (client_data != NULL);
  ops_button = (OpsButton*)client_data;

  if (ops_button->modifier > OPS_BUTTON_MODIFIER_NONE &&
      ops_button->modifier < OPS_BUTTON_MODIFIER_LAST)
    {
      if (ops_button->ext_callbacks[ops_button->modifier - 1] != NULL)
        (ops_button->ext_callbacks[ops_button->modifier - 1]) (widget, NULL);
      else
        (ops_button->callback) (widget, NULL);
    }
  else
    (ops_button->callback) (widget, NULL);

  ops_button->modifier = OPS_BUTTON_MODIFIER_NONE;
}
/* ---------------------------------------  end copy of gimp-1.1.14/app/ops_buttons.c */

/* -------------------------------
 * p_get_number_of_frames
 * -------------------------------
 * calculate the number of frames from first and last frame number.
 * the calculated value may not match the real number of available frame image files
 * (ainfo_ptr->frame_cnt) in case some frame images are missing.
 * Note that the current VCR navigator implemetation renders a default icon
 * for each missing frame.
 */
static gint32
p_get_number_of_frames(GapAnimInfo *ainfo_ptr)
{
  gint32 numFrames;
  
  numFrames = 1;
  if (ainfo_ptr != NULL)
  {
    numFrames += abs(ainfo_ptr->last_frame_nr - ainfo_ptr->first_frame_nr);
  }
  return (numFrames);
}
