/* gap_fg_matting_dialog.c
 * 2011.10.05 hof (Wolfgang Hofer)
 *
 * GAP ... Gimp Animation Plugins
 *
 * This Module contains:
 * - stuff for the GUI dialog
 *   of the foreground selection via alpha matting plug-in.
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
#include "config.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <gtk/gtk.h>
#include <libgimp/gimp.h>
#include <libgimp/gimpui.h>

#include "gap_fg_matting_main.h"
#include "gap_fg_matting_dialog.h"

#include "gap-intl.h"


#define GAP_FOREGROUND_EXTRACT_RESPONSE_RESET 1
#define GAP_FG_USE_LAYER_MASK -44

extern int gap_debug;
#define GAP_DEBUG_DECLARED 1

typedef struct _FgExtractDialogGuiStuff FgExtractDialogGuiStuff;

struct _FgExtractDialogGuiStuff
{
  gint            run;
  gint32          drawable_id;
  gint            countTriMapCanidates;

  GtkWidget       *shell;

  GtkObject       *colordiff_threshold_spinbutton_adj;
  GtkWidget       *colordiff_threshold_spinbutton;

  GtkWidget       *triMapCombo;
  GtkWidget       *triMapLabel;
  GtkWidget       *create_resultCheckbutton;
  GtkWidget       *lock_colorCheckbutton;
  GtkWidget       *create_layermaskCheckbutton;


  GapFgExtractValues *vals;
};




/* --------------------------------------
 * p_update_widget_sensitivity
 * --------------------------------------
 */
static void
p_update_widget_sensitivity (FgExtractDialogGuiStuff *guiStuffPtr)
{
  gtk_widget_set_sensitive(guiStuffPtr->triMapCombo ,                  TRUE);

}  /* end p_update_widget_sensitivity */


/* --------------------------------------
 * p_init_widget_values
 * --------------------------------------
 * update GUI widgets to reflect the current values.
 */
static void
p_init_widget_values(FgExtractDialogGuiStuff *guiStuffPtr)
{
  gint   comboInitalValue;

  comboInitalValue = -1; 

  if(guiStuffPtr == NULL)
  {
    return;
  }
  if(guiStuffPtr->vals == NULL)
  {
    return;
  }

  /* init spnbuttons */
  gtk_adjustment_set_value(GTK_ADJUSTMENT(guiStuffPtr->colordiff_threshold_spinbutton_adj)
                         , (gfloat)guiStuffPtr->vals->colordiff_threshold);


  /* init checkbuttons */
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (guiStuffPtr->create_resultCheckbutton)
                               , guiStuffPtr->vals->create_result);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (guiStuffPtr->lock_colorCheckbutton)
                               , guiStuffPtr->vals->lock_color);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (guiStuffPtr->create_layermaskCheckbutton)
                               , guiStuffPtr->vals->create_layermask);

  if (guiStuffPtr->vals->tri_map_drawable_id == GAP_FG_USE_LAYER_MASK)
  {
      comboInitalValue = GAP_FG_USE_LAYER_MASK;
  }
  else if(gimp_item_is_valid(guiStuffPtr->vals->tri_map_drawable_id))
  {
      comboInitalValue = guiStuffPtr->vals->tri_map_drawable_id;
  }
 
  if (comboInitalValue != -1)
  {
    gimp_int_combo_box_set_active (GIMP_INT_COMBO_BOX (guiStuffPtr->triMapCombo), comboInitalValue);
  }


}  /* end p_init_widget_values */


/* --------------------------------------
 * on_gboolean_button_update
 * --------------------------------------
 */
static void
on_gboolean_button_update (GtkWidget *widget,
                           gpointer   data)
{
  FgExtractDialogGuiStuff *guiStuffPtr;
  gint *toggle_val = (gint *) data;

  if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget)))
  {
    *toggle_val = TRUE;
  }
  else
  {
    *toggle_val = FALSE;
  }
  gimp_toggle_button_sensitive_update (GTK_TOGGLE_BUTTON (widget));
  
  guiStuffPtr = (FgExtractDialogGuiStuff *) g_object_get_data (G_OBJECT (widget), "guiStuffPtr");
  if(guiStuffPtr != NULL)
  {
    p_update_widget_sensitivity (guiStuffPtr);
  }
}


/* ---------------------------------
 * p_fg_extract_response
 * ---------------------------------
 */
static void
p_fg_extract_response (GtkWidget *widget,
                 gint       response_id,
                 FgExtractDialogGuiStuff *guiStuffPtr)
{
  GtkWidget *dialog;

  switch (response_id)
  {
    case GAP_FOREGROUND_EXTRACT_RESPONSE_RESET:
      if(guiStuffPtr)
      {
        /* rset default values */
        gap_fg_matting_init_default_vals(guiStuffPtr->vals);
        p_init_widget_values(guiStuffPtr);
        p_update_widget_sensitivity (guiStuffPtr);
      }
      break;

    case GTK_RESPONSE_OK:
      if(guiStuffPtr)
      {
        if (GTK_WIDGET_VISIBLE (guiStuffPtr->shell))
        {
          gtk_widget_hide (guiStuffPtr->shell);
        }
        guiStuffPtr->run = TRUE;
      }

    default:
      dialog = NULL;
      if(guiStuffPtr)
      {
        dialog = guiStuffPtr->shell;
        if(dialog)
        {
          guiStuffPtr->shell = NULL;
          gtk_widget_destroy (dialog);
        }
      }
      gtk_main_quit ();
      break;
  }
}  /* end p_fg_extract_response */




/* ------------------------------
 * p_tri_map_menu_callback
 * ------------------------------
 * 
 */
static void
p_tri_map_menu_callback(GtkWidget *widget, gint32 *layerId)
{
  gint value;

  gimp_int_combo_box_get_active (GIMP_INT_COMBO_BOX (widget), &value);

  if(gap_debug)
  {
    printf("p_tri_map_menu_callback: LayerAddr:%d value:%d\n"
      ,(int)layerId
      ,(int)value
      );
  }

  if(layerId != NULL)
  {
    if (value == GAP_FG_USE_LAYER_MASK)
    {
      *layerId = GAP_FG_USE_LAYER_MASK;
    }
    else
    {
      *layerId = value;
    }
  }


} /* end p_tri_map_menu_callback */
 
 
/* ------------------------------
 * p_tri_map_combo_constrain
 * ------------------------------
 * cecks available layers to be potential tri-map candiates
 * INDEXED layers and the input drawable are exluded
 */
static gint
p_tri_map_combo_constrain(gint32 image_id, gint32 drawable_id, FgExtractDialogGuiStuff *guiStuffPtr)
{
  gint32 processedImageId;

  if(gap_debug)
  {
    printf("p_tri_map_combo_constrain PROCEDURE image_id:%d drawable_id:%d guiStuffPtr:%d\n"
                          ,(int)image_id
                          ,(int)drawable_id
                          ,(int)guiStuffPtr
                          );
  }

  if(drawable_id < 0)
  {
     /* gimp 1.1 makes a first call of the constraint procedure
      * with drawable_id = -1, and skips the whole image if FALSE is returned
      */
     return(TRUE);
  }

  if(gimp_item_is_valid(drawable_id) != TRUE)
  {
     if(gap_debug)
     {
       printf("-- drawable_id:%d gimp_item_is_valid --\n", (int)drawable_id); 
     }
     return(FALSE);
  }

  processedImageId = gimp_item_get_image(guiStuffPtr->drawable_id);

  if(image_id != processedImageId)
  {
    if ((gimp_drawable_width(drawable_id) != gimp_drawable_width(guiStuffPtr->drawable_id))
    ||  (gimp_drawable_height(drawable_id) != gimp_drawable_height(guiStuffPtr->drawable_id)))
    {
      printf("-- drawable_id:%d has different size --\n", (int)drawable_id); 
      return (FALSE);
    }
  }
 
  if(drawable_id == guiStuffPtr->drawable_id)
  {
    /* disable selection of the input drawable in the tri-mask combo box
     */
    printf("-- drawable_id:%d is the input drawable --\n", (int)drawable_id); 
    return(FALSE);
  }

  if(!gimp_drawable_is_rgb(drawable_id))
  {
    if(!gimp_drawable_is_gray(drawable_id))
    {
      if(gap_debug)
      {
        printf("-- drawable_id:%d is NOT (RGB or GRAY) --\n", (int)drawable_id); 
      }
      /* disable indexed layers */
      return (FALSE);
    }
  }
  
  if(gap_debug)
  {
     printf("++ drawable_id:%d is ACCEPTABLE as tri map --\n", (int)drawable_id); 
  }
  guiStuffPtr->countTriMapCanidates++;

  return(TRUE);

} /* end p_tri_map_combo_constrain */


/* ------------------------------
 * do_dialog
 * ------------------------------
 * create and show the dialog window
 */
static void
do_dialog (FgExtractDialogGuiStuff *guiStuffPtr, GapFgExtractValues *cuvals)
{
  GtkWidget  *vbox;

  GtkWidget *dialog1;
  GtkWidget *dialog_vbox1;
  GtkWidget *frame1;
  GtkWidget *vbox1;
  GtkWidget *label;
  GtkWidget *table1;
  GtkObject *spinbutton_adj;
  GtkWidget *spinbutton;
  GtkWidget *dialog_action_area1;
  GtkWidget *checkbutton;
  GtkWidget *combo;
  gint       row;


  /* Init UI  */
  gimp_ui_init ("Forground Extract", FALSE);


  /*  The dialog1  */
  guiStuffPtr->run = FALSE;
  guiStuffPtr->vals = cuvals;


  /*  The dialog1 and main vbox  */
  dialog1 = gimp_dialog_new (_("Foreground-Extract"), "foreground_extract",
                               NULL, 0,
                               gimp_standard_help_func, PLUG_IN_HELP_ID,
                               
                               GIMP_STOCK_RESET, GAP_FOREGROUND_EXTRACT_RESPONSE_RESET,
                               GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                               GTK_STOCK_OK,     GTK_RESPONSE_OK,
                               NULL);

  guiStuffPtr->shell = dialog1;


  /*
   * g_object_set_data (G_OBJECT (dialog1), "dialog1", dialog1);
   * gtk_window_set_title (GTK_WINDOW (dialog1), _("dialog1"));
   */


  g_signal_connect (G_OBJECT (dialog1), "response",
                      G_CALLBACK (p_fg_extract_response),
                      guiStuffPtr);

  /* the vbox */
  vbox = gtk_vbox_new (FALSE, 2);
  gtk_container_set_border_width (GTK_CONTAINER (vbox), 2);
  gtk_box_pack_start (GTK_BOX (GTK_DIALOG (dialog1)->vbox), vbox,
                      TRUE, TRUE, 0);
  gtk_widget_show (vbox);

  dialog_vbox1 = GTK_DIALOG (dialog1)->vbox;
  g_object_set_data (G_OBJECT (dialog1), "dialog_vbox1", dialog_vbox1);
  gtk_widget_show (dialog_vbox1);



  /* the frame */
  frame1 = gimp_frame_new (_("Options"));

  gtk_widget_show (frame1);
  gtk_box_pack_start (GTK_BOX (dialog_vbox1), frame1, TRUE, TRUE, 0);
  gtk_container_set_border_width (GTK_CONTAINER (frame1), 2);

  vbox1 = gtk_vbox_new (FALSE, 0);
  gtk_container_add (GTK_CONTAINER (frame1), vbox1);
  gtk_widget_show (vbox1);


  /* table1 for ...  */
  table1 = gtk_table_new (4, 2, FALSE);
  gtk_widget_show (table1);
  gtk_box_pack_start (GTK_BOX (vbox1), table1, TRUE, TRUE, 0);
  gtk_container_set_border_width (GTK_CONTAINER (table1), 4);


  row = 0;

  /* the tri-map drawable selection combo box */
  label = gtk_label_new (_("Tri-Map:"));
  guiStuffPtr->triMapLabel = label;
  gtk_widget_show (label);
  gtk_table_attach (GTK_TABLE (table1), label, 0, 1, row, row+1,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);
  gtk_misc_set_alignment (GTK_MISC (label), 0, 0.5);

  combo = gimp_layer_combo_box_new (p_tri_map_combo_constrain, guiStuffPtr);
  guiStuffPtr->triMapCombo = combo;
  gtk_table_attach(GTK_TABLE(table1), combo, 1, 4, row, row+1,
                   GTK_EXPAND | GTK_FILL, 0, 0, 0);

  gimp_help_set_help_data(combo,
                       _("Select the tri_map layer (or layermask). "
                         " the tri-map shall be of same size as the input layer and "
                         " provides a rough user selection "
                         " where WHITE pixels in the tri map defines FOREGROUND (i.e. opaque result) "
                         " BLACK pixels define BACKGROUND (i.e. transparent result)"
                         " GRAY pixels (value 1 up to 240) are marked as Undefined "
                         " (i.e. opacity to be processed by this filter.) )")
                       , NULL);

  /* "Layermask" combo entry (is added in case the input drawable has a layermask that can be used as tri-mask) */
  {
    gint32 layerMaskId;
    gint   comboInitalValue;

    comboInitalValue = -1; 
    if(gimp_item_is_valid(guiStuffPtr->vals->tri_map_drawable_id))
    {
      comboInitalValue = guiStuffPtr->vals->tri_map_drawable_id;
    }  
    layerMaskId = gimp_layer_get_mask(guiStuffPtr->drawable_id);
    if (layerMaskId >= 0)
    {
      gimp_int_combo_box_prepend (GIMP_INT_COMBO_BOX (combo),
                              GIMP_INT_STORE_VALUE,    GAP_FG_USE_LAYER_MASK,
                              GIMP_INT_STORE_LABEL,    _("Layermask"),
                              GIMP_INT_STORE_STOCK_ID, GIMP_STOCK_LAYER_MASK,
                              -1);
      if (comboInitalValue == -1)
      {
        comboInitalValue = GAP_FG_USE_LAYER_MASK;
      }    
      guiStuffPtr->countTriMapCanidates++;
    }
    
    
    gimp_int_combo_box_connect (GIMP_INT_COMBO_BOX (combo),
                              comboInitalValue,
                              G_CALLBACK (p_tri_map_menu_callback),
                              &guiStuffPtr->vals->tri_map_drawable_id);
  }
  gtk_widget_show(combo);
 



  row++;

  /* create_layermask checkbutton  */
  label = gtk_label_new (_("Create Layermask:"));
  gtk_widget_show (label);
  gtk_table_attach (GTK_TABLE (table1), label, 0, 1, row, row+1,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);
  gtk_misc_set_alignment (GTK_MISC (label), 0, 0.5);


  checkbutton = gtk_check_button_new_with_label (" ");
  g_object_set_data (G_OBJECT (checkbutton), "guiStuffPtr", guiStuffPtr);
  guiStuffPtr->create_layermaskCheckbutton = checkbutton;
  gimp_help_set_help_data (checkbutton, _("ON: render opacity by creating a new layer mask, "
                                          "OFF: apply rendered opacity to the alpha channel"), NULL);
  gtk_widget_show (checkbutton);
  gtk_table_attach( GTK_TABLE(table1), checkbutton, 1, 2, row, row+1,
                    GTK_FILL, 0, 0, 0 );
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (checkbutton), cuvals->create_layermask);
  g_signal_connect (checkbutton, "toggled",
                    G_CALLBACK (on_gboolean_button_update),
                    &cuvals->create_layermask);

  row++;


  /* lock_color checkbutton  */
  label = gtk_label_new (_("Lock Colors:"));
  gtk_widget_show (label);
  gtk_table_attach (GTK_TABLE (table1), label, 0, 1, row, row+1,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);
  gtk_misc_set_alignment (GTK_MISC (label), 0, 0.5);

  checkbutton = gtk_check_button_new_with_label (" ");
  g_object_set_data (G_OBJECT (checkbutton), "guiStuffPtr", guiStuffPtr);
  guiStuffPtr->lock_colorCheckbutton = checkbutton;
  gimp_help_set_help_data (checkbutton, _("ON: Keep RGB channels of the input layer, "
                                          "OFF: allow Background color removal in processed undefined regions"), NULL);
  gtk_widget_show (checkbutton);
  gtk_table_attach( GTK_TABLE(table1), checkbutton, 1, 2, row, row+1,
                    GTK_FILL, 0, 0, 0 );
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (checkbutton), cuvals->lock_color);
  g_signal_connect (checkbutton, "toggled",
                    G_CALLBACK (on_gboolean_button_update),
                    &cuvals->lock_color);

  row++;

  /* create_result (create_result checkbutton  */
  label = gtk_label_new (_("Create Result Layer:"));
  //gtk_widget_show (label);
  gtk_table_attach (GTK_TABLE (table1), label, 0, 1, row, row+1,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);
  gtk_misc_set_alignment (GTK_MISC (label), 0, 0.5);


  checkbutton = gtk_check_button_new_with_label (" ");
  g_object_set_data (G_OBJECT (checkbutton), "guiStuffPtr", guiStuffPtr);
  guiStuffPtr->create_resultCheckbutton = checkbutton;
  //gtk_widget_show (checkbutton);
  gimp_help_set_help_data (checkbutton, _("ON: create a new layer as result of the foreground extraction processing. "
                                          "OFF: render transparency and background color removal on "
                                          "the input drawable"), NULL);
  gtk_table_attach( GTK_TABLE(table1), checkbutton, 1, 2, row, row+1,
                    GTK_FILL, 0, 0, 0 );
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (checkbutton), cuvals->create_result);
  g_signal_connect (checkbutton, "toggled",
                    G_CALLBACK (on_gboolean_button_update),
                    &cuvals->create_result);


  row++;


 
  /* colordiff_threshold spinbutton  */
  label = gtk_label_new (_("Color Diff Threshold"));
  //gtk_widget_show (label);
  gtk_table_attach (GTK_TABLE (table1), label, 0, 1, row, row+1,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);
  gtk_misc_set_alignment (GTK_MISC (label), 0, 0.5);
  spinbutton_adj = gtk_adjustment_new (cuvals->colordiff_threshold, 0.0, 100, 0.1, 1, 0);
  spinbutton = gtk_spin_button_new (GTK_ADJUSTMENT (spinbutton_adj), 1, 2);
  gimp_help_set_help_data (spinbutton, _("sensitivity for color comparison"), NULL);
  //gtk_widget_show (spinbutton);
  gtk_table_attach (GTK_TABLE (table1), spinbutton, 1, 2, row, row+1,
                    (GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);
  g_signal_connect (G_OBJECT (spinbutton_adj), "value_changed", G_CALLBACK (gimp_double_adjustment_update),
                    &cuvals->colordiff_threshold);
  guiStuffPtr->colordiff_threshold_spinbutton_adj = spinbutton_adj;
  guiStuffPtr->colordiff_threshold_spinbutton = spinbutton;






  /* -- */


  dialog_action_area1 = GTK_DIALOG (dialog1)->action_area;
  gtk_widget_show (dialog_action_area1);
  gtk_container_set_border_width (GTK_CONTAINER (dialog_action_area1), 10);

  p_init_widget_values(guiStuffPtr);
  p_update_widget_sensitivity (guiStuffPtr);

  gtk_widget_show (dialog1);

  gtk_main ();
  gdk_flush ();


}  /* end do_dialog */



void
gap_fg_matting_init_default_vals(GapFgExtractValues *fgVals)
{
  fgVals->create_result = TRUE;
  fgVals->create_layermask = FALSE;
  fgVals->lock_color = FALSE;
  fgVals->colordiff_threshold = 1.0;
}


gboolean
gap_fg_matting_dialog(GapFgExtractValues *fgVals)
{
  FgExtractDialogGuiStuff  guiStuff;
  FgExtractDialogGuiStuff *guiStuffPtr;
  
  guiStuffPtr = &guiStuff;
  guiStuffPtr->run = FALSE;
  guiStuffPtr->countTriMapCanidates = 0;
  guiStuffPtr->drawable_id = fgVals->input_drawable_id;
 
  /* Get information from the dialog */
  do_dialog(guiStuffPtr, fgVals);
  
  return (guiStuffPtr->run);

}  /* end gap_fg_matting_dialog */
