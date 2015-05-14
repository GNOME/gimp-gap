/* gap_fmac_context.c
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
 * along with this program; if not, see
 * <http://www.gnu.org/licenses/>.
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <glib/gstdio.h>

/* GAP includes */
#include "gap_libgapbase.h"
#include "gap_fmac_context.h"
#include "gap_frame_fetcher.h"
#include "gap_val_file.h"


extern      int gap_debug; /* ==0  ... dont print debug infos */

char *                      p_parse_string(char **scan_ptr);
static void                 p_parse_and_add_GapFmacRefEntry(GapFmacContext *fmacContext, char *line_ptr);


/* --------------------------------------------
 * gap_fmct_set_derived_lookup_filename
 * --------------------------------------------
 * set persistent_id_lookup_filename = filename.fmref
 */
void
gap_fmct_set_derived_lookup_filename(GapFmacContext *fmacContext, const char *filename)
{
  if ((fmacContext) && (filename))
  {
    g_snprintf(fmacContext->persistent_id_lookup_filename , sizeof(fmacContext->persistent_id_lookup_filename), "%s%s"
       ,filename
       ,GAP_FMREF_FILENAME_EXTENSION
       );
  }
}  /* end gap_fmct_set_derived_lookup_filename */


/* --------------------------------------------
 * gap_fmct_setup_GapFmacContext
 * --------------------------------------------
 * setup a filtermacro context for recording or playback mode.
 * the context affects the behaviour of drawable_iteration in the whole gimp_session
 * until the procedure gap_fmct_disable_GapFmacContext is called.
 * This procedure registers a new frame fetcher resource user_id if playback (e.g NOT recording_mode)
 * is used. The drawable_iteration will refere to this ffetch_user_id when fetching
 * frames via mapped persistent drawable ids.
 * 
 * The current implementation can NOT handle parallell processing of multiple filtermacros
 * because there is only ONE context that will be overwritten in case of concurrent calls.
 */
void
gap_fmct_setup_GapFmacContext(GapFmacContext *fmacContext, gboolean recording_mode, const char *filename)
{
  gap_fmct_set_derived_lookup_filename(fmacContext, filename);
  fmacContext->recording_mode = recording_mode;
  fmacContext->enabled = TRUE;
  if (recording_mode)
  {
    fmacContext->ffetch_user_id = -1;
  }
  else
  {
    fmacContext->ffetch_user_id = gap_frame_fetch_register_user("gap_fmct_setup_GapFmacContext");
  }
  fmacContext->fmref_list = NULL;
  gimp_set_data(GAP_FMAC_CONTEXT_KEYWORD, fmacContext, sizeof(GapFmacContext));
}  /* end gap_fmct_setup_GapFmacContext */


/* --------------------------------------------
 * gap_fmct_disable_GapFmacContext
 * --------------------------------------------
 * disable the gimp session wide global filtermacro context.
 * (this also unregisters frame fetcher resource usage if
 * there was a valid context in playback mode at calling time)
 */
void
gap_fmct_disable_GapFmacContext(void)
{
  gint            sizeFmacContext;
  
  sizeFmacContext = gimp_get_data_size(GAP_FMAC_CONTEXT_KEYWORD);

  if(sizeFmacContext == sizeof(GapFmacContext))
  {
    GapFmacContext *fmacContext;
    fmacContext = g_malloc0(sizeFmacContext);

    if(fmacContext)
    {
      gimp_get_data(GAP_FMAC_CONTEXT_KEYWORD, fmacContext);
      fmacContext->enabled = FALSE;
      fmacContext->fmref_list = NULL;
      if(fmacContext->ffetch_user_id >= 0)
      {
        /* unregister (shall drop cached resources of the frame fetcher when last user unregisters) */
        gap_frame_fetch_unregister_user(fmacContext->ffetch_user_id);
      }
      gimp_set_data(GAP_FMAC_CONTEXT_KEYWORD, fmacContext, sizeof(GapFmacContext));
      g_free(fmacContext);
    }
  }
}  /* end gap_fmct_disable_GapFmacContext */


/* --------------------------------------------
 * gap_fmct_free_GapFmacRefList
 * --------------------------------------------
 * free all elements of the fmacContext->fmref_list.
 */
void
gap_fmct_free_GapFmacRefList(GapFmacContext *fmacContext)
{
  GapFmacRefEntry *fmref_entry;
  GapFmacRefEntry *fmref_next;

  fmref_entry = fmacContext->fmref_list;
  while(fmref_entry != NULL)
  {
    fmref_next = fmref_entry->next;
    g_free(fmref_entry);
    fmref_entry = fmref_next;
  }
  fmacContext->fmref_list = NULL;

}  /* end gap_fmct_free_GapFmacRefList */



/* --------------------------
 * p_parse_string_raw
 * --------------------------
 * return a pointer to the start of the next non-white space character
 *        in this line and advance the scan_ptr to the next white space
 *        or to one position after the ':' (that terminates key strings)
 *        NULL is returned if there is no (more) non-white space character
 *        left in the line rest (starting at *scan_ptr).
 * the scan_ptr is rest to NULL when end of line \n or end of string \0
 * is reached.
 */
char *
p_parse_string_raw(char **scan_ptr)
{
  char *ptr;
  char *result_ptr;
  
  result_ptr = NULL;
  ptr = *scan_ptr;

  /* advance to 1st non blank position */
  while(ptr)
  {
    if ((*ptr == '\0') || (*ptr == '\n'))
    {
      /* end of line (or string) reached */
      *scan_ptr = NULL;
      return (result_ptr);
    }
    if ((*ptr != ' ') && (*ptr != '\t'))
    {
      /* first non-white space character found */
      result_ptr = ptr;
      break;
    }
    ptr++;  /* skip white space and continue */
  }
  
  
  while(ptr)
  {
    ptr++;
    if ((*ptr == '\0') || (*ptr == '\n'))
    {
      *scan_ptr = NULL;
      return (result_ptr);
    }
    if (*result_ptr == '"')
    {
      if(*ptr == '"')
      {
        ptr++;
        *scan_ptr = ptr;
        return (result_ptr);
      }
    }
    else
    {
      if ((*ptr == ' ') || (*ptr == '\t'))
      {
        *scan_ptr = ptr;
        return (result_ptr);
      }
      if (*ptr == ':')
      {
        ptr++;
        *scan_ptr = ptr;
        return (result_ptr);
      }
    }
  }
  
  return (result_ptr);
}  /* end p_parse_string_raw */

/* --------------------------
 * p_parse_string
 * --------------------------
 * return a pointer to the start of the next non-white space character
 *        in this line and advance the scan_ptr to the next white space
 *        or to one position after the ':' (that terminates key strings)
 *        NULL is returned if there is no (more) non-white space character
 *        left in the line rest (starting at *scan_ptr).
 * the scan_ptr is rest to NULL when end of line \n or end of string \0
 * is reached.
 */
char *
p_parse_string(char **scan_ptr)
{
  char *l_ptr;
  char *l_result;

  l_result =  NULL;
  l_ptr = p_parse_string_raw(scan_ptr);
  if(l_ptr != NULL)
  {
    l_result = g_strdup(l_ptr);
    if (*scan_ptr != NULL)
    {
      l_result[*scan_ptr - l_ptr] = '\0';
    }
  }
  if(gap_debug)
  {
    if(l_result)
    {
      printf("p_parse_string:(%s)\n", l_result);
    }
    else
    {
      printf("p_parse_string:(NULL)\n");
    }
  }
  return (l_result);
}

/* --------------------------------------------
 * p_parse_and_add_GapFmacRefEntry
 * --------------------------------------------
 * parse one record and add it to
 * the the fmacContext->fmref_list on success.
 */
static void
p_parse_and_add_GapFmacRefEntry(GapFmacContext *fmacContext, char *line_ptr)
{
  char *l_key;
  char *l_value;
  char *scan_ptr;

  long id;
  long ainfo_type;
  long frame_nr;
  long stackposition;
  long track;
  long mtime;
  char *filename;

  id = -1;
  ainfo_type = -1;
  frame_nr = -1;
  stackposition = -1;
  track = -1;
  mtime = -1;
  filename = NULL;
  
  scan_ptr = line_ptr;
  while (scan_ptr != NULL)
  {
    l_key = p_parse_string(&scan_ptr);
    if (l_key == NULL)
    {
      break;
    }
    l_value = p_parse_string(&scan_ptr);
    if (l_value == NULL)
    {
      g_free(l_key);
      break;
    }
    
    if (strcmp(l_key, GAP_FMREF_ID) == 0)
    {
      id = atol(l_value);
    } 
    else if (strcmp(l_key, GAP_FMREF_FRAME_NR) == 0)
    {
      frame_nr = atol(l_value);
    }
    else if (strcmp(l_key, GAP_FMREF_STACK) == 0)
    {
      stackposition = atol(l_value);
    }
    else if (strcmp(l_key, GAP_FMREF_TRACK) == 0)
    {
      track = atol(l_value);
    }
    else if (strcmp(l_key, GAP_FMREF_MTIME) == 0)
    {
      mtime = atol(l_value);
    }
    else if (strcmp(l_key, GAP_FMREF_TYPE) == 0)
    {
      ainfo_type = atol(l_value);
    }
    else if (strcmp(l_key, GAP_FMREF_FILE) == 0)
    {
      int len;
      char *l_raw_filename;
      
      l_raw_filename = l_value;
      if(gap_debug)
      {
        printf("RAW filename:%s:\n", l_raw_filename);
      }
      if (*l_raw_filename == '"')
      {
        l_raw_filename++;
      }
      len = strlen(l_raw_filename);
      if (len > 0)
      {
        if (l_raw_filename[len -1] == '"')
        {
          l_raw_filename[len -1] = '\0';
        }
      }
      filename = g_strdup(l_raw_filename);
    }
    else
    {
      printf("WARNING: unkonwn token: %s\n", l_key);
    }

    g_free(l_key);
    g_free(l_value);
  }

  /* now check if required attributes are present */
  if (id < 0)
  {
    printf("ERROR: incomplete record, expected: %s is missing", GAP_FMREF_ID);
    return;
  }
  if (frame_nr < 0)
  {
    printf("ERROR: incomplete record, expected: %s is missing", GAP_FMREF_FRAME_NR);
    return;
  }
  if (filename == NULL)
  {
    printf("ERROR: incomplete record, expected: %s is missing", GAP_FMREF_FILE);
    return;
  }
  if (ainfo_type < 0)
  {
    printf("ERROR: incomplete record, expected: %s is missing", GAP_FMREF_TYPE);
    return;
  }
  if (mtime < 0)
  {
    printf("ERROR: incomplete record, expected: %s is missing", GAP_FMREF_MTIME);
    return;
  }

  /* create and add the scanned entry to the list */
  gap_fmct_add_GapFmacRefEntry(ainfo_type
      , frame_nr
      , stackposition
      , track
      , id
      , filename
      , TRUE            /* force_id scanned from file (dont generate a new one ) */
      , fmacContext
      );

  if (filename != NULL)
  {
    g_free(filename);
  }

}  /* end p_parse_and_add_GapFmacRefEntry */


/* --------------------------------------------
 * gap_fmct_load_GapFmacContext
 * --------------------------------------------
 */
void
gap_fmct_load_GapFmacContext(GapFmacContext *fmacContext)
{
  GapValTextFileLines *txf_ptr_root;
  GapValTextFileLines *txf_ptr;
  gint32 line_nr;


  gap_fmct_free_GapFmacRefList(fmacContext);

  txf_ptr_root = gap_val_load_textfile(fmacContext->persistent_id_lookup_filename);
  line_nr = 0;

  for(txf_ptr = txf_ptr_root; txf_ptr != NULL; txf_ptr = (GapValTextFileLines *) txf_ptr->next)
  {
    gint l_len;

    line_nr++;
    if(gap_debug)
    {
      printf("line_nr: %d\n", (int)line_nr);
    }
    gap_file_chop_trailingspace_and_nl(&txf_ptr->line[0]);
    l_len = strlen(txf_ptr->line);

    if(gap_debug)
    {
      printf("line:%s:\n", txf_ptr->line);
    }
    
    if (line_nr == 1)
    {
      if (strncmp(txf_ptr->line, GAP_FMREF_FILEHEADER, strlen(GAP_FMREF_FILEHEADER)) != 0)
      {
        printf("ERROR: file:%s does not start with expected header:%s\n"
               , fmacContext->persistent_id_lookup_filename
               , GAP_FMREF_FILEHEADER
               );
        break;
      }
    }
    else
    {
      if (*txf_ptr->line != '#')
      {
        p_parse_and_add_GapFmacRefEntry(fmacContext, txf_ptr->line);
      }
    }

  }

  if(txf_ptr_root)
  {
    gap_val_free_textfile_lines(txf_ptr_root);
  }
  
  if(gap_debug)
  {
    gap_fmct_debug_print_GapFmacContext(fmacContext);
  }

}  /* end gap_fmct_load_GapFmacContext */


/* --------------------------------------------
 * gap_fmct_save_GapFmacContext
 * --------------------------------------------
 * save the list of filtermacro references (to persistent drawable ids)
 * in the filename that is specified via persistent_id_lookup_filename
 * in the filtermacro context.
 * The file created or completely overwritten.
 */
void
gap_fmct_save_GapFmacContext(GapFmacContext *fmacContext)
{
  GapFmacRefEntry *fmref_entry;
  FILE            *fp;

  fp = g_fopen(fmacContext->persistent_id_lookup_filename, "w");
  if (fp == NULL)
  {
    printf("ERROR: could not open write file:%s\n", fmacContext->persistent_id_lookup_filename);
    return;
  }
 
  fprintf(fp, "%s\n", GAP_FMREF_FILEHEADER);
  fprintf(fp, "# type: %d=IMAGE,%d=ANIMIMAGE,%d=FRAMES,%d=MOVIE\n"
          , GAP_AINFO_IMAGE, GAP_AINFO_ANIMIMAGE, GAP_AINFO_FRAMES, GAP_AINFO_MOVIE);

  /* write all reference entries in the following format: 
   * id:800000 frameNr:000001 stack:-1 file:"frame_000001.xcf"   mtime:123455678
   */
  for(fmref_entry = fmacContext->fmref_list; fmref_entry != NULL; fmref_entry = fmref_entry->next)
  {
    fprintf(fp, "%s%06d %s%06d %s%02d %s%d %s%011d  %s%d %s\"%s\"\n"
           , GAP_FMREF_ID         , fmref_entry->persistent_drawable_id
           , GAP_FMREF_FRAME_NR   , fmref_entry->frame_nr
           , GAP_FMREF_STACK      , fmref_entry->stackposition
           , GAP_FMREF_TRACK      , fmref_entry->track
           , GAP_FMREF_MTIME      , fmref_entry->mtime
           , GAP_FMREF_TYPE       , fmref_entry->ainfo_type
           , GAP_FMREF_FILE       , fmref_entry->filename
           );
  }
  
  fclose(fp);

}  /* end gap_fmct_save_GapFmacContext */

/* --------------------------------------------
 * gap_fmct_debug_print_GapFmacContext
 * --------------------------------------------
 * print the current filtermacro context for debug purpose.
 */
void
gap_fmct_debug_print_GapFmacContext(GapFmacContext *fmacContext)
{
  GapFmacRefEntry *fmref_entry;

  printf("fmacContext START  persistent_id_lookup_filename:%s\n"
      , fmacContext->persistent_id_lookup_filename
      );

  printf("  recording_mode:%d  enabled:%d ffetch_user_id:%d\n"
      , fmacContext->recording_mode
      , fmacContext->enabled
      , fmacContext->ffetch_user_id
      ); 

  for(fmref_entry = fmacContext->fmref_list; fmref_entry != NULL; fmref_entry = fmref_entry->next)
  {
    printf("%s%06d %s%06d %s%02d %s%d %s%011d  %s%d %s\"%s\"\n"
           , GAP_FMREF_ID         , fmref_entry->persistent_drawable_id
           , GAP_FMREF_FRAME_NR   , fmref_entry->frame_nr
           , GAP_FMREF_STACK      , fmref_entry->stackposition
           , GAP_FMREF_TRACK      , fmref_entry->track
           , GAP_FMREF_MTIME      , fmref_entry->mtime
           , GAP_FMREF_TYPE       , fmref_entry->ainfo_type
           , GAP_FMREF_FILE       , fmref_entry->filename
           );
  }

  printf("fmacContext END persistent_id_lookup_filename:%s\n"
      , fmacContext->persistent_id_lookup_filename
      );

}  /* end gap_fmct_debug_print_GapFmacContext */



/* --------------------------------------------
 * gap_fmct_get_GapFmacRefEntry_by_persitent_id
 * --------------------------------------------
 */
GapFmacRefEntry *
gap_fmct_get_GapFmacRefEntry_by_persitent_id(GapFmacContext *fmacContext, gint32 persistent_drawable_id)
{
  GapFmacRefEntry *fmref_entry;

  for(fmref_entry = fmacContext->fmref_list; fmref_entry != NULL; fmref_entry = fmref_entry->next)
  {
    if (fmref_entry->persistent_drawable_id == persistent_drawable_id)
    {
      return(fmref_entry);
    }
  }
  return (NULL);
  
}  /* end gap_fmct_get_GapFmacRefEntry_by_persitent_id */

/* --------------------------------------------
 * gap_fmct_add_GapFmacRefEntry
 * --------------------------------------------
 */
gint32
gap_fmct_add_GapFmacRefEntry(GapLibAinfoType ainfo_type
  , gint32 frame_nr
  , gint32 stackposition
  , gint32 track
  , gint32 drawable_id
  , const char *filename
  , gboolean force_id
  , GapFmacContext *fmacContext)
{
  GapFmacRefEntry *new_fmref_entry;
  GapFmacRefEntry *fmref_entry;
  gint32           l_max_persistent_drawable_id;
  
  if(filename == NULL)
  {
     return (drawable_id);
  } 
  if(!g_file_test(filename, G_FILE_TEST_EXISTS))
  {
    return (drawable_id);
  }
  if(fmacContext == NULL)
  {
     return (drawable_id);
  } 


  l_max_persistent_drawable_id = GAP_FMCT_MIN_PERSISTENT_DRAWABLE_ID;

  /* check if entry already present in the list */
  for(fmref_entry = fmacContext->fmref_list; fmref_entry != NULL; fmref_entry = fmref_entry->next)
  {
    if((fmref_entry->ainfo_type == ainfo_type)
    && (fmref_entry->frame_nr == frame_nr)
    && (fmref_entry->stackposition == stackposition)
    && (fmref_entry->track == track)
    && (strcmp(fmref_entry->filename, filename) == 0))
    {
      /* the list already contains an equal entry, nothing left to do.. */
      return(fmref_entry->persistent_drawable_id);
    }
    l_max_persistent_drawable_id = MAX(l_max_persistent_drawable_id, fmref_entry->persistent_drawable_id);
  }

  /* add the new entry (and assign a unique persistent_drawable_id) */
  new_fmref_entry = g_new(GapFmacRefEntry, 1);

  if(force_id)
  {
    new_fmref_entry->persistent_drawable_id = drawable_id;
  }
  else
  {
    new_fmref_entry->persistent_drawable_id = l_max_persistent_drawable_id + 1;
  }
  new_fmref_entry->ainfo_type = ainfo_type;
  new_fmref_entry->frame_nr = frame_nr;
  new_fmref_entry->stackposition = stackposition;
  new_fmref_entry->track = track;
  new_fmref_entry->mtime = gap_file_get_mtime(filename);
  g_snprintf(new_fmref_entry->filename, sizeof(new_fmref_entry->filename), "%s", filename);

    
  new_fmref_entry->next = fmacContext->fmref_list;
  fmacContext->fmref_list = new_fmref_entry;

  gap_fmct_save_GapFmacContext(fmacContext);

  return(new_fmref_entry->persistent_drawable_id);

}  /* end gap_fmct_add_GapFmacRefEntry */

