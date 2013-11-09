/* gap_pdb_calls.c
 *
 * this module contains wraper calls of procedures in the GIMPs Procedural Database
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
 *                  2005/04/03  carol: changed deprecated procedure 'gimp_rotate' to 'gimp_drawable_transform_rotate_default' 
 * version 1.3.25a; 2004/01/20  hof: removed gap_pdb_gimp_file_load_thumbnail
 * version 1.3.14c; 2003/06/15  hof: take care of gimp_image_thumbnail 128x128 sizelimit
 * version 1.3.14b; 2003/06/03  hof: gboolean retcode for thumbnail procedures
 * version 1.3.14a; 2003/05/24  hof: moved vin Procedures to gap_vin module
 * version 1.3.5a;  2002/04/20  hof: gap_pdb_gimp_layer_new_from_drawable. (removed set_drabale)
 * version 1.3.4a;  2002/03/12  hof: removed duplicate wrappers that are available in libgimp too.
 * version 1.2.2b;  2001/12/09  hof: wrappers for tattoo procedures
 * version 1.1.16a; 2000/02/05  hof: path lockedstaus
 * version 1.1.15b; 2000/01/30  hof: image parasites
 * version 1.1.15a; 2000/01/26  hof: pathes
 *                                   removed old gimp 1.0.x PDB Interfaces
 * version 1.1.14a; 2000/01/09  hof: thumbnail save/load,
 *                              Procedures for video_info file
 * version 0.98.00; 1998/11/28  hof: 1.st (pre) release (GAP port to GIMP 1.1)
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* GIMP includes */
#include "libgimp/gimp.h"

/* GAP includes */
#include "gap_pdb_calls.h"

extern int gap_debug;

/* ============================================================================
 * gap_status_to_string
 * ============================================================================
 */
const char *
gap_status_to_string(int status)
{
  switch (status)
  {
    case GIMP_PDB_EXECUTION_ERROR:
      return ("GIMP_PDB_EXECUTION_ERROR");
    case GIMP_PDB_CALLING_ERROR:
      return ("GIMP_PDB_CALLING_ERROR");
    case GIMP_PDB_PASS_THROUGH:
      return ("GIMP_PDB_PASS_THROUGH");
    case GIMP_PDB_SUCCESS:
      return ("GIMP_PDB_SUCCESS");
    case GIMP_PDB_CANCEL:
      return ("GIMP_PDB_CANCEL");
    default:
      return ("* unknown *");
  }
}  /* end gap_status_to_string */


/* check if procedure name is available in the PDB.
 * this procedure reads all available pdb procedure names into static memory
 * when called 1st time. and checks the specified name against the memory.
 * Introduced with GIMP-2.6 that produces annoying Error messages on the GUI
 * when a plug attempt to query information for an unknown name.
 * This workaround help to avoid those error messages,
 * but makes GAP filter all layer feature slower.
 */
gboolean
gap_pdb_procedure_name_available (const gchar *search_name)
{
  static gboolean initialized = FALSE;

  static gchar       **proc_list = NULL;
  static gint          num_procs = 0;
  int loop;
  gboolean found;

  found = FALSE;
 
  if (!initialized)
  {
      /* query for all elements (only at 1.st call in this process)
       *  if we apply the search string to gimp_plugins_query
       *  we get no result, because the search will query MenuPath
       *  and not for the realname of the plug-in)
       */
       // gimp_procedural_db_query
      gimp_procedural_db_query (".*", ".*", ".*", ".*", ".*", ".*", ".*", 
                                &num_procs, &proc_list);

      initialized = TRUE;

      if(gap_debug)
      {
        for (loop = 0; loop < num_procs; loop++)
        {
          printf("PDBname:%s\t\search_name:%s\n", proc_list[loop], search_name);
        }
      }

  }

  if (initialized)
  {
      for (loop = 0; loop < num_procs; loop++)
      {
        if(strcmp(search_name, proc_list[loop]) == 0)
        {
          found = TRUE;
          break;  /* stop at 1st match */
        }
     }
  }

  /* Dont Destroy, but Keep the Returned Parameters for the lifetime of this process */
  /* gimp_destroy_params (return_vals, nreturn_vals); */

  return (found);
}  /* end gap_pdb_procedure_name_available */


/* ============================================================================
 * gap_pdb_procedure_available
 *   if requested procedure is available in the PDB return the number of args
 *      (0 upto n) that are needed to call the procedure.
 *   if not available return -1
 * ============================================================================
 */
gint 
gap_pdb_procedure_available(char *proc_name)
{
  gint             l_nparams;
  gint             l_nreturn_vals;
  GimpPDBProcType   l_proc_type;
  gchar            *l_proc_blurb;
  gchar            *l_proc_help;
  gchar            *l_proc_author;
  gchar            *l_proc_copyright;
  gchar            *l_proc_date;
  GimpParamDef    *l_params;
  GimpParamDef    *l_return_vals;
  gint             l_rc;

  l_rc = 0;
  
  /* Query the gimp application's procedural database
   *  regarding a particular procedure.
   */
  if (gimp_procedural_db_proc_info (proc_name,
                                    &l_proc_blurb,
                                    &l_proc_help,
                                    &l_proc_author,
                                    &l_proc_copyright,
                                    &l_proc_date,
                                    &l_proc_type,
                                    &l_nparams,
                                    &l_nreturn_vals,
                                    &l_params,
                                    &l_return_vals))
    {
      /* procedure found in PDB */
      return (l_nparams);
    }

  printf("Warning: Procedure %s not found.\n", proc_name);
  return -1;
}       /* end gap_pdb_procedure_available */

/* ---------------------- PDB procedure calls  -------------------------- */






/* ============================================================================
 * gap_pdb_gimp_rotate_degree
 *  PDB call of 'gimp_rotate'
 * ============================================================================
 */

gint32
gap_pdb_gimp_rotate_degree(gint32 drawable_id, gboolean interpolation, gdouble angle_deg)
{
   gdouble          l_angle_rad;

   l_angle_rad = (angle_deg * G_PI) / 180.0;

   return(gimp_drawable_transform_rotate_default(drawable_id
                                                , l_angle_rad
                                                , FALSE            /* auto_center */
                                                , 0                /* center_x */
                                                , 0                /* center_y */
                                                , interpolation
                                                , FALSE            /* clip_results */
                                                ));

   
}  /* end gap_pdb_gimp_rotate_degree */



/* ============================================================================
 * gap_pdb_gimp_displays_reconnect
 *   
 * ============================================================================
 */

gboolean
gap_pdb_gimp_displays_reconnect(gint32 old_image_id, gint32 new_image_id)
{
   static char     *l_called_proc = "gimp_displays_reconnect";
   GimpParam          *return_vals;
   int              nreturn_vals;

   return_vals = gimp_run_procedure (l_called_proc,
                                 &nreturn_vals,
                                 GIMP_PDB_IMAGE,  old_image_id,
                                 GIMP_PDB_IMAGE,  new_image_id,
                                 GIMP_PDB_END);

   if (return_vals[0].data.d_status == GIMP_PDB_SUCCESS)
   {
      gimp_destroy_params(return_vals, nreturn_vals);
      return (TRUE);   /* OK */
   }
   gimp_destroy_params(return_vals, nreturn_vals);
   printf("GAP: Error: PDB call of %s failed, d_status:%d %s\n"
      , l_called_proc
      , (int)return_vals[0].data.d_status
      , gap_status_to_string(return_vals[0].data.d_status)
      );
   return(FALSE);
}       /* end gap_pdb_gimp_displays_reconnect */




/* ============================================================================
 * gap_pdb_gimp_file_save_thumbnail
 *   
 * ============================================================================
 */

gboolean
gap_pdb_gimp_file_save_thumbnail(gint32 image_id, char* filename)
{
   static char     *l_called_proc = "gimp_file_save_thumbnail";
   GimpParam          *return_vals;
   int              nreturn_vals;

   /*if(gap_debug) printf("gap_pdb_gimp_file_save_thumbnail: image_id:%d  %s\n", (int)image_id, filename);*/

   return_vals = gimp_run_procedure (l_called_proc,
                                 &nreturn_vals,
                                 GIMP_PDB_IMAGE,     image_id,
                                 GIMP_PDB_STRING,    filename,
                                 GIMP_PDB_END);

   if (return_vals[0].data.d_status == GIMP_PDB_SUCCESS)
   {
      gimp_destroy_params(return_vals, nreturn_vals);
      return (TRUE);
   }

   gimp_destroy_params(return_vals, nreturn_vals);
   printf("GAP: Error: PDB call of %s failed on file: %s (image_id:%d), d_status:%d %s\n"
          , l_called_proc
          , filename
          , (int)image_id
          , (int)return_vals[0].data.d_status
          , gap_status_to_string(return_vals[0].data.d_status)
          );
   return(FALSE);
}       /* end gap_pdb_gimp_file_save_thumbnail */


/* ============================================================================
 * gap_pdb_gimp_image_thumbnail
 *   
 * ============================================================================
 */
gboolean
gap_pdb_gimp_image_thumbnail(gint32 image_id, gint32 width, gint32 height,
                              gint32 *th_width, gint32 *th_height, gint32 *th_bpp,
                              gint32 *th_data_count, unsigned char **th_data)
{
   static char     *l_called_proc = "gimp_image_thumbnail";
   GimpParam          *return_vals;
   int              nreturn_vals;

   *th_data = NULL;

    /* gimp_image_thumbnail 
     *   has a limit of maximal 128x128 pixels. (in gimp-1.3.14)
     *   On bigger sizes it returns success, along with a th_data == NULL.
     * THIS workaround makes a 2nd try with reduced size.
     * TODO:
     *  - if gimp keeps the size limit until the stable 1.4 release
     *       i suggest to check for sizs < 128 before the 1.st attemp.
     *       (for better performance on bigger thumbnail sizes)
     *  - if there will be no size limit in the future,
     *       the 2.nd try cold be removed (just for cleanup reasons)
     * hof, 2003.06.17
     */
workaround:
   return_vals = gimp_run_procedure (l_called_proc,
                                 &nreturn_vals,
                                 GIMP_PDB_IMAGE,    image_id,
                                 GIMP_PDB_INT32,    width,
                                 GIMP_PDB_INT32,    height,
                                 GIMP_PDB_END);

   if (return_vals[0].data.d_status == GIMP_PDB_SUCCESS)
   {
      /* in case of success dont gimp_destroy_params
       * because thumbnail data is returned without copying
       * for performance reasons
       */

      *th_width  = return_vals[1].data.d_int32;
      *th_height = return_vals[2].data.d_int32;
      *th_bpp    = return_vals[3].data.d_int32;
      *th_data_count = return_vals[4].data.d_int32;
      *th_data = (unsigned char *)return_vals[5].data.d_int8array;

      if (*th_data == NULL)
      {
         if(gap_debug)
         {
           printf("(PDB_WRAPPER workaround for gimp_image_thumbnail GIMP_PDB_SUCCESS, th_data:%d  (%d x %d) \n"
            , (int)return_vals[5].data.d_int8array
            , (int)return_vals[1].data.d_int32
            , (int)return_vals[2].data.d_int32
            );
         }
         if(MAX(width, height) > 128)
         {
           if(width > height)
           {
              height = (128 * height) / width;
              width = 128;
           }
           else
           {
              width = (128 * width) / height;
              height = 128;
           }

           goto workaround;
         }
         return(FALSE);  /* this is no success */
      }
      return(TRUE); /* OK */
   }
   gimp_destroy_params(return_vals, nreturn_vals);
   printf("GAP: Error: PDB call of %s failed, d_status:%d %s\n"
      , l_called_proc
      , (int)return_vals[0].data.d_status
      , gap_status_to_string(return_vals[0].data.d_status)
      );
   return(FALSE);
}       /* end gap_pdb_gimp_image_thumbnail */




/* --------------------------------------
 * gap_pdb_call_displace
 * --------------------------------------
 *  
 */
gboolean
gap_pdb_call_displace(gint32 image_id, gint32 layer_id
  ,gdouble amountX, gdouble amountY
  ,gint32 doX, gint32 doY
  ,gint32 mapXId, gint32 mapYId
  ,gint32 displaceType)
{
   static char     *l_called_proc = "plug-in-displace";
   GimpParam       *return_vals;
   int              nreturn_vals;

   return_vals = gimp_run_procedure (l_called_proc,
                                 &nreturn_vals,
                                 GIMP_PDB_INT32,     GIMP_RUN_NONINTERACTIVE,
                                 GIMP_PDB_IMAGE,     image_id,
                                 GIMP_PDB_DRAWABLE,  layer_id,          /* input drawable (to be processed) */
                                 GIMP_PDB_FLOAT,     amountX,           /* Displace multiplier for X or radial direction */
                                 GIMP_PDB_FLOAT,     amountY,           /* Displace multiplier for Y or tangent (degrees) direction */
                                 GIMP_PDB_INT32,     doX,               /* Displace in X or radial direction?  */
                                 GIMP_PDB_INT32,     doY,               /* Displace in Y or tangential direction?  */
                                 GIMP_PDB_DRAWABLE,  mapXId,            /* Displacement map for X or radial direction  */
                                 GIMP_PDB_DRAWABLE,  mapYId,            /* Displacement map for Y or tangent direction  */
                                 GIMP_PDB_INT32,     displaceType,      /* Edge behavior: WRAP (0), SMEAR (1), BLACK (2)  */
                                 GIMP_PDB_END);

   if (return_vals[0].data.d_status == GIMP_PDB_SUCCESS)
   {
      gimp_destroy_params(return_vals, nreturn_vals);
      return (TRUE);   /* OK */
   }
   gimp_destroy_params(return_vals, nreturn_vals);
   g_message("Error: PDB call of %s failed, d_status:%d %s\n"
      , l_called_proc
      , (int)return_vals[0].data.d_status
      , gap_status_to_string(return_vals[0].data.d_status)
      );
   return(FALSE);
} /* end gap_pdb_call_displace */


/* --------------------------------------
 * gap_pdb_call_solid_noise
 * --------------------------------------
 *  
 */
gboolean
gap_pdb_call_solid_noise(gint32 image_id, gint32 layer_id
   , gint32 tileable, gint32 turbulent, gint32 seed, gint32 detail, gdouble xsize, gdouble ysize)
{
   static char     *l_called_proc = "plug-in-solid-noise";
   GimpParam       *return_vals;
   int              nreturn_vals;

   return_vals = gimp_run_procedure (l_called_proc,
                                 &nreturn_vals,
                                 GIMP_PDB_INT32,     GIMP_RUN_NONINTERACTIVE,
                                 GIMP_PDB_IMAGE,     image_id,
                                 GIMP_PDB_DRAWABLE,  layer_id,          /* input drawable (to be processed) */
                                 GIMP_PDB_INT32,     tileable,          /* create tileable output (0:no, 1:yes)  */
                                 GIMP_PDB_INT32,     turbulent,         /* Make turbulent noise (0:no, 1:yes)  */
                                 GIMP_PDB_INT32,     seed,              /* Random seed  */
                                 GIMP_PDB_INT32,     detail,            /* detail level (0 - 15)  */
                                 GIMP_PDB_FLOAT,     xsize,             /* texture size */
                                 GIMP_PDB_FLOAT,     ysize,             /* texture size */
                                 GIMP_PDB_END);

   if (return_vals[0].data.d_status == GIMP_PDB_SUCCESS)
   {
      gimp_destroy_params(return_vals, nreturn_vals);
      return (TRUE);   /* OK */
   }
   gimp_destroy_params(return_vals, nreturn_vals);
   g_message("Error: PDB call of %s failed, d_status:%d %s\n"
      , l_called_proc
      , (int)return_vals[0].data.d_status
      , gap_status_to_string(return_vals[0].data.d_status)
      );
   return(FALSE);
} /* end gap_pdb_call_solid_noise */


/* --------------------------------------
 * gap_pdb_call_normalize
 * --------------------------------------
 *  
 */
gboolean
gap_pdb_call_normalize(gint32 image_id, gint32 layer_id)
{
   static char     *l_called_proc = "plug-in-normalize";
   GimpParam       *return_vals;
   int              nreturn_vals;

   return_vals = gimp_run_procedure (l_called_proc,
                                 &nreturn_vals,
                                 GIMP_PDB_INT32,     GIMP_RUN_NONINTERACTIVE,
                                 GIMP_PDB_IMAGE,     image_id,
                                 GIMP_PDB_DRAWABLE,  layer_id,          /* input drawable (to be processed) */
                                 GIMP_PDB_END);

   if (return_vals[0].data.d_status == GIMP_PDB_SUCCESS)
   {
      gimp_destroy_params(return_vals, nreturn_vals);
      return (TRUE);   /* OK */
   }
   gimp_destroy_params(return_vals, nreturn_vals);
   g_message("Error: PDB call of %s failed, d_status:%d %s\n"
      , l_called_proc
      , (int)return_vals[0].data.d_status
      , gap_status_to_string(return_vals[0].data.d_status)
      );
   return(FALSE);
} /* end gap_pdb_call_normalize */

