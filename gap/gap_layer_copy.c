/* gap_layer_copy.c
 *    by hof (Wolfgang Hofer)
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
 * version 2.1.0a  2004.04.11   hof: added gap_layer_clear_to_color
 * version 1.3.26a 2004.01.28   hof: added gap_layer_copy_from_buffer
 * version 1.3.21c 2003.11.02   hof: added gap_layer_copy_to_image
 * version 1.3.20d 2003.10.14   hof: sourcecode cleanup, new: gap_layer_copy_content, gap_layer_copy_picked_channel
 * version 1.3.5a  2002.04.20   hof: use gimp_layer_new_from_drawable (API cleanup, requries gimp.1.3.6)
 *                                   removed channel_copy
 * version 0.99.00 1999.03.03   hof: use the regular gimp_layer_copy and gimp_channel_copy
 *                                   (removed private variant)
 * version 0.98.00 1998.11.26   hof: added channel copy
 * version         1998.11.26   hof: bugfix have to copy the layer's layer_mask too.
 *                                          type check of destination image
 * version 0.96.00              hof: bugfix memory leak (must set src_tile to unref after use)
 * version 0.93.01              hof: when creating the destination layer
 *                                   add alpha channel if needed in extra call
 * version 0.90.00;             hof: 1.st (pre) release
 */

/* SYTEM (UNIX) includes */
#include "string.h"
/* GIMP includes */
/* GAP includes */
#include "gap_lib_common_defs.h"
#include "gap_layer_copy.h"
#include "gap_pdb_calls.h"

extern      int gap_debug; /* ==0  ... dont print debug infos */


/* ============================================================================
 * gap_layer_copy_to_dest_image
 *    copy src_layer to the dst_image,
 *    return the id of the new created layer (the copy)
 * NOTE: source layer MUST have same type (bpp) for now
 *       it would be fine to extend the code to convert between any type
 * ============================================================================
 */
gint32 gap_layer_copy_to_dest_image (gint32 dst_image_id,
                        gint32 src_layer_id,
                        gdouble    opacity, /* 0.0 upto 100.0 */
                        GimpLayerModeEffects mode,
                        gint *src_offset_x,
                        gint *src_offset_y )
{
  gint32 l_new_layer_id;
  gint32 l_ret_id;
  char  *l_name;

  if(gap_debug) printf("GAP gap_layer_copy_to_dest_image: START\n");

  l_ret_id = -1;       /* prepare error retcode -1 */
  l_name = NULL;

  opacity = CLAMP(opacity, 0.0, 100.0);

  l_name = gimp_item_get_name(src_layer_id);

  /* copy the layer */
  l_new_layer_id = gimp_layer_new_from_drawable(src_layer_id, dst_image_id);

  if(l_new_layer_id >= 0)
  {
    if(! gimp_drawable_has_alpha(l_new_layer_id))
    {
       /* have to add alpha channel */
       gimp_layer_add_alpha(l_new_layer_id);
    }

    /* findout the offsets of the original layer within the source Image */
    gimp_drawable_offsets(src_layer_id, src_offset_x, src_offset_y );

    gimp_item_set_name(l_new_layer_id, l_name);
    gimp_layer_set_opacity(l_new_layer_id, opacity);
    gimp_layer_set_mode(l_new_layer_id, mode);


    l_ret_id = l_new_layer_id;  /* all done OK */
  }

  if(l_name != NULL) { g_free (l_name); }

  if(gap_debug) printf("GAP gap_layer_copy_to_dest_image: ret %d\n", (int)l_ret_id);

  return l_ret_id;
}       /* end gap_layer_copy_to_dest_image */


/* -----------------------
 * gap_layer_copy_to_image
 * -----------------------
 *  copy src_layer 1:1 on top of the layerstack at dst_image_id,
 *    return the id of the new created layer (the copy)
 *
 * NOTES:
 * -  source layer MUST have same type as the destination image
 *   (you cant copy INDEXED or GRAY src_layers to RGB images and so on..)
 * - if the src_layer has no alpha channel,
 *   an alpha_channel is added to the copied layer.
 */
gint32
gap_layer_copy_to_image (gint32 dst_image_id, gint32 src_layer_id)
{
  gint32 l_new_layer_id;
  GimpImageType  l_src_type;
  gdouble        l_src_opacity;
  GimpLayerModeEffects  l_src_mode;
  gint           l_src_offset_x;
  gint           l_src_offset_y;

  /* create new layer in destination image */
  l_src_type    = gimp_drawable_type(src_layer_id);
  l_src_opacity = gimp_layer_get_opacity(src_layer_id);
  l_src_mode    = gimp_layer_get_mode(src_layer_id);

  switch(l_src_type)
  {
    case GIMP_RGB_IMAGE:         /* 0 */
    case GIMP_RGBA_IMAGE:        /* 1 */
       if(gimp_image_base_type(dst_image_id) != GIMP_RGB) { return -1; }
       break;
    case GIMP_GRAY_IMAGE:        /* 2 */
    case GIMP_GRAYA_IMAGE:       /* 3 */
       if(gimp_image_base_type(dst_image_id) != GIMP_GRAY) { return -1; }
       break;
    case GIMP_INDEXED_IMAGE:     /* 4 */
    case GIMP_INDEXEDA_IMAGE:    /* 5 */
       if(gimp_image_base_type(dst_image_id) != GIMP_INDEXED) { return -1; }
       break;
  }


  l_new_layer_id = gap_layer_copy_to_dest_image(dst_image_id
                                               ,src_layer_id
                                               ,l_src_opacity
                                               ,l_src_mode
                                               ,&l_src_offset_x
                                               ,&l_src_offset_y
                                               );
  if(l_new_layer_id < 0)
  {
     return -1;
  }

  if(! gimp_drawable_has_alpha(l_new_layer_id))
  {
     /* have to add alpha channel */
     gimp_layer_add_alpha(l_new_layer_id);
  }

  /* add the copied layer to  destination dst_image_id (0 == on top of layerstack) */
  gimp_image_insert_layer(dst_image_id, l_new_layer_id, 0, 0);
  gimp_layer_set_offsets(l_new_layer_id, l_src_offset_x, l_src_offset_y);

  return l_new_layer_id; /* all done OK */

}       /* end gap_layer_copy_to_image */



/* ---------------------------------
 * p_copy_rgn_render_region
 * ---------------------------------
 */
static void
p_copy_rgn_render_region (const GimpPixelRgn *srcPR
                    ,const GimpPixelRgn *dstPR)
{
  guint    row;
  guchar* src  = srcPR->data;
  guchar* dest = dstPR->data;


  for (row = 0; row < dstPR->h; row++)
  {
      memcpy(dest, src, dstPR->w * dstPR->bpp);

      src  += srcPR->rowstride;
      dest += dstPR->rowstride;
  }
}  /* end p_copy_rgn_render_region */





/* ---------------------------------
 * p_copy_rgn_render_region_bpp
 * ---------------------------------
 * copy region where source and destination bpp
 * are different.
 */
static void
p_copy_rgn_render_region_bpp (const GimpPixelRgn *srcPR
                    ,const GimpPixelRgn *dstPR)
{
  guint    row;
  guint    col;
  guint    minBpp = MIN(srcPR->bpp, dstPR->bpp);
  guchar* src  = srcPR->data;
  guchar* dest = dstPR->data;
  

  for (row = 0; row < dstPR->h; row++)
  {
     gint srcOff;
     gint dstOff;
     gint ii;
     
     srcOff = 0;
     dstOff = 0;
     
     for (col = 0; col < dstPR->w; col++)
     {
       for(ii=0; ii < minBpp; ii++)
       {
        dest[dstOff + ii] = src[srcOff + ii];
       }
     
       srcOff += srcPR->bpp;
       dstOff += dstPR->bpp;
     }

     src  += srcPR->rowstride;
     dest += dstPR->rowstride;
  }
}  /* end p_copy_rgn_render_region_bpp */


/* ============================================================================
 * gap_layer_copy_content
 * - source and dest must be the same size
 * - selections are ignored
 *   (the full drawable content is copied without use of the shadow buffer)
 * - different bpp values will copy only up to the smaller of both bpp values
 *   and leaves the higher channel number(s) unchanged.
 *   useful combinations are:
 *       srcBpp == 4 (RBGA)  and dstBpp == 3 (RBG)   where only RGB is copied
 *       srcBpp == 3 (RBG)   and dstBpp == 4 (RBGA)  where only RGB is copied and alpha is left unchanged
 *       srcBpp == 2 (GRAYA) and dstBpp == 1 (GRAY)  where only the gray value is copied
 *       srcBpp == 1 (GRAY)  and dstBpp == 2 (GRAYA) where only the gray value is copied and alpha is left unchanged
 *   other combinations may not make sense
 *   where gray value or alpha channel is copied to Red or green channel
 *   and gives strange results. 
 *
 * ============================================================================
 */
gboolean
gap_layer_copy_content (gint32 dst_drawable_id, gint32 src_drawable_id)
{
  GimpPixelRgn srcPR, dstPR;
  GimpDrawable *src_drawable;
  GimpDrawable *dst_drawable;
  gpointer  pr;

  src_drawable = gimp_drawable_get (src_drawable_id);
  dst_drawable = gimp_drawable_get (dst_drawable_id);

  if((src_drawable->width  != dst_drawable->width)
  || (src_drawable->height != dst_drawable->height))
  {
    printf("gap_layer_copy_content: calling ERROR src_drawable and dst_drawable do not match in size\n");
    printf("src: w:%d h:%d bpp:%d\n"
           ,(int)src_drawable->width
           ,(int)src_drawable->height
           ,(int)src_drawable->bpp
           );
    printf("dst: w:%d h:%d bpp:%d\n"
           ,(int)dst_drawable->width
           ,(int)dst_drawable->height
           ,(int)dst_drawable->bpp
           );
    gimp_drawable_detach(src_drawable);
    gimp_drawable_detach(dst_drawable);
    return FALSE;
  }

  gimp_pixel_rgn_init (&srcPR, src_drawable, 0, 0
                      , src_drawable->width, src_drawable->height
                      , FALSE     /* dirty */
                      , FALSE     /* shadow */
                       );
  gimp_pixel_rgn_init (&dstPR, dst_drawable, 0, 0
                      , dst_drawable->width, dst_drawable->height
                      , TRUE      /* dirty */
                      , FALSE     /* shadow */
                       );

  if (src_drawable->bpp == dst_drawable->bpp)
  {
    /* can use faster variant for same bpp */
    for (pr = gimp_pixel_rgns_register (2, &srcPR, &dstPR);
         pr != NULL;
         pr = gimp_pixel_rgns_process (pr))
    {
        p_copy_rgn_render_region (&srcPR, &dstPR);
    }
  }
  else
  {
    for (pr = gimp_pixel_rgns_register (2, &srcPR, &dstPR);
         pr != NULL;
         pr = gimp_pixel_rgns_process (pr))
    {
        p_copy_rgn_render_region_bpp (&srcPR, &dstPR);
    }
  }

  gimp_drawable_flush (dst_drawable);
  gimp_drawable_detach(src_drawable);
  gimp_drawable_detach(dst_drawable);
  return TRUE;
}  /* end gap_layer_copy_content */




/* ---------------------------------
 * p_pick_rgn_render_region
 * ---------------------------------
 */
static void
p_pick_rgn_render_region (const GimpPixelRgn *srcPR
                    ,const GimpPixelRgn *dstPR
                    ,guint src_channel_pick
                    ,guint dst_channel_pick)
{
  guint    row;
  guchar* src  = srcPR->data;
  guchar* dest = dstPR->data;

  for (row = 0; row < dstPR->h; row++)
    {
        guchar* l_src  = src;
        guchar* l_dest = dest;
        guint   col = dstPR->w;

        while (col--)
          {
            l_dest[dst_channel_pick] = l_src[src_channel_pick];
            l_src += srcPR->bpp;
            l_dest += dstPR->bpp;
          }

      src  += srcPR->rowstride;
      dest += dstPR->rowstride;
    }
}  /* end p_pick_rgn_render_region */


/* ============================================================================
 * gap_layer_copy_picked_channel
 * copy channelbytes for the picked channel from src_drawable to dst_drawable
 * examples:
 *   to copy the alpha channel from a GRAY_ALPHA (bpp=2) src_drawable into a RGBA (bpp=4) dst_drawable
 *   you can call this procedure with src_channel_pick = 1, dst_channel_pick = 3
 *
 *   to copy a Selection Mask (bpp=1) src_drawable into a GRAY_ALPHA (bpp=2) dst_drawable
 *   you can call this procedure with src_channel_pick = 0, dst_channel_pick = 1

 * - source and dest must be the same size
 * - src_channel_pick selects the src channel and must be less than src bpp
 * - dst_channel_pick selects the src channel and must be less than src bpp
 * ============================================================================
 */
gboolean
gap_layer_copy_picked_channel (gint32 dst_drawable_id,  guint dst_channel_pick
                              , gint32 src_drawable_id, guint src_channel_pick
                              , gboolean shadow)
{
  GimpPixelRgn srcPR, dstPR;
  GimpDrawable *src_drawable;
  GimpDrawable *dst_drawable;
  gint    x1, y1, x2, y2;
  gpointer  pr;

  src_drawable = gimp_drawable_get (src_drawable_id);
  dst_drawable = gimp_drawable_get (dst_drawable_id);

  if((src_drawable->width  != dst_drawable->width)
  || (src_drawable->height != dst_drawable->height)
  || (src_channel_pick     >= src_drawable->bpp)
  || (dst_channel_pick     >= dst_drawable->bpp))
  {
    printf("gap_layer_copy_content: calling ERROR src_drawable and dst_drawable do not match in size or bpp\n");
    printf("src: w:%d h:%d bpp:%d\n"
           ,(int)src_drawable->width
           ,(int)src_drawable->height
           ,(int)src_drawable->bpp
           );
    printf("dst: w:%d h:%d bpp:%d\n"
           ,(int)dst_drawable->width
           ,(int)dst_drawable->height
           ,(int)dst_drawable->bpp
           );
    gimp_drawable_detach(src_drawable);
    gimp_drawable_detach(dst_drawable);
    return FALSE;
  }

  gimp_drawable_mask_bounds (dst_drawable_id, &x1, &y1, &x2, &y2);



  gimp_pixel_rgn_init (&srcPR, src_drawable, x1, y1
                      , (x2 - x1), (y2 - y1)
                      , FALSE     /* dirty */
                      , FALSE     /* shadow */
                       );
  gimp_pixel_rgn_init (&dstPR, dst_drawable, x1, y1
                      ,  (x2 - x1), (y2 - y1)
                      , TRUE      /* dirty */
                      , shadow    /* shadow */
                       );

  if(shadow)
  {
     GimpPixelRgn origPR;
     GimpPixelRgn shadowPR;

     /* since we are operating on the shadow buffer
      * we must copy the original channelbytes to the shadow buffer
      * (because the un-picked channelbytes would be uninitialized
      *  otherwise)
      */

     gimp_pixel_rgn_init (&origPR, dst_drawable, x1, y1
                      ,  (x2 - x1), (y2 - y1)
                      , FALSE    /* dirty */
                      , FALSE    /* shadow */
                       );
     gimp_pixel_rgn_init (&shadowPR, dst_drawable, x1, y1
                      ,  (x2 - x1), (y2 - y1)
                      , TRUE    /* dirty */
                      , TRUE    /* shadow */
                       );
     for (pr = gimp_pixel_rgns_register (2, &origPR, &shadowPR);
          pr != NULL;
          pr = gimp_pixel_rgns_process (pr))
     {
         p_copy_rgn_render_region (&origPR, &shadowPR);
     }
  }

  for (pr = gimp_pixel_rgns_register (2, &srcPR, &dstPR);
       pr != NULL;
       pr = gimp_pixel_rgns_process (pr))
  {
      p_pick_rgn_render_region (&srcPR, &dstPR, src_channel_pick, dst_channel_pick);
  }

  /*  update the processed region  */
  gimp_drawable_flush (dst_drawable);

  if(shadow)
  {
    gimp_drawable_merge_shadow (dst_drawable_id, TRUE);
  }
  gimp_drawable_update (dst_drawable_id, x1, y1, (x2 - x1), (y2 - y1));

  gimp_drawable_detach(src_drawable);
  gimp_drawable_detach(dst_drawable);

  return TRUE;
}  /* end gap_layer_copy_picked_channel */




/* ---------------------------------
 * gap_layer_new_from_buffer
 * ---------------------------------
 * create a new RGB or RGBA layer for the image with image_id
 * and init with supplied data.
 * The caller is responsible to add the returned layer
 * to the image ( by calling gimp_image_insert_layer )
 *
 * return layer_id or -1 in case of errors
 */
gint32 
gap_layer_new_from_buffer(gint32 image_id
                                    , gint32 width
                                    , gint32 height
                                    , gint32 bpp
                                    , guchar *data
                                    )
{
  gint32 layer_id;
  GimpImageBaseType l_basetype;
  GimpImageBaseType l_type;
  GimpPixelRgn  dstPR;
  GimpDrawable *dst_drawable;
 
  l_basetype   = gimp_image_base_type(image_id);
  if(l_basetype != GIMP_RGB)
  {
    printf("**ERROR gap_layer_copy_from_buffer: image != RGB is not supported\n");
    return -1;
  }
  
  l_type   = GIMP_RGB_IMAGE;
  if(bpp == 4)
  {
    l_type   = GIMP_RGBA_IMAGE;
  }

  layer_id = gimp_layer_new(image_id
                , "dummy"
                , width
                , height
                , l_type
                , 100.0   /* full opacity */
                , 0       /* normal mode */
                );

  if(layer_id >= 0)
  {
    dst_drawable = gimp_drawable_get (layer_id);

    gimp_pixel_rgn_init (&dstPR, dst_drawable
                      , 0, 0     /* x1, y1 */
                      , width
                      , height
                      , TRUE     /* dirty */
                      , FALSE    /* shadow */
                       );
    
    gimp_pixel_rgn_set_rect(&dstPR
                           ,data
                           ,0
                           ,0
                           ,width
                           ,height
                           );

    /*  update the processed region  */
    gimp_drawable_flush (dst_drawable);
    gimp_drawable_detach(dst_drawable);

  }
  
  return(layer_id);
}  /* end gap_layer_new_from_buffer */


/* ----------------------------------------------------
 * gap_layer_clear_to_color
 * ----------------------------------------------------
 * set layer to unique color 
 * (color channels are specified within range 0.0 up to 1.0)
 */
void
gap_layer_clear_to_color(gint32 layer_id
                             ,gdouble red
                             ,gdouble green
                             ,gdouble blue
                             ,gdouble alpha
                             )
{
  gint32 image_id;
  
  image_id = gimp_item_get_image(layer_id);

  if(alpha==0.0)
  {
    gimp_selection_none(image_id);
    gimp_edit_clear(layer_id);
  }
  else
  {
    GimpRGB  color;
    GimpRGB  bck_color;
    
    color.r = red;
    color.g = green;
    color.b = blue;
    color.a = alpha;

    if(gap_debug)
    {
      printf("CLR to COLOR gap_layer_clear_to_color: %f %f %f  %f\n"
        ,(float)color.r
        ,(float)color.g
        ,(float)color.b
        ,(float)color.a
        );
    }

    gimp_context_get_background(&bck_color);
    gimp_context_set_background(&color);
    /* fill the drawable (ignoring selections) */
    gimp_drawable_fill(layer_id, GIMP_BACKGROUND_FILL);

    
    /* restore BG color in the context */
    gimp_context_set_background(&bck_color);

  }
 
}  /* end gap_layer_clear_to_color */



/* ----------------------------------------------------
 * gap_layer_flip
 * ----------------------------------------------------
 * flip layer according to flip_request,
 * return the id of the flipped layer.
 * NOTE: flip_request GAP_STB_FLIP_NONE returns the unchanged layer 
 */
gint32
gap_layer_flip(gint32 layer_id, gint32 flip_request)
{
  gint32   center_x;
  gint32   center_y;
  gdouble  axis;

 
  switch(flip_request)
  {
    case GAP_STB_FLIP_HOR:
      gimp_context_set_defaults();
      gimp_context_set_transform_resize(GIMP_TRANSFORM_RESIZE_CLIP);   /* enable clipping */                                 
      axis = (gdouble)(gimp_drawable_width(layer_id)) / 2.0;
      layer_id = gimp_item_transform_flip_simple(layer_id
                                   ,GIMP_ORIENTATION_HORIZONTAL
                                   ,TRUE    /* auto_center */
                                   ,axis
                                   );
      break;
    case GAP_STB_FLIP_VER:
      gimp_context_set_defaults();
      gimp_context_set_transform_resize(GIMP_TRANSFORM_RESIZE_CLIP);   /* enable clipping */                                 
      axis = (gdouble)(gimp_drawable_height(layer_id)) / 2.0;
      layer_id = gimp_item_transform_flip_simple(layer_id
                                   ,GIMP_ORIENTATION_VERTICAL
                                   ,TRUE    /* auto_center */
                                   ,axis
                                   );
      break;
    case GAP_STB_FLIP_BOTH:
      gimp_context_set_defaults();
      gimp_context_set_transform_resize(GIMP_TRANSFORM_RESIZE_CLIP);   /* enable clipping */                                 
      center_x = gimp_drawable_width(layer_id) / 2;
      center_y = gimp_drawable_height(layer_id) / 2;
  
      layer_id = gimp_item_transform_rotate_simple(layer_id
                                  ,GIMP_ROTATE_180
                                  ,TRUE      /* auto_center */
                                  ,center_x
                                  ,center_y
                                  );
      break;
    default:
      break;
  }
  
  return(layer_id);
}  /* end gap_layer_flip */



/* -----------------------------
 * gap_layer_copy_paste_drawable
 * -----------------------------
 * copy specified src_drawable into dst_drawable using
 * gimp copy paste procedures.
 * The selection in the specified image will be removed
 * (and therefore is ignored for copying)
 * the caller shall specify image_id == -1 in case where selection
 * shall be respected.
 */
void
gap_layer_copy_paste_drawable(gint32 image_id, gint32 dst_drawable_id, gint32 src_drawable_id)
{
  gint32   l_fsel_layer_id;
  
  if (image_id >= 0)
  {
    gimp_selection_none(image_id);
  }
  if (TRUE == gimp_edit_copy(src_drawable_id))
  {
    l_fsel_layer_id = gimp_edit_paste(dst_drawable_id, FALSE);
    gimp_floating_sel_anchor(l_fsel_layer_id);
  }
}  /* end gap_layer_copy_paste_drawable */



/* ---------------------------------
 * gap_layer_get_stackposition
 * ---------------------------------
 * get stackposition index of the specified layer.
 * return -1 if the layer is not part of the specified image
 */
gint32
gap_layer_get_stackposition(gint32 image_id, gint32 ref_layer_id)
{
  gint          l_nlayers;
  gint32       *l_layers_list;
  gint32        l_idx;

  l_layers_list = gimp_image_get_layers(image_id, &l_nlayers);
  if(l_layers_list != NULL)
  {
    for(l_idx = 0; l_idx < l_nlayers; l_idx++)
    {
      if(l_layers_list[l_idx] == ref_layer_id)
      {
        g_free (l_layers_list);
        return(l_idx);   
      }
    }
    g_free (l_layers_list);
  }

  return (-1);
  
}  /* end gap_layer_get_stackposition */


/* ---------------------------------
 * gap_layer_get_id_by_stackposition
 * ---------------------------------
 * return -1 if the specified image has no layer at specified stackposition
 */
gint32
gap_layer_get_id_by_stackposition(gint32 image_id, gint32 stackposition)
{
  gint          l_nlayers;
  gint32       *l_layers_list;
  gint32        l_layer_id;

  l_layer_id = -1;
  l_layers_list = gimp_image_get_layers(image_id, &l_nlayers);
  if(l_layers_list != NULL)
  {
    if ((stackposition >= 0) && (stackposition < l_nlayers))
    {
      l_layer_id = l_layers_list[stackposition];
    }
    g_free (l_layers_list);
  }

  return (l_layer_id);

}  /* end gap_layer_get_id_by_stackposition */


/* ------------------------
 * gap_layer_make_duplicate
 * ------------------------
 *
 */
gint32
gap_layer_make_duplicate(gint32 src_layer_id, gint32 image_id
  , const char *name_prefix, const char *name_suffix)
{  
  gint32         l_new_layer_id;
  gint32         l_stackposition;
  const char    *l_suffix;
  const char    *l_prefix;
  char          *l_new_name;
  char          *l_old_name;


  /* make a copy of the layer  */
  l_new_layer_id = gimp_layer_copy(src_layer_id);
  
  /* and add at stackposition above src_layer */
  l_stackposition = gap_layer_get_stackposition(image_id, src_layer_id);
  gimp_image_insert_layer (image_id, l_new_layer_id, 0, l_stackposition);

  /* build name with optional prefix and/or suffix */  
  l_suffix = "\0";
  l_prefix = "\0";
  if (name_suffix != NULL)
  {
    l_suffix = name_suffix;
  }
  if (name_prefix != NULL)
  {
    l_prefix = name_prefix;
  }
  l_old_name = gimp_item_get_name(src_layer_id);
  
  l_new_name = g_strdup_printf("%s%s%s", l_prefix, l_old_name, l_suffix);
  
  gimp_item_set_name(l_new_layer_id, l_new_name);
  g_free(l_old_name);
  g_free(l_new_name);
  
  return (l_new_layer_id);
}  /* end gap_layer_make_duplicate */



/* -----------------------------------------------
 * gap_layer_create_layer_from_layermask
 * -----------------------------------------------
 * create a new layer as copy of the layermask of the
 * specified src_layer_id.
 * The name is built as copy of the src_layer_id's name
 * with optional prefix and/or suffix.
 * (specify NULL to omit name_prefix and/or name_suffix)
 *
 * the new layer is added to the image at top
 * of layerstack
 * or above the src_layer if src_layer is in the same image.
 * 
 * return the id of the newly created layer.
 */
gint32
gap_layer_create_layer_from_layermask(gint32 src_layer_id
  , gint32 image_id
  , const char *name_prefix, const char *name_suffix)
{
  gint32         l_new_layer_id;
  gint32         l_new_layermask_id;

  /* make a copy of the layer (but without the layermask) */
  l_new_layer_id = gap_layer_make_duplicate(src_layer_id, image_id
    , name_prefix, name_suffix);
  l_new_layermask_id = gimp_layer_get_mask(l_new_layer_id);
  if (l_new_layermask_id >= 0)
  {
    /* copy the layermask into the new layer */
    gap_layer_copy_paste_drawable(image_id
                           , l_new_layer_id      /* dst_drawable_id */
                           , l_new_layermask_id  /* src_drawable_id */
                           );
    gimp_layer_remove_mask (l_new_layer_id, GIMP_MASK_DISCARD);
  }
  else
  {
    /* create white layer (in case no layermask was present) */
    gap_layer_clear_to_color(l_new_layer_id
                              ,1.0 /* red */
                              ,1.0 /* green  */
                              ,1.0 /* blue */
                              ,1.0 /* alpha */
                              );
  }

  return (l_new_layer_id);
}  /* end gap_layer_create_layer_from_layermask */


/* ---------------------------------
 * gap_layer_create_layer_from_alpha
 * ---------------------------------
 *
 */
gint32
gap_layer_create_layer_from_alpha(gint32 src_layer_id, gint32 image_id
  , const char *name_prefix, const char *name_suffix
  , gboolean applyExistingLayermask, gboolean useTransferAlpha)
{
  gint32         l_new_layer_id;
  gint32         l_old_layermask_id;
  gint32         l_new_layermask_id;

  /* make a copy of the src_layer */
  l_new_layer_id = gap_layer_make_duplicate(src_layer_id, image_id
                           , name_prefix, name_suffix);
  l_old_layermask_id = gimp_layer_get_mask(l_new_layer_id);

  if (l_old_layermask_id >= 0)
  {
    /* handle already exiting layermask: apply or remove (i.e. ignore) */
    if (applyExistingLayermask)
    {
       /* merge the already existing layermask into the alpha channel */
       gimp_layer_remove_mask (l_new_layer_id, GIMP_MASK_APPLY);
    }
    else
    {
       gimp_layer_remove_mask (l_new_layer_id, GIMP_MASK_DISCARD);
    }
  }

  /* create a new layermask from the alpha channel */
  if (useTransferAlpha == TRUE)
  {
          l_new_layermask_id = gimp_layer_create_mask(l_new_layer_id, GIMP_ADD_ALPHA_TRANSFER_MASK);
  }
  else
  {
          l_new_layermask_id = gimp_layer_create_mask(l_new_layer_id, GIMP_ADD_ALPHA_MASK);
  }


  gimp_layer_add_mask(l_new_layer_id, l_new_layermask_id);
  gap_layer_copy_paste_drawable(image_id, l_new_layer_id, l_new_layermask_id);
  gimp_layer_remove_mask (l_new_layer_id, GIMP_MASK_DISCARD);
  return (l_new_layer_id);

}  /* end gap_layer_create_layer_from_alpha  */

/* ---------------------------------
 * gap_layer_find_by_name
 * ---------------------------------
 * return -1 if the specified image has no layer with the specified name
 */
gint32
gap_layer_find_by_name(gint32 image_id, const char *name)
{
  gint          l_nlayers;
  gint32       *l_layers_list;
  gint32        l_layer_id;

  l_layer_id = -1;
  l_layers_list = gimp_image_get_layers(image_id, &l_nlayers);
  if(l_layers_list != NULL)
  {
    gint ii;
    for(ii=0; ii < l_nlayers; ii++)
    {
      char *layername;
      gboolean isEqual;

      layername = gimp_item_get_name(l_layers_list[ii]);
      isEqual = (strcmp(layername, name) == 0);
      g_free(layername);
      
      if (isEqual)
      {
        l_layer_id = l_layers_list[ii];
        break;
      }
    }
    g_free (l_layers_list);
  }

  return (l_layer_id);

}  /* end gap_layer_find_by_name */

/* --------------------------------------
 * gap_layer_new_same_size_and_offsets
 * --------------------------------------
 * creates a new layer with name "worklayer" above the specified origLayer with same size
 * as the specified imageId (when imageSize == true) or same size as
 * the origLayerId (when imageSize == false)
 * and attached to the specified image_id at same offsets.
 *
 * In case the origLayer is a valid layer in the same image, the newly created layer
 * is placed in the same layergroup one stackposition above.
 *
 * otherwise the position will be above the active layer.
 *
 * Note that the created layer is not yet initialized.
 *   initialize can be done with:
 *   gimp_drawable_fill(workLayerId, GIMP_TRANSPARENT_FILL);
 *
 */
gint32
gap_layer_new_same_size_and_offsets(gint32 image_id, gint32 origLayerId, gboolean imageSize)
{
 gint32 l_new_layer_id;
 gint32 l_parent_id;
 gint32 l_position;
 gint src_offset_x;
 gint src_offset_y;
 
 gint l_width;
 gint l_height;
 
 l_parent_id = 0;
 l_position = -1;  /* -1 specifies position avove the active layer,  where 0 is on top (in the foreground) */
 
 if (origLayerId >= 0)
 {
   if (image_id == gimp_item_get_image(origLayerId))
   {
     l_position = gimp_image_get_item_position (image_id, origLayerId);
     l_parent_id = gimp_item_get_parent(origLayerId);
     if (l_parent_id < 0)
     {
       l_parent_id = 0;
     }
   }
   l_width = gimp_drawable_width(origLayerId);
   l_height = gimp_drawable_height(origLayerId);

   gimp_drawable_offsets(origLayerId, &src_offset_x, &src_offset_y );

 }
 else
 {
   l_width = gimp_image_width(image_id);
   l_height = gimp_image_height(image_id);
   src_offset_x = 0;
   src_offset_y = 0;
 }
 
 l_new_layer_id = gimp_layer_new(image_id, "worklayer",
                                 l_width,
                                 l_height,
                                 ((gint)(gimp_image_base_type(image_id)) * 2),
                                 100.0,     /* Opacity full opaque */
                                 0);        /* NORMAL */
  gimp_image_insert_layer(image_id, l_new_layer_id, l_parent_id, l_position);
  gimp_layer_set_offsets(l_new_layer_id, src_offset_x, src_offset_y);
  

  gimp_item_set_visible(l_new_layer_id, TRUE);
  return (l_new_layer_id);
          
}  /* end gap_layer_new_same_size_and_offsets */
