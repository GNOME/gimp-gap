/*  gap_story_att_trans_dlg.h
 *
 *  This module handles GAP storyboard dialog transition attribute properties window
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
 * version 2.2.1-214; 2006/03/31  hof: created
 */

#ifndef _GAP_STORY_ATT_TRANS_DLG_H
#define _GAP_STORY_ATT_TRANS_DLG_H

#include "libgimp/gimp.h"
#include "gap_story_main.h"


GtkWidget *  gap_story_attw_properties_dialog (GapStbAttrWidget *attw);

void         gap_story_att_stb_elem_properties_dialog ( GapStbTabWidgets *tabw
                                     , GapStoryElem *stb_elem
                                     , GapStoryBoard *stb_dst);
void         gap_story_att_fw_properties_dialog (GapStbFrameWidget *fw);

#endif
