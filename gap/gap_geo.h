/*  gap_geo.h
 *    This module provides structures and procedures to handle 2d geometric stuff
 *    such as perspective transformation.
 *
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

#ifndef _GAP_GEO_H
#define _GAP_GEO_H


#include "config.h"



typedef struct GapLineDescriptionConsts
{
  gdouble  a;
  gdouble  b;
  gdouble  c;
} GapLineDescriptionConsts;

typedef struct GapDoubleCoords
{
  gboolean  valid;
  gdouble  x;
  gdouble  y;
} GapDoubleCoords;

typedef struct GapPixelCoords
{
  gboolean  valid;
  gint32    px;
  gint32    py;
  gdouble   avgColorDiff;    // 0 = best quality, 1 = worst quality
  gint      status;
} GapPixelCoords;

#define GAP_ALIGN_COORDS_MAX 4

typedef struct GapAlignCoords
{
  GapPixelCoords  currCoords[GAP_ALIGN_COORDS_MAX];      /* upto 4 coords in current frame */
  GapPixelCoords  startCoords[GAP_ALIGN_COORDS_MAX];     /* upto 4 coords of first processed (reference) frame */
  GapPixelCoords  untunedCoords[GAP_ALIGN_COORDS_MAX];   /* upto 4 untuned coords in current frame */

  /* pointer sets to pick 4 coordinates correspoinding to coordinates near the 4 corners */
  GapPixelCoords  *trkPtr[4];   /* upto 4 coords in current frame  currCoords[4]; */
  GapPixelCoords  *refPtr[4];   /* upto 4 coords of first processed (reference) frame  startCoords[4]; */
} GapAlignCoords;

#define MAX_ATTEMPTS_PERSPECTIVE          309  //109
#define MAX_ITER_ATTEMPTS_PERSPECTIVE     300  //100

/*
 * GAP_GEO_TRANSFORM_PRECISION_THRSHOLD is used for fine tuning purpose 
 *   (save perspective coords of iterations that are better than this threshold for finetuning)
 */
//#define GAP_GEO_TRANSFORM_PRECISION_THRSHOLD      0.25
//#define GAP_GEO_TRANSFORM_PRECISION               0.1

//#define GAP_GEO_TRANSFORM_PRECISION_THRSHOLD      0.12
//#define GAP_GEO_TRANSFORM_PRECISION               0.005

//#define GAP_GEO_TRANSFORM_PRECISION_THRSHOLD      0.08
//#define GAP_GEO_TRANSFORM_PRECISION               0.001

//#define GAP_GEO_TRANSFORM_PRECISION_THRSHOLD      0.001
//#define GAP_GEO_TRANSFORM_PRECISION               0.00025


//#define GAP_GEO_TRANSFORM_PRECISION_THRSHOLD      0.25
//#define GAP_GEO_TRANSFORM_PRECISION               0.001

#define GAP_GEO_TRANSFORM_PRECISION_THRSHOLD      1.4
#define GAP_GEO_TRANSFORM_PRECISION               0.2



typedef struct GapPerspectiveTransCoords
{
  gdouble  width;
  gdouble  height;
  
  /* The new x/y coordinates of upper-left corner */
  gdouble  x0;    
  gdouble  y0;
    
  /* The new x/y coordinates of upper-right corner */
  gdouble  x1; 
  gdouble  y1;
    
  /* The new x/y coordinate of lower-left corner */
  gdouble  x2;
  gdouble  y2;
  
  /* The new x/y coordinate of lower-right corner */
  gdouble  x3;
  gdouble  y3;



  /* array for some more potential perspective transformation attempts
   * typically with nearly the same values slightly different in precision.
   * (used for fine tuning via probe rendering in the exact aligner)
   */
  gint     numberOfArrayValues;
  gdouble  aprecision[MAX_ATTEMPTS_PERSPECTIVE];
  gdouble  ax0[MAX_ATTEMPTS_PERSPECTIVE];    
  gdouble  ay0[MAX_ATTEMPTS_PERSPECTIVE];
  gdouble  ax1[MAX_ATTEMPTS_PERSPECTIVE]; 
  gdouble  ay1[MAX_ATTEMPTS_PERSPECTIVE];
  gdouble  ax2[MAX_ATTEMPTS_PERSPECTIVE];
  gdouble  ay2[MAX_ATTEMPTS_PERSPECTIVE];
  gdouble  ax3[MAX_ATTEMPTS_PERSPECTIVE];
  gdouble  ay3[MAX_ATTEMPTS_PERSPECTIVE];

  /* precision settings for iterative calculations */
  gdouble    transformPrecision;           /* precision in pixels for calculating the perspective transformation matrix (0.0 to 1.0) */
  gdouble    transformPrecisionThreshold;  /* for fine tuning purpose (use perespective coords with precision lower than threshold) */
  
} GapPerspectiveTransCoords;



void                gap_geo_copy_src_to_dst_coords(GapPixelCoords *srcCoords, GapPixelCoords *dstCoords);
gdouble             gap_geo_calculateSqrDistX2Y2(GapPixelCoords *coordA, gdouble px, gdouble py);
gdouble             gap_geo_calculateSqrDist(GapPixelCoords *coordA, GapPixelCoords *coordB);
gdouble             gap_geo_calculateDist(GapPixelCoords *coordA, GapPixelCoords *coordB);


void                gap_geo_line_description_from_2Points(gdouble x1, gdouble y1, gdouble x2, gdouble y2, GapLineDescriptionConsts *lineConst);
void                gap_geo_line_description_from_2GapPixelCoords(GapPixelCoords *p1, GapPixelCoords *p2, GapLineDescriptionConsts *lineConst);
gdouble             gap_geo_line_getX(GapLineDescriptionConsts *lineConst, gdouble y);
gdouble             gap_geo_line_getY(GapLineDescriptionConsts *lineConst, gdouble x);
void                gap_geo_line_intersection(GapLineDescriptionConsts *line1, GapLineDescriptionConsts *line2, GapDoubleCoords *interPt);
gboolean            gap_geo_pick_corners_from_align_coords(GapAlignCoords *alignCoords, gdouble width, gdouble height);
gboolean            gap_geo_perspective_trans_coords_from_align_coords(gint32 activeDrawableId, GapAlignCoords *alignCoords, GapPerspectiveTransCoords *perCoords);
void                gap_geo_assemble_perspective_transformation_matrix(GimpMatrix3  *matrix, GapPerspectiveTransCoords *perCoords, gint32 activeDrawableId);
void                gap_geo_DuplicateAlignCoords(GapAlignCoords *alignCoords, GapAlignCoords *alignCoordsDup);

/* GIMP core internal matrix and perspective handling procedures 
 *     gimp_transform_matrix_perspective
 *     gimp_matrix3_identity
 * are not available for public use.
 * As workaround this module uses its own copies of those procedures (renamed with prefix gap_)
 */
void          gap_matrix3_mult            (const GimpMatrix3 *matrix1,
                                            GimpMatrix3       *matrix2);
void          gap_matrix3_translate       (GimpMatrix3       *matrix,
                                            gdouble            x,
                                            gdouble            y);
void          gap_matrix3_scale           (GimpMatrix3       *matrix,
                                            gdouble            x,
                                            gdouble            y);
void       gap_matrix3_identity               (GimpMatrix3       *matrix);
void       gap_transform_matrix_perspective   (GimpMatrix3         *matrix,
                                                gint                 x,
                                                gint                 y,
                                                gint                 width,
                                                gint                 height,
                                                gdouble              t_x1,
                                                gdouble              t_y1,
                                                gdouble              t_x2,
                                                gdouble              t_y2,
                                                gdouble              t_x3,
                                                gdouble              t_y3,
                                                gdouble              t_x4,
                                                gdouble              t_y4);
                                                

#endif  /* _GAP_GEO_H */
