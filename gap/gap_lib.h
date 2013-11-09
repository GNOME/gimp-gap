/* gap_lib.h
 * 1997.11.01 hof (Wolfgang Hofer)
 *
 * GAP ... Gimp Animation Plugins
 *
 * basic GAP types and utility procedures
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
 * along with this program; if not, write to the Free Software
 * <http://www.gnu.org/licenses/>.
 */

/* revision history:
 * 2.1.0a   2004/04/18   hof: added gap_lib_fprintf_gdouble
 * 1.3.26a  2004/02/29   hof: ainfo.type changed from long to GapLibAinfoType
 * 1.3.26a  2004/02/01   hof: added: gap_lib_alloc_ainfo_from_name
 * 1.3.19a  2003/09/06   hof: added gap_lib_searchpath_for_exefile
 * 1.3.16c  2003/07/07   hof: extend ainfo  onion_triggers
 * 1.3.14a  2003/05/27   hof: moved basic gap operations to new module gap_base_ops
 * 1.3.12a  2003/05/02   hof: merge into CVS-gimp-gap project, added gap_renumber, upto 6digit framenumber support
 * 1.3.11a  2003/01/18   hof: added gap_lib_gap_check_save_needed
 * 1.1.35a; 2002/04/21   hof: gap locking (moved to gap_lock.h)
 * 1.1.29a; 2000/11/23   hof: gap locking (changed to procedures and placed here)
 * 1.1.20a; 2000/04/25   hof: new: gap_lib_get_video_paste_name p_clear_video_paste
 * 1.1.14a; 2000/01/02   hof: new: gap_lib_get_frame_nr
 * 1.1.8a;  1999/08/31   hof: new: gap_lib_strdup_del_underscore and gap_lib(base)_strdup_add_underscore
 * 0.99.00; 1999/03/15   hof: prepared for win/dos filename conventions
 * 0.96.02; 1998/08/05   hof: extended gap_dup (duplicate range instead of singele frame)
 *                            added gap_shift (framesequence shift)
 * 0.96.00; 1998/06/27   hof: added gap animation sizechange plugins
 *                            (moved range_ops to separate .h file)
 * 0.94.01; 1998/04/27   hof: added flatten_mode to plugin: gap_range_to_multilayer
 * 0.90.00;              hof: 1.st (pre) release
 */

#ifndef _GAP_LIB_H
#define _GAP_LIB_H

#include "libgimp/gimp.h"
#include "gap_lib_common_defs.h"


/* procedures used in other gap*.c files */

gint32      gap_lib_layer_tracking(gint32 image_id
                    , gchar *ref_layer_name
                    , gint32 ref_layer_stackpos
                    , gboolean setActiveLayer
                    , gboolean ignoreOnionLayers
                    , gboolean trackByName
                    , gboolean trackByStackPosition
                    );



int          gap_lib_file_exists(const char *fname);
char*        gap_lib_searchpath_for_exefile(const char *exefile, const char *path);
int          gap_lib_file_copy(char *fname, char *fname_copy);
void         gap_lib_free_ainfo(GapAnimInfo **ainfo);
char*        gap_lib_alloc_basename(const char *imagename, long *number);
char*        gap_lib_alloc_extension(const char *imagename);
GapAnimInfo* gap_lib_alloc_ainfo_from_name(const char *imagename, GimpRunMode run_mode);
GapAnimInfo* gap_lib_alloc_ainfo(gint32 image_id, GimpRunMode run_mode);
GapAnimInfo* gap_lib_alloc_ainfo_unsaved_image(gint32 image_id);
int          gap_lib_dir_ainfo(GapAnimInfo *ainfo_ptr);
int          gap_lib_chk_framerange(GapAnimInfo *ainfo_ptr);
int          gap_lib_chk_framechange(GapAnimInfo *ainfo_ptr);
char*        gap_lib_build_basename_without_ext(const char *filename);

int    gap_lib_save_named_frame (gint32 image_id, char *sav_name);
int    gap_lib_load_named_frame (gint32 image_id, char *lod_name);
gint32 gap_lib_load_image (char *lod_name);
gint32 gap_lib_save_named_image(gint32 image_id, const char *sav_name, GimpRunMode run_mode);
char*  gap_lib_alloc_fname_fixed_digits(char *basename, long nr, char *extension, long digits);
char*  gap_lib_alloc_fname(char *basename, long nr, char *extension);
char*  gap_lib_alloc_fname6(char *basename, long nr, char *extension, long default_digits);
gboolean gap_lib_exists_frame_nr(GapAnimInfo *ainfo_ptr, long nr, long *l_has_digits);

long  gap_lib_get_frame_nr(gint32 image_id);
long  gap_lib_get_frame_nr_from_name(char *fname);
int   gap_lib_image_file_copy(char *fname, char *fname_copy);

gchar *gap_lib_get_video_paste_name(void);
gint32 gap_vid_edit_clear(void);
gint32 gap_vid_edit_framecount(void);
gint   gap_vid_edit_copy(GimpRunMode run_mode, gint32 image_id, long range_from, long range_to);
gint32 gap_vid_edit_paste(GimpRunMode run_mode, gint32 image_id, long paste_mode);

gboolean gap_lib_gap_check_save_needed(gint32 image_id);

int      gap_lib_rename_frame(GapAnimInfo *ainfo_ptr, long from_nr, long to_nr);
int      gap_lib_delete_frame(GapAnimInfo *ainfo_ptr, long nr);
gint32   gap_lib_replace_image(GapAnimInfo *ainfo_ptr);

long     gap_lib_get_next_available_frame_number(long cur_frame_nr, long stepsize,
            char *basename, char *extension, gboolean *frame_found_ptr);
gboolean gap_lib_framefile_with_framenr_exists(GapAnimInfo *ainfo_ptr, long frame_nr);


#endif


