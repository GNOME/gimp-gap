/* gap_bluebox.h
 * 1997.11.01 hof (Wolfgang Hofer)
 *
 * GAP ... Gimp Animation Plugins
 *
 * This Module contains the GAP BlueBox Filter 
 * this filter makes the specified color transparent.
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

#ifndef _GAP_BLUEBOX_H
#define _GAP_BLUEBOX_H

#define GAP_BLUEBOX_PLUGIN_NAME    "plug_in_bluebox"
#define GAP_BLUEBOX_DATA_KEY_VALS  "plug_in_bluebox"
#define GAP_BLUEBOX_HELP_ID        "plug-in-bluebox"


#include <gtk/gtk.h>
#include "libgimp/gimp.h"

typedef enum
{
   GAP_BLUBOX_THRES_RGB        
  ,GAP_BLUBOX_THRES_HSV
  ,GAP_BLUBOX_THRES_VAL
  ,GAP_BLUBOX_THRES_ALL
  ,GAP_BLUBOX_THRES_DELTAE_CIE94
  ,GAP_BLUBOX_THRES_DELTAE_CIEDE200
} GapBlueboxThresMode;

typedef struct GapBlueboxVals {
  GimpRGB               keycolor;
  GapBlueboxThresMode   thres_mode;

  gdouble               thres_r;
  gdouble               thres_g;
  gdouble               thres_b;
  gdouble               thres_h;
  gdouble               thres_s;
  gdouble               thres_v;
  gdouble               thres;
  gdouble               tolerance;

  gdouble               grow;
  gint                  feather_edges;
  gdouble               feather_radius;
  gdouble               source_alpha;  /* touch only pixelswith alpha >= source_alpha */
  gdouble               target_alpha;
} GapBlueboxVals;

typedef struct GapBlueboxGlobalParams {
  gboolean     run_flag;
  GimpRunMode  run_mode;
  gint32       image_id;
  gchar        *image_filename;
  gint32       pv_image_id;
  gint32       pv_layer_id;      /* layer to apply the bluebox effect */
  gint32       pv_display_id;    /* layer to apply the bluebox effect */
  gdouble      pv_master_layer_id;  /* copy of the original layer scaled to preview size */
  gdouble      pv_size_percent;  /* size od the preview (ignored if run_flag == TRUE) */
  gint32       drawable_id;   /* drawable id of the invoking layer (used both for preview and apply) */
  gint32       layer_id;      /* layer to apply the bluebox effect */
  gint         instant_preview;  /* instant apply on preview image */
  gint32       instant_timertag;
  gboolean     instant_apply_request;

  gdouble      sum_threshold_hsv;
  gdouble      sum_threshold_rgb;
  gdouble      inv_tolerance;

  /* GUI widget pointers */
  GtkWidget *shell;  
  GtkWidget *master_table;
  gint       thres_table_attach_row;
  GtkWidget *thres_table;  
  GtkObject *thres_r_adj;
  GtkObject *thres_g_adj;
  GtkObject *thres_b_adj;
  GtkObject *thres_h_adj;
  GtkObject *thres_s_adj;
  GtkObject *thres_v_adj;
  GtkObject *thres_adj;
  GtkObject *tolerance_adj;
  GtkObject *grow_adj;
  GtkObject *feather_radius_adj;
  GtkObject *source_alpha_adj;
  GtkObject *target_alpha_adj;
  GtkWidget *keycolor_button;  
  GtkWidget *feather_edges_toggle;  
  GtkWidget *instant_preview_toggle;  
  GtkWidget *thres_rgb_toggle;  
  GtkWidget *thres_hsv_toggle;  
  GtkWidget *thres_val_toggle;  
  GtkWidget *thres_e94_toggle;  
  GtkWidget *thres_e2000_toggle;  
  GtkWidget *thres_all_toggle;  
  GdkCursor *cursor_wait;
  GdkCursor *cursor_acitve;

  GapBlueboxVals   vals;
 
  GimpHSV          key_hsv;
  
} GapBlueboxGlobalParams;

GapBlueboxGlobalParams *gap_bluebox_bbp_new(gint32 layer_id);
void       gap_bluebox_init_default_vals(GapBlueboxGlobalParams *bbp);
GtkWidget *gap_bluebox_create_dialog(GapBlueboxGlobalParams *bbp);
int        gap_bluebox_dialog(GapBlueboxGlobalParams *bbp);

gboolean   gap_bluebox_apply(GapBlueboxGlobalParams *bbp);




#endif


