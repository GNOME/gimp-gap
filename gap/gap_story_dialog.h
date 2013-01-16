/*  gap_story_dialog.h
 *
 *  This module handles GAP storyboard dialog editor
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

/* revision history:
 * version 1.3.27a; 2004/03/15  hof: gap_story_get_velem
 * version 1.3.25a; 2004/01/23  hof: created
 */

#ifndef _GAP_STORY_DIALOG_H
#define _GAP_STORY_DIALOG_H

#include "libgimp/gimp.h"
#include "gap_story_main.h"
#include "gap_story_properties.h"

void    gap_storyboard_dialog(GapStbMainGlobalParams *sgpp, GapStbCreationParams  *scrp);

void    gap_story_dlg_attw_render_all(GapStbAttrWidget *attw);

void    gap_story_dlg_pw_render_all(GapStbPropWidget *pw, gboolean recreate);
void    gap_story_pw_single_clip_playback(GapStbPropWidget *pw);
void    gap_story_pw_composite_playback(GapStbPropWidget *pw);
void    gap_story_attw_range_playback(GapStbAttrWidget *attw, gint32 begin_frame
            , gint32 end_frame);
void    gap_story_dlg_pw_update_mask_references(GapStbTabWidgets *tabw);

void    gap_story_dlg_spw_section_refresh(GapStbSecpropWidget *spw, GapStorySection *section);


void  gap_story_dlg_recreate_tab_widgets(GapStbTabWidgets *tabw
                                  ,GapStbMainGlobalParams *sgpp
                                  );
void  gap_story_dlg_render_default_icon(GapStoryElem *stb_elem, GapPView   *pv_ptr);
void  gap_story_dlg_tabw_update_frame_label (GapStbTabWidgets *tabw
                           , GapStbMainGlobalParams *sgpp
                           );
void  gap_story_dlg_tabw_undo_redo_sensitivity(GapStbTabWidgets *tabw);

guchar * gap_story_dlg_fetch_videoframe(GapStbMainGlobalParams *sgpp
                   , const char *gva_videofile
                   , gint32 framenumber
                   , gint32 seltrack
                   , const char *preferred_decoder
                   , gdouble delace
                   , gint32 *th_bpp
                   , gint32 *th_width
                   , gint32 *th_height
                   , gboolean do_scale
                   );
GapStoryBoard * gap_story_dlg_tabw_get_stb_ptr (GapStbTabWidgets *tabw);
void            gap_story_dlg_update_edit_settings(GapStoryBoard *stb
                   , GapStbTabWidgets *tabw
                   );

GtkWidget *    gap_gtk_button_new_from_stock_icon(const char *stock_id);

#endif
