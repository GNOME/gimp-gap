/*  gap_blend_fill_main.c
 *    This filter fills the selected area via blending opposite border colors
 *    outside the selction into the selected area.
 *    It was implemented for fixing small pixel defects of my video camera sensor
 *    and is intended to be used as filter when processing video frames
 *    that are shot by such faulty cameras and typically runs
 *    as filtermacro. Therefore the selection can be provided via an external image
 *    or as path vectors via an extrernal SVG file.
 *
 *  2011/11/22
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
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

/* Revision history
 *  (2012/08/27)              hof: use tile cache and skip write for unchanged lines
 *                                 (for performance reasons)
 *  (2011/11/22)  2.7.0       hof: created
 */
int gap_debug = 0;  /* 1 == print debug infos , 0 dont print debug infos */
#define GAP_DEBUG_DECLARED 1

#define PLUG_IN_NAME        "gap-blend-fill"
#define PLUG_IN_BINARY      "gap_blend_fill"
#define PLUG_IN_PRINT_NAME  "Blend Fill"
#define PLUG_IN_IMAGE_TYPES "RGB*"
#define PLUG_IN_AUTHOR      "Wolfgang Hofer (hof@gimp.org)"
#define PLUG_IN_COPYRIGHT   "Wolfgang Hofer"
#define PLUG_IN_HELP_ID     "gap-plug-in-blend-fill"


#include "config.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <gtk/gtk.h>
#include <libgimp/gimp.h>
#include <libgimp/gimpui.h>

#include "gap_libgapbase.h"
#include "gimplastvaldesc.h"
#include "gap_image.h"
#include "gap_arr_dialog.h"

#include "gap-intl.h"

#define SELECTION_FROM_VECTORS -2
#define SELECTION_FROM_SVG_FILE -3

#define ALPHA_NOT_SELECTED              0     /* marks pixel outside the selection */
#define ALPHA_SELECTED_COLOR_UNDEFINED  128   /* marks pixel inside selection, color not (yet) calculated */
#define ALPHA_SELECTED_COLOR_VALID      255   /* marks pixel inside selection, color already calculated */

#define RED_IDX   0          /* index of red   channel in RGBA model */
#define BLU_IDX   1          /* index of green channel in RGBA model */
#define GRN_IDX   2          /* index of blue  channel in RGBA model */
#define ALPHA_IDX 3          /* index of aplha channel in RGBA model */

#define SCALE_MIN_WIDTH 300
#define SPINBUTTON_MIN_WIDTH 50
#define BUTTON_MIN_WIDTH     50
#define MAX_SVG_SIZE     1600

typedef struct FilterVals {
  gboolean               horizontalBlendFlag;
  gboolean               verticalBlendFlag;
  gint32                 altSelection;
  gint32                 borderRadius;
  gchar                  selectionSVGFileName[MAX_SVG_SIZE]; /* contains small xml string or reference to SVG file */

} FilterVals;



typedef struct FilterContext {
  gint32                 imageId;
  gint32                 drawableId;
  gint32                 workLayerId;
  gint                   workLayerWidth;
  gint                   workLayerHeight;
  gint                   workLayerOffsX;
  gint                   workLayerOffsY;

  gint                   origIntersectX;
  gint                   origIntersectY;
  FilterVals             *valPtr;

  gboolean               doProgress;
  gboolean               doFlush;
  gboolean               doClearSelection;
  gdouble                progressStepsDone;
  gdouble                progressStepsTotal;

} FilterContext;




typedef struct BorderPixel {
   gint     pos;
   guchar  *colorRGBAPtr;  /* NULL marks invalid color */
} BorderPixel;

typedef struct GuiStuff {
  gint32      imageId;
  GtkWidget  *ok_button;
  GtkWidget  *msg_label;
  GtkWidget  *svg_entry;
  GtkWidget  *svg_filesel;
  FilterVals *valPtr;
} GuiStuff;


static FilterVals fiVals;




static void  query (void);
static void  run (const gchar *name,          /* name of plugin */
     gint nparams,               /* number of in-paramters */
     const GimpParam * param,    /* in-parameters */
     gint *nreturn_vals,         /* number of out-parameters */
     GimpParam ** return_vals);  /* out-parameters */

static gint32   gap_blend_fill_apply_run(gint32 image_id, gint32 activeDrawableId, gboolean doProgress, gboolean doFlush, FilterVals *fiVals);
static gboolean gap_blend_fill_dialog(FilterVals *fiVals, gint32 drawable_id);


/* Global Variables */
GimpPlugInInfo PLUG_IN_INFO =
{
  NULL,   /* init_proc  */
  NULL,   /* quit_proc  */
  query,  /* query_proc */
  run     /* run_proc   */
};

static const GimpParamDef in_args[] =
{
    { GIMP_PDB_INT32,    "run-mode",      "Interactive, non-interactive" },
    { GIMP_PDB_IMAGE,    "image",         "Input image"                  },
    { GIMP_PDB_DRAWABLE, "drawable",      "Input layer (RGB or RGBA)"               },
    { GIMP_PDB_INT32,    "horizontal",    "0 .. dont blend colors horizontal. "
                                          "1 .. blend colors horizontal." },
    { GIMP_PDB_INT32,    "vertical",      "0 .. dont blend colors vertical. "
                                          "1 .. blend colors vertical." },
    { GIMP_PDB_DRAWABLE, "altSelection",  "-1 use current selection, "
                                          "-2 set selection by loading vectors from XML string (SVG XML string has to be provided in parameter selSVGFile) "
                                          "-3 set selection by loading vectors from SVG file (name has to be provided in parameter selSVGFile) "
                                          "or provide a valid positive drawable id of another layer (in another image) to copy selection from."
                                          "(note: if the image where altSelection layer is part of has no selection, "
                                          " then use a greayscale copy of the altSelection drawable as selection."
                                          " and scale to current image size if necessary." },
    { GIMP_PDB_INT32,    "borderRadius",  "radius for picking border colors (1 to 10)" },
    { GIMP_PDB_STRING,   "selSVGFile",    "optional name of a file that contains the selection as vectors in SVG format. (set altSelection to -2)" }
};


static const GimpParamDef return_vals[] = {
    { GIMP_PDB_DRAWABLE, "drawable",      "unused" }
};

static gint global_number_in_args = G_N_ELEMENTS (in_args);
static gint global_number_out_args = G_N_ELEMENTS (return_vals);





/* Functions */

MAIN ()

static void query (void)
{
  static GimpLastvalDef lastvals[] =
  {
    GIMP_LASTVALDEF_GBOOLEAN        (GIMP_ITER_FALSE,  fiVals.horizontalBlendFlag,     "horizontalBlendFlag"),
    GIMP_LASTVALDEF_GBOOLEAN        (GIMP_ITER_FALSE,  fiVals.verticalBlendFlag,       "verticalBlendFlag"),
    GIMP_LASTVALDEF_DRAWABLE        (GIMP_ITER_TRUE,   fiVals.altSelection,            "altSelection"),
    GIMP_LASTVALDEF_GINT32          (GIMP_ITER_TRUE,   fiVals.borderRadius,            "borderRadius"),
    GIMP_LASTVALDEF_ARRAY           (GIMP_ITER_FALSE,  fiVals.selectionSVGFileName,    "svgFileNameArray"),
    GIMP_LASTVALDEF_GCHAR           (GIMP_ITER_FALSE,  fiVals.selectionSVGFileName[0], "svgFilenameNameChar"),

  };

  /* registration for last values buffer structure (useful for animated filter apply) */
  gimp_lastval_desc_register(PLUG_IN_NAME,
                             &fiVals,
                             sizeof(fiVals),
                             G_N_ELEMENTS (lastvals),
                             lastvals);


  gimp_plugin_domain_register (GETTEXT_PACKAGE, LOCALEDIR);


  /* the actual installation of the plugin */
  gimp_install_procedure (PLUG_IN_NAME,
                          "Fill the selected area via blending border colors outside the selection.",
                          "Fill selected area by blending surrounding colors to cover it. "
                          "The fill area can be represented by the current selection  "
                          "or by an alternative selction (provided as parameter altSelection) "
                          "If the image, that is refered by the altSelection drawable_id has a selection "
                          "then the refered selection is used to identify the fill area. "
                          "otherwise a grayscale copy of the altSelection drawable_id will be used "
                          "to identify the area that shall be filled. "
                          "altSelection value -1 indicates that the current selection of the input image shall be used. "
                          "This plug-in renders colors for all pixels in the selected area "
                          "This is done by blending colors of opposite border pixels outside the selection "
                          "and replaces colors of the selected area by the calculated blend color values. "
                          "The current implementation supports horizontal and/or vertical blend directions. "
                          "This filter is intended to fix small areas in videos shot with a camera that has some defect sensor pixels. "
                          " ",
                          PLUG_IN_AUTHOR,
                          PLUG_IN_COPYRIGHT,
                          GAP_VERSION_WITH_DATE,
                          N_("Blend Fill..."),
                          PLUG_IN_IMAGE_TYPES,
                          GIMP_PLUGIN,
                          global_number_in_args,
                          global_number_out_args,
                          in_args,
                          return_vals);


  {
    /* Menu names */
    const char *menupath_image_layer_tranparency = N_("<Image>/Video/Layer/Enhance/");

    gimp_plugin_menu_register (PLUG_IN_NAME, menupath_image_layer_tranparency);
  }

}  /* end query */

static void
run (const gchar *name,          /* name of plugin */
     gint nparams,               /* number of in-paramters */
     const GimpParam * param,    /* in-parameters */
     gint *nreturn_vals,         /* number of out-parameters */
     GimpParam ** return_vals)   /* out-parameters */
{
  const gchar *l_env;
  gint32       image_id = -1;
  gint32       activeDrawableId = -1;
  gboolean doProgress;
  gboolean doFlush;
  GapLastvalAnimatedCallInfo  animCallInfo;



  /* Get the runmode from the in-parameters */
  GimpRunMode run_mode = param[0].data.d_int32;

  /* status variable, use it to check for errors in invocation usualy only
     during non-interactive calling */
  GimpPDBStatusType status = GIMP_PDB_SUCCESS;

  /* always return at least the status to the caller. */
  static GimpParam values[2];

  INIT_I18N();

  l_env = g_getenv("GAP_DEBUG");
  if(l_env != NULL)
  {
    if((*l_env != 'n') && (*l_env != 'N')) gap_debug = 1;
  }

  if(gap_debug)
  {
    printf("\n\nDEBUG: run %s\n", name);
  }


  doProgress = FALSE;
  doFlush = FALSE;

  /* initialize the return of the status */
  values[0].type = GIMP_PDB_STATUS;
  values[0].data.d_status = status;
  values[1].type = GIMP_PDB_DRAWABLE;
  values[1].data.d_drawable = -1;
  *nreturn_vals = 2;
  *return_vals = values;

  /* init default values */
  fiVals.horizontalBlendFlag = TRUE;
  fiVals.verticalBlendFlag   = TRUE;
  fiVals.altSelection        = SELECTION_FROM_SVG_FILE;
  fiVals.borderRadius        = 3;
  fiVals.selectionSVGFileName[0] = '\0';
  g_snprintf(fiVals.selectionSVGFileName
             , sizeof(fiVals.selectionSVGFileName), "%s"
             , _("selection.svg"));

  /* get image and drawable */
  image_id = param[1].data.d_int32;
  activeDrawableId = param[2].data.d_drawable;

  /* Possibly retrieve data from a previous run */
  gimp_get_data (name, &fiVals);

  /* how are we running today? */
  switch (run_mode)
  {
    gboolean dialogOk;

    dialogOk = FALSE;

    case GIMP_RUN_INTERACTIVE:
      /* Get information from the dialog */
      dialogOk = gap_blend_fill_dialog(&fiVals, activeDrawableId);
      if (!dialogOk)
      {
        /* return without processing in case the Dialog was cancelled */
        return;
      }
      doProgress = TRUE;
      doFlush =  TRUE;

      break;

    case GIMP_RUN_NONINTERACTIVE:
      /* check to see if invoked with the correct number of parameters */
      if (nparams == global_number_in_args)
      {
          fiVals.horizontalBlendFlag     = (param[3].data.d_int32 == 0) ? FALSE : TRUE;
          fiVals.verticalBlendFlag       = (param[4].data.d_int32 == 0) ? FALSE : TRUE;
          fiVals.altSelection  = (gint32)  param[5].data.d_drawable;
          fiVals.borderRadius            = CLAMP(param[6].data.d_int32, 1, 10);

          fiVals.selectionSVGFileName[0] = '\0';
          if(param[7].data.d_string != NULL)
          {
            g_snprintf(fiVals.selectionSVGFileName, MAX_SVG_SIZE -1, "%s", param[7].data.d_string);
          }

      }
      else
      {
        status = GIMP_PDB_CALLING_ERROR;
      }

      break;

    case GIMP_RUN_WITH_LAST_VALS:
      animCallInfo.animatedCallInProgress = FALSE;
      gimp_get_data(GAP_LASTVAL_KEY_ANIMATED_CALL_INFO, &animCallInfo);

      if(animCallInfo.animatedCallInProgress != TRUE)
      {
        doProgress = TRUE;
        doFlush =  TRUE;
      }
      break;

    default:
      break;
  }

  if (status == GIMP_PDB_SUCCESS)
  {
    /* Run the main function */
    values[1].data.d_drawable =
          gap_blend_fill_apply_run(image_id, activeDrawableId, doProgress, doFlush, &fiVals);

    /* Store variable states for next run */
    if (run_mode == GIMP_RUN_INTERACTIVE)
    {
      gimp_set_data (name, &fiVals, sizeof (FilterVals));
    }

    if (values[1].data.d_drawable < 0)
    {
       status = GIMP_PDB_CALLING_ERROR;
    }

    /* If run mode is interactive, flush displays, else (script) don't
     * do it, as the screen updates would make the scripts slow
     */
    if (doFlush)
    {
      gimp_displays_flush ();
    }


  }
  values[0].data.d_status = status;

}       /* end run */



/* ---------------------------------------------- */
/* ---------------------------------------------- */
/* ---------------------------------------------- */
/* ---------------------------------------------- */

/* --------------------------
 * p_to_double
 * --------------------------
 */
static inline gdouble
p_to_double(guchar val)
{
  gdouble dblValue;

  dblValue = val;
  return (dblValue);
}


/* --------------------------
 * p_do_progress_steps
 * --------------------------
 */
static void
p_do_progress_steps(FilterContext *context, gint steps)
{
  if(context->doProgress)
  {
    gdouble currentProgress;

    context->progressStepsDone += steps;
    currentProgress = CLAMP(context->progressStepsDone / context->progressStepsTotal, 0.0, 1.0);
    gimp_progress_update (currentProgress);
  }

}  /* end p_do_progress_steps */


/* --------------------------
 * p_progress_init
 * --------------------------
 */
static void
p_progress_init(FilterContext *context)
{
  if(context->doProgress)
  {
    gdouble areaPixels;

    areaPixels = context->workLayerWidth * context->workLayerHeight;
    context->progressStepsDone = 0.0;

    /* progress steps e.g. pixels to be handled for creating the work layer */
    context->progressStepsTotal = areaPixels;

    if (context->valPtr->horizontalBlendFlag)
    {
      context->progressStepsTotal += areaPixels;
    }
    if (context->valPtr->verticalBlendFlag)
    {
      context->progressStepsTotal += areaPixels;
    }

    gimp_progress_init( _("Blendfill ..."));

  }

}  /* end p_progress_init */


/* -------------------------------------
 * p_mergeValidColors
 * -------------------------------------
 * merge colorRGBA into resultRGBA
 * both are expected as guchar array with 4 elements for the red,green, blue and
 * selection/processing status information in the alpha channel.
 * - in case resultRGBA is still undefined
 *   the specified colorRGBA is copied to resultRGBA without merge.
 * - in case both colors are undefined nothing is done.
 */
static void
p_mergeValidColors(guchar *resultRGBAPtr, guchar *colorRGBAPtr)
{
  if ((colorRGBAPtr == NULL) || (resultRGBAPtr == NULL))
  {
    return;
  }
  if (resultRGBAPtr[ALPHA_IDX] != ALPHA_SELECTED_COLOR_VALID)
  {
    /* resultRGBAPtr is invalid, simply copy colorRGBAPtr because mix would give unwanted results */
    resultRGBAPtr[RED_IDX] = colorRGBAPtr[RED_IDX];
    resultRGBAPtr[BLU_IDX] = colorRGBAPtr[BLU_IDX];
    resultRGBAPtr[GRN_IDX] = colorRGBAPtr[GRN_IDX];
    resultRGBAPtr[ALPHA_IDX] = colorRGBAPtr[ALPHA_IDX];
  }
  else
  {
    gdouble r, g, b;

    /* both resultRGBAPtr and colorRGBAPtr are valid, therefore merge colorRGBAPtr into result */
    r = GAP_BASE_MIX_VALUE(0.5, p_to_double(resultRGBAPtr[0]), p_to_double(colorRGBAPtr[0]));
    g = GAP_BASE_MIX_VALUE(0.5, p_to_double(resultRGBAPtr[1]), p_to_double(colorRGBAPtr[1]));
    b = GAP_BASE_MIX_VALUE(0.5, p_to_double(resultRGBAPtr[2]), p_to_double(colorRGBAPtr[2]));

    resultRGBAPtr[RED_IDX] = CLAMP(r, 0, 255);
    resultRGBAPtr[BLU_IDX] = CLAMP(g, 0, 255);
    resultRGBAPtr[GRN_IDX] = CLAMP(b, 0, 255);
    resultRGBAPtr[ALPHA_IDX] = ALPHA_SELECTED_COLOR_VALID;
  }

}  /* end p_mergeValidColors */


/* -------------------------------------
 * p_mixColorsWeightenedByDistance
 * -------------------------------------
 * mix both border colors weightened by position within selection length
 */
static inline void
p_mixColorsWeightenedByDistance(guchar *colorRGBAPtr
   , BorderPixel *border1, BorderPixel *border2, gint pos)
{
  gdouble r, g, b;
  gdouble mixfactor;
  gdouble selLength;
  gdouble deltaPos;

  selLength = 1 + (border2->pos - border1->pos);
  deltaPos = pos - border1->pos;

  mixfactor = CLAMP(deltaPos / selLength, 0.0, 1.0);

  r = GAP_BASE_MIX_VALUE(mixfactor, p_to_double(border1->colorRGBAPtr[0]), p_to_double(border2->colorRGBAPtr[0]));
  g = GAP_BASE_MIX_VALUE(mixfactor, p_to_double(border1->colorRGBAPtr[1]), p_to_double(border2->colorRGBAPtr[1]));
  b = GAP_BASE_MIX_VALUE(mixfactor, p_to_double(border1->colorRGBAPtr[2]), p_to_double(border2->colorRGBAPtr[2]));

  colorRGBAPtr[RED_IDX] = CLAMP(r, 0, 255);
  colorRGBAPtr[BLU_IDX] = CLAMP(g, 0, 255);
  colorRGBAPtr[GRN_IDX] = CLAMP(b, 0, 255);
  colorRGBAPtr[ALPHA_IDX] = ALPHA_SELECTED_COLOR_VALID;

}  /* end p_mixColorsWeightenedByDistance */


/* -------------------------------------
 * p_copyBorderColor
 * -------------------------------------
 */
static inline void
p_copyBorderColor(guchar *colorRGBA, BorderPixel *border)
{
  colorRGBA[RED_IDX] = border->colorRGBAPtr[RED_IDX];
  colorRGBA[BLU_IDX] = border->colorRGBAPtr[BLU_IDX];
  colorRGBA[GRN_IDX] = border->colorRGBAPtr[GRN_IDX];
  colorRGBA[ALPHA_IDX] = ALPHA_SELECTED_COLOR_VALID;

}  /* end p_copyBorderColor */



/* -------------------------------------
 * p_color_processing_one_pixel
 * -------------------------------------
 */
static void
p_color_processing_one_pixel(guchar *resultRGBAPtr, BorderPixel *border1, BorderPixel *border2, gint pos)
{
  guchar colorRGBA[4];
  guchar *colorRGBAPtr;

  if (resultRGBAPtr == NULL)
  {
    return;
  }

  colorRGBAPtr = NULL;

  if (border1->colorRGBAPtr)
  {
    colorRGBAPtr = &colorRGBA[0];
    if (border2->colorRGBAPtr)
    {
      p_mixColorsWeightenedByDistance(colorRGBAPtr,  border1, border2, pos);
    }
    else
    {
      p_copyBorderColor(colorRGBAPtr, border1);
    }
  }
  else if (border2->colorRGBAPtr)
  {
      colorRGBAPtr = &colorRGBA[0];
      p_copyBorderColor(colorRGBAPtr, border2);
  }
  p_mergeValidColors(resultRGBAPtr, colorRGBAPtr);

}  /* end p_color_processing_one_pixel */


/* ----------------------------------------
 * p_mix_border_colors
 * ----------------------------------------
 * mix unselected neighbour border colors up to specified borderRadius
 * in case there is only one border pixel available, the border->colorRGBAPtr
 * is kept unchanged an no mix can be done.
 * if there are 2 or more unselected neighbour border pixels available
 * the border colors are mixed into mixRGBAPtr and this resulting mixed color
 * is set as new border->colorRGBAPtr.
 * The mix is done 50:50 in a loop, therefore the nearest border pixel is processed
 * as the last one and has typically more weight than the farest border pixel.
 */
static void
p_mix_border_colors(guchar *mixRGBAPtr, guchar *pixLine, gint lineLength
  , BorderPixel *border, FilterContext *context, gint step)
{
  gint startPos;
  gint pos;
  gint idx;
  gint radius;

  if (border->colorRGBAPtr == NULL)
  {
    return;
  }

  radius = 1;
  startPos = pos;
  for (pos = border->pos + step; ((pos < lineLength) && (pos >= 0)); pos += step)
  {
    idx = pos * 4;
    if (pixLine[idx + ALPHA_IDX] != ALPHA_NOT_SELECTED)
    {
      break;
    }
    radius++;
    startPos = pos;
    if(radius >= context->valPtr->borderRadius)
    {
      break;
    }
  }

  if (radius > 1)
  {
    gint cnt;

    pos = startPos;
    idx = pos * 4;

    mixRGBAPtr[RED_IDX] = pixLine[idx + RED_IDX];
    mixRGBAPtr[BLU_IDX] = pixLine[idx + BLU_IDX];
    mixRGBAPtr[GRN_IDX] = pixLine[idx + GRN_IDX];
    mixRGBAPtr[ALPHA_IDX] = ALPHA_SELECTED_COLOR_VALID;


    for (cnt = 1; cnt < radius; cnt++)
    {
      pos -= step;
      idx = pos * 4;

      p_mergeValidColors(mixRGBAPtr, &pixLine[idx]);


    }
    border->colorRGBAPtr = mixRGBAPtr;

  }

}  /* end p_mix_border_colors */


/* ----------------------------------------
 * p_color_blend_pixel_line
 * ----------------------------------------
 * pixLine input is expected as RGBA8 pixel row (or column)
 * where the Alpha channel byte is used as selection and processing state information
 * (and does not represent transparency as normal RGBA colormodel would)
 * The algorithm iterates along the line and fills all selected pixels
 * (identified by Alpha != ALPHA_NOT_SELECTED) with color blend from
 * last border pixel color to next border pixel.
 * The original color of the pixels is ignored in case the pixel has
 * the state ALPHA_SELECTED_COLOR_UNDEFINED but the state is set
 * to ALPHA_SELECTED_COLOR_VALID when the new color is assigned to the pixel.
 *
 * If only one border pixel was found, all its selected neighbour pixels
 * ill be filled with the border color without blending.
 * If there is NO unselected border pixel at all, the selected pixel keeps
 * its original color.
 *
 * in case the processed SELECTED pixel has a already reached state ALPHA_SELECTED_COLOR_VALID
 * it is mixed 50:50 with the calculated blend (or border) color value.
 * (this will occure when both horizontal and vertical blend is applied)
 *
 * return TRUE in case the processing has changedAnyPixel
 */
static gboolean
p_color_blend_pixel_line(guchar *pixLine, gint lineLength, FilterContext *context)
{
  gint pos;
  gint idx;
  BorderPixel border1;
  BorderPixel border2;
  gboolean    insideSelecton;
  gboolean    changedAnyPixel;
  guchar      mix1RGBA[4];
  guchar      mix2RGBA[4];

  border1.colorRGBAPtr = NULL;
  border2.colorRGBAPtr = NULL;
  insideSelecton = FALSE;
  changedAnyPixel = FALSE;
  idx = 0;
  for (pos = 0; pos < lineLength; pos++)
  {
    if (pixLine[idx + ALPHA_IDX] == ALPHA_NOT_SELECTED)
    {
      insideSelecton = FALSE;
      border1.pos = pos;
      border1.colorRGBAPtr = &pixLine[idx];
      border2.colorRGBAPtr = NULL;
    }
    else
    {
      if (insideSelecton != TRUE)
      {
        gint pos2;
        gint idx2;
        insideSelecton = TRUE;
        changedAnyPixel = TRUE;

        /* find (border2) the next pixel that is outside the selection */
        idx2 = idx +4;
        for (pos2 = pos +1; pos2 < lineLength; pos2++)
        {
          if (pixLine[idx2 + ALPHA_IDX] == ALPHA_NOT_SELECTED)
          {
            border2.pos = pos2;
            border2.colorRGBAPtr = &pixLine[idx2];
            break;
          }
          idx2 += 4;
        }

        if (context->valPtr->borderRadius > 1)
        {
          p_mix_border_colors(&mix1RGBA[0], pixLine, lineLength, &border1, context, -1);
          p_mix_border_colors(&mix2RGBA[0], pixLine, lineLength, &border2, context, 1);
        }

      }
      p_color_processing_one_pixel(&pixLine[idx], &border1, &border2, pos);
    }

    idx += 4;
  }

  p_do_progress_steps(context, lineLength);

  return (changedAnyPixel);

}  /* end p_color_blend_pixel_line */


/* ----------------------------------------
 * p_horizontal_color_blend
 * ----------------------------------------
 */
static void
p_horizontal_color_blend(FilterContext *context)
{
  GimpPixelRgn  workPR;
  GimpDrawable *workDrawable;
  guchar       *pixLine;
  gint          row;

  workDrawable = gimp_drawable_get (context->workLayerId);
  gimp_pixel_rgn_init (&workPR, workDrawable
                      , 0, 0
                      , context->workLayerWidth, context->workLayerHeight
                      , TRUE      /* dirty */
                      , FALSE    /* shadow */
                      );



  /*  allocate buffer for one horizontal pixel line  */
  pixLine = g_new (guchar, context->workLayerWidth * workDrawable->bpp);

  for (row = 0; row < context->workLayerHeight; row++)
  {
    gboolean      changedAnyPixel;

    gimp_pixel_rgn_get_row (&workPR, pixLine, 0, row, context->workLayerWidth);
    changedAnyPixel = p_color_blend_pixel_line(pixLine, context->workLayerWidth, context);
    if (changedAnyPixel)
    {
      gimp_pixel_rgn_set_row (&workPR, pixLine, 0, row, context->workLayerWidth);
    }

  }

  gimp_drawable_flush (workDrawable);
  gimp_drawable_update (context->workLayerId, 0, 0, context->workLayerWidth, context->workLayerHeight);
  gimp_drawable_detach(workDrawable);

  g_free (pixLine);

}  /* end p_horizontal_color_blend */


/* ----------------------------------------
 * p_vertical_color_blend
 * ----------------------------------------
 */
static void
p_vertical_color_blend(FilterContext *context)
{
  GimpPixelRgn  workPR;
  GimpDrawable *workDrawable;
  guchar       *pixLine;
  gint          col;

  workDrawable = gimp_drawable_get (context->workLayerId);
  gimp_pixel_rgn_init (&workPR, workDrawable
                      , 0, 0
                      , context->workLayerWidth, context->workLayerHeight
                      , TRUE      /* dirty */
                      , FALSE    /* shadow */
                      );



  /*  allocate buffer for one vertical pixel line  */
  pixLine = g_new (guchar, context->workLayerHeight * workDrawable->bpp);

  for (col = 0; col < context->workLayerWidth; col++)
  {
    gboolean      changedAnyPixel;

    gimp_pixel_rgn_get_col (&workPR, pixLine, col, 0, context->workLayerHeight);
    changedAnyPixel = p_color_blend_pixel_line(pixLine, context->workLayerHeight, context);
    if (changedAnyPixel)
    {
      gimp_pixel_rgn_set_col (&workPR, pixLine, col, 0, context->workLayerHeight);
    }

  }

  gimp_drawable_flush (workDrawable);
  gimp_drawable_update (context->workLayerId, 0, 0, context->workLayerWidth, context->workLayerHeight);
  gimp_drawable_detach(workDrawable);

  g_free (pixLine);

}  /* end p_vertical_color_blend */






/* --------------------------
 * p_set_altSelection
 * --------------------------
 * create selection as Grayscale copy of the specified altSelection layer
 *  - operates on a duplicate of the image references by altSelection
 *  - this duplicate is scaled to same size as specified image_id
 *
 * - if altSelection refers to an image that has a selection
 *   then use this selection instead of the layer itself.
 */
static gboolean
p_set_altSelection(FilterContext *context)
{
  gboolean l_rc;
  if(gap_debug)
  {
    printf("p_set_altSelection: drawable_id:%d  altSelection:%d\n"
      ,(int)context->drawableId
      ,(int)context->valPtr->altSelection
      );
  }

  if ((context->drawableId == context->valPtr->altSelection) || (context->drawableId < 0))
  {
    return (FALSE);
  }
  context->doClearSelection = TRUE;
  l_rc = gap_image_set_selection_from_selection_or_drawable(context->imageId
                                                          , context->valPtr->altSelection
                                                          , FALSE);

  return (l_rc);
}



/* ---------------------------------
 * p_rgn_render_init_workregion
 * ---------------------------------
 */
static void
p_rgn_render_init_workregion (const GimpPixelRgn *origPR
                    , const GimpPixelRgn *workPR
                    , const GimpPixelRgn *selPR
                    , FilterContext *context)
{
  guint    row;
  guchar* orig  = origPR->data;
  guchar* work  = workPR->data;
  guchar* sel   = selPR->data;


  if(gap_debug)
  {
    printf("p_rgn_render_init_workregion  orig W:%d H:%d  work W:%d H:%d  sel W:%d H:%d\n"
       ,(int)origPR->w
       ,(int)origPR->h
       ,(int)workPR->w
       ,(int)workPR->h
       ,(int)selPR->w
       ,(int)selPR->h
       );
  }


  for (row = 0; row < MIN(origPR->h, workPR->h); row++)
  {
    guint   col;
    guint   origIdx = 0;
    guint   workIdx = 0;
    guint   selIdx = 0;

    for (col = 0; col < MIN(origPR->w, workPR->w); col++)
    {
        work[workIdx]    = orig[origIdx];
        work[workIdx +1] = orig[origIdx +1];
        work[workIdx +2] = orig[origIdx +2];

        work[workIdx + ALPHA_IDX] = ALPHA_NOT_SELECTED;
        if (col < selPR->w)
        {
          if (sel[selIdx] != 0)
          {
            work[workIdx + ALPHA_IDX] = ALPHA_SELECTED_COLOR_UNDEFINED;
          }
        }

        origIdx += origPR->bpp;
        workIdx += workPR->bpp;
        selIdx  += selPR->bpp;
    }

    orig  += origPR->rowstride;
    work  += workPR->rowstride;
    sel   += selPR->rowstride;
  }

  p_do_progress_steps(context, workPR->h * workPR->w);

}  /* end p_rgn_render_init_workregion */


/* ----------------------------------
 * p_set_selection_from_vectors_string
 * ----------------------------------
 * interpret the selectionSVGFileName buffer as XML svg content
 * and load all path vectors from this string.
 * (on errors keep current selection)
 */
static void
p_set_selection_from_vectors_string(FilterContext *context)
{
  gboolean vectorsOk;
  gint     num_vectors;
  gint32  *vectors_ids;

  vectorsOk = FALSE;
  if (context->valPtr->selectionSVGFileName != '\0')
  {
    gint length;

    length = strlen(context->valPtr->selectionSVGFileName);
    vectorsOk = gimp_vectors_import_from_string (context->imageId
                                                ,context->valPtr->selectionSVGFileName
                                                ,length
                                                , TRUE  /* Merge paths into a single vectors object. */
                                                , TRUE  /* Scale the SVG to image dimensions. */
                                                , &num_vectors
                                                , &vectors_ids
                                                );
  }

  if ((vectorsOk) && (vectors_ids != NULL) && (num_vectors > 0))
  {
    gboolean       selOk;
    gint32         vectorId;
    GimpChannelOps operation;

    vectorId = vectors_ids[0];
    operation = GIMP_CHANNEL_OP_REPLACE;
    selOk = gimp_vectors_to_selection(vectorId
                                     , operation
                                     , FALSE      /*  antialias */
                                     , FALSE      /*  feather   */
                                     , 0.0        /*  gdouble feather_radius_x */
                                     , 0.0        /*  gdouble feather_radius_y */
                                     );
    gimp_image_remove_vectors(context->imageId, vectorId);
    context->doClearSelection = TRUE;
  }

}  /* end p_set_selection_from_vectors_string */



/* ----------------------------------
 * p_set_selection_from_vectors_file
 * ----------------------------------
 * import selection from an SVG vectors file
 * and replace the current selection on success.
 * (on errors keep current selection)
 */
static void
p_set_selection_from_vectors_file(FilterContext *context)
{
  gboolean vectorsOk;
  gint     num_vectors;
  gint32  *vectors_ids;

  vectorsOk = FALSE;
  if (context->valPtr->selectionSVGFileName != '\0')
  {
    if(g_file_test(context->valPtr->selectionSVGFileName, G_FILE_TEST_EXISTS))
    {
      vectorsOk = gimp_vectors_import_from_file   (context->imageId
                                                  ,context->valPtr->selectionSVGFileName
                                                  , TRUE  /* Merge paths into a single vectors object. */
                                                  , TRUE  /* Scale the SVG to image dimensions. */
                                                  , &num_vectors
                                                  , &vectors_ids
                                                  );
    }
  }


  if ((vectorsOk) && (vectors_ids != NULL) && (num_vectors > 0))
  {
    gboolean       selOk;
    gint32         vectorId;
    GimpChannelOps operation;

    vectorId = vectors_ids[0];
    operation = GIMP_CHANNEL_OP_REPLACE;
    selOk = gimp_vectors_to_selection(vectorId
                                     , operation
                                     , FALSE      /*  antialias */
                                     , FALSE      /*  feather   */
                                     , 0.0        /*  gdouble feather_radius_x */
                                     , 0.0        /*  gdouble feather_radius_y */
                                     );
    gimp_image_remove_vectors(context->imageId, vectorId);
    context->doClearSelection = TRUE;
  }

}  /* end p_set_selection_from_vectors_file */



/* --------------------------
 * p_render_initial_workLayer
 * --------------------------
 * Creates a worklayer in size of the interection of
 * the current selection (expanded by borderRadius) and the input Layer (origDrawable).
 * The worklayer has type RGBA, but the Alpha channel byte is not used for transparency.
 * It represents selection and processing state information that is initalized with:
 *    ALPHA_NOT_SELECTED                for all pixels outside the current (unexpanded) selection
 *    ALPHA_SELECTED_COLOR_UNDEFINED    for all pixels outside the current (unexpanded) selection
 * Note that this implicite sharpens the selection, e.g. weak selected
 * pixels are considered as selected and will be processed too.
 */
static void
p_render_initial_workLayer(FilterContext *context)
{
  GimpPixelRgn origPR, workPR, selPR;
  GimpDrawable *origDrawable;
  GimpDrawable *workDrawable;
  GimpDrawable *selDrawable;
  gpointer  pr;

  origDrawable = gimp_drawable_get (context->drawableId);
  workDrawable = gimp_drawable_get (context->workLayerId);
  selDrawable  = gimp_drawable_get (gimp_image_get_selection(context->imageId));

  gimp_pixel_rgn_init (&origPR, origDrawable
                      , context->origIntersectX, context->origIntersectY
                      , context->workLayerWidth, context->workLayerHeight
                      , FALSE    /* dirty */
                      , FALSE    /* shadow */
                      );

  gimp_pixel_rgn_init (&workPR, workDrawable
                      , 0, 0
                      , context->workLayerWidth, context->workLayerHeight
                      , TRUE      /* dirty */
                      , FALSE    /* shadow */
                      );
  gimp_pixel_rgn_init (&selPR, selDrawable
                      , context->workLayerOffsX, context->workLayerOffsY
                      , context->workLayerWidth, context->workLayerHeight
                      , FALSE     /* dirty */
                      , FALSE     /* shadow */
                       );

  for (pr = gimp_pixel_rgns_register (3, &origPR, &workPR, &selPR);
       pr != NULL;
       pr = gimp_pixel_rgns_process (pr))
  {
      p_rgn_render_init_workregion (&origPR, &workPR, &selPR, context);
  }

  /*  update the processed region  */
  gimp_drawable_flush (workDrawable);

  gimp_drawable_update (context->workLayerId, 0, 0, context->workLayerWidth, context->workLayerHeight);

  gimp_drawable_detach(origDrawable);
  gimp_drawable_detach(workDrawable);
  gimp_drawable_detach(selDrawable);

}  /* end p_render_initial_workLayer */






/* ----------------------------------------
 * p_create_workLayer
 * ----------------------------------------
 * create the workLayer as copy of the drawable rectangle area
 * that intersects with the selection expanded by borderRadius and clipped
 * to drawable boundaries.
 * The alpha channel is copied from the selection and the rgb channels
 * are copied from the drawable.
 */
static void
p_create_workLayer(FilterContext *context)
{
  gboolean has_selection;
  gboolean non_empty;
  gboolean altSelection_success;
  gint     x1, y1, x2, y2;
  gint     ix, iy, ix1, iy1, ix2, iy2;
  gint     iWidth, iHeight;
  gint     borderRadius;


  altSelection_success = FALSE;
  borderRadius = context->valPtr->borderRadius;

  if(context->valPtr->altSelection >= 0)
  {
    if(gap_debug)
    {
      printf("creating altSelection: %d\n", (int)context->valPtr->altSelection);
    }
    altSelection_success = p_set_altSelection(context);
  }

  if(context->valPtr->altSelection == SELECTION_FROM_VECTORS)
  {
    p_set_selection_from_vectors_string(context);
  }
  else if(context->valPtr->altSelection == SELECTION_FROM_SVG_FILE)
  {
    p_set_selection_from_vectors_file(context);
  }

  has_selection  = gimp_selection_bounds(context->imageId, &non_empty, &x1, &y1, &x2, &y2);

  context->workLayerId = -1;
  if (non_empty != TRUE)
  {
    return;
  }

  non_empty = gimp_drawable_mask_intersect(context->drawableId, &ix, &iy, &iWidth, &iHeight);
  if((non_empty != TRUE)
  || (iWidth <= 0)
  || (iHeight <= 0))
  {
    return;
  }

  gimp_drawable_offsets(context->drawableId
                       ,&context->workLayerOffsX
                       ,&context->workLayerOffsY
                       );

  ix2 = MIN(gimp_drawable_width(context->drawableId), ix + iWidth + borderRadius);
  iy2 = MIN(gimp_drawable_height(context->drawableId), iy + iHeight + borderRadius);

  ix1 = MAX(0, ix - borderRadius);
  iy1 = MAX(0, iy - borderRadius);

  context->workLayerOffsX += ix1;
  context->workLayerOffsY += iy1;
  context->workLayerWidth = ix2 - ix1;
  context->workLayerHeight = iy2 - iy1;

  context->origIntersectX = ix1;
  context->origIntersectY = iy1;

  if(gap_debug)
  {
    printf("intersect ix:%d iy:%d iWidth:%d iHeight:%d  ix1:%d iy1:%d ix2:%d iy2:%d\n"
       ,(int)ix
       ,(int)iy
       ,(int)iWidth
       ,(int)iHeight
       ,(int)ix1
       ,(int)iy1
       ,(int)ix2
       ,(int)iy2
       );
    printf("workLayerOffsX:%d workLayerOffsY:%d workLayerWidth:%d workLayerHeight:%d\n"
       ,(int)context->workLayerOffsX
       ,(int)context->workLayerOffsY
       ,(int)context->workLayerWidth
       ,(int)context->workLayerHeight
       );
  }

  p_progress_init(context);

  context->workLayerId = gimp_layer_new(context->imageId
                , "work"
                , context->workLayerWidth
                , context->workLayerHeight
                , GIMP_RGBA_IMAGE
                , 100.0   /* full opacity */
                , 0       /* normal mode */
                );
  gimp_image_add_layer(context->imageId, context->workLayerId, 0);
  gimp_layer_set_offsets(context->workLayerId
                        , context->workLayerOffsX
                        , context->workLayerOffsY
                        );

  p_render_initial_workLayer(context);

}  /* end p_create_workLayer */



/* ----------------------------------------
 * p_set_tile_cache
 * ----------------------------------------
 *
 */
static void
p_set_tile_cache(FilterContext *context)
{
    gulong cache_ntiles;
    gulong regionTileWidth;
    gulong regionTileHeight;

    regionTileWidth = 1 + (context->workLayerWidth / gimp_tile_width()) ;
    regionTileHeight = 1 + (context->workLayerHeight / gimp_tile_height()) ;

    cache_ntiles = regionTileWidth * regionTileHeight;

    gimp_tile_cache_ntiles (CLAMP(cache_ntiles, 32, 300));

}  /* end p_set_tile_cache */


/* ----------------------------------
 * gap_blend_fill_apply_run
 * ----------------------------------
 */
static gint32
gap_blend_fill_apply_run(gint32 image_id, gint32 activeDrawableId, gboolean doProgress, gboolean doFlush, FilterVals *fiVals)
{
  FilterContext  filterContext;
  FilterContext *context;
  gint32         rc;

  gimp_image_undo_group_start (image_id);

  /* init the context */
  context = &filterContext;
  context->doProgress = doProgress;
  context->doFlush = doFlush;
  context->doClearSelection = FALSE;
  context->imageId = image_id;
  context->drawableId = activeDrawableId;
  context->workLayerId = -1;
  context->valPtr = fiVals;
  fiVals->borderRadius = CLAMP(fiVals->borderRadius, 1, 10);

  rc = -1;

  if(gap_debug)
  {
    printf("context Svg:%s\n", context->valPtr->selectionSVGFileName);
  }

  p_create_workLayer(context);
  if (context->workLayerId >= 0)
  {
    p_set_tile_cache(context);

    if (context->valPtr->horizontalBlendFlag)
    {
      p_horizontal_color_blend(context);
    }

    if (context->valPtr->verticalBlendFlag)
    {
      p_vertical_color_blend(context);
    }

    rc = gimp_image_merge_down(image_id, context->workLayerId, GIMP_EXPAND_AS_NECESSARY);

    if(context->doClearSelection)
    {
      gimp_selection_none(context->imageId);
    }

  }

  gimp_image_undo_group_end (image_id);
  return (rc);
}  /* end gap_blend_fill_apply_run */



/* --------------------------- DIALOG stuff ------------------- */
/* --------------------------- DIALOG stuff ------------------- */
/* --------------------------- DIALOG stuff ------------------- */
/* --------------------------- DIALOG stuff ------------------- */
/* --------------------------- DIALOG stuff ------------------- */
/* --------------------------- DIALOG stuff ------------------- */
/* --------------------------- DIALOG stuff ------------------- */



/* --------------------------------------
 * on_gboolean_button_update
 * --------------------------------------
 */
static void
on_gboolean_button_update (GtkWidget *widget,
                           gpointer   data)
{
  gint *toggle_val = (gint *) data;

  if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget)))
  {
    *toggle_val = TRUE;
  }
  else
  {
    *toggle_val = FALSE;
  }
}


/* ----------------------------
 * p_selectionConstraintFunc
 * ----------------------------
 *
 */
static gint
p_selectionConstraintFunc (gint32   image_id,
               gint32   drawable_id,
               gpointer data)
{
  if (image_id < 0)
  {
    return FALSE;
  }

  /* dont accept layers from indexed images */
  if (gimp_drawable_is_indexed (drawable_id))
  {
    return FALSE;
  }

  return TRUE;
}  /* end p_selectionConstraintFunc */


/* --------------------------------------------
 * p_check_exec_condition_and_set_ok_sesitivity
 * --------------------------------------------
 */
static void
p_check_exec_condition_and_set_ok_sesitivity(GuiStuff *guiStuffPtr)
{
  gboolean okButtonSensitive;

  okButtonSensitive = TRUE;

  if ((guiStuffPtr->valPtr == NULL)
  ||  (guiStuffPtr->msg_label == NULL))
  {
    return;
  }
  gtk_label_set_text(GTK_LABEL(guiStuffPtr->msg_label), "");

  if (guiStuffPtr->valPtr->altSelection == SELECTION_FROM_VECTORS)
  {
    gchar   *svgString;
    gint    *allVectors;
    gint     num_vectors;

    svgString = NULL;
    allVectors = gimp_image_get_vectors(guiStuffPtr->imageId, &num_vectors);
    if(allVectors != NULL)
    {
      g_free(allVectors);
    }
    if(num_vectors > 0)
    {
      svgString = gimp_vectors_export_to_string (guiStuffPtr->imageId
                                                 , 0  /* vectors_ID for all vectors merged */
                                                );
    }

    if (svgString != NULL)
    {
      gint     length;

      length = strlen(svgString);
      if(length >= sizeof(guiStuffPtr->valPtr->selectionSVGFileName))
      {
        gchar *msg;

        msg = g_strdup_printf(_("Path Vectors too large to fit into buffersize:%d.")
                              , sizeof(guiStuffPtr->valPtr->selectionSVGFileName));
        gtk_label_set_text(GTK_LABEL(guiStuffPtr->msg_label), msg);
        g_free(msg);
        okButtonSensitive = FALSE;
      }
      g_free(svgString);
    }
    else
    {
      gtk_label_set_text(GTK_LABEL(guiStuffPtr->msg_label)
                        , _("No Path Vectors available."));
      okButtonSensitive = FALSE;
    }
  }

  if (guiStuffPtr->valPtr->altSelection == SELECTION_FROM_SVG_FILE)
  {
    okButtonSensitive = FALSE;
    if (guiStuffPtr->valPtr->selectionSVGFileName[0] != '\0')
    {
      if(g_file_test(guiStuffPtr->valPtr->selectionSVGFileName, G_FILE_TEST_EXISTS))
      {
        okButtonSensitive = TRUE;
      }
      else
      {
        gtk_label_set_text(GTK_LABEL(guiStuffPtr->msg_label)
                        , _("SVG file does not exist (use Save Pats button to create)."));
      }
    }
    else
    {
        gtk_label_set_text(GTK_LABEL(guiStuffPtr->msg_label)
                        , _("please enter SVG filename"));
    }
  }

  if(guiStuffPtr->ok_button)
  {
    gtk_widget_set_sensitive(guiStuffPtr->ok_button, okButtonSensitive);

  }

}  /* end p_check_exec_condition_and_set_ok_sesitivity */

/* ----------------------------
 * p_selectionComboCallback
 * ----------------------------
 *
 */
static void
p_selectionComboCallback (GtkWidget *widget, gint32 *layerId)
{
  gint value;

  gimp_int_combo_box_get_active (GIMP_INT_COMBO_BOX (widget), &value);

  if(gap_debug)
  {
    printf("p_selectionComboCallback: LayerAddr:%d value:%d\n"
      ,(int)layerId
      ,(int)value
      );
  }

  if(layerId != NULL)
  {
    GuiStuff *guiStuffPtr;

    *layerId = value;

    guiStuffPtr = (GuiStuff *) g_object_get_data (G_OBJECT (widget), "guiStuffPtr");
    if(guiStuffPtr != NULL)
    {
      p_check_exec_condition_and_set_ok_sesitivity(guiStuffPtr);
    }
  }

}  /* end p_selectionComboCallback */





/* ---------------------------------
 * svg fileselct callbacks
 * ---------------------------------
 */

static void
on_svg_filesel_destroy          (GtkObject *object,
                                 GuiStuff  *guiStuffPtr)
{
 if(gap_debug) printf("CB: on_svg_filesel_destroy\n");
 if(guiStuffPtr == NULL) return;

 guiStuffPtr->svg_filesel = NULL;
}

static void
on_svg__button_cancel_clicked          (GtkButton *button,
                                        GuiStuff  *guiStuffPtr)
{
 if(gap_debug) printf("CB: on_svg__button_cancel_clicked\n");
 if(guiStuffPtr == NULL) return;

 if(guiStuffPtr->svg_filesel)
 {
   gtk_widget_destroy(guiStuffPtr->svg_filesel);
   guiStuffPtr->svg_filesel = NULL;
 }
}

static void
on_svg__button_OK_clicked       (GtkButton *button,
                                 GuiStuff  *guiStuffPtr)
{
  const gchar *filename;
  GtkEntry *entry;

 if(gap_debug) printf("CB: on_svg__button_OK_clicked\n");
 if(guiStuffPtr == NULL) return;

 if(guiStuffPtr->svg_filesel)
 {
   filename =  gtk_file_selection_get_filename (GTK_FILE_SELECTION (guiStuffPtr->svg_filesel));
   g_snprintf(guiStuffPtr->valPtr->selectionSVGFileName
             , sizeof(guiStuffPtr->valPtr->selectionSVGFileName), "%s"
             , filename);
   entry = GTK_ENTRY(guiStuffPtr->svg_entry);
   if(entry)
   {
      gtk_entry_set_text(entry, filename);
   }
   on_svg__button_cancel_clicked(NULL, (gpointer)guiStuffPtr);
 }
}




/* ----------------------------------------
 * p_create_fileselection
 * ----------------------------------------
 */
static GtkWidget*
p_create_fileselection (GuiStuff *guiStuffPtr)
{
  GtkWidget *svg_filesel;
  GtkWidget *svg__button_OK;
  GtkWidget *svg__button_cancel;

  svg_filesel = gtk_file_selection_new (_("Select vectorfile name"));
  gtk_container_set_border_width (GTK_CONTAINER (svg_filesel), 10);

  svg__button_OK = GTK_FILE_SELECTION (svg_filesel)->ok_button;
  gtk_widget_show (svg__button_OK);
  GTK_WIDGET_SET_FLAGS (svg__button_OK, GTK_CAN_DEFAULT);

  svg__button_cancel = GTK_FILE_SELECTION (svg_filesel)->cancel_button;
  gtk_widget_show (svg__button_cancel);
  GTK_WIDGET_SET_FLAGS (svg__button_cancel, GTK_CAN_DEFAULT);

  g_signal_connect (G_OBJECT (svg_filesel), "destroy",
                      G_CALLBACK (on_svg_filesel_destroy),
                      guiStuffPtr);
  g_signal_connect (G_OBJECT (svg__button_OK), "clicked",
                      G_CALLBACK (on_svg__button_OK_clicked),
                      guiStuffPtr);
  g_signal_connect (G_OBJECT (svg__button_cancel), "clicked",
                      G_CALLBACK (on_svg__button_cancel_clicked),
                      guiStuffPtr);

  gtk_widget_grab_default (svg__button_cancel);
  return svg_filesel;
}  /* end p_create_fileselection */



/* ---------------------------------
 * on_svg_entry_changed
 * ---------------------------------
 */
static void
on_svg_entry_changed              (GtkEditable     *editable,
                                   GuiStuff *guiStuffPtr)
{
  GtkEntry *entry;

 if(gap_debug)
 {
   printf("CB: on_svg_entry_changed\n");
 }
 if(guiStuffPtr == NULL)
 {
   return;
 }

 entry = GTK_ENTRY(guiStuffPtr->svg_entry);
 if(entry)
 {
    g_snprintf(guiStuffPtr->valPtr->selectionSVGFileName
              , sizeof(guiStuffPtr->valPtr->selectionSVGFileName), "%s"
              , gtk_entry_get_text(entry));
    p_check_exec_condition_and_set_ok_sesitivity(guiStuffPtr);
 }
}

/* ---------------------------------
 * on_filesel_button_clicked
 * ---------------------------------
 */
static void
on_filesel_button_clicked             (GtkButton *button,
                                       GuiStuff  *guiStuffPtr)
{
 if(gap_debug)
 {
   printf("CB: on_filesel_button_clicked\n");
 }
 if(guiStuffPtr == NULL)
 {
   return;
 }

 if(guiStuffPtr->svg_filesel == NULL)
 {
   guiStuffPtr->svg_filesel = p_create_fileselection(guiStuffPtr);
   gtk_file_selection_set_filename (GTK_FILE_SELECTION (guiStuffPtr->svg_filesel),
                                    guiStuffPtr->valPtr->selectionSVGFileName);

   gtk_widget_show (guiStuffPtr->svg_filesel);
 }

}  /* end on_filesel_button_clicked */


/* ---------------------------------
 * on_save_svg_clicked
 * ---------------------------------
 */
static void
on_save_svg_clicked(GtkButton  *button,
                    GuiStuff   *guiStuffPtr)
{
  if (guiStuffPtr->valPtr->selectionSVGFileName != '\0')
  {
    gboolean l_wr_permission;

    l_wr_permission = gap_arr_overwrite_file_dialog(guiStuffPtr->valPtr->selectionSVGFileName);

    if(l_wr_permission)
    {
      gboolean svgExportOk;
      gint32   vectors_ID;

      vectors_ID = 0;  /* 0 refers to all vectors in the image */
      svgExportOk = gimp_vectors_export_to_file(guiStuffPtr->imageId
                                          , guiStuffPtr->valPtr->selectionSVGFileName
                                          , vectors_ID
                                          );
      if (svgExportOk != TRUE)
      {
        g_message(_("Failed to write SVG file: %s")
                 , guiStuffPtr->valPtr->selectionSVGFileName);
      }
    }
  }
}  /* end on_save_svg_clicked */




/* ----------------------------------
 * p_save_vectors_to_string
 * ----------------------------------
 */
static void
p_save_vectors_to_string(GuiStuff *guiStuffPtr)
{
  gchar   *svgString;

  svgString = gimp_vectors_export_to_string (guiStuffPtr->imageId, 0  /* vectors_ID for all vectors merged */);
  if (svgString != NULL)
  {
    gint     length;

    if(gap_debug)
    {
      printf("svgString:%s\n", svgString);
    }
    length = strlen(svgString);
    if(length < sizeof(guiStuffPtr->valPtr->selectionSVGFileName))
    {
        g_snprintf(guiStuffPtr->valPtr->selectionSVGFileName
             , sizeof(guiStuffPtr->valPtr->selectionSVGFileName), "%s"
             , svgString);

    }
    else
    {
        g_message(_("Path Vectors too large to fit into buffersize:%d.")
                 , sizeof(guiStuffPtr->valPtr->selectionSVGFileName));
    }
    g_free(svgString);
  }
  else
  {
      g_message(_("No Path Vectors available."));
  }

}  /* end p_save_vectors_to_string */


/* --------------------------
 * gap_blend_fill_dialog
 * --------------------------
 */
static gboolean
gap_blend_fill_dialog (FilterVals *fiVals, gint32 drawable_id)
{
  GuiStuff guiStuffRecord;
  GuiStuff *guiStuffPtr;
  GtkWidget *dialog;
  GtkWidget *main_vbox;
  GtkWidget *label;
  GtkWidget *button;
  GtkWidget *table;
  GtkWidget *combo;
  GtkWidget *entry;
  GtkWidget *checkbutton;
  GtkObject *adj;
  gint       row;
  gboolean   run;
  gint       initalComboElem;


  guiStuffPtr = &guiStuffRecord;
  guiStuffPtr->imageId = gimp_drawable_get_image(drawable_id);
  guiStuffPtr->msg_label = NULL;
  guiStuffPtr->svg_entry = NULL;
  guiStuffPtr->svg_filesel = NULL;
  guiStuffPtr->valPtr = fiVals;


  fiVals->altSelection = -1;

  gimp_ui_init (PLUG_IN_BINARY, TRUE);

  dialog = gimp_dialog_new (_("Blend Fill Selection"), PLUG_IN_BINARY,
                            NULL, 0,
                            gimp_standard_help_func, PLUG_IN_NAME,

                            GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,

                            NULL);

  button = gimp_dialog_add_button (GIMP_DIALOG(dialog), GTK_STOCK_OK, GTK_RESPONSE_OK);
  guiStuffPtr->ok_button = button;

  gtk_dialog_set_alternative_button_order (GTK_DIALOG (dialog),
                                           GTK_RESPONSE_OK,
                                           GTK_RESPONSE_CANCEL,
                                           -1);

  gimp_window_set_transient (GTK_WINDOW (dialog));

  main_vbox = gtk_vbox_new (FALSE, 12);
  gtk_container_set_border_width (GTK_CONTAINER (main_vbox), 12);
  gtk_container_add (GTK_CONTAINER (GTK_DIALOG (dialog)->vbox), main_vbox);
  gtk_widget_show (main_vbox);


  /* Controls */
  table = gtk_table_new (3, 3, FALSE);
  gtk_table_set_col_spacings (GTK_TABLE (table), 6);
  gtk_table_set_row_spacings (GTK_TABLE (table), 6);
  gtk_box_pack_start (GTK_BOX (main_vbox), table, FALSE, FALSE, 0);
  gtk_widget_show (table);

  row = 0;

  /* horizontalBlendFlag checkbutton  */
  label = gtk_label_new (_("fills the selection by blending opposite border colors "
                           "outside the selction to cover the selected area.\n"
                           "Intended to fix small pixel errors"));
  gtk_widget_show (label);
  gtk_table_attach (GTK_TABLE (table), label, 0, 3, row, row+1,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);
  gtk_misc_set_alignment (GTK_MISC (label), 0.5, 0.5);

  row++;

  /* horizontalBlendFlag checkbutton  */
  label = gtk_label_new (_("Horizontal Blend:"));
  gtk_widget_show (label);
  gtk_table_attach (GTK_TABLE (table), label, 0, 1, row, row+1,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);
  gtk_misc_set_alignment (GTK_MISC (label), 0, 0.5);


  checkbutton = gtk_check_button_new_with_label (" ");
  gimp_help_set_help_data (checkbutton, _("ON: enable horizontal color blending. "
                                          "OFF: disable horizontal color blending."), NULL);
  gtk_widget_show (checkbutton);
  gtk_table_attach( GTK_TABLE(table), checkbutton, 1, 2, row, row+1,
                    GTK_FILL, 0, 0, 0 );
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (checkbutton), fiVals->horizontalBlendFlag);
  g_signal_connect (checkbutton, "toggled",
                    G_CALLBACK (on_gboolean_button_update),
                    &fiVals->horizontalBlendFlag);

  row++;

  /* verticalBlendFlag checkbutton  */
  label = gtk_label_new (_("Vertical Blend:"));
  gtk_widget_show (label);
  gtk_table_attach (GTK_TABLE (table), label, 0, 1, row, row+1,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);
  gtk_misc_set_alignment (GTK_MISC (label), 0, 0.5);


  checkbutton = gtk_check_button_new_with_label (" ");
  gimp_help_set_help_data (checkbutton, _("ON: enable vertical color blending. "
                                          "OFF: disable vertical color blending."), NULL);
  gtk_widget_show (checkbutton);
  gtk_table_attach( GTK_TABLE(table), checkbutton, 1, 2, row, row+1,
                    GTK_FILL, 0, 0, 0 );
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (checkbutton), fiVals->verticalBlendFlag);
  g_signal_connect (checkbutton, "toggled",
                    G_CALLBACK (on_gboolean_button_update),
                    &fiVals->verticalBlendFlag);

  row++;

  adj = gimp_scale_entry_new (GTK_TABLE (table), 0, row,
                              _("Border Radius:"), SCALE_MIN_WIDTH, SPINBUTTON_MIN_WIDTH,
                              fiVals->borderRadius, 1.0, 10.0, 1.0, 10.0, 0,
                              TRUE, 0, 0,
                              _("radius for picking border colors"),
                              NULL /* help_id */
                              );
  g_signal_connect (adj, "value-changed",
                    G_CALLBACK (gimp_int_adjustment_update),
                    &fiVals->borderRadius);

  row++;


  /* layer combo_box (altSelection) */
  label = gtk_label_new (_("Set Selection:"));
  gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
  gtk_table_attach (GTK_TABLE (table), label, 0, 1, row, row + 1,
                    GTK_FILL, GTK_FILL, 4, 0);
  gtk_widget_show (label);

  /* layer combo_box (Sample from where to pick the alternative selection */
  combo = gimp_layer_combo_box_new (p_selectionConstraintFunc, NULL);
  g_object_set_data (G_OBJECT (combo), "guiStuffPtr", guiStuffPtr);
  gimp_int_combo_box_prepend (GIMP_INT_COMBO_BOX (combo),
                              GIMP_INT_STORE_VALUE,    SELECTION_FROM_VECTORS,
                              GIMP_INT_STORE_LABEL,    _("Selection From All Paths"),
                              GIMP_INT_STORE_STOCK_ID, GIMP_STOCK_PATHS,
                              -1);
  gimp_int_combo_box_prepend (GIMP_INT_COMBO_BOX (combo),
                              GIMP_INT_STORE_VALUE,    SELECTION_FROM_SVG_FILE,
                              GIMP_INT_STORE_LABEL,    _("Selection From Vectors File"),
                              GIMP_INT_STORE_STOCK_ID, GIMP_STOCK_PATH,
                              -1);

  initalComboElem = drawable_id;
  if (fiVals->altSelection == SELECTION_FROM_VECTORS)
  {
    initalComboElem = SELECTION_FROM_VECTORS;
  }
  else if(fiVals->altSelection == SELECTION_FROM_SVG_FILE)
  {
    initalComboElem = SELECTION_FROM_SVG_FILE;
  }
  else if(gimp_drawable_is_valid(fiVals->altSelection) == TRUE)
  {
    initalComboElem = fiVals->altSelection;
  }
  gimp_int_combo_box_connect (GIMP_INT_COMBO_BOX (combo), initalComboElem,
                              G_CALLBACK (p_selectionComboCallback),
                              &fiVals->altSelection);
  gimp_int_combo_box_set_active (GIMP_INT_COMBO_BOX (combo), initalComboElem);

  gtk_table_attach (GTK_TABLE (table), combo, 1, 3, row, row + 1,
                    GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);
  gtk_widget_show (combo);

  row++;


  /* grab vectors button */
  button = gtk_button_new_with_label (_("Save Paths"));
  gtk_widget_show (button);
  gtk_table_attach (GTK_TABLE (table), button, 0, 1, row, row + 1,
                    GTK_FILL, GTK_FILL, 4, 0);

  gimp_help_set_help_data (button, _("Save all pathes as svg vector file."
                          "(use svg file when large or many pathes shall be used)"), NULL);
  g_signal_connect (G_OBJECT (button), "clicked",
                      G_CALLBACK (on_save_svg_clicked),
                      guiStuffPtr);

  /* the (output) video entry */
  entry = gtk_entry_new ();
  guiStuffPtr->svg_entry = entry;
  gtk_widget_show (entry);
  gtk_table_attach (GTK_TABLE (table), entry, 1, 2, row, row + 1,
                    GTK_FILL, GTK_FILL, 4, 0);
  gimp_help_set_help_data (entry, _("Name of SVG vector file"), NULL);
  if(strncmp("<?xml", guiStuffPtr->valPtr->selectionSVGFileName, 3) == 0)
  {
    g_snprintf(guiStuffPtr->valPtr->selectionSVGFileName
             , sizeof(guiStuffPtr->valPtr->selectionSVGFileName), "%s"
             , _("selection.svg"));
  }
  gtk_entry_set_text(GTK_ENTRY (entry), guiStuffPtr->valPtr->selectionSVGFileName);
  g_signal_connect (G_OBJECT (entry), "changed",
                      G_CALLBACK (on_svg_entry_changed),
                      guiStuffPtr);



  button = gtk_button_new_with_label (_("..."));
  gtk_widget_set_size_request (button, BUTTON_MIN_WIDTH, -1);
  gtk_widget_show (button);
  gtk_table_attach (GTK_TABLE (table), button, 2, 3, row, row + 1,
                    GTK_FILL, GTK_FILL, 4, 0);

  gimp_help_set_help_data (button, _("Select output svg vector file via browser"), NULL);
  g_signal_connect (G_OBJECT (button), "clicked",
                      G_CALLBACK (on_filesel_button_clicked),
                      guiStuffPtr);


  row++;
  label = gtk_label_new (" ");
  guiStuffPtr->msg_label = label;
  gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
  gtk_table_attach (GTK_TABLE (table), label, 0, 3, row, row + 1,
                    GTK_FILL, GTK_FILL, 4, 0);
  gtk_widget_show (label);


  p_check_exec_condition_and_set_ok_sesitivity(guiStuffPtr);

  /* Done */

  gtk_widget_show (dialog);

  run = (gimp_dialog_run (GIMP_DIALOG (dialog)) == GTK_RESPONSE_OK);

  if((run) && (fiVals->altSelection == SELECTION_FROM_VECTORS))
  {
    p_save_vectors_to_string(guiStuffPtr);
  }

  gtk_widget_destroy (dialog);

  if(gap_debug)
  {
    printf("guiStuffPtr.2 Svg:%s\n", guiStuffPtr->valPtr->selectionSVGFileName);
  }

  return run;
}  /* end gap_blend_fill_dialog */
