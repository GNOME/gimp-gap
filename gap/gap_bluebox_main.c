/* gap_bluebox_main.c
 *  by hof (Wolfgang Hofer)
 *
 * GAP ... Gimp Animation Plugins
 *
 * This Module contains the GAP BlueBox Filter PDB Registration and MAIN
 * the Bluebox filter makes the specified color transparent.
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
 * gimp    1.3.23b; 2003/12/06  hof: added mode GAP_BLUBOX_THRES_ALL
 * gimp    1.3.20d; 2003/10/14  hof: creation
 */

static char *gap_bluebox_version = "1.3.23b; 2003/12/06";


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
#include "gap_bluebox.h"


/* ------------------------
 * gap DEBUG switch
 * ------------------------
 */

/* int gap_debug = 1; */    /* print debug infos */
/* int gap_debug = 0; */    /* 0: dont print debug infos */

int gap_debug = 0;

static GimpParamDef args_bluebox[] =
  {
    {GIMP_PDB_INT32, "run_mode", "Interactive"},
    {GIMP_PDB_IMAGE, "image", "the image"},
    {GIMP_PDB_DRAWABLE, "drawable", "the drawable"},

    {GIMP_PDB_COLOR, "keycolor",    "Select Pixels to be treansparent by this KeyColor" },
    {GIMP_PDB_INT32, "thres_mode",  "0 .. use the 3 threshold values for RGB\n"
                                    "1 .. use the 3 threshold values for HSV\n"
                                    "2 .. use only one simple Threshold\n"
                                    "3 .. use all 6 threshold values for HSV and RGB"},
    {GIMP_PDB_FLOAT, "thres_r", "threshold value 0.0 upto 1.0 for RED value   (ignored in thers_modes 1 and 2)"},
    {GIMP_PDB_FLOAT, "thres_g", "threshold value 0.0 upto 1.0 for GREEN value (ignored in thers_modes 1 and 2)"},
    {GIMP_PDB_FLOAT, "thres_b", "threshold value 0.0 upto 1.0 for BLUE value  (ignored in thers_modes 1 and 2)"},
    {GIMP_PDB_FLOAT, "thres_h", "threshold value 0.0 upto 1.0 for HUE value         (ignored in thers_modes 0 and 2)"},
    {GIMP_PDB_FLOAT, "thres_s", "threshold value 0.0 upto 1.0 for SATURATION value  (ignored in thers_modes 0 and 2)"},
    {GIMP_PDB_FLOAT, "thres_v", "threshold value 0.0 upto 1.0 for BRIGHTNESS  value (ignored in thers_modes 0 and 2)"},
    {GIMP_PDB_FLOAT, "thres", "threshold value 0.0 upto 1.0 (ignored in thers_modes 0, 1 and 3)"},
    {GIMP_PDB_FLOAT, "tolerance", "alpha tolerance value 0.0 upto 1.0\n"
                                  "0.0 makes hard pixel selection by color threshold(s)\n"
                                  "greater values allow more or less variable Alpha Values\n"
                                  "for the selected Pixels within the threshold(s)\n"
                                  "depending on their difference to the keycolor"},
    {GIMP_PDB_INT32, "grow",  "Grow the Selection in Pixels (negative values for shrinking the selection)"},
    {GIMP_PDB_INT32, "feather_edges", "TRUE: feather edges using feather_radius, FALSE: sharp edges (do not blur the selection)"},
    {GIMP_PDB_FLOAT, "feather_radius", "Feather Radius (makes the selection smooth)"},
    {GIMP_PDB_FLOAT, "source_alpha", "Select only pixelswith alpha >= source_alpha\n"
                                     " where (0.0 is full transparent, 1.0 is full opaque)"},
    {GIMP_PDB_FLOAT, "target_alpha", "Control the Alpha Value for the Selected Pixels\n"
                                     "(0.0 is full transparent, 1.0 is full opaque)"},
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
  static GapBlueboxVals bbox_vals;  /* this structure is only used as structure model
                                     * for common iterator procedure registration
                                     */
  static GimpLastvalDef lastvals[] =
  {
    GIMP_LASTVALDEF_GIMPRGB         (GIMP_ITER_TRUE,   bbox_vals.keycolor,  "keycolor"),
    GIMP_LASTVALDEF_ENUM            (GIMP_ITER_FALSE,  bbox_vals.thres_mode, "thres_mode"),
    GIMP_LASTVALDEF_GDOUBLE         (GIMP_ITER_TRUE,   bbox_vals.thres_r, "thres_r"),
    GIMP_LASTVALDEF_GDOUBLE         (GIMP_ITER_TRUE,   bbox_vals.thres_g, "thres_g"),
    GIMP_LASTVALDEF_GDOUBLE         (GIMP_ITER_TRUE,   bbox_vals.thres_b, "thres_b"),
    GIMP_LASTVALDEF_GDOUBLE         (GIMP_ITER_TRUE,   bbox_vals.thres_h, "thres_h"),
    GIMP_LASTVALDEF_GDOUBLE         (GIMP_ITER_TRUE,   bbox_vals.thres_s, "thres_s"),
    GIMP_LASTVALDEF_GDOUBLE         (GIMP_ITER_TRUE,   bbox_vals.thres_v, "thres_v"),
    GIMP_LASTVALDEF_GDOUBLE         (GIMP_ITER_TRUE,   bbox_vals.thres, "thres"),
    GIMP_LASTVALDEF_GDOUBLE         (GIMP_ITER_TRUE,   bbox_vals.tolerance, "tolerance"),
    GIMP_LASTVALDEF_GDOUBLE         (GIMP_ITER_TRUE,   bbox_vals.grow, "grow"),
    GIMP_LASTVALDEF_GINT            (GIMP_ITER_TRUE,   bbox_vals.feather_edges, "feather_edges"),
    GIMP_LASTVALDEF_GDOUBLE         (GIMP_ITER_TRUE,   bbox_vals.feather_radius, "feather_radius"),
    GIMP_LASTVALDEF_GDOUBLE         (GIMP_ITER_TRUE,   bbox_vals.source_alpha, "source_alpha"),
    GIMP_LASTVALDEF_GDOUBLE         (GIMP_ITER_TRUE,   bbox_vals.target_alpha, "target_alpha"),
  };


  static GimpParamDef *return_vals = NULL;
  static int nreturn_vals = 0;

  gimp_plugin_domain_register (GETTEXT_PACKAGE, LOCALEDIR);


  /* registration for last values buffer structure (useful for animated filter apply) */
  gimp_lastval_desc_register(GAP_BLUEBOX_PLUGIN_NAME,
                             &bbox_vals,
                             sizeof(bbox_vals),
                             G_N_ELEMENTS (lastvals),
                             lastvals);

  gimp_install_procedure(GAP_BLUEBOX_PLUGIN_NAME,
                         "The bluebox effectfilter makes the specified color transparent",
                         "This plug-in selects pixels in the specified drawable by keycolor "
                         "and makes the Selected Pixels transparent. "
                         "If there is a selection at calling time, then operate only "
                         "on Pixels that are already selected (where selection value is > 0) "
                         "The Slection by color follows threshold values "
                         "The thresholds operate on RGB or HSV colormodel, "
                         "depending on the thres_mode parameter. "
                         "The selection by keycolor can be smoothed (by feather_radius) "
                         "and/or extended by a grow value.",
                         "Wolfgang Hofer (hof@gimp.org)",
                         "Wolfgang Hofer",
                         gap_bluebox_version,
                         N_("Bluebox ..."),
                         "RGB*",
                         GIMP_PLUGIN,
                         G_N_ELEMENTS (args_bluebox), nreturn_vals,
                         args_bluebox, return_vals);

 //gimp_plugin_menu_branch_register("<Image>", "Video");
 gimp_plugin_menu_register (GAP_BLUEBOX_PLUGIN_NAME, N_("<Image>/Video/"));

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
  GapBlueboxGlobalParams  *bbp;

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

  bbp = gap_bluebox_bbp_new(param[2].data.d_drawable);

  run_mode = param[0].data.d_int32;
  bbp->run_mode = param[0].data.d_int32;
  bbp->image_id = param[1].data.d_image;

  gimp_image_undo_group_start (bbp->image_id);

  gap_bluebox_init_default_vals(bbp);

  if(gap_debug) printf("\n\ngap_bluebox main: debug name = %s\n", name);

  if (strcmp (name, GAP_BLUEBOX_PLUGIN_NAME) == 0)
  {
      switch (run_mode)
      {
        case GIMP_RUN_INTERACTIVE:
          {
            /*  Possibly retrieve data  */
            gimp_get_data (GAP_BLUEBOX_DATA_KEY_VALS, &bbp->vals);
            l_rc = gap_bluebox_dialog(bbp);
            if(l_rc < 0)
            {
              status = GIMP_PDB_CANCEL;
            }
          }
          break;
        case GIMP_RUN_NONINTERACTIVE:
          {
            if (n_params != G_N_ELEMENTS (args_bluebox))
              {
                status = GIMP_PDB_CALLING_ERROR;
              }
            else
              {
                bbp->vals.keycolor = param[3].data.d_color;
                bbp->vals.thres_mode = param[4].data.d_int32;
                bbp->vals.thres_r = param[5].data.d_float;
                bbp->vals.thres_g = param[6].data.d_float;
                bbp->vals.thres_b = param[7].data.d_float;
                bbp->vals.thres_h = param[8].data.d_float;
                bbp->vals.thres_s = param[9].data.d_float;
                bbp->vals.thres_v = param[10].data.d_float;
                bbp->vals.thres   = param[11].data.d_float;
                bbp->vals.tolerance = param[12].data.d_float;
                bbp->vals.grow         = (gdouble)param[13].data.d_int32;
                bbp->vals.feather_edges  = param[14].data.d_int32;
                bbp->vals.feather_radius = param[15].data.d_float;
                bbp->vals.source_alpha   = param[16].data.d_float;
                bbp->vals.target_alpha   = param[17].data.d_float;

                bbp->run_flag = TRUE;
              }
          }
          break;
        case GIMP_RUN_WITH_LAST_VALS:
          {
            /*  Possibly retrieve data  */
            gimp_get_data (GAP_BLUEBOX_DATA_KEY_VALS, &bbp->vals);
            bbp->run_flag = TRUE;
          }
          break;
        default:
          break;
      }
  }

  if(status == GIMP_PDB_SUCCESS)
  {
    bbp->layer_id = bbp->drawable_id;

    l_rc = gap_bluebox_apply(bbp);
    if(l_rc < 0)
    {
      status = GIMP_PDB_EXECUTION_ERROR;
    }
    else
    {
      gimp_set_data (GAP_BLUEBOX_DATA_KEY_VALS, &bbp->vals, sizeof (bbp->vals));
    }
  }
  gimp_image_undo_group_end (bbp->image_id);

  /* ---------- return handling --------- */


  values[0].type = GIMP_PDB_STATUS;
  values[0].data.d_status = status;
}  /* end run */
