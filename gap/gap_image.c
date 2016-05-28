/* gap_image.c   procedures
 * 2003.10.09 hof (Wolfgang Hofer)
 *
 * GAP ... Gimp Animation Plugins
 *
 * This Module contains Image specific Procedures
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
 * version 1.3.20d; 2003.10.14   hof: created
 */

/* SYTEM (UNIX) includes */
#include <stdlib.h>
#include <stdio.h>
#include <string.h>


#include <glib.h>
#include <gap_image.h>
#include <gap_base.h>
#include <gap_layer_copy.h>

extern int gap_debug;


/* ============================================================================
 * gap_image_delete_immediate
 *    delete image (with workaround to ensure that most of the
 *    allocatd memory is freed)
 * ============================================================================
 * The workaround disables undo and scales the image down to miniumum size
 * before calling the gimp_image_delete procedure.
 * this way memory resources for big layers will be freed up immediate.
 */
void
gap_image_delete_immediate (gint32 image_id)
{
  gboolean imageDeleteWorkaroundDefault = TRUE;
  if(gap_base_get_gimprc_gboolean_value("gap-image-delete-workaround"
         , imageDeleteWorkaroundDefault))
  {
    if(gap_debug)
    {
      printf("gap_image_delete_immediate: SCALED down to 2x2 id = %d (workaround for gimp_image-delete problem)\n"
              , (int)image_id
              );
    }

    gimp_image_undo_disable(image_id);
    
    //gimp_image_scale_full(image_id, 2, 2, 0 /*INTERPOLATION_NONE*/);

    gimp_context_push();
    gimp_context_set_interpolation(0);
    gimp_image_scale(image_id, 2, 2);
    gimp_context_pop();


    gimp_image_undo_enable(image_id); /* clear undo stack */
  }

  gimp_image_delete(image_id);
}       /* end  gap_image_delete_immediate */


/* ============================================================================
 * gap_image_merge_visible_layers
 *    merge visible layer an return layer_id of the resulting merged layer.
 *    (with workaround, for empty images return transparent layer)
 * ============================================================================
 */
gint32
gap_image_merge_visible_layers(gint32 image_id, GimpMergeType mergemode)
{
  GimpImageBaseType l_type;
  guint   l_width, l_height;
  gint32  l_layer_id;

  /* get info about the image */
  l_width  = gimp_image_width(image_id);
  l_height = gimp_image_height(image_id);
  l_type   = gimp_image_base_type(image_id);

  l_type   = (l_type * 2); /* convert from GimpImageBaseType to GimpImageType */

  /* add 2 full transparent dummy layers at top
   * (because gimp_image_merge_visible_layers complains
   * if there are less than 2 visible layers)
   */
  l_layer_id = gimp_layer_new(image_id, "dummy",
                                 l_width, l_height,  l_type,
                                 0.0,       /* Opacity full transparent */
                                 0);        /* NORMAL */
  gimp_image_insert_layer(image_id, l_layer_id, 0, 0);

  l_layer_id = gimp_layer_new(image_id, "dummy",
                                 10, 10,  l_type,
                                 0.0,       /* Opacity full transparent */
                                 0);        /* NORMAL */
  gimp_image_insert_layer(image_id, l_layer_id, 0, 0);

  return gimp_image_merge_visible_layers (image_id, mergemode);
}       /* end gap_image_merge_visible_layers */


/* ============================================================================
 * gap_image_prevent_empty_image
 *    check if the resulting image has at least one layer
 *    (gimp 1.0.0 tends to crash on layerless images)
 * ============================================================================
 */

void gap_image_prevent_empty_image(gint32 image_id)
{
  GimpImageBaseType l_type;
  guint   l_width, l_height;
  gint32  l_layer_id;
  gint    l_nlayers;
  gint32 *l_layers_list;

  l_layers_list = gimp_image_get_layers(image_id, &l_nlayers);
  if(l_layers_list != NULL)
  {
     g_free (l_layers_list);
  }
  else l_nlayers = 0;

  if(l_nlayers == 0)
  {
     /* the resulting image has no layer, add a transparent dummy layer */

     /* get info about the image */
     l_width  = gimp_image_width(image_id);
     l_height = gimp_image_height(image_id);
     l_type   = gimp_image_base_type(image_id);

     l_type   = (l_type * 2); /* convert from GimpImageBaseType to GimpImageType */

     /* add a transparent dummy layer */
     l_layer_id = gimp_layer_new(image_id, "dummy",
                                    l_width, l_height,  l_type,
                                    0.0,       /* Opacity full transparent */
                                    0);        /* NORMAL */
     gimp_image_insert_layer(image_id, l_layer_id, 0, 0);
  }

}       /* end gap_image_prevent_empty_image */



/* ============================================================================
 * gap_image_new_with_layer_of_samesize
 * ============================================================================
 * create empty image
 *  if layer_id is NOT NULL then create one full transparent layer at full image size
 *  and return the layer_id
 */
gint32
gap_image_new_with_layer_of_samesize(gint32 old_image_id, gint32 *layer_id)
{
  GimpImageBaseType  l_type;
  guint       l_width;
  guint       l_height;
  gint32      new_image_id;
  gdouble     l_xresoulution, l_yresoulution;
  gint32     l_unit;


  /* create empty image  */
  l_width  = gimp_image_width(old_image_id);
  l_height = gimp_image_height(old_image_id);
  l_type   = gimp_image_base_type(old_image_id);
  l_unit   = gimp_image_get_unit(old_image_id);
  gimp_image_get_resolution(old_image_id, &l_xresoulution, &l_yresoulution);

  new_image_id = gimp_image_new(l_width, l_height,l_type);
  gimp_image_set_resolution(new_image_id, l_xresoulution, l_yresoulution);
  gimp_image_set_unit(new_image_id, l_unit);

  if(layer_id)
  {
    l_type   = (l_type * 2); /* convert from GimpImageBaseType to GimpImageType */
    *layer_id = gimp_layer_new(new_image_id, "dummy",
                                 l_width, l_height,  l_type,
                                 0.0,       /* Opacity full transparent */
                                 0);        /* NORMAL */
    gimp_image_insert_layer(new_image_id, *layer_id, 0, 0);
  }

  return (new_image_id);

}  /* end gap_image_new_with_layer_of_samesize */

gint32
gap_image_new_of_samesize(gint32 old_image_id)
{
  return(gap_image_new_with_layer_of_samesize(old_image_id, NULL));
}



/* ------------------------------------
 * gap_image_is_alive
 * ------------------------------------
 * TODO: gimp 1.3.x sometimes keeps a copy of closed images
 *       therefore this proceedure may tell only half the truth
 *
 * return TRUE  if OK (image is still valid)
 * return FALSE if image is NOT valid
 */
gboolean
gap_image_is_alive(gint32 image_id)
{
  gint32 *images;
  gint    nimages;
  gint    l_idi;
  gint    l_found;

  if(image_id < 0)
  {
     return FALSE;
  }

  images = gimp_image_list(&nimages);
  l_idi = nimages -1;
  l_found = FALSE;
  while((l_idi >= 0) && images)
  {
    if(image_id == images[l_idi])
    {
          l_found = TRUE;
          break;
    }
    l_idi--;
  }
  if(images) g_free(images);
  if(l_found)
  {
    return TRUE;  /* OK */
  }

  if(gap_debug) printf("gap_image_is_alive: image_id %d is not VALID\n", (int)image_id);

  return FALSE ;   /* INVALID image id */
}  /* end gap_image_is_alive */


/* ------------------------------------
 * gap_image_get_any_layer
 * ------------------------------------
 * return the id of the active layer
 *   or the id of the first layer found in the image if there is no active layer
 *   or -1 if the image has no layer at all.
 */
gint32
gap_image_get_any_layer(gint32 image_id)
{
  gint32  l_layer_id;
  gint    l_nlayers;
  gint32 *l_layers_list;

  l_layer_id = gimp_image_get_active_layer(image_id);
  if(l_layer_id < 0)
  {
    l_layers_list = gimp_image_get_layers(image_id, &l_nlayers);
    if(l_layers_list != NULL)
    {
       l_layer_id = l_layers_list[0];
       g_free (l_layers_list);
    }
  }
  return (l_layer_id);
}  /* end gap_image_get_any_layer */



/* ------------------------------------
 * gap_image_merge_to_specified_layer
 * ------------------------------------
 * remove all other layers from the image except the specified layer_id
 * (by removing other layers, make ref_layer_id visible and perform merging)
 */
gint32
gap_image_merge_to_specified_layer(gint32 ref_layer_id, GimpMergeType mergemode)
{
  gint32  l_image_id;

  l_image_id = gimp_item_get_image(ref_layer_id);
  if(l_image_id >= 0)
  {
    gint32  l_idx;
    gint    l_nlayers;
    gint32 *l_layers_list;

    l_layers_list = gimp_image_get_layers(l_image_id, &l_nlayers);
    if(l_layers_list != NULL)
    {
      for(l_idx = 0; l_idx < l_nlayers; l_idx++)
      {
        if (l_layers_list[l_idx] == ref_layer_id)
        {
          gimp_item_set_visible(l_layers_list[l_idx], TRUE);
        }
        else
        {
          gimp_image_remove_layer(l_image_id, l_layers_list[l_idx]);
        }
      }
      g_free (l_layers_list);
      return (gap_image_merge_visible_layers(l_image_id, mergemode));
    }
  }
  return (-1);

}  /* end gap_image_merge_to_specified_layer */


/* -------------------------------------------------------
 * gap_image_set_selection_from_selection_or_drawable
 * -------------------------------------------------------
 * create a selection in the specified image_id.
 * The selection is a scaled copy of the selection in another image,
 * referred by ref_drawable_id, or a Grayscale copy of the specified ref_drawable_id
 * (in case the referred image has no selection or the flag force_from_drawable is TRUE)
 *
 *  - operates on a duplicate of the image referred by ref_drawable_id.
 *  - this duplicate is scaled to same size as specified image_id
 *
 * return TRUE in case the selection was successfully created .
 */
gboolean
gap_image_set_selection_from_selection_or_drawable(gint32 image_id, gint32 ref_drawable_id
  , gboolean force_from_drawable)
{
  gint32        l_aux_channel_id;
  gint32        ref_image_id;
  gint32        work_drawable_id;   /* the duplicate of the layer that is used as selction mask */
  gint32        dup_image_id;
  gboolean has_selection;
  gboolean non_empty;
  gint     x1, y1, x2, y2;

  if ((image_id < 0) || (ref_drawable_id < 0))
  {
    return (FALSE);
  }
  ref_image_id = gimp_item_get_image(ref_drawable_id);

  if (ref_image_id < 0)
  {
    printf("ref_drawable_id does not refer to a valid image layer_id:%d\n", (int)ref_drawable_id);
    return (FALSE);
  }



  dup_image_id = gimp_image_duplicate(ref_image_id);
  if (dup_image_id < 0)
  {
    printf("duplicating of image failed, referred souce image_id:%d\n", (int)ref_image_id);
    return (FALSE);
  }
  /* clear undo stack */
  if (gimp_image_undo_is_enabled(dup_image_id))
  {
    gimp_image_undo_disable(dup_image_id);
  }

  if ((gimp_image_width(image_id) != gimp_image_width(dup_image_id))
  ||  (gimp_image_height(image_id) != gimp_image_height(dup_image_id)))
  {
     if(gap_debug)
     {
       printf("scaling tmp image_id: %d\n", (int)dup_image_id);
     }
     gimp_image_scale(dup_image_id, gimp_image_width(image_id), gimp_image_height(image_id));
  }

  has_selection  = gimp_selection_bounds(ref_image_id, &non_empty, &x1, &y1, &x2, &y2);
  if ((has_selection) && (non_empty) && (force_from_drawable != TRUE))
  {
    /* use scaled copy of the already exisating selection in the referred image */
    work_drawable_id = gimp_image_get_selection(dup_image_id);
  }
  else
  {
    gint32        active_layer_stackposition;

    /* create selection as gray copy of the alt_selection layer */

    active_layer_stackposition = gap_layer_get_stackposition(ref_image_id, ref_drawable_id);

    if(gimp_image_base_type(dup_image_id) != GIMP_GRAY)
    {
       if(gap_debug)
       {
         printf("convert to GRAYSCALE tmp image_id: %d\n", (int)dup_image_id);
       }
       gimp_image_convert_grayscale(dup_image_id);
    }
    work_drawable_id = gap_layer_get_id_by_stackposition(dup_image_id, active_layer_stackposition);
    gimp_layer_resize_to_image_size (work_drawable_id);
  }

  gimp_selection_all(image_id);
  //l_sel_channel_id = gimp_image_get_selection(image_id);
  l_aux_channel_id = gimp_selection_save(image_id);

  /* copy the work drawable (layer or channel) into the selection channel
   * the work layer is a grayscale copy GRAY or GRAYA of the alt_selection layer
   *  that is already scaled and resized to fit the size of the target image
   * the work channel is the scaled selection of the image refred by ref_drawable_id
   *
   * copying is done into an auxiliary channel from where we regulary load the selection.
   * this is done because subseqent queries of the selection boudaries will deliver
   * full channel size rectangle after a direct copy into the selection.
   */
  gap_layer_copy_picked_channel (l_aux_channel_id  /* dst_drawable_id*/
                              , 0                  /* dst_channel_pick */
                              , work_drawable_id   /* src_drawable_id */
                              , 0                  /* src_channel_pick */
                              , FALSE              /* gboolean shadow */
                              );

  gimp_image_select_item(image_id, GIMP_CHANNEL_OP_REPLACE, l_aux_channel_id);
  gimp_image_remove_channel(image_id, l_aux_channel_id);

  gap_image_delete_immediate(dup_image_id);
  return (TRUE);

}  /* end gap_image_set_selection_from_selection_or_drawable */


/* ---------------------------------------
 * gap_image_remove_invisble_layers
 * ---------------------------------------
 */
void
gap_image_remove_invisble_layers(gint32 image_id)
{
  gint    l_nlayers;
  gint32 *l_layers_list;

  l_layers_list = gimp_image_get_layers(image_id, &l_nlayers);
  if(l_layers_list != NULL)
  {
    int ii;

    for(ii=0; ii < l_nlayers; ii++)
    {
      if (gimp_item_get_visible(l_layers_list[ii]) != TRUE)
      {
        gimp_image_remove_layer(image_id, l_layers_list[ii]);
      }
    }
    g_free (l_layers_list);
  }
}  /* end gap_image_remove_invisble_layers */


/* ---------------------------------------
 * gap_image_remove_all_guides
 * ---------------------------------------
 */
void
gap_image_remove_all_guides(gint32 image_id)
{
  gint32  guide_id;

  while(TRUE)
  {
    guide_id = 0;  /* 0 starts find at 1st guide */
    guide_id = gimp_image_find_next_guide(image_id, guide_id);

    if (guide_id < 1)
    {
       break;
    }
    gimp_image_delete_guide(image_id, guide_id);
  }


}  /* end gap_image_remove_all_guides */


/* ---------------------------------------
 * gap_image_limit_layers
 * ---------------------------------------
 * keepTopLayers  number of layers to keep on top of the layerstack (Foreground).
 * keepBgLayers   number of layers to keep on bottom of the layerstack (Background).
 *
 * Note that gimp-2.7 or later versions supports layer groups.
 * this procedure does only check for toplevel layers
 * and ignores layers that are nested in groups.
 * A top level group counts as one single layer
 * no matter how many layers und subgroups are in the toplevel group.
 *
 */
void
gap_image_limit_layers(gint32 image_id, gint keepTopLayers,  gint keepBgLayers)
{
  gint      l_nlayers;
  gint32   *l_layers_list;

  l_layers_list = gimp_image_get_layers(image_id, &l_nlayers);
  if(l_layers_list != NULL)
  {
    int ii;

    for(ii=0; ii < l_nlayers; ii++)
    {
      if ((ii >= keepTopLayers)
      &&  ((l_nlayers -ii) > keepBgLayers))
      {
        gimp_image_remove_layer(image_id, l_layers_list[ii]);
      }
    }
    g_free (l_layers_list);

  }
}  /* end gap_image_limit_layers */


/* ----------------------------------------------------
 * gap_image_create_unicolor_image
 * ----------------------------------------------------
 * - create a new image with one black filled layer
 *   (both have the requested size)
 *
 * return the new created image_id
 *   and the layer_id of the black_layer
 */
gint32
gap_image_create_unicolor_image(gint32 *layer_id, gint32 width , gint32 height
                       , gdouble r_f, gdouble g_f, gdouble b_f, gdouble a_f)
{
  gint32 l_empty_layer_id;
  gint32 l_image_id;

  *layer_id = -1;
  l_image_id = gimp_image_new(width, height, GIMP_RGB);
  if(l_image_id >= 0)
  {
    l_empty_layer_id = gimp_layer_new(l_image_id, "black_background",
                          width, height,
                          GIMP_RGBA_IMAGE,
                          100.0,     /* Opacity full opaque */
                          GIMP_NORMAL_MODE);
    gimp_image_insert_layer(l_image_id, l_empty_layer_id, 0, 0);

    /* clear layer to unique color */
    gap_layer_clear_to_color(l_empty_layer_id, r_f, g_f, b_f, a_f);

    *layer_id = l_empty_layer_id;
  }
  return(l_image_id);
}       /* end gap_image_create_unicolor_image */


/* -----------------------------------------------
 * gap_image_get_tree_position_list
 * -----------------------------------------------
 * return a list that represents the stack positions
 * of the specified item_id (typically a layer)
 * which contains positions within the image
 *  and within all its parent group layers.
 *
 * The 1st element in the stack position list refers to the stackposition
 * at toplevel of the image.
 * Note:
 * The caller shall g_free the returned list
 * after use by calling: gap_image_gfree_tree_position_list
 */
GapImageStackPositionsList *
gap_image_get_tree_position_list(gint32 item_id)
{
   gint32 l_image_id;
   gint32 l_curr_item_id;
   GapImageStackPositionsList *posRootPtr;


   posRootPtr = NULL;
   l_image_id =  gimp_item_get_image (item_id);

   if(gap_debug)
   {
     printf("gap_image_get_tree_position_list Start image_id: %d\n"
         , (int)l_image_id
         );
   }

   if (l_image_id < 0)
   {
     return (NULL);
   }

   l_curr_item_id = item_id;

   while(l_curr_item_id > 0)
   {
     GapImageStackPositionsList *posPtr;

     posPtr = g_new(GapImageStackPositionsList, 1);
     posPtr->stack_position = gimp_image_get_item_position (l_image_id,
                                                           l_curr_item_id);
     if(gap_debug)
     {
       printf("item_id:%d stack_position: %d name:%s\n"
         , (int)l_curr_item_id
         , (int)posPtr->stack_position
         , gimp_item_get_name(l_curr_item_id)
         );
     }

     posPtr->next = posRootPtr;
     posRootPtr = posPtr;

     l_curr_item_id = gimp_item_get_parent (l_curr_item_id);

   }

   return (posRootPtr);

}  /* end gap_image_get_tree_position_list */



/* -----------------------------------------------
 * gap_image_gfree_tree_position_list
 * -----------------------------------------------
 */
void
gap_image_gfree_tree_position_list(GapImageStackPositionsList *rootPosPtr)
{
  GapImageStackPositionsList *posPtr;
  GapImageStackPositionsList *nextPosPtr;

  posPtr = rootPosPtr;
  while (posPtr != NULL)
  {
     nextPosPtr = posPtr->next;
     g_free(posPtr);

     posPtr = nextPosPtr;
  }
}  /* end  gap_image_gfree_tree_position_list */


/* -----------------------------------------------
 * gap_image_get_layer_id_by_tree_position_list
 * -----------------------------------------------
 * return the item_id of the layer that matches
 * the specified stack position list.
 * (where each list element describes the position
 * in the next tree level
 * The 1st element in the stack position list refers to the stackposition
 * at toplevel of the image)
 *
 */
gint32
gap_image_get_layer_id_by_tree_position_list(gint32 image_id, GapImageStackPositionsList *rootPosPtr)
{
  GapImageStackPositionsList *posPtr;
  gint        l_nlayers;
  gint        l_treelevel;
  gint32     *l_src_layers;
  gint32      l_layer_id;
  gint32      l_wanted_layer_id;

  l_layer_id = -1;
  l_wanted_layer_id = -1;
  l_treelevel = 0;
  posPtr = rootPosPtr;
  if (posPtr == NULL)
  {
    return (l_layer_id);
  }
  l_src_layers = gimp_image_get_layers (image_id, &l_nlayers);
  if (l_src_layers == NULL)
  {
    return (-1);
  }

  while(posPtr != NULL)
  {
    if ((posPtr->stack_position >= 0) && (posPtr->stack_position < l_nlayers))
    {
      l_layer_id = l_src_layers[posPtr->stack_position];
      if(gap_debug)
      {
        printf("get_layer_id_by_tree_pos layer_id: treelevel:%d %d name:%s N:%d stackPos:%d\n"
           , (int)l_treelevel
           , (int)l_layer_id
           , gimp_item_get_name(l_layer_id)
           , l_nlayers
           , posPtr->stack_position
           );
      }
    }
    g_free(l_src_layers);
    if (l_layer_id < 0)
    {
      if(gap_debug)
      {
        printf("get_layer_id_by_tree_pos l_treelevel:%d stack_position:%d NOT found!\n"
           , (int)l_treelevel
           , (int)posPtr->stack_position
           );
      }
      break;
    }
    posPtr = posPtr->next;
    if(posPtr != NULL)
    {
       if (gimp_item_is_group(l_layer_id))
       {
         l_src_layers = gimp_item_get_children (l_layer_id, &l_nlayers);
         if (l_src_layers == NULL)
         {
           if(gap_debug)
           {
             printf("get_layer_id_by_tree_pos l_treelevel:%d NO CHILD LAYERS FOUND ! for item_id:%d\n"
               , (int)l_treelevel
               , (int)l_layer_id
              );
           }
           return (-1);  /* failed to get group members */
         }
       }
       else
       {
         if(gap_debug)
         {
           printf("get_layer_id_by_tree_pos l_treelevel:%d item_id:%d IS NOT A GROUP !\n"
             , (int)l_treelevel
             , (int)l_layer_id
            );
         }
         return (-1); /* group was expected, but not found, cant evaluate rest of the list */
       }
    }
    else
    {
      l_wanted_layer_id = l_layer_id;
    }
    l_treelevel++;
  }  /* end while */

  return (l_wanted_layer_id);


}  /* end gap_image_get_layer_id_by_tree_position_list */


/* -----------------------------------------------
 * gap_image_greate_group_layer_path
 * -----------------------------------------------
 * create group layer specified by nameArray starting with
 * the element at start_idx end at 1st NULL pointer element.
 * in case the rest in this nameArray contains more than one name
 * a hierarchy of group layers is created, where each name
 * has the previous name as parent.
 */
gint32
gap_image_greate_group_layer_path(gint32 image_id
                             , gint32 parent_id      /* or 0 for top imagelevel */
                             , gint32 stackposition  /* where 0 is on top position */
                             , gchar  **nameArray
                             , gint   start_idx
                             )
{
  gint32 parent_layer_id;
  gint32 group_layer_id;
  gint32 position;
  gchar *namePtr;
  gint   l_ii;

  position = stackposition;
  parent_layer_id = 0;
  if (nameArray != NULL)
  {
    for(l_ii = 0; nameArray[l_ii] != NULL; l_ii++)
    {
      if(l_ii >= start_idx)
      {
        namePtr = nameArray[l_ii];
        group_layer_id = gimp_layer_group_new(image_id);
        gimp_item_set_name(group_layer_id, namePtr);
        gimp_image_insert_layer(image_id, group_layer_id, parent_layer_id, position);
        if(gap_debug)
        {
          printf("gap_image_greate_group_layer_path: name[%d]:%s group_layer_id:%d\n"
                 , (int)l_ii
                 , namePtr
                 , (int)group_layer_id
                 );
        }

        parent_layer_id =  group_layer_id;
      }
      position = 0;
    }
  }

  return (group_layer_id);

}  /* end gap_image_greate_group_layer_path */





/* -----------------------------------------------
 * gap_image_find_or_create_group_layer
 * -----------------------------------------------
 * find the group layer where the name
 * (and concateneated names of all its parent group layers when present)
 * is equal to the specified group_name_path.
 * and return its item_id
 * Group_layer names levels are concatenated with the specified separator character.
 *
 * an empty  group_name_path_string (NULL or \0) will return 0
 * which refers to the toplevel (indicating that no group is relevant)
 *
 * in case the wanted group was not found
 * the result depends on the flag enableCreate.
 * when enableCreate == TRUE
 *    the group layer (and all missing parent groups) are created
 *    and its item_id is returned.
 * when enableCreate == FALSE
 *    -1 is returned (indicating that the wanted group was not found)
 *
 * Example (separator character '/' is assumed)
 *
 *  layertree example
 *  --------------------
 *    toplayer                id:4
 *    gr-sky                  id:3
 *      subgr-1.1             id:7
 *        layer-sun           id:13
 *    gr-2                    id:2
 *      subgr-animals         id:6
 *        layer-cat           id:12
 *        layer-dog           id:11
 *      subgr-house           id:5
 *        layer-roof          id:10
 *        layer-window        id:9
 *        layer-wall          id:8
 *    background              id:1
 *
 * o) a call with group_name_path_string = "gr-2/subgr-animals"
 *    will return 6, because the group layer with name "subgr-animals"
 *    can be found.
 * o) a call with group_name_path_string = "gr-2/subgr-animals/flying/birds"
 *    will either return -1
 *    or create both missing groups "flying" and "birds"
 *    and will return the item_id of the "birds" group.
 *    gr-2                    id:2
 *      subgr-animals         id:6
 *        flying              id:14   # newly created group layer
 *          birds             id:15   # newly created group layer
 *        layer-cat           id:12
 *        layer-dog           id:11
 * o) a call with group_name_path_string = "toplayer"
 *    will return -1, because this layer already exists
 *    but is not a group layer.
 *
 */
gint32
gap_image_find_or_create_group_layer(gint32 image_id
    , gchar *group_name_path_string
    , gchar *delimiter
    , gint stackposition
    , gboolean enableCreate
)
{
  gchar     **nameArray;
  gchar      *namePtr;
  gint        l_nlayers;
  gint        l_ii;
  gint        l_idx;
  gint        l_position;
  gint32     *l_src_layers;

  gint32      l_parent_layer_id;
  gint32      l_group_layer_id;

  if (group_name_path_string == NULL)
  {
    return (0);
  }
  if (*group_name_path_string == '\0')
  {
    return (0);
  }

  l_parent_layer_id = 0; /* start at top imagelevel */
  l_group_layer_id = -1;
  l_position = stackposition;

  if(delimiter != NULL)
  {
    nameArray = g_strsplit(group_name_path_string
                       , delimiter
                       , -1   /* max_tokens  less than 1 splits the string completely. */
                       );
  }
  else
  {
    nameArray = g_malloc(sizeof(gchar*) * 2);
    nameArray[0] = g_strdup(group_name_path_string);
    nameArray[1] = NULL;
  }

  if (nameArray == NULL)
  {
    return (0);
  }
  l_src_layers = gimp_image_get_layers (image_id, &l_nlayers);
  if (l_src_layers == NULL)
  {
    if (enableCreate)
    {
      l_group_layer_id = gap_image_greate_group_layer_path(image_id
                             , l_parent_layer_id  /* 0 is on top imagelevel */
                             , l_position      /* 0 on top stackposition */
                             , nameArray
                             , 0
                             );
    }
    g_strfreev(nameArray);
    return (l_group_layer_id);
  }

  for(l_ii = 0; nameArray[l_ii] != NULL; l_ii++)
  {
    gboolean l_found;
    l_found = FALSE;

    namePtr = nameArray[l_ii];

    if(gap_debug)
    {
      printf("gap_image_find_or_create_group_layer: name[%d]:%s\n"
            , (int)l_ii
            , namePtr
            );
    }


    for(l_idx = 0; l_idx < l_nlayers; l_idx++)
    {
      gchar *l_name;

      l_name = gimp_item_get_name(l_src_layers[l_idx]);
      if(gap_debug)
      {
        printf("cmp: name[%d]:%s  with item[%d]:%d name:%s\n"
            , (int)l_ii
            , namePtr
            , (int)l_idx
            , (int)l_src_layers[l_idx]
            , l_name
            );
      }
      if (strcmp(l_name, namePtr) == 0)
      {
        if (gimp_item_is_group(l_src_layers[l_idx]))
        {
          l_parent_layer_id = l_src_layers[l_idx];
          l_position = 0;
          l_found = TRUE;
        }
        else
        {
          if(gap_debug)
          {
            printf("ERROR: gap_image_find_or_create_group_layer the path\n  %s\n"
                   "  refers to an already existing item that is NOT a GROUP\n"
                   , group_name_path_string
                   );
          }
          g_free(l_name);
          g_free(l_src_layers);
          return (-1);
        }
      }
      g_free(l_name);
      if (l_found)
      {
        break;
      }
    }
    g_free(l_src_layers);
    l_src_layers = NULL;


    if(gap_debug)
    {
      printf(" l_found:%d l_parent_layer_id:%d\n"
                   , (int)l_found
                   , (int)l_parent_layer_id
                   );
    }
    if (l_found)
    {
      if (nameArray[l_ii +1] == NULL)
      {
        /* no more names to compare and all did match, we are done */
        l_group_layer_id = l_parent_layer_id;
      }
      else
      {
        /* check next treelevel i.e. members of the current group */
        l_src_layers = gimp_item_get_children (l_parent_layer_id, &l_nlayers);
      }
    }
    else
    {
      if (enableCreate)
      {
        /* create all missing groups/subgroups */
        l_group_layer_id = gap_image_greate_group_layer_path(image_id
                             , l_parent_layer_id  /* 0 is on top imagelevel */
                             , l_position      /* 0 on top stackposition */
                             , nameArray
                             , l_ii
                             );
      }
      break;
    }
  }

  if(gap_debug)
  {
    printf("gap_image_find_or_create_group_layer BEFORE g_strfreev l_group_layer_id:%d\n"
          ,(int)l_group_layer_id
          );
  }

  g_strfreev(nameArray);

  return(l_group_layer_id);

}  /* end gap_image_find_or_create_group_layer */


/* -----------------------------
 * gap_image_reorder_layer
 * -----------------------------
 * move the specified layer to another position within the same image.
 * (done by removing and then re-inserting the layer)
 * new_groupname
 *   Name of a group or group/subgroup where the specified layer_id shall be moved to.
 *   Note that the string new_groupname uses the delimiter string
 *   to split nested group/sugrup names.
 *   use new_groupname = NULL to move the specified layer_id to image toplevel.
 * enableGroupCreation
 *   TRUE:
 *     in case the group layer with new_groupname does not exist
 *     it will be created automatically.
 *  FALSE:
 *     in case the group layer with new_groupname does not exist
 *     -1 is returned and the reorder operation is not performed.
 * new_position
 *     the desired new stackposition within the specified new_groupname
 *     or toplevel image (when new_groupname is NULL or empty)
 * returns   -1 on error
 */
gint32
gap_image_reorder_layer(gint32 image_id, gint32 layer_id,
              gint32 new_position,
              char *new_groupname,
              char *delimiter,
              gboolean enableGroupCreation,
              char *new_layername)
{
  gint32 l_parent_id;
  gint32 l_dup_layer_id;
  gchar *l_name;

  l_parent_id = gap_image_find_or_create_group_layer(image_id
                          , new_groupname
                          , delimiter
                          , 0      /* stackposition for the group in case it is created at toplevel */
                          , enableGroupCreation
                          );
  if (l_parent_id < 0)
  {
    return (-1);
  }

  l_dup_layer_id = gimp_layer_copy(layer_id);

  l_name = NULL;
  if (new_layername != NULL)
  {
    if (*new_layername != '\0')
    {
      l_name = g_strdup(new_layername);
    }
  }

  if (l_name == NULL)
  {
   l_name = gimp_item_get_name(layer_id);
  }
  gimp_image_remove_layer(image_id, layer_id);

  gimp_image_insert_layer(image_id, l_dup_layer_id, l_parent_id, new_position);
  gimp_item_set_name(l_dup_layer_id, l_name);
  g_free(l_name);

  return (0); /* OK */

}  /* end gap_image_reorder_layer */

/* ------------------------------------
 * gap_image_merge_group_layer
 * ------------------------------------
 * merge the specified group layer and return the id of the resulting layer.
 *
 * The merge strategy
 *  o) create a temporary image  of same size/type (l_tmp_img_id)
 *  o) copy the specified grouplayer to the temporary image (l_tmp_img_id)
 *  o) call gimp_image_merge_visible_layers on the temporary image (l_tmp_img_id, mode)
 *  o) copy the merged layer back to the original image
 *      to the same group at the position of the original layergroup
 *  o) remove the temporary image
 *  o) remove original layergroup
 *  o) rename the resuling merged layer.
 *
 * returns   0 if all done OK
 *           (or -1 on error)
 */
gint32
gap_image_merge_group_layer(gint32 image_id,
              gint32 group_layer_id,
              gint merge_mode)
{
  gint32   l_tmp_img_id;
  gint32   l_new_layer_id;
  gint32   l_merged_layer_id;
  gint32   l_parent_id;
  gint32   l_position;
  gint     l_src_offset_x;
  gint     l_src_offset_y;
  gboolean l_visible;
  char    *l_name;

  if (!gimp_item_is_group(group_layer_id))
  {
    /* the specified group_layer_id is not a group
     * -- no merge is done, return its id as result --
     */
    return(group_layer_id);
  }
  l_visible = gimp_item_get_visible(group_layer_id);
  l_name = gimp_item_get_name(group_layer_id);

  /* create a temporary image */
  l_tmp_img_id = gap_image_new_of_samesize(image_id);

  /* copy the grouplayer to the temporary image  */
  l_new_layer_id = gap_layer_copy_to_dest_image(l_tmp_img_id,
                                         group_layer_id,
                                         100.0,   /* full opacity */
                                         0,       /* NORMAL paintmode */
                                         &l_src_offset_x,
                                         &l_src_offset_y
                                         );

  gimp_image_insert_layer (l_tmp_img_id, l_new_layer_id, 0, 0);
  gimp_layer_set_offsets(l_new_layer_id, l_src_offset_x, l_src_offset_y);
  gimp_item_set_visible(l_new_layer_id, TRUE);


  /* merge visible layers in the temporary image */
  l_merged_layer_id = gimp_image_merge_visible_layers (l_tmp_img_id, merge_mode);
  l_new_layer_id = gap_layer_copy_to_dest_image(image_id,
                                         l_merged_layer_id,
                                         gimp_layer_get_opacity(group_layer_id),
                                         gimp_layer_get_mode(group_layer_id),
                                         &l_src_offset_x,
                                         &l_src_offset_y
                                         );
  l_position = gimp_image_get_item_position (image_id, group_layer_id);
  l_parent_id = gimp_item_get_parent(group_layer_id);
  if (l_parent_id < 0)
  {
    l_parent_id = 0;
  }
  gimp_image_insert_layer (image_id, l_new_layer_id, l_parent_id, l_position);
  gimp_layer_set_offsets(l_new_layer_id, l_src_offset_x, l_src_offset_y);

  /* remove the original group layer from the original image */
  gimp_image_remove_layer(image_id, group_layer_id);

  /* restore the original layer name */
  if (l_name != NULL)
  {
    gimp_item_set_name(l_new_layer_id, l_name);
    g_free(l_name);
  }
  gimp_item_set_visible(l_new_layer_id, l_visible);

  /* remove the temporary image */
  gap_image_delete_immediate(l_tmp_img_id);
  return(l_new_layer_id);

}  /* end gap_image_merge_group_layer */


/* -----------------------------------------------
 * gap_image_get_parentpositions_as_int_stringlist
 * -----------------------------------------------
 * returns the list of parent stackpositions as list of integer numbers
 * separated by the "/" delimiter.
 *  example 2/3/2
 * return NULL in case the specified drawable is invalid or has no perent item 
 */
char *
gap_image_get_parentpositions_as_int_stringlist(gint32 drawable_id)
{
  char *parentpositions;
  gint32 l_parent_id;
  gint32 l_image_id;
  
  parentpositions = NULL;
  l_parent_id = gimp_item_get_parent(drawable_id);
  if (l_parent_id > 0)
  {
    gint l_position;
    
    l_image_id = gimp_item_get_image(drawable_id);
    l_position = gimp_image_get_item_position (l_image_id, l_parent_id);


    parentpositions = g_strdup_printf("%d", l_position);
    while(l_parent_id > 0)
    {
      l_parent_id = gimp_item_get_parent(l_parent_id);
      if (l_parent_id > 0)
      {
        char *new_parentpositions;
        
        l_position = gimp_image_get_item_position (l_image_id, l_parent_id);
        new_parentpositions = g_strdup_printf("%d/%s"
                                 , (int)l_position
                                 , parentpositions
                                 );
        g_free(parentpositions);
        parentpositions = new_parentpositions;
      }
    }
  }

  if(gap_debug)
  {
    printf("parentpositions_as_int_stringlist: %s\n"
       , parentpositions == NULL ? "null" : parentpositions
       );
  }
  return (parentpositions);
  
}  /* end gap_image_get_parentpositions_as_int_stringlist */


/* -----------------------------------------------
 * gap_image_get_layers_at_parentpositions
 * -----------------------------------------------
 * returns the list layers at toplevel image (when parentpositions == NULL)
 *    otherwise return the list of layers of the nested group
 *    where  parentpositions specifies a list of integer stackpositions
 *    from toplevel to the deepest nested group delimited by "/"
 *    note that all specified parentpositions MUST refer to group layers.
 * return NULL in case no matching layerstack was found in the image.
 */
gint32 *
gap_image_get_layers_at_parentpositions(gint32 image_id, gint *nlayers, const char *parentpositions)
{
  gint          l_nlayers;
  gint32       *l_toplevel_layers_list;
  gint32       *l_layers_list;
  gboolean      l_has_parents;

  *nlayers = 0;
  l_toplevel_layers_list = gimp_image_get_layers(image_id, &l_nlayers);
  if (l_toplevel_layers_list == NULL)
  {
    return (NULL);
  }
  l_layers_list = l_toplevel_layers_list;


  l_has_parents = FALSE;
  if(parentpositions != NULL)
  {
    if (*parentpositions != '\0')
    {
      l_has_parents = TRUE;
    }
  }
  if (l_has_parents == TRUE)
  {
    gchar     **posValueArray;
    gint      l_ii;
    posValueArray = g_strsplit(parentpositions
                         , "/"
                         , -1   /* max_tokens  less than 1 splits the string completely. */
                         );
    for(l_ii = 0; posValueArray[l_ii] != NULL; l_ii++)
    {
      long l_pos;
      
      
      l_pos = atol(posValueArray[l_ii]);
      if ((l_pos >= 0) && (l_pos < l_nlayers))
      {
        gint32 l_layer_id;
        
        l_layer_id = l_layers_list[l_pos];
        if (gimp_item_is_group(l_layer_id))
        {
          g_free(l_layers_list);
          l_layers_list = gimp_item_get_children(l_layer_id, &l_nlayers);
        }
        else
          /* error: position is not a group */
          if(gap_debug)
          {
            printf("gap_image_get_layers_at_parentpositions: position %d is no GROUP"
                   "(l_nlayers:%d) l_layer_id:%d (%s) parentpositions: %s \n"
               , (int) l_pos
               , (int) l_nlayers
               , (int) l_layer_id
               , gimp_item_get_name(l_layer_id)
               , parentpositions
               );
          }
          g_strfreev(posValueArray);
          g_free(l_layers_list);
          return (NULL);
        {
        }
      }
      else
      {
        /* error: position is invalid (not an integer within valid stackposition range */
        if(gap_debug)
        {
          printf("gap_image_get_layers_at_parentpositions: position %d not in valid range"
                 " (l_nlayers:%d) parentpositions: %s \n"
               , (int) l_pos
               , (int) l_nlayers
               , parentpositions
               );
        }
        g_strfreev(posValueArray);
        g_free(l_layers_list);
        return (NULL);
      }
      

   }

   g_strfreev(posValueArray);

   *nlayers = l_nlayers;
   return (l_layers_list);
  
  }
  
  /* has no parents: deliver toplevel layers list */

  *nlayers = l_nlayers;
  return (l_toplevel_layers_list);


} /* end gap_image_get_layers_at_parentpositions */


/* -----------------------------
 * gap_image_get_the_layer_below
 * -----------------------------
 * returns the id of the layer below the specified layerId
 *
 * returns -1 in case there is no layer below the specified layerId
 *            Note that -1 is alse returned 
 *            a) in case the specified layerId is not a valid layer (or not attached to an image)
 *            b) in case the specified layerId is on bottom of a layergroup
 *            c) in case the specified layerId is on bottom of the toplevel layerstack
 */
gint32
gap_image_get_the_layer_below(gint32 layerId)
{
  gint32        lowerLayerId;
  gint32        l_parent_id;
  gint          l_nlayers;
  gint32       *l_layers_list;
  gint          l_ii;

  lowerLayerId = -1;
  l_parent_id = 0;
  if (layerId >= 0)
  {
    l_parent_id = gimp_item_get_parent(layerId);
  }
  
  if (l_parent_id > 0)
  {
    l_layers_list = gimp_item_get_children(l_parent_id, &l_nlayers);
  }
  else
  {
    /* use toplevel layers list of the image */
    l_layers_list = gimp_image_get_layers(gimp_item_get_image(layerId), &l_nlayers);
  }


  if (l_layers_list == NULL)
  {
    return (-1);
  }

  for(l_ii = 0; l_ii < l_nlayers; l_ii++)
  {
    if (l_layers_list[l_ii] == layerId)
    {
      gint l_pos_below = l_ii + 1;
      if (l_pos_below < l_nlayers)
      {
        lowerLayerId = l_layers_list[l_pos_below];
        break;
      }
    }
  }
  g_free(l_layers_list);
  
  
  return (lowerLayerId);
  
}  /* end gap_image_get_the_layer_below */
