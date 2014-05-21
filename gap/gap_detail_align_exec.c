/*  gap_detail_align_exec.c
 *    This transforms and/or moves the active layer with 4 or 2 controlpoints.
 *    controlpoints input from current path (or from an xml input file recorded by GAP detail tracking feature)
 *    4 points: rotate scale and move the layer in a way that 2 reference points match 2 target points. 
 *    2 points: simple move the layer from reference point to target point.
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
#include "gap_libgapbase.h"
#include "gap_detail_align_exec.h"

#include "gap-intl.h"

#define GIMPRC_EXACT_ALIGN_PATH_POINT_ORDER "gap-exact-aligner-path-point-order"
#define GAP_RESPONSE_REFRESH_PATH 1



typedef struct AlignDialogVals
{
  gint32    pointOrder;
  
  gboolean  runExactAlign;
  gint      countVaildPoints;
  gint32    image_id;
  gint32    activeDrawableId;

  GtkWidget *radio_order_mode_31_42;
  GtkWidget *radio_order_mode_21_43;
  GtkWidget *infoLabel;
  GtkWidget *okButton;
  GtkWidget *shell;

} AlignDialogVals;


typedef struct PixelCoords
{
  gboolean  valid;
  gint32  px;
  gint32  py;
} PixelCoords;


typedef struct AlingCoords
{
  PixelCoords  currCoords;   /* 1st coords in current frame */
  PixelCoords  currCoords2;  /* 2nd detail coords in current frame */
  PixelCoords  startCoords;  /* 1st coords of first processed (reference) frame */
  PixelCoords  startCoords2; /* 2nd detail coords of first processed frame */
} AlingCoords;


typedef struct ParseContext {
  char  *parsePtr;
  gint32 frameNr;
  AlingCoords *alingCoords;
} ParseContext;



#define DEFAULT_framePhase 1


/* --------------------------------------
 * p_parse_value_gint32
 * --------------------------------------
 */
static gboolean
p_parse_value_gint32(ParseContext *parseCtx, gint32 *valDestPtr, gint *itemCount)
{
  gint64 value64;
  gchar *endptr;
  
  value64 = g_ascii_strtoll(parseCtx->parsePtr, &endptr, 10);
  if(parseCtx->parsePtr == endptr)
  {
    /* pointer was not advanced (no int number could be scanned */
    return (FALSE);
  }
  *valDestPtr = value64;
  parseCtx->parsePtr = endptr;
  *itemCount +=1;
  return (TRUE);

}  /* end p_parse_value_gint32 */


/* --------------------------------
 * p_parse_coords_p1_and_p2
 * --------------------------------
 * parse p1x, p1y, p2x, p2y values into p1 (mandatory) and p2 (ptional) coordinates
 * and parse keyframe_abs value int *frameNrPtr (optional if present)
 * multiple occurances are not tolerated.
 * return TRUE on success.
 */
static gboolean
p_parse_coords_p1_and_p2(ParseContext *parseCtx, PixelCoords *p1, PixelCoords *p2, gint32 *frameNrPtr)
{
  gboolean ok;
  gint     px1Count;
  gint     py1Count;
  gint     px2Count;
  gint     py2Count;
  gint     frCount;
  
  ok = TRUE;
  px1Count = 0;
  py1Count = 0;
  px2Count = 0;
  py2Count = 0;
  frCount  = 0;
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

  if ((ok == TRUE)
  && (px1Count == 1)
  && (py1Count == 1)
  && (frCount <= 1))
  {
    p1->valid = TRUE;
    if ((px2Count == 1)
    &&  (py2Count == 1))
    {
      p2->valid = TRUE;
    }

    return (TRUE);
  }


  
  if(gap_debug)
  {
    printf("p_parse_coords_p1_and_p2 ok:%d px1Count:%d py1Count:%d px2Count:%d py2Count:%d frCount:%d\n"
           " parsePtr:%.200s\n" 
      ,(int)ok
      ,(int)px1Count
      ,(int)py1Count
      ,(int)px2Count
      ,(int)py2Count
      ,(int)frCount
      ,parseCtx->parsePtr 
      );
  }
  return (FALSE);
  
}  /* end p_parse_coords_p1_and_p2 */


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
      
      if(parseCtx->alingCoords->startCoords.valid == FALSE)
      {
        ok = p_parse_coords_p1_and_p2(parseCtx
                               , &parseCtx->alingCoords->startCoords
                               , &parseCtx->alingCoords->startCoords2
                               , &frameNr
                               );
      }
      else
      {
        ok = p_parse_coords_p1_and_p2(parseCtx
                               , &parseCtx->alingCoords->currCoords
                               , &parseCtx->alingCoords->currCoords2
                               , &frameNr
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
      &&  (parseCtx->alingCoords->currCoords2.valid == TRUE))
      {
        return(TRUE);
      }
    }
    parseCtx->parsePtr++;
  }


  if ((ok == TRUE)
  &&  (parseCtx->alingCoords->currCoords2.valid == TRUE))
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
p_parse_xml_controlpoint_coords_from_file(const char *filename, gint32 frameNr, AlingCoords *alingCoords)
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
  parseCtx.alingCoords = alingCoords;
  parseCtx.frameNr = frameNr;

  parseCtx.alingCoords->startCoords.valid  = FALSE;
  parseCtx.alingCoords->startCoords2.valid = FALSE;
  parseCtx.alingCoords->currCoords.valid   = FALSE;
  parseCtx.alingCoords->currCoords2.valid  = FALSE;
  
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
p_set_drawable_offsets(gint32 activeDrawableId, AlingCoords *alingCoords)
{
  gdouble px1, py1, px2, py2;
  gdouble dx, dy;
  gint    offset_x;
  gint    offset_y;
  

  px1 = alingCoords->startCoords.px;
  py1 = alingCoords->startCoords.py;
  px2 = alingCoords->currCoords.px;
  py2 = alingCoords->currCoords.py;

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
p_exact_align_drawable(gint32 activeDrawableId, AlingCoords *alingCoords)
{
  gdouble px1, py1, px2, py2;
  gdouble px3, py3, px4, py4;
  gdouble dx1, dy1, dx2, dy2;
  gdouble angle1Rad, angle2Rad, angleRad;
  gdouble len1, len2;
  gdouble scaleXY;
  gint32  transformedDrawableId;
  
  px1 = alingCoords->startCoords.px;
  py1 = alingCoords->startCoords.py;
  px2 = alingCoords->startCoords2.px;
  py2 = alingCoords->startCoords2.py;
  
  px3 = alingCoords->currCoords.px;
  py3 = alingCoords->currCoords.py;
  px4 = alingCoords->currCoords2.px;
  py4 = alingCoords->currCoords2.py;

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
 * gap_detail_xml_align
 * -----------------------------------
 * This procedure transforms the specified drawable
 * according controlpoints in an XML file that was recorded via detail tracking.
 * it picks the relevant controlpoint at framePhase from the XML file
 * and scales, rotates and aligns the specified drawableId (shall be a layer)
 * in a way that it exactly matches with the 1st (reference) controlpoint in the XML file.
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
  
  if(xaVals->framePhase > 1)
  {
    gboolean  parseOk;
    AlingCoords alingCoords;
    
    parseOk =
      p_parse_xml_controlpoint_coords_from_file(xaVals->moveLogFile
         , xaVals->framePhase, &alingCoords);

    if(parseOk)
    {
      if ((alingCoords.startCoords2.valid == TRUE)
      &&  (alingCoords.currCoords2.valid == TRUE))
      {
        /* exact align transformation with 2 point pairs including rotation and scaling */
        newDrawableId = p_exact_align_drawable(drawableId, &alingCoords);
      }
      else
      {
        /* simple move (to match current recorded point to recorded start point) */
        newDrawableId = p_set_drawable_offsets(drawableId, &alingCoords);
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
  xaVals->framePhase                 = DEFAULT_framePhase;
  xaVals->moveLogFile[0]             = '\0';

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
           "  framePhase:%d  moveLogFile:%s\n"
            , (int)xaVals->framePhase
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
#define DETAIL_ALIGN_XML_DIALOG_ARGC 3

  static GapArrArg  argv[DETAIL_ALIGN_XML_DIALOG_ARGC];
  gint ii;
  gint ii_framePhase;

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



  ii++; gap_arr_arg_init(&argv[ii], GAP_ARR_WGT_DEFAULT_BUTTON);
  argv[ii].label_txt = _("Default");
  argv[ii].help_txt  = _("Reset all parameters to default values");

  if(TRUE == gap_arr_ok_cancel_dialog(_("Detail Align via XML"),
                            _("Settings :"),
                            DETAIL_ALIGN_XML_DIALOG_ARGC, argv))
  {
      xaVals->framePhase               = (gint32)(argv[ii_framePhase].int_ret);
      
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
 *            POINT_ORDER_MODE_21_43 : order 0,2,1,3  
 */
static gint
p_capture_4_vector_points(gint32 imageId, AlingCoords *alingCoords, gint32 pointOrder)
{
  gint32  activeVectorsId;
  PixelCoords *coordPtr[4];
  gint         ii;
  gint         countVaildPoints;
  
  if (pointOrder == POINT_ORDER_MODE_31_42)
  {
    coordPtr[0] = &alingCoords->startCoords;
    coordPtr[1] = &alingCoords->startCoords2;
    coordPtr[2] = &alingCoords->currCoords;
    coordPtr[3] = &alingCoords->currCoords2;
  }
  else
  {
    coordPtr[0] = &alingCoords->startCoords;
    coordPtr[2] = &alingCoords->startCoords2;
    coordPtr[1] = &alingCoords->currCoords;
    coordPtr[3] = &alingCoords->currCoords2;
  }

  
  countVaildPoints = 0;
  for(ii=0; ii < 4; ii++)
  {
    coordPtr[ii]->px = -1;
    coordPtr[ii]->py = -1;
    coordPtr[ii]->valid = FALSE;

  }

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
  
}  /* end p_capture_4_vector_points */


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










/* ================= DIALOG stuff Start ================= */

/* -------------------------------------
 * p_exact_align_calculate_4point_values
 * -------------------------------------
 * calculate 4-point alignment transformation setting.
 * (this procedure is intended for GUI feedback, therfore
 * deliver angle in degree and scale in percent)
 */
static void
p_exact_align_calculate_4point_values(AlingCoords *alingCoords
   , gdouble *angleDeg, gdouble *scalePercent, gdouble *moveDx, gdouble *moveDy)
{
  gdouble px1, py1, px2, py2;
  gdouble px3, py3, px4, py4;
  gdouble dx1, dy1, dx2, dy2;
  gdouble angle1Rad, angle2Rad, angleRad;
  gdouble len1, len2;
  gdouble scaleXY;
  
  px1 = alingCoords->startCoords.px;
  py1 = alingCoords->startCoords.py;
  px2 = alingCoords->startCoords2.px;
  py2 = alingCoords->startCoords2.py;
  
  px3 = alingCoords->currCoords.px;
  py3 = alingCoords->currCoords.py;
  px4 = alingCoords->currCoords2.px;
  py4 = alingCoords->currCoords2.py;

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
  AlingCoords  tmpAlingCoords;
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
  
  if (advPtr->countVaildPoints == 4)
  {
    p_exact_align_calculate_4point_values(&tmpAlingCoords
          , &angleDeg, &scalePercent, &moveDx, &moveDy);


    infoText = g_strdup_printf(_("Current path with 4 point triggers transformations:\n"
                            "    Rotation:   %.4f (degree)\n"
                            "    Scale:      %.1f (%%)\n"
                            "    Movement X: %.0f (pixels)\n"
                            "    Movement Y: %.0f (pixels)\n"
                            "Press OK button to transform the layer\n"
                            "in a way that point3 moves to point1 and point4 moves to point2")
                            , (float) angleDeg
                            , (float) scalePercent
                            , (float) moveDx
                            , (float) moveDy
                            );
    gtk_label_set_text(GTK_LABEL(advPtr->infoLabel), infoText);
    g_free(infoText);
    okSensitiveFlag = TRUE;
  }
  else if (advPtr->countVaildPoints == 2)
  {
    moveDx = tmpAlingCoords.currCoords.px - tmpAlingCoords.startCoords.px;
    moveDy = tmpAlingCoords.currCoords.py - tmpAlingCoords.startCoords.py;
    
    infoText = g_strdup_printf(_("Current path with 2 points triggers simple move:\n"
                            "    Movement X: %.0f (pixels)\n"
                            "    Movement Y: %.0f (pixels)\n"
                            "Press OK button to move the layer\n"
                            "in a way that point2 moves to point1")
                            , (float) moveDx
                            , (float) moveDy
                            );

    gtk_label_set_text(GTK_LABEL(advPtr->infoLabel), infoText);
    g_free(infoText);
    okSensitiveFlag = TRUE;
  }
  else
  {
    gtk_label_set_text(GTK_LABEL(advPtr->infoLabel)
                        , _("This filter requires a current path with 4 or 2 points\n"
                            "It can transform and/or move the current layer according to such path coordinate points.\n"
                            "Please create a path and press the Refresh button."));
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
  radiobutton = gtk_radio_button_new_with_label (vbox1_group, _("( 3 --> 1 )  ( 4 --> 2 )\nTarget is marked by points 1&2 "));
  advPtr->radio_order_mode_31_42 = radiobutton;
  vbox1_group = gtk_radio_button_get_group (GTK_RADIO_BUTTON (radiobutton));
  gtk_widget_show (radiobutton);
  gtk_box_pack_start (GTK_BOX (vbox1), radiobutton, FALSE, FALSE, 0);
  g_signal_connect (G_OBJECT (radiobutton),     "clicked",  G_CALLBACK (on_order_mode_radio_callback), advPtr);
  if(advPtr->pointOrder == POINT_ORDER_MODE_31_42)
  {
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON (radiobutton), TRUE);
  }

  radiobutton = gtk_radio_button_new_with_label (vbox1_group, "( 2 --> 1 )  ( 4 --> 3 )\nTarget is marked by points 1&3");
  advPtr->radio_order_mode_21_43 = radiobutton;
  vbox1_group = gtk_radio_button_get_group (GTK_RADIO_BUTTON (radiobutton));
  gtk_widget_show (radiobutton);
  gtk_box_pack_start (GTK_BOX (vbox1), radiobutton, FALSE, FALSE, 0);
  g_signal_connect (G_OBJECT (radiobutton),     "clicked",  G_CALLBACK (on_order_mode_radio_callback), advPtr);
  if(advPtr->pointOrder == POINT_ORDER_MODE_21_43)
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
  AlingCoords  alingCoordinates;
  AlingCoords *alingCoords;
  gint         countVaildPoints;
  gint32       ret;
  AlignDialogVals         advals;
  
  alingCoords = &alingCoordinates;
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
                                                    , POINT_ORDER_MODE_21_43 /* max */
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
  
  countVaildPoints = p_capture_4_vector_points(image_id, alingCoords, advals.pointOrder);
  if(countVaildPoints == 4)
  {
    ret = p_exact_align_drawable(activeDrawableId, alingCoords);

  }
  else if(countVaildPoints == 2)
  {
    /* force order 0213 
     *  (captures the 2 valid points into startCoords and currCoords)
     */
    countVaildPoints = p_capture_4_vector_points(image_id, alingCoords, POINT_ORDER_MODE_21_43);
    ret = p_set_drawable_offsets(activeDrawableId, alingCoords);
  }
  else
  {
    if (advals.pointOrder == POINT_ORDER_MODE_31_42)
    {
      g_message(_("This filter requires a current path with 4 points,"
                "where point 1 and 2 mark reference positions "
                "and point 3 and 4 mark postions in the target layer."
                "It transforms the target layer in a way that "
                "point3 is moved to point1 and point4 moves to point2."
                "(this may include rotate an scale transformation).\n"
                "A path with 2 points can be used to move point2 to point1."
                "(via simple move operation without rotate and scale)"));
    }
    else
    {
      g_message(_("This filter requires a current path with 4 points,"
                "where point 1 and 3 mark reference positions "
                "and point 2 and 4 mark postions in the target layer."
                "It transforms the target layer in a way that "
                "point2 is moved to point1 and point4 moves to point3."
                "(this may include rotate an scale transformation).\n"
                "A path with 2 points can be used to move point2 to point1."
                "(via simple move operation without rotate and scale)"));
    }
  }
 
  gimp_image_undo_group_end (image_id);
  
  return(ret);
  
  
}  /* end  gap_detail_exact_align_via_4point_path */
