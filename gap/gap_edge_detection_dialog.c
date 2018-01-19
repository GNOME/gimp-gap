/* gap_edge_detection_dialog.c
 *  by hof (Wolfgang Hofer)
 *
 * GAP ... Gimp Animation Plugins
 *
 * This Module contains the GAP Edge detection Filter dialog
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
 * gimp    2.8.xx; 2017/03/10  hof: creation
 */


/* SYTEM (UNIX) includes */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/* GIMP includes */
#include <gtk/gtk.h>
#include <libgimp/gimp.h>
#include <libgimp/gimpui.h>

/* GAP includes */
#include "config.h"
#include "gap-intl.h"
#include "gap_stock.h"

#include "gap_image.h"
#include "gap_layer_copy.h"
#include "gap_lib.h"
#include "gap_pdb_calls.h"
#include "gap_edge_detection.h"
#include "gap_edge_detection_dialog.h"

/* instant apply is implemented via timer, configured to fire 10 times per second (100 msec)
 * this collects instant_apply_requests set by other widget callbacks and events
 * and then update only once.
 * The timer is completely removed, when instant_preview is OFF
 * instant_preview requires much CPU and IO power especially on big images
 * and images with many layers
 */
#define INSTANT_TIMERINTERVAL_MILLISEC  100


#define GAP_EDGE_RESPONSE_RESET 1


/*  some definitions   */

#define SCALE_WIDTH        180
#define SPIN_BUTTON_WIDTH   75


/* ------------------------
 * gap DEBUG switch
 * ------------------------
 */

/* int gap_debug = 1; */    /* print debug infos */
/* int gap_debug = 0; */    /* 0: dont print debug infos */

extern int gap_debug;

typedef struct GapEdgeDetectGuiParams {  /* nick edguiPtr */
  gboolean     run_flag;
  gint32       image_id;
  gint32       drawable_id;   /* drawable id of the invoking layer (used both for preview and apply) */
  gint32       layer_id;      /* layer to apply the bluebox effect */

  gint32       pv_image_id;      /* id of the preview image */
  gint32       pv_layer_id;      /* layer to apply the edge detection filter */
  gint32       pv_display_id;    /* id of the disply connected to the preview image with pv_image_id */
  gdouble      pv_master_layer_id;  /* copy of the original layer scaled to preview size */
  gdouble      pv_size_percent;  /* size od the preview (ignored if run_flag == TRUE) */
  gboolean     instant_preview;  /* instant apply on preview image */
  gint32       instant_timertag;
  gboolean     instant_apply_request;


  /* GUI widget pointers */
  GtkWidget *shell;  
  GtkWidget *master_table;
  GtkObject *blurRadius1X_adj;
  GtkObject *blurRadius1Y_adj;
  GtkObject *blurRadius2X_adj;
  GtkObject *blurRadius2Y_adj;
  GtkObject *shiftLeft_adj;
  GtkObject *shiftRight_adj;
  GtkObject *shiftUp_adj;
  GtkObject *shiftDown_adj;
  GtkWidget *autoLevels_toggle;  
  GtkWidget *desaturate_toggle;  
  GtkWidget *invert_toggle;  
  GtkWidget *createEdgeAsNewLayer_toggle;  
  GtkWidget *instant_preview_toggle;
  // GtkWidget *feather_edges_toggle;  
  // GtkObject *grow_adj;
  // GtkObject *feather_radius_adj;

  
  GdkCursor *cursor_wait;
  GdkCursor *cursor_acitve;

  GapEdgeValues    *vals;
  
} GapEdgeDetectGuiParams;



/* -----------------------
 * procedure declarations
 * -----------------------
 */
static void       p_edge_init_default_vals(GapEdgeValues *edvalPtr);
static GtkWidget * gap_edge_create_dialog (GapEdgeDetectGuiParams *edguiPtr);
static void       p_reset_callback(GtkWidget *w, GapEdgeDetectGuiParams *edguiPtr);
static void       p_quit_callback(GtkWidget *w, GapEdgeDetectGuiParams *edguiPtr);
static void       p_edge_response(GtkWidget *w, gint response_id, GapEdgeDetectGuiParams *edguiPtr);
static void       p_set_waiting_cursor(GapEdgeDetectGuiParams *edguiPtr);
static void       p_set_active_cursor(GapEdgeDetectGuiParams *edguiPtr);
static void       p_apply_callback(GtkWidget *w, GapEdgeDetectGuiParams *edguiPtr);
static void       p_gdouble_adjustment_callback(GtkObject *obj, gpointer val);
static void       p_gint32_adjustment_callback(GtkObject *obj, gpointer val);
static void       p_toggle_update_callback(GtkWidget *widget, gint *val);

static void       p_set_instant_apply_request(GapEdgeDetectGuiParams *edguiPtr);
static void       p_install_timer(GapEdgeDetectGuiParams *edguiPtr);
static void       p_remove_timer(GapEdgeDetectGuiParams *edguiPtr);
static void       p_instant_timer_callback (gpointer   user_data);
static void       p_clear_GapEdgeDetectGuiParams(GapEdgeDetectGuiParams *edguiPtr);
static void       p_remove_additonal_layers_from_preview_image(GapEdgeDetectGuiParams *edguiPtr);
static gboolean   p_edge_detect_apply(GapEdgeDetectGuiParams *edguiPtr);

/* ---------------------------------
 * p_edge_init_default_vals
 * ---------------------------------
 */
static void
p_edge_init_default_vals(GapEdgeValues *edvalPtr)
{
  edvalPtr->blurRadius1X = 0.0;
  edvalPtr->blurRadius1Y  = 0.0;;
  edvalPtr->blurRadius2X = 0.0;
  edvalPtr->blurRadius2Y = 0.0;
  edvalPtr->shiftLeft = 1;
  edvalPtr->shiftRight = 1;
  edvalPtr->shiftUp = 1;
  edvalPtr->shiftDown = 1;
  edvalPtr->autoLevels = TRUE;
  edvalPtr->desaturate = TRUE;
  edvalPtr->invert = FALSE;
  edvalPtr->createEdgeAsNewLayer = TRUE;

}  /* end p_edge_init_default_vals */

/* ---------------------------------
 * deliver new values (initialized with the defaault settings)
 * ---------------------------------
 */
GapEdgeValues * 
gap_edge_edval_new(gint32 acgtivDrawableId)
{
  GapEdgeValues *edvalPtr;
  
  edvalPtr = g_new(GapEdgeValues, 1);
  p_edge_init_default_vals(edvalPtr);
  
  return (edvalPtr);
}


static void
p_clear_GapEdgeDetectGuiParams(GapEdgeDetectGuiParams *edguiPtr)
{
  edguiPtr->shell = NULL;

  edguiPtr->instant_preview = FALSE;
  edguiPtr->instant_apply_request = FALSE;
  edguiPtr->instant_timertag = -1;
  edguiPtr->master_table = NULL;
  edguiPtr->blurRadius1X_adj = NULL;
  edguiPtr->blurRadius1Y_adj = NULL;
  edguiPtr->blurRadius2X_adj = NULL;
  edguiPtr->blurRadius2Y_adj = NULL;
  edguiPtr->shiftLeft_adj = NULL;
  edguiPtr->shiftRight_adj = NULL;
  edguiPtr->shiftUp_adj = NULL;
  edguiPtr->shiftDown_adj = NULL;
  edguiPtr->autoLevels_toggle = NULL;
  edguiPtr->desaturate_toggle = NULL;
  edguiPtr->invert_toggle = NULL;
  edguiPtr->createEdgeAsNewLayer_toggle = NULL;
  edguiPtr->instant_preview_toggle = NULL;

  edguiPtr->cursor_wait = NULL;
  edguiPtr->cursor_acitve = NULL;
  
  edguiPtr->pv_image_id = -1;
  edguiPtr->pv_layer_id = -1;
  edguiPtr->pv_display_id = -1;
  edguiPtr->pv_master_layer_id = -1;
  
  edguiPtr->pv_size_percent = 100;
}

/* ---------------------------------
 * the Edge Detect MAIN dialog
 * ---------------------------------
 * return -1 on Error
 *         0 .. OK
 */
int
gap_edge_dialog(gint32 activeDrawableId, GapEdgeValues *edvalPtr)
{
  GapEdgeDetectGuiParams *edguiPtr;
  
  if(gap_debug) 
  {
    printf("\nSTART gap_edge_dialog\n");
  }
  
  edguiPtr = g_new(GapEdgeDetectGuiParams, 1);
  p_clear_GapEdgeDetectGuiParams(edguiPtr);
  edguiPtr->drawable_id = activeDrawableId;
  edguiPtr->image_id = gimp_item_get_image(activeDrawableId);
  edguiPtr->vals = edvalPtr;

  gimp_ui_init ("gap_edge_detect", FALSE);
  gap_stock_init();

  edguiPtr->run_flag = FALSE;
  edguiPtr->cursor_wait = gdk_cursor_new (GDK_WATCH);
  edguiPtr->cursor_acitve = NULL; /* use the default cursor */

  gap_edge_create_dialog(edguiPtr);
  gtk_widget_show (edguiPtr->shell);


  if(gap_debug) printf("gap_edge_dialog.c BEFORE  gtk_main\n");
  gtk_main ();
  gdk_flush ();

  if(gap_debug) printf("gap_edge_dialog.c END gap_edge_dialog\n");


  if(edguiPtr->run_flag)
  {
    g_free(edguiPtr);
    return 0;  /* OK, request to run the bluebox effect now */
  }
  g_free(edguiPtr);
  return -1;  /* for cancel or close dialog without run request */

}  /* end gap_edge_dialog */


/* ------------------------
 * gap_edge_create_dialog
 * ------------------------
 */
static GtkWidget *
gap_edge_create_dialog (GapEdgeDetectGuiParams *edguiPtr)
{
  GtkWidget *dlg;
  GtkWidget *main_vbox;
  GtkWidget *frame;
  GtkWidget *table;
  GtkWidget *label;
  GtkWidget *button;
  GtkWidget *check_button;
  GtkWidget *hseparator;
  gint       row;
  GtkObject *adj;
  
  if(gap_debug)
  {
    printf("gap_edge_create_dialog START\n");
  }

  dlg = gimp_dialog_new (_("Edge Detect (DoSoG)"), GAP_EDGE_PLUGIN_NAME,
                         NULL, 0,
                         gimp_standard_help_func, GAP_EDGE_HELP_ID,

                         GIMP_STOCK_RESET, GAP_EDGE_RESPONSE_RESET,
                         GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                         GTK_STOCK_OK,     GTK_RESPONSE_OK,
                         NULL);

  edguiPtr->shell = dlg;

  g_signal_connect (G_OBJECT (edguiPtr->shell), "response",
                    G_CALLBACK (p_edge_response),
                    edguiPtr);


  main_vbox = gtk_vbox_new (FALSE, 4);
  gtk_container_set_border_width (GTK_CONTAINER (main_vbox), 6);
  gtk_container_add (GTK_CONTAINER (GTK_DIALOG (dlg)->vbox), main_vbox);

  /*  the frame  */

  frame = gimp_frame_new (_("Edge Detect by Shift and Blur"));
  gtk_box_pack_start (GTK_BOX (main_vbox), frame, FALSE, FALSE, 0);
  gtk_widget_show (frame);

  table = gtk_table_new (10, 3, FALSE);
  edguiPtr->master_table = table;
  gtk_container_set_border_width (GTK_CONTAINER (table), 4);
  gtk_table_set_col_spacings (GTK_TABLE (table), 4);
  gtk_table_set_row_spacings (GTK_TABLE (table), 2);
  gtk_container_add (GTK_CONTAINER (frame), table);
  gtk_widget_show (table);

  row = 0;

  adj = gimp_scale_entry_new (GTK_TABLE (edguiPtr->master_table), 0, row++,
                              _("Blur R1 (X):"), SCALE_WIDTH, SPIN_BUTTON_WIDTH,
                              edguiPtr->vals->blurRadius1X,
                              0.0, 30.0,     /* lower/upper */
                              0.1, 1.0,      /* step, page */
                              1,              /* digits */
                              TRUE,           /* constrain */
                              0.0, 1.0,       /* lower/upper unconstrained */
                              _("Blur radius 1 X direction"), NULL);
  edguiPtr->blurRadius1X_adj = adj;
  g_object_set_data(G_OBJECT(adj), "edguiPtr", edguiPtr);
  g_signal_connect (adj, "value_changed",
                    G_CALLBACK (p_gdouble_adjustment_callback),
                    &edguiPtr->vals->blurRadius1X);

  row++;


  adj = gimp_scale_entry_new (GTK_TABLE (edguiPtr->master_table), 0, row++,
                              _("Blur R1 (Y):"), SCALE_WIDTH, SPIN_BUTTON_WIDTH,
                              edguiPtr->vals->blurRadius1Y,
                              0.0, 30.0,     /* lower/upper */
                              0.1, 1.0,      /* step, page */
                              1,              /* digits */
                              TRUE,           /* constrain */
                              0.0, 1.0,       /* lower/upper unconstrained */
                              _("Blur radius 1 Y direction"), NULL);
  edguiPtr->blurRadius1Y_adj = adj;
  g_object_set_data(G_OBJECT(adj), "edguiPtr", edguiPtr);
  g_signal_connect (adj, "value_changed",
                    G_CALLBACK (p_gdouble_adjustment_callback),
                    &edguiPtr->vals->blurRadius1Y);


  row++;

  adj = gimp_scale_entry_new (GTK_TABLE (edguiPtr->master_table), 0, row++,
                              _("Blur R2 (X):"), SCALE_WIDTH, SPIN_BUTTON_WIDTH,
                              edguiPtr->vals->blurRadius2X,
                              0.0, 30.0,     /* lower/upper */
                              0.1, 1.0,      /* step, page */
                              1,              /* digits */
                              TRUE,           /* constrain */
                              0.0, 1.0,       /* lower/upper unconstrained */
                              _("Blur radius 2 X direction"), NULL);
  edguiPtr->blurRadius2X_adj = adj;
  g_object_set_data(G_OBJECT(adj), "edguiPtr", edguiPtr);
  g_signal_connect (adj, "value_changed",
                    G_CALLBACK (p_gdouble_adjustment_callback),
                    &edguiPtr->vals->blurRadius2X);





  row++;
  
  adj = gimp_scale_entry_new (GTK_TABLE (edguiPtr->master_table), 0, row++,
                                _("Blur R2 (Y):"), SCALE_WIDTH, SPIN_BUTTON_WIDTH,
                                edguiPtr->vals->blurRadius2Y,
                                0.0, 30.0,     /* lower/upper */
                                0.1, 1.0,      /* step, page */
                                1,              /* digits */
                                TRUE,           /* constrain */
                                0.0, 1.0,       /* lower/upper unconstrained */
                                _("Blur radius 2 Y direction"), NULL);
   edguiPtr->blurRadius2Y_adj = adj;
   g_object_set_data(G_OBJECT(adj), "edguiPtr", edguiPtr);
   g_signal_connect (adj, "value_changed",
                     G_CALLBACK (p_gdouble_adjustment_callback),
                     &edguiPtr->vals->blurRadius2Y);
                    
                    
  row++;
  
  adj = gimp_scale_entry_new (GTK_TABLE (edguiPtr->master_table), 0, row++,
                                _("Shift Left:"), SCALE_WIDTH, SPIN_BUTTON_WIDTH,
                                edguiPtr->vals->shiftLeft,
                                0.0, 5.0,     /* lower/upper */
                                1.0, 1.0,      /* step, page */
                                0,              /* digits */
                                TRUE,           /* constrain */
                                0.0, 1.0,       /* lower/upper unconstrained */
                                _("Shift left by n pixels"), NULL);
   edguiPtr->shiftLeft_adj = adj;
   g_object_set_data(G_OBJECT(adj), "edguiPtr", edguiPtr);
   g_signal_connect (adj, "value_changed",
                     G_CALLBACK (p_gint32_adjustment_callback),
                     &edguiPtr->vals->shiftLeft);


  row++;
  
  adj = gimp_scale_entry_new (GTK_TABLE (edguiPtr->master_table), 0, row++,
                                _("Shift Right:"), SCALE_WIDTH, SPIN_BUTTON_WIDTH,
                                edguiPtr->vals->shiftRight,
                                0.0, 5.0,     /* lower/upper */
                                1.0, 1.0,      /* step, page */
                                0,              /* digits */
                                TRUE,           /* constrain */
                                0.0, 1.0,       /* lower/upper unconstrained */
                                _("Shift right by n pixels"), NULL);
   edguiPtr->shiftRight_adj = adj;
   g_object_set_data(G_OBJECT(adj), "edguiPtr", edguiPtr);
   g_signal_connect (adj, "value_changed",
                     G_CALLBACK (p_gint32_adjustment_callback),
                     &edguiPtr->vals->shiftRight);

  row++;
  
  adj = gimp_scale_entry_new (GTK_TABLE (edguiPtr->master_table), 0, row++,
                                _("Shift Up:"), SCALE_WIDTH, SPIN_BUTTON_WIDTH,
                                edguiPtr->vals->shiftUp,
                                0.0, 5.0,     /* lower/upper */
                                1.0, 1.0,      /* step, page */
                                0,              /* digits */
                                TRUE,           /* constrain */
                                0.0, 1.0,       /* lower/upper unconstrained */
                                _("Shift up by n pixels"), NULL);
   edguiPtr->shiftUp_adj = adj;
   g_object_set_data(G_OBJECT(adj), "edguiPtr", edguiPtr);
   g_signal_connect (adj, "value_changed",
                     G_CALLBACK (p_gint32_adjustment_callback),
                     &edguiPtr->vals->shiftUp);


  row++;
  
  adj = gimp_scale_entry_new (GTK_TABLE (edguiPtr->master_table), 0, row++,
                                _("Shift Down:"), SCALE_WIDTH, SPIN_BUTTON_WIDTH,
                                edguiPtr->vals->shiftDown,
                                0.0, 5.0,     /* lower/upper */
                                1.0, 1.0,      /* step, page */
                                0,              /* digits */
                                TRUE,           /* constrain */
                                0.0, 1.0,       /* lower/upper unconstrained */
                                _("Shift down by n pixels"), NULL);
   edguiPtr->shiftDown_adj = adj;
   g_object_set_data(G_OBJECT(adj), "edguiPtr", edguiPtr);
   g_signal_connect (adj, "value_changed",
                     G_CALLBACK (p_gint32_adjustment_callback),
                     &edguiPtr->vals->shiftDown);

  row++;

  label = gtk_label_new(_("Auto Levels:"));
  gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.0);
  gtk_table_attach(GTK_TABLE (table), label, 0, 1, row, row + 1
                  , GTK_FILL, GTK_FILL, 0, 0);
  gtk_widget_show(label);

  /* check button */
  check_button = gtk_check_button_new_with_label (" ");
  gtk_table_attach ( GTK_TABLE (table), check_button, 1, 3, row, row+1, GTK_FILL, 0, 0, 0);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (check_button),
                                edguiPtr->vals->autoLevels);
  gimp_help_set_help_data(check_button, _("ON: apply auto stretch levels"), NULL);
  gtk_widget_show(check_button);
  edguiPtr->autoLevels_toggle = check_button;
  g_object_set_data(G_OBJECT(check_button), "edguiPtr", edguiPtr);
  g_signal_connect (G_OBJECT (check_button), "toggled",
                    G_CALLBACK (p_toggle_update_callback),
                    &edguiPtr->vals->autoLevels);

  row++;


  label = gtk_label_new(_("Desaturate:"));
  gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.0);
  gtk_table_attach(GTK_TABLE (table), label, 0, 1, row, row + 1
                  , GTK_FILL, GTK_FILL, 0, 0);
  gtk_widget_show(label);

  /* check button */
  check_button = gtk_check_button_new_with_label (" ");
  gtk_table_attach ( GTK_TABLE (table), check_button, 1, 3, row, row+1, GTK_FILL, 0, 0, 0);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (check_button),
                                edguiPtr->vals->desaturate);
  gimp_help_set_help_data(check_button, _("ON: Desaturate result to shades of grey"), NULL);
  gtk_widget_show(check_button);
  edguiPtr->desaturate_toggle = check_button;
  g_object_set_data(G_OBJECT(check_button), "edguiPtr", edguiPtr);
  g_signal_connect (G_OBJECT (check_button), "toggled",
                    G_CALLBACK (p_toggle_update_callback),
                    &edguiPtr->vals->desaturate);


  row++;

  label = gtk_label_new(_("Invert:"));
  gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.0);
  gtk_table_attach(GTK_TABLE (table), label, 0, 1, row, row + 1
                  , GTK_FILL, GTK_FILL, 0, 0);
  gtk_widget_show(label);

  /* check button */
  check_button = gtk_check_button_new_with_label (" ");
  gtk_table_attach ( GTK_TABLE (table), check_button, 1, 3, row, row+1, GTK_FILL, 0, 0, 0);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (check_button),
                                edguiPtr->vals->invert);
  gimp_help_set_help_data(check_button, _("ON: Invert (Black edge lines on white area) OFF: White lines on black area"), NULL);
  gtk_widget_show(check_button);
  edguiPtr->invert_toggle = check_button;
  g_object_set_data(G_OBJECT(check_button), "edguiPtr", edguiPtr);
  g_signal_connect (G_OBJECT (check_button), "toggled",
                    G_CALLBACK (p_toggle_update_callback),
                    &edguiPtr->vals->invert);


  row++;


  label = gtk_label_new(_("Create Layer:"));
  gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.0);
  gtk_table_attach(GTK_TABLE (table), label, 0, 1, row, row + 1
                  , GTK_FILL, GTK_FILL, 0, 0);
  gtk_widget_show(label);

  /* check button */
  check_button = gtk_check_button_new_with_label (" ");
  gtk_table_attach ( GTK_TABLE (table), check_button, 1, 3, row, row+1, GTK_FILL, 0, 0, 0);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (check_button),
                                edguiPtr->vals->createEdgeAsNewLayer);
  gimp_help_set_help_data(check_button, _("ON: Render result as new layer OFF: render replaces original layers content"), NULL);
  gtk_widget_show(check_button);
  edguiPtr->createEdgeAsNewLayer_toggle = check_button;
  g_object_set_data(G_OBJECT(check_button), "edguiPtr", edguiPtr);
  g_signal_connect (G_OBJECT (check_button), "toggled",
                    G_CALLBACK (p_toggle_update_callback),
                    &edguiPtr->vals->createEdgeAsNewLayer);


//  row++;
//
//   label = gtk_label_new(_("Feather Edges:"));
//   gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.0);
//   gtk_table_attach(GTK_TABLE (table), label, 0, 1, row, row + 1
//                   , GTK_FILL, GTK_FILL, 0, 0);
//   gtk_widget_show(label);
// 
//   /* check button */
//   check_button = gtk_check_button_new_with_label (" ");
//   gtk_table_attach ( GTK_TABLE (table), check_button, 1, 3, row, row+1, GTK_FILL, 0, 0, 0);
//   gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (check_button),
//                                 edguiPtr->vals->feather_edges);
//   gimp_help_set_help_data(check_button, _("ON: Feather edges using feather radius"), NULL);
//   gtk_widget_show(check_button);
//   edguiPtr->feather_edges_toggle = check_button;
//   g_object_set_data(G_OBJECT(check_button), "edguiPtr", edguiPtr);
//   g_signal_connect (G_OBJECT (check_button), "toggled",
//                     G_CALLBACK (p_toggle_update_callback),
//                     &edguiPtr->vals->feather_edges);
//   row++;
// 
//   adj = gimp_scale_entry_new (GTK_TABLE (edguiPtr->master_table), 0, row++,
//                               _("Feather Radius:"), SCALE_WIDTH, SPIN_BUTTON_WIDTH,
//                               edguiPtr->vals->feather_radius,
//                               0.0, 100.0,     /* lower/upper */
//                               1.0, 10.0,      /* step, page */
//                               1,              /* digits */
//                               TRUE,           /* constrain */
//                               0.0, 100.0,       /* lowr/upper unconstrained */
//                               _("Feather radius for smoothing the alpha channel"), NULL);
//   edguiPtr->feather_radius_adj = adj;
//   g_object_set_data(G_OBJECT(adj), "edguiPtr", edguiPtr);
//   g_signal_connect (adj, "value_changed",
//                     G_CALLBACK (p_gdouble_adjustment_callback),
//                     &edguiPtr->vals->feather_radius);
// 
//   row++;
// 
//   adj = gimp_scale_entry_new (GTK_TABLE (edguiPtr->master_table), 0, row++,
//                               _("Shrink/Grow:"), SCALE_WIDTH, SPIN_BUTTON_WIDTH,
//                               edguiPtr->vals->grow,
//                               -20.0, 20.0,     /* lower/upper */
//                               1.0, 10.0,      /* step, page */
//                               0,              /* digits */
//                               TRUE,           /* constrain */
//                               -20.0, 20.0,       /* lowr/upper unconstrained */
//                               _("Grow selection in pixels (use negative values for shrink)"), NULL);
//   edguiPtr->grow_adj = adj;
//   g_object_set_data(G_OBJECT(adj), "edguiPtr", edguiPtr);
//   g_signal_connect (adj, "value_changed",
//                     G_CALLBACK (p_gdouble_adjustment_callback),
//                     &edguiPtr->vals->grow);

  row++;

  label = gtk_label_new(_("Automatic Preview:"));
  gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.0);
  gtk_table_attach(GTK_TABLE (table), label, 0, 1, row, row + 1, GTK_FILL, GTK_FILL, 0, 0);
  gtk_widget_show(label);


  /* check button */
  check_button = gtk_check_button_new_with_label (" ");
  gtk_table_attach ( GTK_TABLE (table), check_button, 1, 2, row, row+1, GTK_FILL, 0, 0, 0);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (check_button),
                                edguiPtr->instant_preview);
  gimp_help_set_help_data(check_button, _("ON: Keep preview image up to date"), NULL);
  gtk_widget_show(check_button);
  edguiPtr->instant_preview_toggle = check_button;
  g_object_set_data(G_OBJECT(check_button), "edguiPtr", edguiPtr);
  g_signal_connect (G_OBJECT (check_button), "toggled",
                    G_CALLBACK (p_toggle_update_callback),
                    &edguiPtr->instant_preview);

  /* button */
  button = gtk_button_new_with_label(_("Preview"));
  gtk_table_attach(GTK_TABLE (table), button, 2, 3, row, row + 1, GTK_FILL, GTK_FILL, 0, 0);
  gtk_widget_show(button);
  gimp_help_set_help_data(button, _("Show preview as separate image"), NULL);
  g_signal_connect (button, "clicked",
                    G_CALLBACK (p_apply_callback),
                    edguiPtr);

  row++;

  adj = gimp_scale_entry_new (GTK_TABLE (edguiPtr->master_table), 0, row++,
                              _("Previewsize:"), SCALE_WIDTH, SPIN_BUTTON_WIDTH,
                              edguiPtr->pv_size_percent,
                              5.0, 100.0,     /* lower/upper */
                              1.0, 10.0,      /* step, page */
                              1,              /* digits */
                              TRUE,           /* constrain */
                              5.0, 100.0,       /* lowr/upper unconstrained */
                              _("Size of the preview image in percent of the original"), NULL);
  g_object_set_data(G_OBJECT(adj), "edguiPtr", edguiPtr);
  g_signal_connect (adj, "value_changed",
                    G_CALLBACK (p_gdouble_adjustment_callback),
                    &edguiPtr->pv_size_percent);


  hseparator = gtk_hseparator_new ();
  gtk_widget_show (hseparator);
  gtk_box_pack_start (GTK_BOX (main_vbox), hseparator, FALSE, FALSE, 0);

  /*  Show the main containers  */

  gtk_widget_show (main_vbox);

  if(gap_debug)
  {
    printf("gap_edge_create_dialog DONE\n");
  }


  return(dlg);
}  /* end gap_edge_create_dialog */


/* ---------------------------------
 * p_reset_callback
 * ---------------------------------
 */
static void
p_reset_callback(GtkWidget *w, GapEdgeDetectGuiParams *edguiPtr)
{
  if(edguiPtr)
  {
    p_edge_init_default_vals(edguiPtr->vals);

    if(edguiPtr->blurRadius1X_adj)
    {
      gtk_adjustment_set_value (GTK_ADJUSTMENT (edguiPtr->blurRadius1X_adj), (gfloat)edguiPtr->vals->blurRadius1X);
    }
    if(edguiPtr->blurRadius1Y_adj)
    {
      gtk_adjustment_set_value (GTK_ADJUSTMENT (edguiPtr->blurRadius1Y_adj), (gfloat)edguiPtr->vals->blurRadius1Y);
    }
    if(edguiPtr->blurRadius2X_adj)
    {
      gtk_adjustment_set_value (GTK_ADJUSTMENT (edguiPtr->blurRadius2X_adj), (gfloat)edguiPtr->vals->blurRadius2X);
    }
    if(edguiPtr->blurRadius2Y_adj)
    {
      gtk_adjustment_set_value (GTK_ADJUSTMENT (edguiPtr->blurRadius2Y_adj), (gfloat)edguiPtr->vals->blurRadius2Y);
    }
    if(edguiPtr->shiftLeft_adj)
    {
      gtk_adjustment_set_value (GTK_ADJUSTMENT (edguiPtr->shiftLeft_adj), (gfloat)edguiPtr->vals->shiftLeft);
    }
    if(edguiPtr->shiftRight_adj)
    {
      gtk_adjustment_set_value (GTK_ADJUSTMENT (edguiPtr->shiftRight_adj), (gfloat)edguiPtr->vals->shiftRight);
    }
    if(edguiPtr->shiftUp_adj)
    {
      gtk_adjustment_set_value (GTK_ADJUSTMENT (edguiPtr->shiftUp_adj), (gfloat)edguiPtr->vals->shiftUp);
    }
    if(edguiPtr->shiftDown_adj)
    {
      gtk_adjustment_set_value (GTK_ADJUSTMENT (edguiPtr->shiftDown_adj), (gfloat)edguiPtr->vals->shiftDown);
    }

    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (edguiPtr->autoLevels_toggle)
                                  ,edguiPtr->vals->autoLevels);
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (edguiPtr->desaturate_toggle)
                                  ,edguiPtr->vals->desaturate);
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (edguiPtr->invert_toggle)
                                  ,edguiPtr->vals->invert);
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (edguiPtr->createEdgeAsNewLayer_toggle)
                                  ,edguiPtr->vals->createEdgeAsNewLayer);

    //gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (edguiPtr->feather_edges_toggle)
    //                              ,edguiPtr->vals->feather_edges);
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (edguiPtr->instant_preview_toggle)
                                  ,edguiPtr->instant_preview);

  }

}  /* end p_reset_callback */


/* ---------------------------------
 * p_quit_callback
 * ---------------------------------
 */
static void
p_quit_callback(GtkWidget *w, GapEdgeDetectGuiParams *edguiPtr)
{
  static gboolean main_quit_flag = TRUE;

  /* finish pending que draw ops before we close */
  gdk_flush ();

  if(edguiPtr)
  {
     p_remove_timer(edguiPtr);
     if(edguiPtr->shell)
     {
       GtkWidget *l_shell;

       l_shell = edguiPtr->shell;
       main_quit_flag = TRUE;

       /* cleanup wiget stuff */
       p_clear_GapEdgeDetectGuiParams(edguiPtr);

       /* p_quit_callback is the signal handler for the "destroy"
        * signal of the shell window.
        * the gtk_widget_destroy call will immediate reenter this procedure.
        * (for this reason the edguiPtr->shell is set to NULL
        *  before the gtk_widget_destroy call)
        */
       gtk_widget_destroy(l_shell);
     }


  }

  if(main_quit_flag)
  {
    main_quit_flag = FALSE;
    gtk_main_quit();
  }

}  /* end p_quit_callback */


/* ---------------------------------
 * p_edge_response
 * ---------------------------------
 */
static void
p_edge_response (GtkWidget *widget,
                 gint       response_id,
                 GapEdgeDetectGuiParams *edguiPtr)
{
  switch (response_id)
  {
    case GAP_EDGE_RESPONSE_RESET:
      p_reset_callback(widget, edguiPtr);
      break;

    case GTK_RESPONSE_OK:
      edguiPtr->run_flag = TRUE;

    default:
      gtk_widget_hide (widget);
      p_quit_callback (widget, edguiPtr);
      break;
  }
}  /* end p_edge_response */

/* ---------------------------------
 * p_set_waiting_cursor
 * ---------------------------------
 */
static void
p_set_waiting_cursor(GapEdgeDetectGuiParams *edguiPtr)
{
  if(edguiPtr == NULL) return;

  gdk_window_set_cursor(GTK_WIDGET(edguiPtr->shell)->window, edguiPtr->cursor_wait);
  gdk_flush();

  /* g_main_context_iteration makes sure that waiting cursor is displayed */
  while(g_main_context_iteration(NULL, FALSE));

  gdk_flush();
}  /* end p_set_waiting_cursor */

/* ---------------------------------
 * p_set_active_cursor
 * ---------------------------------
 */
static void
p_set_active_cursor(GapEdgeDetectGuiParams *edguiPtr)
{
  if(edguiPtr == NULL) return;

  gdk_window_set_cursor(GTK_WIDGET(edguiPtr->shell)->window, edguiPtr->cursor_acitve);
  gdk_flush();
}  /* end p_set_active_cursor */

/* ---------------------------------
 * p_apply_callback
 * ---------------------------------
 */
static void
p_apply_callback(GtkWidget *w, GapEdgeDetectGuiParams *edguiPtr)
{
  if(edguiPtr)
  {
     if(!edguiPtr->instant_preview)
     {
       p_set_waiting_cursor(edguiPtr);
     }
     edguiPtr->layer_id = edguiPtr->drawable_id;
     if(!p_edge_detect_apply(edguiPtr))
     {
       p_quit_callback(NULL, edguiPtr);
     }
     edguiPtr->layer_id = -1;
     p_set_active_cursor(edguiPtr);
  }
}  /* end p_apply_callback */

/* ---------------------------------
 * p_gdouble_adjustment_callback
 * ---------------------------------
 */
static void
p_gdouble_adjustment_callback(GtkObject *obj, gpointer val)
{
  GapEdgeDetectGuiParams *edguiPtr;

  gimp_double_adjustment_update(GTK_ADJUSTMENT(obj), val);

  edguiPtr = g_object_get_data( G_OBJECT(obj), "edguiPtr" );
  if(edguiPtr)
  {
    p_set_instant_apply_request(edguiPtr);
  }

}  /* end p_gdouble_adjustment_callback */




/* ---------------------------------
 * p_gint32_adjustment_callback
 * ---------------------------------
 */
static void
p_gint32_adjustment_callback(GtkObject *obj, gpointer val)
{
  GapEdgeDetectGuiParams *edguiPtr;

  gimp_int_adjustment_update(GTK_ADJUSTMENT(obj), val);

  edguiPtr = g_object_get_data( G_OBJECT(obj), "edguiPtr" );
  if(edguiPtr)
  {
    p_set_instant_apply_request(edguiPtr);
  }

}  /* end p_gint32_adjustment_callback */


/* ---------------------------------
 * p_toggle_update_callback
 * ---------------------------------
 */
static void
p_toggle_update_callback(GtkWidget *widget, gint *val)
{
  GapEdgeDetectGuiParams *edguiPtr;

  if(val)
  {
    if (GTK_TOGGLE_BUTTON (widget)->active)
    {
      *val = TRUE;
    }
    else
    {
      *val = FALSE;
    }
  }

  edguiPtr = g_object_get_data( G_OBJECT(widget), "edguiPtr" );

  if(edguiPtr)
  {
//     GtkWidget *spinbutton;
//     GtkWidget *scale;
// 
//     spinbutton = GTK_WIDGET(g_object_get_data (G_OBJECT (edguiPtr->feather_radius_adj), "spinbutton"));
//     scale = GTK_WIDGET(g_object_get_data (G_OBJECT (edguiPtr->feather_radius_adj), "scale"));
//     gtk_widget_set_sensitive(spinbutton, edguiPtr->vals->feather_edges);
//     gtk_widget_set_sensitive(scale, edguiPtr->vals->feather_edges);

    p_set_instant_apply_request(edguiPtr);
    if(edguiPtr->instant_preview)
    {
      p_install_timer(edguiPtr);
    }
  }

}  /* end p_toggle_update_callback */


/* ---------------------------------
 * p_set_instant_apply_request
 * ---------------------------------
 */
static void
p_set_instant_apply_request(GapEdgeDetectGuiParams *edguiPtr)
{
  if(edguiPtr)
  {
    edguiPtr->instant_apply_request = TRUE; /* request is handled by timer */
  }
}  /* end p_set_instant_apply_request */

/* --------------------------
 * install / remove timer
 * --------------------------
 */
static void
p_install_timer(GapEdgeDetectGuiParams *edguiPtr)
{
  if(edguiPtr->instant_timertag < 0)
  {
    edguiPtr->instant_timertag = (gint32) g_timeout_add(INSTANT_TIMERINTERVAL_MILLISEC,
                                      (GtkFunction)p_instant_timer_callback
                                      , edguiPtr);
  }
}  /* end p_install_timer */

static void
p_remove_timer(GapEdgeDetectGuiParams *edguiPtr)
{
  if(edguiPtr->instant_timertag >= 0)
  {
    g_source_remove(edguiPtr->instant_timertag);
    edguiPtr->instant_timertag = -1;
  }
}  /* end p_remove_timer */

/* --------------------------
 * p_instant_timer_callback
 * --------------------------
 * This procedure is called periodically via timer
 * when the instant_preview checkbox is ON
 */
static void
p_instant_timer_callback(gpointer   user_data)
{
  GapEdgeDetectGuiParams *edguiPtr;

  edguiPtr = user_data;
  if(edguiPtr == NULL)
  {
    return;
  }

  p_remove_timer(edguiPtr);

  if(edguiPtr->instant_apply_request)
  {
    if(gap_debug) printf("FIRE p_instant_timer_callback >>>> REQUEST is TRUE\n");
    p_apply_callback (NULL, edguiPtr);  /* the request is cleared in this procedure */
  }

  /* restart timer for next cycle */
  if(edguiPtr->instant_preview)
  {
     p_install_timer(edguiPtr);
  }
}  /* end p_instant_timer_callback */


/* ---------------------------------
 * p_remove_additonal_layers_from_preview_image
 * ---------------------------------
 * check and remove additional layer(s) in the preview image
 * (in case previous rendering or the user added new layers)
 */
static void
p_remove_additonal_layers_from_preview_image(GapEdgeDetectGuiParams *edguiPtr)
{
  gint          l_nlayers;
  gint32       *l_layers_list;
  gint32        l_idx;

  l_layers_list = gimp_image_get_layers(edguiPtr->pv_image_id, &l_nlayers);
  if(l_layers_list != NULL)
  {
    for(l_idx = 0; l_idx < l_nlayers; l_idx++)
    {
      if((l_layers_list[l_idx] == edguiPtr->pv_layer_id)
      || (l_layers_list[l_idx] == edguiPtr->pv_master_layer_id))
      {
        continue;
      }
      else
      {
        gimp_image_remove_layer(edguiPtr->pv_image_id, l_layers_list[l_idx]);
      }
    }
    g_free (l_layers_list);
  }

  
}  /* end p_remove_additonal_layers_from_preview_image */


/* ---------------------------------
 * p_edge_detect_apply
 * ---------------------------------
 * this procedure always does the processing on a preview image
 * the preview image has full layersize (not imagesize)
 *
 * the preview image size may be reduced py pv_size_percent.
 * an already existing preview image is reused.
 *
 * Show the preview image (by adding a display)
 *
 * return FALSE on Error
 *        TRUE  .. OK
 */
gboolean
p_edge_detect_apply(GapEdgeDetectGuiParams *edguiPtr)
{
  GimpDrawable *src_drawable;
  gboolean replace_pv_image;
  gint l_width;
  gint l_height;

  if(edguiPtr == NULL)                              { return FALSE; }

  if(gap_debug)
  {
    printf("Edge Detect Params:\n");
    printf("edguiPtr->image_id :%d\n", (int)edguiPtr->image_id);
    printf("edguiPtr->layer_id :%d\n", (int)edguiPtr->layer_id);
    printf("edguiPtr->pv_size_percent :%.3f\n", (float)edguiPtr->pv_size_percent);
    printf("edguiPtr->vals->blurRadius1X     :%.3f\n", (float)edguiPtr->vals->blurRadius1X);
    printf("edguiPtr->vals->blurRadius1Y     :%.3f\n", (float)edguiPtr->vals->blurRadius1Y);
    printf("edguiPtr->vals->blurRadius2X     :%.3f\n", (float)edguiPtr->vals->blurRadius2X);
    printf("edguiPtr->vals->blurRadius2Y     :%.3f\n", (float)edguiPtr->vals->blurRadius2Y);
    
    printf("edguiPtr->vals->shiftLeft        :%d\n", (int)edguiPtr->vals->shiftLeft);
    printf("edguiPtr->vals->shiftRight       :%d\n", (int)edguiPtr->vals->shiftRight);
    printf("edguiPtr->vals->shiftUp          :%d\n", (int)edguiPtr->vals->shiftUp);
    printf("edguiPtr->vals->shiftDown        :%d\n", (int)edguiPtr->vals->shiftDown);

    printf("edguiPtr->vals->autoLevels       :%s\n", edguiPtr->vals->autoLevels ? "ON" : "off" );
    printf("edguiPtr->vals->desaturate       :%s\n", edguiPtr->vals->desaturate ? "ON" : "off" );
    printf("edguiPtr->vals->invert           :%s\n", edguiPtr->vals->invert ? "ON" : "off" );
    printf("edguiPtr->vals->createNewLayer   :%s\n", edguiPtr->vals->createEdgeAsNewLayer ? "ON" : "off" );
    
    //printf("edguiPtr->vals->grow        :%.3f\n", (float)edguiPtr->vals->grow);
    //printf("edguiPtr->vals->feather_edges   :%d\n", (int)edguiPtr->vals->feather_edges);
    //printf("edguiPtr->vals->feather_radius  :%.3f\n", (float)edguiPtr->vals->feather_radius);
  }

  if(!gap_image_is_alive(edguiPtr->image_id))
  {
    g_message(_("Error: Image '%d' not found"), edguiPtr->image_id);
    return FALSE;
  }
  if(!gimp_drawable_is_layer(edguiPtr->layer_id))
  {
    g_message(_("Error: This Edge detection method operates only on layers"));
    return FALSE;
  }

  if(!gimp_drawable_has_alpha(edguiPtr->layer_id))
  {
    gimp_layer_add_alpha(edguiPtr->layer_id);
  }

  src_drawable =  gimp_drawable_get (edguiPtr->layer_id);


  /* set size (or reduced size) */
  l_width = src_drawable->width;
  l_height = src_drawable->height;
  if(edguiPtr->pv_size_percent < 100.0)
  {
    l_width = ((gdouble)src_drawable->width * edguiPtr->pv_size_percent) / 100.0;
    l_height = ((gdouble)src_drawable->height * edguiPtr->pv_size_percent) / 100.0;
  }

  replace_pv_image = FALSE;
  if(gap_image_is_alive(edguiPtr->pv_image_id))
  {
    if(gimp_image_base_type(edguiPtr->image_id) != gimp_image_base_type(edguiPtr->pv_image_id))
    {
      replace_pv_image = TRUE;
    }
    else
    {
      if((l_width     != gimp_image_width(edguiPtr->pv_image_id))
      || (l_height    != gimp_image_height(edguiPtr->pv_image_id)))
      {
        /* check if we can reuse the preview image
         * (that was built in a previus call)
         */
        if(edguiPtr->pv_master_layer_id < 0)
        {
          replace_pv_image = TRUE;
        }
        else
        {
          if(l_width > gimp_image_width(edguiPtr->pv_image_id))
          {
            /* must force recreate the master copy for quality reasons */
            if(edguiPtr->pv_master_layer_id >= 0)
            {
              gimp_image_remove_layer(edguiPtr->pv_image_id, edguiPtr->pv_master_layer_id);
              edguiPtr->pv_master_layer_id = -1;
            }
          }
          gimp_image_scale(edguiPtr->pv_image_id, l_width, l_height);

        }
      }
    }
    if(replace_pv_image)
    {
      gimp_image_delete(edguiPtr->pv_image_id);
      edguiPtr->pv_image_id = -1;
      edguiPtr->pv_master_layer_id = -1;
      edguiPtr->pv_display_id = -1;
    }
  }
  else
  {
    replace_pv_image = TRUE;
  }


  /* (re)create the preview image if there is none */
  if(replace_pv_image)
  {
    edguiPtr->pv_display_id = -1;
    edguiPtr->pv_master_layer_id = -1;
    edguiPtr->pv_image_id = gimp_image_new(l_width
                                     ,l_height
                                     ,GIMP_RGB
                                     );
    gimp_image_set_filename(edguiPtr->pv_image_id, _("EdgeDetectionPreview.xcf"));
    edguiPtr->pv_layer_id = gimp_layer_new(edguiPtr->pv_image_id, _("Previewlayer")
                                 ,l_width
                                 ,l_height
                                 ,GIMP_RGBA_IMAGE
                                 ,100.0            /* Opacity full opaque */
                                 ,GIMP_NORMAL_MODE
                                 );
    gimp_image_insert_layer(edguiPtr->pv_image_id, edguiPtr->pv_layer_id, 0, 0);
    gimp_layer_set_offsets(edguiPtr->pv_layer_id, 0, 0);

    if(!gimp_drawable_has_alpha(edguiPtr->pv_layer_id))
    {
      gimp_layer_add_alpha(edguiPtr->pv_layer_id);
    }
  }

  
  if(edguiPtr->pv_master_layer_id < 0)
  {
    /* at 1.st call create a mastercopy of the original layer
     * at the bottom of the layerstack
     * (and scale to preview size when sizes are different)
     */
    edguiPtr->pv_master_layer_id = gimp_layer_new(edguiPtr->pv_image_id, _("Masterlayer")
                                 ,src_drawable->width
                                 ,src_drawable->height
                                 ,GIMP_RGBA_IMAGE
                                 ,100.0            /* Opacity full opaque */
                                 ,GIMP_NORMAL_MODE
                                 );
    gimp_image_insert_layer(edguiPtr->pv_image_id, edguiPtr->pv_master_layer_id, 0, -1);

    if(!gimp_drawable_has_alpha(edguiPtr->pv_master_layer_id))
    {
      gimp_layer_add_alpha(edguiPtr->pv_master_layer_id);
    }
    gap_layer_copy_content(edguiPtr->pv_master_layer_id  /* dest */
                          , edguiPtr->layer_id           /* src */
                          );
    if ((l_width != src_drawable->width) || (l_height != src_drawable->height))
    {
      gimp_layer_scale(edguiPtr->pv_master_layer_id, l_width, l_height, 0);
    }
    gimp_layer_set_offsets(edguiPtr->pv_master_layer_id, 0, 0);
    gimp_item_set_visible(edguiPtr->pv_master_layer_id, FALSE);
  }

  /* copy from the master (that is a probabl scaled copy of the 
   * original when operating with reduced preview size) 
   */
  gap_layer_copy_content(edguiPtr->pv_layer_id           /* dest */
                       , edguiPtr->pv_master_layer_id    /* src */
                       );
  

  /* keep only 2 layers (master and pv_layer_id) */
  p_remove_additonal_layers_from_preview_image(edguiPtr);
  
  /* reorder the layerstack (master at bottom pv_layer_id on top and set active) */
  gimp_image_lower_item_to_bottom(edguiPtr->pv_image_id, edguiPtr->pv_master_layer_id);

  gimp_image_set_active_layer(edguiPtr->pv_image_id, edguiPtr->pv_layer_id); 



  /* call the edge detection RENDER method */
  gap_edgeDetectionByShiftBlurDiff (edguiPtr->pv_layer_id, edguiPtr->vals);

  if((edguiPtr->pv_display_id < 0)
  && (edguiPtr->pv_image_id >= 0))
  {
    edguiPtr->pv_display_id = gimp_display_new(edguiPtr->pv_image_id);
  }

  gimp_displays_flush();

  edguiPtr->instant_apply_request = FALSE;
  return TRUE;
}  /* end p_edge_detect_apply */
