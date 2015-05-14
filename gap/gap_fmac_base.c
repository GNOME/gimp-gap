/* gap_fmac_base.c
 * 2006.12.11 hof (Wolfgang Hofer)
 *
 * GAP ... Gimp Animation Plugins
 *
 * This Module contains:
 * - filtermacro execution backend procedures.
 *
 * WARNING:
 * filtermacros are a temporary solution, useful for animations
 * but do not expect support for filtermacros in future releases of GIMP-GAP
 * because GIMP may have real makro features in the future ...
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

/* SYTEM (UNIX) includes */
#include <string.h>
#include <stdlib.h>
#include <errno.h>

#include <glib/gstdio.h>

/* GIMP includes */
#include "gtk/gtk.h"
#include "libgimp/gimp.h"
#include "libgimp/gimpui.h"


/* GAP includes */
#include "config.h"
#include "gap-intl.h"
#include "gap_lib.h"
#include "gap_val_file.h"
#include "gap_filter.h"
#include "gap_filter_pdb.h"
#include "gap_fmac_name.h"
#include "gap_fmac_base.h"
#include "gap_fmac_context.h"
#include "gap_frame_fetcher.h"



extern      int gap_debug; /* ==0  ... dont print debug infos */

typedef struct FMacElem {
 char       *filtername;
 gboolean    assigned_flag;
 gboolean    varying_flag;
 char       *buffer_from;
 gint32      buffer_from_length;
 char       *buffer_to;
 char      *iteratorname;
 void       *next;
} FMacElem;

typedef struct FMacLine {
 char       *filtername;
 char       *paramdata;
 gint32      paramlength;
} FMacLine;

static void      p_print_and_free_msg(char *msg, GimpRunMode run_mode);

gint             p_fmac_execute(GimpRunMode run_mode, gint32 image_id, gint32 drawable_id
                        , const char *filtermacro_file1
                        , const char *filtermacro_file2
                        , gdouble current_step
                        , gint32  total_steps
                        );


/* --------------------
 * p_print_and_free_msg
 * --------------------
 */
static void
p_print_and_free_msg(char *msg, GimpRunMode run_mode)
{
  if(run_mode == GIMP_RUN_INTERACTIVE)
  {
    g_message("%s", msg);
  }
  printf("%s\n", msg);
  g_free(msg);
}  /* end p_print_and_free_msg */


/* ------------------------
 * p_free_fmac_line
 * ------------------------
 */
static void
p_free_fmac_line(FMacLine *fmac_line)
{
  if(fmac_line->filtername)
  {
    g_free(fmac_line->filtername);
  }
  if(fmac_line->paramdata)
  {
    g_free(fmac_line->paramdata);
  }
  g_free(fmac_line);
}  /* end p_free_fmac_line */

/* ------------------------
 * p_scan_fmac_line
 * ------------------------
 * parse one filtermacro file line
 * example line:
 *    "plug_in_xyz" 2 0x01 0xF2
 * line contains filtername (quotes are mandatory)
 * length                        # decimal digits
 * and databyte_array[length]    # hexadecimal digits
 */
static FMacLine *
p_scan_fmac_line(GapValTextFileLines *txf_ptr, GimpRunMode run_mode, const char *filtermacro_file)
{
  gchar *l_buf;
  FMacLine *fmac_line;

  if(gap_debug)
  {
       printf("p_scan_fmac_line: %s\n"
              , txf_ptr->line
              );
  }

  l_buf = txf_ptr->line;
  fmac_line = NULL;

  /* handle lines starting with double quotes */
  if (l_buf[0] == '"')
  {
    char *l_plugin_name;
    char *l_scan_ptr;
    char *l_scan_ptr2;
    gint  l_idx;

    
    /* scan plugin-name */
    l_scan_ptr=&l_buf[0];
    l_plugin_name = &l_buf[1];
    for(l_idx=1; l_idx < GAP_VAL_MAX_BYTES_PER_LINE;l_idx++)
    {
      if (l_buf[l_idx] == '"')
      {
        l_buf[l_idx] = '\0';
        l_scan_ptr=&l_buf[l_idx+1];
        break;
      }
    }

    if(gap_debug)
    {
      printf("p_scan_fmac_line: ##l_plugin_name:%s\n", l_plugin_name);
    }
    fmac_line = g_malloc0(sizeof(FMacLine));
    fmac_line->filtername = gimp_canonicalize_identifier(l_plugin_name);


    /* scan for data length */
    fmac_line->paramlength = strtol(l_scan_ptr, &l_scan_ptr2, 10);

    if (l_scan_ptr != l_scan_ptr2)
    {
       if(gap_debug)
       {
         printf("p_scan_fmac_line: ##paramlength:%d\n"
                , (int)fmac_line->paramlength
                );
       }

       fmac_line->paramdata = g_malloc0(fmac_line->paramlength);
       l_scan_ptr = l_scan_ptr2;
       for(l_idx=0; l_idx < fmac_line->paramlength;l_idx++)
       {
          long int l_data_byte;
          gchar   *l_msg;
          
          l_data_byte = strtol(l_scan_ptr, &l_scan_ptr2, 16);
          /* if(gap_debug) printf("p_fmac_execute: l_data_byte:%d\n", (int)l_data_byte); */

          if ((l_data_byte < 0) || (l_scan_ptr == l_scan_ptr2))
          {
            p_free_fmac_line(fmac_line);
            l_msg = g_strdup_printf (_("filtermacro_file: '%s' is corrupted, could not scan databytes")
                                    , filtermacro_file);
            p_print_and_free_msg(l_msg, run_mode);
            return (NULL);
          }
          fmac_line->paramdata[l_idx] = l_data_byte;
          l_scan_ptr = l_scan_ptr2;
       }
    }
  }

  if(gap_debug)
  {
     if(fmac_line)
     {
       printf("p_scan_fmac_line: END fmac_line:%d filtername:%s paramlength:%d\n"
              , (int)fmac_line
              , fmac_line->filtername
              , (int)fmac_line->paramlength
              );
     }
     else
     {
       printf("p_scan_fmac_line: END fmac_line:%d\n"
              , (int)fmac_line
              );
     }
  }
  return(fmac_line);
}  /* end p_scan_fmac_line */


/* ------------------------
 * p_build_fmac_list
 * ------------------------
 * build the list by loading all records from the 1st filtermacro file.
 * init both buffer_from and buffer_to with the same set of filter parameters.
 * init the assigned_flag with FALSE for all elements that are added to the list.
 * Note that the order of elements in the list must match the order in the
 * filtermacro file.
 * Furthermore assign the matching iteratorname (a name of
 * an iterator plug-in that can handle the filter specific parameter value mix
 * for the iterable parameter values and/or can do the mapping of persistent drawable id's
 * in the last values buffer.
 * the varying_flag is initilized with FALSE. 
 *
 * returns root elem of the list or NULL if load failed.
 */
static FMacElem *
p_build_fmac_list(const char *filtermacro_file, GimpRunMode run_mode)
{
  FMacElem *fmac_root;
  FMacElem *fmac_tail;
  gchar   *l_msg;
  GapValTextFileLines *txf_ptr;
  GapValTextFileLines *txf_ptr_root;

  fmac_root = NULL;
  fmac_tail = NULL;

  if(gap_debug)
  {
    printf("p_build_fmac_list: START  filtermacro_file:%s\n"
           ,filtermacro_file
           );
  }

  if (!gap_fmac_chk_filtermacro_file(filtermacro_file))
  {
      l_msg = g_strdup_printf(_("file: %s is not a filtermacro file !")
                             , filtermacro_file);
      p_print_and_free_msg(l_msg, run_mode);
      return NULL;
  }

  /* process filtermacro file (scan line by line and add filtername
   * and params to the fmac_root list) 
   */
  txf_ptr_root = gap_val_load_textfile(filtermacro_file);
  for(txf_ptr = txf_ptr_root; txf_ptr != NULL; txf_ptr = (GapValTextFileLines *) txf_ptr->next)
  {
    FMacLine *fmacLine;
    
    fmacLine = p_scan_fmac_line(txf_ptr, run_mode, filtermacro_file);
    if (fmacLine)
    {
      FMacElem *fmac_elem;
      gint l_count;
      
      /* create fmac_elem according to scanned fmac line */
      fmac_elem = g_malloc0(sizeof(FMacElem));
      fmac_elem->filtername = gimp_canonicalize_identifier(fmacLine->filtername);
      fmac_elem->buffer_from  = g_malloc0(fmacLine->paramlength);
      fmac_elem->buffer_to    = g_malloc0(fmacLine->paramlength);
      fmac_elem->buffer_from_length = fmacLine->paramlength;
      fmac_elem->assigned_flag = FALSE;
      fmac_elem->varying_flag = FALSE;
      fmac_elem->next = NULL;
      /* assign the matching Iterator PluginProcedure
       * (if there is any)
       */
      fmac_elem->iteratorname = 
         gap_filt_pdb_get_iterator_proc(fmac_elem->filtername
                                       , &l_count);
      
      memcpy(fmac_elem->buffer_from , fmacLine->paramdata, fmacLine->paramlength);
      memcpy(fmac_elem->buffer_to , fmacLine->paramdata, fmacLine->paramlength);
      
      p_free_fmac_line(fmacLine);

      /* add fmac_elem at end of fmac_root list. */
      if (fmac_root == NULL)
      {
        fmac_root = fmac_elem;
        fmac_tail = fmac_elem;
      }
      else
      {
        fmac_tail->next = fmac_elem;
        fmac_tail = fmac_elem;
      }
    }

  }

  if(txf_ptr_root)
  {
    gap_val_free_textfile_lines(txf_ptr_root);
  }

  if(gap_debug)
  {
    printf("p_build_fmac_list: END  fmac_root:%d filtermacro_file:%s\n"
           ,(int)fmac_root
           ,filtermacro_file
           );
  }

  return fmac_root;
}  /* end p_build_fmac_list */


/* ------------------------
 * p_merge_fmac_list
 * ------------------------
 * read filters from the 2nd filtermacro file and merge
 * in the buffer_to values by overwriting already initialized
 * values where filtername matches and assigned_flag is not yet set.
 * Note that all non-matching entries are ignored.
 *
 * the varying_flag is set to TRUE for those lines that have a matching line
 * in the 2nd filtermacro file AND have an iterator 
 * (that can do the plug-in specific mix of the parmetervakues)
 */
gboolean
p_merge_fmac_list(FMacElem *fmac_root, const char *filtermacro_file, GimpRunMode run_mode)
{
  gchar   *l_msg;
  GapValTextFileLines *txf_ptr;
  GapValTextFileLines *txf_ptr_root;

  if(gap_debug)
  {
    printf("p_merge_fmac_list:  fmac_root:%d filtermacro_file:%s\n"
           ,(int)fmac_root
           ,filtermacro_file
           );
  }

  if (!gap_fmac_chk_filtermacro_file(filtermacro_file))
  {
      l_msg = g_strdup_printf(_("file: %s is not a filtermacro file !")
                             , filtermacro_file);
      p_print_and_free_msg(l_msg, run_mode);
      return (FALSE);
  }


  /* process filtermacro file (scan line by line and overwrite
   * the 2nd parameter set where filtername matches
   */
  txf_ptr_root = gap_val_load_textfile(filtermacro_file);
  for(txf_ptr = txf_ptr_root; txf_ptr != NULL; txf_ptr = (GapValTextFileLines *) txf_ptr->next)
  {
    FMacLine *fmacLine;

    fmacLine = p_scan_fmac_line(txf_ptr, run_mode, filtermacro_file);
    if (fmacLine)
    {
      FMacElem *fmac_elem;
      
      for(fmac_elem = fmac_root; fmac_elem != NULL; fmac_elem = (FMacElem *)fmac_elem->next)
      {
        if((strcmp(fmac_elem->filtername, fmacLine->filtername) == 0)
        && (fmac_elem->assigned_flag == FALSE))
        {
          if (fmac_elem->buffer_from_length == fmacLine->paramlength)
          {
            /* overwrite param data */
            memcpy(fmac_elem->buffer_to, fmacLine->paramdata, fmacLine->paramlength);
            fmac_elem->assigned_flag = TRUE;

            if(fmac_elem->iteratorname != NULL)
            {
              fmac_elem->varying_flag = TRUE;
            }

          }
          else
          {
            printf("ERROR: parmlength %d does not match %d for filter:%s\n"
                ,(int)fmacLine->paramlength
                ,(int)fmac_elem->buffer_from_length
                ,fmacLine->filtername
                );
          }
          break;
        }
      }
      p_free_fmac_line(fmacLine);

    }
  }

  if(txf_ptr_root)
  {
    gap_val_free_textfile_lines(txf_ptr_root);
  }
  
  return (TRUE);
}  /* end p_merge_fmac_list */


/* ----------------------------
 * p_fmac_execute_single_filter
 * ----------------------------
 */
static void
p_fmac_execute_single_filter(GimpRunMode run_mode, gint32 image_id, gint32 drawable_id
   , FMacElem *fmac_elem
   , gdouble current_step
   , gint32  total_steps
   )
{
  gchar   *l_msg;
  guchar  *l_lastvalues_bck_buffer;
  gint     l_bck_len;
  gint l_rc;
 
  l_lastvalues_bck_buffer = NULL;
  l_bck_len = 0;

  /* make backup of last values buffer (if available) */
  l_bck_len = gimp_get_data_size(fmac_elem->filtername);

  if(l_bck_len > 0)
  {
    if(l_bck_len != fmac_elem->buffer_from_length)
    {
       l_msg = g_strdup_printf (_("parameter data buffer for plug-in: '%s' differs in size\n"
                                "actual size: %d\n"
                                "recorded size: %d")
                               , fmac_elem->filtername
                               , (int)l_bck_len
                               , (int)fmac_elem->buffer_from_length
                               );
       p_print_and_free_msg(l_msg, run_mode);
       
       /* ignore filter call because length missmatch of parameterset was detected
        * (this can happen if the filtermacro was recorded with a release version
        * that differs from the currently installed version).
        * in such a case the filter may crash if called with an incompatible last value
        * parameter set.
        */
       return;
    }
    l_lastvalues_bck_buffer = g_malloc0(l_bck_len);
    gimp_get_data (fmac_elem->filtername, l_lastvalues_bck_buffer);
  }


  if (fmac_elem->iteratorname)
  {
    static char l_key_from[512];
    static char l_key_to[512];

    /* set data. Note that iterators will fail if the filter plugin has never
     * set a lastvalus buffer in the current gimp session because
     * the buffer size is derived from an inital filter run.
     * therefore we set the lastvalus buffer now to simulate an initial run
     * and let the iterators know about the size of the lastavlues buffer.
     */
    gimp_set_data(fmac_elem->filtername, fmac_elem->buffer_from, fmac_elem->buffer_from_length);


    /* Note that FROM and TO are swapped, because iterators were initially
     * desinged for processing layers at inverse order.
     */
    

    g_snprintf(l_key_from, sizeof(l_key_from), "%s%s", fmac_elem->filtername, GAP_ITER_TO_SUFFIX);
    gimp_set_data(l_key_from, fmac_elem->buffer_from, fmac_elem->buffer_from_length);

    g_snprintf(l_key_to, sizeof(l_key_to), "%s%s", fmac_elem->filtername, GAP_ITER_FROM_SUFFIX);
    gimp_set_data(l_key_to, fmac_elem->buffer_to, fmac_elem->buffer_from_length);

    if(gap_debug)
    {
      if(fmac_elem->varying_flag == TRUE)
      {
        printf("p_fmac_execute_single_filter: VARYING APPLY iteratorname:%s\n  key_from:%s\n  key_to:%s\n"
             ,fmac_elem->iteratorname
             ,l_key_from
             ,l_key_to
             );
      }
      else
      {
        printf("p_fmac_execute_single_filter: MAPPING APPLY iteratorname:%s\n  key_from:%s\n  key_to:%s\n"
             ,fmac_elem->iteratorname
             ,l_key_from
             ,l_key_to
             );
      }
    }
    
    /* the iterator call will set the last values buffer for the corresponding
     * filtername. This is done by mixing iterable parameter values of the
     * FROM and TO  parametersets.
     */
    gap_filter_iterator_call(fmac_elem->iteratorname
       , total_steps
       , current_step
       , fmac_elem->filtername
       , fmac_elem->buffer_from_length
       );
  }
  else
  {
    if(gap_debug)
    {
      printf("p_fmac_execute_single_filter: CONST APPLY %s\n"
            ,fmac_elem->filtername
            );
    }
    /* filter without iterator are applied with constant parameterset FROM values
     * Here we set the last values buffer directly
     * (the key of the last values buffer is the filtername by GIMP convention)
     */
    gimp_set_data(fmac_elem->filtername, fmac_elem->buffer_from, fmac_elem->buffer_from_length);
  }

  if(gap_debug)
  {
    guchar  *l_exec_buffer;
    gint     l_exec_len;
    gint     l_ii;

    l_exec_len = gimp_get_data_size(fmac_elem->filtername);
    
    printf("p_fmac_execute_single_filter: EXEC image:%d, drawable:%d filtername:%s len:%d\n"
           ,(int)image_id
           ,(int)drawable_id
           ,fmac_elem->filtername
           ,(int)l_exec_len
           );
    l_exec_buffer = g_malloc0(l_exec_len);
    gimp_get_data (fmac_elem->filtername, l_exec_buffer);
    printf("exec buffer:");
    for(l_ii=0; l_ii < l_exec_len; l_ii++)
    {
      printf(" %02X", (int)l_exec_buffer[l_ii]);
    }
    printf("\n");
  }
  
  /* call the filter plugin itself with runmode RUN_WITH_LAST_VALUES */
  l_rc = gap_filt_pdb_call_plugin(fmac_elem->filtername
            , image_id
            , drawable_id
            , GIMP_RUN_WITH_LAST_VALS
            );

  /* restore lastvalues buffer (if we have a valid backup) */
  if(l_lastvalues_bck_buffer)
  {
    gimp_set_data(fmac_elem->filtername, l_lastvalues_bck_buffer, l_bck_len);
    g_free(l_lastvalues_bck_buffer);
    l_lastvalues_bck_buffer = NULL;
  }

}  /* end p_fmac_execute_single_filter */


/* ---------------------------------
 * p_find_layer_for_next_processing
 * ---------------------------------
 * find a layer in the image that was newly created (e.g is not
 * found in the old_layers_list snaphaot taken of the layerstack
 * before the processing step)
 * If there are more such new layers present, then pick the one
 * that has same position as the ref_layer_id had in the old_layers_list
 */
static gint32
p_find_layer_for_next_processing(gint32 image_id, gint32 ref_layer_id
   , gint32 *old_layers_list, gint old_nlayers)
{
  gint          l_nlayers;
  gint32       *l_layers_list;
  gint32        l_idx;
  gint32        l_old_stackposition;
  gint32        l_relevantLayer;

  if(old_layers_list == NULL)
  {
    return (-1);
  }
  
  if(gap_debug)
  {
    printf("OLD_layerstack:");
    for(l_idx = 0; l_idx < old_nlayers; l_idx++)
    {
      printf(" ID(%d):%d"
        ,(int)l_idx
        ,(int)old_layers_list[l_idx]
        );
    }
    printf("\n");
  }
  
  
  for(l_idx = 0; l_idx < old_nlayers; l_idx++)
  {
    if(old_layers_list[l_idx] == ref_layer_id)
    {
      l_old_stackposition = l_idx;
      break;   
    }
  }


  l_layers_list = gimp_image_get_layers(image_id, &l_nlayers);
  l_relevantLayer = -1;
  
  if(l_layers_list != NULL)
  {
    if(gap_debug)
    {
      printf("NEW_layerstack:");
      for(l_idx = 0; l_idx < l_nlayers; l_idx++)
      {
        printf(" ID(%d):%d"
          ,(int)l_idx
          ,(int)l_layers_list[l_idx]
          );
      }
      printf("\n");
    }


    for(l_idx = 0; l_idx < l_nlayers; l_idx++)
    {
      gint32        l_ii;
      gboolean      l_found;

      l_found = FALSE;
      for(l_ii = 0; l_ii < l_nlayers; l_ii++)
      {
        if (l_layers_list[l_idx] == old_layers_list[l_ii])
        {
          l_found = TRUE;
          break;
        }
      }
      
      if(l_found == FALSE)
      {
        /* we found a new layer that was created during last filtercall */
        l_relevantLayer = l_layers_list[l_idx];
        if(l_idx == l_old_stackposition)
        {
          /* the new layer has same stackposition */
          break;
        }
      }
    }
    g_free (l_layers_list);
  }

  if(gap_debug)
  {
    printf("FIND_LAYER(2): ref_layer_id:%d l_old_stackposition:%d l_relevantLayer:%d\n"
      ,(int)ref_layer_id
      ,(int)l_old_stackposition
      ,(int)l_relevantLayer
      );
  }

  return (l_relevantLayer);
  
}  /* end p_find_layer_for_next_processing */



/* ------------------------
 * p_fmac_execute
 * ------------------------
 * apply filtermacro on the specified drawable_id that must be part of the specified
 * image_id.
 * filtermacro_file1 is mandatory parameter (not NULL)
 *   it specifiies the FROM parameterset(s) in case of applying with varying
 *   values.
 * filtermacro_file2 is optional parameter
 *   caller may pass NULL if applying
 *   with constant parameterset(s) as defined in filtermacro_file1 is required.
 *
 *   if the parameter filtermacro_file2 is not NULL, it refers to a 2nd
 *   collection of filter parameterset(s). In this case the iterable parameters
 *   are built as mix of parametersets from file1 and file2 according to
 *   ratio of current_step/total_steps.
 *   This is restricted to correlated filternames (that appear in both file1 and file2)
 *   
 *   example:
 *
 *   #  file1:         len   data      
 *   --------------------------------
 *   1. plug_in_abc    3     00 01 02 
 *   2. plug_in_xy     2     00 00
 *   3. plug_in_abc    3     00 08 00 
 *   4. plug_in_xy     2     0A 0A
 *   5. plug_in_xy     2     02 02
 *   
 *   #  file2:         len   data      
 *   -----------------------------
 *   1. plug_in_zzz    1     00
 *   2. plug_in_xy     2     00 20
 *   3. plug_in_xy     2     00 00
 *
 *   In this example the plug_in_zzz is ignored (because there is no correlating
 *   filtercall definition in file1)
 *   file2:line 2.) correlates with file1:line 2.)
 *   file2:line 3.) correlates with file1:line 4.)
 *   
 */
gint
p_fmac_execute(GimpRunMode run_mode, gint32 image_id, gint32 drawable_id
   , const char *filtermacro_file1
   , const char *filtermacro_file2
   , gdouble current_step
   , gint32  total_steps
   )
{
  FMacElem *fmac_root;
  
  fmac_root = p_build_fmac_list(filtermacro_file1, run_mode);
  if (fmac_root)
  {
    FMacElem *fmac_elem;
    GapFmacContext theFmacContext;
    GapFmacContext *fmacContext;
    gint32          l_drawable_id;

    l_drawable_id = drawable_id;
    fmacContext = &theFmacContext;

    gap_fmct_setup_GapFmacContext(fmacContext
                                 , FALSE  /* no recording_mode  (e.g. apply mode) */
                                 , filtermacro_file1
                                 );



    if(filtermacro_file2 != NULL)
    {
      p_merge_fmac_list(fmac_root, filtermacro_file2, run_mode);
    }
    for(fmac_elem = fmac_root; fmac_elem != NULL; fmac_elem = (FMacElem *)fmac_elem->next)
    {
      gint          l_nlayers;
      gint32       *l_layers_list;
      
      l_layers_list = gimp_image_get_layers(image_id, &l_nlayers);
      if(l_layers_list != NULL)
      {

        if(gap_debug)
        {
          printf("p_fmac_execute: %s\n", fmac_elem->filtername);
        }
  
        p_fmac_execute_single_filter(run_mode, image_id, l_drawable_id
            , fmac_elem
            , current_step
            , total_steps
            );
      
        /* intermediate cleanup of temporary image duplicates that may have been
         * created while iterating persistent drawable ids (by iterator sub-procedur p_delta_drawable)
         */
        gap_frame_fetch_delete_list_of_duplicated_images(fmacContext->ffetch_user_id);

        if(gimp_drawable_is_valid(l_drawable_id) != TRUE)
        {
          /* the filter ha made the processed drawable_id invalid
           * (probably by merging with another layer)
           * In this case we try to figure out the new layer id
           * that can be used for the further filter calls to be done..
           */
          l_drawable_id = p_find_layer_for_next_processing(image_id
                                                         , l_drawable_id
                                                         , l_layers_list
                                                         , l_nlayers
                                                         );
        }
        g_free (l_layers_list);
      }
    }

    /* disable the sessionwide filtermacro context */
    gap_fmct_disable_GapFmacContext();
    
    //TODO p_free_fmac_list(fmac_root);  /* free the filtermacro processing list */
  }
  
  return(0);
}  /* end p_fmac_execute */



/* ----------------
 * gap_fmac_execute
 * ----------------
 */
gint
gap_fmac_execute(GimpRunMode run_mode, gint32 image_id, gint32 drawable_id
   , const char *filtermacro_file1
   , const char *filtermacro_file2
   , gdouble current_step
   , gint32  total_steps
   )
{
  gint l_rc;

  if(gap_debug)
  {
    printf("gap_fmac_execute: image_id:%d drawable_id:%d total_steps:%d current_step:%f\n"
       ,(int)image_id
       ,(int)drawable_id
       ,(int)total_steps
       ,(float)current_step
       );
    printf("  macrofile1:%s\n", filtermacro_file1 == 0 ? "null" : filtermacro_file1);
    printf("  macrofile2:%s\n", filtermacro_file2 == 0 ? "null" : filtermacro_file2);
  }

  gimp_image_undo_group_start(image_id);
  l_rc = p_fmac_execute(run_mode, image_id,  drawable_id
              , filtermacro_file1
              , filtermacro_file2
              , current_step
              , total_steps
              );
  gimp_image_undo_group_end(image_id);
  return (l_rc);
}  /* end gap_fmac_execute */
