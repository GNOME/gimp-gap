/* gap_thumbnail.h
 * 2003.05.27 hof (Wolfgang Hofer)
 *
 * GAP ... Gimp Animation Plugins
 *
 * basic GAP thumbnail handling utility procedures
 *
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
 * 1.3.25a  2004/01/21   hof: added gap_thumb_file_load_pixbuf_thumbnail
 * 1.3.24a  2004/01/16   hof: added gap_thumb_file_load_thumbnail
 * 1.3.14b  2003/06/03   hof: removed p_gimp_file_has_valid_thumbnail
 * 1.3.14a  2003/05/27   hof: created
 */

#ifndef _GAP_THUMBNAIL_H
#define _GAP_THUMBNAIL_H

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


char *            gap_thumb_gimprc_query_thumbnailsave(void);
gboolean          gap_thumb_thumbnailsave_is_on(void);
gboolean          gap_thumb_cond_gimp_file_save_thumbnail(gint32 image_id, char* filename);


void              gap_thumb_file_delete_thumbnail(char *filename);
void              gap_thumb_file_copy_thumbnail(char *filename_src, char *filename_dst);
void              gap_thumb_file_rename_thumbnail(char *filename_src, char *filename_dst);
gboolean          gap_thumb_file_load_thumbnail(char* filename
                                               , gint32 *th_width, gint32 *th_height
                                               , gint32 *th_data_count
                                               , gint32 *th_bpp
                                               , unsigned char **th_data);


GdkPixbuf *       gap_thumb_file_load_pixbuf_thumbnail(char* filename
                                    , gint32 *th_width
                                    , gint32 *th_height
                                    , gint32 *th_bpp);
#endif
