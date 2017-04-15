/*  gap_detail_tracking_exec.c
 *    This filter locates the position of one or 2 small areas
 *    of a reference layer within a target layer and logs the coordinates
 *    as XML file. It is intended to track details in a frame sequence.
 *
 *  2011/12/01
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
 *  (2011/12/01)  2.7.0       hof: created
 */
extern int gap_debug;

#include "config.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <glib.h>
#include <glib/gstdio.h>

#include <gtk/gtk.h>
#include <libgimp/gimp.h>
#include <libgimp/gimpui.h>
#include <math.h>

#include "gap_geo.h"
#include "gap_base.h"
#include "gap_libgapbase.h"
#include "gap_lib.h"
#include "gap_locate.h"
#include "gap_locate2.h"
#include "gap_colordiff.h"
#include "gap_image.h"
#include "gap_layer_copy.h"
#include "gap_detail_align_exec.h"
#include "gap_detail_tracking_exec.h"
#include "gap_xml_util.h"

#include "gap-intl.h"

#define DEFAULT_refShapeRadius            15
#define DEFAULT_targetMoveRadius          70
#define DEFAULT_loacteColodiffThreshold   0.08
#define DEFAULT_numPointsSelect           4
#define DEFAULT_coordsRelToFrame1         TRUE
#define DEFAULT_offsX                     0
#define DEFAULT_offsY                     0
#define DEFAULT_offsRotate                0.0
#define DEFAULT_enableScaling             TRUE
#define DEFAULT_removeMidlayers           TRUE
#define DEFAULT_bgLayerIsReference        TRUE
#define DEFAULT_addTransformedLayer       TRUE

#define NUMBER_OF_COORDS 12

typedef struct BestIndexes
{
  gint32 bestIdx[4];   /* indexes of the 4 best matching points, -1 indicates an empty unusable index */
} BestIndexes;



static gdouble   p_calculate_angle_in_degree(gint p1x, gint p1y, gint p2x, gint p2y);
static gdouble   p_calculate_scale_factor(gint p1x, gint p1y, gint p2x, gint p2y
                       , gint p3x, gint p3y, gint p4x, gint p4y);
static gdouble   p_getPixelCoordsQuality(GapPixelCoords *coords);
static void      p_capture_n_vector_points(gint32 imageId, PixelCoordsArray *pixelCoordsArray, gint ncoordsToCapture, gchar *vectorsName);
static void      p_copy_src_to_dst_coords_array(PixelCoordsArray *srcCoordsArray, PixelCoordsArray *dstCoordsArray);
static void      p_locate_target(gint32 refLayerId, GapPixelCoords *refCoords
                    , gint32 targetLayerId, GapPixelCoords *targetCoords
                    , gint32 locateOffsetX, gint32 locateOffsetY
                    , FilterValues *valPtr);
static void      p_write_xml_header(FILE *l_fp, gboolean center, gint width, gint height, gint numFrames, FilterValues *valPtr);
static void      p_write_xml_footer(FILE *l_fp);
static gboolean  p_log_to_file(const char *filename, const char *logString
                    , gint32 frameNr, gboolean center, gint width, gint height, FilterValues *valPtr);
static void      p_coords_logging(gint32 frameNr, GapPixelCoords *currCoords,  GapPixelCoords *currCoords2
                    , GapPixelCoords *startCoords, GapPixelCoords *startCoords2, FilterValues *valPtr
                    , gint32 imageId, GapPixelCoords *startRefCoords, gint32 ii1, gint32 ii2
                    );

static void     p_computePredictedCoordinate(GapPixelCoords *predictedCoord
                   , GapPixelCoords *strongRef
                   , GapPixelCoords *strongTrk
                   , GapPixelCoords *weakRef);
static void     p_coords_tune_and_logging_perspective(gint32 frameNr
                   , PixelCoordsArray *currCoordsArray
                   , PixelCoordsArray *startCoordsArray
                   , PixelCoordsArray *prevCoordsArray
                   , BestIndexes *bestIndexes
                   , FilterValues *valPtr
                   , gint32 imageId
                   , gint32 activeDrawableId
                   , gint32 referenceLayerId
                   );

static void     p_pickNearestToCorners(BestIndexes *bestIndexes, gint32 width, gint32 height
                   , PixelCoordsArray *startCoordsArray
                   );
static void     p_select_best_coords(gint32 imageId, gint32 frameNr, PixelCoordsArray *currCoordsArray
                   , PixelCoordsArray *startCoordsArray, FilterValues *valPtr
                   , BestIndexes *bestIndexes, FrameHistInfo *frameHistInfo
                   );
static gint32  p_selective_coords_logging(FrameHistInfo *frameHistInfo
		   , PixelCoordsArray *currCoordsArray
		   , PixelCoordsArray *startCoordsArray
		   , PixelCoordsArray *prevCoordsArray
		   , FilterValues *valPtr
		   , gint32 imageId
		   , gint32 activeDrawableId
		   , gint32 referenceLayerId
                 );
static gint32    p_parse_frame_nr_from_layer_name(gint32 layerId);
static void      p_get_frameHistInfo(FrameHistInfo *frameHistInfo, gint32 imageId);
static void      p_set_frameHistInfo(FrameHistInfo *frameHistInfo, gint32 imageId);
static gint32    p_set_n_vector_points(gint32 imageId, PixelCoordsArray *targetCoordsArray, gchar *vectorsName
                    , gboolean setVisible, gboolean setGuides, gint32 guideIdx);


static void      p_set_debugCoords_from_IntersectionPoints(gint32 frameNr, gchar *vectorName
                    , PixelCoordsArray *debugCoordsArray
                    , GapLineDescriptionConsts *debugLine
                    , GapLineDescriptionConsts *aBorderLine, GapLineDescriptionConsts *bBorderLine);
static void      p_set_debug_intersection_vectors(gint32 frameNr, gint32 imageId, gchar *vectorBasename
                      , PixelCoordsArray *pixelCoordsArray, BestIndexes *bestIndexes);
static void      p_set_debug_vectors(gint32 frameNr, gint32 imageId
                    , PixelCoordsArray *currCoordsArray
                    , PixelCoordsArray *startCoordsArray
                    , BestIndexes *bestIndexes);



/* -----------------------------------
 * p_calculate_angle_in_degree
 * -----------------------------------
 * calculate angle of the line described by coordinates p1, p2
 * returns the angle in degree.
 */
static gdouble
p_calculate_angle_in_degree(gint p1x, gint p1y, gint p2x, gint p2y)
{
  /* calculate angle in degree
   * how to rotate an object that follows the line between p1 and p2
   */
  gdouble l_a;
  gdouble l_b;
  gdouble l_angle_rad;
  gdouble l_angle;

  l_a = p2x - p1x;
  l_b = (p2y - p1y) * (-1.0);

  if(l_a == 0)
  {
    if(l_b < 0)  { l_angle = 90.0; }
    else         { l_angle = 270.0; }
  }
  else
  {
    l_angle_rad = atan(l_b/l_a);
    l_angle = (l_angle_rad * 180.0) / G_PI;

    if(l_a < 0)
    {
      l_angle = 180 - l_angle;
    }
    else
    {
      l_angle = l_angle * (-1.0);
    }
  }

  if(gap_debug)
  {
     printf("p_calc_angle: p1(%d/%d) p2(%d/%d)  a=%f, b=%f, angle=%f\n"
         , (int)p1x, (int)p1y, (int)p2x, (int)p2y
         , (float)l_a, (float)l_b, (float)l_angle);
  }
  return(l_angle);

}  /* end p_calculate_angle_in_degree */


/* -----------------------------------
 * p_calculate_scale_factor
 * -----------------------------------
 * calculate angle of the line described by coordinates p1, p2
 * returns the angle in degree.
 */
static gdouble
p_calculate_scale_factor(gint p1x, gint p1y, gint p2x, gint p2y
                       , gint p3x, gint p3y, gint p4x, gint p4y)
{
  /* calculate angle in degree
   * how to rotate an object that follows the line between p1 and p2
   */
  gdouble l_a;
  gdouble l_b;
  gdouble scaleFactor;

  scaleFactor = 1.0;

  l_a = sqrt(((p2x - p1x) * (p2x - p1x)) + ((p2y - p1y) * (p2y - p1y)));
  l_b = sqrt(((p4x - p3x) * (p4x - p3x)) + ((p4y - p3y) * (p4y - p3y)));

  if ((l_a >= 0) &&(l_b >= 0))
  {
    scaleFactor = l_a / l_b;
  }

  return(scaleFactor);

}  /* end p_calculate_scale_factor */


/* ----------------------------
 * p_getPixelCoordsQuality
 * ----------------------------
 * returns the locating quality between 0.0 and 1.0 where 1.0 is best quality.
 * invalid coords are rated as quiality 0.0
 */
static gdouble
p_getPixelCoordsQuality(GapPixelCoords *coords) 
{
  gdouble qaulity;
  if (coords->valid)
  {
    qaulity = CLAMP((1.0 - coords->avgColorDiff), 0.0, 1.0);
  }
  else
  {
    qaulity = 0.0; 
  }
  return qaulity;

}  /* end p_getPixelCoordsQuality */





/* ------------------------------------------
 * p_capture_n_vector_points
 * ------------------------------------------
 * capture the first n coords of the 1st stroke in the named path vectors
 *  (or active path vectors when vectorsName is null or not found)
 */
static void
p_capture_n_vector_points(gint32 imageId, PixelCoordsArray *pixelCoordsArray, gint ncoordsToCapture, gchar *vectorsName)
{
  gint nCoords;
  gint idx;
  gint32  activeVectorsId;
  gint32  gx1;
  gint32  gy1;
  GapPixelCoords *coordPtr;

  gx1 = -1;
  gy1 = -1;
  
  nCoords = CLAMP(ncoordsToCapture, 0, MAX_PIXEL_COORDS_ARRAY);
  pixelCoordsArray->numberOfCoords = 0;
  for(idx = 0; idx < nCoords; idx++) 
  {
     coordPtr = &pixelCoordsArray->pixCoord[idx];
     coordPtr->valid = FALSE;
     coordPtr->avgColorDiff = 1.0;  /* worst for invalid coords */
  }

  activeVectorsId = -1;
  if (vectorsName != NULL)
  {
    if (*vectorsName != '\0')
    {
       activeVectorsId = gimp_image_get_vectors_by_name(imageId, vectorsName);
    }
  }
  if (activeVectorsId < 0)
  {
    activeVectorsId = gimp_image_get_active_vectors(imageId);
  }


  if(activeVectorsId >= 0)
  {
    gint num_strokes;
    gint *strokes;

    strokes = gimp_vectors_get_strokes (activeVectorsId, &num_strokes);
    if(strokes)
    {
      if(num_strokes > 0)
      {
        gdouble  *points;
        gint      num_points;
        gboolean  closed;
        GimpVectorsStrokeType type;

        points = NULL;
        type = gimp_vectors_stroke_get_points(activeVectorsId, strokes[0],
                                   &num_points, &points, &closed);

        if(gap_debug)
        {
          gint ii;
          for(ii=0; ii < MIN(12, num_points); ii++)
          {
            printf ("point[%d] = %.3f\n", ii, points[ii]);
          }
        }

        if (type == GIMP_VECTORS_STROKE_TYPE_BEZIER)
        {
          /* attempt to pick the first nCoords from the vectors path */
          for(idx = 0; idx < nCoords; idx++)
          {
            /* for GIMP_VECTORS_STROKE_TYPE_BEZIER there are 6 points per coordinate
             * where 
             *   point[0] is the x coordinate of the 1st pathpoint,  point[6] holds x coordinate of the 2nd pathpoint
             *   point[1] is the y coordinate of the 1st pathpoint,  point[7] holds y coordinate of the 2nd pathpoint
             *   point[2] [3] [4] [5] contain other information that is not used here.
             */
            if(num_points > idx * 6) 
            {
              gint idxOffset;
              
              idxOffset = idx *6;
              gx1 = points[0 + idxOffset];
              gy1 = points[1 + idxOffset];
              if((gx1 >= 0) && (gy1 >= 0))
              {
                coordPtr = &pixelCoordsArray->pixCoord[idx];
                coordPtr->px = gx1;
                coordPtr->py = gy1;
                coordPtr->valid = TRUE;
                coordPtr->avgColorDiff = 0.0;     /* best for reference coords */
                pixelCoordsArray->numberOfCoords++;
              }
              else
              {
                break;  /* stop at the first invalid point */
              }
            }
            
          }
          
        }
        if(points)
        {
          g_free(points);
        }

      }
      g_free(strokes);
    }

  }


  if(gap_debug)
  {
     printf("\np_capture_n_vector_points  PathPoints: %d\n", pixelCoordsArray->numberOfCoords);
     for(idx = 0; idx < pixelCoordsArray->numberOfCoords; idx++)
     {
       coordPtr = &pixelCoordsArray->pixCoord[idx];
       printf("x[%d]=%d; y[%d]=%d\n"
          , idx
          , coordPtr->px 
          , idx
          , coordPtr->py
          );
     }
     printf("  numberOfCoords:%d\n", pixelCoordsArray->numberOfCoords);
  }


}  /* end p_capture_n_vector_points */




/* ------------------------------------
 * p_copy_src_to_dst_coords_array
 * ------------------------------------
 */
static void
p_copy_src_to_dst_coords_array(PixelCoordsArray *srcCoordsArray, PixelCoordsArray *dstCoordsArray)
{
  gint idx;
  gint numberOfCoords;
  
  numberOfCoords = CLAMP(srcCoordsArray->numberOfCoords, 0, MAX_PIXEL_COORDS_ARRAY);
  dstCoordsArray->numberOfCoords = numberOfCoords;
  
  for(idx = 0; idx < numberOfCoords; idx++)
  {
    gap_geo_copy_src_to_dst_coords(&srcCoordsArray->pixCoord[idx]
                            ,&dstCoordsArray->pixCoord[idx]
                            );
  }
}



/* ------------------------------------
 * p_locate_target
 * ------------------------------------
 */
static void
p_locate_target(gint32 refLayerId, GapPixelCoords *refCoords
   , gint32 targetLayerId, GapPixelCoords *targetCoords
   , gint32 locateOffsetX, gint32 locateOffsetY
   , FilterValues *valPtr)
{
  gdouble colordiffLocate;
  gint          ref_offset_x;
  gint          ref_offset_y;
  gint          target_offset_x;
  gint          target_offset_y;
  gint32        refX;
  gint32        refY;
  gint32        targetX;
  gint32        targetY;


  /* get offsets of the layers within the image */
  gimp_drawable_offsets (refLayerId, &ref_offset_x, &ref_offset_y);
  gimp_drawable_offsets (targetLayerId, &target_offset_x, &target_offset_y);

  targetCoords->valid = FALSE;

  refX = refCoords->px - ref_offset_x;
  refY = refCoords->py - ref_offset_y;
  colordiffLocate =
       gap_locateAreaWithinRadiusWithOffset (refLayerId
                                    , refX
                                    , refY
                                    , valPtr->refShapeRadius
                                    , targetLayerId
                                    , valPtr->targetMoveRadius
                                    , &targetX
                                    , &targetY
                                    , locateOffsetX
                                    , locateOffsetY
                                    );
  targetCoords->avgColorDiff = colordiffLocate;  /* 0.0 indicates the best. 1.0 the worst locating quality of the target coordinate */

  targetCoords->px = targetX + target_offset_x;
  targetCoords->py = targetY + target_offset_y;


  if (colordiffLocate < valPtr->loacteColodiffThreshold)
  {
    /* successful located the deatail in target layer
     * set target coordinates valid.
     */
    targetCoords->valid = TRUE;
  }

  if(gap_debug)
  {
    printf("p_locate_target: refX:%d refY:%d locateOffsetX:%d locateOffsetY:%d\n"
            "                targetX:%d targetY:%d targetCoords->px:%d py:%d  avgColodiff:%.5f valid:%d\n"
        ,(int)refX
        ,(int)refY
        ,(int)locateOffsetX
        ,(int)locateOffsetY
        ,(int)targetX
        ,(int)targetY
        ,(int)targetCoords->px
        ,(int)targetCoords->py
        ,(float)targetCoords->avgColorDiff
        , targetCoords->valid
        );
  }


}  /* end p_locate_target */


/* -----------------------------------------
 * p_write_xml_header
 * -----------------------------------------
 * write header for a MovePath XML file
 */
static void
p_write_xml_header(FILE *l_fp, gboolean center, gint width, gint height, gint numFrames, FilterValues *valPtr)
{
  fprintf(l_fp, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n");
  
  
  fprintf(l_fp, "<gimp_gap_move_path_parameters version=\"2\" >\n");
  
  fprintf(l_fp, "  <gimp_gap_tracking_parameters  ");
  gap_xml_write_int_value(l_fp, "numPointsSelect", (gint32) valPtr->numPointsSelect);         
  gap_xml_write_int_value(l_fp, "refShapeRadius", (gint32) valPtr->refShapeRadius);         
  gap_xml_write_int_value(l_fp, "targetMoveRadius", (gint32) valPtr->targetMoveRadius);         
  gap_xml_write_gdouble_value(l_fp, "loacteColodiffThreshold", valPtr->loacteColodiffThreshold, 1 /* digits */, 5 /* precision_digits */);
  gap_xml_write_gboolean_value(l_fp, "coordsRelToFrame1", valPtr->coordsRelToFrame1);
  gap_xml_write_int_value(l_fp, "offsX", (gint32) valPtr->offsX);
  gap_xml_write_int_value(l_fp, "offsY", (gint32) valPtr->offsY);
  gap_xml_write_gdouble_value(l_fp, "offsRotate", valPtr->offsRotate, 3 /* digits */, 5 /* precision_digits */);
  gap_xml_write_gboolean_value(l_fp, "enableScaling", valPtr->enableScaling);
  fprintf(l_fp, " />\n");

  fprintf(l_fp, "  <frame_description ");
  gap_xml_write_int_value(l_fp, "width", (gint32) width);         
  gap_xml_write_int_value(l_fp, "height", (gint32) height);         
  gap_xml_write_int_value(l_fp, "range_from", (gint32) 1);         
  gap_xml_write_int_value(l_fp, "range_to", (gint32) numFrames);         
  gap_xml_write_int_value(l_fp, "total_frames", (gint32) numFrames);         
  fprintf(l_fp, " />\n");
          
          
  fprintf(l_fp, "  <tween tween_steps=\"0\" />\n");
  fprintf(l_fp, "  <trace tracelayer_enable=\"FALSE\" />\n");
  fprintf(l_fp, "  <moving_object src_layer_id=\"0\" src_layerstack=\"0\" width=\"%d\" height=\"%d\"\n"
          , (int)width
          , (int)height
          );
          
  if(center)
  {
    fprintf(l_fp, "    src_handle=\"GAP_HANDLE_CENTER\"");
  }
  else
  {
    fprintf(l_fp, "    src_handle=\"GAP_HANDLE_LEFT_TOP\"");
  }
  fprintf(l_fp, " handle_dx=\"0\" handle_dy=\"0\"\n"); 
          
  fprintf(l_fp, "    src_stepmode=\"GAP_STEP_FRAME_ONCE\" step_speed_factor=\"1.00000\"\n");
  fprintf(l_fp, "    src_selmode=\"GAP_MOV_SEL_IGNORE\"\n");
  fprintf(l_fp, "    src_paintmode=\"GIMP_NORMAL_MODE\"\n");
  fprintf(l_fp, "    dst_layerstack=\"0\" src_force_visible=\"TRUE\" clip_to_img=\"FALSE\" src_apply_bluebox=\"FALSE\"\n");
  fprintf(l_fp, "    >\n");
  fprintf(l_fp, "  </moving_object>\n");
  fprintf(l_fp, "\n");

  fprintf(l_fp, "  <controlpoints current_point=\"0\" number_of_points=\"%d\"  >\n"
          , (int)numFrames
          );

}  /* end p_write_xml_header */


/* -----------------------------------------
 * p_write_xml_footer
 * -----------------------------------------
 */
static void
p_write_xml_footer(FILE *l_fp)
{
  fprintf(l_fp, "  </controlpoints>\n");
  fprintf(l_fp, "</gimp_gap_move_path_parameters>");
}  /* end p_write_xml_footer */


/* -----------------------------------------
 * p_log_to_file
 * -----------------------------------------
 * log controlpoint (logString) to XML file.
 * An existing XML file is owerwritten, but the controlpoints
 * of the old content are copied to the newly written XML file generation.
 *
 * return TRUE on success, FALSE on errors.
 */
static gboolean
p_log_to_file(const char *filename, const char *logString
   , gint32 frameNr, gboolean center, gint width, gint height, FilterValues *valPtr)
{
  char *textBuffer;
  gsize lengthTextBuffer;
  gint  ii;
  gint  beginPos;
  gint  endPos;
  gint  copyPos;
  gint  copySize;
  gboolean ok;
  FILE *l_fp;



  copySize = 0;
  beginPos = 0;
  endPos   = 0;
  copyPos  = -1;
  textBuffer = NULL;
  if ((g_file_test (filename, G_FILE_TEST_EXISTS))
  && (frameNr > 1))
  {
    /* get old content of the XML file and findout the block
     * of controlpoint lines to be copied when (over)writing the next
     * generation of the XML file
     */

    if ((g_file_get_contents (filename, &textBuffer, &lengthTextBuffer, NULL) != TRUE)
    || (textBuffer == NULL))
    {
      printf("Could not load XML file:%s\n", filename);
      return(FALSE);
    }

    for(ii=1; ii < lengthTextBuffer; ii++)
    {

      if (textBuffer[ii] == '\n')
      {
        beginPos = -1;
      }

      if ((textBuffer[ii-1] != ' ') && (textBuffer[ii] == ' '))
      {
        beginPos = ii;
      }

      /* find first controlpoint (as start position of block to copy) */
      if (strncmp(&textBuffer[ii], "<controlpoint ", strlen("<controlpoint ")) == 0)
      {
        if (copyPos < 0)
        {
          if (beginPos >= 0)
          {
            copyPos = beginPos;
          }
          else
          {
            copyPos = ii -1;
          }
        }
      }

      /* check end position of the controlpoints block */
      if (strncmp(&textBuffer[ii], "</controlpoints>", strlen("</controlpoints>")) == 0)
      {
        if (beginPos >= 0)
        {
          endPos = beginPos;
        }
        else
        {
          endPos = ii -1;
        }
        if (copyPos > 0)
        {
          copySize = endPos - copyPos;
        }
        break;
      }
    }
    if ((endPos == 0) && (copyPos > 0))
    {
      copySize = lengthTextBuffer - copyPos;
    }
  }



  ok = FALSE;

  l_fp = g_fopen(filename, "w+");
  if(l_fp != NULL)
  {
      p_write_xml_header(l_fp, center, width, height, frameNr, valPtr);

      if (copySize > 0)
      {
        fclose(l_fp);

        /* append controlpoints (use binary mode to prevent additional line feeds in Windows environment)  */
        l_fp = g_fopen(filename, "ab");
        if(l_fp != NULL)
        {
          fwrite(&textBuffer[copyPos], copySize, 1, l_fp);
          fclose(l_fp);
          l_fp = g_fopen(filename, "a+");
        }
      }

      if(l_fp != NULL)
      {
        fprintf(l_fp, "%s\n", logString);
        p_write_xml_footer(l_fp);
        fclose(l_fp);

        ok = TRUE;
      }
  }
  else
  {
    printf("Could not update file:%s", filename);
  }

  if(textBuffer != NULL)
  {
    g_free(textBuffer);
  }

  return (ok);

}  /* end p_log_to_file */



/* ----------------------------
 * p_coords_logging
 * ----------------------------
 * log coordinates to stdout
 * or to move-path controlpoint XML file.
 *
 */
static void
p_coords_logging(gint32 frameNr, GapPixelCoords *currCoords,  GapPixelCoords *currCoords2
  , GapPixelCoords *startCoords, GapPixelCoords *startCoords2, FilterValues *valPtr
  , gint32 imageId, GapPixelCoords *startRefCoords, gint32 ii1, gint32 ii2
  )
{
  gint32  px;
  gint32  py;
  gint32  px1;
  gint32  py1;
  gint32  px2;
  gint32  py2;
  gdouble rotation;
  gchar  *logString;
  gdouble scaleFactor;
  gboolean center;
  gint     width;
  gint     height;
  gint     precision_digits;
  gchar   *rotValueAsString;

  if(startRefCoords->valid != TRUE)
  {
    /* do not record invalid coordinates */
    return;
  }
  
  width = gimp_image_width(imageId);
  height = gimp_image_height(imageId);
  center = FALSE;
  if ((valPtr->coordsRelToFrame1)
  &&  (valPtr->offsX != 0)
  &&  (valPtr->offsY != 0))
  {
    center = TRUE;
  }
  

  scaleFactor = 1.0;
  rotation = 0.0;
  if(currCoords->valid != TRUE)
  {
    /* fallback to initial start reference coordinates */
    px1 = startRefCoords->px;
    py1 = startRefCoords->px;
  }
  else
  {
    px1 = currCoords->px;
    py1 = currCoords->py;
  }
  px2 = currCoords2->px;
  py2 = currCoords2->py;
 

  /* px1 and px2 represent the current coordinate or fallback value at this point */
  if (valPtr->coordsRelToFrame1)
  {
    /* px py is difference */
    px = startCoords->px -px1;
    py = startCoords->py -py1;
    px1 = px;
    py1 = py;
  }
  else
  {
    /* px py is absolute value transformed to the reference point
     * (reference point ist the first in the path)
     */
    px = (px1 - startCoords->px) + startRefCoords->px;
    py = (py1 - startCoords->py) + startRefCoords->py;
  }

  /* add the offsets */
  px += valPtr->offsX;
  py += valPtr->offsY;



  if ((valPtr->coordsRelToFrame1)
  &&  (valPtr->numPointsSelect > 1)
  &&  (startCoords2->valid == TRUE))
  {
    px2 = startCoords2->px -px2;
    py2 = startCoords2->py -py2;

  }

  if((currCoords2->valid  == TRUE)
  &&  (valPtr->numPointsSelect > 1)
  && (startCoords2->valid == TRUE)
  && (startCoords->valid  == TRUE))
  {
    gdouble startAngle;
    gdouble currAngle;

    /* we have 2 valid coordinate points and can calculate rotate compensation
     * in this case movement offest compensation is the average
     * of both tracked coordinate points
     */
    px = ((px1 + px2) / 2) + valPtr->offsX;
    py = ((py1 + py2) / 2) + valPtr->offsY;


    startAngle = p_calculate_angle_in_degree( startCoords->px
                                    , startCoords->py
                                    , startCoords2->px
                                    , startCoords2->py
                                    );
    currAngle = p_calculate_angle_in_degree(  currCoords->px
                                    , currCoords->py
                                    , currCoords2->px
                                    , currCoords2->py
                                    );
    rotation = startAngle - currAngle;
    if (rotation >= 360.0)
    {
      rotation = 360.0 - rotation;
    }
    if (rotation <= -360.0)
    {
      rotation += 360.0;
    }
    scaleFactor = p_calculate_scale_factor( startCoords->px
                                    , startCoords->py
                                    , startCoords2->px
                                    , startCoords2->py
                                    , currCoords->px
                                    , currCoords->py
                                    , currCoords2->px
                                    , currCoords2->py
                                    );
  }

  logString = NULL;

  if((currCoords2->valid == TRUE)
  && (valPtr->numPointsSelect > 1))
  {
    /* double point detail coordinate tracking (allows calculation of rotate and scale factors) */
    gchar *scaleValueAsString;


    precision_digits = 7;
    rotValueAsString = gap_base_gdouble_to_ascii_string(rotation + valPtr->offsRotate, precision_digits);
    precision_digits = 5;
    scaleValueAsString = gap_base_gdouble_to_ascii_string(scaleFactor * 100, precision_digits);

    if(valPtr->enableScaling == TRUE)
    {
      logString = g_strdup_printf("    <controlpoint px=\"%04d\" py=\"%04d\" rotation=\"%s\" width_resize=\"%s\" height_resize=\"%s\" keyframe_abs=\"%06d\" p1x=\"%04d\"  p1y=\"%04d\"  p2x=\"%04d\" p2y=\"%04d\" s1x=\"%04d\" s1y=\"%04d\" s2x=\"%04d\" s2y=\"%04d\" ii1=\"%d\"  ii2=\"%d\"/>"
       , px
       , py
       , rotValueAsString
       , scaleValueAsString
       , scaleValueAsString
       , frameNr
       , currCoords->px
       , currCoords->py
       , currCoords2->px
       , currCoords2->py
       , startCoords->px
       , startCoords->py
       , startCoords2->px
       , startCoords2->py
       , (int) ii1
       , (int) ii2
       );
    }
    else
    {
      logString = g_strdup_printf("    <controlpoint px=\"%04d\" py=\"%04d\" rotation=\"%s\" keyframe_abs=\"%06d\" p1x=\"%04d\"  p1y=\"%04d\"  p2x=\"%04d\" p2y=\"%04d\" s1x=\"%04d\" s1y=\"%04d\" s2x=\"%04d\" s2y=\"%04d\" ii1=\"%d\"  ii2=\"%d\"/>"
       , px
       , py
       , rotValueAsString
       , frameNr
       , currCoords->px
       , currCoords->py
       , currCoords2->px
       , currCoords2->py
       , startCoords->px
       , startCoords->py
       , startCoords2->px
       , startCoords2->py
       , (int) ii1
       , (int) ii2
       );
    }


    g_free(rotValueAsString);
    g_free(scaleValueAsString);
  }
  else
  {
    /* single point detail coordinate tracking */
    if (valPtr->offsRotate == 0.0)
    {
      logString = g_strdup_printf("    <controlpoint px=\"%04d\" py=\"%04d\"  keyframe_abs=\"%06d\" p1x=\"%04d\"  p1y=\"%04d\" s1x=\"%04d\"  s1y=\"%04d\" ii1=\"%d\"/>"
         , px
         , py
         , frameNr
         , currCoords->px
         , currCoords->py
         , startCoords->px
         , startCoords->py
         , (int) ii1
         );
    }
    else
    {
      precision_digits = 7;
      rotValueAsString = gap_base_gdouble_to_ascii_string(valPtr->offsRotate, precision_digits);

      logString = g_strdup_printf("    <controlpoint px=\"%04d\" py=\"%04d\"  rotation=\"%s\" keyframe_abs=\"%06d\" p1x=\"%04d\"  p1y=\"%04d\" s1x=\"%04d\"  s1y=\"%04d\" ii1=\"%d\"/>"
         , px
         , py
         , rotValueAsString
         , frameNr
         , currCoords->px
         , currCoords->py
         , startCoords->px
         , startCoords->py
         , (int) ii1
         );
      g_free(rotValueAsString);
    }

  }

  if ((valPtr->moveLogFile[0] == '\0')
  ||  (valPtr->moveLogFile[0] == '-'))
  {
    printf("%s\n", logString);
  }
  else
  {
    p_log_to_file(&valPtr->moveLogFile[0], logString
                , frameNr
                , center
                , width
                , height
                , valPtr
                );
  }

  if (logString)
  {
    g_free(logString);
  }


}  /* end p_coords_logging */


/* ----------------------------------------
 * p_computePredictedCoordinate
 * ----------------------------------------
 * This prediction calculation assumes the same movement on the weak matching tracked point
 * as it was detected in the strong matching point. (simple movement without any further transformation) 
 * TODO:
 * ideally the prediction shall check for potential scale, rotation and perspective transfromations in case
 * when more than one strong point is available...
 */
static void
p_computePredictedCoordinate(GapPixelCoords *predictedCoord, GapPixelCoords *strongRef, GapPixelCoords *strongTrk, GapPixelCoords *weakRef)
{
  predictedCoord->px = weakRef->px + (strongTrk->px - strongRef->px);
  predictedCoord->py = weakRef->py + (strongTrk->py - strongRef->py);

} /* end p_computePredictedCoordinate */


/* -------------------------------------
 * p_coords_tune_and_logging_perspective
 * -------------------------------------
 * log coordinates to stdout
 * or to move-path controlpoint XML file.
 * This variant of logging handles the case of perspective transformation
 * it provides controlpoints for the GIMP-GAP MovePath tool
 * and for the alternative using the gap_detail_align filter (in combination with the framesModify feature)
 *
 * This procedure also fine-tunes the coordinates (by calling gap_locate_FindTuneOffsShortList)
 * 
 */
static void
p_coords_tune_and_logging_perspective(gint32 frameNr
                   , PixelCoordsArray *currCoordsArray
                   , PixelCoordsArray *startCoordsArray
                   , PixelCoordsArray *prevCoordsArray
                   , BestIndexes *bestIndexes
                   , FilterValues *valPtr
                   , gint32 imageId
                   , gint32 activeDrawableId
                   , gint32 referenceLayerId
                   )
{
  gint32  px;
  gint32  py;
  gint     width;
  gint     height;
  gdouble  w2;
  gdouble  h2;
  gint     precision_digits;
  gchar  *logString;

  GapPerspectiveTransCoords  perspectiveCoords;
  GapPerspectiveTransCoords *perCoords;
  GapAlignCoords             gapAlingCoords;
  GapAlignCoords            *alignCoords;
  gboolean perCoordsOk;
  gdouble ttlx;
  gdouble ttly;
  gdouble ttrx;
  gdouble ttry;
  gdouble tblx;
  gdouble tbly;
  gdouble tbrx;
  gdouble tbry;
  GapPixelCoords  *trkPtr[4];   /* upto 4 coords in current frame  currCoords[4]; */
  GapPixelCoords  *refPtr[4];   /* upto 4 coords of first processed (reference) frame  startCoords[4]; */
  
  GapPixelCoords  *prevPtr[4];   /* upto 4 coords of previous processed frame */
  GapPixelCoords  *predPtr[4];   /* upto 4 coords of previous processed frame */
  GapPixelCoords  *utrkPtr[4];   /* upto 4 coords in current frame  untunedCurrCoords[4]; */
  GapPixelCoords  untunedCurrCoords[4];
  
  gint invalidCount;
  gint idx;
    
  gchar *ttlxValueAsString;
  gchar *ttlyValueAsString;
  gchar *ttrxValueAsString;
  gchar *ttryValueAsString;
  gchar *tblxValueAsString;
  gchar *tblyValueAsString;
  gchar *tbrxValueAsString;
  gchar *tbryValueAsString;

  gchar *attlxValueAsString;
  gchar *attlyValueAsString;
  gchar *attrxValueAsString;
  gchar *attryValueAsString;
  gchar *atblxValueAsString;
  gchar *atblyValueAsString;
  gchar *atbrxValueAsString;
  gchar *atbryValueAsString;

  perCoords = &perspectiveCoords;
  alignCoords = &gapAlingCoords;

  width = gimp_image_width(imageId);
  height = gimp_image_height(imageId);

  ///////////////////////////////////////////// start tuning
  
  if(gap_debug)
  {
    printf(" [frameNr:%d] activeDrawableId:%d referenceLayerId:%d enableScaling(Tuning):%s\n"
          ,(int)frameNr
          ,(int)activeDrawableId
          ,(int)referenceLayerId
          , (valPtr->enableScaling ? "TRUE" : "FALSE")
          );
  }
  
  if(valPtr->enableScaling)  // TODO have own boolean enableTuning
  {
    gboolean                useRefForPredictedCoord;
    gboolean                isStrong[4];
    GapLocateTuneOffsElem  *rootShortList[4];
    gint                    strongCount;
    gint                    idxStrongOne;
    gint                    strongIndexes[4];
    gdouble                 qFactor;
  
    /*  TODO find a practical qFactor in the tests..
     * the qFactor shall eliminate weak matchers (by setting them invalid)
     * in case there is a clear favorite matching offset available,
     * but keep more elements (== tune attempts) in case there are more very similar matching candidates.
     */
    qFactor = 1.4; // TODO...
  
    for(idx=0; idx < 4; idx++)
    {
      isStrong[idx]      = FALSE;
      rootShortList[idx] = NULL;
      strongIndexes[idx] = 0;
      
      gap_geo_copy_src_to_dst_coords(&currCoordsArray->pixCoord[idx]   /* GapPixelCoords *srcCoords*/
                                    ,&untunedCurrCoords[idx]           /* GapPixelCoords*dstCoords */
                                    );
    }
    
    invalidCount = 0;
    strongCount = 0;
    idxStrongOne = 0;
    for(idx=0; idx < 4; idx++)
    {
      trkPtr[idx] = &currCoordsArray->pixCoord[bestIndexes->bestIdx[idx]];
      refPtr[idx] = &startCoordsArray->pixCoord[bestIndexes->bestIdx[idx]];
      prevPtr[idx] = &prevCoordsArray->pixCoord[bestIndexes->bestIdx[idx]];
      utrkPtr[idx] = &untunedCurrCoords[bestIndexes->bestIdx[idx]];
    
      if((trkPtr[idx]->valid != TRUE)
      || (refPtr[idx]->valid != TRUE))
      {
        invalidCount++;
      }
      else
      {
        rootShortList[idx] = gap_locate_FindTuneOffsShortList(activeDrawableId, 
            referenceLayerId,
            refPtr[idx],  //  GapPixelCoords *refCoord, 
            trkPtr[idx],  //  GapPixelCoords *currCoord, 
            qFactor       //  gdouble qFactor
            );
         isStrong[idx] = gap_locate_check_strong_shortlist(rootShortList[idx]
                                                          , 1.02  /* nearlySameFactor */  // TODO find usable value
                                                          , 0.1   /* strongRelDiff    */  // TODO find usable value
                                                          );
         if (isStrong[idx] == TRUE)
         {
           // apply tuning offsets of the 1.st list element for strong points.
           trkPtr[idx]->px = trkPtr[idx]->px + rootShortList[idx]->tuneOffsetX;
           trkPtr[idx]->py = trkPtr[idx]->py + rootShortList[idx]->tuneOffsetY;
            
           strongIndexes[strongCount] = idx;
           idxStrongOne = strongIndexes[0];
           strongCount++;

           if(gap_debug)
           {
             printf(" [frameNr:%d idx:%d] upx:%d upy:%d (tuned: %d %d) relDiff:%f tuneOffsetXY:(%d %d) isSTRONG\n"
                    ,(int)frameNr
                    ,(int)idx
                    ,(int)utrkPtr[idx]->px
                    ,(int)utrkPtr[idx]->py
                    ,(int)trkPtr[idx]->px
                    ,(int)trkPtr[idx]->py
                    ,(float)rootShortList[idx]->relDiff
                    ,(int)rootShortList[idx]->tuneOffsetX
                    ,(int)rootShortList[idx]->tuneOffsetY
                    );
           }
         }
         else
         {
           if(gap_debug)
           {
             printf(" [frameNr:%d idx:%d] upx:%d upy:%d (not yet tuned) relDiff:%f tuneOffsetXY:(%d %d) isWeak\n"
                    ,(int)frameNr
                    ,(int)idx
                    ,(int)utrkPtr[idx]->px
                    ,(int)utrkPtr[idx]->py
                    ,(float)rootShortList[idx]->relDiff
                    ,(int)rootShortList[idx]->tuneOffsetX
                    ,(int)rootShortList[idx]->tuneOffsetY
                    );
           }
         }
    
      }
    }
    
    
    if ((strongCount < 4) && (strongCount > 0))
    {
      // TODO maybe provide better algortithm in case having 2 or 3 strong points
      useRefForPredictedCoord = FALSE;
      if (strongCount > 1)
      {
        gdouble thisSqrDistance;
        gdouble refSqrDistance;
        gdouble prevSqrDistance;
        
        /* calculate distances between 2 strong points for this frame, the previous one and the intial one (ref) */
        thisSqrDistance = gap_geo_calculateSqrDist(trkPtr[strongIndexes[0]],  trkPtr[strongIndexes[1]]);
        refSqrDistance =  gap_geo_calculateSqrDist(refPtr[strongIndexes[0]],  refPtr[strongIndexes[1]]);
        prevSqrDistance = gap_geo_calculateSqrDist(prevPtr[strongIndexes[0]], prevPtr[strongIndexes[1]]);
        
        /* use the closer one for calculation of the predicted coordinate.
         * (remarkable different distances may indicate scaled frames (zoom) or perspective transformation (camera rotations))
         */
        if (fabs(thisSqrDistance - refSqrDistance) <= fabs(thisSqrDistance - prevSqrDistance))
        {
          useRefForPredictedCoord = TRUE;
        }
      }
      for(idx=0; idx < 4; idx++)
      {
        if (useRefForPredictedCoord)
        {
          predPtr[idx] = refPtr[idx]; /* use inital reference coordinate for calcualtion of predicted coords */
        }
        else
        {
          predPtr[idx] = prevPtr[idx];  /* use coordinates of previous frame for calcualtion of predicted coords */
        }
      }
    
      for(idx=0; idx < 4; idx++)
      {
        if (isStrong[idx] != TRUE)
        {
           GapPixelCoords predictedCoord;
           
           p_computePredictedCoordinate(&predictedCoord,  predPtr[idxStrongOne], trkPtr[idxStrongOne], predPtr[idx]);
           gap_locatePickNearestToPredictedCoordinateFromShortlist(trkPtr[idx], &predictedCoord, rootShortList[idx], width, height);
        
           if(gap_debug)
	   {
	     printf(" [frameNr:%d idx:%d] upx:%d upy:%d (tuned px:%d py:%d) predictedCoord.px:%d .py:%d isWeak "
	                       ,(int)frameNr
	                       ,(int)idx
	                       ,(int)utrkPtr[idx]->px
	                       ,(int)utrkPtr[idx]->py
	                       ,(int)trkPtr[idx]->px
	                       ,(int)trkPtr[idx]->py
	                       ,(int)predictedCoord.px
	                       ,(int)predictedCoord.py
	                       );
	     if (useRefForPredictedCoord)
	     {
	       printf(" useRefForPredictedCoord\n");
	     }
	     else
	     {
	       printf(" usePreviosFrameForPredictedCoord\n");
	     }
           }
        
        
        }
      }  
    }
    
    // free short lists
    for(idx=0; idx < 4; idx++)
    {
      gap_locate_freeTuneOffsList(rootShortList[idx]);
    }
  }

  ///////////////////////////////////////////// end tuning

  
  invalidCount = 0;
  for(idx=0; idx < 4; idx++)
  {
    trkPtr[idx] = &currCoordsArray->pixCoord[bestIndexes->bestIdx[idx]];
    refPtr[idx] = &startCoordsArray->pixCoord[bestIndexes->bestIdx[idx]];
  
    if((trkPtr[idx]->valid != TRUE)
    || (refPtr[idx]->valid != TRUE))
    {
      invalidCount++;
    }

    gap_geo_copy_src_to_dst_coords(trkPtr[idx]                     /* GapPixelCoords *srcCoords*/
                                  ,&alignCoords->currCoords[idx]   /* GapPixelCoords*dstCoords */
                                  );
    gap_geo_copy_src_to_dst_coords(refPtr[idx]                     /* GapPixelCoords *srcCoords*/
                                  ,&alignCoords->startCoords[idx]  /* GapPixelCoords*dstCoords */
                                  );
  }


  if(invalidCount > 0)
  {
    /* do not record invalid coordinates */
    return;
  }


 /* calculate absolute coordinates of 4 cornerpoints for the perspective transformation
  * and convert them to GAP Move Path typical perspective notation relative to width/height
  */
  w2 = width / 2.0;
  h2 = height / 2.0;

  ttlx = 1.0;
  ttly = 1.0;
  ttrx = 1.0;
  ttry = 1.0;
  tblx = 1.0;
  tbly = 1.0;
  tbrx = 1.0;
  tbry = 1.0;

  perCoords->width = width;
  perCoords->height = height;

  perCoordsOk = gap_geo_perspective_trans_coords_from_align_coords(-1 /* activeDrawableId */ , alignCoords, perCoords);
  if (perCoordsOk == TRUE)
  {
    ttlx = 0 - ((perCoords->x0 - w2) / w2);
    ttly = 0 - ((perCoords->y0 - h2) / h2);

    ttrx = (perCoords->x1 - w2) / w2;
    ttry = 0 - ((perCoords->y1 - h2) / h2);

    tblx = 0 - ((perCoords->x2 - w2) / w2);
    tbly = (perCoords->y2 - h2) / h2;

    tbrx = (perCoords->x3 - w2) / w2;
    tbry = (perCoords->y3 - h2) / h2;
  }
  else
  {
    if(gap_debug)
    {
      printf("perCoords NOT Ok !");
    }
  }



  logString = NULL;

  precision_digits = 8;
  
  /* relative GAP-MovePath typical coordinate values of the perspective transformation */
  ttlxValueAsString = gap_base_gdouble_to_ascii_string(ttlx, precision_digits);
  ttlyValueAsString = gap_base_gdouble_to_ascii_string(ttly, precision_digits);
  ttrxValueAsString = gap_base_gdouble_to_ascii_string(ttrx, precision_digits);
  ttryValueAsString = gap_base_gdouble_to_ascii_string(ttry, precision_digits);
  tblxValueAsString = gap_base_gdouble_to_ascii_string(tblx, precision_digits);
  tblyValueAsString = gap_base_gdouble_to_ascii_string(tbly, precision_digits);
  tbrxValueAsString = gap_base_gdouble_to_ascii_string(tbrx, precision_digits);
  tbryValueAsString = gap_base_gdouble_to_ascii_string(tbry, precision_digits);

  /* Absoulute pixel coordinate values of the perspective transformation */
  attlxValueAsString = gap_base_gdouble_to_ascii_string(perCoords->x0, precision_digits);
  attlyValueAsString = gap_base_gdouble_to_ascii_string(perCoords->y0, precision_digits);
  attrxValueAsString = gap_base_gdouble_to_ascii_string(perCoords->x1, precision_digits);
  attryValueAsString = gap_base_gdouble_to_ascii_string(perCoords->y1, precision_digits);
  atblxValueAsString = gap_base_gdouble_to_ascii_string(perCoords->x2, precision_digits);
  atblyValueAsString = gap_base_gdouble_to_ascii_string(perCoords->y2, precision_digits);
  atbrxValueAsString = gap_base_gdouble_to_ascii_string(perCoords->x3, precision_digits);
  atbryValueAsString = gap_base_gdouble_to_ascii_string(perCoords->y3, precision_digits);

  /* offsets are top left corner of the resulting new layer size */
  px = rint(MIN(perCoords->x0, perCoords->x2));
  py = rint(MIN(perCoords->y0, perCoords->y1));

  if(valPtr->enableScaling)  // TODO have own boolean enableTuning
  {
    gchar tunedYN;
    
    tunedYN = 'Y';
    if ((utrkPtr[0]->px == trkPtr[0]->px)
    &&  (utrkPtr[0]->py == trkPtr[0]->py)
    &&  (utrkPtr[1]->px == trkPtr[1]->px)
    &&  (utrkPtr[1]->py == trkPtr[1]->py)
    &&  (utrkPtr[2]->px == trkPtr[2]->px)
    &&  (utrkPtr[2]->py == trkPtr[2]->py)
    &&  (utrkPtr[3]->px == trkPtr[3]->px)
    &&  (utrkPtr[3]->py == trkPtr[3]->py))
    {
      tunedYN = 'N';
    }
    
    /* XML with both tuned and untuned coordinates (u1x,u1y ... for test and analyse purpose) */
    logString = g_strdup_printf(
          "    <controlpoint px=\"%04d\" py=\"%04d\" "
          "ttlx=\"%s\" ttly=\"%s\" ttrx=\"%s\" ttry=\"%s\" "
          "tblx=\"%s\" tbly=\"%s\" tbrx=\"%s\" tbry=\"%s\" "
          "attlx=\"%s\" attly=\"%s\" attrx=\"%s\" attry=\"%s\" "
          "atblx=\"%s\" atbly=\"%s\" atbrx=\"%s\" atbry=\"%s\" "
          "keyframe_abs=\"%06d\" "
          "tuned=\"%c\" "
          "u1x=\"%04d\"  u1y=\"%04d\"  u2x=\"%04d\" u2y=\"%04d\" "
          "u3x=\"%04d\"  u3y=\"%04d\"  u4x=\"%04d\" u4y=\"%04d\" "
          "p1x=\"%04d\"  p1y=\"%04d\"  p2x=\"%04d\" p2y=\"%04d\" "
          "p3x=\"%04d\"  p3y=\"%04d\"  p4x=\"%04d\" p4y=\"%04d\" "
          "s1x=\"%04d\" s1y=\"%04d\" s2x=\"%04d\" s2y=\"%04d\" "
          "s3x=\"%04d\" s3y=\"%04d\" s4x=\"%04d\" s4y=\"%04d\" />"
       , px, py
       , ttlxValueAsString, ttlyValueAsString, ttrxValueAsString, ttryValueAsString
       , tblxValueAsString, tblyValueAsString, tbrxValueAsString, tbryValueAsString
       , attlxValueAsString, attlyValueAsString, attrxValueAsString, attryValueAsString
       , atblxValueAsString, atblyValueAsString, atbrxValueAsString, atbryValueAsString
       , frameNr
       , tunedYN
       , utrkPtr[0]->px
       , utrkPtr[0]->py
       , utrkPtr[1]->px
       , utrkPtr[1]->py
       , utrkPtr[2]->px
       , utrkPtr[2]->py
       , utrkPtr[3]->px
       , utrkPtr[3]->py
       , trkPtr[0]->px
       , trkPtr[0]->py
       , trkPtr[1]->px
       , trkPtr[1]->py
       , trkPtr[2]->px
       , trkPtr[2]->py
       , trkPtr[3]->px
       , trkPtr[3]->py
       , refPtr[0]->px
       , refPtr[0]->py
       , refPtr[1]->px
       , refPtr[1]->py
       , refPtr[2]->px
       , refPtr[2]->py
       , refPtr[3]->px
       , refPtr[3]->py
       );
  }
  else
  {
    /* XML without untuned coordinates */
    logString = g_strdup_printf(
          "    <controlpoint px=\"%04d\" py=\"%04d\" "
          "ttlx=\"%s\" ttly=\"%s\" ttrx=\"%s\" ttry=\"%s\" "
          "tblx=\"%s\" tbly=\"%s\" tbrx=\"%s\" tbry=\"%s\" "
          "attlx=\"%s\" attly=\"%s\" attrx=\"%s\" attry=\"%s\" "
          "atblx=\"%s\" atbly=\"%s\" atbrx=\"%s\" atbry=\"%s\" "
          "keyframe_abs=\"%06d\" "
          "p1x=\"%04d\"  p1y=\"%04d\"  p2x=\"%04d\" p2y=\"%04d\" "
          "p3x=\"%04d\"  p3y=\"%04d\"  p4x=\"%04d\" p4y=\"%04d\" "
          "s1x=\"%04d\" s1y=\"%04d\" s2x=\"%04d\" s2y=\"%04d\" "
          "s3x=\"%04d\" s3y=\"%04d\" s4x=\"%04d\" s4y=\"%04d\" />"
       , px, py
       , ttlxValueAsString, ttlyValueAsString, ttrxValueAsString, ttryValueAsString
       , tblxValueAsString, tblyValueAsString, tbrxValueAsString, tbryValueAsString
       , attlxValueAsString, attlyValueAsString, attrxValueAsString, attryValueAsString
       , atblxValueAsString, atblyValueAsString, atbrxValueAsString, atbryValueAsString
       , frameNr
       , trkPtr[0]->px
       , trkPtr[0]->py
       , trkPtr[1]->px
       , trkPtr[1]->py
       , trkPtr[2]->px
       , trkPtr[2]->py
       , trkPtr[3]->px
       , trkPtr[3]->py
       , refPtr[0]->px
       , refPtr[0]->py
       , refPtr[1]->px
       , refPtr[1]->py
       , refPtr[2]->px
       , refPtr[2]->py
       , refPtr[3]->px
       , refPtr[3]->py
       );
  }
  
  g_free(ttlxValueAsString);
  g_free(ttlyValueAsString);
  g_free(ttrxValueAsString);
  g_free(ttryValueAsString);
  g_free(tblxValueAsString);
  g_free(tblyValueAsString);
  g_free(tbrxValueAsString);
  g_free(tbryValueAsString);

  g_free(attlxValueAsString);
  g_free(attlyValueAsString);
  g_free(attrxValueAsString);
  g_free(attryValueAsString);
  g_free(atblxValueAsString);
  g_free(atblyValueAsString);
  g_free(atbrxValueAsString);
  g_free(atbryValueAsString);

  if ((valPtr->moveLogFile[0] == '\0')
  ||  (valPtr->moveLogFile[0] == '-'))
  {
    printf("%s\n", logString);
  }
  else
  {
    p_log_to_file(&valPtr->moveLogFile[0], logString
                , frameNr
                , FALSE  /* center */
                , width
                , height
                , valPtr
                );
  }

  if (logString)
  {
    g_free(logString);
  }


}  /* end p_coords_tune_and_logging_perspective */
  

/* --------------------------------------------------
 * p_pickNearestToCorners
 * --------------------------------------------------
 * pick the 4 indexes of reference coordinate point that are the nearest 
 * to the 4 corners.
 * 
 * Notes on the coordinate status:
 *   Points with status != 0 are ignored
 *                           (-1 indicates invalid coordinates
 *                           (+1 indicates already picked coordinates)
 *
 *   The picked reference coordinates are marked with status 1 in this procedure
 */
static void
p_pickNearestToCorners(BestIndexes *bestIndexes, gint32 width, gint32 height
     , PixelCoordsArray *startCoordsArray)
{
  gint    idn;
  gint    idcPick;  /* index of the selected corner where 0 = topLeft, 1 = TopRight, 2 = BottmLeft, 3 = BottomRight */
  gint    pickIdx;
  
  gdouble   minSqDist;
  gdouble   minSqDistCorner[4];
  gboolean  cornerSelected[4];
  gint      cornerSelectCount;



  /* pick 4 coordinate near ideally near to the 4 corners in loop for idn = 0 to 3 */

  cornerSelected[0] = FALSE;
  cornerSelected[1] = FALSE;
  cornerSelected[2] = FALSE;
  cornerSelected[3] = FALSE;
  cornerSelectCount = 0;

  
  for(idn = 0; idn < 4; idn++)
  {
    gint    idx;
  
    minSqDist = (width + height) * (width + height);
    minSqDistCorner[0] = minSqDist;
    minSqDistCorner[1] = minSqDist;
    minSqDistCorner[2] = minSqDist;
    minSqDistCorner[3] = minSqDist;
  
    idcPick = -1;
    pickIdx = -1;  /* indicates: "could not pick a valid point" */
    for(idx=0; idx < GAP_ALIGN_COORDS_MAX; idx++)
    {
      gdouble sqDist[4];
      gint idc;

      if(startCoordsArray->pixCoord[idx].status != 0)
      {
        continue;  /* Skip already selected and unusable coorinates */
      }

    
      /* square distance to all 4 corners */
      sqDist[0] = gap_geo_calculateSqrDistX2Y2(&startCoordsArray->pixCoord[idx], 0.0, 0.0);
      sqDist[1] = gap_geo_calculateSqrDistX2Y2(&startCoordsArray->pixCoord[idx], (gdouble)width, 0.0);
      sqDist[2] = gap_geo_calculateSqrDistX2Y2(&startCoordsArray->pixCoord[idx], 0.0, (gdouble)height);
      sqDist[3] = gap_geo_calculateSqrDistX2Y2(&startCoordsArray->pixCoord[idx], (gdouble)width, (gdouble)height);
    
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
            pickIdx = idx;
          }
        }
      }
    }      /* end for idx loop over all available coordinates */
    
    if (idcPick >= 0)
    {
      cornerSelectCount++;
      cornerSelected[idcPick] = TRUE;
      bestIndexes->bestIdx[idcPick] = pickIdx;
      startCoordsArray->pixCoord[pickIdx].status = 1; /* mark this coord as already selected */
    }
  }


}  /* end p_pickNearestToCorners */


/* ----------------------------
 * p_select_best_coords
 * ----------------------------
 * pick 1, 2 or 4 best matching coordinates,
 * depending on valPtr->numPointsSelect that determines
 * the type of wanted camera shake compensation.
 *   where 4 is 4point perspective mode
 *   where 2 is 2point Scale/rotate and Move mode
 *   where 1 is 1point simple Move mode
 *
 * weight depends on average colordiff (while locating the coordinate)
 * and distance (the longer the better for good precision 
 *  while calcualting rotation angle and scaling)
 */
static void
p_select_best_coords(gint32 imageId, gint32 frameNr, PixelCoordsArray *currCoordsArray
  , PixelCoordsArray *startCoordsArray, FilterValues *valPtr
  , BestIndexes *bestIndexes, FrameHistInfo *frameHistInfo 
  )
{
#define MAX_AVG_LOOPS 4
  gint idx1;
  gint idx2;
  gint soloPickIdx1;
  gint pickIdx1;
  gint pickIdx2;
  gint numPoints;
  gdouble sqrDistance;
  gdouble quality;          /* 1.0 is best 0.0 is worst quality */
  gdouble soloQuality;      /* 1.0 is best 0.0 is worst quality */
  gdouble weight;
  gdouble maxWeight;
  gdouble maxSoloQuality;
  gint32 sumOffsX;
  gint32 sumOffsY;
  gint32 numValidOffsets;
  gdouble avgOffsX;
  gdouble avgOffsY;
  gint32 numValidOffsets1;
  gdouble avgOffsX1;
  gdouble avgOffsY1;
  gint32  pixelMovementTolerance;
  GapPixelCoords  *currCoords;
  GapPixelCoords  *startCoords;
  gint32 moveOffsetX;
  gint32 moveOffsetY;
  gint32 validPointsCount;
  gint32 width;
  gint32 height;
        
  maxWeight = 0.0;
  maxSoloQuality = 0.0;
  
  soloPickIdx1 = 0;
  pickIdx1 = 0;
  pickIdx2 = 1;
  numPoints = MIN(currCoordsArray->numberOfCoords , startCoordsArray->numberOfCoords);
  
  width = gimp_image_width(imageId);
  height = gimp_image_height(imageId);

  /* -------------------- 4 point perspective mode --------------- */
  if(valPtr->numPointsSelect == 4)
  {
     gint idx;
     
     /* for 4point perspective mode there are other criteria "whats the best set of points"
      * p0 shall be the point with best quality near the upper left corner
      * p1 shall be the point with best quality near the upper right corner
      * p2 shall be the point with best quality near the lower left corner
      * p3 shall be the point with best quality near the lower right corner
      *
      * TODO
      *   .. think about improved pick method that prefers points with higher quality
      *     .. and still provides 4 points ideally one near each corner.
      */
      for(idx=0; idx < numPoints; idx++)
      {
        startCoordsArray->pixCoord[idx].status = -1;  /* indicates unusable pair */
        if((startCoordsArray->pixCoord[idx].valid == TRUE)
        && (currCoordsArray->pixCoord[idx].valid == TRUE))
        {
          startCoordsArray->pixCoord[idx].status = 0;  /* indicates selectable pair */
        }
      }
      
      p_pickNearestToCorners(bestIndexes, width, height, startCoordsArray);
      
      return;

  }

  /* -------------------- 1 point simple move mode --------------- */
  if(valPtr->numPointsSelect == 1)
  {
    gdouble bestQuality = 0.0;
    
    /* standard method: 
     * Singlepoint mode picks the point with best quality in case there are more points available.
     */
    for(idx1 = 0; idx1 < numPoints; idx1++)
    {
      quality = p_getPixelCoordsQuality(&currCoordsArray->pixCoord[idx1]);
      if (quality > bestQuality)
      {
        bestQuality = quality;
        pickIdx1 = idx1;
      }
    }
    
    /* optional extended method: 
     * In case there are more points with top quality (>= 99% compared with best quality)
     * prefere the point with the same index that was picked in the previous handled frame when possible.
     */
    if(valPtr->enableScaling)  // TODO have own boolean enableTuning
    {
      gdouble nearlyBestQuality;
      
      nearlyBestQuality = bestQuality * 0.997;
      pickIdx1 = -1;
      for(idx1 = 0; idx1 < numPoints; idx1++)
      {
        currCoords = &currCoordsArray->pixCoord[idx1];
        quality = p_getPixelCoordsQuality(currCoords);
      
        if(gap_debug)
        {
          printf("frameNr:%d  idx1:%d [%d, %d] histBestIdx:%d quality:%f nearlyBestQuality:%f bestQuality:%f\n"
            , (int)frameNr
            , (int)idx1
            , (int)currCoords->px
            , (int)currCoords->py
            , (int)frameHistInfo->bestIdx[0]
            , (float)quality
            , (float)nearlyBestQuality
            , (float)bestQuality
            );
        }
      
        if ((quality >= nearlyBestQuality) && (frameNr > 2))
        {
          if(idx1 == frameHistInfo->bestIdx[0])
          {
            pickIdx1 = idx1;
          }
        }

        /* in case there are more points with bestQuality pick only the 1st of them */
        if ((quality == bestQuality) && (pickIdx1 < 0))
        {
          pickIdx1 = idx1;
        }
      }
    }
    
    bestIndexes->bestIdx[0] = MAX(0, pickIdx1);
    bestIndexes->bestIdx[1] = -1;   // not used in 1point compensation
    bestIndexes->bestIdx[2] = -1;   // not used in 1point compensation
    bestIndexes->bestIdx[3] = -1;   // not used in 1point compensation

    if(gap_debug)
    {
        printf("frameNr:%d  picked idx:%d  histBestIdx:%d  bestQuality:%f\n"
          , (int)frameNr
          , (int)bestIndexes->bestIdx[0]
          , (int)frameHistInfo->bestIdx[0]
          , (float)bestQuality
          );
    }
  
    return;
  }

  /* calculate average offsets (movemnet of x and y axis) 
   * in the 2nd and further outer loops eliminate extreme values
   */
  pixelMovementTolerance = MAX(5, valPtr->targetMoveRadius / 8);
  avgOffsX = 0.0;
  avgOffsY = 0.0;
  avgOffsX1 = 0.0;
  avgOffsY1 = 0.0;
  
  for(idx2 = 0; idx2 < MAX_AVG_LOOPS; idx2++)
  {
    sumOffsX = 0;
    sumOffsY = 0;
    numValidOffsets = 0;
    validPointsCount = 0;
    for(idx1 = 0; idx1 < numPoints; idx1++)
    {

      currCoords = &currCoordsArray->pixCoord[idx1];
      startCoords = &startCoordsArray->pixCoord[idx1];
      if (startCoords->valid)
      {
        if(currCoords->valid)
        {
          gboolean isPlausible;
          moveOffsetX = currCoords->px - startCoords->px;
          moveOffsetY = currCoords->py - startCoords->py;
          
          validPointsCount++;
          isPlausible = FALSE;
          if (idx2 == 0) 
          {
            /* in the 1st outer loop all valid coords are plausible
             * for calculation of an an initial average value
             */
            isPlausible = TRUE; 
          }
          else if ( (abs(moveOffsetX - avgOffsX) < pixelMovementTolerance)
               &&   (abs(moveOffsetY - avgOffsY) < pixelMovementTolerance))
          {
            /* only coordinates that have similar movement vektors as the average
             * are part of the final average calculation (that eliminates extreme values)
             */
            isPlausible = TRUE; 
          }
        
          if (isPlausible)
          {
            sumOffsX += moveOffsetX;
            sumOffsY += moveOffsetY;
            numValidOffsets++;
          }
        }
      }  
    }
    if (numValidOffsets > 0)
    {
      avgOffsX = (gdouble)sumOffsX / (gdouble)numValidOffsets;
      avgOffsY = (gdouble)sumOffsY / (gdouble)numValidOffsets;
      
      if (idx2 == 0)
      {
        avgOffsX1 = avgOffsX;
        avgOffsY1 = avgOffsY;
        numValidOffsets1 = numValidOffsets;
      }
      else if (numValidOffsets > 2)
      {
        /* we have average X/Y values based on more than 2 points
         * that shall be sufficient to detect points with extreme movement
         */
        break;
      }
      else if (idx2 == MAX_AVG_LOOPS -1)
      {
        /* no more attempts planed, therefore keep the current pixelMovementTolerance */
        break;
      }
      else
      {
        /* the average offsets is based on just one or two points.
         * which is not a good base for detection of extreme movement values.
         * in this case try another loop with increased tolerance
         * to get more points involved in the average calculation.
         */
        pixelMovementTolerance += (pixelMovementTolerance /2);
      }
    }
    else
    {
      /* no points are available for average movement calculation */
      if (validPointsCount > 1)
      {
         /* none of the valid points has movement vektor near the average value
          * therefore try another loop with increased tolerance
          */
        pixelMovementTolerance += (pixelMovementTolerance /2);
      }
      else
      {
        break;  /* escape from loop because less than 2 valid point are available */
      }
    }
  }

  currCoordsArray->numValidOffsets = numValidOffsets;
  currCoordsArray->avgOffsX = avgOffsX;
  currCoordsArray->avgOffsY = avgOffsY;
  
  /* weight calculation loops to select best matching coordinates */
  for(idx1 = 0; idx1 < numPoints; idx1++)
  {
  
    soloQuality = p_getPixelCoordsQuality(&currCoordsArray->pixCoord[idx1])
                * p_getPixelCoordsQuality(&startCoordsArray->pixCoord[idx1]);
    if (soloQuality <= 0.0)
    {
      continue;
    }

    if (numValidOffsets > 1)
    {
      /* there are 2 or more valid coords available
       * in this case eliminate coords with extreme different movement vektors
       */
      currCoords = &currCoordsArray->pixCoord[idx1];
      startCoords = &startCoordsArray->pixCoord[idx1];
      moveOffsetX = currCoords->px - startCoords->px;
      moveOffsetY = currCoords->py - startCoords->py;
      if ( (abs(moveOffsetX - avgOffsX) > pixelMovementTolerance)
      ||   (abs(moveOffsetY - avgOffsY) > pixelMovementTolerance))
      {
        continue;
      }
    }



    if (soloQuality > maxSoloQuality) 
    {
      maxSoloQuality = soloQuality;
      soloPickIdx1 = idx1;
    }
    
    if (currCoordsArray->pixCoord[idx1].valid != TRUE)
    {
      continue;  /* skip further weight checks when invalid points are involved */
    }

    for(idx2 = idx1 + 1; idx2 < numPoints; idx2++)
    {
       if (currCoordsArray->pixCoord[idx2].valid)
       {
         if (numValidOffsets > 1)
         {
           /* there are 2 or more valid coords available
            * in this case eliminate coords with extreme different movement vektors
            */
           currCoords = &currCoordsArray->pixCoord[idx2];
           startCoords = &startCoordsArray->pixCoord[idx2];
           moveOffsetX = currCoords->px - startCoords->px;
           moveOffsetY = currCoords->py - startCoords->py;
           if ( (abs(moveOffsetX - avgOffsX) > pixelMovementTolerance)
           ||   (abs(moveOffsetY - avgOffsY) > pixelMovementTolerance))
           {
             continue;
           }
         }



         quality = soloQuality * p_getPixelCoordsQuality(&currCoordsArray->pixCoord[idx2])
               * p_getPixelCoordsQuality(&startCoordsArray->pixCoord[idx2]);
         sqrDistance = gap_geo_calculateSqrDist(&currCoordsArray->pixCoord[idx1], &currCoordsArray->pixCoord[idx2]);
       
         /* operate with the square distance for performance reason
          * therfore also use square quality to compensate..
          */
         weight = sqrDistance * quality * quality;
         if (weight > maxWeight)
         {
           maxWeight = weight;
           pickIdx1 = idx1;
           pickIdx2 = idx2;
         }
       }
    }
  }
  
  if (maxWeight <= 0.0)
  {
    pickIdx1 = soloPickIdx1;
  }
  

  bestIndexes->bestIdx[0] = pickIdx1;
  bestIndexes->bestIdx[1] = pickIdx2;
  bestIndexes->bestIdx[2] = -1;   // not used in 2point compensation
  bestIndexes->bestIdx[3] = -1;   // not used in 2point compensation

  if(gap_debug)
  {
    for(idx1 = 0; idx1 < numPoints; idx1++)
    {
      gdouble qp1;
      gdouble qs1;
      qp1 = p_getPixelCoordsQuality(&currCoordsArray->pixCoord[idx1]);
      qs1 = p_getPixelCoordsQuality(&startCoordsArray->pixCoord[idx1]);
           
      currCoords = &currCoordsArray->pixCoord[idx1];
      startCoords = &startCoordsArray->pixCoord[idx1];
      moveOffsetX = currCoords->px - startCoords->px;
      moveOffsetY = currCoords->py - startCoords->py;

      printf("p_select_best_coords [%d] curr[].px:%d curr[].py:%d curr[].quality:%.4f "
             "  start[].px:%d start[].py:%d start[].quality:%.4f mvX:%d mvY:%d"
        , (int) idx1
        , currCoordsArray->pixCoord[idx1].px
        , currCoordsArray->pixCoord[idx1].py
        , (float)qp1
        , startCoordsArray->pixCoord[idx1].px
        , startCoordsArray->pixCoord[idx1].py
        , (float)qs1
        , (int)moveOffsetX
        , (int)moveOffsetY
        );
      
      if ((qp1 <= 0.0) || (qs1 <= 0.0))
      {
        printf("\n");
      }
      else if ( (abs(moveOffsetX - avgOffsX) > pixelMovementTolerance)
      ||   (abs(moveOffsetY - avgOffsY) > pixelMovementTolerance))
      {
        printf("(Movement extremeValue > Tolerance: %d) \n"
          ,(int)pixelMovementTolerance
          );
      }
      else
      {
        printf("(Movement typical) \n");
      }

    }
    
    printf("p_select_best_coords: frameNr:%d bestIdx1:%d bestIdx2:%d maxWeight:%.4f avgOffst1 X:%.4f Y:%.4f numValidOffsets:%d avgOffst X:%.4f Y:%.4f\n"
      , (int)frameNr
      , (int)pickIdx1
      , (int)pickIdx2
      , (float)maxWeight
      , (float)avgOffsX1
      , (float)avgOffsY1
      , (int)numValidOffsets
      , (float)avgOffsX
      , (float)avgOffsY
      );
  }
  
}  /* end p_select_best_coords */


/* ----------------------------
 * p_selective_coords_logging
 * ----------------------------
 * log the best selected coordinates to stdout
 * or to move-path controlpoint XML file.
 * Further store the best selected information in the frame history.
 *
 * Note that the controlpoint XML file has additional information
 * that is not relevant for the move-path tool but is used
 * in the detail-align tool (that is an alternative option
 * stabilze video frames based on the tracked XML data)
 */
static gint32  
p_selective_coords_logging(FrameHistInfo *frameHistInfo 
		   , PixelCoordsArray *currCoordsArray
		   , PixelCoordsArray *startCoordsArray
		   , PixelCoordsArray *prevCoordsArray
		   , FilterValues *valPtr
		   , gint32 imageId
		   , gint32 activeDrawableId
		   , gint32 referenceLayerId
                 )
{
  BestIndexes bestIndexes;
  gint32 frameNr;

  frameNr = frameHistInfo->frameNr;
  p_select_best_coords(imageId, frameNr
  , currCoordsArray
  , startCoordsArray
  , valPtr
  , &bestIndexes
  , frameHistInfo
  );

  if(valPtr->numPointsSelect == 4)
  {
    /* perspective logging handles only the 4point variant */
    p_coords_tune_and_logging_perspective(frameNr
                                ,currCoordsArray
                                ,startCoordsArray
                                , prevCoordsArray
                                ,&bestIndexes
                                , valPtr
                                , imageId
                                , activeDrawableId
                                , referenceLayerId
                                );
  }
  else
  {
    p_coords_logging(frameNr
                  , &currCoordsArray->pixCoord[bestIndexes.bestIdx[0]]
                  , &currCoordsArray->pixCoord[bestIndexes.bestIdx[1]]
                  , &startCoordsArray->pixCoord[bestIndexes.bestIdx[0]]
                  , &startCoordsArray->pixCoord[bestIndexes.bestIdx[1]]
                  , valPtr
                  , imageId
                  , &startCoordsArray->pixCoord[0]
                  , bestIndexes.bestIdx[0]
                  , bestIndexes.bestIdx[1]
                  );
  }
  frameHistInfo->bestIdx[0] = bestIndexes.bestIdx[0];
  frameHistInfo->bestIdx[1] = bestIndexes.bestIdx[1];
  frameHistInfo->bestIdx[2] = bestIndexes.bestIdx[2];
  frameHistInfo->bestIdx[3] = bestIndexes.bestIdx[3];

  return (bestIndexes.bestIdx[0]);
  
}  /* end p_selective_coords_logging */


/* --------------------------------
 * p_parse_frame_nr_from_layer_name
 * --------------------------------
 */
static gint32
p_parse_frame_nr_from_layer_name(gint32 layerId)
{
  char     *layername;
  gint32    frameNr;
  gint      len;
  gint      ii;



  frameNr = 0;

  layername = gimp_item_get_name(layerId);
  if(layername)
  {
    len = strlen(layername);
    for(ii=1; ii < len; ii++)
    {
      if ((layername[ii-1] == '_')
      &&  (layername[ii] <= '9')
      &&  (layername[ii] >= '0'))
      {
        frameNr = g_ascii_strtod(&layername[ii], NULL);
        break;
      }
    }
    g_free(layername);
  }

  return (frameNr);

}  /* end p_parse_frame_nr_from_layer_name */


/* -------------------------------
 * p_get_frameHistInfo
 * -------------------------------
 */
static void
p_get_frameHistInfo(FrameHistInfo *frameHistInfo, gint32 imageId)
{
  GimpParasite  *l_parasite;
  int l_len;
  GapPixelCoords *startCoords;
  

  frameHistInfo->workImageId = -1;
  frameHistInfo->frameNr = 0;
  frameHistInfo->trackedFramesCount = 0;
  frameHistInfo->lostTraceCount = 0;
  frameHistInfo->startCoordsArray.numberOfCoords = 0;
  frameHistInfo->bestIdx[0] = -1;
  frameHistInfo->bestIdx[1] = -1;
  frameHistInfo->bestIdx[2] = -1;
  frameHistInfo->bestIdx[3] = -1;

  startCoords = &frameHistInfo->startCoordsArray.pixCoord[0];
  startCoords->valid = FALSE;
  startCoords->px = 0;
  startCoords->py = 0;


  l_parasite = gimp_image_parasite_find(imageId, GAP_DETAIL_FRAME_HISTORY_INFO);
  if(l_parasite)
  {
    l_len = l_parasite->size;
      
    
    if(gap_debug)
    {
        printf("p_get_frameHistInfo: %s len:%d sizeof(FrameHistInfo):%d\n"
           , GAP_DETAIL_FRAME_HISTORY_INFO
           , (int)l_len
           , (int)sizeof(FrameHistInfo)
           );
    }
    
    
    if (l_len == sizeof(FrameHistInfo))
    {
      //// gimp_get_data(GAP_DETAIL_FRAME_HISTORY_INFO, frameHistInfo);
      
      /* copy (uchar) data from parasite to frameHistInfo (structure) */
      memcpy(frameHistInfo, l_parasite->data, l_parasite->size);
    
      if(gap_debug)
      {
        GapPixelCoords *prevCoords;
        
        startCoords = &frameHistInfo->startCoordsArray.pixCoord[0];
        prevCoords  = &frameHistInfo->prevCoordsArray.pixCoord[0];
    
        printf("p_get_frameHistInfo: %s  frameNr:%d px:%d py:%d valid:%d\n"
               "                     prevPx:%d prevPy:%d prevValid:%d lostTraceCount:%d  trackedFramesCount:%d\n"
          , GAP_DETAIL_FRAME_HISTORY_INFO
          , (int)frameHistInfo->frameNr
          , (int)startCoords->px
          , (int)startCoords->py
          , (int)startCoords->valid
          , (int)prevCoords->px
          , (int)prevCoords->py
          , (int)prevCoords->valid
          , (int)frameHistInfo->lostTraceCount
          , (int)frameHistInfo->trackedFramesCount
          );
      }
    
    }

    gimp_parasite_free(l_parasite);
    
  }

}  /* end p_get_frameHistInfo */


/* -------------------------------
 * p_set_frameHistInfo
 * -------------------------------
 * store frame history information as temporary image parasite data
 * (for the next run with the same image in the same gimp session)
 */
static void
p_set_frameHistInfo(FrameHistInfo *frameHistInfo, gint32 imageId)
{
  GimpParasite  *l_parasite;

  if(gap_debug)
  {
      GapPixelCoords *startCoords;
      GapPixelCoords *prevCoords;
      
      startCoords = &frameHistInfo->startCoordsArray.pixCoord[0];
      prevCoords  = &frameHistInfo->prevCoordsArray.pixCoord[0];
      
      printf("p_SET_frameHistInfo: %s  frameNr:%d px:%d py:%d valid:%d  prevPx:%d prevPy:%d prevValid:%d\n\n"
        , GAP_DETAIL_FRAME_HISTORY_INFO
        , (int)frameHistInfo->frameNr
        , (int)startCoords->px
        , (int)startCoords->py
        , (int)startCoords->valid
        , (int)prevCoords->px
        , (int)prevCoords->py
        , (int)prevCoords->valid

        );
  }

  /* attach a parasite to store frame histroy information for detail tracking */
  l_parasite = gimp_parasite_new(GAP_DETAIL_FRAME_HISTORY_INFO
                                 ,0  /* GIMP_PARASITE_PERSISTENT  0 for non persistent */
                                 ,sizeof(FrameHistInfo)  /* parasite->size */
                                 ,frameHistInfo          /* parasite->data */
                                 );

  if(l_parasite)
  {
    gimp_image_parasite_attach(imageId, l_parasite);
    gimp_parasite_free(l_parasite);
  }

}  /* end p_set_frameHistInfo */


/* -------------------------------
 * p_set_n_vector_points
 * -------------------------------
 * if the image already contains a vectors object with the specified vectorsName
 * then  remove all strokes from the vectors object and replace it with the 
 * points in the targetCoordsArray.
 * 
 * in case there is no vectors object with the specified vectorsName create it and add it to the image.
 *
 * if setGuides is TRUE
 * then set guide lines crossing at the target coords[guideIdx] for better visualisation.
 *  (note that path vectors will be not visible in case the path contains just one single point)
 *
  returns the vectorsId
 */
static gint32
p_set_n_vector_points(gint32 imageId, PixelCoordsArray *targetCoordsArray, gchar *vectorsName
   , gboolean setVisible, gboolean setGuides, gint32 guideIdx)
{
  gint32  vectorsId;
  /* gint    newStrokeId; */

  gint32    showGuideIdx;
  gdouble  *points;
  gint      num_points;
  gint      l_idx;
  gboolean  closed;
  GimpVectorsStrokeType type;
  GapPixelCoords *targetCoords;
  
 
  showGuideIdx = CLAMP(guideIdx, 0, targetCoordsArray->numberOfCoords -1);
  targetCoords  = &targetCoordsArray->pixCoord[showGuideIdx];

  if(setGuides == TRUE)
  {
    gimp_image_add_hguide(imageId, targetCoords->py);
    gimp_image_add_vguide(imageId, targetCoords->px);
  }


  if(gap_debug)
  {
    if(setGuides)
    {
      printf("\np_set_n_vector_points vectorsName:%s\n  numberOfCoords:%d  guideIdx:%d showGuideIdx:%d\n"
        , vectorsName
        , (int)targetCoordsArray->numberOfCoords
        , (int)guideIdx
        , (int)showGuideIdx
        );
      for(l_idx = 0; l_idx < targetCoordsArray->numberOfCoords; l_idx++)
      {
        gdouble pdx;
        gdouble pdy;
        targetCoords  = &targetCoordsArray->pixCoord[l_idx];
        if (targetCoords->valid)
        {
           pdx = targetCoords->px;
           pdy = targetCoords->py;
        }
        else
        {
           pdx = 0;
           pdy = 0;
        }
      
        printf("pdx[%d] : %.2f  pdy[%d] : %.2f\n"
          , l_idx
          , (float)pdx
          , l_idx
          , (float)pdy
          );
    
      }
    }
  }

  vectorsId = gimp_image_get_vectors_by_name(imageId, vectorsName);
  if(vectorsId >= 0)
  {
//     gint num_strokes;
//     gint *strokes;
// 
//     strokes = gimp_vectors_get_strokes (vectorsId, &num_strokes);
//     if(strokes)
//     {
//       if(num_strokes > 0)
//       {
//         gint ii;
//         for(ii=0; ii < num_strokes; ii++)
//         {
//           gimp_vectors_remove_stroke(vectorsId, strokes[ii]);
//         }
//       }
//       g_free(strokes);
// 
//     }
    gimp_image_remove_vectors(imageId, vectorsId);
  } 

  /* create new vectors path */
  vectorsId = gimp_vectors_new(imageId, vectorsName);

  
  if(vectorsId >= 0)  
  {
    num_points = 6 * targetCoordsArray->numberOfCoords;
    points = g_new (gdouble, num_points);
    
    for(l_idx = 0; l_idx < targetCoordsArray->numberOfCoords; l_idx++)
    {
      gdouble pdx;
      gdouble pdy;
      gint    offset;
      
      offset = l_idx * 6;
      targetCoords  = &targetCoordsArray->pixCoord[l_idx];
      if (targetCoords->valid)
      {
         pdx = targetCoords->px;
         pdy = targetCoords->py;
      }
      else
      {
         pdx = 0;
         pdy = 0;
      }
      points[0 + offset] = pdx;
      points[1 + offset] = pdy;
      points[2 + offset] = pdx;
      points[3 + offset] = pdy;
      points[4 + offset] = pdx;
      points[5 + offset] = pdy;
    }

    
    closed = FALSE;
    type = GIMP_VECTORS_STROKE_TYPE_BEZIER;
    /* newStrokeId = */ gimp_vectors_stroke_new_from_points (vectorsId
                                     , type
                                     , num_points
                                     , points
                                     , closed
                                     );
    g_free(points);
    
    gimp_image_insert_vectors(imageId, vectorsId, -1, 0);
    gimp_item_set_visible(vectorsId, setVisible);

  }

  return (vectorsId);
  
}  /* end p_set_n_vector_points */




/* --------------------------------------------------
 * p_set_debugCoords_from_IntersectionPoints
 * --------------------------------------------------
 * caclulate intersection points of the specified debugLine with the 2 specified borderLines
 * and set the 2 resulting intersection coordinates rounded to pixel coordinates in the 
 *  Output debugCoordsArray
 *   The resulting output builds the extended debug line connecting the 2 borderLines
 */
static void
p_set_debugCoords_from_IntersectionPoints(gint32 frameNr, gchar *vectorName
  , PixelCoordsArray *debugCoordsArray
  , GapLineDescriptionConsts *debugLine
  , GapLineDescriptionConsts *aBorderLine, GapLineDescriptionConsts *bBorderLine)
{
  GapDoubleCoords aInterPt;
  GapDoubleCoords bInterPt;

  gap_geo_line_intersection(aBorderLine, debugLine,  &aInterPt);
  gap_geo_line_intersection(bBorderLine, debugLine,  &bInterPt);


  debugCoordsArray->numberOfCoords = 2;
  
  debugCoordsArray->pixCoord[0].valid = aInterPt.valid;
  debugCoordsArray->pixCoord[0].px = rint(aInterPt.x);
  debugCoordsArray->pixCoord[0].py = rint(aInterPt.y);
  
  debugCoordsArray->pixCoord[1].valid = bInterPt.valid;
  debugCoordsArray->pixCoord[1].px = rint(bInterPt.x);
  debugCoordsArray->pixCoord[1].py = rint(bInterPt.y);
  
  if(gap_debug)
  {
    printf("frameNr: %d  vectorName:%s p[0] x:%.3f y:%.3f  p[1] x:%.3f y:%.3f\n"
      , (int)frameNr
      , vectorName
      , (float)aInterPt.x
      , (float)aInterPt.y
      , (float)bInterPt.x
      , (float)bInterPt.y
      );
  }

}  /* end p_set_debugCoords_from_IntersectionPoints */



/* --------------------------------------------------
 * p_set_debug_intersection_vectors
 * --------------------------------------------------
 * 
 */
static void
p_set_debug_intersection_vectors(gint32 frameNr, gint32 imageId, gchar *vectorBasename
    , PixelCoordsArray *pixelCoordsArray, BestIndexes *bestIndexes)
{
  PixelCoordsArray debugCoordsArray;
  GapLineDescriptionConsts upperBorderLine;
  GapLineDescriptionConsts lowerBorderLine;
  GapLineDescriptionConsts leftBorderLine;
  GapLineDescriptionConsts rightBorderLine;
  GapLineDescriptionConsts debugLine;
  GapPixelCoords  *cordPtr[4];   /* upto 4 coords of first processed (reference) frame  startCoords[4]; */
  gint32 width;
  gint32 height;
  gint   idx;
  gchar *vectorName;


  width = gimp_image_width(imageId);
  height = gimp_image_height(imageId);


  for(idx=0; idx < 4; idx++)
  {
    cordPtr[idx] = NULL;
    if(bestIndexes->bestIdx[0] >= 0)
    {
      cordPtr[idx] = &pixelCoordsArray->pixCoord[bestIndexes->bestIdx[idx]];
    }
    else
    {
      return;  /* stop in case invalid points are detected */
    }
  }

  /* calculate line description for the drawable border lines */
  gap_geo_line_description_from_2Points(0,     0        /* x1,y1 */
                                       ,width, 0        /* x2,y2 */ 
                                       ,&upperBorderLine);
  gap_geo_line_description_from_2Points(0,     height   /* x1,y1 */
                                       ,width, height   /* x2,y2 */
                                       ,&lowerBorderLine);
  gap_geo_line_description_from_2Points(0,     0        /* x1,y1 */
                                       ,0,     height   /* x2,y2 */ 
                                       ,&leftBorderLine);
  gap_geo_line_description_from_2Points(width, 0        /* x1,y1 */
                                       ,width, height   /* x2,y2 */
                                       ,&rightBorderLine);

  
  /* debug line V1 from p0 to p2 */
  vectorName = g_strdup_printf("%s%s", vectorBasename, "V1");
  gap_geo_line_description_from_2GapPixelCoords(cordPtr[0], cordPtr[2], &debugLine);
  p_set_debugCoords_from_IntersectionPoints(frameNr, vectorName, &debugCoordsArray, &debugLine
      , &upperBorderLine
      , &lowerBorderLine
      );
  p_set_n_vector_points(imageId, &debugCoordsArray, vectorName, TRUE, FALSE, 0);
  g_free(vectorName);
  
  /* debug line V2 from p1 to p3 */
  vectorName = g_strdup_printf("%s%s", vectorBasename, "V2");
  gap_geo_line_description_from_2GapPixelCoords(cordPtr[1], cordPtr[3], &debugLine);
  p_set_debugCoords_from_IntersectionPoints(frameNr, vectorName, &debugCoordsArray, &debugLine
      , &upperBorderLine
      , &lowerBorderLine
      );
  p_set_n_vector_points(imageId, &debugCoordsArray, vectorName, TRUE, FALSE, 0);
  g_free(vectorName);



  /* debug line H1 from p0 to p1 */
  vectorName = g_strdup_printf("%s%s", vectorBasename, "H1");
  gap_geo_line_description_from_2GapPixelCoords(cordPtr[0], cordPtr[1], &debugLine);
  p_set_debugCoords_from_IntersectionPoints(frameNr, vectorName, &debugCoordsArray, &debugLine
      , &leftBorderLine
      , &rightBorderLine
      );
  p_set_n_vector_points(imageId, &debugCoordsArray, vectorName, TRUE, FALSE, 0);
  g_free(vectorName);

  /* debug line H1 from p2 to p3 */
  vectorName = g_strdup_printf("%s%s", vectorBasename, "H2");
  gap_geo_line_description_from_2GapPixelCoords(cordPtr[2], cordPtr[3], &debugLine);
  p_set_debugCoords_from_IntersectionPoints(frameNr, vectorName, &debugCoordsArray, &debugLine
      , &leftBorderLine
      , &rightBorderLine
      );
  p_set_n_vector_points(imageId, &debugCoordsArray, vectorName, TRUE, FALSE, 0);
  g_free(vectorName);



}  /* end p_set_debug_intersection_vectors */


/* --------------------------------------------------
 * p_set_debug_vectors
 * --------------------------------------------------
 *
 * set vectors for the lines (both reference and tracking points)
 * extended to the border intersection points.
 * This feature is for analyse purpose only
 * 
 */
static void
p_set_debug_vectors(gint32 frameNr, gint32 imageId
  , PixelCoordsArray *targetCoordsArray
  , PixelCoordsArray *startCoordsArray
  , BestIndexes *bestIndexes)
{
  p_set_debug_intersection_vectors(frameNr, imageId, "Ref", startCoordsArray, bestIndexes);
  p_set_debug_intersection_vectors(frameNr, imageId, "Trk", targetCoordsArray, bestIndexes);


}  /* end p_set_debug_vectors */





/* -----------------------------------
 * gap_track_detail_on_top_layers
 * -----------------------------------
 * This procedure is typically called
 * on the snapshot image created by the Player.
 * This image has one layer at the first snapshot
 * and each further snapshot adds one layer on top of the layerstack.
 *
 * The start is detected when frameHistInfo->trackedFramesCount == 0
 * or whenever the imageId has changed ( frameHistInfo->workImageId )
 * or whenever the image has exactly one layer. (typical at 1st snapshot or when the user has deleted other layers)
 *
 * optionally the numer of layers can be limted
 * to 2 (or 3) layers.
 *
 * Detail tracking output depends on the specified vlPtr->moveLogFile
 * o) output coordinates offests and rotation information 
 *    is written in XML format to the moveLogFile 
 *       if filename is present and has not .xcf extension
 * o) XML output is written to stdout  
 *       if moveLogFile is empty or starts with the '-' character.
 * o) is rendered as vectors path (with the name "TrackingPoints") 
 *       and saved as frame image 
 *       if the specified moveLogFile has .xcf extension
 */
gint32
gap_track_detail_on_top_layers(gint32 imageId, gboolean doProgress, FilterValues *valPtr)
{
  gint      l_idx;
  gint      l_nlayers;
  gint32   *l_layers_list;

  FrameHistInfo  frameHistInfoData;
  FrameHistInfo *frameHistInfo;
  PixelCoordsArray currCoordsArray;
  PixelCoordsArray targetCoordsArray;
  gint32       locateOffsetX[MAX_PIXEL_COORDS_ARRAY];
  gint32       locateOffsetY[MAX_PIXEL_COORDS_ARRAY];
  gint32       successfulTracedPointsCount;
  gint32       previousLostTraceCount;
  gint32       currFrameNr;
  gchar       *l_extension;
  gboolean     isTrackingToFrameImage;
  gint32       bestIdx1;
  BestIndexes  bestIndexes;
  
  
  

  if(gap_debug)
  {
      printf("\ngap_track_detail_on_top_layers: START\n"
             "  numPointsSelect:%d refShapeRadius:%d targetMoveRadius:%d locateColordiff:%.4f\n"
             "  coordsRelToFrame1:%d  offsX:%d offsY:%d removeMidlayers:%d bgLayerIsReference:%d\n"
             "  moveLogFile:%s\n"
            , (int)valPtr->numPointsSelect 
            , (int)valPtr->refShapeRadius
            , (int)valPtr->targetMoveRadius
            , (float)valPtr->loacteColodiffThreshold
            , (int)valPtr->coordsRelToFrame1
            , (int)valPtr->offsX
            , (int)valPtr->offsY
            , (int)valPtr->removeMidlayers
            , (int)valPtr->bgLayerIsReference
            , valPtr->moveLogFile
            );
  }

  frameHistInfo = &frameHistInfoData;

  successfulTracedPointsCount = 0;
  previousLostTraceCount = 0;
  bestIdx1 = 0;
  currCoordsArray.numberOfCoords = 0;
  currCoordsArray.numValidOffsets = 0;
  targetCoordsArray.numValidOffsets = 0;
  for (l_idx = 0; l_idx < MAX_PIXEL_COORDS_ARRAY; l_idx++)
  {
    currCoordsArray.pixCoord[l_idx].valid = FALSE;
    targetCoordsArray.pixCoord[l_idx].valid = FALSE;
    locateOffsetX[l_idx] = 0;
    locateOffsetY[l_idx] = 0;
  }
  
  /* check if detail tracking output is logged to an XML file valPtr->moveLogFile
   * or is written as vectors path and saved as frame image to an .XCF file (if extension .XCF is specified)
   */
  isTrackingToFrameImage = FALSE;
  l_extension = NULL;
  if (valPtr->moveLogFile[0] != '\0')
  {
    l_extension = gap_lib_alloc_extension(&valPtr->moveLogFile[0]);
  }
  if(l_extension != NULL) 
  {
    if ((strcmp(".xcf", l_extension) == 0)
    ||  (strcmp(".XCF", l_extension) == 0))
    {
      isTrackingToFrameImage = TRUE;
    }
    g_free(l_extension);
    l_extension = NULL;
  }  


  p_capture_n_vector_points(imageId, &currCoordsArray, NUMBER_OF_COORDS, VECTORS_NAME_START_REFERENCE_POINTS);
  if (currCoordsArray.numberOfCoords == 0)
  {
    if(gap_debug)
    {
       printf("gap_track_detail_on_top_layers  NO tracking possible because No vectors path was found\n");
    }  
    return (imageId);
  } 
  

  l_layers_list = gimp_image_get_layers(imageId, &l_nlayers);
  if((l_layers_list != NULL)
  && (l_nlayers > 0))
  {
    gint32 topLayerId;
    gint32 belowTopLayerId;
    gint32 refLayerId;

    if(gap_debug)
    {
      int iil;
      gchar *lname;
      for(iil=0; iil < l_nlayers; iil++)
      {
        gint offset_x;
        gint offset_y;
        gimp_drawable_offsets (l_layers_list[iil], &offset_x, &offset_y);
        lname = gimp_item_get_name(l_layers_list[iil]);
        printf(" layerstack[%d] layer_id:%d name:%s size: %d x %d offset_x:%d offset_y:%d\n"
               ,(int)iil
               ,(int)l_layers_list[iil]
               ,lname
               ,(int)gimp_drawable_width(l_layers_list[iil])
               ,(int)gimp_drawable_height(l_layers_list[iil])
               ,(int)offset_x
               ,(int)offset_y
               );
        if(lname != NULL)
        {
          g_free(lname);
        }
      }
    }  


    topLayerId = l_layers_list[0];

    belowTopLayerId = l_layers_list[1];
    refLayerId = belowTopLayerId;
    if (valPtr->bgLayerIsReference == TRUE)
    {
      refLayerId = l_layers_list[l_nlayers -1];
    }

    /// frameHistInfo->frameNr += 1;

    p_get_frameHistInfo(frameHistInfo, imageId);


    if ((frameHistInfo->trackedFramesCount == 0)
    || (frameHistInfo->workImageId != imageId)
    || (l_nlayers == 1))
    {
      if(gap_debug)
      {
        printf("(A) gap_track_detail_on_top_layers  BEGIN tracking l_nlayers:%d trackedFramesCount:%d workImageId:%d imageId:%d\n"
               ,(int)l_nlayers
               ,(int)frameHistInfo->trackedFramesCount
               ,(int)frameHistInfo->workImageId
               ,(int)imageId
               );
      }  
      /* start of detail tracking when no frame history available and whenever a new workImage was created */
      frameHistInfo->lostTraceCount = 0;
      frameHistInfo->trackedFramesCount = 0;
      p_copy_src_to_dst_coords_array(&currCoordsArray,  &frameHistInfo->startCoordsArray);
      p_copy_src_to_dst_coords_array(&currCoordsArray,  &frameHistInfo->prevCoordsArray);

      /* create (or replace) reference vectors objects */
      p_set_n_vector_points(imageId, &currCoordsArray, VECTORS_NAME_START_REFERENCE_POINTS, FALSE, FALSE, 0);
      p_set_n_vector_points(imageId, &currCoordsArray, VECTORS_NAME_REFERENCE_POINTS, TRUE, FALSE, 0);


      frameHistInfo->frameNr = 1;
      frameHistInfo->workImageId = imageId;
      if (isTrackingToFrameImage != TRUE)
      {
        /* the initial call for frame 1 shall just record the inital tracking points
         * without calling loacte procedure and without tuning
         * (therefore it uses 2x refLayerId 
         *  note that the topLayerId of frame2 is typically already present at this time
         *  but is handled in the 2nd call to p_selective_coords_logging 
         *  later in this procedure)
         */ 
        bestIdx1 = p_selective_coords_logging(frameHistInfo
                      , &currCoordsArray
                      , &frameHistInfo->startCoordsArray
                      , &frameHistInfo->prevCoordsArray
                      , valPtr
                      , imageId
                      , refLayerId
                      , refLayerId
                      );
      }
    }
    else
    {

      if(gap_debug)
      {
        printf("(B) gap_track_detail_on_top_layers  CONTINUE tracking l_nlayers:%d\n"
               ,(int)l_nlayers
               );
      }  



      /* (re)inital capture vector points if the start coords of first processed frame are not valid 
       *  TODO detecting by valid startCoordsArray [0] is no longer sufficient
       *       for now re-init is harcoded disabled (not sure if that is needed ...)
       */
      if(FALSE) //// if (frameHistInfo->startCoordsArray.pixCoord[0].valid != TRUE)
      {
        p_capture_n_vector_points(imageId, &currCoordsArray, NUMBER_OF_COORDS, VECTORS_NAME_START_REFERENCE_POINTS);

        frameHistInfo->lostTraceCount = 0;
        p_copy_src_to_dst_coords_array(&currCoordsArray,  &frameHistInfo->startCoordsArray);
        p_copy_src_to_dst_coords_array(&currCoordsArray,  &frameHistInfo->prevCoordsArray);

        frameHistInfo->frameNr = p_parse_frame_nr_from_layer_name(refLayerId);
        if (isTrackingToFrameImage != TRUE)
        {
          bestIdx1 = p_selective_coords_logging(frameHistInfo
                      , &currCoordsArray
                      , &frameHistInfo->startCoordsArray
                      , &frameHistInfo->prevCoordsArray
                      , valPtr
                      , imageId
                      , refLayerId
                      , refLayerId
                      );
        }
      }
      else if (valPtr->bgLayerIsReference == TRUE)
      {
        gint32 sumOffsX;
        gint32 sumOffsY;
        gint32 numValidOffsets;

        if(gap_debug)
        {
          printf("(Bb) gap_track_detail_on_top_layers  CONTINUE BG is referenence  l_nlayers:%d\n"
                ,(int)l_nlayers
                );
        }  
        
        /* when all trackings refere to initial BG layer (that is always kept as reference
         * for all further frames), we do not capture currCoordsArray
         * but copy the initial start values.
         */
        p_copy_src_to_dst_coords_array(&frameHistInfo->startCoordsArray, &currCoordsArray);

        /* locate shall start investigations at matching coordinates of the previous processed frame
         * because the chance to find the detail near this postion is much greater than near
         * the start coords in the initial frame.
         * (note that locate does use the initial frame i.e. the BG layer in this mode,
         * but without the locateOffsets we might loose track of the detail when it moves outside the targetRadius
         * and increasing the targetRadius would also result in siginificant longer processing time)
         */
        sumOffsX = 0;
	sumOffsY = 0;
        numValidOffsets = 0;
        for (l_idx = 0; l_idx < frameHistInfo->prevCoordsArray.numberOfCoords; l_idx++)
        {
          GapPixelCoords  *prevCoords;
          GapPixelCoords  *startCoords;
          
          prevCoords = &frameHistInfo->prevCoordsArray.pixCoord[l_idx];
          startCoords = &frameHistInfo->startCoordsArray.pixCoord[l_idx];
          if (startCoords->valid)
          {
            if(prevCoords->valid)
            {
              locateOffsetX[l_idx] = prevCoords->px - startCoords->px;
              locateOffsetY[l_idx] = prevCoords->py - startCoords->py;
              
              sumOffsX += locateOffsetX[l_idx];
              sumOffsY += locateOffsetY[l_idx];
              numValidOffsets++;
            }
          }
        }
        if ((frameHistInfo->prevCoordsArray.numberOfCoords > numValidOffsets)
        &&  (numValidOffsets > 0))
        {
          /* for invalid coordinates use the average offests of all valid points
           * as guess. This shall increase the chances to pick up a coordinate
           * that has lost trace some frames ago.
           * (Another option would be to increase the search radius in such cases
           * but this would also significant increase the processing time)
           */
          for (l_idx = 0; l_idx < frameHistInfo->prevCoordsArray.numberOfCoords; l_idx++)
          {
            GapPixelCoords  *prevCoords;
            GapPixelCoords  *startCoords;
          
            prevCoords = &frameHistInfo->prevCoordsArray.pixCoord[l_idx];
            startCoords = &frameHistInfo->startCoordsArray.pixCoord[l_idx];
            if (startCoords->valid)
            {
              if(!prevCoords->valid)
              {
                locateOffsetX[l_idx] = sumOffsX / numValidOffsets;
                locateOffsetY[l_idx] = sumOffsY / numValidOffsets;
              }
            }
          }

        }
         
      }
      else
      {
        if(gap_debug)
        {
          printf("(Bp) gap_track_detail_on_top_layers  CONTINUE Previous Layer is referenence l_nlayers:%d\n"
                ,(int)l_nlayers
                );
        }  
        /* tracking is done with reference to the previous layer
         * therefore refresh capture. (currCoordsArray are set to the targetCoordsArray
         * that were calculated in previous processing step)
         */
        p_capture_n_vector_points(imageId, &currCoordsArray, NUMBER_OF_COORDS, VECTORS_NAME_REFERENCE_POINTS);

      }
   


    }

    /* ----  tracking requires at least 2 layers -------- */

    if (l_nlayers > 1)
    {
      previousLostTraceCount = frameHistInfo->lostTraceCount;
      targetCoordsArray.numberOfCoords = currCoordsArray.numberOfCoords;
      
      if(gap_debug)
      {
        printf("DetailTrack before locating %d coordinates\n", currCoordsArray.numberOfCoords);
      }
      
      for (l_idx = 0; l_idx < currCoordsArray.numberOfCoords; l_idx++)
      {
        GapPixelCoords  *currCoordsPtr;
        GapPixelCoords  *targetCoordsPtr;
        
        currCoordsPtr = &currCoordsArray.pixCoord[l_idx];
        targetCoordsPtr = &targetCoordsArray.pixCoord[l_idx];
        targetCoordsPtr->valid = FALSE;
        targetCoordsPtr->px = 0;
        targetCoordsPtr->py = 0;
         
        if (currCoordsPtr->valid == TRUE)
        {
          p_locate_target(refLayerId
                           , currCoordsPtr
                           , topLayerId
                           , targetCoordsPtr
                           , locateOffsetX[l_idx]
                           , locateOffsetY[l_idx]
                           , valPtr
                           );
          if (targetCoordsPtr->valid == TRUE)
          {
            successfulTracedPointsCount++;
          }
        }

      }
      if (successfulTracedPointsCount < valPtr->numPointsSelect)
      {
        /* the required number of successful traces was not detected
         * therefore increase the lostTraceCount in the frame history
         */
        frameHistInfo->lostTraceCount += 1;
      }
    }

    gap_image_remove_all_guides(imageId);
    currFrameNr = p_parse_frame_nr_from_layer_name(topLayerId);
    
    /* here we could check if at least one points could be successfully located,
     * but continue setting vectors and logging on the ivalid result may help
     * analysis what went wrong when the xml includes the unusables results too.
     * if ((successfulTracedPointsCount > 0) 
     *  || (frameHistInfo->trackedFramesCount == 0))
     */
    if(TRUE)
    {
      // p_copy_src_to_dst_coords_array(&targetCoordsArray,  &frameHistInfo->prevCoordsArray);

      frameHistInfo->frameNr = currFrameNr;
      
      if (isTrackingToFrameImage != TRUE)
      {
        bestIdx1 = p_selective_coords_logging(frameHistInfo
                      , &targetCoordsArray
                      , &frameHistInfo->startCoordsArray
                      , &frameHistInfo->prevCoordsArray
                      , valPtr
                      , imageId
                      , topLayerId
                      , refLayerId
                      );
      }
      else
      {
        
        p_select_best_coords(imageId, frameHistInfo   //->frameNr
	  , &targetCoordsArray
	  , &frameHistInfo->startCoordsArray
	  , valPtr
	  , &bestIndexes
	  , frameHistInfo
        );
        bestIdx1 = bestIndexes.bestIdx[0];
      }
      p_set_n_vector_points(imageId, &targetCoordsArray, VECTORS_NAME_TRACKING_POINTS, TRUE, TRUE, bestIdx1);
    }
    p_copy_src_to_dst_coords_array(&targetCoordsArray,  &frameHistInfo->prevCoordsArray);

    if (valPtr->removeMidlayers == TRUE)
    {
      gap_image_limit_layers(imageId
                            , 2   /* keepTopLayers */
                            , 1   /* keepBgLayers */
                            );
    }


    /* optional save image_id as frame */
    if (isTrackingToFrameImage == TRUE)
    {
      l_extension = gap_lib_alloc_extension(&valPtr->moveLogFile[0]);
      if (l_extension != NULL)
      {
        gchar *frame_filename;
        gchar *basename;
        long   number;
        gint   l_rc;
        gint32 transformedLayerId;
        gint32 currVectorsId;
        gint32 targetVectorsId;

        // TODO enable via env or gimprc
        // p_set_debug_vectors(currFrameNr, imageId, &targetCoordsArray, &frameHistInfo->startCoordsArray, &bestIndexes);

        basename = gap_lib_alloc_basename(&valPtr->moveLogFile[0], &number);
        frame_filename = gap_lib_alloc_fname_fixed_digits(basename, currFrameNr, l_extension, 6 /* digits*/ );

        transformedLayerId = -1;
        currVectorsId = -1;
        targetVectorsId = -1;
        if (valPtr->addTransformedLayer)
        {
	  PixelCoordsArray alignCurrCoordsArray;
	  PixelCoordsArray alignTargetCoordsArray;
	  
	  
	  
	  
	  gint validPairCount;
	  gint idx;
          GapPixelCoords  *trkPtr[4];   /* upto 4 coords in current frame  currCoords[4]; */
          GapPixelCoords  *refPtr[4];   /* upto 4 coords of first processed (reference) frame  startCoords[4]; */
	  
          /* set paths "SRC" and "TARGET" respecting BestIndexes...   for the aligner call */
          validPairCount = 0;
          for(idx=0; idx < 4; idx++)
	  {
	    trkPtr[idx] = &targetCoordsArray.pixCoord[bestIndexes.bestIdx[idx]];
	    refPtr[idx] = &frameHistInfo->startCoordsArray.pixCoord[bestIndexes.bestIdx[idx]];	      
	    if((trkPtr[idx]->valid == TRUE)
	    && (refPtr[idx]->valid == TRUE))
	    {
              gap_geo_copy_src_to_dst_coords(trkPtr[idx]                    /* GapPixelCoords *srcCoords*/
	                                  ,&alignCurrCoordsArray.pixCoord[validPairCount]  /* GapPixelCoords*dstCoords */
	                                  );
              gap_geo_copy_src_to_dst_coords(refPtr[idx]                    /* GapPixelCoords *srcCoords*/
	                                  ,&alignTargetCoordsArray.pixCoord[validPairCount]  /* GapPixelCoords*dstCoords */
	                                  );
	      validPairCount++;
	      alignCurrCoordsArray.numberOfCoords = validPairCount;
	      alignTargetCoordsArray.numberOfCoords = validPairCount;
	    }
	  }

          
          
          /* set src Path name = "SRC" for the aligner and make it active vectors path */
          currVectorsId = p_set_n_vector_points(imageId, &alignCurrCoordsArray, "SRC", TRUE, FALSE, 0);
          if(currVectorsId >= 0)
          {
            /* set target Path name = "TARGET" for the aligner */
            targetVectorsId = p_set_n_vector_points(imageId, &alignTargetCoordsArray, GAP_EXACT_ALIGNER_TARGET_PATH_NAME, TRUE, FALSE, 0);

            gimp_image_set_active_vectors(imageId, currVectorsId);

            /* duplicate Top Layer */
            transformedLayerId = gap_layer_make_duplicate(topLayerId  /* gint32 src_layer_id */
                                     , imageId
                                     , "TRANS_"   /* const char *name_prefix */
                                     , "\0"       /* const char *name_suffix */
                                     );
            /* only the transformed layer on top and the BG layer shall be visible in the saved frame image */
            gimp_item_set_visible(topLayerId, FALSE);
            gimp_item_set_visible(belowTopLayerId, FALSE);
            gimp_item_set_visible(transformedLayerId, TRUE);
            gimp_item_set_visible(refLayerId, TRUE);
              

            /*  call exact aligner plugin to perform transformation according to paths active (SRC) and TARGET */
            gap_detail_exact_align_via_4point_path(imageId, transformedLayerId
	                 , POINT_ORDER_MODE_1234_1234
	                 , GIMP_RUN_NONINTERACTIVE
	                 );
          }
	    
        }


    
        l_rc = gap_lib_save_named_frame(imageId, frame_filename);
    
        g_free(basename);
        g_free(frame_filename);


        if (transformedLayerId >= 0)
        {
          gimp_image_remove_layer(imageId, transformedLayerId);
        }
        if (currVectorsId >= 0)
        {
          gimp_image_remove_vectors(imageId, currVectorsId);
        }
        if (targetVectorsId >= 0)
        {
          gimp_image_remove_vectors(imageId, targetVectorsId);
        }
        
      }
      g_free(l_extension);
      l_extension = NULL;

    }


    
    if((valPtr->bgLayerIsReference != TRUE)
    && (successfulTracedPointsCount >= valPtr->numPointsSelect))
    {
      /* after save image as frame set current target vectors
       * as reference for processing of the next frame (next call of this procedure)
       */
      p_set_n_vector_points(imageId, &targetCoordsArray, VECTORS_NAME_REFERENCE_POINTS, TRUE, FALSE, 0);
    }
    
    frameHistInfo->trackedFramesCount++;
    p_set_frameHistInfo(frameHistInfo, imageId);
    g_free(l_layers_list);

    if ((successfulTracedPointsCount < valPtr->numPointsSelect)
    &&  (frameHistInfo->trackedFramesCount > 1)
    &&  (previousLostTraceCount == 0))
    {
      if (gap_debug)
      {
        printf("Detail Tracking Stopped at frameNr:%d previousLostTraceCount:%d successfulTracedPointsCount:%d (required %d)\n"
          , (int)frameHistInfo->frameNr
          , (int)previousLostTraceCount
          , (int)successfulTracedPointsCount
          , (int)valPtr->numPointsSelect
          );
      }
      /* trace lost the 1st time, display warning */
      gap_arr_msg_popup(GIMP_RUN_INTERACTIVE
                       , _("Detail Tracking Stopped. (could not find corresponding detail)"));;
    }
  }

 
  return(imageId);

}  /* end gap_track_detail_on_top_layers */




/* ---------------------------------------
 * gap_track_detail_on_top_layers_lastvals
 * ---------------------------------------
 * processing based on last values used in the same gimp session.
 * (uses defaults when called the 1.st time)
 * Intended for use in the player
 */
gint32
gap_track_detail_on_top_layers_lastvals(gint32 imageId)
{
  gint32       rc;
  gboolean     doProgress;
  FilterValues fiVals;

  /* clear undo stack */
  if (gimp_image_undo_is_enabled(imageId))
  {
    gimp_image_undo_disable(imageId);
  }

  doProgress = FALSE;
  gap_detail_tracking_get_values(&fiVals);
  rc = gap_track_detail_on_top_layers(imageId, doProgress, &fiVals);

  gimp_image_undo_enable(imageId); /* clear undo stack */

  return(rc);

}  /* end gap_track_detail_on_top_layers_lastvals */


/* -----------------------------------
 * gap_detail_tracking_get_values
 * -----------------------------------
 * This procedure is typically called
 * on the snapshot image created by the Player.
 * This image has one layer at the first snapshot
 * and each further snapshot adds one layer on top of the layerstack.
 *
 * The start is detected when the image has only one layer.
 * optionally the numer of layers can be limted
 * to 2 (or more) layers.
 */
void
gap_detail_tracking_get_values(FilterValues *fiVals)
{
  int l_len;

  /* init default values */
  fiVals->refShapeRadius             = DEFAULT_refShapeRadius;
  fiVals->targetMoveRadius           = DEFAULT_targetMoveRadius;
  fiVals->loacteColodiffThreshold    = DEFAULT_loacteColodiffThreshold;
  fiVals->numPointsSelect            = DEFAULT_numPointsSelect;
  fiVals->coordsRelToFrame1          = DEFAULT_coordsRelToFrame1;
  fiVals->offsX                      = DEFAULT_offsX;
  fiVals->offsY                      = DEFAULT_offsY;
  fiVals->offsRotate                 = DEFAULT_offsRotate;
  fiVals->enableScaling              = DEFAULT_enableScaling;
  fiVals->removeMidlayers            = DEFAULT_removeMidlayers;
  fiVals->bgLayerIsReference         = DEFAULT_bgLayerIsReference;
  fiVals->addTransformedLayer        = DEFAULT_addTransformedLayer;
  fiVals->moveLogFile[0]             = '\0';

  l_len = gimp_get_data_size (GAP_DETAIL_TRACKING_PLUG_IN_NAME);
  if (l_len == sizeof(FilterValues))
  {
    /* Possibly retrieve data from a previous interactive run */
    gimp_get_data (GAP_DETAIL_TRACKING_PLUG_IN_NAME, fiVals);

    if(gap_debug)
    {
      printf("gap_detail_tracking_get_values FOUND data for key:%s\n"
        , GAP_DETAIL_TRACKING_PLUG_IN_NAME
        );
    }
  }

  if(gap_debug)
  {
    printf("gap_detail_tracking_get_values:\n"
           "  refShapeRadius:%d targetMoveRadius:%d locateColordiff:%.4f\n"
           "  coordsRelToFrame1:%d  offsX:%d offsY:%d removeMidlayers:%d bgLayerIsReference:%d\n"
           "  moveLogFile:%s\n"
            , (int)fiVals->refShapeRadius
            , (int)fiVals->targetMoveRadius
            , (float)fiVals->loacteColodiffThreshold
            , (int)fiVals->coordsRelToFrame1
            , (int)fiVals->offsX
            , (int)fiVals->offsY
            , (int)fiVals->removeMidlayers
            , (int)fiVals->bgLayerIsReference
            , fiVals->moveLogFile
            );
  }

}  /* end gap_detail_tracking_get_values */



/* --------------------------
 * gap_detail_tracking_dialog
 * --------------------------
 *   return  TRUE.. OK
 *           FALSE.. in case of Error or cancel
 */
gboolean
gap_detail_tracking_dialog(FilterValues *fiVals)
{
#define SPINBUTTON_ENTRY_WIDTH 80
#define DETAIL_TRACKING_DIALOG_ARGC 15

  static GapArrArg  argv[DETAIL_TRACKING_DIALOG_ARGC];
  gint ii;
  gint ii_numPointsSelect;
  gint ii_loacteColodiffThreshold;
  gint ii_refShapeRadius;
  gint ii_targetMoveRadius;
  gint ii_coordsRelToFrame1;
  gint ii_offsX;
  gint ii_offsY;
  gint ii_offsRotate;
  gint ii_enableScaling;
  gint ii_removeMidlayers;
  gint ii_bgLayerIsReference;
  gint ii_addTransformedLayer;


  ii=0; gap_arr_arg_init(&argv[ii], GAP_ARR_WGT_LABEL);
  argv[0].label_txt = _("This filter requires a current path with one or 2 anchor points\n"
                        "to mark coordinate(s) to be tracked in the target frame(s)");


  ii++; gap_arr_arg_init(&argv[ii], GAP_ARR_WGT_INT_PAIR); ii_numPointsSelect = ii;
  argv[ii].label_txt = _("Select Points:");
  argv[ii].help_txt  = _("1: select only the best path point for movement detection, "
                         "2: select the best 2 points for movement,scale and rotation detection.");
  argv[ii].constraint = FALSE;
  argv[ii].int_min   = 1;
  argv[ii].int_max   = 4;
  argv[ii].umin      = 1;
  argv[ii].umax      = 4;
  argv[ii].int_ret   = fiVals->numPointsSelect;
  argv[ii].entry_width = SPINBUTTON_ENTRY_WIDTH;
  argv[ii].has_default = TRUE;
  argv[ii].int_default = 2;



  ii++; gap_arr_arg_init(&argv[ii], GAP_ARR_WGT_FLT_PAIR); ii_loacteColodiffThreshold = ii;
  argv[ii].constraint = TRUE;
  argv[ii].label_txt = _("Locate colordiff Thres:");
  argv[ii].help_txt  = _("Colordiff threshold value. Locate fails when average color difference is below this value.");
  argv[ii].flt_min   = 0.0;
  argv[ii].flt_max   = 1.0;
  argv[ii].flt_ret   = fiVals->loacteColodiffThreshold;
  argv[ii].flt_step  =  0.01;
  argv[ii].flt_digits =  4;
  argv[ii].entry_width = SPINBUTTON_ENTRY_WIDTH;
  argv[ii].has_default = TRUE;
  argv[ii].flt_default = DEFAULT_loacteColodiffThreshold;


  ii++; gap_arr_arg_init(&argv[ii], GAP_ARR_WGT_INT_PAIR); ii_refShapeRadius = ii;
  argv[ii].label_txt = _("Locate Shape Radius:");
  argv[ii].help_txt  = _("The quadratic area surrounding a marked detail coordinate +- this radius "
                         "is considered as reference shape, to be tracked in the target frame(s).");
  argv[ii].constraint = FALSE;
  argv[ii].int_min   = 1;
  argv[ii].int_max   = 50;
  argv[ii].umin      = 1;
  argv[ii].umax      = 100;
  argv[ii].int_ret   = fiVals->refShapeRadius;
  argv[ii].entry_width = SPINBUTTON_ENTRY_WIDTH;
  argv[ii].has_default = TRUE;
  argv[ii].int_default = DEFAULT_refShapeRadius;

  ii++; gap_arr_arg_init(&argv[ii], GAP_ARR_WGT_INT_PAIR); ii_targetMoveRadius = ii;
  argv[ii].label_txt = _("Locate Target Move Radius:");
  argv[ii].help_txt  = _("Limits attempts to locate the Detail within this radius.");
  argv[ii].constraint = FALSE;
  argv[ii].int_min   = 1;
  argv[ii].int_max   = 500;
  argv[ii].umin      = 1;
  argv[ii].umax      = 1000;
  argv[ii].int_ret   = fiVals->targetMoveRadius;
  argv[ii].entry_width = SPINBUTTON_ENTRY_WIDTH;
  argv[ii].has_default = TRUE;
  argv[ii].int_default = DEFAULT_targetMoveRadius;



  ii++; gap_arr_arg_init(&argv[ii], GAP_ARR_WGT_TOGGLE); ii_coordsRelToFrame1 = ii;
  argv[ii].label_txt = _("Log Relative Coords:");
  argv[ii].help_txt  = _("ON: Coordinates are logged relative to the first coordinate.\n"
                         "OFF: Coordinates are logged as absolute pixel coordinate values.");
  argv[ii].constraint = FALSE;
  argv[ii].int_ret   = fiVals->coordsRelToFrame1;
  argv[ii].has_default = TRUE;
  argv[ii].int_default = DEFAULT_coordsRelToFrame1;


  ii++; gap_arr_arg_init(&argv[ii], GAP_ARR_WGT_TOGGLE); ii_enableScaling = ii;
  argv[ii].label_txt = _("Log Scaling:");
  argv[ii].help_txt  = _("ON: Calculate scaling and rotation when 2 detail Coordinates are tracked.\n"
                         "OFF: Calculate only rotation and keep original size.");
  argv[ii].int_ret   = fiVals->enableScaling;
  argv[ii].has_default = TRUE;
  argv[ii].int_default = DEFAULT_enableScaling;




  ii++; gap_arr_arg_init(&argv[ii], GAP_ARR_WGT_TOGGLE); ii_bgLayerIsReference = ii;
  argv[ii].label_txt = _("BG is Reference:");
  argv[ii].help_txt  = _("ON: Use background layer as reference and foreground layer as target for tracking.\n"
                         "OFF: Use foreground layer as target, and the layer below as reference\n.");
  argv[ii].int_ret   = fiVals->bgLayerIsReference;
  argv[ii].has_default = TRUE;
  argv[ii].int_default = DEFAULT_bgLayerIsReference;


  ii++; gap_arr_arg_init(&argv[ii], GAP_ARR_WGT_TOGGLE); ii_removeMidlayers = ii;
  argv[ii].label_txt = _("Remove Middle Layers:");
  argv[ii].help_txt  = _("ON: removes layers (except BG and 2 Layer on top) that are not relevant for detail tracking.\n"
                         "OFF: Keep all layers.");
  argv[ii].int_ret   = fiVals->removeMidlayers;
  argv[ii].has_default = TRUE;
  argv[ii].int_default = DEFAULT_removeMidlayers;


  ii++; gap_arr_arg_init(&argv[ii], GAP_ARR_WGT_TOGGLE); ii_addTransformedLayer = ii;
  argv[ii].label_txt = _("add Transformed Layer:");
  argv[ii].help_txt  = _("ON: add layer and apply detail_align transformation when tracking to XCF frame image.\n"
                         "OFF: do not apply detail align transformation\n.");
  argv[ii].int_ret   = fiVals->addTransformedLayer;
  argv[ii].has_default = TRUE;
  argv[ii].int_default = DEFAULT_addTransformedLayer;



  ii++; gap_arr_arg_init(&argv[ii], GAP_ARR_WGT_INT_PAIR); ii_offsX = ii;
  argv[ii].label_txt = _("Const X Offset:");
  argv[ii].help_txt  = _("This value is added when logging captured X coordinates.");
  argv[ii].constraint = FALSE;
  argv[ii].int_min   = 0;
  argv[ii].int_max   = 2000;
  argv[ii].umin      = 0;
  argv[ii].umax      = 10000;
  argv[ii].int_ret   = fiVals->offsX;
  argv[ii].entry_width = SPINBUTTON_ENTRY_WIDTH;
  argv[ii].has_default = TRUE;
  argv[ii].int_default = DEFAULT_offsX;


  ii++; gap_arr_arg_init(&argv[ii], GAP_ARR_WGT_INT_PAIR); ii_offsY = ii;
  argv[ii].label_txt = _("Const Y Offset:");
  argv[ii].help_txt  = _("This value is added when logging captured Y coordinates.");
  argv[ii].constraint = FALSE;
  argv[ii].int_min   = 0;
  argv[ii].int_max   = 2000;
  argv[ii].umin      = 0;
  argv[ii].umax      = 10000;
  argv[ii].int_ret   = fiVals->offsY;
  argv[ii].entry_width = SPINBUTTON_ENTRY_WIDTH;
  argv[ii].has_default = TRUE;
  argv[ii].int_default = DEFAULT_offsY;

  ii++; gap_arr_arg_init(&argv[ii], GAP_ARR_WGT_FLT_PAIR); ii_offsRotate = ii;
  argv[ii].constraint = TRUE;
  argv[ii].label_txt = _("Const Rotate Offset:");
  argv[ii].help_txt  = _("This value is added when logging rotation values.");
  argv[ii].flt_min   = -360.0;
  argv[ii].flt_max   = 360.0;
  argv[ii].flt_ret   = fiVals->offsRotate;
  argv[ii].flt_step  =  0.1;
  argv[ii].flt_digits =  4;
  argv[ii].entry_width = SPINBUTTON_ENTRY_WIDTH;
  argv[ii].has_default = TRUE;
  argv[ii].flt_default = DEFAULT_offsRotate;


  ii++; gap_arr_arg_init(&argv[ii], GAP_ARR_WGT_FILESEL);
  argv[ii].label_txt = _("MovePath XML file:");
  argv[ii].help_txt  = _("Name of the file to log the tracked detail coordinates "
                        " as XML parameterfile for later use in the MovePath plug-in.");
  argv[ii].text_buf_len = sizeof(fiVals->moveLogFile);
  argv[ii].text_buf_ret = &fiVals->moveLogFile[0];
  argv[ii].entry_width = 400;



  ii++; gap_arr_arg_init(&argv[ii], GAP_ARR_WGT_DEFAULT_BUTTON);
  argv[ii].label_txt = _("Default");
  argv[ii].help_txt  = _("Reset all parameters to default values");

  if(TRUE == gap_arr_ok_cancel_dialog(_("Detail Tracking"),
                            _("Settings :"),
                            DETAIL_TRACKING_DIALOG_ARGC, argv))
  {
      fiVals->numPointsSelect          = (gint32)(argv[ii_numPointsSelect].int_ret); 
      fiVals->refShapeRadius           = (gint32)(argv[ii_refShapeRadius].int_ret);
      fiVals->targetMoveRadius         = (gint32)(argv[ii_targetMoveRadius].int_ret);
      fiVals->loacteColodiffThreshold  = (gdouble)(argv[ii_loacteColodiffThreshold].flt_ret);
      fiVals->coordsRelToFrame1        = (gint32)(argv[ii_coordsRelToFrame1].int_ret);
      fiVals->offsX                    = (gint32)(argv[ii_offsX].int_ret);
      fiVals->offsY                    = (gint32)(argv[ii_offsY].int_ret);
      fiVals->offsRotate               = (gint32)(argv[ii_offsRotate].flt_ret);
      fiVals->enableScaling            = (gint32)(argv[ii_enableScaling].int_ret);
      fiVals->removeMidlayers          = (gint32)(argv[ii_removeMidlayers].int_ret);
      fiVals->bgLayerIsReference       = (gint32)(argv[ii_bgLayerIsReference].int_ret);
      fiVals->addTransformedLayer      = (gint32)(argv[ii_addTransformedLayer].int_ret);

      gimp_set_data (GAP_DETAIL_TRACKING_PLUG_IN_NAME, fiVals, sizeof (FilterValues));
      return TRUE;
  }
  else
  {
      return FALSE;
  }
}               /* end gap_detail_tracking_dialog */


/* ---------------------------------------
 * gap_detail_tracking_dialog_cfg_set_vals
 * ---------------------------------------
 *   return  TRUE.. OK
 *           FALSE.. in case of Error or cancel
 */
gboolean
gap_detail_tracking_dialog_cfg_set_vals(gint32 image_id)
{
  gboolean     rc;
  FilterValues fiVals;

  gap_detail_tracking_get_values(&fiVals);
  if(image_id >= 0)
  {
    if (fiVals.coordsRelToFrame1)
    {
      if(gap_image_is_alive(image_id))
      {
        /* default offsets for handle at center */
        fiVals.offsX = gimp_image_width(image_id) / 2.0;
        fiVals.offsY = gimp_image_height(image_id) / 2.0;
      }
    }
  }

  rc = gap_detail_tracking_dialog(&fiVals);

  return(rc);

}  /* end gap_detail_tracking_dialog_cfg_set_vals */
