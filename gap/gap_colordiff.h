/* gap_colordiff.h
 *    by hof (Wolfgang Hofer)
 *    color difference procedures
 *  2010/08/06
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

#ifndef _GAP_COLORDIFF_H
#define _GAP_COLORDIFF_H

/* SYTEM (UNIX) includes */
#include <stdio.h>
#include <stdlib.h>

/* GIMP includes */
#include "gtk/gtk.h"
#include "libgimp/gimp.h"


#define GAP_COLORDIFF_DEFAULT_SENSITIVITY 1.35


typedef struct GapColorLAB {
   gdouble      L; 
   gdouble      A; 
   gdouble      B; 
} GapColorLAB;

/* ---------------------------------
 * gap_colordiff_GimpHSV
 * ---------------------------------
 * returns difference of 2 colors as gdouble value
 * in range 0.0 (exact match) to 1.0 (maximal difference)
 *
 * use this procedure in case both colors are already availabe in GimpHSV color.
 */
gdouble
gap_colordiff_GimpHSV(GimpHSV *aHsvPtr
                  , GimpHSV *bHsvPtr
                  , gdouble colorSensitivity
                  , gboolean debugPrint);


/* ---------------------------------
 * gap_colordiff_guchar_GimpHSV
 * ---------------------------------
 * returns difference of 2 colors as gdouble value
 * in range 0.0 (exact match) to 1.0 (maximal difference)
 */
gdouble
gap_colordiff_guchar_GimpHSV(GimpHSV *aHsvPtr
                  , guchar *pixel
                  , gdouble colorSensitivity
                  , gboolean debugPrint);


/* ---------------------------------
 * gap_colordiff_GimpRGB
 * ---------------------------------
 * returns difference of 2 colors as gdouble value
 * in range 0.0 (exact match) to 1.0 (maximal difference)
 *
 * use this procedure in case both colors are availabe in GimpRGB color.
 * Note: 
 * this procedure uses the same HSV colormodel based
 * Algorithm as the HSV specific procedures.
 */
gdouble  gap_colordiff_GimpRGB(GimpRGB *color1
                            , GimpRGB *color2
			    , gdouble sensitivity
                            , gboolean debugPrint
                            );

/* ---------------------------------
 * gap_colordiff_guchar
 * ---------------------------------
 * returns difference of 2 colors as gdouble value
 * in range 0.0 (exact match) to 1.0 (maximal difference)
 *
 * use this procedure in case where both colors are available
 * as rgb byte value components in the range 0 to 255.
 * Note: 
 * this procedure uses the same HSV colormodel based
 * Algorithm as the HSV specific procedures.
 */
gdouble
gap_colordiff_guchar(guchar *aPixelPtr
                   , guchar *bPixelPtr
                   , gdouble colorSensitivity
                   , gboolean debugPrint
                   );


/* ---------------------------------
 * gap_colordiff_simple_GimpRGB
 * ---------------------------------
 * returns difference of 2 colors as gdouble value
 * in range 0.0 (exact match) to 1.0 (maximal difference)
 * Note: 
 * this procedure uses a simple (and faster) algorithm
 * that is based on the RGB colorspace
 */
gdouble
gap_colordiff_simple_GimpRGB(GimpRGB *aRgbPtr
                   , GimpRGB *bRgbPtr
                   , gboolean debugPrint
                   );


/* ---------------------------------
 * gap_colordiff_simple_guchar
 * ---------------------------------
 * returns difference of 2 colors as gdouble value
 * in range 0.0 (exact match) to 1.0 (maximal difference)
 * Note: 
 * this procedure uses a simple (and faster) algorithm
 * that is based on the RGB colorspace
 * PixelPtr must point to guchar data of at least 3 bytes size
 * sequence RGB
 */
gdouble
gap_colordiff_simple_guchar(guchar *aPixelPtr
                   , guchar *bPixelPtr
                   , gboolean debugPrint
                   );






/* ---------------------------------
 * gap_colordiff_hvmax_guchar
 * ---------------------------------
 * returns difference of 2 colors as gdouble value
 * in range 0.0 (exact match) to 1.0 (maximal difference)
 * Note: 
 * this procedure uses an HSV colormodel based
 * Algorithm and calculates difference as max HSV difference.
 * 
 * DEPRECATED
 *
 */
gdouble
gap_colordiff_hvmax_guchar(guchar *aPixelPtr
                   , guchar *bPixelPtr
                   , gboolean debugPrint
                   );


/* ---------------------------------
 * gap_colordiff_hvmax_GimpHSV
 * ---------------------------------
 * returns difference of 2 colors as gdouble value
 * in range 0.0 (exact match) to 1.0 (maximal difference)
 * 
 * DEPRECATED
 */
gdouble
gap_colordiff_hvmax_GimpHSV(GimpHSV *aHsvPtr
                  , GimpHSV *bHsvPtr
                  , gboolean debugPrint);




/* ---------------------------------
 * p_convert_rgb8_to_Lab
 * ---------------------------------
 * 
 */
void     
p_convert_rgb8_to_Lab(guchar *pixelPtrRgb8, GapColorLAB *lab);


/* ---------------------------------
 * gap_colordiff_LabDeltaE2000
 * ---------------------------------
 * 
 */
gdouble  
gap_colordiff_LabDeltaE2000(guchar *aPixelPtr
                   , guchar *bPixelPtr
                   , gboolean debugPrint
                   );                   

/* ---------------------------------
 * gap_colordiff_LabDeltaE94
 * ---------------------------------
 * 
 */
gdouble 
gap_colordiff_LabDeltaE94(guchar *aPixelPtr
                   , guchar *bPixelPtr
                   , gboolean debugPrint
                   );
                  

#endif
