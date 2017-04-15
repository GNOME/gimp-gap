/* gap_edge_detection_main.c
 *  by hof (Wolfgang Hofer)
 *
 * GAP ... Gimp Animation Plugins
 *
 * This Module contains the GAP Edge Detection Filter PDB Registration and MAIN
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
 * gimp    2.8.xx; 201/03/10  hof: creation
 */

static char *gap_edge_version = "1.0; 2017/03/10";


/* SYTEM (UNIX) includes */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/* GIMP includes */
#include <gtk/gtk.h>
#include <libgimp/gimp.h>
#include <libgimp/gimpui.h>

/* GAP includes */
#include "config.h"
#include "gap-intl.h"
#include "gap_lastvaldesc.h"
#include "gap_edge_detection.h"
#include "gap_edge_detection_dialog.h"
#include "gap_edge_detection_dialog.h"


#define GAP_EDGE_DETECT_DATA_KEY_VALS "plug-in-edge-dosog"

/* ------------------------
 * gap DEBUG switch
 * ------------------------
 */

/* int gap_debug = 1; */    /* print debug infos */
/* int gap_debug = 0; */    /* 0: dont print debug infos */

int gap_debug = 0;

static GimpParamDef args_edge_detect[] =
  {
    {GIMP_PDB_INT32, "run_mode", "Interactive"},
    {GIMP_PDB_IMAGE, "image", "the image"},
    {GIMP_PDB_DRAWABLE, "drawable", "the drawable"},

    {GIMP_PDB_FLOAT, "blurRadius1X", "Horizontal Blur Radius (used in the 1st and 3rd internal copy)"},
    {GIMP_PDB_FLOAT, "blurRadius1Y", "Vertical Blur Radius (used in the 1st and 3rd internal copy)"},
    {GIMP_PDB_FLOAT, "blurRadius2X", "Horizontal Blur Radius (used in the 2nd and 4th internal copy)"},
    {GIMP_PDB_FLOAT, "blurRadius2Y", "Vertical Blur Radius (used in the 2st and 4th internal copy)"},
    {GIMP_PDB_INT32, "shiftLeft",    "Deplacement by small amount of pixels (used in the 1st internal copy)"},
    {GIMP_PDB_INT32, "shiftRight",   "Deplacement by small amount of pixels (used in the 2nd internal copy)"},
    {GIMP_PDB_INT32, "shiftUp",      "Deplacement by small amount of pixels (used in the 3rd internal copy)"},
    {GIMP_PDB_INT32, "shiftDown",    "Deplacement by small amount of pixels (used in the 4th internal copyl)"},
    {GIMP_PDB_INT32, "autoLevels",   "TRUE: xxx, FALSE: xxx"},
    {GIMP_PDB_INT32, "desaturate",   "TRUE: Desaturate the result to shades of gray, FALSE: dont desaturate"},
    {GIMP_PDB_INT32, "invert",   "TRUE: invert result (edge lines black on white area), FALSE: keep (edge lines white on black area)"},
    {GIMP_PDB_INT32, "createEdgeAsNewLayer",   "TRUE: delifer result as new layer (above the original), FALSE: replace the original layer content with the result"}
  };

/* -----------------------
 * procedure declarations
 * -----------------------
 */

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

/* ------------------------
 * MAIN, query & run
 * ------------------------
 */

MAIN ()

/* ---------------------------------
 * query
 * ---------------------------------
 */
static void
query ()
{
  static GapEdgeValues edvals;  /* this structure is only used as structure model
                                 * for common iterator procedure registration
                                 */
  static GimpLastvalDef lastvals[] =
  {
    GIMP_LASTVALDEF_GDOUBLE         (GIMP_ITER_TRUE,   edvals.blurRadius1X, "blurRadius1X"),
    GIMP_LASTVALDEF_GDOUBLE         (GIMP_ITER_TRUE,   edvals.blurRadius1Y, "blurRadius1Y"),
    GIMP_LASTVALDEF_GDOUBLE         (GIMP_ITER_TRUE,   edvals.blurRadius2X, "blurRadius2X"),
    GIMP_LASTVALDEF_GDOUBLE         (GIMP_ITER_TRUE,   edvals.blurRadius2Y, "blurRadius2Y"),
    GIMP_LASTVALDEF_GINT32          (GIMP_ITER_TRUE,   edvals.shiftLeft,    "shiftLeft"),
    GIMP_LASTVALDEF_GINT32          (GIMP_ITER_TRUE,   edvals.shiftRight,   "shiftRight"),
    GIMP_LASTVALDEF_GINT32          (GIMP_ITER_TRUE,   edvals.shiftUp,      "shiftUp"),
    GIMP_LASTVALDEF_GINT32          (GIMP_ITER_TRUE,   edvals.shiftDown,    "shiftDown"),
    GIMP_LASTVALDEF_GBOOLEAN        (GIMP_ITER_FALSE,  edvals.autoLevels,   "autoLevels"),
    GIMP_LASTVALDEF_GBOOLEAN        (GIMP_ITER_FALSE,  edvals.desaturate,   "desaturate"),
    GIMP_LASTVALDEF_GBOOLEAN        (GIMP_ITER_FALSE,  edvals.invert,       "invert"),
    GIMP_LASTVALDEF_GBOOLEAN        (GIMP_ITER_FALSE,  edvals.createEdgeAsNewLayer, "createEdgeAsNewLayer")
  };


  static GimpParamDef *return_vals = NULL;
  static int nreturn_vals = 0;

  gimp_plugin_domain_register (GETTEXT_PACKAGE, LOCALEDIR);


  /* registration for last values buffer structure (useful for animated filter apply) */
  gimp_lastval_desc_register(GAP_EDGE_PLUGIN_NAME,
                             &edvals,
                             sizeof(edvals),
                             G_N_ELEMENTS (lastvals),
                             lastvals);

  gimp_install_procedure(GAP_EDGE_PLUGIN_NAME,
                         "The edge detection filter detects and renders the edges of the specified drawable",
                         "This plug-in performs edge detection "
                         "by building difference of optionally shifted and or gaussian blured versions of the specified drawable. "
                         "the result replaces the original or is optional put into a newly created layer.",
                         "Wolfgang Hofer (hof@gimp.org)",
                         "Wolfgang Hofer",
                         gap_edge_version,
                         N_("Edge Detect (DoSoG) ..."),
                         "RGB*",
                         GIMP_PLUGIN,
                         G_N_ELEMENTS (args_edge_detect), nreturn_vals,
                         args_edge_detect, return_vals);

 gimp_plugin_menu_register (GAP_EDGE_PLUGIN_NAME, N_("<Image>/Video/Layer/Render"));

}       /* end query */


/* ---------------------------------
 * run
 * ---------------------------------
 */
static void
run(const gchar *name
   , gint n_params
   , const GimpParam *param
   , gint *nreturn_vals
   , GimpParam **return_vals)
{
  const gchar *l_env;

  static GimpParam values[2];
  GimpRunMode run_mode;
  GimpPDBStatusType status = GIMP_PDB_SUCCESS;
  GapEdgeValues  *edvalPtr;
  gint32 imageId;
  gint32 activeDrawableId;

  gint32     l_rc;

  *nreturn_vals = 1;
  *return_vals = values;
  l_rc = 0;

  INIT_I18N();

  l_env = g_getenv("GAP_DEBUG");
  if(l_env != NULL)
  {
    if((*l_env != 'n') && (*l_env != 'N')) gap_debug = 1;
  }


  run_mode = param[0].data.d_int32;
  imageId = param[1].data.d_image;
  activeDrawableId = param[2].data.d_drawable;
  edvalPtr = gap_edge_edval_new(activeDrawableId);


  if(gap_debug) printf("\n\ngap_edge_detection main: debug name = %s\n", name);

  if (strcmp (name, GAP_EDGE_PLUGIN_NAME) == 0)
  {
      switch (run_mode)
      {
        case GIMP_RUN_INTERACTIVE:
          {
            /*  Possibly retrieve data  */
            gimp_get_data (GAP_EDGE_DETECT_DATA_KEY_VALS, edvalPtr);
            l_rc = gap_edge_dialog(activeDrawableId, edvalPtr);
            if(l_rc < 0)
            {
              status = GIMP_PDB_CANCEL;
            }
          }
          break;
        case GIMP_RUN_NONINTERACTIVE:
          {
            if (n_params != G_N_ELEMENTS (args_edge_detect))
              {
                status = GIMP_PDB_CALLING_ERROR;
              }
            else
              {
                edvalPtr->blurRadius1X = param[3].data.d_float;
                edvalPtr->blurRadius1Y = param[4].data.d_float;
                edvalPtr->blurRadius2X = param[5].data.d_float;
                edvalPtr->blurRadius2Y = param[6].data.d_float;
                edvalPtr->shiftLeft = param[7].data.d_int32;
                edvalPtr->shiftRight = param[8].data.d_int32;
                edvalPtr->shiftUp = param[9].data.d_int32;
                edvalPtr->shiftDown = param[10].data.d_int32;
                edvalPtr->autoLevels = param[11].data.d_int32;
                edvalPtr->desaturate = param[12].data.d_int32;
                edvalPtr->invert = param[13].data.d_int32;
                edvalPtr->createEdgeAsNewLayer = param[14].data.d_int32;
              }
          }
          break;
        case GIMP_RUN_WITH_LAST_VALS:
          {
            /*  Possibly retrieve data  */
            gimp_get_data (GAP_EDGE_DETECT_DATA_KEY_VALS, edvalPtr);
          }
          break;
        default:
          break;
      }
  }

  if(status == GIMP_PDB_SUCCESS)
  {
    gint32 resultLayerId;
    
    gimp_image_undo_group_start (imageId);
    resultLayerId = gap_edgeDetectionByShiftBlurDiff(activeDrawableId, edvalPtr);
    gimp_image_undo_group_end (imageId);

    if(resultLayerId < 0)
    {
      status = GIMP_PDB_EXECUTION_ERROR;
    }
    else
    {
      gimp_set_data (GAP_EDGE_DETECT_DATA_KEY_VALS, edvalPtr, sizeof (struct GapEdgeValues));
    }
     /* If run mode is interactive, flush displays, else (script) don't
        do it, as the screen updates would make the scripts slow */
     if (run_mode != GIMP_RUN_NONINTERACTIVE)
     {
       gimp_displays_flush ();
     }
  }

  /* ---------- return handling --------- */


  values[0].type = GIMP_PDB_STATUS;
  values[0].data.d_status = status;
}  /* end run */
