/* gap_story_undo_types.h
 *
 *  This module defines types for GAP storyboard undo and redo features.
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

#ifndef _GAP_STORY_UNDO_TYPES_H
#define _GAP_STORY_UNDO_TYPES_H

#include "libgimp/gimp.h"
#include "gap_story_file.h"
#include <gtk/gtk.h>
#include <libgimp/gimp.h>
#include <libgimp/gimpui.h>

typedef enum
  {
     GAP_STB_FEATURE_LATEST
    ,GAP_STB_FEATURE_EDIT_CUT
    ,GAP_STB_FEATURE_EDIT_PASTE
    ,GAP_STB_FEATURE_DND_CUT
    ,GAP_STB_FEATURE_DND_PASTE
    ,GAP_STB_FEATURE_CREATE_CLIP
    ,GAP_STB_FEATURE_CREATE_TRANSITION
    ,GAP_STB_FEATURE_CREATE_SECTION_CLIP
    ,GAP_STB_FEATURE_CREATE_SECTION
    ,GAP_STB_FEATURE_DELETE_SECTION
    ,GAP_STB_FEATURE_PROPERTIES_CLIP
    ,GAP_STB_FEATURE_PROPERTIES_TRANSITION
    ,GAP_STB_FEATURE_PROPERTIES_SECTION
    ,GAP_STB_FEATURE_PROPERTIES_MASTER
    ,GAP_STB_FEATURE_SCENE_SPLITTING
    ,GAP_STB_FEATURE_GENERATE_OTONE
  } GapStoryFeatureEnum;


/* storyboard undo file snapshot element
 */
typedef struct GapStoryUndoFileSnapshot {
  char       *filename;
  char       *filecontent;
  gint32      filesize;
  gint32      mtimefile;
}  GapStoryUndoFileSnapshot;



/* storyboard undo element
 */
typedef struct GapStoryUndoElem {
  GapStoryFeatureEnum  feature_id;
  gint32 clip_story_id;            /* -1 if feature modifies more than 1 clip */
  GapStoryBoard       *stb;        /* storyboard backup before
                                    * feature with feature_id was applied
                                    */
  GapStoryUndoFileSnapshot  *fileSnapshotBefore;
  GapStoryUndoFileSnapshot  *fileSnapshotAfter;
  char                     **filenamePtr;
  struct GapStoryUndoElem   *next;
}  GapStoryUndoElem;




#endif
