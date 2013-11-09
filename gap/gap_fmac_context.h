/* gap_fmac_context.h
 *
 *
 *  This module handles the filtermacro context.
 *  The filtermacro context is used for itration of "persistent drawable ids" 
 *  for animated (or constant) filter apply.
 *  If gap controlled filterapply is done via a filtermacro
 *  the iteration is done within a filtermacro context.
 *  (e.g. while filtermacro is beeing recorded or is applied)
 *  the handled drawable ids are mapped with the help of a filtermacro reference file
 *  In this case the "persitent_drawable_id" is used to open the referenced
 *  image, frame or videoframe at apply time (this may happen in another gimp
 *  session than the recording of the filtermacro was done).
 *  
 *
 * Copyright (C) 2008 Wolfgang Hofer <hof@gimp.org>
 *
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

#ifndef _GAP_FMAC_CONTEXT_H
#define _GAP_FMAC_CONTEXT_H

#include "libgimp/gimp.h"
#include "libgimp/gimp.h"
#include "gap_lib_common_defs.h"

typedef struct GapFmacRefEntry {
  GapLibAinfoType ainfo_type;
  gint32      persistent_drawable_id;
  gint32      frame_nr;
  gint32      stackposition;
  gint32      track;
  gint32      mtime;
  char        filename[1024];
  struct GapFmacRefEntry *next;
} GapFmacRefEntry;


typedef struct GapFmacContext {
  gboolean recording_mode;
  gboolean enabled;
  gint32 ffetch_user_id;
  char persistent_id_lookup_filename[1024];
  GapFmacRefEntry  *fmref_list;
} GapFmacContext;



#define GAP_FMREF_FILENAME_EXTENSION  ".fmref"
#define GAP_FMREF_FILEHEADER          "GAP_FILTERMACRO_PERSITENT_DRAWABLE_ID_LOOKUP_FILE"
#define GAP_FMREF_ID         "id:"
#define GAP_FMREF_FRAME_NR   "frameNr:"
#define GAP_FMREF_STACK      "stack:"
#define GAP_FMREF_TRACK      "track:"
#define GAP_FMREF_MTIME      "mtime:"
#define GAP_FMREF_TYPE       "type:"
#define GAP_FMREF_FILE       "file:"


#define GAP_FMAC_CONTEXT_KEYWORD   "GAP_FMAC_CONTEXT_KEYWORD"
#define GAP_FMCT_MIN_PERSISTENT_DRAWABLE_ID  800000

void                 gap_fmct_set_derived_lookup_filename(GapFmacContext *fmacContext, const char *filename);
void                 gap_fmct_setup_GapFmacContext(GapFmacContext *fmacContext, gboolean recording_mode, const char *filename);
void                 gap_fmct_disable_GapFmacContext(void);
void                 gap_fmct_debug_print_GapFmacContext(GapFmacContext *fmacContext);

void                 gap_fmct_load_GapFmacContext(GapFmacContext *fmacContext);
void                 gap_fmct_save_GapFmacContext(GapFmacContext *fmacContext);
GapFmacRefEntry *    gap_fmct_get_GapFmacRefEntry_by_persitent_id(GapFmacContext *fmacContext
                                  , gint32 persistent_drawable_id);
void                 gap_fmct_free_GapFmacRefList(GapFmacContext *fmacContext);
gint32               gap_fmct_add_GapFmacRefEntry(GapLibAinfoType ainfo_type
                                  , gint32 frame_nr
                                  , gint32 stackposition
                                  , gint32 track
                                  , gint32 drawable_id
                                  , const char *filename
                                  , gboolean force_id
                                  , GapFmacContext *fmacContext);

#endif
