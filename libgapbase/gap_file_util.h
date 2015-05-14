/* gap_file_util.h
 *
 *  GAP common encoder tool procedures
 *  with no dependencies to external libraries.
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

/* revision history:
 * version 1.2.1a;  2004.05.14   hof: created
 */

#ifndef GAP_FILE_UTIL_H
#define GAP_FILE_UTIL_H

#include "libgimp/gimp.h"

/* G_DIR_SEPARATOR (is defined in glib.h if you have glib-1.2.0 or later) */
#ifdef G_OS_WIN32

/* Filenames in WIN/DOS Style */
#ifndef G_DIR_SEPARATOR
#define G_DIR_SEPARATOR '\\'
#endif
#define DIR_ROOT ':'

#else  /* !G_OS_WIN32 */

/* Filenames in UNIX Style */
#ifndef G_DIR_SEPARATOR
#define G_DIR_SEPARATOR '/'
#endif
#define DIR_ROOT '/'

#endif /* !G_OS_WIN32 */

#ifdef G_OS_WIN32
#include <direct.h>             /* For _mkdir() */
#define mkdir(path,mode) _mkdir(path)
#endif


#ifdef G_OS_WIN32
#define GAP_FILE_MKDIR_MODE 0  /* not relevant for WIN mkdir */
#else
#define GAP_FILE_MKDIR_MODE (S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH )
#endif

/* --------------------------*/
/* PROCEDURE DECLARATIONS    */
/* --------------------------*/

gint32      gap_file_get_filesize(const char *fname);
char*       gap_file_load_file(const char *fname);
char*       gap_file_load_file_len(const char *fname, gint32 *filelen);
gint32      gap_file_load_file_segment(const char *filename
                  ,guchar *segment_data_ptr /* IN: pointer to memory at least segent_size large, OUT: filled with segment data */
                  ,gint32 seek_index        /* start byte of datasegment in file */
                  ,gint32 segment_size      /* segment size in byets (must be a multiple of 4) */
                  );

int         gap_file_chmod (const char *fname, int mode);
int         gap_file_mkdir (const char *fname, int mode);
void        gap_file_chop_trailingspace_and_nl(char *buff);
char *      gap_file_make_abspath_filename(const char *filename
                  , const char *container_file);

char *      gap_file_build_absolute_filename(const char * filename);
gint32      gap_file_get_mtime(const char *filename);


#endif        /* GAP_FILE_UTIL_H */
