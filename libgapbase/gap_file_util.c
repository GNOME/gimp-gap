/* gap_file_util.c
 *
 *  GAP video encoder tool procedures
 *   - filehandling
 *   - GAP filename (framename) handling
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

/* SYTEM (UNIX) includes */
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>

#include <glib/gstdio.h>

/* GIMP includes */
#include "gtk/gtk.h"
#include "libgimp/gimp.h"


#include "gap_file_util.h"

extern      int gap_debug; /* ==0  ... dont print debug infos */

/*************************************************************
 *          TOOL FUNCTIONS                                   *
 *************************************************************/


/* --------------------
 * gap_file_get_filesize
 * --------------------
 *   get the filesize of a file.
 * in: fname: The filename of the file to check.
 * returns: The filesize.
 */
gint32
gap_file_get_filesize(const char *fname)
{
  GStatBuf  stat_buf;

  if (0 != g_stat(fname, &stat_buf))
  {
    printf ("stat error on '%s'\n", fname);
    return(0);
  }

  return((gint32)(stat_buf.st_size));
}


/* -----------------------
 * gap_file_load_file_segment
 * -----------------------
 * load one segment of (audio)data from a file.
 * into the buffer segment_data_ptr (must be allocated by the calling program)
 *
 * return number of bytes read from file
 */
gint32
gap_file_load_file_segment(const char *filename
                  ,guchar *segment_data_ptr /* IN: pointer to memory at least segent_size large, OUT: filled with segment data */
                  ,gint32 seek_index        /* start byte of datasegment in file */
                  ,gint32 segment_size      /* segment size in byets (must be a multiple of 4) */
                  )
{
  FILE *fp;
  gint32      l_bytes_read;


  l_bytes_read = 0;

  fp = g_fopen(filename, "rb");
  if (fp)
  {

    fseek(fp, seek_index, SEEK_SET);

    /* read full segement size (or smaller rest until EOF) */
    l_bytes_read = fread(segment_data_ptr, 1, (size_t)segment_size, fp);

    fclose(fp);
  }

  if(gap_debug) printf("gap_file_load_file_segment: %s seek_idx:%d  bytes_read:%d\n", filename, (int)seek_index, (int)l_bytes_read);

  return(l_bytes_read);

}  /* end gap_file_load_file_segment */


/* -------------------------
 * gap_file_load_file_len
 * -------------------------
 *    Load a file into a memory buffer
 *  in: fname: The name of the file to load
 *  returns: A pointer to the allocated buffer with the file content.
 */
char *
gap_file_load_file_len(const char *fname, gint32 *filelen)
{
  FILE        *fp;
  char        *l_buff_ptr;
  long         len;

  *filelen = 0;

  /* File Laenge ermitteln */
  len = gap_file_get_filesize(fname);
  if (len < 1)
  {
    return(NULL);
  }

  /* Buffer allocate */
  l_buff_ptr=(char *)g_malloc(len+1);
  if(l_buff_ptr == NULL)
  {
    printf ("calloc error (%d Bytes not available)\n", (int)len);
    return(NULL);
  }
  l_buff_ptr[len] = '\0';

  /* File in eine Buffer laden */
  fp = g_fopen(fname, "rb");                /* open read */
  if(fp == NULL)
  {
    printf ("open(read) error on '%s'\n", fname);
    return(NULL);
  }
  fread(l_buff_ptr, 1, (size_t)len, fp);            /* read */
  fclose(fp);                               /* close */

  *filelen = len;
  return(l_buff_ptr);
}       /* end gap_file_load_file_len */

/* -------------------------
 * gap_file_load_file
 * -------------------------
 */
char *
gap_file_load_file(const char *fname)
{
  gint32 l_filelen;

  return(gap_file_load_file_len(fname, &l_filelen));
}  /* end gap_file_load_file */

/* -------------------------
 * gap_file_chmod
 * -------------------------
 */
int
gap_file_chmod (const char *fname, int mode)
{
  return(g_chmod(fname, mode));
}

/* -------------------------
 * gap_file_mkdir
 * -------------------------
 */
int
gap_file_mkdir (const char *fname, int mode)
{
  return(g_mkdir(fname, mode));
}



/* ----------------------------------------------------
 * gap_file_chop_trailingspace_and_nl
 * ----------------------------------------------------
 * requires a '\0' terminated input buffer 
 * chop trailing white space and newline chars
 * (by replacing those chars with \0 directly in the
 *  specified buffer)
 */
void
gap_file_chop_trailingspace_and_nl(char *buffer)
{
  int len;
  len = strlen(buffer);
  while(len > 0)
  {
    len--;
    if((buffer[len] == '\n')
    || (buffer[len] == '\r')
    || (buffer[len] == '\t')
    || (buffer[len] == ' '))
    {
      buffer[len] = '\0';
    }
    else
    {
      break;
    }
  }
}  /* end gap_file_chop_trailingspace_and_nl */


/* ----------------------------------------------------
 * gap_file_make_abspath_filename
 * ----------------------------------------------------
 * check if filename is specified with absolute path,
 * if true: return 1:1 copy of filename
 * if false: prefix filename with path from container file.
 */
char *
gap_file_make_abspath_filename(const char *filename, const char *container_file)
{
    gboolean l_path_is_relative;
    char    *l_container_path;
    char    *l_ptr;
    char    *l_abs_name;

    l_abs_name = NULL;
    l_path_is_relative = TRUE;
    if(filename[0] == G_DIR_SEPARATOR)
    {
      l_path_is_relative = FALSE;
    }
#ifdef G_OS_WIN32
    {
      gint l_idx;

      /* check for WIN/DOS styled abs pathname "Drive:\dir\file" */
      for(l_idx=0; filename[l_idx] != '\0'; l_idx++)
      {
        if(filename[l_idx] == DIR_ROOT)
        {
          l_path_is_relative = FALSE;
          break;
        }
      }
    }
#endif

    if((l_path_is_relative) && (container_file != NULL))
    {
      l_container_path = g_strdup(container_file);
      l_ptr = &l_container_path[strlen(l_container_path)];
      while(l_ptr != l_container_path)
      {
        if((*l_ptr == G_DIR_SEPARATOR) || (*l_ptr == DIR_ROOT))
        {
          l_ptr++;
          *l_ptr = '\0';   /* terminate the string after the directorypath */
          break;
        }
        l_ptr--;
      }
      if(l_ptr != l_container_path)
      {
        /* prefix the filename with the container_path */
        l_abs_name  = g_strdup_printf("%s%s", l_container_path, filename);
      }
      else
      {
        /* container_file has no path at all (and must be in the current dir)
         * (therefore we cant expand absolute path name)
         */
        l_abs_name  = g_strdup(filename);
      }
      g_free(l_container_path);
    }
    else
    {
      /* use absolute filename as it is */
      l_abs_name  = g_strdup(filename);
    }

    return(l_abs_name);

}  /* end gap_file_make_abspath_filename */


/* --------------------------------
 * gap_file_build_absolute_filename
 * --------------------------------
 */
char *
gap_file_build_absolute_filename(const char * filename)
{
 gchar *absolute;
  if (! g_path_is_absolute (filename))
  {
      gchar *current;

      current = g_get_current_dir ();
      absolute = g_build_filename (current, filename, NULL);
      g_free (current);
  }
  else
  {
      absolute = g_strdup (filename);
  }
  return (absolute);
}  /* end gap_file_build_absolute_filename */

/* --------------------------------
 * gap_file_get_mtime
 * --------------------------------
 */
time_t
gap_file_get_mtime(const char *filename)
{
  GStatBuf  l_stat;

  if(filename != NULL)
  {
    if (0 == g_stat(filename, &l_stat))
    {
      return(l_stat.st_mtime);
    }
  }
  return(0);
  
}  /* end gap_file_get_mtime */


/* --------------------------------
 * gap_file_printf
 * --------------------------------
 * printf to an optional logfile (specified via environment variable GAP_DEBUG_LOGFILE)
 * or to stdout as default (when GAP_DEBUG_LOGFILE not set)
 */
void
gap_file_printf(const char *fmt,
                  ...)
{
  static const char *gapDebugLogFileName = NULL; //"gap_logfile.txt";
  FILE *fp;

  fp = NULL;
  if(gapDebugLogFileName == NULL)
  {
    gapDebugLogFileName = g_getenv("GAP_DEBUG_LOGFILE");
  }
  
  if(gapDebugLogFileName != NULL)
  {
    if(g_file_test(gapDebugLogFileName, G_FILE_TEST_EXISTS))
    {
      fp = g_fopen(gapDebugLogFileName, "ab");
    }
    else
    {
      fp = g_fopen(gapDebugLogFileName, "wb");
    }
  }

  if(fp != NULL)
  {
    va_list argp;
    va_start(argp, fmt);
    vfprintf(fp, fmt, argp);
    va_end(argp);
    fclose(fp);
  
  }
  else
  {
    va_list argp;
    va_start(argp, fmt);
    vprintf(fmt, argp);
    va_end(argp);
  }

}  /* end gap_file_printf */
