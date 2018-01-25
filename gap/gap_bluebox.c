/* gap_bluebox.c
 *  by hof (Wolfgang Hofer)
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

/* revision history:
 * gimp    2.1.0a;  2004/11/29  hof: #159593
 * gimp    2.1.0a;  2004/06/26  hof: #144649 use NULL for the default cursor as active_cursor
 * gimp    1.3.23b; 2003/12/06  hof: added mode GAP_BLUBOX_THRES_ALL
 * gimp    1.3.23a; 2003/11/26  hof: follow API changes for gimp_dialog_new
 * gimp    1.3.20d; 2003/10/14  hof: creation
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
#include "gap_bluebox.h"
#include "gap_colordiff.h"


/* instant apply is implemented via timer, configured to fire 10 times per second (100 msec)
 * this collects instant_apply_requests set by other widget callbacks and events
 * and then update only once.
 * The timer is completely removed, when instant_preview is OFF
 * instant_preview requires much CPU and IO power especially on big images
 * and images with many layers
 */
#define INSTANT_TIMERINTERVAL_MILLISEC  100


#define  GAP_SUMM_HSV(h, s, v) ((50 * h) + (30 * v) + (20 * s))
#define  GAP_SUMM_RGB(r, g, b) ((     r) + (     g) + (     b))

#define GAP_BLUEBOX_RESPONSE_RESET 1


/*  some definitions   */

#define MIX_CHANNEL(b, a, m)  (((a * m) + (b * (255 - m))) / 255)
#define PREVIEW_BG_GRAY1 80
#define PREVIEW_BG_GRAY2 180
#define SCALE_WIDTH        180
#define SPIN_BUTTON_WIDTH   75


/* ------------------------
 * gap DEBUG switch
 * ------------------------
 */

/* int gap_debug = 1; */    /* print debug infos */
/* int gap_debug = 0; */    /* 0: dont print debug infos */

extern int gap_debug;

/* -----------------------
 * procedure declarations
 * -----------------------
 */
static void       p_reset_callback(GtkWidget *w, GapBlueboxGlobalParams *bbp);
static void       p_quit_callback(GtkWidget *w, GapBlueboxGlobalParams *bbp);
static void       p_bluebox_response(GtkWidget *w, gint response_id, GapBlueboxGlobalParams *bbp);
static void       p_set_waiting_cursor(GapBlueboxGlobalParams *bbp);
static void       p_set_active_cursor(GapBlueboxGlobalParams *bbp);
static void       p_apply_callback(GtkWidget *w, GapBlueboxGlobalParams *bbp);
static void       p_gdouble_adjustment_callback(GtkObject *obj, gpointer val);
static void       p_toggle_update_callback(GtkWidget *widget, gint *val);
static void       p_color_update_callback(GtkWidget *widget, gpointer val);
static void       p_radio_thres_RGB_callback(GtkWidget *widget, GapBlueboxGlobalParams *bbp);
static void       p_radio_thres_HSV_callback(GtkWidget *widget, GapBlueboxGlobalParams *bbp);
static void       p_radio_thres_VAL_callback(GtkWidget *widget, GapBlueboxGlobalParams *bbp);
static void       p_radio_thres_E94_callback(GtkWidget *widget, GapBlueboxGlobalParams *bbp);
static void       p_radio_thres_E2000_callback(GtkWidget *widget, GapBlueboxGlobalParams *bbp);
static void       p_radio_thres_ALL_callback(GtkWidget *widget, GapBlueboxGlobalParams *bbp);
static void       p_radio_create_thres_mode(GtkWidget *table, int row, int col, GapBlueboxGlobalParams *bbp);
static void       p_thres_table_RGB_create(GapBlueboxGlobalParams *bbp, gint row);
static void       p_thres_table_HSV_create(GapBlueboxGlobalParams *bbp, gint row);
static void       p_thres_table_VAL_create(GapBlueboxGlobalParams *bbp);
static GtkWidget *p_thres_table_create_or_replace(GapBlueboxGlobalParams *bbp);

static void       p_set_instant_apply_request(GapBlueboxGlobalParams *bbp);
static void       p_install_timer(GapBlueboxGlobalParams *bbp);
static void       p_remove_timer(GapBlueboxGlobalParams *bbp);
static void       p_instant_timer_callback (gpointer   user_data);



/* ---------------------------------
 * gap_bluebox_init_default_vals
 * ---------------------------------
 */
void
gap_bluebox_init_default_vals(GapBlueboxGlobalParams *bbp)
{
  gimp_rgb_set(&bbp->vals.keycolor, 0.2, 0.2, 1.0); /* BLUE keycolor */
  gimp_rgb_set_alpha(&bbp->vals.keycolor, 1.0);
  bbp->vals.thres_mode = GAP_BLUBOX_THRES_HSV;
  bbp->vals.thres_r = 0.85;
  bbp->vals.thres_g = 0.85;
  bbp->vals.thres_b = 0.25;
  bbp->vals.thres_h = 0.1;
  bbp->vals.thres_s = 0.7;
  bbp->vals.thres_v = 0.7;
  bbp->vals.thres = 0.1;
  bbp->vals.tolerance = 0.0;
  bbp->vals.grow = 0;
  bbp->vals.feather_edges = TRUE;
  bbp->vals.feather_radius = 2.0;
  bbp->vals.source_alpha = 0.1;
  bbp->vals.target_alpha = 0.0;

}  /* end gap_bluebox_init_default_vals */

/* ---------------------------------
 * gap_bluebox_bbp_new
 * ---------------------------------
 * allocate Blubox parameters and init them
 * with values from last run
 * (or default values if there is none)
 */
GapBlueboxGlobalParams *
gap_bluebox_bbp_new(gint32 layer_id)
{
  GapBlueboxGlobalParams *bbp;

  bbp = g_new(GapBlueboxGlobalParams, 1);

  bbp->drawable_id = layer_id;
  bbp->layer_id = -1;
  bbp->image_id = -1;
  bbp->image_filename = NULL;
  if(layer_id >= 0)
  {
    bbp->image_id = gimp_item_get_image(layer_id);
    bbp->image_filename = g_strdup(gimp_image_get_filename(bbp->image_id));
  }
  bbp->run_mode = GIMP_RUN_NONINTERACTIVE;
  bbp->pv_display_id = -1;
  bbp->pv_image_id = -1;
  bbp->pv_layer_id = -1;
  bbp->pv_master_layer_id = -1;
  bbp->pv_size_percent = 50.0;
  bbp->shell = NULL;
  bbp->thres_table = NULL;
  bbp->instant_preview = FALSE;
  bbp->instant_apply_request = FALSE;
  bbp->instant_timertag = -1;

  /*  Init with default Values  */
  gap_bluebox_init_default_vals(bbp);

  /*  Possibly retrieve data  */
  gimp_get_data (GAP_BLUEBOX_DATA_KEY_VALS, &bbp->vals);

  return (bbp);
}  /* end gap_bluebox_bbp_new */

/* ---------------------------------
 * the Bluebox MAIN dialog
 * ---------------------------------
 * return -1 on Error
 *         0 .. OK
 */
int
gap_bluebox_dialog(GapBlueboxGlobalParams *bbp)
{
  if(gap_debug) printf("\nSTART gap_bluebox_dialog\n");

  gimp_ui_init ("gap_bluebox", FALSE);
  gap_stock_init();

  bbp->run_flag = FALSE;
  bbp->cursor_wait = gdk_cursor_new (GDK_WATCH);
  bbp->cursor_acitve = NULL; /* use the default cursor */

  gap_bluebox_create_dialog(bbp);
  gtk_widget_show (bbp->shell);


  if(gap_debug) printf("gap_bluebox.c BEFORE  gtk_main\n");
  gtk_main ();
  gdk_flush ();

  if(gap_debug) printf("gap_bluebox.c END gap_bluebox_dialog\n");


  if(bbp->run_flag)
  {
    return 0;  /* OK, request to run the bluebox effect now */
  }
  return -1;  /* for cancel or close dialog without run request */

}  /* end gap_bluebox_dialog */


/* ------------------------
 * gap_bluebox_create_dialog
 * ------------------------
 */
GtkWidget *
gap_bluebox_create_dialog (GapBlueboxGlobalParams *bbp)
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


  dlg = gimp_dialog_new (_("Bluebox"), GAP_BLUEBOX_PLUGIN_NAME,
                         NULL, 0,
                         gimp_standard_help_func, GAP_BLUEBOX_HELP_ID,

                         GIMP_STOCK_RESET, GAP_BLUEBOX_RESPONSE_RESET,
                         GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                         GTK_STOCK_OK,     GTK_RESPONSE_OK,
                         NULL);

  bbp->shell = dlg;

  g_signal_connect (G_OBJECT (bbp->shell), "response",
                    G_CALLBACK (p_bluebox_response),
                    bbp);


  main_vbox = gtk_vbox_new (FALSE, 4);
  gtk_container_set_border_width (GTK_CONTAINER (main_vbox), 6);
  gtk_container_add (GTK_CONTAINER (GTK_DIALOG (dlg)->vbox), main_vbox);

  /*  the frame  */

  frame = gimp_frame_new (_("Select By Color"));
  gtk_box_pack_start (GTK_BOX (main_vbox), frame, FALSE, FALSE, 0);
  gtk_widget_show (frame);

  table = gtk_table_new (10, 3, FALSE);
  bbp->master_table = table;
  gtk_container_set_border_width (GTK_CONTAINER (table), 4);
  gtk_table_set_col_spacings (GTK_TABLE (table), 4);
  gtk_table_set_row_spacings (GTK_TABLE (table), 2);
  gtk_container_add (GTK_CONTAINER (frame), table);
  gtk_widget_show (table);

  row = 0;

  /* the keycolor label */
  label = gtk_label_new (_("Keycolor:"));
  gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.0);
  gtk_table_attach_defaults (GTK_TABLE(table), label, 0, 1, row, row+1);
  gtk_widget_show (label);

  /* the keycolor button */
  button = gimp_color_button_new (_("Bluebox Color Picker"),
                                  40, 20,                     /* WIDTH, HEIGHT, */
                                  &bbp->vals.keycolor,
                                  GIMP_COLOR_AREA_FLAT);
  gtk_table_attach (GTK_TABLE (table), button, 1, 3, row, row+1,
                    GTK_FILL | GTK_EXPAND, GTK_SHRINK, 0, 0) ;
  gtk_widget_show (button);
  g_object_set_data(G_OBJECT(button), "bbp", bbp);
  bbp->keycolor_button = button;

  g_signal_connect (button, "color_changed",
                    G_CALLBACK (p_color_update_callback),
                    &bbp->vals.keycolor);

  row++;
  bbp->thres_table_attach_row = row;

  p_thres_table_create_or_replace(bbp);
  row++;

  p_radio_create_thres_mode(table, row, 0, bbp);
  row++;

  adj = gimp_scale_entry_new (GTK_TABLE (bbp->master_table), 0, row++,
                              _("Alpha Tolerance:"), SCALE_WIDTH, SPIN_BUTTON_WIDTH,
                              bbp->vals.tolerance,
                              0.0, 1.0,     /* lower/upper */
                              0.01, 0.1,      /* step, page */
                              3,              /* digits */
                              TRUE,           /* constrain */
                              0.0, 1.0,       /* lower/upper unconstrained */
                              _("Sharp pixel selection by color with 0.0. "
                                "Values greater than 0 give the selection more or less variable alpha value, "
                                "depending on disparity with the key color."), NULL);
  bbp->tolerance_adj = adj;
  g_object_set_data(G_OBJECT(adj), "bbp", bbp);
  g_signal_connect (adj, "value_changed",
                    G_CALLBACK (p_gdouble_adjustment_callback),
                    &bbp->vals.tolerance);

  row++;

  adj = gimp_scale_entry_new (GTK_TABLE (bbp->master_table), 0, row++,
                              _("Source Alpha:"), SCALE_WIDTH, SPIN_BUTTON_WIDTH,
                              bbp->vals.source_alpha,
                              0.0, 1.0,       /* lower/upper */
                              0.01, 0.1,      /* step, page */
                              3,              /* digits */
                              TRUE,           /* constrain */
                              0.0, 1.0,       /* lowr/upper unconstrained */
                              _("Affect only pixels with alpha >= source alpha "
                                "where 1.0 is full opaque"), NULL);
  bbp->source_alpha_adj = adj;
  g_object_set_data(G_OBJECT(adj), "bbp", bbp);
  g_signal_connect (adj, "value_changed",
                    G_CALLBACK (p_gdouble_adjustment_callback),
                    &bbp->vals.source_alpha);

  row++;

  adj = gimp_scale_entry_new (GTK_TABLE (bbp->master_table), 0, row++,
                              _("Target Alpha:"), SCALE_WIDTH, SPIN_BUTTON_WIDTH,
                              bbp->vals.target_alpha,
                              0.0, 1.0,       /* lower/upper */
                              0.01, 0.1,      /* step, page */
                              3,              /* digits */
                              TRUE,           /* constrain */
                              0.0, 1.0,       /* lowr/upper unconstrained */
                              _("Set alpha of affected pixel to target alpha "
                                "where 0.0 is full transparent"), NULL);
  bbp->target_alpha_adj = adj;
  g_object_set_data(G_OBJECT(adj), "bbp", bbp);
  g_signal_connect (adj, "value_changed",
                    G_CALLBACK (p_gdouble_adjustment_callback),
                    &bbp->vals.target_alpha);


  row++;

  label = gtk_label_new(_("Feather Edges:"));
  gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.0);
  gtk_table_attach(GTK_TABLE (table), label, 0, 1, row, row + 1
                  , GTK_FILL, GTK_FILL, 0, 0);
  gtk_widget_show(label);

  /* check button */
  check_button = gtk_check_button_new_with_label (" ");
  gtk_table_attach ( GTK_TABLE (table), check_button, 1, 3, row, row+1, GTK_FILL, 0, 0, 0);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (check_button),
                                bbp->vals.feather_edges);
  gimp_help_set_help_data(check_button, _("ON: Feather edges using feather radius"), NULL);
  gtk_widget_show(check_button);
  bbp->feather_edges_toggle = check_button;
  g_object_set_data(G_OBJECT(check_button), "bbp", bbp);
  g_signal_connect (G_OBJECT (check_button), "toggled",
                    G_CALLBACK (p_toggle_update_callback),
                    &bbp->vals.feather_edges);
  row++;

  adj = gimp_scale_entry_new (GTK_TABLE (bbp->master_table), 0, row++,
                              _("Feather Radius:"), SCALE_WIDTH, SPIN_BUTTON_WIDTH,
                              bbp->vals.feather_radius,
                              0.0, 100.0,     /* lower/upper */
                              1.0, 10.0,      /* step, page */
                              1,              /* digits */
                              TRUE,           /* constrain */
                              0.0, 100.0,       /* lowr/upper unconstrained */
                              _("Feather radius for smoothing the alpha channel"), NULL);
  bbp->feather_radius_adj = adj;
  g_object_set_data(G_OBJECT(adj), "bbp", bbp);
  g_signal_connect (adj, "value_changed",
                    G_CALLBACK (p_gdouble_adjustment_callback),
                    &bbp->vals.feather_radius);

  row++;

  adj = gimp_scale_entry_new (GTK_TABLE (bbp->master_table), 0, row++,
                              _("Shrink/Grow:"), SCALE_WIDTH, SPIN_BUTTON_WIDTH,
                              bbp->vals.grow,
                              -20.0, 20.0,     /* lower/upper */
                              1.0, 10.0,      /* step, page */
                              0,              /* digits */
                              TRUE,           /* constrain */
                              -20.0, 20.0,       /* lowr/upper unconstrained */
                              _("Grow selection in pixels (use negative values for shrink)"), NULL);
  bbp->grow_adj = adj;
  g_object_set_data(G_OBJECT(adj), "bbp", bbp);
  g_signal_connect (adj, "value_changed",
                    G_CALLBACK (p_gdouble_adjustment_callback),
                    &bbp->vals.grow);

  row++;

  label = gtk_label_new(_("Automatic Preview:"));
  gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.0);
  gtk_table_attach(GTK_TABLE (table), label, 0, 1, row, row + 1, GTK_FILL, GTK_FILL, 0, 0);
  gtk_widget_show(label);


  /* check button */
  check_button = gtk_check_button_new_with_label (" ");
  gtk_table_attach ( GTK_TABLE (table), check_button, 1, 2, row, row+1, GTK_FILL, 0, 0, 0);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (check_button),
                                bbp->instant_preview);
  gimp_help_set_help_data(check_button, _("ON: Keep preview image up to date"), NULL);
  gtk_widget_show(check_button);
  bbp->instant_preview_toggle = check_button;
  g_object_set_data(G_OBJECT(check_button), "bbp", bbp);
  g_signal_connect (G_OBJECT (check_button), "toggled",
                    G_CALLBACK (p_toggle_update_callback),
                    &bbp->instant_preview);

  /* button */
  button = gtk_button_new_with_label(_("Preview"));
  gtk_table_attach(GTK_TABLE (table), button, 2, 3, row, row + 1, GTK_FILL, GTK_FILL, 0, 0);
  gtk_widget_show(button);
  gimp_help_set_help_data(button, _("Show preview as separate image"), NULL);
  g_signal_connect (button, "clicked",
                    G_CALLBACK (p_apply_callback),
                    bbp);

  row++;

  adj = gimp_scale_entry_new (GTK_TABLE (bbp->master_table), 0, row++,
                              _("Previewsize:"), SCALE_WIDTH, SPIN_BUTTON_WIDTH,
                              bbp->pv_size_percent,
                              5.0, 100.0,     /* lower/upper */
                              1.0, 10.0,      /* step, page */
                              1,              /* digits */
                              TRUE,           /* constrain */
                              5.0, 100.0,       /* lowr/upper unconstrained */
                              _("Size of the preview image in percent of the original"), NULL);
  g_object_set_data(G_OBJECT(adj), "bbp", bbp);
  g_signal_connect (adj, "value_changed",
                    G_CALLBACK (p_gdouble_adjustment_callback),
                    &bbp->pv_size_percent);


  hseparator = gtk_hseparator_new ();
  gtk_widget_show (hseparator);
  gtk_box_pack_start (GTK_BOX (main_vbox), hseparator, FALSE, FALSE, 0);

  /*  Show the main containers  */

  gtk_widget_show (main_vbox);

  return(dlg);
}  /* end gap_bluebox_create_dialog */


/* ---------------------------------
 * p_reset_callback
 * ---------------------------------
 */
static void
p_reset_callback(GtkWidget *w, GapBlueboxGlobalParams *bbp)
{
  gboolean radio_active;

  if(bbp)
  {
    gap_bluebox_init_default_vals(bbp);

    if(bbp->thres_r_adj)
    {
      gtk_adjustment_set_value (GTK_ADJUSTMENT (bbp->thres_r_adj), (gfloat)bbp->vals.thres_r);
    }
    if(bbp->thres_g_adj)
    {
      gtk_adjustment_set_value (GTK_ADJUSTMENT (bbp->thres_g_adj), (gfloat)bbp->vals.thres_g);
    }
    if(bbp->thres_r_adj)
    {
      gtk_adjustment_set_value (GTK_ADJUSTMENT (bbp->thres_b_adj), (gfloat)bbp->vals.thres_b);
    }

    if(bbp->thres_h_adj)
    {
      gtk_adjustment_set_value (GTK_ADJUSTMENT (bbp->thres_h_adj), (gfloat)bbp->vals.thres_h);
    }
    if(bbp->thres_s_adj)
    {
      gtk_adjustment_set_value (GTK_ADJUSTMENT (bbp->thres_s_adj), (gfloat)bbp->vals.thres_s);
    }
    if(bbp->thres_v_adj)
    {
      gtk_adjustment_set_value (GTK_ADJUSTMENT (bbp->thres_v_adj), (gfloat)bbp->vals.thres_v);
    }
    if(bbp->thres_adj)
    {
      gtk_adjustment_set_value (GTK_ADJUSTMENT (bbp->thres_adj), (gfloat)bbp->vals.thres);
    }

    gtk_adjustment_set_value (GTK_ADJUSTMENT (bbp->feather_radius_adj), (gfloat)bbp->vals.feather_radius);
    gtk_adjustment_set_value (GTK_ADJUSTMENT (bbp->grow_adj), (gfloat)bbp->vals.grow);
    gtk_adjustment_set_value (GTK_ADJUSTMENT (bbp->source_alpha_adj), (gfloat)bbp->vals.source_alpha);
    gtk_adjustment_set_value (GTK_ADJUSTMENT (bbp->target_alpha_adj), (gfloat)bbp->vals.target_alpha);
    gtk_adjustment_set_value (GTK_ADJUSTMENT (bbp->tolerance_adj), (gfloat)bbp->vals.tolerance);

    gimp_color_button_set_color ((GimpColorButton *)bbp->keycolor_button, &bbp->vals.keycolor);

    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (bbp->feather_edges_toggle)
                                  ,bbp->vals.feather_edges);
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (bbp->instant_preview_toggle)
                                  ,bbp->instant_preview);
    radio_active = (bbp->vals.thres_mode == GAP_BLUBOX_THRES_RGB);
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (bbp->thres_rgb_toggle)
                                  ,radio_active);

    radio_active = (bbp->vals.thres_mode == GAP_BLUBOX_THRES_HSV);
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (bbp->thres_hsv_toggle)
                                  ,radio_active);

    radio_active = (bbp->vals.thres_mode == GAP_BLUBOX_THRES_VAL);
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (bbp->thres_val_toggle)
                                  ,radio_active);

    radio_active = (bbp->vals.thres_mode == GAP_BLUBOX_THRES_DELTAE_CIE94);
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (bbp->thres_e94_toggle)
                                  ,radio_active);

    radio_active = (bbp->vals.thres_mode == GAP_BLUBOX_THRES_DELTAE_CIEDE200);
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (bbp->thres_e2000_toggle)
                                  ,radio_active);

    radio_active = (bbp->vals.thres_mode == GAP_BLUBOX_THRES_ALL);
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (bbp->thres_all_toggle)
                                  ,radio_active);
  }

}  /* end p_reset_callback */


/* ---------------------------------
 * p_quit_callback
 * ---------------------------------
 */
static void
p_quit_callback(GtkWidget *w, GapBlueboxGlobalParams *bbp)
{
  static gboolean main_quit_flag = TRUE;

  /* finish pending que draw ops before we close */
  gdk_flush ();

  if(bbp)
  {
     p_remove_timer(bbp);
     if(bbp->shell)
     {
       GtkWidget *l_shell;

       l_shell = bbp->shell;
       bbp->shell = NULL;
       main_quit_flag = TRUE;

       /* cleanup wiget stuff */
       bbp->thres_table = NULL;
       bbp->instant_preview = FALSE;
       bbp->instant_apply_request = FALSE;
       bbp->instant_timertag = -1;
       bbp->master_table = NULL;
       bbp->thres_r_adj = NULL;
       bbp->thres_g_adj = NULL;
       bbp->thres_b_adj = NULL;
       bbp->thres_h_adj = NULL;
       bbp->thres_s_adj = NULL;
       bbp->thres_v_adj = NULL;
       bbp->thres_adj = NULL;
       bbp->tolerance_adj = NULL;
       bbp->grow_adj = NULL;
       bbp->feather_radius_adj = NULL;
       bbp->source_alpha_adj = NULL;
       bbp->target_alpha_adj = NULL;
       bbp->keycolor_button = NULL;
       bbp->feather_edges_toggle = NULL;
       bbp->instant_preview_toggle = NULL;
       bbp->thres_rgb_toggle = NULL;
       bbp->thres_hsv_toggle = NULL;
       bbp->thres_val_toggle = NULL;
       bbp->thres_e94_toggle = NULL;
       bbp->thres_e2000_toggle = NULL;
       bbp->thres_all_toggle = NULL;

       /* p_quit_callback is the signal handler for the "destroy"
        * signal of the shell window.
        * the gtk_widget_destroy call will immediate reenter this procedure.
        * (for this reason the bbp->shell is set to NULL
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
 * p_bluebox_response
 * ---------------------------------
 */
static void
p_bluebox_response (GtkWidget *widget,
                 gint       response_id,
                 GapBlueboxGlobalParams *bbp)
{
  switch (response_id)
  {
    case GAP_BLUEBOX_RESPONSE_RESET:
      p_reset_callback(widget, bbp);
      break;

    case GTK_RESPONSE_OK:
      bbp->run_flag = TRUE;

    default:
      gtk_widget_hide (widget);
      p_quit_callback (widget, bbp);
      break;
  }
}  /* end p_bluebox_response */

/* ---------------------------------
 * p_set_waiting_cursor
 * ---------------------------------
 */
static void
p_set_waiting_cursor(GapBlueboxGlobalParams *bbp)
{
  if(bbp == NULL) return;

  gdk_window_set_cursor(GTK_WIDGET(bbp->shell)->window, bbp->cursor_wait);
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
p_set_active_cursor(GapBlueboxGlobalParams *bbp)
{
  if(bbp == NULL) return;

  gdk_window_set_cursor(GTK_WIDGET(bbp->shell)->window, bbp->cursor_acitve);
  gdk_flush();
}  /* end p_set_active_cursor */

/* ---------------------------------
 * p_apply_callback
 * ---------------------------------
 */
static void
p_apply_callback(GtkWidget *w, GapBlueboxGlobalParams *bbp)
{
  if(bbp)
  {
     if(!bbp->instant_preview)
     {
       p_set_waiting_cursor(bbp);
     }
     bbp->layer_id = bbp->drawable_id;
     if(!gap_bluebox_apply(bbp))
     {
       p_quit_callback(NULL, bbp);
     }
     bbp->layer_id = -1;
     p_set_active_cursor(bbp);
  }
}  /* end p_apply_callback */

/* ---------------------------------
 * p_gdouble_adjustment_callback
 * ---------------------------------
 */
static void
p_gdouble_adjustment_callback(GtkObject *obj, gpointer val)
{
  GapBlueboxGlobalParams *bbp;

  gimp_double_adjustment_update(GTK_ADJUSTMENT(obj), val);

  bbp = g_object_get_data( G_OBJECT(obj), "bbp" );
  if(bbp)
  {
    p_set_instant_apply_request(bbp);
  }

}  /* end p_gdouble_adjustment_callback */

/* ---------------------------------
 * p_color_update_callback
 * ---------------------------------
 */
static void
p_color_update_callback(GtkWidget *widget, gpointer val)
{
  GapBlueboxGlobalParams *bbp;

  gimp_color_button_get_color((GimpColorButton *)widget, (GimpRGB *)val);

  bbp = g_object_get_data( G_OBJECT(widget), "bbp" );
  if(bbp)
  {
    p_set_instant_apply_request(bbp);
  }

}  /* end p_color_update_callback */


/* ---------------------------------
 * p_toggle_update_callback
 * ---------------------------------
 */
static void
p_toggle_update_callback(GtkWidget *widget, gint *val)
{
  GapBlueboxGlobalParams *bbp;

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

  bbp = g_object_get_data( G_OBJECT(widget), "bbp" );

  if(bbp)
  {
    GtkWidget *spinbutton;
    GtkWidget *scale;

    spinbutton = GTK_WIDGET(g_object_get_data (G_OBJECT (bbp->feather_radius_adj), "spinbutton"));
    scale = GTK_WIDGET(g_object_get_data (G_OBJECT (bbp->feather_radius_adj), "scale"));
    gtk_widget_set_sensitive(spinbutton, bbp->vals.feather_edges);
    gtk_widget_set_sensitive(scale, bbp->vals.feather_edges);

    p_set_instant_apply_request(bbp);
    if(bbp->instant_preview)
    {
      p_install_timer(bbp);
    }
  }

}  /* end p_toggle_update_callback */


/* ---------------------------------
 * p_radio_thres_RGB_callback
 * ---------------------------------
 */
static void
p_radio_thres_RGB_callback(GtkWidget *widget, GapBlueboxGlobalParams *bbp)
{
  if((bbp) && (GTK_TOGGLE_BUTTON (widget)->active))
  {
    bbp->vals.thres_mode = GAP_BLUBOX_THRES_RGB;
    p_thres_table_create_or_replace(bbp);
  }
}  /* end p_radio_thres_RGB_callback */

/* ---------------------------------
 * p_radio_thres_HSV_callback
 * ---------------------------------
 */
static void
p_radio_thres_HSV_callback(GtkWidget *widget, GapBlueboxGlobalParams *bbp)
{
  if((bbp) && (GTK_TOGGLE_BUTTON (widget)->active))
  {
    bbp->vals.thres_mode = GAP_BLUBOX_THRES_HSV;
    p_thres_table_create_or_replace(bbp);
  }
}  /* end p_radio_thres_HSV_callback */

/* ---------------------------------
 * p_radio_thres_VAL_callback
 * ---------------------------------
 */
static void
p_radio_thres_VAL_callback(GtkWidget *widget, GapBlueboxGlobalParams *bbp)
{
  if((bbp) && (GTK_TOGGLE_BUTTON (widget)->active))
  {
    bbp->vals.thres_mode = GAP_BLUBOX_THRES_VAL;
    p_thres_table_create_or_replace(bbp);
  }
}  /* end p_radio_thres_VAL_callback */

/* ---------------------------------
 * p_radio_thres_E94_callback
 * ---------------------------------
 */
static void
p_radio_thres_E94_callback(GtkWidget *widget, GapBlueboxGlobalParams *bbp)
{
  if((bbp) && (GTK_TOGGLE_BUTTON (widget)->active))
  {
    bbp->vals.thres_mode = GAP_BLUBOX_THRES_DELTAE_CIE94;
    p_thres_table_create_or_replace(bbp);
  }
}  /* end p_radio_thres_E94_callback */

/* ---------------------------------
 * p_radio_thres_E2000_callback
 * ---------------------------------
 */
static void
p_radio_thres_E2000_callback(GtkWidget *widget, GapBlueboxGlobalParams *bbp)
{
  if((bbp) && (GTK_TOGGLE_BUTTON (widget)->active))
  {
    bbp->vals.thres_mode = GAP_BLUBOX_THRES_DELTAE_CIEDE200;
    p_thres_table_create_or_replace(bbp);
  }
}  /* end p_radio_thres_E2000_callback */

/* ---------------------------------
 * p_radio_thres_ALL_callback
 * ---------------------------------
 */
static void
p_radio_thres_ALL_callback(GtkWidget *widget, GapBlueboxGlobalParams *bbp)
{
  if((bbp) && (GTK_TOGGLE_BUTTON (widget)->active))
  {
    bbp->vals.thres_mode = GAP_BLUBOX_THRES_ALL;
    p_thres_table_create_or_replace(bbp);
  }
}  /* end p_radio_thres_ALL_callback */

/* ---------------------------------
 * p_radio_create_thres_mode
 * ---------------------------------
 */
static void
p_radio_create_thres_mode(GtkWidget *table, int row, int col, GapBlueboxGlobalParams *bbp)
{
  GtkWidget *label;
  GtkWidget *radio_table;
  GtkWidget *radio_button;
  GSList    *radio_group = NULL;
  gint      l_idx;
  gboolean  l_radio_pressed;

  label = gtk_label_new(_("Threshold Mode:"));
  gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.0);
  gtk_table_attach( GTK_TABLE (table), label, col, col+1, row, row+1, GTK_FILL, GTK_FILL, 0, 10);
  gtk_widget_show(label);

  /* radio_table */
  radio_table = gtk_table_new (1, 4, FALSE);

  l_idx = 0;
  /* radio button thres_mode RGB */
  radio_button = gtk_radio_button_new_with_label ( radio_group, _("RGB") );
  radio_group = gtk_radio_button_get_group ( GTK_RADIO_BUTTON (radio_button) );
  gtk_table_attach ( GTK_TABLE (radio_table), radio_button, l_idx, l_idx+1
                   , 0, 1, GTK_FILL | GTK_EXPAND, 0, 0, 0);
  bbp->thres_rgb_toggle = radio_button;

  l_radio_pressed = (bbp->vals.thres_mode == GAP_BLUBOX_THRES_RGB);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (radio_button),
                                   l_radio_pressed);
  gimp_help_set_help_data(radio_button, _("Apply thresholds in the RGB colormodel"), NULL);

  gtk_widget_show (radio_button);
  g_signal_connect ( G_OBJECT (radio_button), "toggled",
                     G_CALLBACK (p_radio_thres_RGB_callback),
                     bbp);


  l_idx = 1;

  /* radio button thres_mode HSV */
  radio_button = gtk_radio_button_new_with_label ( radio_group, _("HSV") );
  radio_group = gtk_radio_button_get_group ( GTK_RADIO_BUTTON (radio_button) );
  gtk_table_attach ( GTK_TABLE (radio_table), radio_button, l_idx, l_idx+1, 0, 1
                   , GTK_FILL | GTK_EXPAND, 0, 0, 0);
  bbp->thres_hsv_toggle = radio_button;

  l_radio_pressed = (bbp->vals.thres_mode == GAP_BLUBOX_THRES_HSV);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (radio_button),
                                   l_radio_pressed);
  gimp_help_set_help_data(radio_button, _("Apply thresholds in the HSV colormodel"), NULL);

  gtk_widget_show (radio_button);
  g_signal_connect ( G_OBJECT (radio_button), "toggled",
                     G_CALLBACK (p_radio_thres_HSV_callback),
                     bbp);



  l_idx = 2;

  /* radio button thres_mode VAL */
  radio_button = gtk_radio_button_new_with_label ( radio_group, _("VALUE") );
  radio_group = gtk_radio_button_get_group ( GTK_RADIO_BUTTON (radio_button) );
  gtk_table_attach ( GTK_TABLE (radio_table), radio_button, l_idx, l_idx+1, 0, 1
                   , GTK_FILL | GTK_EXPAND, 0, 0, 0);
  bbp->thres_val_toggle = radio_button;

  l_radio_pressed = (bbp->vals.thres_mode == GAP_BLUBOX_THRES_VAL);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (radio_button),
                                   l_radio_pressed);
  gimp_help_set_help_data(radio_button, _("Use single threshold value"), NULL);

  gtk_widget_show (radio_button);
  g_signal_connect ( G_OBJECT (radio_button), "toggled",
                     G_CALLBACK (p_radio_thres_VAL_callback),
                     bbp);

  l_idx = 3;

  /* radio button thres_mode DeltaE CIE94 */
  radio_button = gtk_radio_button_new_with_label ( radio_group, _("E94") );
  radio_group = gtk_radio_button_get_group ( GTK_RADIO_BUTTON (radio_button) );
  gtk_table_attach ( GTK_TABLE (radio_table), radio_button, l_idx, l_idx+1, 0, 1
                   , GTK_FILL | GTK_EXPAND, 0, 0, 0);
  bbp->thres_e94_toggle = radio_button;

  l_radio_pressed = (bbp->vals.thres_mode == GAP_BLUBOX_THRES_DELTAE_CIE94);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (radio_button),
                                   l_radio_pressed);
  gimp_help_set_help_data(radio_button, _("Use single threshold value DeltaE CIE94"), NULL);

  gtk_widget_show (radio_button);
  g_signal_connect ( G_OBJECT (radio_button), "toggled",
                     G_CALLBACK (p_radio_thres_E94_callback),
                     bbp);
  l_idx = 4;

  /* radio button thres_mode DeltaE CIEDE2000 */
  radio_button = gtk_radio_button_new_with_label ( radio_group, _("E2000") );
  radio_group = gtk_radio_button_get_group ( GTK_RADIO_BUTTON (radio_button) );
  gtk_table_attach ( GTK_TABLE (radio_table), radio_button, l_idx, l_idx+1, 0, 1
                   , GTK_FILL | GTK_EXPAND, 0, 0, 0);
  bbp->thres_e2000_toggle = radio_button;

  l_radio_pressed = (bbp->vals.thres_mode == GAP_BLUBOX_THRES_DELTAE_CIEDE200);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (radio_button),
                                   l_radio_pressed);
  gimp_help_set_help_data(radio_button, _("Use single threshold value DeltaE CIEDE2000"), NULL);

  gtk_widget_show (radio_button);
  g_signal_connect ( G_OBJECT (radio_button), "toggled",
                     G_CALLBACK (p_radio_thres_E2000_callback),
                     bbp);

  l_idx = 5;

  /* radio button thres_mode ALL */
  radio_button = gtk_radio_button_new_with_label ( radio_group, _("ALL") );
  radio_group = gtk_radio_button_get_group ( GTK_RADIO_BUTTON (radio_button) );
  gtk_table_attach ( GTK_TABLE (radio_table), radio_button, l_idx, l_idx+1, 0, 1
                   , GTK_FILL | GTK_EXPAND, 0, 0, 0);
  bbp->thres_all_toggle = radio_button;

  l_radio_pressed = (bbp->vals.thres_mode == GAP_BLUBOX_THRES_VAL);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (radio_button),
                                   l_radio_pressed);
  gimp_help_set_help_data(radio_button, _("Use both HSV and RGB threshold values"), NULL);

  gtk_widget_show (radio_button);
  g_signal_connect ( G_OBJECT (radio_button), "toggled",
                     G_CALLBACK (p_radio_thres_ALL_callback),
                     bbp);


  /* attach radio_table */
  gtk_table_attach ( GTK_TABLE (table), radio_table, col+1, col+3, row, row+1, GTK_FILL | GTK_EXPAND, 0, 0, 0);
  gtk_widget_show (radio_table);

}  /* end p_radio_create_thres_mode */



/* ---------------------------------
 * p_thres_table_RGB_create
 * ---------------------------------
 */
static void
p_thres_table_RGB_create(GapBlueboxGlobalParams *bbp, gint row)
{
  GtkObject *adj;

  adj = gimp_scale_entry_new (GTK_TABLE (bbp->thres_table), 0, row,
                              _("Threshold R:"), SCALE_WIDTH, SPIN_BUTTON_WIDTH,
                              bbp->vals.thres_r,
                              0.0, 1.0,       /* lower/upper */
                              0.01, 0.1,      /* step, page */
                              3,              /* digits */
                              TRUE,           /* constrain */
                              0.0, 1.0,       /* lowr/upper unconstrained */
                              _("Threshold for red channel"), NULL);
  bbp->thres_r_adj = adj;
  g_object_set_data(G_OBJECT(adj), "bbp", bbp);
  g_signal_connect (adj, "value_changed",
                    G_CALLBACK (p_gdouble_adjustment_callback),
                    &bbp->vals.thres_r);

  row++;

  adj = gimp_scale_entry_new (GTK_TABLE (bbp->thres_table), 0, row,
                              _("Threshold G:"), SCALE_WIDTH, SPIN_BUTTON_WIDTH,
                              bbp->vals.thres_g,
                              0.0, 1.0,       /* lower/upper */
                              0.01, 0.1,      /* step, page */
                              3,              /* digits */
                              TRUE,           /* constrain */
                              0.0, 1.0,       /* lowr/upper unconstrained */
                              _("Threshold for green channel"), NULL);
  bbp->thres_g_adj = adj;
  g_object_set_data(G_OBJECT(adj), "bbp", bbp);
  g_signal_connect (adj, "value_changed",
                    G_CALLBACK (p_gdouble_adjustment_callback),
                    &bbp->vals.thres_g);


  row++;

  adj = gimp_scale_entry_new (GTK_TABLE (bbp->thres_table), 0, row,
                              _("Threshold B:"), SCALE_WIDTH, SPIN_BUTTON_WIDTH,
                              bbp->vals.thres_b,
                              0.0, 1.0,       /* lower/upper */
                              0.01, 0.1,      /* step, page */
                              3,              /* digits */
                              TRUE,           /* constrain */
                              0.0, 1.0,       /* lowr/upper unconstrained */
                              _("Threshold for blue channel"), NULL);
  bbp->thres_b_adj = adj;
  g_object_set_data(G_OBJECT(adj), "bbp", bbp);
  g_signal_connect (adj, "value_changed",
                    G_CALLBACK (p_gdouble_adjustment_callback),
                    &bbp->vals.thres_b);

}  /* end p_thres_table_RGB_create */


/* ---------------------------------
 * p_thres_table_HSV_create
 * ---------------------------------
 */
static void
p_thres_table_HSV_create(GapBlueboxGlobalParams *bbp, gint row)
{
  GtkObject *adj;

  adj = gimp_scale_entry_new (GTK_TABLE (bbp->thres_table), 0, row,
                              _("Threshold H:"), SCALE_WIDTH, SPIN_BUTTON_WIDTH,
                              bbp->vals.thres_h,
                              0.0, 1.0,       /* lower/upper */
                              0.01, 0.1,      /* step, page */
                              3,              /* digits */
                              TRUE,           /* constrain */
                              0.0, 1.0,       /* lowr/upper unconstrained */
                              _("Threshold for hue"), NULL);
  bbp->thres_h_adj = adj;
  g_object_set_data(G_OBJECT(adj), "bbp", bbp);
  g_signal_connect (adj, "value_changed",
                    G_CALLBACK (p_gdouble_adjustment_callback),
                    &bbp->vals.thres_h);

  row++;

  adj = gimp_scale_entry_new (GTK_TABLE (bbp->thres_table), 0, row,
                              _("Threshold S:"), SCALE_WIDTH, SPIN_BUTTON_WIDTH,
                              bbp->vals.thres_s,
                              0.0, 1.0,       /* lower/upper */
                              0.01, 0.1,      /* step, page */
                              3,              /* digits */
                              TRUE,           /* constrain */
                              0.0, 1.0,       /* lowr/upper unconstrained */
                              _("Threshold for saturation"), NULL);
  bbp->thres_s_adj = adj;
  g_object_set_data(G_OBJECT(adj), "bbp", bbp);
  g_signal_connect (adj, "value_changed",
                    G_CALLBACK (p_gdouble_adjustment_callback),
                    &bbp->vals.thres_s);


  row++;

  adj = gimp_scale_entry_new (GTK_TABLE (bbp->thres_table), 0, row,
                              _("Threshold V:"), SCALE_WIDTH, SPIN_BUTTON_WIDTH,
                              bbp->vals.thres_v,
                              0.0, 1.0,       /* lower/upper */
                              0.01, 0.1,      /* step, page */
                              3,              /* digits */
                              TRUE,           /* constrain */
                              0.0, 1.0,       /* lowr/upper unconstrained */
                              _("Threshold for value"), NULL);
  bbp->thres_v_adj = adj;
  g_object_set_data(G_OBJECT(adj), "bbp", bbp);
  g_signal_connect (adj, "value_changed",
                    G_CALLBACK (p_gdouble_adjustment_callback),
                    &bbp->vals.thres_v);

}  /* end p_thres_table_HSV_create */


/* ---------------------------------
 * p_thres_table_VAL_create
 * ---------------------------------
 */
static void
p_thres_table_VAL_create(GapBlueboxGlobalParams *bbp)
{
  GtkObject *adj;
  gint row;

  row = 0;
  adj = gimp_scale_entry_new (GTK_TABLE (bbp->thres_table), 0, row++,
                              _("Threshold:"), SCALE_WIDTH, SPIN_BUTTON_WIDTH,
                              bbp->vals.thres,
                              0.0, 1.0,       /* lower/upper */
                              0.01, 0.1,      /* step, page */
                              3,              /* digits */
                              TRUE,           /* constrain */
                              0.0, 1.0,       /* lowr/upper unconstrained */
                              _("Common color threshold"), NULL);
  bbp->thres_adj = adj;
  g_object_set_data(G_OBJECT(adj), "bbp", bbp);
  g_signal_connect (adj, "value_changed",
                    G_CALLBACK (p_gdouble_adjustment_callback),
                    &bbp->vals.thres);


}  /* end p_thres_table_VAL_create */


/* ---------------------------------
 * p_thres_table_create_or_replace
 * ---------------------------------
 */
static GtkWidget *
p_thres_table_create_or_replace(GapBlueboxGlobalParams *bbp)
{
 if(bbp->thres_table)
 {
   gtk_widget_destroy(bbp->thres_table);
 }

 bbp->thres_r_adj = NULL;
 bbp->thres_g_adj = NULL;
 bbp->thres_b_adj = NULL;
 bbp->thres_h_adj = NULL;
 bbp->thres_s_adj = NULL;
 bbp->thres_v_adj = NULL;
 bbp->thres_adj = NULL;


 bbp->thres_table = gtk_table_new(3,3,FALSE);

 switch(bbp->vals.thres_mode)
 {
   case GAP_BLUBOX_THRES_RGB:
     p_thres_table_RGB_create(bbp, 0);
     break;
   case GAP_BLUBOX_THRES_HSV:
     p_thres_table_HSV_create(bbp, 0);
     break;
   case GAP_BLUBOX_THRES_VAL:
   case GAP_BLUBOX_THRES_DELTAE_CIE94:
   case GAP_BLUBOX_THRES_DELTAE_CIEDE200:
     /* modes operating with a single threshold value */
     p_thres_table_VAL_create(bbp);
     break;
   case GAP_BLUBOX_THRES_ALL:
     p_thres_table_HSV_create(bbp, 0);
     p_thres_table_RGB_create(bbp, 3);
     break;
 }

 gtk_table_attach (GTK_TABLE (bbp->master_table), bbp->thres_table, 0, 3
                    , bbp->thres_table_attach_row, bbp->thres_table_attach_row +1,
                    GTK_FILL, GTK_SHRINK, 0, 0) ;

 gtk_widget_show(bbp->thres_table);
 return bbp->thres_table;

}  /* end p_thres_table_create_or_replace */


/* ---------------------------------
 * p_set_instant_apply_request
 * ---------------------------------
 */
static void
p_set_instant_apply_request(GapBlueboxGlobalParams *bbp)
{
  if(bbp)
  {
    bbp->instant_apply_request = TRUE; /* request is handled by timer */
  }
}  /* end p_set_instant_apply_request */

/* --------------------------
 * install / remove timer
 * --------------------------
 */
static void
p_install_timer(GapBlueboxGlobalParams *bbp)
{
  if(bbp->instant_timertag < 0)
  {
    bbp->instant_timertag = (gint32) g_timeout_add(INSTANT_TIMERINTERVAL_MILLISEC,
                                      (GtkFunction)p_instant_timer_callback
                                      , bbp);
  }
}  /* end p_install_timer */

static void
p_remove_timer(GapBlueboxGlobalParams *bbp)
{
  if(bbp->instant_timertag >= 0)
  {
    g_source_remove(bbp->instant_timertag);
    bbp->instant_timertag = -1;
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
  GapBlueboxGlobalParams *bbp;

  bbp = user_data;
  if(bbp == NULL)
  {
    return;
  }

  p_remove_timer(bbp);

  if(bbp->instant_apply_request)
  {
    if(gap_debug) printf("FIRE p_instant_timer_callback >>>> REQUEST is TRUE\n");
    p_apply_callback (NULL, bbp);  /* the request is cleared in this procedure */
  }

  /* restart timer for next cycle */
  if(bbp->instant_preview)
  {
     p_install_timer(bbp);
  }
}  /* end p_instant_timer_callback */


/* ---------------------------------
 * p_check_RGB_thres
 * ---------------------------------
 */
static inline gdouble
p_check_RGB_thres (GimpRGB       *src,
              GapBlueboxGlobalParams *bbp)
{
  gdouble l_diff_r;
  gdouble l_diff_g;
  gdouble l_diff_b;
  gdouble l_sumdiff;

  if(bbp->vals.tolerance > 0)
  {
    l_diff_r = fabs(src->r - bbp->vals.keycolor.r);
    if (l_diff_r > bbp->vals.thres_r) { return 1.0; }

    l_diff_g = fabs(src->g - bbp->vals.keycolor.g);
    if (l_diff_g > bbp->vals.thres_g) { return 1.0; }

    l_diff_b = fabs(src->b - bbp->vals.keycolor.b);
    if (l_diff_b > bbp->vals.thres_b) { return 1.0; }

    l_sumdiff = GAP_SUMM_RGB(l_diff_r, l_diff_g, l_diff_b);

    if(bbp->sum_threshold_rgb > 0.001)
    {
      gdouble aa;

      aa = (l_sumdiff / bbp->sum_threshold_rgb);
      if (aa >= bbp->inv_tolerance)
      {
        return ((aa - bbp->inv_tolerance) * (1 / bbp->inv_tolerance));
      }
    }
  }
  else
  {
    if (fabs(src->r - bbp->vals.keycolor.r) > bbp->vals.thres_r)   { return 1.0; }
    if (fabs(src->g - bbp->vals.keycolor.g) > bbp->vals.thres_g)   { return 1.0; }
    if (fabs(src->b - bbp->vals.keycolor.b) > bbp->vals.thres_b)   { return 1.0; }
  }

  return 0.0;
}  /* end p_check_RGB_thres */

/* ---------------------------------
 * p_check_HSV_thres
 * ---------------------------------
 */
static inline gdouble
p_check_HSV_thres (GimpRGB       *src,
              GapBlueboxGlobalParams *bbp)
{
  gdouble l_diff_h;
  gdouble l_diff_s;
  gdouble l_diff_v;
  gdouble l_sumdiff;

  GimpHSV src_hsv;

  gimp_rgb_to_hsv(src, &src_hsv);

  if(bbp->vals.tolerance > 0)
  {
    l_diff_h = fabs(src_hsv.h - bbp->key_hsv.h);

    /* normalize hue difference.
     * hue values represents an angle
     * where value 0.5 equals 180 degree
     * and value 1.0 stands for 360 degree that is
     * equal to 0.0
     * Hue is maximal different at 180 degree.
     *
     * after normalizing, the difference
     * hDiff value 1.0 represents angle difference of 180 degree
     */
    if(l_diff_h > 0.5)
    {
      l_diff_h = (1.0 - l_diff_h) * 2.0;
    }
    else
    {
      l_diff_h = l_diff_h * 2.0;

    }
    if (l_diff_h > bbp->vals.thres_h) { return 1.0; }

    l_diff_s = fabs(src_hsv.s - bbp->key_hsv.s);
    if (l_diff_s > bbp->vals.thres_s) { return 1.0; }

    l_diff_v = fabs(src_hsv.v - bbp->key_hsv.v);
    if (l_diff_v > bbp->vals.thres_v) { return 1.0; }

    l_sumdiff = GAP_SUMM_HSV(l_diff_h, l_diff_s, l_diff_v);

    if(bbp->sum_threshold_hsv > 0.001)
    {
      gdouble aa;

      aa = (l_sumdiff / bbp->sum_threshold_hsv);
      if (aa >= bbp->inv_tolerance)
      {
        return ((aa - bbp->inv_tolerance) * (1 / bbp->inv_tolerance));
      }
    }
  }
  else
  {
    if (fabs(src_hsv.h - bbp->key_hsv.h) > bbp->vals.thres_h) { return 1.0; }
    if (fabs(src_hsv.s - bbp->key_hsv.s) > bbp->vals.thres_s) { return 1.0; }
    if (fabs(src_hsv.v - bbp->key_hsv.v) > bbp->vals.thres_v) { return 1.0; }
  }
  return 0.0;
}  /* end p_check_HSV_thres */


/* ---------------------------------
 * p_check_VAL_thres
 * ---------------------------------
 */
static inline gdouble
p_check_VAL_thres (GimpRGB       *src,
              GapBlueboxGlobalParams *bbp)
{
  gdouble l_maxdiff;

  l_maxdiff = fabs (src->r - bbp->vals.keycolor.r);
  l_maxdiff = MAX(l_maxdiff, fabs (src->g - bbp->vals.keycolor.g));
  l_maxdiff = MAX(l_maxdiff, fabs (src->b - bbp->vals.keycolor.b));

  if(l_maxdiff > bbp->vals.thres)                       { return 1.0; }

  if(bbp->vals.tolerance > 0)
  {
    gdouble aa;

    aa = (l_maxdiff / bbp->vals.thres);
    if (aa >= bbp->inv_tolerance)
    {
      return ((aa - bbp->inv_tolerance) * (1 / bbp->inv_tolerance));
    }
  }

  return 0.0;
}  /* end p_check_VAL_thres */

/* ---------------------------------
 * p_check_DeltaE_thres
 * ---------------------------------
 */
static inline gdouble
p_check_DeltaE_thres (guchar       *srcPixelPtr,
              GapBlueboxGlobalParams *bbp)
{
  gdouble l_maxdiff;
  guchar  keycolor[4];

  /* convert keycolor to uchar 
   * TODO: for  better performance GapBlueboxGlobalParams shall provide a L*A*B colorspace representation (of type GapColorLAB) 
   *   of the keycolor, and there should be additional colordiff methods capable to compare uchar RGB 255 versus  GapColorLAB
   */
  gimp_rgba_get_uchar (&bbp->vals.keycolor, &keycolor[0], &keycolor[1], &keycolor[2], &keycolor[3]);

  if (bbp->vals.thres_mode == GAP_BLUBOX_THRES_DELTAE_CIE94)
  {
    l_maxdiff = gap_colordiff_LabDeltaE94(srcPixelPtr
                   , &keycolor[0]
                   , FALSE /* gboolean debugPrint */
                   );
  }
  else
  {
    l_maxdiff = gap_colordiff_LabDeltaE2000(srcPixelPtr
                   , &keycolor[0]
                   , FALSE /* gboolean debugPrint */
                   );
  }

  if(l_maxdiff > bbp->vals.thres)                       { return 1.0; }

  if(bbp->vals.tolerance > 0)
  {
    gdouble aa;

    aa = (l_maxdiff / bbp->vals.thres);
    if (aa >= bbp->inv_tolerance)
    {
      return ((aa - bbp->inv_tolerance) * (1 / bbp->inv_tolerance));
    }
  }

  return 0.0;
}  /* end p_check_DeltaE_thres */

/* ---------------------------------
 * p_pixel_render_alpha
 * ---------------------------------
 */
static inline void
p_pixel_render_alpha (GimpRGB       *src,  guchar *srcPixelPtr,
              GapBlueboxGlobalParams *bbp)
{
 if(src->a >= bbp->vals.source_alpha)
 {
   gdouble col_diff = 0;

   switch(bbp->vals.thres_mode)
   {
     case GAP_BLUBOX_THRES_RGB:
       col_diff = p_check_RGB_thres(src, bbp);
       break;
     case GAP_BLUBOX_THRES_HSV:
       col_diff = p_check_HSV_thres(src, bbp);
       break;
     case GAP_BLUBOX_THRES_VAL:
       col_diff = p_check_VAL_thres(src, bbp);
       break;
     case GAP_BLUBOX_THRES_DELTAE_CIE94:
       col_diff = p_check_DeltaE_thres(srcPixelPtr, bbp);
       break;
     case GAP_BLUBOX_THRES_DELTAE_CIEDE200:
       col_diff = p_check_DeltaE_thres(srcPixelPtr, bbp);
       break;
     case GAP_BLUBOX_THRES_ALL:
       col_diff = MAX(p_check_HSV_thres(src, bbp), p_check_RGB_thres(src, bbp));
       break;
   }
   if(col_diff < 1.0)
   {
     src->a = bbp->vals.target_alpha + ((src->a - bbp->vals.target_alpha) * col_diff);
   }

 }
}  /* end p_pixel_render_alpha */

/* ---------------------------------
 * p_toalpha_func
 * ---------------------------------
 */
static void
p_toalpha_func (const guchar *src,
              guchar       *dest,
              gint          bpp,
              gpointer      data)
{
  GimpRGB color;
  GapBlueboxGlobalParams *bbp;

  bbp = (GapBlueboxGlobalParams *)data;
  
  gimp_rgba_set_uchar (&color, src[0], src[1], src[2], src[3]);
  p_pixel_render_alpha (&color, src, bbp);
  gimp_rgba_get_uchar (&color, &dest[0], &dest[1], &dest[2], &dest[3]);
}  /* end p_toalpha_func */

/* ---------------------------------
 * p_bluebox_alpha
 * ---------------------------------
 */
static void
p_bluebox_alpha (GapBlueboxGlobalParams *bbp)
{
  GimpDrawable *drawable;

  bbp->sum_threshold_rgb = GAP_SUMM_RGB(bbp->vals.thres_r, bbp->vals.thres_g, bbp->vals.thres_b);
  bbp->sum_threshold_hsv = GAP_SUMM_HSV(bbp->vals.thres_h, bbp->vals.thres_s, bbp->vals.thres_v);
  bbp->inv_tolerance = CLAMP(1.0 - bbp->vals.tolerance, 0.0001, 1.0);

  drawable = gimp_drawable_get (bbp->pv_layer_id);
  gimp_rgn_iterate2 (drawable, GIMP_RUN_NONINTERACTIVE, p_toalpha_func, bbp);

  gimp_drawable_detach (drawable);
}  /* end p_bluebox_alpha */



/* ---------------------------------
 * gap_bluebox_apply
 * ---------------------------------
 * this procedure always does the main processing on a preview image
 * (even in non-interactive runmode)
 * the preview image has full layersize (not imagesize)
 * when called with run_flag == TRUE
 * for preview mode (run_flag == FALSE) the preview image size
 * may be reduced py pv_size_percent.
 * an already existing preview image is reused only in preview mode.
 *
 * Apply the BlueBox Effect to bbp->layer_id
 * when called with run_flag == TRUE
 *
 * Show the preview image (by adding a display)
 * when called in GIMP_RUN_INTERACTIVE with run_flag = FALSE
 *
 * return FALSE on Error
 *        TRUE  .. OK
 */
gboolean
gap_bluebox_apply(GapBlueboxGlobalParams *bbp)
{
  GimpDrawable *src_drawable;
  gboolean replace_pv_image;
  gint l_width;
  gint l_height;

  if(bbp == NULL)                              { return FALSE; }

  if(gap_debug)
  {
    printf("BluBox Params:\n");
    printf("bbp->image_id :%d\n", (int)bbp->image_id);
    printf("bbp->layer_id :%d\n", (int)bbp->layer_id);
    printf("bbp->pv_size_percent :%.3f\n", (float)bbp->pv_size_percent);
    printf("bbp->vals.thres_mode  :%d\n", (int)bbp->vals.thres_mode);
    printf("bbp->vals.thres_r     :%.3f\n", (float)bbp->vals.thres_r);
    printf("bbp->vals.thres_g     :%.3f\n", (float)bbp->vals.thres_g);
    printf("bbp->vals.thres_b     :%.3f\n", (float)bbp->vals.thres_b);
    printf("bbp->vals.thres_h     :%.3f\n", (float)bbp->vals.thres_h);
    printf("bbp->vals.thres_s     :%.3f\n", (float)bbp->vals.thres_s);
    printf("bbp->vals.thres_v     :%.3f\n", (float)bbp->vals.thres_v);
    printf("bbp->vals.thres       :%.3f\n", (float)bbp->vals.thres);
    printf("bbp->vals.tolerance   :%.3f\n", (float)bbp->vals.tolerance);
    printf("bbp->vals.grow        :%.3f\n", (float)bbp->vals.grow);
    printf("bbp->vals.feather_edges   :%d\n", (int)bbp->vals.feather_edges);
    printf("bbp->vals.feather_radius  :%.3f\n", (float)bbp->vals.feather_radius);
    printf("bbp->vals.source_alpha    :%.3f\n", (float)bbp->vals.source_alpha);
    printf("bbp->vals.target_alpha    :%.3f\n", (float)bbp->vals.target_alpha);
  }

  if(!gap_image_is_alive(bbp->image_id))
  {
    if(bbp->run_mode == GIMP_RUN_INTERACTIVE)
    {
      g_message(_("Error: Image '%s' not found"), bbp->image_filename);
    }
    return FALSE;
  }
  if(!gimp_drawable_is_layer(bbp->layer_id))
  {
    if(bbp->run_mode == GIMP_RUN_INTERACTIVE)
    {
      g_message(_("Error: Bluebox effect operates only on layers"));
    }
    return FALSE;
  }

  if(!gimp_drawable_is_rgb(bbp->layer_id))
  {
    if(bbp->run_mode == GIMP_RUN_INTERACTIVE)
    {
      g_message(_("Error: Bluebox effect operates only on RGB layers"));
    }
    return FALSE;
  }

  if(!gimp_drawable_has_alpha(bbp->layer_id))
  {
    gimp_layer_add_alpha(bbp->layer_id);
  }

  src_drawable =  gimp_drawable_get (bbp->layer_id);


  /* set size (reduced size is only for preview processing) */
  l_width = src_drawable->width;
  l_height = src_drawable->height;
  if(!bbp->run_flag)
  {
    if(bbp->pv_size_percent < 100.0)
    {
      l_width = ((gdouble)src_drawable->width * bbp->pv_size_percent) / 100.0;
      l_height = ((gdouble)src_drawable->height * bbp->pv_size_percent) / 100.0;
    }
  }

  replace_pv_image = FALSE;
  if(gap_image_is_alive(bbp->pv_image_id))
  {
    if(gimp_image_base_type(bbp->image_id) != gimp_image_base_type(bbp->pv_image_id))
    {
      replace_pv_image = TRUE;
    }
    else
    {
      if((l_width     != gimp_image_width(bbp->pv_image_id))
      || (l_height    != gimp_image_height(bbp->pv_image_id)))
      {
        /* check if we can reuse the previw image
         * (that was built in a previus call)
         */
        if((bbp->run_flag)
        || (bbp->pv_master_layer_id < 0))
        {
          replace_pv_image = TRUE;
        }
        else
        {
          if(l_width > gimp_image_width(bbp->pv_image_id))
          {
            /* must force recreate the master copy for quality reasons */
            if(bbp->pv_master_layer_id >= 0)
            {
              gimp_image_remove_layer(bbp->pv_image_id, bbp->pv_master_layer_id);
              bbp->pv_master_layer_id = -1;
            }
          }
          gimp_image_scale(bbp->pv_image_id, l_width, l_height);

        }
      }
    }
    if(replace_pv_image)
    {
      gimp_image_delete(bbp->pv_image_id);
      bbp->pv_image_id = -1;
      bbp->pv_master_layer_id = -1;
      bbp->pv_display_id = -1;
    }
  }
  else
  {
    replace_pv_image = TRUE;
  }


  /* (re)create the preview image if there is none */
  if(replace_pv_image)
  {
    bbp->pv_display_id = -1;
    bbp->pv_master_layer_id = -1;
    bbp->pv_image_id = gimp_image_new(l_width
                                     ,l_height
                                     ,GIMP_RGB
                                     );
    gimp_image_set_filename(bbp->pv_image_id, _("BlueboxPreview.xcf"));
    bbp->pv_layer_id = gimp_layer_new(bbp->pv_image_id, _("Previewlayer")
                                 ,l_width
                                 ,l_height
                                 ,GIMP_RGBA_IMAGE
                                 ,100.0            /* Opacity full opaque */
                                 ,GIMP_NORMAL_MODE
                                 );
    gimp_image_insert_layer(bbp->pv_image_id, bbp->pv_layer_id, 0, 0);
    gimp_layer_set_offsets(bbp->pv_layer_id, 0, 0);

    if(!gimp_drawable_has_alpha(bbp->pv_layer_id))
    {
      gimp_layer_add_alpha(bbp->pv_layer_id);
    }
  }

  if(bbp->run_flag)
  {
    gap_layer_copy_content(bbp->pv_layer_id  /* dest */
                         , bbp->layer_id    /* src */
                         );
  }
  else
  {
    if(bbp->pv_master_layer_id < 0)
    {
      /* at 1.st call create a mastercopy of the original layer
       * and scale to preview size
       */
      bbp->pv_master_layer_id = gimp_layer_new(bbp->pv_image_id, _("Masterlayer")
                                   ,src_drawable->width
                                   ,src_drawable->height
                                   ,GIMP_RGBA_IMAGE
                                   ,100.0            /* Opacity full opaque */
                                   ,GIMP_NORMAL_MODE
                                   );
      gimp_image_insert_layer(bbp->pv_image_id, bbp->pv_master_layer_id, 0, 1);

      if(!gimp_drawable_has_alpha(bbp->pv_master_layer_id))
      {
        gimp_layer_add_alpha(bbp->pv_master_layer_id);
      }
      gap_layer_copy_content(bbp->pv_master_layer_id  /* dest */
                            , bbp->layer_id           /* src */
                            );
      gimp_layer_scale(bbp->pv_master_layer_id, l_width, l_height, 0);
      gimp_layer_set_offsets(bbp->pv_master_layer_id, 0, 0);
      gimp_item_set_visible(bbp->pv_master_layer_id, FALSE);
    }

    /* use a the scaled mastercopy when operating with reduced preview size */
    gap_layer_copy_content(bbp->pv_layer_id           /* dest */
                         , bbp->pv_master_layer_id    /* src */
                         );
  }



  if((bbp->vals.thres_mode == GAP_BLUBOX_THRES_HSV)
  || (bbp->vals.thres_mode == GAP_BLUBOX_THRES_ALL))
  {
    gimp_rgb_to_hsv(&bbp->vals.keycolor, &bbp->key_hsv);
  }
  p_bluebox_alpha (bbp);

  if(((bbp->vals.feather_edges) && (bbp->vals.feather_radius > 0.0))
  || (bbp->vals.grow != 0))
  {
    gint32 current_selection_channel_id;

    gimp_selection_layer_alpha(bbp->pv_layer_id);
    if((bbp->vals.feather_edges) && (bbp->vals.feather_radius > 0.0))
    {
      gimp_selection_feather(bbp->pv_image_id, bbp->vals.feather_radius);
    }
    if(bbp->vals.grow > 0)
    {
      gimp_selection_grow(bbp->pv_image_id, (gint)bbp->vals.grow);
    }
    if(bbp->vals.grow < 0)
    {
      gint l_shrink;

      l_shrink = (bbp->vals.grow * -1);
      gimp_selection_shrink(bbp->pv_image_id, l_shrink);
    }
    current_selection_channel_id = gimp_image_get_selection(bbp->pv_image_id);
    gap_layer_copy_picked_channel(bbp->pv_layer_id, 3 /* dst_pick is the alpha channel */
                                 ,current_selection_channel_id, 0
                                 ,FALSE  /* shadow */
                                 );
    gimp_selection_none(bbp->pv_image_id);
  }

  if(bbp->run_flag)
  {
    /* Apply to the calling drawable */
    gap_layer_copy_picked_channel(bbp->layer_id, 3 /* dst_pick is the alpha channel */
                                 ,bbp->pv_layer_id, 3
                                 ,TRUE  /* shadow */
                                 );

    gimp_drawable_detach (src_drawable);

    if(bbp->pv_image_id)
    {
      gimp_image_delete(bbp->pv_image_id);
      bbp->pv_image_id = -1;
      bbp->pv_display_id = -1;
    }
  }
  else
  {
    if((bbp->pv_display_id < 0)
    && (bbp->pv_image_id >= 0)
    && (bbp->run_mode == GIMP_RUN_INTERACTIVE))
    {
      bbp->pv_display_id = gimp_display_new(bbp->pv_image_id);
    }
  }

  if(bbp->run_mode == GIMP_RUN_INTERACTIVE)
  {
    gimp_displays_flush();
  }

  bbp->instant_apply_request = FALSE;
  return TRUE;
}  /* end gap_bluebox_apply */
