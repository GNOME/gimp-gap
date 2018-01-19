/* gap_opacity_exposure_main.c
 *  This filter sets the opacity in the specified layer
 *  in a way that a combination in normal mode with the layer below
 *  will give the best matching average brightness compared to a pecified target value
 *  or a reference layer (of same size)
 *
 *  It is intended to adjust exposure in a series of timelapse frame images  
 *  to compensate changing light conditions during the timelapse shooting period.
 *  The frame images must contain 2 layers representing the same image in 
 *  2 different brightness variants.
 *  (typically created by 2 raw processing loops with different exposure settings)
 *  
 *  Note that only opaque pixels are involved in the comparison with the Reference layer.
 *
 *    by hof (Wolfgang Hofer)
 *  2016/06/15
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
/* Revision history
 *  (2011/12/01)  2.7.0       hof: created
 */
int gap_debug = 0;  /* 1 == print debug infos , 0 dont print debug infos */
#define GAP_DEBUG_DECLARED 1


#include "config.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <gtk/gtk.h>
#include <libgimp/gimp.h>
#include <libgimp/gimpui.h>

#include "gimplastvaldesc.h"
#include "gap_arr_dialog.h"

#include  "gap_libgapbase.h"

#include "gap-intl.h"



#define PLUG_IN_NAME        "gap-opacity-exposure"
#define PLUG_IN_BINARY      "gap_opacity_exposure"
#define PLUG_IN_PRINT_NAME  "Opacity Exposure"
#define PLUG_IN_IMAGE_TYPES "RGB*"
#define PLUG_IN_AUTHOR      "Wolfgang Hofer (hof@gimp.org)"
#define PLUG_IN_COPYRIGHT   "Wolfgang Hofer"
#define PLUG_IN_HELP_ID     "gap-opacity-exposure"


#define OPACITY_LEVEL_UCHAR 50

#define GAP_OECD_RESPONSE_RESET 1
 
 

typedef struct Context {
  gint32  involvedPixelCount;       /* number of (opaque) pixels involved in evaluation of summary values */
  gdouble redSumRef;                /* summ of all Red involved pixels in the refDrawable   */
  gdouble greenSumRef;              /* summ of all Green involved pixels in the refDrawable */
  gdouble blueSumRef;               /* summ of all Blue involved pixels in the refDrawable  */
  gdouble redSumUpper;
  gdouble greenSumUpper;
  gdouble blueSumUpper;
  gdouble redSumLower;
  gdouble greenSumLower;
  gdouble blueSumLower;
  
  GimpDrawable *refDrawable;
  GimpDrawable *upperDrawable;
  GimpDrawable *lowerDrawable;
  
} Context; 

typedef struct FilterValues {
   gdouble    targetLum;              /* 0.0 to 100.0 percent */
   gboolean   useRefLayerLum;
   gboolean   useRefLayerMsk;
   gint32     refLayerId;
} FilterValues;

typedef struct _OpacityExposureDialog OpacityExposureDialog;

struct _OpacityExposureDialog
{
  gint          run;
  gint          show_progress;
  gint32        refLayerId;
  gint32        upperLayerId;
  gint32        lowerLayerId;
  gint          countPotentialRefLayers;


  GtkWidget       *shell;

  GtkWidget       *okButton;
  GtkWidget       *getLuminanceButton;
  GtkWidget       *refLayerCombo;
  GtkWidget       *refLayerLabel;

  GtkObject       *targetLum_spinbutton_adj;
  GtkWidget       *targetLum_spinbutton;
  GtkWidget       *useRefLayerLumCheckbutton;
  GtkWidget       *useRefLayerMskCheckbutton;

  FilterValues   *vals;
};




static void  query (void);
static void  run (const gchar *name,          /* name of plugin */
     gint nparams,               /* number of in-paramters */
     const GimpParam * param,    /* in-parameters */
     gint *nreturn_vals,         /* number of out-parameters */
     GimpParam ** return_vals);  /* out-parameters */

static gdouble   p_getAverageLuminance(gint32 layerId);
static void      p_adjustOpacityToMatchAverageReferenceBrightness(FilterValues *fiVals
                   , gint32  upperDrawableId
                   , gint32  lowerDrawableId
                   );
                   
static void       p_color_channel_sum_regions (const GimpPixelRgn *refPR
                    ,const GimpPixelRgn *upperPR
                    ,const GimpPixelRgn *lowerPR
                    ,Context *context);


static void      p_mainDialogResonse (GtkWidget *widget,
                    gint       response_id,
                   OpacityExposureDialog *oecd);
static void      p_layerMenuCallback(GtkWidget *widget, gint32 *reflayerId);
static gint      p_refLayerConstrain(gint32 image_id, gint32 drawable_id, gpointer data);
static void      on_gboolean_button_update (GtkWidget *widget, gpointer   data);
static void      p_update_widget_sensitivity(OpacityExposureDialog *oecd);
static void      p_init_widget_values (OpacityExposureDialog *oecd);
static gboolean  p_dialog (FilterValues *fiVals, gint32 upperLayerId, gint32 lowerLayerId);
static void      p_get_last_values(FilterValues *fiVals);
static gint32    p_findLowerLayer(gint32 image_id, gint32 upperLayerId);

static gdouble   p_calculateAvgLuminance(gint32 involvedPixelCount
                            , gdouble redSum
                            , gdouble greenSum
                            , gdouble blueSum
                            );
static gint32    p_processing(gint32 image_id, gint32 upperLayerId, gboolean doProgress, FilterValues *fiVals);


/* Global Variables */
GimpPlugInInfo PLUG_IN_INFO =
{
  NULL,   /* init_proc  */
  NULL,   /* quit_proc  */
  query,  /* query_proc */
  run     /* run_proc   */
};


FilterValues     fiVals;


static const GimpParamDef in_args[] =
{
    { GIMP_PDB_INT32,    "run-mode",      "Interactive, non-interactive" },
    { GIMP_PDB_IMAGE,    "image",         "Input image"                  },
    { GIMP_PDB_DRAWABLE, "drawable",      "layer where opacity value shall be set in a way that combination in normal mode"
                                          "with the layer below gives best matching average brightness compared to targetLuminance or to the refLayer"  },
    { GIMP_PDB_FLOAT,    "targetLuminance", "desired average target luminance when merged down to the layer below in NORMAL mode 0.0 to 100.0 percent"},
    { GIMP_PDB_INT32,    "useRefLum",     "TRUE (1): use average luminance of the opaque pixels in the reference layer instead of specified targetLuminance "},
    { GIMP_PDB_INT32,    "useRefMsk",     "TRUE (1): use opaque pixels in the reference layer as mask for average lumninance calculation "},
    { GIMP_PDB_DRAWABLE, "refLayerId",    "(optional) reference layer to be used as reference exposure (average brightness)"
                                          "(note that the reference layer may be in another image)" }
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
    GIMP_LASTVALDEF_GDOUBLE     (GIMP_ITER_TRUE,   fiVals.targetLum,      "targetLum"),
    GIMP_LASTVALDEF_GBOOLEAN    (GIMP_ITER_FALSE,  fiVals.useRefLayerLum, "useRefLayerLum"),
    GIMP_LASTVALDEF_GBOOLEAN    (GIMP_ITER_FALSE,  fiVals.useRefLayerMsk, "useRefLayerMsk"),
    GIMP_LASTVALDEF_DRAWABLE    (GIMP_ITER_FALSE,  fiVals.refLayerId,     "refLayerId"),
  };

  /* registration for last values buffer structure (useful for filter apply via frames_modify feature) */
  gimp_lastval_desc_register(PLUG_IN_NAME,
                             &fiVals,
                             sizeof(fiVals),
                             G_N_ELEMENTS (lastvals),
                             lastvals);


  gimp_plugin_domain_register (GETTEXT_PACKAGE, LOCALEDIR);


  gimp_install_procedure (PLUG_IN_NAME,
                          "Adjust layer opacity of a layer to match brightness with a given trget value or a reference layer.",
                          "This filter operates on 2 layers in the layerstack, where "
                          "the opacity of the upper layer is adjusted in a way that combination in normal mode with the layer below "
                          "will have best possible match in average brightness with the specified targetLuminance or with a specified reference layer. "
                          "For average brightness calculation only opaque pixels are taken into account."
                          "The filter is intended to compensate changing exposure in a series of frame images"
                          "for timelapse video sequences where each frame image has 2 layers showing the same image, "
                          "but were converted from RAW fileformat with different exposure settings "
                          "(that shall give darker and brighter exposure than the wanted exposure of the reference layer) "
                          " ",
                          PLUG_IN_AUTHOR,
                          PLUG_IN_COPYRIGHT,
                          GAP_VERSION_WITH_DATE,
                          N_(PLUG_IN_PRINT_NAME),
                          PLUG_IN_IMAGE_TYPES,
                          GIMP_PLUGIN,
                          global_number_in_args,
                          global_number_out_args,
                          in_args,
                          return_vals);


  {
    /* Menu names */
    const char *menupath = N_("<Image>/Video/Layer/Attributes/");

    gimp_plugin_menu_register (PLUG_IN_NAME, menupath);
  }

}




/* ---------------------------------
 * p_color_channel_sum_regions
 * ---------------------------------
 * calculate summary Colorchanel values of pixels that are opaque in all 3 drawables 
 * in the compared area region.
 */
static void
p_color_channel_sum_regions (const GimpPixelRgn *refPR
                  ,const GimpPixelRgn *upperPR
                  ,const GimpPixelRgn *lowerPR
                  ,Context *context)
{
  guint    row;
  guchar*  ref = refPR->data;       /* the reference drawable */
  guchar*  upper = upperPR->data;   /* the upper drawable */
  guchar*  lower = lowerPR->data;   /* the lower drawable */
  
  guint    commonWidth;
  guint    commonHeight;

  commonWidth = MIN(MIN(upperPR->w, lowerPR->w), refPR->w);
  commonHeight = MIN(MIN(upperPR->h, lowerPR->h), refPR->h);

//   if(gap_debug)
//   {
//     printf("region REF x:%d y:%d w:%d h:%d  upper x:%d y:%d w:%d h:%d  Common w:%d h:%d px:%d py:%d\n"
//        , (int)refPR->x
//        , (int)refPR->y
//        , (int)refPR->w
//        , (int)refPR->h
//        , (int)upperPR->x
//        , (int)upperPR->y
//        , (int)upperPR->w
//        , (int)upperPR->h
//        , (int)commonWidth
//        , (int)commonHeight
//        , (int)context->px
//        , (int)context->py
//        );
//   }


  for (row = 0; row < commonHeight; row++)
  {
    guint  col;
    guint  idxref;
    guint  idxupper;
    guint  idxlower;
    
    idxref = 0;
    idxupper = 0;
    idxlower = 0;
    for(col = 0; col < commonWidth; col++)
    {
      gboolean isInvolved;
      
      isInvolved = TRUE;
      
      if(refPR->bpp > 3)
      {
        if(ref[idxref +3] < OPACITY_LEVEL_UCHAR)
        {
          /* transparent reference pixel is not compared */
          isInvolved = FALSE;
        }
      }

      if(upperPR->bpp > 3)
      {
        if(upper[idxupper +3] < OPACITY_LEVEL_UCHAR)
        {
          /* transparent upper pixel is not compared */
          isInvolved = FALSE;
        }
      }

      if(lowerPR->bpp > 3)
      {
        if(lower[idxlower +3] < OPACITY_LEVEL_UCHAR)
        {
          /* transparent upper pixel is not compared */
          isInvolved = FALSE;
        }
      }

      if (isInvolved == TRUE)
      {
        context->involvedPixelCount += 1;
        context->redSumRef   += ref[idxref];
        context->greenSumRef += ref[idxref +1];
        context->blueSumRef  += ref[idxref +2];
        
        context->redSumUpper   += upper[idxupper];
        context->greenSumUpper += upper[idxupper +1];
        context->blueSumUpper  += upper[idxupper +2];
        
        context->redSumLower   += lower[idxlower];
        context->greenSumLower += lower[idxlower +1];
        context->blueSumLower  += lower[idxlower +2];
      }

      idxref    += refPR->bpp;
      idxupper += upperPR->bpp;
      idxlower += lowerPR->bpp;
    }


    ref += refPR->rowstride;
    upper += upperPR->rowstride;
    lower += lowerPR->rowstride;

  }

}  /* end p_color_channel_sum_regions */


/* ------------------------------------------------
 * p_getAverageLuminance
 * ------------------------------------------------
 * get average luminance value of all opaque pixels
 * in percent (0% for totally black image, 100% for totally white image)
 */
static gdouble
p_getAverageLuminance(gint32  refDrawableId)
{
  Context contextData;
  Context *context;

  GimpPixelRgn refPR;
  gpointer  pr;
  gdouble avgLumRef;
  
  
  /* init Context for full drawable area compare processing
   * note that context is designed for the more complex locating procedures
   * therefore most context elements are not used this time...
   */
  context = &contextData;
  context->involvedPixelCount = 0;
  context->redSumRef      = 0.0;
  context->greenSumRef    = 0.0;   
  context->blueSumRef     = 0.0;   
  context->redSumUpper    = 0.0;
  context->greenSumUpper  = 0.0;
  context->blueSumUpper   = 0.0;
  context->redSumLower    = 0.0;
  context->greenSumLower  = 0.0;
  context->blueSumLower   = 0.0;
  
  context->upperDrawable = NULL;
  context->lowerDrawable = NULL;
  context->refDrawable = gimp_drawable_get(refDrawableId);

  gimp_pixel_rgn_init (&refPR, context->refDrawable
                      , 0, 0      /* origin x, y top left corner*/
                      , context->refDrawable->width, context->refDrawable->height
                      , FALSE     /* dirty */
                      , FALSE     /* shadow */
                       );

  /* calculate summary color channels of pixels that are opaque 
   * (use use the same reference layer for all 3 drawables.
   */
  for (pr = gimp_pixel_rgns_register (1, &refPR);
       pr != NULL;
       pr = gimp_pixel_rgns_process (pr))
  {
    p_color_channel_sum_regions(&refPR, &refPR, &refPR, context);
  }

  avgLumRef = p_calculateAvgLuminance(context->involvedPixelCount
                            , context->redSumRef
                            , context->greenSumRef
                            , context->blueSumRef
                            );  

  
  if (context->involvedPixelCount < 0)
  {
    avgLumRef = 0;
  }
  
  
  if(context->refDrawable != NULL)
  {
    gimp_drawable_detach(context->refDrawable);
  }

  return (avgLumRef * 100.0);

}  /* end p_getAverageLuminance */



/* ------------------------------------------------
 * p_adjustOpacityToMatchAverageReferenceBrightness
 * ------------------------------------------------
 * this procedure sets the opacity of the specified 
 *    upperDrawableId (typically the layer on top of layerstack)
 * in a way that a mix with the 
 *    lowerDrawableId (typically the layer below)
 * reults in best matching average luminance compared to the specified
 *    targetLuminancee (in case useRefLayerLum is FALSE) value OR with the specified
 *    refDrawableId (in case useRefLayerLum is TRUE ue the referencelayer average luminance as desired target)
 * Note that only opaque pixels (opaque in all 3 drawables) are involved in the
 * calculation of the average brightness (exposure) value.
 *
 * Typically all 3 drawables shall have same size (otherwise only the smallest area is processed)
 * Further note that offsets of the layers within the image(s) are ignored for processing.
 */
void
p_adjustOpacityToMatchAverageReferenceBrightness(FilterValues *fiVals
  , gint32  upperDrawableId
  , gint32  lowerDrawableId
  )
{
  Context contextData;
  Context *context;

  GimpPixelRgn refPR;
  GimpPixelRgn upperPR;
  GimpPixelRgn lowerPR;
  gpointer  pr;
  gint      commonAreaWidth, commonAreaHeight;
  gdouble   upperOpacity;
  
  gint32  refDrawableId;
  
  refDrawableId = -1;
  if ((fiVals->useRefLayerLum) || (fiVals->useRefLayerMsk))
  {
    refDrawableId = fiVals->refLayerId;
  }
  
  
  
  /* init Context for full drawable area compare processing
   * note that context is designed for the more complex locating procedures
   * therefore most context elements are not used this time...
   */
  context = &contextData;
  context->involvedPixelCount = 0;
  context->redSumRef      = 0.0;
  context->greenSumRef    = 0.0;   
  context->blueSumRef     = 0.0;   
  context->redSumUpper    = 0.0;
  context->greenSumUpper  = 0.0;
  context->blueSumUpper   = 0.0;
  context->redSumLower    = 0.0;
  context->greenSumLower  = 0.0;
  context->blueSumLower   = 0.0;
  
  context->upperDrawable = gimp_drawable_get(upperDrawableId);
  context->lowerDrawable = gimp_drawable_get(lowerDrawableId);
  context->refDrawable = NULL;
  if ((fiVals->useRefLayerLum) || (fiVals->useRefLayerMsk))
  {
    context->refDrawable = gimp_drawable_get(refDrawableId);
    commonAreaWidth = MIN(context->refDrawable->width, context->upperDrawable->width);
    commonAreaHeight = MIN(context->refDrawable->height, context->upperDrawable->height);
  }
  else
  {
    commonAreaWidth = context->upperDrawable->width;
    commonAreaHeight = context->upperDrawable->height;
  }
  


  if(gap_debug)
  {
    printf ("p_adjustOpacityToMatchAverageReferenceBrightness refDrawableId:%d upperDrawableId:%d lowerDrawableId:%d \n"
       ,(int)refDrawableId
       ,(int)upperDrawableId
       ,(int)lowerDrawableId
      );
  }

  if ((commonAreaWidth <= 0) || (commonAreaHeight <= 0))
  {
    /* there is no common area size to process */
    if(gap_debug)
    {
      printf ("p_adjustOpacityToMatchAverageReferenceBrightness common area size is 0 (DO NOTHING)\n");
    }
    return;
  }  

  if ((fiVals->useRefLayerLum) || (fiVals->useRefLayerMsk))
  {
    gimp_pixel_rgn_init (&refPR, context->refDrawable
                      , 0, 0      /* origin x, y top left corner*/
                      , commonAreaWidth, commonAreaHeight
                      , FALSE     /* dirty */
                      , FALSE     /* shadow */
                       );
  }
  
  gimp_pixel_rgn_init (&upperPR, context->upperDrawable
                      , 0, 0      /* origin x, y top left corner*/
                      , commonAreaWidth, commonAreaHeight
                      , FALSE     /* dirty */
                      , FALSE     /* shadow */
                       );

  gimp_pixel_rgn_init (&lowerPR, context->lowerDrawable
                      , 0, 0      /* origin x, y top left corner*/
                      , commonAreaWidth, commonAreaHeight
                      , FALSE     /* dirty */
                      , FALSE     /* shadow */
                       );

  if ((fiVals->useRefLayerLum) || (fiVals->useRefLayerMsk))
  {
    /* calculate summary color channels of pixels that are opaque in all 3 drawables.
     */
    for (pr = gimp_pixel_rgns_register (3, &refPR, &upperPR, &lowerPR);
       pr != NULL;
       pr = gimp_pixel_rgns_process (pr))
    {
      p_color_channel_sum_regions(&refPR, &upperPR, &lowerPR, context);
    }
  }
  else
  {
    /* calculate summary color channels of pixels that are opaque in all 3 drawables.
     */
    for (pr = gimp_pixel_rgns_register (2, &upperPR, &lowerPR);
       pr != NULL;
       pr = gimp_pixel_rgns_process (pr))
    {
      /* use upperPR twice to fake the missing refLayer */
      p_color_channel_sum_regions(&upperPR, &upperPR, &lowerPR, context);
    }
  }
  
  upperOpacity = 1.0;  /* assume show upper layer full opaque */

  if(gap_debug)
  {
    printf ("p_adjustOpacityToMatchAverageReferenceBrightness involvedPixelCount:%d \n"
            "   refsum(R:%f G:%f B:%f)\n"
            "   uppsum(R:%f G:%f B:%f)\n"
            "   lowsum(R:%f G:%f B:%f)\n"
       ,(int)context->involvedPixelCount
       ,(float)context->redSumRef
       ,(float)context->greenSumRef
       ,(float)context->blueSumRef
       ,(float)context->redSumUpper
       ,(float)context->greenSumUpper
       ,(float)context->blueSumUpper
       ,(float)context->redSumLower
       ,(float)context->greenSumLower
       ,(float)context->blueSumLower
      );
  }

  
  if (context->involvedPixelCount > 0)
  {
    gdouble avgLumRef;
    gdouble avgLumUpper;
    gdouble avgLumLower;

    if (fiVals->useRefLayerLum)
    {
      avgLumRef = p_calculateAvgLuminance(context->involvedPixelCount
                            , context->redSumRef
                            , context->greenSumRef
                            , context->blueSumRef
                            );
    }
    else
    {
      /* get reference from the specified target percent value 
       * (convert to range 0.0 - 1.0)
       */
      avgLumRef = fiVals->targetLum / 100.0;
    }
    
    avgLumUpper = p_calculateAvgLuminance(context->involvedPixelCount
                            , context->redSumUpper
                            , context->greenSumUpper
                            , context->blueSumUpper
                            );
    avgLumLower = p_calculateAvgLuminance(context->involvedPixelCount
                            , context->redSumLower
                            , context->greenSumLower
                            , context->blueSumLower
                            );
    

    if(gap_debug)
    {
      printf ("p_adjustOpacityToMatchAverageReferenceBrightness involvedPixelCount:%d \n"
            "   refLUM: %f\n"
            "   uppLUM: %f\n"
            "   lowLUM: %f\n"
       ,(int)context->involvedPixelCount
       ,(float)avgLumRef
       ,(float)avgLumUpper
       ,(float)avgLumLower
      );
    }


    
    if (avgLumRef <= MIN(avgLumUpper, avgLumLower))
    {
      /* reference is darker than both layers */
      
      if (avgLumLower < avgLumUpper)
      {
        upperOpacity = 0.0; /* set upper transparent to show only the darker lower layer */
      }
    }
    else if (avgLumRef >= MAX(avgLumUpper, avgLumLower))
    {
      /* reference is brighter than both layers */
      if (avgLumLower > avgLumUpper)
      {
        upperOpacity = 0.0; /* set upper transparent to show only the brighter lower layer */
      }
    }
    else
    {
      gdouble factor;
      gdouble uppLUM;
      gdouble lowLUM;
      
      uppLUM = MAX(avgLumUpper, avgLumLower);
      lowLUM = MIN(avgLumUpper, avgLumLower);
      
      factor = 1.0 / (uppLUM - lowLUM);
      upperOpacity = (avgLumRef - lowLUM) * factor;
      
    }

  }
  
  
  gimp_layer_set_opacity(upperDrawableId, CLAMP(upperOpacity * 100.0, 0.0, 100.0));
  
  if(context->refDrawable != NULL)
  {
    gimp_drawable_detach(context->refDrawable);
  }
  if(context->upperDrawable != NULL)
  {
    gimp_drawable_detach(context->upperDrawable);
  }
  if(context->lowerDrawable != NULL)
  {
    gimp_drawable_detach(context->lowerDrawable);
  }


}  /* end p_adjustOpacityToMatchAverageReferenceBrightness */



/* ---------------------------------
 * p_mainDialogResonse
 * ---------------------------------
 */
static void
p_mainDialogResonse (GtkWidget *widget,
                 gint       response_id,
                 OpacityExposureDialog *oecd)
{
  GtkWidget *dialog;

  switch (response_id)
  {
    case GAP_OECD_RESPONSE_RESET:
      if(oecd)
      {
        if(oecd->refLayerId >= 0)
        {
          gdouble targetLum;
          targetLum = p_getAverageLuminance(oecd->refLayerId);
          gtk_adjustment_set_value(GTK_ADJUSTMENT(oecd->targetLum_spinbutton_adj), (gfloat)targetLum);
        }
      }
      break;

    case GTK_RESPONSE_OK:
      if(oecd)
      {
        if (GTK_WIDGET_VISIBLE (oecd->shell))
        {
          gtk_widget_hide (oecd->shell);
        }
        oecd->run = TRUE;
      }

    default:
      dialog = NULL;
      if(oecd)
      {
        dialog = oecd->shell;
        if(dialog)
        {
          oecd->shell = NULL;
          gtk_widget_destroy (dialog);
        }
      }
      gtk_main_quit ();
      break;
  }
}  /* end p_mainDialogResonse */


/* ------------------------------
 * p_layerMenuCallback
 * ------------------------------
 *
 */
static void
p_layerMenuCallback(GtkWidget *widget, gint32 *reflayerId)
{
  gint value;

  gimp_int_combo_box_get_active (GIMP_INT_COMBO_BOX (widget), &value);

  if(gap_debug)
  {
    printf("p_layerMenuCallback: cloudLayerAddr:%ld value:%d\n"
      ,(long)reflayerId
      ,(int)value
      );
  }

  if(reflayerId != NULL)
  {
    *reflayerId = value;
  }


} /* end p_layerMenuCallback */

/* ------------------------------
 * p_refLayerConstrain
 * ------------------------------
 *
 */
static gint
p_refLayerConstrain(gint32 image_id, gint32 drawable_id, gpointer data)
{
  OpacityExposureDialog *oecd;
  
  oecd = (OpacityExposureDialog *)data;

  if(gap_debug)
  {
    printf("p_refLayerConstrain PROCEDURE image_id:%d drawable_id:%d oecd:%ld  countPotentialRefLayers:%d\n"
                          ,(int)image_id
                          ,(int)drawable_id
                          ,(long)oecd
                          ,(int)oecd->countPotentialRefLayers
                          );
  }

  if(drawable_id < 0)
  {
     /* gimp 1.1 makes a first call of the constraint procedure
      * with drawable_id = -1, and skips the whole image if FALSE is returned
      */
     return(TRUE);
  }

  if(gimp_item_is_valid(drawable_id) != TRUE)
  {
     return(FALSE);
  }

  /* the processed layers are not usable as reference */
  if((drawable_id == oecd->upperLayerId)
  || (drawable_id == oecd->lowerLayerId))
  {
    return (FALSE);
  }

  if(!gimp_drawable_is_rgb(drawable_id))
  {
    return (FALSE);
  }

  oecd->countPotentialRefLayers++;
  if(gap_debug)
  {
    printf("p_refLayerConstrain PROCEDURE image_id:%d drawable_id:%d OK layer accepted  countPotentialRefLayers:%d\n"
                          ,(int)image_id
                          ,(int)drawable_id
                          ,(int)oecd->countPotentialRefLayers
                          );
  }

  return(TRUE);

} /* end p_refLayerConstrain */


/* --------------------------------------
 * on_gboolean_button_update
 * --------------------------------------
 */
static void
on_gboolean_button_update (GtkWidget *widget,
                           gpointer   data)
{
  OpacityExposureDialog *oecd;
  gint *toggle_val = (gint *) data;

  if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget)))
  {
    *toggle_val = TRUE;
  }
  else
  {
    *toggle_val = FALSE;
  }
  gimp_toggle_button_sensitive_update (GTK_TOGGLE_BUTTON (widget));

  oecd = (OpacityExposureDialog *) g_object_get_data (G_OBJECT (widget), "oecd");
  if(oecd != NULL)
  {
    p_update_widget_sensitivity (oecd);
  }
}

/* --------------------------------------
 * p_update_widget_sensitivity
 * --------------------------------------
 */
static void
p_update_widget_sensitivity (OpacityExposureDialog *oecd)
{
  if(oecd == NULL)
  {
    return;
  }
  if(oecd->vals == NULL)
  {
    return;
  }

  if (oecd->refLayerId >= 0)
  {
     gtk_widget_set_sensitive(oecd->getLuminanceButton, TRUE);
  }
  else
  {
     gtk_widget_set_sensitive(oecd->getLuminanceButton, FALSE);
  }
  
  if (oecd->vals->useRefLayerLum == TRUE)
  {
     gtk_widget_set_sensitive(oecd->targetLum_spinbutton ,  FALSE);
  }
  else
  {
     gtk_widget_set_sensitive(oecd->targetLum_spinbutton ,  TRUE);
  }

}  /* end p_update_widget_sensitivity */


/* --------------------------------------
 * p_init_widget_values
 * --------------------------------------
 */
static void
p_init_widget_values (OpacityExposureDialog *oecd)
{
  if(oecd == NULL)
  {
    return;
  }
  if(oecd->vals == NULL)
  {
    return;
  }

  /* init spnbuttons */
  gtk_adjustment_set_value(GTK_ADJUSTMENT(oecd->targetLum_spinbutton_adj)
                         , (gfloat)oecd->vals->targetLum);

  /* init checkbuttons */
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (oecd->useRefLayerLumCheckbutton)
                               , oecd->vals->useRefLayerLum);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (oecd->useRefLayerMskCheckbutton)
                               , oecd->vals->useRefLayerMsk);

  
}  /* end p_init_widget_values */



/* ------------------------------
 * p_dialog
 * ------------------------------
 * create and show the dialog window
 * return TRUE in OK was selected.
 */
static gboolean
p_dialog (FilterValues *fiVals, gint32 upperLayerId, gint32 lowerLayerId)
{
  OpacityExposureDialog  oecDialog;
  OpacityExposureDialog *oecd;
  GtkWidget  *vbox;

  GtkWidget *dialog1;
  GtkWidget *dialog_vbox1;
  GtkWidget *frame1;
  GtkWidget *vbox1;
  GtkWidget *label;
  GtkWidget *table1;
  GtkWidget *dialog_action_area1;
  GtkWidget *combo;
  GtkWidget *checkbutton;
  GtkObject *spinbutton_adj;
  GtkWidget *spinbutton;
  gint       row;
  gboolean   errorCount;


  oecd = &oecDialog;

  /* Init UI  */
  gimp_ui_init ("waterpattern", FALSE);


  /*  The dialog1  */
  oecd->run = FALSE;
  oecd->vals = fiVals;
  oecd->refLayerId = -1;
  oecd->upperLayerId = upperLayerId;
  oecd->lowerLayerId = lowerLayerId;
  oecd->countPotentialRefLayers = 0;
  errorCount = 0;

  if(gap_debug)
  {
    printf("p_dialog START upperLayerId:%d lowerLayerId:%d\n"
      ,(int)upperLayerId
      ,(int)lowerLayerId
      );
  }


  /*  The dialog1 and main vbox  */
  dialog1 = gimp_dialog_new (_("Opacity Exposure"), "opacity exposure",
                               NULL, 0,
                               gimp_standard_help_func, PLUG_IN_HELP_ID,

                               GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                               NULL);

  oecd->shell = dialog1;
  oecd->getLuminanceButton = gimp_dialog_add_button (GIMP_DIALOG(dialog1), GIMP_STOCK_RESET, GAP_OECD_RESPONSE_RESET);
  gimp_help_set_help_data(oecd->getLuminanceButton,
                       _("Get Average Luminance From the reference layer")
                       , NULL);
  oecd->okButton = gimp_dialog_add_button (GIMP_DIALOG(dialog1), GTK_STOCK_OK, GTK_RESPONSE_OK);


  g_signal_connect (G_OBJECT (dialog1), "response",
                      G_CALLBACK (p_mainDialogResonse),
                      oecd);

  /* the vbox */
  vbox = gtk_vbox_new (FALSE, 2);
  gtk_container_set_border_width (GTK_CONTAINER (vbox), 2);
  gtk_box_pack_start (GTK_BOX (GTK_DIALOG (dialog1)->vbox), vbox,
                      TRUE, TRUE, 0);
  gtk_widget_show (vbox);

  dialog_vbox1 = GTK_DIALOG (dialog1)->vbox;
  g_object_set_data (G_OBJECT (dialog1), "dialog_vbox1", dialog_vbox1);
  gtk_widget_show (dialog_vbox1);


  /* the frame */
  frame1 = gimp_frame_new (_("Options"));

  gtk_widget_show (frame1);
  gtk_box_pack_start (GTK_BOX (dialog_vbox1), frame1, TRUE, TRUE, 0);
  gtk_container_set_border_width (GTK_CONTAINER (frame1), 2);

  vbox1 = gtk_vbox_new (FALSE, 0);
  gtk_container_add (GTK_CONTAINER (frame1), vbox1);
  gtk_widget_show (vbox1);


  /* add label that describes how to use this filter */
  label = gtk_label_new (_("This filter adjust opacity of a layer\n"
                           "in a way that the combination with the layer below\n"
                           "matches the brightness of a reference layer"));
  gtk_box_pack_start (GTK_BOX (vbox1), label, TRUE, TRUE, 0);
  gtk_widget_show (label);


  /* table1  */
  table1 = gtk_table_new (4, 2, FALSE);
  gtk_widget_show (table1);
  gtk_box_pack_start (GTK_BOX (vbox1), table1, TRUE, TRUE, 0);
  gtk_container_set_border_width (GTK_CONTAINER (table1), 4);


  row = 0;

  label = gtk_label_new (_("Target Luminance:"));
  gtk_widget_show (label);
  gtk_table_attach (GTK_TABLE (table1), label, 0, 1, row, row+1,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);
  gtk_misc_set_alignment (GTK_MISC (label), 0, 0.5);

  /* average luminance spinbutton  */
  spinbutton_adj = gtk_adjustment_new (oecd->vals->targetLum, 0.0, 100.0, 1.0, 10, 0);
  spinbutton = gtk_spin_button_new (GTK_ADJUSTMENT (spinbutton_adj), 1, 2);
  gimp_help_set_help_data (spinbutton, _("Target Average Luminance (when merged with layer below in NORMAL mode)"), NULL);
  gtk_widget_show (spinbutton);
  gtk_table_attach (GTK_TABLE (table1), spinbutton, 1, 2, row, row+1,
                    (GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);
  g_signal_connect (G_OBJECT (spinbutton_adj), "value_changed", G_CALLBACK (gimp_double_adjustment_update), &oecd->vals->targetLum);
  oecd->targetLum_spinbutton_adj = spinbutton_adj;
  oecd->targetLum_spinbutton = spinbutton;

  row++;

  /* use reference layer's average luminance checkbutton  */
  label = gtk_label_new (_("Use RefLayer:"));
  gtk_widget_show (label);
  gtk_table_attach (GTK_TABLE (table1), label, 0, 1, row, row+1,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);
  gtk_misc_set_alignment (GTK_MISC (label), 0, 0.5);


  checkbutton = gtk_check_button_new_with_label (" ");
  g_object_set_data (G_OBJECT (checkbutton), "oecd", oecd);
  oecd->useRefLayerLumCheckbutton = checkbutton;
  gtk_widget_show (checkbutton);
  gimp_help_set_help_data (checkbutton, _("ON: use Average Luminance from opaque pixels in the reference layer (ignore Target) OFF: use specified Target Luminance value"), NULL);
  gtk_table_attach( GTK_TABLE(table1), checkbutton, 1, 2, row, row+1,
                    GTK_FILL, 0, 0, 0 );
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (checkbutton), oecd->vals->useRefLayerLum);
  g_signal_connect (checkbutton, "toggled",
                    G_CALLBACK (on_gboolean_button_update),
                    &oecd->vals->useRefLayerLum);

  row++;

  /* use reference layer as mask checkbutton  */
  label = gtk_label_new (_("Use RefLayer as Mask:"));
  gtk_widget_show (label);
  gtk_table_attach (GTK_TABLE (table1), label, 0, 1, row, row+1,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);
  gtk_misc_set_alignment (GTK_MISC (label), 0, 0.5);


  checkbutton = gtk_check_button_new_with_label (" ");
  g_object_set_data (G_OBJECT (checkbutton), "oecd", oecd);
  oecd->useRefLayerMskCheckbutton = checkbutton;
  gtk_widget_show (checkbutton);
  gimp_help_set_help_data (checkbutton, _("ON: use the opaque pixels of reference layer as mask "), NULL);
  gtk_table_attach( GTK_TABLE(table1), checkbutton, 1, 2, row, row+1,
                    GTK_FILL, 0, 0, 0 );
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (checkbutton), oecd->vals->useRefLayerMsk);
  g_signal_connect (checkbutton, "toggled",
                    G_CALLBACK (on_gboolean_button_update),
                    &oecd->vals->useRefLayerMsk);


  row++;

  /* the reference layer label and combo */
  label = gtk_label_new (_("Exposure Reference Layer"));
  oecd->refLayerLabel = label;
  gtk_widget_show (label);
  gtk_table_attach (GTK_TABLE (table1), label, 0, 1, row, row+1,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);
  gtk_misc_set_alignment (GTK_MISC (label), 0, 0.5);

  combo = gimp_layer_combo_box_new (p_refLayerConstrain, oecd);
  oecd->refLayerCombo = combo;
  gtk_widget_show(combo);
  gtk_table_attach(GTK_TABLE(table1), combo, 1, 4, row, row+1,
                   GTK_EXPAND | GTK_FILL, 0, 0, 0);

  gimp_help_set_help_data(combo,
                       _("Select a reference layer")
                       , NULL);

  gimp_int_combo_box_connect (GIMP_INT_COMBO_BOX (combo),
                              oecd->refLayerId,                      /* initial value */
                              G_CALLBACK (p_layerMenuCallback),
                              &oecd->refLayerId);



  dialog_action_area1 = GTK_DIALOG (dialog1)->action_area;
  gtk_widget_show (dialog_action_area1);
  gtk_container_set_border_width (GTK_CONTAINER (dialog_action_area1), 10);


  /* check required conditions to run this filter 
   * and add warning texts if not fulfilled.
   */
  if (oecd->countPotentialRefLayers < 1)
  {
     /* disable get Luminance button */
     gtk_widget_set_sensitive(oecd->getLuminanceButton, FALSE);

     label = gtk_label_new (_("Warning: no reference layer found \n"
                              "(open a ref image in gimp session)"));
     gtk_box_pack_start (GTK_BOX (vbox1), label, TRUE, TRUE, 0);
     gtk_widget_show (label);
     errorCount++;
  }
  if (oecd->lowerLayerId < 0)
  {
     label = gtk_label_new (_("Warning: no layer found below processed layer\n"
                              "(add the lower layer with other exposure settings)"));
     gtk_box_pack_start (GTK_BOX (vbox1), label, TRUE, TRUE, 0);
     gtk_widget_show (label);
     errorCount++;
  }
  
  if (errorCount > 0)
  {
    /* disable OK button */
    gtk_widget_set_sensitive(oecd->okButton, FALSE);
  }


  p_init_widget_values (oecd);
  p_update_widget_sensitivity (oecd);


  gtk_widget_show (dialog1);

  gtk_main ();
  gdk_flush ();

  oecd->vals->refLayerId = oecd->refLayerId;

  if (errorCount > 0)
  {
    return (FALSE);
  }
  return (oecd->run);

}  /* end p_dialog */

/* -----------------------------------
 * p_get_last_values
 * -----------------------------------
 */
static void
p_get_last_values(FilterValues *fiVals)
{
  int l_len;

  /* init default values */
  fiVals->targetLum             = 50.0; /* 50 percent */
  fiVals->useRefLayerLum        = FALSE;
  fiVals->useRefLayerMsk        = FALSE;
  fiVals->refLayerId            = -1;

  l_len = gimp_get_data_size (PLUG_IN_NAME);
  if (l_len == sizeof(FilterValues))
  {
    /* Possibly retrieve data from a previous interactive run */
    gimp_get_data (PLUG_IN_NAME, fiVals);

    if(gap_debug)
    {
      printf("p_get_last_values FOUND data for key:%s\n"
        , PLUG_IN_NAME
        );
    }
  }

  if(gap_debug)
  {
    printf("p_get_last_values: refLayerId:%d \n"
            , (int)fiVals->refLayerId
            );
  }

}  /* end p_get_last_values */


/* -----------------------------
 * p_findLowerLayer
 * -----------------------------
 */
static gint32
p_findLowerLayer(gint32 image_id, gint32 upperLayerId)
{
  gint32  lowerLayerId;
  gint      l_idx;
  gint      l_nlayers;
  gint32   *l_layers_list;
  
  lowerLayerId = -1;
  
  /* find the layer below upperLayerId 
   * (TODO curent implementation does not support the case where 
   *  where upperLayerId is not on top image level but placed somewhere in nested layergroups)
   */
  l_layers_list = gimp_image_get_layers(image_id, &l_nlayers);
  if(l_layers_list != NULL)
  {
    for(l_idx = 0; l_idx < l_nlayers; l_idx++)
    {
      if (l_layers_list[l_idx] == upperLayerId)
      {
        if (l_idx < l_nlayers -1)
        {
          lowerLayerId = l_layers_list[l_idx +1];
        }
        break;
      }
    }
    g_free(l_layers_list);
  }

  return (lowerLayerId);
  
}  /* end p_findLowerLayer */

/* ---------------------------------
 * p_calculateAvgLuminance
 * ---------------------------------
 * returns difference of 2 colors as gdouble value
 * in range 0.0 (exact match) to 1.0 (maximal difference)
 */
static gdouble
p_calculateAvgLuminance(gint32 involvedPixelCount
                            , gdouble redSum
                            , gdouble greenSum
                            , gdouble blueSum)
{
  GimpRGB rgb;
  gdouble count;

  if (involvedPixelCount > 0)
  {
    gdouble max, min;
    gdouble lum;

    count = involvedPixelCount;
    rgb.r = CLAMP(((redSum / count) / 255.0),   0.0, 1.0);
    rgb.g = CLAMP(((greenSum / count) / 255.0), 0.0, 1.0);
    rgb.b = CLAMP(((blueSum / count) / 255.0),  0.0, 1.0);
    rgb.a = 1.0;
 


    max = gimp_rgb_max (&rgb);
    min = gimp_rgb_min (&rgb);

    lum = (max + min) / 2.0;
    
    if(gap_debug)
    {
      printf("Avg R:%f G:%f B:%f   lum:%f\n"
        ,(float)rgb.r
        ,(float)rgb.g
        ,(float)rgb.b
        ,(float)lum
        );
    }
    

    return (lum);
  }
  
  return (0.0);

}  /* end p_calculateAvgLuminance */

/* -----------------------------
 * p_processing
 * -----------------------------
 * find and check required layers and start processing
 * or return -1 in case checks failed.
 * return upperLayerId in case of success.
 */
static gint32
p_processing(gint32 image_id, gint32 upperLayerId, gboolean doProgress, FilterValues *fiVals)
{
  gint32  lowerLayerId;
  
  lowerLayerId = p_findLowerLayer(image_id, upperLayerId);
  if (lowerLayerId < 0)
  {
    printf("Error: no layer found below layerid: %d in the layerstack\n", (int)upperLayerId );
    return(-1);
  }
  
  if ((fiVals->refLayerId < 0) && (fiVals->useRefLayerLum == TRUE))
  {
    printf("Error: no valid reference layer available\n");
    return(-1);
  }
  if ((fiVals->refLayerId < 0) && (fiVals->useRefLayerMsk == TRUE))
  {
    printf("Error: no valid reference layer available\n");
    return(-1);
  }
  
  p_adjustOpacityToMatchAverageReferenceBrightness(fiVals
                , upperLayerId
                , lowerLayerId
                );
  
  return (upperLayerId);
  
}  /* end p_processing */


/* -------
 * run
 * -------
 * this procedure is invoked by the GIMP to run the plug-in.
 */
static void
run (const gchar *name,          /* name of plugin */
     gint nparams,               /* number of in-paramters */
     const GimpParam * param,    /* in-parameters */
     gint *nreturn_vals,         /* number of out-parameters */
     GimpParam ** return_vals)   /* out-parameters */
{
  const gchar *l_env;
  gint32       image_id = -1;
  gint32       upperLayerId = -1;
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

  /* init default values and Possibly retrieve data from a previous interactive run */
  p_get_last_values(&fiVals);   

  /* get image and drawable */
  image_id = param[1].data.d_int32;
  upperLayerId = param[2].data.d_int32;


  /* how are we running today? */
  switch (run_mode)
  {
    case GIMP_RUN_INTERACTIVE:
      if(strcmp(name, PLUG_IN_NAME) ==0)
      {
        gboolean dialogOk;
        gint32 lowerLayerId;
        
        lowerLayerId = p_findLowerLayer(image_id, upperLayerId);

        dialogOk = p_dialog(&fiVals, upperLayerId, lowerLayerId);
        if( dialogOk != TRUE)
        {
          status = GIMP_PDB_CALLING_ERROR;
        }

      }
      doProgress = TRUE;
      doFlush =  TRUE;
      break;

    case GIMP_RUN_NONINTERACTIVE:
      /* check to see if invoked with the correct number of parameters */
      if (nparams == global_number_in_args)
      {
          fiVals.targetLum                = param[3].data.d_float;;
          fiVals.useRefLayerLum           = param[4].data.d_int32;
          fiVals.useRefLayerMsk           = param[5].data.d_int32;
          fiVals.refLayerId               = param[6].data.d_int32;
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
    gimp_image_undo_group_start (image_id);

    /* Run the main function */
    values[1].data.d_drawable =
          p_processing(image_id
                      , upperLayerId  /* the layer wher opacity is to be set */
                      , doProgress
                      , &fiVals
                      );

    gimp_image_undo_group_end (image_id);

    if(run_mode == GIMP_RUN_INTERACTIVE)
    {
      gimp_set_data (PLUG_IN_NAME, &fiVals, sizeof(fiVals));
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


