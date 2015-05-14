/* gap_cme_gui.h
 * 2001.04.08 hof (Wolfgang Hofer)
 *
 * GAP ... Gimp Animation Plugins
 *
 * This Module contains GUI Procedures for common Video Encoder dialog
 * it calls the galde GTK GUI modules
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


#ifndef GAP_CME_GUI_H
#define GAP_CME_GUI_H

/* revision history:
 * version 2.1.0a;  2004.05.06   hof: integration into gimp-gap project
 * version 1.2.2c;  2003.03.08   hof: added gap_cme_gui_remove_poll_timer
 * version 1.2.2c;  2003.01.09   hof: added gap_cme_gui_requery_vid_extension
 * version 1.2.2b;  2002.10.28   hof: added gap_cme_gui_check_storyboard_file
 * version 1.2.1a;  2001.04.08   hof: created
 */

#include <config.h>
#include "gap_cme_main.h"

#define  GAP_CME_GUI_ENC_MENU_ITEM_ENC_PTR "gap_cme_enc_menu_item_enc_ptr"


gboolean  gap_cme_gui_check_gui_thread_is_active(GapCmeGlobalParams *gpp);

void      gap_cme_gui_upd_vid_extension (GapCmeGlobalParams *gpp);
void      gap_cme_gui_upd_wgt_sensitivity (GapCmeGlobalParams *gpp);
void      gap_cme_gui_update_aud_labels (GapCmeGlobalParams *gpp);
void      gap_cme_gui_update_vid_labels (GapCmeGlobalParams *gpp);
void      gap_cme_gui_util_sox_widgets (GapCmeGlobalParams *gpp);
gint      gap_cme_gui_pdb_call_encoder_gui_plugin (GapCmeGlobalParams *gpp);
gpointer  gap_cme_gui_thread_async_pdb_call(gpointer data);
void      gap_cme_gui_check_storyboard_file(GapCmeGlobalParams *gpp);
void      gap_cme_gui_requery_vid_extension(GapCmeGlobalParams *gpp);
void      gap_cme_gui_remove_poll_timer(GapCmeGlobalParams *gpp);
gboolean  gap_cme_gui_check_encode_OK (GapCmeGlobalParams *gpp);

GtkWidget*      gap_cme_gui_create_fss__fileselection (GapCmeGlobalParams *gpp);
GtkWidget*      gap_cme_gui_create_fsv__fileselection (GapCmeGlobalParams *gpp);
GtkWidget*      gap_cme_gui_create_fsb__fileselection (GapCmeGlobalParams *gpp);
GtkWidget*      gap_cme_gui_create_fsa__fileselection (GapCmeGlobalParams *gpp);

void            gap_cme_gui_update_encoder_status(GapCmeGlobalParams *gpp);
gint32          gap_cme_gui_start_video_encoder(GapCmeGlobalParams *gpp);
gint32          gap_cme_gui_start_video_encoder_as_thread(GapCmeGlobalParams *gpp);



gint32 gap_cme_gui_master_encoder_dialog(GapCmeGlobalParams *gpp);

#endif
