/*  gap_morph_dialog.c
 *
 *  This module handles GAP morph dialog
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
 * version 2.1.0a; 2004/04/18  hof: created
 */

/* define  GAP_MORPH_DEBUG_FEATURES shows both experimental features
 * and testfeatures for developers only.
 */
#undef GAP_MORPH_DEBUG_FEATURES


#include "config.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>

#include <gtk/gtk.h>
#include <libgimp/gimp.h>
#include <libgimp/gimpui.h>

#include "gap_base.h"

#include "gap_colordiff.h"
#include "gap_locate.h"
#include "gap_morph_main.h"
#include "gap_morph_dialog.h"
#include "gap_morph_shape.h"
#include "gap_morph_exec.h"
#include "gap_pdb_calls.h"
#include "gap_pview_da.h"
#include "gap_stock.h"
#include "gap_lib.h"
#include "gap_image.h"
#include "gap_vin.h"
#include "gap_timeconv.h"
#include "gap_thumbnail.h"
#include "gap_arr_dialog.h"


#include "gap-intl.h"

extern int gap_debug;  /* 1 == print debug infos , 0 dont print debug infos */

#define GAP_MORPH_RESPONSE_RESET 1
#define GAP_MORPH_RESPONSE_LOAD  2
#define GAP_MORPH_RESPONSE_SAVE  3
#define GAP_MORPH_RESPONSE_SWAP  4

#define GAP_MORPH_CHECK_SIZE  8
#define GAP_MORPH_PV_WIDTH_DEFAULT  480
#define GAP_MORPH_PV_HEIGHT_DEFAULT 320

#define GAP_MORPH_PREVIEW_WIDTH  "gap_morph_preview_width"
#define GAP_MORPH_PREVIEW_HEIGHT "gap_morph_preview_height"

#define GAP_MORPH_ZOOMSTEP    1.5
#define RADIUS           3
#define RADIUS_SHOW      4
#define GAP_MORPH_MAXGINT32 2147483647

/* for picking: define what is "NEAR" enough in square pixels */
#define GAP_MORPH_PICK_SQR_NEAR_THRESHOLD 25

#define GAP_MORPH_DLG_UPD_REQUEST_NONE         0
#define GAP_MORPH_DLG_UPD_REQUEST_REDRAW       1
#define GAP_MORPH_DLG_UPD_REQUEST_FULL_REFRESH 2

#define GAP_MORPH_KOORD_WPX       1
#define GAP_MORPH_KOORD_WPY       2

/*  event masks for the preview widgets */
#define GAP_MORPH_PVIEW_MASK   GDK_EXPOSURE_MASK | \
                       GDK_BUTTON_PRESS_MASK |\
                       GDK_BUTTON_RELEASE_MASK |\
                       GDK_BUTTON_MOTION_MASK


/* op_mode Radio Buttons */
#define   GAP_MORPH_OP_MODE_SET    0
#define   GAP_MORPH_OP_MODE_MOVE   1
#define   GAP_MORPH_OP_MODE_DELETE 2
#define   GAP_MORPH_OP_MODE_ZOOM   3
#define   GAP_MORPH_OP_MODE_SHOW   4


#define GAP_MORPH_SWAP(ty, x, y)    { ty tmp; tmp = x; x = y; y = tmp; }

static gint32 global_morph_pv_width = GAP_MORPH_PV_WIDTH_DEFAULT;
static gint32 global_morph_pv_height = GAP_MORPH_PV_HEIGHT_DEFAULT;

static void         p_morph_response(GtkWidget *w, gint response_id, GapMorphGUIParams *mgup);
static void         p_upd_widget_values(GapMorphGUIParams *mgup);
static void         p_upd_warp_info_label(GapMorphGUIParams *mgup);
static void         p_generate_outline_shape_workpoints(GapMorphGUIParams *mgup);
static void         p_add_4corner_workpoints(GapMorphGUIParams *mgup);

static void         p_zoom_in(GapMorphSubWin  *swp, gdouble l_x, gdouble l_y);
static void         p_zoom_out(GapMorphSubWin  *swp);
static void         p_fit_zoom_into_pview_size(GapMorphSubWin  *swp);
static void         on_swap_button_pressed_callback(GtkWidget *wgt, GapMorphGUIParams *mgup);
static void         on_fit_zoom_pressed_callback(GtkWidget *wgt, GapMorphSubWin *swp);
static void         p_hvscale_adj_set_limits(GapMorphSubWin *swp);
static void         on_timer_update_job(GapMorphGUIParams *mgup);
static void         p_set_upd_timer_request(GapMorphGUIParams *mgup, gint32 src_request, gint32 dst_request);
static void         on_koord_spinbutton_changed (GtkObject *obj, gint32 koord_id);
static void         on_curr_point_spinbutton_changed(GtkObject *obj, GapMorphGUIParams *mgup);
GapMorphWorkPoint * gap_morph_dlg_new_workpont(gdouble srcx, gdouble srcy, gdouble dstx, gdouble dsty);
static void         p_delete_current_point(GapMorphSubWin  *swp);
static void         p_set_nearest_current_workpoint(GapMorphSubWin  *swp
                                                  , gdouble in_x
                                                  , gdouble in_y
                                                  );
static void         p_set_current_workpoint_no_refresh(GapMorphSubWin  *swp, GapMorphWorkPoint *wp);
static void         p_set_current_workpoint(GapMorphSubWin  *swp, GapMorphWorkPoint *wp);

static void         p_locate_point(gint32  refDrawableId, gint32  targetDrawableId
                               , GapMorphGUIParams *mgup
                               , gdouble rx, gdouble ry
                               , gdouble *tx, gdouble *ty);
static GapMorphWorkPoint * p_add_new_point(GapMorphSubWin  *swp, gdouble in_x, gdouble in_y, gboolean locate_detail);
static void                p_add_new_point_refresh(GapMorphSubWin  *swp, gdouble in_x, gdouble in_y, gboolean locate_detail);
static void                p_refresh_total_points_label(GapMorphGUIParams *mgup);
static GapMorphWorkPoint * p_find_nearest_point(GapMorphSubWin  *swp
                                               , gdouble in_x
                                               , gdouble in_y
                                               , gdouble *ret_sqr_dist
                                               );
static gboolean     p_pick_nearest_point(GapMorphSubWin  *swp, gdouble l_x, gdouble l_y);
static void         p_show_warp_pick_point(GapMorphGUIParams *mgup
                      ,gdouble l_x
                      ,gdouble l_y
                      );
static void         p_draw_workpoints (GapMorphSubWin  *swp);
static void         p_prevw_draw (GapMorphSubWin  *swp);
static void         p_render_zoomed_pview(GapMorphSubWin  *swp);
static gint         on_pview_events (GtkWidget *widget
                                                 , GdkEvent *event
                                                 , GapMorphSubWin  *swp
                                                 );

static void         p_scale_wp_list(GapMorphGUIParams *mgup);
static void         p_imglayer_menu_callback(GtkWidget *widget, GapMorphSubWin *swp);
static gint         p_imglayer_constrain(gint32 image_id, gint32 drawable_id, gpointer data);
static void         p_refresh_layer_menu(GapMorphSubWin *swp, GapMorphGUIParams *mgup);
static void         on_pointcolor_button_changed(GimpColorButton *widget, GimpRGB *pcolor);

static void         on_wp_filesel_destroy   (GtkObject       *object
                                            ,GapMorphGUIParams *mgup);
static void         on_wp_filesel_button_OK_clicked (GtkButton       *button
                                                   ,GapMorphGUIParams *mgup);
static void         on_wp_filesel_button_cancel_clicked (GtkButton   *button
                                                   , GapMorphGUIParams *mgup);
static void         p_create_wp_filesel (GapMorphGUIParams *mgup
                                        ,GdkEventButton    *bevent
                                        ,gboolean save_mode);

static void         on_wp_save_button_clicked (GtkButton        *button
                                             ,GdkEventButton    *bevent
                                             ,GapMorphGUIParams *mgup);
static void         on_wp_load_button_clicked (GtkButton        *button
                                             ,GdkEventButton    *bevent
                                             ,GapMorphGUIParams *mgup);
static void         on_wp_shape_button_clicked (GtkButton *button
                                             ,GdkEventButton  *bevent
                                             ,GapMorphGUIParams *mgup);
static void         on_hvscale_changed_callback(GtkObject *obj, GapMorphSubWin *swp);

static void         on_show_lines_toggled_callback(GtkWidget *widget, GapMorphGUIParams *mgup);
static void         on_use_quality_wp_selection_toggled_callback(GtkWidget *widget, GapMorphGUIParams *mgup);
static void         on_use_gravity_toggled_callback(GtkWidget *widget, GapMorphGUIParams *mgup);
static void         on_have_workpointsets_toggled_callback(GtkWidget *widget, GapMorphGUIParams *mgup);
static void         on_create_tween_layers_toggled_callback(GtkWidget *widget, GapMorphGUIParams *mgup);

static void         on_radio_op_mode_callback(GtkWidget *widget, gint32 op_mode);
static void         on_radio_render_mode_callback(GtkWidget *widget, gint32 op_mode);
static void         p_radio_create_op_mode(GtkWidget *table, int row, int col, GapMorphGUIParams *mgup);
static void         p_radio_create_render_mode(GtkWidget *table, int row, int col, GapMorphGUIParams *mgup);

static GtkWidget *  p_create_subwin(GapMorphSubWin *swp
                                   , const char *title
                                   , GapMorphGUIParams *mgup
                                   , GtkWidget *master_hbox
                                   );
static void         gap_morph_create_dialog(GapMorphGUIParams *mgup);




/* -----------------------------
 * p_morph_response
 * -----------------------------
 */
static void
p_morph_response(GtkWidget *w, gint response_id, GapMorphGUIParams *mgup)
{
  GtkWidget *dialog;

  switch (response_id)
  {
    case GAP_MORPH_RESPONSE_RESET:
      mgup->cancelWorkpointGeneration = TRUE;
      gap_morph_exec_free_workpoint_list(&mgup->mgpp->master_wp_list);
      p_add_4corner_workpoints(mgup);
      p_set_upd_timer_request(mgup
                         ,GAP_MORPH_DLG_UPD_REQUEST_FULL_REFRESH
                         ,GAP_MORPH_DLG_UPD_REQUEST_FULL_REFRESH
                         );
      p_refresh_total_points_label(mgup);
      mgup->mgpp->tween_steps = 1;
      mgup->num_shapepoints = 16;
      mgup->mgpp->affect_radius = 100;
      mgup->mgpp->gravity_intensity = 2.0;
      mgup->mgpp->use_quality_wp_selection = FALSE;
      mgup->mgpp->use_gravity = FALSE;
      mgup->mgpp->create_tween_layers = TRUE;
      mgup->mgpp->have_workpointsets = FALSE;
      p_upd_widget_values(mgup);
      break;
    case GAP_MORPH_RESPONSE_LOAD:
      on_wp_load_button_clicked(NULL, NULL, mgup);
      break;
    case GAP_MORPH_RESPONSE_SAVE:
      on_wp_save_button_clicked(NULL, NULL, mgup);
      break;
    case GAP_MORPH_RESPONSE_SWAP:
      on_swap_button_pressed_callback(NULL, mgup);
      break;
    case GTK_RESPONSE_OK:
      if(mgup)
      {
        mgup->cancelWorkpointGeneration = TRUE;
        if (GTK_WIDGET_VISIBLE (mgup->shell))
        {
          gtk_widget_hide (mgup->shell);
        }
        mgup->run_flag = TRUE;
      }

    default:
      dialog = NULL;
      if(mgup)
      {
        mgup->cancelWorkpointGeneration = TRUE;
        dialog = mgup->shell;
        if(dialog)
        {
          mgup->shell = NULL;
          gtk_widget_destroy (dialog);
        }
      }
      gtk_main_quit ();
      break;
  }
}  /* end p_morph_response */


/* --------------------------------
 * p_upd_widget_values
 * --------------------------------
 */
static void
p_upd_widget_values(GapMorphGUIParams *mgup)
{
  if(mgup)
  {
       gtk_adjustment_set_value(
            GTK_ADJUSTMENT(mgup->tween_steps_spinbutton_adj)
           ,mgup->mgpp->tween_steps);
       gtk_adjustment_set_value(
            GTK_ADJUSTMENT(mgup->num_shapepoints_adj)
           ,mgup->num_shapepoints);
       gtk_adjustment_set_value(
            GTK_ADJUSTMENT(mgup->affect_radius_spinbutton_adj)
           ,mgup->mgpp->affect_radius);
       gtk_adjustment_set_value(
            GTK_ADJUSTMENT(mgup->locate_edge_threshold_spinbutton_adj)
           ,mgup->mgpp->edgeColordiffThreshold);
       gtk_adjustment_set_value(
            GTK_ADJUSTMENT(mgup->locate_radius_spinbutton_adj)
           ,mgup->mgpp->locateDetailMoveRadius);
       gtk_adjustment_set_value(
            GTK_ADJUSTMENT(mgup->locate_shape_radius_spinbutton_adj)
           ,mgup->mgpp->locateDetailShapeRadius);
       gtk_adjustment_set_value(
            GTK_ADJUSTMENT(mgup->gravity_intensity_spinbutton_adj)
           ,mgup->mgpp->gravity_intensity);
       gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (mgup->use_quality_wp_selection_checkbutton)
                                    , mgup->mgpp->use_quality_wp_selection);
       gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (mgup->use_gravity_checkbutton)
                                    , mgup->mgpp->use_gravity);
       gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (mgup->create_tween_layers_checkbutton)
                                    , mgup->mgpp->create_tween_layers);
       gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (mgup->have_workpointsets_checkbutton)
                                    , mgup->mgpp->have_workpointsets);

  }
}  /* end p_upd_widget_values */


/* -----------------------------
 * p_upd_warp_info_label
 * -----------------------------
 * Debug info: show weight and distance of current point
 */
static void
p_upd_warp_info_label(GapMorphGUIParams *mgup)
{
  GapMorphWorkPoint *wp;

  if(mgup)
  {
    wp = mgup->dst_win.curr_wp;
    if((mgup->warp_info_label) && (wp))
    {
      gchar msg[100];

      g_snprintf(&msg[0], sizeof(msg)
                , "si:%d rl:%d d:%.1f w:%.4f"
                , (int)wp->sek_idx
                , (int)wp->xy_relation
                , (float)sqrt(wp->sqr_dist)
                , (float)wp->warp_weight
                );
      gtk_label_set_text(GTK_LABEL(mgup->warp_info_label), msg);
    }
  }

}  /* end p_upd_warp_info_label */




/* -----------------------------------
 * p_generate_outline_shape_workpoints
 * -----------------------------------
 */
static void
p_generate_outline_shape_workpoints(GapMorphGUIParams *mgup)
{
  mgup->mgpp->numOutlinePoints = mgup->num_shapepoints;
  gap_morph_shape_generate_outline_workpoints(mgup->mgpp);

}  /* end p_generate_outline_shape_workpoints */

/* -----------------------------
 * p_add_4corner_workpoints
 * -----------------------------
 */
static void
p_add_4corner_workpoints(GapMorphGUIParams *mgup)
{
  GapMorphWorkPoint *wp;
  gint32        dst_drawable_id;
  gint32        src_drawable_id;
  gdouble sx[4];
  gdouble sy[4];
  gdouble dx[4];
  gdouble dy[4];
  gint ii;

  src_drawable_id = mgup->mgpp->osrc_layer_id;
  dst_drawable_id = mgup->mgpp->fdst_layer_id;

  sx[0] = gimp_drawable_width(src_drawable_id) -1;
  sy[0] = gimp_drawable_height(src_drawable_id) -1;
  dx[0] = gimp_drawable_width(dst_drawable_id) -1;
  dy[0] = gimp_drawable_height(dst_drawable_id) -1;

  sx[1] = gimp_drawable_width(src_drawable_id)  -1;
  sy[1] = 0;
  dx[1] = gimp_drawable_width(dst_drawable_id)  -1;
  dy[1] = 0;

  sx[2] = 0;
  sy[2] = gimp_drawable_height(src_drawable_id) -1;
  dx[2] = 0;
  dy[2] = gimp_drawable_height(dst_drawable_id) -1;

  sx[3] = 0;
  sy[3] = 0;
  dx[3] = 0;
  dy[3] = 0;


  for(ii=0; ii<4; ii++)
  {
    wp = gap_morph_dlg_new_workpont(sx[ii] ,sy[ii] ,dx[ii] ,dy[ii]);
    wp->next = mgup->mgpp->master_wp_list;
    mgup->mgpp->master_wp_list = wp;
  }

}  /* end p_add_4corner_workpoints */


/* -----------------------------
 * p_zoom_in
 * -----------------------------
 * zoom into preview where x/y is the new center
 */
static void
p_zoom_in(GapMorphSubWin  *swp, gdouble l_x, gdouble l_y)
{
  GapMorphGUIParams *mgup;
  gdouble width;
  gdouble height;

  mgup = (GapMorphGUIParams *)swp->mgup;


  swp->zoom = (swp->zoom / GAP_MORPH_ZOOMSTEP);
  if(swp->zoom == 0)
  {
    swp->zoom = 1.0;
  }

  width = swp->zoom * swp->pv_ptr->pv_width;
  height = swp->zoom * swp->pv_ptr->pv_height;

  swp->offs_x = l_x - (width / 2.0);
  swp->offs_y = l_y - (height / 2.0);


  p_set_upd_timer_request(mgup
                         ,GAP_MORPH_DLG_UPD_REQUEST_FULL_REFRESH
                         ,GAP_MORPH_DLG_UPD_REQUEST_FULL_REFRESH
                         );
}  /* end p_zoom_in */

/* -----------------------------
 * p_zoom_out
 * -----------------------------
 * zoom into preview where x/y is the new center
 */
static void
p_zoom_out(GapMorphSubWin  *swp)
{
  GapMorphGUIParams *mgup;

  swp->zoom = (swp->zoom * GAP_MORPH_ZOOMSTEP);
  if(swp->zoom > swp->max_zoom)
  {
    swp->zoom = swp->max_zoom;
  }
  mgup = (GapMorphGUIParams *)swp->mgup;

  swp->offs_x /= GAP_MORPH_ZOOMSTEP;
  swp->offs_y /= GAP_MORPH_ZOOMSTEP;

  p_set_upd_timer_request(mgup
                         ,GAP_MORPH_DLG_UPD_REQUEST_FULL_REFRESH
                         ,GAP_MORPH_DLG_UPD_REQUEST_FULL_REFRESH
                         );
}  /* end p_zoom_out */


/* ----------------------------------
 * p_fit_zoom_into_pview_size
 * ----------------------------------
 */
static void
p_fit_zoom_into_pview_size(GapMorphSubWin  *swp)
{
  swp->zoom = 1.0;

  if(swp->layer_id_ptr)
  {
    if(*swp->layer_id_ptr >= 0)
    {
      gdouble width;
      gdouble height;
      gdouble pv_width;
      gdouble pv_height;
      gdouble pv_pixelsize;
      gint32  src_request;
      gint32  dst_request;


      width = (gdouble) gimp_drawable_width(*swp->layer_id_ptr);
      height = (gdouble) gimp_drawable_height(*swp->layer_id_ptr);


      pv_pixelsize = MAX(global_morph_pv_width, global_morph_pv_height);

      /*
       * Resize the greater one of dwidth and dheight to PREVIEW_SIZE
       */
      if ( width > height )
      {
        /* landscape */
        pv_height = height * pv_pixelsize / width;
        pv_width = pv_pixelsize;

        swp->zoom = width / (gdouble)pv_width;
      }
      else
      {
        /* portrait */
        pv_width = width * pv_pixelsize / height;
        pv_height = pv_pixelsize;
        swp->zoom = height / (gdouble)pv_height;
      }
      swp->max_zoom = swp->zoom;

      if(gap_debug)
      {
        printf("p_fit_zoom_into_pview_size: width: %d height:%d ## ZOOM:%f\n"
              , (int)width
              , (int)height
              , (float)swp->zoom)
              ;
      }


      if((pv_width != swp->pv_ptr->pv_width)
      || (pv_height != swp->pv_ptr->pv_height))
      {
        gap_pview_set_size(swp->pv_ptr
                      , pv_width
                      , pv_height
                      , MAX(GAP_MORPH_CHECK_SIZE, (pv_pixelsize / 16))
                      );
      }

      if(swp->src_flag)
      {
        src_request = GAP_MORPH_DLG_UPD_REQUEST_FULL_REFRESH;
        dst_request = GAP_MORPH_DLG_UPD_REQUEST_NONE;
      }
      else
      {
        src_request = GAP_MORPH_DLG_UPD_REQUEST_NONE;
        dst_request = GAP_MORPH_DLG_UPD_REQUEST_FULL_REFRESH;
      }
      p_set_upd_timer_request(swp->mgup, src_request, dst_request);
    }
  }
}  /* end p_fit_zoom_into_pview_size */

/* -------------------------------
 * on_swap_button_pressed_callback
 * -------------------------------
 */
static void
on_swap_button_pressed_callback(GtkWidget *wgt, GapMorphGUIParams *mgup)
{
  GapMorphWorkPoint *wp;
  gint32 src_layer_width;
  gint32 src_layer_height;
  gint32 dst_layer_width;
  gint32 dst_layer_height;

  if((mgup->mgpp->osrc_layer_id < 0)
  || (mgup->mgpp->fdst_layer_id < 0))
  {
    return;
  }
  if(mgup->workpointGenerationBusy == TRUE)
  {
    return;
  }

  src_layer_width  = gimp_drawable_width(mgup->mgpp->osrc_layer_id);
  src_layer_height = gimp_drawable_height(mgup->mgpp->osrc_layer_id);
  dst_layer_width  = gimp_drawable_width(mgup->mgpp->fdst_layer_id);
  dst_layer_height = gimp_drawable_height(mgup->mgpp->fdst_layer_id);

  GAP_MORPH_SWAP(gint32
                ,mgup->mgpp->osrc_layer_id
                ,mgup->mgpp->fdst_layer_id
                )

  GAP_MORPH_SWAP(gint32
                ,mgup->src_win.offs_x
                ,mgup->dst_win.offs_x
                )

  GAP_MORPH_SWAP(gint32
                ,mgup->src_win.offs_y
                ,mgup->dst_win.offs_y
                )

  GAP_MORPH_SWAP(gdouble
                ,mgup->src_win.max_zoom
                ,mgup->dst_win.max_zoom
                )

  GAP_MORPH_SWAP(gdouble
                ,mgup->src_win.zoom
                ,mgup->dst_win.zoom
                )

  for(wp = mgup->mgpp->master_wp_list; wp != NULL; wp = (GapMorphWorkPoint *)wp->next)
  {
     GAP_MORPH_SWAP(gdouble
                   ,wp->osrc_x
                   ,wp->fdst_x
                   )
     GAP_MORPH_SWAP(gdouble
                   ,wp->osrc_y
                   ,wp->fdst_y
                   )
  }

  if((src_layer_width != dst_layer_width)
  || (src_layer_height != dst_layer_height))
  {
    /* scale is needed because src and dst layer differ in size */
    p_scale_wp_list(mgup);
  }

  gimp_int_combo_box_set_active(GIMP_INT_COMBO_BOX(mgup->src_win.combo)
                               , mgup->mgpp->osrc_layer_id);
  gimp_int_combo_box_set_active(GIMP_INT_COMBO_BOX(mgup->dst_win.combo)
                               , mgup->mgpp->fdst_layer_id);

  p_set_upd_timer_request(mgup
                         ,GAP_MORPH_DLG_UPD_REQUEST_FULL_REFRESH
                         ,GAP_MORPH_DLG_UPD_REQUEST_FULL_REFRESH
                         );

}  /* end on_swap_button_pressed_callback */

/* ------------------------------
 * on_fit_zoom_pressed_callback
 * ------------------------------
 */
static void
on_fit_zoom_pressed_callback(GtkWidget *wgt, GapMorphSubWin *swp)
{
  p_fit_zoom_into_pview_size(swp);
}  /* end on_fit_zoom_pressed_callback */


/* ---------------------------------
 * p_hvscale_adj_set_limits
 * ---------------------------------
 */
static void
p_hvscale_adj_set_limits(GapMorphSubWin *swp)
{
  gdouble upper_limit;
  gdouble lower_limit;
  gdouble page_increment;
  gdouble page_size;
  gdouble value;

  gdouble   fwidth;
  gdouble   fheight;
  gdouble width;
  gdouble height;

  if(swp == NULL) { return; }
  if(swp->vscale_adj == NULL) { return; }
  if(swp->hscale_adj == NULL) { return; }
  if(*swp->layer_id_ptr < 0)  { return; }

  width = (gdouble) gimp_drawable_width(*swp->layer_id_ptr);
  height = (gdouble) gimp_drawable_height(*swp->layer_id_ptr);

  fwidth  = swp->zoom * (gdouble)swp->pv_ptr->pv_width;
  fheight = swp->zoom * (gdouble)swp->pv_ptr->pv_height;




  lower_limit = 0.0;
  upper_limit = width;
  page_size = (gdouble)fwidth;
  page_increment = (gdouble)(page_size / 2);

  value = swp->offs_x;

  if(gap_debug)
  {
    printf("\np_hvscale_adj_set_limits: ###\nfwidth : %d  width:%d\n", (int)fwidth ,(int)width);
    printf("lower_limit %f\n", (float)lower_limit );
    printf("upper_limit %f\n", (float)upper_limit );
    printf("page_size %f\n", (float) page_size);
    printf("page_increment %f\n", (float)page_increment );
    printf("value              %f\n", (float)value );
  }

  GTK_ADJUSTMENT(swp->hscale_adj)->lower = lower_limit;
  GTK_ADJUSTMENT(swp->hscale_adj)->upper = upper_limit;
  GTK_ADJUSTMENT(swp->hscale_adj)->page_increment = page_increment;
  GTK_ADJUSTMENT(swp->hscale_adj)->value = MIN(value, upper_limit);
  GTK_ADJUSTMENT(swp->hscale_adj)->page_size = page_size;


  lower_limit = 0.0;
  upper_limit = height;
  page_size = (gdouble)fheight;
  page_increment = (gdouble)(page_size / 2);

  value = swp->offs_y;

  if(gap_debug)
  {
    printf("\n fheight : %d  height:%d\n", (int)fheight ,(int)height);
    printf("lower_limit %f\n", (float)lower_limit );
    printf("upper_limit %f\n", (float)upper_limit );
    printf("page_size %f\n", (float) page_size);
    printf("page_increment %f\n", (float)page_increment );
    printf("value              %f\n", (float)value );
  }

  GTK_ADJUSTMENT(swp->vscale_adj)->lower = lower_limit;
  GTK_ADJUSTMENT(swp->vscale_adj)->upper = upper_limit;
  GTK_ADJUSTMENT(swp->vscale_adj)->page_increment = page_increment;
  GTK_ADJUSTMENT(swp->vscale_adj)->value = MIN(value, upper_limit);
  GTK_ADJUSTMENT(swp->vscale_adj)->page_size = page_size;

  gtk_widget_queue_draw(swp->hscale);
  gtk_widget_queue_draw(swp->vscale);
}  /* end p_hvscale_adj_set_limits */


/* -------------------
 * on_timer_update_job
 * -------------------
 */
static void
on_timer_update_job(GapMorphGUIParams *mgup)
{
  if(gap_debug) printf("\non_timer_update_job: START\n");

  if(mgup)
  {
    if(mgup->src_win.upd_request != GAP_MORPH_DLG_UPD_REQUEST_NONE)
    {
      if(mgup->src_win.upd_request == GAP_MORPH_DLG_UPD_REQUEST_FULL_REFRESH)
      {
        p_render_zoomed_pview(&mgup->src_win);
      }
      else
      {
        p_prevw_draw(&mgup->src_win);
      }
      mgup->src_win.upd_request = GAP_MORPH_DLG_UPD_REQUEST_NONE;
    }

    if(mgup->dst_win.upd_request  != GAP_MORPH_DLG_UPD_REQUEST_NONE)
    {
      if(mgup->dst_win.upd_request == GAP_MORPH_DLG_UPD_REQUEST_FULL_REFRESH)
      {
        p_render_zoomed_pview(&mgup->dst_win);
      }
      else
      {
        p_prevw_draw(&mgup->dst_win);
      }
      mgup->dst_win.upd_request = GAP_MORPH_DLG_UPD_REQUEST_NONE;
    }



    if(mgup->upd_timertag >= 0)
    {
      g_source_remove(mgup->upd_timertag);
    }
    mgup->upd_timertag = -1;

  }
}  /* end on_timer_update_job */


/* -----------------------
 * p_set_upd_timer_request
 * -----------------------
 */
static void
p_set_upd_timer_request(GapMorphGUIParams *mgup, gint32 src_request, gint32 dst_request)
{
  mgup->src_win.upd_request = MAX(mgup->src_win.upd_request, src_request);
  mgup->dst_win.upd_request = MAX(mgup->dst_win.upd_request, dst_request);

  /* add a timer with 16 millesec delay
   * (no need to do that if the there is already a pending
   *  timer request. if there is no pending request upd_timertag == -1)
   */
  if(mgup->upd_timertag < 0)
  {
    mgup->upd_timertag = (gint32) g_timeout_add(16, (GtkFunction)on_timer_update_job, mgup);
  }
}  /* end p_set_upd_timer_request */



/* -----------------------------
 * on_koord_spinbutton_changed
 * -----------------------------
 * because the val_ptr changes to follow
 * current workpoint we use the koord_id
 * and have to find the val_ptr at runtime
 * new at each call
 */
static void
on_koord_spinbutton_changed             (GtkObject       *obj,
                                         gint32          koord_id)
{
  GapMorphSubWin  *swp;
  gdouble         *val_ptr;
  gdouble         value;

  swp = g_object_get_data( G_OBJECT(obj), "swp" );

  val_ptr = NULL;
  switch(koord_id)
  {
    case GAP_MORPH_KOORD_WPX:
      val_ptr = swp->wpx_ptr;
      break;
    case GAP_MORPH_KOORD_WPY:
      val_ptr = swp->wpy_ptr;
      break;
    default:
      return;
  }

  value = (gint32)GTK_ADJUSTMENT(obj)->value;
  if(value != *val_ptr)
  {
    gimp_double_adjustment_update(GTK_ADJUSTMENT(obj), val_ptr);
    if(swp)
    {
      /* setup a timer request for pview redraw after 16 millisec
       * to prevent multiple updates
       */
      p_set_upd_timer_request((GapMorphGUIParams *)swp->mgup
                             , GAP_MORPH_DLG_UPD_REQUEST_REDRAW  /* src_request */
                             , GAP_MORPH_DLG_UPD_REQUEST_REDRAW  /* dst_request */
                             );
    }
  }
}  /* end on_koord_spinbutton_changed */


/* -----------------------------
 * on_curr_point_spinbutton_changed
 * -----------------------------
 * set current workpoint to the n.the list element
 * where n == the entered number in the spinbutton widget
 */
static void
on_curr_point_spinbutton_changed             (GtkObject       *obj,
                                         GapMorphGUIParams *mgup)
{
  gdouble         value;
  gint32          l_idx;
  GapMorphWorkPoint *wp;
  GapMorphGlobalParams  *mgpp;

  if(mgup == NULL)
  {
    return;
  }

  mgpp = mgup->mgpp;
  if(mgpp == NULL)
  {
    return;
  }


  value = (gint32)GTK_ADJUSTMENT(obj)->value;

  /* find the n.th workpoint in the list */
  l_idx = 1;
  for(wp = mgpp->master_wp_list; wp != NULL; wp= wp->next)
  {
    if(l_idx == value)
    {
      break;
    }
    l_idx++;
  }

  if(wp == NULL)
  {
    gtk_adjustment_set_value(GTK_ADJUSTMENT(obj), (gdouble)(l_idx-1));
  }
  else
  {
    if(wp != mgup->src_win.curr_wp)
    {
      p_set_current_workpoint(&mgup->src_win, wp);
    }
  }
}  /* end on_curr_point_spinbutton_changed */


/* -----------------------------
 * gap_morph_dlg_new_workpont
 * -----------------------------
 */
GapMorphWorkPoint *
gap_morph_dlg_new_workpont(gdouble srcx, gdouble srcy, gdouble dstx, gdouble dsty)
{
  GapMorphWorkPoint *wp;

  wp = gap_morph_shape_new_workpont(srcx, srcy, dstx, dsty);
  return(wp);

}  /* end gap_morph_dlg_new_workpont */

/* -----------------------------
 * p_delete_current_point
 * -----------------------------
 * delete the current workpoint.
 * but: never delete the last workpoint in the list.
 */
static void
p_delete_current_point(GapMorphSubWin  *swp)
{
  GapMorphGUIParams *mgup;
  GapMorphWorkPoint *wp;
  gdouble delx;
  gdouble dely;


  mgup = (GapMorphGUIParams *)swp->mgup;

  if(mgup->mgpp->master_wp_list->next)
  {
    GapMorphWorkPoint *wp_prev;

    wp_prev = NULL;
    for(wp = mgup->mgpp->master_wp_list; wp != NULL; wp = (GapMorphWorkPoint *)wp->next)
    {
      if(wp == mgup->src_win.curr_wp)
      {
        if(swp->src_flag)
        {
          delx = wp->osrc_x;
          dely = wp->osrc_y;
        }
        else
        {
          delx = wp->fdst_x;
          dely = wp->fdst_y;
        }

        if(wp_prev == NULL)
        {
          /* delete the 1.st workpoint */
          wp = mgup->mgpp->master_wp_list->next;
          g_free(mgup->mgpp->master_wp_list);
          mgup->mgpp->master_wp_list = wp;
          p_set_nearest_current_workpoint(swp, delx, dely);
          return;
        }
        /* delete a workpoint that is not the 1.st one */
        wp_prev->next = wp->next;
        g_free(wp);
        p_set_nearest_current_workpoint(swp, delx, dely);
        break;
      }
      wp_prev = wp;
    }
  }

  p_refresh_total_points_label(mgup);

}  /* end p_delete_current_point */


/* -----------------------------
 * p_set_nearest_current_workpoint
 * -----------------------------
 * set reference pointers to the current workpoint
 * always for both (src_win and dst_win)
 */
static void
p_set_nearest_current_workpoint(GapMorphSubWin  *swp, gdouble in_x, gdouble in_y)
{
  GapMorphWorkPoint *wp;
  gdouble ret_sqr_dist;

  wp = p_find_nearest_point(swp, in_x, in_y, &ret_sqr_dist);
  if(wp)
  {
    p_set_current_workpoint(swp, wp);
  }
}  /*end p_set_nearest_current_workpoint */

/* -----------------------------
 * p_set_current_workpoint_no_refresh
 * -----------------------------
 * set reference pointers to the current workpoint
 * always for both (src_win and dst_win)
 */
static void
p_set_current_workpoint_no_refresh(GapMorphSubWin  *swp, GapMorphWorkPoint *wp_cur)
{
  GapMorphGUIParams *mgup;
  GapMorphWorkPoint *wp;
  gint32             l_idx;

  mgup = (GapMorphGUIParams *)swp->mgup;
  if(wp_cur == NULL)
  {
    return;
  }
  mgup->src_win.curr_wp = wp_cur;
  mgup->dst_win.curr_wp = wp_cur;

  mgup->src_win.wpx_ptr = &wp_cur->osrc_x;
  mgup->src_win.wpy_ptr = &wp_cur->osrc_y;
  mgup->dst_win.wpx_ptr = &wp_cur->fdst_x;
  mgup->dst_win.wpy_ptr = &wp_cur->fdst_y;

  if((mgup->mgpp == NULL) || (mgup->curr_point_spinbutton_adj == NULL))
  {
    return;
  }

  p_upd_warp_info_label(mgup);

  /* find the index of the current workpoint in the master list */
  l_idx = 1;
  for(wp = mgup->mgpp->master_wp_list; wp != NULL; wp= wp->next)
  {
    if(wp == wp_cur)
    {
      gtk_adjustment_set_value(GTK_ADJUSTMENT(mgup->curr_point_spinbutton_adj), (gdouble)(l_idx));
      break;
      return;
    }
    l_idx++;
  }

}  /* end p_set_current_workpoint_no_refresh */


/* -----------------------------
 * p_set_current_workpoint
 * -----------------------------
 * set reference pointers to the current workpoint
 * always for both (src_win and dst_win)
 */
static void
p_set_current_workpoint(GapMorphSubWin  *swp, GapMorphWorkPoint *wp_cur)
{
  GapMorphGUIParams *mgup;
  gint wgt_count;

  mgup = (GapMorphGUIParams *)swp->mgup;
  if(wp_cur == NULL)
  {
    return;
  }

  p_set_current_workpoint_no_refresh(swp, wp_cur);
  if(swp->startup_flag)
  {
    return;
  }
  wgt_count = 0;
  if(mgup->src_win.x_spinbutton_adj)
  {
    wgt_count++;
  }

  if(mgup->src_win.y_spinbutton_adj)
  {
    wgt_count++;
  }

  if(mgup->dst_win.x_spinbutton_adj)
  {
    wgt_count++;
  }

  if(mgup->dst_win.y_spinbutton_adj)
  {
    wgt_count++;
  }

  if(wgt_count >= 4)
  {
    /* set adjustment values and start timer request
     * but only if we already have all
     * reqired (4) widgets initialized
     * (otherwise we are in startup and may crash when timer fires too early)
     */
    gtk_adjustment_set_value(GTK_ADJUSTMENT(mgup->src_win.x_spinbutton_adj), wp_cur->osrc_x);
    gtk_adjustment_set_value(GTK_ADJUSTMENT(mgup->src_win.y_spinbutton_adj), wp_cur->osrc_y);
    gtk_adjustment_set_value(GTK_ADJUSTMENT(mgup->dst_win.x_spinbutton_adj), wp_cur->fdst_x);
    gtk_adjustment_set_value(GTK_ADJUSTMENT(mgup->dst_win.y_spinbutton_adj), wp_cur->fdst_y);

    p_set_upd_timer_request(mgup
                           ,GAP_MORPH_DLG_UPD_REQUEST_REDRAW
                           ,GAP_MORPH_DLG_UPD_REQUEST_REDRAW
                           );
  }
}  /* end p_set_current_workpoint */

/* -----------------------------
 * p_locate_point
 * -----------------------------
 * locate point coordinate in the other drawable
 * (by searching for best matching shape within move radius)
 */
static void
p_locate_point(gint32  refDrawableId, gint32  targetDrawableId
  , GapMorphGUIParams *mgup
  , gdouble rx, gdouble ry
  , gdouble *tx, gdouble *ty)
{
  gint32  refX;
  gint32  refY;
  gint32  targetX;
  gint32  targetY;
  gdouble colordiff;

  refX = rx;
  refY = ry;

 
  colordiff = gap_locateDetailWithinRadius(refDrawableId
                  , refX
                  , refY
                  , mgup->mgpp->locateDetailShapeRadius
                  , targetDrawableId
                  , mgup->mgpp->locateDetailMoveRadius
                  , mgup->mgpp->locateColordiffThreshold
                  , &targetX
                  , &targetY
                  );
  if(gap_debug)
  {
    printf("p_locate_point: x/y: %d / %d MoveRadius:%d ShapeRadius:%d Threshold:%.5f colordiff:%.5f\n"
          , (int)refX
          , (int)refY
          , (int)mgup->mgpp->locateDetailMoveRadius
          , (int)mgup->mgpp->locateDetailShapeRadius
          , (float)mgup->mgpp->locateColordiffThreshold
          , (float)colordiff
          );
          
  }

  if(colordiff < mgup->mgpp->locateColordiffThreshold)
  {
    /* found a sufficient matching shape in the target drawable
     * at targetX/Y coordinate. Overwrite tx and ty with this matching point coordinate
     */
    *tx = targetX;
    *ty = targetY;


    if(gap_debug)
    {
      printf("p_locate_point: SUCCESS, found matching shape! tx:%d ty:%d\n"
         ,(int)targetX
         ,(int)targetY
         );
    }
  }

}  /* end p_locate_point */


/* -----------------------------
 * p_add_new_point
 * -----------------------------
 * add a new point at in_x/in_y
 * and calculate the other coordinates
 * (using the offsets of the nearest existing point
 *  as guess)
 */
static GapMorphWorkPoint *
p_add_new_point(GapMorphSubWin  *swp, gdouble in_x, gdouble in_y, gboolean locate_detail)
{
  GapMorphGUIParams *mgup;
  GapMorphSubWin    *swp_other;
  GapMorphWorkPoint *wp_near;
  GapMorphWorkPoint *wp;
  gdouble            srcx;
  gdouble            srcy;
  gdouble            dstx;
  gdouble            dsty;
  gdouble            ret_sqr_dist;
  gdouble wo, ho, w, h;

  mgup = (GapMorphGUIParams *)swp->mgup;
  wo = 1.0;
  ho = 1.0;
  w  = 1.0;
  h  = 1.0;

  /* set the other point with same offset as the nearest available point
   */
  wp_near = p_find_nearest_point(swp, in_x, in_y, &ret_sqr_dist);
  if(swp->src_flag)
  {
    swp_other = &mgup->dst_win;
    if(*swp_other->layer_id_ptr >= 0)
    {
      wo = gimp_drawable_width(*swp_other->layer_id_ptr);
      w  = gimp_drawable_width(*swp->layer_id_ptr);
      ho = gimp_drawable_height(*swp_other->layer_id_ptr);
      h  = gimp_drawable_height(*swp->layer_id_ptr);
    }

    srcx = in_x;
    srcy = in_y;
    
    
    if(mgup->op_mode == GAP_MORPH_OP_MODE_MOVE)
    {
      dstx = (srcx) * w / MAX(wo,1);
      dsty = (srcy) * h / MAX(ho,1);
    }
    else
    {
      dstx = (srcx + (wp_near->fdst_x - wp_near->osrc_x)) * w / MAX(wo,1);
      dsty = (srcy + (wp_near->fdst_y - wp_near->osrc_y)) * h / MAX(ho,1);
    }

    if(locate_detail)
    {
      p_locate_point(*swp->layer_id_ptr          /* reference DrawableId  */
                    , *swp_other->layer_id_ptr   /* target DrawableId  */
                    , mgup
                    , srcx, srcy                 /* reference coordinates */
                    , &dstx, &dsty               /* OUT: target coordinates */
                    );
    }


  }
  else
  {
    swp_other = &mgup->src_win;
    if(*swp_other->layer_id_ptr >= 0)
    {
      wo = gimp_drawable_width(*swp_other->layer_id_ptr);
      w  = gimp_drawable_width(*swp->layer_id_ptr);
      ho = gimp_drawable_height(*swp_other->layer_id_ptr);
      h  = gimp_drawable_height(*swp->layer_id_ptr);
    }
    dstx = in_x;
    dsty = in_y;
    if(mgup->op_mode == GAP_MORPH_OP_MODE_MOVE)
    {
      srcx = (dstx) * w / MAX(wo,1);
      srcy = (dsty) * h / MAX(ho,1);
    }
    else
    {
      srcx = (dstx + (wp_near->osrc_x - wp_near->fdst_x)) * w / MAX(wo,1);
      srcy = (dsty + (wp_near->osrc_y - wp_near->fdst_y)) * h / MAX(ho,1);
    }

    if(locate_detail)
    {
      p_locate_point(*swp->layer_id_ptr          /* reference DrawableId  */
                    , *swp_other->layer_id_ptr   /* target DrawableId  */
                    , mgup
                    , dstx,  dsty                 /* reference coordinates */
                    , &srcx, &srcy                /* OUT: target coordinates */
                    );
    }
  }

  wp = gap_morph_dlg_new_workpont(srcx, srcy, dstx, dsty);

  /* add new workpoint as 1st listelement */
  wp->next = mgup->mgpp->master_wp_list;
  mgup->mgpp->master_wp_list = wp;

  return(wp);

}  /* end p_add_new_point */

/* -----------------------------
 * p_add_new_point_refresh
 * -----------------------------
 * add a new point at in_x/in_y
 * and set the newly added point as current point
 * (this also sets refresh request)
 */
static void
p_add_new_point_refresh(GapMorphSubWin  *swp, gdouble in_x, gdouble in_y, gboolean locate_detail)
{
  GapMorphGUIParams *mgup;
  GapMorphWorkPoint *wp;

  wp = p_add_new_point(swp, in_x, in_y, locate_detail);
  p_set_current_workpoint(swp, wp);

  mgup = (GapMorphGUIParams *)swp->mgup;
  p_refresh_total_points_label(mgup);

}  /* end p_add_new_point_refresh */

/* -----------------------------
 * p_refresh_total_points_label
 * -----------------------------
 * add a new point at in_x/in_y
 * and set the newly added point as current point
 * (this also sets refresh request)
 */
static void
p_refresh_total_points_label(GapMorphGUIParams *mgup)
{
  GapMorphWorkPoint *wp;
  gint  total_points;

  total_points = 0;
  for(wp = mgup->mgpp->master_wp_list; wp != NULL; wp = (GapMorphWorkPoint *)wp->next)
  {
    total_points++;
  }

  if(mgup->toal_points_label)
  {
    char num_buf[10];

    g_snprintf(num_buf, sizeof(num_buf), "%03d", (int)total_points);
    gtk_label_set_text(GTK_LABEL(mgup->toal_points_label), num_buf);
  }

}  /* end p_refresh_total_points_label */

/* ---------------------------------
 * p_find_nearest_point
 * ---------------------------------
 * saerch the workpoint list for the point that is the nearest
 * to position in_x/in_y in the osrc or fdst coord system.
 * (depending on the swp->src_flag)
 */
static GapMorphWorkPoint *
p_find_nearest_point(GapMorphSubWin  *swp
                    , gdouble in_x
                    , gdouble in_y
                    , gdouble *ret_sqr_dist
                    )
{
  GapMorphWorkPoint *wp;
  GapMorphWorkPoint *wp_list;
  GapMorphWorkPoint *wp_ret;
  gdouble            sqr_distance;
  gdouble            min_sqr_distance;
  GapMorphGUIParams *mgup;

  mgup = (GapMorphGUIParams *)swp->mgup;

  wp_list = mgup->mgpp->master_wp_list;
  wp_ret = wp_list;

  min_sqr_distance = GAP_MORPH_MAXGINT32;

  for(wp = wp_list; wp != NULL; wp = (GapMorphWorkPoint *)wp->next)
  {
    register gdouble  adx;
    register gdouble  ady;

    if(swp->src_flag)
    {
      adx = abs(wp->osrc_x - in_x);
      ady = abs(wp->osrc_y - in_y);
    }
    else
    {
      adx = abs(wp->fdst_x - in_x);
      ady = abs(wp->fdst_y - in_y);
    }

    sqr_distance = (adx * adx) + (ady * ady);

    if(sqr_distance < min_sqr_distance)
    {
      wp_ret = wp;
      min_sqr_distance = sqr_distance;
    }
  }

  *ret_sqr_dist = min_sqr_distance;
  return(wp_ret);
}  /* end p_find_nearest_point */


/* -----------------------------
 * p_pick_nearest_point
 * -----------------------------
 * if there is a near point
 *    then set this point as new current workpoint and
 *    return TRUE
 * else
 *    return FALSE
 */
static gboolean
p_pick_nearest_point(GapMorphSubWin  *swp, gdouble l_x, gdouble l_y)
{
  GapMorphWorkPoint *wp;
  gdouble sqr_distance;

  wp = p_find_nearest_point(swp, l_x, l_y, &sqr_distance);
  if(sqr_distance <= GAP_MORPH_PICK_SQR_NEAR_THRESHOLD)
  {
    p_set_current_workpoint(swp, wp);
    return (TRUE);
  }
  return (FALSE);

}  /* end p_pick_nearest_point */


/* --------------------------------
 * p_show_warp_pick_point
 * --------------------------------
 */
static void
p_show_warp_pick_point(GapMorphGUIParams *mgup
                      ,gdouble in_x
                      ,gdouble in_y
                      )
{
  gdouble pick_x;
  gdouble pick_y;
  gdouble scale_x;
  gdouble scale_y;
  gint32 src_layer_width;
  gint32 src_layer_height;
  gint32 dst_layer_width;
  gint32 dst_layer_height;

  scale_x = 1.0;
  scale_y = 1.0;
  if((mgup->mgpp->osrc_layer_id >= 0)
  && (mgup->mgpp->fdst_layer_id >= 0))
  {
    src_layer_width  = gimp_drawable_width(mgup->mgpp->osrc_layer_id);
    src_layer_height = gimp_drawable_height(mgup->mgpp->osrc_layer_id);
    dst_layer_width  = gimp_drawable_width(mgup->mgpp->fdst_layer_id);
    dst_layer_height = gimp_drawable_height(mgup->mgpp->fdst_layer_id);


    scale_x = src_layer_width / MAX(1,dst_layer_width);
    scale_y = src_layer_height / MAX(1,dst_layer_height);
  }

  gap_morph_exec_get_warp_pick_koords(mgup->mgpp->master_wp_list
                                     ,in_x
                                     ,in_y
                                     ,scale_x
                                     ,scale_y
                                     ,mgup->mgpp->use_quality_wp_selection
                                     ,mgup->mgpp->use_gravity
                                     ,mgup->mgpp->gravity_intensity
                                     ,mgup->mgpp->affect_radius
                                     ,&pick_x
                                     ,&pick_y
                                     );
  mgup->show_in_x = in_x;
  mgup->show_in_y = in_y;
  mgup->show_px = pick_x;
  mgup->show_py = pick_y;

  p_upd_warp_info_label(mgup);   /* show debug values distance & weight to current point */
  p_set_upd_timer_request(mgup
                          , GAP_MORPH_DLG_UPD_REQUEST_REDRAW  /* src_request */
                          , GAP_MORPH_DLG_UPD_REQUEST_REDRAW  /* dst_request */
                          );

}  /* end p_show_warp_pick_point */

/* ------------------------------
 * p_draw_workpoints
 * ------------------------------
 */
static void
p_draw_workpoints (GapMorphSubWin  *swp)
{
  GdkColor fg;
  GdkColor fg_curr;
  GdkColor fg_sel;
  GdkColormap *cmap;
  guchar   l_red, l_green, l_blue;
  GapMorphWorkPoint *wp;
  GapMorphWorkPoint *wp_list;
  GapMorphGUIParams *mgup;
  gdouble px, py;
  gdouble scalex, scaley;
  GapMorphSubWin    *swp_other;
  gboolean           show_lines;


  mgup = (GapMorphGUIParams *)swp->mgup;
  wp_list = mgup->mgpp->master_wp_list;

  show_lines = FALSE;
  if(mgup->show_lines_checkbutton)
  {
    if (GTK_TOGGLE_BUTTON (mgup->show_lines_checkbutton)->active)
    {
      show_lines = TRUE;
    }
  }
  if(swp->src_flag)
  {
    swp_other = &mgup->dst_win;
  }
  else
  {
    swp_other = &mgup->src_win;
  }

  scalex = gimp_drawable_width(*swp->layer_id_ptr) / gimp_drawable_width(*swp_other->layer_id_ptr);
  scaley = gimp_drawable_height(*swp->layer_id_ptr) / gimp_drawable_height(*swp_other->layer_id_ptr);

  cmap = gtk_widget_get_colormap(swp->pv_ptr->da_widget);

  gimp_rgb_get_uchar (&mgup->pointcolor, &l_red, &l_green, &l_blue);
  fg.red   = (l_red   << 8) | l_red;
  fg.green = (l_green << 8) | l_green;
  fg.blue  = (l_blue  << 8) | l_blue;

  gimp_rgb_get_uchar (&mgup->curr_pointcolor, &l_red, &l_green, &l_blue);
  fg_curr.red   = (l_red   << 8) | l_red;
  fg_curr.green = (l_green << 8) | l_green;
  fg_curr.blue  = (l_blue  << 8) | l_blue;

  fg_sel.red   = (l_red   << 8) | l_red;
  fg_sel.green = (l_green << 7) | l_green;
  fg_sel.blue  = (l_blue  << 8) | l_blue;

  /*if(gap_debug) printf ("fg.r/g/b (%d %d %d)\n", (int)fg.red ,(int)fg.green, (int)fg.blue); */

  if(cmap)
  {
     gdk_colormap_alloc_color(cmap
                          , &fg
                          , FALSE   /* writeable */
                          , TRUE   /* best_match */
                          );
     gdk_colormap_alloc_color(cmap
                          , &fg_curr
                          , FALSE   /* writeable */
                          , TRUE   /* best_match */
                          );
     gdk_colormap_alloc_color(cmap
                          , &fg_sel
                          , FALSE   /* writeable */
                          , TRUE   /* best_match */
                          );
  }
  /*if(gap_debug) printf ("fg.pixel (%d)\n", (int)fg.pixel); */



  for(wp=(GapMorphWorkPoint *)wp_list; wp != NULL; wp = (GapMorphWorkPoint *)wp->next)
  {
    if(swp->src_flag)
    {
      px = (wp->osrc_x - swp->offs_x) / swp->zoom;
      py = (wp->osrc_y - swp->offs_y) / swp->zoom;
    }
    else
    {
      px = (wp->fdst_x - swp->offs_x) / swp->zoom;
      py = (wp->fdst_y - swp->offs_y) / swp->zoom;
    }
    if(wp == swp->curr_wp)
    {
      gdk_gc_set_foreground (swp->pv_ptr->da_widget->style->black_gc, &fg_curr);
    }
    else
    {
      if((mgup->show_in_x >= 0) && (wp->warp_weight >= 0))
      {
        /* for debug: show in selected color */
        gdk_gc_set_foreground (swp->pv_ptr->da_widget->style->black_gc, &fg_sel);
      }
      else
      {
        gdk_gc_set_foreground (swp->pv_ptr->da_widget->style->black_gc, &fg);
      }
    }

    /* draw the morph workpoint(s) */
    gdk_draw_arc (swp->pv_ptr->da_widget->window, swp->pv_ptr->da_widget->style->black_gc
            , TRUE
            , (px -RADIUS)
            , (py -RADIUS)
            , RADIUS * 2, RADIUS * 2, 0, 23040
            );

    if((!swp->src_flag) && (show_lines))
    {
      gdouble qx, qy;

      qx = ((wp->osrc_x * scalex) - swp->offs_x) / swp->zoom;
      qy = ((wp->osrc_y * scaley) - swp->offs_y) / swp->zoom;

      /* draw vektor of workpoint movement (only in dst window) */
      gdk_draw_line (swp->pv_ptr->da_widget->window
                    ,swp->pv_ptr->da_widget->style->black_gc
                    ,px
                    ,py
                    ,qx
                    ,qy
                    );

    }

  }

  if(mgup->show_in_x >= 0)
  {
    l_red = 250;
    l_green = 16;
    l_blue = 16;

    fg.red   = (l_red   << 8) | l_red;
    fg.green = (l_green << 8) | l_green;
    fg.blue  = (l_blue  << 8) | l_blue;

    if(cmap)
    {
     gdk_colormap_alloc_color(cmap
                          , &fg
                          , FALSE   /* writeable */
                          , TRUE   /* best_match */
                          );
      gdk_gc_set_foreground (swp->pv_ptr->da_widget->style->black_gc, &fg);
    }

    if(swp->src_flag)
    {
      px = (mgup->show_px - swp->offs_x) / swp->zoom;
      py = (mgup->show_py - swp->offs_y) / swp->zoom;
    }
    else
    {
      px = (mgup->show_in_x - swp->offs_x) / swp->zoom;
      py = (mgup->show_in_y - swp->offs_y) / swp->zoom;
    }

    /* draw the warp pick point
     * (the point that will be picked for the current position in src by the warp procedure)
     * THIS is a debug feature
     */
    gdk_draw_arc (swp->pv_ptr->da_widget->window, swp->pv_ptr->da_widget->style->black_gc
          , TRUE
          , (px -RADIUS_SHOW)
          , (py -RADIUS_SHOW)
          , RADIUS_SHOW * 2, RADIUS_SHOW * 2, 0, 23040
          );
  }

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

  gdk_gc_set_foreground (swp->pv_ptr->da_widget->style->black_gc, &fg);

}  /* end p_draw_workpoints */


/* ------------------------------
 * p_prevw_draw
 * ------------------------------
 * Preview Rendering routine
 * does refresh preview and draw the workpoints
 */
static void
p_prevw_draw (GapMorphSubWin  *swp)
{
  /*if(gap_debug) printf("p_prevw_draw: START\n");*/

  if(swp->pv_ptr == NULL)
  {
    return;
  }
  if(swp->pv_ptr->da_widget==NULL)
  {
    return;
  }

  /*if(gap_debug) printf("p_prevw_draw: gap_pview_repaint\n");*/
  gap_pview_repaint(swp->pv_ptr);

  p_draw_workpoints(swp);

}  /* end p_prevw_draw */


/* ----------------------------------
 * p_render_zoomed_pview
 * ----------------------------------
 * set image in the pview widget
 * according to current zoom and offet settings
 */
static void
p_render_zoomed_pview(GapMorphSubWin  *swp)
{
  if(*swp->layer_id_ptr >= 0)
  {
    GimpDrawable *dst_drawable;
    GimpDrawable *src_drawable;
    GimpImageBaseType l_basetype;
    GimpImageBaseType l_type;
    gint32 src_image_id;
    gint32 src_layer_id;
    gint32 tmp_image_id;
    gint32 tmp_layer_id;
    gdouble   fwidth;
    gdouble   fheight;
    gint   width;
    gint   height;
    gint   offs_x;
    gint   offs_y;

    src_layer_id = *swp->layer_id_ptr;

    if(gap_debug) printf("p_render_zoomed_pview START src_layer_id: %d\n", (int)src_layer_id);

    src_image_id = gimp_drawable_get_image(src_layer_id);
    l_basetype   = gimp_image_base_type(src_image_id);
    l_type   = GIMP_RGBA_IMAGE;
    if(l_basetype == GIMP_GRAY)
    {
      l_type   = GIMP_GRAYA_IMAGE;
    }
    src_drawable = gimp_drawable_get (src_layer_id);

    /* constrain to legal values */
    fwidth  = swp->zoom * (gdouble)swp->pv_ptr->pv_width;
    fheight = swp->zoom * (gdouble)swp->pv_ptr->pv_height;
    width  = CLAMP((gint)fwidth,  1, (gint)src_drawable->width);
    height = CLAMP((gint)fheight, 1, (gint)src_drawable->height);
    offs_x = CLAMP(swp->offs_x, 0, ((gint)src_drawable->width - width));
    offs_y = CLAMP(swp->offs_y, 0, ((gint)src_drawable->height - height));

    /* feedback constrained values */
    swp->offs_x = offs_x;
    swp->offs_y = offs_y;

    tmp_image_id = gimp_image_new(width, height, l_basetype);
    tmp_layer_id = gimp_layer_new(tmp_image_id, "bg"
                                , width
                                , height
                                , l_type
                                , 100.0      /* full opaque */
                                , GIMP_NORMAL_MODE
                                );
    gimp_image_insert_layer (tmp_image_id, tmp_layer_id, 0, 0);

    /* copy the visible region to temp_layer_id */
    {
      GimpPixelRgn  srcPR;
      GimpPixelRgn  dstPR;
      guchar *buf_ptr;

      dst_drawable = gimp_drawable_get (tmp_layer_id);
      buf_ptr = g_malloc(dst_drawable->bpp * dst_drawable->width * dst_drawable->height);

      if(gap_debug)
      {
        printf("p_render_zoomed_pview: w/h: %d / %d offs_x/y: %d / %d\n"
              , (int)width
              , (int)height
              , (int)offs_x
              , (int)offs_y
              );
      }

      gimp_pixel_rgn_init (&srcPR, src_drawable
                      , offs_x, offs_y     /* x1, y1 */
                      , width
                      , height
                      , FALSE    /* dirty */
                      , FALSE    /* shadow */
                       );
      gimp_pixel_rgn_init (&dstPR, dst_drawable
                      , 0, 0     /* x1, y1 */
                      , width
                      , height
                      , TRUE     /* dirty */
                      , FALSE    /* shadow */
                       );

      gimp_pixel_rgn_get_rect(&srcPR
                              ,buf_ptr
                              ,offs_x
                              ,offs_y
                              ,width
                              ,height
                              );
      gimp_pixel_rgn_set_rect(&dstPR
                              ,buf_ptr
                              ,0
                              ,0
                              ,width
                              ,height
                              );
      g_free(buf_ptr);

      gimp_drawable_detach(dst_drawable);
      gimp_drawable_detach(src_drawable);

    }

    /* render the preview (this includes scaling to preview size) */
    gap_pview_render_from_image(swp->pv_ptr, tmp_image_id);
    gimp_image_delete(tmp_image_id);

    p_draw_workpoints(swp);
    p_hvscale_adj_set_limits(swp);
  }
}  /* end p_render_zoomed_pview */


/* -----------------------------
 * on_pview_events
 * -----------------------------
 */
static gint
on_pview_events (GtkWidget *widget
               , GdkEvent *event
               , GapMorphSubWin  *swp)
{
  GapMorphGUIParams *mgup;
  GdkEventButton *bevent;
  GdkEventMotion *mevent;
  gint mouse_button;
  gdouble    curx;        /* current mouse position coordinate */
  gdouble    cury;

  static gdouble    prevx;        /* prev mouse position coordinate */
  static gdouble    prevy;
  static gboolean   drag_disabled = FALSE;  /* ALT or CTRL pressed */

  mouse_button = 0;
  bevent = (GdkEventButton *) event;

  if(swp == NULL)       { return FALSE;}
  if(swp->startup_flag) { return FALSE;}
  mgup = (GapMorphGUIParams *)swp->mgup;
  if(mgup == NULL)      { return FALSE;}

  switch (event->type)
    {
    case GDK_EXPOSE:
     /*if(gap_debug) printf("GDK_EXPOSE\n"); */

     p_prevw_draw(swp);  /* draw preview and workpoints */
     return FALSE;
     break;

    case GDK_BUTTON_RELEASE:
      bevent = (GdkEventButton *) event;
      mouse_button = 0 - bevent->button;
      swp->pview_scrolling = FALSE;
      drag_disabled = FALSE;
      prevx = -1;
      prevy = -1;
      return (FALSE);
    case GDK_BUTTON_PRESS:
      bevent = (GdkEventButton *) event;
      mouse_button = bevent->button;
      curx = bevent->x;
      cury = bevent->y;
      goto mouse;

    case GDK_MOTION_NOTIFY:
      mevent = (GdkEventMotion *) event;
      if ( !mevent->state ) break;
      curx = mevent->x;
      cury = mevent->y;
    mouse:
      if((mouse_button == 1)
      || (mouse_button == 3))
      {
        gdouble l_x;
        gdouble l_y;
        gboolean make_new_point;
        gboolean locate_detail;

        locate_detail = FALSE;

         /* Picking of pathpoints is done when
          *   the left mousebutton goes down (mouse_button == 1)
          */
         l_x = swp->offs_x + (curx * swp->zoom);
         l_y = swp->offs_y + (cury * swp->zoom);
         make_new_point = FALSE;

         /* Debug SHOW handling */
         if (mgup->op_mode == GAP_MORPH_OP_MODE_SHOW)
         {
           if(mouse_button == 1)
           {
             p_show_warp_pick_point(mgup, l_x, l_y);
           }
           else
           {
             GapMorphWorkPoint *wp;
             gdouble sqr_distance;

             /* set nearest point as new current point */
             wp = p_find_nearest_point(swp, l_x, l_y, &sqr_distance);
             p_set_current_workpoint(swp, wp);
           }
           return (FALSE);
         }

         /* ZOOM handling */
         if (mgup->op_mode == GAP_MORPH_OP_MODE_ZOOM)
         {
             mgup->show_in_x = -1;
             mgup->show_in_y = -1;

             if(bevent->state & GDK_CONTROL_MASK)
             {
               if(mouse_button == 1)
               {
                 p_zoom_out(swp);
               }
               else
               {
                 p_zoom_in(swp, l_x, l_y);
               }
             }
             else
             {
               if(mouse_button == 1)
               {
                 p_zoom_in(swp, l_x, l_y);
               }
               else
               {
                 p_zoom_out(swp);
               }
             }
             return (FALSE);
         }

         /* DELETE handling */
         if((mouse_button == 3)
         || (mgup->op_mode == GAP_MORPH_OP_MODE_DELETE))
         {
           if((p_pick_nearest_point(swp, l_x, l_y))
           || (mgup->op_mode == GAP_MORPH_OP_MODE_DELETE))
           {
             p_delete_current_point(swp);
             p_prevw_draw(swp);
           }
           return (FALSE);
         }

         if(bevent->state & GDK_CONTROL_MASK)
         {
           if(mgup->op_mode == GAP_MORPH_OP_MODE_MOVE)
           {
             make_new_point = TRUE;
           }
           locate_detail = TRUE;
         }

         if(bevent->state & GDK_SHIFT_MASK)
         {
           /* SHIFT-Click: force creation of new point
            * at mouse pointer position
            */
           make_new_point = TRUE;
         }
         else
         {
           GapMorphWorkPoint *wp;
           gdouble sqr_distance;

           wp = p_find_nearest_point(swp, l_x, l_y, &sqr_distance);
           /* on ALT-click just pick current point
            * but dont drag and dont create new point
            */
           if(bevent->state & GDK_CONTROL_MASK)
           {
             drag_disabled = TRUE;
           }

           if(bevent->state & GDK_MOD1_MASK)
           {
             p_set_current_workpoint(swp, wp);
             drag_disabled = TRUE;
             return(FALSE);
           }
           /* normal SET op_mode: try to pick near point
            * create new point if nothing is near
            * MOVE op_mode: always pick nearest point,
            *      dont create point on click
            */
           if(sqr_distance <= GAP_MORPH_PICK_SQR_NEAR_THRESHOLD)
           {
             p_set_current_workpoint(swp, wp);
           }
           else
           {
             if (mgup->op_mode != GAP_MORPH_OP_MODE_MOVE)
             {
               make_new_point = TRUE;
             }
             else
             {
               p_set_current_workpoint(swp, wp);
             }
           }
         }

         if(make_new_point)
         {
           /* add the new point and handle refresh for both src and dst view */
           p_add_new_point_refresh(swp, l_x, l_y, locate_detail);
           return(FALSE);
         }
      }

      /* Handle SCROLLING */
      if((mgup->op_mode == GAP_MORPH_OP_MODE_ZOOM)
      || (mouse_button == 2)
      || (swp->pview_scrolling))
      {
        gint l_dx;
        gint l_dy;
        gint32  src_request;
        gint32  dst_request;

        l_dx = 0;
        l_dy = 0;
        if((prevx >= 0) && (prevy >= 0))
        {
          l_dx = prevx - curx;
          l_dy = prevy - cury;
        }

        swp->offs_x += l_dx;
        swp->offs_y += l_dy;

        swp->pview_scrolling = TRUE;
        if(gap_debug) printf("scrolling dx:%d dy: %d\n", (int)l_dx ,(int)l_dy);

        prevx = curx;
        prevy = cury;

        if(swp->src_flag)
        {
          src_request = GAP_MORPH_DLG_UPD_REQUEST_FULL_REFRESH;
          dst_request = GAP_MORPH_DLG_UPD_REQUEST_NONE;
        }
        else
        {
          src_request = GAP_MORPH_DLG_UPD_REQUEST_NONE;
          dst_request = GAP_MORPH_DLG_UPD_REQUEST_FULL_REFRESH;
        }
        p_set_upd_timer_request(swp->mgup, src_request, dst_request);
        return (FALSE);
      }

      /* dragging the current workpoint
       * (is locked if we are currently deleting or zooming
       * or by modifier Keys ALT and CTRL in SET mode)
       */
      if((!drag_disabled)
      &  (mgup->op_mode != GAP_MORPH_OP_MODE_SHOW))
      {
        *swp->wpx_ptr = swp->offs_x + (curx * swp->zoom);
        *swp->wpy_ptr = swp->offs_y + (cury * swp->zoom);


        if((swp->x_spinbutton_adj)
        &&(swp->y_spinbutton_adj))
        {
          /* Render the Preview and Workpoints */
          p_prevw_draw(swp);

          gtk_adjustment_set_value (GTK_ADJUSTMENT(swp->x_spinbutton_adj), *swp->wpx_ptr);
          gtk_adjustment_set_value (GTK_ADJUSTMENT(swp->y_spinbutton_adj), *swp->wpy_ptr);
          
        }
      }
      break;

    default:
      break;
    }

  return FALSE;
}  /* end on_pview_events */


/* ----------------------------------
 * p_scale_wp_list
 * ----------------------------------
 * scale workpoints from old width/height to
 * new layer dimensions.
 * (dont scale if old_width value 0
 *  in that case there we had no other valid layerdimension before)
 * this procedures sets the old dimensions width/height equal to the
 * current dimensions at end of processing.
 */
static void
p_scale_wp_list(GapMorphGUIParams *mgup)
{
  GapMorphWorkPoint *wp;
  gint32 src_layer_width;
  gint32 src_layer_height;
  gint32 dst_layer_width;
  gint32 dst_layer_height;
  gdouble src_scale_x;
  gdouble src_scale_y;
  gdouble dst_scale_x;
  gdouble dst_scale_y;


  if(mgup == NULL)
  {
    return;
  }
  src_scale_x = 1.0;
  src_scale_y = 1.0;
  src_layer_width = 0;
  src_layer_height = 0;

  if(mgup->mgpp->osrc_layer_id >= 0)
  {
    src_layer_width  = gimp_drawable_width(mgup->mgpp->osrc_layer_id);
    src_layer_height = gimp_drawable_height(mgup->mgpp->osrc_layer_id);
    if(mgup->old_src_layer_width > 0)
    {
      src_scale_x = (gdouble)src_layer_width  / (gdouble)MAX(1,mgup->old_src_layer_width);
      src_scale_y = (gdouble)src_layer_height / (gdouble)MAX(1,mgup->old_src_layer_height);
    }
  }

  dst_scale_x = 1.0;
  dst_scale_y = 1.0;
  dst_layer_width = 0;
  dst_layer_height = 0;

  if(mgup->mgpp->fdst_layer_id >= 0)
  {
    dst_layer_width  = gimp_drawable_width(mgup->mgpp->fdst_layer_id);
    dst_layer_height = gimp_drawable_height(mgup->mgpp->fdst_layer_id);
    if(mgup->old_dst_layer_width > 0)
    {
      dst_scale_x = (gdouble)dst_layer_width  / (gdouble)MAX(1,mgup->old_dst_layer_width);
      dst_scale_y = (gdouble)dst_layer_height / (gdouble)MAX(1,mgup->old_dst_layer_height);
    }
  }

  if(gap_debug)
  {
    printf("p_scale_wp_list SRC_LAYER: %d  DST_LAYER: %d\n"
            , (int)mgup->mgpp->osrc_layer_id
            , (int)mgup->mgpp->fdst_layer_id
            );
    printf("p_scale_wp_list src_layer_width: (old:%d) %d\n"
            , (int)mgup->old_src_layer_width
            , (int)src_layer_width
            );
    printf("p_scale_wp_list src_layer_height: (old:%d) %d\n"
            , (int)mgup->old_src_layer_height
            , (int)src_layer_height
            );
    printf("p_scale_wp_list dst_layer_width: (old:%d) %d\n"
            , (int)mgup->old_dst_layer_width
            , (int)dst_layer_width
            );
    printf("p_scale_wp_list dst_layer_height: (old:%d) %d\n"
            , (int)mgup->old_dst_layer_height
            , (int)dst_layer_height
            );

    printf("p_scale_wp_list src_scale_x: %f\n", (float)src_scale_x);
    printf("p_scale_wp_list src_scale_y: %f\n", (float)src_scale_y);
    printf("p_scale_wp_list dst_scale_x: %f\n", (float)dst_scale_x);
    printf("p_scale_wp_list dst_scale_y: %f\n", (float)dst_scale_y);
  }

  for(wp = mgup->mgpp->master_wp_list; wp != NULL; wp = (GapMorphWorkPoint *)wp->next)
  {
     /* scale the loaded workpoints
      * (to fit the current layer)
      */
     wp->osrc_x *=  src_scale_x;
     wp->osrc_y *=  src_scale_y;
     wp->fdst_x *=  dst_scale_x;
     wp->fdst_y *=  dst_scale_y;

     wp->src_x = wp->osrc_x;
     wp->src_y = wp->osrc_y;
     wp->dst_x = wp->fdst_x;
     wp->dst_y = wp->fdst_y;
  }

  /* store current dimensions as old dimensions
   * to be prepared for the next call
   */

  if(src_layer_width > 0)
  {
    mgup->old_src_layer_width  = src_layer_width;
    mgup->old_src_layer_height = src_layer_height;
  }

  if(dst_layer_width > 0)
  {
    mgup->old_dst_layer_width  = dst_layer_width;
    mgup->old_dst_layer_height = dst_layer_height;
  }

}  /* end p_scale_wp_list */


/* -----------------------------
 * p_imglayer_menu_callback
 * -----------------------------
 */
static void
p_imglayer_menu_callback(GtkWidget *widget, GapMorphSubWin *swp)
{
  gint32 l_image_id;
  gint   value;
  gint32 layer_id;

  gimp_int_combo_box_get_active (GIMP_INT_COMBO_BOX (widget), &value);
  layer_id = value;

  l_image_id = gimp_drawable_get_image(layer_id);
  if(!gap_image_is_alive(l_image_id))
  {
     if(gap_debug) printf("p_imglayer_menu_callback: NOT ALIVE image_id=%d layer_id=%d\n",
         (int)l_image_id, (int)layer_id);
     return;
  }

  if(swp->layer_id_ptr)
  {
    if(layer_id != *swp->layer_id_ptr)
    {
      *swp->layer_id_ptr = layer_id;
      if(gap_debug) printf("p_imglayer_menu_callback new LAYER_ID: %d\n", (int)layer_id);

      p_scale_wp_list(swp->mgup);
      p_fit_zoom_into_pview_size(swp);
    }
  }


  if(gap_debug)
  {
    printf("p_imglayer_menu_callback: image_id=%d layer_id=%d\n"
          , (int)swp->image_id, (int)*swp->layer_id_ptr);
  }
}  /* end p_imglayer_menu_callback */


/* -----------------------------
 * p_imglayer_constrain
 * -----------------------------
 */
static gint
p_imglayer_constrain(gint32 image_id, gint32 drawable_id, gpointer data)
{
  if(gap_debug)
  {
    printf("GAP-DEBUG: p_imglayer_constrain PROCEDURE image_id:%d drawable_id:%d\n"
                          ,(int)image_id
                          ,(int)drawable_id
                          );
  }

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


  if(!gimp_drawable_has_alpha(drawable_id))
  {
     return(FALSE);
  }

   /* Accept all other RGB and GRAY layers
    */

  if(gimp_drawable_is_rgb(drawable_id))
  {
    return(TRUE);
  }

  if(gimp_drawable_is_gray(drawable_id))
  {
    return(TRUE);
  }

  return (FALSE);

} /* end p_imglayer_constrain */



/* -----------------------------
 * p_refresh_layer_menu
 * -----------------------------
 */
static void
p_refresh_layer_menu(GapMorphSubWin *swp, GapMorphGUIParams *mgup)
{
  gimp_int_combo_box_connect (GIMP_INT_COMBO_BOX (swp->combo),
                              *swp->layer_id_ptr,                    /* the initial value to set */
                              G_CALLBACK (p_imglayer_menu_callback),
                              swp
                              );

}  /* end p_refresh_layer_menu */


/* -----------------------------
 * on_pointcolor_button_changed
 * -----------------------------
 * repaint with new pointcolors
 * (for both pviews)
 */
static void
on_pointcolor_button_changed(GimpColorButton *widget,
                             GimpRGB *pcolor )
{
  GapMorphGUIParams *mgup;

  mgup = g_object_get_data( G_OBJECT(widget), "mgup");
  if(mgup)
  {
    gimp_color_button_get_color(widget, pcolor);
    p_set_upd_timer_request(mgup
                         ,GAP_MORPH_DLG_UPD_REQUEST_REDRAW
                         ,GAP_MORPH_DLG_UPD_REQUEST_REDRAW
                         );

  }
}  /* end on_pointcolor_button_changed */


/* --------------------------------
 * on_wp_filesel_destroy
 * --------------------------------
 */
static void
on_wp_filesel_destroy          (GtkObject       *object
                               ,GapMorphGUIParams *mgup)
{
 if(gap_debug) printf("CB: on_wp_filesel_destroy\n");
 if(mgup == NULL) return;

 mgup->wp_filesel = NULL;
}  /* end on_wp_filesel_destroy */


/* -----------------------------------
 * on_wp_filesel_button_cancel_clicked
 * -----------------------------------
 */
static void
on_wp_filesel_button_cancel_clicked  (GtkButton       *button
                                     ,GapMorphGUIParams *mgup)
{
 if(gap_debug) printf("CB: on_wp_filesel_button_cancel_clicked\n");
 if(mgup == NULL) return;

 /* update workpoint_file_labels */
 if(mgup->workpoint_file_lower_label)
 {
   gtk_label_set_text(GTK_LABEL(mgup->workpoint_file_lower_label)
                     ,mgup->mgpp->workpoint_file_lower
                     );
 }
 if(mgup->workpoint_file_upper_label)
 {
   gtk_label_set_text(GTK_LABEL(mgup->workpoint_file_upper_label)
                     ,mgup->mgpp->workpoint_file_upper
                     );
 }

 if(mgup->wp_filesel)
 {
   gtk_widget_destroy(mgup->wp_filesel);
   mgup->wp_filesel = NULL;
 }
}  /* end on_wp_filesel_button_cancel_clicked */


/* --------------------------------
 * on_wp_filesel_button_OK_clicked
 * --------------------------------
 * used both for save and load
 */
static void
on_wp_filesel_button_OK_clicked      (GtkButton         *button
                                     ,GapMorphGUIParams *mgup)
{
  const gchar *filename;
  gint   l_errno;

 if(gap_debug) printf("CB: on_wp_filesel_button_OK_clicked\n");
 if(mgup == NULL) return;
 if(mgup->workpoint_file_ptr == NULL) return;

 /* quit if Main window was closed */
 if(mgup->shell == NULL) { gtk_main_quit (); return; }


 if(mgup->wp_filesel)
 {
   filename =  gtk_file_selection_get_filename (GTK_FILE_SELECTION (mgup->wp_filesel));
   g_snprintf(mgup->workpoint_file_ptr
                ,GAP_MORPH_WORKPOINT_FILENAME_MAX_LENGTH
                ,"%s"
                ,filename
                );
   if(mgup->wp_save_mode)
   {
     gboolean l_wr_permission;

     l_wr_permission = gap_arr_overwrite_file_dialog(filename);

     /* quit if Main window was closed while overwrite dialog was open */
     if(mgup->shell == NULL) { gtk_main_quit (); return; }

     if(l_wr_permission)
     {
       if(!gap_moprh_exec_save_workpointfile(filename, mgup))
       {
         l_errno = errno;
         g_message (_("Failed to write morph workpointfile\n"
                      "filename: '%s':\n%s")
                   , filename
                   , g_strerror (l_errno)
                   );
       }
     }
   }
   else
   {
     GapMorphWorkPoint *wp_list;

     wp_list = gap_moprh_exec_load_workpointfile(filename, mgup);
     l_errno = errno;
     if(wp_list)
     {
       if(mgup->mgpp->master_wp_list)
       {
         gap_morph_exec_free_workpoint_list(&mgup->mgpp->master_wp_list);
       }
       mgup->mgpp->master_wp_list = wp_list;
       p_set_current_workpoint(&mgup->src_win, mgup->mgpp->master_wp_list);
       p_refresh_total_points_label(mgup);
       p_upd_widget_values(mgup);
     }
     else
     {
       if(l_errno != 0)
       {
         g_message(_("ERROR: Could not open morph workpoints\n"
                   "filename: '%s'\n%s")
                  ,filename, g_strerror (errno));
       }
       else
       {
         g_message(_("ERROR: Could not read morph workpoints\n"
                   "filename: '%s'\n(Is not a valid morph workpoint file)")
                  ,filename);
       }
     }
   }

   on_wp_filesel_button_cancel_clicked(NULL, (gpointer)mgup);
 }
}  /* end on_wp_filesel_button_OK_clicked */




/* ----------------------------------
 * p_create_wp_filesel
 * ----------------------------------
 * videofile selection dialog
 */
static void
p_create_wp_filesel (GapMorphGUIParams *mgup
                    ,GdkEventButton    *bevent
                    ,gboolean save_mode)
{
  GtkWidget *wp_button_OK;
  GtkWidget *wp_button_cancel;

  if(mgup == NULL)
  {
    return;
  }

  mgup->workpoint_file_ptr = &mgup->mgpp->workpoint_file_lower[0];
  if(bevent)
  {
    if(bevent->state & GDK_SHIFT_MASK)
    {
      mgup->workpoint_file_ptr = &mgup->mgpp->workpoint_file_upper[0];
    }
  }


  if(mgup->wp_filesel)
  {
    gtk_window_present(GTK_WINDOW(mgup->wp_filesel));
    printf("p_create_wp_filesel: filesel dialog already open\n");
    return;
  }

  mgup->wp_save_mode = save_mode;
  if(save_mode)
  {
    mgup->wp_filesel = gtk_file_selection_new (_("Save Morph Workpointfile"));
  }
  else
  {
    mgup->wp_filesel = gtk_file_selection_new (_("Load Morph Workpointfile"));
  }
  gtk_container_set_border_width (GTK_CONTAINER (mgup->wp_filesel), 10);

  wp_button_OK = GTK_FILE_SELECTION (mgup->wp_filesel)->ok_button;
  gtk_widget_show (wp_button_OK);
  GTK_WIDGET_SET_FLAGS (wp_button_OK, GTK_CAN_DEFAULT);

  wp_button_cancel = GTK_FILE_SELECTION (mgup->wp_filesel)->cancel_button;
  gtk_widget_show (wp_button_cancel);
  GTK_WIDGET_SET_FLAGS (wp_button_cancel, GTK_CAN_DEFAULT);

  g_signal_connect (G_OBJECT (mgup->wp_filesel), "destroy",
                      G_CALLBACK (on_wp_filesel_destroy),
                      mgup);
  g_signal_connect (G_OBJECT (wp_button_OK), "clicked",
                      G_CALLBACK (on_wp_filesel_button_OK_clicked),
                      mgup);
  g_signal_connect (G_OBJECT (wp_button_cancel), "clicked",
                      G_CALLBACK (on_wp_filesel_button_cancel_clicked),
                      mgup);
  gtk_file_selection_set_filename (GTK_FILE_SELECTION (mgup->wp_filesel),
                                   mgup->workpoint_file_ptr);


  gtk_widget_grab_default (wp_button_cancel);
  gtk_widget_show (mgup->wp_filesel);
}  /* end p_create_wp_filesel */


/* --------------------------------
 * on_wp_save_button_clicked
 * --------------------------------
 */
static void
on_wp_save_button_clicked (GtkButton         *button
                          ,GdkEventButton    *bevent
                          ,GapMorphGUIParams *mgup)
{
  mgup->cancelWorkpointGeneration = TRUE;
  p_create_wp_filesel(mgup
                     ,bevent
                     ,TRUE  /* save_mode */
                     );
}  /* end on_wp_save_button_clicked */

/* --------------------------------
 * on_wp_load_button_clicked
 * --------------------------------
 */
static void
on_wp_load_button_clicked (GtkButton         *button
                          ,GdkEventButton    *bevent
                          ,GapMorphGUIParams *mgup)
{
  mgup->cancelWorkpointGeneration = TRUE;
  p_create_wp_filesel(mgup
                     ,bevent
                     ,FALSE  /* save_mode */
                     );
}  /* end on_wp_load_button_clicked */



/* --------------------------------
 * on_wp_shape_button_clicked
 * --------------------------------
 */
static void
on_wp_shape_button_clicked (GtkButton *button
                           ,GdkEventButton  *bevent
                           ,GapMorphGUIParams *mgup
                           )
{
  if(mgup)
  {
    gboolean clear_old_workpoint_set;
    gboolean autoEdgeShape;

    if(mgup->workpointGenerationBusy == TRUE)
    {
      return;
    }

    clear_old_workpoint_set = TRUE;
    autoEdgeShape = FALSE;
    if(bevent)
    {
      if(bevent->state & GDK_SHIFT_MASK)
      {
        clear_old_workpoint_set = FALSE;
      }
      if(bevent->state & GDK_CONTROL_MASK)
      {
        autoEdgeShape = TRUE;
      }
    }

    if(clear_old_workpoint_set)
    {
      gap_morph_exec_free_workpoint_list(&mgup->mgpp->master_wp_list);
    }
    mgup->num_shapepoints = (gint32)GTK_ADJUSTMENT(mgup->num_shapepoints_adj)->value;
    
    if(autoEdgeShape == TRUE)
    {
      gtk_widget_show (mgup->progressBar);
      mgup->cancelWorkpointGeneration = FALSE;
      mgup->mgpp->numWorkpoints = mgup->num_shapepoints;
      mgup->mgpp->numOutlinePoints = 0;
      gap_moprhShapeDetectionEdgeBased(mgup, &mgup->cancelWorkpointGeneration);
      gtk_widget_hide (mgup->progressBar);
    }
    else
    {
      p_generate_outline_shape_workpoints(mgup);
    }
    
    p_set_upd_timer_request(mgup
                       ,GAP_MORPH_DLG_UPD_REQUEST_FULL_REFRESH
                       ,GAP_MORPH_DLG_UPD_REQUEST_FULL_REFRESH
                       );
    p_refresh_total_points_label(mgup);
  }
}  /* end on_wp_shape_button_clicked */



/* ------------------------------
 * on_hvscale_changed_callback
 * ------------------------------
 */
static void
on_hvscale_changed_callback(GtkObject *obj, GapMorphSubWin *swp)
{
  gint32  value;
  gint32  src_request;
  gint32  dst_request;

  if(swp == NULL) { return;}

  if(obj == swp->vscale_adj)
  {
    value = (gint32)GTK_ADJUSTMENT(swp->vscale_adj)->value;
    if(value != swp->offs_y)
    {
      swp->offs_y = value;
    }
  }
  else
  {
    value = (gint32)GTK_ADJUSTMENT(swp->hscale_adj)->value;
    if(value != swp->offs_x)
    {
      swp->offs_x = value;
    }
  }

  if(swp->src_flag)
  {
    src_request = GAP_MORPH_DLG_UPD_REQUEST_FULL_REFRESH;
    dst_request = GAP_MORPH_DLG_UPD_REQUEST_NONE;
  }
  else
  {
    src_request = GAP_MORPH_DLG_UPD_REQUEST_NONE;
    dst_request = GAP_MORPH_DLG_UPD_REQUEST_FULL_REFRESH;
  }
  p_set_upd_timer_request(swp->mgup, src_request, dst_request);

}  /* end on_hvscale_changed_callback */




/* ------------------------------
 * on_show_lines_toggled_callback
 * ------------------------------
 */
static void
on_show_lines_toggled_callback(GtkWidget *widget, GapMorphGUIParams *mgup)
{
  if(mgup)
  {
    p_set_upd_timer_request(mgup
                          , 0                                 /* src_request */
                          , GAP_MORPH_DLG_UPD_REQUEST_REDRAW  /* dst_request */
                            );
  }
}  /* end on_show_lines_toggled_callback */


/* --------------------------------------------
 * on_use_quality_wp_selection_toggled_callback
 * --------------------------------------------
 */
static void
on_use_quality_wp_selection_toggled_callback(GtkWidget *widget, GapMorphGUIParams *mgup)
{
  if(mgup)
  {
    if(GTK_TOGGLE_BUTTON (mgup->use_quality_wp_selection_checkbutton)->active)
    {
      mgup->mgpp->use_quality_wp_selection = TRUE;
    }
    else
    {
      mgup->mgpp->use_quality_wp_selection = FALSE;
    }
  }
}  /* end on_use_quality_wp_selection_toggled_callback */


/* ------------------------------
 * on_use_gravity_toggled_callback
 * ------------------------------
 */
static void
on_use_gravity_toggled_callback(GtkWidget *widget, GapMorphGUIParams *mgup)
{
  if(mgup)
  {
    if(GTK_TOGGLE_BUTTON (mgup->use_gravity_checkbutton)->active)
    {
      mgup->mgpp->use_gravity = TRUE;
      gtk_widget_set_sensitive(mgup->gravity_intensity_spinbutton, TRUE);
    }
    else
    {
      mgup->mgpp->use_gravity = FALSE;
      gtk_widget_set_sensitive(mgup->gravity_intensity_spinbutton, FALSE);
    }
  }
}  /* end on_use_gravity_toggled_callback */

/* --------------------------------------
 * on_have_workpointsets_toggled_callback
 * --------------------------------------
 */
static void
on_have_workpointsets_toggled_callback(GtkWidget *widget, GapMorphGUIParams *mgup)
{
  if(mgup)
  {
    if(GTK_TOGGLE_BUTTON (mgup->have_workpointsets_checkbutton)->active)
    {
      mgup->mgpp->have_workpointsets = TRUE;
      gtk_widget_show(mgup->workpoint_file_lower_label);
      gtk_widget_show(mgup->workpoint_file_upper_label);
      gtk_widget_show(mgup->workpoint_lower_label);
      gtk_widget_show(mgup->workpoint_upper_label);
    }
    else
    {
      mgup->mgpp->have_workpointsets = FALSE;
      gtk_widget_hide(mgup->workpoint_file_lower_label);
      gtk_widget_hide(mgup->workpoint_file_upper_label);
      gtk_widget_hide(mgup->workpoint_lower_label);
      gtk_widget_hide(mgup->workpoint_upper_label);
    }
  }
}  /* end on_have_workpointsets_toggled_callback */


/* ---------------------------------------
 * on_create_tween_layers_toggled_callback
 * ---------------------------------------
 */
static void
on_create_tween_layers_toggled_callback(GtkWidget *widget, GapMorphGUIParams *mgup)
{
  if(mgup)
  {
    if(GTK_TOGGLE_BUTTON (mgup->create_tween_layers_checkbutton)->active)
    {
      mgup->mgpp->create_tween_layers = TRUE;
    }
    else
    {
      mgup->mgpp->create_tween_layers = FALSE;
    }
  }
}  /* end on_create_tween_layers_toggled_callback */


/* ---------------------------------
 * on_radio_op_mode_callback
 * ---------------------------------
 */
static void
on_radio_op_mode_callback(GtkWidget *widget, gint32 op_mode)
{
  GapMorphGUIParams *mgup;

  mgup = g_object_get_data( G_OBJECT(widget), "mgup");

  if((mgup)
  && (GTK_TOGGLE_BUTTON (widget)->active))
  {
    if(gap_debug) printf("on_radio_op_mode_callback: OP_MODE: %d\n", (int)op_mode);
    mgup->op_mode = op_mode;
  }
}  /* end on_radio_op_mode_callback */




/* ---------------------------------
 * on_radio_render_mode_callback
 * ---------------------------------
 */
static void
on_radio_render_mode_callback(GtkWidget *widget, gint32 render_mode)
{
  GapMorphGUIParams *mgup;

  mgup = g_object_get_data( G_OBJECT(widget), "mgup");

  if((mgup)
  && (GTK_TOGGLE_BUTTON (widget)->active))
  {
    if(gap_debug) printf("on_radio_render_mode_callback: render_mode: %d\n", (int)render_mode);
    mgup->mgpp->render_mode = render_mode;
  }
}  /* end on_radio_render_mode_callback */


/* ---------------------------------
 * p_radio_create_op_mode
 * ---------------------------------
 */
static void
p_radio_create_op_mode(GtkWidget *table, int row, int col, GapMorphGUIParams *mgup)
{
  GtkWidget *label;
  GtkWidget *radio_table;
  GtkWidget *radio_button;
  GSList    *radio_group = NULL;
  gint      l_idx;
  gint      l_idy;
  gboolean  l_radio_pressed;

  label = gtk_label_new(_("Edit Mode:"));
  gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
  gtk_table_attach( GTK_TABLE (table), label, col, col+1, row, row+1
                  , GTK_FILL, GTK_FILL, 8, 0);
  gtk_widget_show(label);

  /* radio_table */
  /*radio_table = gtk_table_new (1, 5, FALSE);*/
  radio_table = table;

  l_idx = col+1;
  l_idy = row;

  /* radio button SET */
  radio_button = gtk_radio_button_new_with_label ( radio_group, _("Set") );
  radio_group = gtk_radio_button_get_group ( GTK_RADIO_BUTTON (radio_button) );
  gtk_table_attach ( GTK_TABLE (radio_table), radio_button, l_idx, l_idx+1, l_idy, l_idy+1
                   , GTK_FILL | GTK_EXPAND, 0, 0, 0);
  mgup->op_mode_set_toggle = radio_button;

  l_radio_pressed = (mgup->op_mode == GAP_MORPH_OP_MODE_SET);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (radio_button), l_radio_pressed);
  gimp_help_set_help_data(radio_button
                         , _("Click: pick and drag point at cursor"
                             " or create new point\n"
                             "SHIFT-Click: force create new point\n"
                             "Alt-Click: disable drag\n"
                             "Right-Click: delete point at cursor")
                         , NULL);

  gtk_widget_show (radio_button);
  g_object_set_data( G_OBJECT(radio_button), "mgup", mgup);
  g_signal_connect ( G_OBJECT (radio_button), "toggled",
                     G_CALLBACK (on_radio_op_mode_callback),
                     (gpointer)GAP_MORPH_OP_MODE_SET);


  l_idx++;

  /* radio button MOVE */
  radio_button = gtk_radio_button_new_with_label ( radio_group, _("Move") );
  radio_group = gtk_radio_button_get_group ( GTK_RADIO_BUTTON (radio_button) );
  gtk_table_attach ( GTK_TABLE (radio_table), radio_button, l_idx, l_idx+1, l_idy, l_idy+1
                   , GTK_FILL | GTK_EXPAND, 0, 0, 0);
  mgup->op_mode_move_toggle = radio_button;

  l_radio_pressed = (mgup->op_mode == GAP_MORPH_OP_MODE_MOVE);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (radio_button), l_radio_pressed);
  gimp_help_set_help_data(radio_button
                         , _("Click: drag next point\n"
                             "SHIFT-Click: force create new point\n"
                             "Alt-Click: disable drag\n"
                             "Right-Click: delete point at cursor\n")
                         , NULL);

  gtk_widget_show (radio_button);
  g_object_set_data( G_OBJECT(radio_button), "mgup", mgup);
  g_signal_connect ( G_OBJECT (radio_button), "toggled",
                     G_CALLBACK (on_radio_op_mode_callback),
                     (gpointer)GAP_MORPH_OP_MODE_MOVE);



  l_idx++;

  /* radio button DELETE */
  radio_button = gtk_radio_button_new_with_label ( radio_group, _("Delete") );
  radio_group = gtk_radio_button_get_group ( GTK_RADIO_BUTTON (radio_button) );
  gtk_table_attach ( GTK_TABLE (radio_table), radio_button, l_idx, l_idx+1, l_idy, l_idy+1
                   , GTK_FILL | GTK_EXPAND, 0, 0, 0);
  mgup->op_mode_delete_toggle = radio_button;

  l_radio_pressed = (mgup->op_mode == GAP_MORPH_OP_MODE_DELETE);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (radio_button), l_radio_pressed);
  gimp_help_set_help_data(radio_button
                         , _("Click: delete next point")
                         , NULL);

  gtk_widget_show (radio_button);
  g_object_set_data( G_OBJECT(radio_button), "mgup", mgup);
  g_signal_connect ( G_OBJECT (radio_button), "toggled",
                     G_CALLBACK (on_radio_op_mode_callback),
                     (gpointer)GAP_MORPH_OP_MODE_DELETE);


  l_idx++;

  /* radio button ZOOM */
  radio_button = gtk_radio_button_new_with_label ( radio_group, _("Zoom") );
  radio_group = gtk_radio_button_get_group ( GTK_RADIO_BUTTON (radio_button) );
  gtk_table_attach ( GTK_TABLE (radio_table), radio_button, l_idx, l_idx+1, l_idy, l_idy+1
                   , GTK_FILL | GTK_EXPAND, 0, 0, 0);
  mgup->op_mode_zoom_toggle = radio_button;

  l_radio_pressed = (mgup->op_mode == GAP_MORPH_OP_MODE_ZOOM);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (radio_button), l_radio_pressed);
  gimp_help_set_help_data(radio_button
                         , _("Click: zoom in,\nCTRL-click: zoom out")
                         , NULL);

  gtk_widget_show (radio_button);
  g_object_set_data( G_OBJECT(radio_button), "mgup", mgup);
  g_signal_connect ( G_OBJECT (radio_button), "toggled",
                     G_CALLBACK (on_radio_op_mode_callback),
                     (gpointer)GAP_MORPH_OP_MODE_ZOOM);


  l_idx++;


  /* radio button SHOW */
  radio_button = gtk_radio_button_new_with_label ( radio_group, _("Show") );
  radio_group = gtk_radio_button_get_group ( GTK_RADIO_BUTTON (radio_button) );
  gtk_table_attach ( GTK_TABLE (radio_table), radio_button, l_idx, l_idx+1, l_idy, l_idy+1
                   , GTK_FILL | GTK_EXPAND, 0, 0, 0);
  mgup->op_mode_show_toggle = radio_button;

  l_radio_pressed = (mgup->op_mode == GAP_MORPH_OP_MODE_SHOW);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (radio_button), l_radio_pressed);
  gimp_help_set_help_data(radio_button
                         , _("Click: show warp pick coordinates in the source window")
                         , NULL);

  gtk_widget_show (radio_button);
  g_object_set_data( G_OBJECT(radio_button), "mgup", mgup);
  g_signal_connect ( G_OBJECT (radio_button), "toggled",
                     G_CALLBACK (on_radio_op_mode_callback),
                     (gpointer)GAP_MORPH_OP_MODE_SHOW);





}  /* end p_radio_create_op_mode */


/* ---------------------------------
 * p_radio_create_render_mode
 * ---------------------------------
 */
static void
p_radio_create_render_mode(GtkWidget *table, int row, int col, GapMorphGUIParams *mgup)
{
  GtkWidget *label;
  GtkWidget *radio_table;
  GtkWidget *radio_button;
  GSList    *radio_group = NULL;
  gint      l_idx;
  gint      l_idy;
  gboolean  l_radio_pressed;

  label = gtk_label_new(_("Render Mode:"));
  gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
  gtk_table_attach( GTK_TABLE (table), label, col, col+1, row, row+1
                  , GTK_FILL, GTK_FILL, 8, 0);
  gtk_widget_show(label);

  /* radio_table */
  radio_table = table;

  l_idx = col +1;
  l_idy = row;

  /* radio button MORPH */
  radio_button = gtk_radio_button_new_with_label ( radio_group, _("Morph") );
  radio_group = gtk_radio_button_get_group ( GTK_RADIO_BUTTON (radio_button) );
  gtk_table_attach ( GTK_TABLE (radio_table), radio_button, l_idx, l_idx+1, l_idy, l_idy+1
                   , GTK_FILL | GTK_EXPAND, 0, 0, 0);
  mgup->render_mode_morph_toggle = radio_button;

  l_radio_pressed = (mgup->mgpp->render_mode == GAP_MORPH_RENDER_MODE_MORPH);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (radio_button), l_radio_pressed);
  gimp_help_set_help_data(radio_button
                         , _("Render morph transition (warp forward, warp backward and cross fade)")
                         , NULL);

  gtk_widget_show (radio_button);
  g_object_set_data( G_OBJECT(radio_button), "mgup", mgup);
  g_signal_connect ( G_OBJECT (radio_button), "toggled",
                     G_CALLBACK (on_radio_render_mode_callback),
                     (gpointer)GAP_MORPH_RENDER_MODE_MORPH);


  l_idx++;

  /* radio button WARP */
  radio_button = gtk_radio_button_new_with_label ( radio_group, _("Warp") );
  radio_group = gtk_radio_button_get_group ( GTK_RADIO_BUTTON (radio_button) );
  gtk_table_attach ( GTK_TABLE (radio_table), radio_button, l_idx, l_idx+1, l_idy, l_idy+1
                   , GTK_FILL | GTK_EXPAND, 0, 0, 0);
  mgup->render_mode_warp_toggle = radio_button;

  l_radio_pressed = (mgup->mgpp->render_mode == GAP_MORPH_RENDER_MODE_WARP);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (radio_button), l_radio_pressed);
  gimp_help_set_help_data(radio_button
                         , _("Render forward warp transitions only")
                         , NULL);

  gtk_widget_show (radio_button);
  g_object_set_data( G_OBJECT(radio_button), "mgup", mgup);
  g_signal_connect ( G_OBJECT (radio_button), "toggled",
                     G_CALLBACK (on_radio_render_mode_callback),
                     (gpointer)GAP_MORPH_RENDER_MODE_WARP);

}  /* end p_radio_create_render_mode */


/* -----------------------------
 * p_create_subwin
 * -----------------------------
 */
static GtkWidget *
p_create_subwin(GapMorphSubWin *swp
               , const char *title
               , GapMorphGUIParams *mgup
               , GtkWidget *master_hbox
               )
{
  GtkWidget *frame;
  GtkWidget *vbox;
  GtkWidget *aspect_frame;
  GtkWidget *da_widget;
  GtkWidget *table;
  GtkWidget *pv_table;
  GtkWidget *spinbutton;
  GtkWidget *button;
  GtkWidget *combo;
  GtkWidget *label;
  gint       row;
  GtkObject *adj;

  /*  the frame  */
  frame = gimp_frame_new (title);
  gtk_box_pack_start (GTK_BOX (master_hbox), frame, FALSE, FALSE, 0);

  /* the vbox */
  vbox = gtk_vbox_new (FALSE, 4);
  gtk_container_add (GTK_CONTAINER (frame), vbox);
  gtk_widget_show (vbox);


  /* table */
  table = gtk_table_new (2, 11, FALSE);
  gtk_widget_show (table);
  gtk_box_pack_start (GTK_BOX (vbox), table, FALSE, FALSE, 0);

  row = 0;

  /* the layer selection combobox */
//   label = gtk_label_new( _("Layer:"));
// 
//   gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
//   gtk_table_attach(GTK_TABLE(table), label, 0, 1, row, row+1
//                   , GTK_FILL, 0, 4, 0);
//   gtk_widget_show(label);

  combo = gimp_drawable_combo_box_new (p_imglayer_constrain, NULL);
  gtk_table_attach(GTK_TABLE(table), combo, 1, 10, row, row+1,
                   GTK_EXPAND | GTK_FILL, 0, 0, 0);

  if(swp->src_flag)
  {
    gimp_help_set_help_data(combo,
                       _("Select the source layer")
                       , NULL);
  }
  else
  {
    gimp_help_set_help_data(combo,
                       _("Select the destination layer ")
                       , NULL);
  }

  gtk_widget_show(combo);
  swp->combo = combo;
  /* p_refresh_layer_menu(swp, mgup); */ /* is done later after the pview widget is initialized */
  gtk_widget_show(combo);


  row++;


  /* the x coordinate label */
  label = gtk_label_new (_("X:"));
  gtk_widget_show (label);
  gtk_table_attach (GTK_TABLE (table), label, 0, 1, row, row+1,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 4, 0);
  gtk_misc_set_alignment (GTK_MISC (label), 0, 0.5);

  /* the x coordinate spinbutton */
  adj = gtk_adjustment_new ( 0
                           , 0
                           , 10000       /* constrain to image width is done later */
                           , 1, 10, 0);
  swp->x_spinbutton_adj = adj;
  spinbutton = gtk_spin_button_new (GTK_ADJUSTMENT (adj), 1, 0);
  gtk_widget_show (spinbutton);

  gtk_table_attach (GTK_TABLE (table), spinbutton, 1, 2, row, row+1,
                    (GtkAttachOptions) (0),
                    (GtkAttachOptions) (0),
                     0, 0);

  gtk_widget_set_size_request (spinbutton, 60, -1);
  gimp_help_set_help_data (spinbutton, _("Morphpoint X coordinate"), NULL);
  g_object_set_data( G_OBJECT(adj), "swp", swp);
  g_signal_connect (G_OBJECT (adj), "value_changed"
                   , G_CALLBACK (on_koord_spinbutton_changed)
                   , (gpointer)GAP_MORPH_KOORD_WPX);



  /* the y coordinate label */
  label = gtk_label_new (_("Y:"));
  gtk_widget_show (label);
  gtk_table_attach (GTK_TABLE (table), label, 2, 3, row, row+1,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 4, 0);
  gtk_misc_set_alignment (GTK_MISC (label), 0, 0.5);


  /* the y coordinate spinbutton */
  adj = gtk_adjustment_new ( 0
                           , 0
                           , 10000       /* constrain to image height is done later */
                           , 1, 10, 0);
  swp->y_spinbutton_adj = adj;
  spinbutton = gtk_spin_button_new (GTK_ADJUSTMENT (adj), 1, 0);
  gtk_widget_show (spinbutton);

  gtk_table_attach (GTK_TABLE (table), spinbutton, 3, 4, row, row+1,
                    (GtkAttachOptions) (0),
                    (GtkAttachOptions) (0),
                     0, 0);

  gtk_widget_set_size_request (spinbutton, 60, -1);
  gimp_help_set_help_data (spinbutton, _("Morphpoint Y coordinate"), NULL);
  g_object_set_data( G_OBJECT(adj), "swp", swp);
  g_signal_connect (G_OBJECT (adj), "value_changed"
                   , G_CALLBACK (on_koord_spinbutton_changed)
                   , (gpointer)GAP_MORPH_KOORD_WPY);


  /* Fit Zoom Button */
  button = gtk_button_new_from_stock ( _("Fit Zoom") );
  gtk_table_attach( GTK_TABLE(table), button, 4, 5, row, row+1,
                    GTK_FILL, 0, 8, 0 );
  gimp_help_set_help_data(button,
                       _("Show the whole layer."
                         " (by adjusting zoom to fit into preview).")
                       , NULL);
  gtk_widget_show (button);
  g_signal_connect (G_OBJECT (button), "pressed",
                    G_CALLBACK (on_fit_zoom_pressed_callback),
                    swp);

  if(!swp->src_flag)
  {
    /* there is just one total_points display (always in the dst frame) */

    /* the current Point label */
    label = gtk_label_new (_("Point:"));
    gtk_widget_show (label);
    gtk_table_attach (GTK_TABLE (table), label, 6, 7, row, row+1,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 4, 0);
    gtk_misc_set_alignment (GTK_MISC (label), 0, 0.5);


    /* the current point spinbutton */
    adj = gtk_adjustment_new ( 0
                           , 1
                           , 10000       /* constrain to image height is done later */
                           , 1, 10, 0);
    mgup->curr_point_spinbutton_adj = adj;
    spinbutton = gtk_spin_button_new (GTK_ADJUSTMENT (adj), 1, 0);
    gtk_widget_show (spinbutton);

    gtk_table_attach (GTK_TABLE (table), spinbutton, 7, 8, row, row+1,
                    (GtkAttachOptions) (0),
                    (GtkAttachOptions) (0),
                     0, 0);

    gtk_widget_set_size_request (spinbutton, 60, -1);
    gimp_help_set_help_data (spinbutton, _("Number of the current point"), NULL);
    g_signal_connect (G_OBJECT (adj), "value_changed"
                   , G_CALLBACK (on_curr_point_spinbutton_changed)
                   , mgup);

    /* the number_of_points label */
    label = gtk_label_new (_("of total:"));
    gtk_widget_show (label);
    gtk_table_attach (GTK_TABLE (table), label, 8, 9, row, row+1,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 4, 0);
    gtk_misc_set_alignment (GTK_MISC (label), 0, 0.5);

    /* the number_of_points label */
    label = gtk_label_new (_("001"));
    mgup->toal_points_label = label;
    gtk_widget_show (label);
    gtk_table_attach (GTK_TABLE (table), label, 9, 10, row, row+1,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 4, 0);
    gtk_misc_set_alignment (GTK_MISC (label), 0, 0.5);
  }



  /* the pv_table (for aspect_frame and vscale hscale)*/
  pv_table = gtk_table_new(2, 2, FALSE);
  gtk_widget_show (pv_table);
  gtk_box_pack_start (GTK_BOX (vbox), pv_table, FALSE, FALSE, 0);


  /* aspect_frame is the CONTAINER for the preview */
  aspect_frame = gtk_aspect_frame_new (NULL   /* without label */
                                      , 0.5   /* xalign center */
                                      , 0.5   /* yalign center */
                                      , global_morph_pv_width / global_morph_pv_height     /* ratio */
                                      , TRUE  /* obey_child */
                                      );


  /* PREVIEW DRAWING AREA */
  /* the preview drawing_area_widget */
  /* ############################### */
  swp->pv_ptr = gap_pview_new(global_morph_pv_width
                            , global_morph_pv_height
                            , GAP_MORPH_CHECK_SIZE
                            , aspect_frame
                            );
  da_widget = swp->pv_ptr->da_widget;
  gtk_container_add (GTK_CONTAINER (aspect_frame), da_widget);
  gtk_table_attach (GTK_TABLE (pv_table), aspect_frame, 0, 1, 0, 1,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (GTK_FILL), 0, 0);

  gtk_widget_show (aspect_frame);
  gtk_widget_show (da_widget);



  gtk_widget_realize (da_widget);
  /*  p_update_pviewsize(gpp); */

  /* the call to gtk_widget_set_events results in WARNING:
   * (gimp-1.3:13789): Gtk-CRITICAL **: file gtkwidget.c: line 5257 (gtk_widget_set_events): assertion `!GTK_WIDGET_REALIZED (widget)' failed
   *
   * must use gtk_widget_add_events because the widget is already realized at this time
   */
  gtk_widget_add_events( GTK_WIDGET(da_widget), GAP_MORPH_PVIEW_MASK );
  g_signal_connect (G_OBJECT (da_widget), "event",
                    G_CALLBACK (on_pview_events),
                    swp);


  {
    gdouble upper_max;
    gdouble initial_val;
    GtkWidget *hscale;
    GtkWidget *vscale;

    upper_max = 1;
    initial_val = 1;


    /* the vscale */
    adj = gtk_adjustment_new (initial_val
                             , 1
                             , upper_max
                             , 1.0, 1.0, 0.0
                            );
    vscale = gtk_vscrollbar_new (GTK_ADJUSTMENT(adj));

    gtk_range_set_update_policy (GTK_RANGE (vscale), GTK_UPDATE_DELAYED); /* GTK_UPDATE_CONTINUOUS */
    gtk_table_attach (GTK_TABLE (pv_table), vscale, 1, 2, 0, 1,
                    (GtkAttachOptions) (0),
                    (GtkAttachOptions) (GTK_FILL), 0, 0);
    g_signal_connect (adj, "value_changed",
                      G_CALLBACK (on_hvscale_changed_callback),
                      swp);
    gtk_widget_show (vscale);

    swp->vscale_adj = adj;
    swp->vscale = vscale;

    /* the hscale */
    adj = gtk_adjustment_new (initial_val
                             , 1
                             , upper_max
                             , 1.0, 1.0, 0.0
                            );
    hscale = gtk_hscrollbar_new (GTK_ADJUSTMENT(adj));

    gtk_range_set_update_policy (GTK_RANGE (hscale), GTK_UPDATE_DELAYED); /* GTK_UPDATE_CONTINUOUS */
    gtk_table_attach (GTK_TABLE (pv_table), hscale, 0, 1, 1, 2,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);
    g_signal_connect (adj, "value_changed",
                      G_CALLBACK (on_hvscale_changed_callback),
                      swp);
    gtk_widget_show (hscale);

    swp->hscale_adj = adj;
    swp->hscale = hscale;

    p_hvscale_adj_set_limits(swp);
  }

  return(frame);
}  /* end p_create_subwin */


/* -----------------------------
 * gap_morph_create_dialog
 * -----------------------------
 */
static void
gap_morph_create_dialog(GapMorphGUIParams *mgup)
{
  GtkWidget *dlg;
  GtkWidget *main_vbox;
  GtkWidget *hbox;
  GtkWidget *vbox;
  GtkWidget *frame;
  GtkWidget *table;
  GtkWidget *label;
  GtkWidget *button;
  GtkWidget *color_button;
  GtkWidget *spinbutton;
  GtkWidget *checkbutton;
  gint       row;
  GtkObject *adj;


  global_morph_pv_width = gap_base_get_gimprc_int_value(GAP_MORPH_PREVIEW_WIDTH
                                 , GAP_MORPH_PV_WIDTH_DEFAULT
				 , 120
				 , 1280
				 );
  global_morph_pv_height = gap_base_get_gimprc_int_value(GAP_MORPH_PREVIEW_HEIGHT
                                 , GAP_MORPH_PV_HEIGHT_DEFAULT
				 , 100
				 , 1024
				 );
  

  dlg = gimp_dialog_new (_("Morph / Warp"), GAP_MORPH_PLUGIN_NAME,
                         NULL, 0,
                         gimp_standard_help_func, GAP_MORPH_HELP_ID,

                         GIMP_STOCK_RESET, GAP_MORPH_RESPONSE_RESET,
                         _("Swap"),        GAP_MORPH_RESPONSE_SWAP,
                         GTK_STOCK_SAVE,   GAP_MORPH_RESPONSE_SAVE,
                         GTK_STOCK_OPEN,   GAP_MORPH_RESPONSE_LOAD,
                         GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                         GTK_STOCK_OK,     GTK_RESPONSE_OK,
                         NULL);
  gtk_window_set_type_hint (dlg, GDK_WINDOW_TYPE_HINT_NORMAL);

  mgup->shell = dlg;
  mgup->src_win.startup_flag = TRUE;
  mgup->dst_win.startup_flag = TRUE;
  mgup->upd_timertag = -1;
  mgup->wp_filesel = NULL;
  mgup->wp_save_mode = FALSE;
  mgup->curr_point_spinbutton_adj = NULL;
  mgup->toal_points_label = NULL;
  mgup->workpoint_file_lower_label = NULL;
  mgup->workpoint_file_upper_label = NULL;
  mgup->warp_info_label = NULL;
  mgup->show_lines_checkbutton = NULL;
  mgup->src_win.x_spinbutton_adj = NULL;
  mgup->src_win.y_spinbutton_adj = NULL;
  mgup->dst_win.x_spinbutton_adj = NULL;
  mgup->dst_win.y_spinbutton_adj = NULL;
  mgup->src_win.layer_id_ptr = NULL;
  mgup->dst_win.layer_id_ptr = NULL;
  gimp_rgb_set(&mgup->pointcolor, 0.1, 1.0, 0.1); /* startup with GREEN pointcolor */
  gimp_rgb_set_alpha(&mgup->pointcolor, 1.0);
  gimp_rgb_set(&mgup->curr_pointcolor, 1.0, 1.0, 0.1); /* startup with YELLOW color */
  gimp_rgb_set_alpha(&mgup->curr_pointcolor, 1.0);
  mgup->old_src_layer_width  = 0;
  mgup->old_src_layer_height = 0;
  mgup->old_dst_layer_width  = 0;
  mgup->old_dst_layer_height = 0;
  mgup->op_mode = GAP_MORPH_OP_MODE_SET;
  mgup->show_in_x = -1;
  mgup->show_in_y = -1;
  mgup->num_shapepoints = 64;

  g_signal_connect (G_OBJECT (mgup->shell), "response",
                    G_CALLBACK (p_morph_response),
                    mgup);


  main_vbox = gtk_vbox_new (FALSE, 4);
  gtk_container_set_border_width (GTK_CONTAINER (main_vbox), 6);
  gtk_container_add (GTK_CONTAINER (GTK_DIALOG (dlg)->vbox), main_vbox);

  /*  the frame  */
  frame = gimp_frame_new (NULL);
  gtk_box_pack_start (GTK_BOX (main_vbox), frame, FALSE, FALSE, 0);
  gtk_widget_show (frame);



  /* the vbox */
  vbox = gtk_vbox_new (FALSE, 4);
  gtk_container_add (GTK_CONTAINER (frame), vbox);
  gtk_widget_show (vbox);

  /* the hbox */
  hbox = gtk_hbox_new (FALSE, 4);
  gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 0);
  gtk_widget_show (hbox);

  /* make sure that we have at least one workpoint */
  if(mgup->mgpp->master_wp_list == NULL)
  {
    mgup->mgpp->master_wp_list = gap_morph_dlg_new_workpont(0,0,0,0);
  }

  /* src_frame */
  mgup->src_win.mgup = mgup;
  mgup->src_win.src_flag = TRUE;
  mgup->src_win.startup_flag = FALSE;
  mgup->src_win.upd_request = GAP_MORPH_DLG_UPD_REQUEST_NONE;
  mgup->src_win.pview_scrolling = FALSE;
  mgup->src_win.max_zoom = 1.0;
  mgup->src_win.zoom = 1.0;
  mgup->src_win.offs_x = 0;
  mgup->src_win.offs_y = 0;
  mgup->src_win.layer_id_ptr = &mgup->mgpp->osrc_layer_id;
  p_set_current_workpoint_no_refresh(&mgup->src_win, mgup->mgpp->master_wp_list);

  frame = p_create_subwin(&mgup->src_win, _("Source"), mgup, hbox);
  gtk_widget_show (frame);

  /* dst_frame */
  mgup->dst_win.mgup = mgup;
  mgup->dst_win.src_flag = FALSE;
  mgup->dst_win.upd_request = GAP_MORPH_DLG_UPD_REQUEST_NONE;
  mgup->dst_win.pview_scrolling = FALSE;
  mgup->dst_win.max_zoom = 1.0;
  mgup->dst_win.zoom = 1.0;
  mgup->dst_win.offs_x = 0;
  mgup->dst_win.offs_y = 0;
  mgup->dst_win.layer_id_ptr = &mgup->mgpp->fdst_layer_id;
  p_set_current_workpoint_no_refresh(&mgup->dst_win, mgup->mgpp->master_wp_list);

  frame = p_create_subwin(&mgup->dst_win, _("Destination"), mgup, hbox);
  gtk_widget_show (frame);

  p_refresh_layer_menu(&mgup->src_win, mgup);
  p_refresh_layer_menu(&mgup->dst_win, mgup);
  mgup->src_win.startup_flag = FALSE;
  mgup->dst_win.startup_flag = FALSE;
  p_set_current_workpoint(&mgup->dst_win, mgup->mgpp->master_wp_list);


  /* table 5rows 14cols */
  table = gtk_table_new (5, 14, FALSE);
  gtk_widget_show (table);
  gtk_box_pack_start (GTK_BOX (vbox), table, FALSE, FALSE, 0);

  row = 0;


  /* the nubner of ShapePoints label */
  label = gtk_label_new (_("ShapePoints:"));
  gtk_widget_show (label);
  gtk_misc_set_alignment (GTK_MISC (label), 0, 0.5);
  gtk_table_attach (GTK_TABLE (table), label, 0, 1, row, row+1,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);

  /* num_shapepoints spinbutton */
  adj = gtk_adjustment_new ( mgup->num_shapepoints
                           , 1
                           , 1000
                           , 1, 10, 0);
  mgup->num_shapepoints_adj = adj;
  spinbutton = gtk_spin_button_new (GTK_ADJUSTMENT (adj), 1, 0);
  gtk_widget_show (spinbutton);

  gtk_table_attach (GTK_TABLE (table), spinbutton, 1, 2, row, row+1,
                    (GtkAttachOptions) (0),
                    (GtkAttachOptions) (0),
                     0, 0);

  gtk_widget_set_size_request (spinbutton, 80, -1);
  gimp_help_set_help_data (spinbutton, _("Number of workpoints to create when Shape button is pressed"), NULL);
  g_signal_connect (G_OBJECT (adj), "value_changed"
                   , G_CALLBACK (gimp_int_adjustment_update)
                   , &mgup->num_shapepoints);

  /* Shape Button */
  button = gtk_button_new_from_stock ( _("Shape") );
  gtk_table_attach( GTK_TABLE(table), button, 2, 4, row, row+1,
                    GTK_FILL, 0, 0, 0 );
  gimp_help_set_help_data(button,
                       _("Create N workpoints following the outline shape of the layer."
                         "the simple shape detection is looking for non-transparent pixels."
                         "CTRL-click uses an edge detection based shape detection algorithm "
                         "that is capable to operate on opaque images."
                         "SHIFT-click: adds the new points and keeps the old points")
                       , NULL);
  gtk_widget_show (button);
  g_signal_connect (G_OBJECT (button), "button_press_event",
                    G_CALLBACK (on_wp_shape_button_clicked),
                    mgup);



  /* RADIO Buttons (attaches to 6 Columns) */
  p_radio_create_op_mode(table, row, 4, mgup);


  /* Swap Windows Button */
//   button = gtk_button_new_from_stock (_("Swap"));
//   gtk_table_attach( GTK_TABLE(table), button, 11, 12, row, row+1,
//                     GTK_FILL, 0, 0, 0 );
//   gimp_help_set_help_data(button,
//                        _("Exchange source and destination")
//                        , NULL);
//   gtk_widget_show (button);
//   g_signal_connect (G_OBJECT (button), "pressed",
//                     G_CALLBACK (on_swap_button_pressed_callback),
//                     mgup);


  row++;

  /* the deform affect radius label */
  label = gtk_label_new (_("Radius:"));
  gtk_widget_show (label);
  gtk_misc_set_alignment (GTK_MISC (label), 0, 0.5);
  gtk_table_attach (GTK_TABLE (table), label, 0, 1, row, row+1,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);

  /* affect radius spinbutton */
  adj = gtk_adjustment_new ( mgup->mgpp->affect_radius
                           , 0
                           , 10000
                           , 1, 10, 0);
  mgup->affect_radius_spinbutton_adj = adj;
  spinbutton = gtk_spin_button_new (GTK_ADJUSTMENT (adj), 1, 0);
  gtk_widget_show (spinbutton);

  gtk_table_attach (GTK_TABLE (table), spinbutton, 1, 2, row, row+1,
                    (GtkAttachOptions) (0),
                    (GtkAttachOptions) (0),
                     0, 0);

  gtk_widget_set_size_request (spinbutton, 80, -1);
  gimp_help_set_help_data (spinbutton, _("Deform radius in pixels."
                                         " Each workpoint causes a move-deform operation"
                                         " within this affect radius.")
                                         , NULL);
  g_signal_connect (G_OBJECT (adj), "value_changed"
                   , G_CALLBACK (gimp_double_adjustment_update)
                   , &mgup->mgpp->affect_radius);

  /* the deform intensity label */
  label = gtk_label_new (_("Intensity:"));
  gtk_widget_show (label);
  gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
  gtk_table_attach (GTK_TABLE (table), label, 4, 5, row, row+1,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 8, 0);

  /* intensity spinbutton */
  adj = gtk_adjustment_new ( mgup->mgpp->gravity_intensity
                           , 1
                           , 5
                           , .1, 1, 0);
  mgup->gravity_intensity_spinbutton_adj = adj;
  spinbutton = gtk_spin_button_new (GTK_ADJUSTMENT (adj), 1, 2);
  mgup->gravity_intensity_spinbutton = spinbutton;
  gtk_widget_show (spinbutton);

  gtk_widget_set_size_request (spinbutton, 80, -1);
  gtk_table_attach (GTK_TABLE (table), spinbutton, 5, 7, row, row+1,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0),
                     0, 0);

  gimp_help_set_help_data (spinbutton, _("Deform intensity.")
                                         , NULL);
  g_signal_connect (G_OBJECT (adj), "value_changed"
                   , G_CALLBACK (gimp_double_adjustment_update)
                   , &mgup->mgpp->gravity_intensity);


  /* the use_intensity checkbutton */
  checkbutton = gtk_check_button_new_with_label ( _("Use Intensity"));
  mgup->use_gravity_checkbutton = checkbutton;
  gtk_widget_show (checkbutton);
  gtk_table_attach( GTK_TABLE(table), checkbutton, 8, 10, row, row+1,
                    GTK_FILL, 0, 0, 0 );
  g_signal_connect (checkbutton, "toggled",
                    G_CALLBACK (on_use_gravity_toggled_callback),
                    mgup);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (checkbutton), mgup->mgpp->use_gravity);
  gimp_help_set_help_data(checkbutton,
                       _("ON: Descending deform action from workpoint (full) to radius (zero). Descend by power of intensity."
                         "OFF: Linear deform action inside the radius")
                       , NULL);


  /* Load Workpoints Button */
//   button = gtk_button_new_from_stock (GTK_STOCK_OPEN );
//   gtk_table_attach( GTK_TABLE(table), button, 11, 12, row, row+1,
//                     GTK_FILL, 0, 0, 0 );
//   gimp_help_set_help_data(button,
//                        _("Load morph workpoints from file. SHIFT-click: define filename of Pointset B")
//                        , NULL);
//   gtk_widget_show (button);
//   g_signal_connect (G_OBJECT (button), "button_press_event",
//                     G_CALLBACK (on_wp_load_button_clicked),
//                     mgup);

//   /* Save Workpoints Button */
//   button = gtk_button_new_from_stock ( GTK_STOCK_SAVE );
//   gtk_table_attach( GTK_TABLE(table), button, 12, 13, row, row+1,
//                     GTK_FILL, 0, 0, 0 );
//   gimp_help_set_help_data(button,
//                        _("Save morph workpoints to file. SHIFT-click: define filename of Pointset B")
//                        , NULL);
//   gtk_widget_show (button);
//   g_signal_connect (G_OBJECT (button), "button_press_event",
//                     G_CALLBACK (on_wp_save_button_clicked),
//                     mgup);


  row++;


  /* the tween_steps label */
  label = gtk_label_new (_("Steps:"));
  gtk_widget_show (label);
  gtk_misc_set_alignment (GTK_MISC (label), 0, 0.5);
  gtk_table_attach (GTK_TABLE (table), label, 0, 1, row, row+1,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);

  /* tween_steps_spinbutton */
  adj = gtk_adjustment_new ( mgup->mgpp->tween_steps
                           , 1
                           , 1000
                           , 1, 10, 0);
  mgup->tween_steps_spinbutton_adj = adj;
  spinbutton = gtk_spin_button_new (GTK_ADJUSTMENT (adj), 1, 0);
  gtk_widget_show (spinbutton);

  gtk_table_attach (GTK_TABLE (table), spinbutton, 1, 2, row, row+1,
                    (GtkAttachOptions) (0),
                    (GtkAttachOptions) (0),
                     0, 0);

  gtk_widget_set_size_request (spinbutton, 80, -1);
  gimp_help_set_help_data (spinbutton, _("Number of layers to create or modify."
                                         " Steps refers to N layers under the destination layer."
                                         " Steps is ignored if render mode is warp"
                                         " and source and destination are different layers"
                                         " of the same image"), NULL);
  g_signal_connect (G_OBJECT (adj), "value_changed"
                   , G_CALLBACK (gimp_int_adjustment_update)
                   , &mgup->mgpp->tween_steps);

  /* the pointcolor colorbutton */
  color_button = gimp_color_button_new (_("Pointcolor"),
                                  25, 12,                     /* WIDTH, HEIGHT, */
                                  &mgup->pointcolor,
                                  GIMP_COLOR_AREA_FLAT);

  gtk_table_attach(GTK_TABLE(table), color_button, 2, 3, row, row+1
                    , 0, 0, 4, 0);
  gtk_widget_show (color_button);
  gimp_help_set_help_data(color_button,
                         _("Set color for the morph workpoints")
                         , NULL);

  g_object_set_data( G_OBJECT(color_button), "mgup", mgup);
  g_signal_connect (color_button, "color_changed",
                    G_CALLBACK (on_pointcolor_button_changed),
                    &mgup->pointcolor);



  /* the currentcolor colorbutton */
  color_button = gimp_color_button_new (_("Current Pointcolor"),
                                  25, 12,                     /* WIDTH, HEIGHT, */
                                  &mgup->curr_pointcolor,
                                  GIMP_COLOR_AREA_FLAT);
  gtk_table_attach(GTK_TABLE(table), color_button, 3, 4, row, row+1
                    , 0, 0, 4, 0);
  gtk_widget_show (color_button);
  gimp_help_set_help_data(color_button,
                         _("Set color for the current morph workpoint")
                         , NULL);

  g_object_set_data( G_OBJECT(color_button), "mgup", mgup);
  g_signal_connect (color_button, "color_changed",
                    G_CALLBACK (on_pointcolor_button_changed),
                    &mgup->curr_pointcolor);







  /* the render_mode RADIO Buttons (attaches to 3 Columns: 4,5,6 ) */
  p_radio_create_render_mode(table, row, 4, mgup);

  /* the multiple pointsets checkbutton */
  checkbutton = gtk_check_button_new_with_label ( _("Multiple Pointsets"));
  mgup->have_workpointsets_checkbutton = checkbutton;
#ifdef GAP_MORPH_DEBUG_FEATURES
  gtk_widget_show (checkbutton);
#endif
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (checkbutton), mgup->mgpp->have_workpointsets);
  gtk_table_attach( GTK_TABLE(table), checkbutton, 11, 13, row, row+1,
                    GTK_FILL, 0, 0, 0 );
  g_signal_connect (checkbutton, "toggled",
                    G_CALLBACK (on_have_workpointsets_toggled_callback),
                    mgup);
  gimp_help_set_help_data(checkbutton,
                       _("ON: use 2 or more pointsets from file. "
                         "Please create and save the pointsets first, "
                         "using filenames with a 2-digit numberpart before the extension "
                         "(points_01.txt, points_02.txt, points_03.txt) "
                         "then open and SHIFT open the first and last pointset\n"
                         "OFF: use current set of workpoints")
                       , NULL);


  row++;



  /* the number_of_points label */
  label = gtk_label_new ("---");
  mgup->warp_info_label = label;
#ifdef GAP_MORPH_DEBUG_FEATURES
  gtk_widget_show (label);
#endif
  gtk_table_attach (GTK_TABLE (table), label, 0, 2, row, row+1,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 4, 0);
  gtk_misc_set_alignment (GTK_MISC (label), 0, 0.5);

  row++;

  /* the lower workpoint label */
  label = gtk_label_new (_("Pointset A:"));
  mgup->workpoint_lower_label = label;
  gtk_misc_set_alignment (GTK_MISC (label), 0, 0.5);
  gtk_table_attach (GTK_TABLE (table), label, 0, 1, row, row+1,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);

  /* the lower workpoint file label */
  label = gtk_label_new (&mgup->mgpp->workpoint_file_lower[0]);
  mgup->workpoint_file_lower_label = label;
  gtk_misc_set_alignment (GTK_MISC (label), 0, 0.5);
  gtk_table_attach (GTK_TABLE (table), label, 1, 12, row, row+1,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);

  row++;

  /* the upper workpoint label */
  label = gtk_label_new (_("Pointset B:"));
  mgup->workpoint_upper_label = label;
  gtk_misc_set_alignment (GTK_MISC (label), 0, 0.5);
  gtk_table_attach (GTK_TABLE (table), label, 0, 1, row, row+1,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);

  /* the upper workpoint file label */
  label = gtk_label_new (&mgup->mgpp->workpoint_file_upper[0]);
  mgup->workpoint_file_upper_label = label;
  gtk_misc_set_alignment (GTK_MISC (label), 0, 0.5);
  gtk_table_attach (GTK_TABLE (table), label, 1, 12, row, row+1,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);

  row++;

  /* the LOCATE label */
  label = gtk_label_new (_("Locate:"));
  gtk_widget_show (label);
  gtk_misc_set_alignment (GTK_MISC (label), 0, 0.5);
  gtk_table_attach (GTK_TABLE (table), label, 0, 1, row, row+1,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);

  /* locate move radius spinbutton */
  adj = gtk_adjustment_new ( mgup->mgpp->locateDetailMoveRadius
                           , 0
                           , 200
                           , 1, 10, 0);
  mgup->locate_radius_spinbutton_adj = adj;
  spinbutton = gtk_spin_button_new (GTK_ADJUSTMENT (adj), 1, 0);
  gtk_widget_show (spinbutton);

  gtk_table_attach (GTK_TABLE (table), spinbutton, 1, 2, row, row+1,
                    (GtkAttachOptions) (0),
                    (GtkAttachOptions) (0),
                     0, 0);

  gtk_widget_set_size_request (spinbutton, 80, -1);
  gimp_help_set_help_data (spinbutton, _("Locate radius in pixels."
                                         " radius for automatically point locate feature "
                                         " triggered by CTRL when setting workpoints."
                                         "(Not relevant for rendering)")
                                         , NULL);
  g_signal_connect (G_OBJECT (adj), "value_changed"
                   , G_CALLBACK (gimp_int_adjustment_update)
                   , &mgup->mgpp->locateDetailMoveRadius);

  /* locate shape radius spinbutton */
  adj = gtk_adjustment_new ( mgup->mgpp->locateDetailShapeRadius
                           , GAP_LOCATE_MIN_REF_SHAPE_RADIUS
                           , GAP_LOCATE_MAX_REF_SHAPE_RADIUS
                           , 1, 10, 0);
  mgup->locate_shape_radius_spinbutton_adj = adj;
  spinbutton = gtk_spin_button_new (GTK_ADJUSTMENT (adj), 1, 0);
  gtk_widget_show (spinbutton);

  gtk_table_attach (GTK_TABLE (table), spinbutton, 2, 3, row, row+1,
                    (GtkAttachOptions) (0),
                    (GtkAttachOptions) (0),
                     0, 0);

  gtk_widget_set_size_request (spinbutton, 80, -1);
  gimp_help_set_help_data (spinbutton, _("Locate Shaperadius in pixels."
                                         " Defines shape size as area around workpoint to be compared "
                                         " when loacting corresponding coordinate"
                                         "(Not relevant for rendering)")
                                         , NULL);
  g_signal_connect (G_OBJECT (adj), "value_changed"
                   , G_CALLBACK (gimp_int_adjustment_update)
                   , &mgup->mgpp->locateDetailShapeRadius);



  /* locate edge detection threshold spinbutton */
  adj = gtk_adjustment_new ( mgup->mgpp->edgeColordiffThreshold
                           , 0.01
                           , 0.35
                           , 0.01, 0.1, 0.0);
  mgup->locate_edge_threshold_spinbutton_adj = adj;
  spinbutton = gtk_spin_button_new (GTK_ADJUSTMENT (adj), 0.01, 2);
  gtk_widget_show (spinbutton);

  gtk_table_attach (GTK_TABLE (table), spinbutton, 3, 4, row, row+1,
                    (GtkAttachOptions) (0),
                    (GtkAttachOptions) (0),
                     0, 0);

  gtk_widget_set_size_request (spinbutton, 80, -1);
  gimp_help_set_help_data (spinbutton, _("Edge detection threshold"
                                         " for automatically point locate feature "
                                         " triggered by CTRL when setting workpoints."
                                         "(Not relevant for rendering)")
                                         , NULL);
  g_signal_connect (G_OBJECT (adj), "value_changed"
                   , G_CALLBACK (gimp_double_adjustment_update)
                   , &mgup->mgpp->edgeColordiffThreshold);



  /* the create tween checkbutton */
  checkbutton = gtk_check_button_new_with_label ( _("Create Layers"));
  mgup->create_tween_layers_checkbutton = checkbutton;
  gtk_widget_show (checkbutton);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (checkbutton), mgup->mgpp->create_tween_layers);
  gtk_table_attach( GTK_TABLE(table), checkbutton, 5, 6, row, row+1,
                    GTK_FILL, 0, 0, 0 );
  g_signal_connect (checkbutton, "toggled",
                    G_CALLBACK (on_create_tween_layers_toggled_callback),
                    mgup);
  gimp_help_set_help_data(checkbutton,
                       _("ON: Create specified number of tween layers. "
                         "OFF: Operate on existing layers below the destination layer")
                       , NULL);

  /* the use_quality_wp_selection checkbutton */
  checkbutton = gtk_check_button_new_with_label ( _("Quality"));
  mgup->use_quality_wp_selection_checkbutton = checkbutton;
  gtk_widget_show (checkbutton);
  gtk_table_attach( GTK_TABLE(table), checkbutton, 6, 7, row, row+1,
                    GTK_FILL, 0, 0, 0 );
  g_signal_connect (checkbutton, "toggled",
                    G_CALLBACK (on_use_quality_wp_selection_toggled_callback),
                    mgup);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (checkbutton), mgup->mgpp->use_quality_wp_selection);
  gimp_help_set_help_data(checkbutton,
                       _("ON: Use quality workpoint selection algorithm."
                         "OFF: Use fast workpoint selection algorithm.")
                       , NULL);

  /* the show lines checkbutton */
  checkbutton = gtk_check_button_new_with_label ( _("Lines"));
  mgup->show_lines_checkbutton = checkbutton;
  gtk_widget_show (checkbutton);
  gtk_table_attach( GTK_TABLE(table), checkbutton, 7, 8, row, row+1,
                    GTK_FILL, 0, 0, 0 );
  g_signal_connect (checkbutton, "toggled",
                    G_CALLBACK (on_show_lines_toggled_callback),
                    mgup);
  gimp_help_set_help_data(checkbutton,
                       _("Show movement vector lines in the destination preview")
                       , NULL);


  /* the progress bar */
  mgup->progressBar = gtk_progress_bar_new ();
  gtk_progress_bar_set_text(GTK_PROGRESS_BAR(mgup->progressBar), " ");
  gtk_widget_hide (mgup->progressBar);
  gtk_table_attach( GTK_TABLE(table), mgup->progressBar, 8, 10, row, row+1,
                    GTK_FILL, 0, 0, 0 );




  /*  Show the main container  */
  gtk_widget_show (main_vbox);

  /* force have_workpointsets callback to show/hide workpoint lables
   * (those labels are only visible when multiple pontsets are enabled
   */
  on_have_workpointsets_toggled_callback(mgup->have_workpointsets_checkbutton, mgup);
  on_use_gravity_toggled_callback(mgup->use_gravity_checkbutton, mgup);
  on_use_quality_wp_selection_toggled_callback(mgup->use_quality_wp_selection_checkbutton, mgup);



}  /* end gap_morph_create_dialog */


/* -----------------------------
 * gap_morph_dialog
 * -----------------------------
 */
gboolean
gap_morph_dialog(GapMorphGlobalParams *mgpp)
{
  GapMorphGUIParams morph_gui_params;
  GapMorphGUIParams *mgup;

  if(gap_debug) printf("gap_morph_dialog: ** START **\n");

  gimp_ui_init ("gap_morph", FALSE);
  gap_stock_init();

  mgup = &morph_gui_params;
  mgup->mgpp = mgpp;
  mgup->workpointGenerationBusy = FALSE;
  mgup->cancelWorkpointGeneration = FALSE;

  if(gap_debug) printf("gap_morph_dialog: START mgpp->master_wp_list: %d\n", (int)mgpp->master_wp_list);

  /* startup with empty workpoint list
   * (the pointer may be initalized with illegal adress
   *  with parameter settings from the last execution)
   */
  mgpp->master_wp_list = NULL;
  mgup->run_flag = FALSE;

  gap_morph_create_dialog(mgup);
  gtk_widget_show (mgup->shell);

  /* fit both src and dst into preview size
   * (this implicitly forces full refresh)
   */
  p_scale_wp_list(mgup);
  p_fit_zoom_into_pview_size(&mgup->src_win);
  p_fit_zoom_into_pview_size(&mgup->dst_win);


  if(gap_debug) printf("gap_morph_dialog.c BEFORE  gtk_main\n");
  gtk_main ();
  gdk_flush ();

  if(gap_debug) printf("gap_morph_dialog.c END\n");


  if(mgup->run_flag)
  {
    return TRUE;   /* OK, request to run the morph plug-in now */
  }
  return FALSE;    /* for cancel or close dialog without run request */

}  /* end gap_morph_dialog */
