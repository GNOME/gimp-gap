/* gap_colordiff.c
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

/* SYTEM (UNIX) includes */
#include "string.h"
/* GIMP includes */
/* GAP includes */
#include "gap_lib_common_defs.h"
#include "gap_pdb_calls.h"
#include "gap_colordiff.h"
#include "gap_lib.h"
#include "gap_image.h"
#include "gap_layer_copy.h"
#include "gap_libgapbase.h"


#define TO_LONG_FACTOR     100000
#define TO_GDOUDBLE_FACTOR 100000.0

extern      int gap_debug; /* ==0  ... dont print debug infos */



static long p_gdouble_to_long(gdouble value)
{
  long lval;
  
  lval = value * TO_LONG_FACTOR;
  return (lval);
}
static gdouble p_long_to_gdouble(long value)
{
  gdouble dval;
  
  dval = (gdouble)value / TO_GDOUDBLE_FACTOR;
  return (dval);
}





/* ---------------------------------
 * gap_colordiff_GimpHSV
 * ---------------------------------
 * returns difference of 2 colors as gdouble value
 * in range 0.0 (exact match) to 1.0 (maximal difference)
 */
gdouble
gap_colordiff_GimpHSV(GimpHSV *aHsvPtr
                  , GimpHSV *bHsvPtr
                  , gdouble colorSensitivity
                  , gboolean debugPrint)
{
  gdouble colorDiff;
  gdouble hDif;
  gdouble sDif;
  gdouble vDif;
  gdouble vMax;
  gdouble sMax;
  gdouble weight;


  hDif = fabs(aHsvPtr->h - bHsvPtr->h);
  /* normalize hue difference.
   * hue values represents an angle
   * where value 0.5 equals 180 degree
   * and value 1.0 stands for 360 degree that is
   * equal to 0.0
   * Hue is maximal different at 180 degree.
   *
   * after normalizing, the difference
   * hDiff value 1.0 represents angle difference of 180 degree
   */
  if(hDif > 0.5)
  {
    hDif = (1.0 - hDif) * 2.0;
  }
  else
  {
    hDif = hDif * 2.0;

  }
  sDif = fabs(aHsvPtr->s - bHsvPtr->s);
  vDif = fabs(aHsvPtr->v - bHsvPtr->v);
  

  vMax = MAX(aHsvPtr->v, bHsvPtr->v);

  if(vMax <= 0.25)
  {
    /* reduce weight of hue and saturation
     * when comparing 2 dark colors
     */
    weight = vMax / 0.25;
    weight *= (weight * weight);
    colorDiff = 1.0 - ((1.0 - pow((hDif * weight), colorSensitivity)) * (1.0 -  pow((sDif * weight), colorSensitivity)) * (1.0 -  pow(vDif, colorSensitivity)));
  }
  else
  {
    sMax = MAX(aHsvPtr->s, bHsvPtr->s);
    if (sMax <= 0.25)
    {
      /* reduce weight of hue and saturation
       * when comparing 2 gray colors
       */
      weight = sMax / 0.25;
      weight *= (weight * weight);
      colorDiff = 1.0 - ((1.0 - pow((hDif * weight), colorSensitivity)) * (1.0 -  pow((sDif * weight), colorSensitivity)) * (1.0 -  pow(vDif, colorSensitivity)));
    }
    else
    {
      weight = 1.0;
      colorDiff = 1.0 - ((1.0 - pow(hDif, colorSensitivity)) * (1.0 -  pow(sDif, colorSensitivity)) * (1.0 -  pow(vDif, colorSensitivity)));
    }
  }



  if(debugPrint)
  {
    printf("HSV: hsv 1/2 (%.3f %.3f %.3f) / (%.3f %.3f %.3f) vMax:%f\n"
       , aHsvPtr->h
       , aHsvPtr->s
       , aHsvPtr->v
       , bHsvPtr->h
       , bHsvPtr->s
       , bHsvPtr->v
       , vMax
       );
    printf("diffHSV:  (%.3f %.3f %.3f)  weight:%.3f colorSensitivity:%.3f colorDiff:%.5f\n"
       , hDif
       , sDif
       , vDif
       , weight
       , colorSensitivity
       , colorDiff
       );
  }

  return (colorDiff);
}  /* end gap_colordiff_GimpHSV */



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
                  , gboolean debugPrint)
{
  GimpRGB bRgb;
  GimpHSV bHsv;
  gdouble colordiff;

  gimp_rgba_set_uchar (&bRgb, pixel[0], pixel[1], pixel[2], 255);
  gimp_rgb_to_hsv(&bRgb, &bHsv);

  colordiff = gap_colordiff_GimpHSV(aHsvPtr, &bHsv, colorSensitivity, debugPrint);
  return (colordiff);

}  /* end gap_colordiff_guchar_GimpHSV */




/* ---------------------------------
 * gap_colordiff_GimpRGB
 * ---------------------------------
 * returns difference of 2 colors as gdouble value
 * in range 0.0 (exact match) to 1.0 (maximal difference)
 * Note: 
 * this procedure uses the same HSV colormodel based
 * Algorithm as the HSV specific procedures.
 */
gdouble
gap_colordiff_GimpRGB(GimpRGB *aRgbPtr
                  , GimpRGB *bRgbPtr
                  , gdouble colorSensitivity
                  , gboolean debugPrint)
{
  GimpHSV aHsv;
  GimpHSV bHsv;
  gdouble colordiff;

  gimp_rgb_to_hsv(aRgbPtr, &aHsv);
  gimp_rgb_to_hsv(bRgbPtr, &bHsv);


  colordiff = gap_colordiff_GimpHSV(&aHsv, &bHsv, colorSensitivity, debugPrint);
  if(debugPrint)
  {
    printf("RGB 1/2 (%.3g %.3g %.3g) / (%.3g %.3g %.3g) colordiff:%f\n"
       , aRgbPtr->r
       , aRgbPtr->g
       , aRgbPtr->b
       , bRgbPtr->r
       , bRgbPtr->g
       , bRgbPtr->b
       , colordiff
       );
  }
  return (colordiff);

}  /* end gap_colordiff_GimpRGB */



/* ---------------------------------
 * gap_colordiff_guchar
 * ---------------------------------
 * returns difference of 2 colors as gdouble value
 * in range 0.0 (exact match) to 1.0 (maximal difference)
 * Note: 
 * this procedure uses the same HSV colormodel based
 * Algorithm as the HSV specific procedures.
 */
gdouble
gap_colordiff_guchar(guchar *aPixelPtr
                   , guchar *bPixelPtr
                   , gdouble colorSensitivity
                   , gboolean debugPrint
                   )
{
  GimpRGB aRgb;
  GimpRGB bRgb;

  gimp_rgba_set_uchar (&aRgb, aPixelPtr[0], aPixelPtr[1], aPixelPtr[2], 255);
  gimp_rgba_set_uchar (&bRgb, bPixelPtr[0], bPixelPtr[1], bPixelPtr[2], 255);
  return (gap_colordiff_GimpRGB(&aRgb
                            , &bRgb
                            , colorSensitivity
                            , debugPrint
                            ));

}  /* end gap_colordiff_guchar */


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
                   )
{
  gdouble rDif;
  gdouble gDif;
  gdouble bDif;
  gdouble colorDiff;

  rDif = fabs(aRgbPtr->r - bRgbPtr->r);
  gDif = fabs(aRgbPtr->g - bRgbPtr->g);
  bDif = fabs(aRgbPtr->b - bRgbPtr->b);

  colorDiff = (rDif + gDif + bDif) / 3.0;

  if(debugPrint)
  {
    printf("RGB 1/2 (%.3g %.3g %.3g) / (%.3g %.3g %.3g) rDif:%.3g gDif:%.3g bDif:%.3g colorDiff:%f\n"
       , aRgbPtr->r
       , aRgbPtr->g
       , aRgbPtr->b
       , bRgbPtr->r
       , bRgbPtr->g
       , bRgbPtr->b
       , rDif
       , gDif
       , bDif
       , colorDiff
       );
  }
  return (colorDiff);
 

}  /* end gap_colordiff_simple_GimpRGB */




/* ---------------------------------
 * gap_colordiff_simple_guchar
 * ---------------------------------
 * returns difference of 2 colors as gdouble value
 * in range 0.0 (exact match) to 1.0 (maximal difference)
 * Note: 
 * this procedure uses a simple (and faster) algorithm
 * that is based on the RGB colorspace
 * in 1 byte per channel representation.
 */
gdouble
gap_colordiff_simple_guchar(guchar *aPixelPtr
                   , guchar *bPixelPtr
                   , gboolean debugPrint
                   )
{
#define MAX_CHANNEL_SUM 765.0
  int rDif;
  int gDif;
  int bDif;
  gdouble colorDiff;

  rDif = abs(aPixelPtr[0] - bPixelPtr[0]);
  gDif = abs(aPixelPtr[1] - bPixelPtr[1]);
  bDif = abs(aPixelPtr[2] - bPixelPtr[2]);

  colorDiff = (gdouble)(rDif + gDif + bDif) / MAX_CHANNEL_SUM;
  if(debugPrint)
  {
    printf("rgb 1/2 (%.3g %.3g %.3g) / (%.3g %.3g %.3g) rDif:%.3g gDif:%.3g bDif:%.3g colorDiff:%f\n"
       , (float)(aPixelPtr[0]) / 255.0
       , (float)(aPixelPtr[1]) / 255.0
       , (float)(aPixelPtr[2]) / 255.0
       , (float)(bPixelPtr[0]) / 255.0
       , (float)(bPixelPtr[1]) / 255.0
       , (float)(bPixelPtr[2]) / 255.0
       , (float)(rDif) / 255.0
       , (float)(gDif) / 255.0
       , (float)(bDif) / 255.0
       , colorDiff
       );
  }
  return (colorDiff);

}  /* end gap_colordiff_simple_guchar */



/* ---------------------------------
 * gap_colordiff_hvmax_GimpHSV
 * ---------------------------------
 * returns difference of 2 colors as gdouble value
 * in range 0.0 (exact match) to 1.0 (maximal difference)
 */
gdouble
gap_colordiff_hvmax_GimpHSV(GimpHSV *aHsvPtr
                  , GimpHSV *bHsvPtr
                  , gboolean debugPrint)
{
  gdouble colorDiff;
  gdouble hDif;
  gdouble h2Dif;
  gdouble sDif;
  gdouble s2Dif;
  gdouble vDif;
  gdouble v2Dif;
  gdouble vMax;
  gdouble sMax;


  hDif = fabs(aHsvPtr->h - bHsvPtr->h);
  /* normalize hue difference.
   * hue values represents an angle
   * where value 0.5 equals 180 degree
   * and value 1.0 stands for 360 degree that is
   * equal to 0.0
   * Hue is maximal different at 180 degree.
   *
   * after normalizing, the difference
   * hDiff value 1.0 represents angle difference of 180 degree
   */
  if(hDif > 0.5)
  {
    hDif = (1.0 - hDif) * 2.0;
  }
  else
  {
    hDif = hDif * 2.0;
  }
  sDif = fabs(aHsvPtr->s - bHsvPtr->s);
  vDif = fabs(aHsvPtr->v - bHsvPtr->v);
  
  sMax = MAX(aHsvPtr->s, bHsvPtr->s);
  vMax = MAX(aHsvPtr->v, bHsvPtr->v);
  
  /* increase hue weight when comparing well saturated colors */
  h2Dif = MIN(1.0, ((4.0 * sMax) * hDif));
  v2Dif = vDif * vDif;
  s2Dif = sDif * sDif * sDif;

  colorDiff = MAX(MAX(v2Dif, h2Dif),s2Dif);



  if(debugPrint)
  {
    printf("max HSV: hsv 1/2 (%.3f %.3f %.3f) / (%.3f %.3f %.3f) vMax:%f\n"
       , aHsvPtr->h
       , aHsvPtr->s
       , aHsvPtr->v
       , bHsvPtr->h
       , bHsvPtr->s
       , bHsvPtr->v
       , vMax
       );
    printf("diffHSV:  (h2:%.3f h:%.3f s2:%.3f s:%.3f v2:%.3f v:%.3f)  max colorDiff:%.5f\n"
       , h2Dif
       , hDif
       , s2Dif
       , sDif
       , v2Dif
       , vDif
       , colorDiff
       );
  }

  return (colorDiff);

}  /* end gap_colordiff_hvmax_GimpHSV */




/* ---------------------------------
 * gap_colordiff_hvmax_guchar
 * ---------------------------------
 * returns difference of 2 colors as gdouble value
 * in range 0.0 (exact match) to 1.0 (maximal difference)
 * Note: 
 * this procedure uses an HSV colormodel based
 * Algorithm and calculates difference as max HSV difference.
 *
 */
gdouble
gap_colordiff_hvmax_guchar(guchar *aPixelPtr
                   , guchar *bPixelPtr
                   , gboolean debugPrint
                   )
{
  GimpRGB aRgb;
  GimpRGB bRgb;
  GimpHSV aHsv;
  GimpHSV bHsv;

  gimp_rgba_set_uchar (&aRgb, aPixelPtr[0], aPixelPtr[1], aPixelPtr[2], 255);
  gimp_rgba_set_uchar (&bRgb, bPixelPtr[0], bPixelPtr[1], bPixelPtr[2], 255);

  gimp_rgb_to_hsv(&aRgb, &aHsv);
  gimp_rgb_to_hsv(&bRgb, &bHsv);

  return (gap_colordiff_hvmax_GimpHSV(&aHsv
                            , &bHsv
                            , debugPrint
                            ));

}  /* end gap_colordiff_hvmax_guchar */

