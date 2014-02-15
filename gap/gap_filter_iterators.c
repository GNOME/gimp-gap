/* gap_filter_iterators.c
 *
 * 1998.01.29 hof (Wolfgang Hofer)
 *
 * GAP ... Gimp Animation Plugins
 *
 * This Module contains:
 * - Implementation of c common Iterator Procedure
 *     for those Plugins
 *     - that can operate on a single drawable,
 *     - and have paramters for varying. (that are documented in the LASTVALUES Decription file)
 *
 * If New plugins were added to the gimp, or existing ones were updated,
 * the Authors should supply original _Iterator Procedures
 * to modify the settings for the plugin when called step by step
 * on animated multilayer Images.
 * Another simpler way is to register the structure description of the LAST_VALUES Buffer.
 * (so that the common iterator can be used)
 *
 *
 * Common things to all Iteratur Plugins:
 * Interface:   run_mode        # is always GIMP_RUN_NONINTERACTIVE
 *              total_steps     # total number of handled layers (drawables)
 *              current_step    # current layer (beginning wit 0)
 *                               has type gdouble for later extensions
 *                               to non-linear iterations.
 *                               the iterator always computes linear inbetween steps,
 *                               but the (central) caller may fake step 1.2345 in the future
 *                               for logaritmic iterations or userdefined curves.
 *
 * Naming Convention:
 *    Iterators must have the name of the plugin (PDB proc_name), whose values
 *    are iterated, with Suffix
 *      "_Iterator"
 *      "_Iterator_ALT"    (if not provided within the original Plugin's sources)
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

/* Change Log:
 * gimp     1.3.26a; 2004/02/07  hof: added p_delta_CML_PARAM, removed gfig
 * gimp     1.3.20b; 2003/09/20  hof: include gap_dbbrowser_utils.h, datatype support for guint, guint32
 *                                    replaced t_GckVector3 and t_GckColor by GimpVector3 and GimpRGB
 * gimp     1.3.17a; 2003/07/29  hof: fixed signed/unsigned comparison warnings
 * gimp     1.3.12a; 2003/05/02  hof: merge into CVS-gimp-gap project, added iter_ALT Procedures again
 * gimp   1.3.8a;    2002/09/21  hof: gap_lastvaldesc
 * gimp   1.3.5a;    2002/04/14  hof: removed Stuctured Iterators (Material, Light ..)
 * gimp   1.3.4b;    2002/03/24  hof: support COMMON_ITERATOR, removed iter_ALT Procedures
 * version gimp 1.1.17b  2000.02.22  hof: - removed limit PLUGIN_DATA_SIZE
 * 1999.11.16 hof: added p_delta_gintdrawable
 * 1999.06.21 hof: removed Colorify iterator
 * 1999.03.14 hof: added iterators for gimp 1.1.3 prerelease
 *                 iterator code reorganized in _iter_ALT.inc Files
 * 1998.06.12 hof: added p_delta_drawable (Iterate layers in the layerstack)
 *                 this enables to apply an animated bumpmap.
 * 1998.01.29 hof: 1st release
 */
#include "config.h"

/* SYTEM (UNIX) includes */
#include <stdio.h>
#include <string.h>
#ifdef HAVE_SYS_WAIT_H
#include <sys/wait.h>
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

/* GIMP includes */
#include "gtk/gtk.h"
#include "libgimp/gimp.h"


/* GAP includes */
#include "gap_lastvaldesc.h"
#include "gap_filter.h"
#include "gap_filter_iterators.h"
#include "gap_dbbrowser_utils.h"
#include "gap_fmac_context.h"
#include "gap_frame_fetcher.h"
#include "gap_drawable_vref_parasite.h"
#include "gap_lib.h"
#include "gap/gap_layer_copy.h"


static gchar *g_plugin_data_from = NULL;
static gchar *g_plugin_data_to = NULL;

extern int gap_debug;

/* Funtion type for use in the common iterator
 */
typedef void (*GapDeltaFunctionType) (void *val, void *val_from, void *val_to, gint32 total_steps, gdouble current_step);


typedef struct JmpTableEntryType
{
  GapDeltaFunctionType func_ptr;
  gint32               item_size;
} JmpTableEntryType;

typedef  struct IterStackItemType
{
   GimpLastvalType lastval_type;   /* only ARRAY and STRUCT are pushed on the stack */
   gint32       elem_size;  /* only for ARRAY and STRUCT */
   gint32       arr_count;
   gint         idx;
   gint32       iter_flag;
}IterStackItemType;

typedef  struct IterStackType
{
   IterStackItemType *stack_arr;   /* only ARRAY and STRUCT are pushed on the stack */
   gint32       stack_max_elem;    /* only for ARRAY and STRUCT */
   gint32       stack_top;         /* current stack top index (-1 for empty stack */
   gint         idx;
}IterStackType;


static JmpTableEntryType jmp_table[GIMP_LASTVAL_END];

static gchar *
p_alloc_plugin_data(char *key)
{
   int l_len;
   gchar *l_plugin_data;

   l_len = gimp_get_data_size (key);
   if(l_len < 1)
   {
      fprintf(stderr, "ERROR: no stored data found for Key %s\n", key);
      return NULL;
   }
   l_plugin_data = g_malloc0(l_len+1);

   if(gap_debug) printf("DEBUG  Key:%s  plugin_data length %d\n", key, (int)l_len);
   return (l_plugin_data);
}

typedef struct {
        guchar color[3];
} t_color;

typedef struct {
        gint color[3];
} t_gint_color;

typedef struct t_CML_PARAM
{
  gint    function;
  gint    composition;
  gint    arrange;
  gint    cyclic_range;
  gdouble mod_rate;             /* diff / old-value */
  gdouble env_sensitivity;      /* self-diff : env-diff */
  gint    diffusion_dist;
  gdouble ch_sensitivity;
  gint    range_num;
  gdouble power;
  gdouble parameter_k;
  gdouble range_l;
  gdouble range_h;
  gdouble mutation_rate;
  gdouble mutation_dist;
} t_CML_PARAM;

static void                 p_delta_drawable_simple(gint32 *val, gint32 val_from, gint32 val_to
                                 , gint32 total_steps, gdouble current_step);

static gint32               p_capture_image_name_and_assign_pesistent_id(GapFmacContext *fmacContext
                                  , gint32 drawable_id);
static gboolean             p_iteration_by_pesistent_id(GapFmacContext *fmacContext
                                  ,gint32 *val, gint32 val_from, gint32 val_to
                                  , gint32 total_steps, gdouble current_step);

/* ----------------------------------------------------------------------
 * iterator functions for basic datatypes
 * (were called from the generated procedures)
 * ----------------------------------------------------------------------
 */

static void p_delta_long(long *val, long val_from, long val_to, gint32 total_steps, gdouble current_step)
{
    double     delta;

    if(total_steps < 1) return;

    delta = ((double)(val_to - val_from) / (double)total_steps) * ((double)total_steps - current_step);
    *val  = val_from + delta;

    if(gap_debug) printf("DEBUG: p_delta_long from: %ld to: %ld curr: %ld    delta: %f\n",
                                  val_from, val_to, *val, delta);
}
static void p_delta_short(short *val, short val_from, short val_to, gint32 total_steps, gdouble current_step)
{
    double     delta;

    if(total_steps < 1) return;

    delta = ((double)(val_to - val_from) / (double)total_steps) * ((double)total_steps - current_step);
    *val  = val_from + delta;
}
static void p_delta_int(int *val, int val_from, int val_to, gint32 total_steps, gdouble current_step)
{
     double     delta;

     if(total_steps < 1) return;

     delta = ((double)(val_to - val_from) / (double)total_steps) * ((double)total_steps - current_step);
     *val  = val_from + delta;
}
static void p_delta_gint(gint *val, gint val_from, gint val_to, gint32 total_steps, gdouble current_step)
{
    double     delta;

    if(total_steps < 1) return;

    delta = ((double)(val_to - val_from) / (double)total_steps) * ((double)total_steps - current_step);
    *val  = val_from + delta;
}
static void p_delta_guint(guint *val, guint val_from, guint val_to, gint32 total_steps, gdouble current_step)
{
    double     delta;

    if(total_steps < 1) return;

    delta = ((double)(val_to - val_from) / (double)total_steps) * ((double)total_steps - current_step);
    *val  = val_from + delta;
}
static void p_delta_gint32(gint32 *val, gint32 val_from, gint32 val_to, gint32 total_steps, gdouble current_step)
{
    double     delta;

    if(total_steps < 1) return;

    delta = ((double)(val_to - val_from) / (double)total_steps) * ((double)total_steps - current_step);
    *val  = val_from + delta;
}
static void p_delta_guint32(guint32 *val, guint32 val_from, guint32 val_to, gint32 total_steps, gdouble current_step)
{
    double     delta;

    if(total_steps < 1) return;

    delta = ((double)(val_to - val_from) / (double)total_steps) * ((double)total_steps - current_step);
    *val  = val_from + delta;
}
static void p_delta_char(char *val, char val_from, char val_to, gint32 total_steps, gdouble current_step)
{
    double     delta;

    if(total_steps < 1) return;

    delta = ((double)(val_to - val_from) / (double)total_steps) * ((double)total_steps - current_step);
    *val  = val_from + delta;
}
static void p_delta_guchar(guchar *val, char val_from, char val_to, gint32 total_steps, gdouble current_step)
{
    double     delta;

    if(total_steps < 1) return;

    delta = ((double)(val_to - val_from) / (double)total_steps) * ((double)total_steps - current_step);
    *val  = val_from + delta;
}
static void p_delta_gdouble(double *val, double val_from, double val_to, gint32 total_steps, gdouble current_step)
{
   double     delta;

   if(total_steps < 1) return;

   delta = ((double)(val_to - val_from) / (double)total_steps) * ((double)total_steps - current_step);
   *val  = val_from + delta;

   if(gap_debug) printf("DEBUG: p_delta_gdouble total: %d  from: %f to: %f curr: %f    delta: %f\n",
                                  (int)total_steps, val_from, val_to, *val, delta);
}
static void p_delta_gfloat(gfloat *val, gfloat val_from, gfloat val_to, gint32 total_steps, gdouble current_step)
{
    double     delta;

    if(total_steps < 1) return;

    delta = ((double)(val_to - val_from) / (double)total_steps) * ((double)total_steps - current_step);
    *val  = val_from + delta;

    if(gap_debug) printf("DEBUG: p_delta_gfloat total: %d  from: %f to: %f curr: %f    delta: %f\n",
                                   (int)total_steps, val_from, val_to, *val, delta);
}

static void p_delta_float(float *val, float val_from, float val_to, gint32 total_steps, gdouble current_step)
{
    double     delta;

    if(total_steps < 1) return;

    delta = ((double)(val_to - val_from) / (double)total_steps) * ((double)total_steps - current_step);
    *val  = val_from + delta;

    if(gap_debug) printf("DEBUG: p_delta_gdouble total: %d  from: %f to: %f curr: %f    delta: %f\n",
                                  (int)total_steps, val_from, val_to, *val, delta);
}
static void p_delta_color(t_color *val, t_color *val_from, t_color *val_to, gint32 total_steps, gdouble current_step)
{
    double     delta;
    guint l_idx;

    if(total_steps < 1) return;

    for(l_idx = 0; l_idx < 3; l_idx++)
    {
       delta = ((double)(val_to->color[l_idx] - val_from->color[l_idx]) / (double)total_steps) * ((double)total_steps - current_step);
       val->color[l_idx]  = val_from->color[l_idx] + delta;

       if(gap_debug) printf("DEBUG: p_delta_color[%d] total: %d  from: %d to: %d curr: %d    delta: %f  current_step: %f\n",
                                  (int)l_idx, (int)total_steps,
                                  (int)val_from->color[l_idx], (int)val_to->color[l_idx], (int)val->color[l_idx],
                                  delta, current_step);
    }
}
static void p_delta_gint_color(t_gint_color *val, t_gint_color *val_from, t_gint_color *val_to, gint32 total_steps, gdouble current_step)
{
    double     delta;
    guint l_idx;

    if(total_steps < 1) return;

    for(l_idx = 0; l_idx < 3; l_idx++)
    {
       delta = ((double)(val_to->color[l_idx] - val_from->color[l_idx]) / (double)total_steps) * ((double)total_steps - current_step);
       val->color[l_idx]  = val_from->color[l_idx] + delta;
    }
}


/* ---------------------------
 * p_delta_drawable
 * ---------------------------
 * the drawable iterator checks for current filtermacro context.
 * if there is such a context, iteration tries to fetch the relvant drawable 
 * via loading an image, or fetching a video frame.
 * In this case, the drawables (gint32 val_from and val_to) are expected to refere
 * to persistent Ids
 * e.g. are >= GAP_FMCT_MIN_PERSISTENT_DRAWABLE_ID
 * The image_ids of all fetched images are added to a list of last recent temp images
 * this list has to be deleted at end of filtermacro processing. if the list contains
 * more than NNNNN entries, the oldest entries are deleted at begin of a new fetch
 * to make space for the next temporary image.
 *
 * if the record flag of the filtermacro context is set
 * we do record identifying information about the the current drawable (*val)
 * in the persistent_id_lookup_filename. The drawable id is therfore mapped into a persitent
 * drawable_id that is unique in the persistent_id_lookup_file.
 * (the 1st entry starts with GAP_FMCT_MIN_PERSISTENT_DRAWABLE_ID)
 */
void
p_delta_drawable(gint32 *val, gint32 val_from, gint32 val_to, gint32 total_steps, gdouble current_step)
{
  gint            sizeFmacContext;

  if(gap_debug)
  {
    printf("p_delta_drawable: START *val drawable_id:%d (from:%d  to:%d)\n"
      ,(int)*val
      ,(int)val_from
      ,(int)val_to
      );
  }
  
  sizeFmacContext = gimp_get_data_size(GAP_FMAC_CONTEXT_KEYWORD);
  
  if(sizeFmacContext == sizeof(GapFmacContext))
  {
    GapFmacContext *fmacContext;
    fmacContext = g_malloc0(sizeFmacContext);
    if(fmacContext)
    {
      gimp_get_data(GAP_FMAC_CONTEXT_KEYWORD, fmacContext);

      if(fmacContext->enabled == TRUE)
      {
        gboolean success;

        gap_fmct_load_GapFmacContext(fmacContext);

        /* check for record conditions */
        if(fmacContext->recording_mode)
        {
          gint32 drawable_id;

          drawable_id = *val;

          /* calculate and assign the mapped persistent darawable_id */
          *val = p_capture_image_name_and_assign_pesistent_id(fmacContext, drawable_id);

          gap_fmct_free_GapFmacRefList(fmacContext);
          g_free(fmacContext);
          return;
        }
        success = p_iteration_by_pesistent_id(fmacContext, val, val_from, val_to, total_steps, current_step);
        if (success)
        {
          gap_fmct_free_GapFmacRefList(fmacContext);
          g_free(fmacContext);
          return;
        }
      }
      gap_fmct_free_GapFmacRefList(fmacContext);
      g_free(fmacContext);
    }

  }
   
  /* simple iteration of layers if in the same image (without context) */
  p_delta_drawable_simple(val, val_from, val_to, total_steps, current_step);
  
}  /* end p_delta_drawable */


/* ---------------------------
 * p_delta_drawable_simple
 * ---------------------------
 * simple iteration for drawable id (in case val_from and val_to both refere to
 * the same image (that is already opened in the current gimp session)
 */
static void 
p_delta_drawable_simple(gint32 *val, gint32 val_from, gint32 val_to, gint32 total_steps, gdouble current_step)
{
  gint    l_nlayers;
  gint32 *l_layers_list;
  gint32  l_tmp_image_id;
  gint    l_idx, l_idx_from, l_idx_to;

  if(gap_debug)
  {
    printf("p_delta_drawable_simple: START *val drawable_id:%d (from:%d  to:%d)\n"
      ,(int)*val
      ,(int)val_from
      ,(int)val_to
      );
  }
  if((val_from < 0) || (val_to < 0))
  {
    return;
  }
  if(gimp_drawable_is_valid(val_from) != TRUE)
  {
    return;
  }
  if(gimp_drawable_is_valid(val_to) != TRUE)
  {
    return;
  }
  l_tmp_image_id = gimp_item_get_image(val_from);

  /* check if from and to values are both valid drawables within the same image */
  if ((l_tmp_image_id > 0)
  &&  (l_tmp_image_id = gimp_item_get_image(val_to)))
  {
     l_idx_from = -1;
     l_idx_to   = -1;

     /* check the layerstack index of from and to drawable */
     l_layers_list = gimp_image_get_layers(l_tmp_image_id, &l_nlayers);
     for (l_idx = l_nlayers -1; l_idx >= 0; l_idx--)
     {
        if( l_layers_list[l_idx] == val_from ) l_idx_from = l_idx;
        if( l_layers_list[l_idx] == val_to )   l_idx_to   = l_idx;

        if((l_idx_from != -1) && (l_idx_to != -1))
        {
          /* OK found both index values, iterate the index (proceed to next layer) */
          p_delta_gint(&l_idx, l_idx_from, l_idx_to, total_steps, current_step);
          *val = l_layers_list[l_idx];
          break;
        }
     }
     g_free (l_layers_list);
  }
}  /* end p_delta_drawable_simple */


/* --------------------------------------------
 * p_capture_image_name_and_assign_pesistent_id
 * --------------------------------------------
 * Capture the identifying information about the image, anim_frame or videofile name and layerstack index
 *   (or frameposition) of the specified drawable_id.
 * Note: videofilename and framenumber can be fetched from layer parasite data. 
 *         (plus additional information prefered_decoder and seltrack)
 *         parasite data shall be set when the player creates a snapshot of a video clip.
 *         (click in the preview)
 *         (Player option: enable videoname parasites.)
 * assign a persistent id that is unique within a persistent_id_lookup_file.
 * filtermacro context.
 * returns the assigned persistent_drawable_id.
 *         (In case no assigned persisten_drawable_id could be created
 *          return the original drawable_id).
 */
static gint32
p_capture_image_name_and_assign_pesistent_id(GapFmacContext *fmacContext, gint32 drawable_id)
{
  gint32 persistent_drawable_id;
  gint32 image_id;
  GapDrawableVideoRef *dvref;
  GapLibAinfoType ainfo_type;
  gint32 frame_nr;
  gint32 stackposition;
  gint32 track;
  char   *filename;
  
  
  if(gap_debug)
  {
    printf("p_capture_image_name_and_assign_pesistent_id: START_REC orignal_drawable_id:%d\n"
      ,drawable_id
      );
  }

  if(gimp_drawable_is_valid(drawable_id) != TRUE)
  {
    /* drawable is no longer valid and can not be mapped.
     * This may happen if the layer was removed or the refered image was closed
     * in the time since the plugin has stored the last values buffer
     * and the time when filtermacro starts recording filtercalls.
     */
    return(drawable_id);
  }
  
  persistent_drawable_id = drawable_id;
  filename = NULL;
  
  dvref = gap_dvref_get_drawable_video_reference_via_parasite(drawable_id);
  if (dvref != NULL)
  {
    if(gap_debug)
    {
      printf("p_capture_image_name_and_assign_pesistent_id: VIDEO ref found for orignal_drawable_id:%d\n"
        ,drawable_id
        );
    }
    ainfo_type = GAP_AINFO_MOVIE;
    frame_nr = dvref->para.framenr;    /* video frame number */
    stackposition = -1;   /* stackposition not relevant for video */
    track = dvref->para.seltrack;
    filename = g_strdup(dvref->videofile);

    gap_dvref_free(&dvref);
  }
  else
  {
    GapAnimInfo *l_ainfo_ptr;
    
    image_id = gimp_item_get_image(drawable_id);
    filename = gimp_image_get_filename(image_id);
    ainfo_type = GAP_AINFO_ANIMIMAGE;
    stackposition = gap_layer_get_stackposition(image_id, drawable_id);
    track = 1;
    frame_nr = 1;

    // here we could check gimp_image_is_dirty(image_id)
    // to check for unsaved changes of the image.
    // but the recording of a filtermacro can happen much later than
    // the initial recording of the plugin's last values buffer.
  
    l_ainfo_ptr = gap_lib_alloc_ainfo(image_id, GIMP_RUN_NONINTERACTIVE);
    if(l_ainfo_ptr != NULL)
    {
      /* refere to GAP_AINFO_FRAMES in case the drawable is part of a gimp-gap typical frame image
       * with a name that ends up in numberpart.extension (something like frame_000006.xcf)
       * and there is one additional frame in sequence (previous or next number)
       */
      if (l_ainfo_ptr->ainfo_type == GAP_AINFO_FRAMES)
      {
        gboolean isAnotherFrameExistent;
	long     has_digits;
	
	isAnotherFrameExistent = gap_lib_exists_frame_nr(l_ainfo_ptr, l_ainfo_ptr->curr_frame_nr +1, &has_digits);
	if (!isAnotherFrameExistent)
	{
	  isAnotherFrameExistent = gap_lib_exists_frame_nr(l_ainfo_ptr, l_ainfo_ptr->curr_frame_nr -1, &has_digits);
	}
	
	if((isAnotherFrameExistent) && (has_digits > 1))
	{
          ainfo_type = GAP_AINFO_FRAMES;
	}

      }
      frame_nr = l_ainfo_ptr->frame_nr;
      track = -1;   /* video track (not relevant for frames and single images) */

      gap_lib_free_ainfo(&l_ainfo_ptr);
    }
  }

  if (filename != NULL)
  {
    persistent_drawable_id = gap_fmct_add_GapFmacRefEntry(ainfo_type
                                 , frame_nr
                                 , stackposition
                                 , track
                                 , drawable_id
                                 , filename
                                 , FALSE           /* force_id NO (e.g generate unique id) */
                                 , fmacContext
                                 );
    g_free(filename);
  }


  if(gap_debug)
  {
    printf("p_capture_image_name_and_assign_pesistent_id: orignal_drawable_id:%d mapped_drawable_id:%d\n"
      ,drawable_id
      ,persistent_drawable_id
      );
  }

  return (persistent_drawable_id);
}  /* end p_capture_image_name_and_assign_pesistent_id */


/* -------------------------------------
 * p_iteration_by_pesistent_id
 * -------------------------------------
 * fetch frame image by persistent id,
 * assign drawable and calculate iteration between
 *   start_drawable_id
 *   end_drawable_id
 * o) check if both start_drawable_id and end_drawable_id 
 *    are found in the persistent_id_lookup
 *
 *    o) if both refere to the same image filename and ainfo_type == GAP_AINFO_ANIMIMAGE
 *       then open this image (if not already opened)
 *       and calculate the drawable id according to current step
 *       by iterating layerstack numbers.
 *    o) if both refere to images, that are anim frames
 *       with the same basename and extension
 *       (for example:
 *         anim_000001.xcf 
 *         anim_000009.xcf
 *       )
 *       then iterate by framenumber (in the example this will be between 1 and 9)
 *
 *    o) filename references to videofiles are supported if ainfo_type == GAP_AINFO_MOVIE
 *
 * Note that each fetch of a persistent darawable (the fetched_layer_id) creates
 * a temporary image in the frame fetcher (assotiated with fmacContext->ffetch_user_id)
 * In this iterator we can not delete those temp. image(s) because they are required for
 * processing in the corresponding filter plug-in.
 * Therefore the master filtermacro processing has to do this clean up step after the
 * filtercall has finished.
 */
static gboolean
p_iteration_by_pesistent_id(GapFmacContext *fmacContext
  ,gint32 *val, gint32 val_from, gint32 val_to, gint32 total_steps, gdouble current_step)
{
  GapFmacRefEntry *fmref_entry_from;

  if(gap_debug)
  {
    printf("p_iteration_by_pesistent_id: START *val drawable_id:%d (from:%d  to:%d)\n"
      ,(int)*val
      ,(int)val_from
      ,(int)val_to
      );
  }
  
  fmref_entry_from = gap_fmct_get_GapFmacRefEntry_by_persitent_id(fmacContext, val_from);
  if (fmref_entry_from)
  {
    GapFmacRefEntry *fmref_entry_to;
    
    fmref_entry_to = gap_fmct_get_GapFmacRefEntry_by_persitent_id(fmacContext, val_to);
    if (fmref_entry_to)
    {
      if ((strcmp(fmref_entry_from->filename, fmref_entry_to->filename) == 0)
      && (fmref_entry_from->track == fmref_entry_to->track)
      && (fmref_entry_from->ainfo_type == fmref_entry_to->ainfo_type))
      {
        gint32 fetched_layer_id;

        fetched_layer_id = -1;

        // TODO: what to do if mtime has changed ? (maybe print warning if this occurs the 1st time ?)
        
        if (fmref_entry_from->ainfo_type == GAP_AINFO_ANIMIMAGE)
        {
          /* iterate stackposition */
          gint32 stackposition;
          
          stackposition = fmref_entry_from->stackposition;
          p_delta_gint32(&stackposition, fmref_entry_from->stackposition, fmref_entry_to->stackposition
                        ,total_steps, current_step);
                        
          fetched_layer_id = gap_frame_fetch_dup_image(fmacContext->ffetch_user_id
                                    ,fmref_entry_from->filename   /* full filename of the image */
                                    ,stackposition                /* pick layer by stackposition */
                                    ,TRUE                         /* enable caching */
                                    );
        }
        else
        {
          /* iterate frame_nr */
          gint32 frame_nr;
          
          frame_nr = fmref_entry_from->frame_nr;
          p_delta_gint32(&frame_nr, fmref_entry_from->frame_nr, fmref_entry_to->frame_nr
                        ,total_steps, current_step);

          if (fmref_entry_from->ainfo_type == GAP_AINFO_MOVIE)
          {
            fetched_layer_id = gap_frame_fetch_dup_video(fmacContext->ffetch_user_id
                                    ,fmref_entry_from->filename   /* full filename of a video */
                                    ,frame_nr                     /* frame within the video (starting at 1) */
                                    ,fmref_entry_from->track      /* videotrack */
                                    , NULL                        /* char *prefered_decoder*/
                                    );
          }
          else
          {
            fetched_layer_id = gap_frame_fetch_dup_image(fmacContext->ffetch_user_id
                                    ,fmref_entry_from->filename   /* full filename of the image */
                                    ,-1                           /* 0 pick layer on top of stack, -1 merge visible layers */
                                    ,TRUE                         /* enable caching */
                                    );
          }

        }

        *val = fetched_layer_id;
        if(gap_debug)
        {
          printf("p_iteration_by_pesistent_id: SUCCESS drawable_id:%d (from:%d  to:%d)\n"
            ,(int)*val
            ,(int)val_from
            ,(int)val_to
            );
        }
        return (TRUE);
      }
    }
  }

  if(gap_debug)
  {
    printf("p_iteration_by_pesistent_id: FAILED *val drawable_id:%d (from:%d  to:%d)\n"
      ,(int)*val
      ,(int)val_from
      ,(int)val_to
      );
  }
  return (FALSE);
}  /* end p_iteration_by_pesistent_id */


static void
p_delta_gintdrawable(gint *val, gint val_from, gint val_to, gint32 total_steps, gdouble current_step)
{
  gint32 l_val, l_from, l_to;
  l_val = *val;
  l_from = val_from;
  l_to   = val_to;
  p_delta_drawable(&l_val, l_from, l_to, total_steps, current_step);
  *val = l_val;
}


/* -----------------------------
 * stuff for the common iterator
 * -----------------------------
 */
/*
static void p_delta_long(long *val, long val_from, long val_to, gint32 total_steps, gdouble current_step)
static void p_delta_short(short *val, short val_from, short val_to, gint32 total_steps, gdouble current_step)
static void p_delta_int(int *val, int val_from, int val_to, gint32 total_steps, gdouble current_step)
static void p_delta_gint(gint *val, gint val_from, gint val_to, gint32 total_steps, gdouble current_step)
static void p_delta_gint32(gint32 *val, gint32 val_from, gint32 val_to, gint32 total_steps, gdouble current_step)
static void p_delta_char(char *val, char val_from, char val_to, gint32 total_steps, gdouble current_step)
static void p_delta_guchar(guchar *val, char val_from, char val_to, gint32 total_steps, gdouble current_step)
static void p_delta_gdouble(double *val, double val_from, double val_to, gint32 total_steps, gdouble current_step)
static void p_delta_gfloat(gfloat *val, gfloat val_from, gfloat val_to, gint32 total_steps, gdouble current_step)
static void p_delta_float(float *val, float val_from, float val_to, gint32 total_steps, gdouble current_step)
static void p_delta_drawable(gint32 *val, gint32 val_from, gint32 val_to, gint32 total_steps, gdouble current_step)
static void p_delta_gintdrawable(gint *val, gint val_from, gint val_to, gint32 total_steps, gdouble current_step)

*/

/* wrapper calls with pointers to value transform
 * (for those delta procedures that do not
    already use pointers for val_from and val-to parameters)
 */
static void gp_delta_long (long *val, long *val_from, long *val_to, gint32 total_steps, gdouble current_step)
{ p_delta_long(val, *val_from, *val_to, total_steps, current_step); }

static void gp_delta_short (short *val, short *val_from, short *val_to, gint32 total_steps, gdouble current_step)
{ p_delta_short(val, *val_from, *val_to, total_steps, current_step); }

static void gp_delta_int (int *val, int *val_from, int *val_to, gint32 total_steps, gdouble current_step)
{ p_delta_int(val, *val_from, *val_to, total_steps, current_step); }

static void gp_delta_gint (gint *val, gint *val_from, gint *val_to, gint32 total_steps, gdouble current_step)
{ p_delta_gint(val, *val_from, *val_to, total_steps, current_step); }

static void gp_delta_guint (guint *val, guint *val_from, guint *val_to, gint32 total_steps, gdouble current_step)
{ p_delta_guint(val, *val_from, *val_to, total_steps, current_step); }

static void gp_delta_gint32 (gint32 *val, gint32 *val_from, gint32 *val_to, gint32 total_steps, gdouble current_step)
{ p_delta_gint32(val, *val_from, *val_to, total_steps, current_step); }

static void gp_delta_guint32 (guint32 *val, guint32 *val_from, guint32 *val_to, gint32 total_steps, gdouble current_step)
{ p_delta_guint32(val, *val_from, *val_to, total_steps, current_step); }

static void gp_delta_char (char *val, char *val_from, char *val_to, gint32 total_steps, gdouble current_step)
{ p_delta_char(val, *val_from, *val_to, total_steps, current_step); }

static void gp_delta_guchar (guchar *val, char *val_from, char *val_to, gint32 total_steps, gdouble current_step)
{ p_delta_guchar(val, *val_from, *val_to, total_steps, current_step); }

static void gp_delta_gdouble (double *val, double *val_from, double *val_to, gint32 total_steps, gdouble current_step)
{ p_delta_gdouble(val, *val_from, *val_to, total_steps, current_step); }

static void gp_delta_gfloat (gfloat *val, gfloat *val_from, gfloat *val_to, gint32 total_steps, gdouble current_step)
{ p_delta_gfloat(val, *val_from, *val_to, total_steps, current_step); }

static void gp_delta_float (float *val, float *val_from, float *val_to, gint32 total_steps, gdouble current_step)
{ p_delta_float(val, *val_from, *val_to, total_steps, current_step); }

static void gp_delta_drawable (gint32 *val, gint32 *val_from, gint32 *val_to, gint32 total_steps, gdouble current_step)
{ p_delta_drawable(val, *val_from, *val_to, total_steps, current_step); }

static void gp_delta_gintdrawable (gint *val, gint *val_from, gint *val_to, gint32 total_steps, gdouble current_step)
{ p_delta_gintdrawable(val, *val_from, *val_to, total_steps, current_step); }


/* wrapper calls of type specific procedure */

void gap_delta_none (void *val, void *val_from, void *val_to, gint32 total_steps, gdouble current_step)
{  return; /* this is just a dummy delta procedure for non-taerable members in a LAST_VALUES buffer */ }

void gap_delta_long (void *val, void *val_from, void *val_to, gint32 total_steps, gdouble current_step)
{  gp_delta_long (val, val_from, val_to, total_steps, current_step); }

void gap_delta_short (void *val, void *val_from, void *val_to, gint32 total_steps, gdouble current_step)
{  gp_delta_short (val, val_from, val_to, total_steps, current_step); }

void gap_delta_int (void *val, void *val_from, void *val_to, gint32 total_steps, gdouble current_step)
{  gp_delta_int (val, val_from, val_to, total_steps, current_step); }

void gap_delta_gint (void *val, void *val_from, void *val_to, gint32 total_steps, gdouble current_step)
{  gp_delta_gint (val, val_from, val_to, total_steps, current_step); }

void gap_delta_guint (void *val, void *val_from, void *val_to, gint32 total_steps, gdouble current_step)
{  gp_delta_guint (val, val_from, val_to, total_steps, current_step); }

void gap_delta_gint32 (void *val, void *val_from, void *val_to, gint32 total_steps, gdouble current_step)
{  gp_delta_gint32 (val, val_from, val_to, total_steps, current_step); }

void gap_delta_guint32 (void *val, void *val_from, void *val_to, gint32 total_steps, gdouble current_step)
{  gp_delta_guint32 (val, val_from, val_to, total_steps, current_step); }

void gap_delta_char (void *val, void *val_from, void *val_to, gint32 total_steps, gdouble current_step)
{  gp_delta_char (val, val_from, val_to, total_steps, current_step); }

void gap_delta_guchar (void *val, void *val_from, void *val_to, gint32 total_steps, gdouble current_step)
{  gp_delta_guchar (val, val_from, val_to, total_steps, current_step); }

void gap_delta_gdouble (void *val, void *val_from, void *val_to, gint32 total_steps, gdouble current_step)
{  gp_delta_gdouble (val, val_from, val_to, total_steps, current_step); }

void gap_delta_gfloat (void *val, void *val_from, void *val_to, gint32 total_steps, gdouble current_step)
{  gp_delta_gfloat (val, val_from, val_to, total_steps, current_step); }

void gap_delta_float (void *val, void *val_from, void *val_to, gint32 total_steps, gdouble current_step)
{  gp_delta_float (val, val_from, val_to, total_steps, current_step); }

void gap_delta_drawable (void *val, void *val_from, void *val_to, gint32 total_steps, gdouble current_step)
{  gp_delta_drawable (val, val_from, val_to, total_steps, current_step); }

void gap_delta_gintdrawable (void *val, void *val_from, void *val_to, gint32 total_steps, gdouble current_step)
{  gp_delta_gintdrawable (val, val_from, val_to, total_steps, current_step); }




void
p_init_iter_jump_table(void)
{
  static gboolean  jmp_table_initialized = FALSE;

  if(jmp_table_initialized != TRUE)
  {
    /* fuction pointers for typespecific delta procedures */
    jmp_table[GIMP_LASTVAL_NONE].func_ptr = gap_delta_none;
    jmp_table[GIMP_LASTVAL_ARRAY].func_ptr = gap_delta_none;
    jmp_table[GIMP_LASTVAL_STRUCT_BEGIN].func_ptr = gap_delta_none;
    jmp_table[GIMP_LASTVAL_STRUCT_END].func_ptr = gap_delta_none;

    jmp_table[GIMP_LASTVAL_LONG].func_ptr = gap_delta_long;
    jmp_table[GIMP_LASTVAL_SHORT].func_ptr = gap_delta_short;
    jmp_table[GIMP_LASTVAL_INT].func_ptr = gap_delta_int;
    jmp_table[GIMP_LASTVAL_GINT].func_ptr = gap_delta_gint;
    jmp_table[GIMP_LASTVAL_GINT32].func_ptr = gap_delta_gint32;
    jmp_table[GIMP_LASTVAL_CHAR].func_ptr = gap_delta_char;
    jmp_table[GIMP_LASTVAL_GCHAR].func_ptr = gap_delta_char;           /* gchar uses same as  char */
    jmp_table[GIMP_LASTVAL_GUCHAR].func_ptr = gap_delta_guchar;
    jmp_table[GIMP_LASTVAL_GDOUBLE].func_ptr = gap_delta_gdouble;
    jmp_table[GIMP_LASTVAL_GFLOAT].func_ptr = gap_delta_gfloat;
    jmp_table[GIMP_LASTVAL_DOUBLE].func_ptr = gap_delta_gdouble;     /* double same as gdouble */
    jmp_table[GIMP_LASTVAL_FLOAT].func_ptr = gap_delta_float;
    jmp_table[GIMP_LASTVAL_DRAWABLE].func_ptr = gap_delta_drawable;
    jmp_table[GIMP_LASTVAL_GINTDRAWABLE].func_ptr = gap_delta_gintdrawable;
    jmp_table[GIMP_LASTVAL_GBOOLEAN].func_ptr = gap_delta_none;
    jmp_table[GIMP_LASTVAL_ENUM].func_ptr = gap_delta_none;
    jmp_table[GIMP_LASTVAL_GUINT].func_ptr = gap_delta_guint;
    jmp_table[GIMP_LASTVAL_GUINT32].func_ptr = gap_delta_guint32;

     /* size of one element for the supported basetypes and predefined structs */
    jmp_table[GIMP_LASTVAL_NONE].item_size = 0;
    jmp_table[GIMP_LASTVAL_ARRAY].item_size = 0;
    jmp_table[GIMP_LASTVAL_STRUCT_BEGIN].item_size = 0;
    jmp_table[GIMP_LASTVAL_STRUCT_END].item_size = 0;

    jmp_table[GIMP_LASTVAL_LONG].item_size = sizeof(long);
    jmp_table[GIMP_LASTVAL_SHORT].item_size = sizeof(short);
    jmp_table[GIMP_LASTVAL_INT].item_size = sizeof(int);
    jmp_table[GIMP_LASTVAL_GINT].item_size = sizeof(gint);
    jmp_table[GIMP_LASTVAL_CHAR].item_size = sizeof(char);
    jmp_table[GIMP_LASTVAL_GCHAR].item_size = sizeof(gchar);
    jmp_table[GIMP_LASTVAL_GUCHAR].item_size = sizeof(guchar);
    jmp_table[GIMP_LASTVAL_GDOUBLE].item_size = sizeof(gdouble);
    jmp_table[GIMP_LASTVAL_GFLOAT].item_size = sizeof(gfloat);
    jmp_table[GIMP_LASTVAL_FLOAT].item_size = sizeof(float);
    jmp_table[GIMP_LASTVAL_DOUBLE].item_size = sizeof(double);
    jmp_table[GIMP_LASTVAL_DRAWABLE].item_size = sizeof(gint32);
    jmp_table[GIMP_LASTVAL_GINTDRAWABLE].item_size = sizeof(gint);
    jmp_table[GIMP_LASTVAL_GBOOLEAN].item_size = sizeof(gboolean);
    jmp_table[GIMP_LASTVAL_ENUM].item_size = sizeof(gint);
    jmp_table[GIMP_LASTVAL_GUINT].item_size = sizeof(guint);
    jmp_table[GIMP_LASTVAL_GUINT32].item_size = sizeof(guint32);

    jmp_table_initialized = TRUE;
  }
}



static gint32
p_stack_offsetsum(IterStackType *iter_stack)
{
  gint32 l_arr_fact;
  gint32 l_offset_sum;
  gint32 l_idx;

  l_arr_fact = 0;
  l_offset_sum = 0;

  for(l_idx=0; l_idx <= iter_stack->stack_top; l_idx++)
  {
    if(iter_stack->stack_arr[l_idx].lastval_type == GIMP_LASTVAL_ARRAY)
    {
       if(l_arr_fact == 0)
       {
         l_arr_fact = iter_stack->stack_arr[l_idx].arr_count;
       }
       else
       {
         l_arr_fact = l_arr_fact * iter_stack->stack_arr[l_idx].arr_count;
       }
    }
    else
    {
       /* GIMP_LASTVAL_STRUCT_BEGIN */
       l_offset_sum += (l_arr_fact * iter_stack->stack_arr[l_idx].elem_size);
       l_arr_fact = 0;
    }
  }
  return(l_offset_sum);
}


static void
p_debug_stackprint(IterStackType *iter_stack)
{
  gint l_idx;

  printf("  p_debug_stackprint stack_top: %d  max_elem: %d\n", (int)iter_stack->stack_top, (int)iter_stack->stack_max_elem);
  for(l_idx=0; l_idx <= iter_stack->stack_top; l_idx++)
  {
    printf("  stack[%02d] lastval_type: %d elem_size: %d arr_count: %d idx: %d\n"
          ,(int)l_idx
          ,(int)iter_stack->stack_arr[l_idx].lastval_type
          ,(int)iter_stack->stack_arr[l_idx].elem_size
          ,(int)iter_stack->stack_arr[l_idx].arr_count
          ,(int)iter_stack->stack_arr[l_idx].idx
          );
  }
}

static void
p_push_iter(IterStackType *iter_stack, IterStackItemType *stack_item)
{
  iter_stack->stack_top++;
  if(iter_stack->stack_top < iter_stack->stack_max_elem)
  {
    iter_stack->stack_arr[iter_stack->stack_top].lastval_type = stack_item->lastval_type;
    iter_stack->stack_arr[iter_stack->stack_top].elem_size = stack_item->elem_size;
    iter_stack->stack_arr[iter_stack->stack_top].arr_count = stack_item->arr_count;
    iter_stack->stack_arr[iter_stack->stack_top].idx       = stack_item->idx;
  }
  else
  {
    printf("p_push_iter: STACK overflow\n");
  }
  if(1==0 /*gap_debug*/ )
  {
     printf("p_push_iter START\n");
     p_debug_stackprint(iter_stack);
  }
}


static void
p_pop_iter(IterStackType *iter_stack, IterStackItemType *stack_item)
{
  if(1==0 /*gap_debug*/ )
  {
     printf("p_pop_iter START\n");
     p_debug_stackprint(iter_stack);
  }
  if(iter_stack->stack_top >= 0)
  {
    stack_item->lastval_type = iter_stack->stack_arr[iter_stack->stack_top].lastval_type;
    stack_item->elem_size = iter_stack->stack_arr[iter_stack->stack_top].elem_size;
    stack_item->arr_count = iter_stack->stack_arr[iter_stack->stack_top].arr_count;
    stack_item->idx       = iter_stack->stack_arr[iter_stack->stack_top].idx;
    iter_stack->stack_top--;
  }
  else
  {
    printf("p_pop_iter: STACK underrun\n");
  }
}


void
p_debug_print_iter_desc (GimpLastvalDescType *lastval_desc_arr, gint32 arg_cnt)
{
  gint32 l_idx;

  printf ("p_debug_print_iter_desc START\n");
  for(l_idx = 0; l_idx < arg_cnt; l_idx++)
  {
     printf("[%03d] lastval_type: %d, offset: %d, elem_size: %d\n"
           , (int)l_idx
           , (int)lastval_desc_arr[l_idx].lastval_type
           , (int)lastval_desc_arr[l_idx].offset
           , (int)lastval_desc_arr[l_idx].elem_size
           );
  }
  printf ("p_debug_print_iter_desc END\n\n");
}


/* ----------------------------------------------------------------------
 * run common iterator
 *  PDB: registered as "plug_in_gap_COMMON_ITERATOR"
 * ----------------------------------------------------------------------
 */

gint
gap_common_iterator(const char *c_keyname, GimpRunMode run_mode, gint32 total_steps, gdouble current_step, gint32 len_struct)
{
   gchar *keyname;
   gchar *key_description;
   gchar *key_from;
   gchar *key_to;
   int buf_size;
   int desc_size;

   keyname = g_strdup(c_keyname);
   if(gap_debug) printf("\ngap_common_iterator START: keyname: %s  total_steps: %d,  curent_step: %f\n", keyname, (int)total_steps, (float)current_step );

   key_description = gimp_lastval_desc_keyname (keyname);
   key_from = g_strdup_printf("%s%s", keyname, GAP_ITER_FROM_SUFFIX);
   key_to   = g_strdup_printf("%s%s", keyname, GAP_ITER_TO_SUFFIX);

   buf_size = gimp_get_data_size(keyname);
   desc_size = gimp_get_data_size(key_description);
   if (buf_size > 0 && desc_size > 0)
   {
      GimpLastvalDescType *lastval_desc_arr;
      gint         arg_cnt;
      gint         l_idx;
      gint         l_idx_next;
      void  *buffer;
      void  *buffer_from;
      void  *buffer_to;

      IterStackType      *stack_iter;
      IterStackItemType  l_stack_item;
      IterStackItemType  l_stack_2_item;

      lastval_desc_arr = g_malloc(desc_size);
      arg_cnt = desc_size / sizeof(GimpLastvalDescType);

      buffer      = g_malloc(buf_size);
      buffer_from = g_malloc(buf_size);
      buffer_to   = g_malloc(buf_size);

      gimp_get_data(key_description, lastval_desc_arr);
      gimp_get_data(keyname,  buffer);
      gimp_get_data(key_from, buffer_from);
      gimp_get_data(key_to,   buffer_to);

      if(gap_debug)
      {
         p_debug_print_iter_desc(lastval_desc_arr, arg_cnt);
      }


      if ((lastval_desc_arr[0].elem_size  != buf_size)
      ||  (lastval_desc_arr[0].elem_size  != len_struct))
      {
         printf("ERROR: %s stored Data missmatch in size %d != %d\n"
                       , keyname
                       , (int)buf_size
                       , (int)lastval_desc_arr[0].elem_size );
         return -1 ; /* NOT OK */
      }

      p_init_iter_jump_table();

      /* init IterStack */
      stack_iter = g_new(IterStackType, 1);
      stack_iter->stack_arr = g_new(IterStackItemType, arg_cnt);
      stack_iter->stack_top = -1;  /* stack is empty */
      stack_iter->stack_max_elem = arg_cnt;  /* stack is empty */

      l_idx=0;
      while(l_idx < arg_cnt)
      {
         void  *buf_ptr;
         void  *buf_ptr_from;
         void  *buf_ptr_to;
         gint   l_iter_idx;
         gint32 l_iter_flag;
         gint32 l_offset;
         gint32 l_ntimes_arraysize;

         if(lastval_desc_arr[l_idx].lastval_type == GIMP_LASTVAL_END)
         {
           break;
         }

         l_iter_idx = (int)lastval_desc_arr[l_idx].lastval_type;
         l_iter_flag = lastval_desc_arr[l_idx].iter_flag;
         l_idx_next = l_idx +1;

         l_stack_item.lastval_type = lastval_desc_arr[l_idx].lastval_type;
         l_stack_item.iter_flag = lastval_desc_arr[l_idx].iter_flag;
         l_stack_item.elem_size = lastval_desc_arr[l_idx].elem_size;
         l_stack_item.arr_count = 0;
         l_stack_item.idx = l_idx_next;

         switch(lastval_desc_arr[l_idx].lastval_type)
         {
           case GIMP_LASTVAL_ARRAY:
             p_push_iter(stack_iter, &l_stack_item);
             break;
           case GIMP_LASTVAL_STRUCT_BEGIN:
             p_push_iter(stack_iter, &l_stack_item);
             break;
           case GIMP_LASTVAL_STRUCT_END:
             l_stack_2_item.lastval_type = GIMP_LASTVAL_NONE;
             if(stack_iter->stack_top >= 0)
             {
               /* 1.st pop should always retrieve the STUCTURE BEGIN */
               p_pop_iter(stack_iter, &l_stack_2_item);
             }
             if(l_stack_2_item.lastval_type != GIMP_LASTVAL_STRUCT_BEGIN)
             {
               printf("stack out of balance, STRUCTURE END without begin\n");
               return -1; /* ERROR */
             }
             if (stack_iter->stack_top >= 0)
             {
                /* 2.nd pop */
                p_pop_iter(stack_iter, &l_stack_item);
                if(l_stack_item.lastval_type == GIMP_LASTVAL_ARRAY)
                {
                  l_stack_item.elem_size--;
                  if(l_stack_item.elem_size > 0)
                  {
                    p_push_iter(stack_iter, &l_stack_item);
                    p_push_iter(stack_iter, &l_stack_2_item);
                    /*  go back to stored index (1st elem of the structure) */
                    l_idx_next = l_stack_2_item.idx;
                  }
                }
                else
                {
                  p_push_iter(stack_iter, &l_stack_item);
                }
             }
             break;
           default:
             l_ntimes_arraysize = 1;
             while(1==1)
             {
               if (stack_iter->stack_top >= 0)
               {
                  p_pop_iter(stack_iter, &l_stack_item);
                  if(l_stack_item.lastval_type == GIMP_LASTVAL_ARRAY)
                  {
                    l_ntimes_arraysize = l_ntimes_arraysize * l_stack_item.elem_size;
                  }
                  else
                  {
                    p_push_iter(stack_iter, &l_stack_item);
                    break;
                  }
               }
             }

             l_offset = p_stack_offsetsum(stack_iter);            /* offest for current array position */
             l_offset += lastval_desc_arr[l_idx].offset;   /* local offest */

             buf_ptr      = buffer      + l_offset;
             buf_ptr_from = buffer_from + l_offset;
             buf_ptr_to   = buffer_to   + l_offset;


             if(gap_debug) printf("Using JumpTable jmp index:%d iter_flag:%d\n", (int)l_iter_idx, (int)l_iter_flag);

             if (l_iter_idx >= 0
             &&  l_iter_idx < (gint)GIMP_LASTVAL_END
             &&  (l_iter_flag & GIMP_ITER_TRUE) == GIMP_ITER_TRUE  )
             {
               gint32 l_cnt;

               for(l_cnt=0; l_cnt < l_ntimes_arraysize; l_cnt++)
               {
                 /* call type specific iterator delta procedure using jmp_table indexed by lastval_type */
                 jmp_table[l_iter_idx].func_ptr(buf_ptr, buf_ptr_from, buf_ptr_to, total_steps, current_step);
                 buf_ptr      += jmp_table[l_iter_idx].item_size;
                 buf_ptr_from += jmp_table[l_iter_idx].item_size;
                 buf_ptr_to   += jmp_table[l_iter_idx].item_size;
               }
             }
             break;
         }

         l_idx = l_idx_next;
      }

      gimp_set_data(keyname, buffer, buf_size);
      g_free(stack_iter->stack_arr);
      g_free(stack_iter);
   }
   else
   {
         printf("ERROR: %s No stored Data/Description found. Datasize: %d, Descriptionsize: %d\n",
                       keyname, (int)buf_size, (int)desc_size );
         return -1 ; /* NOT OK */
   }
   g_free(keyname);
   return 0; /* OK */
}  /* end gap_common_iterator */










/* ----------------------------------------------------------------------
 * iterator UTILITIES for GimpRGB, GimpVector3, Material and Light Sewttings
 * ----------------------------------------------------------------------
 */

    typedef enum {
      POINT_LIGHT,
      DIRECTIONAL_LIGHT,
      SPOT_LIGHT,
      NO_LIGHT
    } t_LightType;

    typedef enum {
      IMAGE_BUMP,
      WAVES_BUMP
    } t_MapType;

    typedef struct
    {
      gdouble ambient_int;
      gdouble diffuse_int;
      gdouble diffuse_ref;
      gdouble specular_ref;
      gdouble highlight;
      GimpRGB  color;
    } t_MaterialSettings;

    typedef struct
    {
      t_LightType  type;
      GimpVector3  position;
      GimpVector3  direction;
      GimpRGB      color;
      gdouble      intensity;
    } t_LightSettings;




static void 
p_delta_GimpRGB(GimpRGB *val, GimpRGB *val_from, GimpRGB *val_to, gint32 total_steps, gdouble current_step)
{
    double     delta;

    if(total_steps < 1) return;

    delta = ((double)(val_to->r - val_from->r) / (double)total_steps) * ((double)total_steps - current_step);
    val->r = val_from->r + delta;

    delta = ((double)(val_to->g - val_from->g) / (double)total_steps) * ((double)total_steps - current_step);
    val->g = val_from->g + delta;

    delta = ((double)(val_to->b - val_from->b) / (double)total_steps) * ((double)total_steps - current_step);
    val->b = val_from->b + delta;

    delta = ((double)(val_to->a - val_from->a) / (double)total_steps) * ((double)total_steps - current_step);
    val->a = val_from->a + delta;
}

static void 
p_delta_GimpVector3(GimpVector3 *val, GimpVector3 *val_from, GimpVector3 *val_to, gint32 total_steps, gdouble current_step)
{
    double     delta;

    if(total_steps < 1) return;

    delta = ((double)(val_to->x - val_from->x) / (double)total_steps) * ((double)total_steps - current_step);
    val->x  = val_from->x + delta;

    delta = ((double)(val_to->y - val_from->y) / (double)total_steps) * ((double)total_steps - current_step);
    val->y  = val_from->y + delta;

    delta = ((double)(val_to->z - val_from->z) / (double)total_steps) * ((double)total_steps - current_step);
    val->z  = val_from->z + delta;
}

static void 
p_delta_MaterialSettings(t_MaterialSettings *val, t_MaterialSettings *val_from, t_MaterialSettings *val_to, gint32 total_steps, gdouble current_step)
{
    p_delta_gdouble(&val->ambient_int, val_from->ambient_int, val_to->ambient_int, total_steps, current_step);
    p_delta_gdouble(&val->diffuse_int, val_from->diffuse_int, val_to->diffuse_int, total_steps, current_step);
    p_delta_gdouble(&val->diffuse_ref, val_from->diffuse_ref, val_to->diffuse_ref, total_steps, current_step);
    p_delta_gdouble(&val->specular_ref, val_from->specular_ref, val_to->specular_ref, total_steps, current_step);
    p_delta_gdouble(&val->highlight, val_from->highlight, val_to->highlight, total_steps, current_step);
    p_delta_GimpRGB(&val->color, &val_from->color, &val_to->color, total_steps, current_step);

}

static void 
p_delta_LightSettings(t_LightSettings *val, t_LightSettings *val_from, t_LightSettings *val_to, gint32 total_steps, gdouble current_step)
{
    /* no delta is done for LightType */
    p_delta_GimpVector3(&val->position, &val_from->position, &val_to->position, total_steps, current_step);
    p_delta_GimpVector3(&val->direction, &val_from->direction, &val_to->direction, total_steps, current_step);
    p_delta_GimpRGB(&val->color, &val_from->color, &val_to->color, total_steps, current_step);
    p_delta_gdouble(&val->intensity, val_from->intensity, val_to->intensity, total_steps, current_step);
}


/* for Lighting */
    /* since gimp-2.2 MapObject and Lighting Types drifted apart
     * therefore we need 2 sets of Material and Light Setting Types
     */

    typedef struct
    {
      gdouble ambient_int;
      gdouble diffuse_int;
      gdouble diffuse_ref;
      gdouble specular_ref;
      gdouble highlight;
      gboolean metallic;
      GimpRGB  color;
    } t_LightingMaterialSettings;

    typedef struct
    {
      t_LightType  type;
      GimpVector3  position;
      GimpVector3  direction;
      GimpRGB      color;
      gdouble      intensity;
      gboolean     active;
    } t_LightingLightSettings;

static void 
p_delta_LightingMaterialSettings(t_LightingMaterialSettings *val, t_LightingMaterialSettings *val_from, t_LightingMaterialSettings *val_to, gint32 total_steps, gdouble current_step)
{
    p_delta_gdouble(&val->ambient_int, val_from->ambient_int, val_to->ambient_int, total_steps, current_step);
    p_delta_gdouble(&val->diffuse_int, val_from->diffuse_int, val_to->diffuse_int, total_steps, current_step);
    p_delta_gdouble(&val->diffuse_ref, val_from->diffuse_ref, val_to->diffuse_ref, total_steps, current_step);
    p_delta_gdouble(&val->specular_ref, val_from->specular_ref, val_to->specular_ref, total_steps, current_step);
    p_delta_gdouble(&val->highlight, val_from->highlight, val_to->highlight, total_steps, current_step);
    p_delta_GimpRGB(&val->color, &val_from->color, &val_to->color, total_steps, current_step);

}

static void 
p_delta_LightingLightSettings(t_LightingLightSettings *val, t_LightingLightSettings *val_from, t_LightingLightSettings *val_to, gint32 total_steps, gdouble current_step)
{
    /* no delta is done for LightType */
    p_delta_GimpVector3(&val->position, &val_from->position, &val_to->position, total_steps, current_step);
    p_delta_GimpVector3(&val->direction, &val_from->direction, &val_to->direction, total_steps, current_step);
    p_delta_GimpRGB(&val->color, &val_from->color, &val_to->color, total_steps, current_step);
    p_delta_gdouble(&val->intensity, val_from->intensity, val_to->intensity, total_steps, current_step);
}

/* for p_plug_in_cml_explorer_iter_ALT */
static void 
p_delta_CML_PARAM(t_CML_PARAM *val, t_CML_PARAM *val_from, t_CML_PARAM *val_to, gint32 total_steps, gdouble current_step)
{
    p_delta_gint(&val->function, val_from->function, val_to->function, total_steps, current_step);
    p_delta_gint(&val->composition, val_from->composition, val_to->composition, total_steps, current_step);
    p_delta_gint(&val->arrange, val_from->arrange, val_to->arrange, total_steps, current_step);
    p_delta_gint(&val->cyclic_range, val_from->cyclic_range, val_to->cyclic_range, total_steps, current_step);
    p_delta_gdouble(&val->mod_rate, val_from->mod_rate, val_to->mod_rate, total_steps, current_step);
    p_delta_gdouble(&val->env_sensitivity, val_from->env_sensitivity, val_to->env_sensitivity, total_steps, current_step);
    p_delta_gint(&val->diffusion_dist, val_from->diffusion_dist, val_to->diffusion_dist, total_steps, current_step);

    p_delta_gdouble(&val->ch_sensitivity, val_from->ch_sensitivity, val_to->ch_sensitivity, total_steps, current_step);
    p_delta_gint(&val->range_num, val_from->range_num, val_to->range_num, total_steps, current_step);
    p_delta_gdouble(&val->power, val_from->power, val_to->power, total_steps, current_step);
    p_delta_gdouble(&val->parameter_k, val_from->parameter_k, val_to->parameter_k, total_steps, current_step);
    p_delta_gdouble(&val->range_l, val_from->range_l, val_to->range_l, total_steps, current_step);
    p_delta_gdouble(&val->range_h, val_from->range_h, val_to->range_h, total_steps, current_step);
    p_delta_gdouble(&val->mutation_rate, val_from->mutation_rate, val_to->mutation_rate, total_steps, current_step);
    p_delta_gdouble(&val->mutation_dist, val_from->mutation_dist, val_to->mutation_dist, total_steps, current_step);
}


/* for gimpressionist */
  typedef struct gimpressionist_vector
  {
    double x, y;
    double dir;
    double dx, dy;
    double str;
    int    type;
  } t_gimpressionist_vector;

  typedef struct gimpressionist_smvector
  {
    double x, y;
    double siz;
    double str;
  } t_gimpressionist_smvector;

static 
void p_delta_gimpressionist_vector(t_gimpressionist_vector *val, t_gimpressionist_vector *val_from, t_gimpressionist_vector *val_to, gint32 total_steps, gdouble current_step)
{
    if(total_steps < 1) return;

    p_delta_gdouble(&val->x, val_from->x, val_to->x, total_steps, current_step);
    p_delta_gdouble(&val->y, val_from->y, val_to->y, total_steps, current_step);
    p_delta_gdouble(&val->dir, val_from->dir, val_to->dir, total_steps, current_step);
    p_delta_gdouble(&val->dx, val_from->dx, val_to->dx, total_steps, current_step);
    p_delta_gdouble(&val->dy, val_from->dy, val_to->dy, total_steps, current_step);
    p_delta_gdouble(&val->str, val_from->str, val_to->str, total_steps, current_step);

    p_delta_gint(&val->type, val_from->type, val_to->type, total_steps, current_step);
}


static 
void p_delta_gimpressionist_smvector(t_gimpressionist_smvector *val, t_gimpressionist_smvector *val_from, t_gimpressionist_smvector *val_to, gint32 total_steps, gdouble current_step)
{
    if(total_steps < 1) return;

    p_delta_gdouble(&val->x, val_from->x, val_to->x, total_steps, current_step);
    p_delta_gdouble(&val->y, val_from->y, val_to->y, total_steps, current_step);
    p_delta_gdouble(&val->siz, val_from->siz, val_to->siz, total_steps, current_step);
    p_delta_gdouble(&val->str, val_from->str, val_to->str, total_steps, current_step);
}


/* for channel_mixer */
  typedef struct
  {
    gdouble red_gain;
    gdouble green_gain;
    gdouble blue_gain;
  } t_channel_mixer_ch_type;


static 
void p_delta_channel_mixer_ch_type(t_channel_mixer_ch_type *val, t_channel_mixer_ch_type *val_from, t_channel_mixer_ch_type *val_to, gint32 total_steps, gdouble current_step)
{
    if(total_steps < 1) return;

    p_delta_gdouble(&val->red_gain, val_from->red_gain, val_to->red_gain, total_steps, current_step);
    p_delta_gdouble(&val->green_gain, val_from->green_gain, val_to->green_gain, total_steps, current_step);
    p_delta_gdouble(&val->blue_gain, val_from->blue_gain, val_to->blue_gain, total_steps, current_step);
}



/* ---------------------------------------- 2.nd Section
 * ----------------------------------------
 * INCLUDE the generated p_XXX_iter_ALT procedures
 * ----------------------------------------
 * ----------------------------------------
 */


#include "iter_ALT/mod/plug_in_Twist_iter_ALT.inc"
#include "iter_ALT/mod/plug_in_alienmap2_iter_ALT.inc"
#include "iter_ALT/mod/plug_in_apply_canvas_iter_ALT.inc"
#include "iter_ALT/mod/plug_in_applylens_iter_ALT.inc"
#include "iter_ALT/mod/plug_in_bump_map_iter_ALT.inc"
#include "iter_ALT/mod/plug_in_cartoon_iter_ALT.inc"
#include "iter_ALT/mod/plug_in_colortoalpha_iter_ALT.inc"
#include "iter_ALT/mod/plug_in_colors_channel_mixer_iter_ALT.inc"
#include "iter_ALT/mod/plug_in_convmatrix_iter_ALT.inc"
#include "iter_ALT/mod/plug_in_depth_merge_iter_ALT.inc"
#include "iter_ALT/mod/plug_in_despeckle_iter_ALT.inc"
#include "iter_ALT/mod/plug_in_dog_iter_ALT.inc"
#include "iter_ALT/mod/plug_in_emboss_iter_ALT.inc"
#include "iter_ALT/mod/plug_in_exchange_iter_ALT.inc"
#include "iter_ALT/mod/plug_in_flame_iter_ALT.inc"
#include "iter_ALT/mod/plug_in_gauss_iter_ALT.inc"
#include "iter_ALT/mod/plug_in_gimpressionist_iter_ALT.inc"
#include "iter_ALT/mod/plug_in_illusion_iter_ALT.inc"
#include "iter_ALT/mod/plug_in_lic_iter_ALT.inc"
#include "iter_ALT/mod/plug_in_lighting_iter_ALT.inc"
#include "iter_ALT/mod/plug_in_map_object_iter_ALT.inc"
#include "iter_ALT/mod/plug_in_maze_iter_ALT.inc"
#include "iter_ALT/mod/plug_in_neon_iter_ALT.inc"
#include "iter_ALT/mod/plug_in_nlfilt_iter_ALT.inc"
#include "iter_ALT/mod/plug_in_nova_iter_ALT.inc"
#include "iter_ALT/mod/plug_in_oilify_iter_ALT.inc"
#include "iter_ALT/mod/plug_in_pagecurl_iter_ALT.inc"
#include "iter_ALT/mod/plug_in_papertile_iter_ALT.inc"
#include "iter_ALT/mod/plug_in_photocopy_iter_ALT.inc"
#include "iter_ALT/mod/plug_in_plasma_iter_ALT.inc"
#include "iter_ALT/mod/plug_in_polar_coords_iter_ALT.inc"
#include "iter_ALT/mod/plug_in_retinex_iter_ALT.inc"
#include "iter_ALT/mod/plug_in_sample_colorize_iter_ALT.inc"
#include "iter_ALT/mod/plug_in_sel_gauss_iter_ALT.inc"
#include "iter_ALT/mod/plug_in_sinus_iter_ALT.inc"
#include "iter_ALT/mod/plug_in_small_tiles_iter_ALT.inc"
#include "iter_ALT/mod/plug_in_sobel_iter_ALT.inc"
#include "iter_ALT/mod/plug_in_softglow_iter_ALT.inc"
#include "iter_ALT/mod/plug_in_solid_noise_iter_ALT.inc"
#include "iter_ALT/mod/plug_in_sparkle_iter_ALT.inc"
#include "iter_ALT/mod/plug_in_unsharp_mask_iter_ALT.inc"





#include "iter_ALT/old/plug_in_CentralReflection_iter_ALT.inc"
#include "iter_ALT/old/plug_in_anamorphose_iter_ALT.inc"
#include "iter_ALT/old/plug_in_blur2_iter_ALT.inc"
#include "iter_ALT/old/plug_in_encript_iter_ALT.inc"
#include "iter_ALT/old/plug_in_figures_iter_ALT.inc"
#include "iter_ALT/old/plug_in_gflare_iter_ALT.inc"
#include "iter_ALT/old/plug_in_holes_iter_ALT.inc"
#include "iter_ALT/old/plug_in_julia_iter_ALT.inc"
#include "iter_ALT/old/plug_in_magic_eye_iter_ALT.inc"
#include "iter_ALT/old/plug_in_mandelbrot_iter_ALT.inc"
#include "iter_ALT/old/plug_in_randomize_iter_ALT.inc"
#include "iter_ALT/old/plug_in_refract_iter_ALT.inc"
#include "iter_ALT/old/plug_in_struc_iter_ALT.inc"
#include "iter_ALT/old/plug_in_tileit_iter_ALT.inc"
#include "iter_ALT/old/plug_in_universal_filter_iter_ALT.inc"
#include "iter_ALT/old/plug_in_warp_iter_ALT.inc"


#include "iter_ALT/gen/plug_in_CML_explorer_iter_ALT.inc"
#include "iter_ALT/gen/plug_in_alpha2color_iter_ALT.inc"
#include "iter_ALT/gen/plug_in_blinds_iter_ALT.inc"
#include "iter_ALT/gen/plug_in_borderaverage_iter_ALT.inc"
#include "iter_ALT/gen/plug_in_checkerboard_iter_ALT.inc"
#include "iter_ALT/gen/plug_in_color_map_iter_ALT.inc"
#include "iter_ALT/gen/plug_in_colorify_iter_ALT.inc"
#include "iter_ALT/gen/plug_in_cubism_iter_ALT.inc"
#include "iter_ALT/gen/plug_in_destripe_iter_ALT.inc"
#include "iter_ALT/gen/plug_in_diffraction_iter_ALT.inc"
#include "iter_ALT/gen/plug_in_displace_iter_ALT.inc"
#include "iter_ALT/gen/plug_in_edge_iter_ALT.inc"
#include "iter_ALT/gen/plug_in_engrave_iter_ALT.inc"
#include "iter_ALT/gen/plug_in_flarefx_iter_ALT.inc"
#include "iter_ALT/gen/plug_in_fractal_trace_iter_ALT.inc"
#include "iter_ALT/gen/plug_in_glasstile_iter_ALT.inc"
#include "iter_ALT/gen/plug_in_grid_iter_ALT.inc"
#include "iter_ALT/gen/plug_in_jigsaw_iter_ALT.inc"
#include "iter_ALT/gen/plug_in_mblur_iter_ALT.inc"
#include "iter_ALT/gen/plug_in_mosaic_iter_ALT.inc"
#include "iter_ALT/gen/plug_in_newsprint_iter_ALT.inc"
#include "iter_ALT/gen/plug_in_noisify_iter_ALT.inc"
#include "iter_ALT/gen/plug_in_pixelize_iter_ALT.inc"
#include "iter_ALT/gen/plug_in_randomize_hurl_iter_ALT.inc"
#include "iter_ALT/gen/plug_in_randomize_pick_iter_ALT.inc"
#include "iter_ALT/gen/plug_in_randomize_slur_iter_ALT.inc"
#include "iter_ALT/gen/plug_in_ripple_iter_ALT.inc"
#include "iter_ALT/gen/plug_in_scatter_hsv_iter_ALT.inc"
#include "iter_ALT/gen/plug_in_sharpen_iter_ALT.inc"
#include "iter_ALT/gen/plug_in_shift_iter_ALT.inc"
#include "iter_ALT/gen/plug_in_spread_iter_ALT.inc"
#include "iter_ALT/gen/plug_in_video_iter_ALT.inc"
#include "iter_ALT/gen/plug_in_vpropagate_iter_ALT.inc"
#include "iter_ALT/gen/plug_in_waves_iter_ALT.inc"
#include "iter_ALT/gen/plug_in_whirl_pinch_iter_ALT.inc"
#include "iter_ALT/gen/plug_in_wind_iter_ALT.inc"

/* table of proc_names and funtion pointers to iter_ALT procedures */
/* del ... Deleted (does not make sense to animate)
 * +  ... generated code did not work (changed manually)
 */
static t_iter_ALT_tab   g_iter_ALT_tab[] =
{
    { "plug-in-alienmap2",              p_plug_in_alienmap2_iter_ALT }
  , { "plug-in-cml-explorer",           p_plug_in_cml_explorer_iter_ALT }
  , { "plug-in-CentralReflection",      p_plug_in_CentralReflection_iter_ALT }
  , { "plug-in-Twist",                  p_plug_in_Twist_iter_ALT }
/*, { "plug-in-alienmap",               p_plug_in_alienmap_iter_ALT }                          */
/*, { "plug-in-align-layers",           p_plug_in_align_layers_iter_ALT }                  */
  , { "plug-in-alpha2color",            p_plug_in_alpha2color_iter_ALT }
  , { "plug-in-anamorphose",            p_plug_in_anamorphose_iter_ALT }
/*, { "plug-in-animationoptimize",      p_plug_in_animationoptimize_iter_ALT }        */
/*, { "plug-in-animationplay",          p_plug_in_animationplay_iter_ALT }                */
/*, { "plug-in-animationunoptimize",    p_plug_in_animationunoptimize_iter_ALT }    */
  , { "plug-in-apply-canvas",           p_plug_in_apply_canvas_iter_ALT }
  , { "plug-in-applylens",              p_plug_in_applylens_iter_ALT }
/*, { "plug-in-autocrop",               p_plug_in_autocrop_iter_ALT }                          */
/*, { "plug-in-autostretch-hsv",        p_plug_in_autostretch_hsv_iter_ALT }            */
  , { "plug-in-blinds",                 p_plug_in_blinds_iter_ALT }
/*, { "plug-in-blur",                   p_plug_in_blur_iter_ALT }                                  */
  , { "plug-in-blur2",                  p_plug_in_blur2_iter_ALT }
/*, { "plug-in-blur-randomize",         p_plug_in_blur_randomize_iter_ALT }              */
  , { "plug-in-borderaverage",          p_plug_in_borderaverage_iter_ALT }
  , { "plug-in-bump-map",               p_plug_in_bump_map_iter_ALT }
/*, { "plug-in-c-astretch",             p_plug_in_c_astretch_iter_ALT }                      */
  , { "plug-in-cartoon",                p_plug_in_cartoon_iter_ALT }
  , { "plug-in-checkerboard",           p_plug_in_checkerboard_iter_ALT }
/*, { "plug-in-color-adjust",           p_plug_in_color_adjust_iter_ALT }                  */
  , { "plug-in-color-map",              p_plug_in_color_map_iter_ALT }
  , { "plug-in-colorify",               p_plug_in_colorify_iter_ALT }
  , { "plug-in-colortoalpha",           p_plug_in_colortoalpha_iter_ALT }
  , { "plug-in-colors-channel-mixer",   p_plug_in_colors_channel_mixer_iter_ALT }
/*, { "plug-in-compose",                p_plug_in_compose_iter_ALT }                            */
  , { "plug-in-convmatrix",             p_plug_in_convmatrix_iter_ALT }
  , { "plug-in-cubism",                 p_plug_in_cubism_iter_ALT }
/*, { "plug-in-decompose",              p_plug_in_decompose_iter_ALT }                        */
/*, { "plug-in-deinterlace",            p_plug_in_deinterlace_iter_ALT }                    */
  , { "plug-in-depth-merge",            p_plug_in_depth_merge_iter_ALT }
  , { "plug-in-despeckle",              p_plug_in_despeckle_iter_ALT }
  , { "plug-in-destripe",               p_plug_in_destripe_iter_ALT }
  , { "plug-in-diffraction",            p_plug_in_diffraction_iter_ALT }
  , { "plug-in-displace",               p_plug_in_displace_iter_ALT }
/*, { "plug-in-ditherize",              p_plug_in_ditherize_iter_ALT }                        */
  , { "plug-in-dog",                    p_plug_in_dog_iter_ALT }
  , { "plug-in-edge",                   p_plug_in_edge_iter_ALT }
  , { "plug-in-emboss",                 p_plug_in_emboss_iter_ALT }
  , { "plug-in-encript",                p_plug_in_encript_iter_ALT }
  , { "plug-in-engrave",                p_plug_in_engrave_iter_ALT }
  , { "plug-in-exchange",               p_plug_in_exchange_iter_ALT }
/*, { "plug-in-export-palette",         p_plug_in_export_palette_iter_ALT }              */
  , { "plug-in-figures",                p_plug_in_figures_iter_ALT }
/*, { "plug-in-film",                   p_plug_in_film_iter_ALT }                                  */
/*, { "plug-in-filter-pack",            p_plug_in_filter_pack_iter_ALT }                    */
  , { "plug-in-flame",                  p_plug_in_flame_iter_ALT }
  , { "plug-in-flarefx",                p_plug_in_flarefx_iter_ALT }
  , { "plug-in-fractal-trace",          p_plug_in_fractal_trace_iter_ALT }
  , { "plug-in-gauss",                  p_plug_in_gauss_iter_ALT }
/*, { "plug-in-gfig",                   p_plug_in_gfig_iter_ALT }                                  */
  , { "plug-in-gflare",                 p_plug_in_gflare_iter_ALT }
  , { "plug-in-gimpressionist",         p_plug_in_gimpressionist_iter_ALT }
  , { "plug-in-glasstile",              p_plug_in_glasstile_iter_ALT }
/*, { "plug-in-gradmap",                p_plug_in_gradmap_iter_ALT }                            */
  , { "plug-in-grid",                   p_plug_in_grid_iter_ALT }
/*, { "plug-in-guillotine",             p_plug_in_guillotine_iter_ALT }                      */
  , { "plug-in-holes",                  p_plug_in_holes_iter_ALT }
/*, { "plug-in-hot",                    p_plug_in_hot_iter_ALT }                                    */
/*, { "plug-in-ifs-compose",            p_plug_in_ifs_compose_iter_ALT }                    */
  , { "plug-in-illusion",               p_plug_in_illusion_iter_ALT }
/*, { "plug-in-image-rot270",           p_plug_in_image_rot270_iter_ALT }                  */
/*, { "plug-in-image-rot90",            p_plug_in_image_rot90_iter_ALT }                    */
/*, { "plug-in-iwarp",                  p_plug_in_iwarp_iter_ALT }                                */
  , { "plug-in-jigsaw",                 p_plug_in_jigsaw_iter_ALT }
  , { "plug-in-julia",                  p_plug_in_julia_iter_ALT }
/*, { "plug-in-laplace",                p_plug_in_laplace_iter_ALT }                            */
/*, { "plug-in-layer-rot270",           p_plug_in_layer_rot270_iter_ALT }                  */
/*, { "plug-in-layer-rot90",            p_plug_in_layer_rot90_iter_ALT }                    */
/*, { "plug-in-layers-import",          p_plug_in_layers_import_iter_ALT }                */
  , { "plug-in-lic",                    p_plug_in_lic_iter_ALT }
  , { "plug-in-lighting",               p_plug_in_lighting_iter_ALT }
  , { "plug-in-magic-eye",              p_plug_in_magic_eye_iter_ALT }
/*, { "plug-in-mail-image",             p_plug_in_mail_image_iter_ALT }                      */
  , { "plug-in-mandelbrot",             p_plug_in_mandelbrot_iter_ALT }
  , { "plug-in-map-object",             p_plug_in_map_object_iter_ALT }
/*, { "plug-in-max-rgb",                p_plug_in_max_rgb_iter_ALT }                            */
  , { "plug-in-maze",                   p_plug_in_maze_iter_ALT }
  , { "plug-in-mblur",                  p_plug_in_mblur_iter_ALT }
  , { "plug-in-mosaic",                 p_plug_in_mosaic_iter_ALT }
  , { "plug-in-neon",                   p_plug_in_neon_iter_ALT }
  , { "plug-in-newsprint",              p_plug_in_newsprint_iter_ALT }
  , { "plug-in-nlfilt",                 p_plug_in_nlfilt_iter_ALT }
  , { "plug-in-rgb-noise",              p_plug_in_noisify_iter_ALT }
/*, { "plug-in-normalize",              p_plug_in_normalize_iter_ALT }                        */
  , { "plug-in-nova",                   p_plug_in_nova_iter_ALT }
  , { "plug-in-oilify",                 p_plug_in_oilify_iter_ALT }
  , { "plug-in-pagecurl",               p_plug_in_pagecurl_iter_ALT }
  , { "plug-in-papertile",              p_plug_in_papertile_iter_ALT }
  , { "plug-in-photocopy",              p_plug_in_photocopy_iter_ALT }
  , { "plug-in-pixelize",               p_plug_in_pixelize_iter_ALT }
  , { "plug-in-plasma",                 p_plug_in_plasma_iter_ALT }
  , { "plug-in-polar-coords",           p_plug_in_polar_coords_iter_ALT }
/*, { "plug-in-qbist",                  p_plug_in_qbist_iter_ALT }                                */
  , { "plug-in-randomize",              p_plug_in_randomize_iter_ALT }
  , { "plug-in-randomize-hurl",         p_plug_in_randomize_hurl_iter_ALT }
  , { "plug-in-randomize-pick",         p_plug_in_randomize_pick_iter_ALT }
  , { "plug-in-randomize-slur",         p_plug_in_randomize_slur_iter_ALT }
  , { "plug-in-refract",                p_plug_in_refract_iter_ALT }
  , { "plug-in-retinex",                p_plug_in_retinex_iter_ALT }
  , { "plug-in-ripple",                 p_plug_in_ripple_iter_ALT }
/*, { "plug-in-rotate",                 p_plug_in_rotate_iter_ALT }                              */
  , { "plug-in-sample-colorize",        p_plug_in_sample_colorize_iter_ALT }
  , { "plug-in-hsv-noise",              p_plug_in_scatter_hsv_iter_ALT }
  , { "plug-in-sel-gauss",              p_plug_in_sel_gauss_iter_ALT }
/*, { "plug-in-semiflatten",            p_plug_in_semiflatten_iter_ALT }                    */
  , { "plug-in-sharpen",                p_plug_in_sharpen_iter_ALT }
  , { "plug-in-shift",                  p_plug_in_shift_iter_ALT }
  , { "plug-in-sinus",                  p_plug_in_sinus_iter_ALT }
  , { "plug-in-small-tiles",            p_plug_in_small_tiles_iter_ALT }
/*, { "plug-in-smooth-palette",         p_plug_in_smooth_palette_iter_ALT }              */
  , { "plug-in-sobel",                  p_plug_in_sobel_iter_ALT }
  , { "plug-in-softglow",               p_plug_in_softglow_iter_ALT }
  , { "plug-in-solid-noise",            p_plug_in_solid_noise_iter_ALT }
  , { "plug-in-sparkle",                p_plug_in_sparkle_iter_ALT }
  , { "plug-in-spread",                 p_plug_in_spread_iter_ALT }
  , { "plug-in-struc",                  p_plug_in_struc_iter_ALT }
/*, { "plug-in-the-egg",                p_plug_in_the_egg_iter_ALT }                            */
/*, { "plug-in-threshold-alpha",        p_plug_in_threshold_alpha_iter_ALT }            */
/*, { "plug-in-tile",                   p_plug_in_tile_iter_ALT }                                  */
  , { "plug-in-tileit",                 p_plug_in_tileit_iter_ALT }
  , { "plug-in-universal-filter",       p_plug_in_universal_filter_iter_ALT }
  , { "plug-in-unsharp-mask",           p_plug_in_unsharp_mask_iter_ALT }
  , { "plug-in-video",                  p_plug_in_video_iter_ALT }
/*, { "plug-in-vinvert",                p_plug_in_vinvert_iter_ALT }                            */
  , { "plug-in-vpropagate",             p_plug_in_vpropagate_iter_ALT }
  , { "plug-in-warp",                   p_plug_in_warp_iter_ALT }
  , { "plug-in-waves",                  p_plug_in_waves_iter_ALT }
  , { "plug-in-whirl-pinch",            p_plug_in_whirl_pinch_iter_ALT }
  , { "plug-in-wind",                   p_plug_in_wind_iter_ALT }
/*, { "plug-in-zealouscrop",            p_plug_in_zealouscrop_iter_ALT }                    */

};  /* end g_iter_ALT_tab */


#define MAX_ITER_ALT ( sizeof(g_iter_ALT_tab) / sizeof(t_iter_ALT_tab) )


/* ----------------------------------------------------------------------
 * install (query) iterators_ALT
 * ----------------------------------------------------------------------
 */
static void p_install_proc_iter_ALT(char *name)
{
  gchar *l_iter_proc_name;
  gchar *l_blurb_text;

  static GimpParamDef args_iter[] =
  {
    {GIMP_PDB_INT32, "run_mode", "non-interactive"},
    {GIMP_PDB_INT32, "total_steps", "total number of steps (# of layers-1 to apply the related plug-in)"},
    {GIMP_PDB_FLOAT, "current_step", "current (for linear iterations this is the layerstack position, otherwise some value inbetween)"},
    {GIMP_PDB_INT32, "len_struct", "length of stored data structure with id is equal to the plug_in  proc_name"},
  };

  static GimpParamDef *return_vals = NULL;
  static int nreturn_vals = 0;

  l_iter_proc_name = g_strdup_printf("%s%s", name, GAP_ITERATOR_ALT_SUFFIX);
  l_blurb_text = g_strdup_printf("This procedure calculates the modified values for one iterationstep for the call of %s", name);

  gimp_install_procedure(l_iter_proc_name,
                         l_blurb_text,
                         "",
                         "Wolfgang Hofer",
                         "Wolfgang Hofer",
                         "Nov. 2007",
                         NULL,    /* do not appear in menus */
                         NULL,
                         GIMP_PLUGIN,
                         G_N_ELEMENTS (args_iter), nreturn_vals,
                         args_iter, return_vals);

  g_free(l_iter_proc_name);
  g_free(l_blurb_text);
}


void
gap_query_iterators_ALT()
{
  guint l_idx;
  for(l_idx = 0; l_idx < MAX_ITER_ALT; l_idx++)
  {
     p_install_proc_iter_ALT (g_iter_ALT_tab[l_idx].proc_name);
  }
}

/* ----------------------------------------------------------------------
 * run iterators_ALT
 * ----------------------------------------------------------------------
 */

gint
gap_run_iterators_ALT(const char *name, GimpRunMode run_mode, gint32 total_steps, gdouble current_step, gint32 len_struct)
{
  gint l_rc;
  guint l_idx;
  char *l_name;
  int   l_cut;

  if(gap_debug)
  {
    printf("gap_run_iterators_ALT  START for name:%s\n", name);
  }

  l_name = g_strdup(name);
  l_cut  = strlen(l_name) - strlen(GAP_ITERATOR_ALT_SUFFIX);
  if(l_cut < 1)
  {
     printf("ERROR: gap_run_iterators_ALT: proc_name ending %s missing%s\n"
            , GAP_ITERATOR_ALT_SUFFIX
            , name
            );
     return -1;
  }
  if(strcmp(&l_name[l_cut], GAP_ITERATOR_ALT_SUFFIX) != 0)
  {
     printf("ERROR: gap_run_iterators_ALT: proc_name ending %s missing%s\n"
            , GAP_ITERATOR_ALT_SUFFIX
            , name
            );
     return -1;
  }


  l_rc = -1;
  l_name[l_cut] = '\0';  /* cut off "-Iterator-ALT" from l_name end */

  /* allocate from/to plugin_data buffers
   * as big as needed for the current plugin named l_name
   */
  g_plugin_data_from = p_alloc_plugin_data(l_name);
  g_plugin_data_to = p_alloc_plugin_data(l_name);

  if((g_plugin_data_from != NULL)
  && (g_plugin_data_to != NULL))
  {
    for(l_idx = 0; l_idx < MAX_ITER_ALT; l_idx++)
    {
        if (strcmp (l_name, g_iter_ALT_tab[l_idx].proc_name) == 0)
        {
          if(gap_debug) printf("DEBUG: gap_run_iterators_ALT: FOUND %s\n", l_name);
          l_rc =  (g_iter_ALT_tab[l_idx].proc_func)(run_mode, total_steps, current_step, len_struct);
        }
    }
  }

  if(l_rc < 0) fprintf(stderr, "ERROR: gap_run_iterators_ALT: NOT FOUND proc_name=%s (%s)\n", name, l_name);

  /* free from/to plugin_data buffers */
  if(g_plugin_data_from) g_free(g_plugin_data_from);
  if(g_plugin_data_to)   g_free(g_plugin_data_to);

  return l_rc;
}  /* end gap_run_iterators_ALT */

