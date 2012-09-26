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
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

/* revision history:
 * version 1.3.20d; 2003.10.14   hof: created
 */

/* SYTEM (UNIX) includes */
#include <stdlib.h>
#include <stdio.h>
#include <string.h>


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

    gimp_image_scale_full(image_id, 2, 2, 0 /*INTERPOLATION_NONE*/);

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
  gimp_image_add_layer(image_id, l_layer_id, 0);

  l_layer_id = gimp_layer_new(image_id, "dummy",
                                 10, 10,  l_type,
                                 0.0,       /* Opacity full transparent */
                                 0);        /* NORMAL */
  gimp_image_add_layer(image_id, l_layer_id, 0);

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
     gimp_image_add_layer(image_id, l_layer_id, 0);
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
    gimp_image_add_layer(new_image_id, *layer_id, 0);
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

  l_image_id = gimp_drawable_get_image(ref_layer_id);
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
          gimp_drawable_set_visible(l_layers_list[l_idx], TRUE);
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
 * refered by ref_drawable_id, or a Grayscale copy of the specified ref_drawable_id
 * (in case the refered image has no selection or the flag force_from_drawable is TRUE)
 *
 *  - operates on a duplicate of the image refered by ref_drawable_id.
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
  ref_image_id = gimp_drawable_get_image(ref_drawable_id);

  if (ref_image_id < 0)
  {
    printf("ref_drawable_id does not refere to a valid image layer_id:%d\n", (int)ref_drawable_id);
    return (FALSE);
  }



  dup_image_id = gimp_image_duplicate(ref_image_id);
  if (dup_image_id < 0)
  {
    printf("duplicating of image failed, refered souce image_id:%d\n", (int)ref_image_id);
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
    /* use scaled copy of the already exisating selection in the refered image */
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

  gimp_selection_load(l_aux_channel_id);
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
      if (gimp_drawable_get_visible(l_layers_list[ii]) != TRUE)
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
 * e.g. a top level group counts as one single layer
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


