/*  gap_fg_matting_main.c
 *    foreground extraction based on alpha matting algorithm.
 *    Render transparancy for a layer based on a tri-map drawable (provided by the user).
 *    The tri-map defines what pixels are FOREGROUND (white) BACKGROUND (black) or UNDEFINED (gray).
 *    foreground extraction affects the UNDEFINED pixels and calculates transparency 
 *    (and optionally color) for those pixels.
 *
 *    This module handles PDB registration as GIMP plug-ins
 *    Implemented Plug-in variants:
 *     a) The user provides the tri map as layermask or as independent layer
 *     b) tri map is generated from the current selection where the user
 *        specifies the width of undefined range inside and outside the selection borderline.
 *        Alpha matting is used to trim the undefined borderline.
 *     
 *  2011/11/15
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
 *  (2011/10/05)  2.7.0       hof: created
 */
int gap_debug = 0;  /* 1 == print debug infos , 0 dont print debug infos */
#define GAP_DEBUG_DECLARED 1


#include "config.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <gtk/gtk.h>
#include <libgimp/gimp.h>
#include <libgimp/gimpui.h>

#include "gap_fg_matting_main.h"
#include "gap_fg_matting_exec.h"
#include "gap_fg_tile_manager.h"
#include "gap_fg_matting_dialog.h"
#include "gap_fg_from_sel_dialog.h"
#include "gap_fg_matting.h"
#include "gap_fg_regions.h"

#include "gap-intl.h"

static GapFgExtractValues fgVals =
{
   -1        /* input_drawable_id */
 , -1        /* tri_map_drawable_id */
 , FALSE     /* create_layermask   */
 , TRUE      /* lock_color */
 , TRUE      /* create_result */
 , 1.0       /* colordiff_threshold */
};


static GapFgSelectValues fsVals =
{
   -1        /* input_drawable_id */
 , -1        /* tri_map_drawable_id */
 , 7         /* innerRadius */
 , 7         /* outerRadius */
 , FALSE     /* create_layermask   */
 , TRUE      /* lock_color */
 , 1.0       /* colordiff_threshold */
};



static void  query (void);
static void  run (const gchar *name,          /* name of plugin */
     gint nparams,               /* number of in-paramters */
     const GimpParam * param,    /* in-parameters */
     gint *nreturn_vals,         /* number of out-parameters */
     GimpParam ** return_vals);  /* out-parameters */



/* Global Variables */
GimpPlugInInfo PLUG_IN_INFO =
{
  NULL,   /* init_proc  */
  NULL,   /* quit_proc  */
  query,  /* query_proc */
  run     /* run_proc   */
};

static const GimpParamDef in_args[] =
{
    { GIMP_PDB_INT32,    "run-mode",      "Interactive, non-interactive" },
    { GIMP_PDB_IMAGE,    "image",         "Input image"                  },
    { GIMP_PDB_DRAWABLE, "drawable",      "Input layer (RGB or RGBA)"               },
    { GIMP_PDB_DRAWABLE, "tri-mask",      "tri-mask drawable that defines "
                                             "konwn forground (>= 240), known background (0) and unkonwn (128) values"
                                             "This drawable must have same size as the Input-Layer "
                                             "specify -1 to use the layer-mask of the input drawable as tri-map."},
    { GIMP_PDB_INT32,    "create-layermask", "0 .. render transparency as alpha channel. "
                                             "1 .. render transparency as layer mask." },
    { GIMP_PDB_INT32,    "lock-color",       "0 .. remove background color in semi transparent pixels. "
                                             "1 .. keep original colors for all pixels (affect only transparency)." }
//    { GIMP_PDB_INT32,    "create-result",    "0 .. overwrite the input layer with the processing result. "
//                                             "1 .. return the result as new layer (and add the resulting layer above the input layer)" },
//    { GIMP_PDB_FLOAT,    "colordiff-threshold", "colordiff threshold in range #.# to #.#." }
};


static const GimpParamDef return_vals[] = {
    { GIMP_PDB_DRAWABLE, "resulting-layer",  "depending on result-mode this may be the input drawable or a newly created layer" }
};

static gint global_number_in_args = G_N_ELEMENTS (in_args);
static gint global_number_out_args = G_N_ELEMENTS (return_vals);


static const GimpParamDef in2_args[] =
{
    { GIMP_PDB_INT32,    "run-mode",      "Interactive, non-interactive" },
    { GIMP_PDB_IMAGE,    "image",         "Input image"                  },
    { GIMP_PDB_DRAWABLE, "drawable",      "Input layer (RGB or RGBA)"               },
    { GIMP_PDB_INT32,    "inner-radius",  "Radius for undefined (e.g. trimmable) area inside the selection border in pixels" },
    { GIMP_PDB_INT32,    "outer-radius",  "Radius for undefined (e.g. trimmable) area outside the selection border in pixels" },
    { GIMP_PDB_INT32,    "create-layermask", "0 .. render transparency as alpha channel. "
                                             "1 .. render transparency as layer mask." },
    { GIMP_PDB_INT32,    "lock-color",       "0 .. remove background color in semi transparent pixels. "
                                             "1 .. keep original colors for all pixels (affect only transparency)." }
//    { GIMP_PDB_FLOAT,    "colordiff-threshold", "colordiff threshold in range #.# to #.#." }
};

static gint global_number_in2_args = G_N_ELEMENTS (in2_args);



/* Functions */

MAIN ()

static void query (void)
{

  gimp_plugin_domain_register (GETTEXT_PACKAGE, LOCALEDIR);


  /* the actual installation of the plugin */
  gimp_install_procedure (PLUG_IN_NAME,
                          "Set layer opacity by foreground extraction based on alpha matting algorithm.",
                          "This plug-in renders the opacity for the specified Input drawable (a layer), "
                          "according to the supplied tri-mask, where the tri mask defines the opacity in the corresponding "
                          "pixel in the resulting layer. "
                          "White pixels (>= 240) in the tri-mask will set corresponding result pixels to fully opaque, "
                          "Black pixels (0) in the tri-mask will set corresponding result pixels to fully transparent, "
                          "Gray pixels in the tri-mask mark UNDEFINED areas and will trigger the calculation of transparency (and optionally color) "
                          "of the corresponding result pixel, depending on its color compared with near foreground and backround pixels. "
                          "the result is written to a newly created layer. "
                          "in case where the input layer has already an alpha channel, fully transparent pixels in the input layer"
                          "are not affected "
                          "(e.g. keep their full transparency, even if the tri-map marks this pixels as foreground) "
                          " ",
                          PLUG_IN_AUTHOR,
                          PLUG_IN_COPYRIGHT,
                          GAP_VERSION_WITH_DATE,
                          N_("Foreground Extract..."),
                          PLUG_IN_IMAGE_TYPES,
                          GIMP_PLUGIN,
                          global_number_in_args,
                          global_number_out_args,
                          in_args,
                          return_vals);

  gimp_install_procedure (PLUG_IN2_NAME,
                          "Duplicate selected foreground to a new layer based on trimmed selection via alpha matting.",
                          "This plug-in generates a tri map based on the current selection and "
                          "attaches the generated tri map as layermask to the Input layer, "
                          "A new resulting layer is created as copy of the selected foreground object of the input layer, "
                          "where the generated tri map is used to trim the selection via the alpha matting algorithm. "
                          "White pixels (>= 240) in the tri-mask will set corresponding result pixels to fully opaque, "
                          "Black pixels (0) in the tri-mask will set corresponding result pixels to fully transparent, "
                          "Gray pixels in the tri-mask mark UNDEFINED areas and will trigger the calculation of transparency (and optionally color) "
                          "of the corresponding result pixel, depending on its color compared with near foreground and backround pixels. "
                          "the result is written to a newly created layer. "
                          "in case where the input layer already has a layer mask it is replaced with the newly generated one. "
                          "are not affected "
                          "(e.g. keep their full transparency, even if the tri-map marks this pixels as foreground) "
                          " ",
                          PLUG_IN2_AUTHOR,
                          PLUG_IN2_COPYRIGHT,
                          GAP_VERSION_WITH_DATE,
                          N_("Foreground Extract Via Selection..."),
                          PLUG_IN2_IMAGE_TYPES,
                          GIMP_PLUGIN,
                          global_number_in2_args,
                          global_number_out_args,
                          in2_args,
                          return_vals);

  {
    /* Menu names */
    const char *menupath_image_layer_tranparency = N_("<Image>/Layer/Transparency/");

    gimp_plugin_menu_register (PLUG_IN_NAME, menupath_image_layer_tranparency);
    gimp_plugin_menu_register (PLUG_IN2_NAME, menupath_image_layer_tranparency);
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
  gint32       activeDrawableId = -1;
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

  if(gap_debug)
  {
    printf("\n\nDEBUG: run %s\n", name);
  }


  doProgress = FALSE;

  /* initialize the return of the status */
  values[0].type = GIMP_PDB_STATUS;
  values[0].data.d_status = status;
  values[1].type = GIMP_PDB_DRAWABLE;
  values[1].data.d_drawable = -1;
  *nreturn_vals = 2;
  *return_vals = values;

  gap_fg_matting_init_default_vals(&fgVals);
  gap_fg_from_sel_init_default_vals(&fsVals);

  /* get image and drawable */
  image_id = param[1].data.d_int32;
  activeDrawableId = param[2].data.d_drawable;

  /* Possibly retrieve data from a previous run */
  gimp_get_data (name, &fgVals);
  gimp_get_data (name, &fsVals);
  fgVals.input_drawable_id = activeDrawableId;
  fsVals.input_drawable_id = activeDrawableId;

  /* how are we running today? */
  switch (run_mode)
  {
    gboolean dialogOk;
    
    dialogOk = FALSE;
    
    case GIMP_RUN_INTERACTIVE:
      /* Get information from the dialog */
      if (strcmp(name, PLUG_IN2_NAME) == 0)
      {
        dialogOk = gap_fg_from_sel_dialog(&fsVals);
      }
      else
      {
        dialogOk = gap_fg_matting_dialog(&fgVals);
      }
      if (!dialogOk)
      {
        /* return without processing in case the Dialog was cancelled */
        return;
      }
      doProgress = TRUE;
      break;

    case GIMP_RUN_NONINTERACTIVE:
      /* check to see if invoked with the correct number of parameters */
      if (strcmp(name, PLUG_IN2_NAME) == 0)
      {
        if (nparams == global_number_in2_args)
        {
            fsVals.input_drawable_id = activeDrawableId;
            fsVals.innerRadius  = (gint32)  param[3].data.d_int32;
            fsVals.outerRadius  = (gint32)  param[4].data.d_int32;
            fsVals.create_layermask     = (param[5].data.d_int32 == 0) ? FALSE : TRUE;
            fsVals.lock_color           = (param[6].data.d_int32 == 0) ? FALSE : TRUE;
//            fsVals.colordiff_threshold  = (gdouble) param[7].data.d_float;
        }
        else
        {
          status = GIMP_PDB_CALLING_ERROR;
        }
      }
      else
      {
        if (nparams == global_number_in_args)
        {
            fgVals.input_drawable_id = activeDrawableId;
            fgVals.tri_map_drawable_id  = (gint32)  param[3].data.d_drawable;
            fgVals.create_layermask     = (param[4].data.d_int32 == 0) ? FALSE : TRUE;
            fgVals.lock_color           = (param[5].data.d_int32 == 0) ? FALSE : TRUE;
//            fgVals.create_result        = (param[6].data.d_int32 == 0) ? FALSE : TRUE;
//            fgVals.colordiff_threshold  = (gdouble) param[7].data.d_float;
        }
        else
        {
          status = GIMP_PDB_CALLING_ERROR;
        }
      }

      break;

    case GIMP_RUN_WITH_LAST_VALS:
      break;

    default:
      break;
  }

  if (status == GIMP_PDB_SUCCESS)
  {
    gboolean doFlush;

    doFlush =  (run_mode == GIMP_RUN_INTERACTIVE);
   
    /* Run the main function */
    if (strcmp(name, PLUG_IN2_NAME) == 0)
    {
      values[1].data.d_drawable =
          gap_fg_from_selection_exec_apply_run(image_id, activeDrawableId, doProgress, doFlush, &fsVals);

      /* Store variable states for next run */
      if (run_mode == GIMP_RUN_INTERACTIVE)
      {
        gimp_set_data (name, &fsVals, sizeof (GapFgSelectValues));
      }
    }
    else
    {
      values[1].data.d_drawable =
          gap_fg_matting_exec_apply_run(image_id, activeDrawableId, doProgress, doFlush, &fgVals);

      /* Store variable states for next run */
      if (run_mode == GIMP_RUN_INTERACTIVE)
      {
        gimp_set_data (name, &fgVals, sizeof (GapFgExtractValues));
      }
    }
    
    if (values[1].data.d_drawable < 0)
    {
       status = GIMP_PDB_CALLING_ERROR;
    }

    /* If run mode is interactive, flush displays, else (script) don't
     * do it, as the screen updates would make the scripts slow
     */
    if (run_mode == GIMP_RUN_INTERACTIVE)
    {
      gimp_displays_flush ();
    }


  }
  values[0].data.d_status = status;

}       /* end run */

