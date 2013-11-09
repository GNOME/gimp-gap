/*  gap_pview_da.c
 *
 *  This module handles GAP drawing_area rendering from thumbnail data buffer
 *  or alternative from a gimp image
 *  (used for video playback preview)
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
 * <http://www.gnu.org/licenses/>.
 */

/* revision history:
 * version 2.0.1b;  2004/05/01  hof: dont attempt to render negative (invalid) image_id's
 * version 1.3.26a; 2004/01/30  hof: added gap_pview_drop_repaint_buffers 
 * version 1.3.25a; 2004/01/22  hof: added gap_pview_render_from_pixbuf 
 * version 1.3.24a; 2004/01/17  hof: speed up gap_pview_render_from_buf 
 *                                   faster rendering of fully opaque pixels
 *                                   for buffers with alpha channel
 * version 1.3.16a; 2003/06/26  hof: use aspect_frame instead of simple frame
 *                                   added gap_pview_render_default_icon
 * version 1.3.14c; 2003/06/19  hof: created
 */

#include "config.h"

#include <stdio.h>
#include <string.h>

#include <gtk/gtk.h>
#include <libgimp/gimp.h>
#include <libgimp/gimpui.h>
#include "gap_pview_da.h"
#include "gap-intl.h"
#include "gap_lib_common_defs.h"


extern int gap_debug;  /* 1 == print debug infos , 0 dont print debug infos */


#define MIX_CHANNEL(b, a, m)  (((a * m) + (b * (255 - m))) / 255)
#define PREVIEW_BG_GRAY1 80
#define PREVIEW_BG_GRAY2 180

#define PREVIEW_BG_GRAY1_GDK 0x505050
#define PREVIEW_BG_GRAY2_GDK 0xb4b4b4

static void     p_replace_pixbuf_by_flipped_pixbuf(GapPView *pv_ptr, gint32 flip_to_perform);
static gint32   p_calculate_flip_request(GapPView *pv_ptr, gint32 flip_request, gint32 flip_status);
static void     p_render_thdata_as_flipped_pixbuf(GapPView *pv_ptr, gint32 flip_to_perform);
static void     p_pview_repaint_desaturated(GapPView *pv_ptr);
static void     p_desaturate(GapPView *pv_ptr, guchar *src_buff, gint src_rowstride);
static void     p_free_desaturated_area_data(GapPView *pv_ptr);
static void     p_create_desaturated_buf_from_pixbuf(GapPView *pv_ptr);
static void     p_create_desaturated_buf_from_area_data(GapPView *pv_ptr);


/* ----------------------------------------------------
 * p_replace_pixbuf_by_flipped_pixbuf
 * ----------------------------------------------------
 */
static void
p_replace_pixbuf_by_flipped_pixbuf(GapPView *pv_ptr, gint32 flip_to_perform)
{
  GdkPixbuf *flipped_pixbuf;
  gboolean   horizontal;

  if(pv_ptr->pixbuf == NULL)
  {
    printf("ERROR cant flip pv_ptr->pixbuf becaus it is NULL\n");
    return;
  }
  
  flipped_pixbuf = NULL;
  switch(flip_to_perform)
  {
    case GAP_STB_FLIP_HOR:
      horizontal = TRUE;
      flipped_pixbuf = gdk_pixbuf_flip(pv_ptr->pixbuf
                                      ,horizontal
                                      );
      break;
    case GAP_STB_FLIP_VER:
      horizontal = FALSE;
      flipped_pixbuf = gdk_pixbuf_flip(pv_ptr->pixbuf
                                      ,horizontal
                                      );
      break;
    case GAP_STB_FLIP_BOTH:
      flipped_pixbuf = gdk_pixbuf_rotate_simple (pv_ptr->pixbuf
                                                ,GDK_PIXBUF_ROTATE_UPSIDEDOWN
                                                );
      break;
    default:
      break;
  }
  if(flipped_pixbuf)
  {
    /* free old (refresh) pixbuf if there is one
     * and replace by fliped variant
     */
    g_object_unref(pv_ptr->pixbuf);
    pv_ptr->pixbuf = flipped_pixbuf;
  }

}  /* end p_replace_pixbuf_by_flipped_pixbuf */


/* ----------------------------------------------------
 * p_calculate_flip_request
 * ----------------------------------------------------
 * return the flip action to be performed on an image
 * that was already transformed as described by flip_status
 * and should be transformed as described by flip_request.
 *
 * in case both values are equal, no transformation is necessary
 * different bits require the corresponding transformation. 
 * (XOR delvers the required result)
 *
 * request  status      return
 *    0       0           0
 *    1       0           1
 *    2       0           2
 *    3       0           3
 *
 * request  status      return
 *    0       1           1
 *    1       1           0
 *    2       1           3
 *    3       1           2
 *
 * request  status      return
 *    0       2           2
 *    1       2           3
 *    2       2           0
 *    3       2           1
 *
 * request  status      return
 *    0       3           3
 *    1       3           2
 *    2       3           1
 *    3       3           0
 */
static gint32
p_calculate_flip_request(GapPView *pv_ptr, gint32 flip_request, gint32 flip_status)
{
  pv_ptr->flip_status = flip_request;
  return ((flip_request ^ flip_status) & 3);
}  /* end p_calculate_flip_request */



/* ----------------------------------------------------
 * p_render_thdata_as_flipped_pixbuf
 * ----------------------------------------------------
 */
static void
p_render_thdata_as_flipped_pixbuf(GapPView *pv_ptr
  , gint32 flip_to_perform
  )
{
  if(pv_ptr->pixbuf)
  {
    /* free old (refresh) pixbuf if there is one
     * and replace by fliped variant
     */
    g_object_unref(pv_ptr->pixbuf);
  }
  
  /* create a pixbuf from pv_area_data
   * use NULL because we dont want to run the destructor notifyer
   * (g_free of pv_area_data is already handled in gap_pview_reset)
   */
  pv_ptr->pixbuf = gdk_pixbuf_new_from_data(pv_ptr->pv_area_data
              , GDK_COLORSPACE_RGB
              , FALSE                 /* has_alpha */
              , 8                     /* bits_per_sample */
              , pv_ptr->pv_width
              , pv_ptr->pv_height
              , pv_ptr->pv_width * pv_ptr->pv_bpp  /* rowstride */
              , NULL                  /* GdkPixbufDestroyNotify NULL: dont call destroy_fn */
              , pv_ptr->pv_area_data  /* gpointer destroy_fn_data */
        );
        
  /* clear flag to let gap_pview_repaint procedure know
   * to use the pixbuf rather than pv_area_data or pixmap for refresh
   */
  pv_ptr->use_pixmap_repaint = FALSE;
  pv_ptr->use_pixbuf_repaint = TRUE;

  p_replace_pixbuf_by_flipped_pixbuf(pv_ptr, flip_to_perform);
  gap_pview_repaint(pv_ptr);

}  /* end p_render_thdata_as_flipped_pixbuf */




/* ----------------------------------------------------
 * p_pview_repaint_desaturated
 * ----------------------------------------------------
 * repaint from the desaturated buffer.
 */
static void
p_pview_repaint_desaturated(GapPView *pv_ptr)
{
  if(gap_debug)
  {
    printf("p_pview_repaint_desaturated START\n");
  }
  if(pv_ptr->pv_desaturated_area_data)
  {
    gdk_draw_rgb_image ( pv_ptr->da_widget->window
                       , pv_ptr->da_widget->style->white_gc
                       , 0
                       , 0
                       , pv_ptr->pv_width
                       , pv_ptr->pv_height
                       , GDK_RGB_DITHER_NORMAL
                       , pv_ptr->pv_desaturated_area_data
                       , pv_ptr->pv_width * 3
                       );
  }

}  /* end p_pview_repaint_desaturated */  
  

/* ----------------------------------------------------
 * p_desaturate
 * ----------------------------------------------------
 * fill the desaturated_area_data with a grayscale copy
 * of the specified src_buff.
 * NOTE: both buffers are same size and have bpp 3 (for RGB)
 * desaturation uses the same gray value for all 3 color channels.
 */
static void
p_desaturate(GapPView *pv_ptr, guchar *src_buff, gint src_rowstride)
{
  gint didx;
  gint sidx;
  gint src_bpp;
  gint row;
  gint col;
  gint dst_rowstride;
  
  
  src_bpp = src_rowstride / MAX(1, pv_ptr->pv_width);
  dst_rowstride = pv_ptr->pv_width * pv_ptr->pv_bpp;
  
  if((pv_ptr->pv_desaturated_area_data)
  && (src_buff))
  {
    sidx = 0;
    didx = 0;
    for(row=0; row < pv_ptr->pv_height; row++ )
    {
      sidx = row * src_rowstride;
      didx = row * dst_rowstride;
      for(col=0; col < pv_ptr->pv_width; col++)
      {
        gdouble grayvalue;
    
        grayvalue = ((gdouble)src_buff[sidx] 
              + (gdouble)src_buff[sidx + 1] 
              + (gdouble)src_buff[sidx + 2]) / 3.0;
        pv_ptr->pv_desaturated_area_data[didx] = (guchar)grayvalue;
        pv_ptr->pv_desaturated_area_data[didx + 1] = (guchar)grayvalue;
        pv_ptr->pv_desaturated_area_data[didx + 2] = (guchar)grayvalue;
      
        sidx += src_bpp;
        didx += pv_ptr->pv_bpp;
      }
    }
  }

}  /* end p_desaturate */


/* ----------------------------------------------------
 * p_free_desaturated_area_data
 * ----------------------------------------------------
 */
static void
p_free_desaturated_area_data(GapPView *pv_ptr)
{  

  if(pv_ptr->pv_desaturated_area_data)
  {
    g_free(pv_ptr->pv_desaturated_area_data);
    pv_ptr->pv_desaturated_area_data = NULL;
  }

}  /* end p_free_desaturated_area_data */

/* ----------------------------------------------------
 * p_create_desaturated_buf_from_pixbuf
 * ----------------------------------------------------
 */
static void
p_create_desaturated_buf_from_pixbuf(GapPView *pv_ptr)
{  

  pv_ptr->pv_desaturated_area_data = g_new ( guchar
                                      , pv_ptr->pv_height * pv_ptr->pv_width * pv_ptr->pv_bpp );

  p_desaturate(pv_ptr
             , gdk_pixbuf_get_pixels(pv_ptr->pixbuf)
             , gdk_pixbuf_get_rowstride(pv_ptr->pixbuf)
             );

}  /* end p_create_desaturated_buf_from_pixbuf */


/* ----------------------------------------------------
 * p_create_desaturated_buf_from_area_data
 * ----------------------------------------------------
 */
static void
p_create_desaturated_buf_from_area_data(GapPView *pv_ptr)
{  
  gint l_rowstride;
  
  l_rowstride = pv_ptr->pv_width * pv_ptr->pv_bpp;
  pv_ptr->pv_desaturated_area_data = g_new ( guchar
                                      , pv_ptr->pv_height *  l_rowstride);

  p_desaturate(pv_ptr, pv_ptr->pv_area_data, l_rowstride);

}  /* end p_create_desaturated_buf_from_area_data */




/* ------------------------------
 * gap_pview_render_from_buf
 * ------------------------------
 */
gboolean
gap_pview_render_from_buf (GapPView *pv_ptr
                 , guchar *src_data
                 , gint    src_width
                 , gint    src_height
                 , gint    src_bpp
                 , gboolean allow_grab_src_data
                 )
{
  return gap_pview_render_f_from_buf(pv_ptr
                                    ,src_data
                                    ,src_width
                                    ,src_height
                                    ,src_bpp
                                    ,allow_grab_src_data
                                    ,GAP_STB_FLIP_NONE
                                    ,GAP_STB_FLIP_NONE
                                    );
}  /* end gap_pview_render_from_buf */

/* ------------------------------
 * gap_pview_render_from_image
 * ------------------------------
 */
void
gap_pview_render_from_image (GapPView *pv_ptr, gint32 image_id)
{
  gap_pview_render_f_from_image(pv_ptr
                               ,image_id
                               ,GAP_STB_FLIP_NONE
                               ,GAP_STB_FLIP_NONE
                               );
}  /* end gap_pview_render_from_image */


/* -------------------------------------
 * gap_pview_render_from_image_duplicate
 * -------------------------------------
 */
void
gap_pview_render_from_image_duplicate (GapPView *pv_ptr, gint32 image_id)
{
  gap_pview_render_f_from_image_duplicate(pv_ptr
                                         ,image_id
                                         ,GAP_STB_FLIP_NONE
                                         ,GAP_STB_FLIP_NONE
                                         );
}  /* end gap_pview_render_from_image_duplicate */


/* ------------------------------
 * gap_pview_render_from_pixbuf
 * ------------------------------
 */
void
gap_pview_render_from_pixbuf (GapPView *pv_ptr, GdkPixbuf *src_pixbuf)
{
  gap_pview_render_f_from_pixbuf(pv_ptr
                                ,src_pixbuf
                                ,GAP_STB_FLIP_NONE
                                ,GAP_STB_FLIP_NONE
                                );
}       /* end gap_pview_render_from_pixbuf */




/* ------------------------------
 * gap_pview_drop_repaint_buffers
 * ------------------------------
 */
void
gap_pview_drop_repaint_buffers(GapPView *pv_ptr)
{
  if(pv_ptr->pixmap)        g_object_unref(pv_ptr->pixmap);
  if(pv_ptr->pixbuf)        g_object_unref(pv_ptr->pixbuf);

  pv_ptr->pixmap = NULL;
  pv_ptr->pixbuf = NULL;
  
  p_free_desaturated_area_data(pv_ptr);
  
} /* end gap_pview_drop_repaint_buffers */

/* ------------------------------
 * gap_pview_reset
 * ------------------------------
 * reset/free precalculated stuff
 */
void
gap_pview_reset(GapPView *pv_ptr)
{
  if(pv_ptr == NULL)
  {
    return;
  }
  if(pv_ptr->src_col) g_free(pv_ptr->src_col);
  if(pv_ptr->pv_area_data)  g_free(pv_ptr->pv_area_data);

  gap_pview_drop_repaint_buffers(pv_ptr);
  
  pv_ptr->src_col = NULL;
  pv_ptr->pv_area_data = NULL;
  pv_ptr->src_width = 0;
  pv_ptr->src_bpp = 0;
  pv_ptr->src_rowstride = 0;
  pv_ptr->use_pixmap_repaint = FALSE;
  pv_ptr->use_pixbuf_repaint = FALSE;
  
} /* end gap_pview_reset */


/* ------------------------------
 * gap_pview_set_size
 * ------------------------------
 * set new preview size
 * and allocate buffers for src columns and row at previesize
 */
void
gap_pview_set_size(GapPView *pv_ptr, gint pv_width, gint pv_height, gint pv_check_size)
{
  g_return_if_fail (pv_width > 0 && pv_height > 0);

  if(pv_ptr->da_widget == NULL) { return; }

  gap_pview_reset(pv_ptr);

  gtk_widget_set_size_request (pv_ptr->da_widget, pv_width, pv_height);
  if(pv_ptr->aspect_frame)
  { 
    gtk_aspect_frame_set (GTK_ASPECT_FRAME(pv_ptr->aspect_frame)
                         ,0.5   /* align x centered */
                         ,0.5   /* align y centered */
                         , pv_width / pv_height
                         , TRUE  /* obey_child */
                         );
    gtk_widget_set_size_request (pv_ptr->aspect_frame
                                , pv_width+5
                                , (gdouble)(pv_width+5) * ( (gdouble)pv_height / (gdouble)pv_width)                       /* pv_height */
                                );
  }
  pv_ptr->pv_width = pv_width;
  pv_ptr->pv_height = pv_height;
  pv_ptr->pv_check_size = pv_check_size;

  pv_ptr->pv_area_data = g_new ( guchar
                                    , pv_ptr->pv_height * pv_ptr->pv_width * pv_ptr->pv_bpp );
  pv_ptr->src_col = g_new ( gint, pv_ptr->pv_width );                   /* column fetch indexes foreach preview col */

  if(pv_ptr->pv_desaturated_area_data)
  {
    g_free(pv_ptr->pv_desaturated_area_data);
    pv_ptr->pv_desaturated_area_data = NULL;
  }
  
}  /* end gap_pview_set_size */


/* ------------------------------
 * gap_pview_new
 * ------------------------------
 * pv_check_size is checkboard size in pixels
 * (for transparent pixels)
 */
GapPView *
gap_pview_new(gint pv_width, gint pv_height, gint pv_check_size, GtkWidget *aspect_frame)
{
  GapPView *pv_ptr;
 
  pv_ptr = g_malloc0(sizeof(GapPView));
  pv_ptr->pv_bpp = 3;
 
  pv_ptr->da_widget = gtk_drawing_area_new ();
  pv_ptr->aspect_frame = aspect_frame;
  gap_pview_set_size(pv_ptr, pv_width, pv_height, pv_check_size);
  pv_ptr->use_pixmap_repaint = FALSE;
  pv_ptr->use_pixbuf_repaint = FALSE;
  pv_ptr->flip_status = GAP_STB_FLIP_NONE;
  pv_ptr->pixmap = NULL;
  pv_ptr->pixbuf = NULL;
  pv_ptr->desaturate_request = FALSE;

  return(pv_ptr);
}  /* end gap_pview_new */


/* ------------------------------
 * gap_pview_repaint
 * ------------------------------
 * Repaint (implicite conversion of gray_pixbuf)
 */
void
gap_pview_repaint(GapPView *pv_ptr)
{
  if(pv_ptr == NULL) { return; }
  if(pv_ptr->da_widget == NULL) { return; }
  if(pv_ptr->da_widget->window == NULL) { return; }

  if((pv_ptr->pixbuf != NULL)
  && (pv_ptr->use_pixbuf_repaint))
  {
    if(pv_ptr->desaturate_request)
    {
      if(pv_ptr->pv_desaturated_area_data == NULL)
      {
        p_create_desaturated_buf_from_pixbuf(pv_ptr);
      }
      if(pv_ptr->pv_desaturated_area_data)
      {
        p_pview_repaint_desaturated(pv_ptr);
        return;
      }
    }
    gdk_draw_pixbuf(
                     pv_ptr->da_widget->window
                   , pv_ptr->da_widget->style->white_gc
                   , pv_ptr->pixbuf
                   , 0                         /*  gint src_x  */
                   , 0                         /*  gint src_y  */
                   , 0                         /*  gint dest_x */
                   , 0                         /*  gint dest_y */
                   , pv_ptr->pv_width
                   , pv_ptr->pv_height
                   , GDK_RGB_DITHER_NORMAL
                   , 0                         /* gint x_dither_offset */
                   , 0                         /* gint y_dither_offset */
                   );
    return;
  }
  
  
  if((pv_ptr->pv_area_data != NULL)
  && (!pv_ptr->use_pixmap_repaint))
  {
    if(pv_ptr->desaturate_request)
    {
      if(pv_ptr->pv_desaturated_area_data == NULL)
      {
        p_create_desaturated_buf_from_area_data(pv_ptr);
      }
      if(pv_ptr->pv_desaturated_area_data)
      {
        p_pview_repaint_desaturated(pv_ptr);
        return;
      }
    }
    gdk_draw_rgb_image ( pv_ptr->da_widget->window
                       , pv_ptr->da_widget->style->white_gc
                       , 0
                       , 0
                       , pv_ptr->pv_width
                       , pv_ptr->pv_height
                       , GDK_RGB_DITHER_NORMAL
                       , pv_ptr->pv_area_data
                       , pv_ptr->pv_width * 3
                       );
    return;
  }

  
  if(pv_ptr->pixmap != NULL)
  {
    gdk_draw_drawable(pv_ptr->da_widget->window
                   ,pv_ptr->da_widget->style->white_gc
                   ,pv_ptr->pixmap
                   ,0
                   ,0
                   ,0
                   ,0
                   ,pv_ptr->pv_width
                   ,pv_ptr->pv_height
                   );
  }
}  /* end gap_pview_repaint */


/* ------------------------------
 * gap_pview_render_f_from_buf
 * ------------------------------
 * render drawing_area widget from src_data buffer
 * quick scaling without any interpolation
 * is done to fit into preview size. 
 * the GapPView structure holds
 * precalculations to allow faster scaling
 * in multiple calls when the src_data dimensions
 * do not change.
 *
 * IN: allow_grab_src_data
 *         if TRUE, grant permission to grab the src_data for internal (refresh) usage
 *         this is a performance feature with benefit for the case of matching buffer size
 *         without alpha channel.
 *
 * return TRUE in case the src_data really was grabbed.
 *             in such a case the caller MUST NOT change the src_data after
 *             the call, and MUST NOT free the src_data buffer.
 *        FALSE src_data was not grabbed, the caller is still responsible
 *             to g_free the src_data
 *            
 */
gboolean
gap_pview_render_f_from_buf (GapPView *pv_ptr
                 , guchar *src_data
                 , gint    src_width
                 , gint    src_height
                 , gint    src_bpp
                 , gboolean allow_grab_src_data
                 , gint32 flip_request
                 , gint32 flip_status
                 )
{
  register guchar  *src, *dest;
  register gint    col;
  gint           row;
  gint           ofs_green, ofs_blue, ofs_alpha;

  if(pv_ptr == NULL) { return FALSE; }
  if(pv_ptr->da_widget == NULL) { return FALSE; }
  if(pv_ptr->da_widget->window == NULL)
  {
    if(gap_debug)
    {
      printf("gap_pview_render_f_from_buf: drawing_area window pointer is NULL, cant render\n");
    }
    return FALSE;
  }

  /* clear flag to let gap_pview_repaint procedure know
   * to use the pv_area_data rather than the pixmap for refresh
   */
  pv_ptr->use_pixmap_repaint = FALSE;
  pv_ptr->use_pixbuf_repaint = FALSE;
  p_free_desaturated_area_data(pv_ptr);

  /* check for col and area_data buffers (allocate if needed) */
  if ((pv_ptr->src_col == NULL)
  ||  (pv_ptr->pv_area_data == NULL))
  {
     gint pv_width, pv_height, pv_check_size;
     
     pv_width = pv_ptr->pv_width;
     pv_height = pv_ptr->pv_height;
     pv_check_size = pv_ptr->pv_check_size;
     
     gap_pview_set_size(pv_ptr, pv_width, pv_height, pv_check_size);
  }
  
  /* init column fetch indexes array (only needed on src size changes) */
  if((src_width != pv_ptr->src_width)
  || (src_bpp != pv_ptr->src_bpp))
  {
     pv_ptr->src_width = src_width;
     pv_ptr->src_bpp   = src_bpp;
     pv_ptr->src_rowstride = src_width * src_bpp;

     /* array of column fetch indexes for quick access
      * to the source pixels. (The source row may be larger or smaller than pv_ptr->pv_width)
      */
     for ( col = 0; col < pv_ptr->pv_width; col++ )
     {
       pv_ptr->src_col[ col ] = ( col * src_width / pv_ptr->pv_width ) * src_bpp;
     }
  }

  if(src_data == NULL)
  { 
    /* if(gap_debug) printf("\n   Paint it Black (NO src_data)\n"); */

    /* Source is empty: draw a black preview */
    memset(pv_ptr->pv_area_data, 0, pv_ptr->pv_height * pv_ptr->pv_width * pv_ptr->pv_bpp);
    gap_pview_repaint(pv_ptr);
    return FALSE;
  }

  if ((src_width == pv_ptr->pv_width)
  &&  (src_height == pv_ptr->pv_height)
  &&  (src_bpp == 3)
  &&  (flip_request == flip_status))
  {

    /* if(gap_debug) printf("\n\n   can use QUICK render\n"); */

    /* for RGB src_data with matching size and without Alpha
     * we can render with just one call (fastest)
     */
    if( allow_grab_src_data )
    {
       /* the calling procedure has permitted to grab the src_data
        * for internal refresh usage.
        */
       g_free(pv_ptr->pv_area_data);
       pv_ptr->pv_area_data = src_data;
       gap_pview_repaint(pv_ptr);
       return TRUE;
    }
    else
    {
      /* here we make a clean copy of src_data to pv_ptr->pv_area_data.
       *  beacause the data might be needed
       *  for refresh of the display (for an "expose" event handler procedure)
       */
       memcpy(pv_ptr->pv_area_data, src_data, pv_ptr->pv_height * pv_ptr->pv_width * pv_ptr->pv_bpp);
    }     
 
    gap_pview_repaint(pv_ptr);
    return FALSE;
  }

  
  ofs_green = 1;
  ofs_blue  = 2;
  ofs_alpha = 3;
  if(src_bpp < 3)
  {
    ofs_green = 0;
    ofs_blue  = 0;
    ofs_alpha = 1;
  }


  if((src_bpp ==3) || (src_bpp == 1))
  {
    guchar   *src_row;
 
    /* Source is Opaque (has no alpha) draw preview row by row */
    dest = pv_ptr->pv_area_data;
    for ( row = 0; row < pv_ptr->pv_height; row++ )
    {
        src_row = &src_data[CLAMP(((row * src_height) / pv_ptr->pv_height)
                                  , 0
                                  , src_height)
                                  * pv_ptr->src_rowstride];
        for ( col = 0; col < pv_ptr->pv_width; col++ )
        {
            src = &src_row[ pv_ptr->src_col[col] ];
            *(dest++) = src[0];
            *(dest++) = src[ofs_green];
            *(dest++) = src[ofs_blue];
        }
    }
  }
  else
  {
    guchar   *src_row;
    guchar   alpha;
    gint     ii;

    /* Source has alpha, draw preview row by row
     * show checkboard as background for transparent pixels
     */
    dest = pv_ptr->pv_area_data;
    for ( row = 0; row < pv_ptr->pv_height; row++ )
    {
      
        if ((row / pv_ptr->pv_check_size) & 1) { ii = 0;}
        else                                   { ii = pv_ptr->pv_check_size; }
        
        src_row = &src_data[CLAMP(((row * src_height) / pv_ptr->pv_height)
                                  , 0
                                  , src_height)
                                  * pv_ptr->src_rowstride];
        for ( col = 0; col < pv_ptr->pv_width; col++ )
        {
          src = &src_row[ pv_ptr->src_col[col] ];
          alpha = src[ofs_alpha];
          if(alpha > 244)
          {
            /* copy full (or nearly full opaque) pixels 1:1
             * without MIXING with checkerboard background.
             * (speeds up rendering of opaque pixels)
             */
            *(dest++) = src[0];
            *(dest++) = src[ofs_green];
            *(dest++) = src[ofs_blue];
          }
          else
          {
            if(((col+ii) / pv_ptr->pv_check_size) & 1)
            {
              if(alpha < 10)
              {
                *(dest++) = PREVIEW_BG_GRAY1;
                *(dest++) = PREVIEW_BG_GRAY1;
                *(dest++) = PREVIEW_BG_GRAY1;
              }
              else
              {
                *(dest++) = MIX_CHANNEL (PREVIEW_BG_GRAY1, src[0], alpha);
                *(dest++) = MIX_CHANNEL (PREVIEW_BG_GRAY1, src[ofs_green], alpha);
                *(dest++) = MIX_CHANNEL (PREVIEW_BG_GRAY1, src[ofs_blue], alpha);
              }
            }
            else
            {
              if(alpha < 10)
              {
                *(dest++) = PREVIEW_BG_GRAY2;
                *(dest++) = PREVIEW_BG_GRAY2;
                *(dest++) = PREVIEW_BG_GRAY2;
              }
              else
              {
                *(dest++) = MIX_CHANNEL (PREVIEW_BG_GRAY2, src[0], alpha);
                *(dest++) = MIX_CHANNEL (PREVIEW_BG_GRAY2, src[ofs_green], alpha);
                *(dest++) = MIX_CHANNEL (PREVIEW_BG_GRAY2, src[ofs_blue], alpha);
              }
            }
          }
        }
    }
  }

  /* check and perform built in flip transformations */
  {
    gint32 l_flip_to_perform;
  
    l_flip_to_perform = p_calculate_flip_request(pv_ptr, flip_request, flip_status);
    if(l_flip_to_perform != GAP_STB_FLIP_NONE)
    {
      p_render_thdata_as_flipped_pixbuf(pv_ptr
                                ,l_flip_to_perform
                                );
      return FALSE;
    }
  }
  gap_pview_repaint(pv_ptr);

  return FALSE;

}       /* end gap_pview_render_f_from_buf */


/* ------------------------------
 * gap_pview_render_f_from_image
 * ------------------------------
 * render preview widget from image.
 * IMPORTANT: the image is scaled to preview size
 *            and the visible layers in the image are merged together !
 *            If the supplied image shall stay unchanged,
 *            you may use  gap_pview_render_f_from_image_duplicate
 *
 * hint: call gtk_widget_queue_draw(pv_ptr->da_widget);
 * after this procedure to make the changes appear on screen.
 */
void
gap_pview_render_f_from_image (GapPView *pv_ptr
    , gint32 image_id
    , gint32 flip_request
    , gint32 flip_status
)
{
  gint32 layer_id;
  guchar *frame_data;
  GimpPixelRgn pixel_rgn;
  GimpDrawable *drawable;

  if(image_id < 0)
  {
    if(gap_debug)
    {
      printf("gap_pview_render_f_from_image: have no image, cant render image_id:%d\n"
            ,(int)image_id
            );
    }
    return;
  }
  p_free_desaturated_area_data(pv_ptr);

  if((gimp_image_width(image_id) != pv_ptr->pv_width)
  || ( gimp_image_height(image_id) != pv_ptr->pv_height))
  {
    gimp_image_scale(image_id, pv_ptr->pv_width, pv_ptr->pv_height);
  }

  /* workaround: gimp_image_merge_visible_layers
   * needs at least 2 layers to work without complaining
   * therefore add 2 full transparent dummy layers
   */
  {
    gint32  l_layer_id;
    GimpImageBaseType l_type;

    l_type   = gimp_image_base_type(image_id);
    l_type   = (l_type * 2); /* convert from GimpImageBaseType to GimpImageType */

    l_layer_id = gimp_layer_new(image_id, "dummy_01"
                                , 4, 4
                                , l_type
                                , 0.0       /* Opacity full transparent */     
                                , 0         /* NORMAL */
                                );   
    gimp_image_add_layer(image_id, l_layer_id, 0);

    l_layer_id = gimp_layer_new(image_id, "dummy_02"
                                , 4, 4
                                , l_type
                                , 0.0       /* Opacity full transparent */     
                                , 0         /* NORMAL */
                                );   
    gimp_image_add_layer(image_id, l_layer_id, 0);
    gimp_layer_resize_to_image_size(l_layer_id);
  }
  
    
  layer_id = gimp_image_merge_visible_layers (image_id, GIMP_CLIP_TO_IMAGE);
  drawable = gimp_drawable_get (layer_id);

  frame_data = g_malloc(drawable->width * drawable->height * drawable->bpp);
  
  gimp_pixel_rgn_init (&pixel_rgn, drawable, 0, 0
                      , drawable->width, drawable->height
                      , FALSE     /* dirty */
                      , FALSE     /* shadow */
                       );
  gimp_pixel_rgn_get_rect (&pixel_rgn, frame_data
                          , 0
                          , 0
                          , drawable->width
                          , drawable->height);
                          
 
 {
    gboolean frame_data_was_grabbed;

    frame_data_was_grabbed = gap_pview_render_f_from_buf (pv_ptr
                   , frame_data
                   , drawable->width
                   , drawable->height
                   , drawable->bpp
                   , TRUE            /* allow_grab_src_data */
                   , flip_request
                   , flip_status

                 );
  
    if(!frame_data_was_grabbed) g_free(frame_data);
    gimp_drawable_detach (drawable);
  }
  
}  /* end gap_pview_render_f_from_image */

/* ---------------------------------------
 * gap_pview_render_f_from_image_duplicate
 * ---------------------------------------
 * render preview widget from image.
 * This procedure creates a temorary copy of the image
 * for rendering of the preview.
 * NOTE: if the caller wants to render a temporary image
 *       that is not needed anymore after the rendering,
 *       the procedure gap_pview_render_f_from_image should be used
 *       to avoid duplicating of the image.
 *
 * hint: call gtk_widget_queue_draw(pv_ptr->da_widget);
 * after this procedure to make the changes appear on screen.
 */
void
gap_pview_render_f_from_image_duplicate (GapPView *pv_ptr
  , gint32 image_id
  , gint32 flip_request
  , gint32 flip_status
  )
{
  gint32 dup_image_id;
  
  dup_image_id = gimp_image_duplicate(image_id);
  gimp_image_undo_disable (dup_image_id); 
  gap_pview_render_f_from_image(pv_ptr
                 , dup_image_id
                 , flip_request
                 , flip_status
                 );
  gimp_image_delete(dup_image_id);
  
}  /* end gap_pview_render_f_from_image_duplicate */

/* ------------------------------
 * gap_pview_render_default_icon
 * ------------------------------
 */
void
gap_pview_render_default_icon(GapPView   *pv_ptr)
{
  GtkWidget *widget;
  int w, h;
  int x1, y1, x2, y2;
  GdkPoint poly[6];
  int foldh, foldw;
  int i;

  if(pv_ptr == NULL) { return; }
  if(pv_ptr->da_widget == NULL) { return; }
  if(pv_ptr->da_widget->window == NULL) { return; }

  widget = pv_ptr->da_widget;  /* the drawing area */

  /* set flag to let gap_pview_repaint procedure know
   * to use the pixmap rather than pv_area_data for refresh
   */
  pv_ptr->use_pixmap_repaint = TRUE;
  pv_ptr->use_pixbuf_repaint = FALSE;
  p_free_desaturated_area_data(pv_ptr);
  
  if(pv_ptr->pixmap)
  {
    gdk_drawable_get_size (pv_ptr->pixmap, &w, &h);
    if((w != pv_ptr->pv_width)
    || (h != pv_ptr->pv_height))
    {
       /* drop the old pixmap because of size missmatch */
       g_object_unref(pv_ptr->pixmap);
       pv_ptr->pixmap = NULL;
    }
  }

  w = pv_ptr->pv_width;
  h = pv_ptr->pv_height;

  if(pv_ptr->pixmap == NULL)
  {
       pv_ptr->pixmap = gdk_pixmap_new (widget->window
                          ,pv_ptr->pv_width
                          ,pv_ptr->pv_height
                          ,-1    /* use same depth as widget->window */
                          );

  }

  x1 = 2;
  y1 = h / 8 + 2;
  x2 = w - w / 8 - 2;
  y2 = h - 2;
  gdk_draw_rectangle (pv_ptr->pixmap, widget->style->bg_gc[GTK_STATE_NORMAL], 1,
                      0, 0, w, h);
/*
  gdk_draw_rectangle (pv_ptr->pixmap, widget->style->black_gc, 0,
                      x1, y1, (x2 - x1), (y2 - y1));
*/

  foldw = w / 4;
  foldh = h / 4;
  x1 = w / 8 + 2;
  y1 = 2;
  x2 = w - 2;
  y2 = h - h / 8 - 2;

  poly[0].x = x1 + foldw; poly[0].y = y1;
  poly[1].x = x1 + foldw; poly[1].y = y1 + foldh;
  poly[2].x = x1; poly[2].y = y1 + foldh;
  poly[3].x = x1; poly[3].y = y2;
  poly[4].x = x2; poly[4].y = y2;
  poly[5].x = x2; poly[5].y = y1;
  gdk_draw_polygon (pv_ptr->pixmap, widget->style->white_gc, 1, poly, 6);

  gdk_draw_line (pv_ptr->pixmap, widget->style->black_gc,
                 x1, y1 + foldh, x1, y2);
  gdk_draw_line (pv_ptr->pixmap, widget->style->black_gc,
                 x1, y2, x2, y2);
  gdk_draw_line (pv_ptr->pixmap, widget->style->black_gc,
                 x2, y2, x2, y1);
  gdk_draw_line (pv_ptr->pixmap, widget->style->black_gc,
                 x1 + foldw, y1, x2, y1);

  for (i = 0; i < foldw; i++)
  {
    gdk_draw_line (pv_ptr->pixmap, widget->style->black_gc,
                   x1 + i, y1 + foldh, x1 + i, (foldw == 1) ? y1 :
                   (y1 + (foldh - (foldh * i) / (foldw - 1))));
  }

  gdk_draw_drawable(widget->window
                   ,widget->style->black_gc
                   ,pv_ptr->pixmap
                   ,0
                   ,0
                   ,0
                   ,0
                   ,w
                   ,h
                   );

  
}  /* end gap_pview_render_default_icon */


#ifdef GAP_PVIEW_USE_GDK_PIXBUF_RENDERING

/* ------------------------------
 * gap_pview_render_f_from_pixbuf (slow)
 * ------------------------------
 * render drawing_area widget from src_pixbuf buffer
 * scaling and flattening against checkerboard background
 * is done implicite using GDK-pixbuf procedures
 *            
 * Thumbnails at size 128 rendered to Widget Size 256x256 pixels
 * at my Pentium IV 2600 MHZ
 * can be Played at Speed of  98 Frames/sec without dropping frames.
 *
 * The other Implementation without GDK-pixbuf procedures
 * is faster (at least on my machine), therefore GAP_PVIEW_USE_GDK_PIXBUF_RENDERING
 * is NOT defined per default.
 */
void
gap_pview_render_f_from_pixbuf (GapPView *pv_ptr
  , GdkPixbuf *src_pixbuf
  , gint32 flip_request
  , gint32 flip_status
  )
{
  static int l_checksize_tab[17] = { 2, 4, 8, 8, 16, 16, 16, 32, 32, 32, 32, 32, 32, 64, 64, 64, 64 };
  int l_check_size;

  /* printf("gap_pview_render_f_from_pixbuf --- USE GDK-PIXBUF procedures\n"); */
  
  if(pv_ptr == NULL) { return; }
  if(pv_ptr->da_widget == NULL) { return; }
  if(pv_ptr->da_widget->window == NULL)
  { 
    if(gap_debug)
    {
      printf("gap_pview_render_f_from_pixbuf: drawing_area window pointer is NULL, cant render\n");
    }
    return ;
  }

  if(src_pixbuf == NULL)
  {
    if(gap_debug)
    {
      printf("gap_pview_render_f_from_pixbuf: src_pixbuf is NULL, cant render\n");
    }
    return ;
  }

  /* clear flag to let gap_pview_repaint procedure know
   * to use the pixbuf rather than pv_area_data or pixmap for refresh
   */
  pv_ptr->use_pixmap_repaint = FALSE;
  pv_ptr->use_pixbuf_repaint = TRUE;
  p_free_desaturated_area_data(pv_ptr);

  /* l_check_size must be a power of 2 (using fixed size for 1.st test) */
  l_check_size = l_checksize_tab[MIN((pv_ptr->pv_check_size >> 2), 8)];
  if(pv_ptr->pixbuf)
  {
    /* free old (refresh) pixbuf if there is one */
    g_object_unref(pv_ptr->pixbuf);
  }
  
  /* scale and flatten the pixbuf */
  pv_ptr->pixbuf = gdk_pixbuf_composite_color_simple(
                         src_pixbuf
                      , (int) pv_ptr->pv_width
                      , (int) pv_ptr->pv_height
                      , GDK_INTERP_NEAREST
                      , 255                          /* overall_alpha */
                      , (int)l_check_size            /* power of 2 required */
                      , PREVIEW_BG_GRAY1_GDK
                      , PREVIEW_BG_GRAY2_GDK
                      );
  if(gap_debug)
  {
    int nchannels;
    int rowstride;
    int width;
    int height;
    guchar *pix_data;
    gboolean has_alpha;

    width = gdk_pixbuf_get_width(pv_ptr->pixbuf);
    height = gdk_pixbuf_get_height(pv_ptr->pixbuf);
    nchannels = gdk_pixbuf_get_n_channels(pv_ptr->pixbuf);
    pix_data = gdk_pixbuf_get_pixels(pv_ptr->pixbuf);
    has_alpha = gdk_pixbuf_get_has_alpha(pv_ptr->pixbuf);
    rowstride = gdk_pixbuf_get_rowstride(pv_ptr->pixbuf);

    printf("gap_pview_render_f_from_pixbuf (AFTER SCALE/FLATTEN):\n");
    printf(" l_check_size: %d (%d)\n", (int)l_check_size, pv_ptr->pv_check_size);
    printf(" width: %d\n", (int)width );
    printf(" height: %d\n", (int)height );
    printf(" nchannels: %d\n", (int)nchannels );
    printf(" pix_data: %d\n", (int)pix_data );
    printf(" has_alpha: %d\n", (int)has_alpha );
    printf(" rowstride: %d\n", (int)rowstride );
  }

  {
    gint32 l_flip_to_perform;
  
    l_flip_to_perform = p_calculate_flip_request(pv_ptr, flip_request, flip_status);
    if(l_flip_to_perform != GAP_STB_FLIP_NONE)
    {
      p_replace_pixbuf_by_flipped_pixbuf(pv_ptr, l_flip_to_perform);
    }
  }

  gap_pview_repaint(pv_ptr);

}       /* end gap_pview_render_f_from_pixbuf */

#else

/* ------------------------------
 * gap_pview_render_f_from_pixbuf (fast)
 * ------------------------------
 * render drawing_area widget from src_pixbuf buffer.
 * 
 * scaling and flattening against checkerboard background
 * is done by calling gap_pview_render_f_from_buf.
 *
 * The scaling in gap_pview_render_f_from_buf is optimized for speed
 * especially when both src and dest sizes are the same as in the
 * previous call.
 *
 * 
 * Thumbnails at size 128 rendered to Widget Size 256x256 pixels
 * at my Pentium IV 2600 MHZ
 * can be Played at Speed of  136 Frames/sec without dropping frames
 *
 */
void
gap_pview_render_f_from_pixbuf (GapPView *pv_ptr
  , GdkPixbuf *src_pixbuf
  , gint32 flip_request
  , gint32 flip_status
  )
{
  /* printf("gap_pview_render_f_from_pixbuf >>NO<< USE OF GDK-PIXBUF procedures\n"); */

  if(src_pixbuf == NULL)
  {
    if(gap_debug)
    {
      printf("gap_pview_render_f_from_pixbuf: src_pixbuf is NULL, cant render\n");
    }
    return ;
  }
  else
  {
    int nchannels;
    int width;
    int height;
    guchar *pix_data;

    width = gdk_pixbuf_get_width(src_pixbuf);
    height = gdk_pixbuf_get_height(src_pixbuf);
    nchannels = gdk_pixbuf_get_n_channels(src_pixbuf);
    pix_data = gdk_pixbuf_get_pixels(src_pixbuf);
     
    gap_pview_render_f_from_buf(pv_ptr
                             , pix_data
                             , width
                             , height
                             , nchannels
                             , FALSE                             /* DONT allow_grab_src_data */
                             , flip_request
                             , flip_status
                             );
  }
  

}       /* end gap_pview_render_f_from_pixbuf */

#endif



/* ------------------------------
 * p_pixmap_data_destructor
 * ------------------------------
 * a wrapper to g_free
 * just for printing debug output at testing
 */
static void
p_pixmap_data_destructor(gpointer data)
{
  if(gap_debug)
  {
    printf(" -- DESTRUCTOR p_pixmap_data_destructor called\n");
    printf(" -- data: %d\n", (int)data);
  }
  g_free(data);
  
}


/* ------------------------------
 * gap_pview_get_repaint_pixbuf
 * ------------------------------
 * return the repaint buffer as GdkPixbuf,
 * or NULL if no buffer is available.
 */
GdkPixbuf *
gap_pview_get_repaint_pixbuf(GapPView   *pv_ptr)
{ 
  int bits_per_sample;
  int rowstride;
  int width;
  int height;
  gboolean has_alpha;
  guchar *pixel_data_src;
  guchar *pixel_data_copy;
  
  
  if (pv_ptr == NULL)
  {
    return(NULL);
  }

  bits_per_sample = 8;
  rowstride = pv_ptr->pv_width * pv_ptr->pv_bpp;
  has_alpha = FALSE;
  width = pv_ptr->pv_width;
  height = pv_ptr->pv_height;
  pixel_data_src = NULL;
  
  if ((pv_ptr->use_pixbuf_repaint) 
  && (pv_ptr->pixbuf))
  {
    pixel_data_src = gdk_pixbuf_get_pixels(pv_ptr->pixbuf);
    rowstride = gdk_pixbuf_get_rowstride(pv_ptr->pixbuf);
    bits_per_sample = gdk_pixbuf_get_bits_per_sample(pv_ptr->pixbuf);
    has_alpha = gdk_pixbuf_get_has_alpha(pv_ptr->pixbuf);
  }
  
  if ((pv_ptr->use_pixmap_repaint) 
  && (pv_ptr->pixmap)
  && (pixel_data_src == NULL))
  { 
    gdk_drawable_get_size (pv_ptr->pixmap, &width, &height);
    // pixel_data_src = ???; 
    // TODO: how to make duplicate of pixeldata for GdkPixmap ?
    // not critical, the pixmap is currently used only for the default icon
    // that is renderd in case no useful pixeldata is available.
    // The caller has to deal with returned NULL pointer anyway.
 
    return(NULL);
  }

  if((pv_ptr->pv_area_data)
  && (pixel_data_src == NULL))
  {
    pixel_data_src = pv_ptr->pv_area_data;
  }

  if(pixel_data_src)
  {
    size_t pixel_data_size = pv_ptr->pv_height * pv_ptr->pv_width * pv_ptr->pv_bpp;
    pixel_data_copy = g_new ( guchar, pixel_data_size );
    memcpy(pixel_data_copy, pixel_data_src, pixel_data_size);                       

    return(gdk_pixbuf_new_from_data(
            pixel_data_copy
            , GDK_COLORSPACE_RGB
            , has_alpha
            , bits_per_sample
            , width
            , height
            , rowstride
            , (GdkPixbufDestroyNotify)p_pixmap_data_destructor  /* destroy_fn */
            , pixel_data_copy    /* gpointer destroy_fn_data */
        ));
  }
  return (NULL);
}  /* end gap_pview_get_repaint_pixbuf */


/* ------------------------------
 * gap_pview_get_repaint_thdata
 * ------------------------------
 * return the repaint buffer as thumbnail data (guchar RGB),
 * or NULL if no buffer is available.
 */
guchar *
gap_pview_get_repaint_thdata(GapPView   *pv_ptr        /* IN */
                            , gint32    *th_size       /* OUT */
                            , gint32    *th_width      /* OUT */
                            , gint32    *th_height     /* OUT */
                            , gint32    *th_bpp        /* OUT */
                            , gboolean  *th_has_alpha  /* OUT */
                            )
{ 
  int bits_per_sample;
  int rowstride;
  int width;
  int height;
  gboolean has_alpha;
  guchar *pixel_data_src;
  guchar *pixel_data_copy;
  
  
  if (pv_ptr == NULL)
  {
    return(NULL);
  }

  bits_per_sample = 8;
  rowstride = pv_ptr->pv_width * pv_ptr->pv_bpp;
  has_alpha = FALSE;
  width = pv_ptr->pv_width;
  height = pv_ptr->pv_height;
  pixel_data_src = NULL;
  
  if ((pv_ptr->use_pixbuf_repaint) 
  && (pv_ptr->pixbuf))
  {
    pixel_data_src = gdk_pixbuf_get_pixels(pv_ptr->pixbuf);
    rowstride = gdk_pixbuf_get_rowstride(pv_ptr->pixbuf);
    bits_per_sample = gdk_pixbuf_get_bits_per_sample(pv_ptr->pixbuf);
    has_alpha = gdk_pixbuf_get_has_alpha(pv_ptr->pixbuf);
  }
  
  if ((pv_ptr->use_pixmap_repaint) 
  && (pv_ptr->pixmap)
  && (pixel_data_src == NULL))
  { 
    gdk_drawable_get_size (pv_ptr->pixmap, &width, &height);
    // pixel_data_src = ???; 
    // TODO: how to make duplicate of pixeldata for GdkPixmap ?
    // not critical, the pixmap is currently used only for the default icon
    // that is renderd in case no useful pixeldata is available.
    // The caller has to deal with returned NULL pointer anyway.
 
    return(NULL);
  }

  if((pv_ptr->pv_area_data)
  && (pixel_data_src == NULL))
  {
    pixel_data_src = pv_ptr->pv_area_data;
  }

  if(pixel_data_src)
  {
    size_t pixel_data_size = pv_ptr->pv_height * pv_ptr->pv_width * pv_ptr->pv_bpp;
    pixel_data_copy = g_new ( guchar, pixel_data_size );
    memcpy(pixel_data_copy, pixel_data_src, pixel_data_size);                       

    *th_size       = width * height * pv_ptr->pv_bpp;
    *th_width      = width;
    *th_height     = height;
    *th_bpp        = pv_ptr->pv_bpp;
    *th_has_alpha  = has_alpha;

    return (pixel_data_copy);
  }
  return (NULL);

} /* end gap_pview_get_repaint_thdata */
