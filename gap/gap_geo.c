/*  gap_geo.c
 *    This module provides structures and procedures to handle 2d geometric stuff
 *    such as perspective transformation.
 *  2016/04/18
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

/* Revision history
 *  (2016/04/18)  2.8.x       hof: created
 */
extern int gap_debug;

#include "config.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <glib.h>
#include <glib/gstdio.h>
#include <math.h>

#include <gtk/gtk.h>
#include <libgimp/gimp.h>
#include <libgimp/gimpui.h>

#include "gap_geo.h"


/* TRANSFORM_MAX_ITERATIONCOUNT is the number of maxiaml iteration attempts 
 * to findout the relevant 4 corners for perspective transformation
 * In test frames it took typical 
 *   less than  25 iterations to reach a transformPrecision of  0.5 pixels
 *   less than  36 iterations to reach a transformPrecision of  0.2 pixels
 *   less than 106 iterations to reach a transformPrecision of  0.005 pixels
 *   less than 128 iterations to reach a transformPrecision of  0.001 pixels
 *
 */
#define TRANSFORM_MAX_ITERATIONCOUNT      500


static void  p_save_perCoords_as_array_value(GapPerspectiveTransCoords *perCoords, gdouble precision);


/* ------------------------------------
 * p_save_perCoords_as_array_value
 * ------------------------------------
 * save perspective coords in the internal array values of perCoords.
 * the number of saved coordinate sets is limited to MAX_ITER_ATTEMPTS_PERSPECTIVE
 * in case th limit is reached, the value with 
 */
static void
p_save_perCoords_as_array_value(GapPerspectiveTransCoords *perCoords, gdouble precision)
{
  gint ida;
  gdouble maxPrecision;
  
  ida = MAX(perCoords->numberOfArrayValues, 0);
  maxPrecision = 0.0;
  if(ida < MAX_ITER_ATTEMPTS_PERSPECTIVE)
  {
    /* as long as there are free array elements
     * save coordinates as new arraylement 
     */
    perCoords->numberOfArrayValues++;
  }
  else
  {
     gint idx;
     
     /* the array is already full, therefore
      * find index ida to overwrite the array value with worst (greatest) precision 
      */
     for(idx = 0; idx < MAX_ITER_ATTEMPTS_PERSPECTIVE; idx++)
     {
       if(perCoords->aprecision[idx] > maxPrecision)
       {
         ida = idx;
         maxPrecision = perCoords->aprecision[idx];
       }
     }
     
     if (precision > maxPrecision)
     {
       /* all recorded values so far, are better than the new precision
        * therefore ignore the new coordinates.
        */
       return;
     }
  }
  
  perCoords->aprecision[ida] = precision;
  perCoords->ax0[ida] = perCoords->x0;    
  perCoords->ay0[ida] = perCoords->y0;
  perCoords->ax1[ida] = perCoords->x1; 
  perCoords->ay1[ida] = perCoords->y1;
  perCoords->ax2[ida] = perCoords->x2;
  perCoords->ay2[ida] = perCoords->y2;
  perCoords->ax3[ida] = perCoords->x3;
  perCoords->ay3[ida] = perCoords->y3;

  if(gap_debug)
  {
    printf("p_save_perCoords_as_array_value (SAVE precision:%f at array index IDA:%d) width:%d height:%d\n"
           "    p0: %f %f\n"
           "    p1: %f %f\n"
           "    p2: %f %f\n"
           "    p3: %f %f\n"
      ,(float)precision
      ,(int)ida
      ,(int)perCoords->width
      ,(int)perCoords->height
      ,(float)perCoords->x0
      ,(float)perCoords->y0
      ,(float)perCoords->x1
      ,(float)perCoords->y1
      ,(float)perCoords->x2
      ,(float)perCoords->y2
      ,(float)perCoords->x3
      ,(float)perCoords->y3
      );
  }  
}    /* end p_save_perCoords_as_array_value */

/* ------------------------------------
 * gap_geo_copy_src_to_dst_coords
 * ------------------------------------
 */
void
gap_geo_copy_src_to_dst_coords(GapPixelCoords *srcCoords, GapPixelCoords *dstCoords)
{
  dstCoords->valid = srcCoords->valid;
  dstCoords->avgColorDiff = srcCoords->avgColorDiff;
  dstCoords->px = srcCoords->px;
  dstCoords->py = srcCoords->py;
}



/* ----------------------------
 * gap_geo_calculateSqrDistX2Y2
 * ----------------------------
 * returns the square distance between coordA and point px/py
 *
 */
gdouble
gap_geo_calculateSqrDistX2Y2(GapPixelCoords *coordA, gdouble px, gdouble py)
{
  gdouble lenX;
  gdouble lenY;
  gdouble sqrDist;
  
  lenX = (gdouble)coordA->px - px;
  lenY = (gdouble)coordA->py - py;
  
  sqrDist = (lenX * lenX) + (lenY * lenY);
  return sqrDist;
}  /* end gap_geo_calculateSqrDistX2Y2 */

/* ----------------------------
 * gap_geo_calculateSqrDist
 * ----------------------------
 * returns the square distance between coordA and coordB
 *
 */
gdouble
gap_geo_calculateSqrDist(GapPixelCoords *coordA, GapPixelCoords *coordB)
{
  gdouble lenX;
  gdouble lenY;
  gdouble sqrDist;
  
  lenX = coordA->px - coordB->px;
  lenY = coordA->py - coordB->py;
  
  sqrDist = (lenX * lenX) + (lenY * lenY);
  return sqrDist;

} /* end gap_geo_calculateSqrDist */

gdouble
gap_geo_calculateDist(GapPixelCoords *coordA, GapPixelCoords *coordB)
{
  gdouble sqrDist;
  gdouble dist;
  
  sqrDist = gap_geo_calculateSqrDist(coordA, coordB);
  dist = sqrt(sqrDist);
  
  return (dist);
  
} /* end gap_geo_calculateDist */


/* -----------------------------------------
 * gap_geo_line_description_from_2Points
 * -----------------------------------------
 * Ermittlung der konstanten a,b,c der Geradengleichung aus 2 punkten x1,y1  und x2,y2
 * Spezialfaelle
 *   Falls a=0 ist, verlauuft die Gerade parallel zur x-Achse, und falls b=0 ist, parallel zur y-Achse.
 *   Falls c=0 ist, handelt es sich bei der Gerade um eine Ursprungsgerade.
 *   Falls c=1 ist, liegt die Geradengleichung in Achsenabschnittsform vor; die Achsenschnittpunkte sind dann (1/a,0) und (0,1/b).
 *
 * a = y1 - y2;
 * b = x2 - x1; 
 * c = (x2 * y1) - (x1 * y2);
 */
void
gap_geo_line_description_from_2Points(gdouble x1, gdouble y1, gdouble x2, gdouble y2, GapLineDescriptionConsts *lineConst)
{

  lineConst->a = y1 - y2;
  lineConst->b = x2 - x1;
  lineConst->c = (x2 * y1) - (x1 * y2);

}  /* end gap_geo_line_description_from_2Points */

void
gap_geo_line_description_from_2GapPixelCoords(GapPixelCoords *p1, GapPixelCoords *p2, GapLineDescriptionConsts *lineConst)
{
  gap_geo_line_description_from_2Points(p1->px, p1->py, p2->px, p2->py, lineConst);
}  /* end gap_geo_line_description_from_2Points */


/* -----------------------------------------
 * gap_geo_line_getX
 * -----------------------------------------
 * return X coordiate at specified y coordinate.
 *     (a * x) + (b * y) = c    // line description
 *      x = (c - (b * y)) / a
 * 
 * Does not work in case the line is parallel to the x axis (a == 0)
 */
gdouble gap_geo_line_getX(GapLineDescriptionConsts *lineConst, gdouble y)
{
  gdouble x;
  
  x = 0;
  if (lineConst->a != 0.0)
  {
    x = (lineConst->c - (lineConst->b * y)) / lineConst->a;
  }
  return (x);

}  /* end gap_geo_line_getX */


/* -----------------------------------------
 * gap_geo_line_getY
 * -----------------------------------------
 * return Y coordiate at specified x coordinate.
 *     (a * x) + (b * y) = c    // line description
 *     y = (c - (a * x)) / b
 * 
 * Does not work in case the line is parallel to the y axis (b == 0)
 */
gdouble gap_geo_line_getY(GapLineDescriptionConsts *lineConst, gdouble x)
{
  gdouble y;
  
  y = 0;
  if (lineConst->b != 0.0)
  {
    y = (lineConst->c - (lineConst->a * x)) / lineConst->b;
  }
  return (y);

}  /* end gap_geo_line_getY */


/* -----------------------------------------
 * gap_geo_line_intersection
 * -----------------------------------------
 * Ermittlung des Schnittpunktes xs,ys zweier geraden
 *
 * denominator = ((a1 * b2) - (a2 * b1));
 *
 * xs = ((c1 * b2) - (c2 * b1)) / denominator;
 * ys = ((a1 * c2) - (a2 * c1)) / denominator;
 *
 */
void
gap_geo_line_intersection(GapLineDescriptionConsts *line1, GapLineDescriptionConsts *line2, GapDoubleCoords *interPt)
{
  gdouble denominator = ((line1->a * line2->b) - (line2->a * line1->b));
  
  if (denominator == 0)
  {
    interPt->valid = FALSE;
  }
  else
  {
    interPt->valid = TRUE;
    interPt->x = ((line1->c * line2->b) - (line2->c * line1->b)) / denominator;
    interPt->y = ((line1->a * line2->c) - (line2->a * line1->c)) / denominator;
  }
  
}  /* end  gap_geo_line_intersection */

/**
 * gap_matrix3_mult:
 * @matrix1: The first input matrix.
 * @matrix2: The second input matrix which will be overwritten by the result.
 *
 * Multiplies two matrices and puts the result into the second one.
 */
void
gap_matrix3_mult (const GimpMatrix3 *matrix1,
                   GimpMatrix3       *matrix2)
{
  gint         i, j;
  GimpMatrix3  tmp;
  gdouble      t1, t2, t3;

  for (i = 0; i < 3; i++)
    {
      t1 = matrix1->coeff[i][0];
      t2 = matrix1->coeff[i][1];
      t3 = matrix1->coeff[i][2];

      for (j = 0; j < 3; j++)
        {
          tmp.coeff[i][j]  = t1 * matrix2->coeff[0][j];
          tmp.coeff[i][j] += t2 * matrix2->coeff[1][j];
          tmp.coeff[i][j] += t3 * matrix2->coeff[2][j];
        }
    }

  *matrix2 = tmp;
}

/**
 * gap_matrix3_translate:
 * @matrix: The matrix that is to be translated.
 * @x: Translation in X direction.
 * @y: Translation in Y direction.
 *
 * Translates the matrix by x and y.
 */
void
gap_matrix3_translate (GimpMatrix3 *matrix,
                        gdouble      x,
                        gdouble      y)
{
  gdouble g, h, i;

  g = matrix->coeff[2][0];
  h = matrix->coeff[2][1];
  i = matrix->coeff[2][2];

  matrix->coeff[0][0] += x * g;
  matrix->coeff[0][1] += x * h;
  matrix->coeff[0][2] += x * i;
  matrix->coeff[1][0] += y * g;
  matrix->coeff[1][1] += y * h;
  matrix->coeff[1][2] += y * i;
}

/**
 * gap_matrix3_scale:
 * @matrix: The matrix that is to be scaled.
 * @x: X scale factor.
 * @y: Y scale factor.
 *
 * Scales the matrix by x and y
 */
void
gap_matrix3_scale (GimpMatrix3 *matrix,
                    gdouble      x,
                    gdouble      y)
{
  matrix->coeff[0][0] *= x;
  matrix->coeff[0][1] *= x;
  matrix->coeff[0][2] *= x;

  matrix->coeff[1][0] *= y;
  matrix->coeff[1][1] *= y;
  matrix->coeff[1][2] *= y;
}

/**
 * gap_matrix3_identity:
 * @matrix: A matrix.
 *
 * Sets the matrix to the identity matrix.
 */
void
gap_matrix3_identity (GimpMatrix3 *matrix)
{
  static const GimpMatrix3 identity = { { { 1.0, 0.0, 0.0 },
                                          { 0.0, 1.0, 0.0 },
                                          { 0.0, 0.0, 1.0 } } };

  *matrix = identity;
}

void
gap_transform_matrix_perspective (GimpMatrix3 *matrix,
                                   gint         x,
                                   gint         y,
                                   gint         width,
                                   gint         height,
                                   gdouble      t_x1,
                                   gdouble      t_y1,
                                   gdouble      t_x2,
                                   gdouble      t_y2,
                                   gdouble      t_x3,
                                   gdouble      t_y3,
                                   gdouble      t_x4,
                                   gdouble      t_y4)
{
  GimpMatrix3 trafo;
  gdouble     scalex;
  gdouble     scaley;

  g_return_if_fail (matrix != NULL);

  scalex = scaley = 1.0;

  if (width > 0)
    scalex = 1.0 / (gdouble) width;

  if (height > 0)
    scaley = 1.0 / (gdouble) height;

  gap_matrix3_translate (matrix, -x, -y);
  gap_matrix3_scale     (matrix, scalex, scaley);

  /* Determine the perspective transform that maps from
   * the unit cube to the transformed coordinates
   */
  {
    gdouble dx1, dx2, dx3, dy1, dy2, dy3;

    dx1 = t_x2 - t_x4;
    dx2 = t_x3 - t_x4;
    dx3 = t_x1 - t_x2 + t_x4 - t_x3;

    dy1 = t_y2 - t_y4;
    dy2 = t_y3 - t_y4;
    dy3 = t_y1 - t_y2 + t_y4 - t_y3;

    /*  Is the mapping affine?  */
    if ((dx3 == 0.0) && (dy3 == 0.0))
      {
        trafo.coeff[0][0] = t_x2 - t_x1;
        trafo.coeff[0][1] = t_x4 - t_x2;
        trafo.coeff[0][2] = t_x1;
        trafo.coeff[1][0] = t_y2 - t_y1;
        trafo.coeff[1][1] = t_y4 - t_y2;
        trafo.coeff[1][2] = t_y1;
        trafo.coeff[2][0] = 0.0;
        trafo.coeff[2][1] = 0.0;
      }
    else
      {
        gdouble det1, det2;

        det1 = dx3 * dy2 - dy3 * dx2;
        det2 = dx1 * dy2 - dy1 * dx2;

        trafo.coeff[2][0] = (det2 == 0.0) ? 1.0 : det1 / det2;

        det1 = dx1 * dy3 - dy1 * dx3;

        trafo.coeff[2][1] = (det2 == 0.0) ? 1.0 : det1 / det2;

        trafo.coeff[0][0] = t_x2 - t_x1 + trafo.coeff[2][0] * t_x2;
        trafo.coeff[0][1] = t_x3 - t_x1 + trafo.coeff[2][1] * t_x3;
        trafo.coeff[0][2] = t_x1;

        trafo.coeff[1][0] = t_y2 - t_y1 + trafo.coeff[2][0] * t_y2;
        trafo.coeff[1][1] = t_y3 - t_y1 + trafo.coeff[2][1] * t_y3;
        trafo.coeff[1][2] = t_y1;
      }

    trafo.coeff[2][2] = 1.0;
  }

  gap_matrix3_mult (&trafo, matrix);
}





/* ----------------------------------------------------
 * gap_geo_assemble_perspective_transformation_matrix
 * ----------------------------------------------------
 *
 */
void
gap_geo_assemble_perspective_transformation_matrix(GimpMatrix3  *matrix, GapPerspectiveTransCoords *perCoords, gint32 activeDrawableId)
{
   gint x, y;
   gint off_x, off_y;
   
   x = 0;
   y = 0;
   
   if(activeDrawableId >= 0)
   {
     gimp_drawable_offsets(activeDrawableId, &off_x, &off_y);
     x += off_x;
     y += off_y;
   }

   
   /* Assemble the transformation matrix */
   gap_matrix3_identity (matrix);
   gap_transform_matrix_perspective (matrix
                                      , x, y
                                      , perCoords->width, perCoords->height
                                      , perCoords->x0, perCoords->y0
                                      , perCoords->x1, perCoords->y1
                                      , perCoords->x2, perCoords->y2
                                      , perCoords->x3, perCoords->y3
                                      );   
}  /* end gap_geo_assemble_perspective_transformation_matrix */


/* ---------------------------------------
 * gap_geo_DuplicateAlignCoords
 * ---------------------------------------
 */
void
gap_geo_DuplicateAlignCoords(GapAlignCoords *alignCoords, GapAlignCoords *alignCoordsDup)
{
  gint idx;
  
  for(idx=0; idx < GAP_ALIGN_COORDS_MAX; idx++)
  {
    gap_geo_copy_src_to_dst_coords(&alignCoords->currCoords[idx], &alignCoordsDup->currCoords[idx]);
    alignCoordsDup->currCoords[idx].status = alignCoords->currCoords[idx].status;

    gap_geo_copy_src_to_dst_coords(&alignCoords->startCoords[idx], &alignCoordsDup->startCoords[idx]);
    alignCoordsDup->startCoords[idx].status = alignCoords->startCoords[idx].status;

    alignCoordsDup->trkPtr[idx] = alignCoords->trkPtr[idx];
    alignCoordsDup->refPtr[idx] = alignCoords->refPtr[idx];

  }
}  /* end gap_geo_DuplicateAlignCoords */

/* ---------------------------------------
 * gap_geo_pick_corners_from_align_coords
 * ---------------------------------------
 * This procedure picks alignCoords near the 4 corners
 * and assigns the pointer refPtr and trkPtr in the GapAlignCoords structure
 * in the following sorted order:
 *  Ptr[0] shall point to coord nearest to upper left corner
 *  Ptr[1] shall point to coord nearest to upper right corner
 *  Ptr[2] shall point to coord nearest to lower left corner
 *  Ptr[3] shall point to coord nearest to lower right corner
 *
 * returns TRUE in case of success
 */
gboolean
gap_geo_pick_corners_from_align_coords(GapAlignCoords *alignCoords, gdouble width, gdouble height)
{
  gint    idn;
  gint    idx;
  gint    idcPick;
  gdouble   minSqDist;
  gdouble   minSqDistCorner[4];
  gboolean  cornerSelected[4];
  gint      cornerSelectCount;
  
  for(idx=0; idx < GAP_ALIGN_COORDS_MAX; idx++)
  {
    alignCoords->refPtr[idx] = NULL;
    alignCoords->trkPtr[idx] = NULL;
    alignCoords->startCoords[idx].status = -1;  /* indicates unusable pair */
    
    if(gap_debug)
    {
      printf("PERc: idx[%d] start px:%d py:%d valid:%d cur px:%d py:%d valid:%d\n"
         ,(int)idx
         ,(int)alignCoords->startCoords[idx].px
         ,(int)alignCoords->startCoords[idx].py
         ,(int)alignCoords->startCoords[idx].valid
         ,(int)alignCoords->currCoords[idx].px
         ,(int)alignCoords->currCoords[idx].py
         ,(int)alignCoords->currCoords[idx].valid
         );
    }

    
    if((alignCoords->startCoords[idx].valid)
    && (alignCoords->currCoords[idx].valid))
    {
      alignCoords->startCoords[idx].status = 0;  /* indicates selectable pair */
    }
    else
    {
      if(gap_debug)
      {
        printf("PER: ret(1)\n");
      }
      return (FALSE);  /* stop in case invalid points are detected */
    }
  }
  

  /* alignCoords->refPtr and alignCoords->trkPtr are assigned (picked) in sorted order:
   * Ptr[0] shall point to coord nearest to upper left corner
   * Ptr[1] shall point to coord nearest to upper right corner
   * Ptr[2] shall point to coord nearest to lower left corner
   * Ptr[3] shall point to coord nearest to lower right corner
   */


  /* pick 4 coordinate near ideally near to the 4 corners in loop for idn = 0 to 3 */

  cornerSelected[0] = FALSE;
  cornerSelected[1] = FALSE;
  cornerSelected[2] = FALSE;
  cornerSelected[3] = FALSE;
  cornerSelectCount = 0;

  
  for(idn = 0; idn < 4; idn++)
  {
    minSqDist = (width + height) * (width + height);
    minSqDistCorner[0] = minSqDist;
    minSqDistCorner[1] = minSqDist;
    minSqDistCorner[2] = minSqDist;
    minSqDistCorner[3] = minSqDist;
  
    idcPick = -1;
    for(idx=0; idx < GAP_ALIGN_COORDS_MAX; idx++)
    {
      gdouble sqDist[4];
      gint idc;

      if(alignCoords->startCoords[idx].status != 0)
      {
        continue;  /* Skip already selected coorinates */
      }

      /* square distance to all 4 corners */
      sqDist[0] = gap_geo_calculateSqrDistX2Y2(&alignCoords->startCoords[idx], 0.0, 0.0);
      sqDist[1] = gap_geo_calculateSqrDistX2Y2(&alignCoords->startCoords[idx], (gdouble)width, 0.0);
      sqDist[2] = gap_geo_calculateSqrDistX2Y2(&alignCoords->startCoords[idx], 0.0, (gdouble)height);
      sqDist[3] = gap_geo_calculateSqrDistX2Y2(&alignCoords->startCoords[idx], (gdouble)width, (gdouble)height);
    
      for(idc = 0; idc < 4; idc++)
      {
        if (cornerSelected[idc] == TRUE)
        {
          continue; /* skip already picked corners */
        }
        if (sqDist[idc] < minSqDistCorner[idc])
        {
          minSqDistCorner[idc] = sqDist[idc];
          if (minSqDistCorner[idc] < minSqDist)
          {
            minSqDist = minSqDistCorner[idc];
            idcPick = idc;
            alignCoords->refPtr[idc] = &alignCoords->startCoords[idx];
            alignCoords->trkPtr[idc] = &alignCoords->currCoords[idx];
          }
        }
      }
    }      /* end for idx loop over all available coordinates */
    
    if (idcPick >= 0)
    {
      cornerSelected[idcPick] = TRUE;
      cornerSelectCount++;
      alignCoords->refPtr[idcPick]->status = 1; /* mark this coord as already selected */
    }
  }
  
  if (cornerSelectCount != 4)
  {
    return (FALSE);
  }
  
  for(idx=0; idx < GAP_ALIGN_COORDS_MAX; idx++)
  {
    if((alignCoords->refPtr[idx] == NULL) 
    || (alignCoords->trkPtr[idx] == NULL))
    {
      /* stop in case some of the coordinate pointers could not be set to valid coordinates. */
      if(gap_debug)
      {
        printf("PERc: ret(2) idx:%d refPtr:%ld trkPtr:%ld\n"
          ,(int)idx
          ,(long)alignCoords->refPtr[idx]
          ,(long)alignCoords->trkPtr[idx]
          );
      }
      return (FALSE);
    }
  }  
  
  
  return (TRUE);

}  /* end gap_geo_pick_corners_from_align_coords */


/* --------------------------------------------------
 * gap_geo_perspective_trans_coords_from_align_coords
 * --------------------------------------------------
 * calculate GapPerspectiveTransCoords from given align coordinates.
 * The align coordinates must contain 4 valid start coordinates
 *  (refrence points) 
 * and 4 valid currentPoints in the activeDrawable (as typical recorded by detail tracking).
 * This procedure calculates new corner points
 * for a gimp perspective transformation
 * that will transform the activeDrawable in a way that all
 * 4 current coordinates will be placed on the 4 corresponding reference start Points.
 * (typical usage is to compensate unwanted camera movement and rotations)
 *
 *
 *    dc0x: -13               dc1x:  -14
 *    dc0y: -12               dc1y:  -10
 *       +-------------------+
 *       |                   |
 *       |                   |
 *       |                   |
 *       |                   |
 *       +-------------------+
 *   dc2x: -44                dc3x: -43
 *   dc2y: -5                 dc3y: -6
 *
 */
gboolean
gap_geo_perspective_trans_coords_from_align_coords(gint32 activeDrawableId, GapAlignCoords *alignCoords, GapPerspectiveTransCoords *perCoords)
{
  gint    idx;
  
  GapLineDescriptionConsts upperBorderLine;
  GapLineDescriptionConsts lowerBorderLine;
  GapLineDescriptionConsts leftBorderLine;
  GapLineDescriptionConsts rightBorderLine;
  GapLineDescriptionConsts refLine;
  GapLineDescriptionConsts trkLine;
  GapLineDescriptionConsts extLine;
  
  GapDoubleCoords refInterPt;
  GapDoubleCoords trkInterPt;
  
  gdouble d0x, d1x, d2x, d3x;      /* horizontal delta offsets at border intersection */ 
  gdouble d0y, d1y, d2y, d3y;      /* vertical   delta offsets at border intersection */
  
  /* extended reference lines interscetion with border lines */
  gdouble riTop0x,   riTop1x;
  gdouble riBot2x,   riBot3x;
  gdouble riLeft0y,  riLeft1y;
  gdouble riRight2y, riRight3y;
  
  gdouble dc0x, dc1x, dc2x, dc3x;  /* horizontal delta offsets 4 corners */ 
  gdouble dc0y, dc1y, dc2y, dc3y;  /* vertical   delta offsets 4 corners */ 
  
  gdouble width;
  gdouble height;
  
  if(activeDrawableId < 0)
  {
    width = perCoords->width;
    height = perCoords->height;
  }
  else
  {
    width = (gdouble)gimp_drawable_width(activeDrawableId);
    height = (gdouble)gimp_drawable_height(activeDrawableId);
    perCoords->width = width;
    perCoords->height = height;
  }

  if(gap_debug)
  {
        printf("PER: activeDrawableId: %d  width:%f height:%f\n"
           , (int)activeDrawableId
           , (float)width
           , (float)height
           );
  }


  /* alignCoords->refPtr and alignCoords->trkPtr are assigned (picked) in sorted order:
   * Ptr[0] shall point to coord nearest to upper left corner
   * Ptr[1] shall point to coord nearest to upper right corner
   * Ptr[2] shall point to coord nearest to lower left corner
   * Ptr[3] shall point to coord nearest to lower right corner
   */
  if(TRUE != gap_geo_pick_corners_from_align_coords(alignCoords, width, height))
  {
    return (FALSE);  /* stop in case picking coords near the corners failed */
  }

  perCoords->numberOfArrayValues = 0;

  /* The initial new x/y coordinates of upper-left corner */
  perCoords->x0 = 0;    
  perCoords->y0 = 0;
      
  /* The initial new x/y coordinates of upper-right corner */
  perCoords->x1 = width; 
  perCoords->y1 = 0;
      
  /* The initial new x/y coordinate of lower-left corner */
  perCoords->x2 = 0;
  perCoords->y2 = height;
    
  /* The initial new x/y coordinate of lower-right corner */
  perCoords->x3 = width;
  perCoords->y3 = height;

  
  /* ---- the following not correct working code part may be is skipped complete
   * ---- and use only the iterative Calculation workaround...
   * ---- ... but it is now executed to have better inital values
   * ----     that leads to less iterations and typical faster execution.
   * ---------------------------------------------------------------------
   */
  // goto iterativeCalculation;



  
  /* calculate line description for the drawable border lines */
  gap_geo_line_description_from_2Points(0.0,     0.0        /* x1,y1 */
                                       ,width, 0.0        /* x2,y2 */ 
                                       ,&upperBorderLine);
  gap_geo_line_description_from_2Points(0.0,     height   /* x1,y1 */
                                       ,width, height   /* x2,y2 */
                                       ,&lowerBorderLine);
  gap_geo_line_description_from_2Points(0.0,     0.0        /* x1,y1 */
                                       ,0.0,     height   /* x2,y2 */ 
                                       ,&leftBorderLine);
  gap_geo_line_description_from_2Points(width, 0.0        /* x1,y1 */
                                       ,width, height   /* x2,y2 */
                                       ,&rightBorderLine);
  
  /* lines from point [0] to point [2] on left side shall have vertical orientation */
  gap_geo_line_description_from_2GapPixelCoords(alignCoords->refPtr[0], alignCoords->refPtr[2], &refLine);
  gap_geo_line_description_from_2GapPixelCoords(alignCoords->trkPtr[0], alignCoords->trkPtr[2], &trkLine);

  gap_geo_line_intersection(&upperBorderLine, &refLine,  &refInterPt);
  gap_geo_line_intersection(&upperBorderLine, &trkLine,  &trkInterPt);
  if((refInterPt.valid != TRUE) || (trkInterPt.valid != TRUE))
  {
    if(gap_debug)
    {
      printf("PER: ret(3)\n");
    }
    return (FALSE);
  }
  d0x = refInterPt.x - trkInterPt.x;
  riTop0x = refInterPt.x;
  
  gap_geo_line_intersection(&lowerBorderLine, &refLine,  &refInterPt);
  gap_geo_line_intersection(&lowerBorderLine, &trkLine,  &trkInterPt);
  if((refInterPt.valid != TRUE) || (trkInterPt.valid != TRUE))
  {
    if(gap_debug)
    {
      printf("PER: ret(4)\n");
    }
    return (FALSE);
  }
  d2x = refInterPt.x - trkInterPt.x;
  riBot2x = refInterPt.x;

  /* lines from point [1] to point [3] on right side shall have vertical orientation */
  gap_geo_line_description_from_2GapPixelCoords(alignCoords->refPtr[1], alignCoords->refPtr[3], &refLine);
  gap_geo_line_description_from_2GapPixelCoords(alignCoords->trkPtr[1], alignCoords->trkPtr[3], &trkLine);

  gap_geo_line_intersection(&upperBorderLine, &refLine,  &refInterPt);
  gap_geo_line_intersection(&upperBorderLine, &trkLine,  &trkInterPt);
  if((refInterPt.valid != TRUE) || (trkInterPt.valid != TRUE))
  {
    if(gap_debug)
    {
      printf("PER: ret(5)\n");
    }
    return (FALSE);
  }
  d1x = refInterPt.x - trkInterPt.x;
  riTop1x = refInterPt.x;

  gap_geo_line_intersection(&lowerBorderLine, &refLine,  &refInterPt);
  gap_geo_line_intersection(&lowerBorderLine, &trkLine,  &trkInterPt);
  if((refInterPt.valid != TRUE) || (trkInterPt.valid != TRUE))
  {
    if(gap_debug)
    {
      printf("PER: ret(6)\n");
    }
    return (FALSE);
  }
  d3x = refInterPt.x - trkInterPt.x;
  riBot3x = refInterPt.x;


  /* lines from point [0] to point [1] on upper side shall have horizontal orientation */
  gap_geo_line_description_from_2GapPixelCoords(alignCoords->refPtr[0], alignCoords->refPtr[1], &refLine);
  gap_geo_line_description_from_2GapPixelCoords(alignCoords->trkPtr[0], alignCoords->trkPtr[1], &trkLine);

  gap_geo_line_intersection(&leftBorderLine, &refLine,  &refInterPt);
  gap_geo_line_intersection(&leftBorderLine, &trkLine,  &trkInterPt);
  if((refInterPt.valid != TRUE) || (trkInterPt.valid != TRUE))
  {
    if(gap_debug)
    {
      printf("PER: ret(7)\n");
    }
    return (FALSE);
  }
  d0y = refInterPt.y - trkInterPt.y;
  riLeft0y = refInterPt.y;

  gap_geo_line_intersection(&rightBorderLine, &refLine,  &refInterPt);
  gap_geo_line_intersection(&rightBorderLine, &trkLine,  &trkInterPt);
  if((refInterPt.valid != TRUE) || (trkInterPt.valid != TRUE))
  {
    if(gap_debug)
    {
      printf("PER: ret(8)\n");
    }
    return (FALSE);
  }
  d1y = refInterPt.y - trkInterPt.y;
  riRight2y = refInterPt.y;


  /* lines from point [2] to point [3] on lower side shall have horizontal orientation */
  gap_geo_line_description_from_2GapPixelCoords(alignCoords->refPtr[2], alignCoords->refPtr[3], &refLine);
  gap_geo_line_description_from_2GapPixelCoords(alignCoords->trkPtr[2], alignCoords->trkPtr[3], &trkLine);

  gap_geo_line_intersection(&leftBorderLine, &refLine,  &refInterPt);
  gap_geo_line_intersection(&leftBorderLine, &trkLine,  &trkInterPt);
  if((refInterPt.valid != TRUE) || (trkInterPt.valid != TRUE))
  {
    if(gap_debug)
    {
      printf("PER: ret(9)\n");
    }
    return (FALSE);
  }
  d2y = refInterPt.y - trkInterPt.y;
  riLeft1y = refInterPt.y;

  gap_geo_line_intersection(&rightBorderLine, &refLine,  &refInterPt);
  gap_geo_line_intersection(&rightBorderLine, &trkLine,  &trkInterPt);
  if((refInterPt.valid != TRUE) || (trkInterPt.valid != TRUE))
  {
    if(gap_debug)
    {
      printf("PER: ret(10)\n");
    }
    return (FALSE);
  }
  d3y = refInterPt.y - trkInterPt.y;
  riRight3y = refInterPt.y;


  /* delta distances at border intersection point are now extrapolated to the corner points */
  gap_geo_line_description_from_2Points(riTop0x, d0x, riTop1x, d1x, &extLine);
  dc0x = gap_geo_line_getY(&extLine, 0);
  dc1x = gap_geo_line_getY(&extLine, width);

  gap_geo_line_description_from_2Points(riBot2x, d2x, riBot3x, d3x, &extLine);
  dc2x = gap_geo_line_getY(&extLine, 0);
  dc3x = gap_geo_line_getY(&extLine, width);
  
  gap_geo_line_description_from_2Points(riLeft0y, d0y, riLeft1y, d2y, &extLine);
  dc0y = gap_geo_line_getY(&extLine, 0);
  dc2y = gap_geo_line_getY(&extLine, height);

  gap_geo_line_description_from_2Points(riRight2y, d1y, riRight3y, d3y, &extLine);
  dc1y = gap_geo_line_getY(&extLine, 0);
  dc3y = gap_geo_line_getY(&extLine, height);
  
  
  
  
  
  /* The new x/y coordinates of upper-left corner */
  perCoords->x0 = dc0x;    
  perCoords->y0 = dc0y;
  
  /* The new x/y coordinates of upper-right corner */
  perCoords->x1 = width + dc1x; 
  perCoords->y1 = dc1y;
  
  /* The new x/y coordinate of lower-left corner */
  perCoords->x2 = dc2x;
  perCoords->y2 = height + dc2y;

  /* The new x/y coordinate of lower-right corner */
  perCoords->x3 = width + dc3x;
  perCoords->y3 = height + dc3y;
  

  if(gap_debug)
  {
    printf("gap_geo_perspective_trans_coords_from_align_coords: activeDrawableId:%d width:%d height:%d\n"
           "    p0: %f %f\n"
           "    p1: %f %f\n"
           "    p2: %f %f\n"
           "    p3: %f %f\n"
           "    d0: %f %f\n"
           "    d1: %f %f\n"
           "    d2: %f %f\n"
           "    d3: %f %f\n"
           "    riTop0x: %f riTop1x: %f\n"
           "    riBot2x: %f riBot3x: %f\n"
           "    riLeft0y: %f riLeft1y: %f\n"
           "    riRight2y: %f riRight3y: %f\n"
           
           "    dc0: %f %f\n"
           "    dc1: %f %f\n"
           "    dc2: %f %f\n"
           "    dc3: %f %f\n"
      ,(int)activeDrawableId
      ,(int)perCoords->width
      ,(int)perCoords->height
      ,(float)perCoords->x0
      ,(float)perCoords->y0
      ,(float)perCoords->x1
      ,(float)perCoords->y1
      ,(float)perCoords->x2
      ,(float)perCoords->y2
      ,(float)perCoords->x3
      ,(float)perCoords->y3
      ,(float)d0x
      ,(float)d0y
      ,(float)d1x
      ,(float)d1y
      ,(float)d2x
      ,(float)d2y
      ,(float)d3x
      ,(float)d3y
      ,(float) riTop0x,   (float) riTop1x
      ,(float) riBot2x,   (float)riBot3x
      ,(float) riLeft0y,  (float)riLeft1y
      ,(float) riRight2y, (float)riRight3y
      ,(float)dc0x
      ,(float)dc0y
      ,(float)dc1x
      ,(float)dc1y
      ,(float)dc2x
      ,(float)dc2y
      ,(float)dc3x
      ,(float)dc3y
      );
  }

iterativeCalculation:

  /* perspective transformation iteratve corner_tuning 
   * Note that the calculation of 
   *   GapPerspectiveTransCoords *perCoords at this point
   * is not yet correct, but is used as 1st guess for the itarative part
   * that follows now to fix it via try and error attempts in a loop.
   */
  if(TRUE)
  {
    gint            iterationCount;
    gdouble         maxDifX;
    gdouble         maxDifY;
    gdouble         iterPrecision;
    gint            idxMaxDifX;
    gint            idxMaxDifY;
    
    GimpMatrix3     matrix;
    
      
    for(iterationCount = 0; iterationCount < TRANSFORM_MAX_ITERATIONCOUNT; iterationCount++)
    {
      gdouble         difX[4];
      gdouble         difY[4];
    
      if(gap_debug)
      {
        printf("gap_geo_perspective_trans_coords_from_align_coords (ITERATION-Attempt): iterationCount:%d width:%d height:%d\n"
             "    p0: %f %f\n"
             "    p1: %f %f\n"
             "    p2: %f %f\n"
             "    p3: %f %f\n"
          ,(int)iterationCount
          ,(int)perCoords->width
          ,(int)perCoords->height
          ,(float)perCoords->x0
          ,(float)perCoords->y0
          ,(float)perCoords->x1
          ,(float)perCoords->y1
          ,(float)perCoords->x2
          ,(float)perCoords->y2
          ,(float)perCoords->x3
          ,(float)perCoords->y3
          );
       }
    
      /* calculate the matrix from 4 displaced corners 
       * (thes ame way as GIMP does for perspective transformation tools) 
       */
      gap_geo_assemble_perspective_transformation_matrix(&matrix, perCoords, activeDrawableId);

      
      /* calculate transformed trace coordinates and difference 
       * to the expected corresponding refernce coordinates that should
       * ideally be equal (or differ in perecision below 1 pixel)
       */
      maxDifX = 0;
      maxDifY = 0;
      idxMaxDifX = 0;
      idxMaxDifY = 0;
      for(idx=0; idx < 4; idx++)
      {
        GapDoubleCoords inPoint;
        GapDoubleCoords outPoint;
        
        inPoint.x = (gdouble)alignCoords->trkPtr[idx]->px;
        inPoint.y = (gdouble)alignCoords->trkPtr[idx]->py;
        
        gimp_matrix3_transform_point (&matrix
	                              , inPoint.x     // gdouble x
	                              , inPoint.y     // gdouble y
	                              , &outPoint.x   // gdouble *newx
	                              , &outPoint.y   // gdouble *newy
	                              );

        
        difX[idx] = (gdouble)alignCoords->refPtr[idx]->px - outPoint.x;
        difY[idx] = (gdouble)alignCoords->refPtr[idx]->py - outPoint.y;
        
        if(fabs(difX[idx]) > fabs(maxDifX))
        {
           maxDifX = difX[idx];
           idxMaxDifX = idx;;
        }
    
        if(fabs(difY[idx]) > fabs(maxDifY))
        {
           maxDifY = difY[idx];
           idxMaxDifY = idx;;
        }
    
        if(gap_debug)
        {
          if (idx == 0)
          {
             printf("iterationCount:%d\n"
               , iterationCount
               );
          }
          printf("  difX[%d]: %3.4f difY[%d]: %3.4f  outPoint.x: %4.5f outPoint.y: %4.5f refPoint.x: %04d refPoint.y: %04d\n"
               , idx
               , (float)difX[idx]
               , idx
               , (float)difY[idx]
               , (float)outPoint.x
               , (float)outPoint.y
               , alignCoords->refPtr[idx]->px
               , alignCoords->refPtr[idx]->py
               );
          if (idx == 3)
          {
             printf("iterationCount:%d idxMaxDifX: %d idxMaxDifY: %d  maxDifX: %f maxDifY: %f\n"
               , iterationCount
               , (int)idxMaxDifX
               , (int)idxMaxDifY
               , (float)maxDifX
               , (float)maxDifY
               );
          }
        }
    
      
      }
      
      iterPrecision = MAX(fabs(maxDifX), fabs(maxDifY));

      if (iterPrecision < perCoords->transformPrecisionThreshold)
      {
        /* record potential matches (for fine tuning render attempts) */
        p_save_perCoords_as_array_value(perCoords, iterPrecision);
      }

      
      if (iterPrecision < perCoords->transformPrecision)
      {
        /* check if all 4 coordinates are sufficent matching now...
         * .. therfore escape from iterationCount loop
         */
        break;
      }
      else
      {
        /* in case of insufficient match iterate by stepsize 1/4 of measured difference 
         * at the corner nearest to the point where max diff occured and give it another try..
         */
        //if (fabs(maxDifX) >= fabs(maxDifY))
        {
          if(difX[0] == maxDifX) { perCoords->x0 += (difX[0] * 0.125); }
          if(difX[1] == maxDifX) { perCoords->x1 += (difX[1] * 0.125); }
          if(difX[2] == maxDifX) { perCoords->x2 += (difX[2] * 0.125); }
          if(difX[3] == maxDifX) { perCoords->x3 += (difX[3] * 0.125); }
        }
        //if (fabs(maxDifY) >= fabs(maxDifX))
        {
          if(difY[0] == maxDifY) { perCoords->y0 += (difY[0] * 0.125); }
          if(difY[1] == maxDifY) { perCoords->y1 += (difY[1] * 0.125); }
          if(difY[2] == maxDifY) { perCoords->y2 += (difY[2] * 0.125); }
          if(difY[3] == maxDifY) { perCoords->y3 += (difY[3] * 0.125); }
        }
        
      }
    
    }  /* end for(iterationCount ...) */
    
    if(gap_debug)
    {
      printf("gap_geo_perspective_trans_coords_from_align_coords (FINAL-Result iterationCount:%d iterPrecision:%f {< %f}) activeDrawableId:%d width:%d height:%d\n"
             "    p0: %f %f\n"
             "    p1: %f %f\n"
             "    p2: %f %f\n"
             "    p3: %f %f\n"
        ,(int)iterationCount
        ,(double)iterPrecision
        ,(double)perCoords->transformPrecision
        ,(int)activeDrawableId
        ,(int)perCoords->width
        ,(int)perCoords->height
        ,(float)perCoords->x0
        ,(float)perCoords->y0
        ,(float)perCoords->x1
        ,(float)perCoords->y1
        ,(float)perCoords->x2
        ,(float)perCoords->y2
        ,(float)perCoords->x3
        ,(float)perCoords->y3
        );
    }
    
    if (iterPrecision > 5)
    {
      printf("gap_geo_perspective_trans_coords_from_align_coords ITERATION FAILED iterPrecision:%f failure is more than 5 pixels\n"
            ,(double)iterPrecision
            );
            
      /* Reset per koordiantes to The initial new x/y coordinates of upper-left corner */
      perCoords->x0 = 0;    
      perCoords->y0 = 0;
      /* The initial new x/y coordinates of upper-right corner */
      perCoords->x1 = width; 
      perCoords->y1 = 0;
      /* The initial new x/y coordinate of lower-left corner */
      perCoords->x2 = 0;
      perCoords->y2 = height;
      /* The initial new x/y coordinate of lower-right corner */
      perCoords->x3 = width;
      perCoords->y3 = height;      
    }

  }
  
  
  return (TRUE);  
  
}  /* end gap_geo_perspective_trans_coords_from_align_coords */

