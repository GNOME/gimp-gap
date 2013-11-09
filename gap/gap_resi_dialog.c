/* gap_resi_dialog.c
 * 1998.07.01 hof (Wolfgang Hofer)
 *
 * GAP ... Gimp Animation Plugins
 *
 * This Module contains the resize and scale Dialog for video frames.
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

/* revision history
 * gimp    1.3.14a; 2003/05/25  hof: reincarnation of gap_resi_dialog, now uses GimpOffsetArea
 *                                   dialog code was inspired by gimp-core resize-dialog.c
 *                                   to give similar look and feel,
 *                                   but without unit and resolution stuff.
 *                                   (videoframes are measured in pixels)
 * gimp    1.1.13b; 1999/12/04  hof: some cosmetic gtk fixes
 *                                   changed Buttons in action area
 *                                   to same style as used in dialogs of the gimp 1.1.13 main dialogs
 * 0.96.00; 1998/07/01   hof: first release
 */

#include "config.h"

/* SYTEM (UNIX) includes */
#include <stdio.h>

/* GIMP includes */
#include "gtk/gtk.h"
#include "gap-intl.h"
#include "libgimp/gimp.h"
#include "libgimp/gimpui.h"

/* GAP includes */
#include "gap_lib.h"
#include "gap_range_ops.h"
#include "gap_resi_dialog.h"

/* common used spinbutton char width */
#define SB_WIDTH 10

#define GAP_HELP_ID_CROP    "plug-in-gap-anim-crop"
#define GAP_HELP_ID_RESIZE  "plug-in-gap-anim-resize"
#define GAP_HELP_ID_SCALE   "plug-in-gap-anim-scale"



typedef struct
{
  GapRangeOpsAsiz asiz_mode;    /* GAP specific resize mode GAP_ASIZ_SCALE, GAP_ASIZ_RESIZE, GAP_ASIZ_CROP */
  gint32 image_id;
  gint32 orig_width;
  gint32 orig_height;
  gint32 width;
  gint32 height;
  gint32 offset_x;
  gint32 offset_y;
  gdouble ratio_x;
  gdouble ratio_y;


  GtkWidget *dlg;
  gint run;
  GtkWidget *shell;
  GtkWidget *constrain;
  GtkWidget *offset_area;   /* GimpOffsetArea */
  GtkWidget *orig_width_label;
  GtkWidget *orig_height_label;

  GtkObject *width_adj;
  GtkObject *height_adj;
  GtkObject *ratio_x_adj;
  GtkObject *ratio_y_adj;
  GtkObject *offset_x_adj;
  GtkObject *offset_y_adj;

  gboolean in_call;
} GapResizePrivateType;




/* -----------------------------
 * p_resize_bound_off_x
 * p_resize_bound_off_y
 * -----------------------------
 * IN: offset value
 * RETURN the offsetvalue CLAMped to the legal boundaries.
 *
 * This procedures do also set lowr/upper limits for the
 * offset adjustment widgets
 */
static gint
p_resize_bound_off_x (GapResizePrivateType *res_private,
                    gint    off_x)
{
  if( res_private->offset_x_adj == NULL) { return 0; }

  if (res_private->orig_width <= res_private->width)
  {
    off_x = CLAMP (off_x, 0, (res_private->width - res_private->orig_width));

    GTK_ADJUSTMENT(res_private->offset_x_adj)->lower = 0;
    GTK_ADJUSTMENT(res_private->offset_x_adj)->upper = res_private->width - res_private->orig_width;
  }
  else
  {
    off_x = CLAMP (off_x, (res_private->width - res_private->orig_width), 0);

    GTK_ADJUSTMENT(res_private->offset_x_adj)->lower = res_private->width - res_private->orig_width;
    GTK_ADJUSTMENT(res_private->offset_x_adj)->upper = 0;
  }
  return off_x;
}

static gint
p_resize_bound_off_y (GapResizePrivateType *res_private,
                    gint    off_y)
{
  if( res_private->offset_y_adj == NULL) { return 0; }

  if (res_private->orig_height <= res_private->height)
  {
    off_y = CLAMP (off_y, 0, (res_private->height - res_private->orig_height));

    GTK_ADJUSTMENT(res_private->offset_y_adj)->lower = 0;
    GTK_ADJUSTMENT(res_private->offset_y_adj)->upper = res_private->height - res_private->orig_height;
  }
  else
  {
    off_y = CLAMP (off_y, (res_private->height - res_private->orig_height), 0);

    GTK_ADJUSTMENT(res_private->offset_y_adj)->lower = res_private->height - res_private->orig_height;
    GTK_ADJUSTMENT(res_private->offset_y_adj)->upper = 0;
  }

  return off_y;
}


/* -----------------------------
 * p_res_cancel_callback
 * -----------------------------
 */
static void
p_res_cancel_callback (GtkWidget *widget,
                       GapResizePrivateType *res_private)
{
  gtk_main_quit ();
}  /* end p_res_cancel_callback */

/* -----------------------------
 * p_res_ok_callback
 * -----------------------------
 */
static void
p_res_ok_callback (GtkWidget *widget,
                   GapResizePrivateType *res_private)
{
  if(res_private)
  {
    res_private->run = TRUE;
    gtk_widget_destroy (GTK_WIDGET (res_private->dlg));
  }
}  /* end p_res_ok_callback */

/* -----------------------------
 * p_res_help_callback
 * -----------------------------
 */
static void
p_res_help_callback (GtkWidget *widget,
                     GapResizePrivateType *res_private)
{
  if(res_private)
  {
    switch(res_private->asiz_mode)
    {
      case GAP_ASIZ_SCALE:
        gimp_standard_help_func(GAP_HELP_ID_SCALE, res_private->dlg);
        break;
      case GAP_ASIZ_RESIZE:
        gimp_standard_help_func(GAP_HELP_ID_RESIZE, res_private->dlg);
        break;
      case GAP_ASIZ_CROP:
        gimp_standard_help_func(GAP_HELP_ID_CROP, res_private->dlg);
        break;
    }
  }
}  /* end p_res_help_callback */

/* -----------------------------
 * p_set_size_spinbuttons
 * -----------------------------
 */
static void
p_set_size_spinbuttons(GapResizePrivateType *res_private)
{
  gtk_adjustment_set_value (GTK_ADJUSTMENT (res_private->width_adj)
                           , (gdouble)res_private->width);
  gtk_adjustment_set_value (GTK_ADJUSTMENT (res_private->height_adj)
                           , (gdouble)res_private->height);
  gtk_adjustment_set_value (GTK_ADJUSTMENT (res_private->ratio_x_adj)
                           , (gdouble)res_private->ratio_x);
  gtk_adjustment_set_value (GTK_ADJUSTMENT (res_private->ratio_y_adj)
                           , (gdouble)res_private->ratio_y);
}  /* end p_set_size_spinbuttons */

/* -----------------------------
 * p_set_offset_spinbuttons
 * -----------------------------
 */
static void
p_set_offset_spinbuttons(GapResizePrivateType *res_private)
{
  if((res_private->offset_x_adj == NULL)
  || (res_private->offset_y_adj == NULL))
  {
    return;
  }

  gtk_adjustment_set_value (GTK_ADJUSTMENT (res_private->offset_x_adj)
                           , res_private->offset_x);
  gtk_adjustment_set_value (GTK_ADJUSTMENT (res_private->offset_y_adj)
                           , res_private->offset_y);
}  /* end p_set_offset_spinbuttons */


/* -----------------------------
 * p_res_reset_callback
 * -----------------------------
 */
static void
p_res_reset_callback (GtkWidget *widget,
                    gpointer   data)
{
  GapResizePrivateType *res_private;

  res_private = (GapResizePrivateType *)data;
  if(res_private == NULL) {return;}

  res_private->width = res_private->orig_width;
  res_private->height = res_private->orig_height;
  p_set_size_spinbuttons(res_private);

  if((res_private->asiz_mode != GAP_ASIZ_SCALE)
  && (res_private->offset_area))
  {
    res_private->offset_x = 0;
    res_private->offset_y = 0;
    p_set_offset_spinbuttons(res_private);
    gimp_offset_area_set_size (GIMP_OFFSET_AREA (res_private->offset_area),
                               res_private->width, res_private->height);
    gimp_offset_area_set_offsets (GIMP_OFFSET_AREA (res_private->offset_area),
                                  res_private->offset_x, res_private->offset_y);
  }
}  /* end p_res_reset_callback */

/* -----------------------------
 * p_size_update
 * -----------------------------
 */
static void
p_size_update (GapResizePrivateType *res_private,
             double  width,
             double  height,
             double  ratio_x,
             double  ratio_y)
{
  res_private->width  = (gint) (width  + 0.5);
  res_private->height = (gint) (height + 0.5);

  res_private->ratio_x = ratio_x;
  res_private->ratio_y = ratio_y;

  if(res_private->in_call) { return; }

  res_private->in_call = TRUE;


  if (res_private->offset_area)
  {
    gimp_offset_area_set_size (GIMP_OFFSET_AREA (res_private->offset_area),
                               res_private->width, res_private->height);
    p_resize_bound_off_x(res_private, 0);
    p_resize_bound_off_y(res_private, 0);
    p_set_offset_spinbuttons(res_private);
  }

  p_set_size_spinbuttons(res_private);

  res_private->in_call = FALSE;

}  /* end p_size_update */


/* -----------------------------
 * p_size_callback
 * -----------------------------
 */
static void
p_size_callback(GtkWidget *w, gpointer   data)
{
  GapResizePrivateType *res_private;
  gdouble        width;
  gdouble        height;
  gdouble        ratio_x;
  gdouble        ratio_y;

  res_private  = (GapResizePrivateType *) data;
  if(res_private == NULL) {return;}

  width  = GTK_ADJUSTMENT (res_private->width_adj)->value;
  height = GTK_ADJUSTMENT (res_private->height_adj)->value;

  ratio_x = width  / (gdouble) res_private->orig_width;
  ratio_y = height / (gdouble) res_private->orig_height;

  if (gimp_chain_button_get_active (GIMP_CHAIN_BUTTON (res_private->constrain)))
  {
    if (ratio_x != res_private->ratio_x)
    {
          ratio_y = ratio_x;
          height = (gdouble) res_private->orig_height * ratio_y;
          height = CLAMP (height, GIMP_MIN_IMAGE_SIZE, GIMP_MAX_IMAGE_SIZE);
    }
    else
    {
          ratio_x = ratio_y;
          width = (gdouble) res_private->orig_width * ratio_x;
          width = CLAMP (width, GIMP_MIN_IMAGE_SIZE, GIMP_MAX_IMAGE_SIZE);
    }
  }

  p_size_update (res_private, width, height, ratio_x, ratio_y);

}  /* end p_size_callback */

/* -----------------------------
 * p_ratio_callback
 * -----------------------------
 */
static void
p_ratio_callback(GtkWidget *w, gpointer   data)
{
  GapResizePrivateType *res_private;
  gdouble        width;
  gdouble        height;
  gdouble        ratio_x;
  gdouble        ratio_y;

  res_private  = (GapResizePrivateType *) data;
  if(res_private == NULL) {return;}
  if(res_private->in_call) {return;}

  width  = GTK_ADJUSTMENT (res_private->width_adj)->value;
  height = GTK_ADJUSTMENT (res_private->height_adj)->value;

  ratio_x = GTK_ADJUSTMENT (res_private->ratio_x_adj)->value;
  ratio_y = GTK_ADJUSTMENT (res_private->ratio_y_adj)->value;

  if (gimp_chain_button_get_active (GIMP_CHAIN_BUTTON (res_private->constrain)))
  {
    if (ratio_x != res_private->ratio_x)
    {
          ratio_y = ratio_x;
    }
    else
    {
          ratio_x = ratio_y;
    }
  }

  width  = CLAMP (res_private->orig_width * ratio_x,
                  GIMP_MIN_IMAGE_SIZE, GIMP_MAX_IMAGE_SIZE);
  height = CLAMP (res_private->orig_height * ratio_y,
                  GIMP_MIN_IMAGE_SIZE, GIMP_MAX_IMAGE_SIZE);

  p_size_update (res_private, width, height, ratio_x, ratio_y);

}  /* end p_ratio_callback */


/* -----------------------------
 * p_offset_update
 * -----------------------------
 */
static void
p_offset_update(GtkWidget *w, gpointer   data)
{
  GapResizePrivateType *res_private;
  gdouble ofs_x;
  gdouble ofs_y;

  res_private  = (GapResizePrivateType *) data;
  if(res_private == NULL) {return;}

  ofs_x = GTK_ADJUSTMENT(res_private->offset_x_adj)->value;
  ofs_y = GTK_ADJUSTMENT(res_private->offset_y_adj)->value;

  res_private->offset_x = p_resize_bound_off_x (res_private, RINT(ofs_x));

  res_private->offset_y = p_resize_bound_off_y (res_private, RINT(ofs_y));


  gimp_offset_area_set_offsets (GIMP_OFFSET_AREA (res_private->offset_area)
                                 , res_private->offset_x
                                 , res_private->offset_y);
}  /* end p_offset_update */

/* -----------------------------
 * p_offset_x_center_clicked
 * -----------------------------
 */
static void
p_offset_x_center_clicked(GtkWidget *w, gpointer   data)
{
  GapResizePrivateType *res_private;
  gint           off_x;

  res_private  = (GapResizePrivateType *) data;
  if(res_private == NULL) {return;}

  off_x = p_resize_bound_off_x (res_private,
                              (res_private->width - res_private->orig_width)  / 2);

  res_private->offset_x = off_x;

  p_set_offset_spinbuttons(res_private);
  gimp_offset_area_set_offsets (GIMP_OFFSET_AREA (res_private->offset_area),
                                  res_private->offset_x, res_private->offset_y);
}  /* end p_offset_x_center_clicked */

/* -----------------------------
 * p_offset_y_center_clicked
 * -----------------------------
 */
static void
p_offset_y_center_clicked(GtkWidget *w, gpointer   data)
{
  GapResizePrivateType *res_private;
  gint           off_y;

  res_private  = (GapResizePrivateType *) data;
  if(res_private == NULL) {return;}

  off_y = p_resize_bound_off_y (res_private,
                              (res_private->height - res_private->orig_height) / 2);

  res_private->offset_y = off_y;

  p_set_offset_spinbuttons(res_private);
  gimp_offset_area_set_offsets (GIMP_OFFSET_AREA (res_private->offset_area),
                                  res_private->offset_x, res_private->offset_y);
}  /* end p_offset_y_center_clicked */


/* -----------------------------
 * p_offset_area_offsets_changed
 * -----------------------------
 */
/* XXX ?? wonder why glib calling code should know about the offest parameters
 * XXX  standard  G_CALLBACK procedures have 2 Parameters Widget and gpointer only ??
 * XXX could it be some object oriented magic ??
 */
static void
p_offset_area_offsets_changed (GtkWidget *offset_area,
                             gint       offset_x,
                             gint       offset_y,
                             gpointer   data)
{
  GapResizePrivateType *res_private;
  /* GimpOffsetArea *oa_ptr; */
  /* gint offset_x;          */
  /* gint offset_y;          */

  res_private  = (GapResizePrivateType *) data;
  if(res_private == NULL) {return;}

  /*  oa_ptr = (GimpOffsetArea *)res_private->offset_area; */
  /*  size_x  = oa_ptr->width;       */
  /*  size_y  = oa_ptr->height;      */
  /*  offset_x  = oa_ptr->offset_x;  */
  /*  offset_y  = oa_ptr->offset_y;  */

  if((res_private->offset_x != offset_x)
  || (res_private->offset_y != offset_y))
  {
    res_private->offset_x = offset_x;
    res_private->offset_y = offset_y;

    p_set_offset_spinbuttons(res_private);
  }
}  /* end p_offset_area_offsets_changed */

/* --------------------------
 * p_orig_labels_update
 * --------------------------
 */
static void
p_orig_labels_update (GtkWidget *widget,
                    gpointer   data)
{
  GapResizePrivateType *res_private;
  gchar          buf[32];


  res_private  = (GapResizePrivateType *) data;

  if(res_private)
  {
    g_snprintf (buf, sizeof (buf), "%d", res_private->orig_width);
    gtk_label_set_text (GTK_LABEL (res_private->orig_width_label), buf);
    g_snprintf (buf, sizeof (buf), "%d", res_private->orig_height);
    gtk_label_set_text (GTK_LABEL (res_private->orig_height_label), buf);
  }
}  /* end p_orig_labels_update */


/* --------------------------
 * gap_resi_dialog
 * --------------------------
 * Resize dialog used for resize and cropping frames
 * based on the GimpOffsetArea widget
 */

gint
gap_resi_dialog (gint32 image_id, GapRangeOpsAsiz asiz_mode, char *title_text,
               long *size_x, long *size_y,
               long *offs_x, long *offs_y)
{
  gint   l_run;

  GapResizePrivateType *res_private;
  GtkWidget *hbbox;
  GtkWidget *button;
  GtkWidget *vbox;


  GtkWidget     *main_vbox;
  GtkWidget     *table;
  GtkWidget     *table2;
  GtkWidget     *table3;
  GtkWidget     *label;
  GtkWidget     *frame;
  GtkWidget     *spinbutton;
  GtkWidget     *abox;

  gdouble l_max_image_width;
  gdouble l_max_image_height;
  gdouble l_max_ratio_x;
  gdouble l_max_ratio_y;


  abox  = NULL;
  frame = NULL;

  /*  Initialize the GapResizePrivateType structure  */
  res_private = (GapResizePrivateType *) g_malloc (sizeof (GapResizePrivateType));
  res_private->image_id = image_id;
  res_private->run = FALSE;
  res_private->in_call = FALSE;
  res_private->asiz_mode = asiz_mode;

  /* get info about the image (size is common to all frames) */
  res_private->orig_width = gimp_image_width(image_id);
  res_private->orig_height = gimp_image_height(image_id);
  res_private->width = res_private->orig_width;
  res_private->height = res_private->orig_height;
  res_private->offset_x = 0;
  res_private->offset_y = 0;
  res_private->ratio_x = 1.0;
  res_private->ratio_y = 1.0;


  l_max_image_width = GIMP_MAX_IMAGE_SIZE;
  l_max_image_height = GIMP_MAX_IMAGE_SIZE;
  l_max_ratio_x = (gdouble) GIMP_MAX_IMAGE_SIZE / (double) res_private->width;
  l_max_ratio_y = (gdouble) GIMP_MAX_IMAGE_SIZE / (double) res_private->height;

  /* for CROP mode only: set sizelimit to original width/height */
  if(res_private->asiz_mode == GAP_ASIZ_CROP)
  {
    l_max_image_width = res_private->orig_width;
    l_max_image_height = res_private->orig_height;
    l_max_ratio_x = 1.0;
    l_max_ratio_y = 1.0;
  }

  res_private->offset_area = NULL;

  gimp_ui_init ("gap_res_dialog", FALSE);


  /*  the dialog  */
  res_private->shell = gtk_dialog_new ();
  res_private->dlg = res_private->shell;
  gtk_window_set_title (GTK_WINDOW (res_private->shell), title_text);
  gtk_window_set_position (GTK_WINDOW (res_private->shell), GTK_WIN_POS_MOUSE);
  g_signal_connect (G_OBJECT (res_private->shell), "destroy",
                    G_CALLBACK (p_res_cancel_callback),
                    NULL);

  switch(res_private->asiz_mode)
  {
    case GAP_ASIZ_SCALE:
      frame        = gimp_frame_new (_("Scale Frames"));
      break;
    case GAP_ASIZ_RESIZE:
      frame        = gimp_frame_new (_("Resize Frames"));
      break;
    case GAP_ASIZ_CROP:
      frame        = gimp_frame_new (_("Crop Frames"));
      break;
  }

  /*  the main vbox  */
  main_vbox = gtk_vbox_new (FALSE, 4);
  gtk_container_set_border_width (GTK_CONTAINER (main_vbox), 4);
  /* gtk_container_add (GTK_CONTAINER (GTK_DIALOG (res_private->shell)->vbox), main_vbox);
   */
  gtk_box_pack_start (GTK_BOX (GTK_DIALOG (res_private->shell)->vbox), main_vbox, TRUE, TRUE, 0);

  /* button hbox */
  gtk_container_set_border_width (GTK_CONTAINER (GTK_DIALOG (res_private->shell)->action_area), 2);
  gtk_box_set_homogeneous (GTK_BOX (GTK_DIALOG (res_private->shell)->action_area), FALSE);

  hbbox = gtk_hbutton_box_new ();
  gtk_box_set_spacing (GTK_BOX (hbbox), 4);
  gtk_box_pack_end (GTK_BOX (GTK_DIALOG (res_private->shell)->action_area), hbbox, FALSE, FALSE, 0);
  gtk_widget_show (hbbox);

  /*  the pixel dimensions frame  */
  gtk_box_pack_start (GTK_BOX (main_vbox), frame, FALSE, FALSE, 0);
  gtk_widget_show (frame);

  vbox = gtk_vbox_new (FALSE, 2);
  gtk_container_set_border_width (GTK_CONTAINER (vbox), 2);
  gtk_container_add (GTK_CONTAINER (frame), vbox);

  table = gtk_table_new (6, 2, FALSE);
  gtk_container_set_border_width (GTK_CONTAINER (table), 2);
  gtk_table_set_col_spacings (GTK_TABLE (table), 2);
  gtk_table_set_col_spacing (GTK_TABLE (table), 0, 4);
  gtk_table_set_row_spacings (GTK_TABLE (table), 2);
  gtk_table_set_row_spacing (GTK_TABLE (table), 0, 4);
  gtk_table_set_row_spacing (GTK_TABLE (table), 1, 4);
  gtk_table_set_row_spacing (GTK_TABLE (table), 3, 4);
  gtk_box_pack_start (GTK_BOX (vbox), table, FALSE, FALSE, 0);


  /*  the original width & height labels  */
  label = gtk_label_new (_("Current width:"));
  gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
  gtk_table_attach (GTK_TABLE (table), label, 0, 1, 0, 1,
                    GTK_SHRINK | GTK_FILL, GTK_SHRINK | GTK_FILL, 0, 0);
  gtk_widget_show (label);

  label = gtk_label_new (_("Current height:"));
  gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
  gtk_table_attach (GTK_TABLE (table), label, 0, 1, 1, 2,
                    GTK_SHRINK | GTK_FILL, GTK_SHRINK | GTK_FILL, 0, 0);
  gtk_widget_show (label);


  /*  the original width & height Values labels  */
  res_private->orig_width_label = gtk_label_new ("");
  gtk_misc_set_alignment (GTK_MISC (res_private->orig_width_label), 0.0, 0.5);
  gtk_table_attach (GTK_TABLE (table),  res_private->orig_width_label, 1, 2, 0, 1,
                    GTK_SHRINK | GTK_FILL, GTK_SHRINK | GTK_FILL, 0, 0);
  gtk_widget_show (res_private->orig_width_label);

  res_private->orig_height_label = gtk_label_new ("");
  gtk_misc_set_alignment (GTK_MISC (res_private->orig_height_label), 0.0, 0.5);
  gtk_table_attach (GTK_TABLE (table), res_private->orig_height_label, 1, 2, 1, 2,
                    GTK_SHRINK | GTK_FILL, GTK_SHRINK | GTK_FILL, 0, 0);
  gtk_widget_show (res_private->orig_height_label);


  /*  the new size labels  */
  label = gtk_label_new (_("New width:"));
  gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
  gtk_table_attach (GTK_TABLE (table), label, 0, 1, 2, 3,
                    GTK_SHRINK | GTK_FILL, GTK_SHRINK | GTK_FILL, 0, 0);
  gtk_widget_show (label);

  label = gtk_label_new (_("New height:"));
  gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
  gtk_table_attach (GTK_TABLE (table), label, 0, 1, 3, 4,
                    GTK_SHRINK | GTK_FILL, GTK_SHRINK | GTK_FILL, 0, 0);
  gtk_widget_show (label);


  /* the spinbutton entry new_width */
  spinbutton = gimp_spin_button_new (&res_private->width_adj,
                                     res_private->orig_width,
                                     GIMP_MIN_IMAGE_SIZE,
                                     l_max_image_width,
                                     1, 10, 0,
                                     1, 2);
  gtk_entry_set_width_chars (GTK_ENTRY (spinbutton), SB_WIDTH);

  gtk_widget_show (spinbutton);

  gtk_table_attach (GTK_TABLE (table), spinbutton, 1, 2, 2, 3,
                    GTK_SHRINK | GTK_FILL, GTK_SHRINK | GTK_FILL, 0, 0);
  g_signal_connect (res_private->width_adj, "value_changed",
                    G_CALLBACK (p_size_callback),
                    res_private);


  /* the spinbutton entry new_height */
  spinbutton = gimp_spin_button_new (&res_private->height_adj,
                                     res_private->orig_height,
                                     GIMP_MIN_IMAGE_SIZE,
                                     l_max_image_height,
                                     1, 10, 0,
                                     1, 2);
  gtk_entry_set_width_chars (GTK_ENTRY (spinbutton), SB_WIDTH);

  gtk_widget_show (spinbutton);

  gtk_table_attach (GTK_TABLE (table), spinbutton, 1, 2, 3, 4,
                    GTK_SHRINK | GTK_FILL, GTK_SHRINK | GTK_FILL, 0, 0);
  g_signal_connect (res_private->height_adj, "value_changed",
                    G_CALLBACK (p_size_callback),
                    res_private);


  /*  initialize the original width & height labels  */
  p_orig_labels_update(NULL, res_private);


  /*  the scale ratio labels  */
  label = gtk_label_new (_("X ratio:"));
  gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
  gtk_table_attach (GTK_TABLE (table), label, 0, 1, 4, 5,
                    GTK_SHRINK | GTK_FILL, GTK_SHRINK | GTK_FILL, 0, 0);
  gtk_widget_show (label);

  label = gtk_label_new (_("Y ratio:"));
  gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
  gtk_table_attach (GTK_TABLE (table), label, 0, 1, 5, 6,
                    GTK_SHRINK | GTK_FILL, GTK_SHRINK | GTK_FILL, 0, 0);
  gtk_widget_show (label);

  /*  a (2nd) table for the spinbuttons and the chainbutton  */
  abox = gtk_alignment_new (0.0, 0.5, 0.0, 1.0);
  table2 = gtk_table_new (2, 2, FALSE);
  gtk_table_set_col_spacing (GTK_TABLE (table2), 0, 2);
  gtk_table_set_row_spacing (GTK_TABLE (table2), 0, 2);
  gtk_container_add (GTK_CONTAINER (abox), table2);
  gtk_table_attach (GTK_TABLE (table), abox, 1, 2, 4, 6,
                    GTK_SHRINK | GTK_FILL, GTK_SHRINK | GTK_FILL, 0, 0);
  gtk_widget_show (abox);

  /* the spinbutton entry X-scale ratio */
  spinbutton =
    gimp_spin_button_new (&res_private->ratio_x_adj,
                          res_private->ratio_x,
                          (double) GIMP_MIN_IMAGE_SIZE / (double) res_private->width,
                          (double) l_max_ratio_x,
                          0.01, 0.1, 0,
                          0.01, 4);
  gtk_entry_set_width_chars (GTK_ENTRY (spinbutton), SB_WIDTH);
  gtk_table_attach_defaults (GTK_TABLE (table2), spinbutton, 0, 1, 0, 1);
  gtk_widget_show (spinbutton);

  g_signal_connect (res_private->ratio_x_adj, "value_changed",
                    G_CALLBACK (p_ratio_callback),
                    res_private);

  /* the spinbutton entry Y-scale ratio */
  spinbutton =
    gimp_spin_button_new (&res_private->ratio_y_adj,
                          res_private->ratio_y,
                          (double) GIMP_MIN_IMAGE_SIZE / (double) res_private->height,
                          (double) l_max_ratio_y,
                          0.01, 0.1, 0,
                          0.01, 4);
  gtk_entry_set_width_chars (GTK_ENTRY (spinbutton), SB_WIDTH);
  gtk_table_attach_defaults (GTK_TABLE (table2), spinbutton, 0, 1, 1, 2);
  gtk_widget_show (spinbutton);

  g_signal_connect (res_private->ratio_y_adj, "value_changed",
                    G_CALLBACK (p_ratio_callback),
                    res_private);


  /*  the constrain ratio chainbutton  */
  res_private->constrain = gimp_chain_button_new (GIMP_CHAIN_RIGHT);
  gimp_chain_button_set_active (GIMP_CHAIN_BUTTON (res_private->constrain), TRUE);
  gtk_table_attach_defaults (GTK_TABLE (table2), res_private->constrain, 1, 2, 0, 2);
  gtk_widget_show (res_private->constrain);

  gimp_help_set_help_data (GIMP_CHAIN_BUTTON (res_private->constrain)->button,
                           _("Constrain aspect ratio"), NULL);

  /* the state of the contrain ratio chainbutton is checked in other callbacks (where needed)
   * there is no need for the chainbutton to have its own callback procedure
   */


  gtk_widget_show (table2);
  gtk_widget_show (table);
  gtk_widget_show (vbox);


  /* code for GAP_ASIZ_RESIZE GAP_ASIZ_CROP using offsets, GAP_ASIZ_SCALE does not */
  if(res_private->asiz_mode != GAP_ASIZ_SCALE)
  {
    /*  the offset frame  */
    frame = gimp_frame_new (_("Offset"));
    gtk_box_pack_start (GTK_BOX (main_vbox), frame, FALSE, FALSE, 0);
    gtk_widget_show (frame);

    vbox = gtk_vbox_new (FALSE, 4);
    gtk_container_set_border_width (GTK_CONTAINER (vbox), 2);
    gtk_container_add (GTK_CONTAINER (frame), vbox);

    abox = gtk_alignment_new (0.5, 0.5, 0.0, 0.0);
    gtk_box_pack_start (GTK_BOX (vbox), abox, FALSE, FALSE, 0);

    /*  a (3rd) table for the offset spinbuttons  */
    table3 = gtk_table_new (2, 3, FALSE);
    gtk_table_set_col_spacing (GTK_TABLE (table3), 0, 2);
    gtk_table_set_row_spacing (GTK_TABLE (table3), 0, 2);

    /* gtk_container_add (GTK_CONTAINER (abox), table3); */

    /* the x/y offest labels */
    label = gtk_label_new (_("X:"));
    gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
    gtk_table_attach (GTK_TABLE (table3), label, 0, 1, 0, 1,
                    GTK_SHRINK | GTK_FILL, GTK_SHRINK | GTK_FILL, 0, 0);
    gtk_widget_show (label);

    label = gtk_label_new (_("Y:"));
    gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
    gtk_table_attach (GTK_TABLE (table3), label, 0, 1, 1, 2,
                    GTK_SHRINK | GTK_FILL, GTK_SHRINK | GTK_FILL, 0, 0);
    gtk_widget_show (label);

    /* the spinbutton entry offset_x */
    spinbutton = gimp_spin_button_new (&res_private->offset_x_adj,
                                         0,
                                         -GIMP_MAX_IMAGE_SIZE,
                                         GIMP_MAX_IMAGE_SIZE,
                                         1, 10, 0,
                                         1, 2);
    gtk_entry_set_width_chars (GTK_ENTRY (spinbutton), SB_WIDTH);
    gtk_table_attach (GTK_TABLE (table3), spinbutton, 1, 2, 0, 1,
                    GTK_SHRINK | GTK_FILL, GTK_SHRINK | GTK_FILL, 0, 0);
    gtk_widget_show (spinbutton);


    g_signal_connect (res_private->offset_x_adj, "value_changed",
                     G_CALLBACK (p_offset_update),
                     res_private);

    /* the spinbutton entry offset_y */
    spinbutton = gimp_spin_button_new (&res_private->offset_y_adj,
                                         0,
                                         -GIMP_MAX_IMAGE_SIZE,
                                         GIMP_MAX_IMAGE_SIZE,
                                         1, 10, 0,
                                         1, 2);
    gtk_entry_set_width_chars (GTK_ENTRY (spinbutton), SB_WIDTH);
    gtk_table_attach (GTK_TABLE (table3), spinbutton, 1, 2, 1, 2,
                    GTK_SHRINK | GTK_FILL, GTK_SHRINK | GTK_FILL, 0, 0);
    gtk_widget_show (spinbutton);

    g_signal_connect (res_private->offset_y_adj, "value_changed",
                     G_CALLBACK (p_offset_update),
                     res_private);


    /* the center offsetX button */
    button = gtk_button_new_with_label (_("Center Horizontal"));
    gtk_widget_show (button);
    gtk_table_attach (GTK_TABLE (table3), button, 2, 3, 0, 1,
                    GTK_SHRINK | GTK_FILL, GTK_SHRINK | GTK_FILL, 0, 0);

    g_signal_connect (button, "clicked",
                      G_CALLBACK (p_offset_x_center_clicked),
                      res_private);


    /* the center offsetY button */
    button = gtk_button_new_with_label (_("Center Vertical"));
    gtk_widget_show (button);
    gtk_table_attach (GTK_TABLE (table3), button, 2, 3, 1, 2,
                    GTK_SHRINK | GTK_FILL, GTK_SHRINK | GTK_FILL, 0, 0);

    g_signal_connect (button, "clicked",
                      G_CALLBACK (p_offset_y_center_clicked),
                      res_private);

    gtk_container_add (GTK_CONTAINER (abox), table3);
    gtk_widget_show (table3);
    gtk_widget_show (abox);


    /*  frame to hold GimpOffsetArea  */
    abox = gtk_alignment_new (0.5, 0.5, 0.0, 0.0);
    gtk_box_pack_start (GTK_BOX (vbox), abox, FALSE, FALSE, 0);

    frame = gimp_frame_new (NULL);
    gtk_container_add (GTK_CONTAINER (abox), frame);

    /* the GimpOffsetArea widget */
    res_private->offset_area = gimp_offset_area_new (res_private->orig_width
                                                   , res_private->orig_height);
    gtk_container_add (GTK_CONTAINER (frame), res_private->offset_area);

    gtk_widget_show (res_private->offset_area);
    g_signal_connect (res_private->offset_area, "offsets_changed",
                        G_CALLBACK (p_offset_area_offsets_changed),
                        res_private);

    gtk_widget_show (frame);
    gtk_widget_show (abox);
    gtk_widget_show (vbox);
  }


  /*  Action area  */

  if (gimp_show_help_button ())
    {
      button = gtk_button_new_from_stock ( GTK_STOCK_HELP);
      gtk_box_pack_end (GTK_BOX (hbbox), button, TRUE, TRUE, 0);
      gtk_widget_show (button);
      g_signal_connect (GTK_OBJECT (button), "clicked",
                        G_CALLBACK  (p_res_help_callback),
                        res_private);
    }

  button = gtk_button_new_from_stock ( GIMP_STOCK_RESET);
  gtk_box_pack_start (GTK_BOX (hbbox), button, TRUE, TRUE, 0);
  gtk_widget_show (button);
  g_signal_connect (GTK_OBJECT (button), "clicked",
                    G_CALLBACK  (p_res_reset_callback),
                    res_private);

  button = gtk_button_new_from_stock ( GTK_STOCK_CANCEL);
  GTK_WIDGET_SET_FLAGS (button, GTK_CAN_DEFAULT);
  gtk_box_pack_start (GTK_BOX (hbbox), button, TRUE, TRUE, 0);
  gtk_widget_show (button);
  g_signal_connect (G_OBJECT (button), "clicked",
                    G_CALLBACK  (p_res_cancel_callback),
                    NULL);

  button = gtk_button_new_from_stock ( GTK_STOCK_OK);
  GTK_WIDGET_SET_FLAGS (button, GTK_CAN_DEFAULT);
  gtk_box_pack_start (GTK_BOX (hbbox), button, TRUE, TRUE, 0);
  gtk_widget_grab_default (button);
  gtk_widget_show (button);
  g_signal_connect (GTK_OBJECT (button), "clicked",
                    G_CALLBACK  (p_res_ok_callback),
                    res_private);


  gtk_widget_show (main_vbox);
  gtk_widget_show (res_private->shell);

  gtk_main ();
  gdk_flush ();



  *size_x  = res_private->width;
  *size_y  = res_private->height;
  *offs_x  = res_private->offset_x;
  *offs_y  = res_private->offset_y;

  if(res_private->asiz_mode == GAP_ASIZ_CROP)
  {
     /* the widgets deliver negative offsets when new size is smaller
      * than original (as needed for gimp_image_resize calls)
      * but gimp_image_crop needs positive offets
      * therefore the sign is switched just for CROP operations
      */
     *offs_x  = - res_private->offset_x;
     *offs_y  = - res_private->offset_y;
  }

  l_run = res_private->run;
  g_free(res_private);
  return (l_run);

}  /* end gap_resi_dialog */
