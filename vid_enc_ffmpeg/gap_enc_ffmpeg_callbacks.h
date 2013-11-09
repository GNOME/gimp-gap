/* gap_enc_ffmpeg_callbacks.h
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
 * along with this program; if not, see
 * <http://www.gnu.org/licenses/>.
 */

/* revision history:
 * version 2.1.0a;  2004.11.06   hof: use some general callbacks.
 *                                   (removed lots of similar callbacks
 *                                    that was needed for the old glade generated code)
 * version 2.1.0a;  2004.06.05   hof: update params from ffmpeg 0.4.6 to 0.4.8
 * version 2.1.0a;  2004.05.12 : created
 */


#ifndef GAP_ENC_FFMPEG_CALLBACKS_H
#define GAP_ENC_FFMPEG_CALLBACKS_H

#include <config.h>
#include "gap_libgapvidutil.h"
#include "gap_libgimpgap.h"

#include "gap_enc_ffmpeg_main.h"

#include <gtk/gtk.h>

void
gap_ffcb_set_widget_sensitivity  (GapGveFFMpegGlobalParams *gpp);

void
on_ff_response (GtkWidget *widget,
                 gint       response_id,
                 GapGveFFMpegGlobalParams *gpp);

void
on_ff_gint32_spinbutton_changed  (GtkWidget *widget,
                                  gint32    *dest_value_ptr);

void
on_ff_gdouble_spinbutton_changed  (GtkWidget *widget,
                                   gdouble    *dest_value_ptr);

void
on_ff_gint32_checkbutton_toggled  (GtkToggleButton *checkbutton,
                                   gint32          *dest_value_ptr);
                                   




void
on_ff_fileformat_combo  (GtkWidget     *wgt_item,
                              GapGveFFMpegGlobalParams *gpp);

void
on_ff_vid_codec_combo  (GtkWidget     *wgt_item,
                             GapGveFFMpegGlobalParams *gpp);

void
on_ff_aud_codec_combo  (GtkWidget     *wgt_item,
                             GapGveFFMpegGlobalParams *gpp);

void
on_ff_aud_bitrate_combo  (GtkWidget     *wgt_item,
                               GapGveFFMpegGlobalParams *gpp);

void
on_ff_gint32_combo  (GtkWidget     *wgt_item,
                       gint32 *val_ptr);



void
on_ff_presets_combo  (GtkWidget     *wgt_item,
                           GapGveFFMpegGlobalParams *gpp);

void
on_ff_aspect_combo  (GtkWidget     *wgt_item,
                           GapGveFFMpegGlobalParams *gpp);

void
on_ff_passlogfile_entry_changed        (GtkEditable     *editable,
                                        GapGveFFMpegGlobalParams *gpp);

void
on_ff_passlogfile_filesel_button_clicked (GtkButton       *button,
                                        GapGveFFMpegGlobalParams *gpp);

void
on_ff_title_entry_changed              (GtkEditable     *editable,
                                        GapGveFFMpegGlobalParams *gpp);

void
on_ff_author_entry_changed             (GtkEditable     *editable,
                                        GapGveFFMpegGlobalParams *gpp);

void
on_ff_copyright_entry_changed          (GtkEditable     *editable,
                                        GapGveFFMpegGlobalParams *gpp);

void
on_ff_comment_entry_changed            (GtkEditable     *editable,
                                        GapGveFFMpegGlobalParams *gpp);

void
on_fsb__fileselection_destroy          (GtkObject       *object,
                                        GapGveFFMpegGlobalParams *gpp);

void
on_fsb__ok_button_clicked              (GtkButton       *button,
                                        GapGveFFMpegGlobalParams *gpp);

void
on_fsb__cancel_button_clicked          (GtkButton       *button,
                                        GapGveFFMpegGlobalParams *gpp);

#endif
