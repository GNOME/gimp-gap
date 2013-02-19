/* gap_mov_dialog.c
 *   by hof (Wolfgang Hofer)
 *
 * GAP ... Gimp Animation Plugins
 *
 * Dialog Window for Move Path (gap_mov)
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
 * gimp    2.1.0b;  2004/11/04  hof: replaced deprecated option_menu by combo box
 * gimp    2.1.0b;  2004/08/14  hof: feature: point navigation + SHIFT ==> mov_follow_keyframe
 * gimp    2.1.0b;  2004/08/10  hof: bugfix save/load Pathpoints work again.
 * gimp    2.1.0a;  2004/06/26  hof: #144649 use NULL for the default cursor as active_cursor
 * gimp    2.0.1a;  2004/05/01  hof: proc: mov_dialog init mgp->drawable with temporary image (pvals->tmp_image_id)
 *                                   this shows a valid copy of the dest frames at startup
 *                                   (from where the move path  plug-in was invoked)
 * gimp    2.1.0a;  2004/04/18  hof: gtk_window_present(GTK_WINDOW(filesel)) on attempt
 *                                   to open an already open filesel dialog window
 * gimp    1.3.21d; 2003/10/29  hof: removed deprecated calls to gtk_window_set_policy
 * gimp    1.3.20d; 2003/10/14  hof: added bluebox filter effect
 * gimp    1.3.20d; 2003/10/05  hof: use gimp_image_undo_disable for internal temporary images
 *                                   (for better performance and less resources)
 *                                   defaults for anim preview subdialog
 *                                   dynamic preview resize on window resize
 * gimp    1.3.20c; 2003/09/29  hof: new features:  instant_apply, perspective transformation,
 *                                   - tween_layer and trace_layer
 *                                   - changed opacity, rotation and resize from int to gdouble
 * gimp    1.3.17b; 2003/07/31  hof: message text fixes for translators (# 118392)
 * gimp    1.3.16c; 2003/07/12  hof: removed deprecated GtkKPreview widget (replaced by drawing_area based gap_pview_da calls)
 *                                   cursor crosslines ar now switchable (show_cursor flag)
 * gimp    1.3.15a; 2003/06/21  hof: attempt to remove some deprecated calls (no success)
 * gimp    1.3.14b; 2003/06/03  hof: added gap_stock_init
 *                                   replaced mov_gtk_button_new_with_pixmap  by  gtk_button_new_from_stock
 * gimp    1.3.14a; 2003/05/24  hof: moved render procedures to module gap_mov_render
 *                                   placed OK button right.
 *                                   added pixmaps (thanks to Jakub Steiner for providing the pixmaps)
 *                              sven:  replaced _gimp_help_init by gimp_ui_init
 * gimp    1.3.12a; 2003/05/03  hof: merge into CVS-gimp-gap project, replace gimp_help_init by _gimp_help_init
 * gimp    1.3.4b;  2002/03/15  hof: temp. reverted setting of preview widget size.
 * gimp    1.3.4;   2002/03/12  hof: ported to gtk+-2.0.0
 *                                   still needs GTK_DISABLE_DEPRECATED (port is not complete)
 * gimp    1.1.29b; 2000/11/30  hof: new feature: FRAME based Stepmodes, changes for NONINTERACTIVE mode
 * gimp    1.1.23a; 2000/06/04  hof: new button: rotation follow path
 * gimp    1.1.20a; 2000/04/25  hof: support for keyframes, anim_preview (suggested by jakub steiner)
 * gimp    1.1.17b; 2000/02/23  hof: bugfix: dont flatten the preview, just merge visible layers
 *                                   bugfix: for current frame never use diskfile for the preview
 *                                           (to avoid inconsitencies, and to speed up a little)
 *                                   added "Show Path", pick and drag Controlpoints
 * gimp    1.1.17a; 2000/02/20  hof: use gimp_help_set_help_data for tooltips
 *                                   added spinbuttons, and more layout cosmetics.
 * gimp    1.1.15a; 2000/01/26  hof: removed gimp 1.0.x support
 * gimp    1.1.13b; 1999/12/04  hof: some cosmetic gtk fixes
 *                                   changed border_width spacing and Buttons in action area
 *                                   to same style as used in dialogs of the gimp 1.1.13 main dialogs
 * gimp   1.1.8a;   1999/08/31  hof: accept video framenames without underscore '_'
 * gimp   1.1.5a;   1999/05/08  hof: call fileselect in gtk+1.2 style
 * version 0.99.00; 1999.03.03  hof: bugfix: update of the preview (did'nt work with gimp1.1.2)
 * version 0.98.00; 1998.11.28  hof: Port to GIMP 1.1: replaced buildmenu.h, apply layermask (before rotate)
 *                                   mov_imglayer_constrain must check for drawable_id -1
 * version 0.97.00; 1998.10.19  hof: Set window title to "Move Path"
 * version 0.96.02; 1998.07.30  hof: added clip to frame option and tooltips
 * version 0.96.00; 1998.07.09  hof: bugfix (filesel did not reopen after cancel)
 * version 0.95.00; 1998.05.12  hof: added rotatation capabilities
 * version 0.94.00; 1998.04.25  hof: use only one point as default
 *                                   bugfix: initial value for src_paintmode
 *                                           fixes the problem reported in gap_layer_copy_to_dest_image (can't get new layer)
 * version 0.90.00; 1997.12.14  hof: 1.st (pre) release
 */


#include "config.h"

#define MOVE_PATH_LAYOUT_BIG_PREVIEW
/* if defined MOVE_PATH_LAYOUT_BIG_PREVIEW use layout with bigger initial preview size
 * and with frame range selection widget group at right side of the preview
 *
 * the other layout variante has the range selection widget group under the preview
 * (maybe this will be a configure option later)
 */


/* SYTEM (UNIX) includes */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <math.h>
#include <time.h>

/* GIMP includes */
#include "gtk/gtk.h"
#include "gap-intl.h"
#include "libgimp/gimp.h"
#include "libgimp/gimpui.h"

/* GAP includes */
#include "gap_libgapbase.h"
#include "gap_layer_copy.h"
#include "gap_lib.h"
#include "gap_image.h"
#include "gap_mov_exec.h"
#include "gap_mov_xml_par.h"
#include "gap_mov_dialog.h"
#include "gap_mov_render.h"
#include "gap_pdb_calls.h"
#include "gap_vin.h"
#include "gap_arr_dialog.h"

#include "gap_pview_da.h"
#include "gap_accel_da.h"
#include "gap_accel_char.h"
#include "gap_stock.h"


extern      int gap_debug; /* ==0  ... dont print debug infos */

/* Some useful defines and  macros */
#define GAP_MOVE_PATH_HELP_ID             "plug-in-gap-move-path"

#define ENTRY_WIDTH 60
#define SPINBUTTON_WIDTH 60
#define SCALE_WIDTH 125

/* instant apply is implemented via timer, configured to fire 10 times per second (100 msec)
 * this collects instant_apply_requests set by other widget callbacks and events
 * and then update only once.
 * The timer is completely removed, when instant_apply is OFF
 * instant_apply requires much CPU and IO power especially on big images
 * and images with many layers
 */
#define INSTANT_TIMERINTERVAL_MILLISEC  100


#ifdef MOVE_PATH_LAYOUT_BIG_PREVIEW
#define PREVIEW_SIZE 340
#else
#define PREVIEW_SIZE 256
#endif

#define RADIUS           3

#define PREVIEW       0x1
#define CURSOR        0x2
#define PATH_LINE     0x4
#define ALL           0xf

#define GAP_MOV_CHECK_SIZE 8

/*  event masks for the preview widget */
#define PREVIEW_MASK   GDK_EXPOSURE_MASK | \
                       GDK_BUTTON_PRESS_MASK |\
                       GDK_BUTTON_RELEASE_MASK |\
                       GDK_BUTTON_MOTION_MASK


#define POINT_INDEX_LABEL_LENGTH 256

typedef struct {
  gint run;
} t_mov_interface;

typedef struct
{
  GimpDrawable  *drawable;
  gint          dwidth, dheight;
  gint          bpp;
  GapPView       *pv_ptr;
  GimpPixelRgn   src_rgn;
  gint           show_path;
  gint           show_cursor;
  gint           show_grid;
  gint           instant_apply;
  gboolean       instant_apply_request;
  gint32         instant_timertag;
  gint           startup;

  gint          pwidth, pheight;
  gint          curx, cury;              /* x,y of cursor in preview */

  GtkWidget     *filesel;
  GtkAdjustment *x_adj;
  GtkAdjustment *y_adj;
  GtkAdjustment *wres_adj;
  GtkAdjustment *hres_adj;
  GtkAdjustment *opacity_adj;
  GtkAdjustment *rotation_adj;
  GtkAdjustment *keyframe_adj;
  GtkAdjustment *preview_frame_nr_adj;

  GimpColorButton  *bluebox_keycolor_color_button;
  GtkAdjustment *dst_range_start_adj;
  GtkAdjustment *dst_range_end_adj;
  GtkAdjustment *dst_layerstack_adj;
  GtkWidget     *src_force_visible_check_button;
  GtkWidget     *clip_to_img_check_button;
  GtkWidget     *tracelayer_enable_check_button;
  GtkWidget     *src_apply_bluebox_check_button;
  GtkWidget     *paintmode_combo;
  GtkWidget     *stepmode_combo;
  GtkWidget     *handlemode_combo;
  GtkWidget     *src_selmode_combo;
  GtkWidget     *src_layer_combo;

  GtkWidget     *constrain;       /* scale width/height keeps ratio constant */
  GtkAdjustment *ttlx_adj;
  GtkAdjustment *ttly_adj;
  GtkAdjustment *ttrx_adj;
  GtkAdjustment *ttry_adj;
  GtkAdjustment *tblx_adj;
  GtkAdjustment *tbly_adj;
  GtkAdjustment *tbrx_adj;
  GtkAdjustment *tbry_adj;
  GtkAdjustment *sel_feather_radius_adj;
  GtkAdjustment *step_speed_factor_adj;
  GtkAdjustment *tween_opacity_initial_adj;
  GtkAdjustment *tween_opacity_desc_adj;
  GtkAdjustment *trace_opacity_initial_adj;
  GtkAdjustment *trace_opacity_desc_adj;
  GtkAdjustment *tween_steps_adj;

  GtkAdjustment *accPosition_adj;
  GtkAdjustment *accOpacity_adj;
  GtkAdjustment *accSize_adj;
  GtkAdjustment *accRotation_adj;
  GtkAdjustment *accPerspective_adj;
  GtkAdjustment *accSelFeatherRadius_adj;


  gchar          point_index_text[POINT_INDEX_LABEL_LENGTH];
  GtkWidget     *point_index_frame;
  gint           p_x, p_y;
  gdouble        opacity;
  gdouble        w_resize;
  gdouble        h_resize;
  gdouble        rotation;
  gdouble        ttlx;     /* 0.0 upto 10.0 transform x top left */
  gdouble        ttly;     /* 0.0 upto 10.0 transform y top left */
  gdouble        ttrx;     /* 0.0 upto 10.0 transform x top right */
  gdouble        ttry;     /* 0.0 upto 10.0 transform y top right */
  gdouble        tblx;     /* 0.0 upto 10.0 transform x bot left */
  gdouble        tbly;     /* 0.0 upto 10.0 transform y bot left */
  gdouble        tbrx;     /* 0.0 upto 10.0 transform x bot right */
  gdouble        tbry;     /* 0.0 upto 10.0 transform y bot right */
  gdouble        sel_feather_radius;

  /* acceleration characteristics */
  gint           accPosition;
  gint           accOpacity;
  gint           accSize;
  gint           accRotation;
  gint           accPerspective;
  gint           accSelFeatherRadius;

  gint           keyframe_abs;
  gint           max_frame;


  gint           preview_frame_nr;      /* default: current frame */
  gint           old_preview_frame_nr;
  GapAnimInfo   *ainfo_ptr;

  gint          in_call;
  char         *pointfile_name;

  gint                first_nr;
  gint                last_nr;
  GtkWidget          *shell;
  gint                shell_initial_width;
  gint                shell_initial_height;
  GtkWidget          *master_vbox;
  GdkCursor     *cursor_wait;
  GdkCursor     *cursor_acitve;
  GimpRGB               pathcolor;


  GtkWidget            *segNumberLabel;
  GtkWidget            *segLengthLabel;
  GtkWidget            *segSpeedLabel;

  /* The movepath dialog has a RecordOnlyMode that is intended
   * to edit and save parameter settings only.
   *
   * TRUE
   *   (typically called from the storyboard for update xml file settings)
   * - invoke from any image is tolerated
   * - rendering of frames is DISABLED 
   *   (Animated preview rendering is allowed)
   * - frame range limits are 1 upto 999999.
   * - the moving object (source image) is fixed by the caller,
   *   stepmode is restricted to GAP_STEP_NONE
   * - OK button triggers automatic save of the settings
   *   to XML file in overwrite mode without confirm question
   *   (but does not render anything)
   *
   * FALSE
   *   (typically called as PDB procedure for productive render purpose)
   * - invoke is limited to an image that represents a frame
   *   (image with gap typical number part in the filename)
   * - rendering of frames is enabled
   * - frame range limits constraint to first and last frame number
   *   found in frame imagefilenames on disc.
   * - the moving object (source image) can be selected by the user
   *   from the list of opened images in the current gimp session:
   *   all stepmodes are available.
   *
   */
  gboolean isRecordOnlyMode;
  gboolean isStandaloneGui;
  
  t_close_movepath_edit_callback_fptr close_fptr;
  gpointer callback_data;
  
  gchar        xml_paramfile[GAP_MOVPATH_XML_FILENAME_MAX_LENGTH];

} t_mov_gui_stuff;


/* Declare local functions.
 */
static long        p_gap_mov_dlg_move_dialog(t_mov_gui_stuff *mgp
                       , GapMovData *mov_ptr
                       , gboolean isRecordOnlyMode
                       , gboolean isStandaloneGui
                       , t_close_movepath_edit_callback_fptr close_fptr
                       , gpointer callback_data
                       , gint32 nframes
                       );
static void        p_free_mgp_resources(t_mov_gui_stuff *mgp);
static void        p_update_point_index_text (t_mov_gui_stuff *mgp);
static void        p_set_sensitivity_by_adjustment(GtkAdjustment *adj, gboolean sensitive);
static void        p_accel_widget_sensitivity(t_mov_gui_stuff *mgp);
static void        p_points_from_tab         (t_mov_gui_stuff *mgp);
static void        p_points_to_tab           (t_mov_gui_stuff *mgp);
static void        p_point_refresh           (t_mov_gui_stuff *mgp);
static void        p_pick_nearest_point      (gint px, gint py);
static void        p_reset_points            ();
static void        p_clear_one_point         (gint idx);
static void        p_mix_one_point(gint idx, gint ref1, gint ref2, gdouble mix_factor);
static void        p_refresh_widgets_after_load(t_mov_gui_stuff *mgp);
static void        p_load_points             (char *filename);
static void        p_save_points             (char *filename, t_mov_gui_stuff *mgp);

static GimpDrawable * p_get_flattened_drawable (gint32 image_id);
static GimpDrawable * p_get_prevw_drawable (t_mov_gui_stuff *mgp);

static gint        mov_dialog ( GimpDrawable *drawable, t_mov_gui_stuff *mgp,
                                gint min, gint max);
static GtkWidget * mov_modify_tab_create (t_mov_gui_stuff *mgp);
static GtkWidget * mov_trans_tab_create  (t_mov_gui_stuff *mgp);
static GtkWidget * mov_acc_tab_create  (t_mov_gui_stuff *mgp);
static GtkWidget * mov_selection_handling_tab_create (t_mov_gui_stuff *mgp);

static void        mov_path_prevw_create ( GimpDrawable *drawable,
                                           t_mov_gui_stuff *mgp,
                                           gboolean vertical_layout);
static void        mov_refresh_src_layer_menu(t_mov_gui_stuff *mgp);
static GtkWidget * mov_src_sel_create (t_mov_gui_stuff *mgp);
static GtkWidget * mov_advanced_tab_create(t_mov_gui_stuff *mgp);
static GtkWidget * mov_edit_button_box_create (t_mov_gui_stuff *mgp);
static GtkWidget * mov_path_framerange_box_create(t_mov_gui_stuff *mgp,
                                                  gboolean vertical_layout
                                                  );
static void        mov_path_prevw_preview_init ( t_mov_gui_stuff *mgp );
static void        mov_path_prevw_draw ( t_mov_gui_stuff *mgp, gint update );
static void        mov_instant_int_adjustment_update (GtkObject *obj, gpointer val);
static void        mov_instant_double_adjustment_update (GtkObject *obj, gpointer val);
static void        mov_path_colorbutton_update ( GimpColorButton *widget, t_mov_gui_stuff *mgp);
static void        mov_path_keycolorbutton_clicked ( GimpColorButton *widget, t_mov_gui_stuff *mgp);
static void        mov_path_keycolorbutton_changed ( GimpColorButton *widget, t_mov_gui_stuff *mgp);
static void        mov_path_keyframe_update ( GtkWidget *widget, t_mov_gui_stuff *mgp );
static void        mov_path_x_adjustment_update ( GtkWidget *widget, gpointer data );
static void        mov_path_y_adjustment_update ( GtkWidget *widget, gpointer data );
static void        mov_path_tfactor_adjustment_update( GtkWidget *widget, gdouble *val);
static void        mov_path_acceleration_adjustment_update( GtkWidget *widget, gint *val);
static void        mov_path_feather_adjustment_update( GtkWidget *widget, gdouble *val );
static void        mov_selmode_menu_callback (GtkWidget *widget, t_mov_gui_stuff *mgp);


static void        mov_path_resize_adjustment_update( GtkObject *obj, gdouble *val);
static void        mov_path_tween_steps_adjustment_update( GtkObject *obj, gint *val);
static void        mov_path_prevw_cursor_update ( t_mov_gui_stuff *mgp );
static gint        mov_path_prevw_preview_expose ( GtkWidget *widget, GdkEvent *event );
static gint        mov_path_prevw_preview_events ( GtkWidget *widget, GdkEvent *event );
static gint        p_chk_keyframes(t_mov_gui_stuff *mgp);

static void        mov_grab_bezier_path(t_mov_gui_stuff *mgp
                         , gint32 vectors_id
                         , gint32 stroke_id
                         , const char *vectorname
                         );
static void        mov_grab_anchorpoints_path(t_mov_gui_stuff *mgp
                                             ,gint num_path_point_details
                                             ,gdouble *points_details
                                             );


static void     mov_upd_seg_labels       (GtkWidget *widget, t_mov_gui_stuff *mgp);
static void     mov_padd_callback        (GtkWidget *widget,gpointer data);
static void     mov_pgrab_callback       (GtkWidget *widget,GdkEventButton *bevent,gpointer data);
static void     mov_pins_callback        (GtkWidget *widget,gpointer data);
static void     mov_pdel_callback        (GtkWidget *widget,gpointer data);
static void     mov_follow_keyframe      (t_mov_gui_stuff *mgp);
static void     mov_pnext_callback       (GtkWidget *widget,GdkEventButton *bevent,gpointer data);
static void     mov_pprev_callback       (GtkWidget *widget,GdkEventButton *bevent,gpointer data);
static void     mov_pfirst_callback      (GtkWidget *widget,GdkEventButton *bevent,gpointer data);
static void     mov_plast_callback       (GtkWidget *widget,GdkEventButton *bevent,gpointer data);
static void     mov_pdel_all_callback    (GtkWidget *widget,gpointer data);
static void     mov_pclr_callback        (GtkWidget *widget,gpointer data);
static void     mov_pclr_all_callback    (GtkWidget *widget,GdkEventButton *bevent,gpointer data);
static void     mov_prot_follow_callback (GtkWidget *widget,GdkEventButton *bevent,gpointer data);
static void     mov_pload_callback       (GtkWidget *widget,gpointer data);
static void     mov_psave_callback       (GtkWidget *widget,gpointer data);
static void     p_points_load_from_file  (GtkWidget *widget,t_mov_gui_stuff *mgp);
static void     p_points_save_to_file    (GtkWidget *widget,t_mov_gui_stuff *mgp);

static gboolean mov_check_valid_src_layer(t_mov_gui_stuff   *mgp);
static void     mov_help_callback        (GtkWidget *widget, t_mov_gui_stuff *mgp);
static void     mov_close_callback       (GtkWidget *widget, t_mov_gui_stuff *mgp);
static void     mov_ok_callback          (GtkWidget *widget, t_mov_gui_stuff *mgp);
static void     mov_upvw_callback        (GtkWidget *widget, t_mov_gui_stuff *mgp);
static void     mov_apv_callback         (GtkWidget *widget,gpointer data);
static void     p_filesel_close_cb       (GtkWidget *widget, t_mov_gui_stuff *mgp);

static gint mov_imglayer_constrain      (gint32 image_id, gint32 drawable_id, gpointer data);
static void mov_imglayer_menu_callback  (GtkWidget *, t_mov_gui_stuff *mgp);
static void mov_paintmode_menu_callback (GtkWidget *, t_mov_gui_stuff *mgp);
static void mov_handmode_menu_callback  (GtkWidget *, t_mov_gui_stuff *mgp);
static void mov_stepmode_menu_callback  (GtkWidget *, t_mov_gui_stuff *mgp);
static void mov_tweenlayer_sensitivity(t_mov_gui_stuff *mgp);
static void mov_tracelayer_sensitivity(t_mov_gui_stuff *mgp);
static void mov_gint_toggle_callback    (GtkWidget *, gpointer);
static void mov_force_visibility_toggle_callback ( GtkWidget *widget, gpointer data );
static void mov_bluebox_callback        (GtkWidget *, gpointer);
static void mov_tracelayer_callback     (GtkWidget *, gpointer);
static void mov_show_path_or_cursor     (t_mov_gui_stuff *mgp);
static void mov_show_path_callback      (GtkWidget *, t_mov_gui_stuff *mgp);
static void mov_show_cursor_callback    (GtkWidget *, t_mov_gui_stuff *mgp);
static void mov_show_grid_callback      (GtkWidget *, t_mov_gui_stuff *mgp);
static void mov_install_timer           (t_mov_gui_stuff *mgp);
static void mov_remove_timer            (t_mov_gui_stuff *mgp);
static void mov_instant_timer_callback (gpointer   user_data);
static void mov_instant_apply_callback  (GtkWidget *, t_mov_gui_stuff *mgp);
static void mov_set_instant_apply_request(t_mov_gui_stuff *mgp);
static void mov_set_waiting_cursor       (t_mov_gui_stuff *mgp);
static void mov_set_active_cursor        (t_mov_gui_stuff *mgp);

GtkObject *
p_mov_spinbutton_new(GtkTable *table
                    ,gint      col
                    ,gint      row
                    ,gchar    *label_text
                    ,gint      scale_width      /* dummy, not used */
                    ,gint      spinbutton_width
                    ,gdouble   initial_val
                    ,gdouble   lower            /* dummy, not used */
                    ,gdouble   upper            /* dummy, not used */
                    ,gdouble   sstep
                    ,gdouble   pagestep
                    ,gint      digits
                    ,gboolean  constrain
                    ,gdouble   umin
                    ,gdouble   umax
                    ,gchar    *tooltip_text
                    ,gchar    *privatetip
                    );
GtkObject *
p_mov_acc_spinbutton_new(GtkTable *table
                    ,gint      col
                    ,gint      row
                    ,gchar    *label_text
                    ,gint      scale_width      /* dummy, not used */
                    ,gint      spinbutton_width
                    ,gdouble   initial_val
                    ,gdouble   lower            /* dummy, not used */
                    ,gdouble   upper            /* dummy, not used */
                    ,gdouble   sstep
                    ,gdouble   pagestep
                    ,gint      digits
                    ,gboolean  constrain
                    ,gdouble   umin
                    ,gdouble   umax
                    ,gchar    *tooltip_text
                    ,gchar    *privatetip
                    );
static void  mov_fit_initial_shell_window(t_mov_gui_stuff *mgp);
static void  mov_shell_window_size_allocate (GtkWidget       *widget,
                                             GtkAllocation   *allocation,
                                             gpointer         user_data);

static void  mov_pview_size_allocate_callback(GtkWidget *widget
                                , GtkAllocation *allocation
                                , t_mov_gui_stuff *mgp
                                );


static GapMovValues *pvals;

static t_mov_interface mov_int =
{
  FALSE     /* run */
};


/* ============================================================================
 ***********************
 *                     *
 *  Dialog interfaces  *
 *                     *
 ***********************
 * ============================================================================
 */
long
gap_mov_dlg_move_dialog    (GapMovData *mov_ptr)
{
  t_mov_gui_stuff *mgp;
  gboolean isRecordOnlyMode;
  gboolean isStandaloneGui;


  mgp = g_new( t_mov_gui_stuff, 1 );
  mgp->shell = NULL;
  mgp->pointfile_name = NULL;
  
  isRecordOnlyMode = FALSE;
  isStandaloneGui = TRUE;
  p_gap_mov_dlg_move_dialog(mgp, mov_ptr, isRecordOnlyMode, isStandaloneGui, NULL, NULL, 1);
  p_free_mgp_resources(mgp);
  g_free(mgp);

  
  if(mov_int.run == TRUE)
  {
    return 0;  /* OK */
  }
  return  -1;  /* Cancel or error occured */
 
}


/* ----------------------------------
 * gap_mov_dlg_edit_movepath_dialog
 * ----------------------------------
 * This procedure creates and shows the movepath edit dialog as widget and returns
 * the created widget.
 * The caller shall provide close_fptr and callback_data. This function callback
 * will be called on close of the widget.
 * Note that resources refered by pvals, ainfo_ptr xml_paramfile
 * must not be freed until the edit dialog is closed.
 *
 * This procedure is typically called be the Storyboard transition attributes dialog
 */
GtkWidget * 
gap_mov_dlg_edit_movepath_dialog (gint32 frame_image_id, gint32 drawable_id
   , const char *xml_paramfile
   , GapAnimInfo *ainfo_ptr
   , GapMovValues *pvals_edit
   , t_close_movepath_edit_callback_fptr close_fptr
   , gpointer callback_data
   , gint32 nframes
   )
{
  t_mov_gui_stuff *mgp;
  gboolean isRecordOnlyMode;
  gboolean isStandaloneGui;
  GapMovData  l_mov_data;
  gboolean isXmlLoadOk;
  gboolean l_rc;

  if(gap_debug)
  {
     printf("gap_mov_dlg_edit_movepath_dialog START frame_image_id:%d drawable_id:%d xml:%s\n"
           " fptr:%d data:%d\n"
      ,(int)frame_image_id
      ,(int)drawable_id
      ,xml_paramfile
      ,(int)close_fptr
      ,(int)callback_data
      );
  }

  isRecordOnlyMode = TRUE;
  isStandaloneGui = FALSE;


  mgp = g_new( t_mov_gui_stuff, 1 );
  mgp->shell = NULL;
  mgp->pointfile_name = NULL;


  pvals = pvals_edit;
  
  pvals->dst_image_id = frame_image_id;
  pvals->bbp_pv = gap_bluebox_bbp_new(-1);

  isXmlLoadOk = FALSE;
  mgp->xml_paramfile[0] = '\0';
  if(xml_paramfile[0] != '\0')
  {
    g_snprintf(mgp->xml_paramfile, sizeof(mgp->xml_paramfile), "%s", xml_paramfile);
    mgp->pointfile_name  = g_strdup_printf("%s", xml_paramfile);
 
    /* attempt to init settings in case the xml_paramfile
     * already contains valid settings
     */
    isXmlLoadOk =  gap_mov_xml_par_load(xml_paramfile
                                  , pvals
                                  , gimp_image_width(frame_image_id)
                                  , gimp_image_height(frame_image_id)
                                  );
  }

  pvals->src_image_id = gimp_drawable_get_image(drawable_id);
  pvals->src_layer_id = drawable_id;

  if(isXmlLoadOk != TRUE)
  {
    pvals->src_handle = GAP_HANDLE_LEFT_TOP;
    pvals->src_selmode = GAP_MOV_SEL_IGNORE;
    pvals->src_paintmode = GIMP_NORMAL_MODE;
    pvals->src_force_visible  = 1;
    pvals->src_apply_bluebox  = 0;
    pvals->bbp  = NULL;
    pvals->bbp_pv  = NULL;
    pvals->clip_to_img  = 0;
    
    pvals->step_speed_factor = 1.0;
    pvals->tracelayer_enable = FALSE;
    pvals->trace_opacity_initial = 100.0;
    pvals->trace_opacity_desc = 80.0;
    pvals->tween_steps = 0;
    pvals->tween_opacity_initial = 80.0;
    pvals->tween_opacity_desc = 80.0;

    p_reset_points();
  }


  l_rc = FALSE;
  if(ainfo_ptr != NULL)
  {
    l_mov_data.val_ptr = pvals_edit;
    l_mov_data.singleFramePtr = NULL;
    l_mov_data.val_ptr->cache_src_image_id = -1;
    l_mov_data.dst_ainfo_ptr = ainfo_ptr;

    p_gap_mov_dlg_move_dialog(mgp, &l_mov_data
       , isRecordOnlyMode, isStandaloneGui
       , close_fptr, callback_data
       , nframes);
  }

  if(mgp->shell != NULL)
  {
    gtk_window_present(GTK_WINDOW(mgp->shell));
  }

  if(gap_debug)
  {
    printf("gap_mov_dlg_edit_movepath_dialog DONE\n");
  }

  return(mgp->shell);

}  /* end gap_mov_dlg_edit_movepath_dialog */


/* ------------------------------------------
 * p_gap_mov_dlg_move_dialog
 * ------------------------------------------
 *
 */
static long
p_gap_mov_dlg_move_dialog(t_mov_gui_stuff *mgp
   , GapMovData *mov_ptr
   , gboolean isRecordOnlyMode
   , gboolean isStandaloneGui
   , t_close_movepath_edit_callback_fptr close_fptr
   , gpointer callback_data
   , gint32 nframes
   )
{
  GimpDrawable *l_drawable_ptr;
  gint       l_first;
  gint       l_last;
  gint       l_max;
  gint       l_curr;
  char      *l_str;

  if(gap_debug)
  {
    printf("gap_mov_dlg_move_dialog START mgp:%d isRecordOnlyMode:%d isStandaloneGui:%d\n"
      , (int)mgp
      , (int)isRecordOnlyMode
      , (int)isStandaloneGui
      );
  }
  if(mgp == NULL)
  {
    printf("error can't alloc path_preview structure\n");
    return -1;
  }
  mgp->close_fptr = close_fptr;
  mgp->callback_data = callback_data;
  mgp->isRecordOnlyMode = isRecordOnlyMode;
  mgp->isStandaloneGui = isStandaloneGui;
  mgp->shell_initial_width = -1;
  mgp->shell_initial_height = -1;
  mgp->show_path = TRUE;
  mgp->show_cursor = TRUE;
  mgp->show_grid = FALSE;
  mgp->instant_apply = FALSE;
  mgp->instant_apply_request = FALSE;
  mgp->instant_timertag = -1;
  mgp->startup = TRUE;
  mgp->keyframe_adj = NULL;
  mgp->preview_frame_nr_adj = NULL;
  mgp->pv_ptr = NULL;
  mgp->cursor_wait = gdk_cursor_new (GDK_WATCH);
  mgp->cursor_acitve = NULL; /* use the default cursor */
  mgp->step_speed_factor_adj = NULL;
  mgp->tween_opacity_initial_adj = NULL;
  mgp->tween_opacity_desc_adj = NULL;
  mgp->trace_opacity_initial_adj = NULL;
  mgp->sel_feather_radius_adj = NULL;
  mgp->segNumberLabel = NULL;
  mgp->segLengthLabel = NULL;
  mgp->segSpeedLabel = NULL;

  mgp->dst_range_start_adj = NULL;
  mgp->dst_range_end_adj = NULL;
  mgp->dst_layerstack_adj = NULL;
  mgp->src_force_visible_check_button = NULL;
  mgp->clip_to_img_check_button = NULL;
  mgp->tracelayer_enable_check_button = NULL;
  mgp->src_apply_bluebox_check_button = NULL;
  mgp->bluebox_keycolor_color_button = NULL;
  mgp->paintmode_combo = NULL;
  mgp->stepmode_combo = NULL;
  mgp->handlemode_combo = NULL;
  mgp->src_selmode_combo = NULL;
  mgp->src_layer_combo = NULL;


  pvals = mov_ptr->val_ptr;

  
  if(mgp->pointfile_name == NULL)
  {
    if(mov_ptr->dst_ainfo_ptr->basename != NULL)
    {
      l_str = gap_base_strdup_del_underscore(mov_ptr->dst_ainfo_ptr->basename);
      mgp->pointfile_name  = g_strdup_printf("%s.movepath.xml", l_str);
      g_free(l_str);
    }
    else
    {
      mgp->pointfile_name = g_strdup("movepath.xml");
    }
  }

  if(mgp->isRecordOnlyMode)
  {
    l_first = 1;
    l_max  = 9999;
    l_last = nframes;
    l_curr  = 1;

    mov_ptr->dst_ainfo_ptr->first_frame_nr = 1;
    mov_ptr->dst_ainfo_ptr->last_frame_nr = nframes;
    mov_ptr->dst_ainfo_ptr->curr_frame_nr = 1;

    pvals->src_stepmode = GAP_STEP_NONE;
    pvals->src_selmode = GAP_MOV_SEL_IGNORE;
  }
  else
  {
    l_first = mov_ptr->dst_ainfo_ptr->first_frame_nr;
    l_last  = mov_ptr->dst_ainfo_ptr->last_frame_nr;
    l_curr  = mov_ptr->dst_ainfo_ptr->curr_frame_nr;
    l_max   = l_last;
    
    pvals->src_image_id = -1;
    pvals->src_layer_id = -1;
    pvals->src_stepmode = GAP_STEP_LOOP;
 
  }

  /* init parameter values */
  pvals->dst_image_id = mov_ptr->dst_ainfo_ptr->image_id;
  pvals->tmp_image_id = -1;
  pvals->tmpsel_image_id= -1;
  pvals->tmpsel_channel_id = -1;
  pvals->tmp_alt_image_id = -1;
  pvals->tmp_alt_framenr = -1;
  pvals->tween_image_id = -1;
  pvals->trace_image_id = -1;

  pvals->apv_mode  = GAP_APV_QUICK;
  pvals->apv_src_frame  = -1;
  pvals->apv_mlayer_image  = -1;
  pvals->apv_gap_paste_buff  = NULL;
  pvals->apv_scalex  = 40.0;
  pvals->apv_scaley  = 40.0;

  pvals->cache_src_image_id  = -1;
  pvals->cache_tmp_image_id  = -1;
  pvals->cache_tmp_layer_id  = -1;
  pvals->cache_frame_number  = -1;
  pvals->cache_ainfo_ptr = NULL;

  if(mgp->isRecordOnlyMode != TRUE)
  {
    pvals->src_handle = GAP_HANDLE_LEFT_TOP;
    pvals->src_selmode = GAP_MOV_SEL_IGNORE;
    pvals->src_paintmode = GIMP_NORMAL_MODE;
    pvals->src_force_visible  = 1;
    pvals->src_apply_bluebox  = 0;
    pvals->bbp  = NULL;
    pvals->bbp_pv  = NULL;
    pvals->clip_to_img  = 0;
    
    pvals->step_speed_factor = 1.0;
    pvals->tracelayer_enable = FALSE;
    pvals->trace_opacity_initial = 100.0;
    pvals->trace_opacity_desc = 80.0;
    pvals->tween_steps = 0;
    pvals->tween_opacity_initial = 80.0;
    pvals->tween_opacity_desc = 80.0;

    p_reset_points();
  }

  /* pvals->point[1].p_x = 100; */  /* default: move from 0/0 to 100/0 */

  pvals->dst_range_start = l_curr;
  pvals->dst_range_end   = l_last;
  pvals->dst_layerstack = 0;   /* 0 ... insert layer on top of stack */

  mgp->filesel = NULL;   /* fileselector is not open */
  mgp->ainfo_ptr            = mov_ptr->dst_ainfo_ptr;
  mgp->preview_frame_nr     = l_curr;
  mgp->old_preview_frame_nr = mgp->preview_frame_nr;
  mgp->point_index_frame = NULL;

  p_points_from_tab(mgp);
  p_update_point_index_text(mgp);

  /* duplicate the curerent image (for flatten & preview)  */
  pvals->tmp_image_id = gimp_image_duplicate(pvals->dst_image_id);

  /* wanted to disable undo for performance reasons
   * but if this is done here, the initial preview is black
   * and get error (gap_main:######): LibGimp-CRITICAL **: file gimppixelrgn.c: line 268 (gimp_pixel_
   * TODO: findout why, then try disable undo again
   */
  /* gimp_image_undo_disable(pvals->tmp_image_id); */

  /* flatten image, and get the (only) resulting drawable */
  l_drawable_ptr = p_get_prevw_drawable(mgp);

  /* do DIALOG window */
  mov_dialog(l_drawable_ptr, mgp, l_first, l_max);


  if(gap_debug)
  {
    printf("GAP-DEBUG: END gap_mov_dlg_move_dialog\n");
  }

}  /* end p_gap_mov_dlg_move_dialog */


/* --------------------------------
 * p_free_mgp_resources
 * --------------------------------
 */
static void
p_free_mgp_resources(t_mov_gui_stuff *mgp)
{
  if(mgp == NULL)
  {
    return;
  }
  
  if(gap_debug)
  {
    printf("p_free_mgp_resources START\n");
  }
  
  /* destroy the tmp image(s) */
  if(pvals->tmp_image_id >= 0)
  {
    gimp_image_delete(pvals->tmp_image_id);
    pvals->tmp_image_id = -1;
  }
  if(pvals->tmp_alt_image_id >= 0)
  {
    gimp_image_delete(pvals->tmp_alt_image_id);
    pvals->tmp_alt_image_id = -1;
  }

  /* delete the temp selection image */
  if(gap_image_is_alive(pvals->tmpsel_image_id))
  {
    gimp_image_delete(pvals->tmpsel_image_id);
  }

  pvals->tmpsel_image_id = -1;
  pvals->tmpsel_channel_id = -1;

  if(mgp->pointfile_name != NULL)
  {
    g_free(mgp->pointfile_name);
    mgp->pointfile_name = NULL;
  }

  /* remove timer if there is one */
  mov_remove_timer(mgp);

}  /* end p_free_mgp_resources */


/* ============================================================================
 *******************
 *                 *
 *   Main Dialog   *
 *                 *
 *******************
 * ============================================================================
 */

static gint
mov_dialog ( GimpDrawable *drawable, t_mov_gui_stuff *mgp,
             gint first_nr, gint last_nr )
{
  GtkWidget *notebook;
  GtkWidget *vbox;
  GtkWidget *hbbox;
  GtkWidget *spc_hbox;
  GtkWidget *dlg;
  GtkWidget *frame;
  GtkWidget *button;
  GtkWidget *label;
  GtkWidget *src_sel_frame;
  GtkWidget *advanced_frame;
  GtkWidget *framerange_table;
  gboolean  vertical_layout;

  if(gap_debug) printf("GAP-DEBUG: START mov_dialog\n");

  if(mgp->isStandaloneGui)
  {
    gimp_ui_init ("gap_move", FALSE);
    gap_stock_init();
  }

#ifdef MOVE_PATH_LAYOUT_BIG_PREVIEW
  vertical_layout = TRUE;
#else
  vertical_layout = FALSE;
#endif
  /* dialog */
  dlg = gtk_dialog_new ();
  gtk_window_set_type_hint (GTK_WINDOW (dlg), GDK_WINDOW_TYPE_HINT_NORMAL);
  mgp->shell = dlg;
  mgp->first_nr = first_nr;
  mgp->last_nr = last_nr;

  if(mgp->isRecordOnlyMode)
  {
    gtk_window_set_title (GTK_WINDOW (dlg), _("Move Path Editor"));
  }
  else
  {
    gtk_window_set_title (GTK_WINDOW (dlg), _("Move Path"));
  }
  gtk_window_set_position (GTK_WINDOW (dlg), GTK_WIN_POS_MOUSE);
  g_signal_connect (G_OBJECT (dlg), "destroy",
                    G_CALLBACK (mov_close_callback),
                    mgp);
  gtk_window_set_resizable(GTK_WINDOW (mgp->shell), TRUE);

  /*  Action area  */
  gtk_container_set_border_width (GTK_CONTAINER (GTK_DIALOG (dlg)->action_area), 2);
  gtk_box_set_homogeneous (GTK_BOX (GTK_DIALOG (dlg)->action_area), FALSE);

  hbbox = gtk_hbutton_box_new ();
  gtk_box_set_spacing (GTK_BOX (hbbox), 2);
  gtk_box_pack_end (GTK_BOX (GTK_DIALOG (dlg)->action_area), hbbox, FALSE, FALSE, 0);
  gtk_widget_show (hbbox);


  /* the HELP button */
  if (gimp_show_help_button ())
    {
      button = gtk_button_new_from_stock ( GTK_STOCK_HELP);
      GTK_WIDGET_SET_FLAGS (button, GTK_CAN_DEFAULT);
      gtk_box_pack_end (GTK_BOX (hbbox), button, FALSE, TRUE, 0);
      gtk_widget_show (button);
      g_signal_connect (G_OBJECT (button), "clicked",
                        G_CALLBACK(mov_help_callback),
                        mgp);
    }

  /* the CANCEL button */
  button = gtk_button_new_from_stock ( GTK_STOCK_CANCEL);
  GTK_WIDGET_SET_FLAGS (button, GTK_CAN_DEFAULT);
  gtk_box_pack_start (GTK_BOX (hbbox), button, TRUE, TRUE, 0);
  gtk_widget_show (button);
  g_signal_connect (G_OBJECT (button), "clicked",
                    G_CALLBACK(mov_close_callback),
                    mgp);

  /* button = gtk_button_new_with_label */
  button = gtk_button_new_from_stock ( GTK_STOCK_REFRESH );
  GTK_WIDGET_SET_FLAGS (button, GTK_CAN_DEFAULT);
  gtk_box_pack_start (GTK_BOX (hbbox), button, TRUE, TRUE, 0);
  gimp_help_set_help_data(button,
                       _("Show preview frame with selected source layer at current controlpoint")
                       , NULL);
  gtk_widget_show (button);
  g_signal_connect (G_OBJECT (button), "clicked",
                    G_CALLBACK  (mov_upvw_callback),
                    mgp);

  button = gtk_button_new_from_stock ( GAP_STOCK_ANIM_PREVIEW );
  GTK_WIDGET_SET_FLAGS (button, GTK_CAN_DEFAULT);
  gtk_box_pack_start (GTK_BOX (hbbox), button, TRUE, TRUE, 0);
  gimp_help_set_help_data(button,
                       _("Generate animated preview as multilayer image")
                       , NULL);
  gtk_widget_show (button);
  g_signal_connect (G_OBJECT (button), "clicked",
                    G_CALLBACK (mov_apv_callback),
                    mgp);


  button = gtk_button_new_from_stock ( GTK_STOCK_OK);
  GTK_WIDGET_SET_FLAGS (button, GTK_CAN_DEFAULT);
  gtk_box_pack_start (GTK_BOX (hbbox), button, TRUE, TRUE, 0);
  gtk_widget_grab_default (button);
  gtk_widget_show (button);
  g_signal_connect (G_OBJECT (button), "clicked",
                    G_CALLBACK  (mov_ok_callback),
                    mgp);

  /*  parameter settings  */
  spc_hbox = gtk_hbox_new (FALSE, 0);
  gtk_widget_show (spc_hbox);

  frame = gimp_frame_new ( _("Copy moving source-layer(s) into frames"));
  gtk_widget_show (frame);

  gtk_box_pack_start (GTK_BOX (spc_hbox), frame, TRUE, TRUE, 10);
  gtk_box_pack_start (GTK_BOX (GTK_DIALOG (dlg)->vbox), spc_hbox, TRUE, TRUE, 2);


  /* the vbox */
  vbox = gtk_vbox_new (FALSE, 3);
  gtk_container_set_border_width (GTK_CONTAINER (vbox), 2);
  gtk_container_add (GTK_CONTAINER (frame), vbox);
  gtk_widget_show (vbox);

  /* the notebook */
  notebook = gtk_notebook_new();
  {
    GtkWidget *hbox;

    hbox = gtk_hbox_new (FALSE, 3);
    gtk_widget_show (hbox);
    gtk_box_pack_start (GTK_BOX (hbox), notebook, TRUE, TRUE, 4);
    gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 0);
  }

  /* the source select frame */
  src_sel_frame = mov_src_sel_create (mgp);
  gtk_container_add (GTK_CONTAINER (notebook), src_sel_frame);
  label = gtk_label_new(_("Source Select"));
  gtk_notebook_set_tab_label (GTK_NOTEBOOK (notebook)
                             , gtk_notebook_get_nth_page (GTK_NOTEBOOK (notebook), 0)
                             , label
                             );
  /* the advanced features frame */
  advanced_frame = mov_advanced_tab_create(mgp);
  gtk_container_add (GTK_CONTAINER (notebook), advanced_frame);
  label = gtk_label_new(_("Advanced Settings"));
  gtk_notebook_set_tab_label (GTK_NOTEBOOK (notebook)
                             , gtk_notebook_get_nth_page (GTK_NOTEBOOK (notebook), 1)
                             , label
                             );
  gtk_widget_show (notebook);

  /* the path preview frame (with all the controlpoint widgets) */
  mgp->max_frame = MAX(first_nr, last_nr);
  mgp->master_vbox = vbox;
  mov_path_prevw_create ( drawable, mgp, vertical_layout);

  if(!vertical_layout)
  {
    /* the box with the framerange selection widgets
     * (if we have vertical_layout using the BIG Preview
     *  the framerange_table was already done in the mov_path_prevw_create procedure,
     *  otherwise we have to create it now)
     */
    framerange_table = mov_path_framerange_box_create(mgp, vertical_layout);
    gtk_box_pack_start (GTK_BOX (vbox), framerange_table, FALSE, FALSE, 0);
  }

  gtk_widget_show (dlg);
  gtk_widget_realize(mgp->shell);

  mgp->startup = FALSE;

  gap_pview_set_size(mgp->pv_ptr
                  , mgp->pwidth
                  , mgp->pheight
                  , GAP_MOV_CHECK_SIZE
                  );

   /* init drawable for preview rendering
    * (at startup this is a copy of the frame from where we were invoked)
    */
   if(pvals->tmp_image_id >= 0)
   {
     mgp->drawable = p_get_flattened_drawable(pvals->tmp_image_id);
   }



  mov_path_prevw_preview_init(mgp);
  mov_show_path_or_cursor(mgp);

  g_signal_connect (G_OBJECT (mgp->shell), "size_allocate",
                    G_CALLBACK (mov_shell_window_size_allocate),
                    mgp);

  if(mgp->isStandaloneGui)
  {
    gtk_main ();
    gdk_flush ();
  }
  
  if(gap_debug) printf("GAP-DEBUG: END mov_dialog\n");

  return mov_int.run;
}  /* end mov_dialog */

static gboolean
mov_check_valid_src_layer(t_mov_gui_stuff   *mgp)
{
  if(mgp->startup)
  {
    return(FALSE);
  }

  if(pvals->src_layer_id < 0)
  {
     g_message(_("No source image was selected.\n"
                 "Please open a 2nd image of the same type before opening 'Move Path'"));
     return(FALSE);
  }
  return(TRUE);
}  /* end mov_check_valid_src_layer */

/* ============================================================================
 * implementation of CALLBACK procedures
 * ============================================================================
 */
static void
mov_help_callback (GtkWidget *widget,
                    t_mov_gui_stuff *mgp)
{
  if(gap_debug) printf("mov_help_callback:\n");

  gimp_standard_help_func(GAP_MOVE_PATH_HELP_ID, mgp->shell);
}  /* end mov_help_callback */

static void
mov_close_callback (GtkWidget *widget,
                    t_mov_gui_stuff *mgp)
{
  if(mgp)
  {
    if(mgp->shell)
    {
      p_points_to_tab(mgp);
      if(mgp->close_fptr != NULL)
      {

        if(gap_debug)
        {
          printf("calling close_fptr close notification\n");
        }
        /* run the callback procedure (that was provided by the caller)
         * to notify the caller about closing of the movepath dialog
         */
        (*mgp->close_fptr)(mgp->callback_data);
      }
    }

    mov_remove_timer(mgp);


    if(mgp->shell)
    {
      GtkWidget *l_shell;

      l_shell = mgp->shell;
      mgp->shell = NULL;

       /* mov_close_callback is the signal handler for the "destroy"
        * signal of the shell window.
        * the gtk_widget_destroy call will immediate reenter this procedure.
        * (for this reason the mgp->shell is set to NULL
        *  before the gtk_widget_destroy call)
        */
      gtk_widget_hide (l_shell);
      gtk_widget_destroy (l_shell);
      p_free_mgp_resources(mgp);
    }
    
    if(mgp->isStandaloneGui)
    {
      if(gap_debug)
      {
        printf("mov_close_callback isStandaloneGui == TRUE, calling gtk_main_quit\n");
      }
      gtk_main_quit ();
    }
  }
  else
  {
    if(gap_debug)
    {
      printf("mov_close_callback mgp is NULL, calling gtk_main_quit\n");
    }
    gtk_main_quit ();
  }
  
}  /* end mov_close_callback */


static void
mov_ok_callback (GtkWidget *widget,
                 t_mov_gui_stuff *mgp)
{

  if(pvals != NULL)
  {
    if(mgp != NULL)
    {
      if(mgp->isRecordOnlyMode == TRUE)
      {
        if(mgp->xml_paramfile[0] != '\0')
        {
          gint l_rc;
          if(gap_debug)
          {
            printf("Saving xml_paramfile:%s\n", mgp->xml_paramfile);
          }
          l_rc = gap_mov_xml_par_save(mgp->xml_paramfile, pvals);
          if(l_rc != 0)
          {
            printf("** Error failed to save xml_paramfile:%s\n", mgp->xml_paramfile);
          }
        }
      }
    }


    if(!mov_check_valid_src_layer(mgp))
    {
      return;
    }
  }

  if(!p_chk_keyframes(mgp))
  {
    return;
  }

  mov_int.run = TRUE;


  if(pvals->point_idx_max == 0)
  {
    /* if we have only one point duplicate that point
     * (move algorithm needs at least 2 points)
     */
    mov_padd_callback(NULL, mgp);
  }

  mov_close_callback(mgp->shell, mgp);

}  /* end mov_ok_callback */

/* ---------------------------
 * mov_upvw_callback
 * ---------------------------
 * this callback does completely draw the preview
 */
static void
mov_upvw_callback (GtkWidget *widget,
                  t_mov_gui_stuff *mgp)
{
  char               *l_filename;
  long                l_frame_nr;
  gint32              l_new_tmp_image_id;
  gint32              l_old_tmp_image_id;

  if(gap_debug)
  {
    printf("mov_upvw_callback nr: %d old_nr: %d\n"
         , (int)mgp->preview_frame_nr
         , (int)mgp->old_preview_frame_nr
         );
  }
  l_filename = NULL;
  
  if(mgp->ainfo_ptr->ainfo_type == GAP_AINFO_FRAMES)
  {
    if(gap_debug)
    {
      printf("mov_upvw_callback ainfo_type == GAP_AINFO_FRAMES nr: %d old_nr: %d\n"
           , (int)mgp->preview_frame_nr
           , (int)mgp->old_preview_frame_nr
           );
    }
    l_frame_nr = (long)mgp->preview_frame_nr;
    l_filename = gap_lib_alloc_fname(mgp->ainfo_ptr->basename,
                             l_frame_nr,
                             mgp->ainfo_ptr->extension);
  }
  else
  {
    if(gap_debug)
    {
      printf("mov_upvw_callback ainfo_type != GAP_AINFO_FRAMES using image_id %d\n"
           , (int)mgp->ainfo_ptr->image_id
           );
    }
  }
  
  
  if(!mgp->instant_apply)
  {
     /* dont show waiting cursor at instant_apply
      * (cursor flickering is boring on fast machines,
      *  and users with slow machines should not touch instant_apply at all)
      */
     mov_set_waiting_cursor(mgp);
  }

  if(l_filename != NULL)
  {
     /* replace the temporary image */
     if(mgp->preview_frame_nr  == mgp->ainfo_ptr->curr_frame_nr)
     {
       l_new_tmp_image_id = gimp_image_duplicate(mgp->ainfo_ptr->image_id);
     }
     else
     {
       if((pvals->tmp_alt_image_id >= 0)
       && (pvals->tmp_alt_framenr == l_frame_nr))
       {
         /* we can reuse the cached frame image */
         l_new_tmp_image_id = gimp_image_duplicate(pvals->tmp_alt_image_id);
       }
       else
       {
         if(pvals->tmp_alt_image_id >= 0)
         {
           gimp_image_delete(pvals->tmp_alt_image_id);
           pvals->tmp_alt_image_id = -1;
           pvals->tmp_alt_framenr = -1;
         }
         /* we must load a frame for preview update */
         l_new_tmp_image_id = gap_lib_load_image(l_filename);

         /* keep a copy of the frame to speed up further (instant) updates */
         pvals->tmp_alt_image_id = gimp_image_duplicate(l_new_tmp_image_id);
         pvals->tmp_alt_framenr = l_frame_nr;
       }
     }
     gimp_image_undo_disable(l_new_tmp_image_id);

     g_free(l_filename);
  }
  else
  {
    /* in case move path dialog was not called from a frame image
     * always show up the image from where the dialog was invoked from
     * as frame (and ignore the preview_frame_nr setting)
     */
    l_new_tmp_image_id = gimp_image_duplicate(mgp->ainfo_ptr->image_id);
  }
     
     
     
  if (l_new_tmp_image_id >= 0)
  {
     /* use the new loaded temporary image */
     l_old_tmp_image_id  = pvals->tmp_image_id;
     pvals->tmp_image_id = l_new_tmp_image_id;

     /* flatten image, and get the (only) resulting drawable */
     mgp->drawable = p_get_prevw_drawable(mgp);

     /* gimp_display_new(pvals->tmp_image_id); */ /* add a display for debugging only */

     /* re initialize preview image */
     mov_path_prevw_preview_init(mgp);
     p_point_refresh(mgp);

     mgp->old_preview_frame_nr = mgp->preview_frame_nr;

     gtk_widget_queue_draw(mgp->pv_ptr->da_widget);
     mov_path_prevw_draw ( mgp, CURSOR | PATH_LINE );
     gdk_flush();

     /* destroy the old tmp image */
     gimp_image_delete(l_old_tmp_image_id);

     mgp->instant_apply_request = FALSE;
  }

  mov_set_active_cursor(mgp);

}  /* end mov_upvw_callback */


static void
mov_apv_callback (GtkWidget *widget,
                      gpointer   data)
{
#define ARGC_APV 5
  t_mov_gui_stuff    *mgp;
  GapVinVideoInfo    *vin_ptr;
  static gint         apv_locked = FALSE;
  gint32              l_new_image_id;
  GimpParam          *return_vals;
  int                 nreturn_vals;
  gint                l_rc;

  static GapArrArg  argv[ARGC_APV];
  static char *radio_apv_mode[3] = { N_("Object on empty frames")
                                   , N_("Object on one frame")
                                   , N_("Exact object on frames")
                                   };
  static int gettextize_loop = 0;



  mgp = data;


  if(!p_chk_keyframes(mgp))
  {
    return;
  }

  if(apv_locked)
  {
    return;
  }

  apv_locked = TRUE;

  if(gap_debug) printf("mov_apv_callback preview_frame_nr: %d\n",
         (int)mgp->preview_frame_nr);


  for (;gettextize_loop < 3; gettextize_loop++)
  {
    radio_apv_mode[gettextize_loop] = gettext(radio_apv_mode[gettextize_loop]);
  }

  gap_arr_arg_init(&argv[0], GAP_ARR_WGT_RADIO);
  argv[0].label_txt = _("Anim Preview Mode:");
  argv[0].help_txt  = NULL;
  argv[0].radio_argc  = 3;
  argv[0].radio_argv = radio_apv_mode;
  argv[0].radio_ret  = 0;
  argv[0].has_default = TRUE;
  argv[0].radio_default = 0;
  switch(pvals->apv_mode)
  {
    case GAP_APV_EXACT:
       argv[0].radio_ret  = 2;
      break;
    case GAP_APV_ONE_FRAME:
       argv[0].radio_ret  = 1;
       break;
    default:
       argv[0].radio_ret  = 0;
       break;
  }

  gap_arr_arg_init(&argv[1], GAP_ARR_WGT_FLT_PAIR);
  argv[1].constraint = TRUE;
  argv[1].label_txt = _("Scale Preview:");
  argv[1].help_txt  = _("Scale down size of the generated animated preview (in %)");
  argv[1].flt_min   = 5.0;
  argv[1].flt_max   = 100.0;
  argv[1].flt_step  = 1.0;
  argv[1].flt_ret   = pvals->apv_scalex;
  argv[1].has_default = TRUE;
  argv[1].flt_default = 40;

  gap_arr_arg_init(&argv[2], GAP_ARR_WGT_FLT_PAIR);
  argv[2].constraint = TRUE;
  argv[2].label_txt = _("Framerate:");
  argv[2].help_txt  = _("Framerate to use in the animated preview in frames/sec");
  argv[2].flt_min   = 1.0;
  argv[2].flt_max   = 100.0;
  argv[2].flt_step  = 1.0;
  argv[2].flt_ret   = 24;

  vin_ptr = gap_vin_get_all(mgp->ainfo_ptr->basename);
  if(vin_ptr)
  {
     if(vin_ptr->framerate > 0) argv[2].flt_ret = vin_ptr->framerate;
     g_free(vin_ptr);
   }
  argv[2].has_default = TRUE;
  argv[2].flt_default = argv[2].flt_ret;

  gap_arr_arg_init(&argv[3], GAP_ARR_WGT_TOGGLE);
  argv[3].label_txt = _("Copy to Video Buffer:");
  argv[3].help_txt  = _("Save all single frames of animated preview to video buffer."
                        "(configured in gimprc by video-paste-dir and video-paste-basename)");
  argv[3].int_ret   = 0;
  argv[3].has_default = TRUE;
  argv[3].int_default = 0;

  gap_arr_arg_init(&argv[4], GAP_ARR_WGT_DEFAULT_BUTTON);
  argv[4].label_txt = _("Default");
  argv[4].help_txt  = _("Reset all parameters to default values");

  l_rc = gap_arr_ok_cancel_dialog( _("Move Path Animated Preview"),
                                 _("Options"),
                                  ARGC_APV, argv);

  /* quit if MovePath Mainwindow was closed */
  if(mgp->shell == NULL) { gtk_main_quit(); return; }

  if(l_rc)
  {
      switch(argv[0].radio_ret)
      {
        case 2:
           pvals->apv_mode = GAP_APV_EXACT;
           break;
        case 1:
           pvals->apv_mode = GAP_APV_ONE_FRAME;
           break;
        default:
           pvals->apv_mode = GAP_APV_QUICK;
           break;
      }

      pvals->apv_scalex = argv[1].flt_ret;
      pvals->apv_scaley = argv[1].flt_ret;
      pvals->apv_framerate = argv[2].flt_ret;

      if(argv[3].int_ret)
      {
         pvals->apv_gap_paste_buff = gap_lib_get_video_paste_name();
         gap_vid_edit_clear();
      }
      else
      {
         pvals->apv_gap_paste_buff = NULL;
      }

      if(gap_debug) printf("Generating Animated Preview\n");

        /* TODO: here we should start a thread for calculate and playback of the anim preview,
         * so the move_path main window is not blocked until playback exits
         */
      p_points_to_tab(mgp);
      if(!p_chk_keyframes(mgp))
      {
        return;
      }

      mov_set_waiting_cursor(mgp);

      l_new_image_id = gap_mov_exec_anim_preview(pvals, mgp->ainfo_ptr, mgp->preview_frame_nr);
      if(l_new_image_id < 0)
      {
         gap_arr_msg_win(GIMP_RUN_INTERACTIVE,
         _("Generation of animated preview failed"));
      }
      else
      {
        gint32           dummy_layer_id;
        dummy_layer_id = gap_image_get_any_layer(l_new_image_id);

        mov_set_waiting_cursor(mgp);  /* for refresh */
        return_vals = gimp_run_procedure ("plug_in_animationplay",
                                 &nreturn_vals,
                                 GIMP_PDB_INT32,    GIMP_RUN_NONINTERACTIVE,
                                 GIMP_PDB_IMAGE,    l_new_image_id,
                                 GIMP_PDB_DRAWABLE, dummy_layer_id,
                                 GIMP_PDB_END);
        gimp_destroy_params(return_vals, nreturn_vals);
      }
      pvals->apv_mlayer_image = -1;
      mov_set_active_cursor(mgp);
  }

  apv_locked = FALSE;
}  /* end mov_apv_callback */

static void
p_copy_point(gint to_idx, gint from_idx)
{
    pvals->point[to_idx].p_x = pvals->point[from_idx].p_x;
    pvals->point[to_idx].p_y = pvals->point[from_idx].p_y;
    pvals->point[to_idx].opacity = pvals->point[from_idx].opacity;
    pvals->point[to_idx].w_resize = pvals->point[from_idx].w_resize;
    pvals->point[to_idx].h_resize = pvals->point[from_idx].h_resize;
    pvals->point[to_idx].rotation = pvals->point[from_idx].rotation;
    pvals->point[to_idx].ttlx = pvals->point[from_idx].ttlx;
    pvals->point[to_idx].ttly = pvals->point[from_idx].ttly;
    pvals->point[to_idx].ttrx = pvals->point[from_idx].ttrx;
    pvals->point[to_idx].ttry = pvals->point[from_idx].ttry;
    pvals->point[to_idx].tblx = pvals->point[from_idx].tblx;
    pvals->point[to_idx].tbly = pvals->point[from_idx].tbly;
    pvals->point[to_idx].tbrx = pvals->point[from_idx].tbrx;
    pvals->point[to_idx].tbry = pvals->point[from_idx].tbry;
    pvals->point[to_idx].sel_feather_radius = pvals->point[from_idx].sel_feather_radius;

    pvals->point[to_idx].accPosition = pvals->point[from_idx].accPosition;
    pvals->point[to_idx].accOpacity = pvals->point[from_idx].accOpacity;
    pvals->point[to_idx].accSize = pvals->point[from_idx].accSize;
    pvals->point[to_idx].accRotation = pvals->point[from_idx].accRotation;
    pvals->point[to_idx].accPerspective = pvals->point[from_idx].accPerspective;
    pvals->point[to_idx].accSelFeatherRadius = pvals->point[from_idx].accSelFeatherRadius;

    /* do not copy keyframe */
    pvals->point[to_idx].keyframe_abs = 0;
    pvals->point[to_idx].keyframe = 0;
}



/* --------------------------
 * mov_grab_bezier_path
 * --------------------------
 * grab the bezier path divided in
 * straight lines and assign them to N-controlpoints.
 * this procedure uses the number of frames to be handled
 * as the wanted number of contolpoints.
 * (but constrain to the maximum allowed number of contolpoints)
 */
static void
mov_grab_bezier_path(t_mov_gui_stuff *mgp, gint32 vectors_id, gint32 stroke_id, const char *vectorname)
{
  gint32 image_id;
  gint             num_lines;
  gint             num_points;
  gdouble          max_distance;
  gdouble          distance;
  gdouble          step_length;
  gdouble          precision;
  gint             l_ii;

  image_id = mgp->ainfo_ptr->image_id;
  step_length = 1.0;

  num_points = 1 + abs(pvals->dst_range_end - pvals->dst_range_start);
  num_points = MIN((GAP_MOV_MAX_POINT-2), num_points);
  num_lines = num_points -1;

  distance   = 0.0;
  precision = 1.0;  /* shall give 1 pixel precision */

  if(num_lines > 0)
  {
    max_distance = gimp_vectors_stroke_get_length(vectors_id, stroke_id, precision);
    step_length = max_distance / ((gdouble)num_lines);
  }


  for(l_ii=0; l_ii < num_points ; l_ii++)
  {
    gdouble  xdouble;
    gdouble  ydouble;
    gdouble  slope;
    gboolean valid;
    gboolean success;


    success = gimp_vectors_stroke_get_point_at_dist(vectors_id
                                         , stroke_id
                                         , distance
                                         , precision
                                         , &xdouble
                                         , &ydouble
                                         , &slope
                                         , &valid
                                         );


    if(gap_debug)
    {
      printf("PATH distance: %.3f, (%.4f / %.4f) X:%03d Y: %03d slope:%.3f valid:%d success:%d\n"
            , (float)distance
            , (float)xdouble
            , (float)ydouble
            , (int)pvals->point[l_ii].p_x
            , (int)pvals->point[l_ii].p_y
            , (float)slope
            , (int)valid
            , (int)success
            );
    }

    if((!valid) || (!success))
    {
       /* stop because we already reached the end of the path.
        * (distance in pixles is greater than number of the frames to handle)
        */
       return;
    }
    pvals->point_idx_max = l_ii;
    p_clear_one_point(l_ii);
    pvals->point[l_ii].p_x = rint(xdouble);
    pvals->point[l_ii].p_y = rint(ydouble);

    distance += step_length;
  }

}  /* end mov_grab_bezier_path */


/* --------------------------
 * mov_grab_anchorpoints_path
 * --------------------------
 * grab the bezier path divided in
 * straight lines and assign them to N-controlpoints.
 * this procedure uses the number of frames to be handled
 * as the wanted number of contolpoints.
 * (but constrain to the maximum allowed number of contolpoints)
 */
static void
mov_grab_anchorpoints_path(t_mov_gui_stuff *mgp,
                           gint num_path_point_details,
                           gdouble *points_details
                           )
{
    gint l_ii;
    gint l_ti;
    gint l_idx;
    gint    point_x;
    gint    point_y;
#define GAP_BEZIER_CTRL1_X_INDEX  0
#define GAP_BEZIER_CTRL1_Y_INDEX  1
#define GAP_BEZIER_ANCHOR_X_INDEX 2
#define GAP_BEZIER_ANCHOR_Y_INDEX 3
#define GAP_BEZIER_CTRL2_X_INDEX  4
#define GAP_BEZIER_CTRL12Y_INDEX  5

    point_x = 0;
    point_y = 0;
    l_ti = 0;
    l_idx = 0;
    l_ii = 0;

    while(l_idx < GAP_MOV_MAX_POINT -2)
    {
      if(gap_debug)
      {
       printf("Point[%03d]: detail: %3.3f\n"
          , (int)l_ii
          , (float)points_details[l_ii]
          );
      }

     /* this implemenatation just fetches the bezier ancor points
      * and ignores bezier control points
      * Each Bezier segment endpoint (anchor, A) has two
      * additional control points (C) associated. They are specified in the
      * order CACCACCAC...
      * where each point consists of 2 flaot values in order x y.
      *
      */
      switch (l_ti)
      {
        case GAP_BEZIER_ANCHOR_X_INDEX:  point_x = (gint)points_details[l_ii]; break;
        case GAP_BEZIER_ANCHOR_Y_INDEX:  point_y = (gint)points_details[l_ii]; break;
        default:  break;
      }

      l_ti++;
      l_ii++;
      if((l_ti >= 6) || (l_ii == num_path_point_details))
      {
        if(gap_debug)
        {
          printf("\n");
        }

        l_ti=0;

        if(gap_debug)
        {
          printf("ANCHOR x:%d y:%d\n\n"
                               ,(int)point_x
                               ,(int)point_y
                               );
        }

        pvals->point_idx_max = l_idx;
        p_clear_one_point(l_idx);
        pvals->point[l_idx].p_x = point_x;
        pvals->point[l_idx].p_y = point_y;
        l_idx++;
      }
      if (l_ii >= num_path_point_details)
      {
        break;
      }
    }

    if(gap_debug)
    {
      printf("\n");
    }


}  /* end mov_grab_anchorpoints_path */



static void
mov_pgrab_callback (GtkWidget *widget,
                    GdkEventButton *bevent,
                    gpointer data)
{
  t_mov_gui_stuff *mgp = data;
  gint32 image_id;
  gint32 vectors_id;

  if(gap_debug) printf("mov_pgrab_callback\n");

  /* get the image where MovePath was invoked from */
  image_id = mgp->ainfo_ptr->image_id;

  vectors_id = gimp_image_get_active_vectors(image_id);
  if(vectors_id >= 0)
  {
    GimpVectorsStrokeType pathtype;
    gboolean path_closed;
    gint num_path_point_details;
    gdouble *points_details;
    gchar *vectorname;
    gint  num_stroke_ids;
    gint  *stroke_ids;

    vectorname = gimp_vectors_get_name(vectors_id);


    points_details = NULL;
    num_path_point_details = 0;

    if(gap_debug)
    {
      printf("vectorname :%s\n", vectorname);
    }

    stroke_ids = gimp_vectors_get_strokes(vectors_id, &num_stroke_ids);

    if(gap_debug)
    {
      printf("num_stroke_ids:%d\n"
            , (int)num_stroke_ids
            );
    }

    if (num_stroke_ids < 1)
    {
      g_message(_("No stroke ids found in path:\n"
                  "'%s'\n"
                  "in the Image:\n"
                  "'%s'")
              ,vectorname
              ,mgp->ainfo_ptr->old_filename
              );
      return;
    }

    /* TODO how to handle path that has more than one stroke_id.
     * the current implementation uses only the 1.st stroke_id
     */


    pathtype = gimp_vectors_stroke_get_points(vectors_id
                                             , stroke_ids[0]
                                             , &num_path_point_details
                                             , &points_details
                                             , &path_closed
                                             );




    if(gap_debug)
    {
      printf("pathtype:%d path_closed flag :%d num_points:%d num_stroke_ids:%d\n"
            , (int)pathtype
            , (int)path_closed
            , (int)num_path_point_details
            , (int)num_stroke_ids
            );
    }

    if(pathtype != GIMP_VECTORS_STROKE_TYPE_BEZIER)
    {
      g_message(_("Unsupported pathtype %d found in path:\n"
                  "'%s'\n"
                  "in the Image:\n"
                  "'%s'")
              ,(int)pathtype
              ,vectorname
              ,mgp->ainfo_ptr->old_filename
              );
      return;
    }

    if(num_path_point_details < 1)
    {
      g_message(_("No controlpoints found in path:\n"
                  "'%s'\n"
                  "in the Image:\n"
                  "'%s'")
              ,vectorname
              ,mgp->ainfo_ptr->old_filename
              );
      return;
    }


    if(bevent->state & GDK_SHIFT_MASK)
    {
      /* When SHIFT Key was pressed
       * the path will be divided in n-parts to get
       * one controlpoint per handled frame.
       */
      mov_grab_bezier_path(mgp, vectors_id, stroke_ids[0], vectorname);
    }
    else
    {
      mov_grab_anchorpoints_path(mgp
                                ,num_path_point_details
                                ,points_details
                                );
    }


    g_free(stroke_ids);
    g_free(points_details);

    pvals->point_idx = 0;
    p_point_refresh(mgp);
    mov_set_instant_apply_request(mgp);
  }
  else
  {
    g_message(_("No path found in the image:\n"
                "'%s'")
              ,mgp->ainfo_ptr->old_filename
              );
  }

}  /* end mov_pgrab_callback */


/* --------------------------------
 * mov_upd_seg_labels
 * --------------------------------
 * update information about max speed and path segment length
 *
 */
static void
mov_upd_seg_labels(GtkWidget *widget, t_mov_gui_stuff *mgp)
{
  if(gap_debug)
  {
    printf("mov_upd_seg_labels START mgp:%d widget:%d\n", (int)mgp, (int)widget);
  }

  if(mgp != NULL)
  {
    if((mgp->segLengthLabel != NULL)
    && (mgp->segSpeedLabel != NULL)
    && (mgp->segNumberLabel != NULL))
    {
       GapMovQuery mov_query;
       char *numString;

       mov_query.pointIndexToQuery = pvals->point_idx;

      if(gap_debug)
      {
        printf("mov_upd_seg_labels pointIndexToQuery:%d dst_range_end:%d\n"
              , (int)mov_query.pointIndexToQuery
              , (int)pvals->dst_range_end
              );
      }

       /* query path segment length and max speed per frame values */
       gap_mov_exec_query(pvals, mgp->ainfo_ptr, &mov_query);

       numString = g_strdup_printf("%d"
                                  , (int)mov_query.segmentNumber
                                  );
       gtk_label_set_text( GTK_LABEL(mgp->segNumberLabel), numString);
       g_free(numString);

       numString = g_strdup_printf("%.1f"
                                  , (float)mov_query.pathSegmentLengthInPixels
                                  );
       gtk_label_set_text( GTK_LABEL(mgp->segLengthLabel), numString);
       g_free(numString);


       numString = g_strdup_printf("%.1f / %.1f"
                                  , (float)mov_query.minSpeedInPixelsPerFrame
                                  , (float)mov_query.maxSpeedInPixelsPerFrame
                                  );
       gtk_label_set_text( GTK_LABEL(mgp->segSpeedLabel), numString);
       g_free(numString);

    }
  }
}


static void
mov_padd_callback (GtkWidget *widget,
                      gpointer   data)
{
  t_mov_gui_stuff *mgp = data;
  gint l_idx;

  if(gap_debug) printf("mov_padd_callback\n");
  l_idx = pvals->point_idx_max;
  if (l_idx < GAP_MOV_MAX_POINT -2)
  {
    /* advance to next point */
    p_points_to_tab(mgp);
    pvals->point_idx_max++;
    pvals->point_idx = pvals->point_idx_max;

    /* copy values from previous point to current (new) point */
    p_copy_point(pvals->point_idx_max, l_idx);
    p_point_refresh(mgp);
  }
}

static void
mov_pins_callback (GtkWidget *widget,
                      gpointer   data)
{
  t_mov_gui_stuff *mgp = data;
  gint l_idx;

  if(gap_debug) printf("mov_pins_callback\n");
  l_idx = pvals->point_idx_max;
  if (l_idx < GAP_MOV_MAX_POINT -2)
  {
    /* advance to next point */
    p_points_to_tab(mgp);
    pvals->point_idx_max++;

    for(l_idx = pvals->point_idx_max; l_idx >  pvals->point_idx; l_idx--)
    {
      /* copy values from prev point */
      p_copy_point(l_idx, l_idx-1);
    }

    pvals->point_idx++;
    p_point_refresh(mgp);
  }
}

static void
mov_pdel_callback (GtkWidget *widget,
                      gpointer   data)
{
  t_mov_gui_stuff *mgp = data;
  gint l_idx;

  if(gap_debug) printf("mov_pdel_callback\n");

  l_idx = pvals->point_idx_max;
  if(pvals->point_idx_max == 0)
  {
    /* This is the last point to delete */
    p_reset_points();
  }
  else
  {
    for(l_idx = pvals->point_idx; l_idx <  pvals->point_idx_max; l_idx++)
    {
      /* copy values from next point */
      p_copy_point(l_idx, l_idx+1);
    }
    pvals->point_idx_max--;
    pvals->point_idx = MIN(pvals->point_idx, pvals->point_idx_max);
  }
  p_point_refresh(mgp);
  mov_set_instant_apply_request(mgp);
}

static void
mov_follow_keyframe(t_mov_gui_stuff *mgp)
{
  gint32 keyframe_abs;

  keyframe_abs = mgp->keyframe_abs;
  if(pvals->point_idx <= 0)
  {
    keyframe_abs = mgp->first_nr;
  }
  else
  {
    if(pvals->point_idx  >= pvals->point_idx_max)
    {
      keyframe_abs = mgp->last_nr;
    }
    else
    {
      if(keyframe_abs < 1)
      {
        return;
      }
    }
  }

  if((keyframe_abs >= mgp->first_nr)
  && (keyframe_abs <= mgp->last_nr))
  {
    mgp->preview_frame_nr = keyframe_abs;
    gtk_adjustment_set_value (mgp->preview_frame_nr_adj,
                            (gdouble)keyframe_abs);
  }
}

static void
mov_pnext_callback (GtkWidget *widget,
                         GdkEventButton *bevent,
                         gpointer data)
{
  t_mov_gui_stuff *mgp = data;

  if(gap_debug) printf("mov_pnext_callback\n");
  if (pvals->point_idx < pvals->point_idx_max)
  {
    /* advance to next point */
    p_points_to_tab(mgp);
    pvals->point_idx++;
    p_point_refresh(mgp);
    if (bevent->state & GDK_SHIFT_MASK)
    {
      mov_follow_keyframe(mgp);
    }
    mov_set_instant_apply_request(mgp);
  }
}


static void
mov_pprev_callback (GtkWidget *widget,
                         GdkEventButton *bevent,
                         gpointer data)
{
  t_mov_gui_stuff *mgp = data;

  if(gap_debug) printf("mov_pprev_callback\n");
  if (pvals->point_idx > 0)
  {
    /* advance to next point */
    p_points_to_tab(mgp);
    pvals->point_idx--;
    p_point_refresh(mgp);
    if (bevent->state & GDK_SHIFT_MASK)
    {
      mov_follow_keyframe(mgp);
    }
    mov_set_instant_apply_request(mgp);
  }
}

static void
mov_pfirst_callback (GtkWidget *widget,
                         GdkEventButton *bevent,
                         gpointer data)
{
  t_mov_gui_stuff *mgp = data;

  if(gap_debug) printf("mov_pfirst_callback\n");

  /* advance to first point */
  p_points_to_tab(mgp);
  pvals->point_idx = 0;
  p_point_refresh(mgp);
  if (bevent->state & GDK_SHIFT_MASK)
  {
    mov_follow_keyframe(mgp);
  }
  mov_set_instant_apply_request(mgp);
}


static void
mov_plast_callback (GtkWidget *widget,
                         GdkEventButton *bevent,
                         gpointer data)
{
  t_mov_gui_stuff *mgp = data;

  if(gap_debug) printf("mov_plast_callback\n");

  /* advance to first point */
  p_points_to_tab(mgp);
  pvals->point_idx = pvals->point_idx_max;
  p_point_refresh(mgp);
  if (bevent->state & GDK_SHIFT_MASK)
  {
    mov_follow_keyframe(mgp);
  }
  mov_set_instant_apply_request(mgp);
}


static void
mov_pclr_callback (GtkWidget *widget,
                      gpointer   data)
{
  t_mov_gui_stuff *mgp = data;

  if(gap_debug) printf("mov_pclr_callback\n");
  p_clear_one_point(pvals->point_idx);         /* clear the current point */
  p_point_refresh(mgp);
  mov_set_instant_apply_request(mgp);
}

static void
mov_pdel_all_callback (GtkWidget *widget,
                      gpointer   data)
{
  t_mov_gui_stuff *mgp = data;

  if(gap_debug) printf("mov_pdel_all_callback\n");
  p_reset_points();
  p_point_refresh(mgp);
  mov_set_instant_apply_request(mgp);
}

static void
mov_pclr_all_callback (GtkWidget *widget,
                      GdkEventButton *bevent,
                      gpointer   data)
{
  gint l_idx;
  gint l_ref_idx1;
  gint l_ref_idx2;
  t_mov_gui_stuff *mgp = data;
  gdouble          mix_factor;

  if(gap_debug) printf("mov_pclr_all_callback\n");

  if(bevent->state & GDK_SHIFT_MASK)
  {
    for(l_idx = 1; l_idx <= pvals->point_idx_max; l_idx++)
    {
      mix_factor = 0.0;
      l_ref_idx1 = 0;
      l_ref_idx2 = 0;

      p_mix_one_point(l_idx, l_ref_idx1, l_ref_idx2, mix_factor);
    }
  }
  else
  {
    if(bevent->state & GDK_CONTROL_MASK)
    {
      for(l_idx = 1; l_idx <= pvals->point_idx_max -1; l_idx++)
      {
        mix_factor = (gdouble)l_idx / (gdouble)pvals->point_idx_max;
        l_ref_idx1 = 0;
        l_ref_idx2 = pvals->point_idx_max;

        p_mix_one_point(l_idx, l_ref_idx1, l_ref_idx2, mix_factor);
      }
    }
    else
    {
      for(l_idx = 0; l_idx <= pvals->point_idx_max; l_idx++)
      {
        p_clear_one_point(l_idx);
      }
    }
  }

  p_point_refresh(mgp);
  mov_set_instant_apply_request(mgp);
}


static void
mov_prot_follow_callback (GtkWidget *widget,
                         GdkEventButton *bevent,
                         gpointer data)
{
  gdouble  l_startangle;
  int     key_modifier;

  t_mov_gui_stuff *mgp = data;
  key_modifier = 0;

  if(gap_debug) printf("mov_prot_follow_callback\n");

  /* SHIFT: GDK_SHIFT_MASK,
   * CTRL:  GDK_CONTROL_MASK,
   * ALT:   GDK_MOD1_MASK
   */
  key_modifier = 0;
  if (bevent->state & GDK_SHIFT_MASK)
  {
    key_modifier = 1;       /* SHIFT */
  }

  if(gap_debug)
  {
    printf("key_modifier %d\n", (int)key_modifier);
  }

  if( pvals->point_idx_max > 1)
  {
    l_startangle = 0.0;
    if(key_modifier != 0)
    {
      /* SHIFT */
      p_points_to_tab(mgp);
      l_startangle = pvals->point[0].rotation;
    }
    gap_mov_exec_calculate_rotate_follow(pvals, l_startangle);
  }

  p_point_refresh(mgp);
  mov_set_instant_apply_request(mgp);
}

static void
p_filesel_close_cb(GtkWidget *widget,
                   t_mov_gui_stuff *mgp)
{
  if(mgp->filesel == NULL) return;

  gtk_widget_destroy(GTK_WIDGET(mgp->filesel));
  mgp->filesel = NULL;   /* now filesel is closed */
}

static void
mov_pload_callback (GtkWidget *widget,
                      gpointer   data)
{
  GtkWidget *filesel;
  t_mov_gui_stuff *mgp = data;

  if(mgp->filesel != NULL)
  {
    gtk_window_present(GTK_WINDOW(mgp->filesel));
    return;  /* filesel is already open */
  }

  filesel = gtk_file_selection_new ( _("Load Path Points from File"));
  mgp->filesel = filesel;

  gtk_window_set_position (GTK_WINDOW (filesel), GTK_WIN_POS_MOUSE);

  g_signal_connect (G_OBJECT (GTK_FILE_SELECTION (filesel)->ok_button),
                   "clicked",
                    G_CALLBACK (p_points_load_from_file),
                    mgp);

  g_signal_connect(G_OBJECT (GTK_FILE_SELECTION (filesel)->cancel_button),
                  "clicked",
                   G_CALLBACK (p_filesel_close_cb),
                   mgp);

  gtk_file_selection_set_filename (GTK_FILE_SELECTION (filesel),
                                   mgp->pointfile_name);

  gtk_widget_show (filesel);
  /* "destroy" has to be the last signal,
   * (otherwise the other callbacks are never called)
   */
  g_signal_connect (G_OBJECT (filesel), "destroy",
                    G_CALLBACK (p_filesel_close_cb),
                    mgp);
}


static void
mov_psave_callback (GtkWidget *widget,
                      gpointer   data)
{
  GtkWidget *filesel;
  t_mov_gui_stuff *mgp = data;

  if(mgp->filesel != NULL)
  {
    gtk_window_present(GTK_WINDOW(mgp->filesel));
    return;  /* filesel is already open */
  }

  filesel = gtk_file_selection_new ( _("Save Path Points to File"));
  mgp->filesel = filesel;

  gtk_window_set_position (GTK_WINDOW (filesel), GTK_WIN_POS_MOUSE);

  g_signal_connect (G_OBJECT (GTK_FILE_SELECTION (filesel)->ok_button),
                   "clicked",
                   G_CALLBACK (p_points_save_to_file),
                   mgp);

  g_signal_connect (G_OBJECT (GTK_FILE_SELECTION (filesel)->cancel_button),
                   "clicked",
                    G_CALLBACK (p_filesel_close_cb),
                    mgp);

  gtk_file_selection_set_filename (GTK_FILE_SELECTION (filesel),
                                   mgp->pointfile_name);

  gtk_widget_show (filesel);
  /* "destroy" has to be the last signal,
   * (otherwise the other callbacks are never called)
   */
  g_signal_connect (G_OBJECT (filesel), "destroy",
                    G_CALLBACK (p_filesel_close_cb),
                    mgp);
}

/* --------------------------------
 * p_refresh_widgets_after_load
 * --------------------------------
 * refresh widgets according to the new values
 * after loading settings from an xml parameter file.
 * This includes all widgets representing controlpoint data
 * and other render relevant parameter data loaded from the xml file.
 * except:
 * - the src_layer (the moving object)
 *   (the layer id of the object saved to the parameter file
 *    may no longer exist at load time)
 *
 * Note that not render relvant widgets are not
 * included in the xml paramter file and are not refreshed here.
 */
static void
p_refresh_widgets_after_load(t_mov_gui_stuff *mgp)
{
  if(mgp == NULL)
  {
    return;
  }

  p_point_refresh(mgp);

  if(mgp->paintmode_combo != NULL)
  {
    gimp_int_combo_box_set_active (GIMP_INT_COMBO_BOX (mgp->paintmode_combo), pvals->src_paintmode);
  }
  if (mgp->stepmode_combo != NULL)
  {
    gimp_int_combo_box_set_active (GIMP_INT_COMBO_BOX (mgp->stepmode_combo), pvals->src_stepmode);
  }
  if (mgp->handlemode_combo != NULL)
  {
    gimp_int_combo_box_set_active (GIMP_INT_COMBO_BOX (mgp->handlemode_combo), pvals->src_handle);
  }
  if (mgp->src_selmode_combo != NULL)
  {
    gimp_int_combo_box_set_active (GIMP_INT_COMBO_BOX (mgp->src_selmode_combo), pvals->src_selmode);
  }

  if(mgp->step_speed_factor_adj != NULL)
  {
    gtk_adjustment_set_value(mgp->step_speed_factor_adj,  pvals->step_speed_factor);
  }


  if(mgp->dst_range_start_adj != NULL)
  {
    gtk_adjustment_set_value(mgp->dst_range_start_adj,  pvals->dst_range_start);
  }
  if(mgp->dst_range_end_adj != NULL)
  {
    gtk_adjustment_set_value(mgp->dst_range_end_adj,  pvals->dst_range_end);
  }
  if(mgp->dst_layerstack_adj != NULL)
  {
    gtk_adjustment_set_value(mgp->dst_layerstack_adj,  pvals->dst_layerstack);
  }


  if(mgp->tween_steps_adj != NULL)
  {
    gtk_adjustment_set_value(mgp->tween_steps_adj,  pvals->tween_steps);
  }
  if(mgp->tween_opacity_initial_adj != NULL)
  {
    gtk_adjustment_set_value(mgp->tween_opacity_initial_adj,  pvals->tween_opacity_initial);
  }
  if(mgp->tween_opacity_desc_adj != NULL)
  {
    gtk_adjustment_set_value(mgp->tween_opacity_desc_adj,  pvals->tween_opacity_desc);
  }

  if(mgp->trace_opacity_initial_adj != NULL)
  {
    gtk_adjustment_set_value(mgp->trace_opacity_initial_adj,  pvals->trace_opacity_initial);
  }
  if(mgp->trace_opacity_desc_adj != NULL)
  {
    gtk_adjustment_set_value(mgp->trace_opacity_desc_adj,  pvals->trace_opacity_desc);
  }
  
  if(mgp->tracelayer_enable_check_button != NULL)
  {
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (mgp->tracelayer_enable_check_button),
                                  pvals->tracelayer_enable);
  }

  if (mgp->src_force_visible_check_button != NULL)
  {
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (mgp->src_force_visible_check_button),
                                  pvals->src_force_visible);
  }
  
  if (mgp->clip_to_img_check_button != NULL)
  {
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (mgp->clip_to_img_check_button),
                                  pvals->clip_to_img);
  }
  
  if (mgp->src_apply_bluebox_check_button != NULL)
  {
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (mgp->src_apply_bluebox_check_button),
                                  pvals->src_apply_bluebox);
  }
  
  if (mgp->bluebox_keycolor_color_button != NULL)
  {
    if(pvals->bbp != NULL)
    {
      gimp_color_button_set_color(mgp->bluebox_keycolor_color_button, &pvals->bbp->vals.keycolor);
    }
  }

}  /* end p_refresh_widgets_after_load */


/* ---------------------------------
 * p_points_load_from_file
 * ---------------------------------
 *
 */
static void
p_points_load_from_file (GtkWidget *widget,
                      t_mov_gui_stuff *mgp)
{
  const gchar        *filename;

  if(gap_debug)
  {
    printf("p_points_load_from_file\n");
  }
  if(mgp->filesel == NULL)
  {
    return;
  }

  filename = gtk_file_selection_get_filename (GTK_FILE_SELECTION (mgp->filesel));
  g_free(mgp->pointfile_name);
  mgp->pointfile_name = g_strdup(filename);

  if(gap_debug)
  {
    printf("p_points_load_from_file %s\n", mgp->pointfile_name);
  }

  gtk_widget_destroy(GTK_WIDGET(mgp->filesel));
  mgp->filesel = NULL;

  p_load_points(mgp->pointfile_name);
  p_refresh_widgets_after_load(mgp);
  mov_set_instant_apply_request(mgp);
}  /* end p_points_load_from_file */

/* ---------------------------------
 * p_points_save_to_file
 * ---------------------------------
 *
 */
static void
p_points_save_to_file (GtkWidget *widget,
                      t_mov_gui_stuff *mgp)
{
  const gchar        *filename;

  if(gap_debug)
  {
    printf("p_points_save_to_file\n");
  }
  if(mgp->filesel == NULL)
  {
    return;  /* filesel is already open */
  }

  filename = gtk_file_selection_get_filename (GTK_FILE_SELECTION (mgp->filesel));
  g_free(mgp->pointfile_name);
  mgp->pointfile_name = g_strdup(filename);

  if(gap_debug)
  {
    printf("p_points_save_to_file %s\n", mgp->pointfile_name);
  }

  gtk_widget_destroy(GTK_WIDGET(mgp->filesel));
  mgp->filesel = NULL;

  p_points_to_tab(mgp);
  p_save_points(mgp->pointfile_name, mgp);

  /* quit if MovePath Mainwindow was closed */
  if(mgp->shell == NULL) { gtk_main_quit(); return; }

  p_point_refresh(mgp);

}  /* end p_points_save_to_file */


static void
p_point_refresh(t_mov_gui_stuff *mgp)
{
  p_points_from_tab(mgp);
  p_update_point_index_text(mgp);

  if(gap_debug) printf("p_point_refresh:newval in_call=%d\n", mgp->in_call );
  if( !mgp->in_call )
  {
      mov_path_prevw_cursor_update( mgp );
      mov_path_prevw_draw ( mgp, CURSOR | PATH_LINE );
  }
  mgp->in_call = TRUE;

  gtk_adjustment_set_value (mgp->x_adj,
                            (gdouble)mgp->p_x);
  gtk_adjustment_set_value (mgp->y_adj,
                            (gdouble)mgp->p_y);
  gtk_adjustment_set_value (mgp->wres_adj,
                            (gdouble)mgp->w_resize);
  gtk_adjustment_set_value (mgp->hres_adj,
                            (gdouble)mgp->h_resize);
  gtk_adjustment_set_value (mgp->opacity_adj,
                            (gdouble)mgp->opacity);
  gtk_adjustment_set_value (mgp->rotation_adj,
                            (gdouble)mgp->rotation);
  gtk_adjustment_set_value (mgp->keyframe_adj,
                            (gdouble)mgp->keyframe_abs);
  gtk_adjustment_set_value (mgp->ttlx_adj,
                            (gdouble)mgp->ttlx);
  gtk_adjustment_set_value (mgp->ttly_adj,
                            (gdouble)mgp->ttly);
  gtk_adjustment_set_value (mgp->ttrx_adj,
                            (gdouble)mgp->ttrx);
  gtk_adjustment_set_value (mgp->ttry_adj,
                            (gdouble)mgp->ttry);
  gtk_adjustment_set_value (mgp->tblx_adj,
                            (gdouble)mgp->tblx);
  gtk_adjustment_set_value (mgp->tbly_adj,
                            (gdouble)mgp->tbly);
  gtk_adjustment_set_value (mgp->tbrx_adj,
                            (gdouble)mgp->tbrx);
  gtk_adjustment_set_value (mgp->tbry_adj,
                            (gdouble)mgp->tbry);
  gtk_adjustment_set_value (mgp->sel_feather_radius_adj,
                            (gdouble)mgp->sel_feather_radius);


  gtk_adjustment_set_value (mgp->accPosition_adj,
                            (gdouble)mgp->accPosition);
  gtk_adjustment_set_value (mgp->accOpacity_adj,
                            (gdouble)mgp->accOpacity);
  gtk_adjustment_set_value (mgp->accSize_adj,
                            (gdouble)mgp->accSize);
  gtk_adjustment_set_value (mgp->accRotation_adj,
                            (gdouble)mgp->accRotation);
  gtk_adjustment_set_value (mgp->accPerspective_adj,
                            (gdouble)mgp->accPerspective);
  gtk_adjustment_set_value (mgp->accSelFeatherRadius_adj,
                            (gdouble)mgp->accSelFeatherRadius);

  mov_upd_seg_labels(NULL, mgp);

  mgp->in_call = FALSE;
}       /* end p_point_refresh */

static void
p_pick_nearest_point(gint px, gint py)
{
  gint l_idx;
  gint l_idx_min;
  gdouble l_sq_dist;
  gdouble l_dx, l_dy;
  gdouble l_sq_dist_min;

  l_idx_min = 0;
  l_sq_dist_min = G_MAXDOUBLE;

  if(gap_debug) printf("\np_pick_nearest_point: near to %4d %4d\n", (int)px, (int)py);

  for(l_idx = pvals->point_idx_max; l_idx >= 0; l_idx--)
  {
    /* calculate x and y distance */
    l_dx = pvals->point[l_idx].p_x - px;
    l_dy = pvals->point[l_idx].p_y - py;

    /* calculate square of the distance */
    l_sq_dist = (l_dx * l_dx) + (l_dy * l_dy);
    if(l_sq_dist < l_sq_dist_min)
    {
      l_sq_dist_min = l_sq_dist;
      l_idx_min = l_idx;
    }

    if(gap_debug)
    {
       printf("  [%2d] %4d %4d %f\n",
             (int)l_idx,
             (int)pvals->point[l_idx].p_x,
             (int)pvals->point[l_idx].p_y,
             (float)l_sq_dist
             );
    }
  }
  if(gap_debug) printf("p_pick_nearest_point: selected %d\n", (int)l_idx_min);

  pvals->point_idx = l_idx_min;
  pvals->point[pvals->point_idx].p_x = px;
  pvals->point[pvals->point_idx].p_y = py;
}       /* end p_pick_nearest_point */


static void
mov_imglayer_menu_callback(GtkWidget *widget, t_mov_gui_stuff *mgp)
{
  gint32 l_image_id;
  gint32 id;
  gint value;

  gimp_int_combo_box_get_active (GIMP_INT_COMBO_BOX (widget), &value);
  id = value;

  l_image_id = gimp_drawable_get_image(id);
  if(!gap_image_is_alive(l_image_id))
  {
     if(gap_debug)
     {
       printf("mov_imglayer_menu_callback: NOT ALIVE image_id=%d layer_id=%d\n",
         (int)l_image_id, (int)id);
     }
     return;
  }

  if(id != pvals->src_layer_id)
  {
    if(pvals->tmpsel_image_id >= 0)
    {
      gimp_image_delete(pvals->tmpsel_image_id);
      pvals->tmpsel_image_id = -1;
    }
  }
  pvals->src_layer_id = id;
  pvals->src_image_id = l_image_id;


  if(gap_debug) printf("mov_imglayer_menu_callback: image_id=%d layer_id=%d\n",
         (int)pvals->src_image_id, (int)pvals->src_layer_id);

  mov_set_instant_apply_request(mgp);

} /* end mov_imglayer_menu_callback */

static gint
mov_imglayer_constrain(gint32 image_id, gint32 drawable_id, gpointer data)
{
  if(gap_debug) printf("GAP-DEBUG: mov_imglayer_constrain PROCEDURE image_id:%d drawable_id:%d\n"
                          ,(int)image_id
                          ,(int)drawable_id
                          );

  if(drawable_id < 0)
  {
     /* gimp 1.1 makes a first call of the constraint procedure
      * with drawable_id = -1, and skips the whole image if FALSE is returned
      */
     return(TRUE);
  }

  if(!gap_image_is_alive(image_id))
  {
     return(FALSE);
  }


   /* dont accept layers from within the destination image id
    * or layers within the internal used tmporary images
    * or layers within images of other base types
    * (conversions between different base_types are not supported in this version)
    */
  return((image_id != pvals->dst_image_id) &&
          (image_id != pvals->cache_tmp_image_id) &&
          (image_id != pvals->apv_mlayer_image) &&
          (image_id != pvals->tmp_image_id) &&
          (image_id != pvals->tmp_alt_image_id) &&
          (image_id != pvals->tmpsel_image_id) &&
          (image_id != pvals->tween_image_id) &&
          (image_id != pvals->trace_image_id) &&
          (gimp_image_base_type(image_id) == gimp_image_base_type(pvals->tmp_image_id)) );
} /* end mov_imglayer_constrain */

static void
mov_paintmode_menu_callback (GtkWidget *widget,  t_mov_gui_stuff *mgp)
{
  gint value;

  gimp_int_combo_box_get_active (GIMP_INT_COMBO_BOX (widget), &value);

  pvals->src_paintmode = value;
  mov_set_instant_apply_request(mgp);
}

static void
mov_handmode_menu_callback (GtkWidget *widget,  t_mov_gui_stuff *mgp)
{
  gint value;

  gimp_int_combo_box_get_active (GIMP_INT_COMBO_BOX (widget), &value);
  pvals->src_handle = value;
  if(mgp == NULL) return;

  mov_set_instant_apply_request(mgp);
}

static void
mov_stepmode_menu_callback (GtkWidget *widget, t_mov_gui_stuff *mgp)
{
  gboolean l_sensitive;
  GtkWidget *spinbutton;
  gint       value;

  gimp_int_combo_box_get_active (GIMP_INT_COMBO_BOX (widget), &value);
  pvals->src_stepmode = value;
  if(mgp == NULL) return;

  l_sensitive = TRUE;
  if((pvals->src_stepmode == GAP_STEP_NONE)
  || (pvals->src_stepmode == GAP_STEP_FRAME_NONE))
  {
    l_sensitive = FALSE;
  }

  if(mgp->step_speed_factor_adj)
  {
    spinbutton = GTK_WIDGET(g_object_get_data (G_OBJECT (mgp->step_speed_factor_adj), "spinbutton"));
    if(spinbutton)
    {
      gtk_widget_set_sensitive(spinbutton, l_sensitive);
    }
  }

  mov_set_instant_apply_request(mgp);

}  /* end mov_stepmode_menu_callback */


static void
mov_tweenlayer_sensitivity(t_mov_gui_stuff *mgp)
{
  gboolean l_sensitive;
  GtkWidget *spinbutton;

  l_sensitive = TRUE;
  if(pvals->tween_steps < 1)
  {
    l_sensitive = FALSE;
  }

  if(mgp->tween_opacity_initial_adj)
  {
    spinbutton = GTK_WIDGET(g_object_get_data (G_OBJECT (mgp->tween_opacity_initial_adj), "spinbutton"));
    if(spinbutton)
    {
      gtk_widget_set_sensitive(spinbutton, l_sensitive);
    }
  }

  if(mgp->tween_opacity_desc_adj)
  {
   spinbutton = GTK_WIDGET(g_object_get_data (G_OBJECT (mgp->tween_opacity_desc_adj), "spinbutton"));
   if(spinbutton)
   {
     gtk_widget_set_sensitive(spinbutton, l_sensitive);
   }
  }
}  /* end mov_tweenlayer_sensitivity */

static void
mov_tracelayer_sensitivity(t_mov_gui_stuff *mgp)
{
  gboolean l_sensitive;
  GtkWidget *spinbutton;

  l_sensitive = FALSE;
  if(pvals->tracelayer_enable)
  {
    l_sensitive = TRUE;
  }

  if(mgp->trace_opacity_initial_adj)
  {
    spinbutton = GTK_WIDGET(g_object_get_data (G_OBJECT (mgp->trace_opacity_initial_adj), "spinbutton"));
    if(spinbutton)
    {
      gtk_widget_set_sensitive(spinbutton, l_sensitive);
    }
  }

  if(mgp->trace_opacity_desc_adj)
  {
   spinbutton = GTK_WIDGET(g_object_get_data (G_OBJECT (mgp->trace_opacity_desc_adj), "spinbutton"));
   if(spinbutton)
   {
     gtk_widget_set_sensitive(spinbutton, l_sensitive);
   }
  }
}  /* end mov_tracelayer_sensitivity */



static void
mov_gint_toggle_callback(GtkWidget *w, gpointer   client_data)
{
  gint *data;

  data = (gint*)client_data;

  if (GTK_TOGGLE_BUTTON (w)->active)
  {
    *data = 1;
  }
  else
  {
    *data = 0;
  }
}  /* end mov_gint_toggle_callback */

static void
mov_force_visibility_toggle_callback    (GtkWidget *widget, gpointer client_data)
{
  t_mov_gui_stuff *mgp;

  if((widget == NULL) || (client_data == NULL))
  {
    return;
  }
  mov_gint_toggle_callback(widget, client_data);
  mgp = g_object_get_data( G_OBJECT(widget), "mgp" );
  mov_set_instant_apply_request(mgp);

}  /* end mov_force_visibility_toggle_callback */

static void
mov_bluebox_callback    (GtkWidget *widget, gpointer client_data)
{
  t_mov_gui_stuff *mgp;

  if((widget == NULL) || (client_data == NULL))
  {
    return;
  }
  mov_gint_toggle_callback(widget, client_data);
  mgp = g_object_get_data( G_OBJECT(widget), "mgp" );

  if(mgp)
  {
    mov_set_instant_apply_request(mgp);
  }

}  /* end mov_bluebox_callback */

static void
mov_tracelayer_callback    (GtkWidget *widget, gpointer client_data)
{
  t_mov_gui_stuff *mgp;

  if((widget == NULL) || (client_data == NULL))
  {
    return;
  }
  mov_gint_toggle_callback(widget, client_data);
  mgp = g_object_get_data( G_OBJECT(widget), "mgp" );

  if(mgp)
  {
    mov_tracelayer_sensitivity(mgp);
  }

}  /* end mov_tracelayer_callback */


static void
mov_show_path_or_cursor(t_mov_gui_stuff *mgp)
{
  if(mgp == NULL) return;
  if(mgp->startup) return;
  if(mgp->pv_ptr == NULL) return;
  if(mgp->pv_ptr->da_widget == NULL) return;
  if(mgp->drawable == NULL) return;

  p_point_refresh(mgp);
  mov_path_prevw_draw ( mgp, CURSOR | PATH_LINE );
  gtk_widget_queue_draw(mgp->pv_ptr->da_widget);
  gdk_flush();
}  /* end mov_show_path_or_cursor */

static void
mov_show_path_callback(GtkWidget *widget, t_mov_gui_stuff *mgp)
{
  mov_gint_toggle_callback(widget, &mgp->show_path);
  mov_show_path_or_cursor(mgp);
}  /* end mov_show_path_callback */

static void
mov_show_cursor_callback(GtkWidget *widget, t_mov_gui_stuff *mgp)
{
  mov_gint_toggle_callback(widget, &mgp->show_cursor);
  mov_show_path_or_cursor(mgp);
}  /* end mov_show_cursor_callback */

static void
mov_show_grid_callback(GtkWidget *widget, t_mov_gui_stuff *mgp)
{
  mov_gint_toggle_callback(widget, &mgp->show_grid);
  mov_show_path_or_cursor(mgp);
  //XX grid rendering and picking not implemented yet !!!
}  /* end mov_show_grid_callback */


/* --------------------------
 * install / remove timer
 * --------------------------
 */
static void
mov_install_timer(t_mov_gui_stuff *mgp)
{
  if(mgp->instant_timertag < 0)
  {
    mgp->instant_timertag = (gint32) g_timeout_add(INSTANT_TIMERINTERVAL_MILLISEC,
                                      (GtkFunction)mov_instant_timer_callback
                                      , mgp);
  }
}  /* end mov_install_timer */

static void
mov_remove_timer(t_mov_gui_stuff *mgp)
{
  if(mgp == NULL)
  {
    return;
  }
  
  if(mgp->instant_timertag >= 0)
  {
    g_source_remove(mgp->instant_timertag);
    mgp->instant_timertag = -1;
  }
}  /* end mov_remove_timer */

/* --------------------------
 * mov_instant_timer_callback
 * --------------------------
 * This procedure is called periodically via timer
 * when the instant_apply checkbox is ON
 */
static void
mov_instant_timer_callback(gpointer   user_data)
{
  t_mov_gui_stuff *mgp;

  mgp = user_data;
  if(mgp == NULL)
  {
    return;
  }

  mov_remove_timer(mgp);

  if(!mov_check_valid_src_layer(mgp))
  {
    return;
  }

  if(mgp->instant_apply_request)
  {
    if(gap_debug) printf("FIRE mov_instant_timer_callback >>>> REQUEST is TRUE\n");
    mov_upvw_callback (NULL, mgp);  /* the request is cleared in this procedure */
  }

  /* restart timer for next cycle */
  if(mgp->instant_apply)
  {
     mov_install_timer(mgp);
  }
}  /* end mov_instant_timer_callback */

static void
mov_instant_apply_callback(GtkWidget *widget, t_mov_gui_stuff *mgp)
{
  mov_gint_toggle_callback(widget, &mgp->instant_apply);
  if(mgp->instant_apply)
  {
    mov_set_instant_apply_request(mgp);
    mov_install_timer(mgp);
  }
  else
  {
    mov_remove_timer(mgp);
  }
}  /* end mov_instant_apply_callback */



/* ------------------------------------------
 * p_set_sensitivity_by_adjustment
 * ------------------------------------------
 * get the optional attached spinbutton and scale
 * and set the specified sensitivity when attached widget is present.
 */
static void
p_set_sensitivity_by_adjustment(GtkAdjustment *adj, gboolean sensitive)
{
  if(adj != NULL)
  {
    GtkWidget *spinbutton;
    GtkWidget *scale;

    spinbutton = GTK_WIDGET(g_object_get_data (G_OBJECT (adj), "spinbutton"));
    if(spinbutton)
    {
      gtk_widget_set_sensitive(spinbutton, sensitive);
    }
    scale = GTK_WIDGET(g_object_get_data (G_OBJECT (adj), "scale"));
    if(scale)
    {
      gtk_widget_set_sensitive(scale, sensitive);
    }
  }
}  /* end p_set_sensitivity_by_adjustment */


/* ----------------------------------
 * p_accel_widget_sensitivity
 * ----------------------------------
 * set sensitivity for all acceleration characteristic widgets
 * Those widgets are sensitive for the first conrolpoint
 * and for keframes that are NOT the last controlpoint.
 */
static void
p_accel_widget_sensitivity(t_mov_gui_stuff *mgp)
{
  gboolean sensitive;

  sensitive = FALSE;
  if(pvals->point_idx == 0)
  {
    sensitive = TRUE;
  }
  else
  {
    if ((pvals->point_idx != pvals->point_idx_max)
    && ((pvals->point[pvals->point_idx].keyframe > 0) || (mgp->keyframe_abs > 0)))
    {
      sensitive = TRUE;
    }
  }

  p_set_sensitivity_by_adjustment(mgp->accPosition_adj, sensitive);
  p_set_sensitivity_by_adjustment(mgp->accOpacity_adj, sensitive);
  p_set_sensitivity_by_adjustment(mgp->accSize_adj, sensitive);
  p_set_sensitivity_by_adjustment(mgp->accRotation_adj, sensitive);
  p_set_sensitivity_by_adjustment(mgp->accPerspective_adj, sensitive);
  p_set_sensitivity_by_adjustment(mgp->accSelFeatherRadius_adj, sensitive);


}  /* end p_accel_widget_sensitivity */


/* ============================================================================
 * procedures to handle POINTS - TABLE
 * ============================================================================
 */


static void
p_points_from_tab(t_mov_gui_stuff *mgp)
{
  mgp->p_x      = pvals->point[pvals->point_idx].p_x;
  mgp->p_y      = pvals->point[pvals->point_idx].p_y;
  mgp->opacity  = pvals->point[pvals->point_idx].opacity;
  mgp->w_resize = pvals->point[pvals->point_idx].w_resize;
  mgp->h_resize = pvals->point[pvals->point_idx].h_resize;
  mgp->rotation = pvals->point[pvals->point_idx].rotation;
  mgp->ttlx     = pvals->point[pvals->point_idx].ttlx;
  mgp->ttly     = pvals->point[pvals->point_idx].ttly;
  mgp->ttrx     = pvals->point[pvals->point_idx].ttrx;
  mgp->ttry     = pvals->point[pvals->point_idx].ttry;
  mgp->tblx     = pvals->point[pvals->point_idx].tblx;
  mgp->tbly     = pvals->point[pvals->point_idx].tbly;
  mgp->tbrx     = pvals->point[pvals->point_idx].tbrx;
  mgp->tbry     = pvals->point[pvals->point_idx].tbry;
  mgp->sel_feather_radius  = pvals->point[pvals->point_idx].sel_feather_radius;
  mgp->keyframe_abs = pvals->point[pvals->point_idx].keyframe_abs;

  mgp->accPosition         = pvals->point[pvals->point_idx].accPosition;
  mgp->accOpacity          = pvals->point[pvals->point_idx].accOpacity;
  mgp->accSize             = pvals->point[pvals->point_idx].accSize;
  mgp->accRotation         = pvals->point[pvals->point_idx].accRotation;
  mgp->accPerspective      = pvals->point[pvals->point_idx].accPerspective;
  mgp->accSelFeatherRadius = pvals->point[pvals->point_idx].accSelFeatherRadius;


  if(( mgp->keyframe_adj != NULL) && (mgp->startup != TRUE))
  {
    gboolean sensitive;

    sensitive = TRUE;
    if((pvals->point_idx == 0) || (pvals->point_idx == pvals->point_idx_max))
    {
      sensitive = FALSE;
    }
    p_set_sensitivity_by_adjustment(mgp->keyframe_adj, sensitive);

    p_accel_widget_sensitivity(mgp);

  }
}

static void
p_points_to_tab(t_mov_gui_stuff *mgp)
{
  if(mgp == NULL)
  {
    return;
  }
  if(pvals == NULL)
  {
    return;
  }

  if(gap_debug)
  {
    printf("p_points_to_tab: idx=%d, rotation=%f\n"
       , (int)pvals->point_idx
       , (float)mgp->rotation
       );
  }

  pvals->point[pvals->point_idx].p_x       = mgp->p_x;
  pvals->point[pvals->point_idx].p_y       = mgp->p_y;
  pvals->point[pvals->point_idx].opacity   = mgp->opacity;
  pvals->point[pvals->point_idx].w_resize  = mgp->w_resize;
  pvals->point[pvals->point_idx].h_resize  = mgp->h_resize;
  pvals->point[pvals->point_idx].rotation  = mgp->rotation;
  pvals->point[pvals->point_idx].ttlx      = mgp->ttlx;
  pvals->point[pvals->point_idx].ttly      = mgp->ttly;
  pvals->point[pvals->point_idx].ttrx      = mgp->ttrx;
  pvals->point[pvals->point_idx].ttry      = mgp->ttry;
  pvals->point[pvals->point_idx].tblx      = mgp->tblx;
  pvals->point[pvals->point_idx].tbly      = mgp->tbly;
  pvals->point[pvals->point_idx].tbrx      = mgp->tbrx;
  pvals->point[pvals->point_idx].tbry      = mgp->tbry;
  pvals->point[pvals->point_idx].sel_feather_radius  = mgp->sel_feather_radius;
  pvals->point[pvals->point_idx].keyframe_abs  = mgp->keyframe_abs;

  pvals->point[pvals->point_idx].accPosition         = mgp->accPosition;
  pvals->point[pvals->point_idx].accOpacity          = mgp->accOpacity;
  pvals->point[pvals->point_idx].accSize             = mgp->accSize;
  pvals->point[pvals->point_idx].accRotation         = mgp->accRotation;
  pvals->point[pvals->point_idx].accPerspective      = mgp->accPerspective;
  pvals->point[pvals->point_idx].accSelFeatherRadius = mgp->accSelFeatherRadius;

  if((mgp->keyframe_abs > 0)
  && (pvals->point_idx != 0)
  && (pvals->point_idx != pvals->point_idx_max))
  {
    pvals->point[pvals->point_idx].keyframe = gap_mov_exec_conv_keyframe_to_rel(mgp->keyframe_abs, pvals);
  }
  else
  {
    pvals->point[pvals->point_idx].keyframe  = 0;
  }
}

void
p_update_point_index_text(t_mov_gui_stuff *mgp)
{
  g_snprintf (&mgp->point_index_text[0], POINT_INDEX_LABEL_LENGTH,
              _("Current Point: [ %3d ] of [ %3d ]"),
              pvals->point_idx + 1, pvals->point_idx_max +1);

  if (mgp->point_index_frame)
    {
      gtk_frame_set_label (GTK_FRAME (mgp->point_index_frame),
                          &mgp->point_index_text[0]);
    }
}


/* ============================================================================
 * p_clear_one_point
 *   clear one controlpoint to default values.
 * ============================================================================
 */
void
p_clear_one_point(gint idx)
{
  if((idx >= 0) && (idx <= pvals->point_idx_max))
  {
    pvals->point[idx].opacity  = 100.0; /* 100 percent (no transparecy) */
    pvals->point[idx].w_resize = 100.0; /* 100%  no resizize (1:1) */
    pvals->point[idx].h_resize = 100.0; /* 100%  no resizize (1:1) */
    pvals->point[idx].rotation = 0.0;   /* no rotation (0 degree) */
    /* 1.0 for all perspective transform factors (== no perspective transformation) */
    pvals->point[idx].ttlx      = 1.0;
    pvals->point[idx].ttly      = 1.0;
    pvals->point[idx].ttrx      = 1.0;
    pvals->point[idx].ttry      = 1.0;
    pvals->point[idx].tblx      = 1.0;
    pvals->point[idx].tbly      = 1.0;
    pvals->point[idx].tbrx      = 1.0;
    pvals->point[idx].tbry      = 1.0;
    pvals->point[idx].sel_feather_radius = 0.0;
    pvals->point[idx].keyframe = 0;   /* 0: controlpoint is not fixed to keyframe */
    pvals->point[idx].keyframe_abs = 0;   /* 0: controlpoint is not fixed to keyframe */

    pvals->point[idx].accPosition = 0;           /* 0: linear (e.g NO acceleration) is default */
    pvals->point[idx].accOpacity = 0;            /* 0: linear (e.g NO acceleration) is default */
    pvals->point[idx].accSize = 0;               /* 0: linear (e.g NO acceleration) is default */
    pvals->point[idx].accRotation = 0;           /* 0: linear (e.g NO acceleration) is default */
    pvals->point[idx].accPerspective = 0;        /* 0: linear (e.g NO acceleration) is default */
    pvals->point[idx].accSelFeatherRadius = 0;   /* 0: linear (e.g NO acceleration) is default */

  }
}       /* end p_clear_one_point */


/* --------------------------
 * p_mix_one_point
 * --------------------------
 * calculate settings for one point by mixing
 * the settings of 2 reference points.
 * All settings EXCEPT the position are affected
 */
void
p_mix_one_point(gint idx, gint ref1, gint ref2, gdouble mix_factor)
{

  if((idx >= 0)
  && (idx <= pvals->point_idx_max)
  && (ref1 >= 0)
  && (ref1 <= pvals->point_idx_max)
  && (ref2 >= 0)
  && (ref2 <= pvals->point_idx_max)
  )
  {
    pvals->point[idx].opacity  = GAP_BASE_MIX_VALUE(mix_factor, pvals->point[ref1].opacity,   pvals->point[ref2].opacity);
    pvals->point[idx].w_resize = GAP_BASE_MIX_VALUE(mix_factor, pvals->point[ref1].w_resize,  pvals->point[ref2].w_resize);
    pvals->point[idx].h_resize = GAP_BASE_MIX_VALUE(mix_factor, pvals->point[ref1].h_resize,  pvals->point[ref2].h_resize);
    pvals->point[idx].rotation = GAP_BASE_MIX_VALUE(mix_factor, pvals->point[ref1].rotation,  pvals->point[ref2].rotation);

    pvals->point[idx].ttlx      = GAP_BASE_MIX_VALUE(mix_factor, pvals->point[ref1].ttlx,  pvals->point[ref2].ttlx);
    pvals->point[idx].ttly      = GAP_BASE_MIX_VALUE(mix_factor, pvals->point[ref1].ttly,  pvals->point[ref2].ttly);
    pvals->point[idx].ttrx      = GAP_BASE_MIX_VALUE(mix_factor, pvals->point[ref1].ttrx,  pvals->point[ref2].ttrx);
    pvals->point[idx].ttry      = GAP_BASE_MIX_VALUE(mix_factor, pvals->point[ref1].ttry,  pvals->point[ref2].ttry);
    pvals->point[idx].tblx      = GAP_BASE_MIX_VALUE(mix_factor, pvals->point[ref1].tblx,  pvals->point[ref2].tblx);
    pvals->point[idx].tbly      = GAP_BASE_MIX_VALUE(mix_factor, pvals->point[ref1].tbly,  pvals->point[ref2].tbly);
    pvals->point[idx].tbrx      = GAP_BASE_MIX_VALUE(mix_factor, pvals->point[ref1].tbrx,  pvals->point[ref2].tbrx);
    pvals->point[idx].tbry      = GAP_BASE_MIX_VALUE(mix_factor, pvals->point[ref1].tbry,  pvals->point[ref2].tbry);

    pvals->point[idx].sel_feather_radius = GAP_BASE_MIX_VALUE(mix_factor, pvals->point[ref1].sel_feather_radius,  pvals->point[ref2].sel_feather_radius);


    pvals->point[idx].accPosition         = GAP_BASE_MIX_VALUE(mix_factor, pvals->point[ref1].accPosition,          pvals->point[ref2].accPosition);
    pvals->point[idx].accOpacity          = GAP_BASE_MIX_VALUE(mix_factor, pvals->point[ref1].accOpacity,           pvals->point[ref2].accOpacity);
    pvals->point[idx].accSize             = GAP_BASE_MIX_VALUE(mix_factor, pvals->point[ref1].accSize,              pvals->point[ref2].accSize);
    pvals->point[idx].accRotation         = GAP_BASE_MIX_VALUE(mix_factor, pvals->point[ref1].accRotation,          pvals->point[ref2].accRotation);
    pvals->point[idx].accPerspective      = GAP_BASE_MIX_VALUE(mix_factor, pvals->point[ref1].accPerspective,       pvals->point[ref2].accPerspective);
    pvals->point[idx].accSelFeatherRadius = GAP_BASE_MIX_VALUE(mix_factor, pvals->point[ref1].accSelFeatherRadius,  pvals->point[ref2].accSelFeatherRadius);



    pvals->point[idx].keyframe = 0;   /* 0: controlpoint is not fixed to keyframe */
    pvals->point[idx].keyframe_abs = 0;   /* 0: controlpoint is not fixed to keyframe */
  }
}       /* end p_mix_one_point */


/* ============================================================================
 * p_reset_points
 *   Init point table with identical 2 Points
 * ============================================================================
 */
void p_reset_points()
{
  pvals->point_idx = 0;        /* 0 == current point */
  pvals->point_idx_max = 0;    /* 0 == there is only one valid point */
  p_clear_one_point(0);
  pvals->point[0].p_x = 0;
  pvals->point[0].p_y = 0;
}       /* end p_reset_points */

/* ---------------------------------
 * p_filename_ends_with_etension_xml
 * ---------------------------------
 */
static gboolean
p_filename_ends_with_etension_xml(const char *filename)
{
  int l_len;
  gboolean l_xml;

  l_xml = FALSE;
  l_len = strlen(filename);
  if(l_len >= 3)
  {
    const char *l_extension;
    l_extension = &filename[l_len -3];
    if (strcmp("xml", l_extension) == 0)
    {
      l_xml = TRUE;
    }
    if (strcmp("XML", l_extension) == 0)
    {
      l_xml = TRUE;
    }
  }

  return(l_xml);

}

/* -----------------------------
 * p_load_points
 * -----------------------------
 * supports pointfile format(s) of older gimp-gap releases that
 * only contains the path controlpoints table
 * and also loads from the newer move path parameterfile in xml format
 * that can contain all parameter settings.
 *
 * old pointfile:
 *   load point table (from named file into global pvals)
 *   (reset points if load failed)
 * new xml file:
 *   load all settings into global pvals including the point table.
 *   Note that the xml load affects all settings (except the src_layer_id that represents
 *   the moving object)
 *   in case some settings (such as perspective settings, bluebox settings ...)
 *   are not present in the xml file
 *   the missing settings are replaced by default values.
 * ============================================================================
 */
void
p_load_points(char *filename)
{
  gint l_rc;
  gint l_errno;

  if (p_filename_ends_with_etension_xml(filename))
  {
    gboolean l_xmlOk;

    l_xmlOk = gap_mov_xml_par_load(filename, pvals
                                  ,gimp_image_width(pvals->dst_image_id)
                                  ,gimp_image_height(pvals->dst_image_id)
                                  );
    if (!l_xmlOk)
    {

      if(l_errno != 0)
      {
        g_message(_("ERROR: Could not open xml parameterfile\n"
                "filename: '%s'\n%s")
               ,filename, g_strerror (l_errno));
      }
      else
      {
        g_message(_("ERROR: Could not read parameterfile\n"
                "filename: '%s'\n(Is not a valid move path xml parameterfile file)")
               ,filename);
      }


    }

    return;
  }

  l_rc = gap_mov_exec_gap_load_pointfile(filename, pvals);
  l_errno = errno;

  if (l_rc < -1)
  {
    p_reset_points();
  }
  if (l_rc != 0)
  {
    if(l_errno != 0)
    {
      g_message(_("ERROR: Could not open controlpoints\n"
                "filename: '%s'\n%s")
               ,filename, g_strerror (l_errno));
    }
    else
    {
      g_message(_("ERROR: Could not read controlpoints\n"
                "filename: '%s'\n(Is not a valid controlpoint file)")
               ,filename);
    }
  }
}  /* end p_load_points */



/* ----------------------------
 * p_save_points
 * ----------------------------
 * depending on the filename extension (.xml)
 *   save point table (from global pvals into named file)
 *
 */
static void
p_save_points(char *filename, t_mov_gui_stuff *mgp)
{
  gint l_rc;
  gint l_errno;
  gboolean l_wr_permission;

  l_wr_permission = gap_arr_overwrite_file_dialog(filename);

  /* quit if MovePath Mainwindow was closed */
  if(mgp->shell == NULL) { gtk_main_quit (); return; }

  if(l_wr_permission)
  {
    if (p_filename_ends_with_etension_xml(filename))
    {
      l_rc = gap_mov_xml_par_save(filename, pvals);
    }
    else
    {
      l_rc = gap_mov_exec_gap_save_pointfile(filename, pvals);
    }
    l_errno = errno;

    if(l_rc != 0)
    {
        g_message (_("Failed to write controlpointfile\n"
                      "filename: '%s':\n%s"),
                   filename, g_strerror (l_errno));
    }
  }
}       /* end p_save_points */

/* ============================================================================
 * mov_refresh_src_layer_menu
 * ============================================================================
 */
static void
mov_refresh_src_layer_menu(t_mov_gui_stuff *mgp)
{
  gimp_int_combo_box_connect (GIMP_INT_COMBO_BOX (mgp->src_layer_combo),
                              pvals->src_layer_id,                      /* initial value */
                              G_CALLBACK (mov_imglayer_menu_callback),
                              mgp);

}  /* end mov_refresh_src_layer_menu */


/* ============================================================================
 * Create new source selection table Frame, and return it.
 *   A frame that contains:
 *   - 2x2 menus (src_image/layer, handle, stepmode, paintmode)
 * ============================================================================
 */

static GtkWidget *
mov_src_sel_create(t_mov_gui_stuff *mgp)
{
  GtkWidget *table;
  GtkWidget *sub_table;
  GtkWidget *combo;
  GtkWidget *label;
  GtkObject      *adj;


  /* the table */
  table = gtk_table_new (2, 4, FALSE);
  gtk_container_set_border_width (GTK_CONTAINER (table), 2);
  gtk_table_set_row_spacings (GTK_TABLE (table), 2);
  gtk_table_set_col_spacings (GTK_TABLE (table), 4);

  /* Source Layer menu */
  label = gtk_label_new( _("Source Image/Layer:"));
  gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
  gtk_table_attach(GTK_TABLE(table), label, 0, 1, 0, 1, GTK_FILL, 0, 4, 0);
  gtk_widget_show(label);

  combo = gimp_layer_combo_box_new (mov_imglayer_constrain, NULL);
  gtk_table_attach(GTK_TABLE(table), combo, 1, 2, 0, 1,
                   GTK_EXPAND | GTK_FILL, 0, 0, 0);

  gimp_help_set_help_data(combo,
                       _("Source object to insert into destination frames of the specified range")
                       , NULL);

  gtk_widget_show(combo);
  mgp->src_layer_combo = combo;
  
  if(mgp->isRecordOnlyMode)
  {
    gtk_widget_hide(label);
    gtk_widget_hide(combo);
  }
  else
  {
    mov_refresh_src_layer_menu(mgp);
    gtk_widget_show(combo);
  }


  /* Paintmode combo (menu) */

  label = gtk_label_new( _("Mode:"));
  gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
  gtk_table_attach(GTK_TABLE(table), label, 2, 3, 0, 1, GTK_FILL, 0, 4, 0);
  gtk_widget_show(label);

  combo = gimp_int_combo_box_new (_("Normal"),         GIMP_NORMAL_MODE,
                                   _("Dissolve"),       GIMP_DISSOLVE_MODE,
                                   _("Behind"),         GIMP_BEHIND_MODE,
                                   _("Multiply"),       GIMP_MULTIPLY_MODE,
                                   _("Divide"),         GIMP_DIVIDE_MODE,
                                   _("Screen"),         GIMP_SCREEN_MODE,
                                   _("Overlay"),        GIMP_OVERLAY_MODE,
                                   _("Dodge"),          GIMP_DODGE_MODE,
                                   _("Burn"),           GIMP_BURN_MODE,
                                   _("Hard Light"),     GIMP_HARDLIGHT_MODE,
                                   _("Soft Light"),     GIMP_SOFTLIGHT_MODE,
                                   _("Grain Extract"),  GIMP_GRAIN_EXTRACT_MODE,
                                   _("Grain Merge"),    GIMP_GRAIN_MERGE_MODE,
                                   _("Difference"),     GIMP_DIFFERENCE_MODE,
                                   _("Addition"),       GIMP_ADDITION_MODE,
                                   _("Subtract"),       GIMP_SUBTRACT_MODE,
                                   _("Darken Only"),    GIMP_DARKEN_ONLY_MODE,
                                   _("Lighten Only"),   GIMP_LIGHTEN_ONLY_MODE,
                                   _("Hue"),            GIMP_HUE_MODE,
                                   _("Saturation"),     GIMP_SATURATION_MODE,
                                   _("Color"),          GIMP_COLOR_MODE,
                                   _("Color Erase"),    GIMP_COLOR_ERASE_MODE,
                                   _("Value"),          GIMP_VALUE_MODE,
                                   _("Keep Paintmode"), GAP_MOV_KEEP_SRC_PAINTMODE,
                                   NULL);

  {
    gint initialValue;
    initialValue = GIMP_NORMAL_MODE;
    
    if(pvals)
    {
      initialValue = pvals->src_paintmode;
    }
    
    gimp_int_combo_box_connect (GIMP_INT_COMBO_BOX (combo),
                              initialValue,
                              G_CALLBACK (mov_paintmode_menu_callback),
                              mgp);
    
  }

  gtk_table_attach(GTK_TABLE(table), combo, 3, 4, 0, 1,
                   GTK_EXPAND | GTK_FILL, 0, 0, 0);
  gimp_help_set_help_data(combo,
                       _("Paintmode")
                       , NULL);
  gtk_widget_show(combo);
  mgp->paintmode_combo = combo;



  /* Loop Stepmode menu (Label) */

  label = gtk_label_new( _("Stepmode:"));
  gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
  gtk_table_attach(GTK_TABLE(table), label, 0, 1, 1, 2, GTK_FILL, 0, 4, 0);
  gtk_widget_show(label);

  /* the sub_table (1 row) */
  sub_table = gtk_table_new (1, 3, FALSE);
  gtk_widget_show(sub_table);
  gtk_container_set_border_width (GTK_CONTAINER (sub_table), 2);
  gtk_table_set_row_spacings (GTK_TABLE (sub_table), 0);
  gtk_table_set_col_spacings (GTK_TABLE (sub_table), 2);

  gtk_table_attach(GTK_TABLE(table), sub_table, 1, 2, 1, 2,
                   GTK_EXPAND | GTK_FILL, 0, 0, 0);



  /* StepSpeedFactor */
  adj = p_mov_spinbutton_new( GTK_TABLE (sub_table), 1, 0,    /* table col, row */
                          _("SpeedFactor:"),                  /* label text */
                          SCALE_WIDTH, ENTRY_WIDTH,           /* scalesize spinsize */
                          (gdouble)pvals->step_speed_factor,  /* initial value */
                          (gdouble)0.0, (gdouble)50.0,        /* lower, upper */
                          0.1, 1,                             /* step, page */
                          3,                                  /* digits */
                          FALSE,                              /* constrain */
                          (gdouble)0.0, (gdouble)50.0,        /* lower, upper (unconstrained) */
                          _("Source and target frames step synchronized at value 1.0. "
                            "A value of 0.5 will step the source half time slower. "
                            "One source step is done only at every 2nd target frame."),
                          NULL);    /* tooltip privatetip */
  g_signal_connect (G_OBJECT (adj), "value_changed",
                    G_CALLBACK (gimp_double_adjustment_update),
                    &pvals->step_speed_factor);
  mgp->step_speed_factor_adj = GTK_ADJUSTMENT(adj);
  
  if(mgp->isRecordOnlyMode)
  {
    GtkWidget *widget;
    
    widget = g_object_get_data(G_OBJECT (adj), "label");
    gtk_widget_hide(widget);
    widget = g_object_get_data(G_OBJECT (adj), "spinbutton");
    gtk_widget_hide(widget);
    
  }


  /* Loop Stepmode combo  */
  combo = gimp_int_combo_box_new (_("Loop"),                 GAP_STEP_LOOP,
                                  _("Loop Reverse"),         GAP_STEP_LOOP_REV,
                                  _("Once"),                 GAP_STEP_ONCE,
                                  _("Once Reverse"),         GAP_STEP_ONCE_REV,
                                  _("Ping Pong"),            GAP_STEP_PING_PONG,
                                  _("None"),                 GAP_STEP_NONE,
                                  _("Frame Loop"),           GAP_STEP_FRAME_LOOP,
                                  _("Frame Loop Reverse"),   GAP_STEP_FRAME_LOOP_REV,
                                  _("Frame Once"),           GAP_STEP_FRAME_ONCE,
                                  _("Frame Once Reverse"),   GAP_STEP_FRAME_ONCE_REV,
                                  _("Frame Ping Pong"),      GAP_STEP_FRAME_PING_PONG,
                                  _("Frame None"),           GAP_STEP_FRAME_NONE,
                                  NULL);

  if(mgp->isRecordOnlyMode)
  {
    gimp_int_combo_box_connect (GIMP_INT_COMBO_BOX (combo),
                              GAP_STEP_NONE,              /* initial int value */
                              G_CALLBACK (mov_stepmode_menu_callback),
                              mgp);
  }
  else
  {
    gimp_int_combo_box_connect (GIMP_INT_COMBO_BOX (combo),
                              GAP_STEP_LOOP,              /* initial int value */
                              G_CALLBACK (mov_stepmode_menu_callback),
                              mgp);
  }
  
  gtk_table_attach(GTK_TABLE(sub_table), combo, 0, 1, 0, 1,
                   GTK_EXPAND | GTK_FILL, 0, 0, 0);
  gimp_help_set_help_data(combo,
                       _("How to fetch the next source layer at the next handled frame")
                       , NULL);
  if(mgp->isRecordOnlyMode)
  {
    gtk_widget_hide(label);
    gtk_widget_hide(combo);
  }
  else
  {
    gtk_widget_show(combo);
  }
  mgp->stepmode_combo = combo;


  /* Source Image Handle menu */

  label = gtk_label_new( _("Handle:"));
  gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
  gtk_table_attach(GTK_TABLE(table), label, 2, 3, 1, 2, GTK_FILL, 0, 4, 0);
  gtk_widget_show(label);

  combo = gimp_int_combo_box_new (_("Left  Top"),     GAP_HANDLE_LEFT_TOP,
                                  _("Left  Bottom"),  GAP_HANDLE_LEFT_BOT,
                                  _("Right Top"),     GAP_HANDLE_RIGHT_TOP,
                                  _("Right Bottom"),  GAP_HANDLE_RIGHT_BOT,
                                  _("Center"),        GAP_HANDLE_CENTER,
                                  NULL);

  
  {
    gint initialValue;

    initialValue = GAP_HANDLE_LEFT_TOP;
    if(pvals)
    {
      switch(pvals->src_handle)
      {
        case GAP_HANDLE_LEFT_TOP:
        case GAP_HANDLE_LEFT_BOT:
        case GAP_HANDLE_RIGHT_TOP:
        case GAP_HANDLE_RIGHT_BOT:
        case GAP_HANDLE_CENTER:
          initialValue = pvals->src_handle;
          break;
      }
    }
    
    gimp_int_combo_box_connect (GIMP_INT_COMBO_BOX (combo),
                              initialValue,
                              G_CALLBACK (mov_handmode_menu_callback),
                              mgp);
  }

  gtk_table_attach(GTK_TABLE(table), combo, 3, 4, 1, 2,
                   GTK_EXPAND | GTK_FILL, 0, 0, 0);
  gimp_help_set_help_data(combo,
                       _("How to place the Source layer at controlpoint coordinates")
                       , NULL);
  gtk_widget_show(combo);
  mgp->handlemode_combo = combo;

  gtk_widget_show( table );

  return table;
}       /* end mov_src_sel_create */


/* ============================================================================
 * Create set of widgets for the advanced Move Path features
 *   A frame that contains:
 *   in the 1.st row
 *   - 3x spinbutton  for tween_steps, tween_opacity_init, tween_opacity_desc
 *   in the 2.nd row
 *   - checkbutton  make_tracelayer
 *   - 2x spinbutton   for trace_opacity_initial, trace_opacity_desc
 * ============================================================================
 */

static GtkWidget *
mov_advanced_tab_create(t_mov_gui_stuff *mgp)
{
  GtkWidget      *table;
  GtkWidget      *check_button;
  GtkObject      *adj;
  guint          row;
  guint          col;


  /* the table (2 rows) */
  table = gtk_table_new (2, 8, FALSE);
  gtk_container_set_border_width (GTK_CONTAINER (table), 2);
  gtk_table_set_row_spacings (GTK_TABLE (table), 2);
  gtk_table_set_col_spacings (GTK_TABLE (table), 4);


  row = 0;
  col = 0;

  /* the bluebox widgets */
  {
    GtkWidget    *check_button;
    GtkWidget    *label;
    GtkWidget    *color_button;

    /* toggle bluebox */
    check_button = gtk_check_button_new_with_label ( _("Bluebox"));
    mgp->src_apply_bluebox_check_button = check_button;
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (check_button),
                                  pvals->src_apply_bluebox);
    gimp_help_set_help_data(check_button,
                         _("Apply the bluebox filter on the moving object(s). "
                           "The bluebox filter makes the keycolor transparent.")
                         , NULL);
    gtk_widget_show (check_button);
    gtk_table_attach(GTK_TABLE(table), check_button, col, col+1, row, row+1
                  ,0 , 0, 0, 0);
    g_object_set_data(G_OBJECT(check_button), "mgp", mgp);
    g_signal_connect (G_OBJECT (check_button), "toggled",
                      G_CALLBACK  (mov_bluebox_callback),
                      &pvals->src_apply_bluebox);

    /* keycolor label */
    label = gtk_label_new( _("Keycolor:"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label, col, col+1, row+1, row+2
                    , 0, 0, 0, 0);
    gtk_widget_show(label);

    if(pvals->bbp_pv == NULL)
    {
      pvals->bbp_pv = gap_bluebox_bbp_new(-1);
    }

    /* keycolor button */
    color_button = gimp_color_button_new (_("Move Path Bluebox Keycolor"),
                                  25, 12,                     /* WIDTH, HEIGHT, */
                                  &pvals->bbp_pv->vals.keycolor,
                                  GIMP_COLOR_AREA_FLAT);
    mgp->bluebox_keycolor_color_button = (GimpColorButton *)color_button;

    /* dont know if it is possible to remove the signal handler for the "clicked" signal
     * on the gimp_color_button.
     * WORKAROUND:
     *   destroy the unwanted standard dialog in my private handler
     *   mov_path_keycolorbutton_clicked
     */

    gtk_table_attach(GTK_TABLE(table), color_button, col+1, col+2, row+1, row+2
                    , 0, 0, 0, 0);
    gtk_widget_show (color_button);
    gimp_help_set_help_data(color_button,
                         _("Open dialog window to set "
                           "parameters and keycolor for the bluebox filter")
                         , NULL);

    g_signal_connect (color_button, "clicked",
                    G_CALLBACK (mov_path_keycolorbutton_clicked),
                    mgp);

    g_signal_connect (color_button, "color_changed",
                    G_CALLBACK (mov_path_keycolorbutton_changed),
                    mgp);

  }

  row = 0;
  col = 2;

  /* toggle Tracelayer */
  check_button = gtk_check_button_new_with_label ( _("Tracelayer"));
  mgp->tracelayer_enable_check_button = check_button;
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (check_button),
                                  pvals->tracelayer_enable);
  gimp_help_set_help_data(check_button,
                         _("Create an additional trace layer in all handled frames")
                         , NULL);
  gtk_widget_show (check_button);
  gtk_table_attach(GTK_TABLE(table), check_button, col, col+1, row, row+1
                  ,0 , 0, 0, 0);
  g_object_set_data(G_OBJECT(check_button), "mgp", mgp);
  g_signal_connect (G_OBJECT (check_button), "toggled",
                      G_CALLBACK  (mov_tracelayer_callback),
                      &pvals->tracelayer_enable);

  /* TraceOpacityInitial */
  adj = p_mov_spinbutton_new( GTK_TABLE (table), col+2, row,    /* table col, row */
                          _("TraceOpacity1:"),                  /* label text */
                          SCALE_WIDTH, ENTRY_WIDTH,             /* scalesize spinsize */
                          (gdouble)pvals->trace_opacity_initial,  /* initial value */
                          (gdouble)0.0, (gdouble)100,           /* lower, upper */
                          1, 10,                                /* step, page */
                          1,                                    /* digits */
                          FALSE,                                /* constrain */
                          (gdouble)0.0, (gdouble)100,           /* lower, upper (unconstrained) */
                          _("Initial opacity of the trace layer"),
                          NULL);    /* tooltip privatetip */
  g_signal_connect (G_OBJECT (adj), "value_changed",
                    G_CALLBACK (gimp_double_adjustment_update),
                    &pvals->trace_opacity_initial);
  mgp->trace_opacity_initial_adj = GTK_ADJUSTMENT(adj);


  /* TraceOpacityDescending */
  adj = p_mov_spinbutton_new( GTK_TABLE (table), col+4, row,    /* table col, row */
                          _("TraceOpacity2:"),                  /* label text */
                          SCALE_WIDTH, ENTRY_WIDTH,             /* scalesize spinsize */
                          (gdouble)pvals->trace_opacity_desc,     /* initial value */
                          (gdouble)0.0, (gdouble)100,           /* lower, upper */
                          1, 10,                                /* step, page */
                          1,                                    /* digits */
                          FALSE,                                /* constrain */
                          (gdouble)0.0, (gdouble)100,           /* lower, upper (unconstrained) */
                          _("Descending opacity of the trace layer"),
                          NULL);    /* tooltip privatetip */
  g_signal_connect (G_OBJECT (adj), "value_changed",
                    G_CALLBACK (gimp_double_adjustment_update),
                    &pvals->trace_opacity_desc);
  mgp->trace_opacity_desc_adj = GTK_ADJUSTMENT(adj);

  row = 1;
  col = 2;

  /* TweenSteps */
  adj = p_mov_spinbutton_new( GTK_TABLE (table), col, row,      /* table col, row */
                          _("Tweensteps:"),                     /* label text */
                          SCALE_WIDTH, ENTRY_WIDTH,             /* scalesize spinsize */
                          (gdouble)pvals->tween_steps,          /* initial value */
                          (gdouble)0, (gdouble)50,              /* lower, upper */
                          1, 2,                                 /* step, page */
                          0,                                    /* digits */
                          FALSE,                                /* constrain */
                          (gdouble)0, (gdouble)50,              /* lower, upper (unconstrained) */
                          _("Calculate n steps between 2 frames. "
                            "The rendered tween steps are collected in a tween layer "
                            "that will be added to the handled destination frames. "
                            "If the tween step value is 0, no tweens are calculated "
                            "and no tween layers are created"),
                          NULL);    /* tooltip privatetip */
  mgp->tween_steps_adj = GTK_ADJUSTMENT(adj);
  g_object_set_data(G_OBJECT(adj), "mgp", mgp);
  g_signal_connect (G_OBJECT (adj), "value_changed",
                    G_CALLBACK (mov_path_tween_steps_adjustment_update),
                    &pvals->tween_steps);


  /* TweenOpacityInitial */
  adj = p_mov_spinbutton_new( GTK_TABLE (table), col+2, row,    /* table col, row */
                          _("TweenOpacity1:"),                  /* label text */
                          SCALE_WIDTH, ENTRY_WIDTH,             /* scalesize spinsize */
                          (gdouble)pvals->tween_opacity_initial,  /* initial value */
                          (gdouble)0.0, (gdouble)100,           /* lower, upper */
                          1, 10,                                /* step, page */
                          1,                                    /* digits */
                          FALSE,                                /* constrain */
                          (gdouble)0.0, (gdouble)100,           /* lower, upper (unconstrained) */
                          _("Initial opacity of the tween layer"),
                          NULL);    /* tooltip privatetip */
  g_signal_connect (G_OBJECT (adj), "value_changed",
                    G_CALLBACK (gimp_double_adjustment_update),
                    &pvals->tween_opacity_initial);
  mgp->tween_opacity_initial_adj = GTK_ADJUSTMENT(adj);

  /* TweenOpacityDescending */
  adj = p_mov_spinbutton_new( GTK_TABLE (table), col+4, row,    /* table col, row */
                          _("TweenOpacity2:"),                  /* label text */
                          SCALE_WIDTH, ENTRY_WIDTH,             /* scalesize spinsize */
                          (gdouble)pvals->tween_opacity_desc,   /* initial value */
                          (gdouble)0.0, (gdouble)100,           /* lower, upper */
                          1, 10,                                /* step, page */
                          1,                                    /* digits */
                          FALSE,                                /* constrain */
                          (gdouble)0.0, (gdouble)100,           /* lower, upper (unconstrained) */
                          _("Descending opacity of the tween layer"),
                          NULL);    /* tooltip privatetip */
  g_signal_connect (G_OBJECT (adj), "value_changed",
                    G_CALLBACK (gimp_double_adjustment_update),
                    &pvals->tween_opacity_desc);
  mgp->tween_opacity_desc_adj = GTK_ADJUSTMENT(adj);

  /* set initial sensitivity */
  mov_tweenlayer_sensitivity(mgp);
  mov_tracelayer_sensitivity(mgp);

  gtk_widget_show( table );

  return table;
}       /* end mov_advanced_tab_create */

/* ============================================================================
 * Create new EditCtrlPoint Frame
 * ============================================================================
 */
static GtkWidget *
mov_edit_button_box_create (t_mov_gui_stuff *mgp)
{
  GtkWidget      *vbox;
  GtkWidget      *hbox;
  GtkWidget      *frame;
  GtkWidget      *button_table;
  GtkWidget      *button;
  gint           row;

  /* the vbox */
  vbox = gtk_vbox_new (FALSE, 3);
  gtk_widget_show (vbox);

  /* the frame */
  frame = gimp_frame_new (_("Edit Controlpoints"));
  gtk_container_set_border_width( GTK_CONTAINER( frame ), 2 );


  /* button_table 7 rows */
  button_table = gtk_table_new (7, 2, TRUE);
  gtk_table_set_row_spacings (GTK_TABLE (button_table), 2);
  gtk_table_set_col_spacings (GTK_TABLE (button_table), 2);
  gtk_widget_show (button_table);

  row = 0;

  /* Add Controlpoint */
  button = gtk_button_new_from_stock ( GAP_STOCK_ADD_POINT );
  GTK_WIDGET_SET_FLAGS (button, GTK_CAN_DEFAULT);
  gtk_table_attach( GTK_TABLE(button_table), button, 0, 1, row, row+1,
                    GTK_FILL, 0, 0, 0 );
  gimp_help_set_help_data(button,
                       _("Add controlpoint at end. The last controlpoint is duplicated.")
                       , NULL);
  gtk_widget_show (button);
  g_signal_connect (G_OBJECT (button), "clicked",
                    G_CALLBACK  (mov_padd_callback),
                    mgp);

  /* Grab Path (Controlpoints from current GIMP Path)  */
  button = gtk_button_new_from_stock ( GAP_STOCK_GRAB_POINTS );
  GTK_WIDGET_SET_FLAGS (button, GTK_CAN_DEFAULT);
  gtk_table_attach( GTK_TABLE(button_table), button, 1, 2, row, row+1,
                    GTK_FILL, 0, 0, 0 );
  gimp_help_set_help_data(button,
                       _("Delete all controlpoints, and replace them with "
                         "a copy of all anchorpoints of the current path "
                         "from the image from which 'MovePath' was invoked. "
                         "Hold down the Shift key to create controlpoints for each handled frame, "
                         "following the Bezier path.")
                       , NULL);
  gtk_widget_show (button);
  g_signal_connect (G_OBJECT (button), "button_press_event",
                    G_CALLBACK  (mov_pgrab_callback),
                    mgp);

  row++;

  /* Insert Controlpoint */
  button = gtk_button_new_from_stock ( GAP_STOCK_INSERT_POINT );
  GTK_WIDGET_SET_FLAGS (button, GTK_CAN_DEFAULT);
  gtk_table_attach( GTK_TABLE(button_table), button, 0, 1, row, row+1,
                    GTK_FILL, 0, 0, 0 );
  gimp_help_set_help_data(button,
                       _("Insert controlpoint. The current controlpoint is duplicated.")
                       , NULL);
  gtk_widget_show (button);
  g_signal_connect (G_OBJECT (button), "clicked",
                    G_CALLBACK (mov_pins_callback),
                    mgp);

  /* Delete Controlpoint */
  button = gtk_button_new_from_stock ( GAP_STOCK_DELETE_POINT );
  GTK_WIDGET_SET_FLAGS (button, GTK_CAN_DEFAULT);
  gtk_table_attach( GTK_TABLE(button_table), button, 1, 2, row, row+1,
                    GTK_FILL, 0, 0, 0 );
  gimp_help_set_help_data(button,
                       _("Delete current controlpoint")
                       , NULL);
  gtk_widget_show (button);
  g_signal_connect (G_OBJECT (button), "clicked",
                    G_CALLBACK (mov_pdel_callback),
                    mgp);

  row++;

  /* Previous Controlpoint */
  button = gtk_button_new_from_stock ( GAP_STOCK_PREV_POINT );
  GTK_WIDGET_SET_FLAGS (button, GTK_CAN_DEFAULT);
  gtk_table_attach( GTK_TABLE(button_table), button, 0, 1, row, row+1,
                    GTK_FILL, 0, 0, 0 );
  gimp_help_set_help_data(button,
                       _("Show previous controlpoint. Hold down the shift key to follow keyframes.")
                       , NULL);
  gtk_widget_show (button);
  g_signal_connect (G_OBJECT (button), "button_press_event",
                    G_CALLBACK (mov_pprev_callback),
                    mgp);

  /* Next Controlpoint */
  button = gtk_button_new_from_stock ( GAP_STOCK_NEXT_POINT );
  GTK_WIDGET_SET_FLAGS (button, GTK_CAN_DEFAULT);
  gtk_table_attach( GTK_TABLE(button_table), button, 1, 2, row, row+1,
                    GTK_FILL, 0, 0, 0 );
  gimp_help_set_help_data(button,
                       _("Show next controlpoint. Hold down the shift key to follow keyframes.")
                       , NULL);
  gtk_widget_show (button);
  g_signal_connect (G_OBJECT (button), "button_press_event",
                    G_CALLBACK (mov_pnext_callback),
                    mgp);

  row++;

  /* First Controlpoint */
  button = gtk_button_new_from_stock ( GAP_STOCK_FIRST_POINT );
  GTK_WIDGET_SET_FLAGS (button, GTK_CAN_DEFAULT);
  gtk_table_attach( GTK_TABLE(button_table), button, 0, 1, row, row+1,
                    GTK_FILL, 0, 0, 0 );
  gimp_help_set_help_data(button,
                       _("Show first controlpoint. Hold down the shift key to follow keyframes.")
                       , NULL);
  gtk_widget_show (button);
  g_signal_connect (G_OBJECT (button), "button_press_event",
                    G_CALLBACK (mov_pfirst_callback),
                    mgp);

  /* Last Controlpoint */
  button = gtk_button_new_from_stock ( GAP_STOCK_LAST_POINT );
  GTK_WIDGET_SET_FLAGS (button, GTK_CAN_DEFAULT);
  gtk_table_attach( GTK_TABLE(button_table), button, 1, 2, row, row+1,
                    GTK_FILL, 0, 0, 0 );
  gimp_help_set_help_data(button,
                       _("Show last controlpoint. Hold down the shift key to follow keyframes.")
                       , NULL);
  gtk_widget_show (button);
  g_signal_connect (G_OBJECT (button), "button_press_event",
                    G_CALLBACK (mov_plast_callback),
                    mgp);

  row++;

  /* Reset Controlpoint */
  button = gtk_button_new_from_stock ( GAP_STOCK_RESET_POINT );
  GTK_WIDGET_SET_FLAGS (button, GTK_CAN_DEFAULT);
  gtk_table_attach( GTK_TABLE(button_table), button, 0, 1, row, row+1,
                    GTK_FILL, 0, 0, 0 );
  gimp_help_set_help_data(button,
                       _("Reset the current controlpoint to default values")
                       , NULL);
  gtk_widget_show (button);
  g_signal_connect (G_OBJECT (button), "clicked",
                    G_CALLBACK (mov_pclr_callback),
                    mgp);

  /* Reset All Controlpoints */
  button = gtk_button_new_from_stock ( GAP_STOCK_RESET_ALL_POINTS );
  GTK_WIDGET_SET_FLAGS (button, GTK_CAN_DEFAULT);
  gtk_table_attach( GTK_TABLE(button_table), button, 1, 2, row, row+1,
                    GTK_FILL, 0, 0, 0 );
  gimp_help_set_help_data(button,
                       _("Reset all controlpoints to default values "
                         "but dont change the path (X/Y values). "
                         "Hold down the shift key to copy settings "
                         "of point1 into all other points. "
                         "Holding down the ctrl key spreads a mix of "
                         "the settings of point1 and the last point "
                         "into the other points inbetween.")
                       , NULL);
  gtk_widget_show (button);
  g_signal_connect (G_OBJECT (button), "button_press_event",
                    G_CALLBACK (mov_pclr_all_callback),
                    mgp);

  row++;

  /* Rotate Follow */
  button = gtk_button_new_from_stock ( GAP_STOCK_ROTATE_FOLLOW );
  GTK_WIDGET_SET_FLAGS (button, GTK_CAN_DEFAULT);
  gtk_table_attach( GTK_TABLE(button_table), button, 0, 1, row, row+1,
                    GTK_FILL, 0, 0, 0 );
  gimp_help_set_help_data(button,
                       _("Set rotation for all controlpoints "
                         "to follow the shape of the path. "
                         "Hold down the shift key to use rotation of contolpoint 1 as offset.")
                       , NULL);
  gtk_widget_show (button);
  g_signal_connect (G_OBJECT (button), "button_press_event",
                    G_CALLBACK (mov_prot_follow_callback),
                    mgp);

  /* Delete All Controlpoints */
  button = gtk_button_new_from_stock ( GAP_STOCK_DELETE_ALL_POINTS );
  GTK_WIDGET_SET_FLAGS (button, GTK_CAN_DEFAULT);
  gtk_table_attach( GTK_TABLE(button_table), button, 1, 2, row, row+1,
                    GTK_FILL, 0, 0, 0 );
  gimp_help_set_help_data(button,
                       _("Delete all controlpoints")
                       , NULL);
  gtk_widget_show (button);
  g_signal_connect (G_OBJECT (button), "clicked",
                    G_CALLBACK (mov_pdel_all_callback),
                    mgp);


  row++;

  /* Load Controlpoints */
  button = gtk_button_new_from_stock (GTK_STOCK_OPEN );
  GTK_WIDGET_SET_FLAGS (button, GTK_CAN_DEFAULT);
  gtk_table_attach( GTK_TABLE(button_table), button, 0, 1, row, row+1,
                    GTK_FILL, 0, 0, 0 );
  gimp_help_set_help_data(button,
                       _("Load controlpoints from file")
                       , NULL);
  gtk_widget_show (button);
  g_signal_connect (G_OBJECT (button), "clicked",
                    G_CALLBACK (mov_pload_callback),
                    mgp);

  /* Save Controlpoints */
  button = gtk_button_new_from_stock ( GTK_STOCK_SAVE );
  GTK_WIDGET_SET_FLAGS (button, GTK_CAN_DEFAULT);
  gtk_table_attach( GTK_TABLE(button_table), button, 1, 2, row, row+1,
                    GTK_FILL, 0, 0, 0 );
  gimp_help_set_help_data(button,
                       _("Save controlpoints to file")
                       , NULL);
  gtk_widget_show (button);
  g_signal_connect (G_OBJECT (button), "clicked",
                    G_CALLBACK (mov_psave_callback),
                    mgp);

  row++;

  gtk_widget_show(button_table);
  gtk_widget_show(frame);

  /* the hbox */
  hbox = gtk_hbox_new (FALSE, 3);
  gtk_widget_show (hbox);

  gtk_box_pack_start (GTK_BOX (hbox), button_table, FALSE, FALSE, 4);
  gtk_container_add (GTK_CONTAINER (frame), hbox);

  gtk_box_pack_start (GTK_BOX (vbox), frame, FALSE, FALSE, 4);
  return vbox;
}  /* end mov_edit_button_box_create */

/* ============================================================================
 * Create set of widgets for the framerange and layerstack widgets
 * ============================================================================
 * vertical_layout == FALSE
 *   +-master_table-------------------------+----------------------+
 *   |  +-----------+--------+------------+ | +-vcbox-----------+  |
 *   |  | FromFrame | scale  | spinbutton | | |                 |  |
 *   |  +-----------+--------+------------+ | | ForceVisibility |  |
 *   |  | ToFrame   | scale  | spinbutton | | |                 |  |
 *   |  +-----------+--------+------------+ | | Clip To Frame   |  |
 *   |  | Layerstack| scale  | spinbutton | | |                 |  |
 *   |  +-----------+--------+------------+ | +-----------------+  |
 *   +--------------------------------------+----------------------+
 *
 * vertical_layout == TRUE
 *
 *   +-master_table-------------------------+
 *   |  +-vcbox---------------------------+ |
 *   |  | ForceVisibility                 | |
 *   |  | Clip To Frame                   | |
 *   |  +---------------------------------+ |
 *   +--------------------------------------+
 *   |  +-----------+--------+------------+ +
 *   |  | FromFrame | scale  | spinbutton | |
 *   |  +-----------+--------+------------+ |
 *   |  | ToFrame   | scale  | spinbutton | |
 *   |  +-----------+--------+------------+ |
 *   |  | Layerstack| scale  | spinbutton | |
 *   |  +-----------+--------+------------+ |
 *   +--------------------------------------+
 */
GtkWidget *
mov_path_framerange_box_create(t_mov_gui_stuff *mgp
                              ,gboolean vertical_layout
                              )
{
  GtkWidget *master_table;
  GtkWidget *table;
  GtkAdjustment *adj;
  GtkWidget *check_button;
  gint  master_rows;
  gint  master_cols;
  gint  tabcol, tabrow, boxcol, boxrow;
  gint  row;

  if(vertical_layout)
  {
    master_rows = 2;
    master_cols = 1;
    tabcol = 0;
    tabrow = 1;
    boxcol = 0;
    boxrow = 0;
  }
  else
  {
    master_rows = 1;
    master_cols = 2;
    tabcol = 0;
    tabrow = 0;
    boxcol = 1;
    boxrow = 0;
  }

  /* the master_table (1 row) */
  master_table = gtk_table_new (master_rows, master_cols, FALSE);
  gtk_widget_show (master_table);

  /* table with 3 rows */
  table = gtk_table_new (3, 3, FALSE);
  gtk_table_set_row_spacings (GTK_TABLE (table), 2);
  gtk_table_set_col_spacings (GTK_TABLE (table), 4);
  gtk_table_attach(GTK_TABLE(master_table), table, tabcol, tabcol+1, tabrow, tabrow+1
                  , GTK_FILL|GTK_EXPAND, GTK_FILL, 4, 0);
  gtk_widget_show (table);

  /* the start frame scale_entry */
  adj = gimp_scale_entry_new( GTK_TABLE (table), 0, 0,          /* table col, row */
                          _("From Frame:"),                     /* label text */
                          SCALE_WIDTH, ENTRY_WIDTH,             /* scalesize spinsize */
                          (gdouble)pvals->dst_range_start,      /* value */
                          (gdouble)mgp->first_nr,               /* lower */
                          (gdouble)mgp->last_nr,                /* upper */
                          1, 10,                                /* step, page */
                          0,                                    /* digits */
                          TRUE,                                 /* constrain */
                          (gdouble)mgp->first_nr,               /* lower, (unconstrained) */
                          (gdouble)mgp->last_nr,                /* upper (unconstrained) */
                          _("First handled destination frame"), NULL);      /* tooltip privatetip */
  g_object_set_data(G_OBJECT(adj), "mgp", mgp);
  g_signal_connect (G_OBJECT (adj), "value_changed",
                    G_CALLBACK (gimp_int_adjustment_update),
                    &pvals->dst_range_start);
  g_signal_connect (adj, "value-changed",
                            G_CALLBACK (mov_upd_seg_labels),
                            mgp);
  mgp->dst_range_start_adj = adj;

  /* the end frame scale_entry */
  adj = gimp_scale_entry_new( GTK_TABLE (table), 0, 1,          /* table col, row */
                          _("To Frame:"),                       /* label text */
                          SCALE_WIDTH, ENTRY_WIDTH,             /* scalesize spinsize */
                          (gdouble)pvals->dst_range_end,        /* value */
                          (gdouble)mgp->first_nr,               /* lower */
                          (gdouble)mgp->last_nr,                /* upper */
                          1, 10,                                /* step, page */
                          0,                                    /* digits */
                          TRUE,                                 /* constrain */
                          (gdouble)mgp->first_nr,               /* lower, (unconstrained) */
                          (gdouble)mgp->last_nr,                /* upper (unconstrained) */
                          _("Last handled destination frame"), NULL);       /* tooltip privatetip */
  g_object_set_data(G_OBJECT(adj), "mgp", mgp);
  g_signal_connect (G_OBJECT (adj), "value_changed",
                    G_CALLBACK (gimp_int_adjustment_update),
                    &pvals->dst_range_end);
  g_signal_connect (adj, "value-changed",
                            G_CALLBACK (mov_upd_seg_labels),
                            mgp);
  mgp->dst_range_end_adj = adj;

  /* the Layerstack scale_entry */
  adj = gimp_scale_entry_new( GTK_TABLE (table), 0, 2,          /* table col, row */
                          _("Layerstack:"),                     /* label text */
                          SCALE_WIDTH, ENTRY_WIDTH,             /* scalesize spinsize */
                          (gdouble)pvals->dst_layerstack,       /* value */
                          0.0, 99.0,                            /* lower, upper */
                          1, 10,                                /* step, page */
                          0,                                    /* digits */
                          FALSE,                                /* constrain */
                          0.0, 999999.0,                        /* lower, upper (unconstrained) */
                          _("How to insert source layer into the "
                            "layerstack of the destination frames. "
                            "layerstack 0 means on top i.e. in front"),
                          NULL);                              /* tooltip privatetip */
  g_object_set_data(G_OBJECT(adj), "mgp", mgp);
  g_signal_connect (G_OBJECT (adj), "value_changed",
                    G_CALLBACK (mov_instant_int_adjustment_update),
                    &pvals->dst_layerstack);
  mgp->dst_layerstack_adj = adj;

  /* the table for checkbuttons and info labels */
  table = gtk_table_new (3, 3, FALSE);
  gtk_widget_show (table);

  row = 0;

  /* toggle force visibility  */
  check_button = gtk_check_button_new_with_label ( _("Force Visibility"));
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (check_button),
                                pvals->src_force_visible);
  gimp_help_set_help_data(check_button,
                       _("Force visibility for all copied source layers")
                       , NULL);
  gtk_widget_show (check_button);
  g_object_set_data(G_OBJECT(check_button), "mgp", mgp);
  g_signal_connect (G_OBJECT (check_button), "toggled",
                    G_CALLBACK  (mov_force_visibility_toggle_callback),
                    &pvals->src_force_visible);
  gtk_table_attach(GTK_TABLE(table), check_button, 0, 1, row, row+1
                  , GTK_FILL, GTK_FILL, 4, 0);
  mgp->src_force_visible_check_button = check_button;

  row = 1;

  /* toggle clip_to_image */
  check_button = gtk_check_button_new_with_label ( _("Clip To Frame"));
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (check_button),
                                pvals->clip_to_img);
  gimp_help_set_help_data(check_button,
                       _("Clip all copied source layers at destination frame boundaries")
                       , NULL);
  gtk_widget_show (check_button);
  g_signal_connect (G_OBJECT (check_button), "toggled",
                    G_CALLBACK (mov_gint_toggle_callback),
                    &pvals->clip_to_img);
  gtk_table_attach(GTK_TABLE(table), check_button, 0, 1, row, row+1
                  , GTK_FILL, GTK_FILL, 4, 0);
  mgp->clip_to_img_check_button = check_button;



  gtk_table_attach(GTK_TABLE(master_table), table, boxcol, boxcol+1, boxrow, boxrow+1
                  , GTK_FILL, GTK_FILL, 4, 0);


  return(master_table);
}  /* end mov_path_framerange_box_create */



/* ============================================================================
 * Create  VBox with
 *   The VBox contains
 *   - Resize    2x spinbutton   (for resizing Width + Height in percent)
 *                  chainbutton  (for constrain both resize widgets)
 *   - Opacity      spinbutton   (0.0 upto 100.0 %)
 *   - Rotation     spinbutton   (-360.0 to 360.0 degrees)
 * ============================================================================
 */
static GtkWidget*
mov_modify_tab_create(t_mov_gui_stuff *mgp)
{
  GtkWidget      *vbox;
  GtkWidget      *table;
  GtkObject      *adj;

  /* the vbox */
  vbox = gtk_vbox_new (FALSE, 3);
  gtk_container_set_border_width (GTK_CONTAINER (vbox), 2);

  /* the table (2 rows) */
  table = gtk_table_new ( 2, 6, FALSE );
  gtk_container_set_border_width (GTK_CONTAINER (table), 2 );
  gtk_table_set_row_spacings (GTK_TABLE (table), 2);
  gtk_table_set_col_spacings (GTK_TABLE (table), 4);
  gtk_box_pack_start (GTK_BOX (vbox), table, TRUE, TRUE, 0);


  /* Width Scale */
  adj = p_mov_spinbutton_new( GTK_TABLE (table), 0, 0,        /* table col, row */
                          _("Width:"),                        /* label text */
                          SCALE_WIDTH, ENTRY_WIDTH,           /* scalesize spinsize */
                          (gdouble)mgp->w_resize,             /* value */
                          (gdouble)1, (gdouble)200,           /* lower, upper */
                          1, 10,                              /* step, page */
                          1,                                  /* digits */
                          FALSE,                              /* constrain */
                          (gdouble)1, (gdouble)1000,          /* lower, upper (unconstrained) */
                          _("Scale source layer's width in percent"),
                          NULL);    /* tooltip privatetip */
  g_object_set_data(G_OBJECT(adj), "mgp", mgp);
  g_signal_connect (G_OBJECT (adj), "value_changed",
                    G_CALLBACK (mov_path_resize_adjustment_update),
                    &mgp->w_resize);
  mgp->wres_adj = GTK_ADJUSTMENT(adj);

  /* Height Scale */
  adj = p_mov_spinbutton_new( GTK_TABLE (table), 0, 1,        /* table col, row */
                          _("Height:"),                       /* label text */
                          SCALE_WIDTH, ENTRY_WIDTH,           /* scalesize spinsize */
                          (gdouble)mgp->h_resize,             /* value */
                          (gdouble)1, (gdouble)200,           /* lower, upper */
                          1, 10,                              /* step, page */
                          1,                                  /* digits */
                          FALSE,                              /* constrain */
                          (gdouble)1, (gdouble)1000,          /* lower, upper (unconstrained) */
                          _("Scale source layer's height in percent"),
                          NULL);    /* tooltip privatetip */
  g_object_set_data(G_OBJECT(adj), "mgp", mgp);
  g_signal_connect (G_OBJECT (adj), "value_changed",
                    G_CALLBACK (mov_path_resize_adjustment_update),
                    &mgp->h_resize);
  mgp->hres_adj = GTK_ADJUSTMENT(adj);


  /*  the constrain ratio chainbutton  */
  mgp->constrain = gimp_chain_button_new (GIMP_CHAIN_RIGHT);
  gimp_chain_button_set_active (GIMP_CHAIN_BUTTON (mgp->constrain), TRUE);
  gtk_table_attach (GTK_TABLE (table), mgp->constrain, 2, 3, 0, 2
                   , 0,0,0,0);
  gtk_widget_show (mgp->constrain);

  gimp_help_set_help_data (GIMP_CHAIN_BUTTON (mgp->constrain)->button,
                           _("Constrain aspect ratio"), NULL);

  /* the state of the contrain ratio chainbutton is checked in other callbacks (where needed)
   * there is no need for the chainbutton to have its own callback procedure
   */


  /* Opacity */
  adj = gimp_scale_entry_new( GTK_TABLE (table), 3, 0,        /* table col, row */
                          _("Opacity:"),                      /* label text */
                          SCALE_WIDTH, ENTRY_WIDTH,           /* scalesize spinsize */
                          (gdouble)mgp->opacity,              /* value */
                          (gdouble)0, (gdouble)100,           /* lower, upper */
                          1, 10,                              /* step, page */
                          1,                                  /* digits */
                          TRUE,                               /* constrain */
                          (gdouble)0, (gdouble)100,           /* lower, upper (unconstrained) */
                          _("Set the source layer's opacity in percent"),
                          NULL);    /* tooltip privatetip */
  g_object_set_data(G_OBJECT(adj), "mgp", mgp);
  g_signal_connect (G_OBJECT (adj), "value_changed",
                    G_CALLBACK (mov_instant_double_adjustment_update),
                    &mgp->opacity);
  mgp->opacity_adj = GTK_ADJUSTMENT(adj);

  /* Rotation */
  adj = gimp_scale_entry_new( GTK_TABLE (table), 3, 1,        /* table col, row */
                          _("Rotate:"),                       /* label text */
                          SCALE_WIDTH, ENTRY_WIDTH,           /* scalesize spinsize */
                          (gdouble)mgp->rotation,             /* value */
                          (gdouble)-360, (gdouble)360,        /* lower, upper */
                          1, 45,                              /* step, page */
                          1,                                  /* digits */
                          FALSE,                              /* constrain */
                          (gdouble)-3600, (gdouble)3600,      /* lower, upper (unconstrained) */
                          _("Rotate source layer (in degrees)"),
                          NULL);    /* tooltip privatetip */
  g_object_set_data(G_OBJECT(adj), "mgp", mgp);
  g_signal_connect (G_OBJECT (adj), "value_changed",
                    G_CALLBACK (mov_instant_double_adjustment_update),
                    &mgp->rotation);
  mgp->rotation_adj = GTK_ADJUSTMENT(adj);


  gtk_widget_show (table);
  gtk_widget_show (vbox);

  /* copile without MOVE_PATH_LAYOUT_frame needs less space */
  return vbox;

}  /* end mov_modify_tab_create */


/* ============================================================================
 * Create  VBox with the 8 transformation factors and return it.
 *   The VBox contains
 *   - Transform 8x spinbutton   (0.01 upto 10.0) 4-point perspective transformation
 * ============================================================================
 */
static GtkWidget *
mov_trans_tab_create (t_mov_gui_stuff *mgp)
{
  GtkWidget      *vbox;
  GtkWidget      *table;
  GtkObject      *adj;


  /* the vbox */
  vbox = gtk_vbox_new (FALSE, 3);
  gtk_container_set_border_width (GTK_CONTAINER (vbox), 2);

  /* the table (2 rows) */
  table = gtk_table_new ( 2, 8, FALSE );
  gtk_container_set_border_width (GTK_CONTAINER (table), 2 );
  gtk_table_set_row_spacings (GTK_TABLE (table), 2);
  gtk_table_set_col_spacings (GTK_TABLE (table), 4);
  gtk_box_pack_start (GTK_BOX (vbox), table, TRUE, TRUE, 0);


  /* ttlx transformfactor */
  adj = p_mov_spinbutton_new( GTK_TABLE (table), 0, 0,        /* table col, row */
                          _("x1:"),                           /* label text */
                          SCALE_WIDTH, ENTRY_WIDTH,           /* scalesize spinsize */
                          (gdouble)mgp->ttlx,                 /* initial value */
                          (gdouble)0, (gdouble)10,            /* lower, upper */
                          0.1, 1,                             /* step, page */
                          3,                                  /* digits */
                          FALSE,                              /* constrain */
                          (gdouble)0, (gdouble)10,            /* lower, upper (unconstrained) */
                          _("Transformfactor for upper left corner X coordinate"),
                          NULL);    /* tooltip privatetip */
  g_object_set_data(G_OBJECT(adj), "mgp", mgp);
  g_signal_connect (G_OBJECT (adj), "value_changed",
                    G_CALLBACK (mov_path_tfactor_adjustment_update),
                    &mgp->ttlx);
  mgp->ttlx_adj = GTK_ADJUSTMENT(adj);


  /* ttly transformfactor */
  adj = p_mov_spinbutton_new( GTK_TABLE (table), 2, 0,        /* table col, row */
                          _("y1:"),                           /* label text */
                          SCALE_WIDTH, ENTRY_WIDTH,           /* scalesize spinsize */
                          (gdouble)mgp->ttly,                 /* initial value */
                          (gdouble)0, (gdouble)10,            /* lower, upper */
                          0.1, 1,                             /* step, page */
                          3,                                  /* digits */
                          FALSE,                              /* constrain */
                          (gdouble)0, (gdouble)10,            /* lower, upper (unconstrained) */
                          _("Transformfactor for upper left corner Y coordinate"),
                          NULL);    /* tooltip privatetip */
  g_object_set_data(G_OBJECT(adj), "mgp", mgp);
  g_signal_connect (G_OBJECT (adj), "value_changed",
                    G_CALLBACK (mov_path_tfactor_adjustment_update),
                    &mgp->ttly);
  mgp->ttly_adj = GTK_ADJUSTMENT(adj);


  /* ttrx transformfactor */
  adj = p_mov_spinbutton_new( GTK_TABLE (table), 4, 0,        /* table col, row */
                          _("x2:"),                           /* label text */
                          SCALE_WIDTH, ENTRY_WIDTH,           /* scalesize spinsize */
                          (gdouble)mgp->ttrx,                 /* initial value */
                          (gdouble)0, (gdouble)10,            /* lower, upper */
                          0.1, 1,                             /* step, page */
                          3,                                  /* digits */
                          FALSE,                              /* constrain */
                          (gdouble)0, (gdouble)10,            /* lower, upper (unconstrained) */
                          _("Transformfactor for upper right corner X coordinate"),
                          NULL);    /* tooltip privatetip */
  g_object_set_data(G_OBJECT(adj), "mgp", mgp);
  g_signal_connect (G_OBJECT (adj), "value_changed",
                    G_CALLBACK (mov_path_tfactor_adjustment_update),
                    &mgp->ttrx);
  mgp->ttrx_adj = GTK_ADJUSTMENT(adj);

  /* ttry transformfactor */
  adj = p_mov_spinbutton_new( GTK_TABLE (table), 6, 0,        /* table col, row */
                          _("y2:"),                           /* label text */
                          SCALE_WIDTH, ENTRY_WIDTH,           /* scalesize spinsize */
                          (gdouble)mgp->ttry,                 /* initial value */
                          (gdouble)0, (gdouble)10,            /* lower, upper */
                          0.1, 1,                             /* step, page */
                          3,                                  /* digits */
                          FALSE,                              /* constrain */
                          (gdouble)0, (gdouble)10,            /* lower, upper (unconstrained) */
                          _("Transformfactor for upper right corner Y coordinate"),
                          NULL);    /* tooltip privatetip */
  g_object_set_data(G_OBJECT(adj), "mgp", mgp);
  g_signal_connect (G_OBJECT (adj), "value_changed",
                    G_CALLBACK (mov_path_tfactor_adjustment_update),
                    &mgp->ttry);
  mgp->ttry_adj = GTK_ADJUSTMENT(adj);

  /* tblx transformfactor */
  adj = p_mov_spinbutton_new( GTK_TABLE (table), 0, 1,        /* table col, row */
                          _("x3:"),                           /* label text */
                          SCALE_WIDTH, ENTRY_WIDTH,           /* scalesize spinsize */
                          (gdouble)mgp->tblx,                 /* initial value */
                          (gdouble)0, (gdouble)10,            /* lower, upper */
                          0.1, 1,                             /* step, page */
                          3,                                  /* digits */
                          FALSE,                              /* constrain */
                          (gdouble)0, (gdouble)10,            /* lower, upper (unconstrained) */
                          _("Transformfactor for lower left corner X coordinate"),
                          NULL);    /* tooltip privatetip */
  g_object_set_data(G_OBJECT(adj), "mgp", mgp);
  g_signal_connect (G_OBJECT (adj), "value_changed",
                    G_CALLBACK (mov_path_tfactor_adjustment_update),
                    &mgp->tblx);
  mgp->tblx_adj = GTK_ADJUSTMENT(adj);

  /* tbly transformfactor */
  adj = p_mov_spinbutton_new( GTK_TABLE (table), 2, 1,        /* table col, row */
                          _("y3:"),                           /* label text */
                          SCALE_WIDTH, ENTRY_WIDTH,           /* scalesize spinsize */
                          (gdouble)mgp->tbly,                 /* initial value */
                          (gdouble)0, (gdouble)10,            /* lower, upper */
                          0.1, 1,                             /* step, page */
                          3,                                  /* digits */
                          FALSE,                              /* constrain */
                          (gdouble)0, (gdouble)10,            /* lower, upper (unconstrained) */
                          _("Transformfactor for lower left corner Y coordinate"),
                          NULL);    /* tooltip privatetip */
  g_object_set_data(G_OBJECT(adj), "mgp", mgp);
  g_signal_connect (G_OBJECT (adj), "value_changed",
                    G_CALLBACK (mov_path_tfactor_adjustment_update),
                    &mgp->tbly);
  mgp->tbly_adj = GTK_ADJUSTMENT(adj);

  /* tbrx transformfactor */
  adj = p_mov_spinbutton_new( GTK_TABLE (table), 4, 1,        /* table col, row */
                          _("x4:"),                           /* label text */
                          SCALE_WIDTH, ENTRY_WIDTH,           /* scalesize spinsize */
                          (gdouble)mgp->tbrx,                 /* initial value */
                          (gdouble)0, (gdouble)10,            /* lower, upper */
                          0.1, 1,                             /* step, page */
                          3,                                  /* digits */
                          FALSE,                              /* constrain */
                          (gdouble)0, (gdouble)10,            /* lower, upper (unconstrained) */
                          _("Transformfactor for lower right corner X coordinate"),
                          NULL);    /* tooltip privatetip */
  g_object_set_data(G_OBJECT(adj), "mgp", mgp);
  g_signal_connect (G_OBJECT (adj), "value_changed",
                    G_CALLBACK (mov_path_tfactor_adjustment_update),
                    &mgp->tbrx);
  mgp->tbrx_adj = GTK_ADJUSTMENT(adj);

  /* tbry transformfactor */
  adj = p_mov_spinbutton_new( GTK_TABLE (table), 6, 1,        /* table col, row */
                          _("y4:"),                           /* label text */
                          SCALE_WIDTH, ENTRY_WIDTH,           /* scalesize spinsize */
                          (gdouble)mgp->tbry,                 /* initial value */
                          (gdouble)0, (gdouble)10,            /* lower, upper */
                          0.1, 1,                             /* step, page */
                          3,                                  /* digits */
                          FALSE,                              /* constrain */
                          (gdouble)0, (gdouble)10,            /* lower, upper (unconstrained) */
                          _("Transformfactor for lower right corner Y coordinate"),
                          NULL);    /* tooltip privatetip */
  g_object_set_data(G_OBJECT(adj), "mgp", mgp);
  g_signal_connect (G_OBJECT (adj), "value_changed",
                    G_CALLBACK (mov_path_tfactor_adjustment_update),
                    &mgp->tbry);
  mgp->tbry_adj = GTK_ADJUSTMENT(adj);

  gtk_widget_show(table);
  gtk_widget_show(vbox);

  return vbox;
}  /* end mov_trans_tab_create */



/* -----------------------------------------
 * mov_acc_tab_create
 * ----------------------------------------
 * Create  VBox with the acceleration characteristics and return it.
 *   The VBox contains
 *   - Transform 8x spinbutton   (0.01 upto 10.0) 4-point perspective transformation
 */
static GtkWidget *
mov_acc_tab_create (t_mov_gui_stuff *mgp)
{
  GtkWidget      *vbox;
  GtkWidget      *table;
  GtkObject      *adj;

#define ACC_MIN  GAP_ACCEL_CHAR_MIN
#define ACC_MAX  GAP_ACCEL_CHAR_MAX

  /* the vbox */
  vbox = gtk_vbox_new (FALSE, 3);
  gtk_container_set_border_width (GTK_CONTAINER (vbox), 2);

  /* the table (2 rows) */
  table = gtk_table_new ( 2, 9, FALSE );
  gtk_container_set_border_width (GTK_CONTAINER (table), 2 );
  gtk_table_set_row_spacings (GTK_TABLE (table), 2);
  gtk_table_set_col_spacings (GTK_TABLE (table), 4);
  gtk_box_pack_start (GTK_BOX (vbox), table, TRUE, TRUE, 0);


  /*  accelaration characteristic for Position (e.g. movement) */
  adj = p_mov_acc_spinbutton_new( GTK_TABLE (table), 0, 0,        /* table col, row */
                          _("Movement:"),                     /* label text */
                          SCALE_WIDTH, ENTRY_WIDTH,           /* scalesize spinsize */
                          (gdouble)mgp->accPosition,          /* initial value */
                          (gdouble)ACC_MIN, (gdouble)ACC_MAX, /* lower, upper */
                          1, 10,                              /* step, page */
                          0,                                  /* digits */
                          FALSE,                              /* constrain */
                          (gdouble)ACC_MIN, (gdouble)ACC_MAX, /* lower, upper (unconstrained) */
                          _("acceleration characteristic for movement (1 for constant speed, positive: acceleration, negative: deceleration)"),
                          NULL);    /* tooltip privatetip */
  g_object_set_data(G_OBJECT(adj), "mgp", mgp);
  g_signal_connect (G_OBJECT (adj), "value_changed",
                    G_CALLBACK (mov_path_acceleration_adjustment_update),
                    &mgp->accPosition);
  mgp->accPosition_adj = GTK_ADJUSTMENT(adj);



  /*  accelaration characteristic */
  adj = p_mov_acc_spinbutton_new( GTK_TABLE (table), 0, 1,        /* table col, row */
                          _("Opacity:"),                      /* label text */
                          SCALE_WIDTH, ENTRY_WIDTH,           /* scalesize spinsize */
                          (gdouble)mgp->accOpacity,           /* initial value */
                          (gdouble)ACC_MIN, (gdouble)ACC_MAX, /* lower, upper */
                          1, 10,                              /* step, page */
                          0,                                  /* digits */
                          FALSE,                              /* constrain */
                          (gdouble)ACC_MIN, (gdouble)ACC_MAX, /* lower, upper (unconstrained) */
                          _("acceleration characteristic for opacity (1 for constant speed, positive: acceleration, negative: deceleration)"),
                          NULL);    /* tooltip privatetip */
  g_object_set_data(G_OBJECT(adj), "mgp", mgp);
  g_signal_connect (G_OBJECT (adj), "value_changed",
                    G_CALLBACK (mov_path_acceleration_adjustment_update),
                    &mgp->accOpacity);
  mgp->accOpacity_adj = GTK_ADJUSTMENT(adj);




  /*  accelaration characteristic for Size (e.g. Zoom) */
  adj = p_mov_acc_spinbutton_new( GTK_TABLE (table), 3, 0,        /* table col, row */
                          _("Scale:"),                        /* label text */
                          SCALE_WIDTH, ENTRY_WIDTH,           /* scalesize spinsize */
                          (gdouble)mgp->accSize,              /* initial value */
                          (gdouble)ACC_MIN, (gdouble)ACC_MAX, /* lower, upper */
                          1, 10,                              /* step, page */
                          0,                                  /* digits */
                          FALSE,                              /* constrain */
                          (gdouble)ACC_MIN, (gdouble)ACC_MAX, /* lower, upper (unconstrained) */
                          _("acceleration characteristic for zoom (1 for constant speed, positive: acceleration, negative: deceleration)"),
                          NULL);    /* tooltip privatetip */
  g_object_set_data(G_OBJECT(adj), "mgp", mgp);
  g_signal_connect (G_OBJECT (adj), "value_changed",
                    G_CALLBACK (mov_path_acceleration_adjustment_update),
                    &mgp->accSize);
  mgp->accSize_adj = GTK_ADJUSTMENT(adj);


  /*  accelaration characteristic for Rotation */
  adj = p_mov_acc_spinbutton_new( GTK_TABLE (table), 3, 1,        /* table col, row */
                          _("Rotation:"),                     /* label text */
                          SCALE_WIDTH, ENTRY_WIDTH,           /* scalesize spinsize */
                          (gdouble)mgp->accRotation,          /* initial value */
                          (gdouble)ACC_MIN, (gdouble)ACC_MAX, /* lower, upper */
                          1, 10,                              /* step, page */
                          0,                                  /* digits */
                          FALSE,                              /* constrain */
                          (gdouble)ACC_MIN, (gdouble)ACC_MAX, /* lower, upper (unconstrained) */
                          _("acceleration characteristic for rotation (1 for constant speed, positive: acceleration, negative: deceleration)"),
                          NULL);    /* tooltip privatetip */
  g_object_set_data(G_OBJECT(adj), "mgp", mgp);
  g_signal_connect (G_OBJECT (adj), "value_changed",
                    G_CALLBACK (mov_path_acceleration_adjustment_update),
                    &mgp->accRotation);
  mgp->accRotation_adj = GTK_ADJUSTMENT(adj);

  /*  accelaration characteristic for Perspective */
  adj = p_mov_acc_spinbutton_new( GTK_TABLE (table), 6, 0,        /* table col, row */
                          _("Perspective:"),                  /* label text */
                          SCALE_WIDTH, ENTRY_WIDTH,           /* scalesize spinsize */
                          (gdouble)mgp->accPerspective,       /* initial value */
                          (gdouble)ACC_MIN, (gdouble)ACC_MAX, /* lower, upper */
                          1, 10,                              /* step, page */
                          0,                                  /* digits */
                          FALSE,                              /* constrain */
                          (gdouble)ACC_MIN, (gdouble)ACC_MAX, /* lower, upper (unconstrained) */
                          _("acceleration characteristic for perspective (1 for constant speed, positive: acceleration, negative: deceleration)"),
                          NULL);    /* tooltip privatetip */
  g_object_set_data(G_OBJECT(adj), "mgp", mgp);
  g_signal_connect (G_OBJECT (adj), "value_changed",
                    G_CALLBACK (mov_path_acceleration_adjustment_update),
                    &mgp->accPerspective);
  mgp->accPerspective_adj = GTK_ADJUSTMENT(adj);


  /*  accelaration characteristic for feather radius */
  adj = p_mov_acc_spinbutton_new( GTK_TABLE (table), 6, 1,        /* table col, row */
                          _("Feather Radius:"),                /* label text */
                          SCALE_WIDTH, ENTRY_WIDTH,           /* scalesize spinsize */
                          (gdouble)mgp->accSelFeatherRadius,  /* initial value */
                          (gdouble)ACC_MIN, (gdouble)ACC_MAX, /* lower, upper */
                          1, 10,                              /* step, page */
                          0,                                  /* digits */
                          FALSE,                              /* constrain */
                          (gdouble)ACC_MIN, (gdouble)ACC_MAX, /* lower, upper (unconstrained) */
                          _("acceleration characteristic for feather radius (1 for constant speed, positive: acceleration, negative: deceleration)"),
                          NULL);    /* tooltip privatetip */
  g_object_set_data(G_OBJECT(adj), "mgp", mgp);
  g_signal_connect (G_OBJECT (adj), "value_changed",
                    G_CALLBACK (mov_path_acceleration_adjustment_update),
                    &mgp->accSelFeatherRadius);
  mgp->accSelFeatherRadius_adj = GTK_ADJUSTMENT(adj);


  gtk_widget_show(table);
  gtk_widget_show(vbox);

  return vbox;
}  /* end mov_acc_tab_create */


/* ============================================================================
 * Create  VBox with the selection handling widgets and return it.
 * ============================================================================
 */
static GtkWidget *
mov_selection_handling_tab_create (t_mov_gui_stuff *mgp)
{
  GtkWidget      *combo;
  GtkWidget      *vbox;
  GtkWidget      *table;
  GtkObject      *adj;

  /* the vbox */
  vbox = gtk_vbox_new (FALSE, 3);
  gtk_container_set_border_width (GTK_CONTAINER (vbox), 2);

  /* the table (2 rows) */
  table = gtk_table_new ( 2, 3, FALSE );
  gtk_container_set_border_width (GTK_CONTAINER (table), 2 );
  gtk_table_set_row_spacings (GTK_TABLE (table), 2);
  gtk_table_set_col_spacings (GTK_TABLE (table), 4);
  gtk_box_pack_start (GTK_BOX (vbox), table, TRUE, TRUE, 0);

  /* Selection combo */
  combo = gimp_int_combo_box_new (_("Ignore selection (in all source images)"),    GAP_MOV_SEL_IGNORE,
                                  _("Use selection (from initial source image)"),  GAP_MOV_SEL_INITIAL,
                                  _("Use selections (from all source images)"),    GAP_MOV_SEL_FRAME_SPECIFIC,
                                  NULL);
  gimp_int_combo_box_connect (GIMP_INT_COMBO_BOX (combo),
                              GAP_MOV_SEL_IGNORE,              /* initial int value */
                              G_CALLBACK (mov_selmode_menu_callback),
                              mgp);
  gtk_table_attach(GTK_TABLE(table), combo, 0, 3, 0, 1,
                   0, 0, 0, 0);
  gimp_help_set_help_data(combo,
                       _("How to handle selections in the source image")
                       , NULL);
  gtk_widget_show(combo);
  mgp->src_selmode_combo = combo;

  /* Feather Radius */
  adj = gimp_scale_entry_new( GTK_TABLE (table), 0, 1,        /* table col, row */
                          _("Selection Feather Radius:"),     /* label text */
                          SCALE_WIDTH, ENTRY_WIDTH,           /* scalesize spinsize */
                          (gdouble)mgp->sel_feather_radius,   /* initial value */
                          (gdouble)0, (gdouble)100,           /* lower, upper */
                          1.0, 10.0,                          /* step, page */
                          1,                                  /* digits */
                          FALSE,                              /* constrain */
                          (gdouble)0, (gdouble)1000,          /* lower, upper (unconstrained) */
                          _("Feather radius in pixels (for smoothing selection(s))"),
                          NULL);    /* tooltip privatetip */
  g_object_set_data(G_OBJECT(adj), "mgp", mgp);
  g_signal_connect (G_OBJECT (adj), "value_changed",
                    G_CALLBACK (mov_path_feather_adjustment_update),
                    &mgp->sel_feather_radius);
  mgp->sel_feather_radius_adj = GTK_ADJUSTMENT(adj);

  /* for initial sensitivity */
  gimp_int_combo_box_set_active (GIMP_INT_COMBO_BOX (combo), pvals->src_selmode);


  gtk_widget_show(table);
  gtk_widget_show(vbox);

  return vbox;
}  /* end mov_selection_handling_tab_create */

/* ============================================================================
 * Create all Widget Blocks that represent
 *   - The current contolpoint settings
 *   - The Preview
 *   - The Buttonbox for Editing Controlpoints
 *   and attach those Blocks to the mgp->master_vbox Widget.
 *
 *   One "ControlPoint" has:
 *   - 2 spinbuttons X/Y, used for positioning
 *   - Keyframe     spinbutton integer (0 to max_frame)
 *   - Notebook  with following sub tables:
 *      - transform    SubTable  4-point perspective transformation
 *      - modify       SubTable  for Resize(Scaling), Opacity and Rotation
 *      - selection    SubTable  for selection handling (mode and feather radius)
 *      - acceleration SubTable  for acceleration characteristics
 * ============================================================================
 */
static void
mov_path_prevw_create ( GimpDrawable *drawable, t_mov_gui_stuff *mgp, gboolean vertical_layout)
{
  GtkWidget      *cpt_frame;
  GtkWidget      *pv_frame;
  GtkWidget      *wrap_box;
  GtkWidget      *vbox;
  GtkWidget      *hbox;
  GtkWidget      *notebook;
  GtkWidget      *table;
  GtkWidget      *label;
  GtkWidget      *aspect_frame;
  GtkWidget      *da_widget;
  GtkWidget      *pv_table;
  GtkWidget      *pv_sub_table;
  GtkWidget      *check_button;
  GtkObject      *adj;
  GtkWidget      *framerange_table;
  GtkWidget      *edit_buttons;

  mgp->drawable = drawable;
  mgp->dwidth   = gimp_drawable_width(drawable->drawable_id );
  mgp->dheight  = gimp_drawable_height(drawable->drawable_id );
  mgp->bpp      = gimp_drawable_bpp(drawable->drawable_id);
  if ( gimp_drawable_has_alpha(drawable->drawable_id) )
    mgp->bpp--;
  mgp->curx = 0;
  mgp->cury = 0;
  mgp->in_call = TRUE;  /* to avoid side effects while initialization */


  /* the vbox */
  vbox = gtk_vbox_new (FALSE, 3);
  gtk_container_set_border_width (GTK_CONTAINER (vbox), 2);
  gtk_widget_show (vbox);
  gtk_box_pack_start (GTK_BOX (mgp->master_vbox), vbox, TRUE, TRUE, 0);

  /* the cpt_frame */
  cpt_frame = gimp_frame_new (" ");  /* text "Current Point: [ %3d ] of [ %3d ]"
                                 * is set later in procedure p_update_point_index_text
                                 */
  gtk_container_set_border_width( GTK_CONTAINER( cpt_frame ), 2 );
  mgp->point_index_frame = cpt_frame;
  p_update_point_index_text(mgp);

  gtk_box_pack_start (GTK_BOX (vbox), cpt_frame, FALSE, FALSE, 0);
  gtk_widget_show( mgp->master_vbox );
  gtk_widget_show( cpt_frame );



  /* the table (3 rows) for other controlpoint specific settings */
  table = gtk_table_new ( 3, 4, FALSE );
  gtk_container_set_border_width (GTK_CONTAINER (table), 2 );
  gtk_table_set_row_spacings (GTK_TABLE (table), 2);
  gtk_table_set_col_spacings (GTK_TABLE (table), 4);
  gtk_container_add (GTK_CONTAINER (cpt_frame), table);


  /* X */
  adj = gimp_scale_entry_new( GTK_TABLE (table), 0, 0,            /* table col, row */
                          _("X:"),                                /* label text */
                          SCALE_WIDTH, SPINBUTTON_WIDTH,          /* scalesize spinsize */
                          (gdouble)mgp->p_x,                      /* value */
                          (gdouble)0, (gdouble)mgp->dwidth,       /* lower, upper */
                          1, 10,                                  /* step, page */
                          0,                                      /* digits */
                          FALSE,                                  /* constrain */
                          (gdouble)(-GIMP_MAX_IMAGE_SIZE),
                          (gdouble)GIMP_MAX_IMAGE_SIZE,           /* lower, upper (unconstrained) */
                          _("X coordinate"),
                          NULL);    /* tooltip privatetip */
  g_signal_connect (G_OBJECT (adj), "value_changed",
                    G_CALLBACK (mov_path_x_adjustment_update),
                    mgp);
  mgp->x_adj = GTK_ADJUSTMENT(adj);

  /* Y */
  adj = gimp_scale_entry_new( GTK_TABLE (table), 0, 1,            /* table col, row */
                          _("Y:"),                                /* label text */
                          SCALE_WIDTH, ENTRY_WIDTH,               /* scalesize spinsize */
                          (gdouble)mgp->p_y,                      /* value */
                          (gdouble)0, (gdouble)mgp->dheight,      /* lower, upper */
                          1, 10,                                  /* step, page */
                          0,                                      /* digits */
                          FALSE,                                  /* constrain */
                          (gdouble)(-GIMP_MAX_IMAGE_SIZE),
                          (gdouble)GIMP_MAX_IMAGE_SIZE,           /* lower, upper (unconstrained) */
                          _("Y coordinate"),
                          NULL);    /* tooltip privatetip */
  g_signal_connect (G_OBJECT (adj), "value_changed",
                    G_CALLBACK (mov_path_y_adjustment_update),
                    mgp);
  mgp->y_adj = GTK_ADJUSTMENT(adj);

  /* Keyframe */
  adj = p_mov_spinbutton_new( GTK_TABLE (table), 1, 2,          /* table col, row */
                          _("Keyframe:"),                       /* label text */
                          SCALE_WIDTH, ENTRY_WIDTH,             /* scalesize spinsize */
                          (gdouble)mgp->keyframe_abs,           /* value */
                          (gdouble)0, (gdouble)mgp->max_frame,  /* lower, upper */
                          1, 10,                                /* step, page */
                          0,                                    /* digits */
                          TRUE,                                 /* constrain */
                          (gdouble)0, (gdouble)mgp->max_frame,  /* lower, upper (unconstrained) */
                          _("Fix controlpoint to keyframe number where 0 == no keyframe"),
                          NULL);    /* tooltip privatetip */
  g_signal_connect (G_OBJECT (adj), "value_changed",
                    G_CALLBACK (mov_path_keyframe_update),
                    mgp);
  mgp->keyframe_adj = GTK_ADJUSTMENT(adj);


  /* the notebook */
  notebook = gtk_notebook_new();

  {
    GtkWidget *modify_table;
    GtkWidget *transform_table;
    GtkWidget *acceleration_table;
    GtkWidget *selhandling_table;

    /* set of modifier widgets for the current controlpoint */
    modify_table = mov_modify_tab_create(mgp);

    /* set of perspective transformation widgets for the current controlpoint */
    transform_table = mov_trans_tab_create(mgp);

    /* set of acceleration characteristic widgets for the current controlpoint */
    acceleration_table = mov_acc_tab_create(mgp);

    /* set of perspective transformation widgets for the current controlpoint */
    selhandling_table = mov_selection_handling_tab_create(mgp);

    gtk_container_add (GTK_CONTAINER (notebook), modify_table);
    label = gtk_label_new(_("Scale and Modify"));
    gtk_notebook_set_tab_label (GTK_NOTEBOOK (notebook)
                             , gtk_notebook_get_nth_page (GTK_NOTEBOOK (notebook), 0)
                             , label
                             );
    gtk_container_add (GTK_CONTAINER (notebook), transform_table);
    label = gtk_label_new(_("Perspective"));
    gtk_notebook_set_tab_label (GTK_NOTEBOOK (notebook)
                             , gtk_notebook_get_nth_page (GTK_NOTEBOOK (notebook), 1)
                             , label
                             );
    gtk_container_add (GTK_CONTAINER (notebook), selhandling_table);
    label = gtk_label_new(_("Selection Handling"));
    gtk_notebook_set_tab_label (GTK_NOTEBOOK (notebook)
                             , gtk_notebook_get_nth_page (GTK_NOTEBOOK (notebook), 2)
                             , label
                             );
    gtk_container_add (GTK_CONTAINER (notebook), acceleration_table);
    label = gtk_label_new(_("Acceleration"));
    gtk_notebook_set_tab_label (GTK_NOTEBOOK (notebook)
                             , gtk_notebook_get_nth_page (GTK_NOTEBOOK (notebook), 3)
                             , label
                             );

  }
  gtk_table_attach(GTK_TABLE(table), notebook, 3, 4          /* column */
                                             , 0, 3          /* all rows */
                                             , 0, 0, 0, 0);

  gtk_widget_show (notebook);
  gtk_widget_show( table );



  /* the hbox (for preview table and Edit Controlpoint Frame) */
  hbox = gtk_hbox_new (FALSE, 3);
  gtk_widget_show (hbox);
  gtk_box_pack_start (GTK_BOX (vbox), hbox, TRUE, TRUE, 0);

  /* the preview frame */
  pv_frame = gimp_frame_new ( _("Preview"));
  gtk_container_set_border_width (GTK_CONTAINER (pv_frame), 2);
  gtk_box_pack_start (GTK_BOX (hbox), pv_frame, TRUE, TRUE, 0);
  gtk_widget_show (pv_frame);

  /* the preview table (3 rows) */
  pv_table = gtk_table_new ( 3, 1, FALSE );
  gtk_container_set_border_width (GTK_CONTAINER (pv_table), 2 );
  gtk_table_set_row_spacings (GTK_TABLE (pv_table), 2);
  gtk_table_set_col_spacings (GTK_TABLE (pv_table), 4);
  gtk_container_add (GTK_CONTAINER (pv_frame), pv_table);
  gtk_widget_show( pv_table );


  /*
   * Resize the greater one of dwidth and dheight to PREVIEW_SIZE
   */
  if ( mgp->dwidth > mgp->dheight )
  {
    mgp->pheight = mgp->dheight * PREVIEW_SIZE / mgp->dwidth;
    mgp->pheight = MAX (1, mgp->pheight);
    mgp->pwidth  = PREVIEW_SIZE;
  }
  else
  {
    mgp->pwidth  = mgp->dwidth * PREVIEW_SIZE / mgp->dheight;
    mgp->pwidth  = MAX (1, mgp->pwidth);
    mgp->pheight = PREVIEW_SIZE;
  }


  /* preview dummy widgets */
  {
    GtkWidget *table11;
    GtkWidget *dummy;

    gint ix;
    gint iy;
    /* aspect_frame is the CONTAINER for the preview drawing area */
    aspect_frame = gtk_aspect_frame_new (NULL   /* without label */
                                      , 0.5   /* xalign center */
                                      , 0.5   /* yalign center */
                                      , mgp->pwidth / mgp->pheight     /* ratio */
                                      , TRUE  /* obey_child */
                                      );


    /* table11 is used to center aspect_frame */
    table11 = gtk_table_new (3, 3, FALSE);
    gtk_widget_show (table11);
    for(ix = 0; ix < 3; ix++)
    {
      for(iy = 0; iy < 3; iy++)
      {
        if((ix == 1) && (iy == 1))
        {
           gtk_table_attach (GTK_TABLE (table11), aspect_frame, ix, ix+1, iy, iy+1,
                            (GtkAttachOptions) (GTK_FILL | GTK_SHRINK | GTK_EXPAND),
                            (GtkAttachOptions) (GTK_FILL | GTK_SHRINK | GTK_EXPAND),
                            0, 0);
        }
        else
        {
          /* dummy widgets to fill up table11  */
          dummy = gtk_vbox_new (FALSE,3);
          gtk_widget_show (dummy);
          gtk_table_attach (GTK_TABLE (table11), dummy, ix, ix+1, iy, iy+1,
                            (GtkAttachOptions) (GTK_FILL | GTK_SHRINK | GTK_EXPAND),
                            (GtkAttachOptions) (GTK_FILL | GTK_SHRINK | GTK_EXPAND),
                            0, 0);
        }
      }
    }

    wrap_box = gtk_vbox_new (FALSE,3);
    gtk_box_pack_start (GTK_BOX (wrap_box), table11, FALSE, FALSE, 0);
    gtk_widget_show(wrap_box);
    gtk_table_attach (GTK_TABLE (pv_table), wrap_box, 0, 1, 0, 1,
                    (GtkAttachOptions) (GTK_EXPAND | GTK_SHRINK | GTK_FILL),
                    (GtkAttachOptions) (GTK_EXPAND | GTK_SHRINK | GTK_FILL), 0, 0);

  }


  /* segment information labels (to show min/max speed in pixels per frame) */
  {
    GtkWidget *label;
    GtkWidget *seg_table;
    gint seg_row;

    seg_row = 0;

    /* the preview sub table (1 row) */
    seg_table = gtk_table_new ( 1, 6, FALSE );
    gtk_widget_show (seg_table);

    label = gtk_label_new(_("Segment:"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_widget_show (label);
    gtk_table_attach(GTK_TABLE(seg_table), label, 0, 1, seg_row, seg_row+1
                  , GTK_FILL, GTK_FILL, 4, 0);

    label = gtk_label_new("0");
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_widget_show (label);
    mgp->segNumberLabel = label;
    gtk_table_attach(GTK_TABLE(seg_table), label, 1, 2, seg_row, seg_row+1
                    , GTK_FILL, GTK_FILL, 4, 0);


    label = gtk_label_new(_("Length:"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_widget_show (label);
    gtk_table_attach(GTK_TABLE(seg_table), label, 2, 3, seg_row, seg_row+1
                  , GTK_FILL, GTK_FILL, 4, 0);


    label = gtk_label_new("0.0");
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_widget_show (label);
    mgp->segLengthLabel = label;
    gtk_table_attach(GTK_TABLE(seg_table), label, 3, 4, seg_row, seg_row+1
                    , GTK_FILL, GTK_FILL, 4, 0);

    label = gtk_label_new(_("Speed Min/Max:"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_widget_show (label);
    gtk_table_attach(GTK_TABLE(seg_table), label, 4, 5, seg_row, seg_row+1
                    , GTK_FILL, GTK_FILL, 4, 0);

    label = gtk_label_new("0.0");
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_widget_show (label);
    mgp->segSpeedLabel = label;
    gtk_table_attach(GTK_TABLE(seg_table), label, 5, 6, seg_row, seg_row+1
                    , GTK_FILL, GTK_FILL, 4, 0);


    gtk_table_attach(GTK_TABLE(pv_table), seg_table, 0, 1, 1, 2,
                     GTK_FILL|GTK_EXPAND, 0, 0, 0);

  }



  /* hbox_show block */
  {
    GtkWidget *hbox_show;


    hbox_show = gtk_hbox_new (FALSE, 3);
    gtk_widget_show (hbox_show);

    /* pathclor selection button */
    {
      GtkWidget      *color_button;

      gimp_rgb_set(&mgp->pathcolor, 1.0, 0.1, 0.1); /* startup with RED pathline color */
      gimp_rgb_set_alpha(&mgp->pathcolor, 1.0);
      color_button = gimp_color_button_new (_("Pathline Color Picker"),
                                  25, 12,                     /* WIDTH, HEIGHT, */
                                  &mgp->pathcolor,
                                  GIMP_COLOR_AREA_FLAT);
      gtk_box_pack_start (GTK_BOX (hbox_show), color_button, TRUE, TRUE, 4);
      gtk_widget_show (color_button);
      gimp_help_set_help_data(color_button,
                         _("Select the color that is used to "
                           "draw pathlines in the preview")
                         , NULL);

      g_signal_connect (color_button, "color_changed",
                    G_CALLBACK (mov_path_colorbutton_update),
                    mgp);

    }


    /* toggle Show path */
    check_button = gtk_check_button_new_with_label ( _("Path"));
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (check_button),
                                  mgp->show_path);
    gimp_help_set_help_data(check_button,
                         _("Show path lines and enable "
                           "pick/drag with left button "
                           "or move with right button")
                         , NULL);
    gtk_widget_show (check_button);
    gtk_box_pack_start (GTK_BOX (hbox_show), check_button, TRUE, TRUE, 0);

    g_signal_connect (G_OBJECT (check_button), "toggled",
                      G_CALLBACK  (mov_show_path_callback),
                      mgp);



    /* toggle Show cursor */
    check_button = gtk_check_button_new_with_label ( _("Cursor"));
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (check_button),
                                  mgp->show_cursor);
    gimp_help_set_help_data(check_button,
                         _("Show cursor crosslines")
                         , NULL);
    gtk_widget_show (check_button);
    gtk_box_pack_start (GTK_BOX (hbox_show), check_button, TRUE, TRUE, 0);

    g_signal_connect (G_OBJECT (check_button), "toggled",
                      G_CALLBACK  (mov_show_cursor_callback),
                      mgp);


    /* toggle Show Grid */
    check_button = gtk_check_button_new_with_label ( _("Grid"));
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (check_button),
                                  mgp->show_grid);
    gimp_help_set_help_data(check_button,
                         _("Show source layer as gridlines")
                         , NULL);
    //XX gtk_widget_show (check_button);
    gtk_box_pack_start (GTK_BOX (hbox_show), check_button, TRUE, TRUE, 0);

    g_signal_connect (G_OBJECT (check_button), "toggled",
                      G_CALLBACK  (mov_show_grid_callback),
                      mgp);

    /* toggle Instant Apply */
    check_button = gtk_check_button_new_with_label ( _("Instant Apply"));
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (check_button),
                                  mgp->instant_apply);
    gimp_help_set_help_data(check_button,
                         _("Update the preview automatically")
                         , NULL);
    gtk_widget_show (check_button);
    gtk_box_pack_start (GTK_BOX (hbox_show), check_button, TRUE, TRUE, 0);

    g_signal_connect (G_OBJECT (check_button), "toggled",
                      G_CALLBACK  (mov_instant_apply_callback),
                      mgp);


    gtk_table_attach(GTK_TABLE(pv_table), hbox_show, 0, 1, 2, 3,
                     GTK_FILL, 0, 0, 0);
  }

  /* the preview sub table (1 row) */
  pv_sub_table = gtk_table_new ( 1, 3, FALSE );

  /* the Preview Frame Number  */
  adj = gimp_scale_entry_new( GTK_TABLE (pv_sub_table), 0, 1,   /* table col, row */
                          _("Frame:"),                          /* label text */
                          SCALE_WIDTH, ENTRY_WIDTH,             /* scalesize spinsize */
                          (gdouble)mgp->preview_frame_nr,       /* value */
                          (gdouble)mgp->first_nr,               /* lower */
                          (gdouble)mgp->last_nr,                /* upper */
                          1, 10,                                /* step, page */
                          0,                                    /* digits */
                          TRUE,                                 /* constrain */
                          (gdouble)mgp->first_nr,               /* lower (unconstrained)*/
                          (gdouble)mgp->last_nr,                /* upper (unconstrained)*/
                          _("Frame to show when 'Refresh' button is pressed"),
                          NULL);                                /* tooltip privatetip */
  g_object_set_data(G_OBJECT(adj), "mgp", mgp);
  g_signal_connect (G_OBJECT (adj), "value_changed",
                    G_CALLBACK (mov_instant_int_adjustment_update),
                    &mgp->preview_frame_nr);
  mgp->preview_frame_nr_adj = GTK_ADJUSTMENT(adj);
  
  if(mgp->ainfo_ptr->ainfo_type != GAP_AINFO_FRAMES)
  {
    GtkWidget *widget;

    widget = GTK_WIDGET(g_object_get_data (G_OBJECT (adj), "label"));
    if(widget)
    {
      gtk_widget_hide(widget);
    }
    widget = GTK_WIDGET(g_object_get_data (G_OBJECT (adj), "spinbutton"));
    if(widget)
    {
      gtk_widget_hide(widget);
    }
    widget = GTK_WIDGET(g_object_get_data (G_OBJECT (adj), "scale"));
    if(widget)
    {
      gtk_widget_hide(widget);
    }
    
  }


  gtk_table_attach( GTK_TABLE(pv_table), pv_sub_table, 0, 1, 3, 4,
                    GTK_FILL|GTK_EXPAND, 0, 0, 0 );
  gtk_widget_show (pv_sub_table);


  /* PREVIEW DRAWING AREA */
  mgp->pv_ptr = gap_pview_new(mgp->pwidth
                                , mgp->pheight
                                , GAP_MOV_CHECK_SIZE
                                , aspect_frame
                                );
  da_widget = mgp->pv_ptr->da_widget;

  g_object_set_data( G_OBJECT(da_widget), "mgp", mgp);
  gtk_widget_set_events( GTK_WIDGET(da_widget), PREVIEW_MASK );
  g_signal_connect_after( G_OBJECT(da_widget), "expose_event",
                          G_CALLBACK (mov_path_prevw_preview_expose),
                          mgp );
  g_signal_connect( G_OBJECT(da_widget), "event",
                    G_CALLBACK  (mov_path_prevw_preview_events),
                    mgp );
  gtk_container_add( GTK_CONTAINER( aspect_frame ), da_widget);
  gtk_widget_show(da_widget);
  gtk_widget_show(aspect_frame);

  /* keep track of resizing events of the preview
   * for automatic preview scale when more or less layoutspace is available.
   */

  g_signal_connect_after (G_OBJECT (wrap_box), "size_allocate",
                      G_CALLBACK (mov_pview_size_allocate_callback),
                      mgp);

  /* Draw the contents of preview, that is saved in the preview widget */
  mov_path_prevw_preview_init( mgp );


  /* edit buttons table */
  edit_buttons = mov_edit_button_box_create(mgp);
  if(vertical_layout)
  {
    GtkWidget *vvbox;

    vvbox = gtk_vbox_new(FALSE, 3);
    gtk_widget_show (vvbox);
    framerange_table = mov_path_framerange_box_create(mgp, vertical_layout);

    gtk_box_pack_start (GTK_BOX (vvbox), edit_buttons, TRUE, TRUE, 0);
    gtk_box_pack_start (GTK_BOX (vvbox), framerange_table, TRUE, TRUE, 0);
    gtk_box_pack_start (GTK_BOX (hbox), vvbox, FALSE, FALSE, 0);
  }
  else
  {
    gtk_box_pack_start (GTK_BOX (hbox), edit_buttons, FALSE, FALSE, 0);
  }


  mov_path_prevw_cursor_update( mgp );

  mgp->in_call = FALSE;   /* End of initialization */
  if(gap_debug) printf("pvals mgp=%d,%d\n", mgp->p_x, mgp->p_y );
  if(gap_debug) printf("mgp cur=%d,%d\n", mgp->curx, mgp->cury );

}  /* end mov_path_prevw_create */


/* ============================================================================
 *  mov_path_prevw_preview_init
 *    Initialize preview
 *    Draw the contents into the internal buffer of the preview widget
 * ============================================================================
 */

static void
mov_path_prevw_preview_init ( t_mov_gui_stuff *mgp )
{
  gint32  image_id;


  if(gap_debug) printf ("mov_path_prevw_preview_init:  START\n");

  if(mgp->pv_ptr)
  {
    if(gap_debug) printf ("mov_path_prevw_preview_init:"
                          " before gap_pview_render_from_image drawable_id:%d\n"
                          , (int)mgp->drawable->drawable_id);
    image_id = gimp_drawable_get_image(mgp->drawable->drawable_id);
    if(gap_debug) printf ("mov_path_prevw_preview_init:"
                          " after gap_pview_render_from_image drawable_id:%d image_id:%d\n"
                          , (int)mgp->drawable->drawable_id
                          , (int)image_id
                          );
    gap_pview_render_from_image(mgp->pv_ptr, image_id);
  }
}

/* ============================================================================
 * mov_path_prevw_draw
 *   Preview Rendering Util routine End
 *     if update & PATH_LINE, draw the path lines
 *     if update & CURSOR,  draw cross cursor
 * ============================================================================
 */

static void
mov_path_prevw_draw ( t_mov_gui_stuff *mgp, gint update )
{
  gint     l_idx;
  GdkColor fg;
  GdkColormap *cmap;
  guchar   l_red, l_green, l_blue;

  if(gap_debug) printf("mov_path_prevw_draw: START update:%d\n", (int)update);

  mov_upd_seg_labels(NULL, mgp);

  if(mgp->pv_ptr == NULL)
  {
    return;
  }
  if(mgp->pv_ptr->da_widget==NULL)
  {
    return;
  }

  if(gap_debug) printf("mov_path_prevw_draw: gap_pview_repaint\n");
  gap_pview_repaint(mgp->pv_ptr);

  /* alternate cross cursor OR path graph */

  if((mgp->show_path)
  && ( pvals != NULL )
  && (update & PATH_LINE))
  {
      /* redraw the preview
       * (to clear path lines and cross cursor)
       */
      gimp_rgb_get_uchar (&mgp->pathcolor, &l_red, &l_green, &l_blue);

      cmap = gtk_widget_get_colormap(mgp->pv_ptr->da_widget);
      fg.red   = (l_red   << 8) | l_red;
      fg.green = (l_green << 8) | l_green;
      fg.blue  = (l_blue  << 8) | l_blue;

      /*if(gap_debug) printf ("fg.r/g/b (%d %d %d)\n", (int)fg.red ,(int)fg.green, (int)fg.blue); */

      if(cmap)
      {
         gdk_colormap_alloc_color(cmap
                              , &fg
                              , FALSE   /* writeable */
                              , TRUE   /* best_match */
                              );
      }
      /*if(gap_debug) printf ("fg.pixel (%d)\n", (int)fg.pixel); */


      gdk_gc_set_foreground (mgp->pv_ptr->da_widget->style->black_gc, &fg);

      p_points_to_tab(mgp);
      for(l_idx = 0; l_idx < pvals->point_idx_max; l_idx++)
        {
           /* draw the path line(s) */
           gdk_draw_line (mgp->pv_ptr->da_widget->window,
                          mgp->pv_ptr->da_widget->style->black_gc,
                          (pvals->point[l_idx].p_x    * mgp->pwidth) / mgp->dwidth,
                          (pvals->point[l_idx].p_y    * mgp->pheight) / mgp->dheight,
                          (pvals->point[l_idx +1].p_x * mgp->pwidth) / mgp->dwidth,
                          (pvals->point[l_idx +1].p_y * mgp->pheight) / mgp->dheight
                          );
           /* draw the path point(s) */
           gdk_draw_arc (mgp->pv_ptr->da_widget->window, mgp->pv_ptr->da_widget->style->black_gc, TRUE,
                            (pvals->point[l_idx +1].p_x  * mgp->pwidth / mgp->dwidth) -RADIUS,
                            (pvals->point[l_idx +1].p_y  * mgp->pheight / mgp->dheight) -RADIUS,
                            RADIUS * 2, RADIUS * 2, 0, 23040);
        }
        /* draw the start point */
        gdk_draw_arc (mgp->pv_ptr->da_widget->window, mgp->pv_ptr->da_widget->style->black_gc, TRUE,
                     (pvals->point[0].p_x * mgp->pwidth / mgp->dwidth) -RADIUS,
                     (pvals->point[0].p_y * mgp->pheight / mgp->dheight) -RADIUS,
                     RADIUS * 2, RADIUS * 2, 0, 23040);

        /* restore black gc */
        fg.red   = 0;
        fg.green = 0;
        fg.blue  = 0;
        if(cmap)
        {
          gdk_colormap_alloc_color(cmap
                                , &fg
                                , FALSE   /* writeable */
                                , TRUE   /* best_match */
                                );
        }

        gdk_gc_set_foreground (mgp->pv_ptr->da_widget->style->black_gc, &fg);
  }

  /* draw CURSOR */
  if(mgp->show_cursor)
  {
      if(gap_debug) printf("mov_path_prevw_draw: draw-cursor cur=%d,%d\n"
             , mgp->curx
             , mgp->cury
             );
      gdk_gc_set_function ( mgp->pv_ptr->da_widget->style->black_gc, GDK_INVERT);

      gdk_draw_line ( mgp->pv_ptr->da_widget->window,
                      mgp->pv_ptr->da_widget->style->black_gc,
                      mgp->curx, 1, mgp->curx, mgp->pheight-1 );
      gdk_draw_line ( mgp->pv_ptr->da_widget->window,
                      mgp->pv_ptr->da_widget->style->black_gc,
                      1, mgp->cury, mgp->pwidth-1, mgp->cury );
      /* current position of cursor is updated */
      gdk_gc_set_function ( mgp->pv_ptr->da_widget->style->black_gc, GDK_COPY);
  }
}

/* Adjustment Update Callbacks (with instant_update request) */

static void
mov_instant_int_adjustment_update(GtkObject *obj, gpointer val)
{
  t_mov_gui_stuff *mgp;

  gimp_int_adjustment_update(GTK_ADJUSTMENT(obj), val);


  mgp = g_object_get_data( G_OBJECT(obj), "mgp" );
  mov_set_instant_apply_request(mgp);
}  /* end mov_instant_int_adjustment_update */

static void
mov_instant_double_adjustment_update(GtkObject *obj, gpointer val)
{
  t_mov_gui_stuff *mgp;

  gimp_double_adjustment_update(GTK_ADJUSTMENT(obj), val);

  mgp = g_object_get_data( G_OBJECT(obj), "mgp" );
  mov_set_instant_apply_request(mgp);
}  /* end mov_instant_double_adjustment_update */


static void
mov_path_colorbutton_update( GimpColorButton *widget,
                             t_mov_gui_stuff *mgp )
{
  if(mgp)
  {
    gimp_color_button_get_color(widget, &mgp->pathcolor);
    mov_path_prevw_cursor_update( mgp );
    mov_path_prevw_draw ( mgp, CURSOR | PATH_LINE );
  }
}  /* end mov_path_colorbutton_update */

static void
mov_path_keycolorbutton_clicked( GimpColorButton *widget,
                             t_mov_gui_stuff *mgp )
{
  if(widget->dialog)
  {
    /* WORKAROUND:
     *  we dont need the coloresction dialog for the keycolor_button,
     *  because only the Bluebox Dioalog Window should open
     *  when the keycolor_button is clicked.
     *  The coloresction dialog is opened by
     *  the standard "clicked" signal handler of the GimpColorButton Widget.
     *  that fires always before this private signal handler.
     *
     *  because i have no idea how to remove the standard signal handler
     *  i used this Workaround, that simply destroys the unwanted dialog
     *  immediate after its creation.
     *  (this works fine, but the coloresction dialog may filcker up for a very short time)
     */
    gtk_widget_destroy(widget->dialog);
    widget->dialog = NULL;
  }

  if(mgp)
  {
    if((pvals->bbp_pv)
    && (pvals->src_layer_id >= 0)
    && (pvals->src_image_id >= 0))
    {
      /* use the current source layer for The Blubox Dialog
       * but do not perform changes here.
       */

      pvals->bbp_pv->image_id = pvals->src_image_id;
      pvals->bbp_pv->drawable_id = pvals->src_layer_id;
      pvals->bbp_pv->layer_id = pvals->src_layer_id;
      pvals->bbp_pv->run_flag = FALSE;
      pvals->bbp_pv->run_mode = GIMP_RUN_INTERACTIVE;

      gap_bluebox_dialog(pvals->bbp_pv);

      if(mgp->shell == NULL)
      {
        /* the MovePath Main Window was closed
         * quit gtk main loop and return immediate
         * without touching any data structures and widgets
         * (could be freed or invalid at this point)
         * the only task that is left to do is to destroy
         * the bluebox dialog shell if it is still there
         */
        if(pvals->bbp_pv->shell)
        {
          gtk_widget_destroy(pvals->bbp_pv->shell);
        }
        gtk_main_quit();
        return;
      }

      if(pvals->bbp == NULL)
      {
        pvals->bbp = gap_bluebox_bbp_new(-1);
        if(pvals->bbp == NULL)
        {
          return;
        }
      }

      /* if Blubox dialog was left with OK button get values for rendering */
      if(pvals->bbp_pv->run_flag)
      {
        memcpy(&pvals->bbp->vals, &pvals->bbp_pv->vals, sizeof(GapBlueboxVals));
      }

      gimp_color_button_set_color(widget, &pvals->bbp->vals.keycolor);
      mov_set_instant_apply_request(mgp);

    }
  }
}  /* end mov_path_keycolorbutton_clicked */


static void
mov_path_keycolorbutton_changed( GimpColorButton *widget,
                             t_mov_gui_stuff *mgp )
{
  if(mgp)
  {
    gimp_color_button_get_color(widget, &pvals->bbp_pv->vals.keycolor);

    if(pvals->bbp == NULL)
    {
      pvals->bbp = gap_bluebox_bbp_new(-1);
      if(pvals->bbp == NULL)
      {
        return;
      }
    }

    memcpy(&pvals->bbp->vals.keycolor, &pvals->bbp_pv->vals.keycolor, sizeof(GimpRGB));
    mov_set_instant_apply_request(mgp);
  }
}  /* end mov_path_keycolorbutton_changed */


static void
mov_path_keyframe_update ( GtkWidget *widget, t_mov_gui_stuff *mgp)
{
  gimp_int_adjustment_update(GTK_ADJUSTMENT(widget), &mgp->keyframe_abs);
  p_accel_widget_sensitivity(mgp);
  p_points_to_tab(mgp);
  mov_upd_seg_labels(NULL, mgp);
}


/*
 *  mov_path_xy_adjustment_update
 */

static void
mov_path_x_adjustment_update( GtkWidget *widget,
                              gpointer data )
{
  t_mov_gui_stuff *mgp;
  gint old_val;

  mgp = (t_mov_gui_stuff *)data;
  if(mgp == NULL) return;

  old_val = mgp->p_x;
  gimp_int_adjustment_update(GTK_ADJUSTMENT(widget), &mgp->p_x);
  if( old_val != mgp->p_x )
  {
      if( !mgp->in_call )
      {
          mov_path_prevw_cursor_update( mgp );
          mov_path_prevw_draw ( mgp, CURSOR | PATH_LINE );
          mov_set_instant_apply_request(mgp);
      }
  }
}

static void
mov_path_y_adjustment_update( GtkWidget *widget,
                              gpointer data )
{
  t_mov_gui_stuff *mgp;
  gint old_val;

  mgp = (t_mov_gui_stuff *)data;
  if(mgp == NULL) return;

  old_val = mgp->p_y;
  gimp_int_adjustment_update(GTK_ADJUSTMENT(widget), &mgp->p_y);
  if( old_val != mgp->p_y )
  {
      if( !mgp->in_call )
      {
          mov_path_prevw_cursor_update( mgp );
          mov_path_prevw_draw ( mgp, CURSOR | PATH_LINE );
          mov_set_instant_apply_request(mgp);
      }
  }
}

/* -----------------------------------------
 * mov_path_tfactor_adjustment_update
 * ----------------------------------------
 * update value for one of the perspective transformation factors
 */
static void
mov_path_tfactor_adjustment_update( GtkWidget *widget,
                            gdouble *val )
{
  gdouble old_val;
  t_mov_gui_stuff *mgp;

  mgp = g_object_get_data( G_OBJECT(widget), "mgp" );

  if(mgp == NULL) return;
  old_val = *val;
  gimp_double_adjustment_update(GTK_ADJUSTMENT(widget), (gpointer)val);
  if( old_val != *val )
  {
      mov_set_instant_apply_request(mgp);
      if( !mgp->in_call )
      {
          mov_path_prevw_cursor_update( mgp );
          mov_path_prevw_draw ( mgp, CURSOR | PATH_LINE );
          //XXX check if we need an additional GRID flag for the preview
      }
  }
}  /* end mov_path_tfactor_adjustment_update */

/* -----------------------------------------
 * mov_path_acceleration_adjustment_update
 * ----------------------------------------
 * update value for one of the acceleration characteristic values
 */
static void
mov_path_acceleration_adjustment_update(GtkWidget *widget,
                            gint *val )
{
  gint old_val;
  t_mov_gui_stuff *mgp;

  mgp = g_object_get_data( G_OBJECT(widget), "mgp" );

  if(mgp == NULL) return;
  old_val = *val;
  gimp_int_adjustment_update(GTK_ADJUSTMENT(widget), (gpointer)val);
  p_points_to_tab(mgp);
  mov_upd_seg_labels(NULL, mgp);

  return;

}  /* end mov_path_acceleration_adjustment_update */



static void
mov_path_feather_adjustment_update( GtkWidget *widget,
                            gdouble *val )
{
  gdouble old_val;
  t_mov_gui_stuff *mgp;

  mgp = g_object_get_data( G_OBJECT(widget), "mgp" );

  if(mgp == NULL) return;
  old_val = *val;
  gimp_double_adjustment_update(GTK_ADJUSTMENT(widget), (gpointer)val);
  if( old_val != *val )
  {
      mov_set_instant_apply_request(mgp);
  }
}  /* end mov_path_feather_adjustment_update */

static void
mov_selmode_menu_callback (GtkWidget *widget, t_mov_gui_stuff *mgp)
{
  gboolean l_sensitive;
  GtkWidget *spinbutton;
  gint value;

  gimp_int_combo_box_get_active (GIMP_INT_COMBO_BOX (widget), &value);
  pvals->src_selmode = value;

  if(mgp == NULL) return;

  l_sensitive = TRUE;
  if(pvals->src_selmode == GAP_MOV_SEL_IGNORE)
  {
    l_sensitive = FALSE;
  }

  if(mgp->sel_feather_radius_adj)
  {
    spinbutton = GTK_WIDGET(g_object_get_data (G_OBJECT (mgp->sel_feather_radius_adj), "spinbutton"));
    if(spinbutton)
    {
      gtk_widget_set_sensitive(spinbutton, l_sensitive);
    }
  }
  mov_set_instant_apply_request(mgp);
}  /* end mov_selmode_menu_callback */


static void
mov_path_resize_adjustment_update( GtkObject *obj, gdouble *val)
{
  gdouble old_val;
  t_mov_gui_stuff *mgp;

  mgp = g_object_get_data( G_OBJECT(obj), "mgp" );

  if(mgp == NULL) return;
  old_val = *val;
  gimp_double_adjustment_update(GTK_ADJUSTMENT(obj), (gpointer)val);
  if( old_val != *val )
  {
    mov_set_instant_apply_request(mgp);
    if (gimp_chain_button_get_active (GIMP_CHAIN_BUTTON (mgp->constrain)))
    {
      /* in the constrain mode we propagate the value
       * for with and height resize factors to the other one
       * this constrains both spinbuttons to the same factor
       * and keeps the original src layer proportions
       */
      if(GTK_ADJUSTMENT(obj) == mgp->wres_adj)
      {
        gtk_adjustment_set_value(mgp->hres_adj, (gfloat)*val);
      }
      else
      {
        gtk_adjustment_set_value(mgp->wres_adj, (gfloat)*val);
      }
    }
  }
}  /* end mov_path_resize_adjustment_update */

static void
mov_path_tween_steps_adjustment_update ( GtkObject *obj, gint *val)
{
  t_mov_gui_stuff *mgp;

  if((obj == NULL) || (val == NULL))
  {
    return;
  }
  gimp_int_adjustment_update(GTK_ADJUSTMENT(obj), val);
  mgp = g_object_get_data( G_OBJECT(obj), "mgp" );
  if(mgp)
  {
    mov_tweenlayer_sensitivity(mgp);
  }

}  /* end mov_path_tween_steps_adjustment_update */


/*
 *  Update the cross cursor's  coordinates accoding to pvals->[xy]path_prevw
 *  but not redraw it
 */

static void
mov_path_prevw_cursor_update ( t_mov_gui_stuff *mgp )
{
  mgp->curx = mgp->p_x * mgp->pwidth / mgp->dwidth;
  mgp->cury = mgp->p_y * mgp->pheight / mgp->dheight;

  if( mgp->curx < 0 )                  mgp->curx = 0;
  else if( mgp->curx >= mgp->pwidth )  mgp->curx = mgp->pwidth-1;
  if( mgp->cury < 0 )                  mgp->cury = 0;
  else if( mgp->cury >= mgp->pheight)  mgp->cury = mgp->pheight-1;

}

/*
 *    Handle the expose event on the preview
 */
static gint
mov_path_prevw_preview_expose( GtkWidget *widget,
                            GdkEvent *event )
{
  t_mov_gui_stuff *mgp;

  mgp = g_object_get_data( G_OBJECT(widget), "mgp" );

  if((mgp->pv_ptr == NULL)
  || (mgp->in_call))
  {
       return FALSE;
  }

  if(mgp->pv_ptr->da_widget == NULL)
  {
       return FALSE;
  }

  mgp->in_call = TRUE;
  mov_path_prevw_draw( mgp, ALL );
  mgp->in_call = FALSE;
  return FALSE;
}


/*
 *    Handle other events on the preview
 */

static gint
mov_path_prevw_preview_events ( GtkWidget *widget,
                             GdkEvent *event )
{
  t_mov_gui_stuff *mgp;
  GdkEventButton *bevent;
  GdkEventMotion *mevent;
  gint upd_flag;
  gint mouse_button;

  mgp = g_object_get_data ( G_OBJECT(widget), "mgp" );

 /* HINT:
  * smooth update of both CURSOR and PATH_LINE
  * on every mousemove works fine on machines with 300MHz.
  * for slower machines it once was better to paint just the cross cursor,
  * and refresh the path lines only at
  * button press and release events.
  * 2003.07.12 hof:
  *    since we use a drawing_area that is repainted at each expose
  *    event, we MUST force painting of the PATH_LINE.
  *    (if we dont, the pathline disappears completely until the mouse
  *     button is released)
  *    The gap_pview_da repaint is faster than the old render_preview procedure
  *    so it may work even for slower machines now.
  */
  upd_flag = CURSOR | PATH_LINE;
  /* upd_flag = CURSOR; */

  mouse_button = 0;

  switch (event->type)
    {
    case GDK_EXPOSE:
      break;

    case GDK_BUTTON_RELEASE:
      bevent = (GdkEventButton *) event;
      mouse_button = 0 - bevent->button;
      mov_set_instant_apply_request(mgp);
      goto mbuttons;
    case GDK_BUTTON_PRESS:
      bevent = (GdkEventButton *) event;
      mouse_button = bevent->button;
    mbuttons:
      mgp->curx = bevent->x;
      mgp->cury = bevent->y;
      upd_flag = CURSOR | PATH_LINE;
      goto mouse;

    case GDK_MOTION_NOTIFY:
      mevent = (GdkEventMotion *) event;
      if ( !mevent->state ) break;
      mgp->curx = mevent->x;
      mgp->cury = mevent->y;
      mov_set_instant_apply_request(mgp);
    mouse:
      if((mouse_button == 1)
      && (mgp->show_path))
      {
         /* Picking of pathpoints is done only if
          *   the left mousebutton goes down (mouse_button == 1)
          *   and only if Path is visible
          */
         p_points_to_tab(mgp);
         mgp->p_x = mgp->curx * mgp->dwidth / mgp->pwidth;
         mgp->p_y = mgp->cury * mgp->dheight / mgp->pheight;
         p_pick_nearest_point(mgp->p_x, mgp->p_y);
         p_point_refresh(mgp);
      }
      else
      {
         mgp->p_x = mgp->curx * mgp->dwidth / mgp->pwidth;
         mgp->p_y = mgp->cury * mgp->dheight / mgp->pheight;
         p_points_to_tab(mgp);
         mov_path_prevw_cursor_update( mgp );
      }
      mov_path_prevw_draw( mgp, upd_flag);
      mgp->in_call = TRUE;
      gtk_adjustment_set_value (mgp->x_adj,
                                (gfloat)mgp->p_x);
      gtk_adjustment_set_value (mgp->y_adj,
                                (gfloat)mgp->p_y);

      mgp->in_call = FALSE;
      break;

    default:
      break;
    }

  return FALSE;
}


/* ============================================================================
 * p_chk_keyframes
 *   check if controlpoints and keyframe settings are OK
 *   return TRUE if OK,
 *   Pop Up error Dialog window and return FALSE if NOT.
 * ============================================================================
 */

static gint
p_chk_keyframes(t_mov_gui_stuff *mgp)
{
  gchar *l_err_lbltext;

  p_points_to_tab(mgp);

  l_err_lbltext = gap_mov_exec_chk_keyframes(pvals);

  if(*l_err_lbltext != '\0')
  {
    g_message(_("Can't operate with current controlpoint\n"
                "or keyframe settings.\n\n"
                "Error List:\n"
                "%s")
             ,l_err_lbltext );
    g_free(l_err_lbltext);
    return(FALSE);

  }
  g_free(l_err_lbltext);
  return(TRUE);
}       /* end p_chk_keyframes */

/* ============================================================================
 * p_get_flattened_drawable
 *   flatten the given image and return pointer to the
 *   (only) remaining drawable.
 * ============================================================================
 */
GimpDrawable *
p_get_flattened_drawable(gint32 image_id)
{
  GimpDrawable *l_drawable_ptr ;

  l_drawable_ptr = gimp_drawable_get (gap_image_merge_visible_layers(image_id, GIMP_CLIP_TO_IMAGE));
  return l_drawable_ptr;
}       /* end p_get_flattened_drawable */



/* ============================================================================
 *   add the selected source layer to the temp. preview image
 *   (modified accordung to current settings)
 *   then flatten the temporary preview image,
 *   and return pointer to the (only) remaining drawable.
 * ============================================================================
 */

GimpDrawable *
p_get_prevw_drawable (t_mov_gui_stuff *mgp)
{
  GapMovCurrent l_curr;
  gint      l_nlayers;

  l_curr.isSingleFrame = FALSE;
  l_curr.singleMovObjLayerId = pvals->src_layer_id;

  /* check if we have a source layer (to add to the preview) */
  if((pvals->src_layer_id >= 0) && (pvals->src_image_id >= 0))
  {
    p_points_to_tab(mgp);

    if(!gap_image_is_alive(pvals->src_image_id))
    {
      mov_refresh_src_layer_menu(mgp);
    }

    /* calculate current settings */
    l_curr.dst_frame_nr    = 0;

    l_curr.currX         = (gdouble)mgp->p_x;
    l_curr.currY         = (gdouble)mgp->p_y;
    l_curr.currOpacity   = (gdouble)mgp->opacity;
    l_curr.currWidth     = (gdouble)mgp->w_resize;
    l_curr.currHeight    = (gdouble)mgp->h_resize;
    l_curr.currRotation  = (gdouble)mgp->rotation;
    l_curr.currTTLX      = (gdouble)mgp->ttlx;
    l_curr.currTTLY      = (gdouble)mgp->ttly;
    l_curr.currTTRX      = (gdouble)mgp->ttrx;
    l_curr.currTTRY      = (gdouble)mgp->ttry;
    l_curr.currTBLX      = (gdouble)mgp->tblx;
    l_curr.currTBLY      = (gdouble)mgp->tbly;
    l_curr.currTBRX      = (gdouble)mgp->tbrx;
    l_curr.currTBRY      = (gdouble)mgp->tbry;
    l_curr.currSelFeatherRadius = (gdouble)mgp->sel_feather_radius;

    l_curr.accPosition         = (gdouble)mgp->accPosition;
    l_curr.accOpacity          = (gdouble)mgp->accOpacity;
    l_curr.accSize             = (gdouble)mgp->accSize;
    l_curr.accRotation         = (gdouble)mgp->accRotation;
    l_curr.accPerspective      = (gdouble)mgp->accPerspective;
    l_curr.accSelFeatherRadius = (gdouble)mgp->accSelFeatherRadius;


    l_curr.src_layer_idx   = 0;
    l_curr.src_layers      = gimp_image_get_layers (pvals->src_image_id, &l_nlayers);

    if((l_curr.src_layers != NULL) && (l_nlayers > 0))
    {
      l_curr.src_last_layer  = l_nlayers -1;
      /* findout index of src_layer_id */
      for(l_curr.src_layer_idx = 0;
          l_curr.src_layer_idx  < l_nlayers;
          l_curr.src_layer_idx++)
      {
         if(l_curr.src_layers[l_curr.src_layer_idx] == pvals->src_layer_id)
            break;
      }
    }
    if(pvals->src_stepmode >= GAP_STEP_FRAME)
    {
      gap_mov_render_fetch_src_frame (pvals, -1);  /* negative value fetches the selected frame number */
    }
    else
    {
     if(pvals->src_selmode != GAP_MOV_SEL_IGNORE)
     {
       gint32        l_sel_channel_id;
       gboolean      l_all_empty;

       l_all_empty = FALSE;
       if(gimp_selection_is_empty(pvals->src_image_id))
       {
         l_all_empty = TRUE;
       }
       l_sel_channel_id = gimp_image_get_selection(pvals->src_image_id);
       gap_mov_render_create_or_replace_tempsel_image(l_sel_channel_id, pvals, l_all_empty);
     }
    }

    /* set offsets (in cur_ptr)
     *  according to handle_mode and src_img dimension (pvals)
     */
    gap_mov_exec_set_handle_offsets(pvals,  &l_curr);

    /* render: add source layer to (temporary) preview image */
    gap_mov_render_render(pvals->tmp_image_id, pvals, &l_curr);

    if(l_curr.src_layers != NULL) g_free(l_curr.src_layers);
    l_curr.src_layers = NULL;
  }

  mov_check_valid_src_layer(mgp);

  /* flatten image, and get the (only) resulting drawable */
  return(p_get_flattened_drawable(pvals->tmp_image_id));

}       /* end p_get_prevw_drawable */


/* ---------------------------------
 * mov_set_instant_apply_request
 * ---------------------------------
 */
static void
mov_set_instant_apply_request(t_mov_gui_stuff *mgp)
{
  if(mgp)
  {
    mgp->instant_apply_request = TRUE; /* request is handled by timer */
  }
}  /* end mov_set_instant_apply_request */

/* ---------------------------------
 * mov_set_waiting_cursor
 * ---------------------------------
 */
static void
mov_set_waiting_cursor(t_mov_gui_stuff *mgp)
{
  if(mgp == NULL) return;

  gdk_window_set_cursor(GTK_WIDGET(mgp->shell)->window, mgp->cursor_wait);
  gdk_flush();

  /* g_main_context_iteration makes sure that waiting cursor is displayed */
  while(g_main_context_iteration(NULL, FALSE));

  gdk_flush();
}  /* end mov_set_waiting_cursor */

/* ---------------------------------
 * mov_set_active_cursor
 * ---------------------------------
 */
static void
mov_set_active_cursor(t_mov_gui_stuff *mgp)
{
  if(mgp == NULL) return;

  gdk_window_set_cursor(GTK_WIDGET(mgp->shell)->window, mgp->cursor_acitve);
  gdk_flush();
}  /* end mov_set_active_cursor */


/* ----------------------------------
 * p_mov_spinbutton_new
 * ----------------------------------
 * create label and spinbutton and add to table
 * return the adjustment of the spinbutton
 * (for compatible parameters to gimp_scale_entry_new
 *  there are some unused dummy parameters)
 */
GtkObject *
p_mov_spinbutton_new(GtkTable *table
                    ,gint      col
                    ,gint      row
                    ,gchar    *label_text
                    ,gint      scale_width      /* dummy, not used */
                    ,gint      spinbutton_width
                    ,gdouble   initial_val
                    ,gdouble   lower            /* dummy, not used */
                    ,gdouble   upper            /* dummy, not used */
                    ,gdouble   sstep
                    ,gdouble   pagestep
                    ,gint      digits
                    ,gboolean  constrain
                    ,gdouble   umin
                    ,gdouble   umax
                    ,gchar    *tooltip_text
                    ,gchar    *privatetip
                    )
{
  GtkObject      *adj;
  GtkWidget      *spinbutton;
  GtkWidget      *label;

  label = gtk_label_new (label_text);
  gtk_misc_set_alignment( GTK_MISC(label), 0.0, 0.5 );
  gtk_table_attach( GTK_TABLE(table), label, col, col+1, row, row+1,
                    GTK_FILL, 0, 4, 0 );
  gtk_widget_show(label);

  spinbutton = gimp_spin_button_new (&adj  /* return value */
                , initial_val
                , umin
                , umax
                , sstep
                , pagestep
                , 0.0                 /* page_size */
                , 1.0                 /* climb_rate */
                , digits
                );
  gtk_widget_set_size_request(spinbutton, spinbutton_width, -1);
  gtk_widget_show (spinbutton);
  gtk_table_attach (GTK_TABLE (table), spinbutton, col+1, col+2, row, row+1,
                    (GtkAttachOptions) (0),
                    (GtkAttachOptions) (0), 0, 0);
  gimp_help_set_help_data (spinbutton, tooltip_text, privatetip);

  g_object_set_data (G_OBJECT (adj), "label", label);
  g_object_set_data (G_OBJECT (adj), "spinbutton", spinbutton);

  return(adj);
}  /* end p_mov_spinbutton_new */

/* ----------------------------------
 * p_mov_acc_spinbutton_new
 * ----------------------------------
 * create label and spinbutton and add to table
 * return the adjustment of the spinbutton
 * (for compatible parameters to gimp_scale_entry_new
 *  there are some unused dummy parameters)
 */
GtkObject *
p_mov_acc_spinbutton_new(GtkTable *table
                    ,gint      col
                    ,gint      row
                    ,gchar    *label_text
                    ,gint      scale_width      /* dummy, not used */
                    ,gint      spinbutton_width
                    ,gdouble   initial_val
                    ,gdouble   lower            /* dummy, not used */
                    ,gdouble   upper            /* dummy, not used */
                    ,gdouble   sstep
                    ,gdouble   pagestep
                    ,gint      digits
                    ,gboolean  constrain
                    ,gdouble   umin
                    ,gdouble   umax
                    ,gchar    *tooltip_text
                    ,gchar    *privatetip
                    )
{
  GtkObject       *adj;
  GapAccelWidget  *accel_wgt;
  gint32           accelerationCharacteristic;

#define ACC_WGT_WIDTH 28
#define ACC_WGT_HEIGHT 26

  adj = p_mov_spinbutton_new(table
                    ,col
                    ,row
                    ,label_text
                    ,scale_width
                    ,spinbutton_width
                    ,initial_val
                    ,lower
                    ,upper
                    ,sstep
                    ,pagestep
                    ,digits
                    ,constrain
                    ,umin
                    ,umax
                    ,tooltip_text
                    ,privatetip
                    );

  accelerationCharacteristic = (gint32)initial_val;
  accel_wgt = gap_accel_new_with_adj(ACC_WGT_WIDTH, ACC_WGT_HEIGHT, accelerationCharacteristic, adj);


  gtk_table_attach( GTK_TABLE(table), accel_wgt->da_widget, col+2, col+3, row, row+1,
                    GTK_FILL, 0, 4, 0 );
  gtk_widget_show (accel_wgt->da_widget);

  return(adj);
}  /* end p_mov_acc_spinbutton_new */


/* --------------------------
 * mov_fit_initial_shell_window
 * --------------------------
 */
static void
mov_fit_initial_shell_window(t_mov_gui_stuff *mgp)
{
  gint width;
  gint height;

  if(mgp == NULL)                   { return; }
  if(mgp->shell_initial_width < 0)  { return; }

  width = mgp->shell_initial_width;
  height = mgp->shell_initial_height;

  gtk_widget_set_size_request (mgp->shell, width, height);  /* shrink shell window */
  gtk_window_set_default_size(GTK_WINDOW(mgp->shell), width, height);  /* shrink shell window */
  gtk_window_resize (GTK_WINDOW(mgp->shell), width, height);  /* shrink shell window */
}  /* end mov_fit_initial_shell_window */

/* -----------------------------
 * mov_shell_window_size_allocate
 * -----------------------------
 */
static void
mov_shell_window_size_allocate (GtkWidget       *widget,
                                GtkAllocation   *allocation,
                                gpointer         user_data)
{
   t_mov_gui_stuff *mgp;

   mgp = (t_mov_gui_stuff*)user_data;
   if((mgp == NULL) || (allocation == NULL))
   {
     return;
   }

   if(gap_debug) printf("mov_shell_window_size_allocate: START  shell_alloc: w:%d h:%d \n"
                           , (int)allocation->width
                           , (int)allocation->height
                           );

   if(mgp->shell_initial_width < 0)
   {
     mgp->shell_initial_width = allocation->width;
     mgp->shell_initial_height = allocation->height;
     mov_fit_initial_shell_window(mgp);  /* for setting window default size */
   }
   else
   {
     if((allocation->width < mgp->shell_initial_width)
     || (allocation->height < mgp->shell_initial_height))
     {
       /* dont allow shrink below initial size */
       mov_fit_initial_shell_window(mgp);
     }
   }
}  /* end mov_shell_window_size_allocate */

/* --------------------------------
 * mov_pview_size_allocate_callback
 * --------------------------------
 */
static void
mov_pview_size_allocate_callback(GtkWidget *widget
                                , GtkAllocation *allocation
                                , t_mov_gui_stuff *mgp
                                )
{
  gint actual_check_size;
  gint pwidth;
  gint pheight;
  gint psize;
  gboolean fit_initial;

  static gint ignore_inital_cnt = 2;

#define PREVIEW_BORDER_X  18
#define PREVIEW_BORDER_Y  18

  fit_initial = FALSE;

  if(ignore_inital_cnt > 0)
  {
    if(gap_debug) printf("\n\n === countdown: %d\n\n", (int)ignore_inital_cnt );
    ignore_inital_cnt--;
    return;
  }

  if((mgp == NULL) || (allocation == NULL))    { return; }
  if(mgp->startup)                             { return; }
  if(mgp->pv_ptr->da_widget == NULL)           { return; }
  if(mgp->pv_ptr->da_widget->window == NULL)   { return; }


  /* fit preview into allocated width and adjust the height */
  pwidth = allocation->width - PREVIEW_BORDER_X;
  pheight =  (pwidth * mgp->dheight) / MAX(mgp->dwidth,1);

  if(pheight + PREVIEW_BORDER_Y > allocation->height)
  {
    /* fit preview into allocated height and adjust the width */
    pheight = allocation->height - PREVIEW_BORDER_Y;
    pwidth = (pheight * mgp->dwidth) / MAX(mgp->dheight,1);
  }

  psize = MAX(pwidth, pheight);

  if ((allocation->width - PREVIEW_BORDER_X < PREVIEW_SIZE)
  ||  (allocation->height - PREVIEW_BORDER_Y < PREVIEW_SIZE)
  ||  (pwidth < mgp->pwidth)    /* TOTAL SHRINK WORKAROUND */
  ||  (pheight < mgp->pheight)  /* TOTAL SHRINK WORKAROUND */
  )
  {
    /* do not allow shrinks smaller than PREVIEW_SIZE */
    if ( mgp->dwidth > mgp->dheight )
    {
      pheight = mgp->dheight * PREVIEW_SIZE / mgp->dwidth;
      pheight = MAX (1, pheight);
      pwidth  = PREVIEW_SIZE;
    }
    else
    {
      pwidth  = mgp->dwidth * PREVIEW_SIZE / mgp->dheight;
      pwidth  = MAX (1, pwidth);
      pheight = PREVIEW_SIZE;
    }
    psize = PREVIEW_SIZE;
    ignore_inital_cnt = 1;
    fit_initial = TRUE;
  }

  actual_check_size = (GAP_MOV_CHECK_SIZE * psize) / PREVIEW_SIZE;

  if(gap_debug)
  {
    printf("allocation w:%d  h:%d  pwidth:%d  pheight:%d   preview: w:%d  h:%d   psize MAX:%d\n"
                      , (int)allocation->width
                      , (int)allocation->height
                      , (int)pwidth
                      , (int)pheight
                      , (int)mgp->pwidth
                      , (int)mgp->pheight
                      , (int)psize
                      );
  }

  if(((pheight / 6) == (mgp->pheight / 6))
  && ((pwidth / 6)  == (mgp->pwidth / 6)))
  {
    /* skip resize if equal size or no significant change (< 6 pixel) */
    if(gap_debug) printf("RET\n");
    return;
  }

  mgp->pwidth = pwidth;
  mgp->pheight = pheight;

  gap_pview_set_size(mgp->pv_ptr
                  , mgp->pwidth
                  , mgp->pheight
                  , actual_check_size
                  );
  mov_upvw_callback (NULL, mgp);

  if(fit_initial)
  {
    mov_fit_initial_shell_window(mgp);
  }

}  /* end mov_pview_size_allocate_callback */



/* -----------------------------------
 * gap_mov_dlg_move_dialog_singleframe
 * -----------------------------------
 * return 0 : OK, got params from the dialog
 *        -1: user has cancelled the dialog
 */
gint
gap_mov_dlg_move_dialog_singleframe(GapMovSingleFrame *singleFramePtr)
{
  static GapArrArg  argv[3];
  gint   l_ii;
  gint   l_ii_frame_phase;
  gint   l_ii_total_frames;

  l_ii = 0;
  gap_arr_arg_init(&argv[l_ii], GAP_ARR_WGT_FILESEL);
  argv[l_ii].label_txt = _("MovePath xmlfile:");
  argv[l_ii].entry_width = 400;
  argv[l_ii].help_txt  = _("Name of the file containing move path paramters and controlpoints in XML format");
  argv[l_ii].text_buf_len = sizeof(singleFramePtr->xml_paramfile);
  argv[l_ii].text_buf_ret = singleFramePtr->xml_paramfile;

  l_ii++;
  l_ii_total_frames = l_ii;
  gap_arr_arg_init(&argv[l_ii], GAP_ARR_WGT_INT_PAIR);
  argv[l_ii].constraint = TRUE;
  argv[l_ii].label_txt = _("Total Frames:");
  argv[l_ii].help_txt  = _("Total number of frames");
  argv[l_ii].int_min   = (gint)1;
  argv[l_ii].int_max   = (gint)MAX(10000, singleFramePtr->total_frames);
  argv[l_ii].int_ret   = (gint)singleFramePtr->total_frames;
  argv[l_ii].has_default = TRUE;
  argv[l_ii].int_default = (gint)50;

  l_ii++;
  l_ii_frame_phase = l_ii;
  gap_arr_arg_init(&argv[l_ii], GAP_ARR_WGT_INT_PAIR);
  argv[l_ii].constraint = TRUE;
  argv[l_ii].label_txt = _("Current Frame:");
  argv[l_ii].help_txt  = _("Curent Frame number (e.g. phase to be phase Total number of frames");
  argv[l_ii].int_min   = (gint)1;
  argv[l_ii].int_max   = (gint)MAX(10000, singleFramePtr->total_frames);
  argv[l_ii].int_ret   = (gint)CLAMP(singleFramePtr->frame_phase, 1, argv[l_ii].int_max);
  argv[l_ii].has_default = TRUE;
  argv[l_ii].int_default = argv[l_ii].int_ret;


  if(TRUE == gap_arr_ok_cancel_dialog( _("Copy Audiofile as Wavefile"),
                                 _("Settings"),
                                  G_N_ELEMENTS(argv), argv))
  {
     singleFramePtr->total_frames = (gint32)(argv[l_ii_total_frames].int_ret);
     singleFramePtr->frame_phase  = (gint32)(argv[l_ii_frame_phase].int_ret);

     return (0);  /* OK */
  }

  return (-1);  /*  dialog cancelled */

}  /* end gap_mov_dlg_move_dialog_singleframe */
