/*  gap_morph_tween_dialog.c
 *
 *  This module handles the GAP morph tween dialogs for
 *     o) gap_morph_one_tween_dialog
 *     o) gap_morph_frame_tweens_dialog
 *     o) gap_morph_generate_frame_tween_workpoints_dialog  (Workpointfile Generator)
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

#include <gtk/gtk.h>
#include <libgimp/gimp.h>
#include <libgimp/gimpui.h>

#include "gap_morph_main.h"
#include "gap_morph_tween_dialog.h"
#include "gap_morph_shape.h"
#include "gap_base.h"

#include "gap-intl.h"

extern int gap_debug;  /* 1 == print debug infos , 0 dont print debug infos */

#define SCALE_WIDTH  200
#define SPIN_BUTTON_WIDTH 60

typedef struct MorphTweenGui { /* nickname: mtg */
     GapMorphGlobalParams *mgpp;
     GtkWidget *morph_filesel;
     GtkWidget *morph_entry;
     GtkWidget *tween_subdir_entry;
     GapAnimInfo *ainfo_ptr;
     
     GtkWidget *shell;
     GtkWidget *masterProgressBar;
     GtkWidget *progressBar;
     gboolean   processingBusy;
     gboolean   cancelProcessing;
     gint32     ret;

     GtkWidget *from_scale;
     GtkWidget *from_spinbutton;
     GtkWidget *to_scale;
     GtkWidget *to_spinbutton;
     
  } MorphTweenGui;
  
  
/* ----------------------------
 * p_selectionConstraintFunc
 * ----------------------------
 *
 */
static gint
p_selectionConstraintFunc (gint32   image_id,
               gint32   drawable_id,
               gpointer data)
{
  if (image_id < 0)
    return FALSE;

  /* dont accept layers from indexed images */
  if (gimp_drawable_is_indexed (drawable_id))
    return FALSE;

  return TRUE;
}  /* end p_selectionConstraintFunc */

/* -----------------------------
 * p_layerSelectionComboCallback
 * -----------------------------
 *
 */
static void
p_layerSelectionComboCallback (GtkWidget *widget)
{
  gint idValue;
  gint32 *layerIdPtr;

  if(gap_debug)
  {
    printf("p_layerSelectionComboCallback START\n");
  }

  gimp_int_combo_box_get_active (GIMP_INT_COMBO_BOX (widget), &idValue);
  layerIdPtr = g_object_get_data (G_OBJECT (widget), "layerIdPtr");

  if(gap_debug)
  {
    printf("p_layerSelectionComboCallback idValue:%d\n", idValue);
  }
  
  if (layerIdPtr)
  {
    *layerIdPtr = idValue;
  }

}  /* end p_layerSelectionComboCallback */


/* --------------------------
 * p_master_progress_callback
 * --------------------------
 */
static void
p_master_progress_callback(gdouble numDone, gdouble numTotal, const char *filename, gpointer data)
{
  MorphTweenGui *mtg;
  
  mtg = (MorphTweenGui *) data;
  if(mtg)
  {
    if(mtg->masterProgressBar)
    {
      guchar *progressText;
      gdouble percentage;
      
      percentage = numDone / MAX(1.0, numTotal);
      
      if(filename != NULL)
      {
        char *shortFileName;
    
        shortFileName = gap_base_shorten_filename(NULL       /* prefix */
                        ,filename     /* filenamepart */
                        ,NULL                  /* suffix */
                        ,130                   /* l_max_chars */
                        );
        progressText = g_strdup_printf("%s  (%.0f / %.0f)"
                     , shortFileName
                     , (float)(MIN(numDone +1.0, numTotal))
                     , (float)(numTotal)
                     );
        g_free(shortFileName);
      }
      else
      {
        progressText = g_strdup_printf(_("Tween %.0f / %.0f")
                     , (float)(MIN(numDone +1.0, numTotal))
                     , (float)(numTotal)
                     );
      }
  
      gtk_progress_bar_set_text(GTK_PROGRESS_BAR(mtg->masterProgressBar), progressText);
      gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(mtg->masterProgressBar), percentage);
      g_free(progressText);

      while (gtk_events_pending ())
      {
        gtk_main_iteration ();
      }
      
    }
  
  }
}  /* end p_master_progress_callback */


/* --------------------------
 * p_progress_callback
 * --------------------------
 */
static void
p_progress_callback(gdouble percentage, gpointer data)
{
  MorphTweenGui *mtg;

  mtg = (MorphTweenGui *) data;
  if(mtg)
  {
    if(mtg->progressBar)
    {
      guchar *progressText;

      if(mtg->mgpp->do_simple_fade == TRUE)
      {
        progressText = g_strdup_printf(_("render tween via fade algorithm %.2f%%")
                     , (float)(percentage * 100)
                     );
      }
      else
      {
        progressText = g_strdup_printf(_("render tween via morphing algorithm %.2f%%")
                     , (float)(percentage * 100)
                     );
      }
      gtk_progress_bar_set_text(GTK_PROGRESS_BAR(mtg->progressBar), progressText);
      gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(mtg->progressBar), percentage);
      g_free(progressText);

      while (gtk_events_pending ())
      {
        gtk_main_iteration ();
      }
      
    }
  }
  
  
}  /* end p_progress_callback */




/* --------------------------
 * FILESEL
 * --------------------------
 */
static void
p_filesel_close_cb(GtkWidget *widget, MorphTweenGui *mtg)
{
  if(mtg->morph_filesel == NULL) return;  /* filesel is already closed */

  gtk_widget_destroy(GTK_WIDGET(mtg->morph_filesel));
  mtg->morph_filesel = NULL;
}

static void
p_filesel_ok_cb(GtkWidget *widget, MorphTweenGui *mtg)
{
  const gchar *filename;

  if(mtg->morph_filesel == NULL) return;  /* filesel is already closed */

  filename = gtk_file_selection_get_filename (GTK_FILE_SELECTION (mtg->morph_filesel));

  gtk_entry_set_text(GTK_ENTRY(mtg->morph_entry), filename);

  p_filesel_close_cb(widget, mtg);
}

/* --------------------------------------
 * p_filesel_open_cb
 * --------------------------------------
 */
static void
p_filesel_open_cb(GtkWidget *widget, MorphTweenGui *mtg)
{
  GtkWidget *filesel;
  GapMorphGlobalParams *mgpp;

  if(mtg->morph_filesel != NULL)
  {
    gtk_window_present(GTK_WINDOW(mtg->morph_filesel));
    return;  /* filesel is already open */
  }
  
  filesel = gtk_file_selection_new (_("Enter Morph Workpoint filename"));
  mtg->morph_filesel = filesel;
  mgpp =  mtg->mgpp;

  gtk_window_set_position (GTK_WINDOW (filesel), GTK_WIN_POS_MOUSE);

  gtk_file_selection_set_filename (GTK_FILE_SELECTION (filesel),
                                   &mgpp->workpoint_file_lower[0]);
  gtk_widget_show (filesel);

  g_signal_connect (G_OBJECT (filesel), "destroy",
                    G_CALLBACK (p_filesel_close_cb),
                    mtg);

  g_signal_connect (G_OBJECT (GTK_FILE_SELECTION (filesel)->ok_button),
                   "clicked",
                    G_CALLBACK (p_filesel_ok_cb),
                    mtg);
  g_signal_connect (G_OBJECT (GTK_FILE_SELECTION (filesel)->cancel_button),
                   "clicked",
                    G_CALLBACK (p_filesel_close_cb),
                    mtg);
}

/* -----------------------------------------------------
 * p_check_workpoint_file_and_use_single_fade_if_missing
 * -----------------------------------------------------
 */
static void
p_check_workpoint_file_and_use_single_fade_if_missing(GapMorphGlobalParams *mgpp)
{
   if(g_file_test(&mgpp->workpoint_file_lower[0], G_FILE_TEST_EXISTS))
   {
     mgpp->do_simple_fade     = FALSE;  /* use morph algortihm when workpoint file is available */
   }
   else
   {
     mgpp->do_simple_fade     = TRUE;  /* use simple fade because we have no workpoint file */
   }
}


/* --------------------------------------
 * on_gboolean_button_update
 * --------------------------------------
 */
static void
on_gboolean_button_update (GtkWidget *widget,
                           gpointer   data)
{
  gint *toggle_val = (gint *) data;

  if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget)))
    *toggle_val = TRUE;
  else
    *toggle_val = FALSE;

  gimp_toggle_button_sensitive_update (GTK_TOGGLE_BUTTON (widget));
}

/* --------------------------------------
 * p_morph_workpoint_entry_update_cb
 * --------------------------------------
 */
static void
p_morph_workpoint_entry_update_cb(GtkWidget *widget, GapMorphGlobalParams *mgpp)
{
   g_snprintf(&mgpp->workpoint_file_lower[0]
           , sizeof(mgpp->workpoint_file_lower)
           , "%s"
           , gtk_entry_get_text(GTK_ENTRY(widget))
           );
   g_snprintf(&mgpp->workpoint_file_upper[0]
           , sizeof(mgpp->workpoint_file_upper)
           , "%s"
           , gtk_entry_get_text(GTK_ENTRY(widget))
           );
   if(gap_debug)
   {
     printf("p_morph_workpoint_entry_update_cb  workpoint_file_lower: %s\n"
       , &mgpp->workpoint_file_lower[0]);
   }

   p_check_workpoint_file_and_use_single_fade_if_missing(mgpp);
   
}  /* end p_morph_workpoint_entry_update_cb */

/* --------------------------------------
 * p_tween_subdir_entry_update_cb
 * --------------------------------------
 */
static void
p_tween_subdir_entry_update_cb(GtkWidget *widget, GapMorphGlobalParams *mgpp)
{
   g_snprintf(&mgpp->tween_subdir[0]
           , sizeof(mgpp->tween_subdir)
           , "%s"
           , gtk_entry_get_text(GTK_ENTRY(widget))
           );
   if(gap_debug)
   {
     printf("p_tween_subdir_entry_update_cb  tween_subdir: %s\n"
       , &mgpp->tween_subdir[0]);
   }
  
}  /* end p_tween_subdir_entry_update_cb */


/* --------------------------------------
 * p_create_morph_workpoint_entry
 * --------------------------------------
 */
static void
p_create_morph_workpoint_entry(GapMorphGlobalParams *mgpp, MorphTweenGui *mtg, gint row, GtkWidget *table)
{
  GtkWidget *entry;
  GtkWidget *label;
  GtkWidget *button;

  /* morph workpoint entry */
  label = gtk_label_new (_("Morph Workpoint file:"));
  gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
  gtk_table_attach (GTK_TABLE (table), label, 0, 1, row, row + 1,
                    GTK_FILL, GTK_FILL, 4, 0);
  gtk_widget_show (label);

  entry = gtk_entry_new();
  mtg->morph_entry = entry;
  gtk_widget_set_size_request(entry, SCALE_WIDTH, -1);
  if(mgpp->workpoint_file_lower[0] != '\0')
  {
    gtk_entry_set_text(GTK_ENTRY(entry), &mgpp->workpoint_file_lower[0]);
  }
  gtk_table_attach(GTK_TABLE(table), entry, 1, 2, row, row + 1, GTK_FILL, GTK_FILL | GTK_EXPAND, 4, 0);
  gimp_help_set_help_data(entry, _("Name of a Workpointfile created with the Morph feature\n"
                                   "(note that tweens are created via simple fade operations when no workpointfile is available)"), NULL);
  gtk_widget_show(entry);

  g_signal_connect(G_OBJECT(entry), "changed",
                   G_CALLBACK (p_morph_workpoint_entry_update_cb),
                   mgpp);
    
    
  /* Button  to invoke filebrowser */  
  button = gtk_button_new_with_label ( "..." );
  gtk_widget_set_size_request(button, SPIN_BUTTON_WIDTH, -1);
  g_object_set_data (G_OBJECT (button), "mgpp", (gpointer)mgpp);
  gtk_table_attach( GTK_TABLE(table), button, 2, 3, row, row +1,
                    0, 0, 0, 0 );
  gtk_widget_show (button);
  g_signal_connect (G_OBJECT (button), "clicked",
                    G_CALLBACK (p_filesel_open_cb),
                    mtg);
}  /* end p_create_morph_workpoint_entry */

/* --------------------------------------
 * gap_morph_one_tween_dialog
 * --------------------------------------
 */
gboolean
gap_morph_one_tween_dialog(GapMorphGlobalParams *mgpp)
{
  MorphTweenGui  morphTweenGui;
  MorphTweenGui  *mtg;
  GtkWidget *dialog;
  GtkWidget *main_vbox;
  GtkWidget *label;
  GtkWidget *table;
  GtkWidget *combo;
  GtkObject *adj;
  gint       row;
  gboolean   run;

  mtg = &morphTweenGui;
  mtg->mgpp = mgpp;
  mtg->morph_filesel = NULL;
  p_check_workpoint_file_and_use_single_fade_if_missing(mgpp);

  gimp_ui_init (GAP_MORPH_TWEEN_PLUGIN_NAME, TRUE);

  dialog = gimp_dialog_new (_("Create one tween as Layer"), GAP_MORPH_TWEEN_PLUGIN_NAME,
                            NULL, 0,
                            gimp_standard_help_func, GAP_MORPH_TWEEN_HELP_ID,

                            GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                            GTK_STOCK_OK,     GTK_RESPONSE_OK,

                            NULL);


  gtk_dialog_set_alternative_button_order (GTK_DIALOG (dialog),
                                           GTK_RESPONSE_OK,
                                           GTK_RESPONSE_CANCEL,
                                           -1);

  gimp_window_set_transient (GTK_WINDOW (dialog));

  main_vbox = gtk_vbox_new (FALSE, 12);
  gtk_container_set_border_width (GTK_CONTAINER (main_vbox), 12);
  gtk_container_add (GTK_CONTAINER (GTK_DIALOG (dialog)->vbox), main_vbox);
  gtk_widget_show (main_vbox);


  /* Controls */
  table = gtk_table_new (3, 3, FALSE);
  gtk_table_set_col_spacings (GTK_TABLE (table), 6);
  gtk_table_set_row_spacings (GTK_TABLE (table), 6);
  gtk_box_pack_start (GTK_BOX (main_vbox), table, FALSE, FALSE, 0);
  gtk_widget_show (table);

  row = 0;

  adj = gimp_scale_entry_new (GTK_TABLE (table), 0, row,
                              _("tween mix:"), SCALE_WIDTH, 7,
                              mgpp->tween_mix_factor, 0.0, 1.0, 0.1, 0.1, 3,
                              TRUE, 0, 0,
                              NULL, NULL);
  g_signal_connect (adj, "value-changed",
                    G_CALLBACK (gimp_double_adjustment_update),
                    &mgpp->tween_mix_factor);

  row++;

  /* layer combo_box (source) */
  label = gtk_label_new (_("Source Layer:"));
  gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
  gtk_table_attach (GTK_TABLE (table), label, 0, 1, row, row + 1,
                    GTK_FILL, GTK_FILL, 4, 0);
  gtk_widget_show (label);

  /* layer combo_box (Sample from where to pick the alternative selection */
  combo = gimp_layer_combo_box_new (p_selectionConstraintFunc, NULL);

  g_object_set_data (G_OBJECT (combo), "layerIdPtr", &mgpp->osrc_layer_id);
  gimp_int_combo_box_connect (GIMP_INT_COMBO_BOX (combo), mgpp->osrc_layer_id,
                              G_CALLBACK (p_layerSelectionComboCallback),
                              NULL);

  gtk_table_attach (GTK_TABLE (table), combo, 1, 3, row, row + 1,
                    GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);
  gtk_widget_show (combo);

  row++;

  /* layer combo_box (source) */
  label = gtk_label_new (_("Destination Layer:"));
  gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
  gtk_table_attach (GTK_TABLE (table), label, 0, 1, row, row + 1,
                    GTK_FILL, GTK_FILL, 4, 0);
  gtk_widget_show (label);

  /* layer combo_box (Sample from where to pick the alternative selection */
  combo = gimp_layer_combo_box_new (p_selectionConstraintFunc, NULL);

  g_object_set_data (G_OBJECT (combo), "layerIdPtr", &mgpp->fdst_layer_id);
  gimp_int_combo_box_connect (GIMP_INT_COMBO_BOX (combo), mgpp->fdst_layer_id,
                              G_CALLBACK (p_layerSelectionComboCallback),
                              NULL);

  gtk_table_attach (GTK_TABLE (table), combo, 1, 3, row, row + 1,
                    GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);
  gtk_widget_show (combo);

  row++;

  p_create_morph_workpoint_entry(mgpp, mtg, row, table);

  /* Done */

  gtk_widget_show (dialog);

  run = (gimp_dialog_run (GIMP_DIALOG (dialog)) == GTK_RESPONSE_OK);

  gtk_widget_destroy (dialog);

  return run;
} /* end gap_morph_one_tween_dialog */




/* -------------------------------
 * on_workpoint_generator_response
 * -------------------------------
 */
static void
on_workpoint_generator_response(GtkWidget *w, gint response_id, MorphTweenGui *mtg)
{
  GtkWidget *dialog;

  switch (response_id)
  {
    case GTK_RESPONSE_OK:
      if(mtg == NULL)
      {
        return;
      }
      if((mtg->cancelProcessing == FALSE))
      {
        if(mtg->processingBusy == TRUE)
        {
          return;
        }
        mtg->processingBusy = TRUE;
        gtk_widget_set_sensitive(mtg->from_scale, FALSE);
        gtk_widget_set_sensitive(mtg->from_spinbutton, FALSE);
        
        
        mtg->mgpp->use_gravity = FALSE;
        if(mtg->mgpp->gravity_intensity >= 1.0)
        {
          mtg->mgpp->use_gravity = TRUE;
        }
        mtg->ret = gap_morph_shape_generate_frame_tween_workpoints(mtg->ainfo_ptr
                                , mtg->mgpp
                                , mtg->masterProgressBar
                                , mtg->progressBar
                                , &mtg->cancelProcessing
                                );
        mtg->cancelProcessing = TRUE;
        mtg->processingBusy = FALSE;
      }
      /* fall through (close dialog window when cancel or done) */
    default:
      dialog = NULL;
      if(mtg)
      {
        if((mtg->processingBusy == TRUE) && (mtg->cancelProcessing == FALSE))
        {
          mtg->cancelProcessing = TRUE;
          return;
        }
        /* close (or terminate) dialog window */
        dialog = mtg->shell;
        if(dialog)
        {
          mtg->shell = NULL;
          gtk_widget_destroy (dialog);
        }
      }
      gtk_main_quit ();
      break;
  }
}  /* end on_workpoint_generator_response */


/* ------------------------------------------------
 * gap_morph_generate_frame_tween_workpoints_dialog
 * ------------------------------------------------
 */
gint32
gap_morph_generate_frame_tween_workpoints_dialog(GapAnimInfo *ainfo_ptr, GapMorphGlobalParams *mgpp)
{
  MorphTweenGui  morphTweenGui;
  MorphTweenGui  *mtg;
  GtkWidget *dialog;
  GtkWidget *main_vbox;
  GtkWidget *table;
  GtkObject *adj;
  GtkWidget *scale;
  GtkWidget *spinbutton;
  GtkWidget *label;
  GtkWidget *checkbutton;
  GtkWidget *progressBar;
  gint       row;

  mgpp->range_from = ainfo_ptr->curr_frame_nr;
  mgpp->range_to = ainfo_ptr->last_frame_nr;
  mtg = &morphTweenGui;
  mtg->mgpp = mgpp;
  mtg->ainfo_ptr = ainfo_ptr;
  mtg->morph_filesel = NULL;
  mtg->cancelProcessing = FALSE;
  mtg->processingBusy = FALSE;
  mtg->ret = 0;
  
  
  p_check_workpoint_file_and_use_single_fade_if_missing(mgpp);


  if(gap_debug)
  {
    printf("gap_morph_generate_frame_tween_workpoints_dialog: mgpp->range_from%d  mgpp->range_to:%d  first:%d, curr:%d, last:%d after_curr:%d\n"
      , (int)mgpp->range_from
      , (int)mgpp->range_to
      , (int)ainfo_ptr->first_frame_nr
      , (int)ainfo_ptr->curr_frame_nr
      , (int)ainfo_ptr->last_frame_nr
      , (int)ainfo_ptr->frame_nr_after_curr_frame_nr
      );
  }


  gimp_ui_init (GAP_MORPH_WORKPOINTS_PLUGIN_NAME, TRUE);

  dialog = gimp_dialog_new (_("Generate Workpointfiles"), GAP_MORPH_WORKPOINTS_PLUGIN_NAME,
                            NULL, 0,
                            gimp_standard_help_func, GAP_MORPH_WORKPOINTS_HELP_ID,

                            GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                            GTK_STOCK_OK,     GTK_RESPONSE_OK,

                            NULL);

  mtg->shell = dialog;

  gtk_dialog_set_alternative_button_order (GTK_DIALOG (dialog),
                                           GTK_RESPONSE_OK,
                                           GTK_RESPONSE_CANCEL,
                                           -1);

  gimp_window_set_transient (GTK_WINDOW (dialog));
  gtk_window_set_type_hint (dialog, GDK_WINDOW_TYPE_HINT_NORMAL);

  g_signal_connect (G_OBJECT (dialog), "response",
                    G_CALLBACK (on_workpoint_generator_response),
                    mtg);

  main_vbox = gtk_vbox_new (FALSE, 12);
  gtk_container_set_border_width (GTK_CONTAINER (main_vbox), 12);
  gtk_container_add (GTK_CONTAINER (GTK_DIALOG (dialog)->vbox), main_vbox);
  gtk_widget_show (main_vbox);


  /* Controls */
  table = gtk_table_new (3, 3, FALSE);
  gtk_table_set_col_spacings (GTK_TABLE (table), 6);
  gtk_table_set_row_spacings (GTK_TABLE (table), 6);
  gtk_box_pack_start (GTK_BOX (main_vbox), table, FALSE, FALSE, 0);
  gtk_widget_show (table);

  row = 0;

  adj = gimp_scale_entry_new (GTK_TABLE (table), 0, row,
                              _("From:"), SCALE_WIDTH, 7,
                              mgpp->range_from, ainfo_ptr->first_frame_nr, ainfo_ptr->last_frame_nr, 1.0, 10.0, 0,
                              TRUE, 0, 0,
                              _("First processed frame"), NULL);
  scale = g_object_get_data(G_OBJECT (adj), "scale");
  spinbutton = g_object_get_data(G_OBJECT (adj), "spinbutton");
  mtg->from_scale = scale;
  mtg->from_spinbutton = spinbutton;
  g_signal_connect (adj, "value-changed",
                    G_CALLBACK (gimp_int_adjustment_update),
                    &mgpp->range_from);


  row++;

  adj = gimp_scale_entry_new (GTK_TABLE (table), 0, row,
                              _("To:"), SCALE_WIDTH, 7,
                              mgpp->range_to, mgpp->range_from +1, ainfo_ptr->last_frame_nr, 1.0, 10.0, 0,
                              TRUE, 0, 0,
                              _("Last processed frame"), NULL);
  scale = g_object_get_data(G_OBJECT (adj), "scale");
  spinbutton = g_object_get_data(G_OBJECT (adj), "spinbutton");
  mtg->to_scale = scale;
  mtg->to_spinbutton = spinbutton;
  g_signal_connect (adj, "value-changed",
                    G_CALLBACK (gimp_int_adjustment_update),
                    &mgpp->range_to);

  row++;

  adj = gimp_scale_entry_new (GTK_TABLE (table), 0, row,
                              _("Num Workpoints:"), SCALE_WIDTH, 7,
                              mgpp->numWorkpoints, 1, 5000, 1.0, 10.0, 0,
                              TRUE, 0, 0,
                              _("Number of workpoints to be generated par processed frame"),
                              NULL);
  g_signal_connect (adj, "value-changed",
                    G_CALLBACK (gimp_int_adjustment_update),
                    &mgpp->numWorkpoints);

  row++;

  adj = gimp_scale_entry_new (GTK_TABLE (table), 0, row,
                              _("Num Outlinepoints:"), SCALE_WIDTH, 7,
                              mgpp->numOutlinePoints, 1, 100, 1.0, 10.0, 0,
                              TRUE, 0, 0,
                              _("Number of additional workpoints on the outline of opaque image area")
                              , NULL);
  g_signal_connect (adj, "value-changed",
                    G_CALLBACK (gimp_int_adjustment_update),
                    &mgpp->numOutlinePoints);


  row++;

  adj = gimp_scale_entry_new (GTK_TABLE (table), 0, row,
                              _("Tween Steps:"), SCALE_WIDTH, 7,
                              mgpp->tween_steps, 1, 100, 1.0, 10.0, 0,
                              TRUE, 0, 0,
                              _("TWEEN-STEPS attribute value to be written to the generated workpoint file."
                                " (Number of tweens to be inserted between 2 frames at tween morphprocessing) "),
                              NULL);
  g_signal_connect (adj, "value-changed",
                    G_CALLBACK (gimp_int_adjustment_update),
                    &mgpp->tween_steps);
  
  row++;


  adj = gimp_scale_entry_new (GTK_TABLE (table), 0, row,
                              _("Deform Radius:"), SCALE_WIDTH, 7,
                              mgpp->affect_radius, 0, 10000, 1.0, 10.0, 0,
                              TRUE, 0, 0,
                              _("AFFECT-RADIUS attribute value to be written to the generated workpoint file."),
                              NULL);
  g_signal_connect (adj, "value-changed",
                    G_CALLBACK (gimp_int_adjustment_update),
                    &mgpp->affect_radius);
  
  row++;

  if(!mgpp->use_gravity)
  {
    mgpp->gravity_intensity = 0.0;
  }

  adj = gimp_scale_entry_new (GTK_TABLE (table), 0, row,
                              _("Intensity:"), SCALE_WIDTH, 7,
                              mgpp->gravity_intensity, 0.0, 5.0, 0.1, 0.1, 1,
                              TRUE, 0, 0,
                              _("INTENSITY attribute value to be written to the generated workpoint file. "
                                "value 0 turns off intensity desceding deformation, "
                                "morph processing will use linear deform action inside the deform radius"),
                              NULL);
  g_signal_connect (adj, "value-changed",
                    G_CALLBACK (gimp_double_adjustment_update),
                    &mgpp->gravity_intensity);





  row++;

  adj = gimp_scale_entry_new (GTK_TABLE (table), 0, row,
                              _("Locate Move Radius:"), SCALE_WIDTH, 7,
                              mgpp->locateDetailMoveRadius, 1, 500, 1.0, 10.0, 0,
                              TRUE, 0, 0,
                              _("Locate radius in pixels. "
                                "The workpoint generation searches for corresponding points "
                                "in the next frame within this radius"),
                              NULL);
  g_signal_connect (adj, "value-changed",
                    G_CALLBACK (gimp_int_adjustment_update),
                    &mgpp->locateDetailMoveRadius);

  row++;

  adj = gimp_scale_entry_new (GTK_TABLE (table), 0, row,
                              _("Locate Shape Radius:"), SCALE_WIDTH, 7,
                              mgpp->locateDetailShapeRadius,
                               GAP_LOCATE_MIN_REF_SHAPE_RADIUS, 
                               GAP_LOCATE_MAX_REF_SHAPE_RADIUS,
                               1.0, 10.0, 0,
                              TRUE, 0, 0,
                              _("Locate Shaperadius in pixels. "
                                "Defines shape size as area around workpoint to be compared "
                                " when loacting corresponding coordinate in the next frame."),
                              NULL);
  g_signal_connect (adj, "value-changed",
                    G_CALLBACK (gimp_int_adjustment_update),
                    &mgpp->locateDetailShapeRadius);
  row++;
  
  adj = gimp_scale_entry_new (GTK_TABLE (table), 0, row,
                                _("Edge Threshold:"), SCALE_WIDTH, 7,
                                mgpp->edgeColordiffThreshold, 0.01, 0.35, 0.01, 0.1, 2,
                                TRUE, 0, 0,
                                _("Edge detection threshold. "
                                  "Workpoints are generated on detected edges. "
                                  "Edges are detected on pixels where color or opacity differs significant "
                                  "from the neighbor pixel."
                                  "(e.g. more than the specified edge detection threshold)."),
                                NULL);
  g_signal_connect (adj, "value-changed",
                      G_CALLBACK (gimp_double_adjustment_update),
                      &mgpp->edgeColordiffThreshold);
  if(gap_debug)
  {

    row++;

    adj = gimp_scale_entry_new (GTK_TABLE (table), 0, row,
                                _("Locate ColordiffEdge Threshold:"), SCALE_WIDTH, 7,
                                mgpp->locateColordiffThreshold, 0.01, 0.1, 0.01, 0.1, 3,
                                TRUE, 0, 0,
                                NULL, NULL);
    g_signal_connect (adj, "value-changed",
                      G_CALLBACK (gimp_double_adjustment_update),
                      &mgpp->locateColordiffThreshold);
 
  }

  row++;

  /* the use_quality_wp_selection checkbutton */
  checkbutton = gtk_check_button_new_with_label ( _("Quality"));
  gtk_widget_show (checkbutton);
  gtk_table_attach( GTK_TABLE(table), checkbutton, 2, 3, row, row+1,
                    GTK_FILL, 0, 0, 0 );
  g_signal_connect (checkbutton, "toggled",
                    G_CALLBACK (on_gboolean_button_update),
                    &mgpp->use_quality_wp_selection);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (checkbutton), mgpp->use_quality_wp_selection);
  gimp_help_set_help_data(checkbutton,
                       _("ON: Use quality workpoint selection algorithm."
                         "OFF: Use fast workpoint selection algorithm.")
                       , NULL);
  row++;


  /* the overwrite checkbutton */
  checkbutton = gtk_check_button_new_with_label ( _("Overwrite"));
  gtk_widget_show (checkbutton);
  gtk_table_attach( GTK_TABLE(table), checkbutton, 1, 2, row, row+1,
                    GTK_FILL, 0, 0, 0 );
  g_signal_connect (checkbutton, "toggled",
                    G_CALLBACK (on_gboolean_button_update),
                    &mgpp->overwrite_flag);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (checkbutton), mgpp->overwrite_flag);
  gimp_help_set_help_data(checkbutton,
                       _("ON: overwrite existing workpointfiles."
                         "OFF: Skip workpoint generation or add new generated workpoints (see append checkbutton).")
                       , NULL);


  /* the overwrite checkbutton */
  checkbutton = gtk_check_button_new_with_label ( _("Append"));
  gtk_widget_show (checkbutton);
  gtk_table_attach( GTK_TABLE(table), checkbutton, 2, 3, row, row+1,
                    GTK_FILL, 0, 0, 0 );
  g_signal_connect (checkbutton, "toggled",
                    G_CALLBACK (on_gboolean_button_update),
                    &mgpp->append_flag);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (checkbutton), mgpp->append_flag);
  gimp_help_set_help_data(checkbutton,
                       _("ON: add newly generated workpoints to existing workpointfiles."
                         "OFF: Skip workpoint generation for frames where workpointfile already exists.")
                       , NULL);

  row++;

  /* the master progress bar */

  /* master progress */
  label = gtk_label_new (_("Create File(s):"));
  gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
  gtk_table_attach (GTK_TABLE (table), label, 0, 1, row, row + 1,
                    GTK_FILL, GTK_FILL, 4, 0);
  gtk_widget_show (label);

  /* the master progress bar */
  progressBar = gtk_progress_bar_new ();
  mtg->masterProgressBar = progressBar;
  {
    char *suffix;
    char *msg;
    suffix = g_strdup_printf("%s.%s"
                         ,"######"
                         ,GAP_MORPH_WORKPOINT_EXTENSION
                         );
    msg = gap_base_shorten_filename(NULL        /* prefix */
                        ,ainfo_ptr->basename    /* filenamepart */
                        ,suffix                 /* suffix */
                        ,130                     /* l_max_chars */
                        );
    gtk_progress_bar_set_text(GTK_PROGRESS_BAR(progressBar), msg);
    g_free(msg);
    g_free(suffix);
  }

  gtk_table_attach (GTK_TABLE (table), progressBar, 1, 3, row, row + 1,
                    GTK_FILL, GTK_FILL, 4, 0);
  gtk_widget_show (progressBar);

  row++;

  label = gtk_label_new (_("Create Points:"));
  gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
  gtk_table_attach (GTK_TABLE (table), label, 0, 1, row, row + 1,
                    GTK_FILL, GTK_FILL, 4, 0);
  gtk_widget_show (label);


  /* the progress bar */
  progressBar = gtk_progress_bar_new ();
  mtg->progressBar = progressBar;
  gtk_progress_bar_set_text(GTK_PROGRESS_BAR(progressBar), " ");
  gtk_widget_show (progressBar);
  gtk_table_attach( GTK_TABLE(table), progressBar, 1, 3, row, row+1,
                    GTK_FILL, 0, 0, 0 );
  
  /* Done */

  gtk_widget_show (dialog);

  gtk_main ();
  gdk_flush ();


  return (mtg->ret);
} /* end gap_morph_generate_frame_tween_workpoints_dialog */



/* -------------------------------
 * on_morph_frame_tweens_response
 * -------------------------------
 */
static void
on_morph_frame_tweens_response(GtkWidget *w, gint response_id, MorphTweenGui *mtg)
{
  GtkWidget *dialog;

  switch (response_id)
  {
    case GTK_RESPONSE_OK:
      if(mtg == NULL)
      {
        return;
      }
      if((mtg->cancelProcessing == FALSE))
      {
        if(mtg->processingBusy == TRUE)
        {
          return;
        }
        mtg->processingBusy = TRUE;
        mtg->mgpp->do_progress = TRUE;
        mtg->mgpp->master_progress_callback_fptr = p_master_progress_callback;
        mtg->mgpp->progress_callback_fptr = p_progress_callback;
        mtg->mgpp->callback_data_ptr = mtg;
        gtk_widget_set_sensitive(mtg->from_scale, FALSE);
        gtk_widget_set_sensitive(mtg->from_spinbutton, FALSE);
        
        mtg->ret = gap_morph_render_frame_tweens(mtg->ainfo_ptr
                                , mtg->mgpp
                                , &mtg->cancelProcessing
                                );
        mtg->cancelProcessing = TRUE;
        mtg->processingBusy = FALSE;
      }
      /* fall through (close dialog window when cancel or done) */
    default:
      dialog = NULL;
      if(mtg)
      {
        if((mtg->processingBusy == TRUE) && (mtg->cancelProcessing == FALSE))
        {
          mtg->cancelProcessing = TRUE;
          return;
        }
        /* close (or terminate) dialog window */
        dialog = mtg->shell;
        if(dialog)
        {
          mtg->shell = NULL;
          gtk_widget_destroy (dialog);
        }
      }
      gtk_main_quit ();
      break;
  }
}  /* end on_morph_frame_tweens_response */








/* --------------------------------------
 * gap_morph_frame_tweens_dialog
 * --------------------------------------
 */
gint32
gap_morph_frame_tweens_dialog(GapAnimInfo *ainfo_ptr, GapMorphGlobalParams *mgpp)
{
  MorphTweenGui  morphTweenGui;
  MorphTweenGui  *mtg;
  GtkWidget *dialog;
  GtkWidget *main_vbox;
  GtkWidget *table;
  GtkObject *adj;
  GtkWidget *scale;
  GtkWidget *spinbutton;
  GtkWidget *label;
  GtkWidget *entry;
  GtkWidget *checkbutton;
  GtkWidget *progressBar;
  gint       row;
  gboolean   isFrameMissing;
  gboolean   isOpertionPossible;
  gint       missingFrames;

  mgpp->range_from = ainfo_ptr->curr_frame_nr;
  mgpp->range_to = ainfo_ptr->frame_nr_after_curr_frame_nr;
  mtg = &morphTweenGui;
  mtg->mgpp = mgpp;
  mtg->ainfo_ptr = ainfo_ptr;
  mtg->morph_filesel = NULL;
  mtg->ret = -1;
  isFrameMissing = FALSE;
  isOpertionPossible = TRUE;

  p_check_workpoint_file_and_use_single_fade_if_missing(mgpp);

  isFrameMissing = (ainfo_ptr->curr_frame_nr +1) != ainfo_ptr->frame_nr_after_curr_frame_nr;
  if(!isFrameMissing)
  {
    mgpp->range_to = ainfo_ptr->last_frame_nr;
  }

  missingFrames = (mgpp->range_to - mgpp->range_from) -1;
  if (missingFrames < 0)
  {
    isOpertionPossible = FALSE;
  }

  if(gap_debug)
  {
    printf("gap_morph_frame_tweens_dialog: mgpp->range_from%d  mgpp->range_to:%d  first:%d, curr:%d, last:%d after_curr:%d\n"
      , (int)mgpp->range_from
      , (int)mgpp->range_to
      , (int)ainfo_ptr->first_frame_nr
      , (int)ainfo_ptr->curr_frame_nr
      , (int)ainfo_ptr->last_frame_nr
      , (int)ainfo_ptr->frame_nr_after_curr_frame_nr
      );
  }


  gimp_ui_init (GAP_MORPH_TWEEN_PLUGIN_NAME, TRUE);

  if (isOpertionPossible)
  {
    dialog = gimp_dialog_new (_("Create Tween Frames"), GAP_MORPH_TWEEN_PLUGIN_NAME,
                            NULL, 0,
                            gimp_standard_help_func, GAP_MORPH_TWEEN_HELP_ID,

                            GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                            GTK_STOCK_OK,     GTK_RESPONSE_OK,

                            NULL);
  }
  else
  {
    dialog = gimp_dialog_new (_("Create Tween Frames"), GAP_MORPH_TWEEN_PLUGIN_NAME,
                            NULL, 0,
                            gimp_standard_help_func, GAP_MORPH_TWEEN_HELP_ID,

                            GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,

                            NULL);
  }

  mtg->shell = dialog;


  gtk_dialog_set_alternative_button_order (GTK_DIALOG (dialog),
                                           GTK_RESPONSE_OK,
                                           GTK_RESPONSE_CANCEL,
                                           -1);

  gimp_window_set_transient (GTK_WINDOW (dialog));
  gtk_window_set_type_hint (dialog, GDK_WINDOW_TYPE_HINT_NORMAL);

  g_signal_connect (G_OBJECT (dialog), "response",
                    G_CALLBACK (on_morph_frame_tweens_response),
                    mtg);

  main_vbox = gtk_vbox_new (FALSE, 12);
  gtk_container_set_border_width (GTK_CONTAINER (main_vbox), 12);
  gtk_container_add (GTK_CONTAINER (GTK_DIALOG (dialog)->vbox), main_vbox);
  gtk_widget_show (main_vbox);


  /* Controls */
  table = gtk_table_new (3, 3, FALSE);
  gtk_table_set_col_spacings (GTK_TABLE (table), 6);
  gtk_table_set_row_spacings (GTK_TABLE (table), 6);
  gtk_box_pack_start (GTK_BOX (main_vbox), table, FALSE, FALSE, 0);
  gtk_widget_show (table);

  row = 0;

  /* label */
  label = gtk_label_new (_("Information:"));
  gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
  gtk_table_attach (GTK_TABLE (table), label, 0, 1, row, row + 1,
                    GTK_FILL, GTK_FILL, 4, 0);
  gtk_widget_show (label);

  if (isOpertionPossible)
  {
    if (isFrameMissing)
    {
      char *msg;
      msg = g_strdup_printf(ngettext("This operation creates %d missing frame between frame %d and %d"
                                      ,"this operation creates %d missing frames between frame %d and %d"
                                      ,(int)missingFrames)
                             ,(int)missingFrames
                             ,(int)mgpp->range_from
                             ,(int)mgpp->range_to
                             );
      label = gtk_label_new (msg);
      g_free(msg);
      mgpp->overwrite_flag = FALSE;
    }
    else
    {
      label = gtk_label_new (_("This operation creates copies of all frames in the specified range\n"
                               "and the specifed number of tweens as additional tween frames\n"
                               "between all the processed frames in the specified subdirectory.\n"
                               "Provide workpointfiles (one per frame) for morphing based tween rendering\n"
                               "(this can be done with the Morph Workpoint Generator)"));
      mgpp->overwrite_flag = TRUE;
    }
  }
  else
  {
      label = gtk_label_new (_("This operation requires more than one frame."));
      mgpp->overwrite_flag = FALSE;
  }
  
  gtk_misc_set_alignment (GTK_MISC (label), 0.5, 0.5);
  gtk_table_attach (GTK_TABLE (table), label, 1, 3, row, row + 1,
                    GTK_FILL, GTK_FILL, 4, 0);
  gtk_widget_show (label);

  row++;

  adj = gimp_scale_entry_new (GTK_TABLE (table), 0, row,
                              _("From:"), SCALE_WIDTH, 7,
                              mgpp->range_from, ainfo_ptr->first_frame_nr, ainfo_ptr->last_frame_nr-1, 1.0, 10.0, 0,
                              TRUE, 0, 0,
                              _("First processed frame"), NULL);
  g_signal_connect (adj, "value-changed",
                    G_CALLBACK (gimp_int_adjustment_update),
                    &mgpp->range_from);

  scale = g_object_get_data(G_OBJECT (adj), "scale");
  spinbutton = g_object_get_data(G_OBJECT (adj), "spinbutton");
  mtg->from_scale = scale;
  mtg->from_spinbutton = spinbutton;
  if(isFrameMissing)
  {
    gtk_widget_set_sensitive(scale, FALSE);
    gtk_widget_set_sensitive(spinbutton, FALSE);
  }
  
  row++;

  adj = gimp_scale_entry_new (GTK_TABLE (table), 0, row,
                              _("To:"), SCALE_WIDTH, 7,
                              mgpp->range_to, mgpp->range_from +1, ainfo_ptr->last_frame_nr, 1.0, 10.0, 0,
                              TRUE, 0, 0,
                              _("Last processed frame"), NULL);
  g_signal_connect (adj, "value-changed",
                    G_CALLBACK (gimp_int_adjustment_update),
                    &mgpp->range_to);
  if (isFrameMissing)
  {
    scale = g_object_get_data(G_OBJECT (adj), "scale");
    spinbutton = g_object_get_data(G_OBJECT (adj), "spinbutton");
    gtk_widget_set_sensitive(scale, FALSE);
    gtk_widget_set_sensitive(spinbutton, FALSE);
  }


  if (isFrameMissing)
  {
    row++;

    p_create_morph_workpoint_entry(mgpp, mtg, row, table);

    mgpp->master_tween_steps = 0;
    mgpp->create_tweens_in_subdir = FALSE;
  }
  else
  {
    row++;

    mgpp->create_tweens_in_subdir = TRUE;
    
    adj = gimp_scale_entry_new (GTK_TABLE (table), 0, row,
                                _("Number of Tweens:"), SCALE_WIDTH, 7,
                                mgpp->master_tween_steps, 0, 100, 1.0, 10.0, 0,
                                TRUE, 0, 0,
                                _("Number of tweens to be inserted between 2 frames. "
                                  "Value 0 renderes missing frames (via morphing or fade) "
                                  "but does not create tweens where the "
                                  "next frame number is equal to the current processed frame number +1"),
                                NULL);
    g_signal_connect (adj, "value-changed",
                      G_CALLBACK (gimp_int_adjustment_update),
                      &mgpp->master_tween_steps);

    row++;

    /* the create_tweens_in_subdir checkbutton */
//     checkbutton = gtk_check_button_new_with_label ( _("Subdirectory:"));
//     gtk_widget_show (checkbutton);
//     gtk_table_attach( GTK_TABLE(table), checkbutton, 0, 1, row, row+1,
//                       GTK_FILL, 0, 0, 0 );
//     g_signal_connect (checkbutton, "toggled",
//                       G_CALLBACK (on_gboolean_button_update),
//                       &mgpp->create_tweens_in_subdir);
//     gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (checkbutton), mgpp->create_tweens_in_subdir);
//     gimp_help_set_help_data(checkbutton,
//                          _("ON: copy processed frames to a subdirectory "
//                            "and create tween frames in this subdirectory via morphing."
//                            "OFF: Render missing frames via moprhing. ")
//                          , NULL);

    /* label */
    label = gtk_label_new (_("Subdirectory:"));
    gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
    gtk_table_attach (GTK_TABLE (table), label, 0, 1, row, row + 1,
                      GTK_FILL, GTK_FILL, 4, 0);
    gtk_widget_show (label);

    /* subdirectory entry */
    entry = gtk_entry_new();
    mtg->tween_subdir_entry = entry;
    gtk_widget_set_size_request(entry, SCALE_WIDTH, -1);
    if(mgpp->tween_subdir[0] != '\0')
    {
      gtk_entry_set_text(GTK_ENTRY(entry), &mgpp->tween_subdir[0]);
    }
    gtk_table_attach(GTK_TABLE(table), entry, 1, 2, row, row + 1, GTK_FILL, GTK_FILL | GTK_EXPAND, 4, 0);
    gimp_help_set_help_data(entry, _("Name of a (Sub)directoy to save copies of processed frames "
                                     "and generated tweens. "
                                     "Note that tweens are created via simple fade operations "
                                     "when no workpointfile for the processed frame is available. "
                                     "(individual workpointfiles per frame are refered by extension .morphpoints)")
                           , NULL);
    gtk_widget_show(entry);

    g_signal_connect(G_OBJECT(entry), "changed",
                     G_CALLBACK (p_tween_subdir_entry_update_cb),
                     mgpp);

    row++;

    /* the overwrite checkbutton */
    checkbutton = gtk_check_button_new_with_label ( _("Overwrite"));
    gtk_widget_show (checkbutton);
    gtk_table_attach( GTK_TABLE(table), checkbutton, 0, 1, row, row+1,
                      GTK_FILL, 0, 0, 0 );
    g_signal_connect (checkbutton, "toggled",
                      G_CALLBACK (on_gboolean_button_update),
                      &mgpp->overwrite_flag);
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (checkbutton), mgpp->overwrite_flag);
    gimp_help_set_help_data(checkbutton,
                         _("ON: overwrite existing frames."
                           "OFF: skip processing when target frame/tween already exists.")
                         , NULL);
  }


  row++;


  /* the master progress bar */

  /* master progress */
  label = gtk_label_new (_("Create Tweenfame(s):"));
  gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
  gtk_table_attach (GTK_TABLE (table), label, 0, 1, row, row + 1,
                    GTK_FILL, GTK_FILL, 4, 0);
  gtk_widget_show (label);

  /* the master progress bar */
  progressBar = gtk_progress_bar_new ();
  mtg->masterProgressBar = progressBar;
  gtk_table_attach (GTK_TABLE (table), progressBar, 1, 3, row, row + 1,
                    GTK_FILL, GTK_FILL, 4, 0);
  gtk_widget_show (progressBar);

  row++;

  label = gtk_label_new (_("Local Progress:"));
  gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
  gtk_table_attach (GTK_TABLE (table), label, 0, 1, row, row + 1,
                    GTK_FILL, GTK_FILL, 4, 0);
  gtk_widget_show (label);


  /* the progress bar */
  progressBar = gtk_progress_bar_new ();
  mtg->progressBar = progressBar;
  gtk_progress_bar_set_text(GTK_PROGRESS_BAR(progressBar), " ");
  gtk_widget_show (progressBar);
  gtk_table_attach( GTK_TABLE(table), progressBar, 1, 3, row, row+1,
                    GTK_FILL, 0, 0, 0 );
  
  /* Done */

  gtk_widget_show (dialog);

  gtk_main ();
  gdk_flush ();


  return (mtg->ret);

} /* end gap_morph_frame_tweens_dialog */
