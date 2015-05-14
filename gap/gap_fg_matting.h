/* gap_fg_matting.h
 *  This module is a modified copy of the alpha matting
 *  forground extraction.
 *  The modifications allow the code to run as plug-in
 *  usable with official GIMP-releases (2.6.x and later)
 *  The original implemantation was done
 *   by Jan Ruegg as gimp core tool in the unofficial repository
 *   https://github.com/rggjan/Gimp-Matting in the "new_layer" branch.
 *
 */
/*
 * matting: Simple Interactive Object Extraction
 *
 * For algorithm documentation refer to:
 * G. Friedland, K. Jantz, L. Knipping, R. Rojas:
 * "Image Segmentation by Uniform Color Clustering
 *  -- Approach and Benchmark Results",
 * Technical Report B-05-07, Department of Computer Science,
 * Freie Universitaet Berlin, June 2005.
 * http://www.inf.fu-berlin.de/inst/pubs/tr-b-05-07.pdf
 *
 * See http://www.matting.org/ for more information.
 *
 * Algorithm idea by Gerald Friedland.
 * This implementation is Copyright (C) 2005
 * by Gerald Friedland <fland@inf.fu-berlin.de>
 * and Kristian Jantz <jantz@inf.fu-berlin.de>.
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


#ifndef __GAP_MATTING_H__
#define __GAP_MATTING_H__

#include "gap_fg_tile_manager.h"


#define MATTING_USER_FOREGROUND 240
#define MATTING_ALGO_FOREGROUND 200
#define MATTING_ALGO_UNDEFINED 128
#define MATTING_USER_BACKGROUND 0
#define MATTING_ALGO_BACKGROUND 20

typedef void (* MattingProgressFunc) (gpointer  progress_data,
                                      gdouble   fraction);

GappMattingState      *matting_init (GappTileManager        *pixels,
                                 const guchar       *colormap,
                                 gint                offset_x,
                                 gint                offset_y,
                                 gint                x,
                                 gint                y,
                                 gint                width,
                                 gint                height);

void matting_foreground_extract (GappMattingState       *state,
                                 GappTileManager        *mask,
                                 gint                x1,
                                 gint                y1,
                                 gint                x2,
                                 gint                y2,
                                 gfloat              start_percentage,
                                 MattingProgressFunc progress_callback,
                                 gpointer            progress_data,
                                 GappTileManager        *result_layer);

void matting_done               (GappMattingState        *state);

#endif /* __GAP_MATTING_H__ */
