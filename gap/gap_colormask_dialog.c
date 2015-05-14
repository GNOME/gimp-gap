/* gap_colormask_dialog.c
 *    by hof (Wolfgang Hofer)
 *    colormask filter dialog handling procedures
 *  2010/02/25
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
 * version 2.7.0;             hof: created
 */

#include "config.h"

/* SYTEM (UNIX) includes */
#include "string.h"

/* GIMP includes */
#include <gtk/gtk.h>
#include <libgimp/gimp.h>
#include <libgimp/gimpui.h>


/* GAP includes */
#include "gap-intl.h"
#include "gap_colordiff.h"
#include "gap_lib_common_defs.h"
#include "gap_pdb_calls.h"
#include "gap_colormask_file.h"
#include "gap_colormask_exec.h"
#include "gap_libgapbase.h"
#include "gap_colormask_dialog.h"

#define PLUG_IN_BINARY      "gapcolormask"
#define PLUG_IN_NAME_HELP   "plug-in-layer-set-alpha-by-colormask"

#define SCALE_WIDTH       140


typedef struct {
   char            *paramFilename;
   GimpDrawable    *layer_drawable;        /* the layer to be processed */
   GimpRGB          layercolor;
   GimpRGB          maskcolor;
   GimpRGB          nbcolor;
   gboolean         showExpertOptions;
   GtkWidget       *baseFrame;
   GtkWidget       *filterFrame;
   GtkWidget       *opacityFrame;
   GtkWidget       *expertFrame;
   GtkWidget       *debugFrame;
   GtkWidget       *keycolorButton;
   GtkWidget       *layercolorButton;
   GtkWidget       *maskcolorButton;
   GtkWidget       *nbcolorButton;
   GtkWidget       *colordiffLabel;
   GtkWidget       *nbColordiffLabel;
   GimpPixelFetcher *pftMask;
   GimpPixelFetcher *pftDest;


  gint               mouse_x;
  gint               mouse_y;
  //gint               drag_mode;

  GtkObject         *offset_adj_x;
  GtkObject         *offset_adj_y;

  guchar           **src_rows;
  guchar           **bm_rows;

  GimpDrawable      *colormask_drawable;        /* the colormask to be applied */
  gint               bm_width;
  gint               bm_height;
  gint               bm_bpp;
  gboolean           bm_has_alpha;

  GimpPixelRgn       src_rgn;
  GimpPixelRgn       colormask_rgn;

  GimpPreview       *preview;
  gboolean           applyImmediate;
  GapColormaskValues *cmaskvalsPtr;

//  GapColormaskValues params;
///  gint run;
} CmaskInterface;


extern      int gap_debug; /* ==0  ... dont print debug infos */

static CmaskInterface cmaskint =
{
  NULL,         /* paramFilename */
  NULL,         /* GimpDrawable    *layer_drawable */
  { 0,0,0,0 },  /* GimpRGB layercolor */
  { 0,0,0,0 },  /* GimpRGB maskcolor */
  { 0,0,0,0 },  /* GimpRGB nbcolor */
  FALSE,        /* showExpertOptions */
  NULL,         /*   GtkWidget       *baseFrame; */
  NULL,         /*   GtkWidget       *filterFrame; */
  NULL,         /*   GtkWidget       *opacityFrame; */
  NULL,         /*   GtkWidget       *expertFrame; */
  NULL,         /*   GtkWidget       *debugFrame; */
  NULL,         /* GtkWidget  *keycolorButton */
  NULL,         /* GtkWidget  *layercolorButton */
  NULL,         /* GtkWidget  *maskcolorButton */
  NULL,         /* GtkWidget  *nbcolorButton */
  NULL,         /* GtkWidget  *colordiffLabel */
  NULL,         /* GtkWidget  *nbColordiffLabel */
  NULL,         /* GimpPixelFetcher *pftMask */
  NULL,         /* GimpPixelFetcher *pftDest */


  0,         /* mouse_x */
  0,         /* mouse_y */
  // DRAG_NONE, /* drag_mode */
  NULL,      /* offset_adj_x */
  NULL,      /* offset_adj_y */
  NULL,      /* src_rows */
  NULL,      /* bm_rows */
  NULL,      /* colormask_drawable */
  0,         /* bm_width */
  0,         /* bm_height */
  0,         /* bm_bpp */
  FALSE,     /* bm_has_alpha */
  { 0, },    /* src_rgn */
  { 0, },    /* colormask_rgn */
  //{ 0, }     /* params */
  NULL,      /* preview */
  FALSE,
  NULL       /*  cmaskvalsPtr */
};

static void     p_keycolor_update_callback(GtkWidget *widget, gpointer val);
static void     p_color_update_callback(GtkWidget *widget, gpointer val);
static void     p_fetch_layercolor_and_maskcolor(gint x, gint y);

static void     p_dialog_new_colormask     (gboolean       init_offsets);
static void     dialog_update_preview      (GimpPreview   *preview);
static gint     dialog_preview_events      (GtkWidget     *area,
                                            GdkEvent      *event,
                                            GimpPreview   *preview);
static void     dialog_get_rows            (GimpPixelRgn  *pr,
                                            guchar       **rows,
                                            gint           x,
                                            gint           y,
                                            gint           width,
                                            gint           height);
static void     dialog_fill_src_rows       (gint           start,
                                            gint           how_many,
                                            gint           yofs);
static void     dialog_fill_bumpmap_rows   (gint           start,
                                            gint           how_many,
                                            gint           yofs);
static gint     dialog_constrain           (gint32         image_id,
                                            gint32         drawable_id,
                                            gpointer       data);
static void     dialog_colormask_callback  (GtkWidget     *widget,
                                            GimpPreview   *preview);

static void     p_update_colorfiff_labels(GtkWidget *widget, gpointer val);
static void     p_update_option_frames_visiblity_callback(GtkWidget     *widget, gpointer      data);

/* -----------------------------------------------
 * p_set_widget_sensitivity_dependent_to_algorithm
 * -----------------------------------------------
 */
static void
p_set_widget_sensitivity_dependent_to_algorithm(GtkWidget *widget, gpointer val)
{
  printf("TODO p_set_widget_sensitivity_dependent_to_algorithm\n");  // TODO

}  /* end p_set_widget_sensitivity_dependent_to_algorithm */




/* -------------------------------
 * p_create_base_options
 * -------------------------------
 *
 */
static GtkWidget*
p_create_base_options(GapColormaskValues *cmaskvals, GtkWidget *preview)
{
  GtkWidget *frame;
  GtkWidget *vbox;
  GtkWidget *table;
  GtkWidget *combo;
  GtkWidget *button;
  GtkObject *adj;
  gint       row = 0;

  /* the frame */
  frame = gimp_frame_new (_("Base Options"));

  gtk_widget_show (frame);
  gtk_container_set_border_width (GTK_CONTAINER (frame), 2);

  vbox = gtk_vbox_new (FALSE, 0);
  gtk_container_add (GTK_CONTAINER (frame), vbox);
  gtk_widget_show (vbox);



  /* table */
  table = gtk_table_new (4, 3, FALSE);
  gtk_table_set_col_spacings (GTK_TABLE (table), 4);
  gtk_table_set_row_spacings (GTK_TABLE (table), 4);
  gtk_widget_show (table);
  gtk_box_pack_start (GTK_BOX (vbox), table, TRUE, TRUE, 0);
  gtk_container_set_border_width (GTK_CONTAINER (table), 4);






  /* menu to select the colormask drawable */
  combo = gimp_drawable_combo_box_new (dialog_constrain, NULL);
  gimp_int_combo_box_connect (GIMP_INT_COMBO_BOX (combo), cmaskvals->colormask_id,
                              G_CALLBACK (dialog_colormask_callback),
                              preview);

  gimp_table_attach_aligned (GTK_TABLE (table), 0, row++,
                             _("Color Mask:"), 0.0, 0.5, combo, 2, FALSE);



  /* loColorThreshold for colordiffence range */
  adj = gimp_scale_entry_new (GTK_TABLE (table), 0, row++,
                              _("Colordiff Threshold:"), SCALE_WIDTH, 6,
                              cmaskvals->loColorThreshold, 0.0, 1.0, 0.01, 0.1, 4,
                              TRUE, 0, 0,
                              _("Colordiff lower threshold. "
                                "pixels that differ in color less than this value "
                                "(compared with the corresponding pixel in the colormask) "
                                "are set to lower opacity value (usually transparent)"),
                               NULL);
  g_signal_connect (adj, "value-changed",
                    G_CALLBACK (gimp_double_adjustment_update),
                    &cmaskvals->loColorThreshold);
  g_signal_connect_swapped (adj, "value-changed",
                            G_CALLBACK (gimp_preview_invalidate),
                            preview);

  /* hiColorThreshold for colordiffence range */
  adj = gimp_scale_entry_new (GTK_TABLE (table), 0, row++,
                              _("_HiColordiff Threshold:"), SCALE_WIDTH, 6,
                              cmaskvals->hiColorThreshold, 0.0, 1.0, 0.01, 0.1, 4,
                              TRUE, 0, 0,
                              _("Colordiff upper threshold. "
                                "pixels that differ in color more than this value "
                                "(compared with the corresponding pixel in the colormask) "
                                "are set to upper opacity value (usually opaque)"),
                              NULL);
  g_signal_connect (adj, "value-changed",
                    G_CALLBACK (gimp_double_adjustment_update),
                    &cmaskvals->hiColorThreshold);
  g_signal_connect_swapped (adj, "value-changed",
                            G_CALLBACK (gimp_preview_invalidate),
                            preview);




  /* keep layermask checkbutton */
  button = gtk_check_button_new_with_mnemonic (_("Keep Layer Mask"));
  gtk_table_attach_defaults (GTK_TABLE (table), button, 0, 3, row, row + 1);
  gtk_widget_show (button);
  row++;

  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button), cmaskvals->keepLayerMask);
  g_signal_connect (button, "toggled",
                    G_CALLBACK (gimp_toggle_button_update),
                    &cmaskvals->keepLayerMask);
  g_signal_connect_swapped (button, "toggled",
                            G_CALLBACK (gimp_preview_invalidate),
                            preview);



  /* apply immediate checkbutton */
  button = gtk_check_button_new_with_mnemonic (_("Apply Immediately"));
  gtk_table_attach_defaults (GTK_TABLE (table), button, 0, 3, row, row + 1);
  gtk_widget_show (button);
  row++;

  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button), cmaskint.applyImmediate);
  g_signal_connect (button, "toggled",
                    G_CALLBACK (gimp_toggle_button_update),
                    &cmaskint.applyImmediate);
  g_signal_connect_swapped (button, "toggled",
                            G_CALLBACK (gimp_preview_invalidate),
                            preview);


  /* show expert options checkbutton */
  button = gtk_check_button_new_with_mnemonic (_("Show All Options"));
  gtk_table_attach_defaults (GTK_TABLE (table), button, 0, 3, row, row + 1);
  gtk_widget_show (button);
  row++;

  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button), cmaskint.showExpertOptions);
  g_signal_connect (button, "toggled",
                     G_CALLBACK (gimp_toggle_button_update),
                     &cmaskint.showExpertOptions);
  g_signal_connect_swapped (button, "toggled",
                            G_CALLBACK (p_update_option_frames_visiblity_callback),
                            NULL);

  return (frame);

}  /* end p_create_base_options */


/* -------------------------------
 * p_create_filter_options
 * -------------------------------
 *
 */
static GtkWidget*
p_create_filter_options(GapColormaskValues *cmaskvals, GtkWidget *preview)
{
  GtkWidget *frame;
  GtkWidget *vbox;
  GtkWidget *table;
  GtkObject *adj;
  gint       row = 0;

  /* the frame */
  frame = gimp_frame_new (_("Filter Options"));

  gtk_widget_show (frame);
  gtk_container_set_border_width (GTK_CONTAINER (frame), 2);

  vbox = gtk_vbox_new (FALSE, 0);
  gtk_container_add (GTK_CONTAINER (frame), vbox);
  gtk_widget_show (vbox);



  /* table  */
  table = gtk_table_new (4, 3, FALSE);
  gtk_table_set_col_spacings (GTK_TABLE (table), 4);
  gtk_table_set_row_spacings (GTK_TABLE (table), 4);
  gtk_widget_show (table);
  gtk_box_pack_start (GTK_BOX (vbox), table, TRUE, TRUE, 0);
  gtk_container_set_border_width (GTK_CONTAINER (table), 4);






  /* isleRadius in pixels (for removing isolated pixels) */
  adj = gimp_scale_entry_new (GTK_TABLE (table), 0, row++,
                              _("Isle Radius:"), SCALE_WIDTH, 6,
                              cmaskvals->isleRadius, 0.0, 15.0, 1.0, 1.0, 0,
                              TRUE, 0, 0,
                              _("Isle removal radius in pixels (use value 0 to disable removal of isolated pixels)"),
                              NULL);
  g_signal_connect (adj, "value-changed",
                    G_CALLBACK (gimp_double_adjustment_update),
                    &cmaskvals->isleRadius);
  g_signal_connect_swapped (adj, "value-changed",
                            G_CALLBACK (gimp_preview_invalidate),
                            preview);

  /* isleAreaPixelLimit in pixels (for removing isolated pixels) */
  adj = gimp_scale_entry_new (GTK_TABLE (table), 0, row++,
                              _("Isle Area:"), SCALE_WIDTH, 6,
                              cmaskvals->isleAreaPixelLimit, 0.0, 30.0, 1.0, 1.0, 0,
                              TRUE, 0, 0,
                              _("Isle Area size in pixels. "
                                "small isolated opaque or transparent pixel areas below that size "
                                "are removed (e.g toggled from opaque to transparent and vice versa)"),
                              NULL);
  g_signal_connect (adj, "value-changed",
                    G_CALLBACK (gimp_double_adjustment_update),
                    &cmaskvals->isleAreaPixelLimit);
  g_signal_connect_swapped (adj, "value-changed",
                            G_CALLBACK (gimp_preview_invalidate),
                            preview);



  /* featherRadius in pixels */
  adj = gimp_scale_entry_new (GTK_TABLE (table), 0, row++,
                              _("Feather Radius:"), SCALE_WIDTH, 6,
                              cmaskvals->featherRadius, 0.0, 10.0, 1.0, 1.0, 0,
                              TRUE, 0, 0,
                              _("feather radius in pixels (use value 0 to disable feathering)"),
                              NULL);
  g_signal_connect (adj, "value-changed",
                    G_CALLBACK (gimp_double_adjustment_update),
                    &cmaskvals->featherRadius);
  g_signal_connect_swapped (adj, "value-changed",
                            G_CALLBACK (gimp_preview_invalidate),
                            preview);



  return (frame);

}  /* end p_create_filter_options */




/* -------------------------------
 * p_create_opacity_options
 * -------------------------------
 *
 */
static GtkWidget*
p_create_opacity_options(GapColormaskValues *cmaskvals, GtkWidget *preview)
{
  GtkWidget *frame;
  GtkWidget *vbox;
  GtkWidget *table;
  GtkWidget *button;
  GtkWidget *label;
  GtkObject *adj;
  gint       row = 0;

  /* the frame */
  frame = gimp_frame_new (_("Opacity Options"));

  gtk_widget_show (frame);
  gtk_container_set_border_width (GTK_CONTAINER (frame), 2);

  vbox = gtk_vbox_new (FALSE, 0);
  gtk_container_add (GTK_CONTAINER (frame), vbox);
  gtk_widget_show (vbox);



  /* table  */
  table = gtk_table_new (4, 3, FALSE);
  gtk_table_set_col_spacings (GTK_TABLE (table), 4);
  gtk_table_set_row_spacings (GTK_TABLE (table), 4);
  gtk_widget_show (table);
  gtk_box_pack_start (GTK_BOX (vbox), table, TRUE, TRUE, 0);
  gtk_container_set_border_width (GTK_CONTAINER (table), 4);



  /* lowerOpacity for colordiffence range */
  adj = gimp_scale_entry_new (GTK_TABLE (table), 0, row,
                              _("Lower Opacity:"), SCALE_WIDTH, 6,
                              cmaskvals->lowerOpacity, 0.0, 1.0, 0.01, 0.1, 4,
                              TRUE, 0, 0,
                              _("Lower opacity value is set for pixels with color difference "
                                "less than Colordiff threshold (use value 0 for transparency)"),
                              NULL);
  g_signal_connect (adj, "value-changed",
                    G_CALLBACK (gimp_double_adjustment_update),
                    &cmaskvals->lowerOpacity);
  g_signal_connect_swapped (adj, "value-changed",
                            G_CALLBACK (gimp_preview_invalidate),
                            preview);

  row++;

  /* upperOpacity for colordiffence range */
  adj = gimp_scale_entry_new (GTK_TABLE (table), 0, row,
                              _("Upper Opacity:"), SCALE_WIDTH, 6,
                              cmaskvals->upperOpacity, 0.0, 1.0, 0.01, 0.1, 4,
                              TRUE, 0, 0,
                              _("Upper opacity is set for pixels with color difference "
                                "greater than High Colordiff threshold (use value 1 for opacity)"),
                              NULL);
  g_signal_connect (adj, "value-changed",
                    G_CALLBACK (gimp_double_adjustment_update),
                    &cmaskvals->upperOpacity);
  g_signal_connect_swapped (adj, "value-changed",
                            G_CALLBACK (gimp_preview_invalidate),
                            preview);


  row++;

  /* trigger alpha (only relevant if colormask has alpha channel) */
  adj = gimp_scale_entry_new (GTK_TABLE (table), 0, row,
                              _("Trigger Alpha:"), SCALE_WIDTH, 6,
                              cmaskvals->triggerAlpha, 0.0, 1.0, 0.1, 0.1, 4,
                              TRUE, 0, 0,
                              _("Trigger alpha is only relevant in case the color mask has an alpha channel. "
                                "All pixels where the alpha channel of the corresponding pixel "
                                "in the color mask is below this trigger value "
                                "are not changed (e.g. keep their original opacity)"),
                              NULL);
  g_signal_connect (adj, "value-changed",
                    G_CALLBACK (gimp_double_adjustment_update),
                    &cmaskvals->triggerAlpha);
  g_signal_connect_swapped (adj, "value-changed",
                            G_CALLBACK (gimp_preview_invalidate),
                            preview);


  return (frame);

}  /* end p_create_opacity_options */


/* -------------------------------
 * p_create_expert_options
 * -------------------------------
 * 
 */
static GtkWidget*
p_create_expert_options(GapColormaskValues *cmaskvals, GtkWidget *preview)
{
  GtkWidget *frame;
  GtkWidget *vbox;
  GtkWidget *table;
  GtkWidget *button;
  GtkWidget *label;
  GtkObject *adj;
  gint       row = 0;

  /* the frame */
  frame = gimp_frame_new (_("Expert Options"));

  gtk_widget_show (frame);
  gtk_container_set_border_width (GTK_CONTAINER (frame), 2);

  vbox = gtk_vbox_new (FALSE, 0);
  gtk_container_add (GTK_CONTAINER (frame), vbox);
  gtk_widget_show (vbox);



  /* table  */
  table = gtk_table_new (4, 3, FALSE);
  gtk_table_set_col_spacings (GTK_TABLE (table), 4);
  gtk_table_set_row_spacings (GTK_TABLE (table), 4);
  gtk_widget_show (table);
  gtk_box_pack_start (GTK_BOX (vbox), table, TRUE, TRUE, 0);
  gtk_container_set_border_width (GTK_CONTAINER (table), 4);






  /* algorithm */
  adj = gimp_scale_entry_new (GTK_TABLE (table), 0, row++,
                              _("_Algorithm:"), SCALE_WIDTH, 6,
                              cmaskvals->algorithm, 0.0, GAP_COLORMASK_ALGO_MAX, 1.0, 1.0, 0,
                              TRUE, 0, 0,
                              NULL, NULL);
  g_signal_connect (adj, "value-changed",
                    G_CALLBACK (gimp_int_adjustment_update),
                    &cmaskvals->algorithm);
  g_signal_connect_swapped (adj, "value-changed",
                            G_CALLBACK (gimp_preview_invalidate),
                            preview);
  g_signal_connect_swapped (adj, "value-changed",
                            G_CALLBACK (p_set_widget_sensitivity_dependent_to_algorithm),
                            preview);



  /* --------------- start of keycolor widgets  ------------- */
  
  /* enableKeyColorThreshold checkbutton */
  button = gtk_check_button_new_with_mnemonic (_("Enable individual color threshold for the key color"));
  gtk_table_attach_defaults (GTK_TABLE (table), button, 0, 3, row, row + 1);
  gtk_widget_show (button);

  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button), cmaskvals->enableKeyColorThreshold);
  g_signal_connect (button, "toggled",
                    G_CALLBACK (gimp_toggle_button_update),
                    &cmaskvals->enableKeyColorThreshold);
  g_signal_connect_swapped (button, "toggled",
                            G_CALLBACK (gimp_preview_invalidate),
                            cmaskint.preview);
  // TODO add another callback with g_signal_connect_swapped
  // to update widget sensitivity (en/disable keycolorButton, keyColorThreshold and keyColorSensitivity

  row++;


  /* the keycolor label */
  label = gtk_label_new (_("Key color:"));
  gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.0);
  gtk_table_attach_defaults (GTK_TABLE(table), label, 0, 1, row, row+1);
  gtk_widget_show (label);

  /* the keycolor button */
  button = gimp_color_button_new (_("Key color"),
                                  40, 20,                     /* WIDTH, HEIGHT, */
                                  &cmaskvals->keycolor,
                                  GIMP_COLOR_AREA_FLAT);
  gtk_table_attach (GTK_TABLE (table), button, 1, 3, row, row+1,
                    GTK_FILL | GTK_EXPAND, GTK_SHRINK, 0, 0) ;
  gtk_widget_show (button);
  cmaskint.keycolorButton = button;

  g_signal_connect (button, "color_changed",
                    G_CALLBACK (p_keycolor_update_callback),
                    &cmaskvals->keycolor);

  g_signal_connect_swapped (button, "color_changed",
                             G_CALLBACK (gimp_preview_invalidate),
                             cmaskint.preview);

  row++;

  /* keyColorThreshold (individual lo threshold for the key color) */
  adj = gimp_scale_entry_new (GTK_TABLE (table), 0, row,
                              _("Key Colordiff threshold:"), SCALE_WIDTH, 6,
                              cmaskvals->loKeyColorThreshold, 0.0, 1.0, 0.01, 0.1, 4,
                              TRUE, 0, 0,
                              NULL, NULL);
  g_signal_connect (adj, "value-changed",
                    G_CALLBACK (gimp_double_adjustment_update),
                    &cmaskvals->loKeyColorThreshold);
  g_signal_connect_swapped (adj, "value-changed",
                            G_CALLBACK (gimp_preview_invalidate),
                            cmaskint.preview);

  row++;

  /* keyColorThreshold (individual lo color threshold for the key color) */
  adj = gimp_scale_entry_new (GTK_TABLE (table), 0, row,
                              _("Key Color Sensitivity:"), SCALE_WIDTH, 6,
                              cmaskvals->keyColorSensitivity, 0.0, 10.0, 0.1, 1.0, 4,
                              TRUE, 0, 0,
                              NULL, NULL);
  g_signal_connect (adj, "value-changed",
                    G_CALLBACK (gimp_double_adjustment_update),
                    &cmaskvals->keyColorSensitivity);
  g_signal_connect_swapped (adj, "value-changed",
                            G_CALLBACK (gimp_preview_invalidate),
                            cmaskint.preview);


  /* --------------- end of keycolor widgets  ------------- */

  row++;



  /* significant colordiff threshold */
  adj = gimp_scale_entry_new (GTK_TABLE (table), 0, row,
                              _("Sig Colordiff Threshold:"), SCALE_WIDTH, 6,
                              cmaskvals->significantColordiff, 0.0, 1.0, 0.1, 1.0, 4,
                              TRUE, 0, 0,
                              NULL, NULL);
  g_signal_connect (adj, "value-changed",
                    G_CALLBACK (gimp_double_adjustment_update),
                    &cmaskvals->significantColordiff);
  g_signal_connect_swapped (adj, "value-changed",
                            G_CALLBACK (gimp_preview_invalidate),
                            cmaskint.preview);
  row++;

  /* significant brightness difference threshold */
  adj = gimp_scale_entry_new (GTK_TABLE (table), 0, row,
                              _("Sig Brightness Threshold:"), SCALE_WIDTH, 6,
                              cmaskvals->significantBrightnessDiff, 0.0, 1.0, 0.1, 1.0, 4,
                              TRUE, 0, 0,
                              NULL, NULL);
  g_signal_connect (adj, "value-changed",
                    G_CALLBACK (gimp_double_adjustment_update),
                    &cmaskvals->significantBrightnessDiff);
  g_signal_connect_swapped (adj, "value-changed",
                            G_CALLBACK (gimp_preview_invalidate),
                            cmaskint.preview);

  row++;

  /* significant brightness difference threshold */
  adj = gimp_scale_entry_new (GTK_TABLE (table), 0, row,
                              _("Sig Radius:"), SCALE_WIDTH, 6,
                              cmaskvals->significantRadius, 0.0, 10.0, 1.0, 1.0, 0,
                              TRUE, 0, 0,
                              NULL, NULL);
  g_signal_connect (adj, "value-changed",
                    G_CALLBACK (gimp_double_adjustment_update),
                    &cmaskvals->significantRadius);
  g_signal_connect_swapped (adj, "value-changed",
                            G_CALLBACK (gimp_preview_invalidate),
                            cmaskint.preview);


  row++;


  /* edgeColorThreshold for colordiffence range */
  adj = gimp_scale_entry_new (GTK_TABLE (table), 0, row,
                              _("_Edge Colordiff threshold:"), SCALE_WIDTH, 6,
                              cmaskvals->edgeColorThreshold, 0.0, 1.0, 0.01, 0.1, 4,
                              TRUE, 0, 0,
                              NULL, NULL);
  g_signal_connect (adj, "value-changed",
                    G_CALLBACK (gimp_double_adjustment_update),
                    &cmaskvals->edgeColorThreshold);
  g_signal_connect_swapped (adj, "value-changed",
                            G_CALLBACK (gimp_preview_invalidate),
                            preview);

  row++;

  /* thresholdColorArea for colordiffence range */
  adj = gimp_scale_entry_new (GTK_TABLE (table), 0, row,
                              _("_Area Colordiff threshold:"), SCALE_WIDTH, 6,
                              cmaskvals->thresholdColorArea, 0.0, 1.0, 0.01, 0.1, 4,
                              TRUE, 0, 0,
                              NULL, NULL);
  g_signal_connect (adj, "value-changed",
                    G_CALLBACK (gimp_double_adjustment_update),
                    &cmaskvals->thresholdColorArea);
  g_signal_connect_swapped (adj, "value-changed",
                            G_CALLBACK (gimp_preview_invalidate),
                            preview);

  row++;

  /* pixelDiagonal in pixels (for small area detection) */
  adj = gimp_scale_entry_new (GTK_TABLE (table), 0, row,
                              _("Small Area Diagonal:"), SCALE_WIDTH, 6,
                              cmaskvals->pixelDiagonal, 0.0, 50.0, 1.0, 1.0, 0,
                              TRUE, 0, 0,
                              NULL, NULL);
  g_signal_connect (adj, "value-changed",
                    G_CALLBACK (gimp_double_adjustment_update),
                    &cmaskvals->pixelDiagonal);
  g_signal_connect_swapped (adj, "value-changed",
                            G_CALLBACK (gimp_preview_invalidate),
                            preview);
  row++;

  /* pixelAreaLimit in pixels (for small area detection) */
  adj = gimp_scale_entry_new (GTK_TABLE (table), 0, row,
                              _("Small Area Pixelsize:"), SCALE_WIDTH, 6,
                              cmaskvals->pixelAreaLimit, 0.0, 1000.0, 1.0, 1.0, 0,
                              TRUE, 0, 0,
                              NULL, NULL);
  g_signal_connect (adj, "value-changed",
                    G_CALLBACK (gimp_double_adjustment_update),
                    &cmaskvals->pixelAreaLimit);
  g_signal_connect_swapped (adj, "value-changed",
                            G_CALLBACK (gimp_preview_invalidate),
                            preview);


  return (frame);

}  /* end p_create_expert_options */


/* -------------------------------
 * p_create_debug_options
 * -------------------------------
 *
 */
static GtkWidget*
p_create_debug_options(GapColormaskValues *cmaskvals, GtkWidget *preview)
{
  GtkWidget *frame;
  GtkWidget *vbox;
  GtkWidget *table;
  GtkWidget *button;
  GtkWidget *label;
  GtkObject *adj;
  gint       row = 0;

  /* the frame */
  frame = gimp_frame_new (_("DEBUG Options"));

  gtk_widget_show (frame);
  gtk_container_set_border_width (GTK_CONTAINER (frame), 2);

  vbox = gtk_vbox_new (FALSE, 0);
  gtk_container_add (GTK_CONTAINER (frame), vbox);
  gtk_widget_show (vbox);



  /* table  */
  table = gtk_table_new (4, 3, FALSE);
  gtk_table_set_col_spacings (GTK_TABLE (table), 4);
  gtk_table_set_row_spacings (GTK_TABLE (table), 4);
  gtk_widget_show (table);
  gtk_box_pack_start (GTK_BOX (vbox), table, TRUE, TRUE, 0);
  gtk_container_set_border_width (GTK_CONTAINER (table), 4);







  /* colorSensitivity  */
  adj = gimp_scale_entry_new (GTK_TABLE (table), 0, row++,
                              _("DiffSensitvity:"), SCALE_WIDTH, 6,
                              cmaskvals->colorSensitivity, 1.0, 2.0, 0.01, 0.1, 4,
                              TRUE, 0, 0,
                              NULL, NULL);
  g_signal_connect (adj, "value-changed",
                    G_CALLBACK (gimp_double_adjustment_update),
                    &cmaskvals->colorSensitivity);
  g_signal_connect_swapped (adj, "value-changed",
                            G_CALLBACK (gimp_preview_invalidate),
                            preview);
  g_signal_connect_swapped (adj, "value-changed",
                            G_CALLBACK (p_update_colorfiff_labels),
                            NULL);




  /* connectByCorner checkbutton */
  button = gtk_check_button_new_with_mnemonic (_("DEBUG Connect by corner (+ use RGB colordiff)"));
  gtk_table_attach_defaults (GTK_TABLE (table), button, 0, 3, row, row + 1);
  gtk_widget_show (button);
  row++;

  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button), cmaskvals->connectByCorner);
  g_signal_connect (button, "toggled",
                    G_CALLBACK (gimp_toggle_button_update),
                    &cmaskvals->connectByCorner);
  g_signal_connect_swapped (button, "toggled",
                            G_CALLBACK (gimp_preview_invalidate),
                            preview);



  /* keep worklayer checkbutton */
  button = gtk_check_button_new_with_mnemonic (_("DEBUG Keep worklayer"));
  gtk_table_attach_defaults (GTK_TABLE (table), button, 0, 3, row, row + 1);
  gtk_widget_show (button);
  row++;

  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button), cmaskvals->keepWorklayer);
  g_signal_connect (button, "toggled",
                    G_CALLBACK (gimp_toggle_button_update),
                    &cmaskvals->keepWorklayer);
  g_signal_connect_swapped (button, "toggled",
                            G_CALLBACK (gimp_preview_invalidate),
                            preview);







  row++;

  /* the layercolor label */
  label = gtk_label_new (_("Layer color:"));
  gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.0);
  gtk_table_attach_defaults (GTK_TABLE(table), label, 0, 1, row, row+1);
  gtk_widget_show (label);

  /* the layercolor button */
  button = gimp_color_button_new (_("Layer color (Colormask)"),
                                  40, 20,                     /* WIDTH, HEIGHT, */
                                  &cmaskint.layercolor,
                                  GIMP_COLOR_AREA_FLAT);
  gtk_table_attach (GTK_TABLE (table), button, 1, 3, row, row+1,
                    GTK_FILL | GTK_EXPAND, GTK_SHRINK, 0, 0) ;
  gtk_widget_show (button);
  cmaskint.layercolorButton = button;

  g_signal_connect (button, "color_changed",
                    G_CALLBACK (p_color_update_callback),
                    &cmaskint.layercolor);
 
  row++;

  /* the maskcolor label */
  label = gtk_label_new (_("Mask color:"));
  gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.0);
  gtk_table_attach_defaults (GTK_TABLE(table), label, 0, 1, row, row+1);
  gtk_widget_show (label);

  /* the maskcolor button */
  button = gimp_color_button_new (_("Mask color (Colormask)"),
                                  40, 20,                     /* WIDTH, HEIGHT, */
                                  &cmaskint.maskcolor,
                                  GIMP_COLOR_AREA_FLAT);
  gtk_table_attach (GTK_TABLE (table), button, 1, 3, row, row+1,
                    GTK_FILL | GTK_EXPAND, GTK_SHRINK, 0, 0) ;
  gtk_widget_show (button);
  cmaskint.maskcolorButton = button;

  g_signal_connect (button, "color_changed",
                    G_CALLBACK (p_color_update_callback),
                    &cmaskint.maskcolor);

  row++;
 

  /* the maskcolor label */
  label = gtk_label_new (_("Nb color:"));
  gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.0);
  gtk_table_attach_defaults (GTK_TABLE(table), label, 0, 1, row, row+1);
  gtk_widget_show (label);

  /* the maskcolor button */
  button = gimp_color_button_new (_("Left Neighbor color"),
                                  40, 20,                     /* WIDTH, HEIGHT, */
                                  &cmaskint.nbcolor,
                                  GIMP_COLOR_AREA_FLAT);
  gtk_table_attach (GTK_TABLE (table), button, 1, 3, row, row+1,
                    GTK_FILL | GTK_EXPAND, GTK_SHRINK, 0, 0) ;
  gtk_widget_show (button);
  cmaskint.nbcolorButton = button;

  g_signal_connect (button, "color_changed",
                    G_CALLBACK (p_color_update_callback),
                    &cmaskint.nbcolor);

  row++;
 
  /* the colordiff label */
  label = gtk_label_new (_("Colordiff:"));
  gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.0);
  gtk_table_attach_defaults (GTK_TABLE(table), label, 0, 1, row, row+1);
  gtk_widget_show (label);

  label = gtk_label_new (_("#.#####"));
  gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.0);
  gtk_table_attach_defaults (GTK_TABLE(table), label, 1, 3, row, row+1);
  gtk_widget_show (label);
  cmaskint.colordiffLabel = label;

  row++;

  /* the neighbor colordiff label */
  label = gtk_label_new (_("NbDiff:"));
  gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.0);
  gtk_table_attach_defaults (GTK_TABLE(table), label, 0, 1, row, row+1);
  gtk_widget_show (label);

  label = gtk_label_new (_("#.#####"));
  gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.0);
  gtk_table_attach_defaults (GTK_TABLE(table), label, 1, 3, row, row+1);
  gtk_widget_show (label);
  cmaskint.nbColordiffLabel = label;


  return (frame);

}  /* end p_create_debug_options */


/* ---------------------------------
 * p_update_colorfiff_labels
 * ---------------------------------
 */
static void
p_update_colorfiff_labels(GtkWidget *widget, gpointer val)
{
  if((cmaskint.colordiffLabel != NULL) && (cmaskint.nbColordiffLabel != NULL))
  {
    gdouble  colorDiff;
    gdouble  colorDiffSimple;
    char    *valueAsText;
    gdouble sensitivity;

    sensitivity = cmaskint.cmaskvalsPtr->colorSensitivity;
    
    colorDiff = gap_colordiff_GimpRGB(&cmaskint.maskcolor
                      , &cmaskint.layercolor
		      , sensitivity, TRUE);

    colorDiffSimple = gap_colordiff_simple_GimpRGB(&cmaskint.maskcolor
                      , &cmaskint.layercolor
		      , TRUE);
                      
printf("colorDiff:%f   %f\n", colorDiff, (float)colorDiff);
printf("colorDiffSimple:%f\n", colorDiffSimple);

    valueAsText = g_strdup_printf("%.5f", (float)colorDiff);
    gtk_label_set_text ( GTK_LABEL(cmaskint.colordiffLabel), valueAsText);
    g_free(valueAsText);

    colorDiff = gap_colordiff_GimpRGB(&cmaskint.nbcolor
                       , &cmaskint.layercolor
		       , sensitivity, TRUE);

    valueAsText = g_strdup_printf("%.5f", (float)colorDiff);
    gtk_label_set_text ( GTK_LABEL(cmaskint.nbColordiffLabel), valueAsText);
    g_free(valueAsText);
   
  }

}  /* end p_update_colorfiff_labels */




/* -----------------------------------------
 * p_update_option_frames_visiblity_callback
 * -----------------------------------------
 *
 */
static void
p_update_option_frames_visiblity_callback(GtkWidget     *widget,
                                          gpointer      data)
{
  if(cmaskint.showExpertOptions)
  {
    const gchar *l_env;
    gtk_widget_show (cmaskint.filterFrame);
    gtk_widget_show (cmaskint.opacityFrame);

    l_env = g_getenv("USERNAME");
    if(l_env != NULL)
    {
      if(strcmp(l_env, "hof") == 0)
      {
        gtk_widget_show (cmaskint.expertFrame);
        gtk_widget_show (cmaskint.debugFrame);
        return;
      }
    }
    
    // Note that the colormask filter is not finished yet
    // the experimental expert and debug options will be hidden
    // from the public (all users != hof) until the code is ready.
    // Expect some of those options to be removed and/or changed
    // in the future....
    
    gtk_widget_hide (cmaskint.expertFrame);
    gtk_widget_hide (cmaskint.debugFrame);
  }
  else
  {
    gtk_widget_hide (cmaskint.filterFrame);
    gtk_widget_hide (cmaskint.opacityFrame);
    gtk_widget_hide (cmaskint.expertFrame);
    gtk_widget_hide (cmaskint.debugFrame);
  }


}  /* end p_update_option_frames_visiblity_callback */




/* ---------------------------------
 * p_keycolor_update_callback
 * ---------------------------------
 */
static void
p_keycolor_update_callback(GtkWidget *widget, gpointer val)
{
  gimp_color_button_get_color((GimpColorButton *)widget, (GimpRGB *)val);
  //

}  /* end p_keycolor_update_callback */


/* ---------------------------------
 * p_color_update_callback
 * ---------------------------------
 */
static void
p_color_update_callback(GtkWidget *widget, gpointer val)
{
  gimp_color_button_get_color((GimpColorButton *)widget, (GimpRGB *)val);
  p_update_colorfiff_labels(widget, val);

}  /* end p_color_update_callback */

/* ---------------------------------
 * p_fetch_layercolor_and_maskcolor
 * ---------------------------------
 * fetch colors at coordinate x/y from
 * drawable and from the colormask
 * and update both colorbuttons
 * (the colordiff label shall update due to value change in the colorbuttons)
 */
static void
p_fetch_layercolor_and_maskcolor(gint x, gint y)
{
  gint x1 = 0;
  gint y1 = 0;
 
  if(cmaskint.preview != NULL)
  {
    gimp_preview_get_position (cmaskint.preview, &x1, &y1);
  }


  //if(gap_debug)
  {
    printf("p_fetch_layercolor_and_maskcolor x:%d  y:%d  x1:%d y1:%d\n"
          ,(int)x
          ,(int)y
          ,(int)x1
          ,(int)y1
          );
  }

  if ((cmaskint.pftDest != NULL) && (cmaskint.pftMask != NULL))
  {
    guchar  pixels[3][4];

    gimp_pixel_fetcher_get_pixel (cmaskint.pftMask
                                     , x1 + x
                                     , y1 + y
                                     , pixels[0]
                                     );
    gimp_pixel_fetcher_get_pixel (cmaskint.pftDest
                                     , x1 + x
                                     , y1 + y
                                     , pixels[1]
                                     );
    gimp_pixel_fetcher_get_pixel (cmaskint.pftDest
                                     , x1 + x -1
                                     , y1 + y
                                     , pixels[2]
                                     );


    //if(gap_debug)
    {
      printf("\npixel[0] r:%d  g:%d  b:%d  a:%d pixel[1] r:%d  g:%d  b:%d a:%d\n"
          ,(int)pixels[0][0]
          ,(int)pixels[0][1]
          ,(int)pixels[0][2]
          ,(int)pixels[0][3]
          ,(int)pixels[1][0]
          ,(int)pixels[1][1]
          ,(int)pixels[1][2]
          ,(int)pixels[1][3]
          );
    }


    gimp_rgba_set_uchar (&cmaskint.maskcolor, pixels[0][0], pixels[0][1], pixels[0][2], pixels[0][3]);
    gimp_rgba_set_uchar (&cmaskint.layercolor, pixels[1][0], pixels[1][1], pixels[1][2], pixels[1][3]);
    gimp_rgba_set_uchar (&cmaskint.nbcolor, pixels[2][0], pixels[2][1], pixels[2][2], pixels[2][3]);

    gimp_color_button_set_color ((GimpColorButton *)cmaskint.maskcolorButton, &cmaskint.maskcolor);
    gimp_color_button_set_color ((GimpColorButton *)cmaskint.layercolorButton, &cmaskint.layercolor);
    gimp_color_button_set_color ((GimpColorButton *)cmaskint.nbcolorButton, &cmaskint.nbcolor);

  }
}  /* end p_fetch_layercolor_and_maskcolor */




/* ---------------------------------
 * 
 * ---------------------------------
 */
static gint
dialog_preview_events (GtkWidget   *area,
                       GdkEvent    *event,
                       GimpPreview *preview)
{
  gint            x, y;
  gint            dx, dy;
  GdkEventButton *bevent;

  gtk_widget_get_pointer (area, &x, &y);

  bevent = (GdkEventButton *) event;

  switch (event->type)
    {
    case GDK_BUTTON_PRESS:
      switch (bevent->button)
        {
        case 2:
          //cmaskint.drag_mode = DRAG_BUMPMAP;
          break;

        default:
          p_fetch_layercolor_and_maskcolor(x, y);
          return FALSE;
        }

      cmaskint.mouse_x = x;
      cmaskint.mouse_y = y;

      gtk_grab_add (area);

      return TRUE;
      break;

    case GDK_BUTTON_RELEASE:
//       if (cmaskint.drag_mode != DRAG_NONE)
//         {
//           gtk_grab_remove (area);
//           cmaskint.drag_mode = DRAG_NONE;
//           gimp_preview_invalidate (preview);
// 
//           return TRUE;
//         }

      break;

    case GDK_MOTION_NOTIFY:
      dx = x - cmaskint.mouse_x;
      dy = y - cmaskint.mouse_y;

      cmaskint.mouse_x = x;
      cmaskint.mouse_y = y;

      if ((dx == 0) && (dy == 0))
        break;

//       switch (cmaskint.drag_mode)
//         {
//         case DRAG_BUMPMAP:
//           cmaskvals->xofs = CLAMP (cmaskvals->xofs - dx, -1000, 1000);
//           g_signal_handlers_block_by_func (cmaskint.offset_adj_x,
//                                            gimp_int_adjustment_update,
//                                            &cmaskvals->xofs);
//           gtk_adjustment_set_value (GTK_ADJUSTMENT (cmaskint.offset_adj_x),
//                                     cmaskvals->xofs);
//           g_signal_handlers_unblock_by_func (cmaskint.offset_adj_x,
//                                              gimp_int_adjustment_update,
//                                              &cmaskvals->xofs);
// 
//           cmaskvals->yofs = CLAMP (cmaskvals->yofs - dy, -1000, 1000);
//           g_signal_handlers_block_by_func (cmaskint.offset_adj_y,
//                                            gimp_int_adjustment_update,
//                                            &cmaskvals->yofs);
//           gtk_adjustment_set_value (GTK_ADJUSTMENT (cmaskint.offset_adj_y),
//                                     cmaskvals->yofs);
//           g_signal_handlers_unblock_by_func (cmaskint.offset_adj_y,
//                                              gimp_int_adjustment_update,
//                                              &cmaskvals->yofs);
//           break;
// 
//         default:
//           return FALSE;
//         }

      gimp_preview_invalidate (preview);

      return TRUE;
      break;

    default:
      break;
    }

  return FALSE;
}

/* -------------------------
 * p_dialog_new_colormask
 * -------------------------
 */
static void
p_dialog_new_colormask (gboolean init_offsets)
{
  if(cmaskint.pftDest != NULL)
  {
    gimp_pixel_fetcher_destroy (cmaskint.pftDest);
    cmaskint.pftDest = NULL;
  }

  /* Get drawable */
  if (cmaskint.colormask_drawable != NULL)
  { 
    if (cmaskint.colormask_drawable != cmaskint.layer_drawable)
    {
      gimp_drawable_detach (cmaskint.colormask_drawable);
    }
  }
  
  if(cmaskint.pftMask != NULL)
  {
    gimp_pixel_fetcher_destroy (cmaskint.pftMask);
    cmaskint.pftMask = NULL;
  }
  
  if (cmaskint.cmaskvalsPtr->colormask_id != -1)
  {
    cmaskint.colormask_drawable = gimp_drawable_get (cmaskint.cmaskvalsPtr->colormask_id);
  }
  else
  {
    cmaskint.colormask_drawable = cmaskint.layer_drawable;
  }

  if (cmaskint.colormask_drawable == NULL)
  {
    return;
  }

  cmaskint.pftMask = gimp_pixel_fetcher_new (cmaskint.colormask_drawable, FALSE /* shadow */);
  gimp_pixel_fetcher_set_edge_mode (cmaskint.pftMask, GIMP_PIXEL_FETCHER_EDGE_BLACK);

  if(cmaskint.layer_drawable != NULL)
  {
    cmaskint.pftDest = gimp_pixel_fetcher_new (cmaskint.layer_drawable, FALSE /* shadow */);
    gimp_pixel_fetcher_set_edge_mode (cmaskint.pftDest, GIMP_PIXEL_FETCHER_EDGE_BLACK);
  }
  
  /* Get sizes */
  cmaskint.bm_width     = gimp_drawable_width (cmaskint.colormask_drawable->drawable_id);
  cmaskint.bm_height    = gimp_drawable_height (cmaskint.colormask_drawable->drawable_id);
  cmaskint.bm_bpp       = gimp_drawable_bpp (cmaskint.colormask_drawable->drawable_id);
  cmaskint.bm_has_alpha = gimp_drawable_has_alpha (cmaskint.colormask_drawable->drawable_id);

//   if (init_offsets)
//     {
//       GtkAdjustment  *adj;
//       gint            bump_offset_x;
//       gint            bump_offset_y;
//       gint            draw_offset_y;
//       gint            draw_offset_x;
// 
//       gimp_drawable_offsets (cmaskint.colormask_drawable->drawable_id,
//                              &bump_offset_x, &bump_offset_y);
//       gimp_drawable_offsets (cmaskint.layer_drawable->drawable_id,
//                              &draw_offset_x, &draw_offset_y);
// 
//       cmaskvals->xofs = draw_offset_x - bump_offset_x;
//       cmaskvals->yofs = draw_offset_y - bump_offset_y;
// 
//       adj = (GtkAdjustment *) cmaskint.offset_adj_x;
//       if (adj)
//         {
//           adj->value = cmaskvals->xofs;
//           g_signal_handlers_block_by_func (adj,
//                                            gimp_int_adjustment_update,
//                                            &cmaskvals->xofs);
//           gtk_adjustment_value_changed (adj);
//           g_signal_handlers_unblock_by_func (adj,
//                                              gimp_int_adjustment_update,
//                                              &cmaskvals->xofs);
//         }
// 
//       adj = (GtkAdjustment *) cmaskint.offset_adj_y;
//       if (adj)
//         {
//           adj->value = cmaskvals->yofs;
//           g_signal_handlers_block_by_func (adj,
//                                            gimp_int_adjustment_update,
//                                            &cmaskvals->yofs);
//           gtk_adjustment_value_changed (adj);
//           g_signal_handlers_unblock_by_func (adj,
//                                              gimp_int_adjustment_update,
//                                              &cmaskvals->yofs);
//         }
//     }

  /* Initialize pixel region */
  gimp_pixel_rgn_init (&cmaskint.colormask_rgn, cmaskint.colormask_drawable,
                       0, 0, cmaskint.bm_width, cmaskint.bm_height, FALSE, FALSE);

}  /* end p_dialog_new_colormask */

static void
dialog_update_preview (GimpPreview *preview)
{
   guchar *dest_row;
   gint    y;
   gint    x1, y1;
   gint    width, height;
   gint    bytes;
 
   gimp_preview_get_position (preview, &x1, &y1);

// if(gap_debug)
   {
     printf("dialog_update_preview x1:%d y1:%d\n"
           ,(int)x1
           ,(int)y1
           );
   }

   if ((cmaskint.colormask_drawable != cmaskint.layer_drawable)
   && (cmaskint.layer_drawable != NULL)
   && (cmaskint.applyImmediate == TRUE)
   && (cmaskint.cmaskvalsPtr->keepLayerMask == TRUE))
   {
     gint32 drawableId;
     
     drawableId = cmaskint.layer_drawable->drawable_id;
     
     gap_colormask_apply_to_layer_of_same_size (drawableId
                        , cmaskint.cmaskvalsPtr
                        , TRUE   // doProgress
                        );
     gimp_drawable_flush (cmaskint.layer_drawable);
     gimp_displays_flush ();
   }


//   gimp_preview_get_size (preview, &width, &height);
//   bytes = cmaskint.layer_drawable->bpp;
// 
//   /* Initialize source rows */
//   gimp_pixel_rgn_init (&cmaskint.src_rgn, cmaskint.layer_drawable,
//                        sel_x1, sel_y1, sel_width, sel_height, FALSE, FALSE);
// 
//   cmaskint.src_rows = g_new (guchar *, height);
// 
//   for (y = 0; y < height; y++)
//     cmaskint.src_rows[y]  = g_new (guchar, sel_width * 4);
// 
//   dialog_fill_src_rows (0, height, y1);
// 
//   /* Initialize bumpmap rows */
//   cmaskint.bm_rows = g_new (guchar *, height + 2);
// 
//   for (y = 0; y < height + 2; y++)
//     cmaskint.bm_rows[y] = g_new (guchar, cmaskint.bm_width * cmaskint.bm_bpp);
// 
//   bumpmap_init_params (&cmaskint.params);
// 
//   dialog_fill_bumpmap_rows (0, height, cmaskvals->yofs + y1);
// 
//   dest_row = g_new (guchar, width * height * 4);
// 
//   /* Bumpmap */
// 
//   for (y = 0; y < height; y++)
//     {
//       gint isfirst = ((y == - cmaskvals->yofs - y1)
//                       && ! cmaskvals->tiled) ? 1 : 0;
//       gint islast = (y == (- cmaskvals->yofs - y1
//                            + cmaskint.bm_height - 1) && ! cmaskvals->tiled) ? 1 : 0;
//       bumpmap_row (cmaskint.src_rows[y] + 4 * x1,
//                    dest_row + 4 * width * y,
//                    width, 4, TRUE,
//                    cmaskint.bm_rows[y + isfirst],
//                    cmaskint.bm_rows[y + 1],
//                    cmaskint.bm_rows[y + 2 - islast],
//                    cmaskint.bm_width, cmaskvals->xofs + x1,
//                    cmaskvals->tiled,
//                    y >= - cmaskvals->yofs - y1 &&
//                    y < (- cmaskvals->yofs - y1 + cmaskint.bm_height),
//                    &cmaskint.params);
// 
//     }
// 
//   gimp_preview_area_draw (GIMP_PREVIEW_AREA (preview->area),
//                           0, 0, width, height,
//                           GIMP_RGBA_IMAGE,
//                           dest_row,
//                           4 * width);
// 
//   g_free (dest_row);
// 
//   for (y = 0; y < height + 2; y++)
//     g_free (cmaskint.bm_rows[y]);
//   g_free (cmaskint.bm_rows);
// 
//   for (y = 0; y < height; y++)
//     g_free (cmaskint.src_rows[y]);
//   g_free (cmaskint.src_rows);
}


// static void
// dialog_get_rows (GimpPixelRgn  *pr,
//                  guchar       **rows,
//                  gint           x,
//                  gint           y,
//                  gint           width,
//                  gint           height)
// {
//   /* This is shamelessly ripped off from gimp_pixel_rgn_get_rect().
//    * Its function is exactly the same, but it can fetch an image
//    * rectangle to a sparse buffer which is defined as separate
//    * rows instead of one big linear region.
//    */
// 
//   gint xstart, ystart;
//   gint xend, yend;
//   gint xboundary;
//   gint yboundary;
//   gint xstep, ystep;
//   gint b, bpp;
//   gint tx, ty;
//   gint tile_width  = gimp_tile_width();
//   gint tile_height = gimp_tile_height();
// 
//   bpp = pr->bpp;
// 
//   xstart = x;
//   ystart = y;
//   xend   = x + width;
//   yend   = y + height;
//   ystep  = 0; /* Shut up -Wall */
// 
//   while (y < yend)
//     {
//       x = xstart;
// 
//       while (x < xend)
//         {
//           GimpTile *tile;
// 
//           tile = gimp_drawable_get_tile2 (pr->drawable, pr->shadow, x, y);
//           gimp_tile_ref (tile);
// 
//           xstep     = tile->ewidth - (x % tile_width);
//           ystep     = tile->eheight - (y % tile_height);
//           xboundary = x + xstep;
//           yboundary = y + ystep;
//           xboundary = MIN (xboundary, xend);
//           yboundary = MIN (yboundary, yend);
// 
//           for (ty = y; ty < yboundary; ty++)
//             {
//               const guchar *src;
//               guchar       *dest;
// 
//               src = tile->data + tile->bpp * (tile->ewidth *
//                                               (ty % tile_height) +
//                                                (x % tile_width));
//               dest = rows[ty - ystart] + bpp * (x - xstart);
// 
//               for (tx = x; tx < xboundary; tx++)
//                 for (b = bpp; b; b--)
//                   *dest++ = *src++;
//             }
// 
//           gimp_tile_unref (tile, FALSE);
// 
//           x += xstep;
//         }
// 
//       y += ystep;
//     }
// }
// 
// static void
// dialog_fill_src_rows (gint start,
//                       gint how_many,
//                       gint yofs)
// {
//   gint x;
//   gint y;
// 
//   dialog_get_rows (&cmaskint.src_rgn,
//                    cmaskint.src_rows + start,
//                    0/*sel_x1*/,
//                    yofs,
//                    sel_width,
//                    how_many);
// 
//   /* Convert to RGBA.  We move backwards! */
// 
//   for (y = start; y < (start + how_many); y++)
//     {
//       const guchar *sp = cmaskint.src_rows[y] + img_bpp * sel_width - 1;
//       guchar       *p  = cmaskint.src_rows[y] + 4 * sel_width - 1;
// 
//       for (x = 0; x < sel_width; x++)
//         {
//           if (img_has_alpha)
//             *p-- = *sp--;
//           else
//             *p-- = 255;
// 
//           if (img_bpp < 3)
//             {
//               *p-- = *sp;
//               *p-- = *sp;
//               *p-- = *sp--;
//             }
//           else
//             {
//               *p-- = *sp--;
//               *p-- = *sp--;
//               *p-- = *sp--;
//             }
//         }
//     }
// }
// 
// static void
// dialog_fill_bumpmap_rows (gint start,
//                           gint how_many,
//                           gint yofs)
// {
//   gint buf_row_ofs;
//   gint remaining;
//   gint this_pass;
// 
//   /* Adapt to offset of selection */
//   yofs = MOD (yofs + sel_y1, cmaskint.bm_height);
// 
//   buf_row_ofs = start;
//   remaining   = how_many;
// 
//   while (remaining > 0)
//     {
//       this_pass = MIN (remaining, cmaskint.bm_height - yofs);
// 
//       dialog_get_rows (&cmaskint.colormask_rgn,
//                        cmaskint.bm_rows + buf_row_ofs,
//                        0,
//                        yofs,
//                        cmaskint.bm_width,
//                        this_pass);
// 
//       yofs         = (yofs + this_pass) % cmaskint.bm_height;
//       remaining   -= this_pass;
//       buf_row_ofs += this_pass;
//     }
// 
//   /* Convert rows */
// 
//   for (; how_many; how_many--)
//     {
//       bumpmap_convert_row (cmaskint.bm_rows[start],
//                            cmaskint.bm_width,
//                            cmaskint.bm_bpp,
//                            cmaskint.bm_has_alpha,
//                            cmaskint.params.lut);
// 
//       start++;
//     }
// }





static gboolean
dialog_constrain (gint32   image_id,
                  gint32   drawable_id,
                  gpointer data)
{
  return (gimp_drawable_is_rgb (drawable_id));
}

static void
dialog_colormask_callback (GtkWidget   *widget,
                         GimpPreview *preview)
{
  gint32  drawable_id;

  gimp_int_combo_box_get_active (GIMP_INT_COMBO_BOX (widget), &drawable_id);

  if (cmaskint.cmaskvalsPtr->colormask_id != drawable_id)
    {
      cmaskint.cmaskvalsPtr->colormask_id = drawable_id;
      p_dialog_new_colormask (TRUE);
      gimp_preview_invalidate (preview);
    }
}


/* ---------------------------
 * gap_colormask_create_dialog
 * ---------------------------
 */
GtkWidget*  
gap_colormask_create_dialog (GapColormaskValues *cmaskvals)
{
  GtkWidget *dialog;
  GtkWidget *paned;
  GtkWidget *hbox;
  GtkWidget *vbox;
  GtkWidget *preview;
  GtkWidget *frame;
  GtkObject *adj;

  gimp_ui_init (PLUG_IN_BINARY, TRUE);

  dialog = gimp_dialog_new (_("Color Mask"), PLUG_IN_BINARY,
                            NULL, 0,
                            gimp_standard_help_func, PLUG_IN_NAME_HELP,

                            GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                            GTK_STOCK_OK,     GTK_RESPONSE_OK,

                            NULL);

  gtk_dialog_set_alternative_button_order (GTK_DIALOG (dialog),
                                           GTK_RESPONSE_OK,
                                           GTK_RESPONSE_CANCEL,
                                           -1);

  gimp_window_set_transient (GTK_WINDOW (dialog));

  cmaskint.pftDest = NULL;
  cmaskint.pftMask = NULL;

  paned = gtk_hpaned_new ();
  gtk_container_set_border_width (GTK_CONTAINER (paned), 12);
  gtk_container_add (GTK_CONTAINER (GTK_DIALOG (dialog)->vbox), paned);
  gtk_widget_show (paned);

  hbox = gtk_hbox_new (FALSE, 0);
  gtk_paned_pack1 (GTK_PANED (paned), hbox, TRUE, FALSE);
  gtk_widget_show (hbox);

  vbox = gtk_vbox_new (FALSE, 0);
  gtk_container_set_border_width (GTK_CONTAINER (vbox), 4);
  gtk_box_pack_end (GTK_BOX (hbox), vbox, FALSE, FALSE, 0);
  gtk_widget_show (vbox);

  preview = gimp_drawable_preview_new (cmaskint.layer_drawable, NULL);
  gtk_container_add (GTK_CONTAINER (hbox), preview);
  gtk_widget_show (preview);

  g_signal_connect (preview, "invalidated",
                    G_CALLBACK (dialog_update_preview),
                    NULL);
  g_signal_connect (GIMP_PREVIEW (preview)->area, "event",
                    G_CALLBACK (dialog_preview_events), preview);

  cmaskint.preview = preview;

  hbox = gtk_hbox_new (FALSE, 0);
  gtk_paned_pack2 (GTK_PANED (paned), hbox, FALSE, FALSE);
  gtk_widget_show (hbox);

  vbox = gtk_vbox_new (FALSE, 0);
  gtk_container_set_border_width (GTK_CONTAINER (vbox), 4);
  gtk_box_pack_start (GTK_BOX (hbox), vbox, FALSE, FALSE, 0);
  gtk_widget_show (vbox);


  /* Base options */
  frame = p_create_base_options(cmaskvals, preview);
  cmaskint.baseFrame = frame;
  gtk_widget_show (frame);
  gtk_box_pack_start (GTK_BOX (vbox), frame, FALSE, FALSE, 0);

  /* Expert options */
  frame = p_create_expert_options(cmaskvals, preview);
  cmaskint.expertFrame = frame;
  gtk_widget_show (frame);
  gtk_box_pack_start (GTK_BOX (vbox), frame, FALSE, FALSE, 0);


  vbox = gtk_vbox_new (FALSE, 0);
  gtk_container_add (GTK_CONTAINER (hbox), vbox);
  gtk_widget_show (vbox);

  /* Filter options */
  frame = p_create_filter_options(cmaskvals, preview);
  cmaskint.filterFrame = frame;
  gtk_widget_hide (frame);
  gtk_box_pack_start (GTK_BOX (vbox), frame, FALSE, FALSE, 0);

  /* Opacity options */
  frame = p_create_opacity_options(cmaskvals, preview);
  cmaskint.opacityFrame = frame;
  gtk_widget_show (frame);
  gtk_box_pack_start (GTK_BOX (vbox), frame, FALSE, FALSE, 0);

  /* Debug options */
  frame = p_create_debug_options(cmaskvals, preview);
  cmaskint.debugFrame = frame;
  gtk_widget_hide (frame);
  gtk_box_pack_start (GTK_BOX (vbox), frame, FALSE, FALSE, 0);

  p_update_option_frames_visiblity_callback(NULL, NULL);

  /* Initialise drawable
   * (don't initialize offsets if colormask_id is already known)
   */
  if (cmaskvals->colormask_id == -1)
  {
    p_dialog_new_colormask (TRUE);
  }
  else
  {
    p_dialog_new_colormask (FALSE);
  }

  return (dialog);
}  /* end gap_colormask_create_dialog */



/* --------------------
 * gap_colormask_dialog
 * --------------------
 */
gboolean
gap_colormask_dialog (GapColormaskValues *cmaskvals, GimpDrawable *layer_drawable)
{
  GtkWidget *dialog;
  gboolean   run;

  cmaskint.applyImmediate = FALSE;
  cmaskint.layer_drawable = layer_drawable;
  cmaskint.paramFilename = g_strdup("colormask.params");
  cmaskint.cmaskvalsPtr = cmaskvals;
  

  dialog = gap_colormask_create_dialog(cmaskvals);

  /* Done */

  gtk_widget_show (dialog);

  run = FALSE;
  if (gimp_dialog_run (GIMP_DIALOG (dialog)) == GTK_RESPONSE_OK)
  {
    // TODO param file save shall be done in the procedure that
    // creates the dialog as pop-up for Storybord clip properties.
    // for testpurpose saving is done here until the rest is implemented...
    gap_colormask_file_save (cmaskint.paramFilename, cmaskvals);
    run = TRUE;
  }

  gtk_widget_destroy (dialog);

  if (cmaskint.colormask_drawable != cmaskint.layer_drawable)
  {
    gimp_drawable_detach (cmaskint.colormask_drawable);
  }

  //if(gap_debug)
  {
    printf("gap_colormask_dialog  run:%d\n", (int)run);
  }

  g_free(cmaskint.paramFilename);
  
  return run;
  
}  /* end gap_colormask_dialog */
