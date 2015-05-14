/*  gap_fg_regions.c
 *    This module contains gimp region processing utilities
 *    intended as helper procedures for preprocessing and postprocessing
 *    for the foreground sextract plug-in.
 *  2011/10/05
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

/* Revision history
 *  (2011/10/05)  2.7.0       hof: created
 */
#include "config.h"

#include <stdio.h>

#include <gtk/gtk.h>
#include <libgimp/gimp.h>

#include "gap_fg_matting_main.h"
#include "gap_fg_matting.h"
#include "gap_fg_regions.h"

#include "gap-intl.h"

/* -------------------------------------
 * p_rgn_render_copy_alpha_channel
 * -------------------------------------
 * copy RGB channels from src to dst region for the current Tile
 * (src and dst must have same size, but may have bpp 3 or 4)
 */
static void
p_rgn_render_copy_alpha_channel (const GimpPixelRgn *srcPR
                    ,const GimpPixelRgn *dstPR
                    , guint srcAlphaOffs, guint dstAlphaOffs
                    )
{
  guint    row;
  guchar* src  = srcPR->data;
  guchar* dst  = dstPR->data;
 

  for (row = 0; row < dstPR->h; row++)
  {
    guint  col;
    guint  idxSrc;
    guint  idxDst;

    idxSrc = 0;
    idxDst = 0;
    
    for(col = 0; col < dstPR->w; col++)
    {
      dst[idxDst + dstAlphaOffs] = src[idxSrc + srcAlphaOffs];  /* alpha */
    
      idxSrc += srcPR->bpp;
      idxDst += dstPR->bpp;
    }
    
    src += srcPR->rowstride;
    dst += dstPR->rowstride;
  }
}  /* end p_rgn_render_copy_alpha_channel */



static guint
getAlphaOffset(guint bpp)
{
  if (bpp == 4) { return (3); }
  if (bpp == 2) { return (1); }
  return (0);
}


/* ---------------------------------
 * gap_fg_rgn_copy_alpha_channel
 * ---------------------------------
 * 
 * copy alpha channel from src to result region
 * (src and result drawables must have same size, but may differ in bpp)
 * the resultDrawable must have an alpha channel (bpp 2 or 4) or
 * must be a layermask or channel (bpp 1)
 * e.g bpp 3 for the resultDrawable is NOT accepted.
 *
 * Note the caller must also call gimp_drawable_update to write
 * the changes back to gimp's tiles.
 */
void 
gap_fg_rgn_copy_alpha_channel(GimpDrawable *drawable
                   ,GimpDrawable *resultDrawable
                   )
{
  GimpPixelRgn  inputPR;
  GimpPixelRgn  resultPR;
  gpointer      pr;
  
  guint srcAlphaOffs;
  guint dstAlphaOffs;

 
  g_return_if_fail(drawable != NULL && resultDrawable != NULL);

  g_return_if_fail(drawable->width == resultDrawable->width);
  g_return_if_fail(drawable->height == resultDrawable->height);
  g_return_if_fail(drawable->bpp != 3 && resultDrawable->bpp != 3);


  gimp_pixel_rgn_init (&resultPR, resultDrawable
                      , 0, 0     /* x1, y1 */
                      , resultDrawable->width
                      , resultDrawable->height
                      , TRUE      /* dirty */
                      , FALSE     /* shadow */
                      );


  gimp_pixel_rgn_init (&inputPR, drawable
                      , 0, 0     /* x1, y1 */
                      , drawable->width
                      , drawable->height
                      , FALSE     /* dirty */
                      , FALSE     /* shadow */
                      );
  srcAlphaOffs = getAlphaOffset(drawable->bpp);
  dstAlphaOffs = getAlphaOffset(resultDrawable->bpp);
  
  for (pr = gimp_pixel_rgns_register (2, &inputPR, &resultPR);
       pr != NULL;
       pr = gimp_pixel_rgns_process (pr))
  {
    p_rgn_render_copy_alpha_channel (&inputPR, &resultPR
                                    , srcAlphaOffs, dstAlphaOffs);
  }


  /* gimp_drawable_update (resultDrawable->drawable_id, 0, 0, resultDrawable->width, resultDrawable->height); */
  
}  /* end gap_fg_rgn_copy_alpha_channel */




/* -------------------------------------
 * p_rgn_render_copy_rgb_channels
 * -------------------------------------
 * copy RGB channels from src to dst region for the current Tile
 * (src and dst must have same size, but may have bpp 3 or 4)
 */
static void
p_rgn_render_copy_rgb_channels (const GimpPixelRgn *srcPR
                    ,const GimpPixelRgn *dstPR)
{
  guint    row;
  guchar* src  = srcPR->data;
  guchar* dst  = dstPR->data;

  for (row = 0; row < dstPR->h; row++)
  {
    guint  col;
    guint  idxSrc;
    guint  idxDst;

    idxSrc = 0;
    idxDst = 0;
    
    for(col = 0; col < dstPR->w; col++)
    {
      dst[idxDst]    = src[idxSrc];     /* red */
      dst[idxDst +1] = src[idxSrc +1];  /* green */
      dst[idxDst +2] = src[idxSrc +2];  /* blue */
    
      idxSrc += srcPR->bpp;
      idxDst += dstPR->bpp;
    }
    
    src += srcPR->rowstride;
    dst += dstPR->rowstride;
  }
}  /* end p_rgn_render_copy_rgb_channels */




/* ---------------------------------
 * gap_fg_rgn_copy_rgb_channels
 * ---------------------------------
 * 
 * copy RGB channels from src to dst region
 * (src and dst must have same size, but may have bpp 3 or 4)
 *
 * Note the caller must also call gimp_drawable_update to write
 * the changes back to gimp's tiles.
 */
void 
gap_fg_rgn_copy_rgb_channels(GimpDrawable *drawable
                   ,GimpDrawable *resultDrawable
                   )
{
  GimpPixelRgn  inputPR;
  GimpPixelRgn  resultPR;
  gpointer      pr;

 
  g_return_if_fail(drawable != NULL && resultDrawable != NULL);

  g_return_if_fail(drawable->width == resultDrawable->width);
  g_return_if_fail(drawable->height == resultDrawable->height);
  g_return_if_fail(resultDrawable->bpp >= 3);
  g_return_if_fail(drawable->bpp >= 3);


  gimp_pixel_rgn_init (&resultPR, resultDrawable
                      , 0, 0     /* x1, y1 */
                      , resultDrawable->width
                      , resultDrawable->height
                      , TRUE      /* dirty */
                      , FALSE     /* shadow */
                      );


  gimp_pixel_rgn_init (&inputPR, drawable
                      , 0, 0     /* x1, y1 */
                      , drawable->width
                      , drawable->height
                      , FALSE     /* dirty */
                      , FALSE     /* shadow */
                      );
  
  for (pr = gimp_pixel_rgns_register (2, &inputPR, &resultPR);
       pr != NULL;
       pr = gimp_pixel_rgns_process (pr))
  {
    p_rgn_render_copy_rgb_channels (&inputPR, &resultPR);
  }


  /* gimp_drawable_update (resultDrawable->drawable_id, 0, 0, resultDrawable->width, resultDrawable->height); */
  
}  /* end gap_fg_rgn_copy_rgb_channels */









/* -------------------------------------
 * p_rgn_render_normalized_alpha_tri_map
 * -------------------------------------
 * processing variant for the case where the
 * input drawable already has an alpha channel
 */
static void
p_rgn_render_normalized_alpha_tri_map (const GimpPixelRgn *srcPR
                    ,const GimpPixelRgn *maskPR)
{
  guint    row;
  guchar* src  = srcPR->data;
  guchar* mask = maskPR->data;

  for (row = 0; row < maskPR->h; row++)
  {
    guint  col;
    guint  idxMask;
    guint  idxSrc;

    idxMask = 0;
    idxSrc = 0;
    
    for(col = 0; col < maskPR->w; col++)
    {
      if ((mask[idxMask] <= MATTING_ALGO_BACKGROUND)
      ||  (src[idxSrc + (srcPR->bpp -1)] < 1))   // alpha channel
      {
        mask[idxMask]= MATTING_USER_BACKGROUND;   // 0
      }
      else if (mask[idxMask] >= MATTING_USER_FOREGROUND)
      {
        mask[idxMask]= MATTING_USER_FOREGROUND;  // 240
      }
      else
      {
          mask[idxMask]= MATTING_ALGO_UNDEFINED;  // 128
      }
    
    
      idxSrc  += srcPR->bpp;
      idxMask += maskPR->bpp;
    }
    
    src  += srcPR->rowstride;
    mask += maskPR->rowstride;
  }
}  /* end p_rgn_render_normalized_alpha_tri_map */


/* ---------------------------------
 * p_rgn_render_normalized_tri_map
 * ---------------------------------
 * processing variant for the case where the
 * input drawable already has NO alpha channel
 * (and therefore is not relevant for normalizing the tri map)
 */
static void
p_rgn_render_normalized_tri_map (const GimpPixelRgn *maskPR)
{
  guint    row;
  guchar* mask = maskPR->data;
  gint alphaOffset;
  
  alphaOffset = maskPR->bpp -1;


  for (row = 0; row < maskPR->h; row++)
  {
    guint  col;
    guint  idxMask;

    idxMask = 0;
    
    for(col = 0; col < maskPR->w; col++)
    {
      if (mask[idxMask + alphaOffset] <= MATTING_ALGO_BACKGROUND)
      {
        mask[idxMask + alphaOffset]= MATTING_USER_BACKGROUND;   // 0
      }
      else if (mask[idxMask + alphaOffset] >= MATTING_USER_FOREGROUND)
      {
        mask[idxMask + alphaOffset]= MATTING_USER_FOREGROUND;  // 240
      }
      else
      {
          mask[idxMask + alphaOffset]= MATTING_ALGO_UNDEFINED;  // 128
      }
    
      idxMask += maskPR->bpp;
    }
    mask += maskPR->rowstride;
  }
}  /* end p_rgn_render_normalized_tri_map */


/* ---------------------------------
 * gap_fg_rgn_tri_map_normalize
 * ---------------------------------
 * 
 * set tri map pixels to one of the  values:
 *   0,  MATTING_USER_BACKGROUND
 *   128 undefined
 *   240 MATTING_USER_FOREGROUND
 *
 * depending on the original mask value and the alpha channel
 * of the input drawable.
 */
void 
gap_fg_rgn_tri_map_normalize(GimpDrawable *drawable
                         , gint32 maskId
                         )
{
  GimpPixelRgn  inputPR;
  GimpPixelRgn  maskPR;
  GimpDrawable *maskDrawable;
  gpointer      pr;

 
  maskDrawable = gimp_drawable_get (maskId);

  g_return_if_fail(drawable->width == maskDrawable->width);
  g_return_if_fail(drawable->height == maskDrawable->height);
  g_return_if_fail(maskDrawable->bpp == 1);


  gimp_pixel_rgn_init (&maskPR, maskDrawable
                      , 0, 0     /* x1, y1 */
                      , maskDrawable->width
                      , maskDrawable->height
                      , TRUE      /* dirty */
                      , FALSE     /* shadow */
                      );

  if (gimp_drawable_has_alpha(drawable->drawable_id))
  {
    gimp_pixel_rgn_init (&inputPR, drawable
                      , 0, 0     /* x1, y1 */
                      , drawable->width
                      , drawable->height
                      , FALSE     /* dirty */
                      , FALSE     /* shadow */
                      );
  
    for (pr = gimp_pixel_rgns_register (2, &inputPR, &maskPR);
         pr != NULL;
         pr = gimp_pixel_rgns_process (pr))
    {
        p_rgn_render_normalized_alpha_tri_map (&inputPR, &maskPR);
    }
  }
  else
  {
    for (pr = gimp_pixel_rgns_register (1, &maskPR);
         pr != NULL;
         pr = gimp_pixel_rgns_process (pr))
    {
        p_rgn_render_normalized_tri_map (&maskPR);
    }
  }
 
 

  /*  update the processed region  */
  gimp_drawable_flush (maskDrawable);
  gimp_drawable_detach(maskDrawable);
  
}  /* end gap_fg_rgn_tri_map_normalize */





/* ----------------------------------
 * gap_fg_rgn_normalize_alpha_channel
 * ----------------------------------
 * 
 * set alpha channel to one of the  values:
 *   0,  MATTING_USER_BACKGROUND
 *   128 undefined
 *   240 MATTING_USER_FOREGROUND
 *
 * depending on the original alpha channel
 * of the input drawable.
 */
void
gap_fg_rgn_normalize_alpha_channel(GimpDrawable *drawable)
{
  GimpPixelRgn  inputPR;
  gpointer      pr;

  gimp_pixel_rgn_init (&inputPR, drawable
                      , 0, 0     /* x1, y1 */
                      , drawable->width
                      , drawable->height
                      , TRUE     /* dirty */
                      , FALSE     /* shadow */
                      );
  
  for (pr = gimp_pixel_rgns_register (1, &inputPR);
       pr != NULL;
       pr = gimp_pixel_rgns_process (pr))
  {
      p_rgn_render_normalized_tri_map (&inputPR);
  }
  gimp_drawable_flush (drawable);
  
}  /* end gap_fg_rgn_normalize_alpha_channel */
