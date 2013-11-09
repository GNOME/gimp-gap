/*  gap_val_file.h
 *
 *  This module handles GAP key/value paramterfiles.
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
 * version 2.1.0a;  2007/05/07  hof: created (splitted of generic key/value parts gap_vin) 
 */

#ifndef _GAP_VAL_FILE_H
#define _GAP_VAL_FILE_H

#include "libgimp/gimp.h"

#define GAP_VAL_MAX_BYTES_PER_LINE 16000


typedef struct GapValTextFileLines {
   char    *line;
   gint32   line_nr;
   void    *next;
} GapValTextFileLines;


typedef enum
{
  GAP_VAL_DUMMY
, GAP_VAL_GINT32
, GAP_VAL_GINT64
, GAP_VAL_GDOUBLE
, GAP_VAL_STRING
, GAP_VAL_GBOOLEAN
, GAP_VAL_G32BOOLEAN
, GAP_VAL_GINT
} GapValDataType;


typedef struct GapValKeyList
{
  char            keyword[50];
  char            comment[80];
  gint32          len;
  gpointer        val_ptr;
  GapValDataType  dataype;
  gboolean        done_flag;
  void *next;
} GapValKeyList;


GapValTextFileLines * gap_val_load_textfile(const char *filename);
void  gap_val_free_textfile_lines(GapValTextFileLines *txf_ptr_root);


GapValKeyList* gap_val_new_keylist(void);
void  gap_val_free_keylist(GapValKeyList *keylist);
void  gap_val_set_keyword(GapValKeyList *keylist
             , const gchar *keyword
             , gpointer val_ptr
             , GapValDataType dataype
             , gint32 len
             , const gchar *comment
             );
int   gap_val_rewrite_file(GapValKeyList *keylist
             , const char *filename
             , const char *hdr_text
             , const char *term_str
             );
int   gap_val_scann_filevalues(GapValKeyList *keylist, const char *filename);


#endif
