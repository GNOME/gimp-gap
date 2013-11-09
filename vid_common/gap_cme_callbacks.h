/* gap_cme_callbacks.h
 * 2001.04.08 hof (Wolfgang Hofer)
 *
 * This Module contains GUI Procedure Callbacks for the common Video Encoder
 * Master dialog
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
 * version 2.1.0a;  2004.05.07   hof: ported to gtk2+2 and integration into gimp-gap project
 * version 1.2.1a;  2001.04.08   hof: created
 */


#ifndef GAP_CME_CALLBACKS_H
#define GAP_CME_CALLBACKS_H


#include <config.h>
#include "gap_cme_main.h"
#include <gtk/gtk.h>


/* ------------------------------------------------------ */
/* BEGIN callbacks                                        */
/* ------------------------------------------------------ */

void
on_cme__response (GtkWidget *widget,
                  gint       response_id,
                  GapCmeGlobalParams *gpp);

void
on_cme__combo_enocder  (GtkWidget     *wgt_item,
                           GapCmeGlobalParams *gpp);

void
on_cme__combo_scale  (GtkWidget     *wgt_item,
                           GapCmeGlobalParams *gpp);

void
on_cme__combo_framerate  (GtkWidget     *wgt_item,
                           GapCmeGlobalParams *gpp);

void
on_cme__combo_outsamplerate  (GtkWidget     *wgt_item,
                           GapCmeGlobalParams *gpp);

void
on_cme__combo_vid_norm  (GtkWidget     *wgt_item,
                           GapCmeGlobalParams *gpp);

void
on_fsv__fileselection_destroy          (GtkObject       *object,
                                        GapCmeGlobalParams *gpp);

void
on_fsv__button_OK_clicked              (GtkButton       *button,
                                        GapCmeGlobalParams *gpp);

void
on_fsv__button_cancel_clicked          (GtkButton       *button,
                                        GapCmeGlobalParams *gpp);

void
on_fsb__fileselection_destroy          (GtkObject       *object,
                                        GapCmeGlobalParams *gpp);

void
on_fsb__button_OK_clicked              (GtkButton       *button,
                                        GapCmeGlobalParams *gpp);

void
on_fsb__button_cancel_clicked          (GtkButton       *button,
                                        GapCmeGlobalParams *gpp);

void
on_fsa__fileselection_destroy          (GtkObject       *object,
                                        GapCmeGlobalParams *gpp);

void
on_fsa__button_OK_clicked              (GtkButton       *button,
                                        GapCmeGlobalParams *gpp);

void
on_fsa__button_cancel_clicked          (GtkButton       *button,
                                        GapCmeGlobalParams *gpp);

void
on_ow__dialog_destroy                  (GtkObject       *object,
                                        GapCmeGlobalParams *gpp);

void
on_ow__button_one_clicked              (GtkButton       *button,
                                        GapCmeGlobalParams *gpp);

void
on_ow__button_cancel_clicked           (GtkButton       *button,
                                        GapCmeGlobalParams *gpp);

void
on_cme__spinbutton_width_changed       (GtkEditable     *editable,
                                        GapCmeGlobalParams *gpp);

void
on_cme__spinbutton_height_changed      (GtkEditable     *editable,
                                        GapCmeGlobalParams *gpp);

void
on_cme__spinbutton_from_changed        (GtkEditable     *editable,
                                        GapCmeGlobalParams *gpp);

void
on_cme__spinbutton_to_changed          (GtkEditable     *editable,
                                        GapCmeGlobalParams *gpp);

void
on_cme__spinbutton_framerate_changed   (GtkEditable     *editable,
                                        GapCmeGlobalParams *gpp);

void
on_cme__entry_audio1_changed           (GtkEditable     *editable,
                                        GapCmeGlobalParams *gpp);

void
on_cme__button_audio1_clicked          (GtkButton       *button,
                                        GapCmeGlobalParams *gpp);

void
on_cme__spinbutton_samplerate_changed  (GtkEditable     *editable,
                                        GapCmeGlobalParams *gpp);

void
on_qte_entry_video_changed             (GtkEditable     *editable,
                                        GapCmeGlobalParams *gpp);

void
on_cme__button_video_clicked           (GtkButton       *button,
                                        GapCmeGlobalParams *gpp);

void
on_cme__entry_video_changed            (GtkEditable     *editable,
                                        GapCmeGlobalParams *gpp);

void
on_cme__shell_window_destroy           (GtkObject       *object,
                                        GapCmeGlobalParams *gpp);

void
on_cme__entry_vidcodec_changed         (GtkEditable     *editable,
                                        GapCmeGlobalParams *gpp);

void
on_cme__button_params_clicked          (GtkButton       *button,
                                        GapCmeGlobalParams *gpp);

void
on_cme__button_sox_save_clicked        (GtkButton       *button,
                                        GapCmeGlobalParams *gpp);

void
on_cme__button_sox_load_clicked        (GtkButton       *button,
                                        GapCmeGlobalParams *gpp);

void
on_cme__entry_sox_changed              (GtkEditable     *editable,
                                        GapCmeGlobalParams *gpp);

void
on_cme__entry_sox_options_changed      (GtkEditable     *editable,
                                        GapCmeGlobalParams *gpp);

void
on_cme__button_sox_default_clicked     (GtkButton       *button,
                                        GapCmeGlobalParams *gpp);

void
on_cme__button_gen_tmp_audfile_clicked (GtkButton       *button,
                                        GapCmeGlobalParams *gpp);

void
on_cme__entry_mac_changed              (GtkEditable     *editable,
                                        GapCmeGlobalParams *gpp);

void
on_cme__button_mac_clicked             (GtkButton       *button,
                                        GapCmeGlobalParams *gpp);

void
on_cme__entry_stb_changed              (GtkEditable     *editable,
                                        GapCmeGlobalParams *gpp);

void
on_cme__button_stb_clicked             (GtkButton       *button,
                                        GapCmeGlobalParams *gpp);

void
on_cme__button_stb_audio_clicked      (GtkButton       *button,
                                       GapCmeGlobalParams *gpp);



void
on_fss__fileselection_destroy          (GtkObject       *object,
                                        GapCmeGlobalParams *gpp);

void
on_fss__button_OK_clicked              (GtkButton       *button,
                                        GapCmeGlobalParams *gpp);

void
on_fss__button_cancel_clicked          (GtkButton       *button,
                                        GapCmeGlobalParams *gpp);

void
on_cme__debug_button_clicked           (GtkButton       *button,
                                        GapCmeGlobalParams *gpp);

void
on_cme__entry_debug_multi_changed      (GtkEditable     *editable,
                                        GapCmeGlobalParams *gpp);

void
on_cme__entry_debug_flat_changed       (GtkEditable     *editable,
                                        GapCmeGlobalParams *gpp);

void
on_cme__checkbutton_enc_monitor_toggled
                                        (GtkCheckButton *checkbutton,
                                        GapCmeGlobalParams *gpp);


#endif
