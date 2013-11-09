/* gap_enc_ffmpeg_gui.h
 * 2004.05.12 hof (Wolfgang Hofer)
 *
 * GAP ... Gimp Animation Plugins
 *
 * This Module contains FFMPEG specific Video Encoder GUI Procedures
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


#ifndef GAP_ENC_FFMPEG_GUI_H
#define GAP_ENC_FFMPEG_GUI_H

/* revision history:
 * version 2.1.0a;  2004.05.12 : created
 */

#include <config.h>
#include "gap_enc_ffmpeg_main.h"

#define GAP_ENC_FFGUI_GPP                    "gap_enc_ffgui_gpp"
#define GAP_ENC_FFGUI_ADJ                    "gap_enc_ffgui_adj"
#define GAP_ENC_FFGUI_COMBO_STRLIST          "gap_enc_ffgui_combo_strlist"

#define GAP_ENC_FFMPEG_RESPONSE_OPEN 1
#define GAP_ENC_FFMPEG_RESPONSE_SAVE 2

void        gap_enc_ffgui_set_default_codecs(GapGveFFMpegGlobalParams *gpp, gboolean set_codec_menus);
void        gap_enc_ffgui_init_main_dialog_widgets(GapGveFFMpegGlobalParams *gpp);

gint        gap_enc_ffgui_gettab_audio_krate(gint idx);
gdouble     gap_enc_ffgui_gettab_aspect(gint idx);

GtkWidget*  gap_enc_ffgui_create_fsb__fileselection (GapGveFFMpegGlobalParams *gpp);
gint        gap_enc_ffgui_ffmpeg_encode_dialog(GapGveFFMpegGlobalParams *gpp);

const char * gap_get_combo_string_by_idx(gpointer string_combo_elem_list, gint idx);

#endif
