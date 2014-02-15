/* gap_onion_base.c   procedures
 * 2003.05.22 hof (Wolfgang Hofer)
 *
 * GAP ... Gimp Animation Plugins
 *
 * This Module contains GAP Onionskin Worker Procedures
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
 * version 2.1.0a;   2004/06/03   hof: added onionskin ref_mode
 * version 1.3.16c;  2003.07.08   hof: created (as extract of the gap_onion_worker.c module)
 */

#include "config.h"

/* SYTEM (UNIX) includes */
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <gap-intl.h>

#include <gtk/gtk.h>
#include <libgimp/gimp.h>
#include <libgimp/gimpui.h>

#include "gap_libgapbase.h"
#include <gap_match.h>
#include <gap_lib.h>
#include <gap_layer_copy.h>
#include <gap_image.h>

#include <gap_onion_base.h>

extern int gap_debug;


/* ============================================================================
 * gap_onion_base_mark_as_onionlayer
 *   set onion layer parasite (store timestamp and tattoo)
 *   tattoos are unique identifiers within an image
 *   and remain unique over sessions -- if saved in xcf format.
 * ============================================================================
 */
void
gap_onion_base_mark_as_onionlayer(gint32 layer_id)
{
  GapOnionBaseParasite_data *l_parasite_data;
  GimpParasite          *l_parasite;

  if(gap_debug) printf("gap_onion_base_mark_as_onionlayer: START\n");

  l_parasite_data = g_malloc(sizeof(GapOnionBaseParasite_data));
  l_parasite_data->timestamp = gap_base_get_current_time();
  l_parasite_data->tattoo = gimp_item_get_tattoo(layer_id);
  if(gap_debug) printf("gap_onion_base_mark_as_onionlayer: tattoo is: %d\n", (int)l_parasite_data->tattoo);

  l_parasite = gimp_parasite_new(GAP_ONION_PARASITE_NAME,
                                 GIMP_PARASITE_PERSISTENT,
                                 sizeof(GapOnionBaseParasite_data),
                                 l_parasite_data);
  if(l_parasite)
  {
    gimp_drawable_parasite_attach(layer_id, l_parasite);
    gimp_parasite_free(l_parasite);
  }
}       /* end gap_onion_base_mark_as_onionlayer */


/* ============================================================================
 * gap_onion_base_check_is_onion_layer
 *   check for onion layer parasite and tattoo.
 *   (if the user made a copy of an onion layer, the copy has an onionparasite
 *    but the originals parasitedata.tattoo will not match with the layers tattoo.
 *    therefore the copy is not identified as onion layer.)
 * returns TRUE if layer is an original onion layer.
 * ============================================================================
 */
gint32
gap_onion_base_check_is_onion_layer(gint32 layer_id)
{
   gint           l_found;
   GapOnionBaseParasite_data *l_parasite_data;
   GimpParasite  *l_parasite;

   if(gap_debug) printf("gap_onion_base_check_is_onion_layer: START layer_id %d\n", (int)layer_id);

   l_found = FALSE;
   l_parasite = gimp_drawable_parasite_find(layer_id, GAP_ONION_PARASITE_NAME);
   if (l_parasite)
   {
      l_parasite_data = (GapOnionBaseParasite_data *)l_parasite->data;
      if(gap_debug) printf("gap_onion_base_check_is_onion_layer: tattoo is: %d\n", (int)l_parasite_data->tattoo);

      if (l_parasite_data->tattoo == gimp_item_get_tattoo(layer_id))
      {
        l_found = TRUE;
        if(gap_debug) printf("gap_onion_base_check_is_onion_layer: ONION_LAYER_FOUND layer_id %d\n", (int)layer_id);
      }
      gimp_parasite_free(l_parasite);
   }

   return l_found;
}       /* end gap_onion_base_check_is_onion_layer */


/* ============================================================================
 * gap_onion_base_onionskin_visibility
 *    set visibility of all onion layer(s) in the current image.
 * ============================================================================
 */
gint
gap_onion_base_onionskin_visibility(gint32 image_id, gint visi_mode)
{
#define VISIBILTY_UNSET  -4444
  gint32     *l_layers_list;
  gint        l_nlayers;
  gint        l_idx;
  gint        l_is_onion;
  gint        l_visible;
  gint32      l_layer_id;

  if(gap_debug)
  {
     printf("gap_onion_base_onionskin_visibility: START visi_mode: %d\n", (int)visi_mode);
     printf("  image_ID: %d\n", (int)image_id);
  }

  l_visible = VISIBILTY_UNSET;
  if(visi_mode == GAP_ONION_VISI_TRUE)
  {
    l_visible = TRUE;
  }
  if(visi_mode == GAP_ONION_VISI_FALSE)
  {
    l_visible = TRUE;
  }

  l_layers_list = gimp_image_get_layers(image_id, &l_nlayers);
  if(l_layers_list)
  {
    for(l_idx=0;l_idx < l_nlayers;l_idx++)
    {
      l_layer_id = l_layers_list[l_idx];
      l_is_onion = gap_onion_base_check_is_onion_layer(l_layer_id);

      if(l_is_onion)
      {
        if (l_visible == VISIBILTY_UNSET)
        {
          l_visible = !gimp_item_get_visible(l_layer_id);
        }

        /* set visibility  */
        if(gap_debug) printf("layer_id %d  visibility: %d\n", (int)l_layer_id ,(int)l_visible);
        gimp_item_set_visible(l_layer_id, l_visible);
      }
    }
    g_free(l_layers_list);
  }
  return 0;
}       /* end gap_onion_base_onionskin_visibility */



/* ============================================================================
 * gap_onion_base_onionskin_delete
 *    remove onion layer(s) from the current image.
 * ============================================================================
 */
gint
gap_onion_base_onionskin_delete(gint32 image_id)
{
  gint32     *l_layers_list;
  gint        l_nlayers;
  gint        l_idx;
  gint        l_is_onion;

  if(gap_debug)
  {
     printf("gap_onion_base_onionskin_delete: START\n");
     printf("  image_ID: %d\n", (int)image_id);
  }

  l_layers_list = gimp_image_get_layers(image_id, &l_nlayers);
  if(gap_debug) printf("gap_onion_base_onionskin_delete: l_nlayers = %d\n", (int)l_nlayers);
  if(l_layers_list)
  {
    for(l_idx=0;l_idx < l_nlayers;l_idx++)
    {
      if(gap_debug) printf("gap_onion_base_onionskin_delete: l_idx = %d\n", (int)l_idx);
      l_is_onion = gap_onion_base_check_is_onion_layer(l_layers_list[l_idx]);

      if(l_is_onion)
      {
        /* remove onion layer from source */
        gimp_image_remove_layer(image_id, l_layers_list[l_idx]);
      }
    }
    g_free(l_layers_list);
  }
  if(gap_debug) printf("gap_onion_base_onionskin_delete: END\n");
  return 0;
}       /* end gap_onion_base_onionskin_delete */


/* ============================================================================
 * gap_onion_base_onionskin_apply
 *    create or replace onion layer(s) in the current image.
 *    Onion layers do show one (or more) merged copies of previos (or next)
 *    videoframe(s).
 *    This procedure first removes onion layers (if there are any)
 *    then reads the other videoframes, merges them (according to config params)
 *    and imports the merged layer(s) as onion layer(s).
 *    Onion Layers are marked by tattoo and parasite
 *    use_chache TRUE:
 *       check if desired frame is already in the image cache we can skip loading
 *       if the cached image is already merged we can simply copy the layer.
 *       Further we update the cache with merged layer.
 *       use_cache is for processing multiple frames (speeds up remarkable)
 *    use_cache FALSE:
 *       always load frames and perform merge,
 *       but we can steal layer from the tmp_image.
 *       (faster than copy when processing a single frame only)
 *
 * The functionpointers
 *     fptr_add_img_to_cache
 *     fptr_find_frame_in_img_cache
 *
 *   are used for caching the handled images.
 *   this is useful when onionskin layers are created for more than one frame
 *   at once. for processing single frames it is OK to pass NULL pointers.
 *
 * returns   value >= 0 if all is ok
 *           (or -1 on error)
 * ============================================================================
 */
gint
gap_onion_base_onionskin_apply(gpointer gpp
             , gint32 image_id
             , GapVinVideoInfo *vin_ptr
             , long   ainfo_curr_frame_nr
             , long   ainfo_first_frame_nr
             , long   ainfo_last_frame_nr
             , char  *ainfo_basename
             , char  *ainfo_extension
             , GapOnionBaseFptrAddImageToCache        fptr_add_img_to_cache
             , GapOnionBaseFptrFindFrameInImageCache fptr_find_frame_in_img_cache
             , gboolean use_cache)
{
  gint32        l_nr;
  gint32        l_sign;

  gint32        l_onr;
  gint32        l_ign;
  gint32        l_idx;
  gint32        l_frame_nr;
  gint32        l_tmp_image_id;
  gint32        l_is_onion;
  gint32        l_layerstack;
  char         *l_new_filename;
  char         *l_name;
  gint32        l_layer_id;
  gint32        l_new_layer_id;
  gint32     *l_layers_list;
  gint        l_nlayers;
  gdouble     l_opacity;
  char       *l_layername;
  gint32      l_active_layer;


  if(gap_debug)
  {
     printf("gap_onion_base_onionskin_apply: START\n");
     printf("  num_olayers: %d\n",   (int)vin_ptr->num_olayers);
     printf("  ref_mode:  %d\n",     (int)vin_ptr->ref_mode);
     printf("  ref_delta: %d\n",     (int)vin_ptr->ref_delta);
     printf("  ref_cycle: %d\n",     (int)vin_ptr->ref_cycle);
     printf("  stack_pos: %d\n",     (int)vin_ptr->stack_pos);
     printf("  stack_top: %d\n",     (int)vin_ptr->stack_top);
     printf("  opacity: %f\n",       (float)vin_ptr->opacity);
     printf("  opacity_delta: %f\n", (float)vin_ptr->opacity_delta);
     printf("  ignore_botlayers: %d\n", (int)vin_ptr->ignore_botlayers);
     printf("  image_ID: %d\n",      (int)image_id);
     printf("  use_cache: %d\n",     (int)use_cache);
     printf("  asc_opacity: %d\n",   (int)vin_ptr->asc_opacity);

     printf("  ainfo_curr_frame_nr: %d\n",     (int)ainfo_curr_frame_nr);
     printf("  ainfo_first_frame_nr: %d\n",     (int)ainfo_first_frame_nr);
     printf("  ainfo_last_frame_nr: %d\n",     (int)ainfo_last_frame_nr);
     printf("  ainfo_basename: %s\n",     ainfo_basename);
     printf("  ainfo_extension: %s\n",    ainfo_extension);

  }

  /* keep information about the active_layer
   * (needed for restore after onionskin layers were recreated)
   */
  l_active_layer = gimp_image_get_active_layer(image_id);
  if(l_active_layer >= 0)
  {
    if(gap_onion_base_check_is_onion_layer(l_active_layer))
    {
      /* no need to remember the active layer, because
       * it is an onionskin layer that will be deleted
       * before recreation
       */
      l_active_layer = -1;
    }
  }

  /* delete onion layers (if there are old ones) */
  gap_onion_base_onionskin_delete(image_id);

  l_layers_list = gimp_image_get_layers(image_id, &l_nlayers);
  if(vin_ptr->stack_top)
  {
    l_layerstack = CLAMP(vin_ptr->stack_pos, 0, l_nlayers);
  }
  else
  {
    l_layerstack = CLAMP((l_nlayers - vin_ptr->stack_pos),0 ,l_nlayers);
  }
  if(l_layers_list) { g_free(l_layers_list); l_layers_list = NULL;}

  /* create new onion layer(s) */
  l_opacity = vin_ptr->opacity;
  l_new_filename = NULL;
  l_frame_nr = ainfo_curr_frame_nr;
  l_sign = -1;
  for(l_onr=1; l_onr <= vin_ptr->num_olayers; l_onr++)
  {
    /* find out reference frame number */

    if(vin_ptr->asc_opacity)
    {
       /* process far neigbours first to give them the highest configured opacity value */
       l_nr = (1+ vin_ptr->num_olayers) - l_onr;
    }
    else
    {
       /* process near neigbours first to give them the highest configured opacity value */
       l_nr = l_onr;
    }

    /* find out reference frame number */
    switch(vin_ptr->ref_mode)
    {
      case GAP_ONION_REFMODE_BIDRIECTIONAL_SINGLE:
        l_sign *= -1; /* toggle sign between -1 and +1 */
        break;
      case GAP_ONION_REFMODE_BIDRIECTIONAL_DOUBLE:
        l_sign *= -1; /* toggle sign between -1 and +1 */
        l_nr = 1 + ((l_nr -1) / 2);
        break;
      case GAP_ONION_REFMODE_NORMAL:
        l_sign = 1;  /* normal mode: always force sign of +1 */
      default:
        break;
    }

    l_frame_nr = ainfo_curr_frame_nr + (l_sign * (vin_ptr->ref_delta * l_nr));


    if(!vin_ptr->ref_cycle)
    {
      if((l_frame_nr < ainfo_first_frame_nr)
      || (l_frame_nr > ainfo_last_frame_nr))
      {
         break;  /* fold back cycle turned off */
      }
    }
    if (l_frame_nr < ainfo_first_frame_nr)
    {
      l_frame_nr = ainfo_last_frame_nr +1 - (ainfo_first_frame_nr - l_frame_nr);
      if (l_frame_nr < ainfo_first_frame_nr)
      {
        break;  /* stop on multiple fold back cycle */
      }
    }
    if (l_frame_nr > ainfo_last_frame_nr)
    {
      l_frame_nr = ainfo_first_frame_nr -1 + (l_frame_nr - ainfo_last_frame_nr);
      if (l_frame_nr > ainfo_last_frame_nr)
      {
         break;  /* stop on multiple fold back cycle */
      }
    }

    l_tmp_image_id = -1;
    l_layer_id = -1;
    if(use_cache)
    {
      if(fptr_find_frame_in_img_cache != NULL)
      {
         (*fptr_find_frame_in_img_cache)(gpp, l_frame_nr, &l_tmp_image_id, &l_layer_id);
      }

      if (l_tmp_image_id == image_id)
      {
        /* never use the same image as source and destination,
         * if references point to same image, always force
         * loading a 2.nd copy for the merge
         */
        l_tmp_image_id = -1;
      }
    }

    if(l_tmp_image_id < 0)
    {
      /* frame is not available in the cache */
      if(gap_debug) printf("gap_onion_base_onionskin_apply: frame is NOT available in the CACHE\n");
      /* build the frame name */
      if(l_new_filename != NULL) g_free(l_new_filename);
      l_new_filename = gap_lib_alloc_fname(ainfo_basename,
                                          l_frame_nr,
                                          ainfo_extension);
      if(g_file_test(l_new_filename, G_FILE_TEST_EXISTS))
      {
        /* load referenced frame */
        l_tmp_image_id = gap_lib_load_image(l_new_filename);
        if(l_tmp_image_id < 0)
        {
          g_free(l_new_filename);
          return -1;
	    }
      }
      else
      {
        /* refered frame image is not available, ignore it */
        g_free(l_new_filename);
        l_new_filename = NULL;
        continue;
      }
      /* dont waste time and memory for undo in noninteracive processing
       * of the src frames
       */
      /*gimp_image_undo_enable(l_tmp_image_id);*/ /* clear undo stack */
      /* no more gimp_image_undo_enable, causes warnings since gimp-2.1.6 */
      /*  Gimp-Core-CRITICAL **: file gimpimage.c: line 1708 (gimp_image_undo_thaw): assertion `gimage->undo_freeze_count > 0' failed */
      gimp_image_undo_disable(l_tmp_image_id); /*  NO Undo */
    }
    else
    {
      if(gap_debug) printf("gap_onion_base_onionskin_apply: frame is AVAILABLE in the CACHE l_tmp_image_id: %d\n", (int)l_tmp_image_id);
    }


    if(l_layer_id < 0)
    {
      /* there was no merged layer in the cache, so we must
       * merge layers now
       */
      if(gap_debug) printf("gap_onion_base_onionskin_apply: layer is NOT available in the CACHE\n");

      /* set some layers invisible
       * a) ignored bottomlayer(s)
       * b) select_mode dependent: layers where layername does not match select-string
       * c) all onion layers
       */
      l_layers_list = gimp_image_get_layers(l_tmp_image_id, &l_nlayers);
      for(l_ign=0, l_idx=l_nlayers -1; l_idx >= 0;l_idx--)
      {
        l_layer_id = l_layers_list[l_idx];
        l_layername = gimp_item_get_name(l_layer_id);


        l_is_onion = gap_onion_base_check_is_onion_layer(l_layer_id);

        if((l_ign <  vin_ptr->ignore_botlayers)
        || (FALSE == gap_match_layer( l_idx
                                  , l_layername
                                  , &vin_ptr->select_string[0]
                                  , vin_ptr->select_mode
                                  , vin_ptr->select_case
                                  , vin_ptr->select_invert
                                  , l_nlayers
                                  , l_layer_id)
           )
        || (l_is_onion))
        {
          gimp_item_set_visible(l_layer_id, FALSE);
        }

        g_free (l_layername);

        if(!l_is_onion)
        {
          /* exclude other onion layers from counting ignored layers
           */
          l_ign++;
        }
      }

      /* merge visible layers (clip at image size) */
      l_layer_id = gap_image_merge_visible_layers(l_tmp_image_id, GIMP_CLIP_TO_IMAGE);
    }
    else
    {
      if(gap_debug) printf("gap_onion_base_onionskin_apply: layer is AVAILABLE in the CACHE l_layer_id: %d\n", (int)l_layer_id);
    }

    /* COPY or MOVE the resulting merged layer to current image image_id */
    if(use_cache)
    {
      /* when using image cache we must copy the merged layer
       * but first update the cache, so we can reuse the merged layer
       * when processing the next frame
       */
      if(fptr_add_img_to_cache != NULL)
      {
         (*fptr_add_img_to_cache)(gpp, l_frame_nr, l_tmp_image_id, l_layer_id);
      }
    }

    if(l_layer_id >= 0)
    {
      gint         l_src_offset_x;    /* layeroffsets as they were in src_image */
      gint         l_src_offset_y;

      /* copy the layer to dest image image_id */
      l_new_layer_id = gap_layer_copy_to_dest_image (image_id
                                     ,l_layer_id
                                     ,CLAMP(l_opacity, 0.0, 100.0)
                                     ,0           /* NORMAL GimpLayerModeEffects */
                                     ,&l_src_offset_x
                                     ,&l_src_offset_y
                                     );

      if(! gimp_drawable_has_alpha(l_new_layer_id))
      {
         /* have to add alpha channel */
         gimp_layer_add_alpha(l_new_layer_id);
      }



       if(gap_debug) printf("ONL:gap_onion_base_onionskin_apply  l_onr:%d, framenr:%d, layerstack:%d opacity:%f\n"
       , (int)l_onr
       , (int)l_frame_nr
       , (int)l_layerstack
       , (float)l_opacity
       );


      /* add the layer to current frame at desired stackposition  */
      gimp_image_insert_layer (image_id, l_new_layer_id, 0, l_layerstack);
      gimp_layer_set_offsets(l_new_layer_id, l_src_offset_x, l_src_offset_y);

      /* set layername */
      l_name = g_strdup_printf(_("onionskin_%06d"), (int) l_frame_nr);
      gimp_item_set_name(l_new_layer_id, l_name);
      g_free(l_name);


      /* Set parasite or tattoo */
      gap_onion_base_mark_as_onionlayer(l_new_layer_id);
      /* gimp_layer_set_opacity(l_new_layer_id, l_opacity); */
    }


    if(l_layers_list != NULL)  { g_free (l_layers_list); l_layers_list = NULL;}

    if(!use_cache)
    {
      /* destroy the tmp image */
      gap_image_delete_immediate(l_tmp_image_id);
    }

    /* opacitiy for next onion layer (reduced by delta percentage) */
    l_opacity = CLAMP((l_opacity * vin_ptr->opacity_delta / 100.0), 0.0 , 100.0);

    /* stackposition for next onion layer */
    l_layerstack++;

    if(l_new_filename) { g_free(l_new_filename); l_new_filename = NULL; };

  }

  if(l_active_layer >= 0)
  {
    gimp_image_set_active_layer(image_id, l_active_layer);
  }

  if(gap_debug) printf("gap_onion_base_onionskin_apply: END\n\n");

  return 0;
}       /* end gap_onion_base_onionskin_apply */


/* ----------------------------------
 * gap_onion_image_has_oinonlayers
 * ----------------------------------
 * check if image has onionskin layers
 * IN: only_visible TRUE check only for visible onionskin layers
 *                  FALSE check for all onionskin layers
 */
gboolean
gap_onion_image_has_oinonlayers(gint32 image_id, gboolean only_visible)
{
  gint32     *l_layers_list;
  gint        l_nlayers;
  gint        l_idx;
  gint        l_is_onion;
  gint32      l_layer_id;
  gboolean    l_has_onion;

  if(gap_debug)
  {
     printf("gap_onion_image_has_oinonlayers: START only_visible: %d image_id: %d\n"
            , (int)only_visible
            , (int)image_id
            );
  }

  l_has_onion = FALSE;

  l_layers_list = gimp_image_get_layers(image_id, &l_nlayers);
  if(l_layers_list)
  {
    for(l_idx=0;l_idx < l_nlayers;l_idx++)
    {
      l_layer_id = l_layers_list[l_idx];
      l_is_onion = gap_onion_base_check_is_onion_layer(l_layer_id);

      if(l_is_onion)
      {
        if (only_visible)
        {
          if(gimp_item_get_visible(l_layer_id))
          {
            l_has_onion = TRUE;
            break;
          }
        }
        else
        {
          l_has_onion = TRUE;
          break;
        }
      }
    }

    g_free(l_layers_list);
  }
  return (l_has_onion);

}  /* end gap_onion_image_has_oinonlayers */


/* ---------------------------------
 * gap_onion_base_image_duplicate
 * ---------------------------------
 * duplicate the image
 * and mark onionskin layers in the copy
 */
gint32
gap_onion_base_image_duplicate(gint32 image_id)
{
  gint32 dup_image_id;

  dup_image_id = gimp_image_duplicate(image_id);
  if(dup_image_id >= 0)
  {
    gint32     *l_layers_list;
    gint32     *l_dup_layers_list;
    gint        l_nlayers;
    gint        l_dup_nlayers;
    gint        l_idx;
    gint        l_is_onion;
    gint32      l_layer_id;
    gboolean    l_has_onion;

    l_layers_list = gimp_image_get_layers(image_id, &l_nlayers);
    l_dup_layers_list = gimp_image_get_layers(dup_image_id, &l_dup_nlayers);

    if((l_layers_list)
    && (l_dup_layers_list))
    {
      for(l_idx=0;l_idx < MIN(l_nlayers, l_dup_nlayers);l_idx++)
      {
        l_layer_id = l_layers_list[l_idx];
        l_is_onion = gap_onion_base_check_is_onion_layer(l_layer_id);

        if(l_is_onion)
        {
          gap_onion_base_mark_as_onionlayer(l_dup_layers_list[l_idx]);
        }
      }

      g_free(l_layers_list);
      g_free(l_dup_layers_list);
    }

  }

  return(dup_image_id);
}  /* end gap_onion_base_image_duplicate */
