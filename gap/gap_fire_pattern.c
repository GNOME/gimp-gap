/* gap_fire_pattern.c
 * 2011.02.11 hof (Wolfgang Hofer)
 *
 * Fire pattern - This is a filter for The GIMP to generate an animated fire effect.
 * This implementation was ported from the script sf-will-flames.scm Copyright (C) 2010  William Morrison
 * to C and was adapted to run on a single layer as animated filter, using GIMP-GAP typical
 * iterator capabilities for animated apply. This provides the ability to apply the effect on already
 * existing layers or frames and adds the capability of varying shape and opacity of the flames
 * for each rendered frame.
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
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

/* Revision history
 *  (2011/02/11)  v1.0  hof: - created
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
#include "gap_libgapbase.h"

/* Defines */
#define PLUG_IN_NAME        "plug-in-firepattern"
#define PLUG_IN_IMAGE_TYPES "RGB*, GRAY*"
#define PLUG_IN_AUTHOR      "Wolfgang Hofer (hof@gimp.org)"
#define PLUG_IN_COPYRIGHT   "Wolfgang Hofer"
#define PLUG_IN_DESCRIPTION "Renders an animated fire effect as new layer(s) or optional onto the processed layer(s)."

#define PLUG_IN_DATA_ITER_FROM  "plug-in-firepattern-ITER-FROM"
#define PLUG_IN_DATA_ITER_TO    "plug-in-firepattern-ITER-TO"
#define PLUG_IN_HELP_ID         "plug-in-firepattern"

#define BLEND_NUM_BURN       0
#define BLEND_NUM_SUBTRACT   1
#define BLEND_NUM_MULTIPLY   2

#define N_RET_VALUES 4

#define GAP_FIREPATTERN_RESPONSE_RESET 1
#define DEFAULT_GRADIENT_NAME "Incandescent"
#define ACTIVE_GRADIENT_NAME  "<ACTIVE_GRADIENT>"

#define N_GRADIENT_COLORSAMPLES        256
#define BPP_SAMPLE_COLOR               4
#define LUMINOSITY(X)   (GIMP_RGB_LUMINANCE (X[0], X[1], X[2]) + 0.5)



#define MAX_GRADIENT_NAME 300

typedef struct
{
  /* params for creating the cloud layer (the pattern where flaes are base on) */
  gdouble   scalex;
  gdouble   scaley;
  gint32    detail;

  /* params for creating the fireShapeLayer (trapezoid) */

  gboolean  useTrapezoidShape;
  gdouble   flameHeight;
  gdouble   flameWidthBase;
  gdouble   flameWidthTop;
  gdouble   flameBorder;
  gdouble   flameOffestX;
  
  /* params to control rendering of the layer(s) */
  gint32    blendNum;
  gdouble   stretchHeight;
  gdouble   shiftPhaseY;

  gboolean  createImage;
  gint32    nframes;
  gboolean  createFireLayer;     /* TRUE: keep the newly created rendered fire layer(s) FALSE: merge fire layer with processed layer(s) */
  gboolean  useTransparentBg;    /* TRUE derive alpha from luminosity, FALSE derive alpha from gradient */
  gdouble   fireOpacity;         /* opacity of the fire layer 0.0 to 100.0 */

  gint32    seed1;
  gint32    cloudLayer1;
  gint32    fireShapeLayer;
  gboolean  forceShapeLayer;
  gboolean  reverseGradient;
  gchar     gradientName[MAX_GRADIENT_NAME];
  
  
} firepattern_val_t;




typedef struct _FirePatternDialog FirePatternDialog;

struct _FirePatternDialog
{
  gint            run;
  gint            show_progress;
  gint32          drawable_id;
  
  gboolean        createNewPattern;         /* FALSE: use cloud layer  entered via dialog */
  gboolean        createNewPatternDefault;  /* FALSE: use cloud layer entered via dialog */
  gboolean        createNewShapeDefault;  /* FALSE: use cloud layer entered via dialog */
  gint32          existingCloud1Id;
  gint32          existingFireShapeLayerId;
  gint32          countPotentialCloudLayers;
  
  GtkWidget       *shell;
  GtkWidget       *radio_blend_burn;
  GtkWidget       *radio_blend_subtract;
  GtkWidget       *radio_blend_multiply;
  GtkObject       *nframes_spinbutton_adj;
  GtkWidget       *nframes_spinbutton;
  GtkObject       *fireOpacity_spinbutton_adj;
  GtkWidget       *fireOpacity_spinbutton;
  GtkObject       *stretchHeight_spinbutton_adj;
  GtkWidget       *stretchHeight_spinbutton;

  GtkObject       *flameHeight_spinbutton_adj;
  GtkWidget       *flameHeight_spinbutton;
  GtkObject       *flameWidthBase_spinbutton_adj;
  GtkWidget       *flameWidthBase_spinbutton;
  GtkObject       *flameWidthTop_spinbutton_adj;
  GtkWidget       *flameWidthTop_spinbutton;
  GtkObject       *flameBorder_spinbutton_adj;
  GtkWidget       *flameBorder_spinbutton;
  GtkObject       *flameOffestX_spinbutton_adj;
  GtkWidget       *flameOffestX_spinbutton;
  
  GtkObject       *shiftPhaseY_spinbutton_adj;
  GtkWidget       *shiftPhaseY_spinbutton;
  GtkObject       *scaleX_spinbutton_adj;
  GtkWidget       *scaleX_spinbutton;
  GtkObject       *scaleY_spinbutton_adj;
  GtkWidget       *scaleY_spinbutton;
  GtkObject       *detail_spinbutton_adj;
  GtkWidget       *detail_spinbutton;
  GtkObject       *seed1_spinbutton_adj;
  GtkWidget       *seed1_spinbutton;

  GtkWidget       *cloud1Combo;
  GtkWidget       *fireShapeCombo;
  GtkWidget       *cloud1Label;
  GtkWidget       *fireShapeLabel;
  GtkWidget       *patternCheckbutton;
  GtkWidget       *patternLabel;
  GtkWidget       *shapeCheckbutton;
  GtkWidget       *shapeLabel;
  GtkWidget       *createImageCheckbutton;
  GtkWidget       *createFireLayerCheckbutton;
  GtkWidget       *useTransparentBgCheckbutton;
  GtkWidget       *reverseGradientCheckbutton;
  GtkWidget       *useTrapezoidShapeCheckbutton;

  GtkWidget       *gradient_select;

  firepattern_val_t *vals;
};


typedef struct
{
  GimpRunMode           run_mode;
  gint32                image_id;
  gint32                ref_image_id;
  
  gint32                width;
  gint32                height;
  gint32                strechedHeight;
  GimpLayerModeEffects  blend_mode;
  gboolean              add_display_to_ref_image;
  
  guchar               *byte_samples;   /* holds 256 colorsamples (bpp=4) from the selected gradient */

  gdouble               flameOffestXInPixels;
  gdouble               flameBorderInPixels;
  gdouble               flameHeightInPixels;
  gdouble               flameWidthTopInPixels;
  gdouble               flameWidthBaseInPixels;
  
  gboolean              forceFireShapeLayerCreationPerFrame;
} firepattern_context_t;


typedef struct
{
  guchar   *samples;
  gboolean  is_rgb;
  gboolean  has_alpha;
  gboolean  useTransparentBg;
} MapParam;




static void  p_init_default_cuvals(firepattern_val_t *cuvals);
static void  p_check_for_valid_cloud_layers(firepattern_val_t *cuvals);
static void  p_init_cuvals(firepattern_val_t *cuvals);

static FirePatternDialog *do_dialog (FirePatternDialog *wcd, firepattern_val_t *);
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
 * p_init_default_cuvals
 * p_init_default_cuvals_without_cloud_layers
 * -----------------------------------------
 * 
 */
static void
p_init_default_cuvals_without_cloud_layers(firepattern_val_t *cuvals)
{
  cuvals->scalex = 16.0;
  cuvals->scaley = 7.0;
  cuvals->detail = 2;
  cuvals->useTrapezoidShape = 1;
  cuvals->flameHeight = 1.0;
  cuvals->flameWidthBase = 0.7;
  cuvals->flameWidthTop = 0.5;
  cuvals->flameBorder = 0.2;
  cuvals->flameOffestX = 0.0;
  cuvals->blendNum = 0;

  cuvals->stretchHeight = 2.0;
  cuvals->shiftPhaseY = 0.0;

  cuvals->createImage = FALSE;
  cuvals->nframes = 50;
  cuvals->createFireLayer = FALSE;
  cuvals->useTransparentBg = TRUE;
  cuvals->fireOpacity = 90.0;
  cuvals->seed1 = 0;
  cuvals->reverseGradient = FALSE;
  g_snprintf (cuvals->gradientName, MAX_GRADIENT_NAME -1, "%s", DEFAULT_GRADIENT_NAME);
  
}  /* end p_init_default_cuvals_without_cloud_layers */

static void
p_init_default_cuvals(firepattern_val_t *cuvals)
{
  p_init_default_cuvals_without_cloud_layers(cuvals);
  cuvals->cloudLayer1 = -1;
  cuvals->fireShapeLayer = -1;
  
}  /* end p_init_default_cuvals */

/* --------------------------------------
 * p_check_for_valid_cloud_layers
 * --------------------------------------
 * 
 */
static void
p_check_for_valid_cloud_layers(firepattern_val_t *cuvals)
{
  if(gimp_drawable_is_valid(cuvals->cloudLayer1) != TRUE)
  {
    cuvals->cloudLayer1 = -1;
  }
  if(gimp_drawable_is_valid(cuvals->fireShapeLayer) != TRUE)
  {
    cuvals->fireShapeLayer = -1;
  }
}  /* end p_check_for_valid_cloud_layers */


/* --------------------------------------
 * p_init_cuvals
 * --------------------------------------
 * 
 */
static void
p_init_cuvals(firepattern_val_t *cuvals)
{
  p_init_default_cuvals(cuvals);
  
  /* Possibly retrieve data from a previous run */
  gimp_get_data (PLUG_IN_NAME, cuvals);
  
  p_check_for_valid_cloud_layers(cuvals);
  
}  /* end p_init_cuvals */

/* --------------------------------------
 * p_caclulate_trapezoid_blend
 * --------------------------------------
 * calculate opacity blend factor (in the range 0.0 for fully black
 * upto 1.0 for keep full original colorchannel e.g. white) for the specified coordinate px/py
 * Coordinates within the trapezoid shape return value 1.0
 * Coorinates left or right outside the shape will result 1.0 > value > 0 (e.g. a shade of gray value)
 * depending on their distance to the tapezoid core shape.
 * Coordinates where distance is greater than the flameBorder will return 0.0.
 * Note that Coordinates above flameHeight immediate switch to full black (e.g. 0.0)
 * because the processed fire shape layer is already initally filled with a soft vertical blend
 * from white at the base line to black at flame height.
 */                          
static gdouble
p_caclulate_trapezoid_blend(gint32 px, gint32 py, firepattern_val_t *cuvals, firepattern_context_t  *ctxt)
{
  gdouble widthAtCurrentHeight;
  gdouble mixFactor;
  gdouble borderWidth;
  gdouble halfImageWidth;
  gdouble borderDistance;
  gdouble blendFactor;
  gint32  ppy;             /* flipped y coord where 0 is on bottom of the image */
  gint32  ppx;             /* x coord shifted by flameOffestX */
  
  ppy = ctxt->height - py;  /* flip y coord */
  ppx = px - ctxt->flameOffestXInPixels;
  
  if((ppy > ctxt->flameHeightInPixels)
  || (ctxt->flameHeightInPixels == 0))
  {
    /* fully black for all pixels above flame height */
    return (0.0);
  }
  
  mixFactor = (gdouble)ppy / ctxt->flameHeightInPixels;
  widthAtCurrentHeight = GAP_BASE_MIX_VALUE(mixFactor, ctxt->flameWidthBaseInPixels, ctxt->flameWidthTopInPixels);
  
  borderWidth = (ctxt->width - widthAtCurrentHeight) / 2.0;
  
  
  halfImageWidth = (gdouble)ctxt->width / 2.0;
  if(ppx > halfImageWidth)
  {
    borderDistance =  ctxt->width - ppx;
  }
  else
  {
    borderDistance =  ppx;
  }
  
  if(borderDistance > borderWidth)
  {
    /* keep full original colorchannel e.g. white for pixel within the trapezoid core area */
    return (1.0);
  }
  
  borderDistance = borderWidth - borderDistance;
  if(borderDistance >= ctxt->flameBorderInPixels)
  {
    /* fully transparent for pixels outside trapezoid shape + flameBorder */
    return (0.0);
  }
  
  blendFactor = (gdouble)(ctxt->flameBorderInPixels - borderDistance) / (gdouble)ctxt->flameBorderInPixels;
  
  return (blendFactor);
  
}  /* p_caclulate_trapezoid_blend */


/* ---------------------------------
 * p_rgn_render_fireshape_trapezoid
 * ---------------------------------
 * render trapezoid shape on the fireshape layer.
 */
static void
p_rgn_render_fireshape_trapezoid (const GimpPixelRgn *destPR
                   , firepattern_val_t *cuvals, firepattern_context_t  *ctxt)
{
  guint    row;
  guint    ii;
  guint    colorChannels;
  gint32   px;
  gint32   py;
  gdouble  blendFactor;  /* 0 ..  black, 1.. keep original color channel value */
  guchar   *dest;        /* the destination drawable */

  dest = destPR->data;   /* the destination drawable */
  
  if(destPR->bpp > 2)
  {
    colorChannels = 3;
  }
  else
  {
    colorChannels = 1;
  }
  
  for (row = 0; row < destPR->h; row++)
  {
    guint  col;
    guint  idxDest;

    idxDest = 0;
    for(col = 0; col < destPR->w; col++)
    {
      px = destPR->x + col;
      py = destPR->y + row;


      blendFactor = p_caclulate_trapezoid_blend(px, py, cuvals, ctxt);
      for(ii=0; ii < colorChannels; ii++)
      {
        gdouble  value;

        value = (gdouble)dest[idxDest + ii];
        value = (value * blendFactor);
        dest[idxDest + ii] = value;
      }

      idxDest += destPR->bpp;
    }

    dest += destPR->rowstride;
  }

}  /* end p_rgn_render_fireshape_trapezoid */



/* --------------------------------------
 * p_render_fireshape_layer
 * --------------------------------------
 * render fire shape layer either as rectangle
 * as blend from white at the base line to black on flameHeight
 * or as trapezoid shape like this:
 *
 *           flameWidthTop
 *         +-------------------+
 *         |                   |
 *         .....oo black oo.....      -------------+
 *        .....ooooooooooooo.....                  | flameHeight
 *       .....ooooooooooooooo.....                 |
 *     .....oooo white ooooooo.....   -------------+
 *     |                     |    |
 *     |                     +----+
 *     |                     flameBorder
 *     |                          |
 *     +--------------------------+
 *          flameWidthBase
 *  
 */
static gboolean
p_render_fireshape_layer(firepattern_context_t *ctxt, firepattern_val_t *cuvals
                        ,gint32 drawableId
                        )
{
  gdouble         x1;
  gdouble         y1;
  gdouble         x2;
  gdouble         y2;
  gboolean        success;
  
  ctxt->flameOffestXInPixels = (gdouble)ctxt->width * cuvals->flameOffestX;
  ctxt->flameBorderInPixels = (gdouble)ctxt->width * cuvals->flameBorder;
  ctxt->flameHeightInPixels = (gdouble)ctxt->height * cuvals->flameHeight;
  ctxt->flameWidthTopInPixels = (gdouble)ctxt->width * cuvals->flameWidthTop;
  ctxt->flameWidthBaseInPixels = (gdouble)ctxt->width * cuvals->flameWidthBase;

  x1 = 0.0;
  x2 = 0.0;
  y1 = ctxt->height - ctxt->flameHeightInPixels;
  y2 = ctxt->height;
  
  success = gimp_edit_blend( drawableId
                           , GIMP_FG_BG_RGB_MODE    /*  GimpBlendMode */
                           , GIMP_NORMAL_MODE       /*  GimpLayerModeEffects  paint_mode */
                           , GIMP_GRADIENT_LINEAR   /*  GimpGradientType      gradient_type */
                           , 100.0                  /*  opacity */
                           , 0.0                    /*  gdouble               offset */
                           , GIMP_REPEAT_NONE       /*  GimpRepeatMode        repeat */
                           , FALSE                  /*  gboolean              reverse */
                           , FALSE                  /*  gboolean              supersample */
                           , 1                      /*  gint                  max_depth */
                           , 0.0                    /*  gdouble               threshold */
                           , FALSE                  /*  gboolean              dither */
                           , x1
                           , y1
                           , x2
                           , y2
                           );

  if ((success) && (cuvals->useTrapezoidShape))
  {
    GimpPixelRgn dstPR;
    GimpDrawable *fireShapeDrawable;
    gpointer  pr;

    fireShapeDrawable = gimp_drawable_get(drawableId);
    gimp_pixel_rgn_init (&dstPR, fireShapeDrawable, 0, 0
                      , fireShapeDrawable->width, fireShapeDrawable->height
                      , TRUE     /* dirty */
                      , FALSE     /* shadow */
                       );

    for (pr = gimp_pixel_rgns_register (1, &dstPR);
         pr != NULL;
         pr = gimp_pixel_rgns_process (pr))
    {
      p_rgn_render_fireshape_trapezoid (&dstPR, cuvals, ctxt);
    }

    gimp_drawable_detach(fireShapeDrawable);
  
  }

  return (success);
  
}  /* end p_render_fireshape_layer */


/* --------------------------------------
 * p_convertBlendNum_to_BlendMode
 * --------------------------------------
 */
static GimpLayerModeEffects 
p_convertBlendNum_to_BlendMode(gint32 blendNum)
{
  GimpLayerModeEffects blendMode = GIMP_BURN_MODE;
  switch(blendNum)
  {
      case BLEND_NUM_BURN: 
        blendMode = GIMP_BURN_MODE;
        break;
      case BLEND_NUM_SUBTRACT: 
        blendMode = GIMP_SUBTRACT_MODE;
        break;
      case BLEND_NUM_MULTIPLY: 
        blendMode = GIMP_MULTIPLY_MODE;
        break;
      default:
        printf("unsupported blend mode, using default value Burn\n");
        break;
  }

  return (blendMode);
}

/* --------------------------------------
 * p_get_samples_gradient
 * --------------------------------------
 *
 * Allocate 256 color samples
 * according to the selected gradient. (or the active gradient)
 * Each sample has 4 bytes (RGBA).
 * returns the allocated byte_samples
 */
static guchar  *
p_get_samples_gradient (const gchar *selectedGradient, gboolean reverseGradient)
{
  const gchar   *gradient_name;
  gint     n_f_samples;
  gdouble *f_samples, *f_samp;  /* float samples */
  guchar  *byte_samples;
  guchar  *b_samp;  /* byte samples */
  gint     bpp;
  gint     i, j;

  gradient_name = gimp_context_get_gradient ();
  if (selectedGradient[0] != '\0')
  {
    if (strcmp(selectedGradient, ACTIVE_GRADIENT_NAME) != 0)
    {
      gradient_name = selectedGradient;  /* use the selected gradient name */
    }
  }


  gimp_gradient_get_uniform_samples (gradient_name, N_GRADIENT_COLORSAMPLES, reverseGradient,
                                     &n_f_samples, &f_samples);

  bpp = BPP_SAMPLE_COLOR;
  byte_samples = g_new (guchar, N_GRADIENT_COLORSAMPLES * bpp);

  for (i = 0; i < N_GRADIENT_COLORSAMPLES; i++)
  {
      b_samp = &byte_samples[i * bpp];
      f_samp = &f_samples[i * 4];
      for (j = 0; j < bpp; j++)
      {
        b_samp[j] = f_samp[j] * 255;
      }
  }

  g_free (f_samples);
  return (byte_samples);
  
}  /* end p_get_samples_gradient */



/* --------------------------------------
 * p_delta_guchar_color
 * --------------------------------------
 */
static void 
p_delta_guchar_color(guchar *val, guchar *val_from, guchar *val_to, gint32 total_steps, gdouble current_step)
{
    double     delta;
    guint l_idx;

    if(total_steps < 1) return;

    for(l_idx = 0; l_idx < BPP_SAMPLE_COLOR; l_idx++)
    {
       delta = ((double)(val_to[l_idx] - val_from[l_idx]) / (double)total_steps) * ((double)total_steps - current_step);
       val[l_idx]  = val_from[l_idx] + delta;

    }
}  /* end p_delta_guchar_color */



/* --------------------------------------
 * p_get_samples_from_gradients
 * --------------------------------------
 * Allocate 256 color samples
 * according to the selected gradient. (or the active gradient)
 * Each sample has 4 bytes (RGBA).
 * returns the allocated byte_samples
 *
 * in case when the plug-in is called in animated call style (multiple calls to render
 * one frame) it checks if from and to values with different gradients were used
 * and returns a mix of both gradients when 2 gradients are available.
 */
static guchar *
p_get_samples_from_gradients(firepattern_val_t *cuvals, firepattern_context_t  *ctxt)
{
  GapLastvalAnimatedCallInfo  animCallInfo;
  firepattern_val_t cval_from;
  firepattern_val_t cval_to;
  gint32 dataSize;
  gboolean haveTwoGradients;
  guchar *byte_samples;

  haveTwoGradients = FALSE;      

  dataSize = gimp_get_data_size(PLUG_IN_DATA_ITER_FROM);

  
  if(dataSize == sizeof(cval_from))
  {
    dataSize = gimp_get_data_size(PLUG_IN_DATA_ITER_TO);
    if(dataSize == sizeof(cval_to))
    {
      dataSize = gimp_get_data_size(GAP_LASTVAL_KEY_ANIMATED_CALL_INFO);
      gimp_get_data(GAP_LASTVAL_KEY_ANIMATED_CALL_INFO, &animCallInfo);

      if(gap_debug)
      {
        printf("p_get_samples_from_gradients: dataSize:%d animatedCallInProgress:%d\n"
           , (int)dataSize
           , (int)animCallInfo.animatedCallInProgress
           );
      }


      if((dataSize == sizeof(GapLastvalAnimatedCallInfo))
      && (animCallInfo.animatedCallInProgress))
      {
        gimp_get_data(PLUG_IN_DATA_ITER_FROM, &cval_from);
        gimp_get_data(PLUG_IN_DATA_ITER_TO, &cval_to);

        if(gap_debug)
        {
          printf("p_get_samples_from_gradients: gradient:%s FROM:%s  TO:%s\n"
             , cuvals->gradientName
             , cval_from.gradientName
             , cval_to.gradientName
             );
        }
        
        if ((strcmp(cval_from.gradientName, cuvals->gradientName) == 0)
        ||  (strcmp(cval_to.gradientName, cuvals->gradientName) == 0))
        {
          if (strcmp(cval_from.gradientName, cval_to.gradientName) != 0)
          {
             haveTwoGradients = TRUE;      
          }
        }
      }
    }
  }

  byte_samples = p_get_samples_gradient (cuvals->gradientName, cuvals->reverseGradient);

  if (haveTwoGradients)
  {

    /* 2 gradients are available, therefore get samples from both and mix
     * all color samples according to current frame phase
     */
    guchar *byte_samples_FROM;
    guchar *byte_samples_TO;
    gint ii;
    
    byte_samples_FROM = p_get_samples_gradient (cval_from.gradientName, cval_from.reverseGradient);
    byte_samples_TO = p_get_samples_gradient (cval_to.gradientName, cval_to.reverseGradient);

    
    for(ii=0; ii < 256; ii++)
    {
      gint idx;
      idx = ii * BPP_SAMPLE_COLOR;
      p_delta_guchar_color(&byte_samples[idx]
                          ,&byte_samples_FROM[idx]
                          ,&byte_samples_TO[idx]
                          ,animCallInfo.total_steps
                          ,animCallInfo.current_step
                          );

    }
    
    g_free(byte_samples_FROM);
    g_free(byte_samples_TO);
     
  }
  
  return (byte_samples);

}  /* end p_get_samples_from_gradients */


/* -----------------------------------------
 * p_init_context_and_cloud_and_shape_layers
 * -----------------------------------------
 * 
 */
static gboolean
p_init_context_and_cloud_and_shape_layers(gint32 drawable_id, firepattern_val_t *cuvals, firepattern_context_t  *ctxt)
{
  gboolean success;
  gdouble  strechedHeight;
  
  ctxt->image_id = gimp_drawable_get_image(drawable_id);
  ctxt->blend_mode = p_convertBlendNum_to_BlendMode(cuvals->blendNum);
  ctxt->width = gimp_image_width(ctxt->image_id);
  ctxt->height = gimp_image_height(ctxt->image_id);
  strechedHeight = (gdouble)gimp_image_height(ctxt->image_id) * cuvals->stretchHeight;
  ctxt->strechedHeight = strechedHeight;
  ctxt->add_display_to_ref_image = FALSE;
  ctxt->forceFireShapeLayerCreationPerFrame = cuvals->forceShapeLayer;
  
  success = TRUE;
  ctxt->ref_image_id = -1;
 
  /* check if cloud layer (the pattern) is already available */
  if(!gimp_drawable_is_valid(cuvals->cloudLayer1))
  {
    /* create both cloud layers */
    GRand  *gr;
    gint32  seed;
    gr = g_rand_new ();
    
    ctxt->ref_image_id = gimp_image_new(ctxt->width, ctxt->height, GIMP_RGB);
    gimp_image_set_filename(ctxt->ref_image_id, "FirePattern");
 
    cuvals->cloudLayer1 = gimp_layer_new(ctxt->ref_image_id
                                        , "Frame"
                                        , ctxt->width
                                        , ctxt->strechedHeight
                                        , GIMP_RGBA_IMAGE
                                        , 100.0               /* full opaque */
                                        , GIMP_NORMAL_MODE    /* 0 */
                                        );
    cuvals->fireShapeLayer = gimp_layer_new(ctxt->ref_image_id
                                        , "FireShape"
                                        , ctxt->width
                                        , ctxt->height
                                        , GIMP_RGBA_IMAGE
                                        , 100.0                 /* full opaque */
                                        , GIMP_NORMAL_MODE      /* 0 */
                                        );
    gimp_image_insert_layer(ctxt->ref_image_id, cuvals->fireShapeLayer, 0, -1);
    gimp_image_insert_layer(ctxt->ref_image_id, cuvals->cloudLayer1, 0, -1);


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
                          ,cuvals->detail          /* detail level */
                          ,cuvals->scalex          /* horizontal texture size */
                          ,cuvals->scaley          /* vertical texture size */
                          );
    if(success) 
    {
      /* render an inital fireshape layer 
       * (intended to be used when shape shall shall not change) 
       */
      success = p_render_fireshape_layer(ctxt, cuvals
                          ,cuvals->fireShapeLayer
                          );
    }

    g_rand_free (gr);
    ctxt->add_display_to_ref_image = TRUE;

  }

  if(!gimp_drawable_is_valid(cuvals->fireShapeLayer))
  {
    if((cuvals->createImage != TRUE) || (ctxt->ref_image_id < 0))
    {
      /* set flag that triggers fire shape rendering for each processed frame */
      ctxt->forceFireShapeLayerCreationPerFrame = TRUE;
    }
    else
    {
      /* render an inital fireshape layer 
       * (intended to be used when shape shall shall not change) 
       */
      success = p_render_fireshape_layer(ctxt, cuvals
                          ,cuvals->fireShapeLayer
                          );
    }
  }

  if(success) 
  {
    ctxt->byte_samples = p_get_samples_from_gradients (cuvals, ctxt);
  }

  return (success);
  
}  /* end p_init_context_and_cloud_and_shape_layers */


/* --------------------------------------
 * p_cloud_size_check
 * --------------------------------------
 * check if layer is same size as processed image size (as specified in the context)
 */
static void
p_cloud_size_check(gint32 layer_id, firepattern_context_t  *ctxt)
{
  if( (gimp_drawable_width(layer_id) != ctxt->width)
  ||  (gimp_drawable_height(layer_id) != ctxt->strechedHeight) )
  {
    gimp_layer_scale(layer_id, ctxt->width, ctxt->strechedHeight, TRUE);
    gimp_layer_set_offsets(layer_id, 0, 0);
  }
}  /* end p_cloud_size_check */


/* --------------------------------------
 * p_shape_size_check
 * --------------------------------------
 * check if layer is same size as processed image size (as specified in the context)
 */
static void
p_shape_size_check(gint32 layer_id, firepattern_context_t  *ctxt)
{
  if( (gimp_drawable_width(layer_id) != ctxt->width)
  ||  (gimp_drawable_height(layer_id) != ctxt->height) )
  {
    gimp_layer_scale(layer_id, ctxt->width, ctxt->height, TRUE);
    gimp_layer_set_offsets(layer_id, 0, 0);
  }
}  /* end p_shape_size_check */



/* --------------------------------------
 * p_map_func
 * --------------------------------------
 * map color for one pixel by its luminosity that
 * is used as index (range 0 to 255) to the table of
 * given sample colors provided in MapParam data.
 * in case the processed drawable has an alpha channel
 * calculate the opacity according to useTransparentBg
 * as specified in MapParam.
 */
static void
p_map_func (const guchar *src,
          guchar       *dest,
          gint          bpp,
          gpointer      data)
{
  MapParam *param = data;
  guint     lum;
  guint     lumSample;
  gint      b;
  guchar    samp[BPP_SAMPLE_COLOR];

  lum = (param->is_rgb) ? LUMINOSITY (src) : src[0];
  samp[0] = param->samples[lum * BPP_SAMPLE_COLOR];
  samp[1] = param->samples[(lum * BPP_SAMPLE_COLOR) +1];
  samp[2] = param->samples[(lum * BPP_SAMPLE_COLOR) +2];
  samp[3] = param->samples[(lum * BPP_SAMPLE_COLOR) +3];
  lumSample = LUMINOSITY (samp);
  if(bpp < 3)
  {
    samp[0] = lumSample;
  }

  if (param->has_alpha)
  {
      for (b = 0; b < bpp - 1; b++)
      {
        dest[b] = samp[b];
      }
      if (param->useTransparentBg)
      {
        guint     lumSample2;

        lumSample2 = MIN((lumSample * 64), 255);

        /* render ALPHA_FROM_LUMINOSITY */
        dest[b] = (lumSample2 * lum) / 255;
      }
      else
      {
        /* render ALPHA_FROM_GRADIENT_SAMPLE */
        dest[b] = ((guint)samp[BPP_SAMPLE_COLOR -1] * (guint)src[b]) / 255;
      }
  }
  else
  {
      for (b = 0; b < bpp; b++)
      {
        dest[b] = samp[b];
      }
  }
}  /* end p_map_func */



/* --------------------------------------
 * p_remap_drawable_with_gradient_colors
 * --------------------------------------
 * colorize the specified drawable with gradient colors
 * where the luminosity is used to both fetch the relevant gradient color sample
 * and to define the alpha channel.
 */
static void
p_remap_drawable_with_gradient_colors (gint32 drawable_id, firepattern_val_t *cuvals, firepattern_context_t  *ctxt)
{
  MapParam param;
  
  GimpDrawable *drawable;
  
  drawable = gimp_drawable_get (drawable_id);

  param.useTransparentBg = cuvals->useTransparentBg;
  param.samples = ctxt->byte_samples;
  param.is_rgb = gimp_drawable_is_rgb (drawable->drawable_id);
  param.has_alpha = gimp_drawable_has_alpha (drawable->drawable_id);

  gimp_rgn_iterate2 (drawable, 0 /* unused */, p_map_func, &param);
  
  gimp_drawable_detach(drawable);

}  /* end p_remap_drawable_with_gradient_colors */



/* --------------------------------------
 * p_drawable_get_name
 * --------------------------------------
 */
static const char*
p_drawable_get_name(gint32 drawable_id)
{
  const char *invalidName = "(invalid)";
  
  if(gimp_drawable_is_valid(drawable_id))
  {
    return(gimp_drawable_get_name(drawable_id));
  }
  
  return (invalidName);
}

/* --------------------------------------
 * p_run_renderFirePattern
 * --------------------------------------
 * renders the fire pattern effect onto the specified drawable.
 * returns TRUE on success
 */
static gboolean
p_run_renderFirePattern(gint32 drawable_id, firepattern_val_t *cuvals, firepattern_context_t  *ctxt)
{
  gint32 templayer_id;
  gint32 newlayer1_id = -1;
  gint32 newlayer2_id = -1;
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
  /* .. and force visibility (required for merge down fire pattern to the active layer) */
  gimp_drawable_set_visible(drawable_id, TRUE);

  templayer_id = drawable_id;
  nframesToProcess = 1;

  gimp_layer_resize_to_image_size(drawable_id);
  success = p_init_context_and_cloud_and_shape_layers(drawable_id, cuvals, ctxt);

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
     printf("p_run_renderFirePattern: drawable_id :%d (%s)\n", (int)drawable_id, p_drawable_get_name(drawable_id));
     printf("p_run_renderFirePattern:  scalex:%f\n", (float)cuvals->scalex);
     printf("p_run_renderFirePattern:  scaley:%f\n", (float)cuvals->scaley);
     printf("p_run_renderFirePattern:  detail:%d\n", (int)cuvals->detail);
     printf("p_run_renderFirePattern:  useTrapezoidShape:%d\n", (int)cuvals->useTrapezoidShape);
     printf("p_run_renderFirePattern:  flameHeight:%f\n", (float)cuvals->flameHeight);
     printf("p_run_renderFirePattern:  flameWidthBase:%f\n", (float)cuvals->flameWidthBase);
     printf("p_run_renderFirePattern:  flameWidthTop:%f\n", (float)cuvals->flameWidthTop);
     printf("p_run_renderFirePattern:  flameBorder:%f\n", (float)cuvals->flameBorder);
     printf("p_run_renderFirePattern:  flameOffestX:%f\n", (float)cuvals->flameOffestX);
     printf("p_run_renderFirePattern:  blendNum:%d\n", (int)cuvals->blendNum);
     printf("p_run_renderFirePattern:  stretchHeight:%f\n", (float)cuvals->stretchHeight);
     printf("p_run_renderFirePattern:  shiftPhaseY:%f\n", (float)cuvals->shiftPhaseY);
     printf("p_run_renderFirePattern:  createImage:%d\n", (int)cuvals->createImage);
     printf("p_run_renderFirePattern:  nframes:%d\n", (int)cuvals->nframes);
     printf("p_run_renderFirePattern:  createFireLayer:%d\n", (int)cuvals->createFireLayer);
     printf("p_run_renderFirePattern:  useTransparentBg:%d\n", (int)cuvals->useTransparentBg);
     printf("p_run_renderFirePattern:  fireOpacity:%f\n", (float)cuvals->fireOpacity);

     printf("p_run_renderFirePattern:  seed1:%d\n", (int)cuvals->seed1);
     printf("p_run_renderFirePattern:  cloudLayer1:%d (%s)\n", (int)cuvals->cloudLayer1, p_drawable_get_name(cuvals->cloudLayer1));
     printf("p_run_renderFirePattern:  fireShapeLayer:%d (%s)\n", (int)cuvals->fireShapeLayer, p_drawable_get_name(cuvals->fireShapeLayer));
     printf("p_run_renderFirePattern:  forceShapeLayer:%d\n", (int)cuvals->forceShapeLayer);
     printf("p_run_renderFirePattern:  reverseGradient:%d\n", (int)cuvals->reverseGradient);
     printf("p_run_renderFirePattern:  gradientName:%s\n", cuvals->gradientName);

     printf("p_run_renderFirePattern:  ref_image_id:%d\n", (int)ctxt->ref_image_id);
     printf("p_run_renderFirePattern:  image_id:%d\n", (int)ctxt->image_id);
     printf("p_run_renderFirePattern:  width:%d\n", (int)ctxt->width);
     printf("p_run_renderFirePattern:  height:%d\n", (int)ctxt->height);
     printf("p_run_renderFirePattern:  strechedHeight:%d\n", (int)ctxt->strechedHeight);
     printf("p_run_renderFirePattern:  blend_mode:%d\n", (int)ctxt->blend_mode);
     printf("p_run_renderFirePattern:  forceFireShapeLayerCreationPerFrame:%d\n", (int)ctxt->forceFireShapeLayerCreationPerFrame);
     printf("p_run_renderFirePattern:  success:%d\n", (int)success);

  }

  if(!success)
  {
    return (FALSE);
  }

  /* save gimp context (before changeing FG/BG color) */
  gimp_context_push();
  gimp_context_set_default_colors(); /* set fg and bg to black and white */

  /* end while nframes loop */
  for(count=0; count < nframesToProcess; count++)
  {
    if (cuvals->createImage)
    {
      /* in case we are creating a multilayer anim in a new image
       * copy the drawable to a new layer in this new image
       */
      templayer_id = gimp_layer_new_from_drawable(drawable_id, ctxt->image_id);
      gimp_image_insert_layer(ctxt->image_id, templayer_id, 0, -1 /* -1 place above active layer */);
    }
  
    /* copy cloud layers from ref image to current processed image_id
     * at stackposition above the current processed layer (templayer_id)
     */
    newlayer1_id = gimp_layer_new_from_drawable(cuvals->cloudLayer1, ctxt->image_id);
    gimp_image_set_active_layer(ctxt->image_id, templayer_id);
    
    if(ctxt->forceFireShapeLayerCreationPerFrame)
    {
      newlayer2_id = gimp_layer_new(ctxt->image_id
                                        , "FireShape"
                                        , ctxt->width
                                        , ctxt->height
                                        , GIMP_RGBA_IMAGE
                                        , 100.0                 /* full opaque */
                                        , GIMP_NORMAL_MODE      /* 0 */
                                        );
      gimp_image_insert_layer(ctxt->image_id, newlayer2_id, 0, -1 /* -1 place above active layer */);
      p_render_fireshape_layer(ctxt, cuvals, newlayer2_id);
    }
    else
    {
      newlayer2_id = gimp_layer_new_from_drawable(cuvals->fireShapeLayer, ctxt->image_id);
      gimp_image_insert_layer(ctxt->image_id, newlayer2_id, 0, -1 /* -1 place above active layer */);
    }

    gimp_image_insert_layer(ctxt->image_id, newlayer1_id, 0, -1 /* -1 place above active layer */);

    p_cloud_size_check(newlayer1_id, ctxt);
    p_shape_size_check(newlayer2_id, ctxt);

    gimp_drawable_set_visible(newlayer1_id, TRUE);
    gimp_drawable_set_visible(newlayer2_id, TRUE);
    gimp_drawable_set_visible(templayer_id, TRUE);
    
    /* calculate offsets to shift the cloud layer according to pahse of the currently processed frame */
    {
      gdouble shifty;
      gint32  intShifty;
      gint32  offsetx;
      gint32  offsety;
    
      if(nframesToProcess > 1)
      {
        gdouble phY;
        phY = cuvals->shiftPhaseY;
        
        if (phY == 0.0)
        {
          phY = 1.0;
        }
        shifty = (gdouble)count * (((gdouble)ctxt->strechedHeight * phY) / (gdouble)nframesToProcess);
      }
      else
      {
        shifty = (gdouble)ctxt->strechedHeight * cuvals->shiftPhaseY;
      }
      intShifty = rint(shifty);
    
      offsetx = 0;
      offsety = 0 - (intShifty % ctxt->strechedHeight);
      gimp_drawable_offset(newlayer1_id, 1 /* 1:Wrap around */, 0 /* fill type */, offsetx, offsety);
    
      gimp_layer_set_mode(newlayer1_id, ctxt->blend_mode);
      gimp_layer_set_mode(newlayer2_id, GIMP_NORMAL_MODE);

      newlayer2_id = gimp_image_merge_down(ctxt->image_id, newlayer1_id, GIMP_EXPAND_AS_NECESSARY);
      
      gimp_layer_resize_to_image_size(newlayer2_id);
    
    }

    /* colorize firepattern layer and calculate its alpha channel from luminosity */
    p_remap_drawable_with_gradient_colors (newlayer2_id, cuvals, ctxt);
     
    gimp_layer_set_opacity(newlayer2_id, cuvals->fireOpacity);
    gimp_layer_set_mode(newlayer2_id, GIMP_NORMAL_MODE);


    /* optional merge the firepattern onto the processed layer */
    if(cuvals->createFireLayer != TRUE)
    {
      templayer_id = gimp_image_merge_down(ctxt->image_id, newlayer2_id, GIMP_EXPAND_AS_NECESSARY);
    }

    if (cuvals->createImage)
    {
      gchar *layerName;
      
      layerName = g_strdup_printf("Frame_%03d", (int)count +1);
      gimp_drawable_set_name(templayer_id, layerName);
      g_free(layerName);
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
  
  /* restore the gimp context */
  gimp_context_pop();
  
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

}  /* end p_run_renderFirePattern */


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
  FirePatternDialog *wcd;

  if(gap_debug)
  {
    printf("on_blend_radio_callback: START\n");
  }
  wcd = (FirePatternDialog*)user_data;
  if(wcd != NULL)
  {
    if(wcd->vals != NULL)
    {
       if(wgt == wcd->radio_blend_burn)        { wcd->vals->blendNum = BLEND_NUM_BURN; }
       if(wgt == wcd->radio_blend_subtract)    { wcd->vals->blendNum = BLEND_NUM_SUBTRACT; }
       if(wgt == wcd->radio_blend_multiply)    { wcd->vals->blendNum = BLEND_NUM_MULTIPLY; }

    }
  }
}

/* --------------------------------------
 * p_update_widget_sensitivity
 * --------------------------------------
 */
static void
p_update_widget_sensitivity (FirePatternDialog *wcd)
{
  gboolean inverseCreateImage;
  gboolean inversecreateNewPattern;
  gboolean inverseForceShape;
  
  
  /* createImage dependent widgets */  
  if(wcd->vals->createImage)
  {
    inverseCreateImage = FALSE;
  }
  else
  {
    inverseCreateImage = TRUE;
  }
  gtk_widget_set_sensitive(wcd->nframes_spinbutton ,          wcd->vals->createImage);
  //gtk_widget_set_sensitive(wcd->shiftPhaseY_spinbutton ,      inverseCreateImage);

  /* createNewPattern dependent widgets */  
  if(wcd->createNewPattern)
  {
    inversecreateNewPattern = FALSE;
  }
  else
  {
    inversecreateNewPattern = TRUE;
  }
  gtk_widget_set_sensitive(wcd->stretchHeight_spinbutton ,     wcd->createNewPattern);
  gtk_widget_set_sensitive(wcd->scaleX_spinbutton ,            wcd->createNewPattern);
  gtk_widget_set_sensitive(wcd->scaleY_spinbutton ,            wcd->createNewPattern);
  gtk_widget_set_sensitive(wcd->seed1_spinbutton ,             wcd->createNewPattern);
  gtk_widget_set_sensitive(wcd->detail_spinbutton ,            wcd->createNewPattern);
  gtk_widget_set_sensitive(wcd->cloud1Combo ,                  inversecreateNewPattern);
  gtk_widget_set_sensitive(wcd->fireShapeCombo ,               inversecreateNewPattern);


  /* createNewShape dependent widgets */  
  if(wcd->vals->forceShapeLayer)
  {
    inverseForceShape = FALSE;
  }
  else
  {
    inverseForceShape = TRUE;
  }
  gtk_widget_set_sensitive(wcd->flameHeight_spinbutton ,       wcd->vals->forceShapeLayer);
  gtk_widget_set_sensitive(wcd->flameWidthBase_spinbutton ,    wcd->vals->forceShapeLayer);
  gtk_widget_set_sensitive(wcd->flameWidthTop_spinbutton ,     wcd->vals->forceShapeLayer);
  gtk_widget_set_sensitive(wcd->flameBorder_spinbutton ,       wcd->vals->forceShapeLayer);
  gtk_widget_set_sensitive(wcd->flameOffestX_spinbutton ,      wcd->vals->forceShapeLayer);
  gtk_widget_set_sensitive(wcd->useTrapezoidShapeCheckbutton , wcd->vals->forceShapeLayer);
  gtk_widget_set_sensitive(wcd->fireShapeCombo ,               inverseForceShape);

}  /* end p_update_widget_sensitivity */


/* --------------------------------------
 * p_init_widget_values
 * --------------------------------------
 * update GUI widgets to reflect the current values.
 */
static void
p_init_widget_values(FirePatternDialog *wcd)
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
  gtk_adjustment_set_value(GTK_ADJUSTMENT(wcd->fireOpacity_spinbutton_adj)
                         , (gfloat)wcd->vals->fireOpacity);
  gtk_adjustment_set_value(GTK_ADJUSTMENT(wcd->stretchHeight_spinbutton_adj)
                         , (gfloat)wcd->vals->stretchHeight);
  gtk_adjustment_set_value(GTK_ADJUSTMENT(wcd->flameHeight_spinbutton_adj)
                         , (gfloat)wcd->vals->flameHeight);
  gtk_adjustment_set_value(GTK_ADJUSTMENT(wcd->flameWidthBase_spinbutton_adj)
                         , (gfloat)wcd->vals->flameWidthBase);
  gtk_adjustment_set_value(GTK_ADJUSTMENT(wcd->flameWidthTop_spinbutton_adj)
                         , (gfloat)wcd->vals->flameWidthTop);
  gtk_adjustment_set_value(GTK_ADJUSTMENT(wcd->flameBorder_spinbutton_adj)
                         , (gfloat)wcd->vals->flameBorder);
  gtk_adjustment_set_value(GTK_ADJUSTMENT(wcd->flameOffestX_spinbutton_adj)
                         , (gfloat)wcd->vals->flameOffestX);
  gtk_adjustment_set_value(GTK_ADJUSTMENT(wcd->shiftPhaseY_spinbutton_adj)
                         , (gfloat)wcd->vals->shiftPhaseY);
  gtk_adjustment_set_value(GTK_ADJUSTMENT(wcd->scaleX_spinbutton_adj)
                         , (gfloat)wcd->vals->scalex);
  gtk_adjustment_set_value(GTK_ADJUSTMENT(wcd->scaleY_spinbutton_adj)
                         , (gfloat)wcd->vals->scaley);
  gtk_adjustment_set_value(GTK_ADJUSTMENT(wcd->detail_spinbutton_adj)
                         , (gfloat)wcd->vals->detail);
  gtk_adjustment_set_value(GTK_ADJUSTMENT(wcd->seed1_spinbutton_adj)
                         , (gfloat)wcd->vals->seed1);



  /* init checkbuttons */
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (wcd->createImageCheckbutton)
                               , wcd->vals->createImage);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (wcd->patternCheckbutton)
                               , wcd->createNewPatternDefault);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (wcd->createFireLayerCheckbutton)
                               , wcd->vals->createFireLayer);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (wcd->useTransparentBgCheckbutton)
                               , wcd->vals->useTransparentBg);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (wcd->reverseGradientCheckbutton)
                               , wcd->vals->reverseGradient);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (wcd->useTrapezoidShapeCheckbutton)
                               , wcd->vals->useTrapezoidShape);


  /* init radiobuttons */
  if(wcd->vals->blendNum == BLEND_NUM_BURN)
  {
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON (wcd->radio_blend_burn), TRUE);
  } 
  else if (wcd->vals->blendNum == BLEND_NUM_SUBTRACT)
  {
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON (wcd->radio_blend_subtract), TRUE);
  }
  else if (wcd->vals->blendNum == BLEND_NUM_MULTIPLY)
  {
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON (wcd->radio_blend_multiply), TRUE);
  }

  countClouds = 0;
  if(gimp_drawable_is_valid(wcd->existingCloud1Id))
  {
    countClouds++;
  }
  if(gimp_drawable_is_valid(wcd->existingFireShapeLayerId))
  {
    countClouds++;
  }

  if(countClouds == 2)
  {
    gimp_int_combo_box_set_active (GIMP_INT_COMBO_BOX (wcd->cloud1Combo), wcd->existingCloud1Id);
    gimp_int_combo_box_set_active (GIMP_INT_COMBO_BOX (wcd->fireShapeCombo), wcd->existingFireShapeLayerId);
  }

  gimp_gradient_select_button_set_gradient (GIMP_GRADIENT_SELECT_BUTTON (wcd->gradient_select),
                                            &wcd->vals->gradientName[0]);

}  /* end p_init_widget_values */


/* --------------------------------------
 * on_gboolean_button_update
 * --------------------------------------
 */
static void
on_gboolean_button_update (GtkWidget *widget,
                           gpointer   data)
{
  FirePatternDialog *wcd;
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
  
  wcd = (FirePatternDialog *) g_object_get_data (G_OBJECT (widget), "wcd");
  if(wcd != NULL)
  {
    p_update_widget_sensitivity (wcd);
  }
}


/* ---------------------------------
 * p_firepattern_response
 * ---------------------------------
 */
static void
p_firepattern_response (GtkWidget *widget,
                 gint       response_id,
                 FirePatternDialog *wcd)
{
  GtkWidget *dialog;

  switch (response_id)
  {
    case GAP_FIREPATTERN_RESPONSE_RESET:
      if(wcd)
      {
        /* rset default values */
        p_init_default_cuvals_without_cloud_layers(wcd->vals);
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
}  /* end p_firepattern_response */


/* --------------------------------------
 * p_gradient_changed_callback
 * --------------------------------------
 *  
 */
static void
p_gradient_changed_callback (GimpGradientSelectButton *button,
                                const gchar              *gradient_name,
                                gint                      width,
                                const gdouble            *grad_data,
                                gboolean                  dialog_closing,
                                gpointer                  user_data)
{
  FirePatternDialog *wcd;
  
  wcd = (FirePatternDialog *) user_data;
  
  if(wcd == NULL)
  {
    return;
  }
  if(wcd->vals == NULL)
  {
    return;
  }
  
  g_snprintf (wcd->vals->gradientName, MAX_GRADIENT_NAME -1, "%s", gradient_name);

}  /* end p_gradient_changed_callback */


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
p_pattern_layer_constrain(gint32 image_id, gint32 drawable_id, FirePatternDialog *wcd)
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
static FirePatternDialog *
do_dialog (FirePatternDialog *wcd, firepattern_val_t *cuvals)
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
  gimp_ui_init ("firepattern", FALSE);


  /*  The dialog1  */
  wcd->run = FALSE;
  wcd->vals = cuvals;
  wcd->createNewPatternDefault = TRUE;
  wcd->createNewShapeDefault = wcd->vals->forceShapeLayer;
  wcd->existingCloud1Id = -1;
  wcd->existingFireShapeLayerId = -1;

  wcd->countPotentialCloudLayers = 0;
  if(gimp_drawable_is_valid(cuvals->cloudLayer1))
  {
    wcd->existingCloud1Id = cuvals->cloudLayer1;
    wcd->createNewPatternDefault = FALSE;
  }
  if(gimp_drawable_is_valid(cuvals->fireShapeLayer))
  {
    wcd->existingFireShapeLayerId = cuvals->fireShapeLayer;
  }
  else
  {
    wcd->createNewShapeDefault = TRUE;
    wcd->vals->forceShapeLayer = TRUE;
  }


  wcd->createNewPattern = wcd->createNewPatternDefault;

  /*  The dialog1 and main vbox  */
  dialog1 = gimp_dialog_new (_("Fire-Pattern"), "fire_pattern",
                               NULL, 0,
                               gimp_standard_help_func, PLUG_IN_HELP_ID,
                               
                               GIMP_STOCK_RESET, GAP_FIREPATTERN_RESPONSE_RESET,
                               GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                               GTK_STOCK_OK,     GTK_RESPONSE_OK,
                               NULL);

  wcd->shell = dialog1;


  /*
   * g_object_set_data (G_OBJECT (dialog1), "dialog1", dialog1);
   * gtk_window_set_title (GTK_WINDOW (dialog1), _("dialog1"));
   */


  g_signal_connect (G_OBJECT (dialog1), "response",
                      G_CALLBACK (p_firepattern_response),
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
  gimp_help_set_help_data (checkbutton, _("ON: create a new image with n copies of the input drawable"
                                          " and render complete animation effect on those copies. "
                                          "OFF: render only one phase of the animation effect on "
                                          "the input drawable"), NULL);
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


 
  /* shiftPhaseY spinbutton  */
  label = gtk_label_new (_("Phase shift"));
  gtk_widget_show (label);
  gtk_table_attach (GTK_TABLE (table1), label, 0, 1, row, row+1,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);
  gtk_misc_set_alignment (GTK_MISC (label), 0, 0.5);
  spinbutton_adj = gtk_adjustment_new (cuvals->shiftPhaseY, 0.0, 100, 0.1, 1, 0);
  spinbutton = gtk_spin_button_new (GTK_ADJUSTMENT (spinbutton_adj), 1, 2);
  gimp_help_set_help_data (spinbutton, _("Vertical shift phase where 1.0 refers to image height"), NULL);
  gtk_widget_show (spinbutton);
  gtk_table_attach (GTK_TABLE (table1), spinbutton, 1, 2, row, row+1,
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
  gimp_help_set_help_data (checkbutton, _("ON: create firepattern cloud layer according to options. OFF: Use external pattern layer. "), NULL);
  gtk_widget_show (checkbutton);
  gtk_table_attach( GTK_TABLE(table1), checkbutton, 1, 2, row, row+1,
                    GTK_FILL, 0, 0, 0 );
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (checkbutton), wcd->createNewPattern);
  g_signal_connect (checkbutton, "toggled",
                    G_CALLBACK (on_gboolean_button_update),
                    &wcd->createNewPattern);
 
  /* stretchHeight spinbutton  */
  spinbutton_adj = gtk_adjustment_new (cuvals->stretchHeight, 0.0, 3.0, 0.1, 0.1, 0);
  spinbutton = gtk_spin_button_new (GTK_ADJUSTMENT (spinbutton_adj), 1, 3);
  gimp_help_set_help_data (spinbutton, _("vertical stretch factor for the fire pattern"), NULL);
  gtk_widget_show (spinbutton);
  gtk_table_attach (GTK_TABLE (table1), spinbutton, 3, 4, row, row+1,
                    (GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);
  g_signal_connect (G_OBJECT (spinbutton_adj), "value_changed", G_CALLBACK (gimp_double_adjustment_update), &cuvals->stretchHeight);
  wcd->stretchHeight_spinbutton_adj = spinbutton_adj;
  wcd->stretchHeight_spinbutton = spinbutton;

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
  gimp_help_set_help_data (spinbutton, _("Horizontal scaling of the random patterns that are created for rendering (cloud layer)"), NULL);
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
  gimp_help_set_help_data (spinbutton, _("Vertical scaling of the random patterns that are created for rendering (cloud layer)"), NULL);
  gtk_widget_show (spinbutton);
  gtk_table_attach (GTK_TABLE (table1), spinbutton, 3, 4, row, row+1,
                    (GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);
  g_signal_connect (G_OBJECT (spinbutton_adj), "value_changed", G_CALLBACK (gimp_double_adjustment_update), &cuvals->scaley);
  wcd->scaleY_spinbutton_adj = spinbutton_adj;
  wcd->scaleY_spinbutton = spinbutton;
  


  row++;

  label = gtk_label_new (_("Seed Pattern:"));
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



  label = gtk_label_new (_("Detail:"));
  gtk_widget_show (label);
  gtk_table_attach (GTK_TABLE (table1), label, 2, 3, row, row+1,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);
  gtk_misc_set_alignment (GTK_MISC (label), 0, 0.5);

  /* detail  spinbutton  */
  spinbutton_adj = gtk_adjustment_new (cuvals->detail, 0.0, 15.0, 1.0, 10, 0);
  spinbutton = gtk_spin_button_new (GTK_ADJUSTMENT (spinbutton_adj), 1, 0);
  gimp_help_set_help_data (spinbutton, _("Detail level for creating random pattern (cloud layer)"), NULL);
  gtk_widget_show (spinbutton);
  gtk_table_attach (GTK_TABLE (table1), spinbutton, 3, 4, row, row+1,
                    (GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);
  g_signal_connect (G_OBJECT (spinbutton_adj), "value_changed", G_CALLBACK (gimp_int_adjustment_update), &cuvals->detail);
  wcd->detail_spinbutton_adj = spinbutton_adj;
  wcd->detail_spinbutton = spinbutton;

  row++;

  /* pattern  */
  label = gtk_label_new (_("Layer Pattern:"));
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



  /* the frame */
  frame1 = gimp_frame_new (_("Fireshape options"));

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
  label = gtk_label_new (_("Create Fireshape:"));
  gtk_widget_show (label);
  wcd->shapeLabel = label;
  gtk_table_attach (GTK_TABLE (table1), label, 0, 1, row, row+1,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);
  gtk_misc_set_alignment (GTK_MISC (label), 0, 0.5);

  checkbutton = gtk_check_button_new_with_label (" ");
  g_object_set_data (G_OBJECT (checkbutton), "wcd", wcd);
  wcd->shapeCheckbutton = checkbutton;
  gimp_help_set_help_data (checkbutton, _("ON: create fire shape layer according to options. OFF: Use external fire shape layer. "), NULL);
  gtk_widget_show (checkbutton);
  gtk_table_attach( GTK_TABLE(table1), checkbutton, 1, 2, row, row+1,
                    GTK_FILL, 0, 0, 0 );
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (checkbutton), wcd->vals->forceShapeLayer);
  g_signal_connect (checkbutton, "toggled",
                    G_CALLBACK (on_gboolean_button_update),
                    &wcd->vals->forceShapeLayer);

  
  /* useTrapezoidShape checkbutton  */
  label = gtk_label_new (_("Trapezoid:"));
  gtk_widget_show (label);
  gtk_table_attach (GTK_TABLE (table1), label, 2, 3, row, row+1,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);
  gtk_misc_set_alignment (GTK_MISC (label), 0, 0.5);

  checkbutton = gtk_check_button_new_with_label (" ");
  g_object_set_data (G_OBJECT (checkbutton), "wcd", wcd);
  wcd->useTrapezoidShapeCheckbutton = checkbutton;
  gimp_help_set_help_data (checkbutton, _("ON: Render trapezoid shaped fire, OFF: render fire at full image width"), NULL);
  gtk_widget_show (checkbutton);
  gtk_table_attach( GTK_TABLE(table1), checkbutton, 3, 4, row, row+1,
                    GTK_FILL, 0, 0, 0 );
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (checkbutton), cuvals->useTrapezoidShape);
  g_signal_connect (checkbutton, "toggled",
                    G_CALLBACK (on_gboolean_button_update),
                    &cuvals->useTrapezoidShape);



  row++;
  
  /* flameHeight spinbutton  */
  label = gtk_label_new (_("Flame Height:"));
  gtk_widget_show (label);
  gtk_table_attach (GTK_TABLE (table1), label, 0, 1, row, row+1,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);
  gtk_misc_set_alignment (GTK_MISC (label), 0, 0.5);

  spinbutton_adj = gtk_adjustment_new (cuvals->flameHeight, 0.1, 4.0, 0.1, 1, 0);
  spinbutton = gtk_spin_button_new (GTK_ADJUSTMENT (spinbutton_adj), 1, 2);
  gimp_help_set_help_data (spinbutton, _("Height of the flame (1.0 refers to full image height)"), NULL);
  gtk_widget_show (spinbutton);
  gtk_table_attach (GTK_TABLE (table1), spinbutton, 1, 2, row, row+1,
                    (GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);
  g_signal_connect (G_OBJECT (spinbutton_adj), "value_changed", G_CALLBACK (gimp_double_adjustment_update), &cuvals->flameHeight);
  wcd->flameHeight_spinbutton_adj = spinbutton_adj;
  wcd->flameHeight_spinbutton = spinbutton;


  /* flameBorder spinbutton  */
  label = gtk_label_new (_("Flame Border:"));
  gtk_widget_show (label);
  gtk_table_attach (GTK_TABLE (table1), label, 2, 3, row, row+1,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);
  gtk_misc_set_alignment (GTK_MISC (label), 0, 0.5);

  spinbutton_adj = gtk_adjustment_new (cuvals->flameBorder, 0.0, 0.5, 0.1, 1, 0);
  spinbutton = gtk_spin_button_new (GTK_ADJUSTMENT (spinbutton_adj), 1, 2);
  gimp_help_set_help_data (spinbutton, _("border of the flame"), NULL);
  gtk_widget_show (spinbutton);
  gtk_table_attach (GTK_TABLE (table1), spinbutton, 3, 4, row, row+1,
                    (GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);
  g_signal_connect (G_OBJECT (spinbutton_adj), "value_changed", G_CALLBACK (gimp_double_adjustment_update), &cuvals->flameBorder);
  wcd->flameBorder_spinbutton_adj = spinbutton_adj;
  wcd->flameBorder_spinbutton = spinbutton;


  row++;

  /* flameWidth checkbuttons  */
  label = gtk_label_new (_("FlameWidth:"));
  gtk_widget_show (label);
  gtk_table_attach (GTK_TABLE (table1), label, 0, 1, row, row+1,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);
  gtk_misc_set_alignment (GTK_MISC (label), 0, 0.5);

  spinbutton_adj = gtk_adjustment_new (cuvals->flameWidthBase, 0.0, 1.0, 0.1, 1, 0);
  spinbutton = gtk_spin_button_new (GTK_ADJUSTMENT (spinbutton_adj), 1, 2);
  gimp_help_set_help_data (spinbutton, _("width of the flame at base line (1.0 for full image width)"), NULL);
  gtk_widget_show (spinbutton);
  gtk_table_attach (GTK_TABLE (table1), spinbutton, 1, 2, row, row+1,
                    (GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);
  g_signal_connect (G_OBJECT (spinbutton_adj), "value_changed", G_CALLBACK (gimp_double_adjustment_update), &cuvals->flameWidthBase);
  wcd->flameWidthBase_spinbutton_adj = spinbutton_adj;
  wcd->flameWidthBase_spinbutton = spinbutton;

  label = gtk_label_new (_("Top:"));
  gtk_widget_show (label);
  gtk_table_attach (GTK_TABLE (table1), label, 2, 3, row, row+1,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);
  gtk_misc_set_alignment (GTK_MISC (label), 0, 0.5);


  spinbutton_adj = gtk_adjustment_new (cuvals->flameWidthTop, 0.0, 1.0, 0.1, 1, 0);
  spinbutton = gtk_spin_button_new (GTK_ADJUSTMENT (spinbutton_adj), 1, 2);
  gimp_help_set_help_data (spinbutton, _("width of the flame at flame height (1.0 for full image width)"), NULL);
  gtk_widget_show (spinbutton);
  gtk_table_attach (GTK_TABLE (table1), spinbutton, 3, 4, row, row+1,
                    (GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);
  g_signal_connect (G_OBJECT (spinbutton_adj), "value_changed", G_CALLBACK (gimp_double_adjustment_update), &cuvals->flameWidthTop);
  wcd->flameWidthTop_spinbutton_adj = spinbutton_adj;
  wcd->flameWidthTop_spinbutton = spinbutton;

  row++;

  /* flameOffestX spinbutton  */
  label = gtk_label_new (_("Flame Center:"));
  gtk_widget_show (label);
  gtk_table_attach (GTK_TABLE (table1), label, 0, 1, row, row+1,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);
  gtk_misc_set_alignment (GTK_MISC (label), 0, 0.5);

  spinbutton_adj = gtk_adjustment_new (cuvals->flameOffestX, -1.0, 1.0, 0.1, 1, 0);
  spinbutton = gtk_spin_button_new (GTK_ADJUSTMENT (spinbutton_adj), 1, 2);
  gimp_help_set_help_data (spinbutton, _("horizontal offset of the flame center (0 for center, -0.5 left border +0.5 at right border of the image)"), NULL);
  gtk_widget_show (spinbutton);
  gtk_table_attach (GTK_TABLE (table1), spinbutton, 1, 2, row, row+1,
                    (GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);
  g_signal_connect (G_OBJECT (spinbutton_adj), "value_changed", G_CALLBACK (gimp_double_adjustment_update), &cuvals->flameOffestX);
  wcd->flameOffestX_spinbutton_adj = spinbutton_adj;
  wcd->flameOffestX_spinbutton = spinbutton;

  row++;


  label = gtk_label_new (_("Fire Shape:"));
  gtk_widget_show (label);
  wcd->fireShapeLabel = label;
  gtk_table_attach (GTK_TABLE (table1), label, 0, 1, row, row+1,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);
  gtk_misc_set_alignment (GTK_MISC (label), 0, 0.5);

  combo = gimp_layer_combo_box_new (p_pattern_layer_constrain, wcd);
  wcd->fireShapeCombo = combo;
  gtk_widget_show(combo);
  gtk_table_attach(GTK_TABLE(table1), combo, 1, 4, row, row+1,
                   GTK_EXPAND | GTK_FILL, 0, 0, 0);

  gimp_help_set_help_data(combo,
                       _("Select an already existing fire shape layer (from previous run)")
                       , NULL);

  gimp_int_combo_box_connect (GIMP_INT_COMBO_BOX (combo),
                              wcd->existingFireShapeLayerId,                      /* initial value */
                              G_CALLBACK (p_cloud_layer_menu_callback),
                              &wcd->existingFireShapeLayerId);


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

  /* createFireLayer checkbutton  */
  label = gtk_label_new (_("Create FireLayer:"));
  gtk_widget_show (label);
  gtk_table_attach (GTK_TABLE (table1), label, 0, 1, row, row+1,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);
  gtk_misc_set_alignment (GTK_MISC (label), 0, 0.5);

  checkbutton = gtk_check_button_new_with_label (" ");
  g_object_set_data (G_OBJECT (checkbutton), "wcd", wcd);
  wcd->createFireLayerCheckbutton = checkbutton;
  gimp_help_set_help_data (checkbutton, _("ON: Render fire pattern effect as separate layer, OFF: merge rendered effect onto processed layer"), NULL);
  gtk_widget_show (checkbutton);
  gtk_table_attach( GTK_TABLE(table1), checkbutton, 1, 2, row, row+1,
                    GTK_FILL, 0, 0, 0 );
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (checkbutton), cuvals->createFireLayer);
  g_signal_connect (checkbutton, "toggled",
                    G_CALLBACK (on_gboolean_button_update),
                    &cuvals->createFireLayer);



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

  radiobutton = gtk_radio_button_new_with_label (vbox1_group, _("Burn"));
  wcd->radio_blend_burn = radiobutton;
  vbox1_group = gtk_radio_button_get_group (GTK_RADIO_BUTTON (radiobutton));
  gtk_widget_show (radiobutton);
  gtk_box_pack_start (GTK_BOX (vbox1), radiobutton, FALSE, FALSE, 0);
  g_signal_connect (G_OBJECT (radiobutton),     "clicked",  G_CALLBACK (on_blend_radio_callback), wcd);
  if(wcd->vals->blendNum == BLEND_NUM_BURN)
  {
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON (radiobutton), TRUE);
  }

  radiobutton = gtk_radio_button_new_with_label (vbox1_group, _("Subtract"));
  wcd->radio_blend_subtract = radiobutton;
  vbox1_group = gtk_radio_button_get_group (GTK_RADIO_BUTTON (radiobutton));
  gtk_widget_show (radiobutton);
  gtk_box_pack_start (GTK_BOX (vbox1), radiobutton, FALSE, FALSE, 0);
  g_signal_connect (G_OBJECT (radiobutton),     "clicked",  G_CALLBACK (on_blend_radio_callback), wcd);
  if(wcd->vals->blendNum == BLEND_NUM_SUBTRACT)
  {
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON (radiobutton), TRUE);
  }

  radiobutton = gtk_radio_button_new_with_label (vbox1_group, _("Multiply"));
  wcd->radio_blend_multiply = radiobutton;
  vbox1_group = gtk_radio_button_get_group (GTK_RADIO_BUTTON (radiobutton));
  gtk_widget_show (radiobutton);
  gtk_box_pack_start (GTK_BOX (vbox1), radiobutton, FALSE, FALSE, 0);
  g_signal_connect (G_OBJECT (radiobutton),     "clicked",  G_CALLBACK (on_blend_radio_callback), wcd);
  if(wcd->vals->blendNum == BLEND_NUM_MULTIPLY)
  {
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON (radiobutton), TRUE);
  }



  row++;

  /* useTransparentBg checkbutton  */
  label = gtk_label_new (_("Transparent BG:"));
  gtk_widget_show (label);
  gtk_table_attach (GTK_TABLE (table1), label, 0, 1, row, row+1,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);
  gtk_misc_set_alignment (GTK_MISC (label), 0, 0.5);


  checkbutton = gtk_check_button_new_with_label (" ");
  g_object_set_data (G_OBJECT (checkbutton), "wcd", wcd);
  wcd->useTransparentBgCheckbutton = checkbutton;
  gimp_help_set_help_data (checkbutton, _("ON: Render fire layer with transparent background, OFF: render with black background"), NULL);
  gtk_widget_show (checkbutton);
  gtk_table_attach( GTK_TABLE(table1), checkbutton, 1, 2, row, row+1,
                    GTK_FILL, 0, 0, 0 );
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (checkbutton), cuvals->useTransparentBg);
  g_signal_connect (checkbutton, "toggled",
                    G_CALLBACK (on_gboolean_button_update),
                    &cuvals->useTransparentBg);

  label = gtk_label_new (_("Opacity:"));
  gtk_widget_show (label);
  gtk_table_attach (GTK_TABLE (table1), label, 2, 3, row, row+1,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);
  gtk_misc_set_alignment (GTK_MISC (label), 0, 0.5);


  /* fireOpacity spinbutton  */
  spinbutton_adj = gtk_adjustment_new (cuvals->fireOpacity, 0.0, 100, 1.0, 10, 0);
  spinbutton = gtk_spin_button_new (GTK_ADJUSTMENT (spinbutton_adj), 1, 3);
  gimp_help_set_help_data (spinbutton, _("The opacity of the flames"), NULL);
  gtk_widget_show (spinbutton);
  gtk_table_attach (GTK_TABLE (table1), spinbutton, 3, 4, row, row+1,
                    (GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);
  g_signal_connect (G_OBJECT (spinbutton_adj), "value_changed", G_CALLBACK (gimp_double_adjustment_update), &cuvals->fireOpacity);
  wcd->fireOpacity_spinbutton_adj = spinbutton_adj;
  wcd->fireOpacity_spinbutton = spinbutton;

  row++;

  /* reverseGradient checkbutton  */
  label = gtk_label_new (_("Reverse Gradient:"));
  gtk_widget_show (label);
  gtk_table_attach (GTK_TABLE (table1), label, 0, 1, row, row+1,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);
  gtk_misc_set_alignment (GTK_MISC (label), 0, 0.5);


  checkbutton = gtk_check_button_new_with_label (" ");
  g_object_set_data (G_OBJECT (checkbutton), "wcd", wcd);
  wcd->reverseGradientCheckbutton = checkbutton;
  gimp_help_set_help_data (checkbutton, _("ON: use reverse gradient colors, OFF: use gradient colors"), NULL);
  gtk_widget_show (checkbutton);
  gtk_table_attach( GTK_TABLE(table1), checkbutton, 1, 2, row, row+1,
                    GTK_FILL, 0, 0, 0 );
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (checkbutton), cuvals->reverseGradient);
  g_signal_connect (checkbutton, "toggled",
                    G_CALLBACK (on_gboolean_button_update),
                    &cuvals->reverseGradient);

  /* the gradient select button */
  {
  GtkWidget *gradientButton;
  
  gradientButton = gimp_gradient_select_button_new ("Gradient", &cuvals->gradientName[0]);
  wcd->gradient_select = gradientButton;
  g_signal_connect (gradientButton, "gradient-set",
                    G_CALLBACK (p_gradient_changed_callback), wcd);
  gtk_widget_show (gradientButton);
  gtk_table_attach( GTK_TABLE(table1), gradientButton, 2, 4, row, row+1,
                    GTK_FILL, 0, 0, 0 );
  }

  /* -- */


  dialog_action_area1 = GTK_DIALOG (dialog1)->action_area;
  gtk_widget_show (dialog_action_area1);
  gtk_container_set_border_width (GTK_CONTAINER (dialog_action_area1), 10);

  p_init_widget_values(wcd);
  p_update_widget_sensitivity (wcd);

  gtk_widget_show (dialog1);

  gtk_main ();
  gdk_flush ();

  if((!wcd->createNewPattern) && (wcd->countPotentialCloudLayers > 0))
  {
    wcd->vals->cloudLayer1 = wcd->existingCloud1Id;
  }
  else
  {
    /* use -1 to force creation of new cloud layer */
    wcd->vals->cloudLayer1 = -1;
 }

  if(!wcd->vals->forceShapeLayer)
  {
    wcd->vals->fireShapeLayer = wcd->existingFireShapeLayerId;
  }
  else
  {
    /* use -1 to force creation of new fireShapeLayer */
     wcd->vals->fireShapeLayer = -1;
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
                  { GIMP_PDB_INT32,      "detail", "detail level for creating cloud pattern (1 upto 15)"},
                  { GIMP_PDB_INT32,      "useTrapezoidShape", "TRUE (1): trapezoid fire shape, FALSE (0) rectangular shape)"},
                  { GIMP_PDB_FLOAT,      "flameHeight", "height of the flame (0.1 upto 4.0) where 1.0 is full image height"},
                  { GIMP_PDB_FLOAT,      "flameWidthBase", "width of the flame at the baseline (0.1 upto 1.0) where 1.0 is full image width"},
                  { GIMP_PDB_FLOAT,      "flameWidthTop", "width of the flame at fireHeight (0.1 upto 1.0) where 1.0 is full image width"},
                  { GIMP_PDB_FLOAT,      "flameBorder", "fading of the fire shape on left/right border  (0.1 upto 0.5) where 0.5 is half image width"},
                  { GIMP_PDB_FLOAT,      "flameOffestX", "horizontal offset of the flame center (0 for center, -0.5 left border +0.5 at right border of the image)"},
                  { GIMP_PDB_INT32,      "blendNum", "Blend mode { BURN (0), SUBTRACT (1), MULTIPLY (2) }"},
                  { GIMP_PDB_FLOAT,      "stretchHeight", "vertical stretch factor for flame pattern (1.0 to 3.0)"},
                  { GIMP_PDB_FLOAT,      "shiftPhaseY", "Vertical shift phase (0.0 to 1.0 full height, values >= 1.0 foldback to range 0.0 - 0.99999)"},
                  { GIMP_PDB_INT32,      "createImage", "TRUE (1): create a new image with n copies of the input drawable and render complete animation effect on those copies. FALSE (0): render only one phase of the anim effect on the input drawable"},
                  { GIMP_PDB_INT32,      "nframes", "number of layer to be created in case createImage option is TRUE (1)"},
                  { GIMP_PDB_INT32,      "createFireLayer", "TRUE (1): create a new image with n copies of the input drawable and render complete animation effect on those copies. FALSE (0): render only one phase of the anim effect on the input drawable"},
                  { GIMP_PDB_INT32,      "useTransparentBg", "TRUE (1): create a new image with n copies of the input drawable and render complete animation effect on those copies. FALSE (0): render only one phase of the anim effect on the input drawable"},
                  { GIMP_PDB_FLOAT,      "fireOpacity", "fire layer opacity in percent (0.0 to 100.0)"},
                  { GIMP_PDB_INT32,      "seed1", "Seed for creating random pattern (cloud1 layer) use 0 for random value"},
                  { GIMP_PDB_DRAWABLE,   "cloudLayer1", "-1 automatically create cloud layer1 that is required to render or pass a layer id that was returned in a previous call to this plug-in (for rendering the next phase of the effect in one call per frame manner)"},
                  { GIMP_PDB_DRAWABLE,   "fireShapeLayer", "-1 automatically create fire shape layer that is required to render or pass a layer id that was returned in a previous call to this plug-in (for rendering the next phase of the effect in one call per frame manner)"},
                  { GIMP_PDB_INT32,      "reverseGradient", "TRUE (1) use reversed gradient, FALSE (0) use gradient as it is"},
                  { GIMP_PDB_STRING,     "gradientName", "Name of the gradient to be used to colorize the firepattern"}
  };
  static int nargs = sizeof(args) / sizeof(args[0]);

  static GimpParamDef return_vals[] =
  {
    { GIMP_PDB_DRAWABLE, "imageId",        "the image that was newly created (when creatImage is TRUE) or otherwise the image id tha contains the processed drawable" },
    { GIMP_PDB_DRAWABLE, "cloudLayer",     "the pattern cloud layer" },
    { GIMP_PDB_DRAWABLE, "fireShapeLayer", "the fire shape layer" }
  };
  static int nreturn_vals = sizeof(return_vals) / sizeof(return_vals[0]);


  static firepattern_val_t firevals;

  static GimpLastvalDef lastvals[] =
  {
    GIMP_LASTVALDEF_GDOUBLE     (GIMP_ITER_FALSE,  firevals.scalex,                  "scalex"),
    GIMP_LASTVALDEF_GDOUBLE     (GIMP_ITER_FALSE,  firevals.scaley,                  "scaley"),
    GIMP_LASTVALDEF_GINT32      (GIMP_ITER_FALSE,  firevals.detail,                  "detail"),
    GIMP_LASTVALDEF_GBOOLEAN    (GIMP_ITER_FALSE,  firevals.useTrapezoidShape,       "useTrapezoidShape"),
    GIMP_LASTVALDEF_GDOUBLE     (GIMP_ITER_TRUE,   firevals.flameHeight,             "flameHeight"),
    GIMP_LASTVALDEF_GDOUBLE     (GIMP_ITER_TRUE,   firevals.flameWidthBase,          "flameWidthBase"),
    GIMP_LASTVALDEF_GDOUBLE     (GIMP_ITER_TRUE,   firevals.flameWidthTop,           "flameWidthTop"),
    GIMP_LASTVALDEF_GDOUBLE     (GIMP_ITER_TRUE,   firevals.flameBorder,             "flameBorder"),
    GIMP_LASTVALDEF_GDOUBLE     (GIMP_ITER_TRUE,   firevals.flameOffestX,            "flameOffestX"),
    GIMP_LASTVALDEF_GINT32      (GIMP_ITER_FALSE,  firevals.blendNum,                "blendNum"),
    GIMP_LASTVALDEF_GDOUBLE     (GIMP_ITER_TRUE,   firevals.stretchHeight,           "stretchHeight"),
    GIMP_LASTVALDEF_GDOUBLE     (GIMP_ITER_TRUE,   firevals.shiftPhaseY,             "shiftPhaseY"),
    GIMP_LASTVALDEF_GBOOLEAN    (GIMP_ITER_FALSE,  firevals.createImage,             "createImage"),
    GIMP_LASTVALDEF_GINT32      (GIMP_ITER_FALSE,  firevals.nframes,                 "nframes"),
    GIMP_LASTVALDEF_GBOOLEAN    (GIMP_ITER_FALSE,  firevals.createFireLayer,         "createFireLayer"),
    GIMP_LASTVALDEF_GBOOLEAN    (GIMP_ITER_FALSE,  firevals.useTransparentBg,        "useTransparentBg"),
    GIMP_LASTVALDEF_GDOUBLE     (GIMP_ITER_TRUE,   firevals.fireOpacity,             "fireOpacity"),
    GIMP_LASTVALDEF_GINT32      (GIMP_ITER_FALSE,  firevals.seed1,                   "seed1"),
    GIMP_LASTVALDEF_DRAWABLE    (GIMP_ITER_TRUE,   firevals.cloudLayer1,             "cloudLayer1"),
    GIMP_LASTVALDEF_DRAWABLE    (GIMP_ITER_TRUE,   firevals.fireShapeLayer,          "fireShapeLayer"),
    GIMP_LASTVALDEF_GBOOLEAN    (GIMP_ITER_FALSE,  firevals.forceShapeLayer,         "forceShapeLayer"),
    GIMP_LASTVALDEF_GBOOLEAN    (GIMP_ITER_FALSE,  firevals.reverseGradient,         "reverseGradient"),
    GIMP_LASTVALDEF_ARRAY       (GIMP_ITER_FALSE,  firevals.gradientName,            "gradientNameArray"),
    GIMP_LASTVALDEF_GCHAR       (GIMP_ITER_FALSE,  firevals.gradientName[0],         "gradientNameChar"),
  };

  gimp_plugin_domain_register (GETTEXT_PACKAGE, LOCALEDIR);

  /* registration for last values buffer structure (for animated filter apply) */
  gimp_lastval_desc_register(PLUG_IN_NAME,
                             &firevals,
                             sizeof(firevals),
                             G_N_ELEMENTS (lastvals),
                             lastvals);

  /* the actual installation of the firepattern plugin */
  gimp_install_procedure (PLUG_IN_NAME,
                          PLUG_IN_DESCRIPTION,
                         "This Plugin generates an animated fire effect."
                         " This Plugin can render the animation in one call, where the input drawable is copied n-times to a newly created image. "
                         " Optional this Plugin can be called to render just one frame/phase of the animated effect."
                         " This type of animated call on multiple already existent layers (or frames) with varying shiftPhase parameter"
                         " is supported by the GIMP-GAP filter all layers or by applying as filter via GIMP-GAP modify frames feature. "
                         " Note that the render one frame per call style offers more flexibility where you can "
                         " apply the flame with varying shape, color and opacity for each rendered frame atomatically. "
                          ,
                          PLUG_IN_AUTHOR,
                          PLUG_IN_COPYRIGHT,
                          GAP_VERSION_WITH_DATE,
                          N_("Fire Pattern..."),
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
  firepattern_val_t l_cuvals;
  FirePatternDialog   *wcd = NULL;
  firepattern_context_t  firepattern_context;
  firepattern_context_t  *ctxt;

  gint32    l_image_id = -1;
  gint32    l_drawable_id = -1;
  gint32    l_handled_drawable_id = -1;



  /* Get the runmode from the in-parameters */
  GimpRunMode run_mode = param[0].data.d_int32;

  ctxt = &firepattern_context;
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
  p_init_cuvals(&l_cuvals);

  if(gap_debug)
  {
    printf("Run Firepattern on l_drawable_id:%d (%s)\n"
      ,(int)l_drawable_id
      ,p_drawable_get_name(l_drawable_id)
      );
  }

  if(status == GIMP_PDB_SUCCESS)
  {
    /* how are we running today? */
    switch (run_mode)
     {
      case GIMP_RUN_INTERACTIVE:
        wcd = g_malloc (sizeof (FirePatternDialog));
        wcd->run = FALSE;
        wcd->drawable_id = l_drawable_id;
        /* Get information from the dialog */
        do_dialog(wcd, &l_cuvals);
        wcd->show_progress = TRUE;
        break;

      case GIMP_RUN_NONINTERACTIVE:
        /* check to see if invoked with the correct number of parameters */
        if (nparams == 25)
        {

           l_cuvals.scalex            = param[3].data.d_float;
           l_cuvals.scalex            = param[4].data.d_float;
           l_cuvals.detail            = param[5].data.d_int32;
           l_cuvals.useTrapezoidShape = param[6].data.d_int32;
           l_cuvals.flameHeight       = param[7].data.d_float;
           l_cuvals.flameWidthBase    = param[8].data.d_float;
           l_cuvals.flameWidthTop     = param[9].data.d_float;
           l_cuvals.flameBorder       = param[10].data.d_float;
           l_cuvals.flameOffestX      = param[11].data.d_float;
           l_cuvals.blendNum          = param[12].data.d_int32;
           l_cuvals.stretchHeight     = param[13].data.d_float;
           l_cuvals.shiftPhaseY       = param[14].data.d_float;
           l_cuvals.createImage       = param[15].data.d_int32;
           l_cuvals.nframes           = param[16].data.d_int32;
           l_cuvals.createFireLayer   = param[17].data.d_int32;
           l_cuvals.useTransparentBg  = param[18].data.d_int32;
           l_cuvals.fireOpacity       = param[19].data.d_float;
           l_cuvals.seed1             = param[20].data.d_int32;
           l_cuvals.cloudLayer1       = param[21].data.d_drawable;
           l_cuvals.fireShapeLayer    = param[22].data.d_drawable;
           l_cuvals.reverseGradient   = param[23].data.d_int32;

           l_cuvals.gradientName[0] = '\0';
           if(param[24].data.d_string != NULL)
           {
              g_snprintf(l_cuvals.gradientName, MAX_GRADIENT_NAME -1, "%s", param[23].data.d_string);
           }
           if(l_cuvals.gradientName[0] == '\0')
           {
              g_snprintf (l_cuvals.gradientName, MAX_GRADIENT_NAME -1, "%s", DEFAULT_GRADIENT_NAME);
           }


           
           p_check_for_valid_cloud_layers(&l_cuvals);
        }
        else
        {
          status = GIMP_PDB_CALLING_ERROR;
        }
        break;

      case GIMP_RUN_WITH_LAST_VALS:
        wcd = g_malloc (sizeof (FirePatternDialog));
        wcd->run = TRUE;
        wcd->show_progress = TRUE;
        wcd->createNewPattern = FALSE;
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
        success = p_run_renderFirePattern(l_drawable_id, &l_cuvals, ctxt);
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
  values[1].data.d_int32 = ctxt->image_id;           /* return the image id of handled layer (or of the newly created anim image */
  values[2].data.d_int32 = l_cuvals.cloudLayer1;     /* return the id of cloud1 layer */
  values[3].data.d_int32 = l_cuvals.fireShapeLayer;  /* return the id of fire shape layer */

}  /* end run */
