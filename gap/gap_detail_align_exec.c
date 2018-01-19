/*  gap_detail_align_exec.c
 *    This transforms and/or moves the active layer with 8, 4 or 2 controlpoints.
 *    controlpoints input from current path (or from an xml input file recorded by GAP detail tracking feature)
 *    4 points: rotate scale and move the layer in a way that 2 reference points match 2 target points. 
 *    2 points: simple move the layer from reference point to target point.
 *
 *    8 points: perform a perspective transformation on the layer in a way that 4 points match 4 target points.
 *              this kind of operation is supported when xml input file with 
 *                 p1x, p1y, p2x, p2y, p3x, p3y, p4x, p4y  and
 *                 s1x, s1y, s2x, s2y, s3x, s3y, s4x, s4y  values
 *              is provided.
 *              From GUI 2 paths are required for the perspective mode, each of them must have 4 points..
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

#include "gap_base.h"
#include "gap_geo.h"
#include "gap_libgapbase.h"
#include "gap_locate2.h"
#include "gap_detail_align_exec.h"

#include "gap-intl.h"

#define GIMPRC_EXACT_ALIGN_PATH_POINT_ORDER "gap-exact-aligner-path-point-order"
#define GAP_RESPONSE_REFRESH_PATH 1

#define GAP_EXACT_ALIGNER_REF_IMAGE "gap-exact-aligner-ref-image"
#define GAP_EXACT_ALIGNER_PREV_FAME_PHASE "gap-exact-aligner-prev-frame-phase"

typedef struct AlignDialogVals
{
  gint32    pointOrder;
  
  gboolean  runExactAlign;
  gint      countVaildPoints;
  gint32    image_id;
  gint32    activeDrawableId;

  GtkWidget *radio_order_mode_31_42;
  GtkWidget *radio_order_mode_21_43;
  GtkWidget *radio_order_mode_1234;
  GtkWidget *infoLabel;
  GtkWidget *okButton;
  GtkWidget *shell;

} AlignDialogVals;




typedef struct ParseContext {
  char  *parsePtr;
  gint32 frameNr;
  GapAlignCoords *alignCoords;
} ParseContext;

typedef struct ShortLists {
  GapLocateTuneOffsElem *shortListP1;
  GapLocateTuneOffsElem *shortListP2;
  GapLocateTuneOffsElem *shortListP3;
  GapLocateTuneOffsElem *shortListP4;
} ShortLists;



#define DEFAULT_framePhase 1

static gboolean     p_parse_value_gint32(ParseContext *parseCtx, gint32 *valDestPtr, gint *itemCount);
static gboolean     p_parse_coords_p1_upto_s4(ParseContext *parseCtx
                      , GapPixelCoords *p1, GapPixelCoords *p2, GapPixelCoords *p3, GapPixelCoords *p4
                      , gint32 *frameNrPtr
                      , GapPixelCoords *s1, GapPixelCoords *s2, GapPixelCoords *s3, GapPixelCoords *s4
                      , GapPixelCoords *u1, GapPixelCoords *u2, GapPixelCoords *u3, GapPixelCoords *u4);
static gboolean     p_parse_xml_controlpoint_coords(ParseContext *parseCtx);
static gboolean     p_parse_xml_controlpoint_coords_from_file(const char *filename, gint32 frameNr, GapAlignCoords *alignCoords);
static gint32       p_set_drawable_offsets(gint32 activeDrawableId, GapAlignCoords *alignCoords);
static gint32       p_exact_align_drawable(gint32 activeDrawableId, GapAlignCoords *alignCoords);

static gint32       p_findRefImage();
static void         p_layerForceAlphaAndImageSize(gint32 layerId);
static gint32       p_recreateRefImage(gint32 referenceLayerId, gint32 transformedDrawableId);
static gint32       p_perspective_align_drawable(gint32 activeDrawableId, GapAlignCoords *alignCoords, gint32 framePhase
                       , gdouble transformPrecisionThreshold, gdouble transformPrecision);
static void         p_create_or_replace_path_vectors(gint32 imageId
                      , GapPixelCoords gapPixelCoordsArray[], gint coordsArrayPointsCount, gchar *vectorsName
                      , gboolean setVisible);


/* internal procedures for GUI purpose */
static void         p_save_gimprc_gint32_value(const char *gimprc_option_name, gint32 value);
static void         p_exact_align_calculate_4point_values(GapAlignCoords *alignCoords
                      , gdouble *angleDeg, gdouble *scalePercent, gdouble *moveDx, gdouble *moveDy);
static void         p_refresh_and_update_infoLabel(GtkWidget *widgetDummy, AlignDialogVals *advPtr);
static void         on_exact_align_response (GtkWidget *widget,
                      gint       response_id, AlignDialogVals *advPtr);
static void         on_order_mode_radio_callback(GtkWidget *wgt, gpointer user_data);
static void         p_align_dialog(AlignDialogVals *advPtr);
static gint         p_capture_4_vector_points(gint32 imageId, GapAlignCoords *alignCoords, gint32 pointOrder);
static gint         p_capture_4_vector_points_from_pathname(gint32 imageId, GapAlignCoords *alignCoords, gint32 pointOrder, char *vectorsName);
static void         p_generateFineTuningPerCoords(GapPerspectiveTransCoords *perCoords);
static void         p_fineTuneProbePerspectiveTransformationOld(gint32 activeDrawableId, gint32 referenceLayerId, GapPerspectiveTransCoords *perCoords);
static void         p_fineTuneProbePerspectiveTransformationOld2(gint32 activeDrawableId, gint32 referenceLayerId, GapAlignCoords *alignCoords, GapPerspectiveTransCoords *perCoords);
GapLocateTuneOffsElem * p_buildTuningShortList(GapPixelCoords *tunedCoord, GapPixelCoords *untunedCoord);
GapLocateTuneOffsElem * p_mergeTuningShortList(GapLocateTuneOffsElem *rootShortListA, GapLocateTuneOffsElem *rootShortListB);
static void         p_filterTuningShortList(GapLocateTuneOffsElem *rootShortList);
static void         p_pickTuneCoordinateVariants(gint32 activeDrawableId, gint32 referenceLayerId, GapAlignCoords *alignCoords, GapPerspectiveTransCoords *perCoords, gint32 framePhase, ShortLists *sl);
static void         p_renderPickedPathVariant(gint32 activeDrawableId, gint32 pickedArrayIdx, GapAlignCoords *alignCoords, gint32 framePhase, ShortLists *sl);
static void         p_fineTuneProbePerspectiveTransformation(gint32 activeDrawableId, gint32 referenceLayerId, GapAlignCoords *alignCoords, GapPerspectiveTransCoords *perCoords, gint32 framePhase);


/* --------------------------------------
 * p_parse_value_gint32
 * --------------------------------------
 */
static gboolean
p_parse_value_gint32(ParseContext *parseCtx, gint32 *valDestPtr, gint *itemCount)
{
  gint64 value64;
  gchar *endptr;
  
  if(gap_debug)
  {
    printf("p_parse_value_gint32  parsePtr:%.200s\n" 
        ,parseCtx->parsePtr 
        );
  }
  
  
  value64 = g_ascii_strtoll(parseCtx->parsePtr, &endptr, 10);
  if(parseCtx->parsePtr == endptr)
  {
    /* pointer was not advanced (no int number could be scanned */
    return (FALSE);
  }
  if (valDestPtr != NULL)
  {
    *valDestPtr = value64;
  }
  parseCtx->parsePtr = endptr;
  
  if(itemCount != NULL)
  {
    *itemCount +=1;
  }
  return (TRUE);

}  /* end p_parse_value_gint32 */


/* --------------------------------
 * p_parse_coords_p1_upto_s4
 * --------------------------------
 * parse p1x, p1y, p2x, p2y ... values into p1 (mandatory) and p2, p3, p4 (optional) coordinates
 * and parse keyframe_abs value int *frameNrPtr (optional if present)
 * multiple occurances are not tolerated.
 * return TRUE on success.
 * Optional parse s1x, s1y, s2x, s2y ..  values into s1, s2, s3 and s4
 *   Note that Detail tracking is now capable to track more than 2 points and does select
 *   the 2 (or 4) best matching results p1 and p2 out of a list of n points. 
 *   therefore p1 at frame[n] may not correspond to p1 at frame [n-1] as it was in older versions..
 *   as consequence the startpoints s1 (corresponds to p1) and s2 (corresponds to p1)
 *   were added to the xml file, to provide all required information for the current frame 
 *   in the current controlpoint structure.
 *   In case s1 and s2 are not provided in the xml file -- still supported older format --
 *   keep the values unchanged as parsed in the initial call
 * Optional parse u1x, u1y, u2x, u2y ..  values of untuned coords into u1, u2, u3 and u4
 */
static gboolean
p_parse_coords_p1_upto_s4(ParseContext *parseCtx
   , GapPixelCoords *p1, GapPixelCoords *p2, GapPixelCoords *p3, GapPixelCoords *p4
   , gint32 *frameNrPtr
   , GapPixelCoords *sn1, GapPixelCoords *sn2, GapPixelCoords *sn3, GapPixelCoords *sn4
   , GapPixelCoords *un1, GapPixelCoords *un2, GapPixelCoords *un3, GapPixelCoords *un4
   )
{
  gboolean ok;
  gboolean ret;
  gint     px1Count;
  gint     py1Count;
  gint     px2Count;
  gint     py2Count;

  gint     px3Count;
  gint     py3Count;
  gint     px4Count;
  gint     py4Count;
  gint     frCount;
  
  gint     sx1Count;
  gint     sy1Count;
  gint     sx2Count;
  gint     sy2Count;

  gint     sx3Count;
  gint     sy3Count;
  gint     sx4Count;
  gint     sy4Count;

  gint     ux1Count;
  gint     uy1Count;
  gint     ux2Count;
  gint     uy2Count;

  gint     ux3Count;
  gint     uy3Count;
  gint     ux4Count;
  gint     uy4Count;

  GapPixelCoords  dummyCoords;
  GapPixelCoords *s1;
  GapPixelCoords *s2;
  GapPixelCoords *s3;
  GapPixelCoords *s4;
  GapPixelCoords *u1;
  GapPixelCoords *u2;
  GapPixelCoords *u3;
  GapPixelCoords *u4;  
  ok = TRUE;
  frCount  = 0;
  px1Count = 0;
  py1Count = 0;
  px2Count = 0;
  py2Count = 0;
  px3Count = 0;
  py3Count = 0;
  px4Count = 0;
  py4Count = 0;

  sx1Count = 0;
  sy1Count = 0;
  sx2Count = 0;
  sy2Count = 0;
  sx3Count = 0;
  sy3Count = 0;
  sx4Count = 0;
  sy4Count = 0;

  ux1Count = 0;
  uy1Count = 0;
  ux2Count = 0;
  uy2Count = 0;
  ux3Count = 0;
  uy3Count = 0;
  ux4Count = 0;
  uy4Count = 0;
  
  s1 = &dummyCoords;
  s2 = &dummyCoords;
  s3 = &dummyCoords;
  s4 = &dummyCoords;
  if(sn1 != NULL)
  {
    s1 = sn1;
  }
  if(sn2 != NULL)
  {
    s2 = sn2;
  }
  if(sn3 != NULL)
  {
    s3 = sn3;
  }
  if(sn4 != NULL)
  {
    s4 = sn4;
  }

  u1 = &dummyCoords;
  u2 = &dummyCoords;
  u3 = &dummyCoords;
  u4 = &dummyCoords;
  if(un1 != NULL)
  {
    u1 = un1;
  }
  if(un2 != NULL)
  {
    u2 = un2;
  }
  if(un3 != NULL)
  {
    u3 = un3;
  }
  if(un4 != NULL)
  {
    u4 = un4;
  }  
  
  while(*parseCtx->parsePtr != '\0')
  {
    if (strncmp(parseCtx->parsePtr, "keyframe_abs=\"", strlen("keyframe_abs=\"")) == 0)
    {
      parseCtx->parsePtr += strlen("keyframe_abs=\"");
      ok = p_parse_value_gint32(parseCtx, frameNrPtr, &frCount);
    }
    else if (strncmp(parseCtx->parsePtr, "p1x=\"", strlen("p1x=\"")) == 0)
    {
      parseCtx->parsePtr += strlen("p1x=\"");
      ok = p_parse_value_gint32(parseCtx, &p1->px, &px1Count);
    }
    else if (strncmp(parseCtx->parsePtr, "p1y=\"", strlen("p1y=\"")) == 0)
    {
      parseCtx->parsePtr += strlen("p1y=\"");
      ok = p_parse_value_gint32(parseCtx, &p1->py, &py1Count);
    }
    else if (strncmp(parseCtx->parsePtr, "p2x=\"", strlen("p2x=\"")) == 0)
    {
      parseCtx->parsePtr += strlen("p2x=\"");
      ok = p_parse_value_gint32(parseCtx, &p2->px, &px2Count);
    }
    else if (strncmp(parseCtx->parsePtr, "p2y=\"", strlen("p2y=\"")) == 0)
    {
      parseCtx->parsePtr += strlen("p2y=\"");
      ok = p_parse_value_gint32(parseCtx, &p2->py, &py2Count);
    }
    else if (strncmp(parseCtx->parsePtr, "p3x=\"", strlen("p3x=\"")) == 0)
    {
      parseCtx->parsePtr += strlen("p3x=\"");
      ok = p_parse_value_gint32(parseCtx, &p3->px, &px3Count);
    }
    else if (strncmp(parseCtx->parsePtr, "p3y=\"", strlen("p3y=\"")) == 0)
    {
      parseCtx->parsePtr += strlen("p3y=\"");
      ok = p_parse_value_gint32(parseCtx, &p3->py, &py3Count);
    }
    else if (strncmp(parseCtx->parsePtr, "p4x=\"", strlen("p4x=\"")) == 0)
    {
      parseCtx->parsePtr += strlen("p4x=\"");
      ok = p_parse_value_gint32(parseCtx, &p4->px, &px4Count);
    }
    else if (strncmp(parseCtx->parsePtr, "p4y=\"", strlen("p4y=\"")) == 0)
    {
      parseCtx->parsePtr += strlen("p4y=\"");
      ok = p_parse_value_gint32(parseCtx, &p4->py, &py4Count);
    }    
    /* ------------ startcoordinates ------------------- */
    else if (strncmp(parseCtx->parsePtr, "s1x=\"", strlen("s1x=\"")) == 0)
    {
      parseCtx->parsePtr += strlen("s1x=\"");
      ok = p_parse_value_gint32(parseCtx, &s1->px, &sx1Count);
    }
    else if (strncmp(parseCtx->parsePtr, "s1y=\"", strlen("s1y=\"")) == 0)
    {
      parseCtx->parsePtr += strlen("s1y=\"");
      ok = p_parse_value_gint32(parseCtx, &s1->py, &sy1Count);
    }
    else if (strncmp(parseCtx->parsePtr, "s2x=\"", strlen("s2x=\"")) == 0)
    {
      parseCtx->parsePtr += strlen("s2x=\"");
      ok = p_parse_value_gint32(parseCtx, &s2->px, &sx2Count);
    }
    else if (strncmp(parseCtx->parsePtr, "s2y=\"", strlen("s2y=\"")) == 0)
    {
      parseCtx->parsePtr += strlen("s2y=\"");
      ok = p_parse_value_gint32(parseCtx, &s2->py, &sy2Count);
    }
    else if (strncmp(parseCtx->parsePtr, "s3x=\"", strlen("s3x=\"")) == 0)
    {
      parseCtx->parsePtr += strlen("s3x=\"");
      ok = p_parse_value_gint32(parseCtx, &s3->px, &sx3Count);
    }
    else if (strncmp(parseCtx->parsePtr, "s3y=\"", strlen("s3y=\"")) == 0)
    {
      parseCtx->parsePtr += strlen("s3y=\"");
      ok = p_parse_value_gint32(parseCtx, &s3->py, &sy3Count);
    }
    else if (strncmp(parseCtx->parsePtr, "s4x=\"", strlen("s4x=\"")) == 0)
    {
      parseCtx->parsePtr += strlen("s4x=\"");
      ok = p_parse_value_gint32(parseCtx, &s4->px, &sx4Count);
    }
    else if (strncmp(parseCtx->parsePtr, "s4y=\"", strlen("s4y=\"")) == 0)
    {
      parseCtx->parsePtr += strlen("s4y=\"");
      ok = p_parse_value_gint32(parseCtx, &s4->py, &sy4Count);
    }  
    /* ------------ untuned coords  ------------------- */
    else if (strncmp(parseCtx->parsePtr, "u1x=\"", strlen("u1x=\"")) == 0)
    {
      parseCtx->parsePtr += strlen("u1x=\"");
      ok = p_parse_value_gint32(parseCtx, &u1->px, &ux1Count);
    }
    else if (strncmp(parseCtx->parsePtr, "u1y=\"", strlen("u1y=\"")) == 0)
    {
      parseCtx->parsePtr += strlen("u1y=\"");
      ok = p_parse_value_gint32(parseCtx, &u1->py, &uy1Count);
    }
    else if (strncmp(parseCtx->parsePtr, "u2x=\"", strlen("u2x=\"")) == 0)
    {
      parseCtx->parsePtr += strlen("u2x=\"");
      ok = p_parse_value_gint32(parseCtx, &u2->px, &ux2Count);
    }
    else if (strncmp(parseCtx->parsePtr, "u2y=\"", strlen("u2y=\"")) == 0)
    {
      parseCtx->parsePtr += strlen("u2y=\"");
      ok = p_parse_value_gint32(parseCtx, &u2->py, &uy2Count);
    }
    else if (strncmp(parseCtx->parsePtr, "u3x=\"", strlen("u3x=\"")) == 0)
    {
      parseCtx->parsePtr += strlen("u3x=\"");
      ok = p_parse_value_gint32(parseCtx, &u3->px, &ux3Count);
    }
    else if (strncmp(parseCtx->parsePtr, "u3y=\"", strlen("u3y=\"")) == 0)
    {
      parseCtx->parsePtr += strlen("u3y=\"");
      ok = p_parse_value_gint32(parseCtx, &u3->py, &uy3Count);
    }
    else if (strncmp(parseCtx->parsePtr, "u4x=\"", strlen("u4x=\"")) == 0)
    {
      parseCtx->parsePtr += strlen("u4x=\"");
      ok = p_parse_value_gint32(parseCtx, &u4->px, &ux4Count);
    }
    else if (strncmp(parseCtx->parsePtr, "u4y=\"", strlen("u4y=\"")) == 0)
    {
      parseCtx->parsePtr += strlen("u4y=\"");
      ok = p_parse_value_gint32(parseCtx, &u4->py, &uy4Count);
    }    
    else if (strncmp(parseCtx->parsePtr, "/>", strlen("/>")) == 0)
    {
      /* stop evaluate when current controlpoint ends */
      parseCtx->parsePtr += strlen("/>");
      break;
    }
    else if (strncmp(parseCtx->parsePtr, "<controlpoint ", strlen("<controlpoint ")) == 0)
    {
      /* stop evaluate when when we run into the next controlpoint */
      break;
    }
    else
    {
      parseCtx->parsePtr++;
    }
    
    if(ok != TRUE)
    {
     break;
    }
  }

  ret = FALSE;
  
  if ((ok == TRUE)
  && (px1Count == 1)
  && (py1Count == 1)
  && (sx1Count <= 1)
  && (sy1Count <= 1)
  && (frCount <= 1))
  {
    p1->valid = TRUE;
    if ((px2Count == 1)
    &&  (py2Count == 1))
    {
      p2->valid = TRUE;
    }
    if ((px3Count == 1)
    &&  (py3Count == 1))
    {
      p3->valid = TRUE;
    }
    if ((px4Count == 1)
    &&  (py4Count == 1))
    {
      p4->valid = TRUE;
    }



    if ((sx1Count == 1)
    && (sy1Count == 1)
    && (s1 != NULL))
    {
      s1->valid = TRUE;
    }

    if ((sx2Count == 1)
    && (sy2Count == 1)
    && (s2 != NULL))
    {
      s2->valid = TRUE;
    }

    if ((sx3Count == 1)
    && (sy3Count == 1)
    && (s3 != NULL))
    {
      s3->valid = TRUE;
    }

    if ((sx4Count == 1)
    && (sy4Count == 1)
    && (s4 != NULL))
    {
      s4->valid = TRUE;
    }

    if ((ux1Count == 1)
    && (uy1Count == 1)
    && (u1 != NULL))
    {
      u1->valid = TRUE;
    }

    if ((ux2Count == 1)
    && (uy2Count == 1)
    && (u2 != NULL))
    {
      u2->valid = TRUE;
    }

    if ((ux3Count == 1)
    && (uy3Count == 1)
    && (u3 != NULL))
    {
      u3->valid = TRUE;
    }

    if ((ux4Count == 1)
    && (uy4Count == 1)
    && (u4 != NULL))
    {
      u4->valid = TRUE;
    }
    
    ret = TRUE;

  }


  
  if(gap_debug)
  {
    printf("p_parse_coords_p1_upto_s4 ok:%d "
           " px1Count:%d py1Count:%d px2Count:%d py2Count:%d px3Count:%d py3Count:%d px4Count:%d py4Count:%d\n"
           " sx1Count:%d sy1Count:%d sx2Count:%d sy2Count:%d sx3Count:%d sy3Count:%d sx4Count:%d sy4Count:%d\n"
           " ux1Count:%d uy1Count:%d ux2Count:%d uy2Count:%d ux3Count:%d uy3Count:%d ux4Count:%d uy4Count:%d\n"
           "  frCount:%d parsePtr:%.400s\n\n" 
      ,(int)ok
      ,(int)px1Count
      ,(int)py1Count
      ,(int)px2Count
      ,(int)py2Count
      ,(int)px3Count
      ,(int)py3Count
      ,(int)px4Count
      ,(int)py4Count
      ,(int)sx1Count
      ,(int)sy1Count
      ,(int)sx2Count
      ,(int)sy2Count
      ,(int)sx3Count
      ,(int)sy3Count
      ,(int)sx4Count
      ,(int)sy4Count
      ,(int)ux1Count
      ,(int)uy1Count
      ,(int)ux2Count
      ,(int)uy2Count
      ,(int)ux3Count
      ,(int)uy3Count
      ,(int)ux4Count
      ,(int)uy4Count
      ,(int)frCount
      ,parseCtx->parsePtr 
      );
  }

  return (ret);
  
}  /* end p_parse_coords_p1_upto_s4 */


static void
p_find_attribute_start(ParseContext *parseCtx)
{
  while(*parseCtx->parsePtr != '\0')
  {
    if(*parseCtx->parsePtr == '<')
    {
      break;
    }
    parseCtx->parsePtr++;
  }
  
  if(gap_debug)
  {
    printf("\nSTRING at parsePtr:%.200s\n"
      , parseCtx->parsePtr
      );
  }
}   /* end p_find_attribute_start */


/* --------------------------------
 * p_parse_xml_controlpoint_coords
 * --------------------------------
 *
 */
static gboolean
p_parse_xml_controlpoint_coords(ParseContext *parseCtx)
{
  gboolean     ok;
  gint32       frameNr;

  ok = TRUE;
  frameNr = -1;
  
  while(*parseCtx->parsePtr != '\0')
  {
    p_find_attribute_start(parseCtx);

    if (strncmp(parseCtx->parsePtr, "<controlpoint ", strlen("<controlpoint ")) == 0)
    {
      parseCtx->parsePtr += strlen("<controlpoint ");
      
      if(parseCtx->alignCoords->startCoords[0].valid == FALSE)
      {
        ok = p_parse_coords_p1_upto_s4(parseCtx
                               , &parseCtx->alignCoords->startCoords[0]
                               , &parseCtx->alignCoords->startCoords[1]
                               , &parseCtx->alignCoords->startCoords[2]
                               , &parseCtx->alignCoords->startCoords[3]
                               , &frameNr
                               , NULL
                               , NULL
                               , NULL
                               , NULL

                               , NULL
                               , NULL
                               , NULL
                               , NULL
                               );
      }
      else
      {
        ok = p_parse_coords_p1_upto_s4(parseCtx
                               , &parseCtx->alignCoords->currCoords[0]
                               , &parseCtx->alignCoords->currCoords[1]
                               , &parseCtx->alignCoords->currCoords[2]
                               , &parseCtx->alignCoords->currCoords[3]
                               , &frameNr
                               , &parseCtx->alignCoords->startCoords[0]
                               , &parseCtx->alignCoords->startCoords[1]
                               , &parseCtx->alignCoords->startCoords[2]
                               , &parseCtx->alignCoords->startCoords[3]
                               , &parseCtx->alignCoords->untunedCoords[0]
                               , &parseCtx->alignCoords->untunedCoords[1]
                               , &parseCtx->alignCoords->untunedCoords[2]
                               , &parseCtx->alignCoords->untunedCoords[3]
                               );
      }

      if(gap_debug)
      {
        printf("p_parse_xml_controlpoint_coords: ok:%d, frameNr:%d parseCtx->frameNr:%d\n"
          ,(int)ok
          ,(int)frameNr
          ,(int)parseCtx->frameNr
          );
      }

      if(ok != TRUE)
      {
        return (FALSE);
      }

      if ((frameNr == parseCtx->frameNr)
      &&  (parseCtx->alignCoords->currCoords[1].valid == TRUE))
      {
        return(TRUE);
      }
    }
    parseCtx->parsePtr++;
  }


  if ((ok == TRUE)
  &&  (parseCtx->alignCoords->currCoords[1].valid == TRUE))
  {
    /* accept the last controlpoint when no matching frameNr was found */
    return(TRUE);
  }
  
  return(FALSE);

}  /* end p_parse_xml_controlpoint_coords */


/* -----------------------------------------
 * p_parse_xml_controlpoint_coords_from_file
 * -----------------------------------------
 * parse coords of 1st controlpoint and the controlpoint at frameNr
 * from the xml file.
 * return TRUE on success.
 */
static gboolean
p_parse_xml_controlpoint_coords_from_file(const char *filename, gint32 frameNr, GapAlignCoords *alignCoords)
{
  char *textBuffer;
  gsize lengthTextBuffer;
  
  ParseContext parseCtx;
  gboolean     parseOk;
  
  textBuffer = NULL;
  parseOk = FALSE;
  if ((g_file_get_contents (filename, &textBuffer, &lengthTextBuffer, NULL) != TRUE) 
  || (textBuffer == NULL))
  {
    printf("Couldn't load XML file:%s\n", filename);
    return(FALSE);
  }

  parseCtx.parsePtr = textBuffer;
  parseCtx.alignCoords = alignCoords;
  parseCtx.frameNr = frameNr;

  parseCtx.alignCoords->startCoords[0].valid  = FALSE;
  parseCtx.alignCoords->startCoords[1].valid = FALSE;
  parseCtx.alignCoords->currCoords[0].valid   = FALSE;
  parseCtx.alignCoords->currCoords[1].valid  = FALSE;
  
  parseOk = p_parse_xml_controlpoint_coords(&parseCtx);
    
    
  g_free(textBuffer);
  
  return (parseOk);

}  /* end p_parse_xml_controlpoint_coords_from_file */


/* -----------------------------------
 * p_set_drawable_offsets
 * -----------------------------------
 * simple 2-point align via offsets (without rotate and scale)
 */
static gint32
p_set_drawable_offsets(gint32 activeDrawableId, GapAlignCoords *alignCoords)
{
  gdouble px1, py1, px2, py2;
  gdouble dx, dy;
  gint    offset_x;
  gint    offset_y;
  

  px1 = alignCoords->startCoords[0].px;
  py1 = alignCoords->startCoords[0].py;
  px2 = alignCoords->currCoords[0].px;
  py2 = alignCoords->currCoords[0].py;

  dx = px2 - px1;
  dy = py2 - py1;

  /* findout the offsets of the original layer within the source Image */
  gimp_drawable_offsets(activeDrawableId, &offset_x, &offset_y );
  gimp_layer_set_offsets(activeDrawableId, offset_x - dx, offset_y - dy);
  
  return (activeDrawableId);

}  /* end p_set_drawable_offsets */


/* -----------------------------------
 * p_exact_align_drawable
 * -----------------------------------
 * 4-point alignment including necessary scale and rotate transformation
 * to match 2 pairs of corresponding coordonates.
 */
static gint32
p_exact_align_drawable(gint32 activeDrawableId, GapAlignCoords *alignCoords)
{
  gdouble px1, py1, px2, py2;
  gdouble px3, py3, px4, py4;
  gdouble dx1, dy1, dx2, dy2;
  gdouble angle1Rad, angle2Rad, angleRad;
  gdouble len1, len2;
  gdouble scaleXY;
  gint32  transformedDrawableId;
  
  px1 = alignCoords->startCoords[0].px;
  py1 = alignCoords->startCoords[0].py;
  px2 = alignCoords->startCoords[1].px;
  py2 = alignCoords->startCoords[1].py;
  
  px3 = alignCoords->currCoords[0].px;
  py3 = alignCoords->currCoords[0].py;
  px4 = alignCoords->currCoords[1].px;
  py4 = alignCoords->currCoords[1].py;

  dx1 = px2 - px1;
  dy1 = py2 - py1;
  dx2 = px4 - px3;
  dy2 = py4 - py3;
  
  /* the angle between the two lines. i.e., the angle layer2 must be clockwise rotated
   * in order to overlap with initial start layer1
   */
  angle1Rad = 0;
  angle2Rad = 0;
  if (dx1 != 0.0)
  {
    angle1Rad = atan(dy1 / dx1);
  }
  if (dx2 != 0.0)
  {
    angle2Rad = atan(dy2 / dx2);
  }
  angleRad = angle1Rad - angle2Rad;
  
  /* the scale factors current layer must be mulitplied by,
   * in order to fit onto reference start layer.
   * this is simply the ratio of the two line lenths from the path we created with the 4 points
   */
  
  len1 = sqrt((dx1 * dx1) + (dy1 * dy1));
  len2 = sqrt((dx2 * dx2) + (dy2 * dy2));

  scaleXY = 1.0;
  if ((len1 != len2)
  &&  (len2 != 0.0))
  {
    scaleXY = len1 / len2;
  }

  gimp_context_set_defaults();
  gimp_context_set_transform_resize(GIMP_TRANSFORM_RESIZE_ADJUST);   /* do NOT clip */                                 
  gimp_context_set_transform_direction(GIMP_TRANSFORM_FORWARD);                                 
  transformedDrawableId = gimp_item_transform_2d(activeDrawableId
                            , px3
                            , py3
                            , scaleXY 
                            , scaleXY
                            , angleRad
                            , px1
                            , py1
                            );

  if(gap_debug)
  {
    printf("p_exact_align_drawable: activeDrawableId:%d transformedDrawableId:%d\n"
           "    p1: %f %f\n"
           "    p2: %f %f\n"
           "    p3: %f %f\n"
           "    p4: %f %f\n"
           "    scale:%f  angleRad:%f (degree:%f)\n"
      ,(int)activeDrawableId
      ,(int)transformedDrawableId
      ,(float)px1
      ,(float)py1
      ,(float)px2
      ,(float)py2
      ,(float)px3
      ,(float)py3
      ,(float)px4
      ,(float)py4
      ,(float)scaleXY
      ,(float)angleRad
      ,(float)(angleRad * 180.0) / G_PI
      );
  }
  return (transformedDrawableId);  
  
}  /* end p_exact_align_drawable */




/* -----------------------------------
 * p_findGint32Value
 * -----------------------------------
 * find the reference image (used in previous calls in the same gimp session)
 */
static gint32
p_findGint32Value(const char *key)
{
  gint32 value;
  int l_len;
  
  /* init default value */
  value = -1;

  l_len = gimp_get_data_size (key);
  if (l_len == sizeof(gint32))
  {
    gimp_get_data (key, &value);
  }

  return (value);

}  /* end p_findGint32Value */


/* -----------------------------------
 * p_findRefImage
 * -----------------------------------
 * find the reference image (used in previous calls in the same gimp session)
 */
static gint32
p_findRefImage()
{
  gint32 refImageId;
  refImageId = p_findGint32Value(GAP_EXACT_ALIGNER_REF_IMAGE);
  return (refImageId);
}  /* end p_findRefImage */

/* -----------------------------------
 * p_layerForceAlphaAndImageSize
 * -----------------------------------
 * make sure that the layer has alpha channel
 * and is same size as its image.
 */
static void 
p_layerForceAlphaAndImageSize(gint32 layerId)
{
  if(! gimp_drawable_has_alpha(layerId))
  {
     /* have to add alpha channel */
     gimp_layer_add_alpha(layerId);
  }

  gimp_layer_resize_to_image_size(layerId);

}  /* end p_layerForceAlphaAndImageSize */


/* -----------------------------------
 * p_recreateRefImage
 * -----------------------------------
 * The reference image is upadted or created from referenceLayerId and the transformedDrawableId.
 * it typically has just one layer named "REF" (GAP_EXACT_ALIGNER_REF_LAYER_NAME)
 * and is used as dynamic changing reference for fine tuning purpose
 * to represent the previous rendered frame,
 * where only the opaque pixels of the initial reference layer are set opaque.
 * Note that opaque pixels typically are used to mark samll background areas around tracking points
 * that will be used for fine tuning the perspective transformation when rendering the current frame.
 * 
 * returns the id of the (re)created reference layer in the reference image.
 */
static gint32    p_recreateRefImage(gint32 referenceLayerId, gint32 transformedDrawableId)
{
  gint32 refImageId;
  gint32 tmpBgLayerId;
  gint32 tmpTopLayerId;
  gint32 newRefLayerId;
  gint32 layermaskId;
  gint    l_src_offset_x, l_src_offset_y;    /* layeroffsets as they were in src_image */
  gboolean addDisplay;

  addDisplay = FALSE;
  refImageId = p_findRefImage();

  if (gap_image_is_alive(refImageId))
  {
    tmpBgLayerId = gimp_image_merge_visible_layers (refImageId, GIMP_CLIP_TO_IMAGE);
  }
  else
  {
    /* create a new ref image */
    addDisplay = TRUE;                    
    refImageId = gap_image_new_of_samesize(gimp_item_get_image(referenceLayerId));

   
    /* copy referenceLayerId as tmpBgLayerId Layer to refImageId */
    tmpBgLayerId = gap_layer_copy_to_dest_image(refImageId
                                                , referenceLayerId   /* l_src_layer_id */
                                                , 100.0   /* Opacity */
                                                ,0        /* NORMAL */
                                                ,&l_src_offset_x
                                                ,&l_src_offset_y
                                                );
    gimp_image_insert_layer(refImageId
                           , tmpBgLayerId
                           , 0
                           , 0              /* top of layerstack */
                           );
                           
  }

  p_layerForceAlphaAndImageSize(tmpBgLayerId);
  
  
  tmpTopLayerId = gap_layer_copy_to_dest_image(refImageId
                                               , transformedDrawableId   /* l_src_layer_id */
                                               , 100.0   /* Opacity */
                                               ,0        /* NORMAL */
                                               ,&l_src_offset_x
                                               ,&l_src_offset_y
                                               );
  gimp_image_insert_layer(refImageId
                          , tmpTopLayerId
                          , 0
                          , 0              /* top of layerstack */
                          );
  gimp_layer_set_offsets(tmpTopLayerId, l_src_offset_x, l_src_offset_y);


  p_layerForceAlphaAndImageSize(tmpTopLayerId);
  
  layermaskId = gimp_layer_create_mask(tmpTopLayerId, GIMP_ADD_WHITE_MASK);
  gimp_layer_add_mask(tmpTopLayerId, layermaskId);

  /* copy the alpha channel from BG to the layermask of the top layer */
  gap_layer_copy_picked_channel(layermaskId, 0    /* dst_pick is the alpha channel */
                               ,tmpBgLayerId, 3     /* src_pick is the alpha channel */
                               ,FALSE               /* shadow */
                               );
  
  newRefLayerId = gimp_image_merge_down(refImageId, tmpTopLayerId, GIMP_EXPAND_AS_NECESSARY);
  
  gimp_item_set_name(newRefLayerId, GAP_EXACT_ALIGNER_REF_LAYER_NAME);

  if (addDisplay)
  {
    gimp_display_new(refImageId);
    gimp_displays_flush();
  }
  
  if(gap_debug)
  {
    printf("p_recreateRefImage setData:%s len:%d  refImageId:%d newRefLayerId:%d\n"
      , GAP_EXACT_ALIGNER_REF_IMAGE
      , (int)sizeof (gint32)
      , (int)refImageId
      , (int)newRefLayerId
      );
  }
  gimp_set_data (GAP_EXACT_ALIGNER_REF_IMAGE
                    , &refImageId, sizeof (gint32));
                    
  return (newRefLayerId);

}  /* end p_recreateRefImage */


/* -----------------------------------
 * p_perspective_align_drawable
 * -----------------------------------
 * 8-point alignment including necessary perspective transformation
 * to match 4 pairs of corresponding coordonates.
 */
static gint32
p_perspective_align_drawable(gint32 activeDrawableId, GapAlignCoords *alignCoords, gint32 framePhase
  , gdouble transformPrecisionThreshold, gdouble transformPrecision)
{
  GapPerspectiveTransCoords  perspectiveCoords;
  GapPerspectiveTransCoords *perCoords;
  gboolean perCoordsOk;
  gint32  transformedDrawableId;
  
  
  if(gap_debug)
  {
    printf("p_perspective_align_drawable: Start on activeDrawableId:%d PrecisionThreshold:%f Precision:%f framePhase:%d\n"
       , (int)activeDrawableId
       , (double)transformPrecisionThreshold
       , (double)transformPrecision
       , (int)framePhase
       );
  }  
  
  perCoords = &perspectiveCoords;
  perCoords->transformPrecisionThreshold = transformPrecisionThreshold;
  perCoords->transformPrecision          = transformPrecision;

  perCoordsOk = gap_geo_perspective_trans_coords_from_align_coords(activeDrawableId, alignCoords, perCoords);
  if (perCoordsOk == TRUE)
  {
    gint32 referenceLayerId;
    gint32 refImageId;
    
    referenceLayerId = gimp_image_get_layer_by_name(gimp_item_get_image(activeDrawableId)
                                                   , GAP_EXACT_ALIGNER_REF_LAYER_NAME
                                                   );
    refImageId = -1;
    if (framePhase > 1)
    {
      refImageId = p_findRefImage();
    }

    if(gap_debug)
    {
      printf("p_perspective_align_drawable referenceLayerId:%d refImageId:%d AT framePhase:%d\n"
        , (int)referenceLayerId
        , (int)refImageId
        , (int)framePhase
        );
    }

    
    if((referenceLayerId < 0) && (refImageId >= 0))
    {
       referenceLayerId = gimp_image_get_layer_by_name(refImageId
                                                   , GAP_EXACT_ALIGNER_REF_LAYER_NAME
                                                   );
    }

    if(gap_debug)
    {
      printf("p_perspective_align_drawable searchresult for layername %s is referenceLayerId:%d refImageId:%d AT framePhase:%d\n"
        , GAP_EXACT_ALIGNER_REF_LAYER_NAME
        , (int)referenceLayerId
        , (int)refImageId
        , (int)framePhase
        );
    }


    if(referenceLayerId >= 0)
    {
      /* The presence of a reference layer triggers the fine tuning probe rendering
       * on a temporary created image to select the best matching transformation.
       */
      /// p_fineTuneProbePerspectiveTransformationOld(activeDrawableId, referenceLayerId, perCoords);  // TODO remove this..
      /// p_fineTuneProbePerspectiveTransformationOld2(activeDrawableId, referenceLayerId, alignCoords, perCoords);
      p_fineTuneProbePerspectiveTransformation(activeDrawableId, referenceLayerId, alignCoords, perCoords, framePhase);

    }

  
    gimp_context_set_defaults();
    gimp_context_set_transform_resize(GIMP_TRANSFORM_RESIZE_ADJUST);   /* do NOT clip */                                 
    gimp_context_set_transform_direction(GIMP_TRANSFORM_FORWARD);                                 
    transformedDrawableId = gimp_item_transform_perspective(activeDrawableId
                              , perCoords->x0, perCoords->y0
                              , perCoords->x1, perCoords->y1
                              , perCoords->x2, perCoords->y2
                              , perCoords->x3, perCoords->y3
                              );
    
    // DISABLED the dynamic change of the reference image (that may produce jitter effects ?)
    if(FALSE) // if(referenceLayerId >= 0)
    {
      gint32 prevFramePhase;
      
      prevFramePhase = p_findGint32Value(GAP_EXACT_ALIGNER_PREV_FAME_PHASE);

      /* note that frames modify typically calls this filter
       * with framePhase sequence 1, n, 2, 3, ... n-1
       * Therefore skip recreation of the reference image in the 2nd call whre framePhase == n
       */
      if ((framePhase == 1)
      ||  (framePhase == prevFramePhase +1))
      {
        p_recreateRefImage(referenceLayerId, transformedDrawableId);
      }

      prevFramePhase = framePhase;
      gimp_set_data (GAP_EXACT_ALIGNER_PREV_FAME_PHASE
                    , &prevFramePhase, sizeof (gint32));
    }
    
    if(gap_debug)
    {
      printf("p_perspective_align_drawable: activeDrawableId:%d transformedDrawableId:%d\n"
             "    p0: %f %f\n"
             "    p1: %f %f\n"
             "    p2: %f %f\n"
             "    p3: %f %f\n"
        ,(int)activeDrawableId
        ,(int)transformedDrawableId
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
    return (transformedDrawableId);  

  }
  else
  {
    if(gap_debug)
    {
      printf("p_perspective_align_drawable: activeDrawableId:%d FAILED\n"
        ,(int)activeDrawableId
        );
    }
    return (-1);
  }
  
}  /* end p_perspective_align_drawable */


/* --------------------------------
 * p_create_or_replace_path_vectors
 * --------------------------------
 * if the image already contains a vectors object with the specified vectorsName
 * then  replace it with the points in gapPixelCoordsArray.
 * 
 * in case there is no vectors object with the specified vectorsName create it and add it to the image.
 *
 */
static void
p_create_or_replace_path_vectors(gint32 imageId, GapPixelCoords gapPixelCoordsArray[], gint coordsArrayPointsCount, gchar *vectorsName
   , gboolean setVisible)
{
  gint32  vectorsId;

  gdouble  *points;
  gint      num_points;
  gint      l_idx;
  gboolean  closed;
  GimpVectorsStrokeType type;
  GapPixelCoords *targetCoords;
  
  vectorsId = gimp_image_get_vectors_by_name(imageId, vectorsName);
  if(vectorsId >= 0)
  {
    gimp_image_remove_vectors(imageId, vectorsId);
  } 

  /* create new vectors path */
  vectorsId = gimp_vectors_new(imageId, vectorsName);

  
  if(vectorsId >= 0)  
  {
    num_points = 6 * GAP_ALIGN_COORDS_MAX;
    points = g_new (gdouble, num_points);
  
    for(l_idx = 0; l_idx < coordsArrayPointsCount; l_idx++)
    {
      gdouble pdx;
      gdouble pdy;
      gint    offset;
    
      offset = l_idx * 6;
      targetCoords  = &gapPixelCoordsArray[l_idx];

      pdx = targetCoords->px;
      pdy = targetCoords->py;

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
  

}  /* end p_create_or_replace_path_vectors */


/* -----------------------------------
 * gap_detail_xml_align
 * -----------------------------------
 * This procedure transforms the specified drawable
 * according controlpoints in an XML file that was recorded via detail tracking.
 * it picks the relevant controlpoint at framePhase from the XML file
 * and scales, rotates and aligns the specified drawableId (shall be a layer)
 * in a way that it exactly matches with the 1st (reference) controlpoint in the XML file.
 *
 * in case the XML file provides 4 tracked points and 4 start (referece) points
 * the alignment is done via perspective transformation.
 * 
 * for usable results the tracked points shall meet the follwing conditions:
 * p1 shall be in the upper left quadrant
 * p2 shall be in the upper right quadrant
 * p3 shall be in the lower right quadrant
 * p4 shall be in the lower left quadrant
 *
 * returns the drawable id of the resulting transformed layer (or -1 on errors)s
 */
gint32
gap_detail_xml_align(gint32 drawableId, XmlAlignValues *xaVals)
{
  gint32 newDrawableId;
  
  newDrawableId = drawableId;
  if(gap_debug)
  {
      printf("gap_detail_xml_align: START\n"
             "  framePhase:%d  moveLogFile:%s\n"
            , (int)xaVals->framePhase
             , xaVals->moveLogFile
            );
  }
  
  if(xaVals->framePhase <= 1)
  {
    gint32 referenceLayerId;

    /* when handling the 1st frame (framePhase == 1)
     * recreate the reference image in case the 1st frame contains a reference layer named "REF".
     * (or clear to -1 in case no REF layer is present)
     */
    
    referenceLayerId = gimp_image_get_layer_by_name(gimp_item_get_image(drawableId)
                                                   , GAP_EXACT_ALIGNER_REF_LAYER_NAME
                                                   );
    if(referenceLayerId >= 0)
    {
      p_recreateRefImage(referenceLayerId, referenceLayerId);
    }
    else
    {
      gint32 refImageId;
      refImageId = -1;
      gimp_set_data (GAP_EXACT_ALIGNER_REF_IMAGE
                    , &refImageId, sizeof (gint32));
    }
  }
  else
  {
    gboolean  parseOk;
    GapAlignCoords alignCoords;
    
    parseOk =
      p_parse_xml_controlpoint_coords_from_file(xaVals->moveLogFile
         , xaVals->framePhase, &alignCoords);

    if(parseOk)
    {
      gint idx;
      gint countValidPairs;
      
      countValidPairs = 0;
      for(idx = 0; idx < 4; idx++)
      {
        if ((alignCoords.startCoords[idx].valid == TRUE)
        &&  (alignCoords.currCoords[idx].valid == TRUE))
        {
          countValidPairs++;
        }
      }
      
      if(gap_debug)
      {
        printf("gap_detail_xml_align: framePhase:%d  countValidPairs:%d\n"
                  , (int)xaVals->framePhase
                  , (int)countValidPairs
               );
      }

      if (countValidPairs > 1)
      {
        gint32 imageId;
        
        imageId = gimp_item_get_image(drawableId);
        
        /* import the xml align coordinates as SRC and TARGET path vectors
         * (this is not relevant for automatical processing, but is useful for analyse purpose
         * and allows manually fixes afterwards)
         */
        if (alignCoords.untunedCoords[0].valid && alignCoords.untunedCoords[3].valid)
        {
          p_create_or_replace_path_vectors(imageId
                                       , &alignCoords.untunedCoords[0], countValidPairs
                                       , GAP_EXACT_ALIGNER_USRC_PATH_NAME /* vectorsName */
                                       , TRUE /* setVisible */ 
                                       );
        }
        p_create_or_replace_path_vectors(imageId
                                       , &alignCoords.startCoords[0], countValidPairs
                                       , GAP_EXACT_ALIGNER_TARGET_PATH_NAME /* vectorsName */
                                       , TRUE /* setVisible */ 
                                       );
        p_create_or_replace_path_vectors(imageId
                                       , &alignCoords.currCoords[0], countValidPairs
                                       , GAP_EXACT_ALIGNER_SRC_PATH_NAME /* vectorsName */
                                       , TRUE /* setVisible */ 
                                       );
      }
      
      
      if (countValidPairs == 4)
      {
        /* perspective align transformation with 4 point pairs */
        newDrawableId = p_perspective_align_drawable(drawableId, &alignCoords, xaVals->framePhase
                           , xaVals->transformPrecisionThreshold, xaVals->transformPrecision);
        
      } else if ((alignCoords.startCoords[1].valid == TRUE)
             &&  (alignCoords.currCoords[1].valid == TRUE))
      {
        /* exact align transformation with 2 point pairs including rotation and scaling */
        newDrawableId = p_exact_align_drawable(drawableId, &alignCoords);
      }
      else
      {
        /* simple move (to match current recorded point to recorded start point) */
        newDrawableId = p_set_drawable_offsets(drawableId, &alignCoords);
      }
    }
    else
    {
      newDrawableId = -1;
    }

  }
 
  return(newDrawableId);
  
}  /* end gap_detail_xml_align */




/* -----------------------------------
 * gap_detail_xml_align_get_values
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
gap_detail_xml_align_get_values(XmlAlignValues *xaVals)
{
  int l_len;

  /* init default values */
  xaVals->framePhase                  = DEFAULT_framePhase;
  xaVals->transformPrecisionThreshold = GAP_GEO_TRANSFORM_PRECISION_THRSHOLD;
  xaVals->transformPrecision          = GAP_GEO_TRANSFORM_PRECISION;
  xaVals->moveLogFile[0]              = '\0';

  l_len = gimp_get_data_size (GAP_DETAIL_TRACKING_XML_ALIGNER_PLUG_IN_NAME);
  if (l_len == sizeof(XmlAlignValues))
  {
    /* Possibly retrieve data from a previous interactive run */
    gimp_get_data (GAP_DETAIL_TRACKING_XML_ALIGNER_PLUG_IN_NAME, xaVals);
    
    if(gap_debug)
    {
      printf("gap_detail_xml_align_get_values FOUND data for key:%s\n"
        , GAP_DETAIL_TRACKING_XML_ALIGNER_PLUG_IN_NAME
        );
    }
  }

  if(gap_debug)
  {
    printf("gap_detail_xml_align_get_values:\n"
           "  framePhase:%d  transformPrecisionThreshold:%f  transformPrecision:%f moveLogFile:%s\n"
            , (int)xaVals->framePhase
            , (double)xaVals->transformPrecisionThreshold
            , (double)xaVals->transformPrecision
            , xaVals->moveLogFile
            );
  }
  
}  /* end gap_detail_xml_align_get_values */



/* ---------------------------
 * gap_detail_xml_align_dialog
 * ---------------------------
 *   return  TRUE.. OK
 *           FALSE.. in case of Error or cancel
 */
gboolean
gap_detail_xml_align_dialog(XmlAlignValues *xaVals)
{
#define SPINBUTTON_ENTRY_WIDTH 70
#define DETAIL_ALIGN_XML_DIALOG_ARGC 5

  static GapArrArg  argv[DETAIL_ALIGN_XML_DIALOG_ARGC];
  gint ii;
  gint ii_framePhase;
  gint ii_precisionThres;
  gint ii_precision;

  ii=0; gap_arr_arg_init(&argv[ii], GAP_ARR_WGT_INT_PAIR); ii_framePhase = ii;
  argv[ii].label_txt = _("Frame Phase:");
  argv[ii].help_txt  = _("Frame number (phase) to be rendered.");
  argv[ii].constraint = FALSE;
  argv[ii].int_min   = 1;
  argv[ii].int_max   = 9999;
  argv[ii].umin      = 1;
  argv[ii].umax      = 999999;
  argv[ii].int_ret   = xaVals->framePhase;
  argv[ii].entry_width = SPINBUTTON_ENTRY_WIDTH;
  argv[ii].has_default = TRUE;
  argv[ii].int_default = DEFAULT_framePhase;


  ii++; gap_arr_arg_init(&argv[ii], GAP_ARR_WGT_FILESEL);
  argv[ii].label_txt = _("XML file:");
  argv[ii].help_txt  = _("Name of the xml file that contains the tracked detail coordinates. "
                        " (recorded with the detail tracking feature).");
  argv[ii].text_buf_len = sizeof(xaVals->moveLogFile);
  argv[ii].text_buf_ret = &xaVals->moveLogFile[0];
  argv[ii].entry_width = 400;


  ii++; ii_precision = ii;
  gap_arr_arg_init(&argv[ii], GAP_ARR_WGT_FLT_PAIR);
  argv[ii].constraint = TRUE;
  argv[ii].label_txt = _("Precision:");
  argv[ii].help_txt  = _("Precision (in pixels) for calculation of perspective transformation matrix. "
                         "Smaller values give more precision (and need more iterations at calculation)");
  argv[ii].flt_min   = 0.0;
  argv[ii].flt_max   = 1.0;
  argv[ii].flt_step  = 0.01;
  argv[ii].pagestep  = 0.1;
  argv[ii].flt_digits = 4;
  argv[ii].flt_ret   = GAP_GEO_TRANSFORM_PRECISION;
  if(xaVals->transformPrecision > 0)
  {
    argv[ii].flt_ret   = xaVals->transformPrecision;
  }
  argv[ii].has_default = TRUE;
  argv[ii].flt_default = GAP_GEO_TRANSFORM_PRECISION;
  
  ii++; ii_precisionThres = ii;
  gap_arr_arg_init(&argv[ii], GAP_ARR_WGT_FLT_PAIR);
  argv[ii].constraint = TRUE;
  argv[ii].label_txt = _("PrecisionThreshold:");
  argv[ii].help_txt  = _("Threshold for fine tuning purpose. "
                         "Iterative calculated coordinates with precision lower than this threshold "
                         "are used for fine tuning probe render attempts. "
                         "increasing the threshold results in more probe attempts "
                         "and makes processing very slow but typically reduces jitter effects. "
                         "Setting the threshold smaller than precision disables finetuning probe rendering. "
                         "Note that finetuning also depends on the presence of a reference layer "
                         "with layername REF in the 1st handled frame");
  argv[ii].flt_min   = 0.0;
  argv[ii].flt_max   = 2.0;
  argv[ii].flt_step  = 0.01;
  argv[ii].pagestep  = 0.1;
  argv[ii].flt_digits = 4;
  argv[ii].flt_ret   = GAP_GEO_TRANSFORM_PRECISION_THRSHOLD;
  if(xaVals->transformPrecisionThreshold > 0)
  {
    argv[ii].flt_ret   = xaVals->transformPrecisionThreshold;
  }
  argv[ii].has_default = TRUE;
  argv[ii].flt_default = GAP_GEO_TRANSFORM_PRECISION_THRSHOLD;
    


  ii++; gap_arr_arg_init(&argv[ii], GAP_ARR_WGT_DEFAULT_BUTTON);
  argv[ii].label_txt = _("Default");
  argv[ii].help_txt  = _("Reset all parameters to default values");

  if(TRUE == gap_arr_ok_cancel_dialog(_("Detail Align via XML"),
                            _("Settings :"),
                            DETAIL_ALIGN_XML_DIALOG_ARGC, argv))
  {
      xaVals->framePhase                  = (gint32)(argv[ii_framePhase].int_ret);
      xaVals->transformPrecisionThreshold = (gdouble)(argv[ii_precisionThres].flt_ret);
      xaVals->transformPrecision          = (gdouble)(argv[ii_precision].flt_ret);
      
      gimp_set_data (GAP_DETAIL_TRACKING_XML_ALIGNER_PLUG_IN_NAME
                    , xaVals, sizeof (XmlAlignValues));
      return TRUE;
  }
  else
  {
      return FALSE;
  }
}               /* end gap_detail_xml_align_dialog */


/* ---------------------------------- */
/* ---------------------------------- */
/* ---------------------------------- */
/* ---------------------------------- */
/* ---------------------------------- */
/* ---------------------------------- */



/* ------------------------------------------
 * p_capture_4_vector_points
 * ------------------------------------------
 * capture the first 4 points of the 1st stroke in the active path vectors
 * pointOrder POINT_ORDER_MODE_31_42 : order 0,1,2,3  compatible with the exactAligner script
 *            POINT_ORDER_MODE_21_43 : order 0,2,1,3  alternative point order.
 *            POINT_ORDER_MODE_1234_1234: alternative with 2 paths, 
 *                                      using active path as Source mark 4 points in the active layer
 *                                      and separate path vectors with name "TARGET"
 *                                        
 */
static gint
p_capture_4_vector_points(gint32 imageId, GapAlignCoords *alignCoords, gint32 pointOrder)
{
  gint pointCount;
  gint         ii;

  for(ii=0; ii < 4; ii++)
  {
      alignCoords->startCoords[ii].px = -1;
      alignCoords->startCoords[ii].py = -1;
      alignCoords->startCoords[ii].valid = FALSE;

      alignCoords->currCoords[ii].px = -1;
      alignCoords->currCoords[ii].py = -1;
      alignCoords->currCoords[ii].valid = FALSE;
  }
  
  pointCount = p_capture_4_vector_points_from_pathname(imageId, alignCoords, pointOrder, NULL);
  
  if (pointOrder == POINT_ORDER_MODE_1234_1234)
  {
    pointCount += p_capture_4_vector_points_from_pathname(imageId, alignCoords, pointOrder, GAP_EXACT_ALIGNER_TARGET_PATH_NAME);
  }
  
  return (pointCount);
  
}  /* end p_capture_4_vector_points */

static gint
p_capture_4_vector_points_from_pathname(gint32 imageId, GapAlignCoords *alignCoords, gint32 pointOrder, char *vectorsName)
{
  gint32  activeVectorsId;
  GapPixelCoords *coordPtr[4];
  gint         countVaildPoints;

  
  
  if (pointOrder == POINT_ORDER_MODE_1234_1234)
  {
    if (vectorsName == NULL)
    {
      /* capture currCoords from the active path */
      coordPtr[0] = &alignCoords->currCoords[0];
      coordPtr[1] = &alignCoords->currCoords[1];
      coordPtr[2] = &alignCoords->currCoords[2];
      coordPtr[3] = &alignCoords->currCoords[3];
    }
    else
    {
      /* capture start coords from path with name: GAP_EXACT_ALIGNER_TARGET_PATH_NAME */
      coordPtr[0] = &alignCoords->startCoords[0];
      coordPtr[1] = &alignCoords->startCoords[1];
      coordPtr[2] = &alignCoords->startCoords[2];
      coordPtr[3] = &alignCoords->startCoords[3];
    }
  }
  else if (pointOrder == POINT_ORDER_MODE_31_42)
  {
    coordPtr[0] = &alignCoords->startCoords[0];
    coordPtr[1] = &alignCoords->startCoords[1];
    coordPtr[2] = &alignCoords->currCoords[0];
    coordPtr[3] = &alignCoords->currCoords[1];
  }
  else
  {
    coordPtr[0] = &alignCoords->startCoords[0];
    coordPtr[2] = &alignCoords->startCoords[1];
    coordPtr[1] = &alignCoords->currCoords[0];
    coordPtr[3] = &alignCoords->currCoords[1];
  }

  
  countVaildPoints = 0;
  
  activeVectorsId = -1;
  if(vectorsName == NULL)
  {
    activeVectorsId = gimp_image_get_active_vectors(imageId);
  }
  else
  {
    if (*vectorsName != '\0')
    {
      activeVectorsId = gimp_image_get_vectors_by_name(imageId, vectorsName);
    }
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
          for(ii=0; ii < MIN(24, num_points); ii++)
          {
            printf ("point[%d] = %.3f\n", ii, points[ii]);
          }
        }
        
        if (type == GIMP_VECTORS_STROKE_TYPE_BEZIER)
        {
          if(num_points >= 6)
          {
            coordPtr[0]->px = points[0];
            coordPtr[0]->py = points[1];
            coordPtr[0]->valid = TRUE;
            countVaildPoints++;
          }
          if(num_points >= 12)
          {
            coordPtr[1]->px = points[6];
            coordPtr[1]->py = points[7];
            coordPtr[1]->valid = TRUE;
            countVaildPoints++;
          }
          if(num_points >= 18)
          {
            coordPtr[2]->px = points[12];
            coordPtr[2]->py = points[13];
            coordPtr[2]->valid = TRUE;
            countVaildPoints++;
          }
          if(num_points >= 24)
          {
            coordPtr[3]->px = points[18];
            coordPtr[3]->py = points[19];
            coordPtr[3]->valid = TRUE;
            countVaildPoints++;
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
 
  return(countVaildPoints);
  
}  /* end p_capture_4_vector_points_from_pathname */


/* ---------------------------------
 * p_save_gimprc_gint32_value
 * ---------------------------------
 */
static void
p_save_gimprc_gint32_value(const char *gimprc_option_name, gint32 value)
{
  gchar  *value_string;

  value_string = g_strdup_printf("%d", value);
  gimp_gimprc_set(gimprc_option_name, value_string);
  g_free(value_string);

}  /* p_save_gimprc_gint32_value */


/* ------------------------------------
 * p_generateFineTuningPerCoords        DEPRECATED
 * ------------------------------------
 * generate modified variants of perspective coords in the internal array values of perCoords.
 * (movement with 1/2 pixel shifts in all directions for fine tuning purpose)
 */
static void
p_generateFineTuningPerCoords(GapPerspectiveTransCoords *perCoords)
{
#define MAX_FINE_SHIFT_ATTEMPTS 9

  gint ida;
  gint idd;
  gdouble dx[MAX_FINE_SHIFT_ATTEMPTS];
  gdouble dy[MAX_FINE_SHIFT_ATTEMPTS];
  

  dx[0] = 0;
  dy[0] = 0;

  dx[1] = -0.25;
  dy[1] =  0;
  dx[2] =  0.25;
  dy[2] =  0;

  dx[3] =  0;
  dy[3] = -0.25;
  dx[4] =  0;
  dy[4] =  0.25;

  dx[5] = -0.25;
  dy[5] = -0.25;
  dx[6] =  0.25;
  dy[6] =  0.25;
  
  dx[7] = -0.25;
  dy[7] =  0.25;
  dx[8] =  0.25;
  dy[8] = -0.25;
  
  
  ida = perCoords->numberOfArrayValues;
  for(idd = 0; idd < MAX_FINE_SHIFT_ATTEMPTS; idd++)
  {
    if(ida < MAX_ATTEMPTS_PERSPECTIVE)
    {
      perCoords->numberOfArrayValues++;
      perCoords->ax0[ida] = perCoords->x0 + dx[idd];    
      perCoords->ay0[ida] = perCoords->y0 + dy[idd];
      perCoords->ax1[ida] = perCoords->x1 + dx[idd]; 
      perCoords->ay1[ida] = perCoords->y1 + dy[idd];
      perCoords->ax2[ida] = perCoords->x2 + dx[idd];
      perCoords->ay2[ida] = perCoords->y2 + dy[idd];
      perCoords->ax3[ida] = perCoords->x3 + dx[idd];
      perCoords->ay3[ida] = perCoords->y3 + dy[idd];
      
      ida++;
    }
    else
    {
      break;
    }
  }


}    /* end p_generateFineTuningPerCoords */

/* ------------------------------
 * p_buildTuningShortList
 * ------------------------------
 * delivers a list that has at least one element (with offsets 0) and up to 4 elements
 * depending on distance between tuned ans untuned coordinate.
 * (the distance is typical <= 4 pixels offset)
 * the returned short list includes coords on the line between tuned and untuned coordiante.
 * it has just 1 element in case tuned and untuned coordinate are equal.
 *
 * example:
 *  tunedCoord:   p1x="299"  p1y="113"
 *  untunedCoord: u1x="297"  u1y="115"
 * delivers a short list with 3 elements:
 *  1.)  tuneOffsetX = 0,  tuneOffsetY = 0    ...  299 + (0)  = 299 (p1x)  ; 113 + (0)  = 113 (p1y)
 *  2.)  tuneOffsetX = -1, tuneOffsetY = 1    ...  299 + (-1) = 298 (p1x)  ; 113 + (1)  = 114 (p1y)
 *  3.)  tuneOffsetX = -2, tuneOffsetY = 2    ...  299 + (-2) = 297 (p1x)  ; 113 + (2)  = 115 (p1y)
 */
GapLocateTuneOffsElem * 
p_buildTuningShortList(GapPixelCoords *tunedCoord, GapPixelCoords *untunedCoord)
{
  GapLocateTuneOffsElem *rootShortList;
  GapLocateTuneOffsElem *tailShortList;
  GapLocateTuneOffsElem *elem;
  
  gint tuneOffsetX;
  gint tuneOffsetY;
  gint maxAbsOffs;
  gdouble relDiff;

  /* 1.st entry  tuneOffsetX = 0, tuneOffsetY = 0 */
  tuneOffsetX = 0;
  tuneOffsetY = 0;
  relDiff = 0.0;
  rootShortList = gap_locate_newGapLocateTuneOffsElem(tuneOffsetX, tuneOffsetX, relDiff);
  tailShortList = rootShortList;

  tuneOffsetX = untunedCoord->px - tunedCoord->px;
  tuneOffsetY = untunedCoord->py - tunedCoord->py;
  maxAbsOffs = MAX(abs(tuneOffsetX), abs(tuneOffsetY));

  if (maxAbsOffs > 3)
  {
    elem = gap_locate_newGapLocateTuneOffsElem((tuneOffsetX / 4), (tuneOffsetY / 4), relDiff);
    tailShortList->next = elem;
    tailShortList = elem;
  }

  if (maxAbsOffs > 1)
  {
    elem = gap_locate_newGapLocateTuneOffsElem((tuneOffsetX / 2), (tuneOffsetY / 2), relDiff);
    tailShortList->next = elem;
    tailShortList = elem;
  }
  if (maxAbsOffs == 3)
  {
    elem = gap_locate_newGapLocateTuneOffsElem((tuneOffsetX / 3), (tuneOffsetY / 3), relDiff);
    tailShortList->next = elem;
    tailShortList = elem;
  }


  if (maxAbsOffs > 3)
  {
    elem = gap_locate_newGapLocateTuneOffsElem(((tuneOffsetX / 2) + (tuneOffsetX / 4)), ((tuneOffsetY / 2) + (tuneOffsetY / 4)), relDiff);
    tailShortList->next = elem;
    tailShortList = elem;
  }
  
  if (maxAbsOffs > 0)
  {
    elem = gap_locate_newGapLocateTuneOffsElem(tuneOffsetX, tuneOffsetY, relDiff);
    tailShortList->next = elem;
    tailShortList = elem;
  }
  
  
  return (rootShortList);
  
}  /* end p_buildTuningShortList */


/* ------------------------------
 * p_mergeTuningShortList
 * ------------------------------
 * returns a list that contains all elements of both lists A and B
 * where list B is appendend at enf of List A and duplicate entries
 * are set invalid in the appended list part (that was list B)
 */
GapLocateTuneOffsElem * 
p_mergeTuningShortList(GapLocateTuneOffsElem *rootShortListA, GapLocateTuneOffsElem *rootShortListB)
{
  GapLocateTuneOffsElem *tailShortListA;
  GapLocateTuneOffsElem *elemA;
  GapLocateTuneOffsElem *elemB;
  
  if (rootShortListA == NULL)
  {
    return (rootShortListB);
  }
  if (rootShortListB == NULL)
  {
    return (rootShortListA);
  }
  
  tailShortListA = rootShortListA;
  for(elemA = rootShortListA; elemA != NULL; elemA = elemA->next)
  {
    tailShortListA = elemA;
    if (elemA->valid != TRUE) 
    { 
      continue;  /* skip invalid list entries */
    }
    for(elemB = rootShortListB; elemB != NULL; elemB = elemB->next)
    {
      if (elemB->valid != TRUE) 
      { 
        continue;  /* skip invalid list entries */
      }
      if((elemA->tuneOffsetX == elemB->tuneOffsetX)
      && (elemA->tuneOffsetY == elemB->tuneOffsetY))
      {
         /* set duplicate entries invalid in list B */
         elemB->valid = FALSE;
      }
    }
  }
  
  tailShortListA->next = rootShortListB;
  
  return (rootShortListA);
  
}  /* end p_mergeTuningShortList */

/* ------------------------------
 * p_filterTuningShortList
 * ------------------------------
 * returns the same list, where unwanted elements are filtered (by setting them invalid)
 * lists starting with strong points are reduced to this first element.
 * weak list with more candiates of similar matching quality are truncated after N elements.
 * (note that the short List has to be sorted already by macthing quality)
 */
static void
p_filterTuningShortList(GapLocateTuneOffsElem *rootShortListA)
{
  GapLocateTuneOffsElem *elemA;
  gint     validElemCount;
  gint     remainingValidElemCount;
  gboolean isStrong;
  
  if (rootShortListA == NULL)
  {
    return;
  }
  
  isStrong = gap_locate_check_strong_shortlist(rootShortListA
                                              , 1.02  /* nearlySameFactor */  // TODO find usable value
                                              , 0.1   /* strongRelDiff    */  // TODO find usable value
                                              );
  validElemCount = 0;
  remainingValidElemCount = 0;
  for(elemA = rootShortListA; elemA != NULL; elemA = elemA->next)
  {
    if (elemA->valid != TRUE) 
    { 
      continue;  /* skip invalid list entries */
    }
    validElemCount++;
    if (isStrong)
    {
      if (validElemCount > 1)
      {
         /* all elements (except the 1st one) are set invalid when the 1st one is a strong matching point */
         elemA->valid = FALSE;
      }
      else
       {
         remainingValidElemCount++;
       }
    }
    else
    {
       if (validElemCount > 30)   /// TODO can we limit to less variants (for performance reason) or shal we do even more fro quality reason ? 
       {
         elemA->valid = FALSE;
       }
       else
       {
         remainingValidElemCount++;
       }
    }
  }
  
  if(gap_debug)
  {
    printf("  p_filterTuningShortList ");
    
    if (isStrong) { printf("STRONG"); }
    else          { printf("Weak"); }
    
    printf("  validElemCount:%d remainingValidElemCount:%d\n"
        , (int)validElemCount
        , (int)remainingValidElemCount
        );
  }
  
  
}  /* end p_filterTuningShortList */


/* ------------------------------
 * p_pickTuneCoordinateVariants
 * ------------------------------
 */
static void
p_pickTuneCoordinateVariants(gint32 activeDrawableId, gint32 referenceLayerId, GapAlignCoords *alignCoords, GapPerspectiveTransCoords *perCoords
  , gint32 framePhase, ShortLists *sl)
{
  GapLocateTuneOffsElem *shortListP1;
  GapLocateTuneOffsElem *shortListP2;
  GapLocateTuneOffsElem *shortListP3;
  GapLocateTuneOffsElem *shortListP4;

  GapLocateTuneOffsElem *elemP1;
  GapLocateTuneOffsElem *elemP2;
  GapLocateTuneOffsElem *elemP3;
  GapLocateTuneOffsElem *elemP4;
  gint ida;
  gint countTruncatedAttempts;
  
  gdouble qFactor;
  
    /*  TODO find a practical qFactor in the tests..
     * the qFactor shall eliminate weak matchers (by setting them invalid)
     * in case there is a clear favorite matching offset available,
     * but keep more elements (== tune attempts) in case there are more very similar matching candidates.
     */
  qFactor = 1.4; // TODO...

  shortListP1 = NULL;
  shortListP2 = NULL;
  shortListP3 = NULL;
  shortListP4 = NULL;

//  if ((alignCoords->untunedCoords[0].valid)
//  &&  (alignCoords->untunedCoords[1].valid)
//  &&  (alignCoords->untunedCoords[2].valid)
//  &&  (alignCoords->untunedCoords[3].valid))
  if (FALSE)  // Disabled because results not good enough...
  {
    if(gap_debug)
    {
      printf("p_pickTuneCoordinateVariants based on already TUNED and UNTUNED align coords\n");
    }
    /* build short lists for tuning attempts 
     * based on set of already tuned and untuned coords already provided in the alignCoords.
     * the built short list includes coords on the line between tuned and untuned coordiante.
     * (tyically when tracking was recorded with additional color realtion tuning)
     */
    shortListP1 = p_buildTuningShortList(&alignCoords->currCoords[0], &alignCoords->untunedCoords[0]);
    shortListP2 = p_buildTuningShortList(&alignCoords->currCoords[1], &alignCoords->untunedCoords[1]);
    shortListP3 = p_buildTuningShortList(&alignCoords->currCoords[2], &alignCoords->untunedCoords[2]);
    shortListP4 = p_buildTuningShortList(&alignCoords->currCoords[3], &alignCoords->untunedCoords[3]);
  }
  else
  {
    if(gap_debug)
    {
      printf("p_pickTuneCoordinateVariants by cheking color relations\n");
    }
    /* find potential tuning offsets in near environment +- 4 pixels by cheking color relations
     * and merge the results into (optional already existing) short lists
     */
    //shortListP1 = p_mergeTuningShortList(shortListP1, gap_locate_FindTuneOffsShortList(activeDrawableId, referenceLayerId, &alignCoords->startCoords[0], &alignCoords->currCoords[0], qFactor));
    //shortListP2 = p_mergeTuningShortList(shortListP2, gap_locate_FindTuneOffsShortList(activeDrawableId, referenceLayerId, &alignCoords->startCoords[1], &alignCoords->currCoords[1], qFactor));
    //shortListP3 = p_mergeTuningShortList(shortListP3, gap_locate_FindTuneOffsShortList(activeDrawableId, referenceLayerId, &alignCoords->startCoords[2], &alignCoords->currCoords[2], qFactor));
    //shortListP4 = p_mergeTuningShortList(shortListP4, gap_locate_FindTuneOffsShortList(activeDrawableId, referenceLayerId, &alignCoords->startCoords[3], &alignCoords->currCoords[3], qFactor));

    shortListP1 = gap_locate_FindTuneOffsShortList(activeDrawableId, referenceLayerId, &alignCoords->startCoords[0], &alignCoords->currCoords[0], qFactor);
    p_filterTuningShortList(shortListP1);

    shortListP2 = gap_locate_FindTuneOffsShortList(activeDrawableId, referenceLayerId, &alignCoords->startCoords[1], &alignCoords->currCoords[1], qFactor);
    p_filterTuningShortList(shortListP2);

    shortListP3 = gap_locate_FindTuneOffsShortList(activeDrawableId, referenceLayerId, &alignCoords->startCoords[2], &alignCoords->currCoords[2], qFactor);
    p_filterTuningShortList(shortListP3);

    shortListP4 = gap_locate_FindTuneOffsShortList(activeDrawableId, referenceLayerId, &alignCoords->startCoords[3], &alignCoords->currCoords[3], qFactor);
    p_filterTuningShortList(shortListP4);
  }

  if(gap_debug)
  {
      printf("p_pickTuneCoordinateVariants\n");
  }


  countTruncatedAttempts = 0;
  ida = 0;
  perCoords->numberOfArrayValues = 0;
  for(elemP1 = shortListP1; elemP1 != NULL; elemP1 = elemP1->next)
  {
    if (elemP1->valid != TRUE) 
    { 
      continue;  /* skip low quality list entries */
    }
    for(elemP2 = shortListP2; elemP2 != NULL; elemP2 = elemP2->next)
    {
      if (elemP2->valid != TRUE) 
      {
        continue;  /* skip low quality list entries */
      }
      for(elemP3 = shortListP3; elemP3 != NULL; elemP3 = elemP3->next)
      {
        if (elemP3->valid != TRUE)
        {
          continue;  /* skip low quality list entries */
        }
        for(elemP4 = shortListP4; elemP4 != NULL; elemP4 = elemP4->next)
        {
          GapAlignCoords alignCoordsDup;
          GapPerspectiveTransCoords  perCoordsTunedStruct;
          GapPerspectiveTransCoords *perCoordsTuned;
          gboolean perCoordsOk;


          if (elemP4->valid != TRUE)
          { 
            continue;  /* skip low quality list entries */
          }

          if (ida >= MAX_ATTEMPTS_PERSPECTIVE)
          {
             countTruncatedAttempts++;
             continue;
	  }
          gap_geo_DuplicateAlignCoords(alignCoords, &alignCoordsDup);
          perCoordsTuned->transformPrecisionThreshold =  perCoords->transformPrecisionThreshold;
	  perCoordsTuned->transformPrecision =  perCoords->transformPrecision;

          
          /* apply tune offsets to the align coordinate duplicates */
          alignCoordsDup.currCoords[0].px = alignCoords->currCoords[0].px + elemP1->tuneOffsetX;
          alignCoordsDup.currCoords[0].py = alignCoords->currCoords[0].py + elemP1->tuneOffsetY;
          
          alignCoordsDup.currCoords[1].px = alignCoords->currCoords[1].px + elemP2->tuneOffsetX;
          alignCoordsDup.currCoords[1].py = alignCoords->currCoords[1].py + elemP2->tuneOffsetY;
          
          alignCoordsDup.currCoords[2].px = alignCoords->currCoords[2].px + elemP3->tuneOffsetX;
          alignCoordsDup.currCoords[2].py = alignCoords->currCoords[2].py + elemP3->tuneOffsetY;
          
          alignCoordsDup.currCoords[3].px = alignCoords->currCoords[3].px + elemP4->tuneOffsetX;
          alignCoordsDup.currCoords[3].py = alignCoords->currCoords[3].py + elemP4->tuneOffsetY;

          perCoordsTuned = &perCoordsTunedStruct;
          perCoordsOk = gap_geo_perspective_trans_coords_from_align_coords(activeDrawableId, &alignCoordsDup, perCoordsTuned);
          if (perCoordsOk == TRUE)
          {
             // record coordinates for potential iteration attempts
             perCoords->ax0[ida] = perCoordsTuned->x0; 
	     perCoords->ay0[ida] = perCoordsTuned->y0;
	     perCoords->ax1[ida] = perCoordsTuned->x1;
	     perCoords->ay1[ida] = perCoordsTuned->y1;
	     perCoords->ax2[ida] = perCoordsTuned->x2;
	     perCoords->ay2[ida] = perCoordsTuned->y2;
	     perCoords->ax3[ida] = perCoordsTuned->x3;
	     perCoords->ay3[ida] = perCoordsTuned->y3;
	     
	     if(gap_debug)
	     {
	       gint ii;
	       for(ii=0; ii < 4; ii++)
	       {
	         printf("TuneVariant ida:%d frame:%d alignCoords->currCoords[%d].px:%d .py:%d  tuned.px:%d .py:%d  tuneOffsetX:%d tuneOffsetY:%d\n"
                   ,(int)ida
                   ,(int)framePhase
	           ,(int)ii
	           ,(int)alignCoords->currCoords[ii].px
	           ,(int)alignCoords->currCoords[ii].py
	           ,(int)alignCoordsDup.currCoords[ii].px
	           ,(int)alignCoordsDup.currCoords[ii].py
	           ,(int)(alignCoordsDup.currCoords[ii].px - alignCoords->currCoords[ii].px)
	           ,(int)(alignCoordsDup.currCoords[ii].py - alignCoords->currCoords[ii].py)
	           );
	       }
	       printf("\n");
	       
	     }
	     
	     
	     ida++;
	     perCoords->numberOfArrayValues = ida;
          }
          
        }
      }
    }
  }

  sl->shortListP1 = shortListP1;
  sl->shortListP2 = shortListP2;
  sl->shortListP3 = shortListP3;
  sl->shortListP4 = shortListP4;

  
  if(gap_debug)
  {
    printf("p_pickTuneCoordinateVariants Done, storedAttempts:%d countTruncatedAttempts:%d\n\n"
      ,(int)perCoords->numberOfArrayValues
      ,(int)countTruncatedAttempts
      );
  }

}  /* end p_pickTuneCoordinateVariants */

/* ------------------------------
 * p_renderPickedPathVariant
 * ------------------------------
 */
static void
p_renderPickedPathVariant(gint32 activeDrawableId, gint32 pickedArrayIdx, GapAlignCoords *alignCoords, gint32 framePhase, ShortLists *sl)
{
  GapLocateTuneOffsElem *shortListP1;
  GapLocateTuneOffsElem *shortListP2;
  GapLocateTuneOffsElem *shortListP3;
  GapLocateTuneOffsElem *shortListP4;

  GapLocateTuneOffsElem *elemP1;
  GapLocateTuneOffsElem *elemP2;
  GapLocateTuneOffsElem *elemP3;
  GapLocateTuneOffsElem *elemP4;
  gint ida;


  shortListP1 = sl->shortListP1;
  shortListP2 = sl->shortListP2;
  shortListP3 = sl->shortListP3;
  shortListP4 = sl->shortListP4;


  ida = 0;
  for(elemP1 = shortListP1; elemP1 != NULL; elemP1 = elemP1->next)
  {
    if (elemP1->valid != TRUE) 
    { 
      continue;  /* skip low quality list entries */
    }
    for(elemP2 = shortListP2; elemP2 != NULL; elemP2 = elemP2->next)
    {
      if (elemP2->valid != TRUE) 
      {
        continue;  /* skip low quality list entries */
      }
      for(elemP3 = shortListP3; elemP3 != NULL; elemP3 = elemP3->next)
      {
        if (elemP3->valid != TRUE)
        {
          continue;  /* skip low quality list entries */
        }
        for(elemP4 = shortListP4; elemP4 != NULL; elemP4 = elemP4->next)
        {
          GapAlignCoords alignCoordsDup;


          if (elemP4->valid != TRUE)
          { 
            continue;  /* skip low quality list entries */
          }

          if (ida >= MAX_ATTEMPTS_PERSPECTIVE)
          {
             /* Truncated Attempts */
             return;
	  }
	  if (pickedArrayIdx == ida)
	  {
            gint32 imageId;

            gap_geo_DuplicateAlignCoords(alignCoords, &alignCoordsDup);
            
            /* apply tune offsets to the align coordinate duplicates */
            alignCoordsDup.currCoords[0].px = alignCoords->currCoords[0].px + elemP1->tuneOffsetX;
            alignCoordsDup.currCoords[0].py = alignCoords->currCoords[0].py + elemP1->tuneOffsetY;
          
            alignCoordsDup.currCoords[1].px = alignCoords->currCoords[1].px + elemP2->tuneOffsetX;
            alignCoordsDup.currCoords[1].py = alignCoords->currCoords[1].py + elemP2->tuneOffsetY;
          
            alignCoordsDup.currCoords[2].px = alignCoords->currCoords[2].px + elemP3->tuneOffsetX;
            alignCoordsDup.currCoords[2].py = alignCoords->currCoords[2].py + elemP3->tuneOffsetY;
          
            alignCoordsDup.currCoords[3].px = alignCoords->currCoords[3].px + elemP4->tuneOffsetX;
            alignCoordsDup.currCoords[3].py = alignCoords->currCoords[3].py + elemP4->tuneOffsetY;
            
	         
	    imageId = gimp_item_get_image(activeDrawableId);
	    p_create_or_replace_path_vectors(imageId
	                                    , &alignCoordsDup.currCoords[0], 4
	                                    , "PICKED" /* vectorsName */
	                                    , TRUE /* setVisible */ 
                                            );

	    if(gap_debug)
	    {
	      gint ii;
	      for(ii=0; ii < 4; ii++)
	      {
	        printf("Render PATH TuneVariant ida:%d frame:%d alignCoords->currCoords[%d].px:%d .py:%d  tuned.px:%d .py:%d  tuneOffsetX:%d tuneOffsetY:%d\n"
                  ,(int)ida
                  ,(int)framePhase
	          ,(int)ii
	          ,(int)alignCoords->currCoords[ii].px
	          ,(int)alignCoords->currCoords[ii].py
	          ,(int)alignCoordsDup.currCoords[ii].px
	          ,(int)alignCoordsDup.currCoords[ii].py
	          ,(int)(alignCoordsDup.currCoords[ii].px - alignCoords->currCoords[ii].px)
	          ,(int)(alignCoordsDup.currCoords[ii].py - alignCoords->currCoords[ii].py)
	          );
	      }
     	      printf("\n");
            }
            
            return;
          }
    
          ida++;
          
        }
      }
    }
  }

}  /* end p_renderPickedPathVariant */



/* ---------------------------
 * p_freeShortLists
 * ---------------------------
 */
static void
p_freeShortLists(ShortLists *sl)
{
  if(sl->shortListP1 != NULL)
  {
   gap_locate_freeTuneOffsList(sl->shortListP1);
   sl->shortListP1 = NULL;
  }
  if(sl->shortListP2 != NULL)
  {
   gap_locate_freeTuneOffsList(sl->shortListP2);
   sl->shortListP2 = NULL;
  }
  if(sl->shortListP3 != NULL)
  {
   gap_locate_freeTuneOffsList(sl->shortListP3);
   sl->shortListP3 = NULL;
  }
  if(sl->shortListP4 != NULL)
  {
   gap_locate_freeTuneOffsList(sl->shortListP4);
   sl->shortListP4 = NULL;
  }

}  /* end p_freeShortLists */


/* --------------------------------------------
 * p_fineTuneProbePerspectiveTransformation
 * --------------------------------------------
 * This procedure creates a temporary image with a copy of a referenceLayer and performs probe rendering
 * on copies of the activeDrawable to select the best matching transformation.
 * Therefore areas of the background (marked as opaque via layermask) 
 * are compared referenceLayer versus probe rendered transformed layer 
 * with a set of similar variants of persepective transformation coorinates.
 *
 *
 * This processing and resource intensive fine tuning shall reduce unwanted jitter effects
 * with small amplitudes near +- 1 pixel.
 *
 * Restrictions: 
 *   activeDrawableId must be a layer at full image size
 *   activeDrawableId and referenceLayerId must have same size and bpp
 */
static void
p_fineTuneProbePerspectiveTransformation(gint32 activeDrawableId, gint32 referenceLayerId
  ,GapAlignCoords *alignCoords, GapPerspectiveTransCoords *perCoords, gint32 framePhase)
{
  gdouble bestAvgColordiff;
  gint    pickedArrayIdx;
  gint    ida;
  gint    idaMax;
  gint    l_src_offset_x, l_src_offset_y;    /* layeroffsets as they were in src_image */
  gint32  tmpImageId;
  gint32  requiredPixelCount;
  gint32  comparedPixelCount;
  gint32  tmpBgLayerId;
  ShortLists shortLists;
  ShortLists *sl;
  
  gdouble width;
  gdouble height;

  width  = (gdouble)gimp_image_width(gimp_item_get_image(activeDrawableId));
  height = (gdouble)gimp_image_height(gimp_item_get_image(activeDrawableId));

  if(TRUE != gap_geo_pick_corners_from_align_coords(alignCoords, width, height))
  {
    printf("p_fineTuneProbePerspectiveTransformation FAILED because 4 vaild coords could not be found\n");
    return;  /* stop in case picking coords near the corners failed */
  }

  /* calculate tuning variants based on color relation checks and store the
   * resulting perspective transformation varinats in the perCoords array 
   */
  sl = &shortLists;
  sl->shortListP1 = NULL;
  sl->shortListP2 = NULL;
  sl->shortListP3 = NULL;
  sl->shortListP4 = NULL;
  
  p_pickTuneCoordinateVariants(activeDrawableId, referenceLayerId, alignCoords, perCoords, framePhase, sl);


  if(gap_debug)
  {
    guchar *fname;
    
    fname = gimp_image_get_filename(gimp_item_get_image(activeDrawableId));
    printf("p_fineTuneProbePerspectiveTransformation START numberOfArrayValues: %d  activeDrawableId:%d  referenceLayerId:%d width:%f height:%f imagename:%s\n"
      , (int)perCoords->numberOfArrayValues
      , (int)activeDrawableId
      , (int)referenceLayerId
      , width
      , height
      , fname
      );
    if(fname != NULL)
    {
      g_free(fname);
    }
  }

  if(activeDrawableId == referenceLayerId)
  {
    p_freeShortLists(sl);
    /* do nothing in case active and reference are the same layer
     */
    return;
  }

  bestAvgColordiff = 1.1; /* init with worst possible value + 0.1 */
  pickedArrayIdx = -1;    /* -1 indicates nothing picked */
  
  /* create tmpImageId */
  tmpImageId = gap_image_new_of_samesize(gimp_item_get_image(activeDrawableId));
  
  /* copy referenceLayerId as tmpBgLayerId Layer to tmpImageId 
   * TODO: check if layermask is included in the copy ...
   */
  tmpBgLayerId = gap_layer_copy_to_dest_image(tmpImageId
                                             , referenceLayerId   /* l_src_layer_id */
                                             , 100.0   /* Opacity */
                                             ,0        /* NORMAL */
                                             ,&l_src_offset_x
                                             ,&l_src_offset_y
                                             );
  gimp_image_insert_layer(tmpImageId
                         , tmpBgLayerId
                         , 0
                         , 0              /* top of layerstack */
                         );
  gimp_layer_resize_to_image_size(tmpBgLayerId);

    
  
  /* apply layermask on tmpBgLayerId (following opacity checks done just on the alphe channel) */
  gimp_layer_remove_mask (tmpBgLayerId, GIMP_MASK_APPLY);


  /* compare refDrawable against itself
   * to count the number of opaque pixels in it
   */
  gap_locateColordiffOpaquePixels(tmpBgLayerId      /* refDrawable */
                                 , tmpBgLayerId     /*  targetDrawableId */
                                 , 1                /* gint32  requiredPixelCount */
                                 , &comparedPixelCount    /* gint32 *comparedPixelCount */
                                 );
  /* 50 percent of the opaque pixels should be involved in further compare steps 
   * (but do not require more than 500 pixels)
   */
  requiredPixelCount = MIN(500, (comparedPixelCount * 50) / 100);
  if(gap_debug)
  {
     printf("FINE TUNE tmpBgLayerId:%d  number of opaque pixels found:%d requiredPixelCount:%d (50 percent or 500)\n"
           ,(int)tmpBgLayerId
           ,(int)comparedPixelCount
           ,(int)requiredPixelCount
           );
  }

  if (comparedPixelCount < 10)
  {
    /* delete the temporary image and skip fine tune attempts (does not make sense
     * when having not enough opaque refernce pixels
     */
    gap_image_delete_immediate(tmpImageId);
    p_freeShortLists(sl);
    return;
  }

  if(gap_debug)
  {
    /* import the xml align coordinates as SRC and TARGET path vectors
     * (this is not relevant for processing, but is useful for analyse purpose)
     */
    p_create_or_replace_path_vectors(tmpImageId
                                       , &alignCoords->startCoords[0], 4
                                       , "Ref" /* vectorsName */
                                       , TRUE /* setVisible */ 
                                       );
    p_create_or_replace_path_vectors(tmpImageId
                                       , &alignCoords->currCoords[0], 4
                                       , "Trk_orig" /* vectorsName */
                                       , FALSE /* setVisible */ 
                                       );
    
    /* when debugging add_display and keep the tmpImageId for analyse purpose */
    //gimp_display_new(tmpImageId);
  }


  /* probe perspective transformation variants with potential settings
   * and pick the one that delivers best match
   * (with the lowest colordiff compared to the reference layer at its opaque areas)
   */
  idaMax = perCoords->numberOfArrayValues; // MIN(40, MAX_ITER_ATTEMPTS_PERSPECTIVE);
  for(ida = 0; ida < idaMax; ida++)
  {
     gint32 attemptLayerId;
     gint32 transformedDrawableId;
     gint   countDisplacementZero;
     gint   countReloacateErrors;
     gint   idl;
     gdouble avgColordiff;
     GapPixelCoords reTracedCoordsArray[4];
          
     /* copy activeDrawableId to tmpImageId */
     attemptLayerId = gap_layer_copy_to_dest_image(tmpImageId
                                             , activeDrawableId   /* l_src_layer_id */
                                             , 100.0   /* Opacity */
                                             ,0        /* NORMAL */
                                             ,&l_src_offset_x
                                             ,&l_src_offset_y
                                             );
     gimp_image_insert_layer(tmpImageId
                             , attemptLayerId
                             , 0
                             , 0              /* top of layerstack */
                             );

     gimp_layer_resize_to_image_size(attemptLayerId);
     
     gimp_context_set_defaults();
     gimp_context_set_transform_resize(GIMP_TRANSFORM_RESIZE_ADJUST);   /* do NOT clip */                                 
     gimp_context_set_transform_direction(GIMP_TRANSFORM_FORWARD);                                 
     transformedDrawableId = gimp_item_transform_perspective(attemptLayerId
                                      , perCoords->ax0[ida], perCoords->ay0[ida]
                                      , perCoords->ax1[ida], perCoords->ay1[ida]
                                      , perCoords->ax2[ida], perCoords->ay2[ida]
                                      , perCoords->ax3[ida], perCoords->ay3[ida]
                                      );

    if(! gimp_drawable_has_alpha(transformedDrawableId))
    {
       /* have to add alpha channel */
       gimp_layer_add_alpha(transformedDrawableId);
    }
     if(gap_debug)
     {
       printf("IDA:%d transformedDrawableId:%d attemptLayerId:%d\n"
         , (int)ida
         , (int)transformedDrawableId
         , (int)attemptLayerId
         );
     }
     
     gimp_layer_resize_to_image_size(transformedDrawableId);
     
     /* compare all opaque pixels and calculate 
      *  average colordifference tmpBgLayerId versus transformedDrawableId
      */
     avgColordiff = gap_locateColordiffOpaquePixels(tmpBgLayerId            /* refDrawable */
                                                   , transformedDrawableId  /*  targetDrawableId */
                                                   , requiredPixelCount     /* gint32  requiredPixelCount */
                                                   , &comparedPixelCount    /* gint32 *comparedPixelCount */
                                                   );
     if(avgColordiff < bestAvgColordiff)
     {
       /* pick the best matching transformation attempt
        * for the same transformation to the activeDrawableId.
        * best matching has smallest color diff compared with referenceLayerId 
        */
       pickedArrayIdx = ida;
       bestAvgColordiff = avgColordiff;
     }
     

     if(gap_debug)
     {
         printf("PROBE ida:%d frame:%d drawableId:%d  comparedPixelCount:%d avgColordiff:%0.12f bestAvgColordiff[%d]:%0.12f\n"
           ,(int)ida
           ,(int)framePhase
           ,(int)transformedDrawableId
           ,(int)comparedPixelCount
           ,(double)avgColordiff
           ,(int)pickedArrayIdx
           ,(double)bestAvgColordiff
           );
     }
     
     if (TRUE)  // DISABLED the re-loaction checks ...
     {
       continue;
     }
     
     
     /* (re)locate 4 target points on base of the alredy transformed layer
      * ideally all 4 target coordinates should be equal to the 4 corresponding reference coordinates.
      *
      */
     countDisplacementZero = 0;
     countReloacateErrors = 0;
     for(idl=0; idl < 4; idl++)
     {
       gint32  refX;
       gint32  refY;
       gint32  targetX;
       gint32  targetY;
       gdouble colordiffLocate;
       gint32  displacementX;
       gint32  displacementY;
       GapPixelCoords *targetCoords;
  
       refX = alignCoords->refPtr[idl]->px;
       refY = alignCoords->refPtr[idl]->py;
       colordiffLocate = gap_locateAreaWithinRadiusWithOffset (tmpBgLayerId  /* reference Layer */
                                    , refX
                                    , refY
                                    , 15   // valPtr->refShapeRadius
                                    , transformedDrawableId                  /* target Layer */
                                    , 8    //  valPtr->targetMoveRadius
                                    , &targetX
                                    , &targetY
                                    , 0  // locateOffsetX  // use locateOffset 0 to start locating exact at ref coordinate
                                    , 0  // locateOffsetY
                                    );
         
       displacementX = refX - targetX;
       displacementY = refY - targetY;
       
       targetCoords = &reTracedCoordsArray[idl];
       targetCoords->px  = (gdouble)targetX;
       targetCoords->py  = (gdouble)targetY;
         

       if(gap_debug)
       {
         printf("  idl:%d displacementX:%d  displacementY:%d colordiffLocate:%0.12f refX:%d refY:%d  targetX:%d targetY:%d\n"
           ,(int)idl
           ,(int)displacementX
           ,(int)displacementY
           ,(double)colordiffLocate
           ,(int)refX
           ,(int)refY
           ,(int)targetX
           ,(int)targetY
           );
       }


     }
     
     /* PAUSE dialog is just for testing (when gap_debug is set TRUE) for test purpose */
     if(gap_debug)
     {
        gchar *msg_txt;
        gboolean l_rc;

        /* import the xml re-traced coordinates as path vectors
         * (this is not relevant for processing, but is useful for analyse purpose)
         */
        p_create_or_replace_path_vectors(tmpImageId
                                       , &reTracedCoordsArray[0], 4
                                       , GAP_EXACT_ALIGNER_RE_TRACED_PATH_NAME /* vectorsName */
                                       , TRUE /* setVisible */ 
                                       );
                                       
        /* display dialog with "OK" Button to pause processing
         * (the tester can analyse the tempory work image while paused.)
         */
        msg_txt = g_strdup_printf(_("Fine Tuning step %d done.\n"
                            ""
                            "press OK for next iteration step\n")
                            ,(int)ida
                         );
        l_rc = gap_arr_confirm_dialog(msg_txt
                                 , _("Detail Align FineTuning PAUSED")   /* title_txt */
                                 , _("Confirm to continue")   /* frame_txt */
                                 );
        g_free(msg_txt);
        if (l_rc != TRUE)
        {
           printf("ESCAPE fineTune iteration at attempt IDA:%d pickedArrayIdx:%d because CANCELLED by user.\n"
                ,(int)ida
                ,(int)pickedArrayIdx
                );
           break;
        }
     
     }
     
     if (countDisplacementZero >= 4)
     {
       if(gap_debug)
       {
         printf("ESCAPE fineTune iteration at attempt IDA:%d pickedArrayIdx:%d because transformed layer exactly matches at 4 ref points\n"
                ,(int)ida
                ,(int)pickedArrayIdx
                );
       }
       break;
     }

     if (countReloacateErrors >= 4)
     {
       if(gap_debug)
       {
         printf("ESCAPE fineTune iteration at attempt IDA:%d pickedArrayIdx:%d because locating of tracking coordinates FAILED\n"
                ,(int)ida
                ,(int)pickedArrayIdx
                );
       }
       break;
     }     
     
     
  }
  
  /////////////////////////////////////////////////
  

 
  if (pickedArrayIdx >= 0)
  {
     p_renderPickedPathVariant(activeDrawableId, pickedArrayIdx, alignCoords, framePhase, sl);

     perCoords->x0 = perCoords->ax0[pickedArrayIdx];    
     perCoords->y0 = perCoords->ay0[pickedArrayIdx];
     perCoords->x1 = perCoords->ax1[pickedArrayIdx]; 
     perCoords->y1 = perCoords->ay1[pickedArrayIdx];
     perCoords->x2 = perCoords->ax2[pickedArrayIdx];
     perCoords->y2 = perCoords->ay2[pickedArrayIdx];
     perCoords->x3 = perCoords->ax3[pickedArrayIdx];
     perCoords->y3 = perCoords->ay3[pickedArrayIdx];
     
     if(gap_debug)
     {
       printf("\nfineTune RESULT pickedArrayIdx:%d\n\n"
             ,(int)pickedArrayIdx
             );
     }
  }

  
  if(gap_debug != TRUE)
  {
    /* remove the temporary image (that was created just for probe purpose) */
    gap_image_delete_immediate(tmpImageId);
  }
  
  p_freeShortLists(sl);


}  /* end p_fineTuneProbePerspectiveTransformation */


/* --------------------------------------------
 * p_fineTuneProbePerspectiveTransformationOld2   DEPRECATED 
 * --------------------------------------------
 * Old implementation. TODO remove this after successful test of the new variante !
 * 222222222222222222222222222222222222222222222222222222222222222222222222222222222222222
 * This procedure creates a temporary image with a copy of a referenceLayer and performs probe rendering
 * on copies of the activeDrawable to select the best matching transformation.
 * Therefore areas of the background (marked as opaque via layermask) 
 * are compared referenceLayer versus probe rendered transformed layer 
 * with a set of similar variants of persepective transformation coorinates.
 *
 *
 * This processing and resource intensive fine tuning shall reduce unwanted jitter effects
 * with small amplitudes near +- 1 pixel.
 *
 * Restrictions: 
 *   activeDrawableId must be a layer at full image size
 *   activeDrawableId and referenceLayerId must have same size and bpp
 */
static void
p_fineTuneProbePerspectiveTransformationOld2(gint32 activeDrawableId, gint32 referenceLayerId
  ,GapAlignCoords *alignCoords, GapPerspectiveTransCoords *perCoords)
{
  gdouble bestAvgColordiff;
  gint    pickedArrayIdx;
  gint    ida;
  gint    idaMax;
  gint    l_src_offset_x, l_src_offset_y;    /* layeroffsets as they were in src_image */
  gint32  tmpImageId;
  gint32  requiredPixelCount;
  gint32  comparedPixelCount;
  gint32  tmpBgLayerId;
  
  gdouble width;
  gdouble height;
  gdouble dx[4];
  gdouble dy[4];
  gdouble ddx;    /* horizontal itaration stepsize  1920 / 32000 = 0,06 pixels */
  gdouble ddy;    /* vertical itaration stepsize  1080 / 32000 = 0,03375 pixels */

  width  = (gdouble)gimp_image_width(gimp_item_get_image(activeDrawableId));
  height = (gdouble)gimp_image_height(gimp_item_get_image(activeDrawableId));

  ddx = width / 32000.0;
  ddy = height / 32000.0;
  
  if(gap_debug)
  {
    printf("p_fineTuneProbePerspectiveTransformationOld2 START numberOfArrayValues: %d  activeDrawableId:%d  referenceLayerId:%d width:%f height:%f ddx:%f ddy:%f\n"
      , (int)perCoords->numberOfArrayValues
      , (int)activeDrawableId
      , (int)referenceLayerId
      , width
      , height
      , ddx
      , ddy
      );
  }

  if(TRUE != gap_geo_pick_corners_from_align_coords(alignCoords, width, height))
  {
    printf("p_fineTuneProbePerspectiveTransformationOld2 FAILED because 4 vaild coords could not be found\n");
    return;  /* stop in case picking coords near the corners failed */
  }

  /* set all initial displacements to 0.0 (the first iteration starts with unmodified perspective settings) */
  dx[0] = 0.0; 
  dy[0] = 0.0;
  dx[1] = 0.0;
  dy[1] = 0.0;
  dx[2] = 0.0;
  dy[2] = 0.0;
  dx[3] = 0.0;
  dy[3] = 0.0;

  if(activeDrawableId == referenceLayerId)
  {
    /* do nothing in case active and reference are the same layer
     */
    return;
  }

  bestAvgColordiff = 1.1; /* init with worst possible value + 0.1 */
  pickedArrayIdx = -1;    /* -1 indicates nothing picked */
  
  /* create tmpImageId */
  tmpImageId = gap_image_new_of_samesize(gimp_item_get_image(activeDrawableId));
  
  /* copy referenceLayerId as tmpBgLayerId Layer to tmpImageId 
   * TODO: check if layermask is included in the copy ...
   */
  tmpBgLayerId = gap_layer_copy_to_dest_image(tmpImageId
                                             , referenceLayerId   /* l_src_layer_id */
                                             , 100.0   /* Opacity */
                                             ,0        /* NORMAL */
                                             ,&l_src_offset_x
                                             ,&l_src_offset_y
                                             );
  gimp_image_insert_layer(tmpImageId
                         , tmpBgLayerId
                         , 0
                         , 0              /* top of layerstack */
                         );
  gimp_layer_resize_to_image_size(tmpBgLayerId);

    
  
  /* apply layermask on tmpBgLayerId (following opacity checks done just on the alphe channel) */
  gimp_layer_remove_mask (tmpBgLayerId, GIMP_MASK_APPLY);


  /* compare refDrawable against itself
   * to count the number of opaque pixels in it
   */
  gap_locateColordiffOpaquePixels(tmpBgLayerId      /* refDrawable */
                                 , tmpBgLayerId     /*  targetDrawableId */
                                 , 1                /* gint32  requiredPixelCount */
                                 , &comparedPixelCount    /* gint32 *comparedPixelCount */
                                 );
  /* 50 percent of the opaque pixels should be involved in further compare steps 
   * (but do not require more than 500 pixels)
   */
  requiredPixelCount = MIN(500, (comparedPixelCount * 50) / 100);
  if(gap_debug)
  {
     printf("FINE TUNE tmpBgLayerId:%d  number of opaque pixels found:%d requiredPixelCount:%d (50 percent or 500)\n"
           ,(int)tmpBgLayerId
           ,(int)comparedPixelCount
           ,(int)requiredPixelCount
           );
  }

  if (comparedPixelCount < 10)
  {
    /* delete the temporary image and skip fine tune attempts (does not make sense
     * when having not enough opaque refernce pixels
     */
    gap_image_delete_immediate(tmpImageId);
    return;
  }

  if(gap_debug)
  {
    /* import the xml align coordinates as SRC and TARGET path vectors
     * (this is not relevant for processing, but is useful for analyse purpose)
     */
    p_create_or_replace_path_vectors(tmpImageId
                                       , &alignCoords->startCoords[0], 4
                                       , "Ref" /* vectorsName */
                                       , TRUE /* setVisible */ 
                                       );
    p_create_or_replace_path_vectors(tmpImageId
                                       , &alignCoords->currCoords[0], 4
                                       , "Trk_orig" /* vectorsName */
                                       , FALSE /* setVisible */ 
                                       );
    
    /* when debugging add_display and keep the tmpImageId for analyse purpose */
    //gimp_display_new(tmpImageId);
  }


  /* probe perspective transformation variants with potential settings
   * and pick the one that delivers best match
   * (with the lowest colordiff compared to the reference layer at its opaque areas)
   */
  idaMax = MIN(40, MAX_ITER_ATTEMPTS_PERSPECTIVE);
  for(ida = 0; ida < idaMax; ida++)
  {
     gint32 attemptLayerId;
     gint32 transformedDrawableId;
     gint   countDisplacementZero;
     gint   countReloacateErrors;
     gint   idl;
     gdouble avgColordiff;
     GapPixelCoords reTracedCoordsArray[4];
          

     
     perCoords->ax0[ida] = perCoords->x0 + dx[0]; 
     perCoords->ay0[ida] = perCoords->y0 + dy[0];
     perCoords->ax1[ida] = perCoords->x1 + dx[1];
     perCoords->ay1[ida] = perCoords->y1 + dy[1];
     perCoords->ax2[ida] = perCoords->x2 + dx[2];
     perCoords->ay2[ida] = perCoords->y2 + dy[2];
     perCoords->ax3[ida] = perCoords->x3 + dx[3];
     perCoords->ay3[ida] = perCoords->y3 + dy[3];
    
     /* copy activeDrawableId to tmpImageId */
     attemptLayerId = gap_layer_copy_to_dest_image(tmpImageId
                                             , activeDrawableId   /* l_src_layer_id */
                                             , 100.0   /* Opacity */
                                             ,0        /* NORMAL */
                                             ,&l_src_offset_x
                                             ,&l_src_offset_y
                                             );
     gimp_image_insert_layer(tmpImageId
                             , attemptLayerId
                             , 0
                             , 0              /* top of layerstack */
                             );

     gimp_layer_resize_to_image_size(attemptLayerId);
     
     gimp_context_set_defaults();
     gimp_context_set_transform_resize(GIMP_TRANSFORM_RESIZE_ADJUST);   /* do NOT clip */                                 
     gimp_context_set_transform_direction(GIMP_TRANSFORM_FORWARD);                                 
     transformedDrawableId = gimp_item_transform_perspective(attemptLayerId
                                      , perCoords->ax0[ida], perCoords->ay0[ida]
                                      , perCoords->ax1[ida], perCoords->ay1[ida]
                                      , perCoords->ax2[ida], perCoords->ay2[ida]
                                      , perCoords->ax3[ida], perCoords->ay3[ida]
                                      );
     if(gap_debug)
     {
       printf("IDA:%d transformedDrawableId:%d attemptLayerId:%d\n"
         , (int)ida
         , (int)transformedDrawableId
         , (int)attemptLayerId
         );
     }
     
     gimp_layer_resize_to_image_size(transformedDrawableId);
     
     /* compare all opaque pixels and calculate 
      *  average colordifference tmpBgLayerId versus transformedDrawableId
      */
     avgColordiff = gap_locateColordiffOpaquePixels(tmpBgLayerId            /* refDrawable */
                                                   , transformedDrawableId  /*  targetDrawableId */
                                                   , requiredPixelCount     /* gint32  requiredPixelCount */
                                                   , &comparedPixelCount    /* gint32 *comparedPixelCount */
                                                   );
     if(avgColordiff < bestAvgColordiff)
     {
       /* pick the best matching transformation attempt
        * for the same transformation to the activeDrawableId.
        * best matching has smallest color diff compared with referenceLayerId 
        */
       pickedArrayIdx = ida;
       bestAvgColordiff = avgColordiff;
     }
     

     if(gap_debug)
     {
         printf("PROBE ida:%d drawableId:%d  comparedPixelCount:%d avgColordiff:%0.12f bestAvgColordiff[%d]:%0.12f\n"
           ,(int)ida
           ,(int)transformedDrawableId
           ,(int)comparedPixelCount
           ,(double)avgColordiff
           ,(int)pickedArrayIdx
           ,(double)bestAvgColordiff
           );
     }
     
     /* (re)locate 4 target points on base of the alredy transformed layer
      * ideally all 4 target coordinates should be equal to the 4 corresponding reference coordinates.
      * in this ideal case no further iteration is needed.
      * otherwise calculate dx[i] dy[i] offests based on the detected remaining displacement.
      */
     countDisplacementZero = 0;
     countReloacateErrors = 0;
     for(idl=0; idl < 4; idl++)
     {
       gint32  refX;
       gint32  refY;
       gint32  targetX;
       gint32  targetY;
       gdouble colordiffLocate;
       gint32  displacementX;
       gint32  displacementY;
       GapPixelCoords *targetCoords;
  
       refX = alignCoords->refPtr[idl]->px;
       refY = alignCoords->refPtr[idl]->py;
       colordiffLocate = gap_locateAreaWithinRadiusWithOffset (tmpBgLayerId  /* reference Layer */
                                    , refX
                                    , refY
                                    , 15   // valPtr->refShapeRadius
                                    , transformedDrawableId                  /* target Layer */
                                    , 8    //  valPtr->targetMoveRadius
                                    , &targetX
                                    , &targetY
                                    , 0  // locateOffsetX  // use locateOffset 0 to start locating exact at ref coordinate
                                    , 0  // locateOffsetY
                                    );
         
       displacementX = refX - targetX;
       displacementY = refY - targetY;
       
       targetCoords = &reTracedCoordsArray[idl];
       targetCoords->px  = (gdouble)targetX;
       targetCoords->py  = (gdouble)targetY;
         

       if(gap_debug)
       {
         printf("  idl:%d displacementX:%d  displacementY:%d colordiffLocate:%0.12f refX:%d refY:%d  targetX:%d targetY:%d\n"
           ,(int)idl
           ,(int)displacementX
           ,(int)displacementY
           ,(double)colordiffLocate
           ,(int)refX
           ,(int)refY
           ,(int)targetX
           ,(int)targetY
           );
       }

       if (colordiffLocate < 0.5)
       {
         if ((displacementX == 0) && (displacementY == 0))
         {
           countDisplacementZero++;
         }
         
         if (displacementX > 0)       { dx[idl] += ddx; }
         else if (displacementX < 0)  { dx[idl] -= ddx; }
       
         if (displacementY > 0)       { dy[idl] += ddy; }
         else if (displacementY < 0)  { dy[idl] -= ddy; }
       
       }
       else
       {
         countReloacateErrors++;
         printf("WARNING could not re-locate this point refX:%d refY:%d  colordiffLocate:%0.12f >= 0.5 \n"
           , (int) refX
           , (int) refY
           , (colordiffLocate)
           );
       }
     }
     
     /* PAUSE dialog is just for testing (when gap_debug is set TRUE) for test purpose */
     if(gap_debug)
     {
        gchar *msg_txt;
        gboolean l_rc;

        /* import the xml re-traced coordinates as path vectors
         * (this is not relevant for processing, but is useful for analyse purpose)
         */
        p_create_or_replace_path_vectors(tmpImageId
                                       , &reTracedCoordsArray[0], 4
                                       , GAP_EXACT_ALIGNER_RE_TRACED_PATH_NAME /* vectorsName */
                                       , TRUE /* setVisible */ 
                                       );
                                       
        /* display dialog with "OK" Button to pause processing
         * (the tester can analyse the tempory work image while paused.)
         */
        msg_txt = g_strdup_printf(_("Fine Tuning step %d done.\n"
                            ""
                            "press OK for next iteration step\n")
                            ,(int)ida
                         );
        l_rc = gap_arr_confirm_dialog(msg_txt
                                 , _("Detail Align FineTuning PAUSED")   /* title_txt */
                                 , _("Confirm to continue")   /* frame_txt */
                                 );
        g_free(msg_txt);
        if (l_rc != TRUE)
        {
           printf("ESCAPE fineTune iteration at attempt IDA:%d pickedArrayIdx:%d because CANCELLED by user.\n"
                ,(int)ida
                ,(int)pickedArrayIdx
                );
           break;
        }
     
     }
     
     if (countDisplacementZero >= 4)
     {
       if(gap_debug)
       {
         printf("ESCAPE fineTune iteration at attempt IDA:%d pickedArrayIdx:%d because transformed layer exactly matches at 4 ref points\n"
                ,(int)ida
                ,(int)pickedArrayIdx
                );
       }
       break;
     }

     if (countReloacateErrors >= 4)
     {
       if(gap_debug)
       {
         printf("ESCAPE fineTune iteration at attempt IDA:%d pickedArrayIdx:%d because locating of tracking coordinates FAILED\n"
                ,(int)ida
                ,(int)pickedArrayIdx
                );
       }
       break;
     }     
     
     
  }
  
  if (pickedArrayIdx >= 0)
  {
     perCoords->x0 = perCoords->ax0[pickedArrayIdx];    
     perCoords->y0 = perCoords->ay0[pickedArrayIdx];
     perCoords->x1 = perCoords->ax1[pickedArrayIdx]; 
     perCoords->y1 = perCoords->ay1[pickedArrayIdx];
     perCoords->x2 = perCoords->ax2[pickedArrayIdx];
     perCoords->y2 = perCoords->ay2[pickedArrayIdx];
     perCoords->x3 = perCoords->ax3[pickedArrayIdx];
     perCoords->y3 = perCoords->ay3[pickedArrayIdx];
     
     if(gap_debug)
     {
       printf("\nfineTune RESULT pickedArrayIdx:%d\n\n"
             ,(int)pickedArrayIdx
             );
     }
  }

  
  if(gap_debug != TRUE)
  {
    /* remove the temporary image (that was created just for probe purpose) */
    gap_image_delete_immediate(tmpImageId);
  }

}  /* end p_fineTuneProbePerspectiveTransformationOld2 */

/* --------------------------------------------
 * p_fineTuneProbePerspectiveTransformationOld   DEPRECATED 
 * --------------------------------------------
 * Old implementation. TODO remove this after successful test of the new variante !
 * --------------------------------------------------------------------------------
 * This procedure creates a temporary image with a copy of a referenceLayer and performs probe rendering
 * of transformed copies of the activeDrawable to select the best matching transformation.
 * Therefore areas of the background (marked as opaque via layermask) 
 * are compared referenceLayer versus probe rendered transformed layer 
 * with similar variants of persepective transformation coorinates.
 *
 *
 * This processing and resource intensive fine tuning shall reduce unwanted jitter effects
 * with small amplitudes near +- 1 pixel.
 */
static void
p_fineTuneProbePerspectiveTransformationOld(gint32 activeDrawableId, gint32 referenceLayerId, GapPerspectiveTransCoords *perCoords)
{
  gdouble bestAvgColordiff;
  gdouble bestPrecision;
  gint    pickedArrayIdx;
  gint    ida;
  gint    l_src_offset_x, l_src_offset_y;    /* layeroffsets as they were in src_image */
  gint32  tmpImageId;
  gint32  requiredPixelCount;
  gint32  comparedPixelCount;
  gint32  tmpBgLayerId;

  
  if(FALSE)
  {
    p_generateFineTuningPerCoords(perCoords);
  }
 
  if(gap_debug)
  {
    printf("p_fineTuneProbePerspectiveTransformation START numberOfArrayValues: %d  activeDrawableId:%d  referenceLayerId:%d\n"
      , (int)perCoords->numberOfArrayValues
      , (int)activeDrawableId
      , (int)referenceLayerId
      );
  }

  if((perCoords->numberOfArrayValues < 2) || (activeDrawableId == referenceLayerId))
  {
    /* do nothing in case there are no alternative values available 
     * or in case active and reference are the same layer
     */
    return;
  }

  bestAvgColordiff = 1.0; /* init with worst possible value */
  bestPrecision = 9999.9; /* worst */
  pickedArrayIdx = -1;
  
  /* create tmpImageId */
  tmpImageId = gap_image_new_of_samesize(gimp_item_get_image(activeDrawableId));
  
  /* copy referenceLayerId as tmpBgLayerId Layer to tmpImageId 
   * TODO: check if layermask is included in the copy ...
   */
  tmpBgLayerId = gap_layer_copy_to_dest_image(tmpImageId
                                             , referenceLayerId   /* l_src_layer_id */
                                             , 100.0   /* Opacity */
                                             ,0        /* NORMAL */
                                             ,&l_src_offset_x
                                             ,&l_src_offset_y
                                             );
  gimp_image_insert_layer(tmpImageId
                         , tmpBgLayerId
                         , 0
                         , 0              /* top of layerstack */
                         );
  gimp_layer_resize_to_image_size(tmpBgLayerId);
  
  // TODO
  //   if (referenceLayerId has no layermask)
  //   {
  //     create a black layermask on tmpBgLayerId with white filled circle around the Target coorinates.
  //   }
  
  
  /* apply layermask on tmpBgLayerId */
  gimp_layer_remove_mask (tmpBgLayerId, GIMP_MASK_APPLY);


  /* compare refDrawable against itself
   * to count the number of opaque pixels in it
   */
  gap_locateColordiffOpaquePixels(tmpBgLayerId      /* refDrawable */
                                 , tmpBgLayerId     /*  targetDrawableId */
                                 , 1                /* gint32  requiredPixelCount */
                                 , &comparedPixelCount    /* gint32 *comparedPixelCount */
                                 );
  /* require 90 percent of the opaque pixels to be involved in further compare steps */
  requiredPixelCount = (comparedPixelCount * 90) / 100;
  if(gap_debug)
  {
     printf("FINE TUNE tmpBgLayerId:%d  number of opaque pixels found:%d requiredPixelCount:%d (90 percent)\n"
           ,(int)tmpBgLayerId
           ,(int)comparedPixelCount
           ,(int)requiredPixelCount
           );
  }

  if (comparedPixelCount < 10)
  {
    /* delete the temporary image and skip fine tune attempts (does not make sense
     * when having not enough opaque refernce pixels
     */
    gap_image_delete_immediate(tmpImageId);
    return;
  }

  /* probe perspective transformation variants with potential settings
   * and pick the one that delivers best match
   * (with the lowest colordiff compared to the reference layer at its opaque areas)
   */
  for(ida = 0; ida < perCoords->numberOfArrayValues; ida++)
  {
     gint32 attemptLayerId;
     gint32 transformedDrawableId;
     gdouble avgColordiff;
     
     
     /* copy activeDrawableId to tmpImageId */
     attemptLayerId = gap_layer_copy_to_dest_image(tmpImageId
                                             , activeDrawableId   /* l_src_layer_id */
                                             , 100.0   /* Opacity */
                                             ,0        /* NORMAL */
                                             ,&l_src_offset_x
                                             ,&l_src_offset_y
                                             );
     gimp_image_insert_layer(tmpImageId
                             , attemptLayerId
                             , 0
                             , 0              /* top of layerstack */
                             );

     
     gimp_context_set_defaults();
     gimp_context_set_transform_resize(GIMP_TRANSFORM_RESIZE_ADJUST);   /* do NOT clip */                                 
     gimp_context_set_transform_direction(GIMP_TRANSFORM_FORWARD);                                 
     transformedDrawableId = gimp_item_transform_perspective(attemptLayerId
                                      , perCoords->ax0[ida], perCoords->ay0[ida]
                                      , perCoords->ax1[ida], perCoords->ay1[ida]
                                      , perCoords->ax2[ida], perCoords->ay2[ida]
                                      , perCoords->ax3[ida], perCoords->ay3[ida]
                                      );
     if(gap_debug)
     {
       printf("IDA:%d transformedDrawableId:%d attemptLayerId:%d\n"
         , (int)ida
         , (int)transformedDrawableId
         , (int)attemptLayerId
         );
     }
     
     gimp_layer_resize_to_image_size(transformedDrawableId);
     
     /* compare all opaque pixels and calculate 
      *  average colordifference tmpBgLayerId versus transformedDrawableId
      */
     avgColordiff = gap_locateColordiffOpaquePixels(tmpBgLayerId            /* refDrawable */
                                                   , transformedDrawableId  /*  targetDrawableId */
                                                   , requiredPixelCount     /* gint32  requiredPixelCount */
                                                   , &comparedPixelCount    /* gint32 *comparedPixelCount */
                                                   );
     if((avgColordiff < bestAvgColordiff)
     || ((avgColordiff == bestAvgColordiff) && ( perCoords->aprecision[ida] < bestPrecision)))
     {
       /* pick the best matching transformation attempt
        * for the same transformation to the activeDrawableId.
        * best matching has smallest color diff compared with referenceLayerId 
        */
       pickedArrayIdx = ida;
       bestAvgColordiff = avgColordiff;
       bestPrecision = perCoords->aprecision[ida];
     }
     

     if(gap_debug)
     {
         printf("PROBE ida:%d drawableId:%d  comparedPixelCount:%d avgColordiff:%0.12f precision:%0.12f bestPrecision:%0.12f bestAvgColordiff[%d]:%0.12f\n"
           ,(int)ida
           ,(int)transformedDrawableId
           ,(int)comparedPixelCount
           ,(double)avgColordiff
           ,(double)perCoords->aprecision[ida]
           ,(double)bestPrecision
           ,(int)pickedArrayIdx
           ,(double)bestAvgColordiff
           );
     }
     
     
  }
  
  if (pickedArrayIdx >= 0)
  {
     perCoords->x0 = perCoords->ax0[pickedArrayIdx];    
     perCoords->y0 = perCoords->ay0[pickedArrayIdx];
     perCoords->x1 = perCoords->ax1[pickedArrayIdx]; 
     perCoords->y1 = perCoords->ay1[pickedArrayIdx];
     perCoords->x2 = perCoords->ax2[pickedArrayIdx];
     perCoords->y2 = perCoords->ay2[pickedArrayIdx];
     perCoords->x3 = perCoords->ax3[pickedArrayIdx];
     perCoords->y3 = perCoords->ay3[pickedArrayIdx];
  }

  
  if(gap_debug)
  {
    /* when debugging add_display and keep the tmpImageId for analyse purpose */
    gimp_display_new(tmpImageId);
  }
  else
  {
    /* remove the temporary image (that was created just for probe purpose) */
    gap_image_delete_immediate(tmpImageId);
  }

}  /* end p_fineTuneProbePerspectiveTransformationOld */




/* ================= DIALOG stuff Start ================= */

/* -------------------------------------
 * p_exact_align_calculate_4point_values
 * -------------------------------------
 * calculate 4-point alignment transformation setting.
 * (this procedure is intended for GUI feedback, therfore
 * deliver angle in degree and scale in percent)
 */
static void
p_exact_align_calculate_4point_values(GapAlignCoords *alignCoords
   , gdouble *angleDeg, gdouble *scalePercent, gdouble *moveDx, gdouble *moveDy)
{
  gdouble px1, py1, px2, py2;
  gdouble px3, py3, px4, py4;
  gdouble dx1, dy1, dx2, dy2;
  gdouble angle1Rad, angle2Rad, angleRad;
  gdouble len1, len2;
  gdouble scaleXY;
  
  px1 = alignCoords->startCoords[0].px;
  py1 = alignCoords->startCoords[0].py;
  px2 = alignCoords->startCoords[1].px;
  py2 = alignCoords->startCoords[1].py;
  
  px3 = alignCoords->currCoords[0].px;
  py3 = alignCoords->currCoords[0].py;
  px4 = alignCoords->currCoords[1].px;
  py4 = alignCoords->currCoords[1].py;

  dx1 = px2 - px1;
  dy1 = py2 - py1;
  dx2 = px4 - px3;
  dy2 = py4 - py3;
  
  /* the angle between the two lines. i.e., the angle layer2 must be clockwise rotated
   * in order to overlap with initial start layer1
   */
  angle1Rad = 0;
  angle2Rad = 0;
  if (dx1 != 0.0)
  {
    angle1Rad = atan(dy1 / dx1);
  }
  if (dx2 != 0.0)
  {
    angle2Rad = atan(dy2 / dx2);
  }
  angleRad = angle1Rad - angle2Rad;
  
  /* the scale factors current layer must be mulitplied by,
   * in order to fit onto reference start layer.
   * this is simply the ratio of the two line lenths from the path we created with the 4 points
   */
  
  len1 = sqrt((dx1 * dx1) + (dy1 * dy1));
  len2 = sqrt((dx2 * dx2) + (dy2 * dy2));

  scaleXY = 1.0;
  if ((len1 != len2)
  &&  (len2 != 0.0))
  {
    scaleXY = len1 / len2;
  }


  *angleDeg = (angleRad * 180.0) / G_PI;
  *scalePercent = scaleXY * 100.0;
  *moveDx = px3 - px1;
  *moveDy = py3 - py1;

  
}  /* end p_exact_align_calculate_4point_values */



/* ------------------------------
 * p_refresh_and_update_infoLabel
 * ------------------------------
 */
static void
p_refresh_and_update_infoLabel(GtkWidget *widgetDummy, AlignDialogVals *advPtr)
{
  GapAlignCoords  tmpAlingCoords;
  gboolean     okSensitiveFlag;
  gdouble      angleDeg;
  gdouble      scalePercent;
  gdouble      moveDx;
  gdouble      moveDy;
  gchar        *infoText;


  if(gap_debug)
  {
    printf("p_refresh_and_update_infoLabel widgetDummy:%ld advPtr:%ld\n"
          , (long) widgetDummy
          , (long) advPtr
          );
  }

  if ((advPtr->okButton == NULL) || (advPtr->infoLabel == NULL))
  {
    return;
  }

  angleDeg = 0.0;
  scalePercent = 100.0;
  moveDx = 0.0;
  moveDy = 0.0;
  
  advPtr->countVaildPoints = p_capture_4_vector_points(advPtr->image_id, &tmpAlingCoords, advPtr->pointOrder);
  if (advPtr->countVaildPoints >= 8)
  {
    GapPerspectiveTransCoords  perspectiveCoords;
    GapPerspectiveTransCoords *perCoords;
    gboolean perCoordsOk;
      
    perCoords = &perspectiveCoords;
    
    perCoords->transformPrecisionThreshold = GAP_GEO_TRANSFORM_PRECISION_THRSHOLD;
    perCoords->transformPrecision          = GAP_GEO_TRANSFORM_PRECISION;
    
    perCoordsOk = gap_geo_perspective_trans_coords_from_align_coords(advPtr->activeDrawableId, &tmpAlingCoords, perCoords);
    if (perCoordsOk == TRUE)
    {
      GimpMatrix3     matrix;

      /* calculate the matrix from 4 displaced corners 
       * (thes ame way as GIMP does for perspective transformation tools) 
       */
      gap_geo_assemble_perspective_transformation_matrix(&matrix, perCoords, advPtr->activeDrawableId);

      
      infoText = g_strdup_printf(_("Current path and %s path triggers Perspective transformation:\n"
                            "    Top Left     Corner x: %.4f  y: %.4f (pixels)\n"
                            "    Top Right    Corner x: %.4f  y: %.4f (pixels)\n"
                            "    Bottom Left  Corner x: %.4f  y: %.4f (pixels)\n"
                            "    Bottom Right Corner x: %.4f  y: %.4f (pixels)\n"
                            "Transformation Matrix\n"
                            "    %12.5f %12.5f %12.5f\n"
                            "    %12.5f %12.5f %12.5f\n"
                            "    %12.5f %12.5f %12.5f\n"
                            "\nPress OK button to perspective transform the layer\n")
                            , GAP_EXACT_ALIGNER_TARGET_PATH_NAME
                            , (float) perCoords->x0, (float)perCoords->y0
                            , (float) perCoords->x1, (float)perCoords->y1
                            , (float) perCoords->x2, (float)perCoords->y2
                            , (float) perCoords->x3, (float)perCoords->y3
                            , (float) matrix.coeff[0][0], (float) matrix.coeff[0][1], (float) matrix.coeff[0][2]
                            , (float) matrix.coeff[1][0], (float) matrix.coeff[1][1], (float) matrix.coeff[1][2]
                            , (float) matrix.coeff[2][0], (float) matrix.coeff[2][1], (float) matrix.coeff[2][2]
                            );
      gtk_label_set_text(GTK_LABEL(advPtr->infoLabel), infoText);
      g_free(infoText);
      okSensitiveFlag = TRUE;
    }
    else
    {
      infoText = g_strdup_printf(_("Current paths are not valid for Perspective transformation:\n"
                            "    For valid transformation 4 points are required\n"
                            "    both in the active path and in another path with the name: %s \n"
                            "    AND the connection of the 4 points must build up 4 different lines.\n"
                            "")
                            , GAP_EXACT_ALIGNER_TARGET_PATH_NAME
                            );
      gtk_label_set_text(GTK_LABEL(advPtr->infoLabel), infoText);
      g_free(infoText);
      okSensitiveFlag = FALSE;
    }
  }  
  else if ((advPtr->countVaildPoints >= 4) 
       && (tmpAlingCoords.startCoords[1].valid)
       && (tmpAlingCoords.currCoords[1].valid))
  {
    p_exact_align_calculate_4point_values(&tmpAlingCoords
          , &angleDeg, &scalePercent, &moveDx, &moveDy);


    infoText = g_strdup_printf(_("Current path with 4 point triggers transformations:\n"
                            "    Rotation:   %.4f (degree)\n"
                            "    Scale:      %.1f (%%)\n"
                            "    Movement X: %.0f (pixels)\n"
                            "    Movement Y: %.0f (pixels)\n"
                            "\nPress OK button to transform the layer\n")
                            , (float) angleDeg
                            , (float) scalePercent
                            , (float) moveDx
                            , (float) moveDy
                            );
    gtk_label_set_text(GTK_LABEL(advPtr->infoLabel), infoText);
    g_free(infoText);
    okSensitiveFlag = TRUE;
  }
  else if ((advPtr->countVaildPoints >= 2)
       && (tmpAlingCoords.startCoords[0].valid)
       && (tmpAlingCoords.currCoords[0].valid))
  {
    moveDx = tmpAlingCoords.currCoords[0].px - tmpAlingCoords.startCoords[0].px;
    moveDy = tmpAlingCoords.currCoords[0].py - tmpAlingCoords.startCoords[0].py;
    
    infoText = g_strdup_printf(_("Current path with 2 points triggers simple move:\n"
                            "    Movement X: %.0f (pixels)\n"
                            "    Movement Y: %.0f (pixels)\n"
                            "\nPress OK button to move the layer\n")
                            , (float) moveDx
                            , (float) moveDy
                            );

    gtk_label_set_text(GTK_LABEL(advPtr->infoLabel), infoText);
    g_free(infoText);
    okSensitiveFlag = TRUE;
  }
  else
  {
    if (advPtr->pointOrder == POINT_ORDER_MODE_1234_1234)
    {
      infoText = g_strdup_printf(_("This filter requires a current path with 4 or 2 points\n"
                                   "and a path with name: %s with same number of points.\n"
                                   "\nPlease create both paths and press the Refresh button.\n")
                                 , GAP_EXACT_ALIGNER_TARGET_PATH_NAME
                                 );
      gtk_label_set_text(GTK_LABEL(advPtr->infoLabel), infoText);
      g_free(infoText);
    }
    else
    {
      gtk_label_set_text(GTK_LABEL(advPtr->infoLabel)
                        , _("This filter requires a current path with 4 or 2 points\n"
                            "It can transform and/or move the current layer according to such path coordinate points.\n"
                            "\nPlease create a path and press the Refresh button."));
    }
    okSensitiveFlag = FALSE;
  }

  gtk_widget_set_sensitive(advPtr->okButton, okSensitiveFlag);
  
}  /* end p_refresh_and_update_infoLabel */



/* ---------------------------------
 * on_exact_align_response
 * ---------------------------------
 */
static void
on_exact_align_response (GtkWidget *widget,
                 gint       response_id,
                 AlignDialogVals *advPtr)
{
  GtkWidget *dialog;

  switch (response_id)
  {
    case GAP_RESPONSE_REFRESH_PATH:
      if(advPtr != NULL)
      {
        p_refresh_and_update_infoLabel(NULL, advPtr);
      }
      break;
    case GTK_RESPONSE_OK:
      if(advPtr)
      {
        if (GTK_WIDGET_VISIBLE (advPtr->shell))
        {
          gtk_widget_hide (advPtr->shell);
        }

        advPtr->runExactAlign = TRUE;
      }

    default:
      dialog = NULL;
      if(advPtr)
      {
        dialog = advPtr->shell;
        if(advPtr)
        {
          advPtr->shell = NULL;
          gtk_widget_destroy (dialog);
        }
      }
      gtk_main_quit ();
      break;
  }
}  /* end on_exact_align_response */


/* --------------------------------------
 * on_order_mode_radio_callback
 * --------------------------------------
 */
static void
on_order_mode_radio_callback(GtkWidget *wgt, gpointer user_data)
{
  AlignDialogVals *advPtr;

  if(gap_debug)
  {
    printf("on_order_mode_radio_callback: START\n");
  }
  advPtr = (AlignDialogVals*)user_data;
  if(advPtr != NULL)
  {
     if(wgt == advPtr->radio_order_mode_31_42)    { advPtr->pointOrder = POINT_ORDER_MODE_31_42; }
     if(wgt == advPtr->radio_order_mode_21_43)    { advPtr->pointOrder = POINT_ORDER_MODE_21_43; }
     if(wgt == advPtr->radio_order_mode_1234)     { advPtr->pointOrder = POINT_ORDER_MODE_1234_1234; }
     

     p_refresh_and_update_infoLabel(NULL, advPtr);

  }
}


/* --------------------------
 * p_align_dialog
 * --------------------------
 */
static void
p_align_dialog(AlignDialogVals *advPtr)

{
  GtkWidget *dialog;
  GtkWidget *main_vbox;
  GtkWidget *label;
  GtkWidget *button;
  GtkWidget *table;
  GtkWidget *vbox1;
  GSList    *vbox1_group = NULL;
  GtkWidget *radiobutton;
  gint       row;
  gchar     *text;


  advPtr->runExactAlign = FALSE;

  gimp_ui_init (GAP_EXACT_ALIGNER_PLUG_IN_NAME_BINARY, TRUE);

  dialog = gimp_dialog_new (_("Transform Layer via 4 (or 2) point Alignment"), GAP_EXACT_ALIGNER_PLUG_IN_NAME_BINARY,
                            NULL, 0,
                            gimp_standard_help_func, GAP_EXACT_ALIGNER_PLUG_IN_NAME,

                            GTK_STOCK_REFRESH, GAP_RESPONSE_REFRESH_PATH,
                            GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,

                            NULL);
  advPtr->shell = dialog;
  button = gimp_dialog_add_button (GIMP_DIALOG(dialog), GTK_STOCK_OK, GTK_RESPONSE_OK);
  advPtr->okButton = button;

  gtk_dialog_set_alternative_button_order (GTK_DIALOG (dialog),
                                           GTK_RESPONSE_OK,
                                           GTK_RESPONSE_CANCEL,
                                           -1);

  gimp_window_set_transient (GTK_WINDOW (dialog));

  main_vbox = gtk_vbox_new (FALSE, 12);
  gtk_container_set_border_width (GTK_CONTAINER (main_vbox), 12);
  gtk_container_add (GTK_CONTAINER (GTK_DIALOG (dialog)->vbox), main_vbox);
  gtk_widget_show (main_vbox);

  g_signal_connect (G_OBJECT (dialog), "response",
                      G_CALLBACK (on_exact_align_response),
                      advPtr);


  /* Controls */
  table = gtk_table_new (3, 3, FALSE);
  gtk_table_set_col_spacings (GTK_TABLE (table), 6);
  gtk_table_set_row_spacings (GTK_TABLE (table), 6);
  gtk_box_pack_start (GTK_BOX (main_vbox), table, FALSE, FALSE, 0);
  gtk_widget_show (table);

  row = 0;

  /* info label  */
  label = gtk_label_new ("--");
  advPtr->infoLabel = label;
  gtk_widget_show (label);
  gtk_table_attach (GTK_TABLE (table), label, 0, 3, row, row+1,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);
  gtk_misc_set_alignment (GTK_MISC (label), 0.5, 0.5);

  row++;


  /* pointOrder radiobutton
   * POINT_ORDER_MODE_31_42:  compatible to the exact aligner script (from the plugin registry)
   */
  label = gtk_label_new (_("Path Point Order:"));
  gtk_widget_show (label);
  gtk_table_attach (GTK_TABLE (table), label, 0, 1, row, row+1,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);
  gtk_misc_set_alignment (GTK_MISC (label), 0, 0.5);


  /* vbox for radio group */
  vbox1 = gtk_vbox_new (FALSE, 0);

  gtk_widget_show (vbox1);
  gtk_table_attach (GTK_TABLE (table), vbox1, 1, 3, row, row+1,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);


  /* Order Mode the radio buttons */
  text = g_strdup_printf(_("( 3 --> 1 )  ( 4 --> 2 )\n"
                           "Source is marked by current path points 3&4\n"
                           "Target is marked by current path points 1&3\n")
                          );
  radiobutton = gtk_radio_button_new_with_label (vbox1_group, text);
  g_free(text);
  advPtr->radio_order_mode_31_42 = radiobutton;
  vbox1_group = gtk_radio_button_get_group (GTK_RADIO_BUTTON (radiobutton));
  gtk_widget_show (radiobutton);
  gtk_box_pack_start (GTK_BOX (vbox1), radiobutton, FALSE, FALSE, 0);
  g_signal_connect (G_OBJECT (radiobutton),     "clicked",  G_CALLBACK (on_order_mode_radio_callback), advPtr);
  if(advPtr->pointOrder == POINT_ORDER_MODE_31_42)
  {
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON (radiobutton), TRUE);
  }

  text = g_strdup_printf(_("( 2 --> 1 )  ( 4 --> 3 )\n"
                           "Source is marked by current path points 2&4\n"
                           "Target is marked by current path points 1&3\n")
                          );
  radiobutton = gtk_radio_button_new_with_label (vbox1_group, text);
  g_free(text);
  advPtr->radio_order_mode_21_43 = radiobutton;
  vbox1_group = gtk_radio_button_get_group (GTK_RADIO_BUTTON (radiobutton));
  gtk_widget_show (radiobutton);
  gtk_box_pack_start (GTK_BOX (vbox1), radiobutton, FALSE, FALSE, 0);
  g_signal_connect (G_OBJECT (radiobutton),     "clicked",  G_CALLBACK (on_order_mode_radio_callback), advPtr);
  if(advPtr->pointOrder == POINT_ORDER_MODE_21_43)
  {
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON (radiobutton), TRUE);
  }

  text = g_strdup_printf(_("( 1 --> T1 ) ( 2 --> T2 ) ( 3 --> T3 ) ( 4 --> T4 )\n"
                           "Source is marked by current path points 1,2,3,4\n"
                           "Target is marked by path with name: %s points 1,2,3,4")
                          , GAP_EXACT_ALIGNER_TARGET_PATH_NAME
                          );
  radiobutton = gtk_radio_button_new_with_label (vbox1_group, text);
  g_free(text);
  advPtr->radio_order_mode_1234 = radiobutton;
  vbox1_group = gtk_radio_button_get_group (GTK_RADIO_BUTTON (radiobutton));
  gtk_widget_show (radiobutton);
  gtk_box_pack_start (GTK_BOX (vbox1), radiobutton, FALSE, FALSE, 0);
  g_signal_connect (G_OBJECT (radiobutton),     "clicked",  G_CALLBACK (on_order_mode_radio_callback), advPtr);
  if(advPtr->pointOrder == POINT_ORDER_MODE_1234_1234)
  {
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON (radiobutton), TRUE);
  }


  p_refresh_and_update_infoLabel(NULL, advPtr);

  /* Done */

  gtk_widget_show (dialog);

  gtk_main ();
  gdk_flush ();


}  /* end p_align_dialog */


/* ================= DIALOG stuff End =================== */



/* ------------------------------------------
 * gap_detail_exact_align_via_4point_path
 * ------------------------------------------
 *
 */
gint32
gap_detail_exact_align_via_4point_path(gint32 image_id, gint32 activeDrawableId
   , gint32 pointOrder, GimpRunMode runMode)
{
  GapAlignCoords  alingCoordinates;
  GapAlignCoords *alignCoords;
  gint         countVaildPoints;
  gint32       ret;
  AlignDialogVals         advals;
  
  alignCoords = &alingCoordinates;
  ret = -1;
  

  advals.runExactAlign = TRUE;
  advals.image_id = image_id;
  advals.activeDrawableId = activeDrawableId;

  if (runMode == GIMP_RUN_NONINTERACTIVE)
  {
    advals.pointOrder = pointOrder;
  }
  else
  {
    /* get last used value (or default) 
     * from gimprc settings
     */
    advals.pointOrder = gap_base_get_gimprc_int_value(GIMPRC_EXACT_ALIGN_PATH_POINT_ORDER
                                                    , POINT_ORDER_MODE_31_42 /* DEFAULT */
                                                    , POINT_ORDER_MODE_31_42 /* min */
                                                    , POINT_ORDER_MODE_1234_1234 /* max */
                                                    );
  }

  
  if (runMode == GIMP_RUN_INTERACTIVE)
  {
    p_align_dialog(&advals);
  }
  
  if (advals.runExactAlign != TRUE)
  {
    return (ret);
  }

  if (runMode == GIMP_RUN_INTERACTIVE)
  {
    /* store order flag when entered via userdialog
     * in gimprc (for next use in same or later gimp session)
     */
    p_save_gimprc_gint32_value(GIMPRC_EXACT_ALIGN_PATH_POINT_ORDER, advals.pointOrder);
  }
  
 
  gimp_image_undo_group_start (image_id);
  
  countVaildPoints = p_capture_4_vector_points(image_id, alignCoords, advals.pointOrder);

  if(gap_debug)
  {
    printf("gap_detail_exact_align_via_4point_path activeDrawableId:%d pointOrder:%d countVaildPoints:%d\n"
      , (int)activeDrawableId
      , (int)advals.pointOrder
      , (int)countVaildPoints
      );
  }

  if(countVaildPoints == 8)
  {
    ret = p_perspective_align_drawable(activeDrawableId, alignCoords, -1
                                      , GAP_GEO_TRANSFORM_PRECISION_THRSHOLD
                                      , GAP_GEO_TRANSFORM_PRECISION);

  } else if(countVaildPoints == 4)
  {
    ret = p_exact_align_drawable(activeDrawableId, alignCoords);

  }
  else if(countVaildPoints == 2)
  {
    /* force order 0213 
     *  (captures the 2 valid points into startCoords and currCoords)
     */
    countVaildPoints = p_capture_4_vector_points(image_id, alignCoords, POINT_ORDER_MODE_21_43);
    ret = p_set_drawable_offsets(activeDrawableId, alignCoords);
  }
  else
  {
    if (advals.pointOrder == POINT_ORDER_MODE_31_42)
    {
      g_message(_("This filter requires a current path with 4 points, "
                "where point 1 and 2 mark reference positions "
                "and point 3 and 4 mark positions in the target layer. "
                "It transforms the target layer in a way that "
                "point3 is moved to point1 and point4 moves to point2. "
                "(this may include rotate and scale transformation).\n"
                "A path with 2 points can be used to move point2 to point1. "
                "(via simple move operation without rotate and scale)"));
    }
    else
    {
      g_message(_("This filter requires a current path with 4 points, "
                "where point 1 and 3 mark reference positions "
                "and point 2 and 4 mark positions in the target layer. "
                "It transforms the target layer in a way that "
                "point2 is moved to point1 and point4 moves to point3. "
                "(this may include rotate and scale transformation).\n"
                "A path with 2 points can be used to move point2 to point1. "
                "(via simple move operation without rotate and scale)"));
    }
  }
 
  gimp_image_undo_group_end (image_id);
  
  return(ret);
  
  
}  /* end  gap_detail_exact_align_via_4point_path */
