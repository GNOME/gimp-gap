/* gap_filter_foreach.c
 * 1997.12.23 hof (Wolfgang Hofer)
 *
 * GAP ... Gimp Animation Plugins
 *
 * This Module contains:
 * - GAP_filter  foreach: call any Filter (==Plugin Proc)
 *                        with varying settings for all
 *                        layers within one Image.
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
 * gimp   1.3.26c;  2004/03/09  hof: bugfix (gimp_destroy_params)
 * gimp   1.3.20b;  2003/09/20  hof: gap_db_browser_dialog new param image_id
 * gimp   1.3.12a;  2003/05/02  hof: merge into CVS-gimp-gap project
 * gimp   1.3.8a;   2002/09/21  hof: gap_lastvaldesc
 * gimp   1.3.4b;   2002/03/24  hof: support COMMON_ITERATOR
 * 1.1.29b; 2000/11/30   hof: use g_snprintf
 * 1.1.28a; 2000/11/05   hof: check for GIMP_PDB_SUCCESS (not for FALSE)
 * version 0.97.00              hof: - modul splitted (2.nd part is now gap_filter_pdb.c)
 * version 0.96.03              hof: - pitstop dialog provides optional backup on each step
 *                                     (and skip option)
 * version 0.96.00              hof: - now using gap_arr_dialog.h
 * version 0.92.00              hof: - pitstop dialog
 *                                     give user a chance to stop after interactive plugin calls
 *                                     if you dont want the dialog export GAP_FILTER_PITSTOP="N"
 *                                   - fixed bug in restore of layervisibility
 *                                   - codegen via explicite button (in gap_debug mode)
 * version 0.91.01; Tue Dec 23  hof: 1.st (pre) release
 */
#include "config.h"

/* SYTEM (UNIX) includes */
#include <stdio.h>
#include <string.h>
#ifdef HAVE_SYS_WAIT_H
#include <sys/wait.h>
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

/* GIMP includes */
#include "gtk/gtk.h"
#include "config.h"
#include "gap-intl.h"
#include "libgimp/gimp.h"

/* GAP includes */
#include "gap_lastvaldesc.h"
#include "gap_arr_dialog.h"
#include "gap_filter.h"
#include "gap_filter_pdb.h"
#include "gap_dbbrowser_utils.h"
#include "gap_lib.h"
#include "gap_base.h"
#include "gap_image.h"
#include "gap_accel_char.h"

#define GAP_DB_BROWSER_FILTERALL_HELP_ID  "gap-filterall-db-browser"

#define GAP_FILTERALL_LAYERS_ENABLE_PITSTOP_DIALOG  "gap-filterall-enable-pitstop-dialog"

/* ------------------------
 * global gap DEBUG switch
 * ------------------------
 */

/* int gap_debug = 1; */    /* print debug infos */
/* int gap_debug = 0; */    /* 0: dont print debug infos */

extern int gap_debug;


static void p_gdisplays_update_full(gint32 image_id);
static gint p_pitstop(GimpRunMode run_mode, char *plugin_name, gint text_flag,
                      char *step_backup_file, gint len_step_backup_file,
                      gint32 layer_idx);

static void p_visibilty_restore(gint32 image_id, gint nlayers, int *visible_tab, char *plugin_name);
static gint32 p_get_indexed_layerid(gint32 image_id, gint *nlayers, gint32 idx, char *plugin_name);

static gint   p_call_pdb_filter_plugin(char *canonical_plugin_name, gint32 image_id, gint32 layer_id, GimpRunMode run_mode
                   , gint32 groupFilterHandlingMode);

static int p_foreach_multilayer(GimpRunMode run_mode, gint32 image_id,
                         const char *plugin_name, gint32 accelCharacteristic, gint32 groupFilterHandlingMode);
static int p_foreach_multilayer2(GimpRunMode run_mode, gint32 image_id,
                         char *canonical_plugin_name, gint32 accelCharacteristic, gint32 groupFilterHandlingMode);

/* ------------------------
 * p_gdisplays_update_full
 * ------------------------
 */
static void
p_gdisplays_update_full(gint32 image_id)
{
   gimp_displays_flush();
}

/* ------------------------
 * p_pitstop
 * ------------------------
 *   return -1 on cancel, 0 .. on continue, 1 .. on skip
 */
static gint
p_pitstop(GimpRunMode run_mode, char *plugin_name, gint text_flag,
                      char *step_backup_file, gint len_step_backup_file,
                      gint32 layer_idx)
{
  gchar            *l_msg;
  static GapArrButtonArg  l_but_argv[3];
  gint              l_but_argc;
  gint              l_argc;
  static GapArrArg  l_argv[2];
  int               l_continue;
  char              l_skip_txt[32];
  int               l_ii;
  int               l_ii_gimprc;


  l_ii = 0;
  l_ii_gimprc = l_ii;
  gap_arr_arg_init(&l_argv[l_ii], GAP_ARR_WGT_TOGGLE);
    l_argv[l_ii].label_txt = _("do not show this dialog again");
    l_argv[l_ii].help_txt  = g_strdup_printf(_("add %s to gimprc configuration to disable this dialog in all further sessions")
                                             ,GAP_FILTERALL_LAYERS_ENABLE_PITSTOP_DIALOG );
    l_argv[l_ii].int_ret = FALSE;
    l_argv[l_ii].int_default = FALSE;
    l_argv[l_ii].has_default = TRUE;

  l_ii = 1;
  gap_arr_arg_init(&l_argv[l_ii], GAP_ARR_WGT_FILESEL);
    l_argv[l_ii].label_txt = _("Backup to file");
    l_argv[l_ii].entry_width = 140;        /* pixel */
    l_argv[l_ii].help_txt  = _("Make backup of the image after each step");
    l_argv[l_ii].text_buf_len = len_step_backup_file;
    l_argv[l_ii].text_buf_ret = step_backup_file;
  
    l_but_argv[0].but_txt  = _("Continue");
    l_but_argv[0].but_val  = 0;
    l_but_argv[1].but_txt  = GTK_STOCK_CANCEL;
    l_but_argv[1].but_val  = -1;
    g_snprintf(l_skip_txt, sizeof(l_skip_txt), _("Skip %d"), (int)layer_idx);
    l_but_argv[2].but_txt  = l_skip_txt;
    l_but_argv[2].but_val  = 1;

   l_but_argc = 2;
   l_argc = 1;
   /* optional dialog between both calls (to see the effect of 1.call) */
   if(run_mode == GIMP_RUN_INTERACTIVE)
   {
      if(!gap_base_get_gimprc_gboolean_value(GAP_FILTERALL_LAYERS_ENABLE_PITSTOP_DIALOG, TRUE))
      {
         return 0;  /* continue without question */
      }

      if(text_flag == 0)
      {
         l_msg = g_strdup_printf (_("2nd call of %s\n(define end-settings)"), plugin_name);
      }
      else
      {
         l_msg = g_strdup_printf (_("Non-Interactive call of %s\n(for all layers in between)"), plugin_name);
         l_but_argc = 3;
         l_argc = 2;
      }
      l_continue = gap_arr_std_dialog (_("Animated Filter Apply"), l_msg,
                                       l_argc,     l_argv,
                                       l_but_argc, l_but_argv, 0);
      g_free (l_msg);
      
      if ((l_argv[l_ii_gimprc].int_ret != FALSE) && (l_continue ==0))
      {
        gimp_gimprc_set(GAP_FILTERALL_LAYERS_ENABLE_PITSTOP_DIALOG, "no");
      }

      if(l_continue < 0) return -1;
      else               return l_continue;

   }

   return 0;  /* continue without question */
}       /* end p_pitstop */



/* ------------------------
 * p_visibilty_restore
 * ------------------------
 */
static void
p_visibilty_restore(gint32 image_id, gint nlayers, int *visible_tab, char *plugin_name)
{
  gint32    *l_layers_list;
  gint       l_nlayers2;
  gint32     l_idx;

  l_layers_list = gimp_image_get_layers(image_id, &l_nlayers2);
  if(l_nlayers2 == nlayers)
  {
    for(l_idx = 0; l_idx < nlayers; l_idx++)
    {
      gimp_item_set_visible(l_layers_list[l_idx], visible_tab[l_idx]);
      if(gap_debug)
      {
        printf("visibilty restore [%d] %d\n"
           , (int)l_idx
           , (int)visible_tab[l_idx]
           );
      }
    }
    p_gdisplays_update_full(image_id);
  }
  else
  {
    g_message(_("Error: Plugin %s has changed the number of layers from %d to %d\ncould not restore Layer visibility.\n"),
            plugin_name, (int)nlayers, (int)l_nlayers2);
  }

  g_free (l_layers_list);
}

/* ------------------------
 * p_get_indexed_layerid
 * ------------------------
 */
static gint32
p_get_indexed_layerid(gint32 image_id, gint *nlayers, gint32 idx, char *plugin_name)
{
  gint32    *l_layers_list;
  gint32     l_layer_id;
  gint       l_nlayers2;

  l_layers_list = gimp_image_get_layers(image_id, &l_nlayers2);
  if(l_layers_list == NULL)
  {
      printf ("Warning: cant get layers (maybe the image was closed)\n");
      return -1;
  }
  if((l_nlayers2 != *nlayers) && (*nlayers > 0))
  {
     printf ("Error: Plugin %s has changed Nr. of layers from %d to %d\nAnim Filter apply stopped.\n",
            plugin_name, (int)*nlayers, (int)l_nlayers2);
      return -1;
  }

  *nlayers = l_nlayers2;
  l_layer_id = l_layers_list[idx];
  g_free (l_layers_list);
  return (l_layer_id);
}

/* ---------------------------------------
 * p_call_pdb_filter_plugin
 * ---------------------------------------
 * apply the given filter (canonical_plugin_name) on the specified layer_id
 * In case the specified layer_id is a group layer
 * the groupFilterHandlingMode
 */
static gint
p_call_pdb_filter_plugin(char *canonical_plugin_name, gint32 image_id, gint32 layer_id, GimpRunMode run_mode
   , gint32 groupFilterHandlingMode)
{
  gint32 l_resulting_item_id;

  if(gap_debug)
  {
    printf("p_call_pdb_filter_plugin: %s  image_id:%d layer_id:%d (%s), run_mode:%d, groupFilterHandlingMode:%d\n"
       , canonical_plugin_name
       , (int)image_id
       , (int)layer_id
       , gimp_item_get_name(layer_id)
       , (int)run_mode
       , (int)groupFilterHandlingMode
       );
  }

  l_resulting_item_id = layer_id;
  if (gimp_item_is_group(layer_id))
  {
    if(groupFilterHandlingMode == GAP_GROUP_FILTER_HANDLING_SKIP)
    {
      if (run_mode == GIMP_RUN_INTERACTIVE)
      {
        /* fail because skipping of the interactive calls
         * is not allowed (those calls are required to setup the last values buffer)
         */
        return (-1);
      }
      return (0);    /* fake OK to skip processing of this group layer */
    }
    
    if(groupFilterHandlingMode == GAP_GROUP_FILTER_HANDLING_MERGE)
    {
      l_resulting_item_id = gap_image_merge_group_layer( image_id
                                                       , layer_id
                                                       , GIMP_EXPAND_AS_NECESSARY
                                                       );
    }
  }
  if (l_resulting_item_id < 0)
  {
    return (-1);
  }
  
  if(gap_debug)
  {
    printf("p_call_pdb_filter_plugin: %s  image_id:%d l_resulting_item_id:%d (%s), run_mode:%d, groupFilterHandlingMode:%d\n"
       , canonical_plugin_name
       , (int)image_id
       , (int)l_resulting_item_id
       , gimp_item_get_name(l_resulting_item_id)
       , (int)run_mode
       , (int)groupFilterHandlingMode
       );
  }
  return (gap_filt_pdb_call_plugin(canonical_plugin_name, image_id, l_resulting_item_id, run_mode));


}  /* end p_call_pdb_filter_plugin */


/* ----------------------
 * p_foreach_multilayer
 * ----------------------
 *    apply the given plugin to each layer of the  image.
 * returns   image_id of the new created multilayer image
 *           (or -1 on error)
 */
static int
p_foreach_multilayer(GimpRunMode run_mode, gint32 image_id,
                     const char *plugin_name, gint32 accelCharacteristic, gint32 groupFilterHandlingMode)
{
  char *canonical_plugin_name;
  int rc;

  canonical_plugin_name = gimp_canonicalize_identifier(plugin_name);
  gimp_image_undo_group_start (image_id);
  rc = p_foreach_multilayer2(run_mode, image_id, canonical_plugin_name, accelCharacteristic, groupFilterHandlingMode);
  gimp_image_undo_group_end (image_id);

  g_free(canonical_plugin_name);

  return (rc);

}  /* ennd p_foreach_multilayer */


/* ----------------------
 * p_foreach_multilayer2
 * ----------------------
 *    apply the given plugin to each toplevel layer of the  image.
 * returns   image_id of the new created multilayer image
 *           (or -1 on error)
 */
static int
p_foreach_multilayer2(GimpRunMode run_mode, gint32 image_id,
                         char *canonical_plugin_name, gint32 accelCharacteristic, gint32 groupFilterHandlingMode)
{
  static char l_key_from[512];
  static char l_key_to[512];
  char      *l_plugin_iterator;
  gint32     l_layer_id;
  gint32     l_top_layer;
  gint32     l_idx;
  gint       l_nlayers;
  gdouble    l_percentage, l_percentage_step;
  int         l_rc;
  gint        l_plugin_data_len;
  int         *l_visible_tab;
  char         l_step_backup_file[120];
  gint         l_pit_rc;
  gint         l_count;

  l_rc = 0;
  l_plugin_data_len = 0;
  l_nlayers = 0;
  l_visible_tab = NULL;
  l_step_backup_file[0] = '\0';

  /* check for the Plugin */

  l_rc = gap_filt_pdb_procedure_available(canonical_plugin_name, GAP_PTYP_CAN_OPERATE_ON_DRAWABLE);
  if(l_rc < 0)
  {
     printf("ERROR: Plugin not available or wrong type %s\n", canonical_plugin_name);
     return -1;
  }


  /* check for matching Iterator PluginProcedures */
  l_plugin_iterator =  gap_filt_pdb_get_iterator_proc(canonical_plugin_name, &l_count);

  l_percentage = 0.0;
  if(run_mode == GIMP_RUN_INTERACTIVE)
  {
    gimp_progress_init( _("Applying filter to all layers..."));
  }

  l_layer_id = p_get_indexed_layerid(image_id, &l_nlayers, 0,  canonical_plugin_name);
  if(l_layer_id >= 0)
  {
    if(l_nlayers < 1)
    {
       printf("ERROR: need at least 1 Layers to apply plugin !\n");
    }
    else
    {
      /* allocate a table to store the visibility attributes for each layer */
      l_visible_tab = (gint*) g_malloc((l_nlayers +1) * sizeof (gint));
      if(l_visible_tab == NULL)
      {
         return -1;
      }

      /* save the visibility of all layers */
      for(l_idx = 0; l_idx < l_nlayers; l_idx++)
      {
        l_layer_id = p_get_indexed_layerid(image_id, &l_nlayers, l_idx,  canonical_plugin_name);
        l_visible_tab[l_idx] = gimp_item_get_visible(l_layer_id);

        /* make the backround visible, all others invisible
         * (so the user can see the effect of the 1.st applied _FROM filter)
         */
        gimp_item_set_visible(l_layer_id, (l_idx == l_nlayers - 1));
      }
      p_gdisplays_update_full(image_id);

      l_percentage_step = 1.0 / l_nlayers;

      if((l_plugin_iterator != NULL)  && (l_nlayers > 1) && (accelCharacteristic != GAP_ACCEL_CHAR_NONE ))
      {
        /* call plugin Interactive for background layer[n] */
        if(gap_debug)
        {
          printf("DEBUG start 1.st Interactive call (_FROM values)\n");
        }

        l_idx = l_nlayers -1;
        l_layer_id = p_get_indexed_layerid(image_id, &l_nlayers, l_idx,  canonical_plugin_name);
        if(l_layer_id < 0)
        {
          l_rc = -1;
        }
        else
        {
          if(gap_debug)
          {
            printf("DEBUG: applying %s on Layerstack %d id=%d\n", canonical_plugin_name, (int)l_idx, (int)l_layer_id);
                  }
          l_rc = p_call_pdb_filter_plugin(canonical_plugin_name, image_id, l_layer_id
                     , GIMP_RUN_INTERACTIVE, groupFilterHandlingMode);

          /* get values, then store with suffix "-ITER-FROM" */
          l_plugin_data_len = gap_filt_pdb_get_data(canonical_plugin_name);
          if(l_plugin_data_len > 0)
          {
             g_snprintf(l_key_from, sizeof(l_key_from), "%s%s", canonical_plugin_name, GAP_ITER_FROM_SUFFIX);
             gap_filt_pdb_set_data(l_key_from, l_plugin_data_len);
          }
          else
          {
            l_rc = -1;
          }

          if(run_mode == GIMP_RUN_INTERACTIVE)
          {
            l_percentage += l_percentage_step;
            gimp_progress_update (l_percentage);
          }
        }

        if((l_rc >= 0) && (l_nlayers > 1))
        {
          if(gap_debug)
          {
            printf("DEBUG start 2.nd Interactive call (_TO values)\n");
          }

          /* optional dialog between both calls (to see the effect of 1.call) */
          if(p_pitstop(run_mode, canonical_plugin_name, 0,
                       l_step_backup_file, sizeof(l_step_backup_file), 0
                      )
             < 0)
          {
              if(gap_debug) printf("TERMINATED: by pitstop dialog\n");
              /* restore the visibility of all layers */
              p_visibilty_restore(image_id, l_nlayers, l_visible_tab, canonical_plugin_name);
              g_free(l_visible_tab);
              return -1;
          }
          else
          {
             l_layer_id = p_get_indexed_layerid(image_id, &l_nlayers, 0,  canonical_plugin_name);
             if(l_layer_id < 0)
             {
               l_rc = -1;
             }
             else
             {
                /* make _TO layer visible */
                gimp_item_set_visible(l_layer_id, TRUE);
                p_gdisplays_update_full(image_id);

                if(gap_debug) printf("DEBUG: apllying %s on Layerstack 0  id=%d\n", canonical_plugin_name, (int)l_layer_id);
                l_rc = p_call_pdb_filter_plugin(canonical_plugin_name, image_id, l_layer_id
                           , GIMP_RUN_INTERACTIVE, groupFilterHandlingMode);

                /* get values, then store with suffix "-ITER-TO" */
                l_plugin_data_len = gap_filt_pdb_get_data(canonical_plugin_name);
                if(l_plugin_data_len > 0)
                {
                   g_snprintf(l_key_to, sizeof(l_key_to), "%s%s", canonical_plugin_name, GAP_ITER_TO_SUFFIX);
                   gap_filt_pdb_set_data(l_key_to, l_plugin_data_len);
                }
                else
                {
                  l_rc = -1;
                }

                if(run_mode == GIMP_RUN_INTERACTIVE)
                {
                  l_percentage += l_percentage_step;
                  gimp_progress_update (l_percentage);
                }

                /* if(gap_debug) printf("DEBUG child process exit %d\n", (int)l_rc); */
                /* exit(l_rc); */                               /* end of childprocess */
             }

         }

        }
        l_top_layer = 1;
      }
      else
      {
          /* no iterator available, call plugin with constant values
           * for each layer
           */
          /* call plugin only ONCE Interactive for background layer[n] */
          l_layer_id = p_get_indexed_layerid(image_id, &l_nlayers, l_nlayers -1,  canonical_plugin_name);
          if(l_layer_id < 0)
          {
             l_rc = -1;
          }
          else
          {
            if(gap_debug) printf("DEBUG: NO Varying, applying %s on Layer id=%d\n", canonical_plugin_name, (int)l_layer_id);
            l_rc = p_call_pdb_filter_plugin(canonical_plugin_name, image_id, l_layer_id
                        , GIMP_RUN_INTERACTIVE, groupFilterHandlingMode);
            l_top_layer = 0;
            if(run_mode == GIMP_RUN_INTERACTIVE)
            {
              l_percentage += l_percentage_step;
              gimp_progress_update (l_percentage);
            }
          }
      }

      if((l_rc >= 0) &&  (l_nlayers > 2))
      {
        /* call plugin foreach layer inbetween
         * with runmode GIMP_RUN_WITH_LAST_VALS
         * and modify the last values
         */
        l_pit_rc = 1;
        for(l_idx = l_nlayers - 2; l_idx >= l_top_layer; l_idx--)
        {
          if(l_rc < 0) break;

          if(l_pit_rc > 0)   /* last pit_rc was a skip, so ask again for the next layer */
          {
            l_pit_rc = p_pitstop(run_mode, canonical_plugin_name, 1,
                               l_step_backup_file, sizeof(l_step_backup_file),
                               l_idx );
          }
          if(l_pit_rc < 0)
          {
              if(gap_debug) printf("TERMINATED: by pitstop dialog\n");
              l_rc = -1;
          }

          l_layer_id = p_get_indexed_layerid(image_id, &l_nlayers, l_idx,  canonical_plugin_name);
          if(l_layer_id < 0)
          {
             l_rc = -1;
             break;
          }

          if(gap_debug)
          {
            printf("DEBUG: applying %s on Layerstack %d id=%d  total_steps:%d current_step:%d\n"
                , canonical_plugin_name
                , (int)l_idx
                , (int)l_layer_id
                , (int)l_nlayers -1
                , (int)l_idx
                );
          }


          if((l_plugin_iterator != NULL) && (accelCharacteristic != GAP_ACCEL_CHAR_NONE ))
          {
            gdouble accelStep;


            accelStep = gap_calculate_current_step_with_acceleration((gdouble)l_idx, l_nlayers -1, accelCharacteristic);
            /* call plugin-specific iterator (or the common iterator), to modify
             * the plugin's last_values
             */
            if(!gap_filter_iterator_call(l_plugin_iterator
                 , l_nlayers -1      /* total steps  */
                 , accelStep         /* current step according to acceleration characteristic */
                 , canonical_plugin_name
                 , l_plugin_data_len
                 ))
            {
              l_rc = -1;
            }
          }

          if(l_rc < 0)
          {
            break;
          }

          if(l_pit_rc == 0)  /* 0 == continue without further dialogs */
          {
             /* call the plugin itself with runmode RUN_WITH_LAST_VALUES */
             l_rc = p_call_pdb_filter_plugin(canonical_plugin_name, image_id, l_layer_id
                       , GIMP_RUN_WITH_LAST_VALS, groupFilterHandlingMode);
             /* check if to save each step to backup file */
             if((l_step_backup_file[0] != '\0') && (l_step_backup_file[0] != ' '))
             {
               printf ("Saving image to backupfile:%s step = %d\n",
                       l_step_backup_file, (int)l_idx);
               gap_filt_pdb_save_xcf(image_id, l_step_backup_file);
             }
          }

          if(run_mode == GIMP_RUN_INTERACTIVE)
          {
            l_percentage += l_percentage_step;
            gimp_progress_update (l_percentage);
          }

        }       /* end for */

      }

      /* restore the visibility of all layers */
      p_visibilty_restore(image_id, l_nlayers, l_visible_tab, canonical_plugin_name);
      g_free(l_visible_tab);

    }

  }


  if(l_plugin_iterator != NULL)      g_free(l_plugin_iterator);

  return l_rc;
}       /* end p_foreach_multilayer2 */


/* ------------------------
 * gap_proc_anim_apply
 * ------------------------
 */
gint
gap_proc_anim_apply(GimpRunMode run_mode, gint32 image_id, char *plugin_name
  , gint32 accelCharacteristic)
{
  GapDbBrowserResult  l_browser_result;
  gint                l_rc;
  l_browser_result.accelCharacteristic = GAP_ACCEL_CHAR_LINEAR;
  l_browser_result.groupFilterHandlingMode = GAP_GROUP_FILTER_HANDLING_NORMAL;

  if(run_mode == GIMP_RUN_INTERACTIVE)
  {

    if(gap_db_browser_dialog( _("Select Filter for Animated Apply"),
                                 _("Apply"),
                                 TRUE,                                  /* showAccelerationCharacteristic */
                                 gap_filt_pdb_constraint_proc,
                                 gap_filt_pdb_constraint_proc_sel1,
                                 gap_filt_pdb_constraint_proc_sel2,
                                 &l_browser_result,
                                 image_id,
                                 GAP_DB_BROWSER_FILTERALL_HELP_ID)
      < 0)
    {
      if(gap_debug) printf("DEBUG: gap_db_browser_dialog cancelled\n");
      return -1;
    }

    strcpy(plugin_name, l_browser_result.selected_proc_name);

    /* invert acceleration to deceleration and vice versa
     * (because processing layer indexes is done from high to low values)
     */
    accelCharacteristic = (-1 * l_browser_result.accelCharacteristic);

    if(gap_debug)
    {
      printf("DEBUG: gap_db_browser_dialog SELECTED:%s accelCharacteristic:%d\n"
           , plugin_name
           , (int)accelCharacteristic
           );
    }

  }

  l_rc = p_foreach_multilayer(run_mode,
                              image_id,
                              plugin_name,
                              accelCharacteristic,
                              l_browser_result.groupFilterHandlingMode);
  return(l_rc);
}
