/* gap_fmac_varying_main.c
 * 2006.12.11 hof (Wolfgang Hofer)
 *
 * GAP ... Gimp Animation Plugins
 *
 * This Module contains:
 * - filtermacro execution with varying values
 *   For NON-INTERACTIVE callers (e.g the storyboard processor)
 *   Allows to apply filters with a mix of parametervalues
 *   where the parameters are provided in 2 filtermacro files
 *
 * see also gap_fmac_main.c for basic filtermacro implementation
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
#include "gap_filter.h"
#include "gap_filter_pdb.h"
#include "gap_fmac_name.h"
#include "gap_fmac_base.h"

/* revision history:
 * gimp   2.2.x;    2006/12/11  hof: created.
 */

/* from gap_fmac_name.h: GAP_FMACNAME_PLUG_IN_NAME_FMAC_VARYING */

#define FMAC_FILE_LENGTH  1500

/* ------------------------
 * global gap DEBUG switch
 * ------------------------
 */

/* int gap_debug = 1; */    /* print debug infos */
/* int gap_debug = 0; */    /* 0: dont print debug infos */

int gap_debug = 0;


/* ############################################################# */


static void query(void);
static void run(const gchar *name
              , gint n_params
          , const GimpParam *param
              , gint *nreturn_vals
          , GimpParam **return_vals);

GimpPlugInInfo PLUG_IN_INFO =
{
  NULL,  /* init_proc */
  NULL,  /* quit_proc */
  query, /* query_proc */
  run,   /* run_proc */
};

MAIN ()

static void
query ()
{
  static GimpParamDef args_fmac_varying[] =
  {
    {GIMP_PDB_INT32, "run_mode", "Interactive"},
    {GIMP_PDB_IMAGE, "image", "Input image"},
    {GIMP_PDB_DRAWABLE, "drawable", "Input drawable to be affected by the filtermacro"},
    {GIMP_PDB_STRING, "filtermacro_1", "Name of the 1st filtermacro_file to execute on the input drawable)"},
    {GIMP_PDB_STRING, "filtermacro_2", "Name of the 2nd filtermacro_file to execute on the input drawable)"},
    {GIMP_PDB_FLOAT, "current_step", "current_step. (e.g curently processed frame) "
                                      " valid range is 0.0 upto total_steps, "
                                      " where 0.0 uses the parameter definitions from filtermacro_1. "
                                      " current_step divided by total_steps defines the value mix ratio"
                                      " A value mix ratio of 1.0 will use the parameter values of filtermacro_2."},
    {GIMP_PDB_INT32, "total_steps",  "total number of steps of varying iterations (e.g. number of frames to process)"},
    
  };


  static GimpParamDef *return_vals = NULL;
  static int nreturn_vals = 0;

  gimp_plugin_domain_register (GETTEXT_PACKAGE, LOCALEDIR);



  gimp_install_procedure(GAP_FMACNAME_PLUG_IN_NAME_FMAC_VARYING,
             "This plug-in executes 2 filtermacro scripts, where parameter values of script 1 and 2 are mixed.",
             "This plug-in allows the non-interactive caller to execute correlated filters "
             "that have already been recorded in 2 filtermacro files where the parameter "
             "values are a mix of filtermacro from file1 and file2 related to progress. "
             "progress is specified by the total_steps and current_step parameters "
             "if current_step is 0.0 use the values as defined in filtermacro file1. "
             "the mix increases the influence of parameter values as definewd in filtermacro file2 "
             " the more current_step approaches total_steps."
             "Correlation is done by name of the filter and position in the filtermacro file1. "
             "filternames that are only present in filtermacro file2 are ignored. "
             "filternames that have no correlated matching filtername in filtermacro file2 "
             "are executed with values as defined in filtermacro file1, independent from current_step. "
             "This non-interactive API is typically used by the GAP storyboard processor for "
             "applying filtermacros with varying values. "
             "WARNING: filtermacro scriptfiles are a temporary solution. "
             "They are machine dependent. Support may be dropped in future gimp "
             "versions.",
             "Wolfgang Hofer (hof@gimp.org)",
             "Wolfgang Hofer",
             GAP_VERSION_WITH_DATE,
             NULL,  /* dont appear in menus */
             "RGB*, INDEXED*, GRAY*",
             GIMP_PLUGIN,
             G_N_ELEMENTS (args_fmac_varying), nreturn_vals,
             args_fmac_varying, return_vals);

}  /* end query */


static void
run(const gchar *name
   , gint n_params
   , const GimpParam *param
   , gint *nreturn_vals
   , GimpParam **return_vals)
{
  static GimpParam values[1];
  GimpRunMode run_mode;
  GimpPDBStatusType status = GIMP_PDB_SUCCESS;
  gint32     image_id;
  gint32     drawable_id;
  char     *filtermacro_file1;
  char     *filtermacro_file2;
  gdouble   current_step;
  gint32    total_steps;

  gint32      l_rc;
  const char *l_env;

  *nreturn_vals = 1;
  *return_vals = values;
  l_rc = 0;

  l_env = g_getenv("GAP_DEBUG");
  if(l_env != NULL)
  {
    if((*l_env != 'n') && (*l_env != 'N')) gap_debug = 1;
  }


  run_mode = param[0].data.d_int32;
  image_id = param[1].data.d_image;
  drawable_id = param[2].data.d_drawable;
  filtermacro_file1 = NULL;
  filtermacro_file2 = NULL;
  current_step = 0.0;
  total_steps = 1;

  INIT_I18N ();


  if(gap_debug)
  {
    fprintf(stderr, "\n\ngap_fmac_varying_main: debug name = %s\n", name);
  }

  if (strcmp (name, GAP_FMACNAME_PLUG_IN_NAME_FMAC_VARYING) == 0)
  {
    if (run_mode != GIMP_RUN_NONINTERACTIVE)
    {
      status = GIMP_PDB_CALLING_ERROR;
      l_rc = -1;
    }
    else
    {
      if(n_params == 7)
      {
        filtermacro_file1 = param[3].data.d_string;
        filtermacro_file2 = param[4].data.d_string;
        current_step = param[5].data.d_float;
        total_steps = param[6].data.d_int32;
        if((filtermacro_file1 == NULL)
        || (filtermacro_file2 == NULL))
        {
              status = GIMP_PDB_CALLING_ERROR;
        }
      }
      else
      {
            status = GIMP_PDB_CALLING_ERROR;
      }

      if(status == GIMP_PDB_SUCCESS)
      {
        l_rc = gap_fmac_execute(run_mode, image_id, drawable_id
                   , filtermacro_file1
                   , filtermacro_file2
                   , current_step
                   , total_steps
                   );
      }
    }
  }
  else
  {
      status = GIMP_PDB_CALLING_ERROR;
  }

  if(l_rc < 0)
  {
    status = GIMP_PDB_EXECUTION_ERROR;
  }


  if (run_mode != GIMP_RUN_NONINTERACTIVE)
  {
    gimp_displays_flush();
  }

  values[0].type = GIMP_PDB_STATUS;
  values[0].data.d_status = status;

}  /* end run */




