/* gap_mod_layer.c
 * 1998.10.14 hof (Wolfgang Hofer)
 *
 * GAP ... Gimp Animation Plugins
 *
 * This Module contains:
 * modify Layer (perform actions (like raise, set visible, apply filter)
 *               - foreach selected layer
 *               - in each frame of the selected framerange)
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
 * gimp   2.1.0b;    2004/10/17  hof: bugfix Function: Add layermask from selection
 *                                    optim: direct processing for the current image
 * gimp   2.1.0b;    2004/08/11  hof: new action_modes "Copy layermask from layer above/below"
 * gimp   1.3.25a;   2004/01/21  hof: message text fixes (# 132030)
 * gimp   1.3.24a;   2004/01/17  hof: added Layermask Handling
 * gimp   1.3.20d;   2003/09/20  hof: sourcecode cleanup,
 *                                    added new Actionmodes for Selction handling
 * gimp   1.3.20b;   2003/09/20  hof: gap_db_browser_dialog new param image_id
 * gimp   1.3.15a;   2003/06/21  hof: textspacing
 * gimp   1.3.14a;   2003/05/17  hof: placed OK button right.
 * gimp   1.3.12a;   2003/05/01  hof: merge into CVS-gimp-gap project
 * gimp   1.3.11a;   2003/01/18  hof: Conditional framesave
 * gimp   1.3.8a;    2002/09/21  hof: gap_lastvaldesc
 * gimp   1.3.4b;    2002/03/24  hof: support COMMON_ITERATOR
 * gimp   1.3.4a;    2002/03/12  hof: replaced private pdb-wrappers
 * gimp   1.1.29b;   2000/11/30  hof: use g_snprintf
 * gimp   1.1.28a;   2000/11/05  hof: check for GIMP_PDB_SUCCESS (not for FALSE)
 * gimp   1.1.6;     1999/06/21  hof: bugix: wrong iterator total_steps and direction
 * gimp   1.1.15.1;  1999/05/08  hof: bugix (dont mix GimpImageType with GimpImageBaseType)
 * version 0.98.00   1998.11.27  hof: - use new module gap_pdb_calls.h
 * version 0.97.00   1998.10.19  hof: - created module
 */

/* SYTEM (UNIX) includes */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

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
#include "gap_pdb_calls.h"
#include "gap_match.h"
#include "gap_lib.h"
#include "gap_layer_copy.h"
#include "gap_image.h"
#include "gap_range_ops.h"
#include "gap_mod_layer.h"
#include "gap_mod_layer_dialog.h"
#include "gap_accel_char.h"


extern      int gap_debug; /* ==0  ... dont print debug infos */


#define GAP_DB_BROWSER_MODFRAMES_HELP_ID  "gap-modframes-db-browser"

static int  p_merge_selected_toplevel_layers(gint32 image_id,
              gint merge_mode,
              GapModLayliElem *layli_ptr,
              gint nlayers,
              gint32 sel_cnt,
              long  curr,
              char *new_layername
              );
static int  p_merge_selected_group_member_layers(gint32 image_id,
              gint merge_mode,
              GapModLayliElem *layli_ptr,
              gint nlayers,
              gint32 sel_cnt,
              long  curr,
              char *new_layername
              );

/* ============================================================================
 * p_pitstop_dialog
 *   return -1  on CANCEL
 *           0  on Continue (OK)
 * ============================================================================
 */
static gint
p_pitstop_dialog(gint text_flag, char *filter_procname)
{
  const gchar      *l_env;
  gchar            *l_msg;
  static GapArrButtonArg  l_but_argv[2];
  gint              l_but_argc;
  gint              l_argc;
  static GapArrArg  l_argv[1];
  gint              l_continue;




  l_but_argv[0].but_txt  = _("Continue");
  l_but_argv[0].but_val  = 0;
  l_but_argv[1].but_txt  = GTK_STOCK_CANCEL;
  l_but_argv[1].but_val  = -1;

  l_but_argc = 2;
  l_argc = 0;

  /* optional dialog between both calls (to see the effect of 1.call) */
  l_env = g_getenv("GAP_FILTER_PITSTOP");
  if(l_env != NULL)
  {
     if((*l_env == 'N') || (*l_env == 'n'))
     {
       return 0;  /* continue without question */
     }
  }
  if(text_flag == 0)
  {
     l_msg = g_strdup_printf (_("2nd call of %s\n(define end-settings)"), filter_procname);
  }
  else
  {
     l_msg = g_strdup_printf ( _("Non-Interactive call of %s\n(for all selected layers)"), filter_procname);
  }
  l_continue = gap_arr_std_dialog ( _("Animated Filter Apply"), l_msg,
                                   l_argc,     l_argv,
                                   l_but_argc, l_but_argv, 0);
  g_free (l_msg);

  return (l_continue);

}       /* end p_pitstop_dialog */


/* ---------------------------------
 * p_get_nb_layer_id
 * ---------------------------------
 * get layer_id of the neigbour layer /above or below)
 * the specified ref_layer
 * with the nb_ref parameter of -1 the neigbour layer one above is picked
 *                            , +1 selects the neigbour one below
 * return -1 if there is no such a neigbourlayer
 */
gint32
p_get_nb_layer_id(gint32 image_id, gint32 ref_layer_id, gint32 nb_ref)
{
  gint          l_nlayers;
  gint32       *l_layers_list;
  gint32        l_nb_layer_id;
  gint32        l_idx;
  gint32        l_nb_idx;

  l_nb_layer_id = -1;

  l_layers_list = gimp_image_get_layers(image_id, &l_nlayers);
  if(l_layers_list != NULL)
  {
    for(l_idx = 0; l_idx < l_nlayers; l_idx++)
    {
      if(l_layers_list[l_idx] == ref_layer_id)
      {
        l_nb_idx = l_idx + nb_ref;
        if((l_nb_idx >= 0) && (l_nb_idx < l_nlayers))
        {
          l_nb_layer_id = l_layers_list[l_nb_idx];
        }
        break;
      }
    }

    g_free (l_layers_list);
  }

  return (l_nb_layer_id);
}  /* end p_get_nb_layer_id */



/* ============================================================================
 * gap_mod_get_1st_selected
 *   return index of the 1.st selected layer
 *   or -1 if no selection was found
 * ============================================================================
 */
int
gap_mod_get_1st_selected (GapModLayliElem * layli_ptr, gint nlayers)
{
  int  l_idx;

  for(l_idx = 0; l_idx < nlayers; l_idx++)
  {
    if(layli_ptr[l_idx].selected != FALSE)
    {
      return (l_idx);
    }
  }
  return(-1);
}       /* end gap_mod_get_1st_selected */

/* -----------------------------
 * gap_mod_alloc_layli_group
 * -----------------------------
 * returns   an array of GapModLayliElem struct with information
 *           on all layers in the specified group or subgroup (sel_groupname)
 *           Note that the specified delimiter string is used to split
 *           sel_groupname into group/subgroup  (a typically delimiter  string is "/")
 *           In case sel_groupname is NULL or an empty string, the information
 *           on toplevel layers in the image is delivered.
 * returns   (NULL on error)
 */
GapModLayliElem *
gap_mod_alloc_layli_group(gint32 image_id, gint32 *l_sel_cnt, gint *nlayers,
              gint32 sel_mode,
              gint32 sel_case,
              gint32 sel_invert,
              char *sel_pattern,
              char *sel_groupname,
              char *delimiter)
{
  gint32 *l_layers_list;
  gint32  l_layer_id;
  gint32  l_idx;
  gint32  l_parent_id;
  GapModLayliElem *l_layli_ptr;
  char      *l_layername;

  *l_sel_cnt = 0;
  l_parent_id = gap_image_find_or_create_group_layer(image_id
                          , sel_groupname
                          , delimiter
                          , 0      /* stackposition for the group in case it is created at toplvel */
                          , FALSE  /* do not enableCreate (just want to find id of a group layer) */
                          );
  if(l_parent_id > 0)
  {
    l_layers_list = gimp_item_get_children(l_parent_id, nlayers);
  }
  else
  {
    if(sel_groupname != NULL)
    {
      if (*sel_groupname != '\0')
      {
        /* the not-empty groupname was not found, therefore no layers shall be affected in this image */
        return (NULL);
      }
    }
    l_layers_list = gimp_image_get_layers(image_id, nlayers);
  }

  if(l_layers_list == NULL)
  {
    return(NULL);
  }

  l_layli_ptr = g_new0(GapModLayliElem, (*nlayers));
  if(l_layli_ptr == NULL)
  {
     g_free (l_layers_list);
     return(NULL);
  }

  for(l_idx = 0; l_idx < (*nlayers); l_idx++)
  {
    l_layer_id = l_layers_list[l_idx];
    l_layername = gimp_item_get_name(l_layer_id);
    l_layli_ptr[l_idx].layer_id  = l_layer_id;
    l_layli_ptr[l_idx].visible   = gimp_item_get_visible(l_layer_id);
    l_layli_ptr[l_idx].selected  = gap_match_layer(l_idx,
                                                 l_layername,
                                                 sel_pattern,
                                                 sel_mode,
                                                 sel_case,
                                                 sel_invert,
                                                 *nlayers, l_layer_id);
    if(l_layli_ptr[l_idx].selected != FALSE)
    {
      (*l_sel_cnt)++;  /* count all selected layers */
    }
    if(gap_debug) printf("gap: gap_mod_alloc_layli_group [%d] id:%d, sel:%d name:%s:\n",
                         (int)l_idx, (int)l_layer_id,
                         (int)l_layli_ptr[l_idx].selected, l_layername);
    g_free (l_layername);
  }

  g_free (l_layers_list);

  return( l_layli_ptr );
}               /* end gap_mod_alloc_layli_group */

/* -------------------------
 * gap_mod_alloc_layli
 * -------------------------
 * returns   an array of GapModLayliElem struct with information
 *           on all toplevel layers in the specified image.
 *           (or NULL on error)
 */
GapModLayliElem *
gap_mod_alloc_layli(gint32 image_id, gint32 *l_sel_cnt, gint *nlayers,
              gint32 sel_mode,
              gint32 sel_case,
              gint32 sel_invert,
              char *sel_pattern )
{
  GapModLayliElem *l_layli_ptr;

  l_layli_ptr = gap_mod_alloc_layli_group(image_id, l_sel_cnt, nlayers
              , sel_mode
              , sel_case
              , sel_invert
              , sel_pattern
              , NULL          /* sel_groupname == NULL refers to toplevel image */
              , NULL          /* delimiter not relevant */
              );
  return( l_layli_ptr );
}               /* end gap_mod_alloc_layli */


/* ============================================================================
 * p_raise_layer
 *   raise layer (check if possible before)
 *   (without the check each failed attempt would open an inf window)
 * ============================================================================
 */
static void
p_raise_layer (gint32 image_id, gint32 layer_id, GapModLayliElem * layli_ptr, gint nlayers, gboolean toTop)
{
  if(layli_ptr[0].layer_id == layer_id)  return;   /* is already on top */

  if(! gimp_drawable_has_alpha (layer_id))
  {
    /* implicitly add an alpha channel before we try to raise */
    gimp_layer_add_alpha(layer_id);
  }
  if (toTop == TRUE)
  {
    gimp_image_raise_item_to_top(image_id, layer_id);
  }
  else
  {
    gimp_image_raise_item(image_id, layer_id);
  }
}       /* end p_raise_layer */

static void
p_lower_layer (gint32 image_id, gint32 layer_id, GapModLayliElem * layli_ptr, gint nlayers, gboolean toBottom)
{
  if(layli_ptr[nlayers-1].layer_id == layer_id)  return;   /* is already on bottom */

  if(! gimp_drawable_has_alpha (layer_id))
  {
    /* implicitly add an alpha channel before we try to lower */
    gimp_layer_add_alpha(layer_id);
  }

  if(nlayers > 1)
  {
    if(((layli_ptr[nlayers-2].layer_id == layer_id) || (toBottom == TRUE))
    && (! gimp_drawable_has_alpha (layli_ptr[nlayers-1].layer_id)))
    {
      /* the layer shall move to bottom or is one step above a "bottom-layer without alpha" */
      /* implicitly add an alpha channel before we try to lower */
      gimp_layer_add_alpha(layli_ptr[nlayers-1].layer_id);
    }
  }
  if (toBottom == TRUE)
  {
    gimp_image_lower_item_to_bottom(image_id, layer_id);
  }
  else
  {
    gimp_image_lower_item(image_id, layer_id);
  }
}       /* end p_lower_layer */


/* ============================================================================
 * p_selection_combine
 *
 *    combine selections
 * ============================================================================
 */
static void
p_selection_combine(gint32 image_id
                   ,gint32 master_channel_id
                   ,GimpChannelOps  operation
                   )
{
  gint32   l_new_channel_id;
  gint32   l_sel_channel_id;

  l_sel_channel_id = gimp_image_get_selection(image_id);
  l_new_channel_id = gimp_channel_copy(l_sel_channel_id);

  /* copy the initial selection master_channel_id
   * to the newly create image
   */
  gap_layer_copy_content( l_new_channel_id      /* dst_drawable_id  */
                        , master_channel_id   /* src_drawable_id  */
                        );
  gimp_channel_combine_masks(l_sel_channel_id
                            ,l_new_channel_id
                            ,operation
                            , 0
                            , 0
                            );
  gimp_drawable_delete(l_new_channel_id);

}  /* end p_selection_combine */


/* ---------------------------------
 * p_apply_selection_action
 * ---------------------------------
 * check/perform action that modify the images selection channel.
 * NOTE: actions where the master_channel_id is required
 *       are NOT performed if the image_id is equal to master_image_id
 *       To perform the action on the master image pass -1 as master_image_id.
 *       (this shall be done deferred after processing all the other frame images)
 *
 * return TRUE if action was handled
 *             (this is also TRUE if the action was skiped due to the condition
 *              image_id == master_image_id)
 * return FALSE for all other actions
 */
static gboolean
p_apply_selection_action(gint32 image_id, gint32 action_mode
     , gint32 master_image_id, gint32  master_channel_id)
{
  if(action_mode == GAP_MOD_ACM_SEL_REPLACE)
  {
    /* check if we are processing the master image
     * (must not replace selection by itself)
     */
    if (image_id != master_image_id)
    {
      gint32 l_sel_channel_id;

      gimp_selection_all(image_id);
      l_sel_channel_id = gimp_image_get_selection(image_id);
      /* copy the initial selection master_channel_id
       * to the newly create image
       */
      gap_layer_copy_content( l_sel_channel_id      /* dst_drawable_id  */
                            , master_channel_id   /* src_drawable_id  */
                            );
    }
    return(TRUE);
  }

  if(action_mode == GAP_MOD_ACM_SEL_ADD)
  {
    /* if we are processing the master image,
     * no need to add selection to itself
     */
    if (image_id != master_image_id)
    {
      p_selection_combine(image_id, master_channel_id, GIMP_CHANNEL_OP_ADD);
    }
    return(TRUE);
  }

  if(action_mode == GAP_MOD_ACM_SEL_SUBTRACT)
  {
    /* if we are processing the master image,
     * we must defere action until all other images are handled
     * to keep original master selection intact.
     * (subtract from itself would clear the selection)
     */
    if (image_id != master_image_id)
    {
      p_selection_combine(image_id, master_channel_id, GIMP_CHANNEL_OP_SUBTRACT);
    }
    return(TRUE);
  }

  if(action_mode == GAP_MOD_ACM_SEL_INTERSECT)
  {
    /* if we are processing the master image,
     * intersect with itself can be skipped, because
     * the result will be the same selection as before.
     */
    if (image_id != master_image_id)
    {
      p_selection_combine(image_id, master_channel_id, GIMP_CHANNEL_OP_INTERSECT);
    }
    return(TRUE);
  }

  if(action_mode == GAP_MOD_ACM_SEL_NONE)
  {
    gimp_selection_none(image_id);
    return(TRUE);
  }

  if(action_mode == GAP_MOD_ACM_SEL_ALL)
  {
    gimp_selection_all(image_id);
    return(TRUE);
  }

  if(action_mode == GAP_MOD_ACM_SEL_INVERT)
  {
    gimp_selection_invert(image_id);
    return(TRUE);
  }


  return(FALSE);

}  /* end p_apply_selection_action */


/* ---------------------------------
 * p_merge_selected_toplevel_layers
 * ---------------------------------
 * perform merge of selcted toplevel layer(s)
 *
 * This merge strategy 
 *  o) hides all unselected layers (at top image level)
 *  o) calls the merge visible layers procedure of the GIMP core
 *  o) (optionally) sets a new name for the merged layer
 *  o) restores visiblility of the other unselected layers.
 *
 * returns   0 if all done OK
 *           (or -1 on error)
 */
static int
p_merge_selected_toplevel_layers(gint32 image_id,
              gint merge_mode,
              GapModLayliElem *layli_ptr,
              gint nlayers,
              gint32 sel_cnt,
              long  curr,
              char *new_layername
              )
{
  int     l_idx;
  int     l_rc;
  gint    l_vis_result;
  char    l_name_buff[MAX_LAYERNAME];
  gint32  l_layer_id;

  l_vis_result = FALSE;

  /* set selected layers visible, all others invisible for merge */
  for(l_idx = 0; l_idx < nlayers; l_idx++)
  {
    if(layli_ptr[l_idx].selected == FALSE)
    {
       gimp_item_set_visible(layli_ptr[l_idx].layer_id, FALSE);
    }
    else
    {
       if(gimp_item_get_visible(layli_ptr[l_idx].layer_id))
       {
         /* result will be visible if at least one of the
          * selected layers was visible before
          */
         l_vis_result = TRUE;
       }
       gimp_item_set_visible(layli_ptr[l_idx].layer_id, TRUE);
    }
  }

  /* merge all visible layers (i.e. all selected layers) */
  l_layer_id = gimp_image_merge_visible_layers (image_id, merge_mode);
  if(l_vis_result == FALSE)
  {
     gimp_item_set_visible(l_layer_id, FALSE);
  }

  /* if new_layername is available use that name
   * for the new merged layer
   */
  if (!gap_match_string_is_empty (new_layername))
  {
      gap_match_substitute_framenr(&l_name_buff[0], sizeof(l_name_buff),
                           new_layername, curr);
      gimp_item_set_name(l_layer_id, &l_name_buff[0]);
  }

  /* restore visibility flags after merge */
  for(l_idx = 0; l_idx < nlayers; l_idx++)
  {
    if(layli_ptr[l_idx].selected == FALSE)
    {
      gimp_item_set_visible(layli_ptr[l_idx].layer_id,
                                layli_ptr[l_idx].visible);
    }
  }

  return(0);
     
}  /* end p_merge_selected_toplevel_layers */


/* ------------------------------------
 * p_merge_selected_group_member_layers
 * ------------------------------------
 * perform merge of selcted layer(s) that are all members of 
 * the same layergroup.
 *
 * This merge strategy 
 *  o) creates a temporary image  of same size/type (l_tmp_img_id)
 *  o) copies all selected layers to the temporary image (l_tmp_img_id)
 *  o) calls gimp_image_merge_visible_layers on the temporary image (l_tmp_img_id, mode)
 *  o) copy the merged layer back to the original image
 *      to the same group at the position of the lowest selected layer
 *  o) removes the temporary image
 *  o) removes all selected layers in the original image.
 *
 * returns   0 if all done OK
 *           (or -1 on error)
 */
static int
p_merge_selected_group_member_layers(gint32 image_id,
              gint merge_mode,
              GapModLayliElem *layli_ptr,
              gint nlayers,
              gint32 sel_cnt,
              long  curr,
              char *new_layername)
{
  int     l_idx;
  int     l_rc;
  char    l_name_buff[MAX_LAYERNAME];
  gint32  l_tmp_img_id;
  gint32  l_layer_id;
  gint32  l_new_layer_id;
  gint32  l_merged_layer_id;
  gint32  l_last_selected_layer_id;
  gint32  l_parent_id;
  gint32  l_position;
  gint    l_src_offset_x;
  gint    l_src_offset_y;
  char   *l_name;
    

  /* create a temporary image */
  l_tmp_img_id = gap_image_new_of_samesize(image_id);
  l_name = NULL;
  
  /* copy all selected layers to the temporary image */
  l_last_selected_layer_id = -1;
  for(l_idx = nlayers; l_idx >= 0; l_idx--)
  {
    if(layli_ptr[l_idx].selected != FALSE)
    {
      l_layer_id = layli_ptr[l_idx].layer_id;
      if (l_last_selected_layer_id < 0)
      {
        l_last_selected_layer_id = l_layer_id;
        l_name = gimp_item_get_name(l_last_selected_layer_id);
      }
    
      /* copy the layer from the temp image to the preview multilayer image */
      l_new_layer_id = gap_layer_copy_to_dest_image(l_tmp_img_id,
                                         l_layer_id,
                                         gimp_layer_get_opacity(l_layer_id),
                                         gimp_layer_get_mode(l_layer_id),
                                         &l_src_offset_x,
                                         &l_src_offset_y
                                         );
      
       gimp_image_insert_layer (l_tmp_img_id, l_new_layer_id, 0, 0);
       gimp_layer_set_offsets(l_new_layer_id, l_src_offset_x, l_src_offset_y);
    }
  }
  
  /* merge visible layers in the temporary image */
  l_merged_layer_id = gimp_image_merge_visible_layers (l_tmp_img_id, merge_mode);
  l_new_layer_id = gap_layer_copy_to_dest_image(image_id,
                                         l_merged_layer_id,
                                         gimp_layer_get_opacity(l_merged_layer_id),
                                         gimp_layer_get_mode(l_merged_layer_id),
                                         &l_src_offset_x,
                                         &l_src_offset_y
                                         );
  l_position = gimp_image_get_item_position (image_id, l_last_selected_layer_id);
  l_parent_id = gimp_item_get_parent(l_last_selected_layer_id);
  gimp_image_insert_layer (image_id, l_new_layer_id, l_parent_id, l_position);
  gimp_layer_set_offsets(l_new_layer_id, l_src_offset_x, l_src_offset_y);

  /* remove the selected layers from the original image */
  for(l_idx = 0; l_idx < nlayers; l_idx++)
  {
    if(layli_ptr[l_idx].selected != FALSE)
    {
      gimp_image_remove_layer(image_id, layli_ptr[l_idx].layer_id);
    }
  }

  /* if new_layername is available use that name
   * for the new merged layer
   */
  if (!gap_match_string_is_empty (new_layername))
  {
      gap_match_substitute_framenr(&l_name_buff[0], sizeof(l_name_buff),
                           new_layername, curr);
      gimp_item_set_name(l_new_layer_id, &l_name_buff[0]);
  }
  else if (l_name != NULL)
  {
    gimp_item_set_name(l_new_layer_id, l_name);
  }
  
  if (l_name != NULL)
  {
    g_free(l_name);
  }
  

  /* remove the temporary image */
  gap_image_delete_immediate(l_tmp_img_id);
  return(0);


}  /* end p_merge_selected_group_member_layers */


/* ---------------------------------
 * p_apply_action2
 * ---------------------------------
 *    perform function (defined by action_mode)
 *    on all selcted layer(s)
 *
 *    note: some functions operate on the images selection channel
 *          and may be skiped when processing the active master_image_id
 *
 * returns   0 if all done OK
 *           (or -1 on error)
 */
static int
p_apply_action2(gint32 image_id,
              gint32 action_mode,
              GapModLayliElem *layli_ptr,
              gint nlayers,
              gint32 sel_cnt,

              long  from,
              long  to,
              long  curr,
              char *new_layername,
              char *filter_procname,
              gint32 master_image_id,
              gint32 new_position,
              char *new_groupname,
              char *delimiter
              )
{
  int   l_idx;
  int   l_rc;
  gint32  l_layer_id;
  gint32  l_layermask_id;
  gint32  l_new_layer_id;
  gint    l_merge_mode;
  char    l_name_buff[MAX_LAYERNAME];

  if(gap_debug) printf("gap: p_apply_action2 START\n");

  l_rc = 0;

  l_merge_mode = -44; /* none of the flatten modes */

 
  if(action_mode == GAP_MOD_ACM_MERGE_EXPAND) l_merge_mode = GAP_RANGE_OPS_FLAM_MERG_EXPAND;
  if(action_mode == GAP_MOD_ACM_MERGE_IMG)    l_merge_mode = GAP_RANGE_OPS_FLAM_MERG_CLIP_IMG;
  if(action_mode == GAP_MOD_ACM_MERGE_BG)     l_merge_mode = GAP_RANGE_OPS_FLAM_MERG_CLIP_BG;

  /* check and perform selection related actions
   * that operate on the image (and not on selected layer(s)
   */
  {
    gint32  master_channel_id;
    gboolean action_was_applied;

    master_channel_id = gimp_image_get_selection(master_image_id);
    action_was_applied = p_apply_selection_action(image_id
                                                , action_mode
                                                , master_image_id
                                                , master_channel_id
                                                );
    if(action_was_applied)
    {
      return l_rc;
    }
  }


  /* some merge actions require special processing per image
   * and are handled here
   */
  if(l_merge_mode != (-44))
  {
     gint32 l_first_selected_layer_id;
     gint32 l_parent_id;

     l_idx = gap_mod_get_1st_selected (layli_ptr, nlayers);
     l_first_selected_layer_id = layli_ptr[l_idx].layer_id;

     if(gap_debug)
     {
       printf("merge: sel_cnt:%d l_first_selected_layer_id:%d %s\n"
            , sel_cnt
            , l_first_selected_layer_id
            , gimp_item_get_name(l_first_selected_layer_id)
            );
     }
     if(sel_cnt < 2)
     {
       if(sel_cnt != 1)
       {
         return(0);  /* OK, nothing to merge */
       }

       if (!gimp_item_is_group(l_first_selected_layer_id))
       {
         return(0);  /* there is only one layer selected that is not a group, nothing to merge */
       }
       
     }
     
     l_parent_id = gimp_item_get_parent(l_first_selected_layer_id);
     if (l_parent_id > 0)
     {
       l_rc = p_merge_selected_group_member_layers(image_id
                                              ,l_merge_mode
                                              ,layli_ptr
                                              ,nlayers
                                              ,sel_cnt
                                              ,curr
                                              ,new_layername
                                              );
     }
     else
     {
       l_rc = p_merge_selected_toplevel_layers(image_id
                                              ,l_merge_mode
                                              ,layli_ptr
                                              ,nlayers
                                              ,sel_cnt
                                              ,curr
                                              ,new_layername
                                              );
     }
     return (l_rc);
  }

  /* -----------------------------*/
  /* non-merge actions (except merge down) require calls foreach selected layer */
  for(l_idx = 0; (l_idx < nlayers) && (l_rc == 0); l_idx++)
  {
    l_layer_id = layli_ptr[l_idx].layer_id;

    /* apply function defined by action_mode */
    if(layli_ptr[l_idx].selected != FALSE)
    {
      if(gap_debug) printf("gap: p_apply_action2 on selected LayerID:%d layerstack:%d\n",
                           (int)l_layer_id, (int)l_idx);
      switch(action_mode)
      {
        case GAP_MOD_ACM_MERGE_DOWN_EXPAND:
          gimp_image_merge_down(image_id, l_layer_id, GAP_RANGE_OPS_FLAM_MERG_EXPAND);
          break;
        case GAP_MOD_ACM_MERGE_DOWN_IMG:
          gimp_image_merge_down(image_id, l_layer_id, GAP_RANGE_OPS_FLAM_MERG_CLIP_IMG);
          break;
        case GAP_MOD_ACM_MERGE_DOWN_BG:
          gimp_image_merge_down(image_id, l_layer_id, GAP_RANGE_OPS_FLAM_MERG_CLIP_BG);
          break;
        case GAP_MOD_ACM_SET_VISIBLE:
          gimp_item_set_visible(l_layer_id, TRUE);
          break;
        case GAP_MOD_ACM_SET_INVISIBLE:
          gimp_item_set_visible(l_layer_id, FALSE);
          break;
        case GAP_MOD_ACM_SET_LINKED:
          gimp_item_set_linked(l_layer_id, TRUE);
          break;
        case GAP_MOD_ACM_SET_UNLINKED:
          gimp_item_set_linked(l_layer_id, FALSE);
          break;
        case GAP_MOD_ACM_RAISE:
          p_raise_layer(image_id, l_layer_id, layli_ptr, nlayers, FALSE);
          break;
        case GAP_MOD_ACM_LOWER:
          p_lower_layer(image_id, l_layer_id, layli_ptr, nlayers, FALSE);
          break;
        case GAP_MOD_ACM_RAISE_TOP:
          p_raise_layer(image_id, l_layer_id, layli_ptr, nlayers, TRUE);
          break;
        case GAP_MOD_ACM_LOWER_BOTTOM:
          p_lower_layer(image_id, l_layer_id, layli_ptr, nlayers, TRUE);
          break;
        case GAP_MOD_ACM_APPLY_FILTER:
          l_rc = gap_filt_pdb_call_plugin(filter_procname,
                               image_id,
                               l_layer_id,
                               GIMP_RUN_WITH_LAST_VALS);
          if(gap_debug) printf("gap: p_apply_action2 FILTER:%s rc =%d\n",
                                filter_procname, (int)l_rc);
          break;
        case GAP_MOD_ACM_APPLY_FILTER_ON_LAYERMASK:
          if(gimp_layer_get_mask(l_layer_id) >= 0)
          {
            l_rc = gap_filt_pdb_call_plugin(filter_procname,
                               image_id,
                               gimp_layer_get_mask(l_layer_id),
                               GIMP_RUN_WITH_LAST_VALS);
          }
          break;
        case GAP_MOD_ACM_DUPLICATE:
          {
            gint32 l_parent_id;
            gint32 l_position;
            
            l_parent_id = gimp_item_get_parent(l_layer_id);
            l_position = gimp_image_get_item_position(image_id, l_layer_id);
            l_new_layer_id = gimp_layer_copy(l_layer_id);
            gimp_image_insert_layer (image_id, l_new_layer_id, l_parent_id, l_position);
            if (!gap_match_string_is_empty (new_layername))
            {
              gap_match_substitute_framenr(&l_name_buff[0], sizeof(l_name_buff),
                                   new_layername, curr);
              gimp_item_set_name(l_new_layer_id, &l_name_buff[0]);
            }
          }
          break;
        case GAP_MOD_ACM_DELETE:
          gimp_image_remove_layer(image_id, l_layer_id);
          break;
        case GAP_MOD_ACM_RENAME:
          gap_match_substitute_framenr(&l_name_buff[0], sizeof(l_name_buff),
                                new_layername, curr);
          gimp_item_set_name(l_layer_id, &l_name_buff[0]);
          break;

        case GAP_MOD_ACM_SEL_ALPHA:
          if(gimp_drawable_has_alpha(l_layer_id))
          {
            gimp_selection_layer_alpha(l_layer_id);
          }
          else
          {
            gimp_selection_none(image_id);
          }
          break;


        case GAP_MOD_ACM_SEL_SAVE:
          {
            gint32 l_sel_channel_id;
            l_sel_channel_id = gimp_selection_save(image_id);
            if(*new_layername != '\0')
            {
              gap_match_substitute_framenr(&l_name_buff[0], sizeof(l_name_buff),
                                           new_layername, curr);
              gimp_item_set_name(l_sel_channel_id, &l_name_buff[0]);
            }
          }
          break;
        case GAP_MOD_ACM_SEL_LOAD:
          {
            if(*new_layername != '\0')
            {
              gint   *l_channels;
              gint    n_channels;
              gint    l_ii;
              gchar  *l_channelname;

              gap_match_substitute_framenr(&l_name_buff[0], sizeof(l_name_buff),
                                           new_layername, curr);

              l_channels = gimp_image_get_channels(image_id, &n_channels);
              for(l_ii=0; l_ii < n_channels; l_ii++)
              {
                l_channelname = gimp_item_get_name(l_channels[l_ii]);
                if(l_channelname)
                {
                  if(strcmp(l_channelname,&l_name_buff[0] ) == 0)
                  {
                    gimp_image_select_item(image_id, GIMP_CHANNEL_OP_REPLACE, l_channels[l_ii]);
                    g_free(l_channelname);
                    break;
                  }
                  g_free(l_channelname);
                }
              }
              if(l_channels)
              {
                g_free(l_channels);
              }
            }
          }
          break;
        case GAP_MOD_ACM_SEL_DELETE:
          {
            if(*new_layername != '\0')
            {
              gint   *l_channels;
              gint    n_channels;
              gint    l_ii;
              gchar  *l_channelname;

              gap_match_substitute_framenr(&l_name_buff[0], sizeof(l_name_buff),
                                           new_layername, curr);

              l_channels = gimp_image_get_channels(image_id, &n_channels);
              for(l_ii=0; l_ii < n_channels; l_ii++)
              {
                l_channelname = gimp_item_get_name(l_channels[l_ii]);
                if(l_channelname)
                {
                  if(strcmp(l_channelname,&l_name_buff[0] ) == 0)
                  {
                    gimp_image_remove_channel(image_id, l_channels[l_ii]);
                  }
                  g_free(l_channelname);
                }
              }
              if(l_channels)
              {
                g_free(l_channels);
              }
            }
          }
          break;
        case GAP_MOD_ACM_ADD_ALPHA:
          if(!gimp_drawable_has_alpha(l_layer_id))
          {
            gimp_layer_add_alpha(l_layer_id);
          }
          break;
        case GAP_MOD_ACM_LMASK_WHITE:
          if(gimp_layer_get_mask(l_layer_id) >= 0)
          {
            gimp_layer_remove_mask (l_layer_id, GIMP_MASK_APPLY);
          }
          l_layermask_id = gimp_layer_create_mask(l_layer_id, GIMP_ADD_WHITE_MASK);
          gimp_layer_add_mask(l_layer_id, l_layermask_id);
          break;
        case GAP_MOD_ACM_LMASK_BLACK:
          if(gimp_layer_get_mask(l_layer_id) >= 0)
          {
            gimp_layer_remove_mask (l_layer_id, GIMP_MASK_APPLY);
          }
          l_layermask_id = gimp_layer_create_mask(l_layer_id, GIMP_ADD_BLACK_MASK);
          gimp_layer_add_mask(l_layer_id, l_layermask_id);
          break;
        case GAP_MOD_ACM_LMASK_ALPHA:
          if(gimp_layer_get_mask(l_layer_id) >= 0)
          {
            gimp_layer_remove_mask (l_layer_id, GIMP_MASK_APPLY);
          }
          l_layermask_id = gimp_layer_create_mask(l_layer_id, GIMP_ADD_ALPHA_MASK);
          gimp_layer_add_mask(l_layer_id, l_layermask_id);
          break;
        case GAP_MOD_ACM_LMASK_TALPHA:
          if(gimp_layer_get_mask(l_layer_id) >= 0)
          {
            gimp_layer_remove_mask (l_layer_id, GIMP_MASK_APPLY);
          }
          l_layermask_id = gimp_layer_create_mask(l_layer_id, GIMP_ADD_ALPHA_TRANSFER_MASK);
          gimp_layer_add_mask(l_layer_id, l_layermask_id);
          break;
        case GAP_MOD_ACM_LMASK_SEL:
          if(gimp_layer_get_mask(l_layer_id) >= 0)
          {
            gimp_layer_remove_mask (l_layer_id, GIMP_MASK_APPLY);
          }
          l_layermask_id = gimp_layer_create_mask(l_layer_id, GIMP_ADD_SELECTION_MASK);
          gimp_layer_add_mask(l_layer_id, l_layermask_id);
          break;
        case GAP_MOD_ACM_LMASK_BWCOPY:
          if(gimp_layer_get_mask(l_layer_id) >= 0)
          {
            gimp_layer_remove_mask (l_layer_id, GIMP_MASK_APPLY);
          }
          l_layermask_id = gimp_layer_create_mask(l_layer_id, GIMP_ADD_COPY_MASK);
          gimp_layer_add_mask(l_layer_id, l_layermask_id);
          break;
        case GAP_MOD_ACM_LMASK_DELETE:
          if(gimp_layer_get_mask(l_layer_id) >= 0)
          {
            gimp_layer_remove_mask (l_layer_id, GIMP_MASK_DISCARD);
          }
          break;
        case GAP_MOD_ACM_LMASK_APPLY:
          if(gimp_layer_get_mask(l_layer_id) >= 0)
          {
            gimp_layer_remove_mask (l_layer_id, GIMP_MASK_APPLY);
          }
          break;
        case GAP_MOD_ACM_LMASK_COPY_FROM_UPPER_LMASK:
        case GAP_MOD_ACM_LMASK_COPY_FROM_LOWER_LMASK:
          /* create black mask per default */
          if(gimp_layer_get_mask(l_layer_id) >= 0)
          {
            gimp_layer_remove_mask (l_layer_id, GIMP_MASK_APPLY);
          }
          l_layermask_id = gimp_layer_create_mask(l_layer_id, GIMP_ADD_BLACK_MASK);
          gimp_layer_add_mask(l_layer_id, l_layermask_id);
          /* get layermask_id from upper neigbour layer (if there is any) */
          if(l_layermask_id >= 0)
          {
            gint32 l_nb_mask_id;
            gint32 l_nb_layer_id;
            gint32 l_nb_ref;
            gint32 l_fsel_layer_id;
            gint32 l_nb_had_layermask;

            l_nb_ref = 1;
            if(action_mode == GAP_MOD_ACM_LMASK_COPY_FROM_UPPER_LMASK)
            {
              l_nb_ref = -1;
            }
            l_nb_layer_id = p_get_nb_layer_id(image_id, l_layer_id, l_nb_ref);
            if(l_nb_layer_id >= 0)
            {
              l_nb_mask_id = gimp_layer_get_mask(l_nb_layer_id);
              l_nb_had_layermask = TRUE;
              if(l_nb_mask_id < 0)
              {
                /* the referenced neigbour layer had no layermask
                 * in this case we create one as bw-copy temporary
                 */
                l_nb_had_layermask = FALSE;
                l_nb_mask_id = gimp_layer_create_mask(l_nb_layer_id, GIMP_ADD_COPY_MASK);
                gimp_layer_add_mask(l_nb_layer_id, l_nb_mask_id);
              }

              if(l_nb_mask_id >= 0)
              {
                /* the referenced neigbour layer has a layermask
                 * we can copy from this mask now
                 */
                gimp_selection_none(image_id);
                gimp_edit_copy(l_nb_mask_id);
                l_fsel_layer_id = gimp_edit_paste(l_layermask_id, FALSE);
                gimp_floating_sel_anchor(l_fsel_layer_id);

                if(l_nb_had_layermask == FALSE)
                {
                  /* remove the temporary created layermask in the neigbour layer */
                  gimp_layer_remove_mask (l_nb_layer_id, GIMP_MASK_DISCARD);
                }
              }
            }
          }
          break;
        case GAP_MOD_ACM_LMASK_INVERT:
          if(gimp_layer_get_mask(l_layer_id) >= 0)
          {
            gimp_invert(gimp_layer_get_mask(l_layer_id));
          }
          break;
        case GAP_MOD_ACM_CREATE_LAYER_FROM_OPACITY:
          /* create a mask layer that represents current opacity
           * (including both the alpha channel and layermask (if exists)
           */
          gap_layer_create_layer_from_alpha(l_layer_id
                                               , image_id
                                               , NULL        /* name_prefix */
                                               , _("_msk")   /* name suffix */
                                               , TRUE        /* applyExistingLayermask */
                                               , FALSE       /* useTransferAlpha */
                                               );
          break;
        case GAP_MOD_ACM_CREATE_LAYER_FROM_ALPHA:
          /* create a mask layer that is a copy of the the alpha channel
           * (ignores an existing layermask)
           */
          gap_layer_create_layer_from_alpha(l_layer_id
                                               , image_id
                                               , NULL        /* name_prefix */
                                               , _("_msk")   /* name suffix */
                                               , FALSE       /* applyExistingLayermask */
                                               , FALSE       /* useTransferAlpha */
                                               );
          break;
        case GAP_MOD_ACM_CREATE_LAYER_FROM_LMASK:
          /* create a mask layer that is a copy of the layermask
           * (ignores the alpha channel)
           */
          gap_layer_create_layer_from_layermask(l_layer_id
                                               , image_id
                                               , NULL        /* name_prefix */
                                               , _("_msk")   /* name suffix */
                                               );
          break;
        case GAP_MOD_ACM_REORDER_LAYER:
          l_name_buff[0] = '\0';
          if (new_layername != NULL)
          {
            if (*new_layername != '\0')
            {
              gap_match_substitute_framenr(&l_name_buff[0], sizeof(l_name_buff),
                                new_layername, curr);
            }
          }
          gap_image_reorder_layer(image_id, l_layer_id
                         , new_position
                         , new_groupname
                         , delimiter
                         , TRUE    /* enable automatically group creation */
                         , &l_name_buff[0]
                         );
          break;
        case GAP_MOD_ACM_NEW_LAYER_GROUP:
          gap_image_find_or_create_group_layer(image_id
                                          , new_groupname
                                          , delimiter
                                          , new_position
                                          , TRUE   /* enableGroupCreation */
                                          );
          break;
        case GAP_MOD_ACM_SET_MODE_NORMAL:
          gimp_layer_set_mode(l_layer_id, GIMP_NORMAL_MODE);
          break;
        case GAP_MOD_ACM_SET_MODE_DISSOLVE:
          gimp_layer_set_mode(l_layer_id, GIMP_DISSOLVE_MODE);
          break;
        case GAP_MOD_ACM_SET_MODE_MULTIPLY:
          gimp_layer_set_mode(l_layer_id, GIMP_MULTIPLY_MODE);
          break;
        case GAP_MOD_ACM_SET_MODE_DIVIDE:
          gimp_layer_set_mode(l_layer_id, GIMP_DIVIDE_MODE);
          break;
        case GAP_MOD_ACM_SET_MODE_SCREEN:
          gimp_layer_set_mode(l_layer_id, GIMP_SCREEN_MODE);
          break;
        case GAP_MOD_ACM_SET_MODE_OVERLAY:
          gimp_layer_set_mode(l_layer_id, GIMP_OVERLAY_MODE);
          break;
        case GAP_MOD_ACM_SET_MODE_DIFFERENCE:
          gimp_layer_set_mode(l_layer_id, GIMP_DIFFERENCE_MODE);
          break;
        case GAP_MOD_ACM_SET_MODE_ADDITION:
          gimp_layer_set_mode(l_layer_id, GIMP_ADDITION_MODE);
          break;
        case GAP_MOD_ACM_SET_MODE_SUBTRACT:
          gimp_layer_set_mode(l_layer_id, GIMP_SUBTRACT_MODE);
          break;
        case GAP_MOD_ACM_SET_MODE_DARKEN_ONLY:
          gimp_layer_set_mode(l_layer_id, GIMP_DARKEN_ONLY_MODE);
          break;
        case GAP_MOD_ACM_SET_MODE_LIGHTEN_ONLY:
          gimp_layer_set_mode(l_layer_id, GIMP_LIGHTEN_ONLY_MODE);
          break;
        case GAP_MOD_ACM_SET_MODE_DODGE:
          gimp_layer_set_mode(l_layer_id, GIMP_DODGE_MODE);
          break;
        case GAP_MOD_ACM_SET_MODE_BURN:
          gimp_layer_set_mode(l_layer_id, GIMP_BURN_MODE);
          break;
        case GAP_MOD_ACM_SET_MODE_HARDLIGHT:
          gimp_layer_set_mode(l_layer_id, GIMP_HARDLIGHT_MODE);
          break;
        case GAP_MOD_ACM_SET_MODE_SOFTLIGHT:
          gimp_layer_set_mode(l_layer_id, GIMP_SOFTLIGHT_MODE);
          break;
        case GAP_MOD_ACM_SET_MODE_COLOR_ERASE:
          gimp_layer_set_mode(l_layer_id, GIMP_COLOR_ERASE_MODE);
          break;
        case GAP_MOD_ACM_SET_MODE_GRAIN_EXTRACT_MODE:
          gimp_layer_set_mode(l_layer_id, GIMP_GRAIN_EXTRACT_MODE);
          break;
        case GAP_MOD_ACM_SET_MODE_GRAIN_MERGE_MODE:
          gimp_layer_set_mode(l_layer_id, GIMP_GRAIN_MERGE_MODE);
          break;
        case GAP_MOD_ACM_SET_MODE_HUE_MODE:
          gimp_layer_set_mode(l_layer_id, GIMP_HUE_MODE);
          break;
        case GAP_MOD_ACM_SET_MODE_SATURATION_MODE:
          gimp_layer_set_mode(l_layer_id, GIMP_SATURATION_MODE);
          break;
        case GAP_MOD_ACM_SET_MODE_COLOR_MODE:
          gimp_layer_set_mode(l_layer_id, GIMP_COLOR_MODE);
          break;
        case GAP_MOD_ACM_SET_MODE_VALUE_MODE:
          gimp_layer_set_mode(l_layer_id, GIMP_VALUE_MODE);
          break;
        case GAP_MOD_ACM_RESIZE_TO_IMG:
          gimp_layer_resize_to_image_size (l_layer_id);
          break;
        default:
          break;
      }
    }
  }

  return (l_rc);
}  /* end p_apply_action2 */              


/* ---------------------------------
 * p_apply_action
 * ---------------------------------
 */
static int
p_apply_action(gint32 image_id,
              gint32 action_mode,
              GapModLayliElem *layli_ptr,
              gint nlayers,
              gint32 sel_cnt,

              long  from,
              long  to,
              long  curr,
              char *new_layername,
              char *filter_procname,
              gint32 master_image_id,
              gint32 new_position,
              char *new_groupname,
              char *delimiter
              )
{
  int l_rc;
  
  gimp_image_undo_group_start (image_id);
  
  l_rc = p_apply_action2(image_id,
              action_mode,
              layli_ptr,
              nlayers,
              sel_cnt,
              from,
              to,
              curr,
              new_layername,
              filter_procname,
              master_image_id,
              new_position,
              new_groupname,
              delimiter
              );
  gimp_image_undo_group_end (image_id);
  return l_rc;

}  /* end p_apply_action */


/* ============================================================================
 * p_do_filter_dialogs
 *    additional dialog steps
 *    a) gap_pdb_browser (select the filter)
 *    b) 1st interactive filtercall
 *    c) 1st pitstop dialog
 * ============================================================================
 */
static int
p_do_filter_dialogs(GapAnimInfo *ainfo_ptr,
                    gint32 image_id, gint32 *dpy_id,
                    GapModLayliElem * layli_ptr, gint nlayers ,
                    char *filter_procname, int filt_len,
                    gint *plugin_data_len,
                    gint32 *accelCharacteristic,
                    gboolean operate_on_layermask
                    )
{
  GapDbBrowserResult  l_browser_result;
  gint32   l_drawable_id;
  int      l_rc;
  int      l_idx;
  static char l_key_from[512];
  static char *canonical_proc_name;

  l_browser_result.accelCharacteristic = GAP_ACCEL_CHAR_LINEAR;

  /* GAP-PDB-Browser Dialog */
  /* ---------------------- */
  if(gap_db_browser_dialog( _("Select Filter for Animated Apply on Frames"),
                            _("Apply"),
                            TRUE, /* showAccelerationCharacteristic */
                            gap_filt_pdb_constraint_proc,
                            gap_filt_pdb_constraint_proc_sel1,
                            gap_filt_pdb_constraint_proc_sel2,
                            &l_browser_result,
                            image_id,
                            GAP_DB_BROWSER_MODFRAMES_HELP_ID) < 0)
  {
      if(gap_debug) printf("DEBUG: gap_db_browser_dialog cancelled\n");
      return -1;
  }

  canonical_proc_name = gimp_canonicalize_identifier(l_browser_result.selected_proc_name);

  strncpy(filter_procname, canonical_proc_name, filt_len-1);
  filter_procname[filt_len-1] = '\0';
  g_free(canonical_proc_name);

  /* invert acceleration to deceleration and vice versa
   * (because processing runs backwards from total_frames down to 0)
   */
  *accelCharacteristic = (-1 * l_browser_result.accelCharacteristic);

  /* 1.st INTERACTIV Filtercall dialog */
  /* --------------------------------- */
  /* check for the Plugin */

  l_rc = gap_filt_pdb_procedure_available(filter_procname, GAP_PTYP_CAN_OPERATE_ON_DRAWABLE);
  if(l_rc < 0)
  {
     printf("ERROR: Plugin not available or wrong type %s\n", filter_procname);
     return -1;
  }

  /* get 1.st selected layer (of 1.st handled frame in range ) */
  l_idx = gap_mod_get_1st_selected(layli_ptr, nlayers);
  if(l_idx < 0)
  {
     printf("ERROR: No layer selected in 1.st handled frame\n");
     return (-1);
  }
  l_drawable_id = layli_ptr[l_idx].layer_id;
  if(operate_on_layermask)
  {
    l_drawable_id = gimp_layer_get_mask(layli_ptr[l_idx].layer_id);
    if(l_drawable_id < 0)
    {
      printf("ERROR: selected layer has no layermask\n");
      return -1;
    }
  }

  /* open a view for the 1.st handled frame */
  *dpy_id = gimp_display_new (image_id);

  l_rc = gap_filt_pdb_call_plugin(filter_procname, image_id, l_drawable_id, GIMP_RUN_INTERACTIVE);

  /* OOPS: cant delete the display here, because
   *       closing the last display seems to free up
   *       at least parts of the image,
   *       and causes crashes if the image_id is used
   *       in further gimp procedures
   */
  /* gimp_display_delete(*dpy_id); */

  /* get values, then store with suffix "-ITER-FROM" */
  *plugin_data_len = gap_filt_pdb_get_data(filter_procname);
  if(*plugin_data_len > 0)
  {
     g_snprintf(l_key_from, sizeof(l_key_from), "%s%s", filter_procname, GAP_ITER_FROM_SUFFIX);
     gap_filt_pdb_set_data(l_key_from, *plugin_data_len);
  }
  else
  {
    return (-1);
  }

  if(*accelCharacteristic == GAP_ACCEL_CHAR_NONE)
  {
    return (p_pitstop_dialog(1, filter_procname));
  }

  return(0);
}       /* end p_do_filter_dialogs */


/* ============================================================================
 * p_do_2nd_filter_dialogs
 *    d) [ 2nd interactive filtercall
 *    e)   2nd pitstop dialog ]
 *
 *   (temporary) open the last frame of the range
 *   get its 1.st selected laye
 *   and do the Interctive Filtercall (to get the end-values)
 *
 * then close everything (without save).
 * (the last frame will be processed later, with all its selected layers)
 * ============================================================================
 */
static gint
p_do_2nd_filter_dialogs(char *filter_procname,
                        gint32  accelCharacteristic,
                        char *last_frame_filename,
                        gint32 sel_mode, gint32 sel_case,
                        gint32 sel_invert, char *sel_pattern,
                        gboolean operate_on_layermask,
                        char *sel_groupname, char *delimiter
                       )
{
  gint32   l_drawable_id;
  gint32   l_dpy_id;
  int      l_rc;
  int      l_idx;
  static char l_key_to[512];
  static char l_key_from[512];
  gint32  l_last_image_id;
  GapModLayliElem *l_layli_ptr;
  gint       l_nlayers;
  gint32     l_sel_cnt;
  gint       l_plugin_data_len;

  l_layli_ptr = NULL;
  l_rc = -1;       /* assume cancel or error */
  l_last_image_id = -1;
  l_dpy_id = -1;

  /* 2.nd INTERACTIV Filtercall dialog */
  /* --------------------------------- */
  if(last_frame_filename == NULL)
  {
    return (-1);  /* there is no 2.nd frame for 2.nd filter call */
  }

  if(p_pitstop_dialog(0, filter_procname) < 0)
     goto cleanup;

  /* load last frame into temporary image */
  l_last_image_id = gap_lib_load_image(last_frame_filename);
  if (l_last_image_id < 0)
     goto cleanup;

  /* get informations (id, visible, selected) about all layers */
  l_layli_ptr = gap_mod_alloc_layli_group(l_last_image_id, &l_sel_cnt, &l_nlayers,
                               sel_mode, sel_case, sel_invert, sel_pattern,
                               sel_groupname, delimiter);

  if (l_layli_ptr == NULL)
     goto cleanup;

  /* get 1.st selected layer (of last handled frame in range ) */
  l_idx = gap_mod_get_1st_selected(l_layli_ptr, l_nlayers);
  if(l_idx < 0)
  {
     g_message (_("Modify Layers cancelled: No layer selected in last handled frame"));
     goto cleanup;
  }
  l_drawable_id = l_layli_ptr[l_idx].layer_id;
  if(operate_on_layermask)
  {
    l_drawable_id = gimp_layer_get_mask(l_layli_ptr[l_idx].layer_id);
    if(l_drawable_id < 0)
    {
      g_message (_("Modify Layers cancelled: first selected layer \"%s\"\nin last frame has no layermask"),
                    gimp_item_get_name(l_layli_ptr[l_idx].layer_id)
                    );
      goto cleanup;
    }
  }

  /* open a view for the last handled frame */
  l_dpy_id = gimp_display_new (l_last_image_id);

  /* 2.nd INTERACTIV Filtercall dialog */
  /* --------------------------------- */
  l_rc = gap_filt_pdb_call_plugin(filter_procname, l_last_image_id, l_drawable_id, GIMP_RUN_INTERACTIVE);

  /* get values, then store with suffix "-ITER-TO" */
  l_plugin_data_len = gap_filt_pdb_get_data(filter_procname);
  if(l_plugin_data_len <= 0)
     goto cleanup;

   g_snprintf(l_key_to, sizeof(l_key_to), "%s%s", filter_procname, GAP_ITER_TO_SUFFIX);
   gap_filt_pdb_set_data(l_key_to, l_plugin_data_len);

   /* get FROM values */
   g_snprintf(l_key_from, sizeof(l_key_from), "%s%s", filter_procname, GAP_ITER_FROM_SUFFIX);
   l_plugin_data_len = gap_filt_pdb_get_data(l_key_from);
   gap_filt_pdb_set_data(filter_procname, l_plugin_data_len);

  l_rc = p_pitstop_dialog(1, filter_procname);

cleanup:
  if(l_last_image_id >= 0)
  {
    gimp_image_delete(l_last_image_id);
  }
  if(l_dpy_id >= 0)
  {
    gimp_display_delete(l_dpy_id);
  }
  if(l_layli_ptr != NULL)
  {
    g_free(l_layli_ptr);
  }

  return (l_rc);


}       /* end p_do_2nd_filter_dialogs */


/* --------------------------
 * gap_mod_frames_modify
 * --------------------------
 *   foreach frame of the range (given by range_from and range_to)
 *   perform function defined by action_mode
 *   on all selected layer(s) described by sel_mode, sel_case
 *                                         sel_invert and sel_pattern
 *   the scope of selectable layers can be image toplevel layers
 *       when sel_groupname is NULL or empty string.
 *   or the members of the specified layergroup
 *       specified by the sel_groupname string
 *       that may refere to a nested group.
 *       note that groupname/subgroupname are splitted by the specified delimter string
 *       (typical "/" is used as delimiter)
 *
 * returns   0 if all done OK
 *           (or -1 on error or cancel)
 */
gint32
gap_mod_frames_modify(GapAnimInfo *ainfo_ptr,
                   long range_from, long range_to,
                   gint32 action_mode, gint32 sel_mode,
                   gint32 sel_case, gint32 sel_invert,
                   char *sel_pattern, char *new_layername,
                   gint32 new_position,
                   char *new_groupname, char *sel_groupname, char *delimiter,
                   GtkWidget *progress_bar,
                   gboolean *run_flag)
{
  long    l_cur_frame_nr;
  long    l_step, l_begin, l_end;
  gint32  l_tmp_image_id;
  gint32  l_dpy_id;
  gint       l_nlayers;
  gdouble    l_percentage, l_percentage_step;
  int        l_rc;
  int        l_idx;
  gint32     l_sel_cnt;
  GapModLayliElem *l_layli_ptr;

  GimpParam     *l_params;
  gint        l_retvals;
  gint        l_plugin_data_len;
  char       l_filter_procname[256];
  char      *l_plugin_iterator;
  gdouble    l_cur_step;
  gint       l_total_steps;
  gint32        accelCharacteristic;
  char         *l_last_frame_filename;
  gint          l_count;
  gboolean      l_operating_on_current_image;
  gboolean      l_operate_on_layermask;
  gboolean      l_frame_found;




  if(gap_debug)
  {
    printf("gap: gap_mod_frames_modify START, action_mode=%d  sel_mode=%d case=%d, invert=%d patt:%s:\n",
        (int)action_mode, (int)sel_mode, (int)sel_case, (int)sel_invert, sel_pattern);
  }

  l_operate_on_layermask = FALSE;
  l_percentage = 0.0;
  if(ainfo_ptr->run_mode == GIMP_RUN_INTERACTIVE)
  {
    gimp_progress_init( _("Modifying frames/layer(s)..."));
  }

  l_operating_on_current_image = TRUE;
  l_begin = range_from;
  l_end   = range_to;
  l_tmp_image_id = -1;
  l_layli_ptr = NULL;
  l_rc = 0;
  l_plugin_iterator = NULL;
  l_plugin_data_len = 0;
  accelCharacteristic = GAP_ACCEL_CHAR_NONE;
  l_dpy_id = -1;
  l_last_frame_filename = NULL;

  /* init step direction */
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

    l_total_steps = l_begin - l_end;
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

    l_total_steps = l_end - l_begin;
  }

  l_cur_step = l_total_steps;

  l_cur_frame_nr = l_begin;
  while(1)              /* loop foreach frame in range */
  {
    if(gap_debug)
    {
      printf("gap_mod_frames_modify While l_cur_frame_nr = %d\n", (int)l_cur_frame_nr);
    }

    /* build the frame name */
    if(ainfo_ptr->new_filename != NULL) g_free(ainfo_ptr->new_filename);
    ainfo_ptr->new_filename = gap_lib_alloc_fname(ainfo_ptr->basename,
                                        l_cur_frame_nr,
                                        ainfo_ptr->extension);
    if(ainfo_ptr->new_filename == NULL)
    {
       goto error;
    }


    if(ainfo_ptr->curr_frame_nr == l_cur_frame_nr)
    {
      /* directly operate on the current image (where we were invoked from) */
      l_operating_on_current_image = TRUE;
      l_tmp_image_id = ainfo_ptr->image_id;
    }
    else
    {
      l_frame_found = g_file_test(ainfo_ptr->new_filename, G_FILE_TEST_EXISTS);
      if (l_frame_found != TRUE)
      {
        goto modify_advance_to_next_frame;
      }
      /* load current frame into temporary image */
      l_operating_on_current_image = FALSE;
      l_tmp_image_id = gap_lib_load_image(ainfo_ptr->new_filename);
    }
    if(l_tmp_image_id < 0)
    {
       goto error;
    }

    /* get informations (id, visible, selected) about all layers at releant level */
    l_layli_ptr = gap_mod_alloc_layli_group(l_tmp_image_id, &l_sel_cnt, &l_nlayers,
                                sel_mode, sel_case, sel_invert, sel_pattern,
                                sel_groupname, delimiter);

    if(l_layli_ptr == NULL)
    {
       printf("gap: gap_mod_frames_modify: cant alloc layer info list\n");
       goto error;
    }

    if((l_cur_frame_nr == l_begin)
    && ((action_mode == GAP_MOD_ACM_APPLY_FILTER) || (action_mode == GAP_MOD_ACM_APPLY_FILTER_ON_LAYERMASK)))
    {
      /* ------------- 1.st frame: extra dialogs for APPLY_FILTER ---------- */

      if(l_sel_cnt < 1)
      {
         g_message(_("No selected layer in start frame"));
         goto error;
      }

      if(action_mode == GAP_MOD_ACM_APPLY_FILTER_ON_LAYERMASK)
      {
        gint l_ii;

        l_operate_on_layermask = TRUE;
        l_ii = gap_mod_get_1st_selected(l_layli_ptr, l_nlayers);
        if(gimp_layer_get_mask(l_layli_ptr[l_ii].layer_id) < 0)
        {
          g_message(_("first selected layer \"%s\"\nin start frame has no layermask"),
                    gimp_item_get_name(l_layli_ptr[l_ii].layer_id)
                    );
          goto error;
        }
      }

      if(l_begin != l_end)
      {
        l_last_frame_filename = gap_lib_alloc_fname(ainfo_ptr->basename,
                                        l_end,
                                        ainfo_ptr->extension);
      }

      /* additional dialog steps  a) gap_pdb_browser (select the filter)
       *                          b) 1st interactive filtercall
       *                          c) 1st pitstop dialog
       *                          d) [ 2nd interactive filtercall
       *                          e)   2nd pitstop dialog ]
       */

      l_rc = p_do_filter_dialogs(ainfo_ptr,
                                 l_tmp_image_id, &l_dpy_id,
                                 l_layli_ptr, l_nlayers,
                                &l_filter_procname[0], sizeof(l_filter_procname),
                                &l_plugin_data_len,
                                &accelCharacteristic,
                                 l_operate_on_layermask
                                 );

      if(l_last_frame_filename != NULL)
      {
        if((l_rc == 0) && (accelCharacteristic != GAP_ACCEL_CHAR_NONE))
        {
          l_rc = p_do_2nd_filter_dialogs(&l_filter_procname[0],
                                   accelCharacteristic,
                                   l_last_frame_filename,
                                   sel_mode, sel_case, sel_invert, sel_pattern,
                                   l_operate_on_layermask,
                                   sel_groupname, delimiter
                                  );
        }

        g_free(l_last_frame_filename);
        l_last_frame_filename = NULL;
      }

      /* the 1st selected layer has been filtered
       * in the INTERACTIVE call b)
       * therefore we unselect this layer, to avoid
       * a 2nd processing
       */
      l_idx = gap_mod_get_1st_selected(l_layli_ptr, l_nlayers);
      if(l_idx >= 0)
      {
        l_layli_ptr[l_idx].selected = FALSE;
        l_sel_cnt--;
      }

      /* check for matching Iterator PluginProcedures */
      if(accelCharacteristic != GAP_ACCEL_CHAR_NONE )
      {
        l_plugin_iterator =  gap_filt_pdb_get_iterator_proc(&l_filter_procname[0], &l_count);
      }
    }

    if(l_rc != 0)
    {
      goto error;
    }

    /* perform function (defined by action_mode) on selcted layer(s) */
    l_rc = p_apply_action(l_tmp_image_id,
                   action_mode,
                   l_layli_ptr,
                   l_nlayers,
                   l_sel_cnt,
                   l_begin, l_end, l_cur_frame_nr,
                   new_layername,
                   &l_filter_procname[0],
                   ainfo_ptr->image_id,     /* MASTER_image_id */
                   new_position,
                   new_groupname,
                   delimiter
                   );
    if(l_rc != 0)
    {
      if(gap_debug) printf("gap: gap_mod_frames_modify p_apply-action failed. rc=%d\n", (int)l_rc);
      goto error;
    }

    /* free layli info table for the current frame */
    if(l_layli_ptr != NULL)
    {
      g_free(l_layli_ptr);
      l_layli_ptr = NULL;
    }

    /* check if the resulting image has at least one layer */
    gap_image_prevent_empty_image(l_tmp_image_id);

    /* save current frame with same name */
    l_rc = gap_lib_save_named_frame(l_tmp_image_id, ainfo_ptr->new_filename);
    if(l_rc < 0)
    {
      printf("gap: gap_mod_frames_modify save frame %d failed.\n", (int)l_cur_frame_nr);
      goto error;
    }
    else l_rc = 0;

    /* iterator call (for filter apply with varying values) */
    if((action_mode == GAP_MOD_ACM_APPLY_FILTER)
    && (l_plugin_iterator != NULL) && (accelCharacteristic != GAP_ACCEL_CHAR_NONE ))
    {
       gdouble accelStep;

       l_cur_step -= 1.0;

       accelStep = gap_calculate_current_step_with_acceleration(l_cur_step, l_total_steps, accelCharacteristic);
       if(gap_debug)
       {
         printf("DEBUG: calling iterator %s  current frame:%d  accelStep:%f\n"
                               ,l_plugin_iterator
                               , (int)l_cur_frame_nr
                               , (float)accelStep
                               );
       }

        /* call plugin-specific iterator, to modify
         * the plugin's last_values
         */

       if(strcmp(l_plugin_iterator, GIMP_PLUGIN_GAP_COMMON_ITER) == 0)
       {
         l_params = gimp_run_procedure (l_plugin_iterator,
                           &l_retvals,
                           GIMP_PDB_INT32,   GIMP_RUN_NONINTERACTIVE,
                           GIMP_PDB_INT32,   l_total_steps,      /* total steps  */
                           GIMP_PDB_FLOAT,   accelStep,          /* current step respecting acceleration characteristic */
                           GIMP_PDB_INT32,   l_plugin_data_len,  /* length of stored data struct */
                           GIMP_PDB_STRING,  &l_filter_procname[0],       /* the common iterator needs the plugin name as additional param */
                           GIMP_PDB_END);
       }
       else
       {
         l_params = gimp_run_procedure (l_plugin_iterator,
                             &l_retvals,
                             GIMP_PDB_INT32,   GIMP_RUN_NONINTERACTIVE,
                             GIMP_PDB_INT32,   l_total_steps,       /* total steps  */
                             GIMP_PDB_FLOAT,   accelStep,           /* current step respecting acceleration characteristic */
                             GIMP_PDB_INT32,   l_plugin_data_len,   /* length of stored data struct */
                             GIMP_PDB_END);
       }
       if (l_params[0].data.d_status != GIMP_PDB_SUCCESS)
       {
         printf("ERROR: iterator %s  failed\n", l_plugin_iterator);
         l_rc = -1;
       }

       gimp_destroy_params(l_params, l_retvals);
    }


    if(l_operating_on_current_image == FALSE)
    {
      /* destroy the tmp image */
      gimp_image_delete(l_tmp_image_id);
      l_tmp_image_id = -1;
    }

    /* close display (if open) */
    if (l_dpy_id >= 0)
    {
      gimp_display_delete(l_dpy_id);
      l_dpy_id = -1;
    }


    if(l_rc != 0)
    {
      goto error;
    }

    if(ainfo_ptr->run_mode == GIMP_RUN_INTERACTIVE)
    {
      l_percentage += l_percentage_step;

      if(progress_bar != NULL)
      {
        guchar *progressText;
        progressText = g_strdup_printf("frame:%d (%d)", (int)l_cur_frame_nr, (int)l_end);

        gtk_progress_bar_set_text(GTK_PROGRESS_BAR(progress_bar), progressText);
        gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(progress_bar), l_percentage);
        g_free(progressText);
        while (gtk_events_pending ())
        {
          gtk_main_iteration ();
        }
        if(*run_flag != TRUE)
        {
          /* cancel button was pressed in interactive mode */
          break;
        }
      }
      else
      {
        gimp_progress_update (l_percentage);
      }
    }

modify_advance_to_next_frame:
    if(l_cur_frame_nr == l_end)
    {
       break;
    }
    /* advance l_cur_frame_nr to the next available frame number
     * (normally to l_cur_frame_nr += l_step;
     * sometimes to higher/lower number when frames are missing)
     */
    l_cur_frame_nr = gap_lib_get_next_available_frame_number(l_cur_frame_nr, l_step
                           , ainfo_ptr->basename, ainfo_ptr->extension, &l_frame_found);
    if(l_frame_found != TRUE)
    {
       break;
    }

  }             /* end while(1)  loop foreach frame in range */

  /* check if master image is included in the affected range */
  if ((ainfo_ptr->curr_frame_nr >= MIN(range_from, range_to))
  &&  (ainfo_ptr->curr_frame_nr <= MAX(range_from, range_to)))
  {
    /* apply defered action concerning modifying the selection
     * (this is done after processing all other frames in the range
     * because the changes affect the selection of the master image)
     */
    if ((action_mode == GAP_MOD_ACM_SEL_SUBTRACT)
    ||  (action_mode == GAP_MOD_ACM_SEL_ADD)
    ||  (action_mode == GAP_MOD_ACM_SEL_INTERSECT))
    {
       gint32  master_channel_id;

       /* create a temporary copy of the selection
        * (because add the selections to itself
        *  behaves different when there are partly selected pixel
        *  than using a copy)
        */
       master_channel_id = gimp_selection_save(ainfo_ptr->image_id);
       p_apply_selection_action(ainfo_ptr->image_id
                             , action_mode
                             , -1   /* MASTER_image_id */
                             , master_channel_id
                             );
       gimp_image_remove_channel(ainfo_ptr->image_id, master_channel_id);
    }
  }

  if(gap_debug) printf("gap_mod_frames_modify End OK\n");

  return 0;

error:
  if(gap_debug) printf("gap: gap_mod_frames_modify exit with Error\n");

  if((l_tmp_image_id >= 0) && (l_operating_on_current_image == FALSE))
  {
    gimp_image_delete(l_tmp_image_id);
  }
  if (l_dpy_id >= 0)
  {
      gimp_display_delete(l_dpy_id);
      l_dpy_id = -1;
  }
  if(l_layli_ptr != NULL) g_free(l_layli_ptr);
  if(l_plugin_iterator != NULL)  g_free(l_plugin_iterator);
  return -1;

}               /* end gap_mod_frames_modify */

/* ============================================================================
 * gap_mod_layer
 * ============================================================================
 */
gint gap_mod_layer(GimpRunMode run_mode, gint32 image_id,
                   gint32 range_from,  gint32 range_to,
                   gint32 action_mode, gint32 sel_mode,
                   gint32 sel_case, gint32 sel_invert,
                   char *sel_pattern, char *new_layername,
                   gint32 new_position,
                   char *new_groupname, char *sel_groupname, char *delimiter)
{
  int    l_rc;
  GapAnimInfo *ainfo_ptr;
  gint32    l_from;
  gint32    l_to;
  gint32    l_action_mode;
  gint32    l_sel_mode;
  gint32    l_sel_case;
  gint32    l_sel_invert;
  GtkWidget *progress_bar;
  GtkWidget *dlg;

  char      l_sel_pattern[MAX_LAYERNAME];
  char      l_new_layername[MAX_LAYERNAME];
  gint32    l_new_position;
  char      l_new_groupname[MAX_LAYERNAME];
  char      l_sel_groupname[MAX_LAYERNAME];
  char      l_delimiter[32];

  l_rc = 0;
  progress_bar = NULL;
  dlg = NULL;

  l_delimiter[0] = '/';
  l_delimiter[1] = '\0';
  l_sel_groupname[0] = '\0';
  l_new_groupname[0] = '\0';

  ainfo_ptr = gap_lib_alloc_ainfo(image_id, run_mode);
  if(ainfo_ptr != NULL)
  {

    if (0 == gap_lib_dir_ainfo(ainfo_ptr))
    {
      if(run_mode == GIMP_RUN_INTERACTIVE)
      {
         /* note: for interactive call the processing is already done
          * as callback of the dialog
          */
         l_rc = gap_mod_frames_dialog (ainfo_ptr, &l_from, &l_to,
                                       &l_action_mode,
                                       &l_sel_mode, &sel_case, &sel_invert,
                                       &l_sel_pattern[0], &l_new_layername[0],
                                       &l_new_position,
                                       &l_new_groupname[0], &l_sel_groupname[0], &l_delimiter[0]
                                       );
         gap_lib_free_ainfo(&ainfo_ptr);
         return (l_rc);
      }
      else
      {
         l_from        = range_from;
         l_to          = range_to;
         l_action_mode = action_mode;
         l_sel_mode    = sel_mode;
         l_sel_case    = sel_case;
         l_sel_invert  = sel_invert;

         g_snprintf(&l_sel_pattern[0], sizeof(l_sel_pattern) -1, "%s", sel_pattern);
         g_snprintf(&l_new_layername[0], sizeof(l_new_layername) -1, "%s", new_layername);

         l_new_position = new_position;
         g_snprintf(&l_new_groupname[0], sizeof(l_new_groupname) -1, "%s", new_groupname);
         g_snprintf(&l_sel_groupname[0], sizeof(l_sel_groupname) -1, "%s", sel_groupname);
         g_snprintf(&l_delimiter[0], sizeof(l_delimiter) -1, "%s", delimiter);


      }

      if(l_rc >= 0)
      {
        gboolean run_flag;

        run_flag = TRUE;
        /* no need to save the current image before processing
         * because the gap_mod_frames_modify procedure operates directly on the current frame
         * and loads all other frames from disc.
         * futher all successful processed frames are saved back to disk
         * (including the current one)
         */
           l_rc = gap_mod_frames_modify(ainfo_ptr, l_from, l_to,
                                  l_action_mode,
                                  l_sel_mode, sel_case, sel_invert,
                                  &l_sel_pattern[0], &l_new_layername[0],
                                  l_new_position,
                                  &l_new_groupname[0], &l_sel_groupname[0], &l_delimiter[0],
                                  progress_bar, &run_flag
                                 );
      }

    }
    gap_lib_free_ainfo(&ainfo_ptr);
  }


  return(l_rc);
}
