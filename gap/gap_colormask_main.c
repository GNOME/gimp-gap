/*  gap_colormask_main.c
 *    color mask filter main and dialog procedures
 *    to set alpha channel for a layer according to matching colors
 *    of a color mask (image)  by Wolfgang Hofer
 *  2010/02/21
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
 *  (2010/02/21)  2.7.0       hof: created
 */

#include "config.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <gtk/gtk.h>
#include <libgimp/gimp.h>
#include <libgimp/gimpui.h>

#include "gap_lastvaldesc.h"
#include "gap_colormask_file.h"
#include "gap_colormask_dialog.h"
#include "gap_colormask_exec.h"

#include "gap-intl.h"

/* Defines */
#define PLUG_IN_NAME        "plug-in-layer-set-alpha-by-colormask"
#define PLUG_IN_NAME_2      "plug-in-layer-set-alpha-by-colormask-in-frame"
#define PLUG_IN_NAME_3      "plug-in-layer-create-colormask-in-frame"
#define PLUG_IN_PRINT_NAME  "Name to Layer"
#define PLUG_IN_IMAGE_TYPES "RGB*, INDEXED*, GRAY*"
#define PLUG_IN_AUTHOR      "Wolfgang Hofer (hof@gimp.org)"
#define PLUG_IN_COPYRIGHT   "Wolfgang Hofer"


int gap_debug = 1;  /* 1 == print debug infos , 0 dont print debug infos */



static GapColormaskValues cmaskvals =
{
   -1    /* colormask_id drawable id */
 , 0.03  /* loColorThreshold */
 , 0.12  /* hiColorThreshold */
 , 1.35  /* colorSensitivity   */
 , 0.0   /* lowerOpacity */
 , 1.0   /* upperOpacity */
 , 0.8   /* triggerAlpha */
 , 1.0   /* isleRadius */
 , 7.0   /* isleAreaPixelLimit */
 , 2.0   /* featherRadius */
 , 0.1   /* edgeColorThreshold */
 , 0.0   /* thresholdColorArea */
 , 6.0   /* pixelDiagonal      */
 , 10.0  /* pixelAreaLimit     */
 , 0     /* connectByCorner    */

 , 1     /* keepLayerMask */
 , 0     /* keepWorklayer */


 , 0            /* gboolean   enableKeyColorThreshold */
 , { 0,0,0,0 }  /* GimpRGB keycolor */
 , 0.06         /* gdouble    loKeyColorThreshold */
 , 1.0          /* gdouble    keyColorSensitivity */


 , GAP_COLORMASK_ALGO_AVG_SMART
};




static GimpDrawable *globalDrawable = NULL;

static void  query (void);
static void  run (const gchar *name,          /* name of plugin */
     gint nparams,               /* number of in-paramters */
     const GimpParam * param,    /* in-parameters */
     gint *nreturn_vals,         /* number of out-parameters */
     GimpParam ** return_vals);  /* out-parameters */

static gint  p_colormask_apply_run (gint32 image_id, gint32 drawable_id, gboolean doProgress, const char *name);




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
                  { GIMP_PDB_DRAWABLE, "drawable", "layer with alpha channel to be processed"},
                  { GIMP_PDB_DRAWABLE, "colormaskDrawable", "rgb drawable (optional with with alpha channel) to be applied"},
                  { GIMP_PDB_FLOAT,    "loColorThreshold", "0.0 to 1.0 color difference lower threshold value (diff below this is considered as matching color)"},
                  { GIMP_PDB_FLOAT,    "hiColorThreshold", "0.0 to 1.0 color difference upper threshold value (diff above this is considered as not matching color)"},
                  { GIMP_PDB_FLOAT,    "colorSensitivity", "1 .. 2 sensitivity for color dff algorithm"},
                  { GIMP_PDB_FLOAT,    "lowerOpacity", "0.0 to 1.0 opacity value for matching pixels"},
                  { GIMP_PDB_FLOAT,    "upperOpacity", "0.0 to 1.0 opacity value for non matching pixels"},
                  { GIMP_PDB_FLOAT,    "triggerAlpha", "0.0 to 1.0 in case the colormask has alpha, greater values than the specifiedone trigger colormasking "},
                  { GIMP_PDB_FLOAT,    "isleRadius",   "radius in pixels for remove isolated pixels"},
                  { GIMP_PDB_FLOAT,    "isleAreaPixelLimit",   "area size in pixels for removal of isolated pixel areas )"},
                  { GIMP_PDB_FLOAT,    "featherRadius", "radius in pixels for smoothing edges "},
                  { GIMP_PDB_FLOAT,    "edgeColorThreshold", "color difference for edge detection (relevant for algorthm 1, range 0.0 to 1.0) "},
                  { GIMP_PDB_FLOAT,    "thresholdColorArea", "threshold for color difference colors belonging to same area. (relevant for algorithm 2 range .0 to 1.0) "},
                  { GIMP_PDB_FLOAT,    "pixelDiagonal", "areas with smaller diagonale are considered small isles (relevant for algorithm 2)"},
                  { GIMP_PDB_FLOAT,    "pixelAreaLimit", "areas with less pixels are considered as small isles  (relevant for algorithm 2)"},
                  { GIMP_PDB_INT32,    "connectByCorner", "pixels toching only at corners ar pat/not part of the area valid range is (relevant for algorithm 2) "},
                  { GIMP_PDB_INT32,    "keepLayerMask", "0 apply the generated layerMask, 1 keep the generated layermask"},
                  { GIMP_PDB_INT32,    "keepWorklayer", "0 delete internal worklayer, 1 keep the generated layermask (relevant for algorithm 2 for DEBUG purpose)"},
                  { GIMP_PDB_INT32,    "enableKeyColorThreshold", "0 disable, 1 enable individual threshold for keycolor"},
                  { GIMP_PDB_COLOR,    "keycolor", "KeyColor" },
                  { GIMP_PDB_FLOAT,    "loKeyColorThreshold", "0.0 to 1.0 color difference lower individual threshold value for the key color"},
                  { GIMP_PDB_FLOAT,    "keyColorSensitivity", "0.0 to 10.0 "},
                  { GIMP_PDB_INT32,    "algorithm", "0 simple, 1 edge, 2 area"}
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
    GIMP_LASTVALDEF_DRAWABLE        (GIMP_ITER_FALSE,  cmaskvals.colormask_id,            "colormaskDrawable"),
    GIMP_LASTVALDEF_GDOUBLE         (GIMP_ITER_FALSE,  cmaskvals.loColorThreshold,        "loColorThreshold"),
    GIMP_LASTVALDEF_GDOUBLE         (GIMP_ITER_FALSE,  cmaskvals.hiColorThreshold,        "hiColorThreshold"),
    GIMP_LASTVALDEF_GDOUBLE         (GIMP_ITER_FALSE,  cmaskvals.colorSensitivity,        "colorSensitivity"),
    GIMP_LASTVALDEF_GDOUBLE         (GIMP_ITER_TRUE,   cmaskvals.lowerOpacity,            "lowerOpacity"),
    GIMP_LASTVALDEF_GDOUBLE         (GIMP_ITER_TRUE,   cmaskvals.upperOpacity,            "upperOpacity"),
    GIMP_LASTVALDEF_GDOUBLE         (GIMP_ITER_TRUE,   cmaskvals.triggerAlpha,            "triggerAlpha"),
    GIMP_LASTVALDEF_GDOUBLE         (GIMP_ITER_TRUE,   cmaskvals.isleRadius,              "isleRadius"),
    GIMP_LASTVALDEF_GDOUBLE         (GIMP_ITER_TRUE,   cmaskvals.isleAreaPixelLimit,      "isleAreaPixelLimit"),
    GIMP_LASTVALDEF_GDOUBLE         (GIMP_ITER_TRUE,   cmaskvals.featherRadius,           "featherRadius"),
    GIMP_LASTVALDEF_GDOUBLE         (GIMP_ITER_TRUE,   cmaskvals.edgeColorThreshold,      "edgeColorThreshold"),
    GIMP_LASTVALDEF_GDOUBLE         (GIMP_ITER_TRUE,   cmaskvals.thresholdColorArea,      "thresholdColorArea"),
    GIMP_LASTVALDEF_GDOUBLE         (GIMP_ITER_TRUE,   cmaskvals.pixelDiagonal,           "pixelDiagonal"),
    GIMP_LASTVALDEF_GDOUBLE         (GIMP_ITER_TRUE,   cmaskvals.pixelAreaLimit,          "pixelAreaLimit"),
    GIMP_LASTVALDEF_GINT32          (GIMP_ITER_FALSE,  cmaskvals.connectByCorner,         "connectByCorner"),
    GIMP_LASTVALDEF_GINT32          (GIMP_ITER_FALSE,  cmaskvals.keepLayerMask,           "keepLayerMask"),
    GIMP_LASTVALDEF_GINT32          (GIMP_ITER_FALSE,  cmaskvals.keepWorklayer,           "keepWorklayer"),
    GIMP_LASTVALDEF_GINT32          (GIMP_ITER_FALSE,  cmaskvals.enableKeyColorThreshold, "enableKeyColorThreshold"),
    GIMP_LASTVALDEF_GIMPRGB         (GIMP_ITER_TRUE,   cmaskvals.keycolor,                "keycolor"),
    GIMP_LASTVALDEF_GDOUBLE         (GIMP_ITER_TRUE,   cmaskvals.loKeyColorThreshold,     "loKeyColorThreshold"),
    GIMP_LASTVALDEF_GDOUBLE         (GIMP_ITER_TRUE,   cmaskvals.keyColorSensitivity,     "keyColorSensitivity"),
    GIMP_LASTVALDEF_GINT            (GIMP_ITER_FALSE,  cmaskvals.algorithm,               "algorithm")
  };



  gimp_plugin_domain_register (GETTEXT_PACKAGE, LOCALEDIR);

  /* registration for last values buffer structure (useful for animated filter apply) */
  gimp_lastval_desc_register(PLUG_IN_NAME,
                             &cmaskvals,
                             sizeof(cmaskvals),
                             G_N_ELEMENTS (lastvals),
                             lastvals);
//   gimp_lastval_desc_register(PLUG_IN_NAME_2,
//                              &cmaskvals,
//                              sizeof(cmaskvals),
//                              G_N_ELEMENTS (lastvals),
//                              lastvals);
//   gimp_lastval_desc_register(PLUG_IN_NAME_3,
//                              &cmaskvals,
//                              sizeof(cmaskvals),
//                              G_N_ELEMENTS (lastvals),
//                              lastvals);

  /* the actual installation of the plugin */
  gimp_install_procedure (PLUG_IN_NAME,
                          "Set layer opacity by applying another color drawable as colormask",
                          "This plug-in sets the opacity for the specified Input drawable (a layer), "
                          "according to matching colors of the specified colormask drawable. "
                          "The resulting opacity is created as layermask. "
                          "This newly created layermask is applied optionally (in case parameter keepLayerMask is FALSE)."
                          "Note that the colormask drawable may have an alpha channel."
                          "in this case the transparent parts of the colormask are protected areas, "
                          "that are not affected by this filter. "
                          "(i.e. the corresponding pixels in processed layer keep their original opacity) "
                          " ",
                          PLUG_IN_AUTHOR,
                          PLUG_IN_COPYRIGHT,
                          GAP_VERSION_WITH_DATE,
                          N_("Apply Colormask..."),
                          PLUG_IN_IMAGE_TYPES,
                          GIMP_PLUGIN,
                          global_number_in_args,
                          global_number_out_args,
                          in_args,
                          return_vals);
//   gimp_install_procedure (PLUG_IN_NAME_2,
//                           "Set Layer Opacity by applying the colormask layer colormask",
//                           "This plug-in sets the alpha channel for the specified Input drawable (a layer), "
//                           "according to matching colors of the colormask. The colormask must be a layer in the same image "
//                           "with the layer name colormask"
//                           "this procedure is intended for processing with individual per frame colormask "
//                           " ",
//                           PLUG_IN_AUTHOR,
//                           PLUG_IN_COPYRIGHT,
//                           GAP_VERSION_WITH_DATE,
//                           N_("Apply Colormask in Frame..."),
//                           PLUG_IN_IMAGE_TYPES,
//                           GIMP_PLUGIN,
//                           global_number_in_args,
//                           global_number_out_args,
//                           in_args,
//                           return_vals);
//   gimp_install_procedure (PLUG_IN_NAME_3,
//                           "Create or replace a individual per frame colormask as top layer",
//                           "The colormask is created as mix of the processed original layer and, "
//                           "an initial colormask and /or the colormask from the previous frame. "
//                           "(the processed drawable shall be a layer of a frame image)"
//                           "xxxxxxx "
//                           " ",
//                           PLUG_IN_AUTHOR,
//                           PLUG_IN_COPYRIGHT,
//                           GAP_VERSION_WITH_DATE,
//                           N_("Create Colormask..."),
//                           PLUG_IN_IMAGE_TYPES,
//                           GIMP_PLUGIN,
//                           global_number_in_args,
//                           global_number_out_args,
//                           in_args,
//                           return_vals);

  {
    /* Menu names */
    const char *menupath_image_video_layer_attr = N_("<Image>/Video/Layer/Attributes/");

    //gimp_plugin_menu_branch_register("<Image>", "Video");
    //gimp_plugin_menu_branch_register("<Image>/Video", "Layer");

    gimp_plugin_menu_register (PLUG_IN_NAME, menupath_image_video_layer_attr);
//     gimp_plugin_menu_register (PLUG_IN_NAME_2, menupath_image_video_layer_attr);
//     gimp_plugin_menu_register (PLUG_IN_NAME_3, menupath_image_video_layer_attr);
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
  gboolean doProgress;



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


  doProgress = FALSE;

  /* initialize the return of the status */
  values[0].type = GIMP_PDB_STATUS;
  values[0].data.d_status = status;
  values[1].type = GIMP_PDB_DRAWABLE;
  values[1].data.d_drawable = -1;
  *nreturn_vals = 2;
  *return_vals = values;


  /* get image and drawable */
  image_id = param[1].data.d_int32;

  /* Get drawable information */
  globalDrawable = gimp_drawable_get (param[2].data.d_drawable);


  /* how are we running today? */
  switch (run_mode)
  {
    case GIMP_RUN_INTERACTIVE:
      /* Possibly retrieve data from a previous run */
      gimp_get_data (name, &cmaskvals);

      /* Get information from the dialog */
      if (!gap_colormask_dialog(&cmaskvals, globalDrawable))
      {
        return;
      }
      doProgress = TRUE;
      break;

    case GIMP_RUN_NONINTERACTIVE:
      /* check to see if invoked with the correct number of parameters */
      if (nparams == global_number_in_args)
      {
          cmaskvals.colormask_id        = (gint32)  param[3].data.d_drawable;
          cmaskvals.loColorThreshold    = (gdouble) param[4].data.d_float;
          cmaskvals.hiColorThreshold    = (gdouble) param[5].data.d_float;
          cmaskvals.colorSensitivity    = (gdouble) param[6].data.d_float;
          cmaskvals.lowerOpacity        = (gdouble) param[7].data.d_float;
          cmaskvals.upperOpacity        = (gdouble) param[8].data.d_float;
          cmaskvals.triggerAlpha        = (gdouble) param[9].data.d_float;
          cmaskvals.isleRadius          = (gdouble) param[10].data.d_float;
          cmaskvals.isleAreaPixelLimit  = (gdouble) param[11].data.d_float;
          cmaskvals.featherRadius       = (gdouble) param[12].data.d_float;

          cmaskvals.thresholdColorArea  = (gdouble) param[13].data.d_float;
          cmaskvals.pixelDiagonal       = (gdouble) param[14].data.d_float;
          cmaskvals.pixelAreaLimit      = (gdouble) param[15].data.d_float;
          cmaskvals.connectByCorner     = (param[16].data.d_int32 == 0) ? FALSE : TRUE;
          cmaskvals.keepLayerMask       = (param[17].data.d_int32 == 0) ? FALSE : TRUE;
          cmaskvals.keepWorklayer       = (param[18].data.d_int32 == 0) ? FALSE : TRUE;

          cmaskvals.enableKeyColorThreshold       = (param[19].data.d_int32 == 0) ? FALSE : TRUE;
          cmaskvals.keycolor                      = param[20].data.d_color;
          cmaskvals.loKeyColorThreshold           = (gdouble) param[21].data.d_float;
          cmaskvals.keyColorSensitivity           = (gdouble) param[22].data.d_float;

          cmaskvals.algorithm           =  param[23].data.d_int32;
      }
      else
      {
        status = GIMP_PDB_CALLING_ERROR;
      }

      break;

    case GIMP_RUN_WITH_LAST_VALS:
      /* Possibly retrieve data from a previous run */
      gimp_get_data (name, &cmaskvals);

      break;

    default:
      break;
  }

  if (status == GIMP_PDB_SUCCESS)
  {
    /* Run the main function */
    values[1].data.d_drawable = p_colormask_apply_run(image_id, param[2].data.d_drawable, doProgress, name);
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
      gimp_set_data (name, &cmaskvals, sizeof (GapColormaskValues));

    if(run_mode != GIMP_RUN_NONINTERACTIVE)
      gimp_drawable_flush (globalDrawable);
  }
  values[0].data.d_status = status;


  gimp_drawable_detach (globalDrawable);
}       /* end run */



/* ----------------------
 * p_colormask_apply_run
 * ----------------------
 * The processing functions
 *
 */
static gint32
p_colormask_apply_run (gint32 image_id, gint32 drawable_id, gboolean doProgress, const char *name)
{
  gint32     retLayerId;

  if (!gimp_drawable_is_layer(drawable_id))
  {
    return (-1);  /* dont operate on other drawables than layers */
  }
  if (!gimp_drawable_has_alpha(drawable_id))
  {
    return (-1);  /* dont operate on layers without alpha channel */
  }

  if (strcmp(name, PLUG_IN_NAME_3) == 0)
  {
    retLayerId = gap_create_or_replace_colormask_layer (drawable_id
                        , &cmaskvals
                        , doProgress
                        );
  }
  else if (strcmp(name, PLUG_IN_NAME_2) == 0)
  {
    retLayerId = gap_colormask_apply_by_name (drawable_id
                        , &cmaskvals
                        , doProgress
                        );
  }
  else
  {
    retLayerId = gap_colormask_apply_to_layer_of_same_size (drawable_id
                        , &cmaskvals
                        , doProgress
                        );
  }

  return (retLayerId);
}       /* end p_colormask_apply_run */

