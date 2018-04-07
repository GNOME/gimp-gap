/*  gap_player_cache.h
 *
 *  This module handles frame caching for GAP video playback
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
 * version 2.2.1;   2006/05/22  hof: created
 */

#ifndef _GAP_PLAYER_CACHE_H
#define _GAP_PLAYER_CACHE_H

#define GAP_PLAYER_CACHE_FRAME_SZIE (3 * 400 * 320)
#define GAP_PLAYER_CACHE_DEFAULT_MAX_BYTESIZE  (200 * GAP_PLAYER_CACHE_FRAME_SZIE) 

#include "libgimp/gimp.h"
#include "gap_lib.h"
#include <gtk/gtk.h>
#include <libgimp/gimp.h>
#include <libgimp/gimpui.h>
#include "gap_pview_da.h"
#include "gap_story_file.h"



typedef enum {
    GAP_PLAYER_CACHE_COMPRESSION_NONE
   ,GAP_PLAYER_CACHE_COMPRESSION_JPEG
  } GapPlayerCacheCompressionType;

typedef enum {
    GAP_PLAYER_CACHE_FRAME_IMAGE
   ,GAP_PLAYER_CACHE_FRAME_MOVIE
   ,GAP_PLAYER_CACHE_FRAME_ANIMIMAGE
  } GapPlayerCacheFrameType;

typedef struct GapPlayerCacheData {
  GapPlayerCacheCompressionType     compression;
  gint32                            th_data_size;
  guchar                           *th_data;
  gint32                            th_width;
  gint32                            th_height;
  gint32                            th_bpp;
  gint32                            flip_status;
  } GapPlayerCacheData;




gint64               gap_player_cache_get_max_bytesize(void);
gint64               gap_player_cache_get_current_bytes_used(void);
gint32               gap_player_cache_get_current_frames_cached(void);
gint64               gap_player_cache_get_gimprc_bytesize(void);

void                 gap_player_cache_set_gimprc_bytesize(gint64 bytesize);
void                 gap_player_cache_set_max_bytesize(gint64 max_bytesize);
GapPlayerCacheData*  gap_player_cache_lookup(const gchar *ckey);
void                 gap_player_cache_insert(const gchar *ckey
                        , GapPlayerCacheData *data);
void                 gap_player_cache_remove_oldest_frame(void);
guchar*              gap_player_cache_decompress(GapPlayerCacheData *cdata);

GapPlayerCacheData*  gap_player_cache_new_data(guchar *th_data
                         , gint32 th_size
                         , gint32 th_width
                         , gint32 th_height
                         , gint32 th_bpp
                         , GapPlayerCacheCompressionType compression
                         , gint32 flip_status
                         );

gchar*               gap_player_cache_new_image_key(const char *filename);
gchar*               gap_player_cache_new_movie_key(const char *filename
                         , gint32  framenr
                         , gint32  seltrack
                         , gdouble delace
                         );
gchar*               gap_player_cache_new_composite_video_key(const char *filename
                         , gint32 framenr
                         , gint32 type
                         , gint32 version);

void                 gap_player_cache_free_all(void);
void                 gap_player_cache_free_cdata(GapPlayerCacheData *cdata);

#endif
