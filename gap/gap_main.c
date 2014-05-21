/* gap_main.c
 * 1997.11.05 hof (Wolfgang Hofer)
 *
 * GAP ... Gimp Animation Package
 *
 * This Module contains:
 * - MAIN of the most GAP_Plugins
 * - query   registration of GAP Procedures (Video Menu) in the PDB
 * - run     invoke the selected GAP procedure by its PDB name
 *
 *
 * GAP provides Animation Functions for the gimp,
 * working on a series of Images stored on disk in gimps .xcf Format.
 *
 * Frames are Images with naming convention like this:
 * Imagename_<number>.<ext>
 * Example:   snoopy_000001.xcf, snoopy_000002.xcf, snoopy_000003.xcf
 *
 * if gzip is installed on your system you may optional
 * use gziped xcf frames with extensions ".xcfgz"
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
 *                  2011/03/09  hof: - moved code for Move Path features to gap_mov_main.c module
 * gimp    2.1.0a;  2004/04/05  hof: - Move Path added option to keep the original paintmode of the src_layer
 * gimp    1.3.24a; 2004/01/17  hof: - get main version from config.h, fixed PDB docs for plug_in_gap_modify
 * gimp    1.3.23b; 2003/12/06  hof: - updated main version
 * gimp    1.3.22a; 2003/10/09  hof: - updated main version
 * gimp    1.3.21a; 2003/10/09  hof: - updated main version
 * gimp    1.3.20d; 2003/10/09  hof: - bluebox filter for movepath
 * gimp    1.3.20d; 2003/10/09  hof: - sourcecode cleanup
 * gimp    1.3.20d; 2003/10/06  hof: - updates for move path extended non-interactive PDB API
 * gimp    1.3.20c; 2003/09/28  hof: - updates for move path
 * gimp    1.3.20b; 2003/09/20  hof: - updated main version
 * gimp    1.3.17b; 2003/07/31  hof: - updated main version, message text fixes for translators (# 118392)
 * gimp    1.3.17a; 2003/07/29  hof: - updated main version,
 *                                   - param types GimpPlugInInfo.run procedure
 * gimp    1.3.16c; 2003/07/09  hof: - updated main version,
 * gimp    1.3.16b; 2003/07/04  hof: - added frame_density plugin, updated main version,
 * gimp    1.3.15a; 2003/06/21  hof: - updated main version,
 * gimp    1.3.14a; 2003/05/27  hof: include gap_base_ops.h
 *                                   Split Image To frame: added parameter "digits" and "only_visible"
 * gimp    1.3.12a; 2003/05/03  hof: merge into CVS-gimp-gap project, updated main version, added gap_renumber
 * gimp    1.3.11a; 2003/01/19  hof: - updated main version,
 * gimp    1.3.5a;  2002/04/21  hof: - updated main version,
 *                                     according to the internal use of gimp_reconnect_displays
 *                                     most gap procedures do return the new image_id on frame changes.
 *                                     (with the old layer_stealing strategy the image_id did not change)
 * gimp    1.3.4a;  2002/02/24  hof: - updated main version
 * gimp    1.1.29b; 2000/11/30  hof: - GAP locks do check (only on UNIX) if locking Process is still alive
 *                                   - NONINTERACTIVE PDB Interface(s) for MovePath
 *                                      plug_in_gap_get_animinfo, plug_in_gap_set_framerate
 *                                   - updated main version, e-mail adress
 * gimp    1.1.20a; 2000/04/25  hof: NON_INTERACTIVE PDB-Interfaces video_edit_copy/paste/clear
 * gimp    1.1.14a; 2000/01/01  hof: bugfix params for gap_dup in noninteractive mode
 * gimp    1.1.13a; 1999/11/26  hof: splitted frontends for external programs (mpeg encoders)
 *                                   to gap_frontends_main.c
 * gimp    1.1.11a; 1999/11/15  hof: changed Menunames (AnimFrames to Video, Submenu Encode)
 * gimp    1.1.10a; 1999/10/22  hof: extended dither options for gap_range_convert
 * gimp    1.1.8a;  1999/08/31  hof: updated main version
 * version 0.99.00; 1999/03/17  hof: updated main version
 * version 0.98.02; 1999/01/27  hof: updated main version
 * version 0.98.01; 1998/12/21  hof: updated main version, e-mail adress
 * version 0.98.00; 1998/11/27  hof: updated main version, started port to GIMP 1.1 interfaces
 *                                   Use no '_' (underscore) in menunames. (has special function in 1.1)
 * version 0.96.03; 1998/08/31  hof: updated main version,
 *                                         gap_range_to_multilayer now returns image_id
 *                                         gap_split_image now returns image_id
 * version 0.96.02; 1998/08/05  hof: updated main version,
 *                                   added gap_shift
 * version 0.96.00; 1998/06/27  hof: added gap animation sizechange plugins
 *                                         gap_split_image
 *                                         gap_mpeg_encode
 * version 0.94.01; 1998/04/28  hof: updated main version,
 *                                   added flatten_mode to plugin: gap_range_to_multilayer
 * version 0.94.00; 1998/04/25  hof: updated main version
 * version 0.93.01; 1998/02/03  hof:
 * version 0.92.00;             hof: set gap_debug from environment
 * version 0.91.00; 1997/12/22  hof:
 * version 0.90.00;             hof: 1.st (pre) release
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
#include "gap_base_ops.h"
#include "gap_lock.h"
#include "gap_match.h"
#include "gap_range_ops.h"
#include "gap_split.h"
#include "gap_mod_layer.h"
#include "gap_arr_dialog.h"
#include "gap_pdb_calls.h"
#include "gap_vin.h"
#include "gap_image.h"

#include "gap-intl.h"


/* ------------------------
 * global gap DEBUG switch
 * ------------------------
 */

/* int gap_debug = 1; */    /* print debug infos */
/* int gap_debug = 0; */    /* 0: dont print debug infos */

int gap_debug = 0;

#define PLUGIN_NAME_GAP_NEXT                 "plug_in_gap_next"
#define PLUGIN_NAME_GAP_PREV                 "plug_in_gap_prev"
#define PLUGIN_NAME_GAP_FIRST                "plug_in_gap_first"
#define PLUGIN_NAME_GAP_LAST                 "plug_in_gap_last"
#define PLUGIN_NAME_GAP_GOTO                 "plug_in_gap_goto"
#define PLUGIN_NAME_GAP_DEL                  "plug_in_gap_del"
#define PLUGIN_NAME_GAP_DUP                  "plug_in_gap_dup"
#define PLUGIN_NAME_GAP_DENSITY              "plug_in_gap_density"
#define PLUGIN_NAME_GAP_EXCHG                "plug_in_gap_exchg"
#define PLUGIN_NAME_GAP_RANGE_TO_MULTILAYER  "plug_in_gap_range_to_multilayer"
#define PLUGIN_NAME_GAP_RANGE_FLATTEN        "plug_in_gap_range_flatten"
#define PLUGIN_NAME_GAP_RANGE_LAYER_DEL      "plug_in_gap_range_layer_del"
#define PLUGIN_NAME_GAP_RANGE_CONVERT        "plug_in_gap_range_convert"
#define PLUGIN_NAME_GAP_RANGE_CONVERT2       "plug_in_gap_range_convert2"
#define PLUGIN_NAME_GAP_ANIM_RESIZE          "plug_in_gap_anim_resize"
#define PLUGIN_NAME_GAP_ANIM_CROP            "plug_in_gap_anim_crop"
#define PLUGIN_NAME_GAP_ANIM_SCALE           "plug_in_gap_anim_scale"
#define PLUGIN_NAME_GAP_SPLIT                "plug_in_gap_split"
#define PLUGIN_NAME_GAP_SHIFT                "plug_in_gap_shift"
#define PLUGIN_NAME_GAP_REVERSE              "plug_in_gap_reverse"
#define PLUGIN_NAME_GAP_RENUMBER             "plug_in_gap_renumber"
#define PLUGIN_NAME_GAP_MODIFY               "plug_in_gap_modify"
#define PLUGIN_NAME_GAP_VIDEO_EDIT_COPY      "plug_in_gap_video_edit_copy"
#define PLUGIN_NAME_GAP_VIDEO_EDIT_PASTE     "plug_in_gap_video_edit_paste"
#define PLUGIN_NAME_GAP_VIDEO_EDIT_CLEAR     "plug_in_gap_video_edit_clear"
#define PLUGIN_NAME_GAP_GET_ANIMINFO         "plug_in_gap_get_animinfo"
#define PLUGIN_NAME_GAP_SET_FRAMERATE        "plug_in_gap_set_framerate"


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


  static GimpParamDef args_std[] =
  {
    {GIMP_PDB_INT32, "run_mode", "Interactive, non-interactive"},
    {GIMP_PDB_IMAGE, "image", "Input image (current one of the video frames)"},
    {GIMP_PDB_DRAWABLE, "drawable", "Input drawable (unused)"},
  };
  static int nargs_std = G_N_ELEMENTS (args_std);

  static GimpParamDef return_std[] =
  {
    { GIMP_PDB_IMAGE, "curr_frame_image", "the resulting current frame image id" }
  };
  static int nreturn_std = G_N_ELEMENTS(return_std) ;

  static GimpParamDef *return_nothing = NULL;
  static int nreturn_nothing = 0;




  static GimpParamDef args_goto[] =
  {
    {GIMP_PDB_INT32, "run_mode", "Interactive, non-interactive"},
    {GIMP_PDB_IMAGE, "image", "Input image (current one of the video frames)"},
    {GIMP_PDB_DRAWABLE, "drawable", "Input drawable (unused)"},
    {GIMP_PDB_INT32, "nr", "frame nr where to go"},
  };
  static int nargs_goto = G_N_ELEMENTS (args_goto);

  static GimpParamDef args_del[] =
  {
    {GIMP_PDB_INT32, "run_mode", "Interactive, non-interactive"},
    {GIMP_PDB_IMAGE, "image", "Input image (current one of the video frames)"},
    {GIMP_PDB_DRAWABLE, "drawable", "Input drawable (unused)"},
    {GIMP_PDB_INT32, "nr", "number of frames to delete (delete starts at current frame)"},
  };
  static int nargs_del = G_N_ELEMENTS (args_del);


  static GimpParamDef args_dup[] =
  {
    {GIMP_PDB_INT32, "run_mode", "Interactive, non-interactive"},
    {GIMP_PDB_IMAGE, "image", "Input image (current one of the video frames)"},
    {GIMP_PDB_DRAWABLE, "drawable", "Input drawable (unused)"},
    {GIMP_PDB_INT32, "nr", "how often to copy current frame"},
    {GIMP_PDB_INT32, "range_from", "frame nr to start"},
    {GIMP_PDB_INT32, "range_to", "frame nr to stop (can be lower than range_from)"},
  };
  static int nargs_dup = G_N_ELEMENTS (args_dup);

  static GimpParamDef args_density[] =
  {
    {GIMP_PDB_INT32, "run_mode", "Interactive, non-interactive"},
    {GIMP_PDB_IMAGE, "image", "Input image (current one of the video frames)"},
    {GIMP_PDB_DRAWABLE, "drawable", "Input drawable (unused)"},
    {GIMP_PDB_INT32, "range_from", "frame nr to start"},
    {GIMP_PDB_INT32, "range_to", "frame nr to stop"},
    {GIMP_PDB_FLOAT, "density_factor", "valid value is 1 upto 100 (where 1.0 has no effect)"},
    {GIMP_PDB_INT32, "density_grow", "TRUE: number of frames in the sel. range is multiplied by density factor (does duplicate frames), FALSE: nuber is divided by density factor (this does delete frames)"},
  };
  static int nargs_density = G_N_ELEMENTS (args_density);

  static GimpParamDef args_exchg[] =
  {
    {GIMP_PDB_INT32, "run_mode", "non-interactive"},
    {GIMP_PDB_IMAGE, "image", "Input image (current one of the video frames)"},
    {GIMP_PDB_DRAWABLE, "drawable", "Input drawable (unused)"},
    {GIMP_PDB_INT32, "nr", "nr of frame to exchange with current frame"},
  };
  static int nargs_exchg = G_N_ELEMENTS (args_exchg);

  static GimpParamDef args_f2multi[] =
  {
    {GIMP_PDB_INT32, "run_mode", "Interactive, non-interactive"},
    {GIMP_PDB_IMAGE, "image", "Input image (one of the video frames)"},
    {GIMP_PDB_DRAWABLE, "drawable", "Input drawable (unused)"},
    {GIMP_PDB_INT32, "range_from", "frame nr to start"},
    {GIMP_PDB_INT32, "range_to", "frame nr to stop (can be lower than range_from)"},
    {GIMP_PDB_INT32, "flatten_mode", "{ expand-as-necessary(0), CLIP-TO_IMG(1), CLIP-TO-BG-LAYER(2), FLATTEN(3) }"},
    {GIMP_PDB_INT32, "bg_visible", "{ BG_NOT_VISIBLE (0), BG_VISIBLE(1) }"},
    {GIMP_PDB_INT32, "framerate", "frame delaytime in ms"},
    {GIMP_PDB_STRING, "frame_basename", "basename for all generated layers"},
    {GIMP_PDB_INT32, "select_mode", "Mode how to identify a layer: 0-3 by layername 0=equal, 1=prefix, 2=suffix, 3=contains, 4=layerstack_numberslist, 5=inv_layerstack, 6=all_visible"},
    {GIMP_PDB_INT32, "select_case", "0: ignore case 1: select_string is case sensitive"},
    {GIMP_PDB_INT32, "select_invert", "0: select normal 1: invert (select all unselected layers)"},
    {GIMP_PDB_STRING, "select_string", "string to match with layername (how to match is defined by select_mode)"},
    {GIMP_PDB_INT32, "regionselect_mode", "How to picup layers: 0: use full layersize, ignore selection,\n"
                     " 1=pick only selected regions (use a fix selection of the invoking frame in all handled frames)\n"
                     " 2=pick only selected regions (use individual selction as it is in each handled frame)"},
  };
  static int nargs_f2multi = G_N_ELEMENTS (args_f2multi);

  static GimpParamDef return_f2multi[] =
  {
    { GIMP_PDB_IMAGE, "new_image", "Output image" }
  };
  static int nreturn_f2multi = G_N_ELEMENTS (return_f2multi);

  static GimpParamDef args_rflatt[] =
  {
    {GIMP_PDB_INT32, "run_mode", "Interactive, non-interactive"},
    {GIMP_PDB_IMAGE, "image", "Input image (one of the video frames)"},
    {GIMP_PDB_DRAWABLE, "drawable", "Input drawable (unused)"},
    {GIMP_PDB_INT32, "range_from", "frame nr to start"},
    {GIMP_PDB_INT32, "range_to", "frame nr to stop (can be lower than range_from)"},
  };
  static int nargs_rflatt = G_N_ELEMENTS (args_rflatt);

  static GimpParamDef args_rlayerdel[] =
  {
    {GIMP_PDB_INT32, "run_mode", "Interactive, non-interactive"},
    {GIMP_PDB_IMAGE, "image", "Input image (one of the video frames)"},
    {GIMP_PDB_DRAWABLE, "drawable", "Input drawable (unused)"},
    {GIMP_PDB_INT32, "range_from", "frame nr to start"},
    {GIMP_PDB_INT32, "range_to", "frame nr to stop (can be lower than range_from)"},
    {GIMP_PDB_INT32, "nr", "layerstack position (0 == on top)"},
  };
  static int nargs_rlayerdel = G_N_ELEMENTS (args_rlayerdel);


  static GimpParamDef args_rconv[] =
  {
    {GIMP_PDB_INT32, "run_mode", "Interactive, non-interactive"},
    {GIMP_PDB_IMAGE, "image", "Input image (one of the video frames)"},
    {GIMP_PDB_DRAWABLE, "drawable", "Input drawable (unused)"},
    {GIMP_PDB_INT32, "range_from", "frame nr to start"},
    {GIMP_PDB_INT32, "range_to", "frame nr to stop (can be lower than range_from)"},
    {GIMP_PDB_INT32, "flatten", "0 .. dont flatten image before save, "
                                "1 .. flatten before save,"
                                "2 .. merge visible layers (cliped to image) and remove invisible layers before save"},
    {GIMP_PDB_INT32, "dest_type", "0=RGB, 1=GRAY, 2=INDEXED"},
    {GIMP_PDB_INT32, "dest_colors", "1 upto 256 (used only for dest_type INDEXED)"},
    {GIMP_PDB_INT32, "dest_dither", "0=no, 1=floyd-steinberg  2=fs/low-bleed, 3=fixed (used only for dest_type INDEXED)"},
    {GIMP_PDB_STRING, "extension", "extension for the destination filetype (jpg, tif ...or any other gimp supported type)"},
    {GIMP_PDB_STRING, "basename", "(optional parameter) here you may specify the basename of the destination frames \"/my_dir/myframe\"  _0001.ext is added)"},
  };
  static int nargs_rconv = G_N_ELEMENTS (args_rconv);

  static GimpParamDef args_rconv2[] =
  {
    {GIMP_PDB_INT32, "run_mode", "Interactive, non-interactive"},
    {GIMP_PDB_IMAGE, "image", "Input image (one of the video frames)"},
    {GIMP_PDB_DRAWABLE, "drawable", "Input drawable (unused)"},
    {GIMP_PDB_INT32, "range_from", "frame nr to start"},
    {GIMP_PDB_INT32, "range_to", "frame nr to stop (can be lower than range_from)"},
    {GIMP_PDB_INT32, "flatten", "0 .. dont flatten image before save, "
                                "1 .. flatten before save,"
                                "2 .. merge visible layers (cliped to image) and remove invisible layers before save"},
    {GIMP_PDB_INT32, "dest_type", "0=RGB, 1=GRAY, 2=INDEXED"},
    {GIMP_PDB_INT32, "dest_colors", "1 upto 256 (used only for dest_type INDEXED)"},
    {GIMP_PDB_INT32, "dest_dither", "0=no, 1=floyd-steinberg 2=fs/low-bleed, 3=fixed(used only for dest_type INDEXED)"},
    {GIMP_PDB_STRING, "extension", "extension for the destination filetype (jpg, tif ...or any other gimp supported type)"},
    {GIMP_PDB_STRING, "basename", "(optional parameter) here you may specify the basename of the destination frames \"/my_dir/myframe\"  _0001.ext is added)"},
    {GIMP_PDB_INT32,  "palette_type", "0 == MAKE_PALETTE, 2 == WEB_PALETTE, 3 == MONO_PALETTE (bw) 4 == CUSTOM_PALETTE (used only for dest_type INDEXED)"},
    {GIMP_PDB_INT32,  "alpha_dither", "dither transparency to fake partial opacity (used only for dest_type INDEXED)"},
    {GIMP_PDB_INT32,  "remove_unused", "remove unused or double colors from final palette (used only for dest_type INDEXED)"},
    {GIMP_PDB_STRING, "palette", "name of the custom palette to use (used only for dest_type INDEXED and palette_type == CUSTOM_PALETTE) "},
  };
  static int nargs_rconv2 = G_N_ELEMENTS (args_rconv2);


  static GimpParamDef return_rconv[] =
  {
    { GIMP_PDB_IMAGE, "frame_image", "the image_id of the converted frame with lowest frame numer (of the converted range)" }
  };
  static int nreturn_rconv = G_N_ELEMENTS(return_rconv) ;



  /* resize and crop share the same parameters */
  static GimpParamDef args_resize[] =
  {
    {GIMP_PDB_INT32, "run_mode", "Interactive, non-interactive"},
    {GIMP_PDB_IMAGE, "image", "Input image (one of the video frames)"},
    {GIMP_PDB_DRAWABLE, "drawable", "Input drawable (unused)"},
    {GIMP_PDB_INT32, "new_width", "width of the resulting  video frame images in pixels"},
    {GIMP_PDB_INT32, "new_height", "height of the resulting  video frame images in pixels"},
    {GIMP_PDB_INT32, "offset_x", "X offset in pixels"},
    {GIMP_PDB_INT32, "offset_y", "Y offset in pixels"},
  };
  static int nargs_resize = G_N_ELEMENTS (args_resize);

  static GimpParamDef args_scale[] =
  {
    {GIMP_PDB_INT32, "run_mode", "Interactive, non-interactive"},
    {GIMP_PDB_IMAGE, "image", "Input image (one of the video frames)"},
    {GIMP_PDB_DRAWABLE, "drawable", "Input drawable (unused)"},
    {GIMP_PDB_INT32, "new_width", "width of the resulting  video frame images in pixels"},
    {GIMP_PDB_INT32, "new_height", "height of the resulting  video frame images in pixels"},
  };
  static int nargs_scale = G_N_ELEMENTS (args_scale);


  static GimpParamDef args_split[] =
  {
    {GIMP_PDB_INT32, "run_mode", "Interactive, non-interactive"},
    {GIMP_PDB_IMAGE, "image", "Input image (NO video frame allowed)"},
    {GIMP_PDB_DRAWABLE, "drawable", "Input drawable (unused)"},
    {GIMP_PDB_INT32, "inverse_order", "True/False"},
    {GIMP_PDB_INT32, "no_alpha", "True: remove alpha channel(s) in the destination frames"},
    {GIMP_PDB_STRING, "extension", "extension for the destination filetype (jpg, tif ...or any other gimp supported type)"},
    {GIMP_PDB_INT32, "only visible", "TRUE: Handle only visible layers, FALSE: Handle all layers and force visibility"},
    {GIMP_PDB_INT32, "digits", "Number of digits for the number part in the frames filenames"},
    {GIMP_PDB_INT32, "copy_properties", "True: copy image properties (channels, paths, guides ...)"
                                        " to all created frame images (algorithm based on gimp_image_duplicate)"
                                        "False: copy only layers (faster)"},
  };
  static int nargs_split = G_N_ELEMENTS (args_split);

  static GimpParamDef return_split[] =
  {
    { GIMP_PDB_IMAGE, "new_image", "Output image (first or last resulting frame)" }
  };
  static int nreturn_split = G_N_ELEMENTS (return_split);

  static GimpParamDef args_shift[] =
  {
    {GIMP_PDB_INT32, "run_mode", "Interactive, non-interactive"},
    {GIMP_PDB_IMAGE, "image", "Input image (current one of the video frames)"},
    {GIMP_PDB_DRAWABLE, "drawable", "Input drawable (unused)"},
    {GIMP_PDB_INT32, "nr", "how many framenumbers to shift the Frame Sequence"},
    {GIMP_PDB_INT32, "range_from", "frame nr to start"},
    {GIMP_PDB_INT32, "range_to", "frame nr to stop"},
  };
  static int nargs_shift = G_N_ELEMENTS (args_shift);

  static GimpParamDef args_reverse[] =
  {
    {GIMP_PDB_INT32, "run_mode", "Interactive, non-interactive"},
    {GIMP_PDB_IMAGE, "image", "Input image (current one of the video frames)"},
    {GIMP_PDB_DRAWABLE, "drawable", "Input drawable (unused)"},
    {GIMP_PDB_INT32, "range_from", "frame nr to start"},
    {GIMP_PDB_INT32, "range_to", "frame nr to stop"},
  };
  static int nargs_reverse = G_N_ELEMENTS (args_reverse);

  static GimpParamDef args_renumber[] =
  {
    {GIMP_PDB_INT32, "run_mode", "Interactive, non-interactive"},
    {GIMP_PDB_IMAGE, "image", "Input image (current one of the video frames)"},
    {GIMP_PDB_DRAWABLE, "drawable", "Input drawable (unused)"},
    {GIMP_PDB_INT32, "nr", "start frame number for the first frame (usual value is 1)"},
    {GIMP_PDB_INT32, "digits", "how many digits to use for the framenumber part (1 upto 8)"},
  };
  static int nargs_renumber = G_N_ELEMENTS (args_renumber);

  static GimpParamDef args_modify[] =
  {
    {GIMP_PDB_INT32, "run_mode", "Interactive, non-interactive"},
    {GIMP_PDB_IMAGE, "image", "Input image (current one of the video frames)"},
    {GIMP_PDB_DRAWABLE, "drawable", "Input drawable (unused)"},
    {GIMP_PDB_INT32, "range_from", "frame nr to start"},
    {GIMP_PDB_INT32, "range_to", "frame nr to stop"},
    {GIMP_PDB_INT32, "action_mode", "0:set_visible"
                                    ", 1:set_invisible"
                                    ", 2:set_linked"
                                    ", 3:set_unlinked"
                                    ", 4:raise"
                                    ", 5:lower"
                                    ", 6:merge_expand"
                                    ", 7:merge_img"
                                    ", 8:merge_bg"
                                    ", 9:apply_filter"
                                    ", 10:duplicate"
                                    ", 11:delete"
                                    ", 12:rename"
                                    ", 13:replace selection"
                                    ", 14:add selection"
                                    ", 15:sbtract selection"
                                    ", 16:intersect selection"
                                    ", 17:selection none"
                                    ", 18:selection all"
                                    ", 19:selection invert"
                                    ", 20:save selection to channel"
                                    ", 21:load selection from channel"
                                    ", 22:delete channel (by name)"
                                    ", 23:add alpha channel"
                                    ", 24:add white layermask (opaque)"
                                    ", 25:add black layermask (transparent)"
                                    ", 26:add alpha layermask"
                                    ", 27:add alpha transfer layermask"
                                    ", 28:add selection as layermask"
                                    ", 29:add bw copy as layermask"
                                    ", 30:delete layermask"
                                    ", 31:apply layermask"
                                    ", 32:copy layermask from layer above"
                                    ", 33:copy layermask from layer below"
                                    ", 34:invert layermask"
                                    ", 35:set layer mode to normal"
                                    ", 36:set layer mode to dissolve"
                                    ", 37:set layer mode to multiply"
                                    ", 38:set layer mode to divide"
                                    ", 39:set layer mode to screen"
                                    ", 40:set layer mode to overlay"
                                    ", 41:set layer mode to difference"
                                    ", 42:set layer mode to addition"
                                    ", 43:set layer mode to subtract"
                                    ", 44:set layer mode to darken_only"
                                    ", 45:set layer mode to lighten_only"
                                    ", 46:set layer mode to dodge"
                                    ", 47:set layer mode to burn"
                                    ", 48:set layer mode to hardlight"
                                    ", 49:set layer mode to softlight"
                                    ", 50:set layer mode to color_erase"
                                    ", 51:set layer mode to grain_extract_mode"
                                    ", 52:set layer mode to grain_merge_mode"
                                    ", 53:set layer mode to hue_mode"
                                    ", 54:set layer mode to saturation_mode"
                                    ", 55:set layer mode to color_mode"
                                    ", 56:set layer mode to value_mode"
                                    ", 57:apply filter on layermask"
                                    ", 58:set selection from alphachannel"
                                    ", 59:resize layer to mage size"
                                    ", 60:create layer from opacity"
                                    ", 61:create layer from layermask"
                                    ", 62:create layer from alphachannel"
                                    ", 63:reorder layer (move to new group and/or position)"
                                    ", 64:create empty layergroup"
                                    ", 65:raise to top"
                                    ", 66:lower to bg"
                                    ", 67:merge down expand"
                                    ", 68:merge down clip to image"
                                    ", 69:merge down clip to bg"
                                    },
    {GIMP_PDB_INT32, "select_mode", "Mode how to identify a layer: 0-3 by layername 0=equal, 1=prefix, 2=suffix, 3=contains, 4=layerstack_numberslist, 5=inv_layerstack, 6=all_visible"},
    {GIMP_PDB_INT32, "select_case", "0: ignore case 1: select_string is case sensitive"},
    {GIMP_PDB_INT32, "select_invert", "0: select normal 1: invert (select all unselected layers)"},
    {GIMP_PDB_STRING, "select_string", "string to match with layername (how to match is defined by select_mode)"},
    {GIMP_PDB_STRING, "new_layername", "is only used at action rename. [####] is replaced by the framnumber"},

    {GIMP_PDB_INT32,  "new_position", "new stackposition within the image or in the new group. is only used at action reorder."},
    {GIMP_PDB_STRING, "new_groupname", "is only used at action reorder. the selected layer(s) is(are) moved to this new_groupname (group will be created automatically if it does not already exist)"},
    {GIMP_PDB_STRING, "select_groupname", "defines the selection scope, that is the name of the group where its layers can be selected by name or position(s). An empty string defines the images toplevel layers as selection scope"},
    {GIMP_PDB_STRING, "delimiter", "Delimiter characterstring used in group names to identify subgroup names."},
  };
  static int nargs_modify = G_N_ELEMENTS (args_modify);

  static GimpParamDef args_video_copy[] =
  {
    {GIMP_PDB_INT32, "run_mode", "always non-interactive"},
    {GIMP_PDB_IMAGE, "image", "Input image (current one of the video frames)"},
    {GIMP_PDB_DRAWABLE, "drawable", "Input drawable (unused)"},
    {GIMP_PDB_INT32, "range_from", "frame nr to start"},
    {GIMP_PDB_INT32, "range_to", "frame nr to stop"},
  };
  static int nargs_video_copy = G_N_ELEMENTS (args_video_copy);

  static GimpParamDef args_video_paste[] =
  {
    {GIMP_PDB_INT32, "run_mode", "always non-interactive"},
    {GIMP_PDB_IMAGE, "image", "Input image (current one of the video frames)"},
    {GIMP_PDB_DRAWABLE, "drawable", "Input drawable (unused)"},
    {GIMP_PDB_INT32, "paste_mode", "0 .. paste at current frame (replacing current and following frames)"
                                "1 .. paste insert before current frame "
                                "2 .. paste insert after current frame"},
  };
  static int nargs_video_paste = G_N_ELEMENTS (args_video_paste);

  static GimpParamDef args_video_clear[] =
  {
    {GIMP_PDB_INT32, "run_mode", "always non-interactive"},
    {GIMP_PDB_IMAGE, "image", "Input image (is ignored)"},
    {GIMP_PDB_DRAWABLE, "drawable", "Input drawable (unused)"},
  };

  static GimpParamDef return_ainfo[] =
  {
    { GIMP_PDB_INT32,  "first_frame_nr", "lowest frame number of all video frame discfiles" },
    { GIMP_PDB_INT32,  "last_frame_nr",  "highest frame number of all video frame discfiles" },
    { GIMP_PDB_INT32,  "curr_frame_nr",  "current frame number (extracted from the imagename)" },
    { GIMP_PDB_INT32,  "frame_cnt",      "total number of all video frame discfiles that belong to the passed image" },
    { GIMP_PDB_STRING, "basename",       "basename of the video frame (without frame number and extension)\n"
                                         " may also include path (depending how the current frame image was opened)" },
    { GIMP_PDB_STRING, "extension",      "extension of the video frames (.xcf)" },
    { GIMP_PDB_FLOAT,  "framerate",      "framerate in frames per second" }
  };
  static int nreturn_ainfo = G_N_ELEMENTS (return_ainfo);

  static GimpParamDef args_setrate[] =
  {
    {GIMP_PDB_INT32, "run_mode", "non-interactive"},
    {GIMP_PDB_IMAGE, "image", "Input image (current one of the video frames)"},
    {GIMP_PDB_DRAWABLE, "drawable", "Input drawable (unused)"},
    {GIMP_PDB_FLOAT,  "framerate",      "framerate in frames per second" }
  };
  static int nargs_setrate = G_N_ELEMENTS (args_setrate);



MAIN ()

static void
query ()
{
  gimp_plugin_domain_register (GETTEXT_PACKAGE, LOCALEDIR);

  gimp_install_procedure(PLUGIN_NAME_GAP_NEXT,
                         "This plugin exchanges current image with (next numbered) image from disk.",
                         "",
                         "Wolfgang Hofer (hof@gimp.org)",
                         "Wolfgang Hofer",
                         GAP_VERSION_WITH_DATE,
                         N_("Next Frame"),
                         "RGB*, INDEXED*, GRAY*",
                         GIMP_PLUGIN,
                         nargs_std, nreturn_std,
                         args_std, return_std);

  gimp_install_procedure(PLUGIN_NAME_GAP_PREV,
                         "This plugin exchanges current image with (previous numbered) image from disk.",
                         "",
                         "Wolfgang Hofer (hof@gimp.org)",
                         "Wolfgang Hofer",
                         GAP_VERSION_WITH_DATE,
                         N_("Previous Frame"),
                         "RGB*, INDEXED*, GRAY*",
                         GIMP_PLUGIN,
                         nargs_std, nreturn_std,
                         args_std, return_std);

  gimp_install_procedure(PLUGIN_NAME_GAP_FIRST,
                         "This plugin exchanges current image with (lowest numbered) image from disk.",
                         "",
                         "Wolfgang Hofer (hof@gimp.org)",
                         "Wolfgang Hofer",
                         GAP_VERSION_WITH_DATE,
                         N_("First Frame"),
                         "RGB*, INDEXED*, GRAY*",
                         GIMP_PLUGIN,
                         nargs_std, nreturn_std,
                         args_std, return_std);

  gimp_install_procedure(PLUGIN_NAME_GAP_LAST,
                         "This plugin exchanges current image with (highest numbered) image from disk.",
                         "",
                         "Wolfgang Hofer (hof@gimp.org)",
                         "Wolfgang Hofer",
                         GAP_VERSION_WITH_DATE,
                         N_("Last Frame"),
                         "RGB*, INDEXED*, GRAY*",
                         GIMP_PLUGIN,
                         nargs_std, nreturn_std,
                         args_std, return_std);

  gimp_install_procedure(PLUGIN_NAME_GAP_GOTO,
                         "This plugin exchanges current image with requested image (nr) from disk.",
                         "",
                         "Wolfgang Hofer (hof@gimp.org)",
                         "Wolfgang Hofer",
                         GAP_VERSION_WITH_DATE,
                         N_("Any Frame..."),
                         "RGB*, INDEXED*, GRAY*",
                         GIMP_PLUGIN,
                         nargs_goto, nreturn_std,
                         args_goto, return_std);

  gimp_install_procedure(PLUGIN_NAME_GAP_DEL,
                         "This plugin deletes the given number of frames from disk including the current frame.",
                         "",
                         "Wolfgang Hofer (hof@gimp.org)",
                         "Wolfgang Hofer",
                         GAP_VERSION_WITH_DATE,
                         N_("Delete Frames..."),
                         "RGB*, INDEXED*, GRAY*",
                         GIMP_PLUGIN,
                         nargs_del, nreturn_std,
                         args_del, return_std);

  gimp_install_procedure(PLUGIN_NAME_GAP_DUP,
                         "This plugin duplicates the current frames on disk n-times.",
                         "",
                         "Wolfgang Hofer (hof@gimp.org)",
                         "Wolfgang Hofer",
                         GAP_VERSION_WITH_DATE,
                         N_("Duplicate Frames..."),
                         "RGB*, INDEXED*, GRAY*",
                         GIMP_PLUGIN,
                         nargs_dup, nreturn_std,
                         args_dup, return_std);

  gimp_install_procedure(PLUGIN_NAME_GAP_DENSITY,
                         "This plugin changes the number of frames (density) on disk to match a"
                         " new target framerate that is densty_factor times higher"
                         " than the origianl framerate."
                         " (or 1/density_factor lower if density_grow is FALSE)"
                         " changing of density results in duplicating (or deleting)"
                         " of frames on disk",
                         "",
                         "Wolfgang Hofer (hof@gimp.org)",
                         "Wolfgang Hofer",
                         GAP_VERSION_WITH_DATE,
                         N_("Frames Density..."),
                         "RGB*, INDEXED*, GRAY*",
                         GIMP_PLUGIN,
                         nargs_dup, nreturn_std,
                         args_dup, return_std);

  gimp_install_procedure(PLUGIN_NAME_GAP_EXCHG,
                         "This plugin exchanges content of the current with destination frame.",
                         "",
                         "Wolfgang Hofer (hof@gimp.org)",
                         "Wolfgang Hofer",
                         GAP_VERSION_WITH_DATE,
                         N_("Exchange Frame..."),
                         "RGB*, INDEXED*, GRAY*",
                         GIMP_PLUGIN,
                         nargs_exchg, nreturn_std,
                         args_exchg, return_std);


  gimp_install_procedure(PLUGIN_NAME_GAP_RANGE_TO_MULTILAYER,
                         "This plugin creates a new image from the given range of frame-images. Each frame is converted to one layer in the new image, according to flatten_mode. (the frames on disk are not changed).",
                         "",
                         "Wolfgang Hofer (hof@gimp.org)",
                         "Wolfgang Hofer",
                         GAP_VERSION_WITH_DATE,
                         N_("Frames to Image..."),
                         "RGB*, INDEXED*, GRAY*",
                         GIMP_PLUGIN,
                         nargs_f2multi, nreturn_f2multi,
                         args_f2multi, return_f2multi);

  gimp_install_procedure(PLUGIN_NAME_GAP_RANGE_FLATTEN,
                         "This plugin flattens the given range of frame-images (on disk)",
                         "",
                         "Wolfgang Hofer (hof@gimp.org)",
                         "Wolfgang Hofer",
                         GAP_VERSION_WITH_DATE,
                         N_("Frames Flatten..."),
                         "RGB*, INDEXED*, GRAY*",
                         GIMP_PLUGIN,
                         nargs_rflatt, nreturn_std,
                         args_rflatt, return_std);

  gimp_install_procedure(PLUGIN_NAME_GAP_RANGE_LAYER_DEL,
                         "This plugin deletes one layer in the given range of frame-images (on disk). exception: the last remaining layer of a frame is not deleted",
                         "",
                         "Wolfgang Hofer (hof@gimp.org)",
                         "Wolfgang Hofer",
                         GAP_VERSION_WITH_DATE,
                         N_("Frames Layer Delete..."),
                         "RGB*, INDEXED*, GRAY*",
                         GIMP_PLUGIN,
                         nargs_rlayerdel, nreturn_std,
                         args_rlayerdel, return_std);

  gimp_install_procedure(PLUGIN_NAME_GAP_RANGE_CONVERT,
                         "This plugin converts the given range of frame-images to other fileformats (on disk) depending on extension",
                         "WARNING this procedure is obsolete, please use plug_in_gap_range_convert2",
                         "Wolfgang Hofer (hof@gimp.org)",
                         "Wolfgang Hofer",
                         GAP_VERSION_WITH_DATE,
                         NULL,                      /* do not appear in menus */
                         "RGB*, INDEXED*, GRAY*",
                         GIMP_PLUGIN,
                         nargs_rconv, nreturn_rconv,
                         args_rconv, return_rconv);

  gimp_install_procedure(PLUGIN_NAME_GAP_RANGE_CONVERT2,
                         "This plugin converts the given range of frame-images to other fileformats (on disk) depending on extension",
                         "only one of the converted frames is returned (the one with lowest handled frame number)",
                         "Wolfgang Hofer (hof@gimp.org)",
                         "Wolfgang Hofer",
                         GAP_VERSION_WITH_DATE,
                         N_("Frames Convert..."),
                         "RGB*, INDEXED*, GRAY*",
                         GIMP_PLUGIN,
                         nargs_rconv2, nreturn_rconv,
                         args_rconv2, return_rconv);

  gimp_install_procedure(PLUGIN_NAME_GAP_ANIM_RESIZE,
                         "This plugin resizes all video frames (images on disk) to the given new_width/new_height",
                         "",
                         "Wolfgang Hofer (hof@gimp.org)",
                         "Wolfgang Hofer",
                         GAP_VERSION_WITH_DATE,
                         N_("Frames Resize..."),
                         "RGB*, INDEXED*, GRAY*",
                         GIMP_PLUGIN,
                         nargs_resize, nreturn_std,
                         args_resize, return_std);

  gimp_install_procedure(PLUGIN_NAME_GAP_ANIM_CROP,
                         "This plugin crops all video frames (images on disk) to the given new_width/new_height",
                         "",
                         "Wolfgang Hofer (hof@gimp.org)",
                         "Wolfgang Hofer",
                         GAP_VERSION_WITH_DATE,
                         N_("Frames Crop..."),
                         "RGB*, INDEXED*, GRAY*",
                         GIMP_PLUGIN,
                         nargs_resize, nreturn_std,
                         args_resize, return_std);

  gimp_install_procedure(PLUGIN_NAME_GAP_ANIM_SCALE,
                         "This plugin scales all video frames (images on disk) to the given new_width/new_height",
                         "",
                         "Wolfgang Hofer (hof@gimp.org)",
                         "Wolfgang Hofer",
                         GAP_VERSION_WITH_DATE,
                         N_("Frames Scale..."),
                         "RGB*, INDEXED*, GRAY*",
                         GIMP_PLUGIN,
                         nargs_scale, nreturn_std,
                         args_scale, return_std);

  gimp_install_procedure(PLUGIN_NAME_GAP_SPLIT,
                         "This plugin splits the current image to video frames (images on disk). Each layer is saved as one frame",
                         "",
                         "Wolfgang Hofer (hof@gimp.org)",
                         "Wolfgang Hofer",
                         GAP_VERSION_WITH_DATE,
                         N_("Split Image to Frames..."),
                         "RGB*, INDEXED*, GRAY*",
                         GIMP_PLUGIN,
                         nargs_split, nreturn_split,
                         args_split, return_split);

  gimp_install_procedure(PLUGIN_NAME_GAP_SHIFT,
       "This plugin exchanges frame numbers in the given range. (discfile frame_0001.xcf is renamed to frame_0002.xcf, 2->3, 3->4 ... n->1)",
       "",
       "Wolfgang Hofer (hof@gimp.org)",
       "Wolfgang Hofer",
       GAP_VERSION_WITH_DATE,
       N_("Frame Sequence Shift..."),
       "RGB*, INDEXED*, GRAY*",
       GIMP_PLUGIN,
       nargs_shift, nreturn_std,
       args_shift, return_std);

  gimp_install_procedure(PLUGIN_NAME_GAP_REVERSE,
       "This plugin reverse the order of frame numbers in the given range. ( 3-4-5-6-7 ==> 7-6-5-4-3)",
       "",
       "Saul Goode (saulgoode@brickfilms.com)",
       "Saul Goode",
       GAP_VERSION_WITH_DATE,
       N_("Frame Sequence Reverse..."),
       "RGB*, INDEXED*, GRAY*",
       GIMP_PLUGIN,
       nargs_reverse, nreturn_std,
       args_reverse, return_std);

  gimp_install_procedure(PLUGIN_NAME_GAP_RENUMBER,
                         "This plugin renumbers all frames (discfiles) starting at start_frame_nr using n digits for the framenumber part)",
                         "",
                         "Wolfgang Hofer (hof@gimp.org)",
                         "Wolfgang Hofer",
                         GAP_VERSION_WITH_DATE,
                         N_("Frames Renumber..."),
                         "RGB*, INDEXED*, GRAY*",
                         GIMP_PLUGIN,
                         nargs_renumber, nreturn_std,
                         args_renumber, return_std);

  gimp_install_procedure(PLUGIN_NAME_GAP_MODIFY,
                         "This plugin performs a modifying action on each selected layer in each selected framerange",
                         "",
                         "Wolfgang Hofer (hof@gimp.org)",
                         "Wolfgang Hofer",
                         GAP_VERSION_WITH_DATE,
                         N_("Frames Modify..."),
                         "RGB*, INDEXED*, GRAY*",
                         GIMP_PLUGIN,
                         nargs_modify, nreturn_std,
                         args_modify, return_std);

  gimp_install_procedure(PLUGIN_NAME_GAP_VIDEO_EDIT_COPY,
                         "This plugin appends the selected framerange to the video paste buffer"
                         " the video paste buffer is a directory configured by gimprc (video-paste-dir )"
                         " and a framefile basename configured by gimprc (video-paste-basename)"
                         ,
                         "",
                         "Wolfgang Hofer (hof@gimp.org)",
                         "Wolfgang Hofer",
                         GAP_VERSION_WITH_DATE,
                         NULL,                     /* do not appear in menus */
                         "RGB*, INDEXED*, GRAY*",
                         GIMP_PLUGIN,
                         nargs_video_copy, nreturn_nothing,
                         args_video_copy, return_nothing);

  gimp_install_procedure(PLUGIN_NAME_GAP_VIDEO_EDIT_PASTE,
                         "This plugin copies all frames from the video paste buffer"
                         " to the current video. Depending on the paste_mode parameter"
                         " the copied frames are replacing frames beginning at current frame"
                         " or are inserted before or after the current frame"
                         " the pasted frames are scaled to fit the current video size"
                         " and converted in Imagetype (RGB,GRAY,INDEXED) if necessary"
                         " the video paste buffer is a directory configured by gimprc (video-paste-dir)"
                         " and a framefile basename configured by gimprc (video-paste-basename)"
                         ,
                         "",
                         "Wolfgang Hofer (hof@gimp.org)",
                         "Wolfgang Hofer",
                         GAP_VERSION_WITH_DATE,
                         NULL,                     /* do not appear in menus */
                         "RGB*, INDEXED*, GRAY*",
                         GIMP_PLUGIN,
                         nargs_video_paste, nreturn_std,
                         args_video_paste, return_std);

  gimp_install_procedure(PLUGIN_NAME_GAP_VIDEO_EDIT_CLEAR,
                         "Clear the video paste buffer by deleting all framefiles"
                         " the video paste buffer is a directory configured by gimprc (video-paste-dir)"
                         " and a framefile basename configured by gimprc (video-paste-basename)"
                         ,
                         "",
                         "Wolfgang Hofer (hof@gimp.org)",
                         "Wolfgang Hofer",
                         GAP_VERSION_WITH_DATE,
                         NULL,                     /* do not appear in menus */
                         "RGB*, INDEXED*, GRAY*",
                         GIMP_PLUGIN,
                         G_N_ELEMENTS (args_video_clear),
                         nreturn_nothing,
                         args_video_clear, return_nothing);

  gimp_install_procedure(PLUGIN_NAME_GAP_GET_ANIMINFO,
                         "This plugin gets animation infos about video frames."
                         ,
                         "Informations about the video frames belonging to the\n"
                         " passed image_id are returned. (therefore the directory\n"
                         " is scanned and checked for video frame discfiles.\n"
                         " If you call this plugin on images without a Name\n"
                         " You will get just default values."
                         ,
                         "Wolfgang Hofer (hof@gimp.org)",
                         "Wolfgang Hofer",
                         GAP_VERSION_WITH_DATE,
                         NULL,                /* no menu */
                         "RGB*, INDEXED*, GRAY*",
                         GIMP_PLUGIN,
                         nargs_std, nreturn_ainfo,
                         args_std, return_ainfo);

  gimp_install_procedure(PLUGIN_NAME_GAP_SET_FRAMERATE,
                         "This plugin sets the framerate for video frames",
                         "The framerate is stored in a video info file"
                         " named like the basename of the video frames"
                         " without a framenumber. The extension"
                         " of this video info file is .vin",
                         "Wolfgang Hofer (hof@gimp.org)",
                         "Wolfgang Hofer",
                         GAP_VERSION_WITH_DATE,
                         NULL,                      /* no menu */
                         "RGB*, INDEXED*, GRAY*",
                         GIMP_PLUGIN,
                         nargs_setrate, nreturn_nothing,
                         args_setrate, return_nothing);

  {
     /* Menu names */
     const char *menupath_image_video = N_("<Image>/Video/");
     const char *menupath_image_video_goto = N_("<Image>/Video/Go To/");

     //gimp_plugin_menu_branch_register("<Image>", "Video");
     //gimp_plugin_menu_branch_register("<Image>/Video", "Go To");

     gimp_plugin_menu_register (PLUGIN_NAME_GAP_NEXT, menupath_image_video_goto);
     gimp_plugin_menu_register (PLUGIN_NAME_GAP_PREV, menupath_image_video_goto);
     gimp_plugin_menu_register (PLUGIN_NAME_GAP_FIRST, menupath_image_video_goto);
     gimp_plugin_menu_register (PLUGIN_NAME_GAP_LAST, menupath_image_video_goto);
     gimp_plugin_menu_register (PLUGIN_NAME_GAP_GOTO, menupath_image_video_goto);

     gimp_plugin_menu_register (PLUGIN_NAME_GAP_DEL, menupath_image_video);
     gimp_plugin_menu_register (PLUGIN_NAME_GAP_DUP, menupath_image_video);
     gimp_plugin_menu_register (PLUGIN_NAME_GAP_DENSITY, menupath_image_video);
     gimp_plugin_menu_register (PLUGIN_NAME_GAP_EXCHG, menupath_image_video);
     gimp_plugin_menu_register (PLUGIN_NAME_GAP_RANGE_TO_MULTILAYER, menupath_image_video);
     gimp_plugin_menu_register (PLUGIN_NAME_GAP_RANGE_FLATTEN, menupath_image_video);
     gimp_plugin_menu_register (PLUGIN_NAME_GAP_RANGE_LAYER_DEL, menupath_image_video);
     gimp_plugin_menu_register (PLUGIN_NAME_GAP_RANGE_CONVERT2, menupath_image_video);
     gimp_plugin_menu_register (PLUGIN_NAME_GAP_ANIM_RESIZE, menupath_image_video);
     gimp_plugin_menu_register (PLUGIN_NAME_GAP_ANIM_CROP, menupath_image_video);
     gimp_plugin_menu_register (PLUGIN_NAME_GAP_ANIM_SCALE, menupath_image_video);
     gimp_plugin_menu_register (PLUGIN_NAME_GAP_SPLIT, menupath_image_video);
     gimp_plugin_menu_register (PLUGIN_NAME_GAP_SHIFT, menupath_image_video);
     gimp_plugin_menu_register (PLUGIN_NAME_GAP_REVERSE, menupath_image_video);
     gimp_plugin_menu_register (PLUGIN_NAME_GAP_RENUMBER, menupath_image_video);
     gimp_plugin_menu_register (PLUGIN_NAME_GAP_MODIFY, menupath_image_video);
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

  char        l_extension[32];
  gint32      l_new_position;
  char        l_delimiter[32];
  char        l_sel_groupname[MAX_LAYERNAME];
  char        l_new_groupname[MAX_LAYERNAME];
  char        l_sel_str[MAX_LAYERNAME];
  char        l_layername[MAX_LAYERNAME];
  char       *l_basename_ptr;
  char       *l_palette_ptr;
  static GimpParam values[20];
  GimpRunMode run_mode;
  GimpRunMode lock_run_mode;
  GimpPDBStatusType status;
  gint32     image_id;
  gint32     lock_image_id;
  gint32     nr;
  long       range_from, range_to;
  GimpImageBaseType dest_type;
  gint32     dest_colors;
  gint32     dest_dither;
  gint32     palette_type;
  gint32     alpha_dither;
  gint32     remove_unused;
  gint32     mode;
  long       new_width;
  long       new_height;
  long       offs_x;
  long       offs_y;
  gint32     inverse_order;
  gint32     no_alpha;
  long       framerate;
#define      FRAME_BASENAME_LEN   256
  char       frame_basename[FRAME_BASENAME_LEN];
  gint32     sel_mode, sel_case, sel_invert;
  gint32     selection_mode;

  gint32     l_rc_image;

  /* init std return values status and image (as used in most of the gap plug-ins) */
  *nreturn_vals = 2;
  *return_vals = values;
  status = GIMP_PDB_SUCCESS;
  values[0].type = GIMP_PDB_STATUS;
  values[0].data.d_status = GIMP_PDB_EXECUTION_ERROR;
  values[1].type = GIMP_PDB_IMAGE;
  values[1].data.d_int32 = -1;

  nr = 0;
  l_rc_image = -1;


  l_env = g_getenv("GAP_DEBUG");
  if(l_env != NULL)
  {
    if((*l_env != 'n') && (*l_env != 'N')) gap_debug = 1;
  }

  run_mode = param[0].data.d_int32;
  lock_run_mode = run_mode;
  if((strcmp (name, PLUGIN_NAME_GAP_PREV) == 0)
  || (strcmp (name, PLUGIN_NAME_GAP_NEXT) == 0))
  {
    /* stepping from frame to frame is often done
     * in quick sequence via Shortcut Key, and should
     * not open dialog windows if the previous GAP lock is still active.
     * therefore i set NONINTERACTIVE run_mode to prevent
     * gap_lock_check_for_lock from open a gimp_message window.
     */
    lock_run_mode = GIMP_RUN_NONINTERACTIVE;
  }

  if(gap_debug) fprintf(stderr, "\n\ngap_main: debug name = %s\n", name);

  /* gimp_ui_init is sometimes needed even in NON-Interactive
   * runmodes.
   * because thumbnail handling uses the procedure gdk_pixbuf_new_from_file
   * that will crash if not initialized
   * so better init always, just to be on the save side.
   * (most diaolgs do init a 2.nd time but this worked well)
   */
  gimp_ui_init ("gap_main", FALSE);

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
   * NON-LOCKING gap_plugins
   * ---------------------------
   */
  if (strcmp (name, PLUGIN_NAME_GAP_GET_ANIMINFO) == 0)
  {
      GapAnimInfo *ainfo_ptr;
      GapVinVideoInfo *vin_ptr;

      ainfo_ptr = gap_lib_alloc_ainfo(image_id, GIMP_RUN_NONINTERACTIVE);
      if(ainfo_ptr == NULL)
      {
          status = GIMP_PDB_EXECUTION_ERROR;
      }
      else if (0 != gap_lib_dir_ainfo(ainfo_ptr))
      {
          status = GIMP_PDB_EXECUTION_ERROR;
      }


      /* return the new generated image_id */
      *nreturn_vals = nreturn_ainfo +1;

      values[0].data.d_status = status;

      values[1].type = GIMP_PDB_INT32;
      values[2].type = GIMP_PDB_INT32;
      values[3].type = GIMP_PDB_INT32;
      values[4].type = GIMP_PDB_INT32;
      values[5].type = GIMP_PDB_STRING;
      values[6].type = GIMP_PDB_STRING;
      values[7].type = GIMP_PDB_FLOAT;

      if(ainfo_ptr != NULL)
      {
        values[1].data.d_int32  = ainfo_ptr->first_frame_nr;
        values[2].data.d_int32  = ainfo_ptr->last_frame_nr;
        values[3].data.d_int32  = ainfo_ptr->curr_frame_nr;
        values[4].data.d_int32  = ainfo_ptr->frame_cnt;
        values[5].data.d_string = g_strdup(ainfo_ptr->basename);
        values[6].data.d_string = g_strdup(ainfo_ptr->extension);
        values[7].data.d_float  = -1.0;

        vin_ptr = gap_vin_get_all(ainfo_ptr->basename);
        if(vin_ptr != NULL)
        {
           values[7].data.d_float = vin_ptr->framerate;
        }
      }
      return;
  }
  else if (strcmp (name, PLUGIN_NAME_GAP_SET_FRAMERATE) == 0)
  {
      GapAnimInfo *ainfo_ptr;
      GapVinVideoInfo *vin_ptr;

      *nreturn_vals = nreturn_nothing +1;

      if (n_params != nargs_setrate)
      {
         status = GIMP_PDB_CALLING_ERROR;
      }
      else
      {
        ainfo_ptr = gap_lib_alloc_ainfo(image_id, GIMP_RUN_NONINTERACTIVE);
        if(ainfo_ptr == NULL)
        {
            status = GIMP_PDB_EXECUTION_ERROR;
        }
        else if (0 != gap_lib_dir_ainfo(ainfo_ptr))
        {
            status = GIMP_PDB_EXECUTION_ERROR;;
        }
        else
        {
          vin_ptr = gap_vin_get_all(ainfo_ptr->basename);
          if(vin_ptr == NULL)
          {
            status = GIMP_PDB_EXECUTION_ERROR;;
          }
          else
          {
            vin_ptr->framerate = param[3].data.d_float;
            gap_vin_set_common(vin_ptr, ainfo_ptr->basename);
          }
        }
      }

     values[0].data.d_status = status;
     return;
  }



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

  /* ---------------------------
   * LOCKING gap_plugins
   * ---------------------------
   */

  if (strcmp (name, PLUGIN_NAME_GAP_NEXT) == 0)
  {
      if (run_mode == GIMP_RUN_NONINTERACTIVE)
      {
        if (n_params != nargs_std)
        {
          status = GIMP_PDB_CALLING_ERROR;
        }
      }

      if (status == GIMP_PDB_SUCCESS)
      {
        l_rc_image = gap_base_next(run_mode, image_id);
      }
  }
  else if (strcmp (name, PLUGIN_NAME_GAP_PREV) == 0)
  {
      if (run_mode == GIMP_RUN_NONINTERACTIVE)
      {
        if (n_params != nargs_std)
        {
          status = GIMP_PDB_CALLING_ERROR;
        }
      }

      if (status == GIMP_PDB_SUCCESS)
      {
        l_rc_image = gap_base_prev(run_mode, image_id);
      }
  }
  else if (strcmp (name, PLUGIN_NAME_GAP_FIRST) == 0)
  {
      if (run_mode == GIMP_RUN_NONINTERACTIVE)
      {
        if (n_params != nargs_std)
        {
          status = GIMP_PDB_CALLING_ERROR;
        }
      }

      if (status == GIMP_PDB_SUCCESS)
      {
        l_rc_image = gap_base_first(run_mode, image_id);
      }
  }
  else if (strcmp (name, PLUGIN_NAME_GAP_LAST) == 0)
  {
      if (run_mode == GIMP_RUN_NONINTERACTIVE)
      {
        if (n_params != nargs_std)
        {
          status = GIMP_PDB_CALLING_ERROR;
        }
      }

      if (status == GIMP_PDB_SUCCESS)
      {
        l_rc_image = gap_base_last(run_mode, image_id);
      }
  }
  else if (strcmp (name, PLUGIN_NAME_GAP_GOTO) == 0)
  {
      if (run_mode == GIMP_RUN_NONINTERACTIVE)
      {
        if (n_params != nargs_goto)
        {
          status = GIMP_PDB_CALLING_ERROR;
        }
      }

      if (status == GIMP_PDB_SUCCESS)
      {
        nr       = param[3].data.d_int32;  /* destination frame nr */
        l_rc_image = gap_base_goto(run_mode, image_id, nr);
      }
  }
  else if (strcmp (name, PLUGIN_NAME_GAP_DEL) == 0)
  {
      if (run_mode == GIMP_RUN_NONINTERACTIVE)
      {
        if (n_params != nargs_del)
        {
          status = GIMP_PDB_CALLING_ERROR;
        }
      }

      if (status == GIMP_PDB_SUCCESS)
      {
        nr       = param[3].data.d_int32;  /* number of frames to delete */
        l_rc_image = gap_base_del(run_mode, image_id, nr);
      }
  }
  else if (strcmp (name, PLUGIN_NAME_GAP_DUP) == 0)
  {
      if (run_mode == GIMP_RUN_NONINTERACTIVE)
      {
        /* accept the old interface with 4 parameters */
        if ((n_params != 4) && (n_params != nargs_dup))
        {
          status = GIMP_PDB_CALLING_ERROR;
        }
      }

      if (status == GIMP_PDB_SUCCESS)
      {
        nr       = param[3].data.d_int32;  /* how often to copy current frame */
        if (n_params > 5)
        {
           range_from = param[4].data.d_int32;  /* frame nr to start */
           range_to   = param[5].data.d_int32;  /* frame nr to stop  */
        }
        else
        {
           range_from = -1;
           range_to   = -1;
        }

        l_rc_image = gap_base_dup(run_mode, image_id, nr, range_from, range_to );

      }
  }
  else if (strcmp (name, PLUGIN_NAME_GAP_DENSITY) == 0)
  {
     gdouble density_factor;
     gboolean density_grow;

      if (run_mode == GIMP_RUN_NONINTERACTIVE)
      {
        if (n_params != nargs_density)
        {
          status = GIMP_PDB_CALLING_ERROR;
        }
      }

      if (status == GIMP_PDB_SUCCESS)
      {
        range_from      = param[3].data.d_int32;  /* frame nr to start */
        range_to        = param[4].data.d_int32;  /* frame nr to stop  */
        density_factor  = param[5].data.d_float;
        density_grow    = param[6].data.d_int32;

        l_rc_image = gap_base_density(run_mode, image_id, range_from, range_to, density_factor, density_grow);
      }
  }
  else if (strcmp (name, PLUGIN_NAME_GAP_EXCHG) == 0)
  {
      if (run_mode == GIMP_RUN_NONINTERACTIVE)
      {
        if (n_params != nargs_exchg)
        {
          status = GIMP_PDB_CALLING_ERROR;
        }
      }

      if (status == GIMP_PDB_SUCCESS)
      {
        nr       = param[3].data.d_int32;  /* nr of frame to exchange with current frame */
        l_rc_image = gap_base_exchg(run_mode, image_id, nr);
      }
  }
  else if (strcmp (name, PLUGIN_NAME_GAP_RANGE_TO_MULTILAYER) == 0)
  {
      *nreturn_vals = nreturn_f2multi + 1;
      l_rc_image = -1;
      selection_mode = GAP_RANGE_OPS_SEL_IGNORE;

      if (run_mode == GIMP_RUN_NONINTERACTIVE)
      {
        if ((n_params != 7) && (n_params != 9) && (n_params != nargs_f2multi))
        {
          status = GIMP_PDB_CALLING_ERROR;
        }
      }

      if (status == GIMP_PDB_SUCCESS)
      {
        range_from = param[3].data.d_int32;  /* frame nr to start */
        range_to   = param[4].data.d_int32;  /* frame nr to stop  */
        mode       = param[5].data.d_int32;  /* { expand-as-necessary(0), CLIP-TO_IMG(1), CLIP-TO-BG-LAYER(2), FLATTEN(3)} */
        nr         = param[6].data.d_int32;  /* { BG_NOT_VISIBLE (0), BG_VISIBLE(1)} */
        /* old interface: use default values for framerate and frame_basename */
        framerate = 50;
        strcpy(frame_basename, "frame");

        sel_mode = GAP_MTCH_ALL_VISIBLE;
        sel_invert = FALSE;
        sel_case   = TRUE;

        if ( n_params >= 9 )
        {
           framerate  = param[7].data.d_int32;
           if(param[8].data.d_string != NULL)
           {
              strncpy(frame_basename, param[8].data.d_string, FRAME_BASENAME_LEN -1);
              frame_basename[FRAME_BASENAME_LEN -1] = '\0';
           }
        }
        if ( n_params >= 13 )
        {
          sel_mode   = param[9].data.d_int32;
          sel_case   = param[10].data.d_int32;
          sel_invert = param[11].data.d_int32;
          if(param[12].data.d_string != NULL)
          {
            strncpy(l_sel_str, param[12].data.d_string, sizeof(l_sel_str) -1);
            l_sel_str[sizeof(l_sel_str) -1] = '\0';
          }
          selection_mode = param[13].data.d_int32;
        }

        l_rc_image = gap_range_to_multilayer(run_mode, image_id, range_from, range_to, mode, nr,
                                       framerate, frame_basename, FRAME_BASENAME_LEN,
                                       sel_mode, sel_case, sel_invert, l_sel_str, selection_mode);

      }
      values[1].type = GIMP_PDB_IMAGE;
      values[1].data.d_int32 = l_rc_image;  /* return the new generated image_id */
  }
  else if (strcmp (name, PLUGIN_NAME_GAP_RANGE_FLATTEN) == 0)
  {
      if (run_mode == GIMP_RUN_NONINTERACTIVE)
      {
        if (n_params != nargs_rflatt)
        {
          status = GIMP_PDB_CALLING_ERROR;
        }
      }

      if (status == GIMP_PDB_SUCCESS)
      {
        range_from = param[3].data.d_int32;  /* frame nr to start */
        range_to   = param[4].data.d_int32;  /* frame nr to stop  */

        l_rc_image = gap_range_flatten(run_mode, image_id, range_from, range_to);

      }
  }
  else if (strcmp (name, PLUGIN_NAME_GAP_RANGE_LAYER_DEL) == 0)
  {
      if (run_mode == GIMP_RUN_NONINTERACTIVE)
      {
        if (n_params != nargs_rlayerdel)
        {
          status = GIMP_PDB_CALLING_ERROR;
        }
      }

      if (status == GIMP_PDB_SUCCESS)
      {
        range_from = param[3].data.d_int32;  /* frame nr to start */
        range_to   = param[4].data.d_int32;  /* frame nr to stop  */
        nr         = param[5].data.d_int32;  /* layerstack position (0 == on top) */

        l_rc_image = gap_range_layer_del(run_mode, image_id, range_from, range_to, nr);

      }
  }
  else if ((strcmp (name, PLUGIN_NAME_GAP_RANGE_CONVERT) == 0)
       ||  (strcmp (name, PLUGIN_NAME_GAP_RANGE_CONVERT2) == 0))
  {
      *nreturn_vals = nreturn_rconv +1;
      l_basename_ptr = NULL;
      l_palette_ptr = NULL;
      palette_type = GIMP_MAKE_PALETTE;
      alpha_dither = 0;
      remove_unused = 1;
      if (run_mode == GIMP_RUN_NONINTERACTIVE)
      {
        if ((n_params != 10) && (n_params != nargs_rconv) && (n_params != nargs_rconv2))
        {
          status = GIMP_PDB_CALLING_ERROR;
        }
        else
        {
          strncpy(l_extension, param[9].data.d_string, sizeof(l_extension) -1);
          l_extension[sizeof(l_extension) -1] = '\0';
          if (n_params >= 11)
          {
            l_basename_ptr = param[10].data.d_string;
          }
          if (n_params >= 15)
          {
             l_palette_ptr = param[14].data.d_string;
             palette_type = param[11].data.d_int32;
             alpha_dither = param[12].data.d_int32;
             remove_unused = param[13].data.d_int32;
          }
        }
      }

      if (status == GIMP_PDB_SUCCESS)
      {
        image_id    = param[1].data.d_image;
        range_from  = param[3].data.d_int32;  /* frame nr to start */
        range_to    = param[4].data.d_int32;  /* frame nr to stop  */
        nr          = param[5].data.d_int32;  /* flatten (0 == no , 1 == flatten, 2 == merge visible) */
        dest_type   = param[6].data.d_int32;
        dest_colors = param[7].data.d_int32;
        dest_dither = param[8].data.d_int32;

        l_rc_image = gap_range_conv(run_mode, image_id, range_from, range_to, nr,
                              dest_type, dest_colors, dest_dither,
                              l_basename_ptr, l_extension,
                              palette_type,
                              alpha_dither,
                              remove_unused,
                              l_palette_ptr);

      }
  }
  else if (strcmp (name, PLUGIN_NAME_GAP_ANIM_RESIZE) == 0)
  {
      if (run_mode == GIMP_RUN_NONINTERACTIVE)
      {
        if (n_params != nargs_resize)
        {
          status = GIMP_PDB_CALLING_ERROR;
        }
      }

      if (status == GIMP_PDB_SUCCESS)
      {
        new_width  = param[3].data.d_int32;
        new_height = param[4].data.d_int32;
        offs_x     = param[5].data.d_int32;
        offs_y     = param[6].data.d_int32;

        l_rc_image = gap_range_anim_sizechange(run_mode, GAP_ASIZ_RESIZE, image_id,
                                   new_width, new_height, offs_x, offs_y);

      }
  }
  else if (strcmp (name, PLUGIN_NAME_GAP_ANIM_CROP) == 0)
  {
      if (run_mode == GIMP_RUN_NONINTERACTIVE)
      {
        if (n_params != 7)
        {
          status = GIMP_PDB_CALLING_ERROR;
        }
      }

      if (status == GIMP_PDB_SUCCESS)
      {
        new_width  = param[3].data.d_int32;
        new_height = param[4].data.d_int32;
        offs_x     = param[5].data.d_int32;
        offs_y     = param[6].data.d_int32;

        l_rc_image = gap_range_anim_sizechange(run_mode, GAP_ASIZ_CROP, image_id,
                                   new_width, new_height, offs_x, offs_y);

      }
  }
  else if (strcmp (name, PLUGIN_NAME_GAP_ANIM_SCALE) == 0)
  {
      if (run_mode == GIMP_RUN_NONINTERACTIVE)
      {
        if (n_params != nargs_scale)
        {
          status = GIMP_PDB_CALLING_ERROR;
        }
      }

      if (status == GIMP_PDB_SUCCESS)
      {
        new_width  = param[3].data.d_int32;
        new_height = param[4].data.d_int32;

        l_rc_image = gap_range_anim_sizechange(run_mode, GAP_ASIZ_SCALE, image_id,
                                   new_width, new_height, 0, 0);

      }
  }
  else if (strcmp (name, PLUGIN_NAME_GAP_SPLIT) == 0)
  {
      gint32 l_digits;
      gint32 l_only_visible;
      gint32 l_copy_properties;

      *nreturn_vals = nreturn_split +1;
      l_rc_image = -1;
      if (run_mode == GIMP_RUN_NONINTERACTIVE)
      {
        if (n_params != nargs_split)
        {
          status = GIMP_PDB_CALLING_ERROR;
        }
        else
        {
          strncpy(l_extension, param[5].data.d_string, sizeof(l_extension) -1);
          l_extension[sizeof(l_extension) -1] = '\0';
        }
      }

      if (status == GIMP_PDB_SUCCESS)
      {
        inverse_order = param[3].data.d_int32;
        no_alpha      = param[4].data.d_int32;
        l_only_visible= param[6].data.d_int32;
        l_digits      = param[7].data.d_int32;
        l_copy_properties = param[8].data.d_int32;
        l_rc_image = gap_split_image(run_mode, image_id,
                              inverse_order, no_alpha, l_extension, l_only_visible, l_copy_properties, l_digits);

      }
      /* IMAGE ID is filled at end (same as in standard  return handling) */
      values[1].type = GIMP_PDB_IMAGE;
      values[1].data.d_int32 = l_rc_image;  /* return the new generated image_id */
  }
  else if (strcmp (name, PLUGIN_NAME_GAP_SHIFT) == 0)
  {
      if (run_mode == GIMP_RUN_NONINTERACTIVE)
      {
        if (n_params != nargs_shift)
        {
          status = GIMP_PDB_CALLING_ERROR;
        }
      }

      if (status == GIMP_PDB_SUCCESS)
      {
        nr       = param[3].data.d_int32;  /* how many framenumbers to shift the framesequence */
        range_from = param[4].data.d_int32;  /* frame nr to start */
        range_to   = param[5].data.d_int32;  /* frame nr to stop  */

        l_rc_image = gap_base_shift(run_mode, image_id, nr, range_from, range_to );

      }
  }
  else if (strcmp (name, PLUGIN_NAME_GAP_REVERSE) == 0)
  {
      if (run_mode == GIMP_RUN_NONINTERACTIVE)
      {
        if (n_params != nargs_shift)
        {
          status = GIMP_PDB_CALLING_ERROR;
        }
      }

      if (status == GIMP_PDB_SUCCESS)
      {
        range_from = param[3].data.d_int32;  /* frame nr to start */
        range_to   = param[4].data.d_int32;  /* frame nr to stop  */

        l_rc_image = gap_base_reverse(run_mode, image_id, range_from, range_to );

      }
  }
  else if (strcmp (name, PLUGIN_NAME_GAP_RENUMBER) == 0)
  {
      gint32 digits;

      digits = GAP_LIB_DEFAULT_DIGITS;
      if (run_mode == GIMP_RUN_NONINTERACTIVE)
      {
        if (n_params != nargs_renumber)
        {
          status = GIMP_PDB_CALLING_ERROR;
        }
      }

      if (status == GIMP_PDB_SUCCESS)
      {
        nr       = param[3].data.d_int32;  /* new start framenumber of the 1.st frame in the framesequence */
        digits   = param[4].data.d_int32;  /* digits to use for framenumber part in the filename(s) */

        l_rc_image = gap_base_renumber(run_mode, image_id, nr, digits);

      }
  }
  else if (strcmp (name, PLUGIN_NAME_GAP_VIDEO_EDIT_COPY) == 0)
  {
      *nreturn_vals = nreturn_nothing +1;
      if (n_params != nargs_video_copy)
      {
          status = GIMP_PDB_CALLING_ERROR;
      }

      if (status == GIMP_PDB_SUCCESS)
      {
        range_from = param[3].data.d_int32;  /* frame nr to start */
        range_to   = param[4].data.d_int32;  /* frame nr to stop  */

        l_rc_image = gap_vid_edit_copy(run_mode, image_id, range_from, range_to );
      }
  }
  else if (strcmp (name, PLUGIN_NAME_GAP_VIDEO_EDIT_PASTE) == 0)
  {
      if (n_params != nargs_video_paste)
      {
        status = GIMP_PDB_CALLING_ERROR;
      }

      if (status == GIMP_PDB_SUCCESS)
      {
        nr       = param[3].data.d_int32;  /* paste_mode (0,1 or 2) */

        l_rc_image = gap_vid_edit_paste(run_mode, image_id, nr );
      }
  }
  else if (strcmp (name, PLUGIN_NAME_GAP_VIDEO_EDIT_CLEAR) == 0)
  {
      *nreturn_vals = nreturn_nothing +1;
      if (status == GIMP_PDB_SUCCESS)
      {
        if(gap_vid_edit_clear() < 0)
        {
          l_rc_image = -1;
        }
        else
        {
          l_rc_image = 0;
        }
      }
  }
  else if (strcmp (name, PLUGIN_NAME_GAP_MODIFY) == 0)
  {
      l_sel_str[0] = '\0';
      l_delimiter[0] = '/';
      l_delimiter[1] = '\0';
      l_sel_groupname[0] = '\0';
      l_new_groupname[0] = '\0';

      if (run_mode == GIMP_RUN_NONINTERACTIVE)
      {
        if (n_params != nargs_modify)
        {
          status = GIMP_PDB_CALLING_ERROR;
        }
        else
        {
          if(param[9].data.d_string != NULL)
          {
            g_snprintf(l_sel_str, sizeof(l_sel_str) -1, "%s", param[9].data.d_string);
          }
          if(param[10].data.d_string != NULL)
          {
            g_snprintf(l_layername, sizeof(l_layername) -1, "%s", param[10].data.d_string);
          }

          if(param[12].data.d_string != NULL)
          {
            g_snprintf(l_new_groupname, sizeof(l_new_groupname) -1, "%s", param[12].data.d_string);
          }
          if(param[13].data.d_string != NULL)
          {
            g_snprintf(l_sel_groupname, sizeof(l_sel_groupname) -1, "%s", param[13].data.d_string);
          }
          if(param[14].data.d_string != NULL)
          {
            g_snprintf(l_delimiter, sizeof(l_delimiter) -1, "%s", param[14].data.d_string);
          }
        }
      }

      if (status == GIMP_PDB_SUCCESS)
      {
        range_from = param[3].data.d_int32;  /* frame nr to start */
        range_to   = param[4].data.d_int32;  /* frame nr to stop  */
        nr         = param[5].data.d_int32;    /* action_nr */
        sel_mode   = param[6].data.d_int32;
        sel_case   = param[7].data.d_int32;
        sel_invert = param[8].data.d_int32;

        l_new_position = param[11].data.d_int32;


        l_rc_image = gap_mod_layer(run_mode, image_id, range_from, range_to,
                             nr, sel_mode, sel_case, sel_invert,
                             l_sel_str, l_layername,
                             l_new_position, l_new_groupname, l_sel_groupname, l_delimiter);

      }
  }

  /* ---------- return handling --------- */

 if(l_rc_image < 0)
 {
    if(gap_debug) printf("gap_main: return GIMP_PDB_EXECUTION_ERROR\n");
    status = GIMP_PDB_EXECUTION_ERROR;
 }
 else
 {
    if(gap_debug) printf("gap_main: return OK\n");
    /* most gap_plug-ins return an image_id in values[1] */
    if (values[1].type == GIMP_PDB_IMAGE)
    {
      if(gap_debug) printf("gap_main: return image_id:%d\n", (int)l_rc_image);
      values[1].data.d_int32 = l_rc_image;  /* return image id  of the resulting (current frame) image */
    }
 }

 if (run_mode != GIMP_RUN_NONINTERACTIVE)
 {
    if(gap_debug) printf("gap_main BEFORE gimp_displays_flush\n");

    gimp_displays_flush();

    if(gap_debug) printf("gap_main AFTER gimp_displays_flush\n");
 }

  values[0].data.d_status = status;

  /* remove LOCK on this image for all gap_plugins */
  gap_lock_remove_lock(lock_image_id);
}

