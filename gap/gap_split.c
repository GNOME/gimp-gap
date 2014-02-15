/* gap_split.c
 * 1997.11.06 hof (Wolfgang Hofer)
 *
 * GAP ... Gimp Animation Plugins
 *
 * This Module contains
 * - gap_split_image
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
 * 2.1.0;   2004/11/12   hof: added help button
 * 1.3.14a; 2003/05/25   hof: bugfix: defaultname for unnamed image
 *                            added digits and only_visible parameter
 * 1.1.28a; 2000/10/29   hof: subsequent save calls use GIMP_RUN_WITH_LAST_VALS
 * 1.1.9a;  1999/09/21   hof: bugfix GIMP_RUN_NONINTERACTIVE mode did not work
 * 1.1.8a;  1999/08/31   hof: accept video framenames without underscore '_'
 * 1.1.5a;  1999/05/08   hof: bugix (dont mix GimpImageType with GimpImageBaseType)
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
#include <glib/gstdio.h>
#include "gtk/gtk.h"
#include "config.h"
#include "gap-intl.h"
#include "libgimp/gimp.h"

/* GAP includes */
#include "gap_libgapbase.h"
#include "gap_layer_copy.h"
#include "gap_lib.h"
#include "gap_arr_dialog.h"


#define PLUGIN_NAME_GAP_SPLIT                "plug_in_gap_split"

typedef struct
{
  gint32     inverse_order;
  gint32     no_alpha;
  gint32     only_visible;
  gint32     copy_properties;
  gint32     digits;

  char       extension[32];
} split_vals_t;


extern      int gap_debug; /* ==0  ... dont print debug infos */

#define GAP_HELP_ID_SPLIT           "plug-in-gap-split"

/* ------------------------------
 * p_overwrite_dialog
 * ------------------------------
 */
static gint
p_overwrite_dialog(char *filename, gint overwrite_mode)
{
  static  GapArrButtonArg  l_argv[3];
  static  GapArrArg  argv[1];

  if(g_file_test(filename, G_FILE_TEST_EXISTS))
  {
    if (overwrite_mode < 1)
    {
       l_argv[0].but_txt  = _("Overwrite Frame");
       l_argv[0].but_val  = 0;
       l_argv[1].but_txt  = _("Overwrite All");
       l_argv[1].but_val  = 1;
       l_argv[2].but_txt  = GTK_STOCK_CANCEL;
       l_argv[2].but_val  = -1;

       gap_arr_arg_init(&argv[0], GAP_ARR_WGT_LABEL);
       argv[0].label_txt = filename;

       return(gap_arr_std_dialog ( _("GAP Question"),
                                   _("File already exists"),
                                   1, argv,
                                   3, l_argv, -1));
    }
  }
  return (overwrite_mode);
}  /* end p_overwrite_dialog */


/* --------------------
 * p_split_image
 * --------------------
 * splits a multilayer image into frames, where each selected layer
 * produces a resulting frame.
 *
 * returns   value >= 0 if all is ok  return the image_id of
 *                      the new created image (the last handled video frame)
 *           (or -1 on error)
 */
static int
p_split_image(GapAnimInfo *ainfo_ptr, split_vals_t *valPtr)
{
  GimpImageBaseType l_type;
  guint   l_width, l_height;
  GimpRunMode l_run_mode;
  gint32  l_new_image_id;
  gint    l_nlayers;
  gint32 *l_layers_list;
  gint32  l_src_layer_id;
  gint32  l_cp_layer_id;
  gint    l_src_offset_x, l_src_offset_y;    /* layeroffsets as they were in src_image */
  gdouble l_percentage, l_percentage_step;
  char   *l_sav_name;
  char   *l_str;
  gint32  l_rc;
  int     l_idx;
  int     l_framenumber;
  long    l_layer_idx;
  gint    l_overwrite_mode;
  gdouble l_xresoulution, l_yresoulution;
  gint32  l_unit;

  if(gap_debug)
  {
    printf("DEBUG: p_split_image inverse_order:%d no_alpha:%d only_visible:%d copy_properties:%d digits:%d ext:%s\n"
         , (int)valPtr->inverse_order
         , (int)valPtr->no_alpha
         , (int)valPtr->only_visible
         , (int)valPtr->copy_properties
         , (int)valPtr->digits
         , valPtr->extension
         );
  }
  l_rc = -1;
  l_percentage = 0.0;
  l_run_mode  = ainfo_ptr->run_mode;
  l_overwrite_mode = 0;
  if(ainfo_ptr->run_mode == GIMP_RUN_INTERACTIVE)
  {
    gimp_progress_init( _("Splitting image into frames..."));
  }

  l_new_image_id = -1;
  /* get info about the image  */
  l_width  = gimp_image_width(ainfo_ptr->image_id);
  l_height = gimp_image_height(ainfo_ptr->image_id);
  l_type = gimp_image_base_type(ainfo_ptr->image_id);
  l_unit = gimp_image_get_unit(ainfo_ptr->image_id);
  gimp_image_get_resolution(ainfo_ptr->image_id, &l_xresoulution, &l_yresoulution);

  l_layers_list = gimp_image_get_layers(ainfo_ptr->image_id, &l_nlayers);
  if(l_layers_list != NULL)
  {
    gint32 l_max_framenumber;

    /* count number of relevant layers (to be written as frame images) */
    l_max_framenumber = 0;
    for(l_idx = 0; l_idx < l_nlayers; l_idx++)
    {
       if(valPtr->only_visible)
       {
          if (! gimp_item_get_visible(l_layers_list[l_idx]))
          {
             /* skip invisible layers in only_visible Mode */
             continue;
          }
       }
       l_max_framenumber++;
    }    
  
    l_percentage_step = 1.0 / (l_nlayers);

    l_framenumber = l_max_framenumber;
    for(l_idx = 0; l_idx < l_nlayers; l_idx++)
    {
       if (l_new_image_id >= 0)
       {
          /* destroy the tmp image (it was saved to disk before) */
          gimp_image_delete(l_new_image_id);
          l_new_image_id = -1;
       }

       if(valPtr->inverse_order != TRUE) l_layer_idx = l_idx;
       else                              l_layer_idx = (l_nlayers - 1 ) - l_idx;

       l_src_layer_id = l_layers_list[l_layer_idx];

       if(valPtr->only_visible)
       {
          if (! gimp_item_get_visible(l_src_layer_id))
          {
             /* skip invisible layers in only_visible Mode */
             continue;
          }
       }

       /* the implementation for duplicate mode is slow, but keeps all gimp image stuff
        * (such as channels, path, guides, parasites and whatever
        * will be added in future gimp versions....) in each copied frame.
        */
       if(valPtr->copy_properties)
       {
         gint    l_dup_idx;
         gint    l_dup_nlayers;
         gint32 *l_dup_layers_list;
         
         l_new_image_id = gimp_image_duplicate(ainfo_ptr->image_id);
         l_cp_layer_id = -1;
         l_dup_layers_list = gimp_image_get_layers(l_new_image_id, &l_dup_nlayers);
         for(l_dup_idx = 0; l_dup_idx < l_dup_nlayers; l_dup_idx++)
         {
           if (l_dup_idx == l_layer_idx)
           {
             l_cp_layer_id = l_dup_layers_list[l_dup_idx];
           }
           else
           {
             gimp_image_remove_layer(l_new_image_id, l_dup_layers_list[l_dup_idx]);
           }
           
         }
         g_free (l_dup_layers_list);
       }
       else
       {
         /* create new image */
         l_new_image_id =  gimp_image_new(l_width, l_height,l_type);
         gimp_image_set_resolution(l_new_image_id, l_xresoulution, l_yresoulution);
         gimp_image_set_unit(l_new_image_id, l_unit);

         if(l_new_image_id < 0)
         {
           l_rc = -1;
           break;
         }
         
         /* copy colormap in case of indexed image */
         if (l_type == GIMP_INDEXED)
         {
           gint        l_ncolors;
           guchar     *l_cmap;
          
           l_cmap = gimp_image_get_colormap(ainfo_ptr->image_id, &l_ncolors);
           gimp_image_set_colormap(l_new_image_id, l_cmap, l_ncolors);
           g_free(l_cmap);

         }

         /* copy the layer */
         l_cp_layer_id = gap_layer_copy_to_dest_image(l_new_image_id,
                                     l_src_layer_id,
                                     100.0,   /* Opacity */
                                     0,       /* NORMAL */
                                     &l_src_offset_x,
                                     &l_src_offset_y);
         /* add the copied layer to current destination image */
          gimp_image_insert_layer(l_new_image_id, l_cp_layer_id, 0, 0);
          gimp_layer_set_offsets(l_cp_layer_id, l_src_offset_x, l_src_offset_y);
          gimp_item_set_visible(l_cp_layer_id, TRUE);
       }


       /* delete alpha channel ? */
       if (valPtr->no_alpha == TRUE)
       {
           /* add a dummy layer (flatten needs at least 2 layers) */
           l_cp_layer_id = gimp_layer_new(l_new_image_id, "dummy",
                                          4, 4,         /* width, height */
                                          ((l_type * 2 ) + 1),  /* convert from GimpImageBaseType to GimpImageType, and add alpha */
                                          0.0,          /* Opacity full transparent */
                                          0);           /* NORMAL */
           gimp_image_insert_layer(l_new_image_id, l_cp_layer_id, 0, 0);
           gimp_image_flatten (l_new_image_id);

       }


       /* build the name for output image */
       l_str = gap_base_dup_filename_and_replace_extension_by_underscore(ainfo_ptr->old_filename);
       l_sav_name = gap_lib_alloc_fname6(l_str,
                                  l_framenumber,       /* start at 1 (not at 0) */
                                  valPtr->extension,
                                  valPtr->digits);
       l_framenumber--;
       g_free(l_str);


       if(l_sav_name != NULL)
       {
          gboolean writePermission;
          
          writePermission = TRUE;
          /* check overwrite if Destination frame already exsts */
          l_overwrite_mode = p_overwrite_dialog(l_sav_name, l_overwrite_mode);
          
          if(gap_debug)
          {
            printf("l_overwrite_mode:%d  file:%s\n", l_overwrite_mode, l_sav_name);
          }
          
          if (l_overwrite_mode < 0)
          {
            if(gap_debug)
            {
                 printf("overwrite of file:%s was cancelled\n", l_sav_name);
            }
            writePermission = FALSE;
          }
          else
          {
             g_remove(l_sav_name);
             if(g_file_test(l_sav_name, G_FILE_TEST_EXISTS))
             {
               char *errMsg;
               
               errMsg = g_strdup_printf(_("failed to overwrite %s (check permissions ?)")
                                      , l_sav_name);
               g_message("%s", errMsg);
               g_free(errMsg);
               writePermission = FALSE;
             }
          }


          if(writePermission == TRUE)
          {
           /* save with selected save procedure
            * (regardless if image was flattened or not)
            */
            l_rc = gap_lib_save_named_image(l_new_image_id, l_sav_name, l_run_mode);
            if(l_rc < 0)
            {
              gap_arr_msg_win(ainfo_ptr->run_mode, _("Split Frames: Save operation failed.\n"
                                               "desired save plugin can't handle type\n"
                                               "or desired save plugin not available."));
            }
          }
          else
          {
            l_rc = -1;
          }

          if(l_rc < 0)
          {
            break;
          }

          l_run_mode  = GIMP_RUN_WITH_LAST_VALS;  /* for all further calls */

          /* set image name */
          gimp_image_set_filename (l_new_image_id, l_sav_name);

          /* prepare return value */
          l_rc = l_new_image_id;

          g_free(l_sav_name);
       }

       /* show progress bar */
       if(ainfo_ptr->run_mode == GIMP_RUN_INTERACTIVE)
       {
         l_percentage += l_percentage_step;
         gimp_progress_update (l_percentage);
       }

       if (l_framenumber <= 0)
       {
         break;
       }

    }
    g_free (l_layers_list);
  }


  return l_rc;
}  /* end p_split_image */



/* -------------------
 * p_split_dialog
 * -------------------
 *
 * return  0 (OK)
 *    or  -1 in case of Error or cancel
 */
static long
p_split_dialog(GapAnimInfo *ainfo_ptr, split_vals_t *valPtr)
{
  static GapArrArg  argv[9];
  gchar   *buf;
  gchar   *extptr;
  gchar   *baseName;

  extptr = &valPtr->extension[0];
  if(extptr)
  {
    if(*extptr == '.')
    {
      extptr++;
    }
  }

  baseName = gap_base_dup_filename_and_replace_extension_by_underscore(ainfo_ptr->old_filename);

  buf = g_strdup_printf (_("Make a frame (diskfile) from each layer.\n"
                           "Frames are named in the style:\n"
                           "<basename><framenumber>.<extension>\n"
                           "The first frame for the current case gets the name\n\n"
                           "%s000001.%s\n")
                         ,baseName
                         ,extptr
                         );

  g_free(baseName);

  gap_arr_arg_init(&argv[0], GAP_ARR_WGT_LABEL);
  argv[0].label_txt = &buf[0];

  gap_arr_arg_init(&argv[1], GAP_ARR_WGT_TEXT);
  argv[1].label_txt = _("Extension:");
  argv[1].help_txt  = _("Extension of resulting frames. The extension is also used to define fileformat.");
  argv[1].text_buf_len = sizeof(valPtr->extension);
  argv[1].text_buf_ret = valPtr->extension;
  argv[1].has_default = TRUE;
  argv[1].text_buf_default = ".xcf";

  gap_arr_arg_init(&argv[2], GAP_ARR_WGT_TOGGLE);
  argv[2].label_txt = _("Inverse Order:");
  argv[2].help_txt  = _("ON: Start with frame 000001 at top layer.\n"
                        "OFF: Start with frame 000001 at background layer.");
  argv[2].int_ret   = valPtr->inverse_order;
  argv[2].has_default = TRUE;
  argv[2].int_default = 0;

  gap_arr_arg_init(&argv[3], GAP_ARR_WGT_TOGGLE);
  argv[3].label_txt = _("Flatten:");
  argv[3].help_txt  = _("ON: Remove alpha channel in resulting frames. Transparent parts are filled with the background color.\n"
                        "OFF: Layers in the resulting frames keep their alpha channel.");
  argv[3].int_ret   = valPtr->no_alpha;
  argv[3].has_default = TRUE;
  argv[3].int_default = 0;

  gap_arr_arg_init(&argv[4], GAP_ARR_WGT_TOGGLE);
  argv[4].label_txt = _("Only Visible:");
  argv[4].help_txt  = _("ON: Handle only visible layers.\n"
                        "OFF: handle all layers and force visibiblity");
  argv[4].int_ret   = valPtr->only_visible;
  argv[4].has_default = TRUE;
  argv[4].int_default = 0;

  gap_arr_arg_init(&argv[5], GAP_ARR_WGT_TOGGLE);
  argv[5].label_txt = _("Copy properties:");
  argv[5].help_txt  = _("ON: Copy all image properties (channels, pathes, guides) to all frame images.\n"
                        "OFF: copy only layers without image properties to frame images");
  argv[5].int_ret   = valPtr->copy_properties;
  argv[5].has_default = TRUE;
  argv[5].int_default = 0;

  gap_arr_arg_init(&argv[6], GAP_ARR_WGT_INT);
  argv[6].constraint = TRUE;
  argv[6].label_txt = _("Digits:");
  argv[6].help_txt  = _("How many digits to use for the framenumber filename part");
  argv[6].int_min   = (gint)1;
  argv[6].int_max   = (gint)6;
  argv[6].int_ret   = (gint)valPtr->digits;
  argv[6].entry_width = 60;
  argv[6].has_default = TRUE;
  argv[6].int_default = 6;

  gap_arr_arg_init(&argv[7], GAP_ARR_WGT_DEFAULT_BUTTON);
  argv[7].label_txt = _("Default");
  argv[7].help_txt  = _("Reset all parameters to default values");

  gap_arr_arg_init(&argv[8], GAP_ARR_WGT_HELP_BUTTON);
  argv[8].help_id = GAP_HELP_ID_SPLIT;


  if(TRUE == gap_arr_ok_cancel_dialog( _("Split Image into Frames"),
                             _("Split Settings"),
                             8, argv))
  {
    g_free (buf);
    valPtr->inverse_order   = argv[2].int_ret;
    valPtr->no_alpha        = argv[3].int_ret;
    valPtr->only_visible    = argv[4].int_ret;
    valPtr->copy_properties = argv[5].int_ret;
    valPtr->digits          = argv[6].int_ret;
    return 0;
  }
  else
  {
    g_free (buf);
    return -1;
  }
}  /* end p_split_dialog */


/* ----------------
 * gap_split_image
 * ----------------
 * Split one (multilayer) image into video frames
 * one frame per layer.
 */
int gap_split_image(GimpRunMode run_mode,
                      gint32     image_id,
                      gint32     inverse_order,
                      gint32     no_alpha,
                      char      *extension,
                      gint32     only_visible,
                      gint32     copy_properties,
                      gint32     digits
                      )

{
  gint32  l_new_image_id;
  gint32  l_rc;

  char   *l_imagename;
  
  split_vals_t l_splitVals;
  split_vals_t *valPtr;
  GapAnimInfo *ainfo_ptr;

  l_rc = -1;
  if(gap_debug)
  {
    printf("START: gap_split_image run_mod:%d, image_id:%d\n"
      ,(int) run_mode
      ,(int) image_id
      );
  }
  valPtr = &l_splitVals;
  
  /* force a default name without framenumber part for unnamed images */
  l_imagename = gimp_image_get_filename(image_id);
  if(l_imagename == NULL)
  {
    gimp_image_set_filename(image_id, "frame_.xcf");
  }

  ainfo_ptr = gap_lib_alloc_ainfo(image_id, run_mode);
  if(ainfo_ptr != NULL)
  {
    valPtr->extension[0] = '\0';
      
    /* Possibly retrieve data from a previous run */
    gimp_get_data (PLUGIN_NAME_GAP_SPLIT, &l_splitVals);
      
    if (l_splitVals.extension[0] == '\0')
    {
      /* use default values 
       * (because there are no useful values of a previous run in the same session)
       */
      strcpy(&valPtr->extension[0], ".xcf");
      valPtr->inverse_order   = 0;
      valPtr->no_alpha        = 0;
      valPtr->only_visible    = 0;
      valPtr->copy_properties = 0;
      valPtr->digits = 6;
    }

    if(run_mode == GIMP_RUN_INTERACTIVE)
    {
       l_rc = p_split_dialog (ainfo_ptr, valPtr);
    }
    else if(run_mode == GIMP_RUN_NONINTERACTIVE)
    {
       l_rc = 0;
       valPtr->inverse_order   =  inverse_order;
       valPtr->no_alpha        =  no_alpha;
       valPtr->only_visible    =  only_visible;
       valPtr->copy_properties =  copy_properties;
       valPtr->digits          =  digits;
       strncpy(&valPtr->extension[0], extension, sizeof(valPtr->extension) -1);
       valPtr->extension[sizeof(valPtr->extension) -1] = '\0';
    }
    else if (run_mode == GIMP_RUN_WITH_LAST_VALS)
    {
       l_rc = 0;
    }

    if(l_rc >= 0)
    {
       l_new_image_id = p_split_image(ainfo_ptr, valPtr);

       if (l_new_image_id >= 0)
       {
         /* Store values for next run */
         if (run_mode == GIMP_RUN_INTERACTIVE)
         {
           gimp_set_data(PLUGIN_NAME_GAP_SPLIT, &l_splitVals, sizeof(l_splitVals));
         }

         /* create a display for the new created image
          * (it is the first or the last frame of the
          *  new created animation sequence)
          */
         gimp_display_new(l_new_image_id);
       }
       l_rc = l_new_image_id;
    }
    gap_lib_free_ainfo(&ainfo_ptr);
  }

  return(l_rc);
}       /* end   gap_split_image */
