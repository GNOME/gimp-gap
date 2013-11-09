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
 * along with this program; if not, write to the Free Software
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

#include "gap_base.h"
#include "gap_libgapbase.h"
#include "gap_locate.h"
#include "gap_locate2.h"
#include "gap_colordiff.h"
#include "gap_image.h"
#include "gap_layer_copy.h"
#include "gap_detail_tracking_exec.h"

#include "gap-intl.h"

#define DEFAULT_refShapeRadius            15
#define DEFAULT_targetMoveRadius          70
#define DEFAULT_loacteColodiffThreshold   0.08
#define DEFAULT_coordsRelToFrame1         TRUE
#define DEFAULT_offsX                     0
#define DEFAULT_offsY                     0
#define DEFAULT_offsRotate                0.0
#define DEFAULT_enableScaling             TRUE
#define DEFAULT_removeMidlayers           TRUE
#define DEFAULT_bgLayerIsReference        TRUE

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




/* ------------------------------------------
 * p_capture_2_vector_points
 * ------------------------------------------
 * capture the first 2 points of the 1st stroke in the active path vectors
 */
static void
p_capture_2_vector_points(gint32 imageId, PixelCoords *coordPtr, PixelCoords *coordPtr2) {
  gint32  activeVectorsId;
  gint32  gx1;
  gint32  gy1;
  gint32  gx2;
  gint32  gy2;

  gx1 = -1;
  gy1 = -1;
  gx2 = -1;
  gy2 = -1;

  activeVectorsId = gimp_image_get_active_vectors(imageId);
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
          if(num_points >= 6)
          {
            gx1 = points[0];
            gy1 = points[1];
          }
          if(num_points >= 12)
          {
            gx2 = points[6];
            gy2 = points[7];
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


  coordPtr->valid = FALSE;
  coordPtr2->valid = FALSE;

  if((gx1 >= 0) && (gy1 >= 0))
  {
    if(gap_debug)
    {
      printf("\nPathPoints: x1=%d; y1=%d x2=%d; y2=%d\n"
          , gx1
          , gy1
          , gx2
          , gy2
          );
    }

    coordPtr->px = gx1;
    coordPtr->py = gy1;
    coordPtr->valid = TRUE;

    if((gx2 != gx1) || (gy2 != gy1))
    {
      coordPtr2->px = gx2;
      coordPtr2->py = gy2;
      coordPtr2->valid = TRUE;
    }

  }

}  /* end p_capture_2_vector_points */


/* ------------------------------------
 * p_copy_src_to_dst_coords
 * ------------------------------------
 */
static void
p_copy_src_to_dst_coords(PixelCoords *srcCoords, PixelCoords *dstCoords)
{
  dstCoords->valid = srcCoords->valid;
  dstCoords->px = srcCoords->px;
  dstCoords->py = srcCoords->py;
}

/* ------------------------------------
 * p_locate_target
 * ------------------------------------
 */
static void
p_locate_target(gint32 refLayerId, PixelCoords *refCoords
   , gint32 targetLayerId, PixelCoords *targetCoords
   , gint32 locateOffsetX, gint32 locateOffsetY
   , FilterValues *valPtr, gint32 *lostTraceCount)
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

  targetCoords->px = targetX + target_offset_x;
  targetCoords->py = targetY + target_offset_y;


  if (colordiffLocate < valPtr->loacteColodiffThreshold)
  {
    /* successful located the deatail in target layer
     * set target coordinates valid.
     */
    targetCoords->valid = TRUE;
  }
  else
  {
    (*lostTraceCount) += 1;
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
        ,(float)colordiffLocate
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
p_write_xml_header(FILE *l_fp, gboolean center, gint width, gint height, gint numFrames)
{
  fprintf(l_fp, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n");
  fprintf(l_fp, "<gimp_gap_move_path_parameters version=\"2\" >\n");
  fprintf(l_fp, "  <frame_description width=\"%d\" height=\"%d\" range_from=\"1\" range_to=\"%d\" total_frames=\"%d\" />\n"
          , (int)width
          , (int)height
          , (int)numFrames
          , (int)numFrames
          );
  fprintf(l_fp, "  <tween tween_steps=\"0\" />\n");
  fprintf(l_fp, "  <trace tracelayer_enable=\"FALSE\" />\n");
  fprintf(l_fp, "  <moving_object src_layer_id=\"0\" src_layerstack=\"0\" width=\"%d\" height=\"%d\"\n"
          , (int)width
          , (int)height
          );
  if(center)
  {
    fprintf(l_fp, "    src_handle=\"GAP_HANDLE_CENTER\"\n");
  }
  else
  {
    fprintf(l_fp, "    src_handle=\"GAP_HANDLE_LEFT_TOP\"\n");
  }
  fprintf(l_fp, "    src_stepmode=\"GAP_STEP_FRAME_ONCE\" step_speed_factor=\"1.00000\"\n");
  fprintf(l_fp, "    src_selmode=\"GAP_MOV_SEL_IGNORE\"\n");
  fprintf(l_fp, "    src_paintmode=\"GIMP_NORMAL_MODE\"\n");
  fprintf(l_fp, "    dst_layerstack=\"0\" src_force_visible=\"TRUE\" clip_to_img=\"FALSE\" src_apply_bluebox=\"FALSE\"\n");
  fprintf(l_fp, "    >\n");
  fprintf(l_fp, "  </moving_object>\n");
  fprintf(l_fp, "\n");
  fprintf(l_fp, "  <controlpoints current_point=\"1\" number_of_points=\"%d\"  >\n"
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
   , gint32 frameNr, gboolean center, gint width, gint height)
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
      p_write_xml_header(l_fp, center, width, height, frameNr);

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
p_coords_logging(gint32 frameNr, PixelCoords *currCoords,  PixelCoords *currCoords2
  , PixelCoords *startCoords, PixelCoords *startCoords2, FilterValues *valPtr
  , gint32 imageId
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

  if(currCoords->valid != TRUE)
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
  px1 = currCoords->px;
  py1 = currCoords->py;

  px2 = currCoords2->px;
  py2 = currCoords2->py;

  if ((valPtr->coordsRelToFrame1)
  &&  (startCoords->valid == TRUE))
  {
    px1 = startCoords->px -px1;
    py1 = startCoords->py -py1;
  }


  px = px1 + valPtr->offsX;
  py = py1 + valPtr->offsY;

  if ((valPtr->coordsRelToFrame1)
  &&  (startCoords2->valid == TRUE))
  {
    px2 = startCoords2->px -px2;
    py2 = startCoords2->py -py2;

  }

  if((currCoords2->valid  == TRUE)
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

  if(currCoords2->valid == TRUE)
  {
    /* double point detail coordinate tracking (allows calculation of rotate and scale factors) */
    gchar *scaleValueAsString;


    precision_digits = 7;
    rotValueAsString = gap_base_gdouble_to_ascii_string(rotation + valPtr->offsRotate, precision_digits);
    precision_digits = 5;
    scaleValueAsString = gap_base_gdouble_to_ascii_string(scaleFactor * 100, precision_digits);

    if(valPtr->enableScaling == TRUE)
    {
      logString = g_strdup_printf("    <controlpoint px=\"%04d\" py=\"%04d\" rotation=\"%s\" width_resize=\"%s\" height_resize=\"%s\" keyframe_abs=\"%d\" p1x=\"%04d\"  p1y=\"%04d\"  p2x=\"%04d\" p2y=\"%04d\"/>"
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
       );
    }
    else
    {
      logString = g_strdup_printf("    <controlpoint px=\"%04d\" py=\"%04d\" rotation=\"%s\" keyframe_abs=\"%d\" p1x=\"%04d\"  p1y=\"%04d\"  p2x=\"%04d\" p2y=\"%04d\"/>"
       , px
       , py
       , rotValueAsString
       , frameNr
       , currCoords->px
       , currCoords->py
       , currCoords2->px
       , currCoords2->py
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
      logString = g_strdup_printf("    <controlpoint px=\"%04d\" py=\"%04d\"  keyframe_abs=\"%d\" p1x=\"%04d\"  p1y=\"%04d\"/>"
         , px
         , py
         , frameNr
         , currCoords->px
         , currCoords->py
         );
    }
    else
    {
      rotValueAsString = gap_base_gdouble_to_ascii_string(valPtr->offsRotate, precision_digits);
      precision_digits = 7;

      logString = g_strdup_printf("    <controlpoint px=\"%04d\" py=\"%04d\"  rotation=\"%s\" keyframe_abs=\"%d\" p1x=\"%04d\"  p1y=\"%04d\"/>"
         , px
         , py
         , rotValueAsString
         , frameNr
         , currCoords->px
         , currCoords->py
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
                );
  }

  if (logString)
  {
    g_free(logString);
  }


}  /* end p_coords_logging */



/* -------------------------------
 * p_parse_frame_nr_from_layerId
 * -------------------------------
 */
static gint32
p_parse_frame_nr_from_layerId(gint32 layerId)
{
  char     *layername;
  gint32    frameNr;
  gint      len;
  gint      ii;



  frameNr = 0;

  layername = gimp_drawable_get_name(layerId);
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

}  /* end p_parse_frame_nr_from_layerId */


/* -------------------------------
 * p_get_frameHistInfo
 * -------------------------------
 */
static void
p_get_frameHistInfo(FrameHistInfo *frameHistInfo)
{
  int l_len;

  frameHistInfo->workImageId = -1;
  frameHistInfo->frameNr = 0;
  frameHistInfo->startCoords.valid = FALSE;
  frameHistInfo->startCoords.px = 0;
  frameHistInfo->startCoords.py = 0;
  frameHistInfo->lostTraceCount = 0;

  l_len = gimp_get_data_size (GAP_DETAIL_FRAME_HISTORY_INFO);

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

    gimp_get_data(GAP_DETAIL_FRAME_HISTORY_INFO, frameHistInfo);

    if(gap_debug)
    {
      printf("p_get_frameHistInfo: %s  frameNr:%d px:%d py:%d valid:%d\n"
             "                     prevPx:%d prevPy:%d prevValid:%d lostTraceCount:%d\n"
        , GAP_DETAIL_FRAME_HISTORY_INFO
        , (int)frameHistInfo->frameNr
        , (int)frameHistInfo->startCoords.px
        , (int)frameHistInfo->startCoords.py
        , (int)frameHistInfo->startCoords.valid
        , (int)frameHistInfo->prevCoords.px
        , (int)frameHistInfo->prevCoords.py
        , (int)frameHistInfo->prevCoords.valid
        , (int)frameHistInfo->lostTraceCount
        );
    }

  }

}  /* end p_get_frameHistInfo */


/* -------------------------------
 * p_set_frameHistInfo
 * -------------------------------
 * store frame history information
 * (for the next run in the same gimp session)
 */
static void
p_set_frameHistInfo(FrameHistInfo *frameHistInfo)
{
  if(gap_debug)
  {
      printf("p_SET_frameHistInfo: %s  frameNr:%d px:%d py:%d valid:%d  prevPx:%d prevPy:%d prevValid:%d\n\n"
        , GAP_DETAIL_FRAME_HISTORY_INFO
        , (int)frameHistInfo->frameNr
        , (int)frameHistInfo->startCoords.px
        , (int)frameHistInfo->startCoords.py
        , (int)frameHistInfo->startCoords.valid
        , (int)frameHistInfo->prevCoords.px
        , (int)frameHistInfo->prevCoords.py
        , (int)frameHistInfo->prevCoords.valid

        );
  }

  gimp_set_data(GAP_DETAIL_FRAME_HISTORY_INFO, frameHistInfo, sizeof(FrameHistInfo));

}  /* end p_set_frameHistInfo */



/* -------------------------------
 * p_set_2_vector_points
 * -------------------------------
 * remove all strokes from the active path vectors
 * and set a new stroke containing targetCoords (one or 2 depend on the valid flag)
 * For better visualisation set guide lines crossing at the first target coords.
 */
static void
p_set_2_vector_points(gint32 imageId, PixelCoords *targetCoords, PixelCoords *targetCoords2)
{
  gint32  activeVectorsId;
  gint    newStrokeId;

  gdouble  *points;
  gint      num_points;
  gboolean  closed;
  GimpVectorsStrokeType type;


  gimp_image_add_hguide(imageId, targetCoords->py);
  gimp_image_add_vguide(imageId, targetCoords->px);


  activeVectorsId = gimp_image_get_active_vectors(imageId);
  if(activeVectorsId >= 0)
  {
    gint num_strokes;
    gint *strokes;

    strokes = gimp_vectors_get_strokes (activeVectorsId, &num_strokes);
    if(strokes)
    {
      if(num_strokes > 0)
      {
        gint ii;
        for(ii=0; ii < num_strokes; ii++)
        {
          gimp_vectors_remove_stroke(activeVectorsId, strokes[ii]);
        }
      }
      g_free(strokes);

    }

    if (targetCoords->valid)
    {
      closed = FALSE;
      num_points = 6;
      if (targetCoords2->valid)
      {
        num_points = 12;
      }
      points = g_new (gdouble, num_points);
      points[0] = targetCoords->px;
      points[1] = targetCoords->py;
      points[2] = targetCoords->px;
      points[3] = targetCoords->py;
      points[4] = targetCoords->px;
      points[5] = targetCoords->py;
      if(targetCoords2->valid)
      {
        points[6] = targetCoords2->px;
        points[7] = targetCoords2->py;
        points[8] = targetCoords2->px;
        points[9] = targetCoords2->py;
        points[10] = targetCoords2->px;
        points[11] = targetCoords2->py;
      }

      type = GIMP_VECTORS_STROKE_TYPE_BEZIER;
      newStrokeId = gimp_vectors_stroke_new_from_points (activeVectorsId
                                     , type
                                     , num_points
                                     , points
                                     , closed
                                     );
      g_free(points);
    }


  }

}  /* end p_set_2_vector_points */


/* -----------------------------------
 * gap_track_detail_on_top_layers
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
gint32
gap_track_detail_on_top_layers(gint32 imageId, gboolean doProgress, FilterValues *valPtr)
{
  gint      l_nlayers;
  gint32   *l_layers_list;

  FrameHistInfo  frameHistInfoData;
  FrameHistInfo *frameHistInfo;
  PixelCoords  currCoords;
  PixelCoords  currCoords2;
  PixelCoords  targetCoords;
  PixelCoords  targetCoords2;
  gint32       locateOffsetX;
  gint32       locateOffsetY;
  gint32       locateOffsetX2;
  gint32       locateOffsetY2;
  gint32       lostTraceCount;

  if(gap_debug)
  {
      printf("gap_track_detail_on_top_layers: START\n"
             "  refShapeRadius:%d targetMoveRadius:%d locateColordiff:%.4f\n"
             "  coordsRelToFrame1:%d  offsX:%d offsY:%d removeMidlayers:%d bgLayerIsReference:%d\n"
             "  moveLogFile:%s\n"
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

  currCoords.valid = FALSE;
  targetCoords.valid = FALSE;

  l_layers_list = gimp_image_get_layers(imageId, &l_nlayers);
  if((l_layers_list != NULL)
  && (l_nlayers > 0))
  {
    gint32 topLayerId;

    topLayerId = l_layers_list[0];

    frameHistInfo->frameNr += 1;

    p_get_frameHistInfo(frameHistInfo);
    locateOffsetX = 0;
    locateOffsetY = 0;
    locateOffsetX2 = 0;
    locateOffsetY2 = 0;

    if (l_nlayers == 1)
    {
      p_capture_2_vector_points(imageId, &currCoords, &currCoords2);

      frameHistInfo->lostTraceCount = 0;
      p_copy_src_to_dst_coords(&currCoords,  &frameHistInfo->startCoords);
      p_copy_src_to_dst_coords(&currCoords2, &frameHistInfo->startCoords2);
      p_copy_src_to_dst_coords(&currCoords,  &frameHistInfo->prevCoords);
      p_copy_src_to_dst_coords(&currCoords2, &frameHistInfo->prevCoords2);

      frameHistInfo->frameNr = 1;
      p_coords_logging(frameHistInfo->frameNr
                      , &currCoords
                      , &currCoords2
                      , &frameHistInfo->startCoords
                      , &frameHistInfo->startCoords2
                      , valPtr
                      , imageId
                      );
    }
    else
    {
      gint32 refLayerId;

      refLayerId = l_layers_list[1];
      if (valPtr->bgLayerIsReference == TRUE)
      {
        refLayerId = l_layers_list[l_nlayers -1];
      }


      if(frameHistInfo->startCoords.valid != TRUE)
      {
        p_capture_2_vector_points(imageId, &currCoords, &currCoords2);

        frameHistInfo->lostTraceCount = 0;
        p_copy_src_to_dst_coords(&currCoords,  &frameHistInfo->startCoords);
        p_copy_src_to_dst_coords(&currCoords2, &frameHistInfo->startCoords2);
        p_copy_src_to_dst_coords(&currCoords,  &frameHistInfo->prevCoords);
        p_copy_src_to_dst_coords(&currCoords2, &frameHistInfo->prevCoords2);

        frameHistInfo->frameNr = p_parse_frame_nr_from_layerId(refLayerId);
        p_coords_logging(frameHistInfo->frameNr
                      , &currCoords
                      , &currCoords2
                      , &frameHistInfo->startCoords
                      , &frameHistInfo->startCoords2
                      , valPtr
                      , imageId
                      );
      }
      else if (valPtr->bgLayerIsReference == TRUE)
      {
        /* when all trackings refere to initial BG layer (that is always kept as reference
         * for all further frames), we do not capture currCoords
         * but copy the initial start values.
         */
        p_copy_src_to_dst_coords(&frameHistInfo->startCoords, &currCoords);
        p_copy_src_to_dst_coords(&frameHistInfo->startCoords2, &currCoords2);

        /* locate shall start investigations at matching coordinates of the previous processed frame
         * because the chance to find the detail near this postion is much greater than near
         * the start coords in the initial frame.
         * (note that locate does use the initial frame e.g. the BG layer in this mode,
         * but without the locateOffsets we might loose track of the detail when it moves outside the targetRadius
         * and increasing the targetRadius would also result in siginificant longer processing time)
         */
        if (frameHistInfo->prevCoords.valid)
        {
          locateOffsetX = frameHistInfo->prevCoords.px - frameHistInfo->startCoords.px;
          locateOffsetY = frameHistInfo->prevCoords.py - frameHistInfo->startCoords.py;
	    }
        if (frameHistInfo->prevCoords2.valid)
        {
          locateOffsetX2 = frameHistInfo->prevCoords2.px - frameHistInfo->startCoords2.px;
          locateOffsetY2 = frameHistInfo->prevCoords2.py - frameHistInfo->startCoords2.py;
	    }
      }
      else
      {
	    /* tracking is done with reference to the previous layer
	     * therefore refresh capture. (currCoords are set to the targetCoords
	     * that were calculated in previous processing step)
	     */
        p_capture_2_vector_points(imageId, &currCoords, &currCoords2);
      }

      lostTraceCount = frameHistInfo->lostTraceCount;

      if (currCoords.valid == TRUE)
      {
        p_locate_target(refLayerId
                           , &currCoords
                           , topLayerId
                           , &targetCoords
                           , locateOffsetX
                           , locateOffsetY
                           , valPtr
                           , &frameHistInfo->lostTraceCount
                           );
      }

      if (currCoords2.valid == TRUE)
      {
        p_locate_target(refLayerId
                           , &currCoords2
                           , topLayerId
                           , &targetCoords2
                           , locateOffsetX2
                           , locateOffsetY2
                           , valPtr
                           , &frameHistInfo->lostTraceCount
                           );
      }


    }



    if (targetCoords.valid == TRUE)
    {
      gap_image_remove_all_guides(imageId);
      p_set_2_vector_points(imageId, &targetCoords, &targetCoords2);

      p_copy_src_to_dst_coords(&targetCoords,  &frameHistInfo->prevCoords);
      p_copy_src_to_dst_coords(&targetCoords2, &frameHistInfo->prevCoords2);

      frameHistInfo->frameNr = p_parse_frame_nr_from_layerId(topLayerId);
      p_coords_logging(frameHistInfo->frameNr
                      , &targetCoords
                      , &targetCoords2
                      , &frameHistInfo->startCoords
                      , &frameHistInfo->startCoords2
                      , valPtr
                      , imageId
                      );

    }

    if (valPtr->removeMidlayers == TRUE)
    {
      gap_image_limit_layers(imageId
                            , 2   /* keepTopLayers */
                            , 1   /* keepBgLayers */
                            );
    }

    p_set_frameHistInfo(frameHistInfo);
    g_free(l_layers_list);

    if ((lostTraceCount == 0)
    &&  (frameHistInfo->lostTraceCount > 0))
    {
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
  fiVals->coordsRelToFrame1          = DEFAULT_coordsRelToFrame1;
  fiVals->offsX                      = DEFAULT_offsX;
  fiVals->offsY                      = DEFAULT_offsY;
  fiVals->offsRotate                 = DEFAULT_offsRotate;
  fiVals->enableScaling              = DEFAULT_enableScaling;
  fiVals->removeMidlayers            = DEFAULT_removeMidlayers;
  fiVals->bgLayerIsReference         = DEFAULT_bgLayerIsReference;
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
#define DETAIL_TRACKING_DIALOG_ARGC 12

  static GapArrArg  argv[DETAIL_TRACKING_DIALOG_ARGC];
  gint ii;
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


  ii=0; gap_arr_arg_init(&argv[ii], GAP_ARR_WGT_LABEL);
  argv[0].label_txt = _("This filter requires a current path with one or 2 anchor points\n"
                        "to mark coordinate(s) to be tracked in the target frame(s)");


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
                         "OFF: Calculate only rotation and keep orignal size.");
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

