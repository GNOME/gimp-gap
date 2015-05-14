/* gap_water_pattern.c
 * 2011.01.31 hof (Wolfgang Hofer)
 *
 * Water pattern - This is a filter for The GIMP to generate a highlight pattern such as those found on the bottom of pools.
 * This implementation was ported from the script sf-will-wavetank.scm Copyright (C) 2010  William Morrison
 * to C and was adapted to run on a single layer as animated filter, using GIMP-GAP typical
 * iterator capabilities for animated apply. This provides the ability to apply the effect on already
 * existing layers or frames.
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
 *  (2011/01/31)  v1.0  hof: - created
 */

#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <gtk/gtk.h>

#include <libgimp/gimp.h>
#include <libgimp/gimpui.h>

#include "gap-intl.h"
#include "gap_lastvaldesc.h"
#include "gap_pdb_calls.h"

/* Defines */
#define PLUG_IN_NAME        "plug-in-waterpattern"
#define PLUG_IN_IMAGE_TYPES "RGB*, GRAY*"
#define PLUG_IN_AUTHOR      "Wolfgang Hofer (hof@gimp.org)"
#define PLUG_IN_COPYRIGHT   "Wolfgang Hofer"
#define PLUG_IN_DESCRIPTION "Generates a water pattern effect as if the active layer were on the bottom of a ripple tank."

#define PLUG_IN_DATA_ITER_FROM  "plug-in-waterpattern-ITER-FROM"
#define PLUG_IN_DATA_ITER_TO    "plug-in-waterpattern-ITER-TO"
#define PLUG_IN_HELP_ID         "plug-in-waterpattern"

#define BLEND_NUM_OVERLAY    0
#define BLEND_NUM_ADDITION   1
#define BLEND_NUM_SCREEN     2
#define BLEND_NUM_DODGE      3

#define N_RET_VALUES 4

#define GAP_WATERPATTERN_RESPONSE_RESET 1

typedef struct
{
  gdouble   scalex;
  gdouble   scaley;
  gint32    blendNum;
  gdouble   shiftPhaseX;
  gdouble   shiftPhaseY;
  gboolean  useHighlights;
  gdouble   highlightOpacity;
  gboolean  useDisplaceMap;
  gdouble   displaceStrength;

  gboolean  createImage;
  gint32    nframes;
  gint32    seed1;
  gint32    seed2;
  gint32    cloudLayer1;
  gint32    cloudLayer2;

} waterpattern_val_t;


typedef struct _WaterPatternDialog WaterPatternDialog;

struct _WaterPatternDialog
{
  gint          run;
  gint          show_progress;
  gint32          drawable_id;

  gboolean        createNewPatterns;         /* FALSE: use cloud layers 1/2 entered via dialog */
  gboolean        createNewPatternsDefault;  /* FALSE: use cloud layers 1/2 entered via dialog */
  gint32          existingCloud1Id;
  gint32          existingCloud2Id;
  gint32          countPotentialCloudLayers;

  GtkWidget       *shell;
  GtkWidget       *radio_blend_overlay;
  GtkWidget       *radio_blend_addition;
  GtkWidget       *radio_blend_screen;
  GtkWidget       *radio_blend_dodge;
  GtkObject       *nframes_spinbutton_adj;
  GtkWidget       *nframes_spinbutton;
  GtkObject       *highlightOpacity_spinbutton_adj;
  GtkWidget       *highlightOpacity_spinbutton;
  GtkObject       *displaceStrength_spinbutton_adj;
  GtkWidget       *displaceStrength_spinbutton;

  GtkObject       *shiftPhaseX_spinbutton_adj;
  GtkWidget       *shiftPhaseX_spinbutton;
  GtkObject       *shiftPhaseY_spinbutton_adj;
  GtkWidget       *shiftPhaseY_spinbutton;
  GtkObject       *scaleX_spinbutton_adj;
  GtkWidget       *scaleX_spinbutton;
  GtkObject       *scaleY_spinbutton_adj;
  GtkWidget       *scaleY_spinbutton;
  GtkObject       *seed1_spinbutton_adj;
  GtkWidget       *seed1_spinbutton;
  GtkObject       *seed2_spinbutton_adj;
  GtkWidget       *seed2_spinbutton;

  GtkWidget       *cloud1Combo;
  GtkWidget       *cloud2Combo;
  GtkWidget       *cloud1Label;
  GtkWidget       *cloud2Label;
  GtkWidget       *patternCheckbutton;
  GtkWidget       *patternLabel;
  GtkWidget       *createImageCheckbutton;
  GtkWidget       *useHighlichtsCheckbutton;
  GtkWidget       *useDisplaceMapCheckbutton;

  waterpattern_val_t *vals;
};


typedef struct
{
  GimpRunMode           run_mode;
  gint32                image_id;
  gint32                ref_image_id;

  gint32                width;
  gint32                height;
  GimpLayerModeEffects  blend_mode;
  gboolean              add_display_to_ref_image;
} waterpattern_context_t;


static void  p_int_default_cuvals(waterpattern_val_t *cuvals);
static void  p_check_for_valid_cloud_layers(waterpattern_val_t *cuvals);
static void  p_int_cuvals(waterpattern_val_t *cuvals);

static WaterPatternDialog *do_dialog (WaterPatternDialog *wcd, waterpattern_val_t *);
static void  query (void);
static void run(const gchar *name
           , gint nparams
           , const GimpParam *param
           , gint *nreturn_vals
           , GimpParam **return_vals);

/* Global Variables */
GimpPlugInInfo PLUG_IN_INFO =
{
  NULL,   /* init_proc  */
  NULL,   /* quit_proc  */
  query,  /* query_proc */
  run     /* run_proc   */
};


gint  gap_debug = 0;  /* 0.. no debug, 1 .. print debug messages */

/* --------------
 * procedures
 * --------------
 */

/* -----------------------------------------
 * p_int_default_cuvals
 * p_int_default_cuvals_without_cloud_layers
 * -----------------------------------------
 *
 */
static void
p_int_default_cuvals_without_cloud_layers(waterpattern_val_t *cuvals)
{
  cuvals->scalex = 2.0;
  cuvals->scaley = 5.0;
  cuvals->blendNum = 0;
  cuvals->shiftPhaseX = 0.0;
  cuvals->shiftPhaseY = 0.0;
  cuvals->useHighlights = 1;
  cuvals->useDisplaceMap = 1;
  cuvals->displaceStrength = 0.01;
  cuvals->highlightOpacity = 50.0;
  cuvals->createImage = FALSE;
  cuvals->nframes = 50;
  cuvals->seed1 = 0;
  cuvals->seed2 = 0;

}  /* end p_int_default_cuvals_without_cloud_layers */

static void
p_int_default_cuvals(waterpattern_val_t *cuvals)
{
  p_int_default_cuvals_without_cloud_layers(cuvals);
  cuvals->cloudLayer1 = -1;
  cuvals->cloudLayer2 = -1;

}  /* end p_int_default_cuvals */

/* --------------------------------------
 * p_check_for_valid_cloud_layers
 * --------------------------------------
 *
 */
static void
p_check_for_valid_cloud_layers(waterpattern_val_t *cuvals)
{
  if(gimp_drawable_is_valid(cuvals->cloudLayer1) != TRUE)
  {
    cuvals->cloudLayer1 = -1;
  }
  if(gimp_drawable_is_valid(cuvals->cloudLayer2) != TRUE)
  {
    cuvals->cloudLayer2 = -1;
  }
}  /* end p_check_for_valid_cloud_layers */


/* --------------------------------------
 * p_int_cuvals
 * --------------------------------------
 *
 */
static void
p_int_cuvals(waterpattern_val_t *cuvals)
{
  p_int_default_cuvals(cuvals);

  /* Possibly retrieve data from a previous run */
  gimp_get_data (PLUG_IN_NAME, cuvals);

  p_check_for_valid_cloud_layers(cuvals);

}  /* end p_int_cuvals */



/* --------------------------------------
 * p_convertBlendNum_to_BlendMode
 * --------------------------------------
 */
static GimpLayerModeEffects
p_convertBlendNum_to_BlendMode(gint32 blendNum)
{
  GimpLayerModeEffects blendMode = GIMP_OVERLAY_MODE;
  switch(blendNum)
  {
      case BLEND_NUM_OVERLAY:
        blendMode = GIMP_OVERLAY_MODE;  /* 5 */
        break;
      case BLEND_NUM_ADDITION:
        blendMode = GIMP_ADDITION_MODE; /* 7 */
        break;
      case BLEND_NUM_SCREEN:
        blendMode = GIMP_SCREEN_MODE;  /* 4 */
        break;
      case BLEND_NUM_DODGE:
        blendMode = GIMP_DODGE_MODE; /* 16 */
        break;
      default:
        printf("unsupported blend mode, using default value Overlay\n");
        break;
  }

  return (blendMode);
}


/* --------------------------------------
 * p_init_context_and_cloud_layers
 * --------------------------------------
 *
 */
static gboolean
p_init_context_and_cloud_layers(gint32 drawable_id, waterpattern_val_t *cuvals, waterpattern_context_t  *ctxt)
{
  gboolean success;

  ctxt->image_id = gimp_drawable_get_image(drawable_id);
  ctxt->blend_mode = p_convertBlendNum_to_BlendMode(cuvals->blendNum);
  ctxt->width = gimp_image_width(ctxt->image_id);
  ctxt->height = gimp_image_height(ctxt->image_id);
  ctxt->add_display_to_ref_image = FALSE;

  success = TRUE;


  /* check if both cloud layers are already available */
  if((!gimp_drawable_is_valid(cuvals->cloudLayer1)) || (!gimp_drawable_is_valid(cuvals->cloudLayer2)))
  {
    /* create both cloud layers */
    GRand  *gr;
    gint32  seed;
    gr = g_rand_new ();

    ctxt->ref_image_id = gimp_image_new(ctxt->width, ctxt->height, GIMP_RGB);
    gimp_image_set_filename(ctxt->ref_image_id, "WaterPattern");

    cuvals->cloudLayer1 = gimp_layer_new(ctxt->ref_image_id
                                        , "Frame"
                                        , ctxt->width
                                        , ctxt->height
                                        , GIMP_RGBA_IMAGE
                                        , 100.0               /* full opaque */
                                        , GIMP_NORMAL_MODE    /* 0 */
                                        );
    cuvals->cloudLayer2 = gimp_layer_new(ctxt->ref_image_id
                                        , "diff"
                                        , ctxt->width
                                        , ctxt->height
                                        , GIMP_RGBA_IMAGE
                                        , 100.0                 /* full opaque */
                                        , GIMP_DIFFERENCE_MODE  /* 6 */
                                        );
    gimp_image_add_layer(ctxt->ref_image_id, cuvals->cloudLayer1, -1);
    gimp_image_add_layer(ctxt->ref_image_id, cuvals->cloudLayer2, -1);


    /* Adds the solid noise */
    seed = cuvals->seed1;
    if (seed == 0)
    {
      seed = g_rand_int_range(gr, 0, 2100000000);
    }
    success = gap_pdb_call_solid_noise(ctxt->ref_image_id
                          ,cuvals->cloudLayer1
                          ,1                       /* 1 create tileable output */
                          ,0                       /* turbulent noise 0 = no, 1 = yes */
                          ,seed                    /* rand(2100000000) */
                          ,1                       /* detail level */
                          ,cuvals->scalex          /* horizontal texture size */
                          ,cuvals->scaley          /* vertical texture size */
                          );
    if(success)
    {
      seed = cuvals->seed2;
      if (seed == 0)
      {
        seed = g_rand_int_range(gr, 0, 2100000000);
      }
      success = gap_pdb_call_solid_noise(ctxt->ref_image_id
                          ,cuvals->cloudLayer2
                          ,1
                          ,0
                          ,seed
                          ,1
                          ,cuvals->scalex
                          ,cuvals->scaley
                          );
    }

    if(success)
    {
      success = gap_pdb_call_normalize(ctxt->ref_image_id, cuvals->cloudLayer1);
    }
    if(success)
    {
      success = gap_pdb_call_normalize(ctxt->ref_image_id, cuvals->cloudLayer2);
    }

    g_rand_free (gr);
    ctxt->add_display_to_ref_image = TRUE;

  }

  return (success);

}  /* end p_init_context_and_cloud_layers */

/* --------------------------------------
 * p_cloud_size_check
 * --------------------------------------
 * check if layer is same size as processed image size (as specified in the context)
 */
static void
p_cloud_size_check(gint32 layer_id, waterpattern_context_t  *ctxt)
{
  if( (gimp_drawable_width(layer_id) != ctxt->width)
  ||  (gimp_drawable_height(layer_id) != ctxt->height) )
  {
    gimp_layer_scale(layer_id, ctxt->width, ctxt->height, TRUE);
    gimp_layer_set_offsets(layer_id, 0, 0);
  }
}  /* end p_cloud_size_check */


/* --------------------------------------
 * p_run_renderWaterPattern
 * --------------------------------------
 * renders the water pattern effect onto the specified drawable.
 * returns TRUE on success
 */
static gboolean
p_run_renderWaterPattern(gint32 drawable_id, waterpattern_val_t *cuvals, waterpattern_context_t  *ctxt)
{
  #define NUMBER_VAL_ARRAY_ELEMENTS 6
  static guchar curveValuesArray[NUMBER_VAL_ARRAY_ELEMENTS] = { 0, 255, 64, 64, 255, 0 };
  gint32 templayer_id;
  gint32 newlayer1_id = -1;
  gint32 newlayer2_id = -1;
  gint32 displace_layer_id = -1;
  gboolean isVisible;
  gboolean success;
  gint32   count;
  gint32   nframesToProcess;



  if (!gimp_drawable_is_layer(drawable_id))
  {
    g_message(_("drawable:%d is not a layer\n"), drawable_id);
    return (FALSE);
  }

  /* save visibility status of processed layer .. */
  isVisible = gimp_drawable_get_visible(drawable_id);
  /* .. and force visibility (required for merge down effects) */
  gimp_drawable_set_visible(drawable_id, TRUE);

  templayer_id = drawable_id;
  nframesToProcess = 1;

  gimp_layer_resize_to_image_size(drawable_id);
  success = p_init_context_and_cloud_layers(drawable_id, cuvals, ctxt);

  if (cuvals->createImage)
  {
    ctxt->image_id = gimp_image_new(ctxt->width, ctxt->height, GIMP_RGB);
    /* dont save history during the creation of the animation layers */
    gimp_image_undo_freeze(ctxt->image_id);
    templayer_id = -1;

    if (cuvals->nframes > 1)
    {
      nframesToProcess = cuvals->nframes;
    }
  }



  if(gap_debug)
  {
     printf("p_run_renderWaterPattern: drawable_id :%d (%s)\n", (int)drawable_id, gimp_drawable_get_name(drawable_id));
     printf("p_run_renderWaterPattern:  scalex:%f\n", (float)cuvals->scalex);
     printf("p_run_renderWaterPattern:  scaley:%f\n", (float)cuvals->scaley);
     printf("p_run_renderWaterPattern:  blendNum:%d\n", (int)cuvals->blendNum);
     printf("p_run_renderWaterPattern:  shiftPhaseX:%f\n", (float)cuvals->shiftPhaseX);
     printf("p_run_renderWaterPattern:  shiftPhaseY:%f\n", (float)cuvals->shiftPhaseY);
     printf("p_run_renderWaterPattern:  useHighlights:%d\n", (int)cuvals->useHighlights);
     printf("p_run_renderWaterPattern:  highlightOpacity:%f\n", (float)cuvals->highlightOpacity);
     printf("p_run_renderWaterPattern:  useDisplaceMap:%d\n", (int)cuvals->useDisplaceMap);
     printf("p_run_renderWaterPattern:  displaceStrength:%f\n", (float)cuvals->displaceStrength);
     printf("p_run_renderWaterPattern:  createImage:%d\n", (int)cuvals->createImage);
     printf("p_run_renderWaterPattern:  nframes:%d\n", (int)cuvals->nframes);
     printf("p_run_renderWaterPattern:  seed1:%d\n", (int)cuvals->seed1);
     printf("p_run_renderWaterPattern:  seed2:%d\n", (int)cuvals->seed2);
     printf("p_run_renderWaterPattern:  cloudLayer1:%d (%s)\n", (int)cuvals->cloudLayer1, gimp_drawable_get_name(cuvals->cloudLayer1));
     printf("p_run_renderWaterPattern:  cloudLayer2:%d (%s)\n", (int)cuvals->cloudLayer2, gimp_drawable_get_name(cuvals->cloudLayer2));

     printf("p_run_renderWaterPattern:  ref_image_id:%d\n", (int)ctxt->ref_image_id);
     printf("p_run_renderWaterPattern:  image_id:%d\n", (int)ctxt->image_id);
     printf("p_run_renderWaterPattern:  width:%d\n", (int)ctxt->width);
     printf("p_run_renderWaterPattern:  height:%d\n", (int)ctxt->height);
     printf("p_run_renderWaterPattern:  blend_mode:%d\n", (int)ctxt->blend_mode);
     printf("p_run_renderWaterPattern:  success:%d\n", (int)success);

  }

  if(!success)
  {
    return (FALSE);
  }


  /* end while nframes loop */
  for(count=0; count < nframesToProcess; count++)
  {
    if (cuvals->createImage)
    {
      /* in case we are creating a multilayer anim in a new image
       * copy the drawable to a new layer in this new image
       */
      templayer_id = gimp_layer_new_from_drawable(drawable_id, ctxt->image_id);
      gimp_image_add_layer(ctxt->image_id, templayer_id, -1 /* -1 place above active layer */);
    }

    /* copy cloud layers from ref image to current processed image_id
     * at stackposition above the current processed layer (templayer_id)
     */
    newlayer1_id = gimp_layer_new_from_drawable(cuvals->cloudLayer1, ctxt->image_id);
    newlayer2_id = gimp_layer_new_from_drawable(cuvals->cloudLayer2, ctxt->image_id);

    gimp_image_set_active_layer(ctxt->image_id, templayer_id);
    gimp_image_add_layer(ctxt->image_id, newlayer1_id, -1 /* -1 place above active layer */);
    gimp_image_add_layer(ctxt->image_id, newlayer2_id, -1 /* -1 place above active layer */);

    p_cloud_size_check(newlayer1_id, ctxt);
    p_cloud_size_check(newlayer2_id, ctxt);

    /* offsets */
    {
      gdouble shiftx;
      gdouble shifty;
      gint32  intShiftx;
      gint32  intShifty;
      gint32  offsetx;
      gint32  offsety;

      if(nframesToProcess > 1)
      {
        gdouble phX;
        gdouble phY;

        phY = cuvals->shiftPhaseY;
        phX = cuvals->shiftPhaseX;
        
        if ((phX == 0.0) && (phY == 0.0))
        {
          phX = 1.0;
          phY = 1.0;
        }

        shiftx = (gdouble)count * (((gdouble)ctxt->width * phX) / (gdouble)nframesToProcess);
        shifty = (gdouble)count * (((gdouble)ctxt->height * phY) / (gdouble)nframesToProcess);
      }
      else
      {
        shiftx = (gdouble)ctxt->width * cuvals->shiftPhaseX;
        shifty = (gdouble)ctxt->height * cuvals->shiftPhaseY;
      }
      intShiftx = rint(shiftx);
      intShifty = rint(shifty);

      offsetx = intShiftx % ctxt->width;
      offsety = intShifty % ctxt->height;
      gimp_drawable_offset(newlayer2_id, 1 /* 1:Wrap around */, 0 /* fill type */, offsetx, offsety);

      offsetx = 0 - offsetx;
      offsety = 0 - offsety;
      gimp_drawable_offset(newlayer1_id, 1 /* 1:Wrap around */, 0 /* fill type */, offsetx, offsety);

      newlayer1_id = gimp_image_merge_down(ctxt->image_id, newlayer2_id, GIMP_EXPAND_AS_NECESSARY);

      if(cuvals->useDisplaceMap)
      {
        displace_layer_id = gimp_layer_new_from_drawable(newlayer1_id, ctxt->image_id);
      }

      /* call color curves tool with GimpHistogramChannel GIMP_HISTOGRAM_VALUE 0 */
      gimp_curves_spline(newlayer1_id, GIMP_HISTOGRAM_VALUE, NUMBER_VAL_ARRAY_ELEMENTS, curveValuesArray);

    }

    /* optional merge the highlights onto the processed layer */
    if(cuvals->useHighlights)
    {
      gimp_layer_set_mode(newlayer1_id, ctxt->blend_mode);
      gimp_layer_set_opacity(newlayer1_id, cuvals->highlightOpacity);
      templayer_id = gimp_image_merge_down(ctxt->image_id, newlayer1_id, GIMP_EXPAND_AS_NECESSARY);
    }
    else
    {
      gimp_image_remove_layer(ctxt->image_id, newlayer1_id);
    }

    if (cuvals->createImage)
    {
      gchar *layerName;

      layerName = g_strdup_printf("Frame_%03d", (int)count +1);
      gimp_drawable_set_name(templayer_id, layerName);
      g_free(layerName);
    }

    /* optional displaces the final result according to the displacement map */
    if(cuvals->useDisplaceMap)
    {
      gdouble displaceAmountX;
      gdouble displaceAmountY;


      displaceAmountX = cuvals->displaceStrength * (gdouble)ctxt->width;
      displaceAmountY = cuvals->displaceStrength * (gdouble)ctxt->height;

      success = gap_pdb_call_displace(ctxt->image_id, templayer_id
                       , displaceAmountX, displaceAmountY
                       , 1, 1
                       , displace_layer_id, displace_layer_id
                       , 1  /* 0:WRAP, 1: SMEAR, 2: BLACK */
                       );
      /* delete the displace_layer_id (that is not added to any image yet) */
      gimp_drawable_delete(displace_layer_id);
    }

    if(!success)
    {
      break;
    }

  }  /* end while nframes loop */


  /* restore visibility status of processed layer */
  gimp_drawable_set_visible(templayer_id, isVisible);

  if (cuvals->createImage)
  {
    gimp_image_undo_thaw(ctxt->image_id);
  }

  /* in case a new image was created in an interactive mode,
   * add a display for this newly created imge
   */
  if(ctxt->run_mode != GIMP_RUN_NONINTERACTIVE)
  {
    if (cuvals->createImage)
    {
      gimp_display_new(ctxt->image_id);
    }
    if(ctxt->add_display_to_ref_image)
    {
      gimp_display_new(ctxt->ref_image_id);
    }
  }

  return (success);

}  /* end p_run_renderWaterPattern */


/*
 * DIALOG and callback stuff
 */


/* --------------------------------------
 * on_blend_radio_callback
 * --------------------------------------
 */
static void
on_blend_radio_callback(GtkWidget *wgt, gpointer user_data)
{
  WaterPatternDialog *wcd;

  if(gap_debug)
  {
    printf("on_blend_radio_callback: START\n");
  }
  wcd = (WaterPatternDialog*)user_data;
  if(wcd != NULL)
  {
    if(wcd->vals != NULL)
    {
       if(wgt == wcd->radio_blend_overlay)     { wcd->vals->blendNum = BLEND_NUM_OVERLAY; }
       if(wgt == wcd->radio_blend_addition)    { wcd->vals->blendNum = BLEND_NUM_ADDITION; }
       if(wgt == wcd->radio_blend_screen)      { wcd->vals->blendNum = BLEND_NUM_SCREEN; }
       if(wgt == wcd->radio_blend_dodge)       { wcd->vals->blendNum = BLEND_NUM_DODGE; }

    }
  }
}

/* --------------------------------------
 * p_update_widget_sensitivity
 * --------------------------------------
 */
static void
p_update_widget_sensitivity (WaterPatternDialog *wcd)
{
  gboolean inverseCreateNewPatterns;


  /* createImage dependent widgets */
  gtk_widget_set_sensitive(wcd->nframes_spinbutton ,          wcd->vals->createImage);

  /* createNewPatterns dependent widgets */
  if(wcd->createNewPatterns)
  {
    inverseCreateNewPatterns = FALSE;
  }
  else
  {
    inverseCreateNewPatterns = TRUE;
  }
  gtk_widget_set_sensitive(wcd->scaleX_spinbutton ,            wcd->createNewPatterns);
  gtk_widget_set_sensitive(wcd->scaleY_spinbutton ,            wcd->createNewPatterns);
  gtk_widget_set_sensitive(wcd->seed1_spinbutton ,             wcd->createNewPatterns);
  gtk_widget_set_sensitive(wcd->seed2_spinbutton ,             wcd->createNewPatterns);
  gtk_widget_set_sensitive(wcd->cloud1Combo ,                  inverseCreateNewPatterns);
  gtk_widget_set_sensitive(wcd->cloud2Combo ,                  inverseCreateNewPatterns);


  /* highlight dependent widgets */
  gtk_widget_set_sensitive(wcd->radio_blend_overlay ,          wcd->vals->useHighlights);
  gtk_widget_set_sensitive(wcd->radio_blend_addition ,         wcd->vals->useHighlights);
  gtk_widget_set_sensitive(wcd->radio_blend_screen ,           wcd->vals->useHighlights);
  gtk_widget_set_sensitive(wcd->radio_blend_dodge ,            wcd->vals->useHighlights);
  gtk_widget_set_sensitive(wcd->highlightOpacity_spinbutton ,  wcd->vals->useHighlights);

  /* displace dependent widgets */
  gtk_widget_set_sensitive(wcd->displaceStrength_spinbutton ,  wcd->vals->useDisplaceMap);


}  /* end p_update_widget_sensitivity */


/* --------------------------------------
 * p_init_widget_values
 * --------------------------------------
 * update GUI widgets to reflect the current values.
 */
static void
p_init_widget_values(WaterPatternDialog *wcd)
{
  gint countClouds;
  if(wcd == NULL)
  {
    return;
  }
  if(wcd->vals == NULL)
  {
    return;
  }

  /* init spnbuttons */
  gtk_adjustment_set_value(GTK_ADJUSTMENT(wcd->nframes_spinbutton_adj)
                         , (gfloat)wcd->vals->nframes);
  gtk_adjustment_set_value(GTK_ADJUSTMENT(wcd->highlightOpacity_spinbutton_adj)
                         , (gfloat)wcd->vals->highlightOpacity);
  gtk_adjustment_set_value(GTK_ADJUSTMENT(wcd->displaceStrength_spinbutton_adj)
                         , (gfloat)wcd->vals->displaceStrength);
  gtk_adjustment_set_value(GTK_ADJUSTMENT(wcd->shiftPhaseX_spinbutton_adj)
                         , (gfloat)wcd->vals->shiftPhaseX);
  gtk_adjustment_set_value(GTK_ADJUSTMENT(wcd->shiftPhaseY_spinbutton_adj)
                         , (gfloat)wcd->vals->shiftPhaseY);
  gtk_adjustment_set_value(GTK_ADJUSTMENT(wcd->scaleX_spinbutton_adj)
                         , (gfloat)wcd->vals->scalex);
  gtk_adjustment_set_value(GTK_ADJUSTMENT(wcd->scaleY_spinbutton_adj)
                         , (gfloat)wcd->vals->scaley);
  gtk_adjustment_set_value(GTK_ADJUSTMENT(wcd->seed1_spinbutton_adj)
                         , (gfloat)wcd->vals->seed1);
  gtk_adjustment_set_value(GTK_ADJUSTMENT(wcd->seed2_spinbutton_adj)
                         , (gfloat)wcd->vals->seed2);



  /* init checkbuttons */
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (wcd->createImageCheckbutton)
                               , wcd->vals->createImage);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (wcd->patternCheckbutton)
                               , wcd->createNewPatternsDefault);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (wcd->useHighlichtsCheckbutton)
                               , wcd->vals->useHighlights);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (wcd->useDisplaceMapCheckbutton)
                               , wcd->vals->useDisplaceMap);

  /* init radiobuttons */
  if(wcd->vals->blendNum == BLEND_NUM_OVERLAY)
  {
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON (wcd->radio_blend_overlay), TRUE);
  }
  else if (wcd->vals->blendNum == BLEND_NUM_ADDITION)
  {
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON (wcd->radio_blend_addition), TRUE);
  }
  else if (wcd->vals->blendNum == BLEND_NUM_SCREEN)
  {
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON (wcd->radio_blend_screen), TRUE);
  }
  else if (wcd->vals->blendNum == BLEND_NUM_DODGE)
  {
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON (wcd->radio_blend_dodge), TRUE);
  }

  countClouds = 0;
  if(gimp_drawable_is_valid(wcd->existingCloud1Id))
  {
    countClouds++;
  }
  if(gimp_drawable_is_valid(wcd->existingCloud2Id))
  {
    countClouds++;
  }

  if(countClouds == 2)
  {
    gimp_int_combo_box_set_active (GIMP_INT_COMBO_BOX (wcd->cloud1Combo), wcd->existingCloud1Id);
    gimp_int_combo_box_set_active (GIMP_INT_COMBO_BOX (wcd->cloud2Combo), wcd->existingCloud2Id);
  }

}  /* end p_init_widget_values */


/* --------------------------------------
 * on_gboolean_button_update
 * --------------------------------------
 */
static void
on_gboolean_button_update (GtkWidget *widget,
                           gpointer   data)
{
  WaterPatternDialog *wcd;
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

  wcd = (WaterPatternDialog *) g_object_get_data (G_OBJECT (widget), "wcd");
  if(wcd != NULL)
  {
    p_update_widget_sensitivity (wcd);
  }
}


/* ---------------------------------
 * p_waterpattern_response
 * ---------------------------------
 */
static void
p_waterpattern_response (GtkWidget *widget,
                 gint       response_id,
                 WaterPatternDialog *wcd)
{
  GtkWidget *dialog;

  switch (response_id)
  {
    case GAP_WATERPATTERN_RESPONSE_RESET:
      if(wcd)
      {
        /* rset default values */
        p_int_default_cuvals_without_cloud_layers(wcd->vals);
        p_init_widget_values(wcd);
        p_update_widget_sensitivity (wcd);
      }
      break;

    case GTK_RESPONSE_OK:
      if(wcd)
      {
        if (GTK_WIDGET_VISIBLE (wcd->shell))
        {
          gtk_widget_hide (wcd->shell);
        }
        wcd->run = TRUE;
      }

    default:
      dialog = NULL;
      if(wcd)
      {
        dialog = wcd->shell;
        if(dialog)
        {
          wcd->shell = NULL;
          gtk_widget_destroy (dialog);
        }
      }
      gtk_main_quit ();
      break;
  }
}  /* end p_waterpattern_response */



/* ------------------------------
 * p_cloud_layer_menu_callback
 * ------------------------------
 *
 */
static void
p_cloud_layer_menu_callback(GtkWidget *widget, gint32 *cloudLayerId)
{
  gint value;

  gimp_int_combo_box_get_active (GIMP_INT_COMBO_BOX (widget), &value);

  if(gap_debug)
  {
    printf("p_cloud_layer_menu_callback: cloudLayerAddr:%d value:%d\n"
      ,(int)cloudLayerId
      ,(int)value
      );
  }

  if(cloudLayerId != NULL)
  {
    *cloudLayerId = value;
  }


} /* end p_cloud_layer_menu_callback */


/* ------------------------------
 * p_pattern_layer_constrain
 * ------------------------------
 *
 */
static gint
p_pattern_layer_constrain(gint32 image_id, gint32 drawable_id, WaterPatternDialog *wcd)
{
  gint32 processedImageId;

  if(gap_debug)
  {
    printf("p_pattern_layer_constrain PROCEDURE image_id:%d drawable_id:%d wcd:%d\n"
                          ,(int)image_id
                          ,(int)drawable_id
                          ,(int)wcd
                          );
  }

  if(drawable_id < 0)
  {
     /* gimp 1.1 makes a first call of the constraint procedure
      * with drawable_id = -1, and skips the whole image if FALSE is returned
      */
     return(TRUE);
  }

  if(gimp_drawable_is_valid(drawable_id) != TRUE)
  {
     return(FALSE);
  }

  processedImageId = gimp_drawable_get_image(wcd->drawable_id);

  if(image_id == processedImageId)
  {
    return (FALSE);
  }

  if(!gimp_drawable_is_rgb(drawable_id))
  {
    if(!gimp_drawable_is_gray(drawable_id))
    {
      return (FALSE);
    }
  }

  wcd->countPotentialCloudLayers++;

  return(TRUE);

} /* end p_pattern_layer_constrain */


/* ------------------------------
 * do_dialog
 * ------------------------------
 * create and show the dialog window
 */
static WaterPatternDialog *
do_dialog (WaterPatternDialog *wcd, waterpattern_val_t *cuvals)
{
  GtkWidget  *vbox;

  GtkWidget *dialog1;
  GtkWidget *dialog_vbox1;
  GtkWidget *frame1;
  GtkWidget *vbox1;
  GtkWidget *label;
  GSList *vbox1_group = NULL;
  GtkWidget *radiobutton;
  GtkWidget *table1;
  GtkObject *spinbutton_adj;
  GtkWidget *spinbutton;
  GtkWidget *dialog_action_area1;
  GtkWidget *checkbutton;
  GtkWidget *combo;
  gint       countClouds;
  gint       row;


  /* Init UI  */
  gimp_ui_init ("waterpattern", FALSE);


  /*  The dialog1  */
  wcd->run = FALSE;
  wcd->vals = cuvals;
  wcd->createNewPatternsDefault = TRUE;
  wcd->existingCloud1Id = -1;
  wcd->existingCloud2Id = -1;
  countClouds = 0;
  wcd->countPotentialCloudLayers = 0;
  if(gimp_drawable_is_valid(cuvals->cloudLayer1))
  {
    countClouds++;
    wcd->existingCloud1Id = cuvals->cloudLayer1;
  }
  if(gimp_drawable_is_valid(cuvals->cloudLayer2))
  {
    countClouds++;
    wcd->existingCloud2Id = cuvals->cloudLayer2;
  }

  if(countClouds == 2)
  {
    /* both cloud layers from aprevious run are still valid
     * in this case createNewPatterns checkbutton shall be FALSE as default
     */
    wcd->createNewPatternsDefault = FALSE;
  }
  wcd->createNewPatterns = wcd->createNewPatternsDefault;

  /*  The dialog1 and main vbox  */
  dialog1 = gimp_dialog_new (_("Water-Pattern"), "water_pattern",
                               NULL, 0,
                               gimp_standard_help_func, PLUG_IN_HELP_ID,

                               GIMP_STOCK_RESET, GAP_WATERPATTERN_RESPONSE_RESET,
                               GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                               GTK_STOCK_OK,     GTK_RESPONSE_OK,
                               NULL);

  wcd->shell = dialog1;


  /*
   * g_object_set_data (G_OBJECT (dialog1), "dialog1", dialog1);
   * gtk_window_set_title (GTK_WINDOW (dialog1), _("dialog1"));
   */


  g_signal_connect (G_OBJECT (dialog1), "response",
                      G_CALLBACK (p_waterpattern_response),
                      wcd);

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
  frame1 = gimp_frame_new (_("Animation options"));

  gtk_widget_show (frame1);
  gtk_box_pack_start (GTK_BOX (dialog_vbox1), frame1, TRUE, TRUE, 0);
  gtk_container_set_border_width (GTK_CONTAINER (frame1), 2);

  vbox1 = gtk_vbox_new (FALSE, 0);
  gtk_container_add (GTK_CONTAINER (frame1), vbox1);
  gtk_widget_show (vbox1);


  /* table1 for spinbuttons  */
  table1 = gtk_table_new (4, 2, FALSE);
  gtk_widget_show (table1);
  gtk_box_pack_start (GTK_BOX (vbox1), table1, TRUE, TRUE, 0);
  gtk_container_set_border_width (GTK_CONTAINER (table1), 4);


  row = 0;

  /* createImage checkbutton  */
  label = gtk_label_new (_("Create Image:"));
  gtk_widget_show (label);
  gtk_table_attach (GTK_TABLE (table1), label, 0, 1, row, row+1,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);
  gtk_misc_set_alignment (GTK_MISC (label), 0, 0.5);


  checkbutton = gtk_check_button_new_with_label (" ");
  g_object_set_data (G_OBJECT (checkbutton), "wcd", wcd);
  wcd->createImageCheckbutton = checkbutton;
  gtk_widget_show (checkbutton);
  gimp_help_set_help_data (checkbutton, _("ON: create a new image with n copies of the input drawable and render complete animation effect on those copies. OFF: render only one phase of the animation effect on the input drawable"), NULL);
  gtk_table_attach( GTK_TABLE(table1), checkbutton, 1, 2, row, row+1,
                    GTK_FILL, 0, 0, 0 );
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (checkbutton), cuvals->createImage);
  g_signal_connect (checkbutton, "toggled",
                    G_CALLBACK (on_gboolean_button_update),
                    &cuvals->createImage);

  row++;

  label = gtk_label_new (_("N-Frames:"));
  gtk_widget_show (label);
  gtk_table_attach (GTK_TABLE (table1), label, 0, 1, row, row+1,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);
  gtk_misc_set_alignment (GTK_MISC (label), 0, 0.5);

  /* nframes spinbutton  */
  spinbutton_adj = gtk_adjustment_new (cuvals->nframes, 1.0, 500, 1.0, 10, 0);
  spinbutton = gtk_spin_button_new (GTK_ADJUSTMENT (spinbutton_adj), 1, 0);
  gimp_help_set_help_data (spinbutton, _("Number of frames to be rendered as layer in the newly created image."), NULL);
  gtk_widget_show (spinbutton);
  gtk_table_attach (GTK_TABLE (table1), spinbutton, 1, 2, row, row+1,
                    (GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);
  g_signal_connect (G_OBJECT (spinbutton_adj), "value_changed", G_CALLBACK (gimp_int_adjustment_update), &cuvals->nframes);
  wcd->nframes_spinbutton_adj = spinbutton_adj;
  wcd->nframes_spinbutton = spinbutton;

  row++;



  /* shiftPhaseX spinbutton  */
  label = gtk_label_new (_("Phase shift X:"));
  gtk_widget_show (label);
  gtk_table_attach (GTK_TABLE (table1), label, 0, 1, row, row+1,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);
  gtk_misc_set_alignment (GTK_MISC (label), 0, 0.5);
  spinbutton_adj = gtk_adjustment_new (cuvals->shiftPhaseX, 0.0, 100, 0.1, 1, 0);
  spinbutton = gtk_spin_button_new (GTK_ADJUSTMENT (spinbutton_adj), 1, 2);
  gimp_help_set_help_data (spinbutton, _("Horizontal shift phase where 1.0 refers to image width"), NULL);
  gtk_widget_show (spinbutton);
  gtk_table_attach (GTK_TABLE (table1), spinbutton, 1, 2, row, row+1,
                    (GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);
  g_signal_connect (G_OBJECT (spinbutton_adj), "value_changed", G_CALLBACK (gimp_double_adjustment_update), &cuvals->shiftPhaseX);
  wcd->shiftPhaseX_spinbutton_adj = spinbutton_adj;
  wcd->shiftPhaseX_spinbutton = spinbutton;


  /* shiftPhaseY spinbutton  */
  label = gtk_label_new (_("Y:"));
  gtk_widget_show (label);
  gtk_table_attach (GTK_TABLE (table1), label, 2, 3, row, row+1,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);
  gtk_misc_set_alignment (GTK_MISC (label), 0, 0.5);
  spinbutton_adj = gtk_adjustment_new (cuvals->shiftPhaseY, 0.0, 100, 0.1, 1, 0);
  spinbutton = gtk_spin_button_new (GTK_ADJUSTMENT (spinbutton_adj), 1, 2);
  gimp_help_set_help_data (spinbutton, _("Vertical shift phase where 1.0 refers to image height"), NULL);
  gtk_widget_show (spinbutton);
  gtk_table_attach (GTK_TABLE (table1), spinbutton, 3, 4, row, row+1,
                    (GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);
  g_signal_connect (G_OBJECT (spinbutton_adj), "value_changed", G_CALLBACK (gimp_double_adjustment_update), &cuvals->shiftPhaseY);
  wcd->shiftPhaseY_spinbutton_adj = spinbutton_adj;
  wcd->shiftPhaseY_spinbutton = spinbutton;




  /* the frame */
  frame1 = gimp_frame_new (_("Pattern options"));

  gtk_widget_show (frame1);
  gtk_box_pack_start (GTK_BOX (dialog_vbox1), frame1, TRUE, TRUE, 0);
  gtk_container_set_border_width (GTK_CONTAINER (frame1), 2);


  vbox1 = gtk_vbox_new (FALSE, 0);
  gtk_container_add (GTK_CONTAINER (frame1), vbox1);
  gtk_widget_show (vbox1);



  /* table1 for spinbuttons  */
  table1 = gtk_table_new (4, 2, FALSE);
  gtk_widget_show (table1);
  gtk_box_pack_start (GTK_BOX (vbox1), table1, TRUE, TRUE, 0);
  gtk_container_set_border_width (GTK_CONTAINER (table1), 4);



  row = 0;
  /* use existing Patterns checkbutton  */
  label = gtk_label_new (_("Create Pattern:"));
  gtk_widget_show (label);
  wcd->patternLabel = label;
  gtk_table_attach (GTK_TABLE (table1), label, 0, 1, row, row+1,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);
  gtk_misc_set_alignment (GTK_MISC (label), 0, 0.5);

  checkbutton = gtk_check_button_new_with_label (" ");
  g_object_set_data (G_OBJECT (checkbutton), "wcd", wcd);
  wcd->patternCheckbutton = checkbutton;
  gimp_help_set_help_data (checkbutton, _("ON: create waterpattern cloud layers according options. OFF: Use external pattern layers. "), NULL);
  gtk_widget_show (checkbutton);
  gtk_table_attach( GTK_TABLE(table1), checkbutton, 1, 2, row, row+1,
                    GTK_FILL, 0, 0, 0 );
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (checkbutton), wcd->createNewPatterns);
  g_signal_connect (checkbutton, "toggled",
                    G_CALLBACK (on_gboolean_button_update),
                    &wcd->createNewPatterns);
  row++;

  /* pattern  */
  label = gtk_label_new (_("Layer Pattern 1:"));
  wcd->cloud1Label = label;
  gtk_widget_show (label);
  gtk_table_attach (GTK_TABLE (table1), label, 0, 1, row, row+1,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);
  gtk_misc_set_alignment (GTK_MISC (label), 0, 0.5);

  combo = gimp_layer_combo_box_new (p_pattern_layer_constrain, wcd);
  wcd->cloud1Combo = combo;
  gtk_widget_show(combo);
  gtk_table_attach(GTK_TABLE(table1), combo, 1, 4, row, row+1,
                   GTK_EXPAND | GTK_FILL, 0, 0, 0);

  gimp_help_set_help_data(combo,
                       _("Select an already existing pattern layer (from previous run)")
                       , NULL);

  gimp_int_combo_box_connect (GIMP_INT_COMBO_BOX (combo),
                              wcd->existingCloud1Id,                      /* initial value */
                              G_CALLBACK (p_cloud_layer_menu_callback),
                              &wcd->existingCloud1Id);

  row++;

  label = gtk_label_new (_("Layer Pattern 2:"));
  gtk_widget_show (label);
  wcd->cloud2Label = label;
  gtk_table_attach (GTK_TABLE (table1), label, 0, 1, row, row+1,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);
  gtk_misc_set_alignment (GTK_MISC (label), 0, 0.5);

  combo = gimp_layer_combo_box_new (p_pattern_layer_constrain, wcd);
  wcd->cloud2Combo = combo;
  gtk_widget_show(combo);
  gtk_table_attach(GTK_TABLE(table1), combo, 1, 4, row, row+1,
                   GTK_EXPAND | GTK_FILL, 0, 0, 0);

  gimp_help_set_help_data(combo,
                       _("Select an already existing pattern layer (from previous run)")
                       , NULL);

  gimp_int_combo_box_connect (GIMP_INT_COMBO_BOX (combo),
                              wcd->existingCloud2Id,                      /* initial value */
                              G_CALLBACK (p_cloud_layer_menu_callback),
                              &wcd->existingCloud2Id);

  row++;


  /* scalex spinbutton  */
  label = gtk_label_new (_("Scale Pattern X:"));
  gtk_widget_show (label);
  gtk_table_attach (GTK_TABLE (table1), label, 0, 1, row, row+1,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);
  gtk_misc_set_alignment (GTK_MISC (label), 0, 0.5);

  spinbutton_adj = gtk_adjustment_new (cuvals->scalex, 0.1, 16, 0.1, 1, 0);
  spinbutton = gtk_spin_button_new (GTK_ADJUSTMENT (spinbutton_adj), 1, 2);
  gimp_help_set_help_data (spinbutton, _("Horizontal scaling of the random patterns that are created for rendering (cloud1 and cloud2 layers)"), NULL);
  gtk_widget_show (spinbutton);
  gtk_table_attach (GTK_TABLE (table1), spinbutton, 1, 2, row, row+1,
                    (GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);
  g_signal_connect (G_OBJECT (spinbutton_adj), "value_changed", G_CALLBACK (gimp_double_adjustment_update), &cuvals->scalex);
  wcd->scaleX_spinbutton_adj = spinbutton_adj;
  wcd->scaleX_spinbutton = spinbutton;


  label = gtk_label_new (_("Y:"));
  gtk_widget_show (label);
  gtk_table_attach (GTK_TABLE (table1), label, 2, 3, row, row+1,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);
  gtk_misc_set_alignment (GTK_MISC (label), 0, 0.5);

  spinbutton_adj = gtk_adjustment_new (cuvals->scaley, 0.1, 16, 0.1, 1, 0);
  spinbutton = gtk_spin_button_new (GTK_ADJUSTMENT (spinbutton_adj), 1, 2);
  gimp_help_set_help_data (spinbutton, _("Vertical scaling of the random patterns that are created for rendering (cloud1 and cloud2 layers)"), NULL);
  gtk_widget_show (spinbutton);
  gtk_table_attach (GTK_TABLE (table1), spinbutton, 3, 4, row, row+1,
                    (GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);
  g_signal_connect (G_OBJECT (spinbutton_adj), "value_changed", G_CALLBACK (gimp_double_adjustment_update), &cuvals->scaley);
  wcd->scaleY_spinbutton_adj = spinbutton_adj;
  wcd->scaleY_spinbutton = spinbutton;


  row++;

  label = gtk_label_new (_("Seed Pattern 1:"));
  gtk_widget_show (label);
  gtk_table_attach (GTK_TABLE (table1), label, 0, 1, row, row+1,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);
  gtk_misc_set_alignment (GTK_MISC (label), 0, 0.5);

  /* seed1  spinbutton  */
  spinbutton_adj = gtk_adjustment_new (cuvals->seed1, 0.0, 2147483647, 1.0, 10, 0);
  spinbutton = gtk_spin_button_new (GTK_ADJUSTMENT (spinbutton_adj), 1, 0);
  gimp_help_set_help_data (spinbutton, _("Seed for creating random pattern (cloud1 layer) use 0 for random value."), NULL);
  gtk_widget_show (spinbutton);
  gtk_table_attach (GTK_TABLE (table1), spinbutton, 1, 2, row, row+1,
                    (GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);
  g_signal_connect (G_OBJECT (spinbutton_adj), "value_changed", G_CALLBACK (gimp_int_adjustment_update), &cuvals->seed1);
  wcd->seed1_spinbutton_adj = spinbutton_adj;
  wcd->seed1_spinbutton = spinbutton;


  label = gtk_label_new ("2:");
  gtk_widget_show (label);
  gtk_table_attach (GTK_TABLE (table1), label, 2, 3, row, row+1,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);
  gtk_misc_set_alignment (GTK_MISC (label), 0, 0.5);

  /* seed2 spinbutton  */
  spinbutton_adj = gtk_adjustment_new (cuvals->seed2, 0.0, 2147483647, 1.0, 10, 0);
  spinbutton = gtk_spin_button_new (GTK_ADJUSTMENT (spinbutton_adj), 1, 0);
  gimp_help_set_help_data (spinbutton, _("Seed for creating random pattern (cloud2 layer) use 0 for random value."), NULL);
  gtk_widget_show (spinbutton);
  gtk_table_attach (GTK_TABLE (table1), spinbutton, 3, 4, row, row+1,
                    (GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);
  g_signal_connect (G_OBJECT (spinbutton_adj), "value_changed", G_CALLBACK (gimp_int_adjustment_update), &cuvals->seed2);
  wcd->seed2_spinbutton_adj = spinbutton_adj;
  wcd->seed2_spinbutton = spinbutton;



  /* the frame */
  frame1 = gimp_frame_new (_("Render options"));

  gtk_widget_show (frame1);
  gtk_box_pack_start (GTK_BOX (dialog_vbox1), frame1, TRUE, TRUE, 0);
  gtk_container_set_border_width (GTK_CONTAINER (frame1), 2);


  vbox1 = gtk_vbox_new (FALSE, 0);
  gtk_container_add (GTK_CONTAINER (frame1), vbox1);
  gtk_widget_show (vbox1);



  /* table1 for spinbuttons  */
  table1 = gtk_table_new (4, 2, FALSE);
  gtk_widget_show (table1);
  gtk_box_pack_start (GTK_BOX (vbox1), table1, TRUE, TRUE, 0);
  gtk_container_set_border_width (GTK_CONTAINER (table1), 4);



  row = 0;

  row++;

  /* useHighlights checkbutton  */
  label = gtk_label_new (_("Use Highlights:"));
  gtk_widget_show (label);
  gtk_table_attach (GTK_TABLE (table1), label, 0, 1, row, row+1,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);
  gtk_misc_set_alignment (GTK_MISC (label), 0, 0.5);

  checkbutton = gtk_check_button_new_with_label (" ");
  g_object_set_data (G_OBJECT (checkbutton), "wcd", wcd);
  wcd->useHighlichtsCheckbutton = checkbutton;
  gimp_help_set_help_data (checkbutton, _("Render water pattern highlight effect"), NULL);
  gtk_widget_show (checkbutton);
  gtk_table_attach( GTK_TABLE(table1), checkbutton, 1, 2, row, row+1,
                    GTK_FILL, 0, 0, 0 );
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (checkbutton), cuvals->useHighlights);
  g_signal_connect (checkbutton, "toggled",
                    G_CALLBACK (on_gboolean_button_update),
                    &cuvals->useHighlights);


  label = gtk_label_new (_("Opacity:"));
  gtk_widget_show (label);
  gtk_table_attach (GTK_TABLE (table1), label, 2, 3, row, row+1,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);
  gtk_misc_set_alignment (GTK_MISC (label), 0, 0.5);


  /* highlightOpacity spinbutton  */
  spinbutton_adj = gtk_adjustment_new (cuvals->highlightOpacity, 0.0, 100, 1.0, 10, 0);
  spinbutton = gtk_spin_button_new (GTK_ADJUSTMENT (spinbutton_adj), 1, 3);
  gimp_help_set_help_data (spinbutton, _("The highlight strength (e.g. opacity)"), NULL);
  gtk_widget_show (spinbutton);
  gtk_table_attach (GTK_TABLE (table1), spinbutton, 3, 4, row, row+1,
                    (GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);
  g_signal_connect (G_OBJECT (spinbutton_adj), "value_changed", G_CALLBACK (gimp_double_adjustment_update), &cuvals->highlightOpacity);
  wcd->highlightOpacity_spinbutton_adj = spinbutton_adj;
  wcd->highlightOpacity_spinbutton = spinbutton;

  row++;

  /* Highlights blend mode  */
  label = gtk_label_new (_("Blend Mode:"));
  gtk_widget_show (label);
  gtk_table_attach (GTK_TABLE (table1), label, 0, 1, row, row+1,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);
  gtk_misc_set_alignment (GTK_MISC (label), 0, 0.5);


  vbox1 = gtk_vbox_new (FALSE, 0);

  gtk_widget_show (vbox1);
  gtk_table_attach (GTK_TABLE (table1), vbox1, 3, 4, row, row+1,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);


  /* Blend Mode the radio buttons */

  radiobutton = gtk_radio_button_new_with_label (vbox1_group, _("Overlay"));
  wcd->radio_blend_overlay = radiobutton;
  vbox1_group = gtk_radio_button_get_group (GTK_RADIO_BUTTON (radiobutton));
  gtk_widget_show (radiobutton);
  gtk_box_pack_start (GTK_BOX (vbox1), radiobutton, FALSE, FALSE, 0);
  g_signal_connect (G_OBJECT (radiobutton),     "clicked",  G_CALLBACK (on_blend_radio_callback), wcd);
  if(wcd->vals->blendNum == BLEND_NUM_OVERLAY)
  {
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON (radiobutton), TRUE);
  }

  radiobutton = gtk_radio_button_new_with_label (vbox1_group, _("Addition"));
  wcd->radio_blend_addition = radiobutton;
  vbox1_group = gtk_radio_button_get_group (GTK_RADIO_BUTTON (radiobutton));
  gtk_widget_show (radiobutton);
  gtk_box_pack_start (GTK_BOX (vbox1), radiobutton, FALSE, FALSE, 0);
  g_signal_connect (G_OBJECT (radiobutton),     "clicked",  G_CALLBACK (on_blend_radio_callback), wcd);
  if(wcd->vals->blendNum == BLEND_NUM_ADDITION)
  {
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON (radiobutton), TRUE);
  }

  radiobutton = gtk_radio_button_new_with_label (vbox1_group, _("Screen"));
  wcd->radio_blend_screen = radiobutton;
  vbox1_group = gtk_radio_button_get_group (GTK_RADIO_BUTTON (radiobutton));
  gtk_widget_show (radiobutton);
  gtk_box_pack_start (GTK_BOX (vbox1), radiobutton, FALSE, FALSE, 0);
  g_signal_connect (G_OBJECT (radiobutton),     "clicked",  G_CALLBACK (on_blend_radio_callback), wcd);
  if(wcd->vals->blendNum == BLEND_NUM_SCREEN)
  {
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON (radiobutton), TRUE);
  }

  radiobutton = gtk_radio_button_new_with_label (vbox1_group, _("Dodge"));
  wcd->radio_blend_dodge = radiobutton;
  vbox1_group = gtk_radio_button_get_group (GTK_RADIO_BUTTON (radiobutton));
  gtk_widget_show (radiobutton);
  gtk_box_pack_start (GTK_BOX (vbox1), radiobutton, FALSE, FALSE, 0);
  g_signal_connect (G_OBJECT (radiobutton),     "clicked",  G_CALLBACK (on_blend_radio_callback), wcd);
  if(wcd->vals->blendNum == BLEND_NUM_DODGE)
  {
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON (radiobutton), TRUE);
  }





  row++;

  /* useDisplaceMap checkbutton  */
  label = gtk_label_new (_("Use Displace Map:"));
  gtk_widget_show (label);
  gtk_table_attach (GTK_TABLE (table1), label, 0, 1, row, row+1,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);
  gtk_misc_set_alignment (GTK_MISC (label), 0, 0.5);


  checkbutton = gtk_check_button_new_with_label (" ");
  g_object_set_data (G_OBJECT (checkbutton), "wcd", wcd);
  wcd->useDisplaceMapCheckbutton = checkbutton;
  gimp_help_set_help_data (checkbutton, _("Render water pattern distortion effect (by applying a generated displace map)"), NULL);
  gtk_widget_show (checkbutton);
  gtk_table_attach( GTK_TABLE(table1), checkbutton, 1, 2, row, row+1,
                    GTK_FILL, 0, 0, 0 );
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (checkbutton), cuvals->useDisplaceMap);
  g_signal_connect (checkbutton, "toggled",
                    G_CALLBACK (on_gboolean_button_update),
                    &cuvals->useDisplaceMap);

  label = gtk_label_new (_("Strength:"));
  gtk_widget_show (label);
  gtk_table_attach (GTK_TABLE (table1), label, 2, 3, row, row+1,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);
  gtk_misc_set_alignment (GTK_MISC (label), 0, 0.5);

  /* displaceStrength spinbutton  */
  spinbutton_adj = gtk_adjustment_new (cuvals->displaceStrength, 0.0, 1.0, 0.01, 0.1, 0);
  spinbutton = gtk_spin_button_new (GTK_ADJUSTMENT (spinbutton_adj), 1, 3);
  gimp_help_set_help_data (spinbutton, _("The distortion displace strength"), NULL);
  gtk_widget_show (spinbutton);
  gtk_table_attach (GTK_TABLE (table1), spinbutton, 3, 4, row, row+1,
                    (GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);
  g_signal_connect (G_OBJECT (spinbutton_adj), "value_changed", G_CALLBACK (gimp_double_adjustment_update), &cuvals->displaceStrength);
  wcd->displaceStrength_spinbutton_adj = spinbutton_adj;
  wcd->displaceStrength_spinbutton = spinbutton;

  /* -- */


  dialog_action_area1 = GTK_DIALOG (dialog1)->action_area;
  gtk_widget_show (dialog_action_area1);
  gtk_container_set_border_width (GTK_CONTAINER (dialog_action_area1), 10);

  p_init_widget_values(wcd);
  p_update_widget_sensitivity (wcd);


  gtk_widget_show (dialog1);

  gtk_main ();
  gdk_flush ();

  if((!wcd->createNewPatterns) && (wcd->countPotentialCloudLayers > 0))
  {
    wcd->vals->cloudLayer1 = wcd->existingCloud1Id;
    wcd->vals->cloudLayer2 = wcd->existingCloud2Id;
  }
  else
  {
    /* use -1 to force creation of new clod layers */
    wcd->vals->cloudLayer1 = -1;
    wcd->vals->cloudLayer2 = -1;
  }

  return wcd;

}  /* end do_dialog */





MAIN ()

/* --------------------------------------
 * query
 * --------------------------------------
 *
 */
static void
query (void)
{
  static GimpParamDef args[] = {
                  { GIMP_PDB_INT32,      "run_mode", "Interactive, non-interactive"},
                  { GIMP_PDB_IMAGE,      "image", "Input image" },
                  { GIMP_PDB_DRAWABLE,   "drawable", "Input drawable (must be a layer without layermask)"},
                  { GIMP_PDB_FLOAT,      "scalex", "Horizontal scale (0.1 upto 16.0)"},
                  { GIMP_PDB_FLOAT,      "scaley", "Vertical scale (0.1 upto 16.0)"},
                  { GIMP_PDB_INT32,      "blendNum", "Blend mode { OVERLAY (0), ADDITION (1), SCREEN (2),  DODGE (3) }"},
                  { GIMP_PDB_FLOAT,      "shiftPhaseX", "Horizontal shift phase (0.0 to 1.0 full width, values >= 1.0 foldback to range 0.0 - 0.99999)"},
                  { GIMP_PDB_FLOAT,      "shiftPhaseY", "Vertical shift phase (0.0 to 1.0 full height, values >= 1.0 foldback to range 0.0 - 0.99999)"},
                  { GIMP_PDB_INT32,      "useHighlights", "TRUE (1): Apply watereffect highlights)"},
                  { GIMP_PDB_FLOAT,      "highlightOpacity", "highlight strength (0.0 to 100.0 opacity of the highlight effect, only relevant in case useHighlights is TRUE)"},
                  { GIMP_PDB_INT32,      "useDisplaceMap", "TRUE (1): Apply watereffect displace distortion"},
                  { GIMP_PDB_FLOAT,      "displaceStrength", "displace strength (0.0 to 1.0, only relevant in case useDisplaceMap is TRUE)"},
                  { GIMP_PDB_INT32,      "createImage", "TRUE (1): create a new image with n copies of the input drawable and render complete animation effect on those copies. FALSE (0): render only one phase of the anim effect on the input drawable"},
                  { GIMP_PDB_INT32,      "nframes", "number of layer to be created in case createImage option is TRUE (1)"},
                  { GIMP_PDB_INT32,      "seed1", "Seed for creating random pattern (cloud1 layer) use 0 for random value"},
                  { GIMP_PDB_INT32,      "seed2", "Seed for creating random pattern (cloud2 layer) use 0 for random value"},
                  { GIMP_PDB_DRAWABLE,   "cloudLayer1", "-1 automatically create cloud layer1 that is required to render or pass a layer id that was returned in a previous call to this plug-in (for rendering the next phase of the effect in one call per frame manner)"},
                  { GIMP_PDB_DRAWABLE,   "cloudLayer2", "-1 automatically create cloud layer2 that is required to render or pass a layer id that was returned in a previous call to this plug-in (for rendering the next phase of the effect in one call per frame manner)"}
  };
  static int nargs = sizeof(args) / sizeof(args[0]);

  static GimpParamDef return_vals[] =
  {
    { GIMP_PDB_DRAWABLE, "imageId",      "the image that was newly created (when creatImage is TRUE) or otherwise the image id tha contains the processed drawable" },
    { GIMP_PDB_DRAWABLE, "cloudLayer1",  "the pattern cloud layer1" },
    { GIMP_PDB_DRAWABLE, "cloudLayer2",  "the pattern cloud layer2" }
  };
  static int nreturn_vals = sizeof(return_vals) / sizeof(return_vals[0]);


  static waterpattern_val_t watervals;

  static GimpLastvalDef lastvals[] =
  {
    GIMP_LASTVALDEF_GDOUBLE     (GIMP_ITER_FALSE,  watervals.scalex,                  "scalex"),
    GIMP_LASTVALDEF_GDOUBLE     (GIMP_ITER_FALSE,  watervals.scaley,                  "scaley"),
    GIMP_LASTVALDEF_GINT32      (GIMP_ITER_FALSE,  watervals.blendNum,                "blendNum"),
    GIMP_LASTVALDEF_GDOUBLE     (GIMP_ITER_TRUE,   watervals.shiftPhaseX,             "shiftPhaseX"),
    GIMP_LASTVALDEF_GDOUBLE     (GIMP_ITER_TRUE,   watervals.shiftPhaseY,             "shiftPhaseY"),
    GIMP_LASTVALDEF_GBOOLEAN    (GIMP_ITER_FALSE,  watervals.useHighlights,           "useHighlights"),
    GIMP_LASTVALDEF_GDOUBLE     (GIMP_ITER_TRUE,   watervals.highlightOpacity,        "highlightOpacity"),
    GIMP_LASTVALDEF_GBOOLEAN    (GIMP_ITER_FALSE,  watervals.useDisplaceMap,          "useDisplaceMap"),
    GIMP_LASTVALDEF_GDOUBLE     (GIMP_ITER_TRUE,   watervals.displaceStrength,        "displaceStrength"),
    GIMP_LASTVALDEF_GBOOLEAN    (GIMP_ITER_FALSE,  watervals.createImage,             "createImage"),
    GIMP_LASTVALDEF_GINT32      (GIMP_ITER_FALSE,  watervals.nframes,                 "nframes"),
    GIMP_LASTVALDEF_GINT32      (GIMP_ITER_FALSE,  watervals.seed1,                   "seed1"),
    GIMP_LASTVALDEF_GINT32      (GIMP_ITER_FALSE,  watervals.seed2,                   "seed2"),
    GIMP_LASTVALDEF_DRAWABLE    (GIMP_ITER_TRUE,   watervals.cloudLayer1,             "cloudLayer1"),
    GIMP_LASTVALDEF_DRAWABLE    (GIMP_ITER_TRUE,   watervals.cloudLayer2,             "cloudLayer2")
  };

  gimp_plugin_domain_register (GETTEXT_PACKAGE, LOCALEDIR);

  /* registration for last values buffer structure (for animated filter apply) */
  gimp_lastval_desc_register(PLUG_IN_NAME,
                             &watervals,
                             sizeof(watervals),
                             G_N_ELEMENTS (lastvals),
                             lastvals);

  /* the actual installation of the waterpattern plugin */
  gimp_install_procedure (PLUG_IN_NAME,
                          PLUG_IN_DESCRIPTION,
                         "This Plugin generates an animated water effect that looks like the bottom of a ripple tank."
                         " This Plugin can render the animation in one call, where n copies of the input drawable is copied n-times to a newly created image. "
                         " To render just one frame/phase of the animated effect this plug-in can be called on multiple layers with varying shiftPhase parameters."
                         " (note that GIMP-GAP provides such animated filter calls) "
                          ,
                          PLUG_IN_AUTHOR,
                          PLUG_IN_COPYRIGHT,
                          GAP_VERSION_WITH_DATE,
                          N_("Water Pattern..."),
                          PLUG_IN_IMAGE_TYPES,
                          GIMP_PLUGIN,
                          nargs,
                          nreturn_vals,
                          args,
                          return_vals);



  {
    /* Menu names */
    const char *menupath_image_video_layer_colors = N_("<Image>/Video/Layer/Render/");

    gimp_plugin_menu_register (PLUG_IN_NAME, menupath_image_video_layer_colors);
  }

}  /* end query */


/* --------------------------------------
 * run
 * --------------------------------------
 */
static void
run(const gchar *name
           , gint nparams
           , const GimpParam *param
           , gint *nreturn_vals
           , GimpParam **return_vals)
{
  waterpattern_val_t l_cuvals;
  WaterPatternDialog   *wcd = NULL;
  waterpattern_context_t  waterpattern_context;
  waterpattern_context_t  *ctxt;

  gint32    l_image_id = -1;
  gint32    l_drawable_id = -1;
  gint32    l_handled_drawable_id = -1;



  /* Get the runmode from the in-parameters */
  GimpRunMode run_mode = param[0].data.d_int32;

  ctxt = &waterpattern_context;
  ctxt->run_mode = run_mode;

  /* status variable, use it to check for errors in invocation usualy only
     during non-interactive calling */
  GimpPDBStatusType status = GIMP_PDB_SUCCESS;

  /*always return at least the status to the caller. */
  static GimpParam values[N_RET_VALUES];


  INIT_I18N();

  /* initialize the return of the status */
  values[0].type = GIMP_PDB_STATUS;
  values[0].data.d_status = status;
  values[1].type = GIMP_PDB_DRAWABLE;
  values[1].data.d_int32 = -1;
  values[2].type = GIMP_PDB_DRAWABLE;
  values[2].data.d_int32 = -1;
  values[3].type = GIMP_PDB_DRAWABLE;
  values[3].data.d_int32 = -1;
  *nreturn_vals = N_RET_VALUES;
  *return_vals = values;


  /* get image and drawable */
  l_image_id = param[1].data.d_int32;
  l_drawable_id = param[2].data.d_drawable;

  /* Possibly retrieve data from a previous run, otherwise init with default values */
  p_int_cuvals(&l_cuvals);


  if(status == GIMP_PDB_SUCCESS)
  {
    /* how are we running today? */
    switch (run_mode)
     {
      case GIMP_RUN_INTERACTIVE:
        wcd = g_malloc (sizeof (WaterPatternDialog));
        wcd->run = FALSE;
        wcd->drawable_id = l_drawable_id;
        /* Get information from the dialog */
        do_dialog(wcd, &l_cuvals);
        wcd->show_progress = TRUE;
        break;

      case GIMP_RUN_NONINTERACTIVE:
        /* check to see if invoked with the correct number of parameters */
        if (nparams == 18)
        {

           l_cuvals.scalex            = param[3].data.d_float;
           l_cuvals.scalex            = param[4].data.d_float;
           l_cuvals.blendNum          = param[5].data.d_int32;
           l_cuvals.shiftPhaseX       = param[6].data.d_float;
           l_cuvals.shiftPhaseY       = param[7].data.d_float;
           l_cuvals.useHighlights     = param[8].data.d_int32;
           l_cuvals.highlightOpacity  = param[9].data.d_float;
           l_cuvals.useDisplaceMap    = param[10].data.d_int32;
           l_cuvals.displaceStrength  = param[11].data.d_float;
           l_cuvals.createImage       = param[12].data.d_int32;
           l_cuvals.nframes           = param[13].data.d_int32;
           l_cuvals.seed1             = param[14].data.d_int32;
           l_cuvals.seed2             = param[15].data.d_int32;
           l_cuvals.cloudLayer1       = param[16].data.d_drawable;
           l_cuvals.cloudLayer2       = param[17].data.d_drawable;

           p_check_for_valid_cloud_layers(&l_cuvals);
        }
        else
        {
          status = GIMP_PDB_CALLING_ERROR;
        }
        break;

      case GIMP_RUN_WITH_LAST_VALS:
        wcd = g_malloc (sizeof (WaterPatternDialog));
        wcd->run = TRUE;
        wcd->show_progress = TRUE;
        wcd->createNewPatterns = FALSE;
        /* Possibly retrieve data from a previous run */
        gimp_get_data (PLUG_IN_NAME, &l_cuvals);
        break;

      default:
        break;
    }
  }

  if (wcd == NULL)
  {
    status = GIMP_PDB_EXECUTION_ERROR;
  }

  if (status == GIMP_PDB_SUCCESS)
  {
     /* Run the main function */
     if(wcd->run)
     {
        gboolean success;

        gimp_image_undo_group_start (l_image_id);
        success = p_run_renderWaterPattern(l_drawable_id, &l_cuvals, ctxt);
        l_handled_drawable_id = l_drawable_id;
        gimp_image_undo_group_end (l_image_id);

        if(success)
        {
          /* Store variable states for next run */
          if (run_mode == GIMP_RUN_INTERACTIVE)
          {
            gimp_set_data(PLUG_IN_NAME, &l_cuvals, sizeof(l_cuvals));
          }
        }
        else
        {
          status = GIMP_PDB_EXECUTION_ERROR;       /* dialog ended with cancel button */
        }

     }
     else
     {
       status = GIMP_PDB_EXECUTION_ERROR;       /* dialog ended with cancel button */
     }

     /* If run mode is interactive, flush displays, else (script) don't
        do it, as the screen updates would make the scripts slow */
     if (run_mode != GIMP_RUN_NONINTERACTIVE)
     {
       gimp_displays_flush ();
     }
  }
  values[0].data.d_status = status;
  values[1].data.d_int32 = ctxt->image_id;          /* return the image id of handled layer (or of the newly created anim image */
  values[2].data.d_int32 = l_cuvals.cloudLayer1;    /* return the id of cloud1 layer */
  values[3].data.d_int32 = l_cuvals.cloudLayer2;    /* return the id of cloud2 layer */

}  /* end run */
