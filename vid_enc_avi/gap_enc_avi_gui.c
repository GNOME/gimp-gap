/* gap_enc_avi_gui.c
 * 2004.06.12 hof (Wolfgang Hofer)
 *
 * GAP ... Gimp Animation Plugins
 *
 * This Module contains AVI specific Video Encoder GUI Procedures
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
 * version 2.1.0a;  2004.06.12   hof: created
 */

#include <config.h>

#include <sys/types.h>

#include <string.h>

#include <gdk/gdkkeysyms.h>
#include <gtk/gtk.h>
#include <libgimp/gimp.h>
#include <libgimp/gimpui.h>



#include "gap_libgapvidutil.h"
#include "gap-intl.h"

#include "gap_libgapvidutil.h"
#include "gap_libgimpgap.h"

#include "gap_enc_avi_main.h"
#include "gap_enc_avi_gui.h"

/* Includes for encoder specific extra LIBS */
#include "avilib.h"

#define GAP_ENC_AVI_RESPONSE_RESET 1

static const char *gtab_avi_codecname[GAP_AVI_VIDCODEC_MAX_ELEMENTS]
  = { GAP_AVI_CODEC_JPEG
    , GAP_AVI_CODEC_MJPG
    , GAP_AVI_CODEC_PNG
    , GAP_AVI_CODEC_RAW
    , GAP_AVI_CODEC_XVID
    };


static void
on_avi_response (GtkWidget *widget,
                 gint       response_id,
                 GapGveAviGlobalParams *gpp);
static void p_init_widget_values(GapGveAviGlobalParams *gpp);
static void on_combo_video_codec  (GtkWidget     *widget, GapGveAviGlobalParams *gpp);
static void on_checkbutton_toggled(GtkToggleButton *togglebutton, gint32 *val_ptr);
static void on_gint32_spinbutton_changed (GtkAdjustment *adj, gint32 *val_ptr);

static void p_set_codec_dependent_wgt_senistive(GapGveAviGlobalParams *gpp, gint32 idx);


/* ---------------------------------
 * on_avi_response
 * ---------------------------------
 */
static void
on_avi_response (GtkWidget *widget,
                 gint       response_id,
                 GapGveAviGlobalParams *gpp)
{
  GtkWidget *dialog;

  if(gpp)
  {
    gpp->val.run = FALSE;
  }

  switch (response_id)
  {
    case GAP_ENC_AVI_RESPONSE_RESET:
      /* rset default values */
      gap_enc_avi_main_init_default_params(&gpp->evl);
      p_init_widget_values(gpp);
      break;
    case GTK_RESPONSE_OK:
      if(gpp)
      {
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
}  /* end on_avi_response */

/* ----------------------------------------
 * p_get_initial_video_codec_idx
 * ----------------------------------------
 */
static gint
p_get_initial_video_codec_idx(GapGveAviGlobalParams *gpp)
{
  GapGveAviValues *epp;
  gint l_idx;

  epp = &gpp->evl;

  for(l_idx=0;l_idx < GAP_AVI_VIDCODEC_MAX_ELEMENTS; l_idx++)
  {
    if(strcmp(gtab_avi_codecname[l_idx], epp->codec_name) == 0)
    {
      return(l_idx);
    }
  }
  return (0);

}  /* end p_get_initial_video_codec_idx */

/* ----------------------------------------
 * p_init_widget_values
 * ----------------------------------------
 */
static void
p_init_widget_values(GapGveAviGlobalParams *gpp)
{
  GapGveAviValues *epp;

  epp = &gpp->evl;
  if(epp == NULL)
  {
    return;
  }

  /* init spnbuttons */
  gtk_adjustment_set_value(GTK_ADJUSTMENT(gpp->jpg_quality_spinbutton_adj)
                         , (gfloat)epp->jpeg_quality);

  gpp->xvid_kbitrate = epp->xvid.rc_bitrate / 1000;

  gtk_adjustment_set_value(GTK_ADJUSTMENT(gpp->xvid_rc_kbitrate_spinbutton_adj)
                         , (gfloat)gpp->xvid_kbitrate);
  gtk_adjustment_set_value(GTK_ADJUSTMENT(gpp->xvid_rc_reaction_delay_spinbutton_adj)
                         , (gfloat)epp->xvid.rc_reaction_delay_factor);
  gtk_adjustment_set_value(GTK_ADJUSTMENT(gpp->xvid_rc_avg_period_spinbutton_adj)
                         , (gfloat)epp->xvid.rc_averaging_period);
  gtk_adjustment_set_value(GTK_ADJUSTMENT(gpp->xvid_rc_buffer_spinbutton_adj)
                         , (gfloat)epp->xvid.rc_buffer);
  gtk_adjustment_set_value(GTK_ADJUSTMENT(gpp->xvid_max_quantizer_spinbutton_adj)
                         , (gfloat)epp->xvid.max_quantizer);
  gtk_adjustment_set_value(GTK_ADJUSTMENT(gpp->xvid_min_quantizer_spinbutton_adj)
                         , (gfloat)epp->xvid.min_quantizer);
  gtk_adjustment_set_value(GTK_ADJUSTMENT(gpp->xvid_max_key_interval_spinbutton_adj)
                         , (gfloat)epp->xvid.max_key_interval);
  gtk_adjustment_set_value(GTK_ADJUSTMENT(gpp->xvid_quality_spinbutton_adj)
                         , (gfloat)epp->xvid.quality_preset);


  /* init checkbuttons */
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (gpp->jpg_dont_recode_checkbutton)
                               , epp->jpeg_dont_recode_frames);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (gpp->jpg_interlace_checkbutton)
                               , epp->jpeg_interlaced);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (gpp->jpg_odd_first_checkbutton)
                               , epp->jpeg_odd_even);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (gpp->app0_checkbutton)
                               , epp->APP0_marker);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (gpp->raw_vflip_checkbutton)
                               , epp->raw_vflip);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (gpp->raw_bgr_checkbutton)
                               , epp->raw_bgr);

  /* set initial value according to codec */
  {
    gint l_idx;
    l_idx = p_get_initial_video_codec_idx(gpp);
    gimp_int_combo_box_set_active (GIMP_INT_COMBO_BOX (gpp->combo_codec), l_idx);
    p_set_codec_dependent_wgt_senistive(gpp, l_idx);
  }
}  /* end p_init_widget_values */



/* ----------------------------------------
 * p_set_codec_dependent_wgt_senistive
 * ----------------------------------------
 * idx is the video codec index
 * and is corresponding with the notebook tab
 */
static void
p_set_codec_dependent_wgt_senistive(GapGveAviGlobalParams *gpp, gint32 idx)
{
  gboolean png_sensitive;
  gboolean jpeg_sensitive;
  gboolean xvid_sensitive;
  gboolean raw_sensitive;
  gint notebook_page_idx;

  png_sensitive = TRUE;
  jpeg_sensitive = TRUE;
  xvid_sensitive = TRUE;

  switch(idx)
  {
    case GAP_AVI_VIDCODEC_00_JPEG:
    case GAP_AVI_VIDCODEC_01_MJPG:
      notebook_page_idx = GAP_AVI_VIDCODEC_00_JPEG;
      png_sensitive = FALSE;
      jpeg_sensitive = TRUE;
      xvid_sensitive = FALSE;
      raw_sensitive = FALSE;
      break;
    case GAP_AVI_VIDCODEC_02_PNG:
      notebook_page_idx = idx -1;
      png_sensitive = TRUE;
      jpeg_sensitive = FALSE;
      xvid_sensitive = FALSE;
      raw_sensitive = FALSE;
      break;
    case GAP_AVI_VIDCODEC_03_RAW:
      notebook_page_idx = idx -1;
      png_sensitive = FALSE;
      jpeg_sensitive = FALSE;
      xvid_sensitive = FALSE;
      raw_sensitive = TRUE;
      break;
    case GAP_AVI_VIDCODEC_04_XVID:
      notebook_page_idx = idx -1;
      png_sensitive = FALSE;
      jpeg_sensitive = FALSE;
      xvid_sensitive = TRUE;
      raw_sensitive = FALSE;
      break;
    default:
      notebook_page_idx = GAP_AVI_VIDCODEC_00_JPEG;
      png_sensitive = FALSE;
      jpeg_sensitive = TRUE;
      xvid_sensitive = FALSE;
      raw_sensitive = FALSE;
      break;
  }

  if(gpp->notebook_main)
    gtk_notebook_set_current_page(GTK_NOTEBOOK(gpp->notebook_main), notebook_page_idx);

  if(gpp->jpg_dont_recode_checkbutton)
    gtk_widget_set_sensitive(gpp->jpg_dont_recode_checkbutton, jpeg_sensitive);
  if(gpp->jpg_interlace_checkbutton)
    gtk_widget_set_sensitive(gpp->jpg_interlace_checkbutton,   jpeg_sensitive);
  if(gpp->jpg_odd_first_checkbutton)
    gtk_widget_set_sensitive(gpp->jpg_odd_first_checkbutton,   (jpeg_sensitive
                                                             && gpp->evl.jpeg_interlaced) );
  if(gpp->jpg_quality_spinbutton)
    gtk_widget_set_sensitive(gpp->jpg_quality_spinbutton,      jpeg_sensitive);

  if(gpp->xvid_rc_kbitrate_spinbutton)
    gtk_widget_set_sensitive(gpp->xvid_rc_kbitrate_spinbutton,       xvid_sensitive);
  if(gpp->xvid_rc_reaction_delay_spinbutton)
    gtk_widget_set_sensitive(gpp->xvid_rc_reaction_delay_spinbutton, xvid_sensitive);
  if(gpp->xvid_rc_avg_period_spinbutton)
    gtk_widget_set_sensitive(gpp->xvid_rc_avg_period_spinbutton,     xvid_sensitive);
  if(gpp->xvid_rc_buffer_spinbutton)
    gtk_widget_set_sensitive(gpp->xvid_rc_buffer_spinbutton,         xvid_sensitive);
  if(gpp->xvid_max_quantizer_spinbutton)
    gtk_widget_set_sensitive(gpp->xvid_max_quantizer_spinbutton,     xvid_sensitive);
  if(gpp->xvid_min_quantizer_spinbutton)
    gtk_widget_set_sensitive(gpp->xvid_min_quantizer_spinbutton,     xvid_sensitive);
  if(gpp->xvid_max_key_interval_spinbutton)
    gtk_widget_set_sensitive(gpp->xvid_max_key_interval_spinbutton,  xvid_sensitive);
  if(gpp->xvid_quality_spinbutton)
    gtk_widget_set_sensitive(gpp->xvid_quality_spinbutton,           xvid_sensitive);

  if(gpp->png_dont_recode_checkbutton)
    gtk_widget_set_sensitive(gpp->png_dont_recode_checkbutton, png_sensitive);
  if(gpp->png_interlace_checkbutton)
    gtk_widget_set_sensitive(gpp->png_interlace_checkbutton,   png_sensitive);
  if(gpp->png_compression_spinbutton)
    gtk_widget_set_sensitive(gpp->png_compression_spinbutton,      png_sensitive);

  if(gpp->raw_vflip_checkbutton)
    gtk_widget_set_sensitive(gpp->raw_vflip_checkbutton, raw_sensitive);
  if(gpp->raw_bgr_checkbutton)
    gtk_widget_set_sensitive(gpp->raw_bgr_checkbutton, raw_sensitive);

}  /* end p_set_codec_dependent_wgt_senistive  */


/* ----------------------------------------
 * on_combo_video_codec
 * ----------------------------------------
 */
static void
on_combo_video_codec  (GtkWidget     *widget,
                       GapGveAviGlobalParams *gpp)
{
  gint       l_idx;
  gint       value;

  if(gap_debug) printf("CB: on_combo_video_codec\n");

  if(gpp == NULL) return;

  gimp_int_combo_box_get_active (GIMP_INT_COMBO_BOX (widget), &value);
  l_idx = value;

  if(gap_debug) printf("CB: on_combo_video_codec index: %d\n", (int)l_idx);
  if(l_idx < 1)
  {
    l_idx = 0;
  }
  if(l_idx >= GAP_AVI_VIDCODEC_MAX_ELEMENTS)
  {
    l_idx = GAP_AVI_VIDCODEC_MAX_ELEMENTS -1;
  }

  g_snprintf(gpp->evl.codec_name, sizeof(gpp->evl.codec_name)
           , "%s"
           , gtab_avi_codecname[l_idx]);

  p_set_codec_dependent_wgt_senistive(gpp, l_idx);


}  /* end on_combo_video_codec */


/* ----------------------------------------
 * on_checkbutton_toggled
 * ----------------------------------------
 */
static void
on_checkbutton_toggled (GtkToggleButton *togglebutton
                       , gint32 *val_ptr)
{
 GapGveAviGlobalParams *gpp;

 if(gap_debug) printf("CB: on_checkbutton_toggled: %d\n", (int)togglebutton);

 if(val_ptr)
 {
    if (togglebutton->active)
    {
       *val_ptr = 1; // TRUE;
    }
    else
    {
       *val_ptr = 0; // FALSE;
    }
 }

 gpp = (GapGveAviGlobalParams *)g_object_get_data(G_OBJECT(togglebutton), "gpp");
 if(gpp)
 {
    if(gpp->evl.jpeg_interlaced)
    {
      gtk_widget_set_sensitive(gpp->jpg_odd_first_checkbutton, TRUE);
    }
    else
    {
      gtk_widget_set_sensitive(gpp->jpg_odd_first_checkbutton, FALSE);
    }
 }
}  /* end on_checkbutton_toggled */

/* ----------------------------------------
 * on_gint32_spinbutton_changed
 * ----------------------------------------
 */
static void
on_gint32_spinbutton_changed       (GtkAdjustment *adj,
                                    gint32 *val_ptr)
{
 //GapGveAviGlobalParams *gpp;

 if(gap_debug) printf("CB: on_gint32_spinbutton_changed\n");

 if((adj) && (val_ptr))
 {
   if(gap_debug) printf("spin value: %.2f\n", (float)adj->value );

   if((gint32)adj->value != *val_ptr)
   {
     *val_ptr = (gint32)adj->value;
   }
 }

 // gpp = (GapGveAviGlobalParams *)g_object_get_data(adj, "gpp");
 // if(gpp)
 // {
 // }

}  /* end on_gint32_spinbutton_changed */


/* ----------------------------------------
 * p_create_shell_window
 * ----------------------------------------
 */
static GtkWidget*
p_create_shell_window (GapGveAviGlobalParams *gpp)
{
  GapGveAviValues *epp;
  gint      master_row;
  gint      jpg_row;
  gint      png_row;
  gint      xvid_row;
  gint      raw_row;
  GtkWidget *shell_window;
  GtkWidget *vbox1;
  GtkWidget *frame_main;
  GtkWidget *notebook_main;
  GtkWidget *frame_jpg;
  GtkWidget *frame_png;
  GtkWidget *frame_xvid;
  GtkWidget *frame_raw;
  GtkWidget *label;
  GtkWidget *table_master;
  GtkWidget *table_jpg;
  GtkWidget *table_png;
  GtkWidget *table_xvid;
  GtkWidget *table_raw;
  GtkWidget *checkbutton;
  GtkWidget *spinbutton;
  GtkObject *adj;
  GtkWidget *combo_codec;
  gint       notebook_page_idx;

  epp = &gpp->evl;


  shell_window = gimp_dialog_new (_("AVI Video Encode Parameters"),
                         GAP_PLUGIN_NAME_AVI_PARAMS,
                         NULL, 0,
                         gimp_standard_help_func, GAP_HELP_ID_AVI_PARAMS,

                         GIMP_STOCK_RESET, GAP_ENC_AVI_RESPONSE_RESET,
                         GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                         GTK_STOCK_OK,     GTK_RESPONSE_OK,
                         NULL);
  gtk_window_set_type_hint (shell_window, GDK_WINDOW_TYPE_HINT_NORMAL);

  g_signal_connect (G_OBJECT (shell_window), "response",
                    G_CALLBACK (on_avi_response),
                    gpp);


  vbox1 = GTK_DIALOG (shell_window)->vbox;
  gtk_widget_show (vbox1);

  frame_main = gimp_frame_new (NULL);
  gtk_widget_show (frame_main);
  gtk_box_pack_start (GTK_BOX (vbox1), frame_main, TRUE, TRUE, 0);


  /* the master_table for all parameter widgets */
  table_master = gtk_table_new (5, 2, FALSE);
  gtk_widget_show (table_master);
  gtk_container_add (GTK_CONTAINER (frame_main), table_master);
  gtk_container_set_border_width (GTK_CONTAINER (frame_main), 4);

  master_row = 0;

  /* the Video CODEC label */
  label = gtk_label_new (_("Video CODEC:"));
  gtk_widget_show (label);
  gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
  gtk_table_attach (GTK_TABLE (table_master), label, 0, 1, master_row, master_row+1,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);


  /* the Video CODEC combo */
  combo_codec = gimp_int_combo_box_new ("JPEG",   GAP_AVI_VIDCODEC_00_JPEG,
                                        "MJPG",   GAP_AVI_VIDCODEC_01_MJPG,
                                        "PNG",    GAP_AVI_VIDCODEC_02_PNG,
                                        "RAW",    GAP_AVI_VIDCODEC_03_RAW,
#ifdef ENABLE_LIBXVIDCORE
                                        "XVID",   GAP_AVI_VIDCODEC_04_XVID,
#endif
                                     NULL);

  gimp_int_combo_box_connect (GIMP_INT_COMBO_BOX (combo_codec),
                             p_get_initial_video_codec_idx(gpp),  /* inital value */
                             G_CALLBACK (on_combo_video_codec),
                             gpp);

  gpp->combo_codec = combo_codec;
  gtk_widget_show (combo_codec);
  gtk_table_attach (GTK_TABLE (table_master), combo_codec, 1, 2, master_row, master_row+1,
                    (GtkAttachOptions) (0),
                    (GtkAttachOptions) (0), 0, 0);
  gtk_widget_set_size_request (combo_codec, 160, -1);
  gimp_help_set_help_data (combo_codec, _("Select video codec"), NULL);


  master_row++;

  /* the Audio CODEC label */
  label = gtk_label_new (_("Audio CODEC:"));
  gtk_widget_show (label);
  gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
  gtk_table_attach (GTK_TABLE (table_master), label, 0, 1, master_row, master_row+1,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);

  /* the Audio CODEC label */
  label = gtk_label_new (_("RAW PCM"));
  gtk_widget_show (label);
  gtk_label_set_justify (GTK_LABEL (label), GTK_JUSTIFY_LEFT);
  gtk_table_attach (GTK_TABLE (table_master), label, 1, 2, master_row, master_row+1,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);

  master_row++;

  /* the APP0 Marker label */
  label = gtk_label_new (_("APP0 Marker:"));
  gtk_widget_show (label);
  gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
  gtk_table_attach (GTK_TABLE (table_master), label, 0, 1, master_row, master_row+1,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);

  /* the APP0 Marker checkbutton */
  checkbutton = gtk_check_button_new_with_label (" ");
  gpp->app0_checkbutton = checkbutton;
  gtk_widget_show (checkbutton);
  gtk_table_attach (GTK_TABLE (table_master), checkbutton, 1, 2, master_row, master_row+1,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);
        g_object_set_data (G_OBJECT (checkbutton), "gpp"
                          , (gpointer)gpp);
        g_signal_connect (G_OBJECT (checkbutton), "toggled"
                   , G_CALLBACK (on_checkbutton_toggled)
                   , &epp->APP0_marker);
  gimp_help_set_help_data (checkbutton
                   , _("Write APP0 Marker for each encoded frame."
                       " The APP0 marker is evaluated by some windows programs for AVIs"
                       " but can cause playback with wrong colors on some players"
                       " (in most cases you should NOT write the APP0 marker)")
                   , NULL);

  master_row++;

  /* the notebook widget */
  notebook_main = gtk_notebook_new ();
  gpp->notebook_main = notebook_main;
  gtk_widget_show (notebook_main);
  gtk_table_attach (GTK_TABLE (table_master), notebook_main, 0, 2, master_row, master_row+1,
                    (GtkAttachOptions) (0),
                    (GtkAttachOptions) (0), 0, 0);


  /* the notebook page for JPEG Codec options */
  /* ----------------------------------------- */
  notebook_page_idx = 0;
  frame_jpg = gimp_frame_new (_("JPEG / MJPG Codec Options"));
  gtk_widget_show (frame_jpg);
  gtk_container_add (GTK_CONTAINER (notebook_main), frame_jpg);
  gtk_container_set_border_width (GTK_CONTAINER (frame_jpg), 4);

  label = gtk_label_new (_("JPEG Options"));
  gtk_widget_show (label);
  gtk_notebook_set_tab_label (GTK_NOTEBOOK (notebook_main)
      , gtk_notebook_get_nth_page (GTK_NOTEBOOK (notebook_main), notebook_page_idx), label);


  /* the table for the JPEG option widgets */
  table_jpg = gtk_table_new (4, 3, FALSE);
  gtk_widget_show (table_jpg);
  gtk_container_add (GTK_CONTAINER (frame_jpg), table_jpg);
  gtk_container_set_border_width (GTK_CONTAINER (table_jpg), 5);
  gtk_table_set_row_spacings (GTK_TABLE (table_jpg), 2);
  gtk_table_set_col_spacings (GTK_TABLE (table_jpg), 2);

  jpg_row = 0;

  /* the dont recode label */
  label = gtk_label_new (_("Dont Recode:"));
  gtk_widget_show (label);
  gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
  gtk_table_attach (GTK_TABLE (table_jpg), label, 0, 1, jpg_row, jpg_row+1,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);

  /* the dont recode checkbutton */
  checkbutton = gtk_check_button_new_with_label (" ");
  gpp->jpg_dont_recode_checkbutton = checkbutton;
  gtk_widget_show (checkbutton);
  gtk_table_attach (GTK_TABLE (table_jpg), checkbutton, 1, 2, jpg_row, jpg_row+1,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);
        g_object_set_data (G_OBJECT (checkbutton), "gpp"
                          , (gpointer)gpp);
        g_signal_connect (G_OBJECT (checkbutton), "toggled"
                   , G_CALLBACK (on_checkbutton_toggled)
                   , &epp->jpeg_dont_recode_frames);
  gimp_help_set_help_data (checkbutton
                   , _("Don't recode the input JPEG frames."
                       " WARNING: This option may produce an unusable video"
                       " when refered JPEG frames are not YUV 4:2:2 encoded.")
                   , NULL);


  jpg_row++;

  /* the interlace label */
  label = gtk_label_new (_("Interlace:"));
  gtk_widget_show (label);
  gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
  gtk_table_attach (GTK_TABLE (table_jpg), label, 0, 1, jpg_row, jpg_row+1,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);


  /* the interlace checkbutton */
  checkbutton = gtk_check_button_new_with_label (" ");
  gpp->jpg_interlace_checkbutton = checkbutton;
  gtk_widget_show (checkbutton);
  gtk_table_attach (GTK_TABLE (table_jpg), checkbutton, 1, 2, jpg_row, jpg_row+1,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);
        g_object_set_data (G_OBJECT (checkbutton), "gpp"
                          , (gpointer)gpp);
        g_signal_connect (G_OBJECT (checkbutton), "toggled"
                   , G_CALLBACK (on_checkbutton_toggled)
                   , &epp->jpeg_interlaced);
  gimp_help_set_help_data (checkbutton
                   , _("Generate interlaced JPEGs (two frames for odd/even lines)")
                   , NULL);


  jpg_row++;

  /* the odd frames first label */
  label = gtk_label_new (_("Odd Frames first:"));
  gtk_widget_show (label);
  gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
  gtk_table_attach (GTK_TABLE (table_jpg), label, 0, 1, jpg_row, jpg_row+1,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);
  gtk_label_set_justify (GTK_LABEL (label), GTK_JUSTIFY_RIGHT);

  /* the odd frames first checkbutton */
  checkbutton = gtk_check_button_new_with_label (" ");
  gpp->jpg_odd_first_checkbutton = checkbutton;
  gtk_widget_show (checkbutton);
  gtk_table_attach (GTK_TABLE (table_jpg), checkbutton, 1, 2, jpg_row, jpg_row+1,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);
        g_object_set_data (G_OBJECT (checkbutton), "gpp"
                          , (gpointer)gpp);
        g_signal_connect (G_OBJECT (checkbutton), "toggled"
                   , G_CALLBACK (on_checkbutton_toggled)
                   , &epp->jpeg_odd_even);
  gimp_help_set_help_data (checkbutton
                   , _("Check if you want the odd frames to be coded first (only for interlaced JPEGs)")
                   , NULL);


  jpg_row++;

  /* the jpeg quality label */
  label = gtk_label_new (_("Quality:"));
  gtk_widget_show (label);
  gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
  gtk_table_attach (GTK_TABLE (table_jpg), label, 0, 1, jpg_row, jpg_row+1,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);

  /* the jpeg quality spinbutton */
  adj = gtk_adjustment_new (epp->jpeg_quality
                           , 0, 100
                           , 1, 10, 0);
  gpp->jpg_quality_spinbutton_adj = adj;
  spinbutton = gtk_spin_button_new (GTK_ADJUSTMENT (adj), 1, 0);
  gpp->jpg_quality_spinbutton = spinbutton;
  gtk_widget_show (spinbutton);
  gtk_table_attach (GTK_TABLE (table_jpg), spinbutton, 1, 2, jpg_row, jpg_row+1,
                    (GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);
        g_object_set_data (G_OBJECT (checkbutton), "gpp"
                          , (gpointer)gpp);
        g_signal_connect (G_OBJECT (adj), "value_changed"
                         , G_CALLBACK (on_gint32_spinbutton_changed)
                         , &epp->jpeg_quality);
  gimp_help_set_help_data (spinbutton
                    , _("The quality setting of the encoded JPEG frames (100=best quality)")
                    , NULL);

  /* the notebook page for PNG Codec options */
  /* ----------------------------------------- */
  notebook_page_idx++;

  frame_png = gimp_frame_new (_("PNG Codec Options"));
  gtk_widget_show (frame_png);
  gtk_container_add (GTK_CONTAINER (notebook_main), frame_png);
  gtk_container_set_border_width (GTK_CONTAINER (frame_png), 4);

  label = gtk_label_new (_("PNG Options"));
  gtk_widget_show (label);
  gtk_notebook_set_tab_label (GTK_NOTEBOOK (notebook_main)
      , gtk_notebook_get_nth_page (GTK_NOTEBOOK (notebook_main), notebook_page_idx), label);


  /* the table for the JPEG option widgets */
  table_png = gtk_table_new (4, 3, FALSE);
  gtk_widget_show (table_png);
  gtk_container_add (GTK_CONTAINER (frame_png), table_png);
  gtk_container_set_border_width (GTK_CONTAINER (table_png), 5);
  gtk_table_set_row_spacings (GTK_TABLE (table_png), 2);
  gtk_table_set_col_spacings (GTK_TABLE (table_png), 2);

  png_row = 0;

  /* the dont recode label */
  label = gtk_label_new (_("Dont Recode:"));
  gtk_widget_show (label);
  gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
  gtk_table_attach (GTK_TABLE (table_png), label, 0, 1, png_row, png_row+1,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);

  /* the dont recode checkbutton */
  checkbutton = gtk_check_button_new_with_label (" ");
  gpp->png_dont_recode_checkbutton = checkbutton;
  gtk_widget_show (checkbutton);
  gtk_table_attach (GTK_TABLE (table_png), checkbutton, 1, 2, png_row, png_row+1,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);
        g_object_set_data (G_OBJECT (checkbutton), "gpp"
                          , (gpointer)gpp);
        g_signal_connect (G_OBJECT (checkbutton), "toggled"
                   , G_CALLBACK (on_checkbutton_toggled)
                   , &epp->png_dont_recode_frames);
  gimp_help_set_help_data (checkbutton
                   , _("Don't recode the input PNG frames when possible."
                       " WARNING: This option may produce an unusable video")
                   , NULL);


  png_row++;

  /* the interlace label */
  label = gtk_label_new (_("Interlace:"));
  gtk_widget_show (label);
  gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
  gtk_table_attach (GTK_TABLE (table_png), label, 0, 1, png_row, png_row+1,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);


  /* the interlace checkbutton */
  checkbutton = gtk_check_button_new_with_label (" ");
  gpp->png_interlace_checkbutton = checkbutton;
  gtk_widget_show (checkbutton);
  gtk_table_attach (GTK_TABLE (table_png), checkbutton, 1, 2, png_row, png_row+1,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);
        g_object_set_data (G_OBJECT (checkbutton), "gpp"
                          , (gpointer)gpp);
        g_signal_connect (G_OBJECT (checkbutton), "toggled"
                   , G_CALLBACK (on_checkbutton_toggled)
                   , &epp->png_interlaced);
  gimp_help_set_help_data (checkbutton
                   , _("Generate interlaced PNGs")
                   , NULL);


  png_row++;


  /* the png compression label */
  label = gtk_label_new (_("Compression:"));
  gtk_widget_show (label);
  gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
  gtk_table_attach (GTK_TABLE (table_png), label, 0, 1, png_row, png_row+1,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);

  /* the jpeg quality spinbutton */
  adj = gtk_adjustment_new (epp->png_compression
                           , 0, 9
                           , 1, 10, 0);
  gpp->png_compression_spinbutton_adj = adj;
  spinbutton = gtk_spin_button_new (GTK_ADJUSTMENT (adj), 1, 0);
  gpp->png_compression_spinbutton = spinbutton;
  gtk_widget_show (spinbutton);
  gtk_table_attach (GTK_TABLE (table_png), spinbutton, 1, 2, png_row, png_row+1,
                    (GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);
        g_object_set_data (G_OBJECT (checkbutton), "gpp"
                          , (gpointer)gpp);
        g_signal_connect (G_OBJECT (adj), "value_changed"
                         , G_CALLBACK (on_gint32_spinbutton_changed)
                         , &epp->jpeg_quality);
  gimp_help_set_help_data (spinbutton
                    , _("The compression setting of the encoded PNG frames (9=best compression"
                        "0=fastest)")
                    , NULL);



  /* the notebook page for RAW Codec options */
  /* ----------------------------------------- */
  notebook_page_idx++;

  frame_raw = gimp_frame_new (_("RAW Codec Options"));
  gtk_widget_show (frame_raw);
  gtk_container_add (GTK_CONTAINER (notebook_main), frame_raw);
  gtk_container_set_border_width (GTK_CONTAINER (frame_raw), 4);

  label = gtk_label_new (_("RAW Options"));
  gtk_widget_show (label);
  gtk_notebook_set_tab_label (GTK_NOTEBOOK (notebook_main)
      , gtk_notebook_get_nth_page (GTK_NOTEBOOK (notebook_main), notebook_page_idx), label);

  /* the table for the RAW option widgets */
  table_raw = gtk_table_new (1, 3, FALSE);
  gtk_widget_show (table_raw);
  gtk_container_add (GTK_CONTAINER (frame_raw), table_raw);
  gtk_container_set_border_width (GTK_CONTAINER (table_raw), 5);
  gtk_table_set_row_spacings (GTK_TABLE (table_raw), 2);
  gtk_table_set_col_spacings (GTK_TABLE (table_raw), 2);

  raw_row = 0;

  /* the raw codec info label */
  label = gtk_label_new (_("The RAW codec has no encoding options.\n"
                           "The resulting videoframes will be\n"
                           "uncompressed."));
  gtk_widget_show (label);
  gtk_table_attach (GTK_TABLE (table_raw), label, 0, 3, raw_row, raw_row+1,
                    (GtkAttachOptions) (0),
                    (GtkAttachOptions) (0), 0, 0);
  gtk_label_set_justify (GTK_LABEL (label), GTK_JUSTIFY_LEFT);


  raw_row++;

  /* the vflip label */
  label = gtk_label_new (_("Vertical flip:"));
  gtk_widget_show (label);
  gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
  gtk_table_attach (GTK_TABLE (table_raw), label, 0, 1, raw_row, raw_row+1,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);
  gtk_label_set_justify (GTK_LABEL (label), GTK_JUSTIFY_RIGHT);

  /* the vflip checkbutton */
  checkbutton = gtk_check_button_new_with_label (" ");
  gpp->raw_vflip_checkbutton = checkbutton;
  gtk_widget_show (checkbutton);
  gtk_table_attach (GTK_TABLE (table_raw), checkbutton, 1, 2, raw_row, raw_row+1,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);
        g_object_set_data (G_OBJECT (checkbutton), "gpp"
                          , (gpointer)gpp);
        g_signal_connect (G_OBJECT (checkbutton), "toggled"
                   , G_CALLBACK (on_checkbutton_toggled)
                   , &epp->raw_vflip);
  gimp_help_set_help_data (checkbutton
                   , _("Check if you want to encode frames vertically flipped "
                       "(suitable for playback on WinDVD player) "
                       "or as is (suitable for gmplayer on linux)")
                   , NULL);

  raw_row++;

  /* the BGR label */
  label = gtk_label_new (_("BGR (rgb):"));
  gtk_widget_show (label);
  gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
  gtk_table_attach (GTK_TABLE (table_raw), label, 0, 1, raw_row, raw_row+1,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);
  gtk_label_set_justify (GTK_LABEL (label), GTK_JUSTIFY_RIGHT);


  /* the bgr checkbutton */
  checkbutton = gtk_check_button_new_with_label (" ");
  gpp->raw_bgr_checkbutton = checkbutton;
  gtk_widget_show (checkbutton);
  gtk_table_attach (GTK_TABLE (table_raw), checkbutton, 1, 2, raw_row, raw_row+1,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);
        g_object_set_data (G_OBJECT (checkbutton), "gpp"
                          , (gpointer)gpp);
        g_signal_connect (G_OBJECT (checkbutton), "toggled"
                   , G_CALLBACK (on_checkbutton_toggled)
                   , &epp->raw_bgr);
  gimp_help_set_help_data (checkbutton
                   , _("Check if you want to encode frames in BGR (prefered) or RGB colormodel "
                       "(most players like WinDvD, VLC-player want BGR colormodel for RAW data) "
                       "other players want RGB colromodel for RAW avi data)")
                   , NULL);

#ifdef ENABLE_LIBXVIDCORE

  /* the notebook page for XVID Codec options */
  /* ----------------------------------------- */
  notebook_page_idx++;
  frame_xvid = gimp_frame_new (_("XVID Codec Options"));
  gtk_widget_show (frame_xvid);
  gtk_container_add (GTK_CONTAINER (notebook_main), frame_xvid);
  gtk_container_set_border_width (GTK_CONTAINER (frame_xvid), 4);

  label = gtk_label_new (_("XVID Options"));
  gtk_widget_show (label);
  gtk_notebook_set_tab_label (GTK_NOTEBOOK (notebook_main)
       , gtk_notebook_get_nth_page (GTK_NOTEBOOK (notebook_main), notebook_page_idx), label);


  /* the table for the XVID option widgets */
  table_xvid = gtk_table_new (10, 3, FALSE);
  gtk_widget_show (table_xvid);
  gtk_container_add (GTK_CONTAINER (frame_xvid), table_xvid);
  gtk_container_set_border_width (GTK_CONTAINER (table_xvid), 5);
  gtk_table_set_row_spacings (GTK_TABLE (table_xvid), 2);
  gtk_table_set_col_spacings (GTK_TABLE (table_xvid), 2);

  xvid_row = 0;

  /* the xvid KBitrate label */
  label = gtk_label_new (_("KBitrate:"));
  gtk_widget_show (label);
  gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
  gtk_table_attach (GTK_TABLE (table_xvid), label, 0, 1, xvid_row, xvid_row+1,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);

  /* the xvid KBitrate spinbutton */
  adj = gtk_adjustment_new (epp->xvid.rc_bitrate / 1000
                           , -1, 9999
                           , 1, 10, 0);
  spinbutton = gtk_spin_button_new (GTK_ADJUSTMENT (adj), 1, 0);
  gpp->xvid_rc_kbitrate_spinbutton_adj = adj;
  gpp->xvid_rc_kbitrate_spinbutton = spinbutton;
  gtk_widget_show (spinbutton);
  gtk_table_attach (GTK_TABLE (table_xvid), spinbutton, 1, 2, xvid_row, xvid_row+1,
                    (GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);
        g_object_set_data (G_OBJECT (checkbutton), "gpp"
                          , (gpointer)gpp);
        g_signal_connect (G_OBJECT (adj), "value_changed"
                         , G_CALLBACK (on_gint32_spinbutton_changed)
                         , &gpp->xvid_kbitrate);
  gimp_help_set_help_data (spinbutton
                    , _("Kilobitrate for XVID Codec (1 = 1000 Bit/sec) -1 for default")
                    , NULL);


  xvid_row++;

  /* the xvid Reaction Delay label */
  label = gtk_label_new (_("Reaction Delay:"));
  gtk_widget_show (label);
  gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
  gtk_table_attach (GTK_TABLE (table_xvid), label, 0, 1, xvid_row, xvid_row+1,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);

  /* the xvid Reaction Delay spinbutton */
  adj = gtk_adjustment_new (epp->xvid.rc_reaction_delay_factor
                           , -1, 100
                           , 1, 10, 0);
  spinbutton = gtk_spin_button_new (GTK_ADJUSTMENT (adj), 1, 0);
  gpp->xvid_rc_reaction_delay_spinbutton_adj = adj;
  gpp->xvid_rc_reaction_delay_spinbutton = spinbutton;
  gtk_widget_show (spinbutton);
  gtk_table_attach (GTK_TABLE (table_xvid), spinbutton, 1, 2, xvid_row, xvid_row+1,
                    (GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);
        g_object_set_data (G_OBJECT (checkbutton), "gpp"
                          , (gpointer)gpp);
        g_signal_connect (G_OBJECT (adj), "value_changed"
                         , G_CALLBACK (on_gint32_spinbutton_changed)
                         , &epp->xvid.rc_reaction_delay_factor);
  gimp_help_set_help_data (spinbutton
                    , _("reaction delay factor (-1 for default)")
                    , NULL);


  xvid_row++;

  /* the xvid AVG Period label */
  label = gtk_label_new (_("AVG Period:"));
  gtk_widget_show (label);
  gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
  gtk_table_attach (GTK_TABLE (table_xvid), label, 0, 1, xvid_row, xvid_row+1,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);

  /* the xvid AVG Period spinbutton */
  adj = gtk_adjustment_new (epp->xvid.rc_averaging_period
                           , -1, 1000
                           , 1, 10, 0);
  spinbutton = gtk_spin_button_new (GTK_ADJUSTMENT (adj), 1, 0);
  gpp->xvid_rc_avg_period_spinbutton_adj = adj;
  gpp->xvid_rc_avg_period_spinbutton = spinbutton;
  gtk_widget_show (spinbutton);
  gtk_table_attach (GTK_TABLE (table_xvid), spinbutton, 1, 2, xvid_row, xvid_row+1,
                    (GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);
        g_object_set_data (G_OBJECT (checkbutton), "gpp"
                          , (gpointer)gpp);
        g_signal_connect (G_OBJECT (adj), "value_changed"
                         , G_CALLBACK (on_gint32_spinbutton_changed)
                         , &epp->xvid.rc_averaging_period);
  gimp_help_set_help_data (spinbutton
                    , _("averaging period (-1 for default)")
                    , NULL);


  xvid_row++;

  /* the xvid Buffer label */
  label = gtk_label_new (_("Buffer:"));
  gtk_widget_show (label);
  gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
  gtk_table_attach (GTK_TABLE (table_xvid), label, 0, 1, xvid_row, xvid_row+1,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);

  /* the xvid Buffer spinbutton */
  adj = gtk_adjustment_new (epp->xvid.rc_buffer
                           , -1, 100
                           , 1, 10, 0);
  spinbutton = gtk_spin_button_new (GTK_ADJUSTMENT (adj), 1, 0);
  gpp->xvid_rc_buffer_spinbutton_adj = adj;
  gpp->xvid_rc_buffer_spinbutton = spinbutton;
  gtk_widget_show (spinbutton);
  gtk_table_attach (GTK_TABLE (table_xvid), spinbutton, 1, 2, xvid_row, xvid_row+1,
                    (GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);
        g_object_set_data (G_OBJECT (checkbutton), "gpp"
                          , (gpointer)gpp);
        g_signal_connect (G_OBJECT (adj), "value_changed"
                         , G_CALLBACK (on_gint32_spinbutton_changed)
                         , &epp->xvid.rc_buffer);
  gimp_help_set_help_data (spinbutton
                    , _("Buffersize (-1 for default)")
                    , NULL);


  xvid_row++;

  /* the xvid max_quantizer label */
  label = gtk_label_new (_("Max Quantizer:"));
  gtk_widget_show (label);
  gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
  gtk_table_attach (GTK_TABLE (table_xvid), label, 0, 1, xvid_row, xvid_row+1,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);

  /* the xvid max_quantizer spinbutton */
  adj = gtk_adjustment_new (epp->xvid.max_quantizer
                           , 1, 31
                           , 1, 10, 0);
  spinbutton = gtk_spin_button_new (GTK_ADJUSTMENT (adj), 1, 0);
  gpp->xvid_max_quantizer_spinbutton_adj = adj;
  gpp->xvid_max_quantizer_spinbutton = spinbutton;
  gtk_widget_show (spinbutton);
  gtk_table_attach (GTK_TABLE (table_xvid), spinbutton, 1, 2, xvid_row, xvid_row+1,
                    (GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);
        g_object_set_data (G_OBJECT (checkbutton), "gpp"
                          , (gpointer)gpp);
        g_signal_connect (G_OBJECT (adj), "value_changed"
                         , G_CALLBACK (on_gint32_spinbutton_changed)
                         , &epp->xvid.max_quantizer);
  gimp_help_set_help_data (spinbutton
                    , _("upper limit for quantize Range 1 == BEST Quality")
                    , NULL);

  xvid_row++;

  /* the xvid min_quantizer label */
  label = gtk_label_new (_("Min Quantizer:"));
  gtk_widget_show (label);
  gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
  gtk_table_attach (GTK_TABLE (table_xvid), label, 0, 1, xvid_row, xvid_row+1,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);

  /* the xvid min_quantizer spinbutton */
  adj = gtk_adjustment_new (epp->xvid.min_quantizer
                           , 1, 31
                           , 1, 10, 0);
  spinbutton = gtk_spin_button_new (GTK_ADJUSTMENT (adj), 1, 0);
  gpp->xvid_min_quantizer_spinbutton_adj = adj;
  gpp->xvid_min_quantizer_spinbutton = spinbutton;
  gtk_widget_show (spinbutton);
  gtk_table_attach (GTK_TABLE (table_xvid), spinbutton, 1, 2, xvid_row, xvid_row+1,
                    (GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);
        g_object_set_data (G_OBJECT (checkbutton), "gpp"
                          , (gpointer)gpp);
        g_signal_connect (G_OBJECT (adj), "value_changed"
                         , G_CALLBACK (on_gint32_spinbutton_changed)
                         , &epp->xvid.min_quantizer);
  gimp_help_set_help_data (spinbutton
                    , _("lower limit for quantize Range 1 == BEST Quality")
                    , NULL);


  xvid_row++;

  /* the xvid max_key_interval label */
  label = gtk_label_new (_("Key Interval:"));
  gtk_widget_show (label);
  gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
  gtk_table_attach (GTK_TABLE (table_xvid), label, 0, 1, xvid_row, xvid_row+1,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);

  /* the xvid max_key_interval spinbutton */
  adj = gtk_adjustment_new (epp->xvid.max_key_interval
                           , 1, 500
                           , 1, 10, 0);
  spinbutton = gtk_spin_button_new (GTK_ADJUSTMENT (adj), 1, 0);
  gpp->xvid_max_key_interval_spinbutton_adj = adj;
  gpp->xvid_max_key_interval_spinbutton = spinbutton;
  gtk_widget_show (spinbutton);
  gtk_table_attach (GTK_TABLE (table_xvid), spinbutton, 1, 2, xvid_row, xvid_row+1,
                    (GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);
        g_object_set_data (G_OBJECT (checkbutton), "gpp"
                          , (gpointer)gpp);
        g_signal_connect (G_OBJECT (adj), "value_changed"
                         , G_CALLBACK (on_gint32_spinbutton_changed)
                         , &epp->xvid.max_key_interval);
  gimp_help_set_help_data (spinbutton
                    , _("max distance for keyframes (I-frames)")
                    , NULL);

  xvid_row++;

  /* the xvid quality label */
  label = gtk_label_new (_("Quality:"));
  gtk_widget_show (label);
  gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
  gtk_table_attach (GTK_TABLE (table_xvid), label, 0, 1, xvid_row, xvid_row+1,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);

  /* the xvid quality spinbutton */
  adj = gtk_adjustment_new (epp->xvid.quality_preset
                           , 0, 6
                           , 1, 10, 0);
  spinbutton = gtk_spin_button_new (GTK_ADJUSTMENT (adj), 1, 0);
  gpp->xvid_quality_spinbutton_adj = adj;
  gpp->xvid_quality_spinbutton = spinbutton;
  gtk_widget_show (spinbutton);
  gtk_table_attach (GTK_TABLE (table_xvid), spinbutton, 1, 2, xvid_row, xvid_row+1,
                    (GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);
        g_object_set_data (G_OBJECT (checkbutton), "gpp"
                          , (gpointer)gpp);
        g_signal_connect (G_OBJECT (adj), "value_changed"
                         , G_CALLBACK (on_gint32_spinbutton_changed)
                         , &epp->xvid.quality_preset);
  gimp_help_set_help_data (spinbutton
                    , _("XVID codec algoritm presets where 0==low quality(fast) 6==best(slow)")
                    , NULL);

#endif




  return shell_window;
}  /* end p_create_shell_window */




/* ----------------------------------------
 * gap_enc_avi_gui_dialog
 * ----------------------------------------
 * AVI Encoder GUI dialog
 *
 *   return  0 .. OK
 *          -1 .. in case of Error or cancel
 */
gint
gap_enc_avi_gui_dialog(GapGveAviGlobalParams *gpp)
{
  if(gap_debug) printf("gap_enc_avi_gui_dialog: Start\n");

  gimp_ui_init ("gap_enc_avi_params", FALSE);
  gap_stock_init();

  /* ---------- dialog ----------*/

  if(gap_debug) printf("gap_enc_avi_gui_dialog: Before create_shell_window\n");

  gpp->notebook_main = NULL;
  gpp->jpg_dont_recode_checkbutton = NULL;
  gpp->jpg_interlace_checkbutton = NULL;
  gpp->jpg_odd_first_checkbutton = NULL;
  gpp->jpg_quality_spinbutton = NULL;
  gpp->xvid_rc_kbitrate_spinbutton = NULL;
  gpp->xvid_rc_reaction_delay_spinbutton = NULL;
  gpp->xvid_rc_avg_period_spinbutton = NULL;
  gpp->xvid_rc_buffer_spinbutton = NULL;
  gpp->xvid_max_quantizer_spinbutton = NULL;
  gpp->xvid_min_quantizer_spinbutton = NULL;
  gpp->xvid_max_key_interval_spinbutton = NULL;
  gpp->xvid_quality_spinbutton = NULL;
  gpp->raw_vflip_checkbutton = NULL;
  gpp->raw_bgr_checkbutton = NULL;


  gpp->shell_window = p_create_shell_window (gpp);
  p_init_widget_values(gpp);

  if(gap_debug) printf("gap_enc_avi_gui_dialog: After create_shell_window\n");

  gtk_widget_show (gpp->shell_window);

  gpp->val.run = 0;
  gtk_main ();

  if(gap_debug) printf("A F T E R gtk_main run:%d\n", (int)gpp->val.run);

  gpp->shell_window = NULL;
  gpp->evl.xvid.rc_bitrate = gpp->xvid_kbitrate * 1000;

  if(gpp->val.run)
  {
    return 0;
  }
  return -1;
}  /* end gap_enc_avi_gui_dialog */
