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

static gdouble  p_PivotRgb(gdouble n);
static gdouble  p_PivotXyz(gdouble n);


// static long p_gdouble_to_long(gdouble value)
// {
//   long lval;
//   
//   lval = value * TO_LONG_FACTOR;
//   return (lval);
// }
// static gdouble p_long_to_gdouble(long value)
// {
//   gdouble dval;
//   
//   dval = (gdouble)value / TO_GDOUDBLE_FACTOR;
//   return (dval);
// }





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
    printf("Simple rgb 1/2 (%03d %03d %03d) / (%03d %03d %03d) rDif:%03d gDif:%03d bDif:%03d colorDiff:%f\n"
       , (int)(aPixelPtr[0])
       , (int)(aPixelPtr[1])
       , (int)(aPixelPtr[2])
       , (int)(bPixelPtr[0])
       , (int)(bPixelPtr[1])
       , (int)(bPixelPtr[2])
       , (int)(rDif)
       , (int)(gDif)
       , (int)(bDif)
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
 *
 * DEPRECATED
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
 * DEPRECATED
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



/* #########################################
 * LAB based methods
 * #########################################
 *
 * this code uses the L*A*B conversion methods 
 * found at https://github.com/THEjoezack/ColorMine/blob/master/ColorMine/ColorSpaces/Conversions/LabConverter.cs
 *  ported to C.
 * because:
 * - they fit the colordiff methods from same source.
 * - did not check yet for possible L*A*B support available in GIMP-2.9
 *   (because this code is supposed to run with GIMP-2.8)
 * 
 */

/* ---------------------------------
 * p_PivotRgb
 * ---------------------------------
 */
static gdouble p_PivotRgb(gdouble n)
{
  gdouble ret;
  if (n > 0.04045)
  {
    ret = pow((n + 0.055) / 1.055, 2.4);
  }
  else
  {
    ret = n / 12.92;
  }
  return (ret * 100.0);
  
}  /* end p_PivotRgb */

/* ---------------------------------
 * p_PivotXyz
 * ---------------------------------
 */
static gdouble p_PivotXyz(gdouble n)
{
#define  XYZCONVERTER_EPSILON 0.008856 /* Intent is 216/24389 */
#define  XYZCONVERTER_KAPPA   903.3    /* Intent is 24389/27 */
  gdouble ret;
  if (n > XYZCONVERTER_EPSILON)
  {
    /* ret = CubicRoot(n) */
    ret = pow(n, 1.0 / 3.0);
  }
  else
  {
    ret = (XYZCONVERTER_KAPPA * n + 16.0) / 116.0;
  }
  return ret;

}  /* end p_PivotXyz */


/* ---------------------------------
 * p_convert_rgb8_to_Lab
 * ---------------------------------
 * based on algorithm found at 
 *  https://github.com/THEjoezack/ColorMine/blob/master/ColorMine/ColorSpaces/Conversions/LabConverter.cs
 *  https://github.com/THEjoezack/ColorMine/blob/master/ColorMine/ColorSpaces/Conversions/XyzConverter.cs
 *
 */
void p_convert_rgb8_to_Lab(guchar *pixelPtrRgb8, GapColorLAB *lab)
{
  /* XYZ colorspace constants */
  #define  WHITE_X   95.047
  #define  WHITE_Y  100.0  
  #define  WHITE_Z  108.883
  
  gdouble xyzX,xyzY,xyzZ;
  gdouble x,y,z;
  gdouble r,g,b;
  
  r = p_PivotRgb((gdouble)pixelPtrRgb8[0] / 255.0);
  g = p_PivotRgb((gdouble)pixelPtrRgb8[1] / 255.0);
  b = p_PivotRgb((gdouble)pixelPtrRgb8[2] / 255.0);
  
  /* Observer. = 2°, Illuminant = D65 */
  xyzX = r * 0.4124 + g * 0.3576 + b * 0.1805;
  xyzY = r * 0.2126 + g * 0.7152 + b * 0.0722;
  xyzZ = r * 0.0193 + g * 0.1192 + b * 0.9505;

  x = p_PivotXyz(xyzX / WHITE_X);
  y = p_PivotXyz(xyzY / WHITE_Y);
  z = p_PivotXyz(xyzZ / WHITE_Z);  


  lab->L = MAX(0.0, 116.0 * y - 16.0);
  lab->A = 500.0 * (x - y);
  lab->B = 200.0 * (y - z);

}  /* end p_convert_rgb8_to_Lab */



/* ---------------------------------
 * gap_colordiff_LabDeltaE2000
 * ---------------------------------
 * uses algortihm found at http://colormine.org/delta-e-calculator/cie2000
 * based on L*a*b Colorspace.
 * returns deltaE scaled down to a range of 0.0 to 1.0
 */
gdouble gap_colordiff_LabDeltaE2000(guchar *aPixelPtr
                   , guchar *bPixelPtr
                   , gboolean debugPrint
                   )
{
  //Set weighting factors to 1
  gdouble k_L = 1.0;
  gdouble k_C = 1.0;
  gdouble k_H = 1.0;


  GapColorLAB lab1;
  GapColorLAB lab2;
  
  //Change Color Space to L*a*b:
  p_convert_rgb8_to_Lab(aPixelPtr, &lab1);
  p_convert_rgb8_to_Lab(bPixelPtr, &lab2);
  

  //Calculate Cprime1, Cprime2, Cabbar
  gdouble c_star_1_ab = sqrt(lab1.A * lab1.A + lab1.B * lab1.B);
  gdouble c_star_2_ab = sqrt(lab2.A * lab2.A + lab2.B * lab2.B);
  gdouble c_star_average_ab = (c_star_1_ab + c_star_2_ab) / 2;

  gdouble c_star_average_ab_pot7 = c_star_average_ab * c_star_average_ab * c_star_average_ab;
  c_star_average_ab_pot7 *= c_star_average_ab_pot7 * c_star_average_ab;

  gdouble G = 0.5 * (1 - sqrt(c_star_average_ab_pot7 / (c_star_average_ab_pot7 + 6103515625))); //25^7
  gdouble a1_prime = (1 + G) * lab1.A;
  gdouble a2_prime = (1 + G) * lab2.A;

  gdouble C_prime_1 = sqrt(a1_prime * a1_prime + lab1.B * lab1.B);
  gdouble C_prime_2 = sqrt(a2_prime * a2_prime + lab2.B * lab2.B);
  //Angles in Degree.
  gdouble h_prime_1 = (int)rint(gimp_rad_to_deg(atan2(lab1.B, a1_prime)) + 360) % 360;
  gdouble h_prime_2 = (int)rint(gimp_rad_to_deg(atan2(lab2.B, a2_prime)) + 360) % 360;

  gdouble delta_L_prime = lab2.L - lab1.L;
  gdouble delta_C_prime = C_prime_2 - C_prime_1;

  gdouble h_bar = abs(h_prime_1 - h_prime_2);
  gdouble delta_h_prime;
  if (C_prime_1 * C_prime_2 == 0) delta_h_prime = 0;
  else
  {
      if (h_bar <= 180.0)
      {
          delta_h_prime = h_prime_2 - h_prime_1;
      }
      else if (h_bar > 180.0 && h_prime_2 <= h_prime_1)
      {
          delta_h_prime = h_prime_2 - h_prime_1 + 360.0;
      }
      else
      {
          delta_h_prime = h_prime_2 - h_prime_1 - 360.0;
      }
  }
  gdouble delta_H_prime = 2 * sqrt(C_prime_1 * C_prime_2) * sin(gimp_deg_to_rad(delta_h_prime / 2));

  // Calculate CIEDE2000
  gdouble L_prime_average = (lab1.L + lab2.L) / 2.0;
  gdouble C_prime_average = (C_prime_1 + C_prime_2) / 2.0;

  //Calculate h_prime_average

  gdouble h_prime_average;
  if (C_prime_1 * C_prime_2 == 0) h_prime_average = 0;
  else
  {
      if (h_bar <= 180.0)
      {
          h_prime_average = (h_prime_1 + h_prime_2) / 2;
      }
      else if (h_bar > 180.0 && (h_prime_1 + h_prime_2) < 360.0)
      {
          h_prime_average = (h_prime_1 + h_prime_2 + 360.0) / 2;
      }
      else
      {
          h_prime_average = (h_prime_1 + h_prime_2 - 360.0) / 2;
      }
  }
  gdouble L_prime_average_minus_50_square = (L_prime_average - 50);
  L_prime_average_minus_50_square *= L_prime_average_minus_50_square;

  gdouble S_L = 1 + ((.015d * L_prime_average_minus_50_square) / sqrt(20 + L_prime_average_minus_50_square));
  gdouble S_C = 1 + .045d * C_prime_average;
  gdouble T = 1
      - .17 * cos(gimp_deg_to_rad(h_prime_average - 30))
      + .24 * cos(gimp_deg_to_rad(h_prime_average * 2))
      + .32 * cos(gimp_deg_to_rad(h_prime_average * 3 + 6))
      - .2 * cos(gimp_deg_to_rad(h_prime_average * 4 - 63));
  gdouble S_H = 1 + .015 * T * C_prime_average;
  gdouble h_prime_average_minus_275_div_25_square = (h_prime_average - 275) / (25);
  h_prime_average_minus_275_div_25_square *= h_prime_average_minus_275_div_25_square;
  gdouble delta_theta = 30 * exp(-h_prime_average_minus_275_div_25_square);

  gdouble C_prime_average_pot_7 = C_prime_average * C_prime_average * C_prime_average;
  C_prime_average_pot_7 *= C_prime_average_pot_7 * C_prime_average;
  gdouble R_C = 2 * sqrt(C_prime_average_pot_7 / (C_prime_average_pot_7 + 6103515625));

  gdouble R_T = -sin(gimp_deg_to_rad(2 * delta_theta)) * R_C;

  gdouble delta_L_prime_div_k_L_S_L = delta_L_prime / (S_L * k_L);
  gdouble delta_C_prime_div_k_C_S_C = delta_C_prime / (S_C * k_C);
  gdouble delta_H_prime_div_k_H_S_H = delta_H_prime / (S_H * k_H);

  gdouble CIEDE2000 = sqrt(
      delta_L_prime_div_k_L_S_L * delta_L_prime_div_k_L_S_L
      + delta_C_prime_div_k_C_S_C * delta_C_prime_div_k_C_S_C
      + delta_H_prime_div_k_H_S_H * delta_H_prime_div_k_H_S_H
      + R_T * delta_C_prime_div_k_C_S_C * delta_H_prime_div_k_H_S_H
      );


  gdouble colorDiff = CIEDE2000 / 100.0;  /* normalize range [from 0.0 to 100.0] ==> [from 0.0 to 1.0] */
  if(debugPrint)
  {
    printf("L*a*b (%.3f, %.3f, %.3f) rgb 1/2 (%03d %03d %03d) / (%03d %03d %03d) CIEDE2000:%f colorDiff:%f\n"
       , lab1.L
       , lab1.A
       , lab1.B
       , (int)(aPixelPtr[0])
       , (int)(aPixelPtr[1])
       , (int)(aPixelPtr[2])
       , (int)(bPixelPtr[0])
       , (int)(bPixelPtr[1])
       , (int)(bPixelPtr[2])
       , CIEDE2000
       , colorDiff
       );
  }

  return (colorDiff);

}  /* end gap_colordiff_LabDeltaE2000 */


/* ---------------------------------
 * gap_colordiff_LabDeltaE94
 * ---------------------------------
 * uses algortihm found at http://colormine.org/delta-e-calculator/cie94
 * based on L*a*b Colorspace.
 * returns deltaE scaled down to a range of 0.0 to 1.0
 */
gdouble gap_colordiff_LabDeltaE94(guchar *aPixelPtr
                   , guchar *bPixelPtr
                   , gboolean debugPrint
                   )
{
  // case Application.GraphicArts:
  gdouble CONSTANTS_Kl = 1.0;
  gdouble CONSTANTS_K1 = .045;
  gdouble CONSTANTS_K2 = .015;
  
  // case Application.Textiles:
  //gdouble CONSTANTS_Kl = 2.0;
  //gdouble CONSTANTS_K1 = .048;
  //gdouble CONSTANTS_K2 = .014;

  gdouble sl = 1.0;
  gdouble kc = 1.0;
  gdouble kh = 1.0;


  GapColorLAB labA;
  GapColorLAB labB;
  
  //Change Color Space to L*a*b:
  p_convert_rgb8_to_Lab(aPixelPtr, &labA);
  p_convert_rgb8_to_Lab(bPixelPtr, &labB);

  gdouble deltaL = labA.L - labB.L;
  gdouble deltaA = labA.A - labB.A;
  gdouble deltaB = labA.B - labB.B;

  gdouble c1 = sqrt(labA.A * labA.A + labA.B * labA.B);
  gdouble c2 = sqrt(labB.A * labB.A + labB.B * labB.B);
  gdouble deltaC = c1 - c2;

  gdouble deltaH = deltaA * deltaA + deltaB * deltaB - deltaC * deltaC;
  deltaH = deltaH < 0 ? 0 : sqrt(deltaH);


  gdouble sc = 1.0 + CONSTANTS_K1 * c1;
  gdouble sh = 1.0 + CONSTANTS_K2 * c1;

  gdouble deltaLKlsl = deltaL / (CONSTANTS_Kl * sl);
  gdouble deltaCkcsc = deltaC / (kc * sc);
  gdouble deltaHkhsh = deltaH / (kh * sh);
  gdouble i = deltaLKlsl * deltaLKlsl + deltaCkcsc * deltaCkcsc + deltaHkhsh * deltaHkhsh;

  gdouble cie94 = (i < 0) ? 0 : sqrt(i);

  gdouble colorDiff = cie94 / 100.0;  /* normalize range [from 0.0 to 100.0] ==> [from 0.0 to 1.0] */
  if(debugPrint)
  {
    printf("L*a*b (%.3f, %.3f, %.3f) (%.3f, %.3f, %.3f) rgb 1/2 (%03d %03d %03d) / (%03d %03d %03d) cie94:%f colorDiff:%f\n"
       , labA.L
       , labA.A
       , labA.B
       , labB.L
       , labB.A
       , labB.B
       , (int)(aPixelPtr[0])
       , (int)(aPixelPtr[1])
       , (int)(aPixelPtr[2])
       , (int)(bPixelPtr[0])
       , (int)(bPixelPtr[1])
       , (int)(bPixelPtr[2])
       , cie94
       , colorDiff
       );
  }

  return (colorDiff);


}  /* end gap_colordiff_LabDeltaE94 */
 
