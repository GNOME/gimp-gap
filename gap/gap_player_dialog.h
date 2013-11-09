/*  gap_player_dialog.h
 *
 *  This module handles GAP video playback
 *  based on thumbnail files
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
 * version 1.3.25c; 2004/01/26  hof: support for storyboard level1 playback
 * version 1.3.14c; 2003/06/09  hof: created
 */

#ifndef _GAP_PLAYER_DIALOG_H
#define _GAP_PLAYER_DIALOG_H

#include "libgimp/gimp.h"
#include "gap_player_main.h"

void    gap_player_dlg_create(GapPlayerMainGlobalParams *gpp);
void    gap_player_dlg_cleanup(GapPlayerMainGlobalParams *gpp);
void    gap_player_dlg_restart(GapPlayerMainGlobalParams *gpp
                      , gboolean autostart
                      , gint32   image_id
                      , char *imagename
                      , gint32 imagewidth
                      , gint32 imageheight
                      , GapStoryBoard *stb_ptr
                      , gint32 begin_frame
                      , gint32 end_frame
                      , gboolean play_selection_only
                      , gint32 seltrack
                      , gdouble delace
                      , const char *preferred_decoder
                      , gboolean  force_open_as_video
                      , gint32 flip_request
                      , gint32 flip_status
                      , gint32 stb_in_track
                      );

void    gap_player_dlg_playback_dialog(GapPlayerMainGlobalParams *gpp);



#endif
