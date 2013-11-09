/* gap_mov_main.c
 * 2011.03.09 hof (Wolfgang Hofer)
 *
 * GAP ... Gimp Animation Package
 *
 * This Module contains:
 * - MAIN of the GAP Move Path Plugin (and its non interactive variants)
 * - query   registration of GAP Procedures (Video Menu) in the PDB
 * - run     invoke the selected GAP procedure by its PDB name
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
 * 2011/03/09  hof: created (moved already existing code from gap_main.c to this new module)
 */


#include "config.h"

/* SYTEM (UNIX) includes */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/* GIMP includes */
#include "gtk/gtk.h"
#include "libgimp/gimp.h"
#include "libgimp/gimpui.h"

/* GAP includes */
#include "gap_lib.h"
#include "gimplastvaldesc.h"
#include "gap_image.h"
#include "gap_base_ops.h"
#include "gap_lock.h"
#include "gap_mov_exec.h"

#include "gap-intl.h"


/* ------------------------
 * global gap DEBUG switch
 * ------------------------
 */

/* int gap_debug = 1; */    /* print debug infos */
/* int gap_debug = 0; */    /* 0: dont print debug infos */

int gap_debug = 0;

#define PLUGIN_NAME_GAP_MOVE                 "plug_in_gap_move"
#define PLUGIN_NAME_GAP_MOVE_PATH_EXT        "plug_in_gap_move_path_ext"
#define PLUGIN_NAME_GAP_MOVE_PATH_EXT2       "plug_in_gap_move_path_ext2"
#define PLUGIN_NAME_GAP_MOVE_SINGLEFRAME     "plug-in-move-path-singleframe"


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

  static GapMovSingleFrame singleframevals;

  static GimpParamDef return_std[] =
  {
    { GIMP_PDB_IMAGE, "curr_frame_image", "the resulting current frame image id" }
  };
  static int nreturn_std = G_N_ELEMENTS(return_std) ;

  static GimpParamDef return_single[] =
  {
    { GIMP_PDB_DRAWABLE, "resulting_layer", "the resulting processed layer id" }
  };
  static int nreturn_single = G_N_ELEMENTS(return_single) ;


  static GimpParamDef args_mov[] =
  {
    {GIMP_PDB_INT32, "run_mode", "Interactive"},
    {GIMP_PDB_IMAGE, "image", "Input image (one of the video frames)"},
    {GIMP_PDB_DRAWABLE, "drawable", "Input drawable (unused)"},
  };

  static GimpParamDef args_mov_path_ext[] =
  {
    {GIMP_PDB_INT32,        "run_mode",   "non-interactive"},
    {GIMP_PDB_IMAGE,        "dst_image",  "Destination image (one of the video frames), where to insert the animated source layers"},
    {GIMP_PDB_DRAWABLE,     "drawable",   "drawable (unused)"},
    {GIMP_PDB_INT32,        "range_from", "destination frame nr to start"},
    {GIMP_PDB_INT32,        "range_to",   "destination frame nr to stop (can be lower than range_from)"},
    {GIMP_PDB_INT32,        "nr",         "layerstack position where to insert source layer (0 == on top)"},
    /* source specs */
    { GIMP_PDB_LAYER,      "src_layer_id",      "starting LayerID of SourceObject. (use any Multilayeranimated Image, or a video frame of anoter Animation)"},
    { GIMP_PDB_INT32,      "src_stepmode",      "0-5     derive inserted object as copy of one layer from a multilayer src_image \n"
                                                "100-105 derive inserted object as copy of merged visible layers of a source video frame \n"
                                                "0:  Layer Loop  1: Layer Loop reverse  2: Layer Once  3: Layer Once reverse  4: Layer PingPong \n"
                                                "5: None (use onle the selected src_layer)\n"
                                                "100: Frame Loop  101: Frame Loop reverse  102: Frame Once  103: Frame Once reverse  104: Frame PingPong \n"
                                                "105: Frame None (use onle the flat copy of the selected frame)\n"
                                                },
    { GIMP_PDB_INT32,      "src_handle",        "0: handle left top   1: handle left bottom \n"
                                                "2: handle right top  3: handle right bottom \n"
                                                "4: handle center"},
    { GIMP_PDB_INT32,      "src_paintmode",     "4444: keep original paintmode of src_layer 0: GIMP_NORMAL_MODE (see GimpLayerModeEffects -- libgimp/gimpenums.h -- for more information)"},
    { GIMP_PDB_INT32,      "src_force_visible", "1: Set inserted layres visible, 0: insert layers as is"},
    { GIMP_PDB_INT32,      "clip_to_img",       "1: Clip inserted layers to Image size of the destination video frame, 0: dont clip"},
    /* extras */
    { GIMP_PDB_INT32,      "rotation_follow",   "0: NO automatic calculation (use the rotation array parameters as it is) \n"
                                                "1: Automatic calculation of rotation, following the path vectors, (Ignore rotation array parameters)\n"},
    { GIMP_PDB_FLOAT,      "startangle",        "start angle for the first contolpoint (only used if rotation-follow is on)"},

    /* new features of the _ext[ended] API */
    {GIMP_PDB_FLOAT,        "step_speed_factor",       "Allows stepping Source and Destination at different speed. (0.1 upto 50 where 1.0 does step snychron, 2.0 Src makes 2 Steps while Destination makes 1 step) "},
    {GIMP_PDB_INT32,        "tween_steps",             "0 upto 50, Number of virtual Frames to calculate between 2 destination Frames. (use value 0 if no tween processing should be done)"},
    {GIMP_PDB_FLOAT,        "tween_opacity_initial",   "opacity 0.0 upto 100.0 for the tween step that is nearest to the (next) real frame"},
    {GIMP_PDB_FLOAT,        "tween_opacity_desc",      "descending opacity 0.0 upto 100.0  for the othertween steps"},
    {GIMP_PDB_INT32,        "tracelayer_enable",       "TRUE: calculate a tracelayer (with all steps of the moving objects since first step)"},
    {GIMP_PDB_FLOAT,        "trace_opacity_initial",   "opacity 0.0 upto 100.0 for the nearest tracestep to the actual destination frame"},
    {GIMP_PDB_FLOAT,        "trace_opacity_desc",      "descending opacity 0.0 upto 100.0 for fading out older positions (that are done before the actual step)"},
    {GIMP_PDB_INT32,        "apply_bluebox",           "TRUE: apply blubox filter (using bluebox param VALUES of last successful bluebox run)"},
    {GIMP_PDB_INT32,        "src_selmode",      "0: ignore selections in all source images\n"
                                                "1: use one selection (from the inital source image) for all handled src layers \n"
                                                "2: use selections in all source images (for stepmodes 0-5 there is only one source image)"},


    /* CONTROLPOINT Arrays (the _ext API uses FLOAT arrays for more precision) */
    { GIMP_PDB_INT32,      "argc_p_x",          "number of controlpoints"},
    { GIMP_PDB_INT32ARRAY, "p_x",               "Controlpoint x-koordinate"},
    { GIMP_PDB_INT32,      "argc_p_y",          "number of controlpoints"},
    { GIMP_PDB_INT32ARRAY, "p_y",               "Controlpoint y-koordinate"},
    { GIMP_PDB_INT32,      "argc_opacity",      "number of controlpoints"},
    { GIMP_PDB_FLOATARRAY, "opacity",           "Controlpoint opacity value 0 <= value <= 100"},
    { GIMP_PDB_INT32,      "argc_w_resize",     "number of controlpoints"},
    { GIMP_PDB_FLOATARRAY, "w_resize",          "width scaling in percent"},
    { GIMP_PDB_INT32,      "argc_h_resize",     "number of controlpoints"},
    { GIMP_PDB_FLOATARRAY, "h_resize",          "height scaling in percent"},
    { GIMP_PDB_INT32,      "argc_rotation",     "number of controlpoints"},
    { GIMP_PDB_FLOATARRAY, "rotation",          "rotation in degrees"},
    { GIMP_PDB_INT32,      "argc_keyframe_abs", "number of controlpoints"},
    { GIMP_PDB_INT32ARRAY, "keyframe_abs",      "n: fix controlpoint to this frame number, 0: for controlpoints that are not fixed to a frame."},

    /* new CONTROLPOINT ARRAY items of the _ext[ended] API */
    { GIMP_PDB_INT32,      "argc_ttlx",     "number of controlpoints"},
    { GIMP_PDB_FLOATARRAY, "ttlx",          "perspective transformfactor for top left X Coordinate (0.0 upto 5.0, value 1.0 does no trasformation)"},
    { GIMP_PDB_INT32,      "argc_ttly",     "number of controlpoints"},
    { GIMP_PDB_FLOATARRAY, "ttly",          "perspective transformfactor for top left Y Coordinate (0.0 upto 5.0, value 1.0 does no trasformation)"},
    { GIMP_PDB_INT32,      "argc_ttrx",     "number of controlpoints"},
    { GIMP_PDB_FLOATARRAY, "ttrx",          "perspective transformfactor for top right X Coordinate (0.0 upto 5.0, value 1.0 does no trasformation)"},
    { GIMP_PDB_INT32,      "argc_ttry",     "number of controlpoints"},
    { GIMP_PDB_FLOATARRAY, "ttry",          "perspective transformfactor for top right Y Coordinate (0.0 upto 5.0, value 1.0 does no trasformation)"},
    { GIMP_PDB_INT32,      "argc_tblx",     "number of controlpoints"},
    { GIMP_PDB_FLOATARRAY, "tblx",          "perspective transformfactor for bottom left X Coordinate (0.0 upto 5.0, value 1.0 does no trasformation)"},
    { GIMP_PDB_INT32,      "argc_tbly",     "number of controlpoints"},
    { GIMP_PDB_FLOATARRAY, "tbly",          "perspective transformfactor for bottom left Y Coordinate (0.0 upto 5.0, value 1.0 does no trasformation)"},
    { GIMP_PDB_INT32,      "argc_tbrx",     "number of controlpoints"},
    { GIMP_PDB_FLOATARRAY, "tbrx",          "perspective transformfactor for bottom right X Coordinate (0.0 upto 5.0, value 1.0 does no trasformation)"},
    { GIMP_PDB_INT32,      "argc_tbry",     "number of controlpoints"},
    { GIMP_PDB_FLOATARRAY, "tbry",          "perspective transformfactor for bottom right Y Coordinate (0.0 upto 5.0, value 1.0 does no trasformation)"},
    { GIMP_PDB_INT32,      "argc_sel",      "number of controlpoints"},
    { GIMP_PDB_FLOATARRAY, "sel_feather_radius", "feather radius for selections"},
  };
  static int nargs_mov_path_ext = G_N_ELEMENTS (args_mov_path_ext);

  static GimpParamDef args_mov_path_ext2[] =
  {
    {GIMP_PDB_INT32,        "run_mode",   "non-interactive"},
    {GIMP_PDB_IMAGE,        "dst_image",  "Destination image (one of the video frames), where to insert the animated source layers"},
    {GIMP_PDB_DRAWABLE,     "drawable",   "drawable (unused)"},
    {GIMP_PDB_INT32,        "range_from", "destination frame nr to start"},
    {GIMP_PDB_INT32,        "range_to",   "destination frame nr to stop (can be lower than range_from)"},
    {GIMP_PDB_INT32,        "nr",         "layerstack position where to insert source layer (0 == on top)"},
    /* source specs */
    { GIMP_PDB_LAYER,      "src_layer_id",      "starting LayerID of SourceObject. (use any Multilayeranimated Image, or an video frame of anoter Animation)"},
    { GIMP_PDB_INT32,      "src_stepmode",      "0-5     derive inserted object as copy of one layer from a multilayer src_image \n"
                                                "100-105 derive inserted object as copy of merged visible layers of a source video frame \n"
                                                "0:  Layer Loop  1: Layer Loop reverse  2: Layer Once  3: Layer Once reverse  4: Layer PingPong \n"
                                                "5: None (use onle the selected src_layer)\n"
                                                "100: Frame Loop  101: Frame Loop reverse  102: Frame Once  103: Frame Once reverse  104: Frame PingPong \n"
                                                "105: Frame None (use onle the flat copy of the selected frame)\n"
                                                },
    { GIMP_PDB_INT32,      "src_handle",        "0: handle left top   1: handle left bottom \n"
                                                "2: handle right top  3: handle right bottom \n"
                                                "4: handle center"},
    { GIMP_PDB_INT32,      "src_paintmode",     "4444: keep original paintmode of src_layer 0: GIMP_NORMAL_MODE (see GimpLayerModeEffects -- libgimp/gimpenums.h -- for more information)"},
    { GIMP_PDB_INT32,      "src_force_visible", "1: Set inserted layres visible, 0: insert layers as is"},
    { GIMP_PDB_INT32,      "clip_to_img",       "1: Clip inserted layers to Image size of the destination video frame, 0: dont clip"},
    /* extras */
    { GIMP_PDB_INT32,      "rotation_follow",   "0: NO automatic calculation (use the rotation array parameters as it is) \n"
                                                "1: Automatic calculation of rotation, following the path vectors, (Ignore rotation array parameters)\n"},
    { GIMP_PDB_FLOAT,      "startangle",        "start angle for the first contolpoint (only used if rotation-follow is on)"},

    /* new features of the _ext[ended] API */
    {GIMP_PDB_FLOAT,        "step_speed_factor",       "Allows stepping Source and Destination at different speed. (0.1 upto 50 where 1.0 does step snychron, 2.0 Src makes 2 Steps while Destination makes 1 step) "},
    {GIMP_PDB_INT32,        "tween_steps",             "0 upto 50, Number of virtual Frames to calculate between 2 destination Frames. (use value 0 if no tween processing should be done)"},
    {GIMP_PDB_FLOAT,        "tween_opacity_initial",   "opacity 0.0 upto 100.0 for the tween step that is nearest to the (next) real frame"},
    {GIMP_PDB_FLOAT,        "tween_opacity_desc",      "descending opacity 0.0 upto 100.0  for the othertween steps"},
    {GIMP_PDB_INT32,        "tracelayer_enable",       "TRUE: calculate a tracelayer (with all steps of the moving objects since first step)"},
    {GIMP_PDB_FLOAT,        "trace_opacity_initial",   "opacity 0.0 upto 100.0 for the nearest tracestep to the actual destination frame"},
    {GIMP_PDB_FLOAT,        "trace_opacity_desc",      "descending opacity 0.0 upto 100.0 for fading out older positions (that are done before the actual step)"},
    {GIMP_PDB_INT32,        "apply_bluebox",           "TRUE: apply blubox filter (using bluebox param VALUES of last successful bluebox run)"},

    /* CONTROLPOINTs from file */
    { GIMP_PDB_STRING,     "pointfile",         "a file with contolpoints (readable text file with one line per controlpoint)"},
  };
  static int nargs_mov_path_ext2 = G_N_ELEMENTS (args_mov_path_ext2);


  static GimpParamDef args_mov_path_single_frame[] =
  {
    {GIMP_PDB_INT32,        "run_mode",      "non-interactive"},
    {GIMP_PDB_IMAGE,        "dst_image",     "Destination image (one of the video frames), where to insert the animated source layers"},
    {GIMP_PDB_DRAWABLE,     "drawable",      "drawable to be transfromed and moved according to current phase"},
    {GIMP_PDB_INT32,        "frame_phase",   "current frame nr starting at 1 (e.g. phase of movent and transformation along path)"},
    {GIMP_PDB_INT32,        "total_frames",  "number of frames for the full movement/transformation. (value 0 uses the recorded number of frames from the xml file)"},
    {GIMP_PDB_STRING,       "xml_paramfile", "a file with move path parameter settings in XML format "},
  };
  static int nargs_mov_path_single_frame = G_N_ELEMENTS (args_mov_path_single_frame);






MAIN ()

static void
query ()
{
  gchar *l_help_str;

  static GimpLastvalDef lastvals[] =
  {
    GIMP_LASTVALDEF_GINT32      (GIMP_ITER_FALSE,  singleframevals.drawable_id,             "drawable_id"),
    GIMP_LASTVALDEF_GINT32      (GIMP_ITER_TRUE,   singleframevals.frame_phase,             "frame_phase"),
    GIMP_LASTVALDEF_GINT32      (GIMP_ITER_FALSE,  singleframevals.total_frames,            "total_frames"),
    GIMP_LASTVALDEF_GCHAR       (GIMP_ITER_FALSE,  singleframevals.xml_paramfile[0],        "xml_paramfile"),
  };


  gimp_plugin_domain_register (GETTEXT_PACKAGE, LOCALEDIR);

  /* registration for last values buffer structure (for animated filter apply) */
  gimp_lastval_desc_register(PLUGIN_NAME_GAP_MOVE_SINGLEFRAME,
                             &singleframevals,
                             sizeof(singleframevals),
                             G_N_ELEMENTS (lastvals),
                             lastvals);


  gimp_install_procedure(PLUGIN_NAME_GAP_MOVE,
                         "This plugin copies layer(s) from one sourceimage to multiple frames on disk, varying position, size and opacity.",
                         "For NONINTERACTIVE PDB interfaces see also (plug_in_gap_move_path_ext, plug_in_gap_move_path_ext2)",
                         "Wolfgang Hofer (hof@gimp.org)",
                         "Wolfgang Hofer",
                         GAP_VERSION_WITH_DATE,
                         N_("Move Path..."),
                         "RGB*",
                         GIMP_PLUGIN,
                         G_N_ELEMENTS (args_mov), nreturn_std,
                         args_mov, return_std);

  l_help_str = g_strdup_printf(
                         "This plugin inserts one layer in each frame of the selected frame range of an Animation\n"
                         " (specified by the dst_image parameter).\n"
                         " An Animation is a series of numbered video frame images on disk where only the current\n"
                         " Frame is opened in the gimp\n"
                         " The inserted layer is derived from another (multilayer)image\n"
                         " or from another Animation (as merged copy of the visible layers in a source frame)\n"
                         " the affected destination frame range is selected by the range_from and range_to parameters\n"
                         " the src_stepmode parameter controls how to derive the layer that is to be inserted.\n"
                         " With the Controlpoint Parameters you can control position (coordinates),\n"
                         " size, rotation, perspective and opacity values of the inserted layer\n"
                         " If you want to move an Object from position AX/AY to BX/BY in a straight line within the range of 24 frames\n"
                         " you need 2 Contolpoints, if you want the object to move following a path\n"
                         " you need some more Controlpoints to do that.\n"
                         " With the rotation_follow Parameter you can force automatic calculation of the rotation for the inserted\n"
                         " layer according to the path vectors it is moving along.\n"
                         " A controlpoint can be fixed to a special framenumber using the keyframe_abs controlpoint-parameter.\n"
                         " Restrictions:\n"
                         " - keyframe_abs numbers must be 0 (== not fixed) or a frame_number within the affected frame range\n"
                         " - keyframes_abs must be in sequence (ascending or descending)\n"
                         " - the first and last controlpoint are always implicit keyframes, and should be passed with keyframe_abs = 0\n"
                         " - the number of controlpoints is limited to a maximum of %d.\n"
                         "   the number of controlpoints must be passed in all argc_* parameters\n"
                         "If the TraceLayer feature is turned on, an additional layer\n"
                         "  is inserted below the moving object. This Tracelayer shows all steps\n"
                         "  of the moving object since the 1st Frame.\n"
                         "With TweenSteps you can calculate virtual Frames between 2 destination frames\n"
                         "  all these Steps are collected in another additional Layer.\n"
                         "  this Tweenlayer is added below the moving Object in all handled destination Frames\n"
                         "See also (plug_in_gap_move_path, plug_in_gap_move)",
                         (int)GAP_MOV_MAX_POINT);

  gimp_install_procedure(PLUGIN_NAME_GAP_MOVE_PATH_EXT,
                         "This plugin copies layer(s) from one sourceimage or source animation to multiple frames on disk,\n"
                         "with varying position, size, perspective and opacity.\n"
                         ,
                         l_help_str,
                         "Wolfgang Hofer (hof@gimp.org)",
                         "Wolfgang Hofer",
                         GAP_VERSION_WITH_DATE,
                         NULL,                      /* do not appear in menus */
                         "RGB*",
                         GIMP_PLUGIN,
                         nargs_mov_path_ext, nreturn_std,
                         args_mov_path_ext, return_std);
  g_free(l_help_str);

  gimp_install_procedure(PLUGIN_NAME_GAP_MOVE_PATH_EXT2,
                         "This plugin copies layer(s) from one sourceimage or source animation to multiple frames on disk,\n"
                         "with varying position, size, perspective and opacity.\n"
                         ,
                         "This plugin is just another Interface for the MovePath (plug_in_gap_move_path_ext)\n"
                         " using a File to specify Controlpoints (rather than Array parameters).\n"
                         " Notes:\n"
                         " - you can create a controlpoint file with in the MovePath Dialog (interactive call of plug_in_gap_move)\n"
                         " - for more infos about controlpoints see help of (plug_in_gap_move_path)\n"
                         ,
                         "Wolfgang Hofer (hof@gimp.org)",
                         "Wolfgang Hofer",
                         GAP_VERSION_WITH_DATE,
                         NULL,                      /* do not appear in menus */
                         "RGB*",
                         GIMP_PLUGIN,
                         nargs_mov_path_ext2, nreturn_std,
                         args_mov_path_ext2, return_std);


  gimp_install_procedure(PLUGIN_NAME_GAP_MOVE_SINGLEFRAME,
                         "This plugin transforms and moves the specified layer according to the settings of the \n"
                         "specified move path xml parameterfile and to the specified frame_phase parameter.\n"
                         ,
                         "This plugin is intended to run as filter in the gimp-gap modify frames feature\n"
                         " to transform and move an already existng layer along a path where each frame \n"
                         " is processed in a separate call of this plug-in. The frame_phase parameter \n"
                         " shall start at 1 in the 1st call and shall count up to total_frames in the other calls.\n"
                         " Notes:\n"
                         " - you can create the xml parameterfile with in the MovePath Dialog (interactive call of plug_in_gap_move)\n"
                         ,
                         "Wolfgang Hofer (hof@gimp.org)",
                         "Wolfgang Hofer",
                         GAP_VERSION_WITH_DATE,
                         N_("Move Path Singleframe..."),
                         "RGB*",
                         GIMP_PLUGIN,
                         nargs_mov_path_single_frame, nreturn_single,
                         args_mov_path_single_frame, return_single);




  {
     /* Menu names */
     const char *menupath_image_video = N_("<Image>/Video/");

     //gimp_plugin_menu_branch_register("<Image>", "Video");

     gimp_plugin_menu_register (PLUGIN_NAME_GAP_MOVE, menupath_image_video);
     gimp_plugin_menu_register (PLUGIN_NAME_GAP_MOVE_SINGLEFRAME, menupath_image_video);
  }
}       /* end query */



static void
run (const gchar *name
    , gint n_params
    , const GimpParam *param
    , gint *nreturn_vals
    , GimpParam **return_vals)
{
  const char *l_env;

  static GimpParam values[20];
  GimpRunMode run_mode;
  GimpRunMode lock_run_mode;
  GimpPDBStatusType status;
  gint32     image_id;
  gint32     lock_image_id;


  gint32     l_rc_image;

  /* init std return values status and image (as used in most of the gap plug-ins) */
  *nreturn_vals = 2;
  *return_vals = values;
  status = GIMP_PDB_SUCCESS;
  values[0].type = GIMP_PDB_STATUS;
  values[0].data.d_status = GIMP_PDB_EXECUTION_ERROR;
  values[1].type = GIMP_PDB_IMAGE;
  values[1].data.d_int32 = -1;

  l_rc_image = -1;


  l_env = g_getenv("GAP_DEBUG");
  if(l_env != NULL)
  {
    if((*l_env != 'n') && (*l_env != 'N')) gap_debug = 1;
  }

  run_mode = param[0].data.d_int32;
  lock_run_mode = run_mode;

  if(gap_debug)
  {
    printf("\ngap_mov_main: debug name = %s\n", name);
  }

  /* gimp_ui_init is sometimes needed even in NON-Interactive
   * runmodes.
   * because thumbnail handling uses the procedure gdk_pixbuf_new_from_file
   * that will crash if not initialized
   * so better init always, just to be on the save side.
   * (most diaolgs do init a 2.nd time but this worked well)
   */
  gimp_ui_init ("gap_mov_main", FALSE);

  image_id = param[1].data.d_image;
  if(!gap_image_is_alive(image_id))
  {
     printf("GAP plug-in was called on INVALID IMAGE_ID:%d (terminating)\n",
                  (int)image_id);
     status = GIMP_PDB_EXECUTION_ERROR;
     values[0].data.d_status = status;
     return ;
  }
  lock_image_id = image_id;


  /* ---------------------------
   * check for LOCKS
   * ---------------------------
   */
  if(gap_lock_check_for_lock(lock_image_id, lock_run_mode))
  {
       status = GIMP_PDB_EXECUTION_ERROR;
       values[0].data.d_status = status;
       return ;
  }


  /* set LOCK on current image (for all gap_plugins) */
  gap_lock_set_lock(lock_image_id);

  INIT_I18N();

  if (strcmp (name, PLUGIN_NAME_GAP_MOVE) == 0)
  {
      GapMovValues *pvals;

      pvals = gap_mov_exec_new_GapMovValues();
      pvals->dst_image_id = image_id;
      if (run_mode == GIMP_RUN_NONINTERACTIVE)
      {
          status = GIMP_PDB_CALLING_ERROR;
      }

      if (status == GIMP_PDB_SUCCESS)
      {
        l_rc_image = gap_mov_exec_move_path(run_mode, image_id, pvals, NULL, 0, 0);
      }
      g_free(pvals);
  }
  else if (strcmp (name, PLUGIN_NAME_GAP_MOVE_SINGLEFRAME) == 0)
  {
      GapMovValues *pvals;
      gint          l_dataSize;

      values[1].type = GIMP_PDB_DRAWABLE;

      /* init pvals with default values (to provide defined settings
       * for optional data that may not be present in the xml parameter file)
       */
      pvals = gap_mov_exec_new_GapMovValues();
      pvals->dst_image_id = image_id;

      /* Possibly retrieve data from a previous run */
      l_dataSize = gimp_get_data_size(PLUGIN_NAME_GAP_MOVE_SINGLEFRAME);
      if(l_dataSize == sizeof(singleframevals))
      {
        gimp_get_data (PLUGIN_NAME_GAP_MOVE_SINGLEFRAME, &singleframevals);
      }

      singleframevals.drawable_id = param[2].data.d_drawable;
      singleframevals.keep_proportions = FALSE;
      singleframevals.fit_width = TRUE;
      singleframevals.fit_height = TRUE;

      if (run_mode == GIMP_RUN_NONINTERACTIVE)
      {
        if (n_params != nargs_mov_path_single_frame)
        {
          status = GIMP_PDB_CALLING_ERROR;
        }
        else
        {
           singleframevals.frame_phase   = param[3].data.d_int32;
           singleframevals.total_frames  = param[4].data.d_int32;
           if(param[5].data.d_string != NULL)
           {
              if(param[23].data.d_string != NULL)
              {
                 g_snprintf(&singleframevals.xml_paramfile[0], sizeof(singleframevals.xml_paramfile)
                          , "%s"
                          , param[5].data.d_string
                          );
              }
           }
           else
           {
             status = GIMP_PDB_CALLING_ERROR;
           }
        }

      }

      if (status == GIMP_PDB_SUCCESS)
      {
        gint32 l_rc_layer_id;
        
        l_rc_layer_id = gap_mov_exec_move_path_singleframe(run_mode, image_id, pvals, &singleframevals);
        values[1].data.d_int32 = l_rc_layer_id;  /* return layer id  of the resulting (processed) layer */
        if(l_rc_image >= 0)
        {
          gimp_set_data(PLUGIN_NAME_GAP_MOVE_SINGLEFRAME, &singleframevals, sizeof(singleframevals));
        }
      }
      g_free(pvals);

  }
  else if ((strcmp (name, PLUGIN_NAME_GAP_MOVE_PATH_EXT) == 0)
       ||  (strcmp (name, PLUGIN_NAME_GAP_MOVE_PATH_EXT2) == 0))
  {
      GapMovValues *pvals;
      gchar        *pointfile;
      gint          l_idx;
      gint          l_numpoints;
      gint          l_rotation_follow;
      gint32        l_startangle;

      pointfile = NULL;
      pvals = gap_mov_exec_new_GapMovValues();
      pvals->dst_image_id = image_id;
      l_rotation_follow = 0;
      l_startangle = 0;


      if (run_mode == GIMP_RUN_NONINTERACTIVE)
      {
        if ( ((n_params != nargs_mov_path_ext)  && (strcmp (name, PLUGIN_NAME_GAP_MOVE_PATH_EXT)  == 0))
        ||   ((n_params != nargs_mov_path_ext2) && (strcmp (name, PLUGIN_NAME_GAP_MOVE_PATH_EXT2) == 0)))
        {
          status = GIMP_PDB_CALLING_ERROR;
        }
        else
        {
           pvals->dst_range_start   = param[3].data.d_int32;
           pvals->dst_range_end     = param[4].data.d_int32;
           pvals->dst_layerstack    = param[5].data.d_int32;

           pvals->src_layer_id      = param[6].data.d_layer;
           pvals->src_stepmode      = param[7].data.d_int32;
           pvals->src_handle        = param[8].data.d_int32;
           pvals->src_paintmode     = param[9].data.d_int32;
           pvals->src_force_visible = param[10].data.d_int32;
           pvals->clip_to_img       = param[11].data.d_int32;

           l_rotation_follow        = param[12].data.d_int32;
           l_startangle             = param[13].data.d_float;

           pvals->step_speed_factor      = param[14].data.d_float;
           pvals->tween_steps            = param[15].data.d_int32;
           pvals->tween_opacity_initial  = param[16].data.d_float;
           pvals->tween_opacity_desc     = param[17].data.d_float;
           pvals->tracelayer_enable      = param[18].data.d_int32;
           pvals->trace_opacity_initial  = param[19].data.d_float;
           pvals->trace_opacity_desc     = param[20].data.d_float;
           pvals->src_apply_bluebox      = param[21].data.d_int32;
           pvals->src_selmode            = param[22].data.d_int32;

           if (strcmp (name, PLUGIN_NAME_GAP_MOVE_PATH_EXT)  == 0)
           {
              /* PLUGIN_NAME_GAP_MOVE_PATH_EXT passes controlpoints as array parameters */
              l_numpoints = param[23].data.d_int32;
              if ((l_numpoints != param[25].data.d_int32)
              ||  (l_numpoints != param[27].data.d_int32)
              ||  (l_numpoints != param[29].data.d_int32)
              ||  (l_numpoints != param[31].data.d_int32)
              ||  (l_numpoints != param[33].data.d_int32)
              ||  (l_numpoints != param[35].data.d_int32)
              ||  (l_numpoints != param[37].data.d_int32)
              ||  (l_numpoints != param[39].data.d_int32)
              ||  (l_numpoints != param[41].data.d_int32)
              ||  (l_numpoints != param[43].data.d_int32)
              ||  (l_numpoints != param[45].data.d_int32)
              ||  (l_numpoints != param[47].data.d_int32)
              ||  (l_numpoints != param[49].data.d_int32)
              ||  (l_numpoints != param[51].data.d_int32)
              ||  (l_numpoints != param[53].data.d_int32))
              {
                printf("plug_in_gap_move_path_ext: CallingError: different numbers in the controlpoint array argc parameters\n");
                status = GIMP_PDB_CALLING_ERROR;
              }
              else
              {
                pvals->point_idx_max = l_numpoints -1;
                for(l_idx = 0; l_idx < l_numpoints; l_idx++)
                {
                   pvals->point[l_idx].p_x = param[24].data.d_int32array[l_idx];
                   pvals->point[l_idx].p_y = param[26].data.d_int32array[l_idx];
                   pvals->point[l_idx].opacity  = param[28].data.d_floatarray[l_idx];
                   pvals->point[l_idx].w_resize = param[30].data.d_floatarray[l_idx];
                   pvals->point[l_idx].h_resize = param[32].data.d_floatarray[l_idx];
                   pvals->point[l_idx].rotation = param[34].data.d_floatarray[l_idx];
                   pvals->point[l_idx].keyframe_abs = param[36].data.d_int32array[l_idx];
                   /* pvals->point[l_idx].keyframe = ; */ /* relative keyframes are calculated later */
                   pvals->point[l_idx].ttlx = param[38].data.d_floatarray[l_idx];
                   pvals->point[l_idx].ttly = param[40].data.d_floatarray[l_idx];
                   pvals->point[l_idx].ttrx = param[42].data.d_floatarray[l_idx];
                   pvals->point[l_idx].ttry = param[44].data.d_floatarray[l_idx];
                   pvals->point[l_idx].tblx = param[46].data.d_floatarray[l_idx];
                   pvals->point[l_idx].tbly = param[48].data.d_floatarray[l_idx];
                   pvals->point[l_idx].tbrx = param[50].data.d_floatarray[l_idx];
                   pvals->point[l_idx].tbry = param[52].data.d_floatarray[l_idx];
                   pvals->point[l_idx].sel_feather_radius = param[54].data.d_floatarray[l_idx];
                }
              }
           }
           else
           {
              /* PLUGIN_NAME_GAP_MOVE_PATH_EXT2 operates with controlpoint file */
              if(param[23].data.d_string != NULL)
              {
                 pointfile = g_strdup(param[23].data.d_string);
              }
           }
        }

      }

      if (status == GIMP_PDB_SUCCESS)
      {
        l_rc_image = gap_mov_exec_move_path(run_mode, image_id, pvals, pointfile, l_rotation_follow, (gdouble)l_startangle);
      }
      g_free(pvals);
      if(pointfile != NULL)
      {
        g_free(pointfile);
      }
  }

  /* ---------- return handling --------- */

 if(l_rc_image < 0)
 {
    if(gap_debug)
    {
      printf("gap_mov_main: return GIMP_PDB_EXECUTION_ERROR\n");
    }
    status = GIMP_PDB_EXECUTION_ERROR;
 }
 else
 {
    if(gap_debug) printf("gap_mov_main: return OK\n");
    /* most gap_plug-ins return an image_id in values[1] */
    if (values[1].type == GIMP_PDB_IMAGE)
    {
      if(gap_debug)
      {
        printf("gap_mov_main: return image_id:%d\n", (int)l_rc_image);
      }
      values[1].data.d_int32 = l_rc_image;  /* return image id  of the resulting (current frame) image */
    }
 }

 if (run_mode != GIMP_RUN_NONINTERACTIVE)
 {
    gimp_displays_flush();
 }

 values[0].data.d_status = status;

 /* remove LOCK on this image for all gap_plugins */
 gap_lock_remove_lock(lock_image_id);
}
