/* gap_fg_tile_manager.h
 * 2011.10.05 hof (Wolfgang Hofer)
 *
 * GAP ... Gimp Animation Plugins
 *
 * This Module contains:
 * - simplified tile manager emulation for porting the
 *   foreground selection via alpha matting.
 *
 * This module provides types and procedures for porting
 * calls of gimp core tile manager procedures that are used in the alpha matting implementation
 * (done by Jan Ruegg as gimp-tool in the unofficial repository
 *   https://github.com/rggjan/Gimp-Matting in the "new_layer" branch)
 *
 * Note that this module does NOT cover the full features of the tile manager in the GIMP core
 * it provides just a reduced subset that was required to compile and run the
 * alpha matting foreground selection code with only few trivial changes 
 * (prefixing the relevant names names with gapp_).
 *
 * tiles are emulated via pixel regions (that are accessable in plug-ins)
 * the real tile management is still done in the GIMP-core implicite when
 * drawable and pixel region calls to libgimp are performed.
 *
 * the simplified tileManager emulation is limited to handle only one tile at once.
 * e.g. requesting the next tile from the same tileManager implicite
 * releases the previous requested tile.
 * (that is sufficient to run the alpha matting)
 *
 * All emulated procedures are declared as static inline 
 * and already implemented in this header file for performance reasons.
 * gap_fg_tile_manager.c is not required for this reason.
 *
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

#ifndef GAP_FG_TILE_MANAGER_H
#define GAP_FG_TILE_MANAGER_H

#include "libgimp/gimp.h"


#define GAPP_TILE_WIDTH   64
#define GAPP_TILE_HEIGHT  64


typedef struct _GappMattingState GappMattingState;


/* simplified Tile emulation via Pixel Region (where one PixelRegion represents a tile of size 64x64) */
typedef struct GappTile {
  void /*GappTileManager*/  *tm;
  GimpPixelRgn               pixelRegion;
     
} GappTile;


typedef struct GappTileManager {
     GappTile      tile;           /* holds information about the (one) managed tile */
     gint32        drawableId;
     GimpDrawable *drawable;
     GimpPixelRgn *currentPR;
} GappTileManager;

#ifndef GAP_DEBUG_DECLARED
extern      int gap_debug; /* ==0  ... dont print debug infos */
#endif


static inline gint
gapp_tile_manager_width(const GappTileManager *tm)
{
  return (tm->drawable->width);
}

static inline gint
gapp_tile_manager_height(const GappTileManager *tm)
{
  return (tm->drawable->height);
}

static inline gint
gapp_tile_manager_bpp(const GappTileManager *tm)
{
  return (tm->drawable->bpp);
}


/* -----------------------------------
 * gapp_tile_ewidth
 * -----------------------------------
 * get effective width of the tile. (64 or smaller for tiles on the drawable borders)
 */
static inline gint
gapp_tile_ewidth (GappTile *tile)
{
  if (tile == NULL) 
  { 
    return (0);
  }
  return (tile->pixelRegion.w);
}

/* -----------------------------------
 * gapp_tile_eheight
 * -----------------------------------
 * get effective height of the tile. (64 or smaller for tiles on the drawable borders)
 */
static inline gint
gapp_tile_eheight (GappTile *tile)
{
  if (tile == NULL) 
  { 
    return (0);
  }
  return (tile->pixelRegion.h);
}

/* -----------------------------------
 * gapp_tile_eheight
 * -----------------------------------
 * get bytes per pixel for the tile. (1 upto 4)
 */
static inline gint
gapp_tile_bpp (GappTile *tile)
{
  if (tile == NULL) 
  { 
    return (0);
  }
  return (tile->pixelRegion.bpp);
}

/* -----------------------------------
 * gapp_tile_data_pointer
 * -----------------------------------
 * get data pointer to specified pixel in the tile.
 * coordinates are local to tile in unit pixels.
 * and must be 0 <= coord <= effective tile witdth or height
 */
static inline gpointer
gapp_tile_data_pointer (GappTile *tile,
                   gint  xoff,
                   gint  yoff)
{
  if (tile != NULL)
  {
    if (xoff < tile->pixelRegion.w && xoff >= 0
    &&  yoff < tile->pixelRegion.h && yoff >= 0)
    {
      gint offs;
      guchar *pointer;
      
      offs = (xoff * tile->pixelRegion.bpp) + (yoff * tile->pixelRegion.w * tile->pixelRegion.bpp);
      pointer = tile->pixelRegion.data + offs;
  
      return (pointer);
    }
  
  }
  return (NULL);
  
  
}  /* end gapp_tile_data_pointer */



/* -----------------------------------
 * gapp_tile_manager_get_at
 * -----------------------------------
 * get data pointer to tile at tile raster position tile_col / tile_row.
 * Note that the returned tile points to the one tile that is part
 * of the GappTilemanager structure and MUST NOT be free'd by the caller.
 * the caller shall call gapp_tile_release when processing of the tile is finished.
 *
 * RESTRICTION:
 * This simplified tile manager can only deal with one tile at a time.
 * (e.g. the next call implicite releases the tile and provides the newly requested one)
 * Therfore the caller MUST not use 2 or more tiles obtained from tha same
 * tile manager at the same time.
 *
 * TODO: how to emulate the wantread flag,
 * TODO: maybe operate on shadow tiles (requires merge_in_shadows before drawable detach)
 *
 */
static inline GappTile *
gapp_tile_manager_get_at (GappTileManager *tm,
                          gint         tile_col,
                          gint         tile_row,
                          gboolean     wantread,
                          gboolean     wantwrite)
{
  GappTile *tile;
  gint px;
  gint py;
  
  g_return_val_if_fail (tm != NULL, NULL);
  
  if(tm->drawable == NULL)
  {
    tm->drawable = gimp_drawable_get (tm->drawableId);
  }
  g_return_val_if_fail (tm->drawable != NULL, NULL);

  if((tile_col < 0) || (tile_row < 0))
  {
    return (NULL);
  }
  if((tile_col >= tm->drawable->width / GAPP_TILE_WIDTH ) 
  || (tile_row >= tm->drawable->height / GAPP_TILE_HEIGHT))
  {
    return (NULL);
  }

  
  while (tm->currentPR != NULL)
  {
    tm->currentPR = gimp_pixel_rgns_process (tm->currentPR);

    if (tm->currentPR != NULL)
    {
      printf("gapp_tile_manager_get_at ERROR: next region is NOT NULL region is larger than expected one tile size 64x64\n");
    }

  }

  tile = &tm->tile;
  tile->tm = tm;

  px = tile_col * GAPP_TILE_WIDTH;
  py = tile_row * GAPP_TILE_HEIGHT;

//  if(gap_debug)
//  {
//    printf("gapp_tile_manager_get_at: col:%d row:%d  px:%d py:%d drawable->width:%d height:%d\n"
//      , (int)tile_col
//      , (int)tile_row
//      , (int)px
//      , (int)py
//      , (int)tm->drawable->width
//      , (int)tm->drawable->height
//      );
//  }

  gimp_pixel_rgn_init (&tile->pixelRegion, tm->drawable, px, py
                      , GAPP_TILE_WIDTH, GAPP_TILE_HEIGHT
                      , wantwrite     /* dirty */
                      , FALSE         /* shadow */      
                       );
  tm->currentPR = gimp_pixel_rgns_register (1, &tile->pixelRegion);
  
  
  return (tile);

}  /* end gapp_tile_manager_get_at */


/* -----------------------------------
 * gapp_tile_release
 * -----------------------------------
 * release the tile via calling gimp_pixel_rgns_process.
 * this procedure also sets the pointer in the tile manager to NULL (tm->currentPR)
 * to indicate that there is no currently managed tile available.
 */
static inline void
gapp_tile_release (GappTile     *tile,
              gboolean  dirty)
{
  GappTileManager *tm;
  
  if (tile == NULL)
  {
    return;
  }
  
  tm = (GappTileManager*)tile->tm;
  g_return_if_fail (tm != NULL);


  if (dirty)
  {
//   TODO: how to force settin dirty state (the flag is not public , and not accessable here)
//    tm->currentPR.dirty = 1;
  }

  while (tm->currentPR != NULL)
  {
    tm->currentPR = gimp_pixel_rgns_process (tm->currentPR);

    if (tm->currentPR != NULL)
    {
      printf("gapp_tile_release ERROR: next region is NOT NULL region is larger than expected one tile size 64x64\n");
    }

  }

}  /* end gapp_tile_release */


/* -----------------------------------
 * gapp_tile_manager_new
 * -----------------------------------
 */
static inline GappTileManager * 
gapp_tile_manager_new(GimpDrawable *drawable)
{
  GappTileManager *tm;
  
  tm = g_new ( GappTileManager, 1 );
  tm->drawableId = drawable->drawable_id;
  tm->drawable = drawable;
  tm->currentPR = NULL;         /* NULL indicates invalid tile */
  tm->tile.tm = tm;
  
  return (tm);
}

#endif
