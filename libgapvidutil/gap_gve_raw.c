/* gap_gve_raw.c by Wolfgang Hofer (hof@gimp.org)
 *
 * (GAP ... GIMP Animation Plugins, now also known as GIMP Video)
 *    Routines for RAW video encoding
 *    useable for uncompressed AVI video.
 *    (GimpDrawable <--> Raw Bitmap Data)
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
 * version 1.2.2; 2003.04.18  hof: gap_gve_raw_YUV420P_drawable_encode get all ros from gimp pixel region with one call
 *                                 (need full buffersize but is faster than get row by row)
 * version 1.2.2; 2002.12.15  hof: created
 */


#include <config.h>


/* SYSTEM (UNIX) includes */
#include <stdio.h>
#include <string.h>

/* GIMP includes */
#include "gtk/gtk.h"
#include "libgimp/gimp.h"

/* GAP includes */
#include "gap_base.h"
#include "gap_gve_raw.h"


/* the raw CODEC needs no extra LIB includes */

typedef struct DrawableToRgbBufferProcessorData {  /* drgb */
    GimpDrawable       *src_drawable;
    GimpImageType       drawable_type;
    GapRgbPixelBuffer  *rgbBuffer;
    gint                startCol;
    gint                colWidth;
    gint                cpuId;
    gboolean            isFinished;
    
} DrawableToRgbBufferProcessorData;

extern int gap_debug;


/*************************************************************
 *          TOOL FUNCTIONS                                   *
 *************************************************************/

/* ------------------------------------
 * gap_gve_init_GapRgbPixelBuffer
 * ------------------------------------
 *
 */
void
gap_gve_init_GapRgbPixelBuffer(GapRgbPixelBuffer *rgbBuffer, guint width, guint height)
{
  rgbBuffer->bpp = 3;
  rgbBuffer->width = width;
  rgbBuffer->height = height;
  rgbBuffer->rowstride = rgbBuffer->width * rgbBuffer->bpp;
  
}  /* end gap_gve_init_GapRgbPixelBuffer */

/* ------------------------------------
 * gap_gve_new_GapRgbPixelBuffer
 * ------------------------------------
 *
 */
GapRgbPixelBuffer *
gap_gve_new_GapRgbPixelBuffer(guint width, guint height)
{
  GapRgbPixelBuffer *rgbBuffer;
  
  rgbBuffer = g_new(GapRgbPixelBuffer, 1);

  gap_gve_init_GapRgbPixelBuffer(rgbBuffer, width, height);

  rgbBuffer->data = (guchar *)g_malloc0((rgbBuffer->height * rgbBuffer->rowstride));
  
  return (rgbBuffer);
  
}  /* end gap_gve_new_GapRgbPixelBuffer */



/* ------------------------------------
 * gap_gve_free_GapRgbPixelBuffer
 * ------------------------------------
 *
 */
void
gap_gve_free_GapRgbPixelBuffer(GapRgbPixelBuffer *rgbBuffer)
{
  if(rgbBuffer)
  {
    if(rgbBuffer->data)
    {
      g_free(rgbBuffer->data);
    }
    g_free(rgbBuffer);
  }
}  /* end gap_gve_free_GapRgbPixelBuffer */




/* ----------------------------------------
 * gap_gve_convert_GapRgbPixelBuffer_To_BGR
 * ----------------------------------------
 */
void
gap_gve_convert_GapRgbPixelBuffer_To_BGR(GapRgbPixelBuffer *rgbBuffer)
{
  gint32   pixelCount;
  guchar  *ptrA;
  guchar  *ptrB;
  
  ptrA = &rgbBuffer->data[0];
  ptrB = &rgbBuffer->data[2];
  
  for (pixelCount = rgbBuffer->width * rgbBuffer->height; pixelCount > 0; pixelCount--)
  {
    guchar tmp;
    
    tmp = *ptrA;
    *ptrA = *ptrB;
    *ptrB = tmp;
    
    ptrA += rgbBuffer->bpp;
    ptrB += rgbBuffer->bpp;
  }
  
}  /* end gap_gve_convert_GapRgbPixelBuffer_To_BGR */


/* ----------------------------------------
 * gap_gve_vflip_GapRgbPixelBuffer
 * ----------------------------------------
 */
void
gap_gve_vflip_GapRgbPixelBuffer(GapRgbPixelBuffer *rgbBuffer)
{
  gint32  row;
  
  for (row = 0; row < rgbBuffer->height / 2; row++)
  {
    gint    upperRow;
    gint32  ii;
    guchar  *ptrA;
    guchar  *ptrB;
    
    upperRow = (rgbBuffer->height -1) - row;
    ptrA = &rgbBuffer->data[row * rgbBuffer->rowstride];
    ptrB = &rgbBuffer->data[upperRow * rgbBuffer->rowstride];
    
    for(ii = 0; ii < rgbBuffer->rowstride; ii++)
    {
      guchar tmp;
      
      tmp = *ptrA;
      *(ptrA++) = *ptrB;
      *(ptrB++) = tmp;
    }
  }
  
}  /* end gap_gve_vflip_GapRgbPixelBuffer */



/* --------------------------------------
 * gap_gve_raw_RGB_or_BGR_drawable_encode
 * --------------------------------------
 * Encode drawable to RAW Buffer (Bytesequence RGB or BGR)
 *
 */
guchar *
gap_gve_raw_RGB_or_BGR_drawable_encode(GimpDrawable *drawable, gint32 *RAW_size, gboolean vflip
                        ,guchar *app0_buffer, gint32 app0_length, gboolean convertToBGR)
{
  GapRgbPixelBuffer  rgbBufferLocal;
  GapRgbPixelBuffer *rgbBuffer;
  guchar *RAW_data;

  rgbBuffer = &rgbBufferLocal;
  gap_gve_init_GapRgbPixelBuffer(rgbBuffer, drawable->width, drawable->height);
  
  *RAW_size = drawable->width * drawable->height * 3;

  RAW_data = (guchar *)g_malloc0((drawable->width * drawable->height * 3)
           + app0_length);
  if(app0_buffer)
  {
    memcpy(RAW_data, app0_buffer, app0_length);
    *RAW_size += app0_length;
  }

  rgbBuffer->data = RAW_data + app0_length;;
  gap_gve_drawable_to_RgbBuffer(drawable, rgbBuffer);
  
  if(convertToBGR)
  {
    gap_gve_convert_GapRgbPixelBuffer_To_BGR(rgbBuffer);
  }
  
  if(vflip == TRUE)
  {
    gap_gve_vflip_GapRgbPixelBuffer(rgbBuffer);
  }

  return(RAW_data);
}    /* end gap_gve_raw_RGB_or_BGR_drawable_encode */


/* ------------------------------------
 * gap_gve_raw_BGR_drawable_encode
 * ------------------------------------
 * Encode drawable to RAW Buffer (Bytesequence BGR)
 * the resulting data can b optionally vertically mirrored
 * and optionally prefixed with header data provided in app0_buffer app0_length
 */
guchar *
gap_gve_raw_BGR_drawable_encode(GimpDrawable *drawable, gint32 *RAW_size, gboolean vflip
                        ,guchar *app0_buffer, gint32 app0_length)
{
  return (gap_gve_raw_RGB_or_BGR_drawable_encode(drawable, RAW_size, vflip, app0_buffer, app0_length, TRUE));
}    /* end gap_gve_raw_BGR_drawable_encode */



/* ------------------------------------
 * gap_gve_raw_RGB_drawable_encode
 * ------------------------------------
 * Encode drawable to RAW Buffer (Bytesequence RGB)
 * the resulting data can b optionally vertically mirrored
 * and optionally prefixed with header data provided in app0_buffer app0_length
 */
guchar *
gap_gve_raw_RGB_drawable_encode(GimpDrawable *drawable, gint32 *RAW_size, gboolean vflip
                        ,guchar *app0_buffer, gint32 app0_length)
{
  return (gap_gve_raw_RGB_or_BGR_drawable_encode(drawable, RAW_size, vflip, app0_buffer, app0_length, FALSE));
}    /* end gap_gve_raw_RGB_drawable_encode */




/* ------------------------------------
 * gap_gve_raw_YUV444_drawable_encode
 * ------------------------------------
 * Encode drawable to RAW Buffer (YUV 4:4:4 colormodel)
 *
 * for image width =5 , height = 3 the returned buffer is filled
 * in the folloing Byte order:
 * YYYYY YYYYY YYYYY UUUUU UUUUU UUUUU VVVVV VVVVV VVVVV
 */
guchar *
gap_gve_raw_YUV444_drawable_encode(GimpDrawable *drawable, gint32 *RAW_size, gboolean vflip
                        ,guchar *app0_buffer, gint32 app0_length
                        ,gint32 matrix_coefficients
                        )
{
  guint j;
  int i;
  int r, g, b;
  double y, u, v;
  double cr, cg, cb, cu, cv;
  unsigned char *yp, *up, *vp;
  static unsigned char *y444, *u444, *v444;
  static double coef[7][3] = {
    {0.2125,0.7154,0.0721}, /* ITU-R Rec. 709 (1990) */
    {0.299, 0.587, 0.114},  /* unspecified */
    {0.299, 0.587, 0.114},  /* reserved */
    {0.30,  0.59,  0.11},   /* FCC */
    {0.299, 0.587, 0.114},  /* ITU-R Rec. 624-4 System B, G */
    {0.299, 0.587, 0.114},  /* SMPTE 170M */
    {0.212, 0.701, 0.087}}; /* SMPTE 240M (1987) */
  GimpPixelRgn srcPR;

  gint32  l_rowstride;
  guchar *pixelrow_data;
  guchar *RAW_data;
  guchar *RAW_ptr;
  GimpImageType drawable_type;
  gint32  l_blue;
  gint32  l_green;
  gint32  l_red;
  guint   l_row;


  drawable_type = gimp_drawable_type (drawable->drawable_id);
  l_rowstride = drawable->width * drawable->bpp;
  pixelrow_data = (guchar *)g_malloc0(l_rowstride);
  *RAW_size = drawable->width * drawable->height * 3;

  RAW_data = (guchar *)g_malloc0((drawable->width * drawable->height * 3)
           + app0_length);
  if(app0_buffer)
  {
    memcpy(RAW_data, app0_buffer, app0_length);
    *RAW_size += app0_length;
  }

  RAW_ptr = RAW_data + app0_length;
  l_red   = 0;
  l_green = 1;
  l_blue  = 2;
  if((drawable_type == GIMP_GRAY_IMAGE)
  || (drawable_type == GIMP_GRAYA_IMAGE))
  {
    l_green = 0;
    l_blue  = 0;
  }


  i = matrix_coefficients;
  if ((i>8) || (i<0))
  {
    i = 3;
  }

  cr = coef[i-1][0];
  cg = coef[i-1][1];
  cb = coef[i-1][2];
  cu = 0.5/(1.0-cb);
  cv = 0.5/(1.0-cr);

  y444 = RAW_ptr;
  u444 = RAW_ptr + (drawable->width * drawable->height);
  v444 = RAW_ptr + ((drawable->width * drawable->height) * 2 );


  /* get drawable */
  gimp_pixel_rgn_init (&srcPR, drawable, 0, 0, drawable->width, drawable->height,
                         FALSE, FALSE);

  /* copy imagedata to yuv rowpointers */
  for(l_row = 0; l_row < drawable->height; l_row++)
  {
     gint32   l_row_x_width;
     guchar *l_bptr;
     gint32 l_src_row;

     if(vflip)  { l_src_row = (drawable->height - 1) - l_row; }
     else       { l_src_row = l_row;}

     l_row_x_width = l_row * drawable->width;
     yp = y444 + l_row_x_width;
     up = u444 + l_row_x_width;
     vp = v444 + l_row_x_width;

    gimp_pixel_rgn_get_row (&srcPR, pixelrow_data, 0, l_src_row, drawable->width);

    l_bptr = pixelrow_data;
    for (j=0; j < drawable->width; j++)
    {
      /* peek RGB (for grey images: R==G==B) */
      r=l_bptr[l_red];
      g=l_bptr[l_green];
      b=l_bptr[l_blue];

      /* convert to YUV */
      y = cr*r + cg*g + cb*b;
      u = cu*(b-y);
      v = cv*(r-y);
      yp[j] = (219.0/256.0)*y + 16.5;  /* nominal range: 16..235 */
      up[j] = (224.0/256.0)*u + 128.5; /* 16..240 */
      vp[j] = (224.0/256.0)*v + 128.5; /* 16..240 */

      l_bptr += drawable->bpp;   /* advance read pointer */
    }
  }

  g_free(pixelrow_data);
  return(RAW_data);
}    /* end gap_gve_raw_YUV444_drawable_encode */



/* ------------------------------
 * gap_gve_raw_YUV420P_drawable_encode
 * ------------------------------
 * - convert Gimp drawable to YUV420P colormodel
 *   suitable for FFMEG Codec input.
 * - both width and height must be a multiple of 2
 * - output is written to yuv420_buffer (without size checks)
 *   the caller must provide this buffer large enough
 *   (width * height * 1.5)
 *
 *   ouput byte sequence example for image width = 6, height = 4
 *   YYYYYY YYYYYY YYYYYY YYYYYY UUU UUU VVV VVV
 *
 */

#define SCALEBITS 8
#define ONE_HALF  (1 << (SCALEBITS - 1))
#define FIX(x)          ((int) ((x) * (1L<<SCALEBITS) + 0.5))

void
gap_gve_raw_YUV420P_drawable_encode(GimpDrawable *drawable, guchar *yuv420_buffer)
{
  guint x, y;
  int wrap, wrap3;
  int r, g, b, r1, g1, b1;
  guchar *p;
  guchar *lum;
  guchar *cb;
  guchar *cr;
  GimpImageType drawable_type;
  GimpPixelRgn srcPR;
  gint32  l_blue;
  gint32  l_green;
  gint32  l_red;
  guchar *all_pixelrows;
  int     size;

  drawable_type = gimp_drawable_type (drawable->drawable_id);
  l_red   = 0;
  l_green = 1;
  l_blue  = 2;
  if((drawable_type == GIMP_GRAY_IMAGE)
  || (drawable_type == GIMP_GRAYA_IMAGE))
  {
    l_red   = 0;
    l_green = 0;
    l_blue  = 0;
  }

  /* init Pixel Region */
  gimp_pixel_rgn_init (&srcPR, drawable, 0, 0, drawable->width, drawable->height,
                         FALSE, FALSE);

  wrap = drawable->width;
  wrap3 = drawable->width * drawable->bpp;
  size = drawable->width * drawable->height;
  lum = yuv420_buffer;
  cb = yuv420_buffer + size;
  cr = cb + (size / 4);

    /* buffer for 2 pixelrows (RGB, RGBA, GREY, or GRAYA Gimp Colormodel) */
    all_pixelrows = g_malloc(wrap3 * drawable->height);

    gimp_pixel_rgn_get_rect (&srcPR, all_pixelrows
                              , 0
                              , 0
                              , drawable->width
                              , drawable->height               /* get all rows */
                              );

    for(y=0;y < drawable->height;y+=2)
    {
        p = all_pixelrows + (y * wrap3);
        for(x=0;x < drawable->width;x+=2)
        {
            r = p[l_red];
            g = p[l_green];
            b = p[l_blue];
            r1 = r;
            g1 = g;
            b1 = b;
            lum[0] = (FIX(0.29900) * r + FIX(0.58700) * g +
                      FIX(0.11400) * b + ONE_HALF) >> SCALEBITS;
            r = p[drawable->bpp + l_red];
            g = p[drawable->bpp + l_green];
            b = p[drawable->bpp + l_blue];
            r1 += r;
            g1 += g;
            b1 += b;
            lum[1] = (FIX(0.29900) * r + FIX(0.58700) * g +
                      FIX(0.11400) * b + ONE_HALF) >> SCALEBITS;

            /* step to next row (same column) */
            p += wrap3;
            lum += wrap;

            r = p[l_red];
            g = p[l_green];
            b = p[l_blue];
            r1 += r;
            g1 += g;
            b1 += b;
            lum[0] = (FIX(0.29900) * r + FIX(0.58700) * g +
                      FIX(0.11400) * b + ONE_HALF) >> SCALEBITS;
            r = p[drawable->bpp + l_red];
            g = p[drawable->bpp + l_green];
            b = p[drawable->bpp + l_blue];
            r1 += r;
            g1 += g;
            b1 += b;
            lum[1] = (FIX(0.29900) * r + FIX(0.58700) * g +
                      FIX(0.11400) * b + ONE_HALF) >> SCALEBITS;

            cb[0] = ((- FIX(0.16874) * r1 - FIX(0.33126) * g1 +
                      FIX(0.50000) * b1 + 4 * ONE_HALF - 1) >> (SCALEBITS + 2)) + 128;
            cr[0] = ((FIX(0.50000) * r1 - FIX(0.41869) * g1 -
                     FIX(0.08131) * b1 + 4 * ONE_HALF - 1) >> (SCALEBITS + 2)) + 128;

            cb++;
            cr++;

            /* backstep to current row and advance 2 column */
            p += -wrap3 + 2 * drawable->bpp;
            lum += -wrap + 2;
        }
        lum += wrap;
    }

    g_free(all_pixelrows);

}   /* end gap_gve_raw_YUV420P_drawable_encode */

/* ------------------------------------
 * gap_gve_raw_YUV420_drawable_encode
 * ------------------------------------
 * Encode drawable to RAW Buffer (YUV420 encoded)
 *
 */
guchar *
gap_gve_raw_YUV420_drawable_encode(GimpDrawable *drawable, gint32 *RAW_size, gboolean vflip
                        ,guchar *app0_buffer, gint32 app0_length)
{
  guchar *RAW_data;
  guchar *RAW_ptr;


  *RAW_size = drawable->width * drawable->height * 3;

  RAW_data = (guchar *)g_malloc0((drawable->width * drawable->height * 3)
           + app0_length);
  if(app0_buffer)
  {
    memcpy(RAW_data, app0_buffer, app0_length);
    *RAW_size += app0_length;
  }
  RAW_ptr = RAW_data + app0_length;

  gap_gve_raw_YUV420P_drawable_encode(drawable, RAW_ptr);

  return(RAW_data);
}    /* end gap_gve_raw_YUV420_drawable_encode */



/* ---------------------------------
 * p_copyPixelRegionToRgbBuffer
 * ---------------------------------
 */
static inline void
p_copyPixelRegionToRgbBuffer (const GimpPixelRgn *srcPR
                    ,const GapRgbPixelBuffer *dstBuff
                    ,GimpImageType drawable_type)
{
  guint    row;
  guchar*  src;
  guchar*  dest;
   
  src  = srcPR->data;
  dest = dstBuff->data 
       + (srcPR->y * dstBuff->rowstride)
       + (srcPR->x * dstBuff->bpp);

  if(srcPR->bpp == dstBuff->bpp)
  {
    /* at same bbp size we can use fast memcpy */
    for (row = 0; row < srcPR->h; row++)
    {
       memcpy(dest, src, srcPR->w * srcPR->bpp);
       src  += srcPR->rowstride;
       dest += dstBuff->rowstride;
    }
    return;
  
  }


  if((srcPR->bpp != dstBuff->bpp)
  && (dstBuff->bpp == 3))
  {
    guchar       *RAW_ptr;
    gint32        l_idx;
    gint32        l_red;
    gint32        l_green;
    gint32        l_blue;

    l_red   = 0;
    l_green = 1;
    l_blue  = 2;
    if((drawable_type == GIMP_GRAY_IMAGE)
    || (drawable_type == GIMP_GRAYA_IMAGE))
    {
      l_green = 0;
      l_blue  = 0;
    }
    
    /* copy gray or rgb channel(s) from src tile to RGB dest buffer */
    for (row = 0; row < srcPR->h; row++)
    {
      RAW_ptr = dest;
      for(l_idx=0; l_idx < srcPR->rowstride; l_idx += srcPR->bpp)
      {
        *(RAW_ptr++) = src[l_idx + l_red];
        *(RAW_ptr++) = src[l_idx + l_green];
        *(RAW_ptr++) = src[l_idx + l_blue];
      }

      src  += srcPR->rowstride;
      dest += dstBuff->rowstride;
    }
    return;
  }

  
  printf("** ERROR p_copyPixelRegionToRgbBuffer: unsupported conversion from src bpp:%d to  dest bpp:%d\n"
    , (int)srcPR->bpp
    , (int)dstBuff->bpp
    );
  
}  /* end p_copyPixelRegionToRgbBuffer */




/* ------------------------------------
 * gap_gve_drawable_to_RgbBuffer        for singleprocessor
 * ------------------------------------
 * Encode drawable to RGBBuffer (Bytesequence RGB)
 *
 * Performance notes:
 * loggging runtime test with 720x480 videoframe was:
 * id:021 gap_gve_drawable_to_RgbBuffer (singleporocessor tiled)            calls:001951 sum:8182955 min:2944 max:18477 avg:4194 usecs
 * id:022 gap_gve_drawable_to_RgbBuffer (singleporocessor rect)             calls:001951 sum:9665986 min:3036 max:18264 avg:4954 usecs
 *
 * Note that this test used gimp-2.6 installation from Suse rpm package
 * (this binary might have been compiled without multiprocessor support ?)
 * TODO: repeat the test with self compiled gimp-2.6 to make sure sure that gimp core
 *       uses multiple threads (that might result in faster processing when
 *       the full frame size is processed at once in the gimp core)
 *       
 *  
 */
void
gap_gve_drawable_to_RgbBuffer(GimpDrawable *drawable, GapRgbPixelBuffer *rgbBuffer)
{
  GimpImageType    drawable_type;
  GimpPixelRgn     srcPR;
  gpointer         pr;

  static gboolean           isPerftestInitialized = FALSE;          
  static gboolean           isPerftestApiTilesDefault;
  static gboolean           isPerftestApiTiles;          /* copy tile-by-tile versus gimp_pixel_rgn_set_rect all at once */

  static gint32 funcIdTile = -1;
  static gint32 funcIdRect = -1;

  GAP_TIMM_GET_FUNCTION_ID(funcIdTile, "gap_gve_drawable_to_RgbBuffer (tiled)");
  GAP_TIMM_GET_FUNCTION_ID(funcIdRect, "gap_gve_drawable_to_RgbBuffer (rect at once)");

  /* PERFTEST configuration values to test performance of various strategies on multiprocessor machines */
  if (isPerftestInitialized != TRUE)
  {
    isPerftestInitialized = TRUE;

    if(gap_base_get_numProcessors() > 1)
    {
      /* copy full size as one big rectangle gives the gimp core the chance to process with more than one thread */
      isPerftestApiTilesDefault = FALSE;
    }
    else
    {
      /* tile based copy was a little bit faster in tests where gimp-core used only one CPU */
      isPerftestApiTilesDefault = TRUE;
    }
    isPerftestApiTiles = gap_base_get_gimprc_gboolean_value("isPerftestApiTiles", isPerftestApiTilesDefault);
  }

  if(isPerftestApiTiles != TRUE)
  {
    if((drawable->bpp == rgbBuffer->bpp)
    && (drawable->width == rgbBuffer->width)
    && (drawable->height == rgbBuffer->height))
    {
      GAP_TIMM_START_FUNCTION(funcIdRect);
 
      gimp_pixel_rgn_init (&srcPR, drawable, 0, 0
                       , drawable->width, drawable->height
                       , FALSE     /* dirty */
                       , FALSE     /* shadow */
                       );
      gimp_pixel_rgn_get_rect (&srcPR, rgbBuffer->data
                       , 0
                       , 0
                       , drawable->width
                       , drawable->height);
 
      GAP_TIMM_STOP_FUNCTION(funcIdRect);
      return;  
    }
  }


  GAP_TIMM_START_FUNCTION(funcIdTile);
  
     
  drawable_type = gimp_drawable_type (drawable->drawable_id);
  gimp_pixel_rgn_init (&srcPR, drawable, 0, 0
                        , drawable->width, drawable->height
                        , FALSE     /* dirty */
                        , FALSE     /* shadow */
                         );
  for (pr = gimp_pixel_rgns_register (1, &srcPR);
       pr != NULL;
       pr = gimp_pixel_rgns_process (pr))
  {
      p_copyPixelRegionToRgbBuffer (&srcPR, rgbBuffer, drawable_type);
  }
  
  GAP_TIMM_STOP_FUNCTION(funcIdTile);



}    /* end gap_gve_drawable_to_RgbBuffer */





// /* --------------------------------------------
//  * p_drawable_to_RgbBuffer_WorkerThreadFunction
//  * --------------------------------------------
//  * this function runs in concurrent parallel worker threads in multiprocessor environment.
//  * each one of the parallel running threads processes another portion of the drawable
//  * (e.g. stripes starting at startCol and colWidth wide. of the processed drawable differs for each thread) 
//  */
// static void
// p_drawable_to_RgbBuffer_WorkerThreadFunction(DrawableToRgbBufferProcessorData *drgb)
// {
//   GimpPixelRgn     srcPR;
//   gpointer  pr;
//   GStaticMutex    *gimpMutex;
// 
//   if(gap_debug)
//   {
//     printf("RgbBuffer_WorkerThreadFunction: START cpu[%d] drawableID:%d (before mutex_lock)\n"
//       ,(int)drgb->cpuId
//       ,(int)drgb->src_drawable->drawable_id
//       );
//   }  
//   gimpMutex = gap_base_get_gimp_mutex();
//    
//   g_static_mutex_lock(gimpMutex);
//  
//   gimp_pixel_rgn_init (&srcPR, drgb->src_drawable
//                       , drgb->startCol, 0
//                       , drgb->colWidth, drgb->src_drawable->height
//                       , FALSE     /* dirty */
//                       , FALSE     /* shadow */
//                        );
//   for (pr = gimp_pixel_rgns_register (1, &srcPR);
//        pr != NULL;
//        pr = gimp_pixel_rgns_process (pr))
//   {
//       g_static_mutex_unlock(gimpMutex);
//       p_copyPixelRegionToRgbBuffer (&srcPR, drgb->rgbBuffer, drgb->drawable_type);
//       if(gap_debug)
//       {
//         printf("RgbBuffer_WorkerThreadFunction: PR-LOOP cpu[%d] drawableID:%d (before mutex_lock)\n"
//           ,(int)drgb->cpuId
//           ,(int)drgb->src_drawable->drawable_id
//           );
//       }  
//       g_static_mutex_lock(gimpMutex);
//   }
// 
//   if(gap_debug)
//   {
//     printf("RgbBuffer_WorkerThreadFunction: DONE cpu[%d] drawableID:%d\n"
//       ,(int)drgb->cpuId
//       ,(int)drgb->src_drawable->drawable_id
//       );
//   }  
// 
//   drgb->isFinished = TRUE;
//   
//   g_static_mutex_unlock(gimpMutex);
//    
// }  /* end p_drawable_to_RgbBuffer_WorkerThreadFunction */
// 
// 
// 
// /* -----------------------------------------
//  * gap_gve_drawable_to_RgbBuffer_multithread
//  * -----------------------------------------
//  * Encode drawable to RGBBuffer (Bytesequence RGB) on multiprocessor machines.
//  * This implementation uses threads to spread the work to the configured
//  * number of processors by using a thread pool.
//  *
//  * ==> tests on my 4 CPU system showed that this runs slower than the singleprocessor implementation
//  *     main reason is:
//  *       lot of time wasted with wait to get the mutex that must be used to synchronize
//  *       access to the gimp plug-in communication
//  */
// void
// gap_gve_drawable_to_RgbBuffer_multithread(GimpDrawable *drawable, GapRgbPixelBuffer *rgbBuffer)
// {
// #define GAP_GVE_MAX_THREADS 16
//   static GThreadPool  *threadPool = NULL;
//   GStaticMutex        *gimpMutex;
//   gboolean             isMultithreadEnabled;
//   gint                 numThreads;
//   gint                 tilesPerRow;
//   gint                 tilesPerCpu;
//   gint                 colsPerCpu;
//   gint                 retry;
//   gint                 startCol;
//   gint                 colWidth;
//   gint                 ii;
//   GimpImageType        drawable_type;
//   DrawableToRgbBufferProcessorData  drgbArray[GAP_GVE_MAX_THREADS];
//   DrawableToRgbBufferProcessorData *drgb;
//   GError *error;
// 
//   static gint32 funcId = -1;
// 
//   GAP_TIMM_GET_FUNCTION_ID(funcId, "gap_gve_drawable_to_RgbBuffer_multithread");
//   GAP_TIMM_START_FUNCTION(funcId);
// 
// 
//   error = NULL;
// 
//    
//   numThreads = MIN(gap_base_get_numProcessors(), GAP_GVE_MAX_THREADS);
// 
//   /* check and init thread system */
//   isMultithreadEnabled = gap_base_thread_init();
//   
//   if((isMultithreadEnabled != TRUE)
//   || (drawable->height < numThreads)
//   || (numThreads < 2))
//   {
//     /* use singleproceesor implementation where multithread variant is not
//      * available or would be slower than singleprocessor implementation
//      */
//     gap_gve_drawable_to_RgbBuffer(drawable, rgbBuffer);
//     return;
//   }
//     
//   if (threadPool == NULL)
//   {
//     GError *error = NULL;
// 
// 
//     /* init the treadPool at first multiprocessing call
//      * (and keep the threads until end of main process..)
//      */
//     threadPool = g_thread_pool_new((GFunc) p_drawable_to_RgbBuffer_WorkerThreadFunction
//                                          ,NULL        /* user data */
//                                          ,GAP_GVE_MAX_THREADS          /* max_threads */
//                                          ,TRUE        /* exclusive */
//                                          ,&error      /* GError **error */
//                                          );
//   }
// 
//   gimpMutex = gap_base_get_gimp_mutex();
//   drawable_type = gimp_drawable_type (drawable->drawable_id);
// 
//   tilesPerRow = drawable->width / gimp_tile_width();
//   tilesPerCpu = tilesPerRow / numThreads;
//   colsPerCpu = tilesPerCpu * gimp_tile_width();
//   if(colsPerCpu < drawable->width)
//   {
//     tilesPerCpu++;
//     colsPerCpu = tilesPerCpu * gimp_tile_width();
//   }
//   
//   if(gap_debug)
//   {
//     printf("gap_gve_drawable_to_RgbBuffer_multithread drawableId :%d size:%d x %d tileWidth:%d numThreads:%d tilesPerCpu:%d colsPerCpu:%d\n"
//       ,(int)drawable->drawable_id
//       ,(int)drawable->width
//       ,(int)drawable->height
//       ,(int)gimp_tile_width()
//       ,(int)numThreads
//       ,(int)tilesPerCpu
//       ,(int)colsPerCpu
//       );
//   }
// 
//   startCol = 0;
//   colWidth = colsPerCpu;
//   
//   if(gap_debug)
//   {
//     printf("gap_gve_drawable_to_RgbBuffer_multithread: before LOCK gimpMutex INITIAL drawableId :%d\n"
//         ,(int)drawable->drawable_id
//       );
//   }
//   g_static_mutex_lock(gimpMutex);
// 
//   for(ii=0; ii < numThreads; ii++)
//   {
//     DrawableToRgbBufferProcessorData *drgb;
// 
//     if(gap_debug)
//     {
//       printf("gap_gve_drawable_to_RgbBuffer_multithread drawableId :%d Cpu[%d] startCol:%d colWidth:%d\n"
//         ,(int)drawable->drawable_id
//         ,(int)ii
//         ,(int)startCol
//         ,(int)colWidth
//         );
//     }
//     
//     drgb = &drgbArray[ii];
//     drgb->drawable_type = drawable_type;
//     drgb->src_drawable = drawable;
//     drgb->rgbBuffer = rgbBuffer;
//     
//     drgb->startCol = startCol;
//     drgb->colWidth = colWidth;
//     drgb->cpuId = ii;
//     drgb->isFinished = FALSE;
//     
//     /* (re)activate thread */
//     g_thread_pool_push (threadPool
//                        , drgb    /* user Data for the worker thread*/
//                        , &error
//                        );
//     startCol += colsPerCpu;
//     if((startCol + colsPerCpu) >  drawable->width)
//     {
//       /* the last thread handles a stripe with the remaining columns
//        * (note that the partitioning into stripes is done in tile with portions
//        * to avoid concurrent thread access to the same tile.)
//        */
//       colWidth = drawable->width - startCol;
//     }
//   }
// 
//   g_static_mutex_unlock(gimpMutex);
// 
//   for(retry = 0; retry < 2000; retry++)
//   {
//     gboolean isAllWorkDone;
// 
//     /* assume all threads finished work (this may already be the case after firts retry) */
//     isAllWorkDone = TRUE;
//     
//     // g_static_mutex_lock(gimpMutex);
//     
//     for(ii=0; ii < numThreads; ii++)
//     {
//       drgb = &drgbArray[ii];
//       if(gap_debug)
//       {
//         printf("gap_gve_drawable_to_RgbBuffer_multithread: STATUS Cpu[%d] retry :%d  isFinished:%d\n"
//           ,(int)ii
//           ,(int)retry
//           ,(int)drgb->isFinished
//           );
//       }
//       if(drgb->isFinished != TRUE)
//       {
//         isAllWorkDone = FALSE;
//         break;
//       }
//     }
// 
//     // g_static_mutex_unlock(gimpMutex);
//      
//     if(isAllWorkDone == TRUE)
//     {
//       break;
//     }
//     
//     if(gap_debug)
//     {
//       printf("gap_gve_drawable_to_RgbBuffer_multithread: SLEEP retry :%d\n"
//         ,(int)retry
//         );
//     }
//   
//     g_usleep (500);
// 
//     if(gap_debug)
//     {
//       printf("gap_gve_drawable_to_RgbBuffer_multithread: WAKE-UP drawableId:%d retry :%d\n"
//         , (int)drawable->drawable_id
//         , (int)retry
//         );
//     }
// 
//     
//   }
// 
// 
//   GAP_TIMM_STOP_FUNCTION(funcId);
// 
//   if(gap_debug)
//   {
//     printf("gap_gve_drawable_to_RgbBuffer_multithread: DONE drawableId:%d retry :%d\n"
//         , (int)drawable->drawable_id
//         , (int)retry
//         );
//   }
//   
// }    /* end gap_gve_drawable_to_RgbBuffer_multithread */
