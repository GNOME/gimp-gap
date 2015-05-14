/*  gap_pview_da.h
 *
 *  This module handles GAP preview rendering from thumbnail data buffer
 *  used for video playback preview
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
 * version 1.3.26a; 2004/01/30  hof: added gap_pview_drop_repaint_buffers 
 * version 1.3.25a; 2004/01/21  hof: added gap_pview_render_from_pixbuf 
 * version 1.3.16a; 2003/06/27  hof: use aspect_frame instead of simple frame, added gap_pview_render_default_icon
 * version 1.3.14c; 2003/06/12  hof: created
 */

#ifndef _GAP_PVIEW_H
#define _GAP_PVIEW_H

#include <gtk/gtk.h>
#include <libgimp/gimp.h>
#include <libgimp/gimpui.h>

/* flip_request bits */
#define GAP_STB_FLIP_NONE      0
#define GAP_STB_FLIP_HOR       1
#define GAP_STB_FLIP_VER       2
#define GAP_STB_FLIP_BOTH      3


typedef struct GapPView
{
  GtkWidget *da_widget;       /* the preview drawing_area */
  GtkWidget *aspect_frame;
  gint   *src_col;            /* Pointer array[pv_width] used for width scaling */
  gint    src_width;          /* width of last handled src buffer */
  gint    src_bpp;            /* byte per pixel of last handled src buffer */
  gint    src_rowstride;
  gint    pv_width;           /* Preview Width in pixels */
  gint    pv_height;          /* Preview Height in pixels */
  gint    pv_bpp;             /* BPP of the preview currently always 3 */
  gint    pv_check_size;      /* size of the cheks in pixels */
  gint32  flip_status;
  gboolean use_pixbuf_repaint;
  gboolean use_pixmap_repaint;
  guchar *pv_area_data;       /* buffer to hold RGB image in preview size */
  GdkPixmap *pixmap;          /* alternative pixmap buffer */
  GdkPixbuf *pixbuf;          /* alternative pixbuf buffer */
  
  gboolean desaturate_request;        /* TRUE: render gray, FALSE render in color */
  guchar *pv_desaturated_area_data;   /* buffer to hold gray copy of image in preview size */
  
} GapPView;


void       gap_pview_render_f_from_pixbuf (GapPView *pv_ptr, GdkPixbuf *src_pixbuf
                 , gint32 flip_request
                 , gint32 flip_status
                 );
gboolean   gap_pview_render_f_from_buf (GapPView *pv_ptr
                 , guchar *src_data
                 , gint    src_width
                 , gint    src_height
                 , gint    src_bpp
                 , gboolean allow_grab_src_data
                 , gint32 flip_request
                 , gint32 flip_status
                 );
void       gap_pview_render_f_from_image_duplicate (GapPView *pv_ptr, gint32 image_id
                 , gint32 flip_request
                 , gint32 flip_status
                 );
void       gap_pview_render_f_from_image (GapPView *pv_ptr, gint32 image_id
                 , gint32 flip_request
                 , gint32 flip_status
                 );




void       gap_pview_render_from_pixbuf (GapPView *pv_ptr, GdkPixbuf *src_pixbuf);
gboolean   gap_pview_render_from_buf (GapPView *pv_ptr
                 , guchar *src_data
                 , gint    src_width
                 , gint    src_height
                 , gint    src_bpp
                 , gboolean allow_grab_src_data
                 );
void       gap_pview_render_from_image_duplicate (GapPView *pv_ptr, gint32 image_id);
void       gap_pview_render_from_image (GapPView *pv_ptr, gint32 image_id);
void       gap_pview_repaint(GapPView *pv_ptr);

GapPView   *gap_pview_new(gint pv_width, gint pv_height, gint pv_check_size, GtkWidget *frame);
void       gap_pview_set_size(GapPView *pv_ptr, gint pv_width, gint pv_height, gint pv_check_size);
void       gap_pview_reset(GapPView *pv_ptr);
void       gap_pview_drop_repaint_buffers(GapPView *pv_ptr);

void       gap_pview_render_default_icon(GapPView   *pv_ptr);
GdkPixbuf *gap_pview_get_repaint_pixbuf(GapPView   *pv_ptr);
guchar *   gap_pview_get_repaint_thdata(GapPView   *pv_ptr        /* IN */
                            , gint32    *th_size       /* OUT */
                            , gint32    *th_width      /* OUT */
                            , gint32    *th_height     /* OUT */
                            , gint32    *th_bpp        /* OUT */
                            , gboolean  *th_has_alpha  /* OUT */
                            );
#endif
