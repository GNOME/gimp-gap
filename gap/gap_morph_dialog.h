/*  gap_morph_dialog.h
 *
 *  This module handles the GAP morph dialog
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
 * version 2.1.0a; 2004/03/31  hof: created
 */

#ifndef _GAP_MORPH_DIALOG_H
#define _GAP_MORPH_DIALOG_H

#include "libgimp/gimp.h"
#include "gap_morph_main.h"

#define GAP_MORPH_PLUGIN_NAME    "plug_in_gap_morph"
#define GAP_MORPH_HELP_ID        "plug-in-gap-morph"


typedef struct GapMorphSubWin  { /* nickname: swp */
  void      *mgup;               /* pointer to parent GapMorphGUIParams struct */
  GtkObject  *x_spinbutton_adj;
  GtkObject  *y_spinbutton_adj;
  GtkWidget  *combo;
  GtkObject  *vscale_adj;
  GtkObject  *hscale_adj;
  GtkWidget  *vscale;
  GtkWidget  *hscale;
  
  GapPView   *pv_ptr;
  gboolean   src_flag;
  gboolean   startup_flag;

  GapMorphWorkPoint *curr_wp;  
  gdouble   *wpx_ptr;     /* pointer to current morph point x koord */
  gdouble   *wpy_ptr;     /* pointer to current morph point y koord */
  gint32    *layer_id_ptr;
  gint32     image_id;
  gdouble    max_zoom;             /* */
  gdouble    zoom;                 /* zoom of the preview 1.0 == original size */
  gint32     offs_x;               /* x offset of the preview at original size */
  gint32     offs_y;               /* y offset of the preview at original size */
  gint32     upd_request;
  gboolean   pview_scrolling;
} GapMorphSubWin;


typedef struct GapMorphGUIParams  { /* nickname: mgup */
  gboolean    run_flag;
  GapMorphGlobalParams  *mgpp;
  
  /* GUI widget pointers */
  GtkWidget  *shell;

  GtkObject  *num_shapepoints_adj;
  GtkWidget  *create_tween_layers_checkbutton;  
  GtkWidget  *have_workpointsets_checkbutton;  

  GapMorphSubWin src_win;
  GapMorphSubWin dst_win;

  GtkObject  *tween_steps_spinbutton_adj;
  GtkObject  *affect_radius_spinbutton_adj;
  GtkObject  *locate_edge_threshold_spinbutton_adj;
  GtkObject  *locate_radius_spinbutton_adj;
  GtkObject  *locate_shape_radius_spinbutton_adj;
  GtkObject  *gravity_intensity_spinbutton_adj;
  GtkWidget  *gravity_intensity_spinbutton;
  GtkWidget  *use_gravity_checkbutton;
  GtkWidget  *use_quality_wp_selection_checkbutton;

  GtkWidget  *wp_filesel;
  gboolean    wp_save_mode;
  gint32      upd_timertag;
  GimpRGB     pointcolor;
  GimpRGB     curr_pointcolor;

  gint32      old_src_layer_width;
  gint32      old_src_layer_height;
  gint32      old_dst_layer_width;
  gint32      old_dst_layer_height;

  gint32     op_mode;
  GtkWidget *op_mode_set_toggle;  
  GtkWidget *op_mode_move_toggle;  
  GtkWidget *op_mode_delete_toggle;  
  GtkWidget *op_mode_zoom_toggle;  
  GtkWidget *op_mode_show_toggle;  

  GtkWidget *render_mode_morph_toggle;  
  GtkWidget *render_mode_warp_toggle;  

  GtkObject *curr_point_spinbutton_adj;
  GtkWidget *toal_points_label;
  GtkWidget *warp_info_label;
  GtkWidget *show_lines_checkbutton;  

  gdouble   show_px;
  gdouble   show_py;
  gdouble   show_in_x;
  gdouble   show_in_y;

  gint32     num_shapepoints;
  
  char      *workpoint_file_ptr;
  GtkWidget *workpoint_file_lower_label;  
  GtkWidget *workpoint_file_upper_label;  
  GtkWidget *workpoint_lower_label;  
  GtkWidget *workpoint_upper_label;

  GtkWidget *progressBar;
  gboolean   workpointGenerationBusy;
  gboolean   cancelWorkpointGeneration;
  
} GapMorphGUIParams;


gboolean   gap_morph_dialog(GapMorphGlobalParams *mgpp);

#endif
