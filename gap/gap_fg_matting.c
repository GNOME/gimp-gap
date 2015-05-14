/* gap_fg_matting.h
 *  This module is a modified copy of the alpha matting
 *  forground extraction.
 *  The modifications allow the code to run as plug-in
 *  usable with official GIMP-releases (2.6.x and later)
 *  The original implemantation was done
 *   by Jan Ruegg as gimp core tool in the unofficial repository
 *   https://github.com/rggjan/Gimp-Matting in the "new_layer" branch.
 *
 * modifications: 
 *  - use emulation of tiles and tile manager calls (prefixed names with gapp_)
 *  - allow input layer that already has an alpha channel (allow bpp 3 or 4 not just 3)
 *
 *
 */
/* GIMP - The GNU Image Manipulation Program
 * Copyright (C) 1995 Spencer Kimball and Peter Mattis
 *
 * Matting
 * Copyright (C) 2011 Jan Ruegg <rggjan@gmail.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <glib-object.h>
#include <glib-2.0/glib/gprintf.h>

#include "libgimpmath/gimpmath.h"

//////////#include "core/core-types.h"
//////////#include "pixel-region.h"

#include "gap_fg_tile_manager.h"
#include "gap_fg_matting.h"

//#define IMAGE_DEBUG_PPM

// Stop after certain phase
#define DEBUG_PHASE 4


#define CAN_USE_ORIGINAL_COLORS
//#define HIGH_WEIGHT_FG_BG

#ifdef IMAGE_DEBUG_PPM
#include "stdio.h"
#include "stdlib.h"
#endif

// TODO move slider before painting doesn't work!

#define LAMBDA (10)
#define SEARCH_RADIUS (10)
#define MATTING_SQUARED_COLOR_DISTANCE (25)

// fspecial('gaussian', 13, sqrt((100/(9*pi))))
const gfloat gauss[13][13] =
{
  {1.7101814497842592e-06, 8.0985727252839541e-06, 2.8905528202266243e-05, 7.7760574635269639e-05, 1.5766821136856327e-04, 2.4095444850519678e-04, 2.7754402650812860e-04, 2.4095444850519678e-04, 1.5766821136856327e-04, 7.7760574635269639e-05, 2.8905528202266243e-05, 8.0985727252839541e-06, 1.7101814497842592e-06},
  {8.0985727252839541e-06, 3.8350831249506833e-05, 1.3688227195911312e-04, 3.6823558630170203e-04, 7.4663859580211824e-04, 1.1410409842453801e-03, 1.3143111120915069e-03, 1.1410409842453801e-03, 7.4663859580211824e-04, 3.6823558630170203e-04, 1.3688227195911312e-04, 3.8350831249506833e-05, 8.0985727252839541e-06},
  {2.8905528202266243e-05, 1.3688227195911312e-04, 4.8856193637079371e-04, 1.3143111120915069e-03, 2.6649119196619805e-03, 4.0726179129162605e-03, 4.6910558447545393e-03, 4.0726179129162605e-03, 2.6649119196619805e-03, 1.3143111120915069e-03, 4.8856193637079371e-04, 1.3688227195911312e-04, 2.8905528202266243e-05},
  {7.7760574635269639e-05, 3.6823558630170203e-04, 1.3143111120915069e-03, 3.5357107682171015e-03, 7.1690467226628403e-03, 1.0956004919274648e-02, 1.2619703593780167e-02, 1.0956004919274648e-02, 7.1690467226628403e-03, 3.5357107682171015e-03, 1.3143111120915069e-03, 3.6823558630170203e-04, 7.7760574635269639e-05},
  {1.5766821136856327e-04, 7.4663859580211824e-04, 2.6649119196619805e-03, 7.1690467226628403e-03, 1.4536039365470805e-02, 2.2214518185719723e-02, 2.5587852237016186e-02, 2.2214518185719723e-02, 1.4536039365470805e-02, 7.1690467226628403e-03, 2.6649119196619805e-03, 7.4663859580211824e-04, 1.5766821136856327e-04},
  {2.4095444850519678e-04, 1.1410409842453801e-03, 4.0726179129162605e-03, 1.0956004919274648e-02, 2.2214518185719723e-02, 3.3949056260531733e-02, 3.9104311330013700e-02, 3.3949056260531733e-02, 2.2214518185719723e-02, 1.0956004919274648e-02, 4.0726179129162605e-03, 1.1410409842453801e-03, 2.4095444850519678e-04},
  {2.7754402650812860e-04, 1.3143111120915069e-03, 4.6910558447545393e-03, 1.2619703593780167e-02, 2.5587852237016186e-02, 3.9104311330013700e-02, 4.5042405681608985e-02, 3.9104311330013700e-02, 2.5587852237016186e-02, 1.2619703593780167e-02, 4.6910558447545393e-03, 1.3143111120915069e-03, 2.7754402650812860e-04},
  {2.4095444850519678e-04, 1.1410409842453801e-03, 4.0726179129162605e-03, 1.0956004919274648e-02, 2.2214518185719723e-02, 3.3949056260531733e-02, 3.9104311330013700e-02, 3.3949056260531733e-02, 2.2214518185719723e-02, 1.0956004919274648e-02, 4.0726179129162605e-03, 1.1410409842453801e-03, 2.4095444850519678e-04},
  {1.5766821136856327e-04, 7.4663859580211824e-04, 2.6649119196619805e-03, 7.1690467226628403e-03, 1.4536039365470805e-02, 2.2214518185719723e-02, 2.5587852237016186e-02, 2.2214518185719723e-02, 1.4536039365470805e-02, 7.1690467226628403e-03, 2.6649119196619805e-03, 7.4663859580211824e-04, 1.5766821136856327e-04},
  {7.7760574635269639e-05, 3.6823558630170203e-04, 1.3143111120915069e-03, 3.5357107682171015e-03, 7.1690467226628403e-03, 1.0956004919274648e-02, 1.2619703593780167e-02, 1.0956004919274648e-02, 7.1690467226628403e-03, 3.5357107682171015e-03, 1.3143111120915069e-03, 3.6823558630170203e-04, 7.7760574635269639e-05},
  {2.8905528202266243e-05, 1.3688227195911312e-04, 4.8856193637079371e-04, 1.3143111120915069e-03, 2.6649119196619805e-03, 4.0726179129162605e-03, 4.6910558447545393e-03, 4.0726179129162605e-03, 2.6649119196619805e-03, 1.3143111120915069e-03, 4.8856193637079371e-04, 1.3688227195911312e-04, 2.8905528202266243e-05},
  {8.0985727252839541e-06, 3.8350831249506833e-05, 1.3688227195911312e-04, 3.6823558630170203e-04, 7.4663859580211824e-04, 1.1410409842453801e-03, 1.3143111120915069e-03, 1.1410409842453801e-03, 7.4663859580211824e-04, 3.6823558630170203e-04, 1.3688227195911312e-04, 3.8350831249506833e-05, 8.0985727252839541e-06},
  {1.7101814497842592e-06, 8.0985727252839541e-06, 2.8905528202266243e-05, 7.7760574635269639e-05, 1.5766821136856327e-04, 2.4095444850519678e-04, 2.7754402650812860e-04, 2.4095444850519678e-04, 1.5766821136856327e-04, 7.7760574635269639e-05, 2.8905528202266243e-05, 8.0985727252839541e-06, 1.7101814497842592e-06},
};

#define GAUSS(x, y) (gauss[x+6][y+6])

typedef union
{
  //guint32 value;
  struct
  {
    guint16 x; // TODO assert x not bigger than this!
    guint16 y;
  } coords;
} HashAddress;

struct HashEntry_;

struct HashEntry_
{
  guchar color[3];

  guchar foreground[3];
  guchar background[3];

  guchar foreground_refined[3];
  guchar background_refined[3];

  guchar alpha;
  guchar alpha_refined;

  gfloat sigma_f_squared;
  gfloat sigma_b_squared;

  gfloat confidence;

  gboolean valid;

  HashAddress this;
  struct HashEntry_ *next;
};

typedef struct HashEntry_ HashEntry;

// TODO should be 300*6/64, actually...
#define BIG_CACHE_CHANNELS 4
#define BIG_CACHE_RADIUS 6
#define BIG_CACHE_SIZE ((BIG_CACHE_RADIUS*2+1)*64)
#define GET_PIXEL(big_cache, x, y, color) (big_cache[BIG_CACHE_RADIUS*64+y][BIG_CACHE_RADIUS*64+x][color])
typedef guchar BigCache[BIG_CACHE_SIZE][BIG_CACHE_SIZE][BIG_CACHE_CHANNELS];

#define HASH_CACHE_EXTRA 14
#define HASH_CACHE_SIZE (64+HASH_CACHE_EXTRA*2)
#define GET_ENTRY(hash_cache, x, y) (hash_cache[HASH_CACHE_EXTRA+y][HASH_CACHE_EXTRA+x])
typedef HashEntry* HashCache[HASH_CACHE_SIZE][HASH_CACHE_SIZE];

/* A struct that holds the MATTING current state */
struct _GappMattingState
{
  GappTileManager *pixels;
  GappTileManager *result_layer;
  GappTileManager *mask;

  gboolean enough_pixels;

  BigCache big_cache;
  HashCache hash_cache;

  gint x1, y1, x2, y2;
  gint tx, ty;

  gint width, height;
};

/* Progressbar update callback */
static inline void
matting_progress_update (MattingProgressFunc  progress_callback,
                         gpointer             progress_data,
                         gdouble              value)
{
  if (progress_data)
    progress_callback (progress_data, value);
}

#ifdef IMAGE_DEBUG_PPM
static void
debug_image (const char* filename, int dimx, int dimy, guchar *data, int padding, int num_colors)
{
  int i, j;
  FILE *fp = fopen (filename, "wb"); /* b - binary mode */

  if (num_colors == 3)
    {
      fprintf (fp, "P6\n%d %d\n255\n", dimx, dimy);
    }
  else if (num_colors == 1)
    {
      fprintf (fp, "P5\n%d %d\n255\n", dimx, dimy);
    }
  else
    {
      printf ("Problem!\n");
      exit (1);
    }

  {
    guchar* current = data;

    for (j = 0; j < dimy; ++j)
      {
        for (i = 0; i < dimx; ++i)
          {
            if (num_colors == 3)
              {
                static unsigned char color[3];
                color[0] = current[0];
                color[1] = current[1];
                color[2] = current[2];
                (void) fwrite (color, 1, 3, fp);
              }
            else
              {
                static unsigned char color;
                color = current[0];
                (void) fwrite (&color, 1, 1, fp);
              }

            current += padding;
          }
      }
  }
  fclose (fp);
}

static void debug_cache (const char* filename, BigCache cache, int radius)
{
  static char filename_alpha[100];
  static char filename_rgb[100];
  const int size = (radius * 2 + 1) * 64;

  FILE *f_color, *f_alpha;

  snprintf (filename_alpha, 100, "%s_alpha.ppm", filename);
  snprintf (filename_rgb, 100, "%s.ppm", filename);

  f_color = fopen (filename_rgb, "wb"); /* b - binary mode */
  f_alpha = fopen (filename_alpha, "wb"); /* b - binary mode */

  fprintf (f_color, "P6\n%d %d\n255\n", size, size);
  fprintf (f_alpha, "P5\n%d %d\n255\n", size, size);
  {
    int x, y;
    for (y = -radius * 64; y < (radius + 1) * 64; y++)
      {
        for (x = -radius * 64; x < (radius + 1) * 64; x++)
          {
            fwrite (&GET_PIXEL(cache, x, y, 0), 1, 3, f_color);
            fwrite (&GET_PIXEL(cache, x, y, 3), 1, 1, f_alpha);
          }
      }
  }
  fclose (f_color);
  fclose (f_alpha);
}
#endif

GappMattingState *
matting_init (GappTileManager  *pixels,
              const guchar *colormap,
              gint          offset_x,
              gint          offset_y,
              gint          x,
              gint          y,
              gint          width,
              gint          height)
{
  GappMattingState *state;

  g_return_val_if_fail (pixels != NULL, NULL);
  state = g_slice_new (GappMattingState);

  state->pixels   = pixels;
  state->enough_pixels = FALSE;

  return state;
}

static void
load_big_cache (GappTileManager *source, GappTileManager *mask, BigCache big_cache,
                gint tx, gint ty, gint radius)
{
  gint    xdiff;
  gint    ydiff;
  gint    x, y;
  gint    bx, by;
  gint    width_tile, height_tile;

  GappTile   *src_tile = NULL;
  GappTile   *mask_tile = NULL;
  guchar *src_pointer = NULL;
  guchar *mask_pointer = NULL;

  guchar src_bpp = gapp_tile_manager_bpp(source);
  guchar mask_bpp = gapp_tile_manager_bpp(mask);

  g_return_if_fail ((gapp_tile_manager_bpp(source) == 4) ||
                    (gapp_tile_manager_bpp(source) == 3 && mask != NULL));

  for (ydiff = -radius; ydiff <= radius; ydiff++)
    {
      for (xdiff = -radius; xdiff <= radius; xdiff++)
        {
          src_tile = gapp_tile_manager_get_at (source, tx + xdiff, ty + ydiff, TRUE, FALSE);
          if (mask)
            mask_tile = gapp_tile_manager_get_at (mask, tx + xdiff, ty + ydiff, TRUE, FALSE);

          width_tile = 0;
          height_tile = 0;

          if (src_tile)
            {
              src_pointer = gapp_tile_data_pointer (src_tile, 0, 0);
              if (mask)
                {
                  g_return_if_fail(mask_tile);

                  mask_pointer = gapp_tile_data_pointer (mask_tile, 0, 0);
                }

              width_tile = gapp_tile_ewidth (src_tile);
              height_tile = gapp_tile_eheight (src_tile);

              for (y = 0; y < height_tile; y++)
                {
                  by = ydiff * 64 + y;
                  for (x = 0; x < width_tile; x++)
                    {
                      bx = xdiff * 64 + x;

                      GET_PIXEL(big_cache, bx, by, 0) = src_pointer[0];
                      GET_PIXEL(big_cache, bx, by, 1) = src_pointer[1];
                      GET_PIXEL(big_cache, bx, by, 2) = src_pointer[2];
                      if (mask)
                        GET_PIXEL(big_cache, bx, by, 3) = mask_pointer[mask_bpp-1];
                      else
                        GET_PIXEL(big_cache, bx, by, 3) = src_pointer[3];

                      if (mask)
                        {
                          src_pointer += src_bpp;
                          mask_pointer += mask_bpp;
                        }
                      else
                        {
                          src_pointer += src_bpp;
                        }
                    }
                }

              gapp_tile_release (src_tile, FALSE);
              if (mask)
                gapp_tile_release (mask_tile, FALSE);
            }

          for (y = 0; y < 64; y++)
            {
              by = ydiff * 64 + y;
              for (x = width_tile; x < 64; x++)
                {
                  bx = xdiff * 64 + x;

                  //GET_PIXEL(big_cache, bx, by, 0) = 0;
                  //GET_PIXEL(big_cache, bx, by, 1) = 0;
                  //GET_PIXEL(big_cache, bx, by, 2) = 0;
                  GET_PIXEL(big_cache, bx, by, 3) = 128;
                }
            }

          for (y = height_tile; y < 64; y++)
            {
              by = ydiff * 64 + y;
              for (x = 0; x < width_tile; x++)
                {
                  bx = xdiff * 64 + x;

                  //GET_PIXEL(big_cache, bx, by, 0) = 0;
                  //GET_PIXEL(big_cache, bx, by, 1) = 0;
                  //GET_PIXEL(big_cache, bx, by, 2) = 0;
                  GET_PIXEL(big_cache, bx, by, 3) = 128;
                }
            }
        }
    }
}

typedef struct
{
  guchar color[3];
  gboolean found;
  gint distance;
  gint x;
  gint y;
} SearchStructure;

// Project the Point P onto the line from B-A.
// alpha_pointer receives the calculated alpha value between 0 and 1
// where (0=B, 1=A)
// returns the squared distance of the best projected point to P
static inline gfloat projection (guchar B[3], guchar A[3], guchar P[3], float* alpha_pointer)
{
  gint ABx = B[0] - A[0];
  gint ABy = B[1] - A[1];
  gint ABz = B[2] - A[2];

  gint APx = P[0] - A[0];
  gint APy = P[1] - A[1];
  gint APz = P[2] - A[2];

  gint dot_AB = ABx * ABx + ABy * ABy + ABz * ABz;
  gfloat alpha = (float)(ABx * APx + ABy * APy + ABz * APz) / dot_AB;

  gfloat PPx, PPy, PPz;

  PPx = P[0] - (A[0] + alpha * ABx);
  PPy = P[1] - (A[1] + alpha * ABy);
  PPz = P[2] - (A[2] + alpha * ABz);

  alpha = alpha > 1 ? 1 : (alpha < 0 ? 0 : alpha);

  if (alpha_pointer)
    *alpha_pointer = alpha;

  // Normalize, so that it is in the unit cube of colors
  return (PPx * PPx + PPy * PPy + PPz * PPz) / (255. * 255.);
}

// evaluate the energy function for found color fg/bg for pixel situated at x/y
static inline float
objective_function (SearchStructure *fg,
                    SearchStructure *bg,
                    gint x, gint xmin, gint xmax,
                    gint y, gint ymin, gint ymax,
                    float* best_alpha,
                    float pfp,
                    GappMattingState *state)
{
  gint xi, yi;
  float ap;
  float finalAlpha;
  float Np = projection (fg->color, bg->color,
                         &GET_PIXEL (state->big_cache, x, y, 0),
                         &finalAlpha);

  for (yi = ymin; yi <= ymax; yi++)
    {
      for (xi = xmin; xi <= ymax; xi++)
        {
          guchar *P = &(GET_PIXEL (state->big_cache, x + xi, y + yi, 0));

          if (xi != 0 || yi != 0)
            {
              Np += projection (fg->color, bg->color, P, NULL);
            }
        }
    }

  ap = pfp + (1 - 2 * pfp) * finalAlpha;

  *best_alpha = finalAlpha;
  return pow(Np, 3) * pow(bg->distance, 4) * pow(fg->distance, 1) * pow(ap, 2);
}

// searches for known regions for a given unknown pixel in a hash table

static inline gfloat dist_squared(guchar x1, guchar y1, guchar z1, guchar x2, guchar y2, guchar z2)
{
  gint diff_x = (gint)x1 - (gint)x2;
  gint diff_y = (gint)y1 - (gint)y2;
  gint diff_z = (gint)z1 - (gint)z2;

  return (gfloat) (diff_x * diff_x + diff_y * diff_y + diff_z * diff_z);
}

static gfloat calculate_variance (guchar P[3], gint x, gint y, GappMattingState *state)
{
  gint yi;
  gint xi;
  guchar my_alpha = GET_PIXEL (state->big_cache, x, y, 3);

  gfloat sum = 0;
  gint num_found = 0;

  for (yi = -2; yi <= 2; yi++)
    {
      for (xi = -2; xi <= 2; xi++)
        {
          if (GET_PIXEL (state->big_cache, x + xi, y + yi, 3) == my_alpha &&
              state->tx * 64 + x + xi < state->width  && state->tx * 64 + x >= 0 &&
              state->ty * 64 + y + yi < state->height && state->ty * 64 + y >= 0)
            {
              sum += dist_squared(GET_PIXEL (state->big_cache, x + xi, y + yi, 0),
                                  GET_PIXEL (state->big_cache, x + xi, y + yi, 1),
                                  GET_PIXEL (state->big_cache, x + xi, y + yi, 2),
                                  P[0],
                                  P[1],
                                  P[2]);

              num_found++;
            }
        }
    }

  if (num_found == 0)
    {
      g_return_val_if_reached(0);
    }
  else
    {
      return sum / num_found;
    }
}

typedef struct
{
  gint x;
  gint y;
  gfloat diff;
} TopColor;

static void inline load_hash_cache (GHashTable* unknown_hash, HashCache hash_cache,
                                    gint tx, gint ty, gint bordersize)
{

  gint x, y;
  gint xoff = tx * 64;
  gint yoff = ty * 64;

  for (y = -bordersize; y < bordersize + 64; y++)
    {
      for (x = -bordersize; x < bordersize + 64; x++)
        {
          HashAddress address;
          address.coords.x = x + xoff;
          address.coords.y = y + yoff;

          GET_ENTRY(hash_cache, x, y) = g_hash_table_lookup (unknown_hash, &address);
        }
    }
}

static void inline
compare_neighborhood (HashEntry* entry, GHashTable* unknown_hash,
                      GappMattingState *state)
{
  gint pos_x, pos_y;
  gint tx, ty;

  gint xdiff, ydiff;
  gint num;
  gint matches = 0;

  HashEntry *current;
  TopColor top3[3];

  // Load coordinates from entry
  pos_x = entry->this.coords.x;
  pos_y = entry->this.coords.y;

  tx = pos_x / 64;
  ty = pos_y / 64;

  pos_x = pos_x - 64 * tx;
  pos_y = pos_y - 64 * ty;

  if (state->tx != tx || state->ty != ty)
    {
      load_hash_cache (unknown_hash, state->hash_cache, tx, ty, HASH_CACHE_EXTRA);

      state->tx = tx;
      state->ty = ty;
    }

  for (num = 0; num < 3; num++)
    {
      top3[num].diff = INFINITY;
    }

  // TODO: change this to radius! (not square)
  for (ydiff = -HASH_CACHE_EXTRA; ydiff <= HASH_CACHE_EXTRA; ydiff++)
    {
      for (xdiff = -HASH_CACHE_EXTRA; xdiff <= HASH_CACHE_EXTRA; xdiff++)
        {
          current = GET_ENTRY(state->hash_cache, pos_x + xdiff, pos_y + ydiff);

          if (current && current->valid)
            {
              gfloat temp = projection (current->foreground,
                                        current->background,
                                        entry->color,
                                        NULL);

              // check if color is better than least best of colors, add the color and sort the list
              if (temp < top3[2].diff)
                {
                  gint i = 2;
                  for (num = 1; num >= 0; num--)
                    {
                      if (temp < top3[num].diff)
                        {
                          i = num;
                        }
                    }
                  for (num = 2; num > i; num--)
                    {
                      top3[num] = top3[num-1];
                    }
                  top3[i].diff = temp;
                  top3[i].x = pos_x + xdiff;
                  top3[i].y = pos_y + ydiff;
                  matches++;
                }
            }
        }
    }

  if (matches >= 1)
    {
      gfloat colordiff, current_alpha;

      guchar new_fg[3] = {0, 0, 0};
      guchar new_bg[3] = {0, 0, 0};
      gfloat new_sigma_f_squared = 0;
      gfloat new_sigma_b_squared = 0;

      gint index;

      matches = matches > 3 ? 3 : matches;

      for (num = 0; num < matches; num++)
        {
          current = GET_ENTRY(state->hash_cache, top3[num].x, top3[num].y);
          for (index = 0; index < 3; index++)
            {
              // TODO: check if we should use ints here!
              new_fg[index] += floor(current->foreground[index] / matches);
              new_bg[index] += floor(current->background[index] / matches);
            }

          new_sigma_f_squared += current->sigma_f_squared / matches;
          new_sigma_b_squared += current->sigma_b_squared / matches;
        }

      colordiff = dist_squared(entry->color[0],
                               entry->color[1],
                               entry->color[2],
                               new_fg[0],
                               new_fg[1],
                               new_fg[2]);

      if (colordiff > new_sigma_f_squared)
        {
          for (index = 0; index < 3; index++)
            {
              entry->foreground_refined[index] = new_fg[index];
            }
        }
      else
        {
          for (index = 0; index < 3; index++)
            {
#ifdef CAN_USE_ORIGINAL_COLORS
              entry->foreground_refined[index] = entry->color[index];
#else
              entry->foreground_refined[index] = new_fg[index];
#endif
            }
        }

      colordiff = dist_squared(entry->color[0],
                               entry->color[1],
                               entry->color[2],
                               new_bg[0],
                               new_bg[1],
                               new_bg[2]);

      if (colordiff > new_sigma_b_squared)
        {
          for (index = 0; index < 3; index++)
            {
              entry->background_refined[index] = new_bg[index];
            }
        }
      else
        {
          for (index = 0; index < 3; index++)
            {
#ifdef CAN_USE_ORIGINAL_COLORS
              entry->background_refined[index] = entry->color[index];
#else
              entry->background_refined[index] = new_bg[index];
#endif
            }
        }

      {
        gfloat mp;
        gint i;
        gboolean same = TRUE;

        mp = projection (new_fg,
                         new_bg,
                         entry->color,
                         &current_alpha);

        entry->alpha_refined = current_alpha * 255;

        for (i = 0; i < 3; i++)
          {
            if (entry->foreground_refined[i] != entry->background_refined[i])
              {
                same = FALSE;
                break;
              }
          }

        if (same)
          {
            entry->confidence = 1e-8;
          }
        else
          {
            mp = projection (entry->foreground_refined,
                             entry->background_refined,
                             entry->color,
                             NULL);

            entry->confidence = exp(-LAMBDA * sqrt(mp));
          }
      }
      entry->valid = TRUE;
    }
}

static void inline
calculate_final_colors (gfloat gauss, gint x, gint y, gfloat alpha_orig,
                        gdouble fq[3], gdouble *fd, gdouble bq[3], gdouble *bd,
                        gdouble *meandiff_q, gdouble *meandiff_d,
                        gdouble *low_freq_alpha_q, gdouble *low_freq_alpha_d,
                        gboolean is_middle, GappMattingState *state)
{
  guchar *fg, *bg;
  gint i;
  gfloat alpha, confidence;
  gdouble weight;
  HashEntry* current;

  current = GET_ENTRY(state->hash_cache, x, y);
  if(current == NULL || !current->valid)
    {
      alpha = GET_PIXEL (state->big_cache, x, y, 3);
      if (alpha != 255 && alpha != 0) // We are outside of the image!
        confidence = 0;
      else
        confidence = 1;

      if (current)
        alpha = 0.5;
      else
        alpha = (alpha == 255 ? 1 : 0);

      fg = &GET_PIXEL (state->big_cache, x, y, 0);
      bg = fg;
    }
  else
    {
      gfloat diff_weight;

      g_return_if_fail (GET_PIXEL (state->big_cache, x, y, 3) == 128);

      fg = current->foreground_refined;
      bg = current->background_refined;

      alpha = current->alpha_refined / 255.;
      confidence = current->confidence;

      diff_weight = alpha * (1 - alpha) * confidence;
      *meandiff_d += diff_weight;
      *meandiff_q += diff_weight * sqrt(dist_squared(
                                          current->foreground_refined[0],
                                          current->foreground_refined[1],
                                          current->foreground_refined[2],
                                          current->background_refined[0],
                                          current->background_refined[1],
                                          current->background_refined[2]));
    }

  // Special Case for original pixel
  if (is_middle)
    {
      weight = gauss * confidence;
    }
  else
    {
      gfloat diff = alpha_orig - alpha;
      if (diff < 0)
        diff = -diff;

      weight = gauss * confidence * diff;
    }

  for (i = 0; i < 3; i++)
    {

      fq[i] += weight * alpha * fg[i];
      bq[i] += weight * (1 - alpha) * bg[i];
    }
  {
    gdouble low_freq_weight = confidence * gauss;
#ifdef HIGH_WEIGHT_FG_BG
    low_freq_weight += (current == NULL && confidence == 1);
#endif

    *bd += weight * (1 - alpha);
    *fd += weight * alpha;

    *low_freq_alpha_q += low_freq_weight * alpha;
    *low_freq_alpha_d += low_freq_weight;
  }
}

static void inline
local_smoothing (HashEntry* entry, GHashTable* unknown_hash, GappMattingState *state)
{
  gint pos_x, pos_y;
  gint tx, ty;
  gint i;

  gint xdiff, ydiff;
  gdouble fq[3] = {0, 0, 0};
  gdouble fd = 0;
  gdouble bq[3] = {0, 0, 0};
  gdouble bd = 0;

  gdouble meandiff_q = 0;
  gdouble meandiff_d = 0;

  gdouble low_freq_alpha_q = 0;
  gdouble low_freq_alpha_d = 0;

  // Load coordinates from entry
  pos_x = entry->this.coords.x;
  pos_y = entry->this.coords.y;

  tx = pos_x / 64;
  ty = pos_y / 64;

  pos_x = pos_x - 64 * tx;
  pos_y = pos_y - 64 * ty;

  if (state->tx != tx || state->ty != ty)
    {
      load_hash_cache (unknown_hash, state->hash_cache, tx, ty, 6);
      load_big_cache (state->pixels, state->result_layer, state->big_cache, tx, ty, 1);

      state->tx = tx;
      state->ty = ty;
    }

  for (ydiff = -6; ydiff <= 6; ydiff++)
    {
      for (xdiff = -6; xdiff <= 6; xdiff++)
        {
          calculate_final_colors (GAUSS(xdiff, ydiff),
                                  pos_x + xdiff, pos_y + ydiff,
                                  entry->alpha_refined / 255.,
                                  fq, &fd, bq, &bd,
                                  &meandiff_q, &meandiff_d,
                                  &low_freq_alpha_q, &low_freq_alpha_d,
                                  xdiff == 0 && ydiff == 0,
                                  state);
        }
    }

  for (i = 0; i < 3; i++)
    {
      if (fd != 0)
        entry->foreground[i] = fq[i] / fd;
      if (bd != 0)
        entry->background[i] = bq[i] / bd;
    }

  {
    gfloat current_alpha, mp;
    gdouble low_freq_alpha = low_freq_alpha_q / low_freq_alpha_d;
    gdouble meandiff;

    gdouble final_confidence = sqrt(dist_squared(
                                      entry->foreground[0],
                                      entry->foreground[1],
                                      entry->foreground[2],
                                      entry->background[0],
                                      entry->background[1],
                                      entry->background[2]));

    if (meandiff_d == 0)
      meandiff_d = 1;

    meandiff = meandiff_q / meandiff_d;
    final_confidence /= meandiff;

    if (final_confidence > 1)
      final_confidence = 1;

    mp = projection (entry->foreground,
                     entry->background,
                     entry->color,
                     &current_alpha);
    final_confidence *= exp(-LAMBDA * sqrt(mp));

    if (!(final_confidence <= 1 && final_confidence >= 0))
      g_printf("Problem!: final_confidence: %f\n", final_confidence);

    entry->alpha = (final_confidence * current_alpha + (1 - final_confidence) * low_freq_alpha) * 255;
    //entry->alpha = low_freq_alpha * 255;
  }
}

static void inline
search_neighborhood (HashEntry* entry, GappMattingState *state)
{
  gint pos_x, pos_y;
  gint orig_pos_x, orig_pos_y;
  gint tx, ty;
  gint xdiff, ydiff;
  gfloat min_gradients[2] = {INFINITY, INFINITY};
  gfloat gradients[4] = {0, 0, 0, 0};

  gint direction, toggle, distance;

  SearchStructure found[2][4];

  guchar prevval[4][3] =
  {
    {0}
  };
  double angle;
  gint permutation[4];

  for (toggle = 0; toggle < 2; toggle++)
    {
      for (direction = 0; direction < 4; direction++)
        {
          found[toggle][direction].found = FALSE;
        }
    }

  // Load coordinates from entry
  orig_pos_x = entry->this.coords.x;
  orig_pos_y = entry->this.coords.y;

  tx = orig_pos_x / 64;
  ty = orig_pos_y / 64;

  pos_x = orig_pos_x - 64 * tx;
  pos_y = orig_pos_y - 64 * ty;

  if (state->tx != tx || state->ty != ty)
    {
      load_big_cache (state->pixels, state->result_layer, state->big_cache, tx, ty, 6);

#ifdef IMAGE_DEBUG_PPM
      {
        static char buffer[100];

        snprintf (buffer, 100, "pixels_result_%i_%i", tx, ty);
        debug_cache (buffer, state->big_cache, 3);
      }
#endif

      state->tx = tx;
      state->ty = ty;
    }

  // in a 9x9 window, we want to have values in a 90° window
  angle = ((orig_pos_x % 3) + (orig_pos_y % 3) * 3) * 2.*G_PI / 9. / 4.;

  for (distance = 6; distance < 6 * 64; distance += 6)
    {
      // TODO precalculate these!
      xdiff = cos (angle) * distance;
      ydiff = sin (angle) * distance;

      permutation[0] = xdiff;
      permutation[1] = ydiff;
      permutation[2] = -xdiff;
      permutation[3] = -ydiff;

      for (direction = 0; direction < 4; direction++)
        {
          if (found[0][direction].found && found[1][direction].found)
            {
              continue;
            }
          else
            {
              guchar r, g, b, a;

              gint xtmp = permutation[direction] + pos_x;
              gint ytmp = permutation[(direction + 1) % 4] + pos_y;

              r = GET_PIXEL (state->big_cache, xtmp, ytmp, 0);
              g = GET_PIXEL (state->big_cache, xtmp, ytmp, 1);
              b = GET_PIXEL (state->big_cache, xtmp, ytmp, 2);
              a = GET_PIXEL (state->big_cache, xtmp, ytmp, 3);

              gradients[direction] += sqrt(dist_squared(prevval[direction][0],
                                                        prevval[direction][1],
                                                        prevval[direction][2],
                                                        r, g, b));

              prevval[direction][0] = r;
              prevval[direction][1] = g;
              prevval[direction][2] = b;

              // check if it's foreground or background, depending on alpha
              // toggle = 0 equals foreground
              for (toggle = 0; toggle < 2; toggle++)
                {
                  if (a == (toggle == 0 ? 255 : 0) &&
                      !found[toggle][direction].found)
                    {
                      if (gradients[direction] < min_gradients[toggle])
                        min_gradients[toggle] = gradients[direction];

                      found[toggle][direction].color[0] = r;
                      found[toggle][direction].color[1] = g;
                      found[toggle][direction].color[2] = b;
                      found[toggle][direction].distance = distance;
                      found[toggle][direction].found = TRUE;
                      found[toggle][direction].x = xtmp;
                      found[toggle][direction].y = ytmp;
                    }
                }
            }
        }
    }

  {
    gfloat min = INFINITY;
    gint minindexf = -1;
    gint minindexb = -1;

    gint foreground_direction, background_direction;

    gfloat best_alpha = -1;
    gfloat pfp = 0.5;
    if (min_gradients[1] > 0 || min_gradients[0] > 0)
      pfp = min_gradients[1] / (min_gradients[0] + min_gradients[1]);

    // calculate energy function for every fg/bg pair
    for (foreground_direction = 0; foreground_direction < 4; foreground_direction++)
      {
        if (found[0][foreground_direction].found)
          {
            for (background_direction = 0; background_direction < 4; background_direction++)
              {
                if (found[1][background_direction].found)
                  {
                    float current_alpha, cost;
                    gint xmin = -1, ymin = -1, xmax = 1, ymax = 1;

                    if (orig_pos_x <= 0)
                      xmin++;
                    else if (orig_pos_x >= state->width - 1)
                      xmax--;
                    if (orig_pos_y <= 0)
                      ymin++;
                    else if (orig_pos_y >= state->height - 1)
                      ymax++;

                    cost = objective_function (&(found[0][foreground_direction]),
                                               &(found[1][background_direction]),
                                               pos_x, xmin, xmax,
                                               pos_y, ymin, ymax,
                                               &current_alpha,
                                               pfp,
                                               state);
                    if (cost < min)
                      {
                        best_alpha = current_alpha;
                        min = cost;
                        minindexf = foreground_direction;
                        minindexb = background_direction;
                      }
                  }

              }
          }
      }

    if (min != INFINITY)
      {
        SearchStructure best_foreground = found[0][minindexf];
        SearchStructure best_background = found[1][minindexb];

        // test: do combination of best match
        // should return a very similar image as before
        entry->foreground[0] = best_foreground.color[0];
        entry->foreground[1] = best_foreground.color[1];
        entry->foreground[2] = best_foreground.color[2];

        entry->background[0] = best_background.color[0];
        entry->background[1] = best_background.color[1];
        entry->background[2] = best_background.color[2];

        entry->alpha = best_alpha * 255;
        entry->valid = TRUE;

        entry->sigma_b_squared = calculate_variance (best_background.color, best_background.x, best_background.y, state);
        entry->sigma_f_squared = calculate_variance (best_foreground.color, best_foreground.x, best_foreground.y, state);
      }

    //printf("values: %i %i %i | %i %i %i | %i %i %i | %i %i %i\n", values[0], values[1], values[2], values[3], values[4], values[5], values[6], values[7], values[8], values[9], values[10], values[11]);
  }
}

// static gfloat
// mask_percent_unknown (GappTileManager* mask_layer,
//                       GappMattingState* state)
// {
//   PixelRegion mask;
//   PixelRegionIterator *pr;
//   gint row, col;
//   gint pixels_unknown = 0;
//   guint width = state->x2 - state->x1;
//   guint height = state->y2 - state->y1;
//   guint pixels_total = width * height;
// 
//   pixel_region_init (&mask, mask_layer, state->x1, state->y1, width, height, TRUE);
// 
//   if (mask.bytes != 1)
//     return 1;
// 
//   for (pr = pixel_regions_register (1, &mask);
//        pr != NULL;
//        pr = pixel_regions_process (pr))
//     {
//       guchar *mask_data = mask.data;
// 
//       for (row = 0; row < mask.h; row++)
//         {
//           guchar *m = mask_data;
// 
//           for (col = 0; col < mask.w; col++, ++m)
//             {
//               if (m[0] != MATTING_USER_FOREGROUND && m[0] != MATTING_USER_BACKGROUND)
//                 {
//                   pixels_unknown++;
//                 }
//             }
// 
//           mask_data += mask.rowstride;
//         }
//     }
//   return (gfloat) pixels_unknown / pixels_total;
// }


// static void
// update_mask (GappTileManager* result_layer,
//              GappTileManager* mask_layer,
//              GappMattingState *state)
// {
//   PixelRegion result, mask;
//   PixelRegionIterator *pr;
//   gint row, col;
//   gint width, height;
// 
//   width = state->x2 - state->x1;
//   height = state->y2 - state->y1; // TODO give this as argument
// 
//   pixel_region_init (&result, result_layer, state->x1, state->y1, width, height, FALSE);
//   pixel_region_init (&mask, mask_layer, state->x1, state->y1, width, height, TRUE);
// 
//   g_return_if_fail (result.bytes == 4 && mask.bytes == 1); // TODO check if indexed etc...
// 
//   for (pr = pixel_regions_register (2, &result, &mask);
//        pr != NULL;
//        pr = pixel_regions_process (pr))
//     {
//       const guchar *result_data = result.data;
//       guchar *mask_data = mask.data;
// 
//       for (row = 0; row < result.h; row++)
//         {
//           const guchar *r = result_data;
//           guchar *m = mask_data;
// 
//           for (col = 0; col < result.w; col++, r += result.bytes, ++m) // TODO check if 4 is ok
//             {
//               guchar mask;
//               mask = m[0];
//               if (mask != MATTING_USER_FOREGROUND &&
//                   mask != MATTING_USER_BACKGROUND)
//                 {
//                   guchar result = r[3];
//                   if (result == MATTING_USER_FOREGROUND)
//                     result--;
//                   if (result == MATTING_USER_BACKGROUND)
//                     result++;
//                   m[0] = result;
//                 }
//             }
// 
//           result_data += result.rowstride;
//           mask_data += mask.rowstride;
//         }
//     }
// }

static inline gboolean
check_closeness (guchar color[3], BigCache big_cache, gint x, gint y, guchar* result)
{
  guchar value;

  value = GET_PIXEL (big_cache, x, y, 3);

  if (value == MATTING_USER_FOREGROUND || value == MATTING_USER_BACKGROUND)
    {
      gint   color_distance_sum;
      gint   color_distance;
      gint   i;
      color_distance_sum = 0;
      for (i = 0; i < 3; i++)
        {
          color_distance = (gint)color[i] - GET_PIXEL (big_cache, x, y, i);
          color_distance_sum += color_distance * color_distance;
        }

      if (color_distance_sum < MATTING_SQUARED_COLOR_DISTANCE)
        {
          if (value == MATTING_USER_FOREGROUND)
            *result = 255;
          else
            *result = 0;
          return TRUE;
        }
    }

  return FALSE;
}

static inline guchar
search_for_neighbours (gint x, gint y, guchar* color, GappMattingState *state)
{
  gint   radius, n;
  guchar alpha;

  alpha = GET_PIXEL (state->big_cache, x, y, 3);

  if (alpha == MATTING_USER_BACKGROUND)
    {
      return 0;
    }

  if (alpha == MATTING_USER_FOREGROUND)
    {
      return 255;
    }

  for (radius = 0; radius <= SEARCH_RADIUS; radius++)
    {
      for (n = -radius; n < radius; n++)
        {
          if (check_closeness (color, state->big_cache, x + radius, y + n, &alpha))
            return alpha;

          if (check_closeness (color, state->big_cache, x - radius, y + n, &alpha))
            return alpha;

          if (check_closeness (color, state->big_cache, x + n, y + radius, &alpha))
            return alpha;

          if (check_closeness (color, state->big_cache, x + n, y - radius, &alpha))
            return alpha;
        }
    }
  return 128;
}

void
matting_foreground_extract (GappMattingState       *state,
                            GappTileManager        *mask,
                            gint                x1,
                            gint                y1,
                            gint                x2,
                            gint                y2,
                            gfloat              start_percentage,
                            MattingProgressFunc progress_callback,
                            gpointer            progress_data,
                            GappTileManager        *result_layer)
{
  GappTile        *tile;

  gint         tx, ty, x, y;
  guchar      *pointer;

  gint         hash_size, hash_step;

  HashEntry   *first_entry = NULL;
  HashEntry   *previous_entry = NULL;

  static       GHashTable *unknown_hash = NULL;

  state->width = gapp_tile_manager_width (mask);
  state->height = gapp_tile_manager_height (mask);

  g_return_if_fail (state != NULL);
  g_return_if_fail (mask != NULL && gapp_tile_manager_bpp (mask) == 1);
  g_return_if_fail (x1 >= 0);
  g_return_if_fail (x2 > x1 && x2 <= state->width);
  g_return_if_fail (y1 >= 0);
  g_return_if_fail (y2 > y1 && y2 <= state->height);
  g_return_if_fail (start_percentage >= 0 && start_percentage <= 1);
  g_return_if_fail (progress_data == NULL || progress_callback != NULL);

  g_return_if_fail (gapp_tile_manager_bpp (state->pixels) == 3 || gapp_tile_manager_bpp (state->pixels) == 4);
  g_return_if_fail (gapp_tile_manager_bpp (result_layer) == 4);

  state->x1 = x1;
  state->y1 = y1;
  state->x2 = x2;
  state->y2 = y2;

  state->result_layer = result_layer;
  state->mask = mask;

//   if (!state->enough_pixels)
//     {
//       gfloat unknown_percent = mask_percent_unknown (mask, state);
// 
//       //g_printf("Unknown pixels: %f (%f)\n", unknown_percent, start_percentage);
//       if (unknown_percent > (1 - start_percentage))
//         {
//           return;
//         }
//       state->enough_pixels = TRUE;
//     }

  unknown_hash = g_hash_table_new(g_int_hash, g_int_equal); // TODO assert int = int32

  for (ty = y1 / 64; ty <= (y2-1) / 64; ty++)
    {
      for (tx = x1 / 64; tx <= (x2-1) / 64; tx++)
        {
          guint   height_tile;
          guint   width_tile;

          load_big_cache (state->pixels, mask, state->big_cache, tx, ty, 1);

#ifdef IMAGE_DEBUG_PPM
          {
            static char buffer[100];

            snprintf (buffer, 100, "pixels_mask_%i_%i", tx, ty);
            debug_cache (buffer, state->big_cache, 1);
          }
#endif

          tile = gapp_tile_manager_get_at (result_layer, tx, ty, TRUE, TRUE);

          pointer = gapp_tile_data_pointer (tile, 0, 0);

          width_tile = gapp_tile_ewidth (tile);
          height_tile = gapp_tile_eheight (tile);

          for (y = 0; y < height_tile; y++)
            {
              for (x = 0; x < width_tile; x++, pointer += 4)
                {
                  guchar alpha;

                  pointer[0] = GET_PIXEL (state->big_cache, x, y, 0);
                  pointer[1] = GET_PIXEL (state->big_cache, x, y, 1);
                  pointer[2] = GET_PIXEL (state->big_cache, x, y, 2);
                  alpha = search_for_neighbours (x, y, pointer, state);
                  pointer[3] = alpha;

                  if (alpha == 128)
                    {
                      HashEntry *entry = g_slice_new (HashEntry);
                      // TODO: free this memory

                      // TODO: check if this can really be uncomented
                      //entry->foreground[0] = 255;
                      //entry->foreground[1] = 0;
                      //entry->foreground[2] = 0;
                      //entry->alpha = 255;
                      entry->valid = FALSE;
                      entry->this.coords.x = tx * 64 + x;
                      entry->this.coords.y = ty * 64 + y;

                      // TODO maybe look this up in image, instead of
                      // saving it redundantly in cache
                      entry->color[0] = pointer[0];
                      entry->color[1] = pointer[1];
                      entry->color[2] = pointer[2];
                      // TODO remove color from entry

                      // Linked list creation
                      if(first_entry == NULL)
                        first_entry = entry;
                      if (previous_entry != NULL)
                        previous_entry->next = entry;
                      previous_entry = entry;

                      g_hash_table_insert (unknown_hash, &(entry->this), entry);
                    }
                }
            }

          gapp_tile_release (tile, TRUE);
        }
    }

  g_return_if_fail(previous_entry != NULL);

  // End the list with a null pointer
  previous_entry->next = NULL;
  hash_size = g_hash_table_size(unknown_hash);
  hash_step = hash_size / 33;

  if (DEBUG_PHASE > 1)
    {
      // Phase 2, loop over values in hash and fill them in
      {
        HashEntry *current = first_entry;
        gint counter = 0;

        state->tx = -1;
        state->ty = -1;

        while (current != NULL)
          {
            counter++;
            search_neighborhood (current, state);
            current = current->next;

            if (counter % hash_step == 0)
              matting_progress_update(progress_callback, progress_data, (float)counter / hash_size * 0.33);
          }
      }

      if (DEBUG_PHASE > 2)
        {
          // Phase 3, get better values from neighbours
          {
            HashEntry *current = first_entry;
            gint counter = 0;

            state->tx = -1;
            state->ty = -1;

            while (current != NULL)
              {
                counter++;
                compare_neighborhood (current, unknown_hash, state);
                current = current->next;

                if (counter % hash_step == 0)
                  matting_progress_update(progress_callback, progress_data, (float)counter / hash_size * 0.33 + 0.33);
              }
          }

          if (DEBUG_PHASE > 3)
            // Phase 4, get final color values
            {
              HashEntry *current = first_entry;
              gint counter = 0;

              state->tx = -1;
              state->ty = -1;

              while (current != NULL)
                {
                  counter++;
                  if (current->valid)
                    local_smoothing (current, unknown_hash, state);

                  current = current->next;

                  if (counter % hash_step == 0)
                    matting_progress_update(progress_callback, progress_data, (float)counter / hash_size * 0.33 + 0.66);
                }
            }
        }
    }

  // Last phase, fill values from hash back into result layer
  {
    HashEntry *current = first_entry;
    HashEntry *tmp;
    GappTile *tile = NULL;
    gint current_tx = -1;
    gint current_ty = -1;
    gint tx, ty, pos_x, pos_y;
    gint i;

    while (current != NULL)
      {
        if (current->valid)
          {
            tx = current->this.coords.x / 64;
            ty = current->this.coords.y / 64;

            pos_x = current->this.coords.x - 64 * tx;
            pos_y = current->this.coords.y - 64 * ty;

            if (current_tx != tx || current_ty != ty)
              {
                if (tile != NULL)
                  gapp_tile_release(tile, TRUE);

                tile = gapp_tile_manager_get_at (result_layer, tx, ty, TRUE, TRUE);

                current_tx = tx;
                current_ty = ty;
              }

            pointer = gapp_tile_data_pointer (tile, pos_x, pos_y);

            for (i = 0; i < 3; i++)
              {
                if (DEBUG_PHASE == 3)
                  pointer[i] = current->foreground_refined[i];
                else
                  pointer[i] = current->foreground[i];
              }
            if (DEBUG_PHASE == 3)
              pointer[3] = current->alpha_refined;
            else
              pointer[3] = current->alpha;
          }

        tmp = current->next;
        g_slice_free(HashEntry, current);
        current = tmp;
      }

    if (tile != NULL)
      gapp_tile_release(tile, TRUE);
  }

  g_hash_table_destroy(unknown_hash);

  matting_progress_update(progress_callback, progress_data, 1);
  //update_mask (result_layer, mask, state);
}

void
matting_done (GappMattingState * state)
{
  g_return_if_fail (state != NULL);

  g_slice_free (GappMattingState, state);
}
