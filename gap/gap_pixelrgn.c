/* gap_pixelrgn.c
 *    extension for libgimp pixel region processing.
 *    by hof (Wolfgang Hofer)
 *  2011/12/07
 *
 * NOTE: This module deals with libgimp internal private structures
 * and therefore should become part of libgimp in future versions.
 * TODO: when future libgimp versions provide comparable functionality
 *       use this module as wrapper to call libgimp
 *
 */
/* LIBGIMP - The GIMP Library
 * Copyright (C) 1995-1997 Peter Mattis and Spencer Kimball
 *
 * gimppixelrgn.c
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include "libgimp/gimp.h"
#include "gap_pixelrgn.h"

typedef struct _GimpPixelRgnHolder    GimpPixelRgnHolder;
typedef struct _GimpPixelRgnIterator  GimpPixelRgnIterator;

struct _GimpPixelRgnHolder
{
  GimpPixelRgn *pr;
  guchar       *original_data;
  gint          startx;
  gint          starty;
  gint          count;
};

struct _GimpPixelRgnIterator
{
  GSList *pixel_regions;
  gint    region_width;
  gint    region_height;
  gint    portion_width;
  gint    portion_height;
  gint    process_count;
};

/**
 * gap_gimp_pixel_rgns_unref:
 * @pri_ptr: a regions iterator returned by #gimp_pixel_rgns_register,
 *           #gimp_pixel_rgns_register2 or #gimp_pixel_rgns_process.
 *
 * This function cleans up by unref tiles that were initialized in a
 * loop constuct with gimp_pixel_rgns_register and gimp_pixel_rgns_process calls.
 * It is intended for clean escape from such a loop at early time
 * e.g. before all tiles are processed.
 *
 * Note: leaving such a loop construct with break or return
 *        without clean up keeps tile references alive
 *       and leads to memory leaks.
 * Usage CODE example:
 *
 *  for (pr = gimp_pixel_rgns_register (2, &PR1, &PR2);
 *      pr != NULL;
 *      pr = gimp_pixel_rgns_process (pr))
 *  {
 *      ... evaluate pixel data in pixel regions PR1, PR2
 *      ...
 *      if (further evaluation of the remaining tiles is not necessary)
 *      {
 *        // escaping from the loop without the call
 *        // to gap_gimp_pixel_rgns_unref
 *        // leads to memory leaks because of unbalanced tile ref/unref calls.
 *        break;
 *      }
 *  }
 *  if (pr != NULL)
 *  {
 *    gap_gimp_pixel_rgns_unref(pr);
 *  }
 **/
void
gap_gimp_pixel_rgns_unref (gpointer pri_ptr)
{
  GimpPixelRgnIterator *pri;
  GSList               *list;

  g_return_val_if_fail (pri_ptr != NULL, NULL);

  pri = (GimpPixelRgnIterator*) pri_ptr;
  pri->process_count++;

  /*  Unref all referenced tiles and increment the offsets  */

  for (list = pri->pixel_regions; list; list = list->next)
    {
      GimpPixelRgnHolder *prh = list->data;

      if ((prh->pr != NULL) && (prh->pr->process_count != pri->process_count))
        {
          /*  This eliminates the possibility of incrementing the
           *  same region twice
           */
          prh->pr->process_count++;

          /*  Unref the last referenced tile if the underlying region
           *  is a tile manager
           */
          if (prh->pr->drawable)
            {
              GimpTile *tile = gimp_drawable_get_tile2 (prh->pr->drawable,
                                                        prh->pr->shadow,
                                                        prh->pr->x,
                                                        prh->pr->y);
              gimp_tile_unref (tile, prh->pr->dirty);
            }

          prh->pr->x += pri->portion_width;

          if ((prh->pr->x - prh->startx) >= pri->region_width)
            {
              prh->pr->x  = prh->startx;
              prh->pr->y += pri->portion_height;
            }
        }
    }

 

  /*  free the pixel regions list  */
  for (list = pri->pixel_regions; list; list = list->next)
    {
      g_slice_free (GimpPixelRgnHolder, list->data);
    }

  g_slist_free (pri->pixel_regions);
  g_slice_free (GimpPixelRgnIterator, pri);



}
