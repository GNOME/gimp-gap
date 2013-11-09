/* gap_gve_png.c by Wolfgang Hofer
 *
 * (GAP ... GIMP Animation Plugins, now also known as GIMP Video)
 *
 *
 * In short, this module contains
 * .) software PNG encoder or
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


/* revision history (see svn)
 * 2008.06.21   hof: created
 */


/* SYSTEM (UNIX) includes */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>
#include <unistd.h>

#include <glib/gstdio.h>

/* GIMP includes */
#include "gtk/gtk.h"
#include "libgimp/gimp.h"

/* GAP includes */
#include "gap_libgapbase.h"
#include "gap_pdb_calls.h"

#include "gtk/gtk.h"

extern int gap_debug;

/* --------------------------------
 * p_save_as_tmp_png_file
 * --------------------------------
 * call the GIMP PNG file save plug-in as "PNG" encoding engine.
 */
gboolean
p_save_as_tmp_png_file(const char *filename, gint32 image_id, gint32 drawable_id
   , gint32 png_interlaced, gint32 png_compression)
{
   static char     *l_called_proc = "file-png-save2";
   GimpParam          *return_vals;
   int              nreturn_vals;

   if(gap_debug)
   {
     printf("GAP: PNG encode via call of %s on filename: %s, image_id:%d, drawable_id:%d\n"
            , l_called_proc
            , filename
            , image_id
            , drawable_id
            );
   }

   return_vals = gimp_run_procedure (l_called_proc,
                                 &nreturn_vals,
                                 GIMP_PDB_INT32,     GIMP_RUN_NONINTERACTIVE /* runmode */,
                                 GIMP_PDB_IMAGE,     image_id,
                                 GIMP_PDB_DRAWABLE,  drawable_id,
                                 GIMP_PDB_STRING,    filename,
                                 GIMP_PDB_STRING,    filename,
                                 GIMP_PDB_INT32,     png_interlaced,
                                 GIMP_PDB_INT32,     png_compression,
                                 GIMP_PDB_INT32,     0,      /* Write bKGD chunk?  */
                                 GIMP_PDB_INT32,     0,      /* Write gAMA chunk?  */
                                 GIMP_PDB_INT32,     0,      /* Write oFFs chunk?  */
                                 GIMP_PDB_INT32,     0,      /* Write pHYs chunk?  */
                                 GIMP_PDB_INT32,     0,      /* Write tIME chunk?  */
                                 GIMP_PDB_INT32,     0,      /* Write comment?  */
                                 GIMP_PDB_INT32,     0,      /* Preserve color of transparent pixels?  */
                                 GIMP_PDB_END);

   if (return_vals[0].data.d_status == GIMP_PDB_SUCCESS)
   {
      gimp_destroy_params(return_vals, nreturn_vals);
      return (TRUE);
   }

   gimp_destroy_params(return_vals, nreturn_vals);
   printf("GAP: Error: PDB call of %s failed on filename: %s, image_id:%d, drawable_id:%d, d_status:%d %s\n"
          , l_called_proc
          , filename
          , image_id
          , drawable_id
          , (int)return_vals[0].data.d_status
          , gap_status_to_string(return_vals[0].data.d_status)
          );
   return(FALSE);

}  /* end p_save_as_tmp_png_file */



/* --------------------------------
 * gap_gve_png_drawable_encode_png
 * --------------------------------
 * gap_gve_png_drawable_encode_png
 *  in: drawable: Describes the picture to be compressed in GIMP terms.
 *      png_interlaced: TRUE: Generate interlaced png.
 *      png_compression: The compression of the generated PNG (0-9, where 9 is best, 0 fastest).
 *      app0_buffer: if != NULL, the content of the APP0-marker to write.
 *      app0_length: the length of the APP0-marker.
 *  out:PNG_size: The size of the buffer that is returned.
 *  returns: guchar *: A buffer, allocated by this routines, which contains
 *                     the compressed PNG, NULL on error.
 */
guchar *
gap_gve_png_drawable_encode_png(GimpDrawable *drawable, gint32 png_interlaced, gint32 *PNG_size,
                               gint32 png_compression,
                               void *app0_buffer, gint32 app0_length)
{
  guchar *buffer;
  guchar *PNG_data;
  size_t totalsize = 0;
  gint32 image_id;
  gboolean l_pngSaveOk;
  char *l_tmpname;
  
  l_tmpname = gimp_temp_name("tmp.png");
  image_id = gimp_drawable_get_image(drawable->drawable_id);
  
  l_pngSaveOk = p_save_as_tmp_png_file(l_tmpname
                       , image_id
                       , drawable->drawable_id
                       , png_interlaced
                       , png_compression
                       );
  if (l_pngSaveOk)
  {
    gint32 fileSize;
    gint32 bytesRead;
    
    fileSize = gap_file_get_filesize(l_tmpname);
    
    totalsize = app0_length + fileSize;
    buffer = g_malloc(totalsize);
    PNG_data = buffer;
    if(app0_length > 0)
    {
      memcpy(buffer, app0_buffer, app0_length);
      PNG_data = buffer + app0_length;
  
      bytesRead = gap_file_load_file_segment(l_tmpname
                      ,PNG_data
                      ,0            /* seek_index, start byte of datasegment in file */
                      ,fileSize     /* segment size in byets */
                      );
      if (bytesRead != fileSize)
      {
        g_free(buffer);
        buffer = NULL;
        totalsize = 0;
        printf("gap_gve_png_drawable_encode_png: read error: bytesRead:%d (expected: %d) file:%s\n"
               ,(int)bytesRead
               ,(int)fileSize
               ,l_tmpname
               );
      }
    }
    
  }

  if(g_file_test(l_tmpname, G_FILE_TEST_EXISTS))
  {
    g_remove(l_tmpname);
  }

  /* free the temporary buffer */
  g_free (l_tmpname);

  *PNG_size = totalsize;
  return buffer;
  
}  /* end gap_gve_png_drawable_encode_png */
