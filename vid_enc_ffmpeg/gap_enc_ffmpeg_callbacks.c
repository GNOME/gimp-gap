/* gap_enc_ffmpeg_callbacks.c
 * 2004.05.12 hof (Wolfgang Hofer)
 *
 * GAP ... Gimp Animation Plugins
 *
 * This Module contains FFMPEG specific Video Encoder GUI Callback Procedures
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
 * version 2.1.0a;  2005.03.24   hof: added ffpar_fileselection.
 * version 2.1.0a;  2004.11.06   hof: use some general callbacks.
 *                                   (removed lots of similar callbacks
 *                                    that was needed for the old glade generated code)
 *                               - entry widgets do not use a common callback,
 *                                 because individual callbacks do know about
 *                                 the size of the destination stringbuffer.
 *
 * version 2.1.0a;  2004/11/05  hof: replaced deprecated option menu by gimp_int_combo_box
 * version 2.1.0a;  2004.05.12 : created
 */

#include <config.h>

#include <gtk/gtk.h>

#include "gap_enc_ffmpeg_main.h"
#include "gap_enc_ffmpeg_gui.h"
#include "gap_enc_ffmpeg_callbacks.h"
#include "gap_enc_ffmpeg_par.h"

/* Includes for encoder specific extra LIBS */
#include "avformat.h"
#include "avcodec.h"

#define GAP_FFPAR_FILE_KEY "gap_ffpar_file_key"

static void p_create_ffpar_fileselection (GapGveFFMpegGlobalParams *gpp, gboolean save_flag);
static void on_ffpar_fileselection_destroy (GtkObject *object, GapGveFFMpegGlobalParams *gpp);
static void on_ffpar_cancel_button_clicked (GtkButton *button, GapGveFFMpegGlobalParams *gpp);
static void on_ffpar_ok_button_clicked (GtkButton *button, GapGveFFMpegGlobalParams *gpp);


/* ---------------------------------
 * on_ff_response
 * ---------------------------------
 */
void
on_ff_response (GtkWidget *widget,
                 gint       response_id,
                 GapGveFFMpegGlobalParams *gpp)
{
  GtkWidget *dialog;
  char *l_msg;

  if(gpp)
  {
    gpp->val.run = FALSE;
  }

  switch (response_id)
  {
    case GAP_ENC_FFMPEG_RESPONSE_OPEN:
      if(gpp)
      {
        p_create_ffpar_fileselection (gpp, FALSE /* save_flag */ );
      }
      break;
    case GAP_ENC_FFMPEG_RESPONSE_SAVE:
      if(gpp)
      {
        p_create_ffpar_fileselection (gpp, TRUE /* save_flag */ );
      }
      break;
    case GTK_RESPONSE_OK:
      if(gpp)
      {
        if (gpp->val.vid_width % 2 != 0)
        {
            l_msg = g_strdup_printf("Error:\nWidth (%d) must be an integer multiple of 2",
                                 (int)gpp->val.vid_width);
           g_message("%s", l_msg);
           g_free(l_msg);
           return;
        }
        if (gpp->val.vid_height % 2 != 0)
        {
            l_msg = g_strdup_printf("Error:\nHeight (%d) must be an integer multiple of 2",
                               (int)gpp->val.vid_height);
            g_message("%s", l_msg);
            g_free(l_msg);
            return;
        }
        gpp->val.run = TRUE;
      }
      /* now run into the default case, to close the shell_window (dont break) */
    default:
      dialog = NULL;
      if(gpp)
      {
        dialog = gpp->shell_window;
        if(dialog)
        {
          gpp->shell_window = NULL;
          gtk_widget_destroy (dialog);
        }
      }
      gtk_main_quit ();
      break;
  }
}  /* end on_ff_response */


/* --------------------------------
 * on_ff_gint32_spinbutton_changed
 * --------------------------------
 */
void
on_ff_gint32_spinbutton_changed  (GtkWidget *widget,
                                  gint32    *dest_value_ptr)
{
  GtkAdjustment *adj;
  gint32         value;

  if(gap_debug)
  {
    printf("CB: on_ff_gint32_spinbutton_changed widget: %d\n", (int)widget);
  }
  if((dest_value_ptr == NULL) || (widget == NULL))
  { 
    return; 
  }
 
  adj =  (GtkAdjustment *) g_object_get_data (G_OBJECT (widget), GAP_ENC_FFGUI_ADJ);
  if(adj)
  {
    value = (gint32) gtk_adjustment_get_value(GTK_ADJUSTMENT(adj));
    
    if(gap_debug)
    {
      printf("spin value: %d\n", (int)value );
    }

    if(value != *dest_value_ptr)
    {
      *dest_value_ptr = value;
    }
  }
 
}  /* end on_ff_gint32_spinbutton_changed */

/* --------------------------------
 * on_ff_gdouble_spinbutton_changed
 * --------------------------------
 */
void
on_ff_gdouble_spinbutton_changed  (GtkWidget *widget,
                                   gdouble    *dest_value_ptr)
{
  GtkAdjustment *adj;
  gdouble        value;

  if(gap_debug)
  {
    printf("CB: on_ff_gdouble_spinbutton_changed widget: %d\n", (int)widget);
  }

  if((dest_value_ptr == NULL) || (widget == NULL))
  { 
    return; 
  }
 
  adj =  (GtkAdjustment *) g_object_get_data (G_OBJECT (widget), GAP_ENC_FFGUI_ADJ);
  if(adj)
  {
    value = (gdouble) gtk_adjustment_get_value(GTK_ADJUSTMENT(adj));
    
    if(gap_debug)
    {
      printf("spin value: %f\n", (float)value );
    }

    if(value != *dest_value_ptr)
    {
      *dest_value_ptr = value;
    }
  }
 
}  /* end on_ff_gdouble_spinbutton_changed */


/* ---------------------------------
 * gap_ffcb_set_widget_sensitivity
 * ---------------------------------
 * some of the ffmpeg checkbuttons need extra actions (set other widgets sensitiv)
 * or use other values for TRUE/FALSE representations
 */
void
gap_ffcb_set_widget_sensitivity  (GapGveFFMpegGlobalParams *gpp)
{
  gboolean l_sensitive;


  if(gpp == NULL)
  { 
    return; 
  }

  if(gpp->evl.intra)
  {
    l_sensitive = FALSE;
  }
  else
  {
    l_sensitive = TRUE;
  }
  if(gpp->ff_gop_size_spinbutton)
  {
    gtk_widget_set_sensitive(gpp->ff_gop_size_spinbutton, l_sensitive);
    gtk_widget_set_sensitive(gpp->ff_b_frames_spinbutton, l_sensitive);
  }
  
  
  if(gpp->evl.set_aspect_ratio)
  {
    l_sensitive = TRUE;
  }
  else
  {
    l_sensitive = FALSE;
  }
  if(gpp->ff_aspect_combo)
  {
    gtk_widget_set_sensitive(gpp->ff_aspect_combo, l_sensitive);
  }



  if(gpp->show_expert_settings)
  {
    l_sensitive = TRUE;
  }
  else
  {
    l_sensitive = FALSE;
  }
      
  if(gpp->main_notebook != NULL) 
  {
    gtk_notebook_set_show_tabs(gpp->main_notebook, l_sensitive);
  }
 

}  /* end gap_ffcb_set_widget_sensitivity */

/* ---------------------------------
 * on_ff_gint32_checkbutton_toggled
 * ---------------------------------
 * some of the ffmpeg checkbuttons need extra actions (set other widgets sensitiv)
 * or use other values for TRUE/FALSE representations
 */
void
on_ff_gint32_checkbutton_toggled  (GtkToggleButton *checkbutton,
                                   gint32          *dest_value_ptr)
{
  GapGveFFMpegGlobalParams *gpp;
  gboolean l_sensitive;

  if(gap_debug) printf("CB: on_ff_gdouble_spinbutton_changed widget: %d\n", (int)checkbutton);


  if((dest_value_ptr == NULL) || (checkbutton == NULL))
  { 
    return; 
  }

  if (checkbutton->active)
  {
     *dest_value_ptr = TRUE;
  }
  else
  {
     *dest_value_ptr = FALSE;
  }

  gpp = g_object_get_data (G_OBJECT (checkbutton), GAP_ENC_FFGUI_GPP);
  if(gpp)
  {
    if(((GtkWidget *)checkbutton ==  gpp->ff_intra_checkbutton)
    || ((GtkWidget *)checkbutton ==  gpp->ff_aspect_checkbutton)
    || ((GtkWidget *)checkbutton ==  gpp->show_expert_settings_checkbutton))
    {
      gap_ffcb_set_widget_sensitivity(gpp);
    }

    if((GtkWidget *)checkbutton ==  gpp->show_expert_settings_checkbutton)
    {
      gap_ffcb_set_widget_sensitivity(gpp);
      
      /* persist user decision (for next gimp session) */
      if(gpp->show_expert_settings)
      {
        gimp_gimprc_set(GAP_GVE_FFMPEG_SHOW_EXPERT_SETTINGS, "yes");
      }
      else
      {
        gimp_gimprc_set(GAP_GVE_FFMPEG_SHOW_EXPERT_SETTINGS, "no");
        gimp_int_combo_box_set_active (GIMP_INT_COMBO_BOX (gpp->ff_presets_combo)
                                     , GAP_GVE_FFMPEG_PRESET_00_NONE
                                     );
      }
    }


  }


}  /* end on_ff_gint32_checkbutton_toggled */

/* --- combos with string  results
 * --- accessing string via a list of strings
 * --- (the list root is fetched as gpointer using GAP_ENC_FFGUI_COMBO_STRLIST)
 */

/* --------------------------------
 * on_ff_fileformat_combo
 * --------------------------------
 */
void
on_ff_fileformat_combo  (GtkWidget     *widget,
                         GapGveFFMpegGlobalParams *gpp)
{
  const char *name;
  gpointer   string_combo_elem_list;
  gint       value;

  if(gap_debug) printf("CB: on_ff_fileformat_combo\n");

  if(gpp == NULL) return;

  name = NULL;

  gimp_int_combo_box_get_active (GIMP_INT_COMBO_BOX (widget), &value);
  string_combo_elem_list = (gpointer) g_object_get_data (G_OBJECT (widget)
                                    , GAP_ENC_FFGUI_COMBO_STRLIST);
  if(string_combo_elem_list)
  {
    /* get the internal string assotiated with the index value */
    name = gap_get_combo_string_by_idx(string_combo_elem_list, value);

    if(gap_debug) printf("FMT listroot: %d  name: %s  index:%d\n", (int)string_combo_elem_list, name, (int)value);

  }

  if(name)
  {
     if(strcmp(gpp->evl.format_name, name) != 0)
     {
       g_snprintf(gpp->evl.format_name, sizeof(gpp->evl.format_name), "%s", name);
       gap_enc_ffgui_set_default_codecs(gpp, TRUE);               /* update info labels AND set CODEC combos */
     }
  }
}  /* end on_ff_fileformat_combo */


/* --------------------------------
 * on_ff_vid_codec_combo
 * --------------------------------
 */
void
on_ff_vid_codec_combo  (GtkWidget     *widget,
                        GapGveFFMpegGlobalParams *gpp)
{
  const char *name;
  gpointer   string_combo_elem_list;
  gint       value;

  if(gap_debug) printf("CB: on_ff_vid_codec_combo\n");

  if(gpp == NULL) return;

  name = NULL;
 
  gimp_int_combo_box_get_active (GIMP_INT_COMBO_BOX (widget), &value);
  string_combo_elem_list = (gpointer) g_object_get_data (G_OBJECT (widget)
                                    , GAP_ENC_FFGUI_COMBO_STRLIST);
  if(string_combo_elem_list)
  {
    /* get the internal string assotiated with the index value */
    name = gap_get_combo_string_by_idx(string_combo_elem_list, value);

    if(gap_debug) printf("VCO listroot: %d  name: %s  index:%d\n", (int)string_combo_elem_list, name, (int)value);
  }
  
  if(name)
  {
     g_snprintf(gpp->evl.vcodec_name, sizeof(gpp->evl.vcodec_name), "%s", name);
  }
}  /* end on_ff_vid_codec_combo */


/* --------------------------------
 * on_ff_aud_codec_combo
 * --------------------------------
 */
void
on_ff_aud_codec_combo  (GtkWidget     *widget,
                        GapGveFFMpegGlobalParams *gpp)
{
  const char *name;
  gpointer   string_combo_elem_list;
  gint       value;

  if(gap_debug) printf("CB: on_ff_aud_codec_combo\n");

  if(gpp == NULL) return;

  name = NULL;

  gimp_int_combo_box_get_active (GIMP_INT_COMBO_BOX (widget), &value);
  string_combo_elem_list = (gpointer) g_object_get_data (G_OBJECT (widget)
                                    , GAP_ENC_FFGUI_COMBO_STRLIST);
  if(string_combo_elem_list)
  {
    /* get the internal string assotiated with the index value */
    name = gap_get_combo_string_by_idx(string_combo_elem_list, value);

    if(gap_debug) printf("ACO listroot: %d  name: %s  index:%d\n", (int)string_combo_elem_list, name, (int)value);
  }

  if(name)
  {
     g_snprintf(gpp->evl.acodec_name, sizeof(gpp->evl.acodec_name), "%s", name);
  }
}  /* end on_ff_aud_codec_combo */



/* --- combos with gint32 results
 */



/* --------------------------------
 * on_ff_aud_bitrate_combo
 * --------------------------------
 */
void
on_ff_aud_bitrate_combo  (GtkWidget     *widget,
                          GapGveFFMpegGlobalParams *gpp)
{
  gint       l_idx;
  gint       l_krate;
  gint       value;


  if(gap_debug) printf("CB: on_ff_aud_bitrate_combo\n");

  if(gpp == NULL) return;

  gimp_int_combo_box_get_active (GIMP_INT_COMBO_BOX (widget), &value);

  l_idx = value;

  if(gap_debug) printf("CB: on_ff_aud_bitrate_combo index: %d\n", (int)l_idx);

  l_krate = gap_enc_ffgui_gettab_audio_krate(l_idx);
  if(gpp->evl.audio_bitrate != l_krate)
  {
     gpp->evl.audio_bitrate = l_krate;
     gap_enc_ffgui_init_main_dialog_widgets(gpp);     /* for update audio_bitrate spinbutton */
  }
}  /* end on_ff_aud_bitrate_combo */


/* --------------------------------
 * on_ff_gint32_combo
 * --------------------------------
 */
void
on_ff_gint32_combo  (GtkWidget     *widget,
                     gint32 *val_ptr)
{
  gint       value;

  if(gap_debug) 
  {
    printf("CB: on_ff_gint32_combo\n");
  }

  if(val_ptr == NULL) return;

  gimp_int_combo_box_get_active (GIMP_INT_COMBO_BOX (widget), &value);
  *val_ptr = value;

  if(gap_debug)
  { 
    printf("CB: on_ff_gint32_combo index: val_ptr: %d %d\n"
           , (int)val_ptr
           , (int)value
           );
  }
}  /* end on_ff_gint32_combo */



/* --------------------------------
 * on_ff_presets_combo
 * --------------------------------
 */
void
on_ff_presets_combo  (GtkWidget     *widget,
                      GapGveFFMpegGlobalParams *gpp)
{
  gint       l_idx;
  gint       value;

  if(gap_debug)
  {
    printf("CB: on_ff_presets_combo\n");
  }

  if(gpp == NULL) return;

  gimp_int_combo_box_get_active (GIMP_INT_COMBO_BOX (widget), &value);
  l_idx = value;

  if(gap_debug)
  {
    printf("CB: on_ff_presets_combo index: %d\n", (int)l_idx);
  }

  if(l_idx > 0)
  {
    /* index 0 is used for OOPS do not use preset menu entry and is not a PRESET
     */
    l_idx;

    if(gap_debug)
    {
      printf ("** encoder PRESET: %d\n", (int)l_idx);
    }
    gap_enc_ffmpeg_main_init_preset_params(&gpp->evl, l_idx);
    gap_enc_ffgui_init_main_dialog_widgets(gpp);                /* update all wdgets */
  
    /* switch back to index 0 (OOPS, do not change presets)
     * after presets were loaded.
     * NOTE this switchback workaround is not needed in case
     * the expert flags are all hidden where the user can not change
     * single values in the current preset.
     * therefore it does not matter when selecting the same preset again
     * does not trigger reset all values to the the values of the preset.
     */
    if (gpp->show_expert_settings)
    {
      gimp_int_combo_box_set_active (GIMP_INT_COMBO_BOX (widget), GAP_GVE_FFMPEG_PRESET_00_NONE);
    }
  }

}  /* end on_ff_presets_combo */





/* --------------------------------
 * on_ff_aspect_combo
 * --------------------------------
 */
void
on_ff_aspect_combo  (GtkWidget     *widget,
                     GapGveFFMpegGlobalParams *gpp)
{
  gint       l_idx;
  gint       value;

  if(gap_debug) printf("CB: on_ff_aspect_combo\n");

  if(gpp == NULL) return;

  gimp_int_combo_box_get_active (GIMP_INT_COMBO_BOX (widget), &value);
  l_idx = value;

  if(gap_debug) printf("CB: on_ff_aspect_combo index: %d\n", (int)l_idx);
  gpp->evl.factor_aspect_ratio = gap_enc_ffgui_gettab_aspect(l_idx);
}  /* end on_ff_aspect_combo */




/* ---------------------------------
 * fsb fileselct  callbacks
 * ---------------------------------
 */


void
on_fsb__fileselection_destroy          (GtkObject       *object,
                                        GapGveFFMpegGlobalParams *gpp)
{
 if(gap_debug) printf("CB: on_fsb__fileselection_destroy\n");

 if(gpp == NULL) return;

 gpp->fsb__fileselection = NULL;
}


void
on_fsb__ok_button_clicked              (GtkButton       *button,
                                        GapGveFFMpegGlobalParams *gpp)
{
 const gchar *filename;
 GtkEntry *entry;

 if(gap_debug) printf("CB: on_fsb__ok_button_clicked\n");
 if(gpp == NULL) return;

 if(gpp->fsb__fileselection)
 {
   filename = gtk_file_selection_get_filename (GTK_FILE_SELECTION (gpp->fsb__fileselection));
   g_snprintf(gpp->evl.passlogfile, sizeof(gpp->evl.passlogfile), "%s"
             , filename);
   entry = GTK_ENTRY(gpp->ff_passlogfile_entry);
   if(entry)
   {
        gtk_entry_set_text(entry, filename);
   }
   on_fsb__cancel_button_clicked(NULL, (gpointer)gpp);
 }
}


void
on_fsb__cancel_button_clicked          (GtkButton       *button,
                                        GapGveFFMpegGlobalParams *gpp)
{
 if(gap_debug) printf("CB: on_fsb__cancel_button_clicked\n");
 if(gpp == NULL) return;

 if(gpp->fsb__fileselection)
 {
   gtk_widget_destroy(gpp->fsb__fileselection);
   gpp->fsb__fileselection = NULL;
 }
}



/* ---------------------------------
 * ffpar fileselct  callbacks
 * ---------------------------------
 */

static void
on_ffpar_fileselection_destroy          (GtkObject       *object,
                                        GapGveFFMpegGlobalParams *gpp)
{
 if(gap_debug) 
 {
   printf("CB: on_ffpar_fileselection_destroy\n");
 }

 if(gpp == NULL) return;

 gpp->ffpar_fileselection = NULL;
}


static void
on_ffpar_cancel_button_clicked          (GtkButton       *button,
                                        GapGveFFMpegGlobalParams *gpp)
{
 if(gap_debug) printf("CB: on_ffpar_cancel_button_clicked\n");
 if(gpp == NULL) return;

 if(gpp->ffpar_fileselection)
 {
   gtk_widget_destroy(gpp->ffpar_fileselection);
   gpp->ffpar_fileselection = NULL;
 }
}

static void
on_ffpar_ok_button_clicked              (GtkButton       *button,
                                        GapGveFFMpegGlobalParams *gpp)
{
 const gchar *filename;

 if(gap_debug) printf("CB: on_ffpar_ok_button_clicked\n");
 if(gpp == NULL) return;

 if(gpp->ffpar_fileselection)
 {
   filename = gtk_file_selection_get_filename (GTK_FILE_SELECTION (gpp->ffpar_fileselection));
   g_snprintf(gpp->ffpar_filename, sizeof(gpp->ffpar_filename), "%s"
             , filename);
   on_ffpar_cancel_button_clicked(NULL, (gpointer)gpp);
   
   gimp_set_data(GAP_FFPAR_FILE_KEY
                ,&gpp->ffpar_filename[0]
                ,sizeof(gpp->ffpar_filename)
                );
   
   if(gpp->ffpar_save_flag)
   {
     gboolean write_permission;
     
     write_permission = gap_arr_overwrite_file_dialog(filename);
     if(write_permission)
     {
       gap_ffpar_set(filename, &gpp->evl);
     }
   }
   else
   {
     gap_ffpar_get(filename, &gpp->evl);
     gap_enc_ffgui_init_main_dialog_widgets(gpp);                /* update all wdgets */
     /* switch back to index 0 (OOPS, do not change presets)
      * after presets were loaded.
      */
     gimp_int_combo_box_set_active (GIMP_INT_COMBO_BOX (gpp->ff_presets_combo), GAP_GVE_FFMPEG_PRESET_00_NONE);
   }
 }
}

 

/* -----------------------------------------
 * p_create_ffpar_fileselection
 * -----------------------------------------
 */
static void
p_create_ffpar_fileselection (GapGveFFMpegGlobalParams *gpp, gboolean save_flag)
{
  GtkWidget *ffpar_fileselection;
  GtkWidget *ffpar_ok_button;
  GtkWidget *ffpar_cancel_button;

  if(gpp == NULL)
  {
    return;
  }

  if(gpp->ffpar_fileselection)
  {
    return;
  }

  if(save_flag)
  {
    ffpar_fileselection = gtk_file_selection_new (_("Save ffmpeg-encoder parameters"));
  }
  else
  {
    ffpar_fileselection = gtk_file_selection_new (_("Load ffmpeg-encoder parameters"));
  }
  
  gpp->ffpar_fileselection = ffpar_fileselection;
  gpp->ffpar_save_flag = save_flag;
  gtk_container_set_border_width (GTK_CONTAINER (ffpar_fileselection), 10);

  /* get previously (in this session) used ffpar_filename */
  if(gimp_get_data_size(GAP_FFPAR_FILE_KEY) == sizeof(gpp->ffpar_filename))
  {
    gimp_get_data(GAP_FFPAR_FILE_KEY, &gpp->ffpar_filename[0]);
  }

  if(gpp->ffpar_filename[0] != '\0')
  {
    gtk_file_selection_set_filename (GTK_FILE_SELECTION (ffpar_fileselection),
                                   gpp->ffpar_filename);
  }

  ffpar_ok_button = GTK_FILE_SELECTION (ffpar_fileselection)->ok_button;
  gtk_widget_show (ffpar_ok_button);
  GTK_WIDGET_SET_FLAGS (ffpar_ok_button, GTK_CAN_DEFAULT);

  ffpar_cancel_button = GTK_FILE_SELECTION (ffpar_fileselection)->cancel_button;
  gtk_widget_show (ffpar_cancel_button);
  GTK_WIDGET_SET_FLAGS (ffpar_cancel_button, GTK_CAN_DEFAULT);

  g_signal_connect (G_OBJECT (ffpar_fileselection), "destroy",
                    G_CALLBACK (on_ffpar_fileselection_destroy),
                    gpp);
  g_signal_connect (G_OBJECT (ffpar_ok_button), "clicked",
                    G_CALLBACK (on_ffpar_ok_button_clicked),
                    gpp);

  g_signal_connect (G_OBJECT (ffpar_cancel_button), "clicked",
                    G_CALLBACK (on_ffpar_cancel_button_clicked),
                    gpp);

  gtk_widget_show(ffpar_fileselection);
  
}  /* end p_create_ffpar_fileselection */





/* ---------------------------------
 * FFMPEG ENCODER Dialog
 * ---------------------------------
 */


/* ---------------------------------
 * Other Widgets
 * ---------------------------------
 */



void
on_ff_passlogfile_entry_changed        (GtkEditable     *editable,
                                        GapGveFFMpegGlobalParams *gpp)
{
 if(gap_debug) printf("CB: on_ff_passlogfile_entry_changed\n");

 if(gpp)
 {
    g_snprintf(gpp->evl.passlogfile, sizeof(gpp->evl.passlogfile), "%s"
              , gtk_entry_get_text(GTK_ENTRY(editable)));
 }
}


void
on_ff_passlogfile_filesel_button_clicked
                                        (GtkButton       *button,
                                        GapGveFFMpegGlobalParams *gpp)
{
 if(gap_debug) printf("CB: on_ff_passlogfile_filesel_button_clicked\n");

 if(gpp)
 {
   if(gpp->fsb__fileselection != NULL)
   {
     gtk_window_present(GTK_WINDOW(gpp->fsb__fileselection));
     return;
   }

   gpp->fsb__fileselection = gap_enc_ffgui_create_fsb__fileselection(gpp);
   if(gpp->fsb__fileselection)
   {
     gtk_widget_show (gpp->fsb__fileselection);
   }
 }
}



void
on_ff_title_entry_changed              (GtkEditable     *editable,
                                        GapGveFFMpegGlobalParams *gpp)
{
 if(gap_debug) printf("CB: on_ff_title_entry_changed\n");

 if(gpp)
 {
    g_snprintf(gpp->evl.title, sizeof(gpp->evl.title), "%s"
              , gtk_entry_get_text(GTK_ENTRY(editable)));
 }
}


void
on_ff_author_entry_changed             (GtkEditable     *editable,
                                        GapGveFFMpegGlobalParams *gpp)
{
 if(gap_debug) printf("CB: on_ff_author_entry_changed\n");

 if(gpp)
 {
    g_snprintf(gpp->evl.author, sizeof(gpp->evl.author), "%s"
              , gtk_entry_get_text(GTK_ENTRY(editable)));
 }
}


void
on_ff_copyright_entry_changed          (GtkEditable     *editable,
                                        GapGveFFMpegGlobalParams *gpp)
{
 if(gap_debug) printf("CB: on_ff_copyright_entry_changed\n");

 if(gpp)
 {
    g_snprintf(gpp->evl.copyright, sizeof(gpp->evl.copyright), "%s"
              , gtk_entry_get_text(GTK_ENTRY(editable)));
 }
}


void
on_ff_comment_entry_changed            (GtkEditable     *editable,
                                        GapGveFFMpegGlobalParams *gpp)
{
 if(gap_debug) printf("CB: on_ff_comment_entry_changed\n");

 if(gpp)
 {
    g_snprintf(gpp->evl.comment, sizeof(gpp->evl.comment), "%s"
              , gtk_entry_get_text(GTK_ENTRY(editable)));
 }

}

