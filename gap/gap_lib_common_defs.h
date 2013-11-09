/*  gap_lib_common_defs.h
 *
 *  This module provides common used definitions and types
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
 * version 2.3.0;   2006/04/14  hof: created
 */

#ifndef _GAP_LIB_COMMON_DEFS_H
#define _GAP_LIB_COMMON_DEFS_H

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

/* GapLibAinfoType enum values are subset of GapStoryRecordType
 * from the sourcefile gap_story_file.h
 */
typedef enum
{
     GAP_AINFO_UNUSED_1
    ,GAP_AINFO_UNUSED_2
    ,GAP_AINFO_IMAGE
    ,GAP_AINFO_ANIMIMAGE    
    ,GAP_AINFO_FRAMES
    ,GAP_AINFO_MOVIE
    ,GAP_AINFO_UNUSED_3
    ,GAP_AINFO_UNKNOWN
} GapLibAinfoType;


typedef struct GapAnimInfo {
   gint32      image_id;
   char        *basename;    /* may include path */
   long         frame_nr; 
   char        *extension;
   char        *new_filename;
   char        *old_filename;
   GimpRunMode run_mode;
   long         width;       
   long         height;      
   long         frame_cnt;   
   long         curr_frame_nr; 
   long         first_frame_nr; 
   long         last_frame_nr;
   long         frame_nr_before_curr_frame_nr;   /* -1 if no frame found before curr_frame_nr */
   long         frame_nr_after_curr_frame_nr;    /* -1 if no frame found after curr_frame_nr */
   
   GapLibAinfoType  ainfo_type;
   gint32           seltrack;    /* input videotrack (used only for GAP_AINFO_MOVIE) */
   gdouble          delace;      /* deinterlace params (used only for GAP_AINFO_MOVIE) */
   gdouble          density;     
} GapAnimInfo;


/* flip_request bits */
#define GAP_STB_FLIP_NONE      0
#define GAP_STB_FLIP_HOR       1
#define GAP_STB_FLIP_VER       2
#define GAP_STB_FLIP_BOTH      3


#define  GAP_VID_PASTE_REPLACE         0
#define  GAP_VID_PASTE_INSERT_BEFORE   1
#define  GAP_VID_PASTE_INSERT_AFTER    2

#define  GAP_LIB_MAX_DIGITS     8
#define  GAP_LIB_DEFAULT_DIGITS 6


typedef enum
{
   GAP_RNGTYPE_FRAMES
  ,GAP_RNGTYPE_LAYER
  ,GAP_RNGTYPE_STORYBOARD
  ,GAP_RNGTYPE_IMAGE
  ,GAP_RNGTYPE_MOVIE
} GapLibTypeInputRange;



#endif




