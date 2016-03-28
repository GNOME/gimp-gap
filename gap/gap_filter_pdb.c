/* gap_filter_pdb.c
 * 1998.10.14 hof (Wolfgang Hofer)
 *
 * GAP ... Gimp Animation Plugins
 *
 * This Module contains:
 * - GAP_filter  pdb: functions for calling any Filter (==Plugin Proc)
 *                    that operates on a drawable
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
 * gimp   2.1.0a;  2004/11/16   hof: init string args with "\0" rather than NULL
 *                                   when calling a PDB-Procedure
 * gimp   1.3.12a; 2003/05/02   hof: merge into CVS-gimp-gap project, re-added support of iter_ALT procedures
 * gimp   1.3.8a;  2002/09/21   hof: gap_lastvaldesc
 * gimp   1.3.4b;  2002/03/24   hof: gap_filt_pdb_get_iterator_proc supports COMMON_ITERATOR, removed support of iter_ALT procedures
 * gimp   1.1.28a; 2000/11/05   hof: check for GIMP_PDB_SUCCESS (not for FALSE)
 * version gimp 1.1.17b  2000.02.22  hof: - removed limit PLUGIN_DATA_SIZE
 *                                        - removed support for old gimp 1.0.x PDB-interface.
 * version 0.97.00                   hof: - created module (as extract gap_filter_foreach)
 */
#include "config.h"

/* SYTEM (UNIX) includes */ 
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef HAVE_SYS_WAIT_H
#include <sys/wait.h>
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

/* GIMP includes */
#include "gtk/gtk.h"
#include "libgimp/gimp.h"

/* GAP includes */
#include "gap_lastvaldesc.h"
#include "gap_arr_dialog.h"
#include "gap_filter.h"
#include "gap_filter_pdb.h"
#include "gap_pdb_calls.h"
#include "gap_dbbrowser_utils.h"
#include "gap_lib.h"


/* ------------------------
 * global gap DEBUG switch
 * ------------------------
 */

/* int gap_debug = 1; */    /* print debug infos */
/* int gap_debug = 0; */    /* 0: dont print debug infos */

extern int gap_debug;

static char *global_plugin_data = NULL;


static char * p_filt_pdb_get_alternative_iterator_proc(char *plugin_name, gint *count);
static gint   p_count_iterable_params(gchar *key_description, gint   desc_size);

static void 
p_setGapLastvalAnimatedCallInfo(gint32 total_steps, gdouble current_step)
{
  GapLastvalAnimatedCallInfo  animCallInfo;

  animCallInfo.animatedCallInProgress = TRUE;
  animCallInfo.total_steps = total_steps;
  animCallInfo.current_step = current_step;
  gimp_set_data(GAP_LASTVAL_KEY_ANIMATED_CALL_INFO
               , &animCallInfo
               , sizeof(animCallInfo)
               ); 
}

static void 
p_clearGapLastvalAnimatedCallInfo()
{
  GapLastvalAnimatedCallInfo  animCallInfo;

  animCallInfo.animatedCallInProgress = FALSE;
  animCallInfo.total_steps = 0;
  animCallInfo.current_step = 0.0;
  gimp_set_data(GAP_LASTVAL_KEY_ANIMATED_CALL_INFO
               , &animCallInfo
               , sizeof(animCallInfo)
               ); 
}

/* ------------------------
 * gap_filt_pdb_call_plugin
 * ------------------------
 */
gint 
gap_filt_pdb_call_plugin(char *plugin_name, gint32 image_id, gint32 layer_id, GimpRunMode run_mode)
{
  GimpParam       *l_ret_params;
  GimpParam       *l_argv;
  gint             l_retvals;
  gint             l_idx;
  gint             l_nparams;
  gint             l_nreturn_vals;
  GimpPDBProcType  l_proc_type;
  gchar           *l_proc_blurb;
  gchar           *l_proc_help;
  gchar           *l_proc_author;
  gchar           *l_proc_copyright;
  gchar           *l_proc_date;
  GimpParamDef    *l_params;
  GimpParamDef    *l_return_vals;
  gint             l_rc;

  /* query for plugin_name to get its argument types */
  if (!gimp_procedural_db_proc_info (plugin_name,
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
    printf("ERROR: Plugin not available, Name was %s\n", plugin_name);
    return -1;
  }                         

  /* construct the procedures arguments */
  l_argv = g_new (GimpParam, l_nparams);
  memset (l_argv, 0, (sizeof (GimpParam) * l_nparams));

  /* initialize the argument types */
  for (l_idx = 0; l_idx < l_nparams; l_idx++)
  {
    l_argv[l_idx].type = l_params[l_idx].type;
    switch(l_params[l_idx].type)
    {
      case GIMP_PDB_DISPLAY:
        l_argv[l_idx].data.d_display  = -1;
        break;
      case GIMP_PDB_DRAWABLE:
      case GIMP_PDB_LAYER:
      case GIMP_PDB_CHANNEL:
        l_argv[l_idx].data.d_drawable  = layer_id;
        break;
      case GIMP_PDB_IMAGE:
        l_argv[l_idx].data.d_image  = image_id;
        break;
      case GIMP_PDB_INT32:
      case GIMP_PDB_INT16:
      case GIMP_PDB_INT8:
        l_argv[l_idx].data.d_int32  = 0;
        break;
      case GIMP_PDB_FLOAT:
        l_argv[l_idx].data.d_float  = 0.0;
        break;
      case GIMP_PDB_STRING:
        l_argv[l_idx].data.d_string  =  g_strdup("\0");
        break;
      default:
        l_argv[l_idx].data.d_int32  = 0;
        break;
      
    }
  }

  /* init the standard parameters, that should be common to all plugins */
  l_argv[0].data.d_int32     = run_mode;
  l_argv[1].data.d_image     = image_id;
  l_argv[2].data.d_drawable  = layer_id;

  /* run the plug-in procedure */
  l_ret_params = gimp_run_procedure2 (plugin_name, &l_retvals, l_nparams, l_argv);

  /*  free up arguments and values  */
  gimp_destroy_params (l_argv, l_nparams);


  /*  free the query information  */
  g_free (l_proc_blurb);
  g_free (l_proc_help);
  g_free (l_proc_author);
  g_free (l_proc_copyright);
  g_free (l_proc_date);
  g_free (l_params);
  g_free (l_return_vals);


  l_rc = -1;
  if (l_ret_params[0].data.d_status != GIMP_PDB_SUCCESS)
  {
    printf("ERROR: gap_filt_pdb_call_plugin %s failed with status:%d\n"
       , plugin_name
       ,(int)l_ret_params[0].data.d_status
       );
  }
  else
  {
    if(gap_debug)
    {
      printf("DEBUG: gap_filt_pdb_call_plugin: %s successful.\n", plugin_name);
    }
    l_rc = 0;  /* OK */
  }
  gimp_destroy_params(l_ret_params, l_retvals);

  p_clearGapLastvalAnimatedCallInfo();
  
  return(l_rc);
}  /* end gap_filt_pdb_call_plugin */


/* ------------------------
 * gap_filt_pdb_get_data
 * ------------------------
 * gap_filt_pdb_get_data
 *    try to get the plugin's data (key is usually the name of the plugin)
 *    and check for the length of the retrieved data.
 * if all done OK return the length of the retrieved data,
 * return -1 in case of errors.
 */
gint
gap_filt_pdb_get_data(char *key)
{
   int l_len;

   l_len = gimp_get_data_size (key);
   if(l_len < 1)
   {
      printf("ERROR gap_filt_pdb_get_data: no stored data found for Key %s\n", key);
      return -1;
   }
   if(global_plugin_data)
   {
     g_free(global_plugin_data);
   }
   global_plugin_data = g_malloc0(l_len+1);
   gimp_get_data(key, global_plugin_data);

   if(gap_debug) printf("DEBUG gap_filt_pdb_get_data Key:%s  retrieved bytes %d\n", key, (int)l_len);
   return (l_len);
}  /* end  gap_filt_pdb_get_data */

/* ------------------------
 * gap_filt_pdb_set_data
 * ------------------------
 *    set global_plugin_data
 */
void 
gap_filt_pdb_set_data(char *key, gint plugin_data_len)
{
  if(global_plugin_data)
  {
    gimp_set_data(key, global_plugin_data, plugin_data_len);
  }
}  /* end gap_filt_pdb_set_data */










/* --------------------------------
 * gap_filt_pdb_procedure_available
 * --------------------------------
 * return 0 if available, -1 if not available
 *
 * if ptype is GAP_PTYP_ITERATOR then check for typical iterator procedure PDB parameters
 *          and return -1 if the procedure is available but has no typical parameters.
 *
 * if ptype is GAP_PTYP_CAN_OPERATE_ON_DRAWABLE:
 *           return -1 if procedure has not the 3 typical parameters INT32, IMAGE, DRAWABLE
 */
gint
gap_filt_pdb_procedure_available(char  *proc_name, GapFiltPdbProcType ptype)
{
  gint             l_nparams;
  gint             l_nreturn_vals;
  GimpPDBProcType  l_proc_type;
  gchar           *l_proc_blurb;
  gchar           *l_proc_help;
  gchar           *l_proc_author;
  gchar           *l_proc_copyright;
  gchar           *l_proc_date;
  GimpParamDef    *l_params;
  GimpParamDef    *l_return_vals;
  gint             l_rc;

  l_rc = 0;

  if(gap_pdb_procedure_name_available (proc_name) != TRUE)
  {
     if(gap_debug)
     {
       printf("DEBUG: NOT found in PDB %s\n", proc_name);
     }
     return -1;
  }
  
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
     if(gap_debug)
     {
       printf("DEBUG: found in PDB %s\n", proc_name);
     }

     switch(ptype)
     {
        case GAP_PTYP_ITERATOR:
           /* check exactly for Input Parametertypes (common to all Iterators) */
           if (l_proc_type != GIMP_PLUGIN )     { l_rc = -1; break; }
           if (l_nparams  != 4)                    { l_rc = -1; break; }
           if (l_params[0].type != GIMP_PDB_INT32)    { l_rc = -1; break; }
           if (l_params[1].type != GIMP_PDB_INT32)    { l_rc = -1; break; }
           if (l_params[2].type != GIMP_PDB_FLOAT)    { l_rc = -1; break; }
           if (l_params[3].type != GIMP_PDB_INT32)    { l_rc = -1; break; }
           break;
        case GAP_PTYP_CAN_OPERATE_ON_DRAWABLE:
           /* check if plugin can be a typical one, that works on one drawable */
           if (l_proc_type != GIMP_PLUGIN)         { l_rc = -1; break; }
           if (l_nparams  < 3)                      { l_rc = -1; break; }
           if (l_params[0].type !=  GIMP_PDB_INT32)    { l_rc = -1; break; }
           if (l_params[1].type !=  GIMP_PDB_IMAGE)    { l_rc = -1; break; }
           if (l_params[2].type !=  GIMP_PDB_DRAWABLE) { l_rc = -1; break; }
           break;
        default:
           break;
     }
     /*  free the query information  */
     g_free (l_proc_blurb);
     g_free (l_proc_help);
     g_free (l_proc_author);
     g_free (l_proc_copyright);
     g_free (l_proc_date);
     g_free (l_params);
     g_free (l_return_vals);
  }
  else
  {
     /* procedure is not n the PDB */
     return -1;
  }
  

  return l_rc;
}       /* end gap_filt_pdb_procedure_available */


/* ------------------------
 * p_count_iterable_params
 * ------------------------
 *   Count iterable Parameters in the last_values_description.
 */
static gint
p_count_iterable_params(gchar *key_description, gint   desc_size)
{
  GimpLastvalDescType *lastval_desc_arr;
  gint                 l_idx;
  gint                 arg_cnt;
  gint                 l_count;

  l_count  = 0;
  lastval_desc_arr = g_malloc(desc_size);
  arg_cnt = desc_size / sizeof(GimpLastvalDescType);
  gimp_get_data(key_description, lastval_desc_arr);

  for(l_idx = 0; l_idx < arg_cnt; l_idx++)
  {
    if(lastval_desc_arr[l_idx].lastval_type == GIMP_LASTVAL_END)
    {
      break;
    }
    if(lastval_desc_arr[l_idx].iter_flag == GIMP_ITER_TRUE
    && lastval_desc_arr[l_idx].lastval_type > GIMP_LASTVAL_STRUCT_END)
    {
      l_count++;
    }
  }

  if (gap_debug) printf("p_count_iterable_params: %s COUNT: %d\n", key_description, (int)l_count);

  return (l_count);
}  /* end p_count_iterable_params */


/* -----------------------------------------------
 * p_filt_pdb_get_alternative_iterator_proc
 * -----------------------------------------------
 */
static char * 
p_filt_pdb_get_alternative_iterator_proc(char *plugin_name, gint *count)
{
  char      *l_plugin_iterator;
  gchar     *l_key_description;
  gint       l_desc_size;
  
  l_plugin_iterator = NULL;

  /* read all last value descriptions from file and set data in memory */
  gimp_lastval_desc_update();

  /* check for a description of LAST_VALUES buffer (we use a COMMON ITERATOR if available) */
  l_key_description = gimp_lastval_desc_keyname (plugin_name);
  l_desc_size = gimp_get_data_size(l_key_description);

  if(l_desc_size > 0)
  {
    *count = p_count_iterable_params(l_key_description, l_desc_size);
    l_plugin_iterator = g_strdup(GIMP_PLUGIN_GAP_COMMON_ITER);
  }
  g_free(l_key_description);

  if(l_plugin_iterator == NULL)
  {
    l_plugin_iterator = g_strdup_printf("%s%s", plugin_name, GAP_ITERATOR_ALT_SUFFIX);

    /* check for alternative Iterator   -Iterator-ALT
     * for gimp-1.3.x i made some Iterator Plugins using the ending -ALT,
     * If New plugins were added or existing ones were updated
     * the Authors should supply original _Iterator Procedures
     * to be used instead of my Hacked versions without name conflicts.
     *  -Iterator-ALT procedures should be replaced by common iterator in future gimp releases
     */
    if(gap_filt_pdb_procedure_available(l_plugin_iterator, GAP_PTYP_ITERATOR) < 0)
    {
       /* no iterator available */
       g_free(l_plugin_iterator);
       l_plugin_iterator = NULL;
    }
    else
    {
      *count = 1;
    }
  }
  
  return (l_plugin_iterator);
  
}  /* end p_filt_pdb_get_alternative_iterator_proc */


/* --------------------------------
 * gap_filt_pdb_get_iterator_proc
 * --------------------------------
 *   check the PDB for Iterator Procedures in the following order:
 *   1.) a PDB procedurename with suffix "-Iterator"  or
 *   1.1) a PDB procedurename in old naming style with underscore character
 *          and suffix "-Iterator"  
 *   2.) search for a description of LAST_VALUES buffer in file
 *         (and set all available descriptions in memory,
 *          to speed up further searches in this session)
 *   3.) a PDB procedurename with suffix "-Iterator-ALT"
 * return Pointer to the name of the Iterator Procedure
 *        or NULL if not found
 *
 * The name of the common iterator procedure "plug_in_gap_COMMON_ITERATOR"
 * is returned for
 * the case when the description of LAST_VALUES buffer is available
 * and there is no individual Iterator Procedure.
 */
char * 
gap_filt_pdb_get_iterator_proc(const char *plugin_name, gint *count)
{
  char      *l_plugin_iterator;
  char      *canonical_name;
  
  canonical_name = gimp_canonicalize_identifier(plugin_name);
  
  /* check for matching Iterator PluginProcedures */
  l_plugin_iterator = g_strdup_printf("%s%s", canonical_name, GAP_ITERATOR_SUFFIX);

  /* check if iterator (new naming style) is available in PDB */
  if(gap_filt_pdb_procedure_available(l_plugin_iterator, GAP_PTYP_ITERATOR) < 0)
  {
     g_free(l_plugin_iterator);

     *count = 0;
     l_plugin_iterator = p_filt_pdb_get_alternative_iterator_proc(canonical_name, count);
  }
  else
  {
    *count = 1;
  }

  if(gap_debug)
  {
    printf("gap_filt_pdb_get_iterator_proc: END\n  plugin:%s\n  canonical:%s\n  iterator_proc:%s\n"
       , plugin_name
       , canonical_name
       , l_plugin_iterator
       );
  }

  g_free(canonical_name);

  return (l_plugin_iterator);
}       /* end gap_filt_pdb_get_iterator_proc */


/* ---------------------------------
 * gap_filt_pdb_constraint_proc_sel1
 * ---------------------------------
 * constraint procedures
 *
 * are responsible for:
 * - sensitivity of the dbbrowser's Apply Buttons
 * - filter for dbbrowser's listbox
 */

int
gap_filt_pdb_constraint_proc_sel1(gchar *proc_name, gint32 image_id)
{
  /* here we should check, if proc_name
   * can operate on the current Imagetype (RGB, INDEXED, GRAY)
   * if not, 0 should be returned.
   */
  return 1;

#ifdef THIS_IS_A_COMMENT_DONT_COMPILE
  {
    int        l_rc;
    GimpImageBaseType l_base_type;

    l_rc = 0;    /* 0 .. set Apply Button in_sensitive */

    l_base_type = gimp_image_base_type(image_id);
    switch(l_base_type)
    {
      case GIMP_RGB:
      case GIMP_GRAY:
      case GIMP_INDEXED:
        l_rc = 1;
        break;
    }
    return l_rc;
  }  
#endif  
}

int gap_filt_pdb_constraint_proc_sel2(gchar *proc_name, gint32 image_id)
{
  char *l_plugin_iterator;
  int   l_rc;
  gint  l_count;
  
  l_rc = gap_filt_pdb_constraint_proc_sel1(proc_name, image_id);
  if(l_rc != 0)
  {
    l_plugin_iterator =  gap_filt_pdb_get_iterator_proc(proc_name, &l_count);
    if(l_plugin_iterator != NULL)
    {
       g_free(l_plugin_iterator);
       if(l_count > 0)
       {
         /* Plug-In has Iterator and is able to iterate at least 1 Parameter */
         return 1;    /* 1 .. set "Apply Varying" Button sensitive */
       }
    }
  }
  
  return 0;         /* 0 .. set "Apply Varying" Button in_sensitive */
}  /* end  */

/* -------------------------------------------------
 * gap_filt_pdb_check_additional_supported_procedure
 * -------------------------------------------------
 * check for procedures that are able to run with filter all layers
 * (limited to Constant apply) without having an iterator.
 * most of them have no dialog for the interactive runmode.
 *
 * some of them operate without a LAST_VALUES buffer
 * to support even those plug-ins a dummy buffer is added here
 *
 * this hardcoded check is based on tests with gimp-2.2pre1
 */
gboolean
gap_filt_pdb_check_additional_supported_procedure(const char *proc_name)
{
  static gint buf;
  
  /* if(strcmp(proc_name, "plug_in_autocrop_layer") == 0) { return (TRUE); }  */ 

  if(strcmp(proc_name, "plug-in-autostretch-hsv") == 0)   { return (TRUE); }
  if(strcmp(proc_name, "plug-in-blur") == 0)              { return (TRUE); }
  if(strcmp(proc_name, "plug-in-c-astretch") == 0)        { return (TRUE); }
  if(strcmp(proc_name, "plug-in-color-adjust") == 0)      { return (TRUE); }
  if(strcmp(proc_name, "plug-in-color-enhance") == 0)     { return (TRUE); }
  if(strcmp(proc_name, "plug-in-deinterlace") == 0)       { return (TRUE); }
  if(strcmp(proc_name, "plug-in-dilate") == 0)            { return (TRUE); }
  if(strcmp(proc_name, "plug-in-erode") == 0)             { return (TRUE); }
  if(strcmp(proc_name, "plug-in-gradmap") == 0)           { return (TRUE); }
  if(strcmp(proc_name, "plug-in-hot") == 0)               { return (TRUE); }
  if(strcmp(proc_name, "plug-in-laplace") == 0)           { return (TRUE); }
  if(strcmp(proc_name, "plug-in-make-seamless") == 0)
  {
    /* add a dummy LAST_VALUE buffer if there is none */
    if(gimp_get_data_size(proc_name) == 0)
    {
       gimp_set_data(proc_name, &buf, sizeof(buf)); 
    }
    return (TRUE); 
  }
  if(strcmp(proc_name, "plug-in-max-rgb") == 0)           { return (TRUE); }
  if(strcmp(proc_name, "plug-in-normalize") == 0)         { return (TRUE); }
  if(strcmp(proc_name, "plug-in-pixelize2") == 0)         { return (TRUE); }
  if(strcmp(proc_name, "plug-in-qbist") == 0)             { return (TRUE); }
  if(strcmp(proc_name, "plug-in-vinvert") == 0) 
  {
    /* add a dummy LAST_VALUE buffer if there is none */
    if(gimp_get_data_size(proc_name) == 0)
    {
       gimp_set_data(proc_name, &buf, sizeof(buf)); 
    }
    return (TRUE); 
  }

  return (FALSE);
}  /* end gap_filt_pdb_check_additional_supported_procedure */


/* ----------------------------
 * gap_filt_pdb_constraint_proc
 * ----------------------------
 * checks if the specified proc_name
 * is relevant to be available in the GIMP-GAP filter browser.
 *
 * returns 1 for relevant proc_names
 * returns 0 for NON-relevant proc_names
 */
int
gap_filt_pdb_constraint_proc(gchar *proc_name, gint32 image_id)
{
  int l_rc;
  char *l_plugin_iterator;
  gint  l_count;

  if(strncmp(proc_name, "file", 4) == 0)
  {
     /* Do not add file Plugins (check if name starts with "file") */
     return 0;
  }

  if(strncmp(proc_name, "plug-in-gap-", 12) == 0)
  {
     /* Do not add GAP Plugins (check if name starts with "plug-in-gap-") */
     return 0;
  }
  if(strncmp(proc_name, "plug_in_gap_", 12) == 0)
  {
     /* Do not add GAP Plugins (check if name starts with "plug_in_gap_" (old name style)) */
     return 0;
  }
  
  l_rc = gap_filt_pdb_procedure_available(proc_name, GAP_PTYP_CAN_OPERATE_ON_DRAWABLE);

  if(l_rc < 0)
  {
     /* Do not add, Plug-in not available or wrong type */
     return 0;
  }
 
  if(gap_debug)
  {
    /* skip the last check for iterator in debug mode 
     * want to see the other procedures too in that case (hof)
     */
    return 1;    /* 1 add the plugin procedure */
  }

  l_plugin_iterator =  gap_filt_pdb_get_iterator_proc(proc_name, &l_count);
  if(l_plugin_iterator == NULL)
  {
      /* hardcoded check for some known exceptions
       * that are useful for constant apply
       * (even if they have no iterator)
       */
     if(gap_filt_pdb_check_additional_supported_procedure(proc_name))
     {
       return 1;    /* 1 add the plugin procedure */
     }

     /* do not add Plug-In without Iterator or Common Iterator */
     return 0;
  }
  g_free(l_plugin_iterator);

  return 1;    /* 1 add the plugin procedure */
}  /* end gap_filt_pdb_constraint_proc */



/* ------------------------
 * gap_filter_iterator_call
 * ------------------------
 * performs one iteration step
 * for varying the last value buffer for the plug in plugin_name
 * by calling the relevant terator procedure
 * specified by iteratorname.
 *
 */
gboolean
gap_filter_iterator_call(const char *iteratorname
   , gint32      total_steps
   , gdouble     current_step
   , const char *plugin_name
   , gint32      plugin_data_len
)
{
  GimpParam     *l_params;
  gint           l_retvals;
  gboolean        l_rc;

  l_rc = TRUE;
  
  /* set information about the current processed step of an animated filtercall
   * This information is provided to be queried from called plugins
   * so those filters can detect that they are called in animated style
   * (most plugins do not need this information. one example is the
   * gap_fire_pattern filter that uses this information to implement
   * iteration of colorsamples of 2 different gradients)
   */
  p_setGapLastvalAnimatedCallInfo(total_steps, current_step);
  
  /* call plugin-specific iterator (or the common iterator), to modify
   * the plugin's last_values
   */
  if(gap_debug)
  {
    printf("DEBUG: calling iterator %s  current step:%f total:%d\n"
                      , iteratorname
                      , (float)current_step
                      , (int)total_steps
                      );
  }
  if(strcmp(iteratorname, GIMP_PLUGIN_GAP_COMMON_ITER) == 0)
  {
    l_params = gimp_run_procedure (iteratorname,
           &l_retvals,
           GIMP_PDB_INT32,   GIMP_RUN_NONINTERACTIVE,
           GIMP_PDB_INT32,   total_steps,
           GIMP_PDB_FLOAT,   current_step,
           GIMP_PDB_INT32,   plugin_data_len, /* length of stored data struct */
           GIMP_PDB_STRING,  plugin_name,       /* the common iterator needs the plugin name as additional param */
           GIMP_PDB_END);
  }
  else
  {
    l_params = gimp_run_procedure (iteratorname,
           &l_retvals,
           GIMP_PDB_INT32,   GIMP_RUN_NONINTERACTIVE,
           GIMP_PDB_INT32,   total_steps,
           GIMP_PDB_FLOAT,   current_step,
           GIMP_PDB_INT32,   plugin_data_len, /* length of stored data struct */
           GIMP_PDB_END);
  }
  if (l_params[0].data.d_status != GIMP_PDB_SUCCESS)
  {
    printf("ERROR: iterator %s  failed\n", iteratorname);
    l_rc = FALSE;
  }

  gimp_destroy_params(l_params, l_retvals);
  return (l_rc);

}  /* end gap_filter_iterator_call */   

