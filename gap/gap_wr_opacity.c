/*  gap_wr_opacity.c
 *    wrapper plugin to set Layer Opacity  by Wolfgang Hofer
 *  2004/01/15
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

/* Revision history
 * 1.3.25a; 2004/01/21   hof: message text fixes (# 132030)
 *  (2004/01/15)  v1.0       hof: created
 */

#include "config.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <gtk/gtk.h>
#include <libgimp/gimp.h>
#include <libgimp/gimpui.h>

#include "gap_arr_dialog.h"
#include "gap_lastvaldesc.h"

#include "gap-intl.h"

/* Defines */
#define PLUG_IN_NAME        "plug-in-wr-set-opacity"
#define PLUG_IN_PRINT_NAME  "Name to Layer"
#define PLUG_IN_IMAGE_TYPES "RGB*, INDEXED*, GRAY*"
#define PLUG_IN_AUTHOR      "Wolfgang Hofer (hof@gimp.org)"
#define PLUG_IN_COPYRIGHT   "Wolfgang Hofer"

int gap_debug = 0;  /* 1 == print debug infos , 0 dont print debug infos */


typedef struct {
  gdouble opacity;
  gint32  mode;
} OpaValues;

typedef struct {
  gint run;
} OpaInterface;



static OpaValues glob_vals =
{
   100.0
 , 0           /* 0..set, 1..add, 2..sub */
};


static void  query (void);
static void  run (const gchar *name,          /* name of plugin */
     gint nparams,               /* number of in-paramters */
     const GimpParam * param,    /* in-parameters */
     gint *nreturn_vals,         /* number of out-parameters */
     GimpParam ** return_vals);  /* out-parameters */

static gint  p_opa_run (gint32 image_id, gint32 drawable_id);
static gint  p_opa_dialog (void);


/* Global Variables */
GimpPlugInInfo PLUG_IN_INFO =
{
  NULL,   /* init_proc  */
  NULL,   /* quit_proc  */
  query,  /* query_proc */
  run     /* run_proc   */
};

static GimpParamDef in_args[] = {
                  { GIMP_PDB_INT32,    "run_mode", "Interactive, non-interactive"},
                  { GIMP_PDB_IMAGE,    "image", "Input image" },
                  { GIMP_PDB_DRAWABLE, "drawable", "Input drawable ()"},
                  { GIMP_PDB_FLOAT,    "opacity", "0.0 (full transparent) upto 100.0 (full opaque)"},
                  { GIMP_PDB_INT32,    "mode", "0..set opacity, 1..ADD opacity to old opacity value, 2..subtract opacity from old opacity value, 3..multiply"},
  };

static GimpParamDef return_vals[] = {
    { GIMP_PDB_DRAWABLE, "the_drawable", "the handled drawable" }
};

static gint global_number_in_args = G_N_ELEMENTS (in_args);
static gint global_number_out_args = G_N_ELEMENTS (return_vals);


/* Functions */

MAIN ()

static void query (void)
{
  static GimpLastvalDef lastvals[] =
  {
    GIMP_LASTVALDEF_GDOUBLE         (GIMP_ITER_TRUE,   glob_vals.opacity,  "opacity"),
    GIMP_LASTVALDEF_GINT32          (GIMP_ITER_FALSE,  glob_vals.mode,     "mode"),
  };



  gimp_plugin_domain_register (GETTEXT_PACKAGE, LOCALEDIR);

  /* registration for last values buffer structure (useful for animated filter apply) */
  gimp_lastval_desc_register(PLUG_IN_NAME,
                             &glob_vals,
                             sizeof(glob_vals),
                             G_N_ELEMENTS (lastvals),
                             lastvals);

  /* the actual installation of the plugin */
  gimp_install_procedure (PLUG_IN_NAME,
                          "Set Layer Opacity",
                          "This plug-in is a wrapper for gimp_layer_set opacity functionality, "
                          "and provides the typical PDB interface for calling it as "
                          "filter with varying values."
                          "(for the use with GAP Video Frame manipulation)",
                          PLUG_IN_AUTHOR,
                          PLUG_IN_COPYRIGHT,
                          GAP_VERSION_WITH_DATE,
                          N_("Set Layer Opacity..."),
                          PLUG_IN_IMAGE_TYPES,
                          GIMP_PLUGIN,
                          global_number_in_args,
                          global_number_out_args,
                          in_args,
                          return_vals);
  {
    /* Menu names */
    const char *menupath_image_video_layer_attr = N_("<Image>/Video/Layer/Attributes/");

    //gimp_plugin_menu_branch_register("<Image>", "Video");
    //gimp_plugin_menu_branch_register("<Image>/Video", "Layer");

    gimp_plugin_menu_register (PLUG_IN_NAME, menupath_image_video_layer_attr);
  }

}  /* end query */

static void
run (const gchar *name,          /* name of plugin */
     gint nparams,               /* number of in-paramters */
     const GimpParam * param,    /* in-parameters */
     gint *nreturn_vals,         /* number of out-parameters */
     GimpParam ** return_vals)   /* out-parameters */
{
  const gchar *l_env;
  gint32       image_id = -1;


  /* Get the runmode from the in-parameters */
  GimpRunMode run_mode = param[0].data.d_int32;

  /* status variable, use it to check for errors in invocation usualy only
     during non-interactive calling */
  GimpPDBStatusType status = GIMP_PDB_SUCCESS;

  /* always return at least the status to the caller. */
  static GimpParam values[2];

  INIT_I18N();

  l_env = g_getenv("GAP_DEBUG");
  if(l_env != NULL)
  {
    if((*l_env != 'n') && (*l_env != 'N')) gap_debug = 1;
  }

  if(gap_debug) printf("\n\nDEBUG: run %s\n", name);

  /* initialize the return of the status */
  values[0].type = GIMP_PDB_STATUS;
  values[0].data.d_status = status;
  values[1].type = GIMP_PDB_DRAWABLE;
  values[1].data.d_drawable = -1;
  *nreturn_vals = 2;
  *return_vals = values;


  /* get image and drawable */
  image_id = param[1].data.d_int32;


  /* how are we running today? */
  switch (run_mode)
  {
    case GIMP_RUN_INTERACTIVE:
      /* Possibly retrieve data from a previous run */
      gimp_get_data (PLUG_IN_NAME, &glob_vals);

      /* Get information from the dialog */
      if (p_opa_dialog() != 0)
                return;
      break;

    case GIMP_RUN_NONINTERACTIVE:
      /* check to see if invoked with the correct number of parameters */
      if (nparams == global_number_in_args)
      {
          glob_vals.opacity     = (gdouble) param[3].data.d_float;
          glob_vals.mode        = (gint32)  param[4].data.d_int32;
      }
      else
      {
        status = GIMP_PDB_CALLING_ERROR;
      }

      break;

    case GIMP_RUN_WITH_LAST_VALS:
      /* Possibly retrieve data from a previous run */
      gimp_get_data (PLUG_IN_NAME, &glob_vals);

      break;

    default:
      break;
  }

  if (status == GIMP_PDB_SUCCESS)
  {
    /* Run the main function */
    values[1].data.d_drawable = p_opa_run(image_id, param[2].data.d_drawable);
    if (values[1].data.d_drawable < 0)
    {
       status = GIMP_PDB_CALLING_ERROR;
    }

    /* If run mode is interactive, flush displays, else (script) don't
       do it, as the screen updates would make the scripts slow */
    if (run_mode != GIMP_RUN_NONINTERACTIVE)
      gimp_displays_flush ();

    /* Store variable states for next run */
    if (run_mode == GIMP_RUN_INTERACTIVE)
      gimp_set_data (PLUG_IN_NAME, &glob_vals, sizeof (OpaValues));
  }
  values[0].data.d_status = status;
}       /* end run */



/* ============================================================================
 * p_opa_run
 *        The main function
 * ============================================================================
 */
static gint32
p_opa_run (gint32 image_id, gint32 drawable_id)
{
  gdouble l_old_opacity;
  gdouble l_new_opacity;


  if (!gimp_drawable_is_layer(drawable_id))
  {
    return (-1);  /* dont operate on other drawables */
  }

  l_old_opacity = gimp_layer_get_opacity(drawable_id);


  switch (glob_vals.mode)
  {
    case 3:
      l_new_opacity = l_old_opacity * glob_vals.opacity;
      break;
    case 2:
      l_new_opacity = l_old_opacity - glob_vals.opacity;
      break;
    case 1:
      l_new_opacity = l_old_opacity + glob_vals.opacity;
      break;
    default:
      l_new_opacity = glob_vals.opacity;
      break;
  }

  if(gap_debug)
  {
    printf("mode: %d\n", (int)glob_vals.mode);
    printf("opacity: %f\n", (float)glob_vals.opacity);
    printf("l_old_opacity: %f\n", (float)l_old_opacity);
    printf("l_new_opacity: %f\n", (float)l_new_opacity);
  }

  gimp_layer_set_opacity(drawable_id, CLAMP(l_new_opacity, 0.0, 100.0));

  return (drawable_id);
}       /* end p_opa_run */


/* ------------------
 * p_opa_dialog
 * ------------------
 *   return  0 .. OK
 *          -1 .. in case of Error or cancel
 */
static gint
p_opa_dialog(void)
{
#define WR_OPA_DIALOG_ARGC 3
#define VR_MODELIST_SIZE 4
  static GapArrArg  argv[WR_OPA_DIALOG_ARGC];
  gint ii;
  gint ii_mode;
  gint ii_opacity;
  static char *radio_modes[VR_MODELIST_SIZE]  = {"Set", "Add", "Subtract", "Multiply" };

  ii=0; gap_arr_arg_init(&argv[ii], GAP_ARR_WGT_FLT); ii_opacity = ii;
  argv[ii].constraint = TRUE;
  argv[ii].label_txt = _("Opacity:");
  argv[ii].help_txt  = _("New opacity value where 0 is transparent and 100.0 is opaque");
  argv[ii].flt_min   = 0.0;
  argv[ii].flt_max   = 100.0;
  argv[ii].flt_ret   = (gint)glob_vals.opacity;
  argv[ii].flt_step  =  1.0;
  argv[ii].entry_width = 80;
  argv[ii].has_default = TRUE;
  argv[ii].flt_default = 100.0;


  ii++; gap_arr_arg_init(&argv[ii], GAP_ARR_WGT_OPTIONMENU); ii_mode = ii;
  argv[ii].label_txt = _("Mode:");
  argv[ii].help_txt  = _("Modes set opacity or change the old opacity value by adding, subtracting or multiplying by the supplied new value");
  argv[ii].radio_argc  = VR_MODELIST_SIZE;
  argv[ii].radio_argv = radio_modes;
  argv[ii].radio_ret  = 0;
  argv[ii].int_default = 0;
  argv[ii].has_default = TRUE;
  argv[ii].text_buf_default = g_strdup("\0");



  ii++; gap_arr_arg_init(&argv[ii], GAP_ARR_WGT_DEFAULT_BUTTON);
  argv[ii].label_txt = _("Default");
  argv[ii].help_txt  = _("Reset all parameters to default values");

  if(TRUE == gap_arr_ok_cancel_dialog(_("Set Layer Opacity"),
                            _("Settings :"),
                            WR_OPA_DIALOG_ARGC, argv))
  {
      glob_vals.opacity          = (gdouble)(argv[ii_opacity].flt_ret);
      glob_vals.mode             = (gint32)(argv[ii_mode].radio_ret);
      return 0;
  }
  else
  {
      return -1;
  }
}               /* end p_opa_dialog */
