/* gap_range_ops.c
 * 1997.11.06 hof (Wolfgang Hofer)
 *
 * GAP ... Gimp Animation Plugins
 *
 * This Module contains implementation of range based frame operations.
 * - gap_range_to_multilayer
 * - gap_range_flatten
 * - gap_range_layer_del
 * - gap_range_conv
 * - gap_anim_scale
 * - gap_anim_resize
 * - gap_anim_crop
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

/* revision history
 * 2.1.0a;  2004/11/12   hof: added help buttons
 * 2.1.0a;  2004/04/26   hof: frames_to_multilayer: do not force save of current image
 *                            and use gimp_image_duplicate for the current frame
 *                            works faster, and avoid the "not using xcf" dialog (# 125977)
 *                            for this plug-in when working with other imagefile formats.
 * 1.3.25a; 2004/01/21   hof: message text fixes (# 132030)
 * 1.3.20d; 2003/10/09   hof: sourcecode cleanup,
 *                            extended p_frames_to_multilayer to handle selected regions
 * 1.3.17b; 2003/07/31   hof: message text fixes for translators (# 118392)
 * 1.3.15a; 2003/06/21   hof: textspacing
 * 1.3.14a; 2003/05/18   hof: again using gap_resi_dialog (now based on GimpOffsetArea widget)
 * 1.3.12a; 2003/05/01   hof: merge into CVS-gimp-gap project
 * 1.3.11a; 2003/01/18   hof: Conditional frame save, added Default Button (p_anim_sizechange_dialog)
 * 1.3.5a;  2002/04/20   hof: API cleanup
 * 1.3.5a;  2002/04/06   hof: p_type_convert: use only gimp-1.3.5 supported dither modes (removed GIMP_NODESTRUCT_DITHER)
 * 1.3.4a;  2002/03/12   hof: removed calls to old resize widget
 * 1.1.28a; 2000/11/05   hof: check for GIMP_PDB_SUCCESS (not for FALSE)
 * 1.1.24a  2000/07/01   hof: bugfix: flatten of singlelayer images has to remove alpha channel
 * 1.1.17b  2000/02/26   hof: bugfixes
 * 1.1.14a  2000/01/06   hof: gap_range_to_multilayer: use framerate (from video info file) in framenames
 *                            bugfix: gap_range_to_multilayer: first save current frame
 * 1.1.10a  1999/10/22   hof: bugfix: have to use the changed PDB-Interface
 *                            for gimp_convert_indexed
 *                            (with extended dither options and extra dialog window)
 * 1.1.9a   1999/09/21   hof: bugfix GIMP_RUN_NONINTERACTIVE did not work in
 *                            plug_in_gap_range_convert
 *                            plug_in_gap_range_layer_del
 *                            plug_in_gap_range_flatten
 * 1.1.8    1999/08/31   hof: frames convert: save subsequent frames
 *                            with rumode GIMP_RUN_WITH_LAST_VALS
 * 0.97.00; 1998/10/19   hof: gap_range_to_multilayer: extended layer selection
 * 0.96.03; 1998/08/31   hof: gap_range_to_multilayer: all params available
 *                            in non-interactive runmode
 * 0.96.02; 1998/08/05   hof: - p_frames_to_multilayer added framerate support
 * 0.96.00; 1998/07/01   hof: - added scale, resize and crop
 *                              (affects full range == all video frames)
 *                            - now using gap_arr_dialog.h
 * 0.94.01; 1998/04/28   hof: added flatten_mode to plugin: gap_range_to_multilayer
 * 0.92.00  1998.01.10   hof: bugfix in p_frames_to_multilayer
 *                            layers need alpha (to be raise/lower able)
 * 0.90.00               first development release
 */
#include "config.h"

/* SYTEM (UNIX) includes */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <errno.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

/* GIMP includes */
#include "gtk/gtk.h"
#include "config.h"
#include "gap-intl.h"
#include "libgimp/gimp.h"

/* GAP includes */
#include "gap_layer_copy.h"
#include "gap_lib.h"
#include "gap_pdb_calls.h"
#include "gap_match.h"
#include "gap_arr_dialog.h"
#include "gap_resi_dialog.h"
#include "gap_mod_layer.h"
#include "gap_image.h"
#include "gap_range_ops.h"
#include "gap_vin.h"


extern      int gap_debug; /* ==0  ... dont print debug infos */

#define GAP_HELP_ID_FLATTEN          "plug-in-gap-range-flatten"
#define GAP_HELP_ID_LAYER_DEL        "plug-in-gap-range-layer-del"
#define GAP_HELP_ID_CONVERT          "plug-in-gap-range-convert"
#define GAP_HELP_ID_TO_MULTILAYER    "plug-in-gap-range-to-multilayer"


#define FLATTEN_MODE_NONE           0
#define FLATTEN_MODE_FLATTEN        1
#define FLATTEN_MODE_MERGE_VISIBLE  2

/* ============================================================================
 * p_anim_sizechange_dialog
 *   dialog window with 2 (or 4) entry fields
 *   where the user can select the new video frame (Image)-Size
 *   (if cnt == 4 additional Inputfields for offests are available)
 * return -1  in case of cancel or any error
 *            (include check for change of current frame)
 * return positve (0 or layerstack position) if everythig OK
 * ============================================================================
 */
static int
p_anim_sizechange_dialog(GapAnimInfo *ainfo_ptr, GapRangeOpsAsiz asiz_mode,
               long *size_x, long *size_y,
               long *offs_x, long *offs_y)
{
  static GapArrArg  argv[5];
  gint   cnt;
  gchar *title;
  gchar *hline;
  gint   l_width;
  gint   l_height;
  gint   l_rc;

  /* get info about the image (size is common to all frames) */
  l_width  = gimp_image_width(ainfo_ptr->image_id);
  l_height = gimp_image_height(ainfo_ptr->image_id);

  gap_arr_arg_init(&argv[0], GAP_ARR_WGT_INT_PAIR);
  argv[0].label_txt = _("New Width:");
  argv[0].constraint = FALSE;
  argv[0].int_min   = 1;
  argv[0].int_max   = 1024;
  argv[0].umin      = 1;
  argv[0].umax      = 10000;
  argv[0].int_ret   = l_width;
  argv[0].has_default = TRUE;
  argv[0].int_default = l_width;

  gap_arr_arg_init(&argv[1], GAP_ARR_WGT_INT_PAIR);
  argv[1].label_txt = _("New Height:");
  argv[1].constraint = FALSE;
  argv[1].int_min    = 1;
  argv[1].int_max    = 1024;
  argv[1].umin       = 1;
  argv[1].umax       = 10000;
  argv[1].int_ret   = l_height;
  argv[1].has_default = TRUE;
  argv[1].int_default = l_height;

  gap_arr_arg_init(&argv[2], GAP_ARR_WGT_INT_PAIR);
  argv[2].label_txt = _("Offset X:");
  argv[2].constraint = FALSE;
  argv[2].int_min    = 0;
  argv[2].int_max    = l_width;
  argv[2].umin       = 0;
  argv[2].umax       = 10000;
  argv[2].int_ret   = 0;
  argv[2].has_default = TRUE;
  argv[2].int_default = 0;

  gap_arr_arg_init(&argv[3], GAP_ARR_WGT_INT_PAIR);
  argv[3].label_txt = _("Offset Y:");
  argv[3].constraint = FALSE;
  argv[3].int_min    = 0;
  argv[3].int_max    = l_height;
  argv[3].umin       = 0;
  argv[3].umax       = 10000;
  argv[3].int_ret   = 0;
  argv[3].has_default = TRUE;
  argv[3].int_default = 0;


  switch(asiz_mode)
  {
    case GAP_ASIZ_CROP:
      title = _("Crop Video Frames (all)");
      hline = g_strdup_printf (_("Crop (original %dx%d)"), l_width, l_height);
      argv[0].int_max   = l_width;
      argv[0].constraint = TRUE;
      argv[1].int_max   = l_height;
      argv[1].constraint = TRUE;
      argv[2].constraint = TRUE;
      argv[3].constraint = TRUE;
      cnt = 4;
      break;
    case GAP_ASIZ_RESIZE:
      title = _("Resize Video Frames (all)");
      hline = g_strdup_printf (_("Resize (original %dx%d)"), l_width, l_height);
      argv[2].int_min    = -l_width;
      argv[3].int_min    = -l_height;
     cnt = 4;
      break;
    default:
      title = _("Scale Video Frames (all)");
      hline = g_strdup_printf (_("Scale (original %dx%d)"), l_width, l_height);
      cnt = 2;
      break;
  }

  gap_arr_arg_init(&argv[cnt], GAP_ARR_WGT_DEFAULT_BUTTON);
  argv[cnt].label_txt =  _("Reset");                /* should use GIMP_STOCK_RESET if possible */
  argv[cnt].help_txt  = _("Reset parameters to original size");

  cnt++;

  if(0 != gap_lib_chk_framerange(ainfo_ptr))   return -1;

  if(1==0)
  {
    /* array dialog is a primitive GUI to CROP SCALE or RESIZE Frames.
     * In gimp-1.2 GAP used a copy of the old gimp RESIZE and SCALE dialog-widget code.
     */
    l_rc = gap_arr_ok_cancel_dialog(title, hline, cnt, argv);

    *size_x = (long)(argv[0].int_ret);
    *size_y = (long)(argv[1].int_ret);
    *offs_x = (long)(argv[2].int_ret);
    *offs_y = (long)(argv[3].int_ret);


   if(asiz_mode == GAP_ASIZ_CROP)
   {
      /* Clip size down to image borders */
      if((*size_x + *offs_x) > l_width)
      {
        *size_x = l_width - *offs_x;
      }
      if((*size_y + *offs_y) > l_height)
      {
        *size_y = l_height - *offs_y;
      }
   }
  }
  else
  {
    /* better GUI (analog to GIMP-core) is not finished YET  */
    l_rc = gap_resi_dialog(ainfo_ptr->image_id, asiz_mode, title,
                         size_x, size_y, offs_x, offs_y);
  }

  g_free (hline);

  if(l_rc == TRUE)
  {
       if(0 != gap_lib_chk_framechange(ainfo_ptr))
       {
           return -1;
       }
       return 0;        /* OK */
  }
  else
  {
     return -1;
  }

}       /* end p_anim_sizechange_dialog */


/* ============================================================================
 * p_range_dialog
 *   dialog window with 2 (or 3) entry fields
 *   where the user can select a frame range (FROM TO)
 *   (if cnt == 3 additional Layerstackposition)
 * return -1  in case of cancel or any error
 *            (include check for change of current frame)
 * return positve (0 or layerstack position) if everythig OK
 * ============================================================================
 */
static int
p_range_dialog(GapAnimInfo *ainfo_ptr,
               long *range_from, long *range_to,
               char *title, char *hline, gint cnt,
               const char *help_id)
{
  static GapArrArg  argv[4];
  gint              argc;


  argc = 0;

  gap_arr_arg_init(&argv[argc], GAP_ARR_WGT_INT_PAIR);
  argv[argc].label_txt = _("From Frame:");
  argv[argc].help_txt  = _("First handled frame");
  argv[argc].constraint = TRUE;
  argv[argc].int_min   = (gint)ainfo_ptr->first_frame_nr;
  argv[argc].int_max   = (gint)ainfo_ptr->last_frame_nr;
  argv[argc].int_ret   = (gint)ainfo_ptr->curr_frame_nr;
  argc++;


  gap_arr_arg_init(&argv[argc], GAP_ARR_WGT_INT_PAIR);
  argv[argc].label_txt = _("To Frame:");
  argv[argc].help_txt  = _("Last handled frame");
  argv[argc].constraint = TRUE;
  argv[argc].int_min   = (gint)ainfo_ptr->first_frame_nr;
  argv[argc].int_max   = (gint)ainfo_ptr->last_frame_nr;
  argv[argc].int_ret   = (gint)ainfo_ptr->last_frame_nr;
  argc++;

  if(cnt == 3)
  {
    gap_arr_arg_init(&argv[argc], GAP_ARR_WGT_INT_PAIR);
    argv[argc].label_txt = _("Layerstack:");
    argv[argc].help_txt  = _("Layerstack position where 0 is the top layer");
    argv[argc].constraint = FALSE;
    argv[argc].int_min   = 0;
    argv[argc].int_max   = 99;
    argv[argc].umin      = 0;
    argv[argc].umax      = 999999;
    argv[argc].int_ret   = 0;
    argc++;
  }


  if(help_id)
  {
    gap_arr_arg_init(&argv[argc], GAP_ARR_WGT_HELP_BUTTON);
    argv[argc].help_id = help_id;
    argc++;
  }


  if(0 != gap_lib_chk_framerange(ainfo_ptr))   return -1;
  if(TRUE == gap_arr_ok_cancel_dialog(title, hline, argc, argv))
  {   *range_from = (long)(argv[0].int_ret);
      *range_to   = (long)(argv[1].int_ret);

       if(0 != gap_lib_chk_framechange(ainfo_ptr))
       {
           return -1;
       }
       return (int)(argv[2].int_ret);
  }
  else
  {
     return -1;
  }

}       /* end p_range_dialog */

/* ============================================================================
 * p_convert_indexed_dialog
 *
 * extra dialog with dither options (when converting to indexed image type)
 *   return  0 .. OK
 *          -1 .. in case of Error or cancel
 * ============================================================================
 */
static long
p_convert_indexed_dialog(gint32 *dest_colors, gint32 *dest_dither,
                      gint32 *palette_type, gint32 *alpha_dither, gint32 *remove_unused,
                      char *palette,   gint len_palette)
{
#define ARGC_INDEXED 6
  static GapArrArg  argv[ARGC_INDEXED];
  static char *radio_paltype[4]  = { N_("Generate Optimal Palette")
                                   , N_("WEB Palette")
                                   , N_("Use Custom Palette")
                                   , N_("Use Black/White (1-Bit) Palette")
                                   };
  static char *radio_dither[4]  = { N_("Floyd-Steinberg Color Dithering (Normal)")
                                  , N_("Floyd-Steinberg Color Dithering (Reduced Color Bleeding)")
                                  , N_("Positioned Color Dithering")
                                  , N_("No Color Dithering")
                                  };
  static int gettextize_paltype = 0;
  static int gettextize_dither = 0;

  for (;gettextize_paltype < 4; gettextize_paltype++)
    radio_paltype[gettextize_paltype] = gettext(radio_paltype[gettextize_paltype]);

  for (;gettextize_dither < 4; gettextize_dither++)
    radio_dither[gettextize_dither] = gettext(radio_dither[gettextize_dither]);

  gap_arr_arg_init(&argv[0], GAP_ARR_WGT_RADIO);
  argv[0].label_txt = _("Palette Type");
  argv[0].help_txt  = NULL;
  argv[0].radio_argc  = 4;
  argv[0].radio_argv = radio_paltype;
  argv[0].radio_ret  = 0;

  gap_arr_arg_init(&argv[1], GAP_ARR_WGT_TEXT);
  argv[1].label_txt = _("Custom Palette");
  argv[1].help_txt  = _("Name of a custom palette (ignored if palette type is not custom)");
  argv[1].text_buf_len = len_palette;
  argv[1].text_buf_ret = palette;

  gap_arr_arg_init(&argv[2], GAP_ARR_WGT_TOGGLE);
  argv[2].label_txt = _("Remove Unused");
  argv[2].help_txt  = _("Remove unused or double colors (ignored if palette type is not custom)");
  argv[2].int_ret   = 1;

  gap_arr_arg_init(&argv[3], GAP_ARR_WGT_INT_PAIR);
  argv[3].constraint = TRUE;
  argv[3].label_txt = _("Number of Colors");
  argv[3].help_txt  = _("Number of resulting colors (ignored if palette type is not generate optimal palette)");
  argv[3].int_min   = 2;
  argv[3].int_max   = 256;
  argv[3].int_ret   = 255;


  gap_arr_arg_init(&argv[4], GAP_ARR_WGT_RADIO);
  argv[4].label_txt = _("Dither Options");
  argv[4].help_txt  = NULL;
  argv[4].radio_argc  = 4;
  argv[4].radio_argv = radio_dither;
  argv[4].radio_ret  = 0;

  gap_arr_arg_init(&argv[5], GAP_ARR_WGT_TOGGLE);
  argv[5].label_txt = _("Enable Transparency");
  argv[5].help_txt  = _("Enable dithering of transparency");
  argv[5].int_ret   = 0;

  if(TRUE == gap_arr_ok_cancel_dialog( _("Convert Frames to Indexed"),
                                 _("Palette and Dither Settings"),
                                  ARGC_INDEXED, argv))
  {
      switch(argv[0].radio_ret)
      {
        case 3:
           *palette_type = GIMP_MONO_PALETTE;
           break;
        case 2:
           *palette_type = GIMP_CUSTOM_PALETTE;
           break;
        case 1:
           *palette_type = GIMP_WEB_PALETTE;
           break;
        default:
           *palette_type = GIMP_MAKE_PALETTE;
           break;
      }
      *remove_unused = (gint32)(argv[2].int_ret);;
      *dest_colors = (gint32)(argv[3].int_ret);
      switch(argv[4].radio_ret)
      {
        case 3:
           *dest_dither = GIMP_NO_DITHER;  /* 0 */
           break;
        case 2:
           *dest_dither = GIMP_FIXED_DITHER; /* 3 */
           break;
        case 1:
           *dest_dither = GIMP_FSLOWBLEED_DITHER; /* 2 */
           break;
        default:
           *dest_dither = GIMP_FS_DITHER;  /* 1 */
           break;
      }

      *alpha_dither = (gint32)(argv[5].int_ret);

      return 0;
  }
  else
  {
     return -1;
  }
}


/* ============================================================================
 * p_convert_dialog
 *
 *   return  0 .. OK
 *          -1 .. in case of Error or cancel
 * ============================================================================
 */
static long
p_convert_dialog(GapAnimInfo *ainfo_ptr,
                      long *range_from, long *range_to, long *flatten,
                      GimpImageBaseType *dest_type, gint32 *dest_colors, gint32 *dest_dither,
                      char *basename, gint len_base,
                      char *extension, gint len_ext,
                      gint32 *palette_type, gint32 *alpha_dither, gint32 *remove_unused,
                      char *palette,   gint len_palette)
{
  static GapArrArg  argv[8];
  static char *radio_args[4]  = {
    N_("Keep Type"),
    N_("Convert to RGB"),
    N_("Convert to Gray"),
    N_("Convert to Indexed")
  };
  static char *radio_flatten_modes[3]  = {
    N_("None"),
    N_("Flatten"),
    N_("Merge Visible Layers")
  };
  static char *radio_flatten_modes_help[3] = {
    N_("Do not merge layers before save to the selected fileformat. "
       "Example: use this when converting to XCF that can handle transparency and multiple layers."),
    N_("Flatten all resulting frames. Most fileformats can not handle multiple layers "
       "and need flattened frames (flattening does melt down all layers to one composite layer)."
       "Example: JPEG can not handle multiple layers and requires flattened frames."),
    N_("Merge resulting frame down to one layer. This keeps transparency information "
       "Example: use this for PNG fileformat that can handle transparency (alpha channel) "
       "but is limited to one layer)") 
  };
    
  static int gettext_cnt1 = 0;
  static int gettext_cnt2 = 0;
  static int gettext_cnt3 = 0;


  for (;gettext_cnt1 < 4; gettext_cnt1++)
    radio_args[gettext_cnt1] = gettext(radio_args[gettext_cnt1]);

  for (;gettext_cnt2 < 3; gettext_cnt2++)
    radio_flatten_modes[gettext_cnt2] = gettext(radio_flatten_modes[gettext_cnt2]);

  for (;gettext_cnt3 < 3; gettext_cnt3++)
    radio_flatten_modes_help[gettext_cnt3] = gettext(radio_flatten_modes_help[gettext_cnt3]);


  gap_arr_arg_init(&argv[0], GAP_ARR_WGT_INT_PAIR);
  argv[0].constraint = TRUE;
  argv[0].label_txt = _("From Frame:");
  argv[0].help_txt  = _("First handled frame");
  argv[0].int_min   = (gint)ainfo_ptr->first_frame_nr;
  argv[0].int_max   = (gint)ainfo_ptr->last_frame_nr;
  argv[0].int_ret   = (gint)ainfo_ptr->curr_frame_nr;

  gap_arr_arg_init(&argv[1], GAP_ARR_WGT_INT_PAIR);
  argv[1].constraint = TRUE;
  argv[1].label_txt = _("To Frame:");
  argv[1].help_txt  = _("Last handled frame");
  argv[1].int_min   = (gint)ainfo_ptr->first_frame_nr;
  argv[1].int_max   = (gint)ainfo_ptr->last_frame_nr;
  argv[1].int_ret   = (gint)ainfo_ptr->last_frame_nr;

  gap_arr_arg_init(&argv[2], GAP_ARR_WGT_LABEL);
  argv[2].label_txt = " ";

  gap_arr_arg_init(&argv[3], GAP_ARR_WGT_FILESEL);
  argv[3].label_txt = _("Basename:");
  argv[3].help_txt  = _("basename of the resulting frames. The number part and extension "
                        "(000001.ext) is added automatically to all converted frames.");
  argv[3].text_buf_len = len_base;
  argv[3].text_buf_ret = basename;

  gap_arr_arg_init(&argv[4], GAP_ARR_WGT_TEXT);
  argv[4].label_txt = _("Extension:");
  argv[4].help_txt  = _("The extension of resulting frames is also used to define the fileformat. "
                        "Please note that fileformats differ in capabilities to store information for "
                        "multiple layers and other things. "
                        "Some fileformats may require converting to another imagetype "
                        "and/or flattening the frames.");
  argv[4].text_buf_len = len_ext;
  argv[4].text_buf_ret = extension;


  gap_arr_arg_init(&argv[5], GAP_ARR_WGT_OPTIONMENU);
  argv[5].label_txt = _("Imagetype:");
  argv[5].help_txt  = _("Convert to another imagetype, or keep imagetype as it is. "
                        "Most fileformats can't handle all types and may require a conversion. "
                        "Example: GIF can not handle RGB and requires convert to indexed imagetype.");
  argv[5].radio_argc  = 4;
  argv[5].radio_argv = radio_args;
  argv[5].radio_ret  = 0;

  gap_arr_arg_init(&argv[6], GAP_ARR_WGT_RADIO);
  argv[6].label_txt = _("Merge Layers:");
  argv[6].help_txt  = _("Flatten all resulting frames. Most fileformats can not handle multiple layers "
                        "and need flattened frames (flattening does melt down all layers to one composite layer)."
                        "Example: JPEG can not handle multiple layers and requires flattened frames.");
  argv[6].radio_argc  = 3;
  argv[6].radio_argv = radio_flatten_modes;
  argv[6].radio_help_argv = radio_flatten_modes_help;
  argv[6].radio_ret  = 1;

  gap_arr_arg_init(&argv[7], GAP_ARR_WGT_HELP_BUTTON);
  argv[7].help_id = GAP_HELP_ID_CONVERT;

  if(0 != gap_lib_chk_framerange(ainfo_ptr))   return -1;

  if(TRUE == gap_arr_ok_cancel_dialog( _("Convert Frames to other Formats"),
                                 _("Convert Settings"),
                                  8, argv))
  {
      *range_from  = (long)(argv[0].int_ret);
      *range_to    = (long)(argv[1].int_ret);
      switch(argv[5].radio_ret)
      {
        case 1:
           *dest_type = GIMP_RGB;
           break;
        case 2:
           *dest_type = GIMP_GRAY;
           break;
        case 3:
           *dest_type = GIMP_INDEXED;
           break;
        default:
          *dest_type = 9444;   /*  huh ??  */
           break;
      }
      *flatten     = (long)(argv[6].radio_ret);

      *dest_colors = 255;
      *dest_dither = 0;
      *palette_type = 2; /* WEB palette */
      *alpha_dither = 0;
      *remove_unused = 0;

       if(*dest_type == GIMP_INDEXED)
       {
          /* Open a 2.nd dialog for the Dither Options */
          if(0 != p_convert_indexed_dialog(dest_colors,
                                           dest_dither,
                                           palette_type,
                                           alpha_dither,
                                           remove_unused,
                                           palette,
                                           len_palette
            ))
          {
             return -1;
          }
       }

       if(0 != gap_lib_chk_framechange(ainfo_ptr))
       {
           return -1;
       }
       return 0;
  }
  else
  {
     return -1;
  }
}               /* end p_convert_dialog */

/* ============================================================================
 * p_range_to_multilayer_dialog
 *   dialog window with 4 entry fields
 *   where the user can select a frame range (FROM TO)
 * return -1  in case of cancel or any error
 *            (include check for change of current frame)
 * return positve (0 or layerstack position) if everythig OK
 * ============================================================================
 */
static int
p_range_to_multilayer_dialog(GapAnimInfo *ainfo_ptr,
               long *range_from, long *range_to,
               long *flatten_mode, long *bg_visible,
               long *framrate, char *frame_basename, gint len_frame_basename,
               char *title, char *hline,

               gint32 *layersel_mode, gint32 *layersel_case,
               gint32 *sel_invert, char *sel_pattern,
               gint32 *selection_mode)
{
  static GapArrArg  argv[13];
  int argc;

  static char *radio_args[4] = { N_("Expand as necessary"),
                                 N_("Clipped to image"),
                                 N_("Clipped to bottom layer"),
                                 N_("Flattened image") };
  static char *radio_help[4] = { N_("Resulting layer size is made of the outline-rectangle of all visible layers (may differ from frame to frame)"),
                                 N_("Resulting layer size is the frame size"),
                                 N_("Resulting layer size is the size of the bottom layer (may differ from frame to frame)"),
                                 N_("Resulting layer size is the frame size and transparent parts are filled with the background color") };

  /* Layer select modes */
  static char *layersel_args[7] = { N_("Pattern is equal to layer name"),
                                  N_("Pattern is start of layer name"),
                                  N_("Pattern is end of layer name"),
                                  N_("Pattern is a part of layer name"),
                                  N_("Pattern is a list of layerstack numbers"),
                                  N_("Pattern is a list of reverse layerstack numbers"),
                                  N_("All visible (ignore pattern)")
                                  };
  static char *layersel_help[7] = { N_("Select all layers where layername is equal to pattern"),
                                  N_("Select all layers where layername starts with pattern"),
                                  N_("Select all layers where layername ends up with pattern"),
                                  N_("Select all layers where layername contains pattern"),
                                  N_("Select layerstack positions where 0 is the top layer.\nExample: 0, 4-5, 8"),
                                  N_("Select layerstack positions where 0 is the background layer.\nExample: 0, 4-5, 8"),
                                  N_("Select all visible layers")
                                  };

  /* Selection modes */
  static char *selection_args[3] = { N_("Ignore"),
                                  N_("Initial frame"),
                                  N_("Frame specific")
                                  };
  static char *selection_help[3] = { N_("Pick layers at full size. "
                                        "Ignore all pixel selections in all frames"),
                                     N_("Pick only the selected pixels. "
                                        "Use the selection from the invoking frame "
                                        "as fixed selection in all handled frames."),
                                     N_("Pick only the selected pixels. "
                                        "Use the individual selection "
                                        "as it is in each handled frame.")
                                  };

  static int gettextize_radio = 0, gettextize_layersel = 0, gettextize_sel = 0;
  for (;gettextize_radio < 4; gettextize_radio++) {
    radio_args[gettextize_radio] = gettext(radio_args[gettextize_radio]);
    radio_help[gettextize_radio] = gettext(radio_help[gettextize_radio]);
  }
  for (;gettextize_layersel < 4; gettextize_layersel++) {
    layersel_args[gettextize_layersel] = gettext(layersel_args[gettextize_layersel]);
    layersel_help[gettextize_layersel] = gettext(layersel_help[gettextize_layersel]);
  }

  for (;gettextize_sel < 4; gettextize_sel++) {
    layersel_args[gettextize_sel] = gettext(layersel_args[gettextize_sel]);
    layersel_help[gettextize_sel] = gettext(layersel_help[gettextize_sel]);
  }

  gap_arr_arg_init(&argv[0], GAP_ARR_WGT_INT_PAIR);
  argv[0].constraint = TRUE;
  argv[0].label_txt = _("From Frame:");
  argv[0].help_txt  = _("First handled frame");
  argv[0].int_min   = (gint)ainfo_ptr->first_frame_nr;
  argv[0].int_max   = (gint)ainfo_ptr->last_frame_nr;
  argv[0].int_ret   = (gint)ainfo_ptr->curr_frame_nr;

  gap_arr_arg_init(&argv[1], GAP_ARR_WGT_INT_PAIR);
  argv[1].constraint = TRUE;
  argv[1].label_txt = _("To Frame:");
  argv[1].help_txt  = _("Last handled frame");
  argv[1].int_min   = (gint)ainfo_ptr->first_frame_nr;
  argv[1].int_max   = (gint)ainfo_ptr->last_frame_nr;
  argv[1].int_ret   = (gint)ainfo_ptr->last_frame_nr;

  gap_arr_arg_init(&argv[2], GAP_ARR_WGT_TEXT);
  argv[2].label_txt = _("Layer Basename:");
  argv[2].help_txt  = _("Basename for all layers where the string '[######]' is replaced by the frame number");
  argv[2].text_buf_len = len_frame_basename;
  argv[2].text_buf_ret = frame_basename;

  /* Framerate is not used any longer */
/*
  gap_arr_arg_init(&argv[3], GAP_ARR_WGT_INT_PAIR);
  argv[3].constraint = FALSE;
  argv[3].label_txt = "Framerate :";
  argv[3].help_txt  = "Framedelay in ms";
  argv[3].int_min   = (gint)0;
  argv[3].int_max   = (gint)300;
  argv[3].int_ret   = (gint)50;
 */
  gap_arr_arg_init(&argv[3], GAP_ARR_WGT_LABEL);
  argv[3].label_txt = " ";

  gap_arr_arg_init(&argv[4], GAP_ARR_WGT_RADIO);
  argv[4].label_txt = _("Layer Mergemode:");
  argv[4].radio_argc = 4;
  argv[4].radio_argv = radio_args;
  argv[4].radio_help_argv = radio_help;
  argv[4].radio_ret  = 1;

  gap_arr_arg_init(&argv[5], GAP_ARR_WGT_TOGGLE);
  argv[5].label_txt = _("Exclude BG-Layer:");
  argv[5].help_txt  = _("Exclude the background layer in all handled frames, "
                        "regardless of the other settings of layer selection.");
  argv[5].int_ret   = 0;   /* 1: exclude BG Layer from all selections */


  /* Layer select mode RADIO buttons */
  gap_arr_arg_init(&argv[6], GAP_ARR_WGT_RADIO);
  argv[6].label_txt = _("Layer Selection:");
  argv[6].radio_argc = 7;
  argv[6].radio_argv = layersel_args;
  argv[6].radio_help_argv = layersel_help;
  argv[6].radio_ret  = 6;

  /* Layer select pattern string */
  g_snprintf (sel_pattern, 2, "0");
  gap_arr_arg_init(&argv[7], GAP_ARR_WGT_TEXT);
  argv[7].label_txt = _("Layer Pattern:");
  argv[7].entry_width = 140;       /* pixel */
  argv[7].help_txt  = _("String to identify layer(s) by name or by layerstack position numbers. "
                        "Example: 0,3-5");
  argv[7].text_buf_len = MAX_LAYERNAME;
  argv[7].text_buf_ret = sel_pattern;

  /* case sensitive checkbutton */
  gap_arr_arg_init(&argv[8], GAP_ARR_WGT_TOGGLE);
  argv[8].label_txt = _("Case sensitive:");
  argv[8].help_txt  = _("Lowercase and uppercase letters are considered as different");
  argv[8].int_ret   = 1;

  /* invert selection checkbutton */
  gap_arr_arg_init(&argv[9], GAP_ARR_WGT_TOGGLE);
  argv[9].label_txt = _("Invert Layer Selection:");
  argv[9].help_txt  = _("Use all unselected layers");
  argv[9].int_ret   = 0;


  /* selection mode */
  gap_arr_arg_init(&argv[10], GAP_ARR_WGT_RADIO);
  argv[10].label_txt = _("Pixel Selection:");
  argv[10].radio_argc = 3;
  argv[10].radio_argv = selection_args;
  argv[10].radio_help_argv = selection_help;
  argv[10].radio_ret  = GAP_RANGE_OPS_SEL_IGNORE;


  gap_arr_arg_init(&argv[11], GAP_ARR_WGT_HELP_BUTTON);
  argv[11].help_id = GAP_HELP_ID_TO_MULTILAYER;

  argc = 12;
  if (gimp_image_base_type(ainfo_ptr->image_id) == GIMP_INDEXED)
  {
    gap_arr_arg_init(&argv[12], GAP_ARR_WGT_LABEL);
    argv[12].label_txt = _("You are using INDEXED frames. Please note that the result will be an RGB image");
    argc = 13;
  }

  if(0 != gap_lib_chk_framerange(ainfo_ptr))   return -1;

  if(TRUE == gap_arr_ok_cancel_dialog(title, hline, argc, argv))
  {   *range_from   = (long)(argv[0].int_ret);
      *range_to     = (long)(argv[1].int_ret);
      *framrate     = (long)(argv[3].int_ret);
      *flatten_mode = (long)(argv[4].int_ret);
      if (argv[5].int_ret == 0)  *bg_visible = 1; /* 1: use BG like any Layer */
      else                       *bg_visible = 0; /* 0: exclude (ignore) BG Layer */

      *layersel_mode     = argv[6].int_ret;
                        /*     [7] sel_pattern  */
      *layersel_case     = argv[8].int_ret;
      *sel_invert        = argv[9].int_ret;
      *selection_mode    = argv[10].int_ret;

       if(0 != gap_lib_chk_framechange(ainfo_ptr))
       {
           return -1;
       }
       return 0;
  }
  else
  {
     return -1;
  }

}       /* end p_range_to_multilayer_dialog */


/* ============================================================================
 * p_frames_to_multilayer
 * returns   image_id of the new created multilayer image
 *           (or -1 on error)
 * ============================================================================
 */
static gint32
p_frames_to_multilayer(GapAnimInfo *ainfo_ptr,
                      long range_from, long range_to,
                      long flatten_mode, long bg_visible,
                      long framerate, char *frame_basename,
                      gint32 layersel_mode, gint32 layersel_case,
                      gint32 sel_invert, char *sel_pattern,
                      gint32 selection_mode)
{
  GimpImageBaseType l_type;
  guint   l_width, l_height;
  long    l_cur_frame_nr;
  long    l_step, l_begin, l_end;
  long    l_vidx;
  gint32  l_tmp_image_id;
  gint32  l_new_image_id;
  gint32  l_cp_layer_id;
  gint32  l_tmp_layer_id;
  gint    l_src_offset_x, l_src_offset_y;    /* layeroffsets as they were in src_image */
  gint       l_nlayers;
  gint32    *l_layers_list;
  gint       l_visible;
  gint       l_nvisible;
  gint       l_nlayers_result;
  gdouble    l_percentage, l_percentage_step;
  static  char l_layername[256];
  GapModLayliElem *l_layli_ptr;
  gint32     l_sel_cnt;
  gboolean   l_clear_selected_area;
  gint32 calling_image_id;
  gint32 calling_frame_nr;
  gdouble    l_xresoulution, l_yresoulution;
  gint32     l_unit;
  gboolean   l_frame_found;

  calling_image_id = ainfo_ptr->image_id;
  calling_frame_nr = ainfo_ptr->curr_frame_nr;

  l_percentage = 0.0;
  l_nlayers_result = 0;
  if(ainfo_ptr->run_mode == GIMP_RUN_INTERACTIVE)
  {
    gimp_progress_init( _("Creating layer-animated image..."));
  }

  l_tmp_layer_id = -1;

  /* get info about the image (size type and resolution is common to all frames) */
  l_width  = gimp_image_width(ainfo_ptr->image_id);
  l_height = gimp_image_height(ainfo_ptr->image_id);
  l_type   = gimp_image_base_type(ainfo_ptr->image_id);
  l_unit   = gimp_image_get_unit(ainfo_ptr->image_id);
  gimp_image_get_resolution(ainfo_ptr->image_id, &l_xresoulution, &l_yresoulution);
  
 
  if (l_type == GIMP_INDEXED)
  {
    l_type = GIMP_RGB;
  }

  l_new_image_id = gimp_image_new(l_width, l_height,l_type);
  gimp_image_set_resolution(l_new_image_id, l_xresoulution, l_yresoulution);
  gimp_image_set_unit(l_new_image_id, l_unit);
  
  l_visible = TRUE;   /* only the 1.st layer should be visible */

  l_clear_selected_area = FALSE;

  l_begin = range_from;
  l_end   = range_to;

  if(range_from > range_to)
  {
    l_step  = -1;     /* operate in descending (reverse) order */
    l_percentage_step = 1.0 / ((1.0 + range_from) - range_to);

    if(range_to < ainfo_ptr->first_frame_nr)
    { l_begin = ainfo_ptr->first_frame_nr;
    }
    if(range_from > ainfo_ptr->last_frame_nr)
    { l_end = ainfo_ptr->last_frame_nr;
    }
  }
  else
  {
    l_step  = 1;      /* operate in ascending order */
    l_percentage_step = 1.0 / ((1.0 + range_to) - range_from);

    if(range_from < ainfo_ptr->first_frame_nr)
    { l_begin = ainfo_ptr->first_frame_nr;
    }
    if(range_to > ainfo_ptr->last_frame_nr)
    { l_end = ainfo_ptr->last_frame_nr;
    }
  }


  if(selection_mode == GAP_RANGE_OPS_SEL_INITIAL)
  {
    if(!gimp_selection_is_empty(ainfo_ptr->image_id))
    {
      gint32 l_initial_selection_channel_id;
      gint32 l_new_selection_channel_id;

      gimp_selection_all(l_new_image_id);
      l_new_selection_channel_id = gimp_image_get_selection(l_new_image_id);

      /* get the initial selection from the image
       * (from where this plugin was invoked)
       */
      l_initial_selection_channel_id = gimp_image_get_selection(ainfo_ptr->image_id);

      /* copy the initial selection to the newly create image */
      gap_layer_copy_content(l_new_selection_channel_id  /* dst_drawable_id  */
                            , l_initial_selection_channel_id   /* src_drawable_id  */
                            );
      /* invert the (copied initial selection) in the newly create image */
      gimp_selection_invert(l_new_image_id);

      l_clear_selected_area = TRUE;
    }
  }


  l_cur_frame_nr = l_begin;
  l_frame_found = TRUE;
  while(l_frame_found == TRUE)
  {
    /* build the frame name */
    if(ainfo_ptr->new_filename != NULL) g_free(ainfo_ptr->new_filename);
    ainfo_ptr->new_filename = gap_lib_alloc_fname(ainfo_ptr->basename,
                                        l_cur_frame_nr,
                                        ainfo_ptr->extension);
    if(ainfo_ptr->new_filename == NULL)
       goto error;

    if(l_cur_frame_nr == calling_frame_nr)
    {
      /* for the current image use duplicate */
      l_tmp_image_id = gimp_image_duplicate(calling_image_id);
    }
    else
    {
      if (g_file_test(ainfo_ptr->new_filename, G_FILE_TEST_EXISTS))
      {
        /* load current frame into temporary image */
        l_tmp_image_id = gap_lib_load_image(ainfo_ptr->new_filename);
      }
      else
      {
         goto frames_to_multilayer_advance_to_next_frame;
      }
    }
    if(l_tmp_image_id < 0)
       goto error;

    gimp_image_undo_disable(l_tmp_image_id);
    if (gimp_image_base_type(l_tmp_image_id) == GIMP_INDEXED)
    {
      /* INDEXED frame images are converted to RGB
       * - the resulting multilayer image has no loss in quality
       *   (regardless if the frames have different palettes.)
       * - some of the following processing steps would not
       *   work with INDEXED images.
       */
      gimp_image_convert_rgb(l_tmp_image_id);
    }

    /* get informations (id, visible, selected) about all layers */
    l_layli_ptr = gap_mod_alloc_layli(l_tmp_image_id, &l_sel_cnt, &l_nlayers,
                               layersel_mode, layersel_case, sel_invert, sel_pattern);
    if(l_layli_ptr == NULL)
        goto error;

    l_nvisible  = l_sel_cnt;  /* count visible Layers == all selected layers */
    for(l_vidx=0; l_vidx < l_nlayers; l_vidx++)
    {
      /* set all selected layers visible, all others invisible */
      l_tmp_layer_id = l_layli_ptr[l_vidx].layer_id;
      gimp_item_set_visible(l_tmp_layer_id,
                                l_layli_ptr[l_vidx].selected);

      if((bg_visible == 0) && (l_vidx == (l_nlayers -1)))
      {
         /* set BG_Layer invisible */
         gimp_item_set_visible(l_tmp_layer_id, FALSE);
         if(l_layli_ptr[l_vidx].selected)
         {
           l_nvisible--;
         }
      }
    }


    g_free(l_layli_ptr);

    if((flatten_mode >= GAP_RANGE_OPS_FLAM_MERG_EXPAND) && (flatten_mode <= GAP_RANGE_OPS_FLAM_MERG_CLIP_BG))
    {
       if(gap_debug) fprintf(stderr, "p_frames_to_multilayer: %d MERGE visible layers=%d\n", (int)flatten_mode, (int)l_nvisible);

       /* merge all visible Layers */
       if(l_nvisible > 1)  gimp_image_merge_visible_layers  (l_tmp_image_id, flatten_mode);
    }
    else
    {
        if(gap_debug) fprintf(stderr, "p_frames_to_multilayer: %d FLATTEN\n", (int)flatten_mode);
        /* flatten temporary image (reduce to single layer) */
        gimp_image_flatten (l_tmp_image_id);
    }


    /* copy (the only visible) layer from temporary image */
    l_layers_list = gimp_image_get_layers(l_tmp_image_id, &l_nlayers);
    if(l_layers_list != NULL)
    {
      for(l_vidx=0; l_vidx < l_nlayers; l_vidx++)
      {
        l_tmp_layer_id = l_layers_list[l_vidx];

        /* stop at 1.st visible layer (this should be the only visible layer) */
        if(gimp_item_get_visible(l_tmp_layer_id)) break;

        /* stop at 1.st layer if image was flattened */
        if((flatten_mode < GAP_RANGE_OPS_FLAM_MERG_EXPAND) || (flatten_mode > GAP_RANGE_OPS_FLAM_MERG_CLIP_BG))  break;
      }
      g_free (l_layers_list);


      if(selection_mode == GAP_RANGE_OPS_SEL_FRAME_SPECIFIC)
      {
        if(!gimp_selection_is_empty(l_tmp_image_id))
        {
          gimp_selection_invert(l_tmp_image_id);
          gimp_edit_clear(l_tmp_layer_id);
        }
      }


      if(l_vidx < l_nlayers)
      {
         l_cp_layer_id = gap_layer_copy_to_dest_image(l_new_image_id,
                                     l_tmp_layer_id,
                                     100.0,   /* Opacity */
                                     0,       /* NORMAL */
                                     &l_src_offset_x,
                                     &l_src_offset_y);

        /* add the copied layer to current destination image */
        gimp_image_insert_layer(l_new_image_id, l_cp_layer_id, 0, 0);
        gimp_layer_set_offsets(l_cp_layer_id, l_src_offset_x, l_src_offset_y);

        if(l_clear_selected_area)
        {
          gimp_edit_clear(l_cp_layer_id);
        }

        l_nlayers_result++;


        /* add aplha channel to all layers
         * (without alpha raise and lower would not work on that layers)
         */
        gimp_layer_add_alpha(l_cp_layer_id);

        /* set name and visibility */
        if (frame_basename == NULL)  frame_basename = "frame_[######]";
        if (*frame_basename == '\0') frame_basename = "frame_[######]";

        gap_match_substitute_framenr(&l_layername[0], sizeof(l_layername),
                              frame_basename, (long)l_cur_frame_nr);
        gimp_item_set_name(l_cp_layer_id, &l_layername[0]);

        gimp_item_set_visible(l_cp_layer_id, l_visible);
        l_visible = FALSE;   /* all further layers are set invisible */
      }
      /* else: tmp image has no visible layers, ignore that frame */
    }

    /* destroy the tmp image */
    gimp_image_delete(l_tmp_image_id);

frames_to_multilayer_advance_to_next_frame:
    if(ainfo_ptr->run_mode == GIMP_RUN_INTERACTIVE)
    {
      l_percentage += l_percentage_step;
      gimp_progress_update (l_percentage);
    }

    /* advance to next frame */
    if(l_cur_frame_nr == l_end)
    {
      break;
    }

     /* advance l_hi to the next available frame number 
      * (normally to l_cur_frame_nr += l_step;
      * sometimes to higher/lower number when frames are missing) 
      */
    l_cur_frame_nr = gap_lib_get_next_available_frame_number(l_cur_frame_nr, l_step
                           , ainfo_ptr->basename, ainfo_ptr->extension, &l_frame_found);
    if (l_frame_found != TRUE)
    {
      break;
    }
  }  /* end while */

  gap_image_prevent_empty_image(l_new_image_id);

  return l_new_image_id;

error:
  gimp_image_delete(l_new_image_id);
  return -1;
}       /* end p_frames_to_multilayer */



/* ============================================================================
 * gap_range_to_multilayer
 * ============================================================================
 */
gint32 gap_range_to_multilayer(GimpRunMode run_mode, gint32 image_id,
                             long range_from, long range_to,
                             long flatten_mode, long bg_visible,
                             long   framerate,
                             char  *frame_basename, int frame_basename_len,
                             gint32 layersel_mode, gint32 layersel_case,
                             gint32 sel_invert, char *sel_pattern,
                             gint32 selection_mode)
{
  gint32  new_image_id;
  gint32  l_rc;
  long   l_from, l_to;
  GapAnimInfo *ainfo_ptr;
  GapVinVideoInfo *vin_ptr;
  gint32    l_layersel_mode;
  gint32    l_selection_mode;
  gint32    l_layersel_case;
  gint32    l_sel_invert;
  gdouble   l_framerate;

  char      l_sel_pattern[MAX_LAYERNAME];

  l_rc = -1;
  ainfo_ptr = gap_lib_alloc_ainfo(image_id, run_mode);
  if(ainfo_ptr != NULL)
  {
    if (0 == gap_lib_dir_ainfo(ainfo_ptr))
    {
      if(run_mode == GIMP_RUN_INTERACTIVE)
      {
         l_framerate = 24.0;
         vin_ptr = gap_vin_get_all(ainfo_ptr->basename);
         if(vin_ptr)
         {
           if(vin_ptr->framerate > 0) l_framerate = vin_ptr->framerate;
           g_free(vin_ptr);
         }
         g_snprintf(frame_basename, frame_basename_len, "frame_[######] (%dms)", (int)(1000/l_framerate));
         framerate = 0;
         l_layersel_case    = layersel_case;
         l_sel_invert       = sel_invert;
         l_rc = p_range_to_multilayer_dialog (ainfo_ptr, &l_from, &l_to,
                                &flatten_mode, &bg_visible,
                                &framerate, frame_basename, frame_basename_len,
                                _("Frames to Image"),
                                _("Create Multilayer-Image from Frames"),
                                &l_layersel_mode, &l_layersel_case,
                                &l_sel_invert, &l_sel_pattern[0],
                                &l_selection_mode
                                );
      }
      else
      {
         l_from = range_from;
         l_to   = range_to;
         l_rc = 0;
         l_layersel_mode    = layersel_mode;
         l_layersel_case    = layersel_case;
         l_sel_invert       = sel_invert;
         l_selection_mode   = selection_mode;

         strncpy(&l_sel_pattern[0], sel_pattern, sizeof(l_sel_pattern) -1);
         l_sel_pattern[sizeof(l_sel_pattern) -1] = '\0';
      }

      if(l_rc >= 0)
      {
         new_image_id = p_frames_to_multilayer(ainfo_ptr, l_from, l_to,
                                               flatten_mode, bg_visible,
                                               framerate, frame_basename,
                                               l_layersel_mode, l_layersel_case,
                                               l_sel_invert, &l_sel_pattern[0],
                                               l_selection_mode);
         gimp_display_new(new_image_id);
         l_rc = new_image_id;
      }
    }
    gap_lib_free_ainfo(&ainfo_ptr);
  }

  return(l_rc);
}       /* end gap_range_to_multilayer */

/* ============================================================================
 * p_type_convert
 *   convert image to desired type (reduce to dest_colors for INDEXED type)
 * ============================================================================
 */
static int
p_type_convert(gint32 image_id, GimpImageBaseType dest_type, gint32 dest_colors, gint32 dest_dither,
               gint32 palette_type, gint32  alpha_dither, gint32  remove_unused,  char *palette)
{
  gboolean  l_rc;
  gboolean  l_alpha_dither;
  gboolean  l_remove_unused;
  GimpConvertDitherType     l_dither_type;
  GimpConvertPaletteType    l_palette_type;

  l_rc = TRUE;

  switch(dest_type)
  {
    case GIMP_INDEXED:
      if(gap_debug) fprintf(stderr, "DEBUG: p_type_convert to INDEXED ncolors=%d, palette_type=%d palette_name=%s'\n",
                                   (int)dest_colors, (int)palette_type, palette);

      switch(dest_dither)
      {
         case 1:   l_dither_type = GIMP_FS_DITHER; break;
         case 2:   l_dither_type = GIMP_FSLOWBLEED_DITHER; break;
         case 3:   l_dither_type =  GIMP_FIXED_DITHER; break;
         default:  l_dither_type = GIMP_NO_DITHER; break;
      }
      switch(palette_type)
      {
         case 1:   l_palette_type = GIMP_REUSE_PALETTE; break;
         case 2:   l_palette_type = GIMP_WEB_PALETTE; break;
         case 3:   l_palette_type = GIMP_MONO_PALETTE; break;
         case 4:   l_palette_type = GIMP_CUSTOM_PALETTE; break;
         default:  l_palette_type = GIMP_MAKE_PALETTE; break;
      }
      l_alpha_dither = (alpha_dither != 0);
      l_remove_unused = (remove_unused != 0);

      l_rc  = gimp_image_convert_indexed(image_id,
                                         l_dither_type,
                                         l_palette_type,   /* value 0: MAKE_PALETTE, 2: WEB_PALETTE 4:CUSTOM_PALETTE */
                                         dest_colors,
                                         l_alpha_dither,
                                         l_remove_unused,
                                         palette          /* name of custom palette */
                                         );
      break;
    case GIMP_GRAY:
      if(gap_debug) fprintf(stderr, "DEBUG: p_type_convert to GRAY'\n");
      l_rc = gimp_image_convert_grayscale(image_id);
      break;
    case GIMP_RGB:
      if(gap_debug) fprintf(stderr, "DEBUG: p_type_convert to RGB'\n");
      l_rc = gimp_image_convert_rgb(image_id);
      break;
    default:
      if(gap_debug) fprintf(stderr, "DEBUG: p_type_convert AS_IT_IS (dont convert)'\n");
      return 0;
      break;
  }


  if (l_rc) return 0;
  return -1;
}       /* end p_type_convert */

/* ============================================================================
 * p_frames_convert
 *    convert frames (multiple images) into desired fileformat and type
 *    (flatten the images if desired)
 *
 *   if save_proc_name == NULL
 *   then   use xcf save (and flatten image)
 *          and new_basename and new_extension
 *   else
 *          save in specified fileformat
 *          and return image_id of the frame with lowest frame number
 *          (all other frames were deleted after successful save)
 *
 * returns   value >= 0 if all is ok
 *           (or -1 on error)
 * ============================================================================
 */
static gint32
p_frames_convert(GapAnimInfo *ainfo_ptr,
                 long range_from, long range_to,
                 char *save_proc_name, char *new_basename, char *new_extension,
                 int flatten,
                 GimpImageBaseType dest_type, gint32 dest_colors, gint32 dest_dither,
                 gint32  palette_type, gint32  alpha_dither, gint32  remove_unused,  char   *palette)
{
  GimpRunMode l_run_mode;
  gint32  l_tmp_image_id;
  gint32  l_start_image_id;
  long    l_cur_frame_nr;
  long    l_step, l_begin, l_end;
  gint    l_nlayers;
  gint    l_img_already_flat;
  gint32 *l_layers_list;
  gdouble l_percentage, l_percentage_step;
  char   *l_sav_name;
  gint32  l_rc;
  gint    l_overwrite_mode;
  static  GapArrButtonArg  l_argv[3];


  l_rc = 0;
  l_overwrite_mode = 0;
  l_percentage = 0.0;
  l_start_image_id = -1;
  l_run_mode  = ainfo_ptr->run_mode;
  if(ainfo_ptr->run_mode == GIMP_RUN_INTERACTIVE)
  {
    if(save_proc_name == NULL) gimp_progress_init( _("Flattening frames..."));
    else                       gimp_progress_init( _("Converting frames..."));
  }


  l_begin = range_from;
  l_end   = range_to;

  if(range_from > range_to)
  {
    l_step  = -1;     /* operate in descending (reverse) order */
    l_percentage_step = 1.0 / ((1.0 + range_from) - range_to);

    if(range_to < ainfo_ptr->first_frame_nr)
    { l_begin = ainfo_ptr->first_frame_nr;
    }
    if(range_from > ainfo_ptr->last_frame_nr)
    { l_end = ainfo_ptr->last_frame_nr;
    }
  }
  else
  {
    l_step  = 1;      /* operate in ascending order */
    l_percentage_step = 1.0 / ((1.0 + range_to) - range_from);

    if(range_from < ainfo_ptr->first_frame_nr)
    { l_begin = ainfo_ptr->first_frame_nr;
    }
    if(range_to > ainfo_ptr->last_frame_nr)
    { l_end = ainfo_ptr->last_frame_nr;
    }
  }


  l_cur_frame_nr = l_begin;
  while(l_rc >= 0)
  {
    /* build the frame name */
    if(ainfo_ptr->new_filename != NULL)
    {
      g_free(ainfo_ptr->new_filename);
    }
    
    ainfo_ptr->new_filename = gap_lib_alloc_fname(ainfo_ptr->basename,
                                        l_cur_frame_nr,
                                        ainfo_ptr->extension);
    if(ainfo_ptr->new_filename == NULL)
    {
       goto check_end_or_advance_to_next_frame;
    }

    if(1 != gap_lib_file_exists(ainfo_ptr->new_filename)) 
    {
       goto check_end_or_advance_to_next_frame;
    }
    
    /* load current frame */
    l_tmp_image_id = gap_lib_load_image(ainfo_ptr->new_filename);
    if(l_tmp_image_id < 0)
    {
       return -1;
    }

    l_img_already_flat = FALSE; /* an image without any layer is considered as not flattend */
    l_layers_list = gimp_image_get_layers(l_tmp_image_id, &l_nlayers);
    if(l_layers_list != NULL)
    {
      if( (l_nlayers == 1)
      &&  (! gimp_drawable_has_alpha(l_layers_list[0]))
      &&  (! gimp_item_get_visible(l_layers_list[0])))
      {
        l_img_already_flat = TRUE;
      }
      g_free (l_layers_list);
    }

    if((l_img_already_flat == FALSE) &&  (flatten != 0))
    {
       gint32 l_dummy_layer_id;
       if(gap_debug) fprintf(stderr, "DEBUG: p_frames_convert flatten tmp image'\n");

        /* hof:
         * we add dummy layers to make sure that flatten works on any kind of image.
         * even if the image had no layer at all, or all its layers were invisible.
         *   (flatten need at least 2 layers and at least one visible layer to work.
         *    if just invisible layers are flattened
         *    we do not get a resulting layer (returned l_layer_id == -1)
         */
        l_dummy_layer_id = gimp_layer_new(l_tmp_image_id, "dummy",
                                 1,
                                 1,
                                 ((gint)(gimp_image_base_type(l_tmp_image_id)) * 2),
                                 100.0,     /* Opacity full opaque */
                                 0);        /* NORMAL */
         gimp_image_insert_layer(l_tmp_image_id, l_dummy_layer_id, 0, 0);
         gimp_layer_set_offsets(l_dummy_layer_id, -1, -1);

         if(l_nlayers == 0)
         {
           /* on empty images we need 2 dummies to make flatten happy */
           l_dummy_layer_id = gimp_layer_new(l_tmp_image_id, "dummy2",
                                   1,
                                   1,
                                   ((gint)(gimp_image_base_type(l_tmp_image_id)) * 2),
                                   100.0,     /* Opacity full opaque */
                                   0);        /* NORMAL */
           gimp_image_insert_layer(l_tmp_image_id, l_dummy_layer_id, 0, 0);
           gimp_layer_set_offsets(l_dummy_layer_id, -1, -1);
         }


       /* flatten current frame image (reduce to single layer) */
       if (flatten == FLATTEN_MODE_MERGE_VISIBLE)
       {
         gimp_image_merge_visible_layers (l_tmp_image_id, GIMP_CLIP_TO_IMAGE);
         /* remove the remaining invisible layers because saving to
          * imageformats that can not handle multiple layers would
          * trigger the gimp export dialog (that is not desired
          * for processing multiple frames) on attempt to save
          * an image with more than 1 layer.
          */
         gap_image_remove_invisble_layers(l_tmp_image_id);
       }
       else
       {
         gimp_image_flatten (l_tmp_image_id);
       }

       /* save back the current frame with same name */
       if(save_proc_name == NULL)
       {
          l_rc = gap_lib_save_named_frame(l_tmp_image_id, ainfo_ptr->new_filename);
       }
    }

    if(save_proc_name != NULL)
    {
       if(dest_type != gimp_image_base_type(l_tmp_image_id))
       {
          /* have to convert to desired type (RGB, INDEXED, GRAYSCALE) */
          p_type_convert(l_tmp_image_id, dest_type, dest_colors, dest_dither,
                         palette_type, alpha_dither, remove_unused, palette);
       }


       /* build the name for output image */
       l_sav_name = gap_lib_alloc_fname(new_basename,
                                  l_cur_frame_nr,
                                  new_extension);
       if(l_sav_name != NULL)
       {
          if(1 == gap_lib_file_exists(l_sav_name))
          {
            if (l_overwrite_mode < 1)
            {
               l_argv[0].but_txt  = _("Overwrite Frame");
               l_argv[0].but_val  = 0;
               l_argv[1].but_txt  = _("Overwrite All");
               l_argv[1].but_val  = 1;
               l_argv[2].but_txt  = GTK_STOCK_CANCEL;
               l_argv[2].but_val  = -1;

               l_overwrite_mode =  gap_arr_buttons_dialog  ( _("GAP Question"), l_sav_name, 3, l_argv, -1);
            }

          }

          gimp_image_set_filename(l_tmp_image_id, l_sav_name);

          if(l_cur_frame_nr == MIN(range_from, range_to))
          {
            l_start_image_id = l_tmp_image_id;
          }

          if(l_overwrite_mode < 0)  { l_rc = -1; }
          else
          {
            /* save with selected save procedure
             * (regardless if image was flattened or not)
             */
             l_rc = gap_lib_save_named_image(l_tmp_image_id, l_sav_name, l_run_mode);
             if(l_rc < 0)
             {
               gap_arr_msg_win(ainfo_ptr->run_mode, _("Convert Frames: Save operation failed.\n"
                                                "Desired save plugin can't handle type\n"
                                                "or desired save plugin not available."));
             }
          }
          if(l_run_mode == GIMP_RUN_INTERACTIVE)
          {
            l_run_mode  = GIMP_RUN_WITH_LAST_VALS;  /* for all further calls */
          }
          g_free(l_sav_name);
       }
    }

    if(l_start_image_id != l_tmp_image_id)
    {
      /* destroy the tmp image */
      gimp_image_delete(l_tmp_image_id);
    }

    if(ainfo_ptr->run_mode == GIMP_RUN_INTERACTIVE)
    {
      l_percentage += l_percentage_step;
      gimp_progress_update (l_percentage);
    }


    /* break on error */
    if(l_rc < 0)
    {
      break;
    }

check_end_or_advance_to_next_frame:

    /* break at last handled frame */
    if(l_cur_frame_nr == l_end)
    {
       if(save_proc_name == NULL)
       {
          l_rc = 0;
       }
       else
       {
         l_rc = l_start_image_id;
       }
       break;
    }

    /* advance to next frame */
    l_cur_frame_nr = gap_lib_get_next_available_frame_number(l_cur_frame_nr
                      , l_step, ainfo_ptr->basename, ainfo_ptr->extension, NULL);
  }

  return l_rc;

}       /* end p_frames_convert */



/* ============================================================================
 * p_image_sizechange
 *     scale, resize or crop one image
 * ============================================================================
 */
static
int p_image_sizechange(gint32 image_id,
               GapRangeOpsAsiz asiz_mode,
               long size_x, long size_y,
               long offs_x, long offs_y
)
{
  gboolean  l_rc;

  if(gap_debug)
  {
    printf("p_image_sizechange: image_id: %d\n", (int)image_id);

    printf("size_x:%d  size_y: %d\n", (int)size_x , (int)size_y );
    printf("size_x:%d  size_y: %d\n", (int)size_x , (int)size_y );

    if(asiz_mode != GAP_ASIZ_SCALE)
    {
           printf("offs_x: %d\n", (int)offs_x);
           printf("offs_y: %d\n", (int)offs_y);
    }
  }

  switch(asiz_mode)
  {
    case GAP_ASIZ_CROP:
      l_rc = gimp_image_crop(image_id, size_x, size_y, offs_x, offs_y);
      break;
    case GAP_ASIZ_RESIZE:
      l_rc = gimp_image_resize(image_id, size_x, size_y, offs_x, offs_y);
      break;
    default:
      l_rc = gimp_image_scale(image_id, size_x, size_y);
      break;
  }

  if(l_rc) return 0;
  return -1;
}       /* end p_image_sizechange */

/* ============================================================================
 * p_anim_sizechange
 *     scale, resize or crop all frames in the animation
 * ============================================================================
 */
static gint32
p_anim_sizechange(GapAnimInfo *ainfo_ptr,
               GapRangeOpsAsiz asiz_mode,
               long size_x, long size_y,
               long offs_x, long offs_y
)
{
  long    l_cur_frame_nr;
  long    l_step, l_begin, l_end;
  gint32  l_tmp_image_id;
  gdouble    l_percentage, l_percentage_step;
  int         l_rc;
  gboolean    l_frame_found;

  l_rc = 0;
  l_percentage = 0.0;
  if(ainfo_ptr->run_mode == GIMP_RUN_INTERACTIVE)
  {
    switch(asiz_mode)
    {
      case GAP_ASIZ_CROP:
        gimp_progress_init( _("Cropping all video frames..."));
        break;
      case GAP_ASIZ_RESIZE:
        gimp_progress_init( _("Resizing all video frames..."));
        break;
      default:
        gimp_progress_init( _("Scaling all video frames..."));
        break;
    }
  }


  /* get info about the image (size and type is common to all frames) */
  l_begin = ainfo_ptr->first_frame_nr;
  l_end   = ainfo_ptr->last_frame_nr;

  l_step  = 1;      /* operate in ascending order */
  l_percentage_step = 1.0 / ((1.0 + l_end) - l_begin);


  l_cur_frame_nr = l_begin;

  l_frame_found = TRUE;
  while(l_frame_found == TRUE)
  {
    /* build the frame name */
    if(ainfo_ptr->new_filename != NULL) g_free(ainfo_ptr->new_filename);
    ainfo_ptr->new_filename = gap_lib_alloc_fname(ainfo_ptr->basename,
                                        l_cur_frame_nr,
                                        ainfo_ptr->extension);
    if(ainfo_ptr->new_filename == NULL)
    {
       return -1;
    }

    if (g_file_test(ainfo_ptr->new_filename, G_FILE_TEST_EXISTS))
    {
      /* load current frame into temporary image */
      l_tmp_image_id = gap_lib_load_image(ainfo_ptr->new_filename);
      if(l_tmp_image_id < 0)
      {
         return -1;
      }

      l_rc = p_image_sizechange(l_tmp_image_id, asiz_mode,
                              size_x, size_y, offs_x, offs_y);
      if(l_rc < 0)
      {
        break;
      }

      /* save back the current frame with same name */
      l_rc = gap_lib_save_named_frame(l_tmp_image_id, ainfo_ptr->new_filename);
      if(l_rc < 0)
      {
        break;
      }

      /* destroy the tmp image */
      gimp_image_delete(l_tmp_image_id);
    }


    if(ainfo_ptr->run_mode == GIMP_RUN_INTERACTIVE)
    {
      l_percentage += l_percentage_step;
      gimp_progress_update (l_percentage);
    }

    /* advance to next frame */
    if(l_cur_frame_nr == l_end)
    {
       break;
    }

    /* advance l_cur_frame_nr to the next available frame number 
     * (normally to l_cur_frame_nr += l_step; 
     * sometimes to higher number when frames are missing) 
     */
    l_cur_frame_nr = gap_lib_get_next_available_frame_number(l_cur_frame_nr, l_step
                           , ainfo_ptr->basename, ainfo_ptr->extension, &l_frame_found);

  }   /* end while loop over all frames*/

  return l_rc;
}       /* end  p_anim_sizechange */





/* ============================================================================
 * gap_range_flatten
 *
 * return image_id (of the new loaded current frame) on success
 *        or -1 on errors
 * ============================================================================
 */
gint32
gap_range_flatten(GimpRunMode run_mode, gint32 image_id,
                 long range_from, long range_to)
{
  int    l_rc;
  long   l_from, l_to;
  GapAnimInfo *ainfo_ptr;

  l_rc = -1;
  ainfo_ptr = gap_lib_alloc_ainfo(image_id, run_mode);
  if(ainfo_ptr != NULL)
  {
    if (0 == gap_lib_dir_ainfo(ainfo_ptr))
    {
      if(run_mode == GIMP_RUN_INTERACTIVE)
      {
         l_rc = p_range_dialog (ainfo_ptr, &l_from, &l_to,
                                _("Flatten Frames"),
                                _("Select Frame Range"), 2,
                                GAP_HELP_ID_FLATTEN);

      }
      else
      {
         l_rc = 0;
         l_from = range_from;
         l_to   = range_to;
      }

      if(l_rc >= 0)
      {
         if(gap_lib_gap_check_save_needed(ainfo_ptr->image_id))
         {
           l_rc = gap_lib_save_named_frame(ainfo_ptr->image_id, ainfo_ptr->old_filename);
         }
         if(l_rc >= 0)
         {
           l_rc = p_frames_convert(ainfo_ptr, l_from, l_to, NULL, NULL, NULL, 1, 0,0,0, 0,0,0, "");
           if(l_rc >= 0)
           {
             l_rc = gap_lib_load_named_frame(ainfo_ptr->image_id, ainfo_ptr->old_filename);
           }
         }
      }
    }
    gap_lib_free_ainfo(&ainfo_ptr);
  }

  if(l_rc < 0)
  {
    return -1;
  }
  return(l_rc);
}       /* end gap_range_flatten */




/* ============================================================================
 * p_frames_layer_del
 * returns   image_id of the new created multilayer image
 *           (or -1 on error)
 * ============================================================================
 */
static int
p_frames_layer_del(GapAnimInfo *ainfo_ptr,
                   long range_from, long range_to, long position)
{
  gint32  l_tmp_image_id;

  long    l_cur_frame_nr;
  long    l_step, l_begin, l_end;
  gint32  l_tmp_layer_id;
  gint       l_nlayers;
  gint32    *l_layers_list;
  gdouble    l_percentage, l_percentage_step;
  gchar     *l_buff;
  int        l_rc;
  gboolean   l_frame_found;


  l_rc = 0;
  l_percentage = 0.0;
  if(ainfo_ptr->run_mode == GIMP_RUN_INTERACTIVE)
  {
    l_buff = g_strdup_printf (_("Removing layer (pos:%ld) from frames..."), position);
    gimp_progress_init(l_buff);
    g_free (l_buff);
  }


  /* get info about the image (size and type is common to all frames) */

  l_begin = range_from;
  l_end   = range_to;

  if(range_from > range_to)
  {
    l_step  = -1;     /* operate in descending (reverse) order */
    l_percentage_step = 1.0 / ((1.0 + range_from) - range_to);

    if(range_to < ainfo_ptr->first_frame_nr)
    { l_begin = ainfo_ptr->first_frame_nr;
    }
    if(range_from > ainfo_ptr->last_frame_nr)
    { l_end = ainfo_ptr->last_frame_nr;
    }
  }
  else
  {
    l_step  = 1;      /* operate in ascending order */
    l_percentage_step = 1.0 / ((1.0 + range_to) - range_from);

    if(range_from < ainfo_ptr->first_frame_nr)
    { l_begin = ainfo_ptr->first_frame_nr;
    }
    if(range_to > ainfo_ptr->last_frame_nr)
    { l_end = ainfo_ptr->last_frame_nr;
    }
  }


  l_cur_frame_nr = l_begin;
  l_frame_found = TRUE;
  while(l_frame_found == TRUE)
  {
    /* build the frame name */
    if(ainfo_ptr->new_filename != NULL) g_free(ainfo_ptr->new_filename);
    ainfo_ptr->new_filename = gap_lib_alloc_fname(ainfo_ptr->basename,
                                        l_cur_frame_nr,
                                        ainfo_ptr->extension);
    if(ainfo_ptr->new_filename == NULL)
    {
       return -1;
    }
   
    if (g_file_test(ainfo_ptr->new_filename, G_FILE_TEST_EXISTS))
    {
      /* load current frame */
      l_tmp_image_id = gap_lib_load_image(ainfo_ptr->new_filename);
      if(l_tmp_image_id < 0)
      {
        return -1;
      }

      /* remove layer[position] */
      l_layers_list = gimp_image_get_layers(l_tmp_image_id, &l_nlayers);
      if(l_layers_list != NULL)
      {
        /* findout layer id of the requestetd position within layerstack */
        if(position < l_nlayers) l_tmp_layer_id = l_layers_list[position];
        else                     l_tmp_layer_id = l_layers_list[l_nlayers -1];

        g_free (l_layers_list);

        /* check for last layer (MUST NOT be deleted !) */
        if(l_nlayers > 1)
        {
          /* remove and delete requested layer */
          gimp_image_remove_layer(l_tmp_image_id, l_tmp_layer_id);

          /* save current frame */
          l_rc = gap_lib_save_named_frame(l_tmp_image_id, ainfo_ptr->new_filename);
        }
      }

      /* destroy the tmp image */
      gimp_image_delete(l_tmp_image_id);
    }
    
    if(ainfo_ptr->run_mode == GIMP_RUN_INTERACTIVE)
    {
      l_percentage += l_percentage_step;
      gimp_progress_update (l_percentage);
    }

    /* advance to next frame */
    if((l_cur_frame_nr == l_end) || (l_rc < 0))
    {
       break;
    }

    /* advance l_cur_frame_nr to the next available frame number 
     * (normally to l_cur_frame_nr += l_step; 
     * sometimes to higher number when frames are missing) 
     */
    l_cur_frame_nr = gap_lib_get_next_available_frame_number(l_cur_frame_nr, l_step
                           , ainfo_ptr->basename, ainfo_ptr->extension, &l_frame_found);
  }

  return l_rc;

}       /* end p_frames_layer_del */


/* ============================================================================
 * gap_range_layer_del
 *
 * return image_id (of the new loaded current frame) on success
 *        or -1 on errors
 * ============================================================================
 */
gint32
gap_range_layer_del(GimpRunMode run_mode, gint32 image_id,
                         long range_from, long range_to, long position)
{
  int    l_rc;
  long   l_position;
  long   l_from, l_to;
  GapAnimInfo *ainfo_ptr;

  l_rc = -1;
  l_position = 0;
  ainfo_ptr = gap_lib_alloc_ainfo(image_id, run_mode);
  if(ainfo_ptr != NULL)
  {
    if (0 == gap_lib_dir_ainfo(ainfo_ptr))
    {
      if(run_mode == GIMP_RUN_INTERACTIVE)
      {
         l_rc = p_range_dialog (ainfo_ptr, &l_from, &l_to,
                                _("Delete Layers in Frames"),
                                _("Select Frame Range & Stack Position"), 3,
                                GAP_HELP_ID_LAYER_DEL);
         l_position = l_rc;

      }
      else
      {
         l_rc = 0;
         l_from = range_from;
         l_to   = range_to;
         l_position = position;
      }

      if(l_rc >= 0)
      {
         if(gap_lib_gap_check_save_needed(ainfo_ptr->image_id))
         {
           l_rc = gap_lib_save_named_frame(ainfo_ptr->image_id, ainfo_ptr->old_filename);
         }
         if(l_rc >= 0)
         {
           l_rc = p_frames_layer_del(ainfo_ptr, l_from, l_to, l_position);
           if(l_rc >= 0)
           {
             l_rc = gap_lib_load_named_frame(ainfo_ptr->image_id, ainfo_ptr->old_filename);
           }
         }
      }
    }
    gap_lib_free_ainfo(&ainfo_ptr);
  }

  if(l_rc < 0)
  {
    return -1;
  }
  return(l_rc);
}       /* end gap_range_layer_del */


/* ============================================================================
 * gap_range_conv
 *   convert frame range to any gimp supported fileformat
 *
 * return image_id of the first (or last) of the converted frame images
 *        or -1 on errors
 * ============================================================================
 */
gint32
gap_range_conv(GimpRunMode run_mode, gint32 image_id,
                      long range_from, long range_to,
                      long       flatten,
                      GimpImageBaseType dest_type,
                      gint32     dest_colors,
                      gint32     dest_dither,
                      char      *basename,
                      char      *extension,
                      gint32     palette_type,
                      gint32     alpha_dither,
                      gint32     remove_unused,
                      char   *palette)
{
  gint32  l_rc;
  long   l_from, l_to;
  long   l_flatten;
  gint32     l_dest_colors;
  gint32     l_dest_dither;
  gint32     l_palette_type;
  gint32     l_alpha_dither;
  gint32     l_remove_unused;
  GimpImageBaseType l_dest_type;

  GapAnimInfo *ainfo_ptr;
  char  l_save_proc_name[128];
  char  l_basename[256];
  char *l_basename_ptr;
  long  l_number;
  char  l_extension[32];
  char  l_palette[256];

  strcpy(l_save_proc_name, "gimp_file_save");
  strcpy(l_extension, ".jpg");

  l_rc = -1;
  ainfo_ptr = gap_lib_alloc_ainfo(image_id, run_mode);
  if(ainfo_ptr != NULL)
  {
    if (0 == gap_lib_dir_ainfo(ainfo_ptr))
    {
      strncpy(l_basename, ainfo_ptr->basename, sizeof(l_basename) -1);
      l_basename[sizeof(l_basename) -1] = '\0';

      if(run_mode == GIMP_RUN_INTERACTIVE)
      {

         l_flatten = 1;
         /* p_convert_dialog : select destination type
          * to find out extension
          */
         strcpy(l_palette, "Default");
         l_rc = p_convert_dialog (ainfo_ptr, &l_from, &l_to, &l_flatten,
                                  &l_dest_type, &l_dest_colors, &l_dest_dither,
                                  &l_basename[0], sizeof(l_basename),
                                  &l_extension[0], sizeof(l_extension),
                                  &l_palette_type, &l_alpha_dither, &l_remove_unused,
                                  &l_palette[0], sizeof(l_palette));

      }
      else
      {
         l_rc = 0;
         l_from    = range_from;
         l_to      = range_to;
         l_flatten = flatten;
         l_dest_type   = dest_type;
         l_dest_colors = dest_colors;
         l_dest_dither = dest_dither;
         l_palette_type   = palette_type;
         l_alpha_dither   = alpha_dither;
         l_remove_unused  = remove_unused;
         if(basename != NULL)
         {
            strncpy(l_basename, basename, sizeof(l_basename) -1);
            l_basename[sizeof(l_basename) -1] = '\0';
         }
         if(palette != NULL)
         {
            strncpy(l_palette, palette, sizeof(l_palette) -1);
            l_palette[sizeof(l_palette) -1] = '\0';
         }
         strncpy(l_extension, extension, sizeof(l_extension) -1);
         l_extension[sizeof(l_extension) -1] = '\0';

      }

      if(l_rc >= 0)
      {
         /* cut off extension and trailing frame number */
         l_basename_ptr = gap_lib_alloc_basename(&l_basename[0], &l_number);
         if(l_basename_ptr == NULL)  { l_rc = -1; }
         else
         {
            l_rc = p_frames_convert(ainfo_ptr, l_from, l_to,
                                    l_save_proc_name,
                                    l_basename_ptr,
                                    l_extension,
                                    l_flatten,
                                    l_dest_type,
                                    l_dest_colors,
                                    l_dest_dither,
                                    l_palette_type,
                                    l_alpha_dither,
                                    l_remove_unused,
                                    l_palette);
            g_free(l_basename_ptr);
            if((l_rc >= 0)  &&  (run_mode == GIMP_RUN_INTERACTIVE))
            {
               gimp_display_new(l_rc);
            }
         }
      }
    }
    gap_lib_free_ainfo(&ainfo_ptr);
  }

  return(l_rc);
}       /* end gap_range_conv */



/* ============================================================================
 * gap_range_anim_sizechange
 *    scale, resize or crop all video frame images of the animation
 *    (depending on asiz_mode)
 * ============================================================================
 */
int
gap_range_anim_sizechange(GimpRunMode run_mode, GapRangeOpsAsiz asiz_mode, gint32 image_id,
                  long size_x, long size_y, long offs_x, long offs_y)
{
  int    l_rc;
  long   l_size_x, l_size_y;
  long   l_offs_x, l_offs_y;
  GapAnimInfo *ainfo_ptr;

  l_rc = 0;
  ainfo_ptr = gap_lib_alloc_ainfo(image_id, run_mode);
  if(ainfo_ptr != NULL)
  {
    if (0 == gap_lib_dir_ainfo(ainfo_ptr))
    {
      if(run_mode == GIMP_RUN_INTERACTIVE)
      {
         l_rc = p_anim_sizechange_dialog (ainfo_ptr, asiz_mode,
                                          &l_size_x, &l_size_y,
                                          &l_offs_x, &l_offs_y);
      }
      else
      {
         l_size_x = size_x;
         l_size_y = size_y;
         l_offs_x = offs_x;
         l_offs_y = offs_y;
      }

      if(l_rc >= 0)
      {
         if(gap_lib_gap_check_save_needed(ainfo_ptr->image_id))
         {
           l_rc = gap_lib_save_named_frame(ainfo_ptr->image_id, ainfo_ptr->old_filename);
         }
         if(l_rc >= 0)
         {
           /* we have to resize the current video frame image in gimp's ram
            *(from where we were invoked)
            * Note: All video frames on disc and the current one in ram
            *       must fit in size and type, to allow further animation operations.
            *       (Restriction of duplicate_into)
            */
           gimp_image_undo_disable(ainfo_ptr->image_id);
           l_rc = p_image_sizechange(ainfo_ptr->image_id, asiz_mode,
                                     l_size_x, l_size_y, l_offs_x, l_offs_y);

           if(l_rc == 0)
           {
              /* sizechange for all video frames on disk */
              l_rc = p_anim_sizechange(ainfo_ptr, asiz_mode,
                                       l_size_x, l_size_y,
                                       l_offs_x, l_offs_y );
           }
           /* gap_lib_load_named_frame(ainfo_ptr->image_id, ainfo_ptr->old_filename); */
           /* dont need to reload, because the same sizechange operation was
            * applied both to ram-image and discfile
            *
            * But we must clear all undo steps.
            * (If the user could undo the sizechange on the current image,
            *  it would not fit to the other frames on disk.)
            */
           gimp_image_undo_enable(ainfo_ptr->image_id); /* clear undo stack */
         }
      }
    }
    gap_lib_free_ainfo(&ainfo_ptr);
  }

  return(l_rc);
}       /* end gap_range_anim_sizechange */
