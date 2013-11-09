/*  gap_detail_tracking_main.c
 *  This main module provides multiple filters:
 *
 *  A) Detail Tracking
 *
 *    This filter locates the position of a small area of one layer
 *    in another layer.
 *    It was implemented for recording postions as XML input
 *    for the MovePath tool by tracking a detail
 *    in a series of video frames.
 *    The recorded positions can also be used as XML input for the XML aligner
 *    plug-in (available in this module) and can be used as filter
 *    in the frames modify feature.
 *
 *
 *    Applying the recorded position can compensate unwanted camera moves
 *    when static scenes where shot without using a stativ.
 *  Note that the recording of positions is usually triggered by the
 *  Player's Snaphot feature where this filter runs on the 2 topmost layers
 *  (or on top and BG layer)
 *  in the snapshot image that is created and updated by the player.
 *
 *  B) Transformation of a Layer by 4 or 2 controlpoints.
 *    4 points: rotate scale and move the layer in a way that 2 reference points match 2 target points. 
 *       (Note that the 4 point mode works similar to the Exact Aligner script in the plug-in registry)
 *    2 points: simple move the layer from reference point to target point.
 *
 *    This filter transformation is available in 2 variants:
 *    B1) controlpoints input from current path
 *    B2) controlpoints from an xml input file recorded by GAP detail tracking feature.
 *
 *  2011/12/01
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
 *  (2011/12/01)  2.7.0       hof: created
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

#include "gimplastvaldesc.h"
#include "gap_detail_tracking_exec.h"
#include "gap_detail_align_exec.h"
#include "gap_arr_dialog.h"

#include "gap-intl.h"



#define PLUG_IN_NAME        GAP_DETAIL_TRACKING_PLUG_IN_NAME
#define PLUG_IN_NAME_CFG    "gap-detail-tracking-config"
#define PLUG_IN_BINARY      "gap_detail_tracking"
#define PLUG_IN_PRINT_NAME  "Detail Tracking"
#define PLUG_IN_IMAGE_TYPES "RGB*"
#define PLUG_IN_AUTHOR      "Wolfgang Hofer (hof@gimp.org)"
#define PLUG_IN_COPYRIGHT   "Wolfgang Hofer"
#define PLUG_IN_HELP_ID     "gap-plug-detail-tracking"


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


FilterValues     fiVals;
XmlAlignValues   xaVals;


static const GimpParamDef in_args[] =
{
    { GIMP_PDB_INT32,    "run-mode",      "Interactive, non-interactive" },
    { GIMP_PDB_IMAGE,    "image",         "Input image"                  },
    { GIMP_PDB_DRAWABLE, "drawable",      "ignored"               },
    { GIMP_PDB_INT32,    "refShapeRadius",      "radius in pixels to identify the reference shape." },
    { GIMP_PDB_INT32,    "targetMoveRadius",    "maximal expected movement radius. "
                                                "(the shape is searched in the target layer only within this radius." },
    { GIMP_PDB_FLOAT,    "loacteColodiffThreshold",     "0.0 upto 1.0 threshold that defines tolerated average colordiff for successful detail tracking."
                                                        " ." },
    { GIMP_PDB_INT32,    "coordsRelToFrame1",    "1 .. substract coords of initial position from all recorded positions."
                                                 "     (e.g. recording starts with px=0 py=0) "
                                                 "0 .. record absolute positions" },
    { GIMP_PDB_INT32,    "offsX",                "fix X offset (is added to all recorded positions)" },
    { GIMP_PDB_INT32,    "offsY",                "fix Y offset (is added to all recorded positions)" },
    { GIMP_PDB_FLOAT,    "offsRotate",           "fix rotation offset in degree (is added to all rotation values)" },
    { GIMP_PDB_INT32,    "enableScaling",        "1: calculate scaling and rotation when 2 points are tracked."
                                                 "0: calculate only rotation when 2 points are tracked." },
    { GIMP_PDB_INT32,    "bgLayerIsReference",   "1: BG layer is used as reference layer for detail tracking, "
                                                 "   (this is typically the 1st frame of the sequence)."
                                                 "0: The layer below the foreground layer is used as reference."
                                                 "   (this is typically the previous frame of the sequence)" },
    { GIMP_PDB_INT32,    "removeMidlayers",      "1: delete all layers except BG layer and 2 layer on top of the layerstack, "
                                                 "0: do not delete anything and keep all layers." },
    { GIMP_PDB_STRING,   "moveLogFile",          "optional name of a move path controlpoint xml file. (use - to write to stdout) " }
};

static const GimpParamDef in_xml_args[] =
{
    { GIMP_PDB_INT32,    "run-mode",      "Interactive, non-interactive" },
    { GIMP_PDB_IMAGE,    "image",         "Input image"                  },
    { GIMP_PDB_DRAWABLE, "drawable",      "layer to be aligned"               },
    { GIMP_PDB_INT32,    "framePhase",    "frame number e.g. phase to render (1 upto n recorded points in the xml file)." },
    { GIMP_PDB_STRING,   "moveLogFile",          "optional name of a move path controlpoint xml file. (use - to write to stdout) " }
};

static const GimpParamDef in_exalign_args[] =
{
    { GIMP_PDB_INT32,    "run-mode",      "Interactive, non-interactive" },
    { GIMP_PDB_IMAGE,    "image",         "Input image"                  },
    { GIMP_PDB_DRAWABLE, "drawable",      "layer to be aligned"          },
    { GIMP_PDB_INT32,    "pointOrder",    "0: use path coordinate points in order: 3 --> 1, 4 --> 2 "
                                          "1: use path coordinate points in order: 2 --> 1, 4 --> 3 " }
};




static const GimpParamDef return_vals[] = {
    { GIMP_PDB_DRAWABLE, "drawable",      "unused" }
};

static gint global_number_in_args = G_N_ELEMENTS (in_args);
static gint global_number_out_args = G_N_ELEMENTS (return_vals);
static gint global_number_in_xml_args = G_N_ELEMENTS (in_xml_args);
static gint global_number_in_exalign_args = G_N_ELEMENTS (in_exalign_args);




/* Functions */

MAIN ()

static void query (void)
{
  static GimpLastvalDef lastvals[] =
  {
    GIMP_LASTVALDEF_GINT32       (GIMP_ITER_FALSE,  fiVals.refShapeRadius,             "refShapeRadius"),
    GIMP_LASTVALDEF_GINT32       (GIMP_ITER_FALSE,  fiVals.targetMoveRadius,           "targetMoveRadius"),
    GIMP_LASTVALDEF_GDOUBLE      (GIMP_ITER_FALSE,  fiVals.loacteColodiffThreshold,    "loacteColodiffThreshold"),
    GIMP_LASTVALDEF_GBOOLEAN     (GIMP_ITER_FALSE,  fiVals.coordsRelToFrame1,          "coordsRelToFrame1"),
    GIMP_LASTVALDEF_GINT32       (GIMP_ITER_TRUE,   fiVals.offsX,                      "offsX"),
    GIMP_LASTVALDEF_GINT32       (GIMP_ITER_TRUE,   fiVals.offsY,                      "offsY"),
    GIMP_LASTVALDEF_GDOUBLE      (GIMP_ITER_TRUE,   fiVals.offsRotate,                 "offsRotate"),
    GIMP_LASTVALDEF_GBOOLEAN     (GIMP_ITER_FALSE,  fiVals.enableScaling,              "enableScaling"),
    GIMP_LASTVALDEF_GBOOLEAN     (GIMP_ITER_FALSE,  fiVals.bgLayerIsReference,         "bgLayerIsReference"),
    GIMP_LASTVALDEF_GBOOLEAN     (GIMP_ITER_FALSE,  fiVals.removeMidlayers,            "removeMidlayers"),
    GIMP_LASTVALDEF_ARRAY        (GIMP_ITER_FALSE,  fiVals.moveLogFile,                "moveLogFileArray"),
    GIMP_LASTVALDEF_GCHAR        (GIMP_ITER_FALSE,  fiVals.moveLogFile[0],             "moveLogFileChar"),

  };

  static GimpLastvalDef xaLastvals[] =
  {
    GIMP_LASTVALDEF_GINT32       (GIMP_ITER_TRUE,   xaVals.framePhase,                 "framePhase"),
    GIMP_LASTVALDEF_ARRAY        (GIMP_ITER_FALSE,  xaVals.moveLogFile,                "moveLogFileArray"),
    GIMP_LASTVALDEF_GCHAR        (GIMP_ITER_FALSE,  xaVals.moveLogFile[0],             "moveLogFileChar"),

  };

  /* registration for last values buffer structure (useful for animated filter apply) */
  gimp_lastval_desc_register(PLUG_IN_NAME,
                             &fiVals,
                             sizeof(fiVals),
                             G_N_ELEMENTS (lastvals),
                             lastvals);

  gimp_lastval_desc_register(GAP_DETAIL_TRACKING_XML_ALIGNER_PLUG_IN_NAME,
                             &xaVals,
                             sizeof(xaVals),
                             G_N_ELEMENTS (xaLastvals),
                             xaLastvals);

  gimp_plugin_domain_register (GETTEXT_PACKAGE, LOCALEDIR);


  /* the actual installation of the plugin with configuration dialog */
  gimp_install_procedure (PLUG_IN_NAME_CFG,
                          "Locate the position of a small area of one layer in another layer.",
                          "This filter operates on 2 layers on top of the layerstack, where "
                          "the topmost layer is the target and the layer below acts as reference layer.  "
                          "The position in the reference layer must be provided by the user as active path with one or 2 points. "
                          "For proper operation, both reference and target layer must have exact image size."
                          "The filter loactes the position of the corresponding detail within a specified radius in the target layer "
                          "and adjusts the marked positions on the corresponding detail in the target layer. "
                          "This new position is logged in XML format, suitable as input for the MovePath plug-in."
                          "Note that this filter is typically invoked from the Player on the snapshot image, "
                          "whenever the player puts the next frame on top of the snaphot image and detail tracking is enabled. "
                          "Detail tracking can record the unwanted camera movements in a static scene of a video shot freehand (without a stativ) "
                          "Applying the recorded movements with the MovePath feature can compensate such unwanted movements. "
                          " ",
                          PLUG_IN_AUTHOR,
                          PLUG_IN_COPYRIGHT,
                          GAP_VERSION_WITH_DATE,
                          N_("DetailTracking Config..."),
                          PLUG_IN_IMAGE_TYPES,
                          GIMP_PLUGIN,
                          global_number_in_args,
                          global_number_out_args,
                          in_args,
                          return_vals);

  gimp_install_procedure (PLUG_IN_NAME,
                          "Non-Interactive Locate the position of a small area of one layer in another layer.",
                          "This filter operates on 2 layers on top of the layerstack, where "
                          "the topmost layer is the target and the layer below acts as reference layer.  "
                          "The position in the reference layer must be provided by the user as active path with one or 2 points. "
                          "For proper operation, both reference and target layer must have exact image size."
                          "The filter loactes the position of the corresponding detail within a specified radius in the target layer "
                          "and adjusts the marked positions on the corresponding detail in the target layer. "
                          "This new position is logged in XML format, suitable as input for the MovePath plug-in."
                          "Note that this filter is typically invoked from the Player on the snapshot image, "
                          "whenever the player puts the next frame on top of the snaphot image and detail tracking is enabled. "
                          "Detail tracking can record the unwanted camera movements in a static scene of a video shot freehand (without a stativ) "
                          "Applying the recorded movements with the MovePath feature can compensate such unwanted movements. "
                          " ",
                          PLUG_IN_AUTHOR,
                          PLUG_IN_COPYRIGHT,
                          GAP_VERSION_WITH_DATE,
                          N_("DetailTracking"),
                          PLUG_IN_IMAGE_TYPES,
                          GIMP_PLUGIN,
                          global_number_in_args,
                          global_number_out_args,
                          in_args,
                          return_vals);

  /* the  installation of the xml based aligner plugin */
  gimp_install_procedure (GAP_DETAIL_TRACKING_XML_ALIGNER_PLUG_IN_NAME,
                          "Exact Align Layer via transformation according to current phase of detail tracking (recorded in XML file) .",
                          "This filter tranforms the specified layer. "
                          "It uses the relevant controlpoint (that matches the framePhase parameter) in the recorded XML file as input.  "
                          "and calculates offsts, scaling and rotation to transform the layer in a way that the points p1x p1y p2x p2y "
                          "will exactly match with the points p1x p1y p2x p2y of the 1st controlpoint in the XML file."
                          "(calling this filter with framePhase 1 does no transformation) "
                          "This filter is intended to run under control of the gimp-gap frames modify feature "
                          "to align multiple frames according to the controlpoints recorde in an XML file (via Detail tracking feature)."
                          " ",
                          PLUG_IN_AUTHOR,
                          PLUG_IN_COPYRIGHT,
                          GAP_VERSION_WITH_DATE,
                          N_("Align Transform via XML file..."),
                          PLUG_IN_IMAGE_TYPES,
                          GIMP_PLUGIN,
                          global_number_in_xml_args,
                          global_number_out_args,
                          in_xml_args,
                          return_vals);

  /* the  installation of the 4-point path based aligner plugin */
  gimp_install_procedure (GAP_EXACT_ALIGNER_PLUG_IN_NAME,
                          "Exact Align Layer via transformation according 4 points specified in the current path.",
                          "This filter expects a current path with 4 points as input where point 1 and 2 mark positions "
                          "within a reference layer and points 3 and 4 mark 2 corresponding point in the target layer. "
                          "The transformation is applied to the target layer and sets offsets, scaling and rotation "
                          "in a way that point3 is placed on position of point1, and point4 is placed on position of point2."
                          " "
                          " ",
                          PLUG_IN_AUTHOR,
                          PLUG_IN_COPYRIGHT,
                          GAP_VERSION_WITH_DATE,
                          N_("Exact Align via 4-Point Path."),
                          PLUG_IN_IMAGE_TYPES,
                          GIMP_PLUGIN,
                          global_number_in_exalign_args,
                          global_number_out_args,
                          in_exalign_args,
                          return_vals);


  {
    /* Menu names */
    const char *menupath_image_layer_enhance = N_("<Image>/Video/Layer/Enhance/");
    const char *menupath_image_layer_transform = N_("<Image>/Layer/Transform/");

    gimp_plugin_menu_register (PLUG_IN_NAME_CFG, menupath_image_layer_enhance);
    gimp_plugin_menu_register (PLUG_IN_NAME, menupath_image_layer_enhance);
    gimp_plugin_menu_register (GAP_DETAIL_TRACKING_XML_ALIGNER_PLUG_IN_NAME, menupath_image_layer_enhance);
    gimp_plugin_menu_register (GAP_EXACT_ALIGNER_PLUG_IN_NAME, menupath_image_layer_transform);
  }

}  /* end query */


static void
runExactAlign (const gchar *name,  /* name of plugin */
     gint nparams,               /* number of in-paramters */
     const GimpParam * param,    /* in-parameters */
     gint *nreturn_vals,         /* number of out-parameters */
     GimpParam ** return_vals)   /* out-parameters */
{
  gint32       image_id = -1;
  gint32       activeDrawableId = -1;
  gboolean     doFlush;

  /* Get the runmode from the in-parameters */
  GimpRunMode run_mode = param[0].data.d_int32;

  /* status variable, use it to check for errors in invocation usualy only
     during non-interactive calling */
  GimpPDBStatusType status = GIMP_PDB_SUCCESS;

  /* always return at least the status to the caller. */
  static GimpParam values[2];

  doFlush = TRUE;

  /* initialize the return of the status */
  values[0].type = GIMP_PDB_STATUS;
  values[0].data.d_status = status;
  values[1].type = GIMP_PDB_DRAWABLE;
  values[1].data.d_drawable = -1;
  *nreturn_vals = 2;
  *return_vals = values;

  /* get image and drawable */
  image_id = param[1].data.d_int32;
  activeDrawableId = param[2].data.d_drawable;



  if (status == GIMP_PDB_SUCCESS)
  {
    gint32 pointOrder;
    
    pointOrder = POINT_ORDER_MODE_31_42;
    if(nparams >=  global_number_in_exalign_args)
    {
      pointOrder = param[3].data.d_int32;
    }
    
 
    /* Run the main function */
    values[1].data.d_drawable =
          gap_detail_exact_align_via_4point_path(image_id, activeDrawableId
                                                , pointOrder,  run_mode);

 
    if (values[1].data.d_drawable < 0)
    {
       status = GIMP_PDB_CALLING_ERROR;
    }

    /* If run mode is interactive, flush displays, else (script) don't
     * do it, as the screen updates would make the scripts slow
     */
    if (doFlush)
    {
      gimp_displays_flush ();
    }


  }
  values[0].data.d_status = status;

}       /* end runExactAlign */



static void
runXmlAlign (const gchar *name,  /* name of plugin */
     gint nparams,               /* number of in-paramters */
     const GimpParam * param,    /* in-parameters */
     gint *nreturn_vals,         /* number of out-parameters */
     GimpParam ** return_vals)   /* out-parameters */
{
  gint32       image_id = -1;
  gint32       activeDrawableId = -1;
  gboolean doProgress;
  gboolean doFlush;
  GapLastvalAnimatedCallInfo  animCallInfo;

  /* Get the runmode from the in-parameters */
  GimpRunMode run_mode = param[0].data.d_int32;

  /* status variable, use it to check for errors in invocation usualy only
     during non-interactive calling */
  GimpPDBStatusType status = GIMP_PDB_SUCCESS;

  /* always return at least the status to the caller. */
  static GimpParam values[2];

  doProgress = FALSE;
  doFlush = FALSE;

  /* initialize the return of the status */
  values[0].type = GIMP_PDB_STATUS;
  values[0].data.d_status = status;
  values[1].type = GIMP_PDB_DRAWABLE;
  values[1].data.d_drawable = -1;
  *nreturn_vals = 2;
  *return_vals = values;

  /* init default values and Possibly retrieve data from a previous interactive run */
  gap_detail_xml_align_get_values(&xaVals);

  /* get image and drawable */
  image_id = param[1].data.d_int32;
  activeDrawableId = param[2].data.d_drawable;


  /* how are we running today? */
  switch (run_mode)
  {
    case GIMP_RUN_INTERACTIVE:
      {
        gboolean dialogOk;

        dialogOk = gap_detail_xml_align_dialog(&xaVals);
        if( dialogOk != TRUE)
        {
          status = GIMP_PDB_CALLING_ERROR;
        }

      }
      doProgress = TRUE;
      doFlush =  TRUE;
      break;

    case GIMP_RUN_NONINTERACTIVE:
      /* check to see if invoked with the correct number of parameters */
      if (nparams == global_number_in_args)
      {
          xaVals.framePhase               = param[3].data.d_int32;
          xaVals.moveLogFile[0] = '\0';
          if(param[4].data.d_string != NULL)
          {
            g_snprintf(xaVals.moveLogFile, sizeof(xaVals.moveLogFile) -1, "%s", param[4].data.d_string);
          }

      }
      else
      {
        status = GIMP_PDB_CALLING_ERROR;
      }

      break;

    case GIMP_RUN_WITH_LAST_VALS:
      animCallInfo.animatedCallInProgress = FALSE;
      gimp_get_data(GAP_LASTVAL_KEY_ANIMATED_CALL_INFO, &animCallInfo);

      if(animCallInfo.animatedCallInProgress != TRUE)
      {
        doProgress = TRUE;
        doFlush =  TRUE;
      }
      else
      {
        if(gap_debug)
        {
          // TODO never saw this in tests ...
          printf("animCallInfo.total_steps: %d current_step:%f\n"
            ,(int)animCallInfo.total_steps
            ,(float)animCallInfo.current_step
            );
        }
        if(xaVals.framePhase == 1)
        {
          /* apply with constant value framePhase 1 does not make sense
           * Therefore use current_step as framePhase
           */
          xaVals.framePhase = animCallInfo.current_step;
        }

      }
      break;

    default:
      break;
  }

  if (status == GIMP_PDB_SUCCESS)
  {

    gimp_image_undo_group_start (image_id);


    /* Run the main function */
    values[1].data.d_drawable =
          gap_detail_xml_align(activeDrawableId, &xaVals);

    gimp_image_undo_group_end (image_id);

    if (values[1].data.d_drawable < 0)
    {
       status = GIMP_PDB_CALLING_ERROR;
    }

    /* If run mode is interactive, flush displays, else (script) don't
     * do it, as the screen updates would make the scripts slow
     */
    if (doFlush)
    {
      gimp_displays_flush ();
    }


  }
  values[0].data.d_status = status;

}       /* end runXmlAlign */



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
  gboolean doFlush;
  GapLastvalAnimatedCallInfo  animCallInfo;


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



  if(strcmp(name, GAP_DETAIL_TRACKING_XML_ALIGNER_PLUG_IN_NAME) == 0)
  {
    runXmlAlign(name, nparams, param, nreturn_vals, return_vals);
    return;
  }

  if(strcmp(name, GAP_EXACT_ALIGNER_PLUG_IN_NAME) == 0)
  {
    runExactAlign(name, nparams, param, nreturn_vals, return_vals);
    return;
  }


  doProgress = FALSE;
  doFlush = FALSE;

  /* initialize the return of the status */
  values[0].type = GIMP_PDB_STATUS;
  values[0].data.d_status = status;
  values[1].type = GIMP_PDB_DRAWABLE;
  values[1].data.d_drawable = -1;
  *nreturn_vals = 2;
  *return_vals = values;

  /* init default values and Possibly retrieve data from a previous interactive run */
  gap_detail_tracking_get_values(&fiVals);

  /* get image and drawable */
  image_id = param[1].data.d_int32;
  activeDrawableId = param[2].data.d_drawable;


  /* how are we running today? */
  switch (run_mode)
  {
    case GIMP_RUN_INTERACTIVE:
      /* detail tracking primary feature is intended to work without dialog interaction
       * when ivoked by menu or keyboard shortcut using PLUG_IN_NAME.
       * This plug in also registers with a 2nd variant PLUG_IN_NAME_CFG
       * where the user can configure the options (for one gimp session)
       */
      if(strcmp(name, PLUG_IN_NAME_CFG) ==0)
      {
        gboolean dialogOk;

        if (fiVals.coordsRelToFrame1)
        {
          /* default offsets for handle at center */
          fiVals.offsX = gimp_image_width(image_id) / 2.0;
          fiVals.offsY = gimp_image_height(image_id) / 2.0;
        }


        dialogOk = gap_detail_tracking_dialog(&fiVals);
        if( dialogOk != TRUE)
        {
          status = GIMP_PDB_CALLING_ERROR;
        }

      }
      doProgress = TRUE;
      doFlush =  TRUE;
      break;

    case GIMP_RUN_NONINTERACTIVE:
      /* check to see if invoked with the correct number of parameters */
      if (nparams == global_number_in_args)
      {
          fiVals.refShapeRadius               = param[3].data.d_int32;
          fiVals.targetMoveRadius             = param[4].data.d_int32;
          fiVals.loacteColodiffThreshold      = param[5].data.d_float;
          fiVals.coordsRelToFrame1            = (param[6].data.d_int32 == 0) ? FALSE : TRUE;
          fiVals.offsX                        = param[7].data.d_int32;
          fiVals.offsY                        = param[8].data.d_int32;
          fiVals.offsRotate                   = param[9].data.d_float;
          fiVals.enableScaling                = (param[10].data.d_int32 == 0) ? FALSE : TRUE;
          fiVals.bgLayerIsReference           = (param[11].data.d_int32 == 0) ? FALSE : TRUE;
          fiVals.removeMidlayers              = (param[12].data.d_int32 == 0) ? FALSE : TRUE;

          fiVals.moveLogFile[0] = '\0';
          if(param[13].data.d_string != NULL)
          {
            g_snprintf(fiVals.moveLogFile, sizeof(fiVals.moveLogFile) -1, "%s", param[13].data.d_string);
          }

      }
      else
      {
        status = GIMP_PDB_CALLING_ERROR;
      }

      break;

    case GIMP_RUN_WITH_LAST_VALS:
      animCallInfo.animatedCallInProgress = FALSE;
      gimp_get_data(GAP_LASTVAL_KEY_ANIMATED_CALL_INFO, &animCallInfo);

      if(animCallInfo.animatedCallInProgress != TRUE)
      {
        doProgress = TRUE;
        doFlush =  TRUE;
      }
      break;

    default:
      break;
  }

  if (status == GIMP_PDB_SUCCESS)
  {
    gulong cache_ntiles;
    gulong regionTileWidth;
    gulong regionTileHeight;

    gimp_image_undo_group_start (image_id);

    /* this plug in repeatedly accesses the same tiles in the same pixel regionsarea
     * therefore tile caching is essential for performance reason
     * therefore calculate optimal tile cache size (but limit to 300 tiles that should be enogh
     * in most practical use cases)
     */
    regionTileWidth = 1 + (2 * (fiVals.targetMoveRadius + fiVals.refShapeRadius))/ gimp_tile_width() ;
    regionTileHeight = 1 + (2 * (fiVals.targetMoveRadius + fiVals.refShapeRadius))/ gimp_tile_height() ;

    /* processing may track 2 details in different regions of same size */
    cache_ntiles = (regionTileWidth * regionTileHeight) * 2;

    gimp_tile_cache_ntiles (MAX(300, cache_ntiles));

    /* Run the main function */
    values[1].data.d_drawable =
          gap_track_detail_on_top_layers(image_id, doProgress, &fiVals);

    gimp_image_undo_group_end (image_id);

    if (values[1].data.d_drawable < 0)
    {
       status = GIMP_PDB_CALLING_ERROR;
    }

    /* If run mode is interactive, flush displays, else (script) don't
     * do it, as the screen updates would make the scripts slow
     */
    if (doFlush)
    {
      gimp_displays_flush ();
    }


  }
  values[0].data.d_status = status;

}       /* end run */

