/* gap_morph_exec.c
 * 2004.02.12 hof (Wolfgang Hofer)
 * layer morphing worker procedures
 *
 * Note:
 *  using multiple workpoint sets is an unfinshed feature and does not work yet.
 *  (this feature was intended for morphing between 2 videos, where each handled video frame
 *  can have its own workpoint set. but his is no practical solution since creating of the
 *  workpoint files is too much manual work)
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
 * <http://www.gnu.org/licenses/>.
 */

/* revision history:
 * gimp    2.0.2b; 2004/07/28  hof: added p_mix_layers
 * gimp    2.0.2b; 2004/07/17  hof: p_pixel_warp_core: changed workpoint selection (both FAST and Quality strategy)
 * gimp    2.0.2a; 2004/04/07  hof: created
 */

#include "config.h"

/* SYTEM (UNIX) includes */
#include "math.h"
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <errno.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include <glib/gstdio.h>

/* GIMP includes */
#include "gtk/gtk.h"
#include "libgimp/gimp.h"

/* GAP includes */
#include "gap_libgapbase.h"
#include "gap-intl.h"
#include "gap_image.h"
#include "gap_morph_main.h"
#include "gap_morph_exec.h"
#include "gap_layer_copy.h"


/* GAP_MORPH_USE_SIMPLE_PIXEL_WARP is used for development test only
 * (gives bad quality results, you should NOT enable this option)
 */
#undef  GAP_MORPH_USE_SIMPLE_PIXEL_WARP

#define GAP_MORPH_NEAR_SQR_RADIUS  64
#define GAP_MORPH_NEAR_RADIUS  4
#define GAP_MORPH_FAR_RADIUS  40
#define GAP_MORPH_TOL_FACT_NEAR  3.0
#define GAP_MORPH_TOL_FACT_FAR   1.5

#define GAP_MORPH_8_SEKTORS 8
#define GAP_MORPH_TOL_FAKTOR 2.25
#define GAP_MORPH_MIN_TOL_FAKTOR 4.5
#define GAP_MORPH_XY_REL_FAKTOR 5


typedef struct GapMorphExeLayerstack
{
  gint32    *src_layers;
  gint       src_nlayers;
  gint32    *dst_layers;
  gint       dst_nlayers;
  gint       src1_idx;
  gint       src2_idx;
  gint       dst1_idx;
  gint       dst2_idx;

  gint32 available_wp_sets;
  GapMorphWarpCoreAPI **tab_wp_sets;
  
} GapMorphExeLayerstack;

extern int gap_debug;

static inline gdouble     p_get_tolerance(gdouble dist);
static void               p_clip_gimp_pixel_fetcher_get_pixel(GimpPixelFetcher *pft
                                   ,GimpDrawable     *drawable
                                   ,gint xi
                                   ,gint yi
                                   ,guchar *pixel
                                   );
static void               p_bilinear_get_pixel(GimpPixelFetcher *pft
                                         , GimpDrawable     *drawable
                                         , gdouble needx
                                         , gdouble needy
                                         , guchar *dest
                                         , gint dst_bpp
                                         );
static gdouble            p_linear_advance(gdouble total_steps
                                     ,gdouble current_step
                                     ,gdouble val_from
                                     ,gdouble val_to
                                     );
static inline gint        p_calc_sector(GapMorphWorkPoint *wp, gdouble dx, gdouble dy);
static inline gint        p_calc_angle_core(GapMorphWorkPoint *wp, gdouble dx, gdouble dy, gint num_sectors);
static gint               p_calc_angle(GapMorphWorkPoint *wp, gint32 in_x, gint32 in_y);
static void               p_pixel_warp_core(GapMorphWarpCoreAPI *wcap
                                      , gint32        in_x
                                      , gint32        in_y
                                      , gdouble *out_pick_x
                                      , gdouble *out_pick_y
                                      );
static void               p_pixel_warp_pick(GapMorphWarpCoreAPI *wcap
                                      , gint32        in_x
                                      , gint32        in_y
                                      , gint          set_idx
                                      , gdouble      *out_pick_x
                                      , gdouble      *out_pick_y
                                      );
static void               p_pixel_warp_multipick(GapMorphWarpCoreAPI *wcap_1
                                      , GapMorphWarpCoreAPI *wcap_2
                                      , gdouble       wp_mix_factor
                                      , gint32        in_x
                                      , gint32        in_y
                                      , gdouble      *out_pick_x
                                      , gdouble      *out_pick_y
                                      );
GapMorphWorkPoint *       p_calculate_work_point_movement(gdouble total_steps
                               ,gdouble current_step
                               ,GapMorphWorkPoint *master_list
                               ,gint32 src_layer_id
                               ,gint32 dst_layer_id
                               ,gint32 curr_width
                               ,gint32 curr_height
                               ,gboolean forward_move
                               );

static gint32 p_get_tween_steps_and_layerstacks(GapMorphGlobalParams *mgpp, GapMorphExeLayerstack *mlayers);

#define GAP_MORPH_WORKPOINT_FILE_HEADER "GAP-MORPH workpoint file"


typedef struct GapMorphExePickCache
{
  gboolean valid;
  gdouble  xx;
  gdouble  yy;
  gdouble  pick_x;
  gdouble  pick_y;
  
} GapMorphExePickCache;

#define GAP_MORPH_PCK_CACHE_SIZE 32
static  GapMorphExePickCache gloabl_pick_cache[GAP_MORPH_PCK_CACHE_SIZE][2];
static  gint gloabl_pick_cache_idx[2];


/* ---------------------------------
 * p_get_tolerance
 * ---------------------------------
 */
static inline gdouble
p_get_tolerance(gdouble dist)
{
  gdouble factor;
  
  if(dist <= GAP_MORPH_NEAR_RADIUS)
  {
    return(GAP_MORPH_TOL_FACT_NEAR);
  }
  if(dist >= GAP_MORPH_FAR_RADIUS)
  {
    return(GAP_MORPH_TOL_FACT_FAR);
  }
  
  factor = (dist - GAP_MORPH_NEAR_RADIUS) / GAP_MORPH_FAR_RADIUS;
  return(  (GAP_MORPH_TOL_FACT_NEAR * (1 -factor)) 
         + (GAP_MORPH_TOL_FACT_FAR *  factor)
        );
}  /* end  p_get_tolerance */

static void
p_error_message_with_filename(GimpRunMode run_mode, const char *msg_fmt, const char *filename)
{
  char *l_msg;

  l_msg = g_strdup_printf(msg_fmt, filename);
  printf("%s RUN_MODE:%d\n", l_msg, (int)run_mode);
  
  if(run_mode != GIMP_RUN_NONINTERACTIVE)
  {
    g_message("%s", l_msg);
  }
  g_free(l_msg);

}



/* ---------------------------------
 * gap_moprh_exec_save_workpointfile
 * ---------------------------------
 */
gboolean
gap_moprh_exec_save_workpointfile(const char *filename
                                 , GapMorphGUIParams *mgup
                                 )
{
  FILE *l_fp;
  GapMorphWorkPoint *wp;
  gint32 src_layer_width;
  gint32 src_layer_height;
  gint32 dst_layer_width;
  gint32 dst_layer_height;
  guchar l_red, l_green, l_blue;

  if(filename == NULL) return -1;

  l_fp = g_fopen(filename, "w+");
  if(l_fp != NULL)
  {
    src_layer_width  = gimp_drawable_width(mgup->mgpp->osrc_layer_id);
    src_layer_height = gimp_drawable_height(mgup->mgpp->osrc_layer_id);
    dst_layer_width  = gimp_drawable_width(mgup->mgpp->fdst_layer_id);
    dst_layer_height = gimp_drawable_height(mgup->mgpp->fdst_layer_id);


    fprintf(l_fp, "%s\n", GAP_MORPH_WORKPOINT_FILE_HEADER);
    fprintf(l_fp, "LAYER-SIZES: %d  %d  %d %d  # src_width src_height dst_width dst_height\n"
                  ,(int)src_layer_width
                  ,(int)src_layer_height
                  ,(int)dst_layer_width
                  ,(int)dst_layer_height
                  );
    fprintf(l_fp, "TWEEN-STEPS: %d\n"
                  ,(int)mgup->mgpp->tween_steps
                  );
    fprintf(l_fp, "AFFECT-RADIUS:");
    gap_base_fprintf_gdouble(l_fp, mgup->mgpp->affect_radius, 4, 2, " ");
    fprintf(l_fp, " # pixels\n");

    fprintf(l_fp, "INTENSITY:");
    if(mgup->mgpp->use_gravity)
    {
      gap_base_fprintf_gdouble(l_fp, mgup->mgpp->gravity_intensity, 2, 3, " ");
      fprintf(l_fp, "\n");
    }
    else
    {
      fprintf(l_fp, "0 # OFF\n");
    }

    if(mgup->mgpp->use_quality_wp_selection)
    {
      fprintf(l_fp, "QUALITY-WP-SELECT: 1 # TRUE\n");
    }
    else
    {
      fprintf(l_fp, "QUALITY-WP-SELECT: 0 # FALSE (faster but lower quality)\n");
    }

    gimp_rgb_get_uchar (&mgup->pointcolor, &l_red, &l_green, &l_blue);
    fprintf(l_fp, "POINTCOLOR: %d %d %d\n"
                  ,(int)l_red
                  ,(int)l_green
                  ,(int)l_blue
                  );
    gimp_rgb_get_uchar (&mgup->curr_pointcolor, &l_red, &l_green, &l_blue);
    fprintf(l_fp, "CURR-POINTCOLOR: %d %d %d\n"
                  ,(int)l_red
                  ,(int)l_green
                  ,(int)l_blue
                  );


    fprintf(l_fp, "# WP: src_x  src_y  dst_x dst_y\n");

    for(wp = mgup->mgpp->master_wp_list; wp != NULL; wp = (GapMorphWorkPoint *)wp->next)
    {
      fprintf(l_fp, "WP: ");
      gap_base_fprintf_gdouble(l_fp, wp->osrc_x, 4, 3, " ");
      gap_base_fprintf_gdouble(l_fp, wp->osrc_y, 4, 3, " ");
      gap_base_fprintf_gdouble(l_fp, wp->fdst_x, 4, 3, " ");
      gap_base_fprintf_gdouble(l_fp, wp->fdst_y, 4, 3, " ");

      fprintf(l_fp, "\n");   /* terminate the output line */
    }

    fclose(l_fp);
    return (TRUE);
  }
  return (FALSE);
}  /* end gap_moprh_exec_save_workpointfile */


/* ----------------------------------
 * p_load_workpointfile
 * ----------------------------------
 * return root pointer to the loaded workpoint list on SUCCESS
 * return NULL if load failed.
 */
GapMorphWorkPoint *
p_load_workpointfile(const char *filename
                                 , gint32 src_layer_id
                                 , gint32 dst_layer_id
                                 , gint32 *tween_steps
                                 , gdouble *affect_radius
                                 , gdouble *gravity_intensity
                                 , gboolean *use_gravity
                                 , gboolean *use_quality_wp_selection
                                 , GimpRunMode run_mode
                                 )
{
#define POINT_REC_MAX 512
#define MAX_NUMVALUES_PER_LINE 17
  FILE   *l_fp;
  GapMorphWorkPoint *wp;
  GapMorphWorkPoint *wp_list;
  gint32 src_layer_width;
  gint32 src_layer_height;
  gint32 dst_layer_width;
  gint32 dst_layer_height;
  gdouble src_scale_x;
  gdouble src_scale_y;
  gdouble dst_scale_x;
  gdouble dst_scale_y;
  
  
  char    l_buff[POINT_REC_MAX +1 ];
  char   *l_ptr;
  gint    l_cnt;
  gdouble l_farr[MAX_NUMVALUES_PER_LINE];


  src_layer_width  = gimp_drawable_width(src_layer_id);
  src_layer_height = gimp_drawable_height(src_layer_id);
  dst_layer_width  = gimp_drawable_width(dst_layer_id);
  dst_layer_height = gimp_drawable_height(dst_layer_id);
  src_scale_x = 1.0;
  src_scale_y = 1.0;
  dst_scale_x = 1.0;
  dst_scale_y = 1.0;
  wp_list = NULL;
  wp = NULL;
  if(filename == NULL) return(NULL);

  *use_gravity = FALSE;
  *use_quality_wp_selection = FALSE;
  l_fp = g_fopen(filename, "r");
  if(l_fp != NULL)
  {
    gint l_len;
    
    fgets (l_buff, POINT_REC_MAX, l_fp);
    l_len = strlen(GAP_MORPH_WORKPOINT_FILE_HEADER);
    if(strncmp(l_buff, GAP_MORPH_WORKPOINT_FILE_HEADER, l_len) != 0)
    {
      p_error_message_with_filename(run_mode, _("File: %s\n ==>is no workpointfile (header is missing)"), filename);
      fclose(l_fp);
      return (NULL);
    }
    
    while (NULL != fgets (l_buff, POINT_REC_MAX, l_fp))
    {
       /* skip leading blanks */
       l_ptr = l_buff;
       while(*l_ptr == ' ') { l_ptr++; }

       /* check if line empty or comment only (starts with '#') */
       if((*l_ptr != '#') && (*l_ptr != '\n') && (*l_ptr != '\0'))
       {
         l_len = strlen("LAYER-SIZES:");
         if(strncmp(l_ptr, "LAYER-SIZES:", l_len) == 0)
         {
           l_ptr += l_len;
           l_cnt = gap_base_sscan_flt_numbers(l_ptr, &l_farr[0], MAX_NUMVALUES_PER_LINE);
           if(l_cnt >= 4)
           {
             src_scale_x =  MAX(1,src_layer_width)  / MAX(1,l_farr[0]);
             src_scale_y =  MAX(1,src_layer_height) / MAX(1,l_farr[1]);
             dst_scale_x =  MAX(1,dst_layer_width)  / MAX(1,l_farr[2]);
             dst_scale_y =  MAX(1,dst_layer_height) / MAX(1,l_farr[3]);
           }
           else
           {
             p_error_message_with_filename(run_mode, _("file: %s\n ==> is corrupted (LAYER-SIZES: record requires 4 numbers)")
                   , filename);
             fclose(l_fp);
             return (NULL);
           }
         }

         l_len = strlen("TWEEN-STEPS:");
         if(strncmp(l_ptr, "TWEEN-STEPS:", l_len) == 0)
         {
           l_ptr += l_len;
           l_cnt = gap_base_sscan_flt_numbers(l_ptr, &l_farr[0], MAX_NUMVALUES_PER_LINE);
           if(l_cnt >= 1)
           {
             *tween_steps = MAX(1,l_farr[0]);
           }
           else
           {
             p_error_message_with_filename(run_mode, _("file: %s\n ==> is corrupted (TWEEN-STEPS record requires 1 number)")
                   , filename);
             fclose(l_fp);
             return (NULL);
           }
         }

         l_len = strlen("AFFECT-RADIUS:");
         if(strncmp(l_ptr, "AFFECT-RADIUS:", l_len) == 0)
         {
           l_ptr += l_len;
           l_cnt = gap_base_sscan_flt_numbers(l_ptr, &l_farr[0], MAX_NUMVALUES_PER_LINE);
           if(l_cnt >= 1)
           {
             *affect_radius = MAX(0,l_farr[0]);
           }
           else
           {
             p_error_message_with_filename(run_mode, _("file: %s ==> is corrupted (AFFECT-RADIUS record requires 1 number)")
                   , filename);
             fclose(l_fp);
             return (NULL);
           }
         }

         l_len = strlen("INTENSITY:");
         if(strncmp(l_ptr, "INTENSITY:", l_len) == 0)
         {
           l_ptr += l_len;
           l_cnt = gap_base_sscan_flt_numbers(l_ptr, &l_farr[0], MAX_NUMVALUES_PER_LINE);
           if(l_cnt >= 1)
           {
             *gravity_intensity = MAX(0,l_farr[0]);
             if(*gravity_intensity > 0.0)
             {
               *use_gravity = TRUE;
             }
           }
           else
           {
             p_error_message_with_filename(run_mode, _("file: %s\n ==>is corrupted (INTENSITY record requires 1 number)")
                   , filename);
             fclose(l_fp);
             return (NULL);
           }
         }

         l_len = strlen("QUALITY-WP-SELECT:");
         if(strncmp(l_ptr, "QUALITY-WP-SELECT:", l_len) == 0)
         {
           l_ptr += l_len;
           l_cnt = gap_base_sscan_flt_numbers(l_ptr, &l_farr[0], MAX_NUMVALUES_PER_LINE);
           if(l_cnt >= 1)
           {
             *use_quality_wp_selection = FALSE;
             if(l_farr[0] != 0)
             {
               *use_quality_wp_selection = TRUE;
             }
           }
           else
           {
             p_error_message_with_filename(run_mode, _("file: %s\n ==>is corrupted (QUALITY-WP-SELECT record requires 1 number)")
                   , filename);
             fclose(l_fp);
             return (NULL);
           }
         }
         
         l_len = strlen("WP:");
         if(strncmp(l_ptr, "WP:", l_len) == 0)
         {
           l_ptr += l_len;
           l_cnt = gap_base_sscan_flt_numbers(l_ptr, &l_farr[0], MAX_NUMVALUES_PER_LINE);
           if(l_cnt >= 4)
           {
             wp = g_new(GapMorphWorkPoint, 1);
             
             /* scale the loaded workpoints 
              * (to fit the current layer)
              */
             wp->osrc_x = l_farr[0] * src_scale_x;
             wp->osrc_y = l_farr[1] * src_scale_y;
             wp->fdst_x = l_farr[2] * dst_scale_x;
             wp->fdst_y = l_farr[3] * dst_scale_y;

             wp->src_x = wp->osrc_x;
             wp->src_y = wp->osrc_y;
             wp->dst_x = wp->fdst_x;
             wp->dst_y = wp->fdst_y;
             
             wp->next = wp_list;
             wp_list = wp;
           }
           else
           {
             p_error_message_with_filename(run_mode, _("file: %s\n ==> is corrupted (WP: record requires 4 numbers)")
                   , filename);
             fclose(l_fp);
             return (NULL);
           }
         }
         
       }
    }

    fclose(l_fp);
  }
  return (wp_list);
}  /* end p_load_workpointfile */


/* ----------------------------------
 * gap_moprh_exec_load_workpointfile
 * ----------------------------------
 * return root pointer to the loaded workpoint list on SUCCESS
 * return NULL if load failed.
 */
GapMorphWorkPoint *
gap_moprh_exec_load_workpointfile(const char *filename
                                 , GapMorphGUIParams *mgup
                                 )
{
  GapMorphWorkPoint *wp_list;
  
  wp_list = p_load_workpointfile(filename
                                ,mgup->mgpp->osrc_layer_id
                                ,mgup->mgpp->fdst_layer_id
                                ,&mgup->mgpp->tween_steps
                                ,&mgup->mgpp->affect_radius
                                ,&mgup->mgpp->gravity_intensity
                                ,&mgup->mgpp->use_gravity
                                ,&mgup->mgpp->use_quality_wp_selection
                                ,mgup->mgpp->run_mode
                                );
  return(wp_list);
}  /* end gap_moprh_exec_load_workpointfile */

/* ---------------------------------
 * p_load_workpoint_set
 * ---------------------------------
 * load workpoint set from file.
 * in case file not exists or other loading troubles
 * use the master workpoint list as the default workpoint set.
 */
static GapMorphWarpCoreAPI*
p_load_workpoint_set(const char *filename
                    ,GapMorphGlobalParams  *mgpp
                    ,GapMorphExeLayerstack *mlayers
                    )
{
  GapMorphWarpCoreAPI *wps;
  gint32              tween_steps;
  
  wps = g_new(GapMorphWarpCoreAPI ,1);
  wps->wp_list = p_load_workpointfile(filename
                                ,mgpp->osrc_layer_id
                                ,mgpp->fdst_layer_id
                                ,&tween_steps
                                ,&wps->affect_radius
                                ,&wps->gravity_intensity
                                ,&wps->use_gravity
                                ,&wps->use_quality_wp_selection
                                ,mgpp->run_mode
                                );
  if(wps->wp_list == NULL)
  {
    wps->wp_list = mgpp->master_wp_list;
    wps->affect_radius = mgpp->affect_radius;
    wps->gravity_intensity = mgpp->gravity_intensity;
    wps->use_gravity = mgpp->use_gravity;
    wps->use_quality_wp_selection = mgpp->use_quality_wp_selection;
  }
  return(wps);
}   /* end p_load_workpoint_set */



/* ---------------------------------
 * p_build_wp_set_table
 * ---------------------------------
 */
static void
p_build_wp_set_table(GapMorphGlobalParams  *mgpp
                    ,GapMorphExeLayerstack *mlayers
                    )
{
  long lower_num;
  long upper_num;
  gchar *lower_basename;
  gchar *upper_basename;
  gchar *lower_ext;
  gchar *upper_ext;
  
  lower_basename = gap_lib_alloc_basename(mgpp->workpoint_file_lower
                                         ,&lower_num);
  upper_basename = gap_lib_alloc_basename(mgpp->workpoint_file_upper
                                         ,&upper_num);
  lower_ext = gap_lib_alloc_extension(mgpp->workpoint_file_lower);
  upper_ext = gap_lib_alloc_extension(mgpp->workpoint_file_upper);
  
  if((lower_num != upper_num)
  && (strcmp(lower_basename, upper_basename) == 0)
  && (strcmp(lower_ext, upper_ext) == 0))
  {
    gchar *wp_filename;
    gint   ii;
    gint   wp_idx;
    
    if(gap_debug)
    {
      printf("p_build_wp_set_table: using multiple workpoint sets\n"
        " lower_basename:%s\n"
        " upper_basename:%s\n lower_num:%d upper_num:%d\n"
        ,lower_basename
        ,upper_basename
        ,(int)lower_num
        ,(int)upper_num
        );
    }
    
    
    mlayers->available_wp_sets = 0;
    for(ii=MIN(lower_num, upper_num); ii <= MAX(upper_num, lower_num); ii++)
    {
      wp_filename = g_strdup_printf("%s%02d%s"
                                   ,lower_basename
                                   ,(int)ii
                                   ,lower_ext
                                   );
      if(g_file_test(wp_filename, G_FILE_TEST_EXISTS))
      {
        mlayers->available_wp_sets++;
      }
      g_free(wp_filename);
    }

    mlayers->tab_wp_sets = g_new(GapMorphWarpCoreAPI*, mlayers->available_wp_sets);
    wp_idx = 0;
    for(ii=MIN(lower_num, upper_num); ii <= MAX(upper_num, lower_num); ii++)
    {
      wp_filename = g_strdup_printf("%s%02d%s"
                                   ,lower_basename
                                   ,(int)ii
                                   ,lower_ext
                                   );
      if(g_file_test(wp_filename, G_FILE_TEST_EXISTS))
      {
        if(wp_idx < mlayers->available_wp_sets)
        {
          if(gap_debug)
          {
            printf("p_build_wp_set_table: LOADING workpoint file: %s\n", wp_filename);
          }
          mlayers->tab_wp_sets[wp_idx] = p_load_workpoint_set(wp_filename, mgpp, mlayers);
        }
        wp_idx++;
      }
      g_free(wp_filename);
    }
    
  }
  else
  {
    /* create 2 workpoint sets from upper and lower file */
    if(gap_debug)
    {
      printf("p_build_wp_set_table: create 2 workpoint sets from upper and lower file\n lower:%s\n upper:%s\n"
        ,mgpp->workpoint_file_lower
        ,mgpp->workpoint_file_upper
        );
    }
    mlayers->available_wp_sets = 2;
    mlayers->tab_wp_sets = g_new(GapMorphWarpCoreAPI*, mlayers->available_wp_sets);
    mlayers->tab_wp_sets[0] = p_load_workpoint_set(mgpp->workpoint_file_lower, mgpp, mlayers);
    mlayers->tab_wp_sets[1] = p_load_workpoint_set(mgpp->workpoint_file_upper, mgpp, mlayers);
  }
  
  g_free(lower_basename);
  g_free(upper_basename);
  g_free(lower_ext);
  g_free(upper_ext);
  
}   /* end p_build_wp_set_table */



/* ---------------------------------
 * gap_morph_exec_find_dst_drawable
 * ---------------------------------
 * search for a 2.nd layer in image_id
 * that has same size as the input layer_id
 * (prefere layer on top of the input layer_id)
 */
gint32  
gap_morph_exec_find_dst_drawable(gint32 image_id, gint32 layer_id)
{
  gint l_width;
  gint l_height;
  gint    l_ii;
  gint    l_nlayers;
  gint32 *l_layers_list;
  gint32  l_ret_layer_id;
 
  l_ret_layer_id = -1;
  l_width = gimp_drawable_width(layer_id);
  l_height = gimp_drawable_height(layer_id);

  l_layers_list = gimp_image_get_layers(image_id, &l_nlayers);
  if(l_layers_list == NULL)
  {
     return (-1);
  }
  
  for(l_ii=0; l_ii < l_nlayers; l_ii++)
  {
    if(l_layers_list[l_ii] != layer_id)
    {
      if((gimp_drawable_width(l_layers_list[l_ii]) == l_width)
      && (gimp_drawable_height(l_layers_list[l_ii]) == l_height))
      {
         l_ret_layer_id = l_layers_list[l_ii];
         break;
      }
      if(l_ret_layer_id == -1)
      {
        l_ret_layer_id = l_layers_list[l_ii];
      }
    }
  }
  g_free (l_layers_list);
  return(l_ret_layer_id);
}  /* end gap_morph_exec_find_dst_drawable */


/* -----------------------------------
 * p_clip_gimp_pixel_fetcher_get_pixel
 * -----------------------------------
 */
static void
p_clip_gimp_pixel_fetcher_get_pixel(GimpPixelFetcher *pft
                                   ,GimpDrawable     *drawable
                                   ,gint xi
                                   ,gint yi
                                   ,guchar *pixel
                                   )
{
  if((xi >= 0)
  && (yi >= 0)
  && (xi < (gint)drawable->width)
  && (yi < (gint)drawable->height))
  {
    gimp_pixel_fetcher_get_pixel (pft, xi, yi, pixel);
  }
  else
  {
    /* deliver full transparent black pixel when out of bounds */
    pixel[0] = 0;
    pixel[1] = 0;
    pixel[2] = 0;
    pixel[3] = 0;
  }
}  /* end p_clip_gimp_pixel_fetcher_get_pixel */

/* ------------------------
 * p_bilinear_get_pixel
 * ------------------------
 * pixel picking at gdouble needx/needy koords
 * picks 4 pixels at nearest integer koordinates
 * and calculate the value by bilinear interpolation.
 */
static void
p_bilinear_get_pixel(GimpPixelFetcher *pft
                    ,GimpDrawable     *drawable
                    , gdouble needx
                    , gdouble needy
                    , guchar *dest
                    , gint dst_bpp
                    )
{
  guchar  pixel[4][4];
  guchar  values[4];
  guchar  val;

  gint    xi, yi;
  guint k;

  if (needx >= 0.0)
    xi = (int) needx;
  else
    xi = -((int) -needx + 1);

  if (needy >= 0.0)
    yi = (int) needy;
  else
    yi = -((int) -needy + 1);

  p_clip_gimp_pixel_fetcher_get_pixel (pft, drawable, xi,     yi,     pixel[0]);
  p_clip_gimp_pixel_fetcher_get_pixel (pft, drawable, xi + 1, yi,     pixel[1]);
  p_clip_gimp_pixel_fetcher_get_pixel (pft, drawable, xi,     yi + 1, pixel[2]);
  p_clip_gimp_pixel_fetcher_get_pixel (pft, drawable, xi + 1, yi + 1, pixel[3]);

  for (k = 0; k < drawable->bpp; k++)
  {
    values[0] = pixel[0][k];
    values[1] = pixel[1][k];
    values[2] = pixel[2][k];
    values[3] = pixel[3][k];
    val = gimp_bilinear_8 (needx, needy, values);

    *dest++ = val;
  }

}  /* end p_bilinear_get_pixel */


/* ---------------------------------
 * p_linear_advance
 * ---------------------------------
 */
static gdouble
p_linear_advance(gdouble total_steps
                ,gdouble current_step
                ,gdouble val_from
                ,gdouble val_to
                )
{
   gdouble     delta;

   if(total_steps < 1) return(val_from);

   delta = ((val_to - val_from) / total_steps) * (total_steps - current_step);
   return (val_to - delta);
}  /* end p_linear_advance */

/* ----------------------------------
 * gap_morph_exec_free_workpoint_list
 * ----------------------------------
 */
void
gap_morph_exec_free_workpoint_list(GapMorphWorkPoint **wp_list)
{
  if(wp_list)
  {
    GapMorphWorkPoint *wp;
    GapMorphWorkPoint *wp_next;

    wp_next = NULL;
    for(wp = *wp_list; wp != NULL; wp = wp_next)
    {
      wp_next = wp->next;
      g_free(wp);
    }
    
    *wp_list = NULL;
  }
}  /* end gap_morph_exec_free_workpoint_list */


/* ---------------------------------
 * p_calc_sector
 * ---------------------------------
 * return sector index (an integer val 0 upto 7)
 * (is much faster than calculating the angle, but restricted to 8 sectors)
 */
static inline gint
p_calc_sector(GapMorphWorkPoint *wp, gdouble dx, gdouble dy)
{
  if(dx >= 0)
  {
    if(dy >= 0)
    {
      if(dx >= dy)
      {
        return(3);        /*  91 - 135 degree */
      }
      return(2);          /* 136 - 180 degree */
    }
    if(dx >= abs(dy))
    {
      return(4);          /* 181 - 225 degree */
    }
    return(5);            /* 226 - 270 degree */
  }
  
  if(dy >= 0)
  {
    if(abs(dx) >= dy)
    {
      return(0);          /*   0 -  45 degree */
    }
    return(1);            /*  46 -  90 degree */
  }

  if(abs(dx) >= abs(dy))
  {
      return(7);          /* 316 - 360 degree */
  }
  return(6);              /* 271 - 315 degree */
  
}  /* end p_calc_sector */



/* ---------------------------------
 * p_calc_angle_core
 * ---------------------------------
 * calculate the angle and return as sector (rounded to sectorsize)
 */
static inline gint
p_calc_angle_core(GapMorphWorkPoint *wp, gdouble dx, gdouble dy, gint num_sectors)
{
  gdouble angle_rad;
  gdouble sector;

  if(dx == 0)
  {
    if(dy < 0)
    {
      wp->angle_deg  = 270;
    }
    else
    {
      if(dy > 0)
      {
        wp->angle_deg  = 90;
      }
      else
      {
        wp->angle_deg  = 0;
      }
    }
  }
  else
  {
    gdouble adx;
    gdouble ady;

    adx = abs(dx);
    ady = abs(dy);

    angle_rad = atan(ady / adx);

    /* angle_rad = atan(abs(dy) / abs(dx)); */ /* this version crashes sometimes, dont know why ? */
    
     
    wp->angle_deg  = 180.0 * (angle_rad / G_PI);

    if(dx > 0)
    {
      if(dy < 0)
      {
        wp->angle_deg  = 360 - wp->angle_deg;
      }
    }
    else
    {
      if(dy > 0)
      {
        wp->angle_deg  = 180 - wp->angle_deg;
      }
      else
      {
        wp->angle_deg  += 180;
      }
    }
  }
  
  sector = (wp->angle_deg / 360.0) * num_sectors;
  return(CLAMP((gint)sector, 0 , (num_sectors-1)));
  
}   /* end p_calc_angle_core */


/* ---------------------------------
 * p_calc_angle
 * ---------------------------------
 * calculate the angle (rounded to sectorsize) and distance
 * between in_x/in_y and the workpoint
 */
static gint
p_calc_angle(GapMorphWorkPoint *wp, gint32 in_x, gint32 in_y)
{
  gdouble dx;
  gdouble dy;
  gdouble adx;
  gdouble ady;
  
  if(wp)
  {
    dx = in_x - wp->dst_x;
    dy = in_y - wp->dst_y;

    wp->sqr_dist = (dx * dx ) + (dy * dy);
    wp->dist = sqrt(wp->sqr_dist);

        adx = abs(dx);
        ady = abs(dy);
        if(adx > ady)
        {
          wp->xy_relation = (gint)(0.5 + (GAP_MORPH_XY_REL_FAKTOR * (ady / adx)));
        }
        else
        {
          if(dy != 0)
          {
            wp->xy_relation = (gint)(0.5 + (GAP_MORPH_XY_REL_FAKTOR * (adx / ady)));
          }
          else
          {
            wp->xy_relation = 0;
          }
        }

    return(p_calc_angle_core(wp, dx, dy, GAP_MORPH_8_SEKTORS));
  }
  return (0);

}   /* end p_calc_angle */


/* ---------------------------------
 * p_pixel_warp_core
 * ---------------------------------
 *  the movement vektor for in_x/in_y is calculated by mixing
 *  the movement vektors of the selected workpoints,
 *  weighted by invers distances of the workpoint to in_x/in_y
 *
 *  there are 2 strategies (FAST / QUALITY) for selecting relevant
 *  workpoints. 
 *  FAST strategy uses just the nearest workpoints in 8 sektors,
 *  QUALITY looks for all workpoints per sektor within a tolerance radius
 *  but discards points with similar angle and greater distance
 *
 *  in case there is no workpoint available
 *       the pixel is picked by simply scaling in_x/in_y to ssrc koords
 *
 */
static void
p_pixel_warp_core(GapMorphWarpCoreAPI *wcap
            , gint32        in_x
            , gint32        in_y
            , gdouble *out_pick_x
            , gdouble *out_pick_y
            )
{
  register GapMorphWorkPoint *wp;
  GapMorphWorkPoint *wp_sek;
  GapMorphWorkPoint *wp_sektor_tab[GAP_MORPH_8_SEKTORS];
  gdouble     tab_tol[GAP_MORPH_8_SEKTORS];
  
  /* index tables to access neigbour sectors */
  static gint tab_l1_idx[GAP_MORPH_8_SEKTORS] = {7,0,1,2,3,4,5,6};
  static gint tab_l2_idx[GAP_MORPH_8_SEKTORS] = {6,7,0,1,2,3,4,5};

  static gint tab_r1_idx[GAP_MORPH_8_SEKTORS] = {1,2,3,4,5,6,7,0};
  static gint tab_r2_idx[GAP_MORPH_8_SEKTORS] = {2,3,4,5,6,7,0,1};

  gdouble            vektor_x;  /* movement vektor for the current point at in_x/in_y */
  gdouble            vektor_y;
  gdouble            new_vektor_x;
  gdouble            new_vektor_y;
  gdouble            weight;
  gdouble            sum_inv_dist;
  gboolean           direct_hit_flag;
  gint32             sek_idx;
  gdouble            min_sqr_dist;
  gdouble            limit_sqr_dist;
  gdouble dx;
  gdouble dy;
  gdouble adx;
  gdouble ady;



  /* reset sektor tab */
  for(sek_idx=0; sek_idx < GAP_MORPH_8_SEKTORS; sek_idx++)
  {
    wp_sektor_tab[sek_idx] = NULL;
  }
  
  vektor_x = 0;
  vektor_y = 0;
  direct_hit_flag = FALSE;
  min_sqr_dist = wcap->sqr_affect_radius;


  /* loop1 calculate square distance, 
   * check for direct hits
   * and build sector table (nearest workpoints foreach sector
   */
  for(wp=wcap->wp_list; wp != NULL; wp = (GapMorphWorkPoint *)wp->next)
  {
    dx = in_x - wp->dst_x;
    dy = in_y - wp->dst_y;
    
    wp->sqr_dist =  (dx * dx ) + (dy * dy);
    wp->sek_idx = 0;
    
    if(wp->sqr_dist <= wcap->sqr_affect_radius)
    {
      if(wp->sqr_dist < 1)
      {
        new_vektor_x = (wp->src_x - wp->dst_x);
        new_vektor_y = (wp->src_y - wp->dst_y);

        if(direct_hit_flag)
        {
          vektor_x = (new_vektor_x + vektor_x) / 2.0;
          vektor_y = (new_vektor_y + vektor_y) / 2.0;
        }
        else
        {
          vektor_x = new_vektor_x;
          vektor_y = new_vektor_y;
        }
        direct_hit_flag = TRUE;
      }
      else
      {
        wp->is_alive = TRUE;

        /* sektor index 8 sectors */
        sek_idx = p_calc_sector(wp, dx, dy);
        wp->sek_idx = sek_idx;

        if((wp_sek = wp_sektor_tab[sek_idx]) != NULL)
        {
          /* there is already a workpoint for this sektor */
          if(wp->sqr_dist <= wp_sek->sqr_dist)
          {
            /* set current workpoint as the new nearest point for this sektor */
            wp_sektor_tab[sek_idx] = wp;
          }
          else
          {
            continue;
          }
        }
        else
        {
          /* found 1.st workpint for this sektor */
          wp_sektor_tab[sek_idx] = wp;
        }
        wp->next_sek = NULL;  /* init empty sektors list */
        if(wp->sqr_dist < min_sqr_dist)
        {
          min_sqr_dist = wp->sqr_dist;
        }

      }
    }
  }


  if(!direct_hit_flag)
  {
    GapMorphWorkPoint *wp_selected_list;

    sum_inv_dist = 0;
    wp_selected_list = NULL;
    

    /* calculate sector sqr_distance limit table for the 8 sektors */
    if(min_sqr_dist <= GAP_MORPH_NEAR_SQR_RADIUS)
    {
      limit_sqr_dist = min_sqr_dist * GAP_MORPH_MIN_TOL_FAKTOR;
      for(sek_idx = 0; sek_idx < GAP_MORPH_8_SEKTORS; sek_idx++)
      {
        tab_tol[sek_idx] = limit_sqr_dist;
      }
    }
    else
    {
      limit_sqr_dist = wcap->sqr_affect_radius;

      for(sek_idx = 0; sek_idx < GAP_MORPH_8_SEKTORS; sek_idx++)
      {
        wp_sek = wp_sektor_tab[sek_idx];

        if(wp_sek)
        {
          GapMorphWorkPoint *wp_l_sek;  /* left neigbour sektor */
          GapMorphWorkPoint *wp_r_sek;  /* right neigbour sektor */
          gdouble l_tol;
          gdouble r_tol;

          /* xy_relation is the representation of the angle
           * (is used to compare for similar angle in steps 10tiems finer than the 8 sektors.
           *  and is faster to calculate than the real angle)
           */
          adx = abs(in_x - wp_sek->dst_x);
          ady = abs(in_y - wp_sek->dst_y);
          if(adx > ady)
          {
            wp_sek->xy_relation = (gint)(0.5 + (GAP_MORPH_XY_REL_FAKTOR * (ady / adx)));
          }
          else
          {
            if(ady != 0)
            {
              wp_sek->xy_relation = (gint)(0.5 + (GAP_MORPH_XY_REL_FAKTOR * (adx / ady)));
            }
            else
            {
              wp_sek->xy_relation = 0;
            }
          }

          wp_l_sek = wp_sektor_tab[tab_l1_idx[sek_idx]];
          wp_r_sek = wp_sektor_tab[tab_r1_idx[sek_idx]];
          if(wp_l_sek == NULL)
          {
            wp_l_sek = wp_sektor_tab[tab_l2_idx[sek_idx]];
          }
          if(wp_r_sek == NULL)
          {
             wp_r_sek = wp_sektor_tab[tab_r2_idx[sek_idx]];
          }

          if(wp_l_sek)
          {
            if(wp_r_sek)
            {
              l_tol = wp_l_sek->sqr_dist * GAP_MORPH_TOL_FAKTOR;
              r_tol = wp_r_sek->sqr_dist * GAP_MORPH_TOL_FAKTOR;
              tab_tol[sek_idx] = MIN(MAX(l_tol, r_tol), (wp_sek->sqr_dist * GAP_MORPH_TOL_FAKTOR));
            }
            else
            {
              tab_tol[sek_idx] = wp_sek->sqr_dist * GAP_MORPH_TOL_FAKTOR;
            }
          }
          else
          {
            tab_tol[sek_idx] = wp_sek->sqr_dist * GAP_MORPH_TOL_FAKTOR;
          }
        }
        else
        {
          tab_tol[sek_idx] = limit_sqr_dist;
        }
      }

    }


    
    if(wcap->use_quality_wp_selection)
    {
      /* using the QUALITY workpoint selection strategy:
       * select all points per sector within the sektor specific tolerance
       * but discard those points that have same angle as another nearer point
       * in the same sektor
       */
      for(wp=wcap->wp_list; wp != NULL; wp = (GapMorphWorkPoint *)wp->next)
      {
        sek_idx = CLAMP(wp->sek_idx, 0, (GAP_MORPH_8_SEKTORS -1));

        if(wp->sqr_dist <= tab_tol[sek_idx])
        {
          gboolean sel_flag;
          
          sel_flag = TRUE;
          
          /* xy_relation is the representation of the angle
           * (is used to compare for similar angle in steps 10tiems finer than the 8 sektors.
           *  and is faster to calculate than the real angle)
           */
          adx = abs(in_x - wp->dst_x);
          ady = abs(in_y - wp->dst_y);
          if(adx > ady)
          {
            wp->xy_relation = (gint)(0.5 + (GAP_MORPH_XY_REL_FAKTOR * (ady / adx)));
          }
          else
          {
            if(ady != 0)
            {
              wp->xy_relation = (gint)(0.5 + (GAP_MORPH_XY_REL_FAKTOR * (adx / ady)));
            }
            else
            {
              wp->xy_relation = 0;
            }
          }
          
          
          for(wp_sek = wp_sektor_tab[sek_idx]; wp_sek != NULL; wp_sek = (GapMorphWorkPoint *)wp_sek->next_sek)
          {
            if(wp_sek == wp)
            {
              sel_flag = FALSE;  /* dont add the same point twice to the sektors list */
              continue;
            }
            if((wp_sek->is_alive) 
            && (wp->xy_relation == wp_sek->xy_relation))  /* check for (nearly) same angle */
            {
              if(wp->sqr_dist <  wp_sek->sqr_dist)
              {
                /* discard the (farer) point from the sektor list by flag
                 * (is faster than remove the list element)
                 */
                wp_sek->is_alive = FALSE;
              }
              else
              {
                sel_flag = FALSE;
              }
              break;
            }
          }
          
          if(sel_flag)
          {
            /* add wp to the sektor list */
            wp->next_sek = wp_sektor_tab[sek_idx];
            wp_sektor_tab[sek_idx] = wp;
          }
        }
      }
    }
    
    /* build selection list by merging all 8 sektor sublists where workpoint is still alive
     * for the FAST strategy each sektor sublist ha s just one (the nearest) or zero elements
     * the QUALITY strategy can have more points per sektor within the sektor specific tolerance.
     */
    for(sek_idx = 0; sek_idx < GAP_MORPH_8_SEKTORS; sek_idx++)
    {
      for(wp=wp_sektor_tab[sek_idx]; wp != NULL; wp = (GapMorphWorkPoint *)wp->next_sek)
      {
        if((wp->is_alive) 
        && (wp->sqr_dist <= tab_tol[sek_idx]))
        {
          wp->dist = sqrt(wp->sqr_dist);
          if(wcap->printf_flag)
          {
             /* this is a debug feature for tuning the warp algorithm */
             printf("sek: %02d, ang: %.2f, dist:%.3f, sqr_dist: %.3f TAB_TOL:%.3f SELECTED\n"
                   ,(int)sek_idx
                   ,(float)wp->angle_deg
                   ,(float)wp->dist
                   ,(float)wp->sqr_dist
                   ,(float)tab_tol[sek_idx]
                   );
          }
          
          /* add point to selection list */
          wp->next_selected = wp_selected_list;
          wp_selected_list = wp;

          wp->inv_dist = 1.0 / wp->dist;
          sum_inv_dist += wp->inv_dist;
        }
      }
    }    /* end  for(sek_idx..) */
    

    /* build result vektor as weightened mix of all selected workpoint vektors */
    for(wp = wp_selected_list; wp != NULL; wp = wp->next_selected)
    {
      wp->gravity = 1.0;
      if(wcap->use_gravity)
      {
        wp->gravity -= pow((MIN(wp->sqr_dist,wcap->sqr_affect_radius) / wcap->sqr_affect_radius) , wcap->gravity_intensity);
      }

      new_vektor_x = (wp->src_x - wp->dst_x);
      new_vektor_y = (wp->src_y - wp->dst_y);

      weight = (wp->inv_dist / sum_inv_dist) * wp->gravity;
      vektor_x += (new_vektor_x * weight);
      vektor_y += (new_vektor_y * weight);
      
      if(wcap->printf_flag)
      {
        wp->warp_weight = weight;
        printf("wp->src_x/y:  %.3f / %.3f  wp->dst_x/y: %.3f / %.3f sek_idx: %d xy_rel:%d dist:%.3f weight: %.7f vek: %.3f / %.3f \n"
              , (float)wp->src_x
              , (float)wp->src_y
              , (float)wp->dst_x
              , (float)wp->dst_y
              , (int)  wp->sek_idx
              , (int)  wp->xy_relation
              , (float)wp->dist
              , (float)weight
              , (float)vektor_x
              , (float)vektor_y
              );
      }
    }
  }  /* end direct_hit_flag */

  *out_pick_x = (in_x + vektor_x) * wcap->scale_x;
  *out_pick_y = (in_y + vektor_y) * wcap->scale_y;

}  /* end p_pixel_warp_core */






/* -----------------------------------
 * gap_morph_exec_get_warp_pick_koords
 * -----------------------------------
 * this procedure is used for the show option (a debug feature in the GUI)
 * it is not used for the rendering, but should give an idea
 * how selecting of relevant workpoints will work and set
 * the weight values for all workpoints for later display in the GUI
 * This was the major quality tuning tool for the development of the warp
 * algorithm
 */
void
gap_morph_exec_get_warp_pick_koords(GapMorphWorkPoint *wp_list
                                      , gint32        in_x
                                      , gint32        in_y
                                      , gdouble       scale_x
                                      , gdouble       scale_y
                                      , gboolean      use_quality_wp_selection
                                      , gboolean      use_gravity
                                      , gdouble       gravity_intensity
                                      , gdouble       affect_radius
                                      , gdouble *out_pick_x
                                      , gdouble *out_pick_y
                                      )
{
  GapMorphWarpCoreAPI  wcap_struct;
  GapMorphWarpCoreAPI *wcap;
  GapMorphWorkPoint *wp;
  gdouble            pick_x;
  gdouble            pick_y;
  gdouble            sum_pick_x;
  gdouble            sum_pick_y;
  gint xx;
  gint yy;
  gint nn;


  wcap = &wcap_struct;
  
  wcap->sqr_affect_radius = affect_radius * affect_radius;
  wcap->wp_list = wp_list;
  wcap->scale_x = scale_x;
  wcap->scale_y = scale_y;
  wcap->use_gravity = use_gravity;
  wcap->gravity_intensity = gravity_intensity;
  wcap->printf_flag = TRUE;
  wcap->use_quality_wp_selection = use_quality_wp_selection;
  
  
  for(wp=wp_list; wp != NULL; wp = (GapMorphWorkPoint *)wp->next)
  {
    wp->warp_weight = -1;  /* for debug display in warp_info_label */
    wp->dist = p_calc_angle(wp, in_x, in_y); /* for debug display in warp_info_label */
    if(wp->dist < 1)
    {
      wp->warp_weight = -4;  /* for debug display in warp_info_label */
    }
  }

  printf("==== scale: %.2f / %.2f  in_x:%02d in_y:%02d == \n"
     , (float)scale_x
     , (float)scale_y
     , (int)in_x
     , (int)in_y
     );
  nn = 0;
  sum_pick_x = 0;
  sum_pick_y = 0;

#ifdef   GAP_MORPH_USE_SIMPLE_PIXEL_WARP
  p_pixel_warp_core(wcap
                   ,in_x
                   ,in_y
                   ,&pick_x
                   ,&pick_y
                   );


#else
  for(xx=MAX(0,in_x -3); xx <= in_x +3; xx+=3)
  {
    wcap->printf_flag = FALSE;
    for(yy=MAX(0,in_y -3); yy <= in_y +3; yy+=3)
    {
      p_pixel_warp_core(wcap
                       ,xx
                       ,yy
                       ,&pick_x
                       ,&pick_y
                       );
       sum_pick_x += pick_x;                   
       sum_pick_y += pick_y;
       nn++;                   
    }
  }
  
  pick_x = sum_pick_x / (gdouble)nn;
  pick_y = sum_pick_y / (gdouble)nn;

  /* this call is for reset wp->angel and other things to the center pick point */
  wcap->printf_flag = TRUE;
  p_pixel_warp_core(wcap
                   ,in_x
                   ,in_y
                   ,out_pick_x
                   ,out_pick_y
                   );

#endif



  *out_pick_x = pick_x;
  *out_pick_y = pick_y;

  printf("pick_x/y :  %.3f / %.3f\n"
            , (float)*out_pick_x
            , (float)*out_pick_y
            );

}  /* end gap_morph_exec_get_warp_pick_koords */



/* ---------------------------------
 * p_pixel_warp_pick
 * ---------------------------------
 */
static void
p_pixel_warp_pick(GapMorphWarpCoreAPI *wcap
            , gint32        in_x
            , gint32        in_y
            , gint          set_idx
            , gdouble      *out_pick_x
            , gdouble      *out_pick_y
            )
{
  gdouble            pick_x;
  gdouble            pick_y;
  gdouble            sum_pick_x;
  gdouble            sum_pick_y;
  gint xx;
  gint yy;
  gint nn;
  GapMorphExePickCache *pcp;

#ifdef GAP_MORPH_USE_SIMPLE_PIXEL_WARP
        p_pixel_warp_core(wcap
                       ,in_x
                       ,in_y
                       ,out_pick_x
                       ,out_pick_y
                       );

#else
  /* the implementation of the   p_pixel_warp_core
   * produces unwanted hard edges at those places where
   * the selection of relevant near workpoints
   * switches to another set of workpoints with
   * signifikant different movement settings.
   * To comensate this unwanted effect, we do multiple picks 
   * and use the average pick koordinates.
   * We use gloabl_pick_cache to reduce the effective
   * number of p_pixel_warp_core calls
   */
  nn = 0;
  sum_pick_x = 0;                      
  sum_pick_y = 0;                      
  for(xx=MAX(0,in_x -3); xx <= in_x +3; xx+=3)
  {
    for(yy=MAX(0,in_y -3); yy <= in_y +3; yy+=3)
    {
      gint ii;
      
      for(ii=0; ii < GAP_MORPH_PCK_CACHE_SIZE; ii++)
      {
        pcp = &gloabl_pick_cache[ii][set_idx];
        if ((pcp->valid)
        &&  (pcp->xx == xx)
        &&  (pcp->yy == yy))
        {
          pick_x = pcp->pick_x;
          pick_y = pcp->pick_y;
          
          /* printf("** CACHE HIT\n"); */
          break;
        }
      }
      if(ii == GAP_MORPH_PCK_CACHE_SIZE)
      {
        /* printf("** must do pixel warp\n"); */
        p_pixel_warp_core(wcap
                       ,xx
                       ,yy
                       ,&pick_x
                       ,&pick_y
                       );
         /* save pick koords in cache table */
         pcp = &gloabl_pick_cache[gloabl_pick_cache_idx[set_idx]][set_idx];

         pcp->xx     = xx;
         pcp->yy     = yy;
         pcp->pick_x = pick_x;
         pcp->pick_y = pick_y;
         pcp->valid  = TRUE;
         gloabl_pick_cache_idx[set_idx]++;
         if(gloabl_pick_cache_idx[set_idx] >= GAP_MORPH_PCK_CACHE_SIZE)
         {
           gloabl_pick_cache_idx[set_idx] = 0;
         }
       }
       sum_pick_x += (pick_x - ((in_x - xx) * wcap->scale_x));
       sum_pick_y += (pick_y - ((in_y - yy) * wcap->scale_y));
       nn++;                   
    }
  }
  *out_pick_x = sum_pick_x / (gdouble)nn;
  *out_pick_y = sum_pick_y / (gdouble)nn;

#endif
}  /* end p_pixel_warp_pick */


/* ---------------------------------
 * p_pixel_warp_multipick
 * ---------------------------------
 */
static void
p_pixel_warp_multipick(GapMorphWarpCoreAPI *wcap_1
                      ,GapMorphWarpCoreAPI *wcap_2
                      ,gdouble wp_mix_factor
                      , gint32        in_x
                      , gint32        in_y
                      , gdouble      *out_pick_x
                      , gdouble      *out_pick_y
                      )
{
  gdouble            pick_x_1;
  gdouble            pick_y_1;
  gdouble            pick_x_2;
  gdouble            pick_y_2;

  pick_x_1 = 0;
  pick_y_1 = 0;
  pick_x_2 = 0;
  pick_y_2 = 0;
  
  if(wp_mix_factor != 0.0)
  {
    p_pixel_warp_pick(wcap_1
          , in_x
          , in_y
          , 0                      /* set_idx */
          , &pick_x_1
          , &pick_y_1
          );
  }
  
  if(wp_mix_factor != 1.0)
  {
    p_pixel_warp_pick(wcap_1
          , in_x
          , in_y
          , 1                      /* set_idx */
          , &pick_x_2
          , &pick_y_2
          );
  }
  
  *out_pick_x = (pick_x_1 * wp_mix_factor) + (pick_x_2 * (1 - wp_mix_factor));
  *out_pick_y = (pick_y_1 * wp_mix_factor) + (pick_y_2 * (1 - wp_mix_factor));

}  /* end p_pixel_warp_multipick */


/* ---------------------------------
 * p_calculate_work_point_movement
 * ---------------------------------
 * return the newly created workpoint list
 * the list contains duplicates of the input workpoint master_list
 * with transformed point koordinates, according to current movement
 * step and scaling.
 * 
 * forward_move == TRUE:
 *   dup->fdest_x,y := master->fdest_x,y
 *   dup->orsc_x,y  := master->orsc_x,y
 *   dup->src_x,y   := scale (master->osrc_x,y)
 *   dup->dst_x,y   := linear_advance(master->orsc_x,y  --> master->fdest_x,y)
 *
 * forward_move == FALSE:
 *   dup->fdest_x,y := master->osrc_x,y
 *   dup->orsc_x,y  := master->fdest_x,y
 *   dup->src_x,y   := scale (master->fdest_x,y)
 *   dup->dst_x,y   := linear_advance(master->fdest_x,y  --> master->orsc_x,y)
 *
 */
GapMorphWorkPoint *
p_calculate_work_point_movement(gdouble total_steps
                               ,gdouble current_step
                               ,GapMorphWorkPoint *master_list
                               ,gint32 src_layer_id
                               ,gint32 dst_layer_id
                               ,gint32 curr_width
                               ,gint32 curr_height
                               ,gboolean forward_move
                               )
{
  GapMorphWorkPoint *wp_list;
  GapMorphWorkPoint *wp_elem_orig;
  GapMorphWorkPoint *wp_elem_dup;
  GapMorphWorkPoint *wp_elem_prev;
  gdouble            scalex;
  gdouble            scaley;



  if(forward_move)
  {
    scalex = (gdouble)curr_width  / (gdouble)gimp_drawable_width(src_layer_id);
    scaley = (gdouble)curr_height / (gdouble)gimp_drawable_height(src_layer_id);
  }
  else
  {
    scalex = (gdouble)curr_width  / (gdouble)gimp_drawable_width(dst_layer_id);
    scaley = (gdouble)curr_height / (gdouble)gimp_drawable_height(dst_layer_id);
  }

  if(gap_debug)
  {
    gint32  countPoints;

    countPoints = 0;
    for(wp_elem_orig = master_list; wp_elem_orig != NULL; wp_elem_orig = (GapMorphWorkPoint *)wp_elem_orig->next)
    {
      countPoints++;
    }
    printf ("p_calculate_work_point_movement: START total_steps:%.2f current_step:%.2f forward_move:%d scalex:%.3f scaley:%.3f master_list:%d countPoints:%d\n"
      ,(float)total_steps
      ,(float)current_step
      ,(int)forward_move
      ,(float)scalex
      ,(float)scaley
      ,(int)master_list
      ,(int)countPoints
      );
  }
  
  wp_list = NULL;
  wp_elem_prev = NULL;
  
  
  for(wp_elem_orig = master_list; wp_elem_orig != NULL; wp_elem_orig = (GapMorphWorkPoint *)wp_elem_orig->next)
  {
    wp_elem_dup = g_new(GapMorphWorkPoint, 1);
    wp_elem_dup->next = NULL;
    if(forward_move)
    {
      wp_elem_dup->fdst_x = wp_elem_orig->fdst_x;
      wp_elem_dup->fdst_y = wp_elem_orig->fdst_y;
      wp_elem_dup->osrc_x = wp_elem_orig->osrc_x;
      wp_elem_dup->osrc_y = wp_elem_orig->osrc_y;
      wp_elem_dup->dst_x = p_linear_advance((gdouble)total_steps
                                ,(gdouble)current_step
                                ,(gdouble)wp_elem_orig->osrc_x
                                ,(gdouble)wp_elem_orig->fdst_x
                                );
      wp_elem_dup->dst_y = p_linear_advance((gdouble)total_steps
                                ,(gdouble)current_step
                                ,(gdouble)wp_elem_orig->osrc_y
                                ,(gdouble)wp_elem_orig->fdst_y
                                );
      wp_elem_dup->src_x = scalex * wp_elem_orig->osrc_x;
      wp_elem_dup->src_y = scaley * wp_elem_orig->osrc_y;
    }
    else
    {
      wp_elem_dup->fdst_x = wp_elem_orig->osrc_x;
      wp_elem_dup->fdst_y = wp_elem_orig->osrc_y;
      wp_elem_dup->osrc_x = wp_elem_orig->fdst_x;
      wp_elem_dup->osrc_y = wp_elem_orig->fdst_y;
      wp_elem_dup->dst_x = p_linear_advance((gdouble)total_steps
                                ,(gdouble)(total_steps - current_step)
                                ,(gdouble)wp_elem_orig->fdst_x
                                ,(gdouble)wp_elem_orig->osrc_x
                                );
      wp_elem_dup->dst_y = p_linear_advance((gdouble)total_steps
                                ,(gdouble)(total_steps - current_step)
                                ,(gdouble)wp_elem_orig->fdst_y
                                ,(gdouble)wp_elem_orig->osrc_y
                                );
      wp_elem_dup->src_x = scalex * wp_elem_orig->fdst_x;
      wp_elem_dup->src_y = scaley * wp_elem_orig->fdst_y;
    }
    

    if(gap_debug)
    {
      printf("WP_DUP: osrc: %03.2f / %03.2f   fdst: %03.2f / %03.2f src: %03.2f / %03.2f   dst: %03.2f / %03.2f\n"
          ,(float)wp_elem_dup->osrc_x
          ,(float)wp_elem_dup->osrc_y
          ,(float)wp_elem_dup->fdst_x
          ,(float)wp_elem_dup->fdst_y
          ,(float)wp_elem_dup->src_x
          ,(float)wp_elem_dup->src_y
          ,(float)wp_elem_dup->dst_x
          ,(float)wp_elem_dup->dst_y
          );
    }



    if(wp_elem_prev == NULL)
    {
      wp_list = wp_elem_dup;
    }
    else
    {
      wp_elem_prev->next = wp_elem_dup;
    }
    wp_elem_prev = wp_elem_dup;
  }
  
  return(wp_list);
  
}  /* end p_calculate_work_point_movement */


/* ---------------------------------
 * p_layer_warp_move
 * ---------------------------------
 */
static void
p_layer_warp_move (GapMorphWorkPoint     *wp_list_1
                  ,GapMorphWorkPoint     *wp_list_2
                  , gint32                src_layer_id
                  , gint32                dst_layer_id
                  , GapMorphGlobalParams *mgpp
                  , GapMorphWarpCoreAPI  *p1
                  , GapMorphWarpCoreAPI  *p2
                  , gdouble               wp_mix_factor
                  )
{
  GapMorphWarpCoreAPI  wcap_struct_1;
  GapMorphWarpCoreAPI  wcap_struct_2;
  GapMorphWarpCoreAPI *wcap_1;
  GapMorphWarpCoreAPI *wcap_2;
  GimpDrawable *src_drawable;
  GimpDrawable *dst_drawable;
  GimpPixelFetcher *src_pixfet;
  GimpPixelRgn    dstPR;
  gpointer        pr;
  guchar         *pixel_ptr;
  guint32         l_row;
  guint32         l_col;
  gdouble         scale_x;
  gdouble         scale_y;
  gdouble         l_max_progress;
  gdouble         l_progress;
  gint ii;
  gint set_idx;

  wcap_1 = &wcap_struct_1;
  wcap_2 = &wcap_struct_2;

  wcap_1->wp_list = wp_list_1;
  wcap_2->wp_list = wp_list_2;
  
  wcap_1->sqr_affect_radius = mgpp->affect_radius * mgpp->affect_radius;
  wcap_1->use_gravity = mgpp->use_gravity;
  wcap_1->gravity_intensity = mgpp->gravity_intensity;
  if(p1)
  {
    wcap_1->sqr_affect_radius = p1->affect_radius * p1->affect_radius;
    wcap_1->use_gravity = p1->use_gravity;
    wcap_1->gravity_intensity = p1->gravity_intensity;
  }
  
  wcap_2->sqr_affect_radius = mgpp->affect_radius * mgpp->affect_radius;
  wcap_2->use_gravity = mgpp->use_gravity;
  wcap_2->gravity_intensity = mgpp->gravity_intensity;
  if(p2)
  {
    wcap_2->sqr_affect_radius = p2->sqr_affect_radius * p2->sqr_affect_radius;
    wcap_2->use_gravity = p2->use_gravity;
    wcap_2->gravity_intensity = p2->gravity_intensity;
  }
  

  wcap_1->printf_flag = FALSE;
  wcap_2->printf_flag = FALSE;

  wcap_1->use_quality_wp_selection = mgpp->use_quality_wp_selection;
  wcap_2->use_quality_wp_selection = mgpp->use_quality_wp_selection;
  
  /* clear the global cache for picking koordinates */
  for(set_idx=0; set_idx < 2; set_idx++)
  {
    for(ii=0; ii < GAP_MORPH_PCK_CACHE_SIZE; ii++)
    {
       gloabl_pick_cache[ii][set_idx].valid = FALSE;
    }
    gloabl_pick_cache_idx[set_idx] = 0;
  }

  src_drawable = gimp_drawable_get (src_layer_id);
  dst_drawable = gimp_drawable_get (dst_layer_id);

  if(src_drawable == NULL)                   { return; }
  if(dst_drawable == NULL)                   { return; }
  if(src_drawable->bpp != dst_drawable->bpp) { return; }
  if((src_drawable->bpp != 4)  && (src_drawable->bpp != 2))
  { 
    return;
  }


  /* if src and dst size not equal: caluclate scaling factors x/y */
  scale_x = src_drawable->width / MAX(1,dst_drawable->width);
  scale_y = src_drawable->height / MAX(1,dst_drawable->height);

  wcap_1->scale_x = scale_x;
  wcap_2->scale_x = scale_x;
  wcap_1->scale_y = scale_y;
  wcap_2->scale_y = scale_y;

  /* init Pixel Fetcher for source drawable  */
  src_pixfet = gimp_pixel_fetcher_new (src_drawable, FALSE /*shadow*/);
  if(src_drawable == NULL)                   { return; }

  
  /* init Pixel Region  */
  gimp_pixel_rgn_init (&dstPR, dst_drawable
                      , 0
                      , 0
                      , dst_drawable->width
                      , dst_drawable->height
                      , TRUE      /* dirty */
                      , TRUE      /* shadow */
                       );

  l_max_progress = dst_drawable->width * dst_drawable->height;
  l_progress = 0;


  for (pr = gimp_pixel_rgns_register (1, &dstPR);
       pr != NULL;
       pr = gimp_pixel_rgns_process (pr))
  {
     guint x;
     guint y;
     guchar *dest;

     dest = dstPR.data;
     for (y = 0; y < dstPR.h; y++)
     {
        pixel_ptr = dest;
        l_row = dstPR.y + y;
        
        for (x = 0; x < dstPR.w; x++)
        {
           gdouble            pick_x;
           gdouble            pick_y;
           
           l_col = dstPR.x + x;
           if(mgpp->have_workpointsets)
           {
             /* pick based on 2 sets of workpoints */
             p_pixel_warp_multipick(wcap_1  /*  list1 */
                   , wcap_2                 /*  list2 */
                   , wp_mix_factor
                   , l_col
                   , l_row
                   , &pick_x
                   , &pick_y
                   );
           }
           else
           {
             /* pick based on a single set of workpoints */
             p_pixel_warp_pick(wcap_1
                   , l_col
                   , l_row
                   , 0                      /* set_idx */
                   , &pick_x
                   , &pick_y
                   );
           }
           p_bilinear_get_pixel (src_pixfet
                                ,src_drawable
                                ,pick_x
                                ,pick_y
                                ,pixel_ptr
                                ,dst_drawable->bpp);
           pixel_ptr += dstPR.bpp;
        }
        dest += dstPR.rowstride;
     }
     
     if(mgpp->do_progress)
     {
       gdouble l_total_progress;
       
       l_progress += (dstPR.w * dstPR.h);
       l_total_progress = mgpp->master_progress 
                           + (mgpp->layer_progress_step * (l_progress /l_max_progress));

       /*
        * if)gap_debug)
        * {
        *   printf("Progress: mgpp->master_progress:%f mgpp->layer_progress_step:%f l_total_progress:%f\n"
        *    ,(float)mgpp->master_progress
        *    ,(float)mgpp->layer_progress_step
        *    ,(float)l_total_progress
        *    );
        * }
        */
        
       if(mgpp->progress_callback_fptr == NULL)
       {
         gimp_progress_update(l_total_progress);
       }
       else
       {
         (*mgpp->progress_callback_fptr)(l_total_progress, mgpp->callback_data_ptr);
       }
     }

  }

  gimp_drawable_flush (dst_drawable);
  gimp_drawable_merge_shadow (dst_drawable->drawable_id, TRUE);
  gimp_drawable_update (dst_layer_id
                      , 0
                      , 0
                      , dst_drawable->width
                      , dst_drawable->height
                      );
  gimp_pixel_fetcher_destroy (src_pixfet);

  gimp_drawable_detach(src_drawable);
  gimp_drawable_detach(dst_drawable);
}   /* end p_layer_warp_move */

/* ---------------------------------
 * p_mix_layers
 * ---------------------------------
 * create a layer by mixing bg_layer_id and top_layer_id
 */
static gint32
p_mix_layers (gint32  curr_image_id
                  ,gint32  bg_layer_id
                  ,gint32  top_layer_id
                  ,gint32  curr_width
                  ,gint32  curr_height
                  ,gdouble curr_mix_factor  /* 0.0 <= mix <= 1.0 */
                  )
{
  gint32        dst_layer_id;
  GimpDrawable *top_drawable;
  GimpDrawable *bg_drawable;
  GimpDrawable *dst_drawable;
  GimpPixelRgn    dstPR;
  GimpPixelRgn    topPR;
  GimpPixelRgn    bgPR;
  gpointer        pr;

  if(gap_debug)
  {
    printf("p_mix_layers START curr_mix_factor: %f\n"
      ,(float)curr_mix_factor
      );
  }

  top_drawable = gimp_drawable_get (top_layer_id);
  bg_drawable  = gimp_drawable_get (bg_layer_id);

  if(top_drawable == NULL)                   { return (-1); }
  if(bg_drawable == NULL)                    { return (-1); }
  if(top_drawable->bpp < 3)
  {
    dst_layer_id = gimp_layer_new(curr_image_id, "morph_tween"
                               , curr_width
                               , curr_height
                               , GIMP_GRAYA_IMAGE
                               , 100.0      /* full opaque */
                               , GIMP_NORMAL_MODE
                               );
  }
  else
  {
    dst_layer_id = gimp_layer_new(curr_image_id, "morph_tween"
                               , curr_width
                               , curr_height
                               , GIMP_RGBA_IMAGE
                               , 100.0      /* full opaque */
                               , GIMP_NORMAL_MODE
                               );
  }
  

  gimp_image_add_layer(curr_image_id, dst_layer_id, 0);


  dst_drawable = gimp_drawable_get (dst_layer_id);
  if(dst_drawable == NULL)                   { return (-1); }

  if(gap_debug)
  {
    printf("p_mix_layers dst_layer_id: %d bpp:%d  top_layer_id:%d bpp:%d  bg_layer_id:%d bpp:%d\n"
      ,(int)dst_layer_id
      ,(int)dst_drawable->bpp
      ,(int)top_layer_id
      ,(int)top_drawable->bpp
      ,(int)bg_layer_id
      ,(int)bg_drawable->bpp
      );
  }


  
  /* init Pixel Region  */
  gimp_pixel_rgn_init (&dstPR, dst_drawable
                      , 0
                      , 0
                      , dst_drawable->width
                      , dst_drawable->height
                      , TRUE      /* dirty */
                      , TRUE      /* shadow */
                       );
  gimp_pixel_rgn_init (&topPR, top_drawable
                      , 0
                      , 0
                      , top_drawable->width
                      , top_drawable->height
                      , FALSE      /* dirty */
                      , FALSE      /* shadow */
                       );
  gimp_pixel_rgn_init (&bgPR, bg_drawable
                      , 0
                      , 0
                      , bg_drawable->width
                      , bg_drawable->height
                      , FALSE      /* dirty */
                      , FALSE      /* shadow */
                       );

  for (pr = gimp_pixel_rgns_register (3, &dstPR, &topPR, &bgPR);
       pr != NULL;
       pr = gimp_pixel_rgns_process (pr))
  {
     guint x;
     guint y;
     guchar *dest;
     guchar *top;
     guchar *bg;
     guchar         *pixel_ptr;
     guchar         *top_ptr;
     guchar         *bg_ptr;

     dest = dstPR.data;
     top  = topPR.data;
     bg   = bgPR.data;
     for (y = 0; y < dstPR.h; y++)
     {
        pixel_ptr = dest;
        top_ptr = top;
        bg_ptr = bg;
        
        for (x = 0; x < dstPR.rowstride ; x++)
        {
           gdouble val;
           
           val = GAP_BASE_MIX_VALUE(curr_mix_factor, (gdouble)(*bg_ptr), (gdouble)(*top_ptr));
           *pixel_ptr = (guchar)val;
           
           pixel_ptr++;
           top_ptr++;
           bg_ptr++;
        }
        dest += dstPR.rowstride;
        top  += topPR.rowstride;
        bg   += bgPR.rowstride;
     }
  }

  gimp_drawable_flush (dst_drawable);
  gimp_drawable_merge_shadow (dst_drawable->drawable_id, TRUE);
  gimp_drawable_update (dst_layer_id
                      , 0
                      , 0
                      , dst_drawable->width
                      , dst_drawable->height
                      );

  gimp_drawable_detach(bg_drawable);
  gimp_drawable_detach(top_drawable);
  gimp_drawable_detach(dst_drawable);

  return(dst_layer_id);

}   /* end p_mix_layers */

/* ---------------------------------
 * p_create_morph_tween_frame
 * ---------------------------------
 * return the newly created tween morphed layer
 */
static gint32
p_create_morph_tween_frame(gint32 total_steps
                          ,gint32 current_step
                          ,GapMorphGlobalParams *mgpp
                          ,GapMorphExeLayerstack *mlayers
                          )
{
   GimpDrawable *dst_drawable;
   gint32  curr_image_id;  /* a temp image of current layer size */
   gint32  bg_layer_id;
   gint32  top_layer_id;
   gint32  merged_layer_id;
   gint32  curr_width;
   gint32  curr_height;
   gdouble curr_opacity;
   GapMorphWorkPoint *curr_wp_list_1;
   GapMorphWorkPoint *curr_wp_list_2;
   GapMorphWarpCoreAPI *wp_set_1;
   GapMorphWarpCoreAPI *wp_set_2;
   gint32 src_layer_id;
   gint32 dst_layer_id;
   gboolean forward_move;
   gdouble  wp_mix_factor;
   
   forward_move = TRUE;
   src_layer_id = mgpp->osrc_layer_id;
   dst_layer_id = mgpp->fdst_layer_id;

   wp_set_1 = NULL;
   wp_set_2 = NULL;
   wp_mix_factor = 1.0;

   if(gap_debug)
   {
     printf("p_create_morph_tween_frame START total_steps:%d  current_step:%d\n"
            " mgpp->create_tween_layers:%d\n"
            " mgpp->have_workpointsets:%d\n"
            " mlayers->available_wp_sets:%d\n"
            " mgpp->render_mode:%d\n"
       ,(int)total_steps
       ,(int)current_step
       ,(int)mgpp->create_tween_layers
       ,(int)mgpp->have_workpointsets
       ,(int)mlayers->available_wp_sets
       ,(int)mgpp->render_mode
       );
   }

   if(mgpp->create_tween_layers)
   {
     /* size of the new frame */
     curr_width = p_linear_advance((gdouble)total_steps
                                  ,(gdouble)current_step
                                  ,(gdouble)gimp_drawable_width(src_layer_id)
                                  ,(gdouble)gimp_drawable_width(dst_layer_id)
                                  );
     curr_height = p_linear_advance((gdouble)total_steps
                                  ,(gdouble)current_step
                                  ,(gdouble)gimp_drawable_height(src_layer_id)
                                  ,(gdouble)gimp_drawable_height(dst_layer_id)
                                  );
   }
   else
   {
     gint ii;
     
     ii = mlayers->dst1_idx - (current_step -1);
     
     /* operate on existing destination layers */
     dst_layer_id = mlayers->dst_layers[ii];
     curr_width = gimp_drawable_width(dst_layer_id);
     curr_height = gimp_drawable_height(dst_layer_id);
     
     ii = p_linear_advance((gdouble)total_steps
                                  ,(gdouble)current_step
                                  ,(gdouble)mlayers->src1_idx
                                  ,(gdouble)mlayers->src2_idx
                                  );

     /* in this case also operate with changing source layers (if there are more than one) */
     src_layer_id = mlayers->src_layers[ii];
   }

   if((mgpp->have_workpointsets)
   && (mlayers->available_wp_sets > 1))
   {
     gint wps_idx;
     gint wps_idx_2;
     gdouble wp_stepspeed;
     gdouble ref;
     
     wp_stepspeed = (gdouble)(mlayers->available_wp_sets -1) / (gdouble)MIN((total_steps -1),1);
     ref = (gdouble)((gdouble)(current_step -1) * wp_stepspeed);
     wps_idx = (gint)ref;
     wps_idx_2 = MIN(wps_idx+1, (mlayers->available_wp_sets -1));

     wp_set_1 = mlayers->tab_wp_sets[wps_idx];
     wp_set_2 = mlayers->tab_wp_sets[wps_idx_2];
     wp_mix_factor = ref - (gdouble)wps_idx;
   }

   dst_drawable = gimp_drawable_get (dst_layer_id);

   if(gap_debug)
   {
     printf("p_create_morph_tween_frame dst_layer_id:%d, bpp:%d\n"
        ,(int)dst_layer_id
        ,(int)dst_drawable->bpp
        );
   }   

   /* add empty BG layer */
   if(dst_drawable->bpp < 3)
   {
     /* create the tween frame image */
     curr_image_id = gimp_image_new(curr_width, curr_height, GIMP_GRAY);

     bg_layer_id = gimp_layer_new(curr_image_id, "bg_morph_layer"
                               , curr_width
                               , curr_height
                               , GIMP_GRAYA_IMAGE
                               , 100.0      /* full opaque */
                               , GIMP_NORMAL_MODE
                               );
   }
   else
   {
     /* create the tween frame image */
     curr_image_id = gimp_image_new(curr_width, curr_height, GIMP_RGB);
     bg_layer_id = gimp_layer_new(curr_image_id, "bg_morph_layer"
                               , curr_width
                               , curr_height
                               , GIMP_RGBA_IMAGE
                               , 100.0      /* full opaque */
                               , GIMP_NORMAL_MODE
                               );
   }
   gimp_image_add_layer(curr_image_id, bg_layer_id, 0);

   /* add empty TOP layer */
   curr_opacity = p_linear_advance((gdouble)total_steps
                                ,(gdouble)current_step
                                ,(gdouble)0.0
                                ,(gdouble)100.0
                                );
   if(dst_drawable->bpp < 3)
   {
     top_layer_id = gimp_layer_new(curr_image_id, "top_morph_layer"
                               , curr_width
                               , curr_height
                               , GIMP_GRAYA_IMAGE
                               , curr_opacity
                               , GIMP_NORMAL_MODE
                               );
   }
   else
   {
     top_layer_id = gimp_layer_new(curr_image_id, "top_morph_layer"
                               , curr_width
                               , curr_height
                               , GIMP_RGBA_IMAGE
                               , curr_opacity
                               , GIMP_NORMAL_MODE
                               );
   }
   gimp_image_add_layer(curr_image_id, top_layer_id, 0);


   if(!mgpp->render_mode == GAP_MORPH_RENDER_MODE_WARP)
   {
     if((wp_set_1) && (wp_set_2))
     {
       /* multi workpoint settings for src_layer */
       curr_wp_list_1 = p_calculate_work_point_movement((gdouble)total_steps
                                                     ,(gdouble)current_step
                                                     ,wp_set_1->wp_list
                                                     ,mgpp->osrc_layer_id
                                                     ,mgpp->fdst_layer_id
                                                     ,curr_width
                                                     ,curr_height
                                                     ,forward_move
                                                     );

       curr_wp_list_2 = p_calculate_work_point_movement((gdouble)total_steps
                                                     ,(gdouble)current_step
                                                     ,wp_set_2->wp_list
                                                     ,mgpp->osrc_layer_id
                                                     ,mgpp->fdst_layer_id
                                                     ,curr_width
                                                     ,curr_height
                                                     ,forward_move
                                                     );

       /* warp the BG layer */
       p_layer_warp_move ( curr_wp_list_1
                         , curr_wp_list_2
                         , src_layer_id
                         , bg_layer_id
                         , mgpp
                         , wp_set_1
                         , wp_set_2
                         , wp_mix_factor
                         );
       gap_morph_exec_free_workpoint_list(&curr_wp_list_1);
       gap_morph_exec_free_workpoint_list(&curr_wp_list_2);
     }
     else
     {
       if(gap_debug)
       {
         printf("simple workpoint setting for BG LAYER total_steps:%.2f current_step:%.2f forward_move:%d  wp_mix_factor:%.3f\n"
            , (float)total_steps
            , (float)current_step
            , (int)forward_move
            , (float)wp_mix_factor
            );
       }
     
       /* simple workpoint settings for src_layer */
       curr_wp_list_1 = p_calculate_work_point_movement((gdouble)total_steps
                                                     ,(gdouble)current_step
                                                     ,mgpp->master_wp_list
                                                     ,mgpp->osrc_layer_id
                                                     ,mgpp->fdst_layer_id
                                                     ,curr_width
                                                     ,curr_height
                                                     ,forward_move
                                                     );

       /* warp the BG layer */
       p_layer_warp_move ( curr_wp_list_1
                         , NULL
                         , src_layer_id
                         , bg_layer_id
                         , mgpp
                         , NULL
                         , NULL
                         , wp_mix_factor
                         );

       gap_morph_exec_free_workpoint_list(&curr_wp_list_1);
     }
     forward_move = FALSE;
     mgpp->master_progress += mgpp->layer_progress_step;
   }
   
   if((wp_set_1) && (wp_set_2))
   {
     /* milti workpoint settings for sc_layer */
     curr_wp_list_1 = p_calculate_work_point_movement((gdouble)total_steps
                                                   ,(gdouble)current_step
                                                   ,wp_set_1->wp_list
                                                   ,mgpp->osrc_layer_id
                                                   ,mgpp->fdst_layer_id
                                                   ,curr_width
                                                   ,curr_height
                                                   ,forward_move
                                                   );
     
     curr_wp_list_2 = p_calculate_work_point_movement((gdouble)total_steps
                                                   ,(gdouble)current_step
                                                   ,wp_set_2->wp_list
                                                   ,mgpp->osrc_layer_id
                                                   ,mgpp->fdst_layer_id
                                                   ,curr_width
                                                   ,curr_height
                                                   ,forward_move
                                                   );
     
     /* warp the TOP layer */
     p_layer_warp_move ( curr_wp_list_1
                       , curr_wp_list_2
                       , dst_layer_id
                       , top_layer_id
                       , mgpp
                       , wp_set_1
                       , wp_set_2
                       , wp_mix_factor
                       );

     gap_morph_exec_free_workpoint_list(&curr_wp_list_1);
     gap_morph_exec_free_workpoint_list(&curr_wp_list_2);
   }
   else
   {
     if(gap_debug)
     {
       printf("simple workpoint setting for TOP LAYER total_steps:%.2f current_step:%.2f forward_move:%d  wp_mix_factor:%.3f\n"
          , (float)total_steps
          , (float)current_step
          , (int)forward_move
          , (float)wp_mix_factor
          );
     }

     /* simple workpoint settings for sc_layer */
     curr_wp_list_1 = p_calculate_work_point_movement((gdouble)total_steps
                                                   ,(gdouble)current_step
                                                   ,mgpp->master_wp_list
                                                   ,mgpp->osrc_layer_id
                                                   ,mgpp->fdst_layer_id
                                                   ,curr_width
                                                   ,curr_height
                                                   ,forward_move
                                                   );

     /* warp the TOP layer */
     p_layer_warp_move ( curr_wp_list_1
                       , NULL
                       , dst_layer_id
                       , top_layer_id
                       , mgpp
                       , NULL
                       , NULL
                       , wp_mix_factor
                       );

     gap_morph_exec_free_workpoint_list(&curr_wp_list_1);
   }

   gimp_drawable_detach(dst_drawable);

   if(mgpp->render_mode == GAP_MORPH_RENDER_MODE_WARP)
   {
     /* for warp only mode all is done at this point */
     return(top_layer_id);
   }

   /* merge BG and TOP Layer (does mix opacity according to current step) */
   merged_layer_id = p_mix_layers(curr_image_id
               ,bg_layer_id
               ,top_layer_id
               ,curr_width
               ,curr_height
               ,(gdouble)(curr_opacity / 100.0)
               );

   /* DEBUG code: show duplicate of the temporary tween image */
   if(FALSE)
   {
     gint32 dup_id;

     dup_id = gimp_image_duplicate(curr_image_id);
     gimp_display_new (dup_id);

   }

   return(merged_layer_id);

}  /* end p_create_morph_tween_frame  */


/* ---------------------------------
 * p_get_tween_steps_and_layerstacks
 * ---------------------------------
 * get layerstack for both src and dst
 * 
 * return the constraint tween_steps value
 * when operating on existing destination layers
 */
static gint32
p_get_tween_steps_and_layerstacks(GapMorphGlobalParams *mgpp, GapMorphExeLayerstack *mlayers)
{
  gint32 src_image_id;
  gint32 dst_image_id;
  gint32 tween_steps;
  gint32 ii;
  
  tween_steps = 0;
  mlayers->src_nlayers = 0;
  mlayers->src_layers = NULL;
  mlayers->dst_nlayers = 0;
  mlayers->dst_layers = NULL;

  mlayers->src1_idx = -1;
  mlayers->src2_idx = -1;
  mlayers->dst1_idx = -1;
  mlayers->dst2_idx = -1;
  src_image_id = -1;
  dst_image_id = -1;


  if(gap_debug)
  {
    printf("p_get_tween_steps_and_layerstacks: START\n");
  }
  
  if(mgpp->osrc_layer_id >= 0)
  {
    src_image_id = gimp_drawable_get_image(mgpp->osrc_layer_id);
    
    mlayers->src_layers = gimp_image_get_layers (src_image_id
                                             ,&mlayers->src_nlayers
                                             );
    for(ii=0; ii < mlayers->src_nlayers; ii++)
    {
      if(gap_debug)
      {
        printf("src: id[%d]: %d\n", (int)ii, (int)mlayers->src_layers[ii] );
      }
      if(mlayers->src_layers[ii] == mgpp->osrc_layer_id)
      {
        mlayers->src1_idx = ii;  /* src at 1.st step */
        mlayers->src2_idx = ii;  /* src at last step */
        break;
      }
    }
  }
  
  if(mgpp->fdst_layer_id >= 0)
  {
    tween_steps = mgpp->tween_steps;
    
    dst_image_id = gimp_drawable_get_image(mgpp->fdst_layer_id);
    mlayers->dst_layers = gimp_image_get_layers (dst_image_id
                                             ,&mlayers->dst_nlayers
                                             );
    for(ii=0; ii < mlayers->dst_nlayers; ii++)
    {

      if(gap_debug)
      {
        printf("dst: id[%d]: %d\n", (int)ii, (int)mlayers->dst_layers[ii] );
      }

      if(mlayers->dst_layers[ii] == mgpp->fdst_layer_id)
      {
        mlayers->dst2_idx = ii;  /* dst at last step */
        break;
      }
    }
  }

  if(mgpp->create_tween_layers)
  {
    mlayers->dst1_idx = mlayers->dst2_idx + (tween_steps -1);
    if(gap_debug)
    {
      printf("p_get_tween_steps_and_layerstacks: create_tween_layers RETURN tween_steps:%d\n"
         ,(int)tween_steps
         );
    }
    return(tween_steps);
  }
  
  if(mgpp->render_mode == GAP_MORPH_RENDER_MODE_WARP)
  {
    if(dst_image_id == src_image_id)
    {
      if(mlayers->dst2_idx > mlayers->src1_idx)
      {
        gint32 tmp;
        /* source layer is above the destination layer
         * in this case swap src with dst layer
         */
        tmp = mgpp->fdst_layer_id;
        mgpp->fdst_layer_id = mgpp->osrc_layer_id;
        mgpp->osrc_layer_id = tmp;
        
        tmp = mlayers->dst2_idx;
        mlayers->dst2_idx = mlayers->src1_idx;
        mlayers->src1_idx = tmp;
        mlayers->src2_idx = tmp;
      }
      tween_steps = 1 + (mlayers->src1_idx - mlayers->dst2_idx );
    }
    else
    {
      tween_steps = MIN(tween_steps, (mlayers->dst_nlayers - mlayers->dst2_idx) );
    }
  }

  mlayers->dst1_idx = mlayers->dst2_idx + (tween_steps -1);
  if(mgpp->render_mode == GAP_MORPH_RENDER_MODE_WARP)
  {
    mgpp->osrc_layer_id = mlayers->dst_layers[mlayers->dst1_idx];
  }
  
  mlayers->src1_idx = MIN((mlayers->src2_idx + (tween_steps -1)) , (mlayers->src_nlayers -1));

  if(gap_debug)
  {
    printf("p_get_tween_steps_and_layerstacks: create_tween_layers END, RETURN tween_steps:%d\n"
       ,(int)tween_steps
       );
  }

  return(tween_steps);
}   /* end p_get_tween_steps_and_layerstacks */


/* ---------------------------------
 * gap_morph_execute
 * ---------------------------------
 * layer morphing master procedure
 */
gint32  
gap_morph_execute(GapMorphGlobalParams *mgpp)
{
  gint32 total_steps;
  gint32 current_step;
  gint32 last_step;
  gint32 new_layer_id;
  gint32 cp_layer_id;
  gint32 dst_image_id;
  gint32 dst_stack_position;
  GapMorphExeLayerstack mlayers_struct;
  GapMorphExeLayerstack *mlayers;
  
  mlayers = &mlayers_struct;
  mlayers->tab_wp_sets = NULL;
  mlayers->available_wp_sets = 0;

  mgpp->tween_steps = p_get_tween_steps_and_layerstacks(mgpp, mlayers);
  
  if(mgpp->have_workpointsets)
  {
    /* check for available workpointfile sets and load them.
     * (note: this also handles the case when working with a single workpointfile
     *  where the same workpoint file is used both as upper and lower set)
     */
    p_build_wp_set_table(mgpp ,mlayers);
  }
  
  if(gap_debug) printf("gap_morph_execute: START src_layer:%d dst_layer:%d  tween_steps:%d\n"
         ,(int)mgpp->osrc_layer_id
         ,(int)mgpp->fdst_layer_id
         ,(int)mgpp->tween_steps
         );

  if(mgpp->osrc_layer_id == mgpp->fdst_layer_id)
  {
     /* for identical layers we do only warp
      * (fading identical images makes no sense, force warp only mode)
      */
    mgpp->render_mode = GAP_MORPH_RENDER_MODE_WARP;
  }

  if((mgpp->do_progress) && (mgpp->progress_callback_fptr == NULL))
  {
    if(mgpp->render_mode == GAP_MORPH_RENDER_MODE_MORPH)
    {
      gimp_progress_init(_("creating morph tween layers..."));
    }
    else
    {
      gimp_progress_init(_("creating warp tween layers..."));
    }
    
  }

  cp_layer_id = -1;
  dst_image_id = gimp_drawable_get_image(mgpp->fdst_layer_id);
  dst_stack_position = 1;

  /* findout stackposition (where to insert new tween layer(s)
   * in the destination image.
   * (one below the mgpp->fdst_layer_id Layer)
   * where 0 is on top of stack
   */
  {
    gint       ii;
    gint       l_nlayers;
    gint32    *l_layers_list;

    l_layers_list = gimp_image_get_layers(dst_image_id, &l_nlayers);
    for(ii=0; ii < l_nlayers; ii++)
    {
      if(l_layers_list[ii] == mgpp->fdst_layer_id)
      {
        dst_stack_position = ii +1;
        break;
      }
    }
    if(l_layers_list)
    {
      g_free (l_layers_list);
    }
  }

  
  last_step = mgpp->tween_steps;
  total_steps = 1 + mgpp->tween_steps;
  mgpp->master_progress = 0.0;
  mgpp->layer_progress_step = 0.5 / (gdouble)(MAX(1, last_step));

  if(mgpp->render_mode == GAP_MORPH_RENDER_MODE_WARP)
  {
    /* warp_only mode (equal layer)
     * must do one extra step to render the final distortion step
     * (in morph mode this makes no sense, because this extra step
     *  would deliver the destination layer again as result)
     */
    last_step = mgpp->tween_steps;
    total_steps = mgpp->tween_steps;

    mgpp->layer_progress_step = 1.0 / (gdouble)(MAX(1, last_step));
  }

  
  for(current_step=1; current_step <= last_step; current_step++)
  {
    new_layer_id = p_create_morph_tween_frame(total_steps
                          ,current_step
                          ,mgpp
                          ,mlayers
                          );
    if(new_layer_id < 0)
    {
      continue;
      break;
    }
    else
    {
      gint32 tween_image_id;
      gint   l_src_offset_x, l_src_offset_y;    /* layeroffsets as they were in src_image */
      
      if(mgpp->create_tween_layers)
      {
        cp_layer_id = gap_layer_copy_to_dest_image(dst_image_id
                                     ,new_layer_id
                                     ,100.0           /* Opacity */
                                     ,0               /* NORMAL */
                                     ,&l_src_offset_x
                                     ,&l_src_offset_y
                                     );
        gimp_image_add_layer(dst_image_id
                            , cp_layer_id
                            , dst_stack_position
                            );
      }
      else
      {
        gint ii;

        /* replace content of current dst_layer[ii] by content of new_layer_id */
        ii = mlayers->dst1_idx - (current_step -1);
        gap_layer_copy_content(mlayers->dst_layers[ii], new_layer_id);
      }
      tween_image_id = gimp_drawable_get_image(new_layer_id);
      gimp_image_delete(tween_image_id);
    }


    if(mgpp->do_progress)
    {
      mgpp->master_progress = (gdouble)current_step / (gdouble)(MAX(1, last_step));
      if(mgpp->progress_callback_fptr == NULL)
      {
        gimp_progress_update(mgpp->master_progress);
      }
      else
      {
        (*mgpp->progress_callback_fptr)(mgpp->master_progress, mgpp->callback_data_ptr);
      }
    }

    
  }
  return(cp_layer_id);
  
}  /* end gap_morph_execute */



/* ----------------------------------
 * p_create_simple_fade_tween_frame
 * ----------------------------------
 *
 * return the newly created tween morphed layer
 *
 */
static gint32
p_create_simple_fade_tween_frame(gint32 total_steps
                          ,gint32 current_step
                          ,GapMorphGlobalParams *mgpp
                          )
{
   gint32  curr_image_id;  /* a temp image of current layer size */
   gint32  bg_layer_id;
   gint32  top_layer_id;
   gint32  merged_layer_id;
   gint32  curr_width;
   gint32  curr_height;
   gdouble curr_opacity;
   gint32 src_layer_id;
   gint32 dst_layer_id;
   GimpImageType  l_src_type;
   
   src_layer_id = mgpp->osrc_layer_id;
   dst_layer_id = mgpp->fdst_layer_id;


   /* check size of the new frame */
   curr_width = gimp_drawable_width(src_layer_id);
   if (curr_width != gimp_drawable_width(dst_layer_id))
   {
     return -1;
   }

   curr_height = gimp_drawable_height(src_layer_id);
   if (curr_height != gimp_drawable_height(dst_layer_id))
   {
     return -1;
   }


  l_src_type    = gimp_drawable_type(src_layer_id);
  if (l_src_type != gimp_drawable_type(dst_layer_id))
  {
    return -1;
  }
  
  curr_image_id = -1;
  
  switch(l_src_type)
  {
    case GIMP_RGB_IMAGE:         /* 0 */
    case GIMP_RGBA_IMAGE:        /* 1 */
       curr_image_id = gimp_image_new(curr_width, curr_height, GIMP_RGB);
       break;
    case GIMP_GRAY_IMAGE:        /* 2 */
    case GIMP_GRAYA_IMAGE:       /* 3 */
       curr_image_id = gimp_image_new(curr_width, curr_height, GIMP_GRAY);
       break;
    case GIMP_INDEXED_IMAGE:     /* 4 */
    case GIMP_INDEXEDA_IMAGE:    /* 5 */
       return -1;
       break;
  }   


   bg_layer_id = gap_layer_copy_to_image (curr_image_id, mgpp->osrc_layer_id);
   top_layer_id = gap_layer_copy_to_image (curr_image_id, mgpp->fdst_layer_id);

   /* merge BG and TOP Layer (does mix opacity according to current step) */
   curr_opacity = p_linear_advance((gdouble)total_steps
                                ,(gdouble)current_step
                                ,(gdouble)0.0
                                ,(gdouble)100.0
                                );
   if(mgpp->do_progress)
   {
     if(mgpp->progress_callback_fptr != NULL)
     {
         (*mgpp->progress_callback_fptr)(0.05, mgpp->callback_data_ptr);
     }
   }

   merged_layer_id = 
   p_mix_layers(curr_image_id
               ,bg_layer_id
               ,top_layer_id
               ,curr_width
               ,curr_height
               ,(gdouble)(curr_opacity / 100.0)
               );
 
   if(mgpp->do_progress)
   {
     if(mgpp->progress_callback_fptr != NULL)
     {
         (*mgpp->progress_callback_fptr)(1.0, mgpp->callback_data_ptr);
     }
   }
   mgpp->master_progress += mgpp->layer_progress_step;


   /* DEBUG code: show duplicate of the temporary tween image */
   if(FALSE)
   {
     gint32 dup_id;

     dup_id = gimp_image_duplicate(curr_image_id);
     gimp_display_new (dup_id);

   }

   return(merged_layer_id);

}  /* end p_create_simple_fade_tween_frame  */


/* ----------------------------------
 * gap_morph_render_one_of_n_tweens
 * ----------------------------------
 * This procedure creates only one tween (a scene between 2 video frames)
 * according to the specified total_steps and current_step parameters.
 * the created tween is part of a new created image
 * that contians the tween layer.
 *
 * return the newly created tween morphed layer
 *
 */
gint32
gap_morph_render_one_of_n_tweens(GapMorphGlobalParams *mgpp, gdouble total_steps, gdouble current_step)
{
  gint32 new_layer_id;
  GapMorphExeLayerstack mlayers_struct;
  GapMorphExeLayerstack *mlayers;
  gint32              bck_tween_steps;
  gint32              ret_layer_id;
  
  mlayers = &mlayers_struct;
  mlayers->tab_wp_sets = NULL;
  mlayers->available_wp_sets = 0;
  new_layer_id = -1;
  
  p_get_tween_steps_and_layerstacks(mgpp, mlayers);

  mgpp->create_tween_layers = TRUE;

  /* check if workpoint filename is available */
  if(mgpp->workpoint_file_lower)
  {
    if(*mgpp->workpoint_file_lower != '\0')
    {
      /* load a single workpointfile */
      mlayers->available_wp_sets = 1;
      mlayers->tab_wp_sets = g_new(GapMorphWarpCoreAPI*, mlayers->available_wp_sets);
      
      mlayers->tab_wp_sets[0] = p_load_workpoint_set(mgpp->workpoint_file_lower, mgpp, mlayers);
      mgpp->master_wp_list = mlayers->tab_wp_sets[0]->wp_list;
      
      mgpp->have_workpointsets = FALSE;
      mlayers->available_wp_sets = 0;
    }
  }
  
  /* overrule the number of steps with the explicite parameter value */
  bck_tween_steps = mgpp->tween_steps;
  mgpp->tween_steps = total_steps;

  if(mgpp->do_simple_fade)
  {
    if(gap_debug)
    {
      printf("do_simple_fade == TRUE, skip morph and do fade operation instead\n");
    }

    /* we have no workpoints available
     * in this case make an attempt with a fast fade operation
     *  (but restricted to frames of same size and type)
     */
    new_layer_id =  p_create_simple_fade_tween_frame(total_steps
                          ,current_step
                          ,mgpp
                          );
    
  }
  
  if( new_layer_id < 0)
  {
    if(gap_debug)
    {
      printf("calling the full morph algorithm  total_steps:%f current_step:%f\n"
        ,(float)total_steps
        ,(float)current_step
        );
    }
    new_layer_id = p_create_morph_tween_frame(total_steps
                          ,current_step
                          ,mgpp
                          ,mlayers
                          );
  }
  ret_layer_id = gap_image_merge_to_specified_layer(new_layer_id, GIMP_CLIP_TO_IMAGE);

  /* restore tween_steps value */
  mgpp->tween_steps = bck_tween_steps;
  
  return(ret_layer_id);
  
}  /* end gap_morph_render_one_of_n_tweens */


/* ----------------------------------
 * gap_morph_render_one_tween
 * ----------------------------------
 * This procedure creates only one tween (a scene between 2 video frames)
 * according to the specified mix factor (mgpp->tween_mix_factor)
 * the created tween is part of a new created image
 * that contians the tween layer.
 *
 * return the newly created tween morphed layer
 *
 */
gint32
gap_morph_render_one_tween(GapMorphGlobalParams *mgpp)
{
  gint32 new_layer_id;
  gdouble total_steps;
  gdouble current_step;
  
  total_steps = 1.0 * 100000;
  current_step = CLAMP(mgpp->tween_mix_factor, 0.0, 1.0) * 100000;

  
  new_layer_id = gap_morph_render_one_of_n_tweens(mgpp, total_steps, current_step);

  return (new_layer_id);
}  /* end gap_morph_render_one_tween */




/* -----------------------------------------------
 * p_buildTargetTweenFilename
 * -----------------------------------------------
 * build filename for the target Tween frame with the specified targetFrameNr number part
 * with full directory path (specified by tweenDirectory)
 *
 * the caller is rsponsible to g_free the returned filename.
 */
static char*
p_buildTargetTweenFilename(GapAnimInfo *ainfo_ptr, const char *tweenDirectory, gint32 targetFrameNr)
{
  char *fileBasenameWithoutDir;
  char *fileNameTarget;
  char *fileNameTargetWithDir;
  char *checkName;
  
  
  fileBasenameWithoutDir = g_path_get_basename(ainfo_ptr->basename);
  fileNameTarget = g_strdup_printf("%s%06d%s"
                          , fileBasenameWithoutDir
                          , (int)targetFrameNr
                          , ainfo_ptr->extension
                          );

  checkName = g_build_filename(tweenDirectory, fileBasenameWithoutDir, NULL);
  
  if((strcmp(checkName, ainfo_ptr->basename) == 0)
  || (*tweenDirectory == '.'))
  {
    if(gap_debug)
    {
      printf("WARNING: create tween in directory: %s\n"
           "  with additional prefix tween_"
           "  to prevent overwrite of source frames in the same directory\n"
        , tweenDirectory
        );
    }
    fileNameTarget = g_strdup_printf("tween_%s%06d%s"
                          , fileBasenameWithoutDir
                          , (int)targetFrameNr
                          , ainfo_ptr->extension
                          );
  }
  else
  {
    fileNameTarget = g_strdup_printf("%s%06d%s"
                          , fileBasenameWithoutDir
                          , (int)targetFrameNr
                          , ainfo_ptr->extension
                          );
  }

  fileNameTargetWithDir = g_build_filename(tweenDirectory, fileNameTarget, NULL);


  if(gap_debug)
  {
    printf("checkName: %s\n", checkName);
    printf("fileBasenameWithoutDir: %s\n", fileBasenameWithoutDir);
    printf("fileNameTarget: %s\n", fileNameTarget);
    printf("fileNameTargetWithDir: %s\n", fileNameTargetWithDir);
  }
  
  if(fileBasenameWithoutDir)    { g_free(fileBasenameWithoutDir); }
  if(fileNameTarget)            { g_free(fileNameTarget); }
  if(checkName)                 { g_free(checkName); }

  return(fileNameTargetWithDir);

}  /* end p_buildTargetTweenFilename */


/* -----------------------------------------------
 * p_copyAndGetMergedFrameImageLayer
 * -----------------------------------------------
 * load the specified frame as temporary image, merge to one layer and return this layer's id.
 * further copy the original image to the directory specified via tweenDirectory
 * the filename of the copy is built with original filename and extension but the number part is replaced
 * by the specified targetFrameNr.
 *
 * return -1 if FAILED, or layerId (positive integer) on success
 */
gint32
p_copyAndGetMergedFrameImageLayer(GapAnimInfo *ainfo_ptr, gint32 frameNr
   , const char *tweenDirectory, gint32 targetFrameNr, gboolean overwrite_flag)
{
  gint32 imageId;
  gint32 layerId;

  layerId = -1;
  if(ainfo_ptr->new_filename != NULL)
  {
    g_free(ainfo_ptr->new_filename);
  }
  ainfo_ptr->new_filename = gap_lib_alloc_fname(ainfo_ptr->basename,
                                      frameNr,
                                      ainfo_ptr->extension);
  if(ainfo_ptr->new_filename == NULL)
  {
     printf("could not create frame filename for frameNr:%d\n", (int)frameNr);
     return -1;
  }

  if(!g_file_test(ainfo_ptr->new_filename, G_FILE_TEST_EXISTS))
  {
     printf("file not found: %s\n", ainfo_ptr->new_filename);
     return -1;
  }

  /* load current frame */
  imageId = gap_lib_load_image(ainfo_ptr->new_filename);
  if(imageId >= 0)
  {
    char *targetTweenFilename;
    
    layerId = gap_image_merge_visible_layers(imageId, GIMP_CLIP_TO_IMAGE);
    
    targetTweenFilename = p_buildTargetTweenFilename(ainfo_ptr, tweenDirectory, targetFrameNr);
    
    if(gap_debug)
    {
      printf("COPY: %s ==> %s\n", ainfo_ptr->new_filename, targetTweenFilename);
    }


    if((!g_file_test(targetTweenFilename, G_FILE_TEST_EXISTS))
    || (overwrite_flag == TRUE))
    {
      gap_lib_file_copy(ainfo_ptr->new_filename /* the original file */
                     ,targetTweenFilename     /* the copy */
                     );
    }
    

  }
  
  return(layerId);

}  /* end p_copyAndGetMergedFrameImageLayer */




/* -------------------------------------
 * p_morph_render_frame_tweens_in_subdir
 * -------------------------------------
 *
 * return one of the newly created morphed tween frame layers
 *       (the one that was created last is picked)
 *
 */
gint32
p_morph_render_frame_tweens_in_subdir(GapAnimInfo *ainfo_ptr, GapMorphGlobalParams *mgpp, gboolean *cancelFlagPtr)
{
  char   *tweenDirectory;
  char   *targetTweenFrameFilename;
  gint32 l_tween_layer_id;
  gint32 l_rc;
  gint   l_errno;
  gint32 frameCount;
  gint32 currFrameNr;
  gint32 nextFrameNr;
  gint32 nextTargetFrameNr;
  gint32 baseTargetFrameNr;
  gint32 currTargetFrameNr;
  gint32 currLayerId;
  gint32 nextLayerId;
  gint32 firstFrameNr;
  gint32 lastFrameNr;
  gdouble framesToProcess;
  gdouble nTweenFramesTotal;   /* for outer progress (frames that are just copied are not included in progress) */
  gdouble nTweenFramesDone;    /* rendered tween frames so far (frames that are just copied are not included) */



  l_tween_layer_id = -1;
  l_errno = 0;
  targetTweenFrameFilename = NULL;

  if(gap_debug)
  {
    printf("p_morph_render_frame_tweens_in_subdir  START\n");
  }
  
  if((mgpp->tween_subdir[0] == '\0') || (mgpp->tween_subdir[0] == '.'))
  {
    /* use same directory as source frames
     * (in this case tween frame filenames are automatically prefixed with "tween_"
     * to prevent overwrite of the source frames while processing)
     */
    tweenDirectory = g_path_get_dirname(ainfo_ptr->basename);
  }
  else
  {
    tweenDirectory = gap_file_make_abspath_filename(&mgpp->tween_subdir[0]
                     , ainfo_ptr->basename
                     );
  }
  

  if(gap_debug)
  {
    printf("p_morph_render_frame_tweens_in_subdir  tweenDirectory:%s\n"
       ,tweenDirectory
       );
  }



  if (!g_file_test(tweenDirectory, G_FILE_TEST_IS_DIR))
  {
    gap_file_mkdir (tweenDirectory, GAP_FILE_MKDIR_MODE);
    l_errno = errno;
  }

  if (!g_file_test(tweenDirectory, G_FILE_TEST_IS_DIR))
  {
    g_message (_("Failed to create tween subdirectory: '%s':\n%s")
                   , tweenDirectory
                   , g_strerror (l_errno)
                   );
    return (-1);
  }


  frameCount = 0;
  nextLayerId = -1;
 
  if(gap_lib_chk_framerange(ainfo_ptr) != 0)
  {
    return(0);
  }
  
  
  firstFrameNr = CLAMP(mgpp->range_from, ainfo_ptr->first_frame_nr, ainfo_ptr->last_frame_nr);
  currFrameNr = firstFrameNr;
  currTargetFrameNr = firstFrameNr;
  nextTargetFrameNr = firstFrameNr;
  baseTargetFrameNr = firstFrameNr;

  currLayerId = p_copyAndGetMergedFrameImageLayer(ainfo_ptr
                      , currTargetFrameNr
                      , tweenDirectory
                      , currTargetFrameNr
                      , mgpp->overwrite_flag
                      );
  nextFrameNr = currFrameNr + 1;


  lastFrameNr = MIN(mgpp->range_to, ainfo_ptr->last_frame_nr);
  nTweenFramesTotal = MAX(1.0, (lastFrameNr - firstFrameNr) * mgpp->master_tween_steps);
  nTweenFramesDone = 0;

  /* loop to process all frames within the specified frame range */
  while(TRUE)
  {
    gdouble nFrames;
    gint32  tweenFramesToBeCreated;
    gdouble l_current_step;

    /* recalculate (in case range_to is changed on GUI while processing is already running) */
    lastFrameNr = MIN(mgpp->range_to, ainfo_ptr->last_frame_nr);
    framesToProcess = MAX(1, (lastFrameNr - currFrameNr) -1);
    nTweenFramesTotal = MAX(1.0, (lastFrameNr - firstFrameNr) * mgpp->master_tween_steps);
   
    if (nextFrameNr > lastFrameNr)
    {
      if(gap_debug)
      {
        printf("BREAK nextFrameNr:%d lastFrameNr:%d\n"
          ,(int)nextFrameNr
          ,(int)lastFrameNr
          );
      }
     
      break;
    }

    nFrames = nextFrameNr - currFrameNr;
    tweenFramesToBeCreated = (nFrames * mgpp->master_tween_steps) + (nFrames -1);

    nextTargetFrameNr = baseTargetFrameNr + (1 + tweenFramesToBeCreated);
    
    if(gap_debug)
    {
      printf("ATTR currFrameNr:%d  nextFrameNr:%d  master_tween_steps:%d  tweenFramesToBeCreated:%d  baseTargetFrameNr:%d  nextTargetFrameNr:%d\n"
        ,(int)currFrameNr
        ,(int)nextFrameNr
        ,(int)mgpp->master_tween_steps
        ,(int)tweenFramesToBeCreated
        ,(int)baseTargetFrameNr
        ,(int)nextTargetFrameNr
        );
    }
    nextLayerId = p_copyAndGetMergedFrameImageLayer(ainfo_ptr
                         , nextFrameNr
                         , tweenDirectory
                         , nextTargetFrameNr
                         , mgpp->overwrite_flag
                         );
    
    if((nextLayerId >= 0) && (currLayerId >= 0))
    {
      char *workpointFileName;
      gboolean success;
        
   
      success = TRUE;
      baseTargetFrameNr = nextTargetFrameNr;

      workpointFileName = gap_lib_alloc_fname(ainfo_ptr->basename,
                                      currFrameNr,
                                      "." GAP_MORPH_WORKPOINT_EXTENSION);      
      if(g_file_test(workpointFileName, G_FILE_TEST_EXISTS))
      {
        if(gap_debug)
        {
          printf("USING workpointfile: %s\n"
                 , workpointFileName
                 );
        }
        g_snprintf(&mgpp->workpoint_file_lower[0]
                , sizeof(mgpp->workpoint_file_lower)
                , "%s"
                , workpointFileName
                );
        g_snprintf(&mgpp->workpoint_file_upper[0]
                , sizeof(mgpp->workpoint_file_upper)
                , "%s"
                , workpointFileName
                );
        
        mgpp->do_simple_fade = FALSE;
      }
      else
      {
        if(gap_debug)
        {
          printf("have no valid frame specific workpointfile: %s\n"
                 "  (reset names to force simple fade operaton\n"
                 , workpointFileName
                 );
        }
        mgpp->workpoint_file_lower[0] = '\0';
        mgpp->workpoint_file_upper[0] = '\0';
        mgpp->do_simple_fade = TRUE;
      }

      
       mgpp->osrc_layer_id = currLayerId;
       mgpp->fdst_layer_id = nextLayerId;

      /* loop to generate tween frames between currentFrameNr and nextFrameNr */
      for (l_current_step = 1; l_current_step <= tweenFramesToBeCreated; l_current_step++)
      {
        gint32  l_tween_tmp_image_id;
        
        currTargetFrameNr++;

        mgpp->master_progress = 0.0;
        mgpp->layer_progress_step = 0.5;

        if(l_tween_layer_id >= 0)
        {
          /* delete the previous handled tween frame image in memory. 
           * (that is already saved to disk)
           * we keep only the last one opened in gimp.
           */
          gap_image_delete_immediate(gimp_drawable_get_image(l_tween_layer_id));
        }

        if(targetTweenFrameFilename != NULL)
        {
          g_free(targetTweenFrameFilename);
        }
        targetTweenFrameFilename = p_buildTargetTweenFilename(ainfo_ptr
                                            , tweenDirectory
                                            , currTargetFrameNr);
        if(targetTweenFrameFilename == NULL)
        {
          success = FALSE;
          break;
        }
        
        if(gap_debug)
        {
          printf("p_morph_render_frame_tweens_in_subdir creating tween:%s\n"
             , targetTweenFrameFilename
             );
        }

        /* progress and cancel handling */
        if(mgpp->do_progress)
        {
           gdouble percentage;
           percentage = nTweenFramesDone / nTweenFramesTotal;
           if(gap_debug)
           {
             printf("\nTWEEN_PROGRESS: nTweenFramesDone:%.0f nTweenFramesTotal:%.0f percentage:%.5f\n\n"
               , (float)nTweenFramesDone
               , (float)nTweenFramesTotal
               , (float)percentage
               );
           }
           if(mgpp->master_progress_callback_fptr != NULL)
           {
             (*mgpp->master_progress_callback_fptr)( nTweenFramesDone
                                                   , nTweenFramesTotal
                                                   , targetTweenFrameFilename
                                                   , mgpp->callback_data_ptr
                                                   );           
           }
           if(cancelFlagPtr != NULL)
           {
             if (*cancelFlagPtr == TRUE)
             {
                success = FALSE;
                break;
             }           
           }
        }

        if (mgpp->overwrite_flag != TRUE)
        {
          if(g_file_test(targetTweenFrameFilename, G_FILE_TEST_EXISTS))
          {
            l_tween_layer_id = -1;
            if(gap_debug)
            {
              printf("SKIP tween generation because file: %s already exists"
                 , targetTweenFrameFilename
                 );
            }
            continue;
          }
        }


        /* CALL of the morphing processor */

        l_tween_layer_id = gap_morph_render_one_of_n_tweens(mgpp
                                                           , tweenFramesToBeCreated +1
                                                           , l_current_step
                                                           );
        l_tween_tmp_image_id = gimp_drawable_get_image(l_tween_layer_id);
        if(gap_debug)
        {
          printf("p_morph_render_frame_tweens_in_subdir saving tween:%s :%d\n"
             ,targetTweenFrameFilename
             ,(int)l_tween_tmp_image_id
             );
        }
        l_rc = gap_lib_save_named_frame(l_tween_tmp_image_id, targetTweenFrameFilename);
        if(targetTweenFrameFilename != NULL)
        {
          g_free(targetTweenFrameFilename);
          targetTweenFrameFilename = NULL;
        }
        if (l_rc < 0)
        {
          l_tween_layer_id = -1;
          p_error_message_with_filename(mgpp->run_mode, _("file: %s save failed"), targetTweenFrameFilename);
          success = FALSE;
          break;
        }

        nTweenFramesDone++;

      }

      g_free(workpointFileName);
      gap_image_delete_immediate(gimp_drawable_get_image(currLayerId));
      if(!success)
      {
        break;
      }

      currFrameNr = nextFrameNr;
      currLayerId = nextLayerId;
      currTargetFrameNr++;

    }
    nextFrameNr++;

  }
  
  if(nextLayerId >= 0)
  {
    gap_image_delete_immediate(gimp_drawable_get_image(nextLayerId));
  }

  return(l_tween_layer_id);

}  /* end p_morph_render_frame_tweens_in_subdir */




/* ----------------------------------
 * gap_morph_render_frame_tweens
 * ----------------------------------
 *
 * return the newly created tween morphed layer
 *
 */
gint32
gap_morph_render_frame_tweens(GapAnimInfo *ainfo_ptr, GapMorphGlobalParams *mgpp, gboolean *cancelFlagPtr)
{
  gint32 l_tween_layer_id;
  gint32 l_tween_frame_nr;
  gint32 l_rc;
  gdouble total_steps;


  if(mgpp->create_tweens_in_subdir == TRUE)
  {
    return(p_morph_render_frame_tweens_in_subdir(ainfo_ptr, mgpp, cancelFlagPtr));
  }


  l_tween_layer_id = -1;

  total_steps = (mgpp->range_to -  mgpp->range_from);
  mgpp->tween_steps = (mgpp->range_to -  mgpp->range_from);

  mgpp->master_progress = 0.0;
  mgpp->layer_progress_step = 0.5;
  
  if (mgpp->tween_steps > 0)
  {
    gint32 l_tmp_image_id;
    if(ainfo_ptr->new_filename != NULL)
    {
      g_free(ainfo_ptr->new_filename);
    }
    ainfo_ptr->new_filename = gap_lib_alloc_fname(ainfo_ptr->basename,
                                        mgpp->range_to,
                                        ainfo_ptr->extension);
    if(ainfo_ptr->new_filename == NULL)
    {
       printf("could not create frame filename for frameNr:%d\n", (int)mgpp->range_to);
       return -1;
    }

    if(!g_file_test(ainfo_ptr->new_filename, G_FILE_TEST_EXISTS))
    {
       printf("target frame does not exist, name: %s\n", ainfo_ptr->new_filename);
       if (mgpp->run_mode != GIMP_RUN_NONINTERACTIVE)
       {
         g_message(_("target frame does not exist, name: %s"), ainfo_ptr->new_filename);
       }
       return -1;
    }



    /* load current frame */
    l_tmp_image_id = gap_lib_load_image(ainfo_ptr->new_filename);

    if(gap_debug)
    {
      printf("gap_morph_render_frame_tweens to frame: %s  l_tmp_image_id:%d  RUN_MODE:%d\n"
         ,ainfo_ptr->new_filename
         ,(int)l_tmp_image_id
         ,(int)mgpp->run_mode
         );
    }

    mgpp->osrc_layer_id = gap_image_merge_visible_layers(gimp_image_duplicate(mgpp->image_id), GIMP_CLIP_TO_IMAGE);
    mgpp->fdst_layer_id = gap_image_merge_visible_layers(l_tmp_image_id, GIMP_CLIP_TO_IMAGE);

    if(gap_debug)
    {
      printf("gap_morph_render_frame_tweens osrc_layer_id:%d fdst_layer_id:%d \n"
         ,(int)mgpp->osrc_layer_id
         ,(int)mgpp->fdst_layer_id
         );
    }

    for (l_tween_frame_nr = mgpp->range_from +1; l_tween_frame_nr < mgpp->range_to; l_tween_frame_nr++)
    {
      gint32  l_tween_tmp_image_id;
      gdouble l_current_step;
      
      mgpp->master_progress = 0.0;
      mgpp->layer_progress_step = 0.5;


      if(l_tween_layer_id >= 0)
      {
        /* delete the previous handled tween frame image in memory. (that is already saved to disk
         * we keep only the last one opened)
         */
        gap_image_delete_immediate(gimp_drawable_get_image(l_tween_layer_id));
      }
      
      l_current_step = l_tween_frame_nr - mgpp->range_from;
      if(ainfo_ptr->new_filename != NULL)
      {
        g_free(ainfo_ptr->new_filename);
      }
      ainfo_ptr->new_filename = gap_lib_alloc_fname(ainfo_ptr->basename,
                                          l_tween_frame_nr,
                                          ainfo_ptr->extension);
      /* progress and cancel handling */
      if(mgpp->do_progress)
      {
         gdouble percentage;
         gdouble nTweenFramesDone;
         gdouble nTweenFramesTotal;
         
         nTweenFramesDone = l_tween_frame_nr - mgpp->range_from;
         nTweenFramesTotal = mgpp->range_to - mgpp->range_from;
         
         percentage = nTweenFramesDone / nTweenFramesTotal;
         if(gap_debug)
         {
           printf("\nTWEEN_PROGRESS: nTweenFramesDone:%.0f nTweenFramesTotal:%.0f percentage:%.5f\n\n"
             , (float)nTweenFramesDone
             , (float)nTweenFramesTotal
             , (float)percentage
             );
         }
         if(mgpp->master_progress_callback_fptr != NULL)
         {
           (*mgpp->master_progress_callback_fptr)( nTweenFramesDone
                                                 , nTweenFramesTotal
                                                 , ainfo_ptr->new_filename
                                                 , mgpp->callback_data_ptr
                                                 );           
         }
         if(cancelFlagPtr != NULL)
         {
           if (*cancelFlagPtr == TRUE)
           {
              break;
           }           
         }
      }



      if(gap_debug)
      {
        printf("gap_morph_render_frame_tweens creating tween:%s\n"
           ,ainfo_ptr->new_filename
           );
      }

      if (mgpp->overwrite_flag != TRUE)
      {
        if(g_file_test(ainfo_ptr->new_filename, G_FILE_TEST_EXISTS))
        {
          l_tween_layer_id = -1;
          p_error_message_with_filename(mgpp->run_mode, _("file: %s already exists"), ainfo_ptr->new_filename);
          break;
        }
      }

      l_tween_layer_id = gap_morph_render_one_of_n_tweens(mgpp, total_steps, l_current_step);
      l_tween_tmp_image_id = gimp_drawable_get_image(l_tween_layer_id);
      if(gap_debug)
      {
        printf("gap_morph_render_frame_tweens saving tween:%s :%d\n"
           ,ainfo_ptr->new_filename
           ,(int)l_tween_tmp_image_id
           );
      }
      l_rc = gap_lib_save_named_frame(l_tween_tmp_image_id, ainfo_ptr->new_filename);
      if(ainfo_ptr->new_filename != NULL)
      {
        g_free(ainfo_ptr->new_filename);
        ainfo_ptr->new_filename = NULL;
      }
      if (l_rc < 0)
      {
        l_tween_layer_id = -1;
        p_error_message_with_filename(mgpp->run_mode, _("file: %s save failed"), ainfo_ptr->new_filename);
        break;
      }

    }
    gap_image_delete_immediate(gimp_drawable_get_image(mgpp->osrc_layer_id));
    gap_image_delete_immediate(gimp_drawable_get_image(mgpp->fdst_layer_id));
  }

  return(l_tween_layer_id);

}  /* end gap_morph_render_frame_tweens */

