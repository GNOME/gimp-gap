/* gap_story_undo.h
 *
 *  This module handles GAP storyboard undo and redo features.
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
 * version 1.3.25a; 2007/10/18  hof: created
 */

#ifndef _GAP_STORY_UNDO_H
#define _GAP_STORY_UNDO_H

#include "libgimp/gimp.h"
#include "gap_lib.h"
#include <gtk/gtk.h>
#include <libgimp/gimp.h>
#include <libgimp/gimpui.h>
#include "gap_story_main.h"
#include "gap_story_file.h"
#include "gap_story_undo_types.h"

void                    gap_stb_undo_debug_fprint_stack(FILE *fp, GapStbTabWidgets *tabw);
const char *            gap_stb_undo_feature_to_string(GapStoryFeatureEnum feature_id);
GapStoryBoard *         gap_stb_undo_pop(GapStbTabWidgets *tabw);
GapStoryBoard *         gap_stb_undo_redo(GapStbTabWidgets *tabw);
void                    gap_stb_undo_destroy_undo_stack(GapStbTabWidgets *tabw);
void                    gap_stb_undo_push_clip(GapStbTabWidgets *tabw
                           , GapStoryFeatureEnum feature_id
                           , gint32 story_id
                           );
void                    gap_stb_undo_push_clip_with_file_snapshot(GapStbTabWidgets *tabw
                           , GapStoryFeatureEnum feature_id, gint32 story_id
                           , char **filenamePtr);

void                    gap_stb_undo_push(GapStbTabWidgets *tabw, GapStoryFeatureEnum feature_id);
void                    gap_stb_undo_group_begin(GapStbTabWidgets *tabw);
void                    gap_stb_undo_group_end(GapStbTabWidgets *tabw);

const char *            gap_stb_undo_get_undo_feature(GapStbTabWidgets *tabw);
const char *            gap_stb_undo_get_redo_feature(GapStbTabWidgets *tabw);
void                    gap_stb_undo_stack_set_unsaved_changes(GapStbTabWidgets *tabw);

#endif
