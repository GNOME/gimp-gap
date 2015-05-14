/* gap_colormask_file.h
 *    by hof (Wolfgang Hofer)
 *    colormask filter parameter file handling procedures (load/save)
 *  2010/02/25
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
 * version 2.7.0;             hof: created
 */

#ifndef _GAP_COLORMASK_FILE_H
#define _GAP_COLORMASK_FILE_H

/* SYTEM (UNIX) includes */
#include <stdio.h>
#include <stdlib.h>

/* GIMP includes */
#include "gtk/gtk.h"
#include "libgimp/gimp.h"

#define GAP_COLORMASK_ALGO_SIMPLE        0
#define GAP_COLORMASK_ALGO_AVG_CHANGE_1  1
#define GAP_COLORMASK_ALGO_AVG_CHANGE_2  2
#define GAP_COLORMASK_ALGO_AVG_SMART     3
#define GAP_COLORMASK_ALGO_AVG_AREA      4

#define GAP_COLORMASK_ALGO_EDGE          5
#define GAP_COLORMASK_ALGO_MAX           5.0

typedef struct {
     gint32     colormask_id;
     gdouble    loColorThreshold;      /* color difference threshold 0.0 to 1.0 */
     gdouble    hiColorThreshold;    /* color difference threshold 0.0 to 1.0 */
     gdouble    colorSensitivity;    /* 1.0 to 2.0 sensitivity for colr diff algorithm */
     gdouble    lowerOpacity;        /* opacity for matching pixels 0.0 to 1.0 */
     gdouble    upperOpacity;        /* opacity for non matching pixels 0.0 to 1.0 */
     gdouble    triggerAlpha;        /* 0.0 to 1.0 mask opacity greater than this alpha value trigger color masking */
     gdouble    isleRadius;          /* radius in pixels for remove isaolated pixels */
     gdouble    isleAreaPixelLimit;  /* size of area in pixels for remove isaolated pixel(area)s */
     gdouble    featherRadius;       /* radius in pixels for feathering edges */

     gdouble    edgeColorThreshold;
     gdouble    thresholdColorArea;
     gdouble    pixelDiagonal;
     gdouble    pixelAreaLimit;
     gboolean   connectByCorner;

     gboolean   keepLayerMask;
     gboolean   keepWorklayer;       /* shall be FALSE for productive usage
                                      * TRUE: debug feature for visible verification of the area algorithm processing
                                      */
     gboolean   enableKeyColorThreshold;
     GimpRGB    keycolor;
     gdouble    loKeyColorThreshold;    /* color difference lower threshold for keycolor 0.0 to 1.0 */
     gdouble    keyColorSensitivity;    /* 0.1 upto 10.0 default 1.0 */

     gdouble significantRadius;              /* radius to limit search for significant brightness/colorchanges */
     gdouble significantColordiff;           /* threshold to define when colors are considered as significant different */
     gdouble significantBrightnessDiff;      /* threshold to define significant brightness changes */


     gint       algorithm;           /* 0 = simple, 1 = edge, 2= area */

} GapColormaskValues;


gboolean gap_colormask_file_load (const char *filename, GapColormaskValues *cmaskvals);

gboolean gap_colormask_file_save (const char *filename, GapColormaskValues *cmaskvals);


#endif
