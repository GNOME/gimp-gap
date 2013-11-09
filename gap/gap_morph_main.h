/*  gap_morph_main.h
 *
 *  creation of morphing animations (transform source image into des. image) by Wolfgang Hofer
 *  2004/02/11
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
 * version 1.3.15a; 2004/02/12  hof: created
 */

#ifndef _GAP_MORPH_MAIN_H
#define _GAP_MORPH_MAIN_H

#include "libgimp/gimp.h"
#include "gap_lib.h"
#include <gtk/gtk.h>
#include <libgimp/gimp.h>
#include <libgimp/gimpui.h>
#include "gap_pview_da.h"

/* render_mode Radio Buttons */
#define   GAP_MORPH_RENDER_MODE_MORPH    0
#define   GAP_MORPH_RENDER_MODE_WARP     1    

#define   GAP_MORPH_WORKPOINT_FILENAME_MAX_LENGTH     1024 

#define   GAP_MORPH_WORKPOINT_EXTENSION "morphpoints"

/* Function Typedefs */
typedef  void       (*t_progress_callback_fptr)(gdouble percentage, gpointer data);
typedef  void       (*t_master_progress_callback_fptr)(gdouble numDone, gdouble numTotal, const char *filename, gpointer data);

typedef struct GapMorphWorkPoint { /* nickname: wp */
     gdouble fdst_x;   /* final dest koord (as set by user for last dest. frame) */
     gdouble fdst_y;
     gdouble osrc_x;   /* start source koord (as set by user for the 1.st frame) */
     gdouble osrc_y;

     gdouble dst_x;    /* koord trans */
     gdouble dst_y;
     gdouble src_x;    /* osrc_x scaled to koords of current (dest) frame */
     gdouble src_y;

     void *next;
     
     /* for calculations per pixel */
     gdouble  warp_weight;
     gdouble  gravity;
     gdouble  sqr_dist;
     gdouble  dist;
     gdouble  inv_dist;    /* 1 / sqr_distance */
     gdouble  angle_deg;
     gint     xy_relation;
     gint     sek_idx;
     gboolean is_alive;
     void    *next_selected;
     void    *next_sek;

     /* for automatically workpoint genration */
     gdouble  locateColordiff; /* 0.0 exact match up to theoretical 1.0 */
     
  } GapMorphWorkPoint;


typedef struct GapMorphGlobalParams  { /* nickname: mgpp */
  GimpRunMode  run_mode;
  gint32       image_id;

  gint32              tween_steps;
  gint32              fdst_layer_id;
  gint32              osrc_layer_id;


  GapMorphWorkPoint  *master_wp_list;

  char                workpoint_file_lower[GAP_MORPH_WORKPOINT_FILENAME_MAX_LENGTH];
  char                workpoint_file_upper[GAP_MORPH_WORKPOINT_FILENAME_MAX_LENGTH];

  gboolean            create_tween_layers;       /* FALSE: operate on existing layers only */
  gboolean            have_workpointsets;        /* FALSE: use the default workpointset master_wp_list
                                                  * TRUE: use lower_wp_list and upper_wp_list
                                                  *       foreach handled frame the
                                                  *       lower and upper list are fetched from 
                                                  *       best matching workpointfile.
                                                  *       (using the numberpart of the filename)
                                                  */
  gboolean            use_quality_wp_selection;
  gboolean            use_gravity;
  gdouble             gravity_intensity;     /* 1.0 upto 5 (gravity power) */
  gdouble             affect_radius;         /* distortion pixelradius (0 == no gravity) */

  gint32              render_mode;
  gboolean            do_progress;
  gdouble             master_progress;
  gdouble             layer_progress_step;

  gdouble             tween_mix_factor;  /* 0.0 upto 1.0 wher 0.0 gives source layer 1.0 dest layer as resut */
  gint32              range_from;
  gint32              range_to;
  gboolean            overwrite_flag;
  gboolean            append_flag;
  gboolean            do_simple_fade;   /* bypass morph algortihm when renderiing tweens and use simple fade instead */
  
  gdouble             edgeColordiffThreshold;
  gdouble             locateColordiffThreshold;
  gint32              locateDetailShapeRadius;
  gint32              locateDetailMoveRadius;
  gint32              numWorkpoints;
  gint32              numOutlinePoints;

  /* additional stuff (only relevant for tween frame rendering) */
  gint32              master_tween_steps;
  gboolean            create_tweens_in_subdir;
  char                tween_subdir[GAP_MORPH_WORKPOINT_FILENAME_MAX_LENGTH];

  t_master_progress_callback_fptr master_progress_callback_fptr;
  t_progress_callback_fptr        progress_callback_fptr;
  gpointer                        callback_data_ptr;
} GapMorphGlobalParams;

typedef struct GapMorphWarpCoreAPI  { /* nickname: wcap */
  GapMorphWorkPoint *wp_list;
  gboolean      use_quality_wp_selection;
  gboolean      use_gravity;
  gdouble       gravity_intensity;
  gdouble       affect_radius;         /* distortion pixelradius (0 == no gravity) */
  gdouble       sqr_affect_radius;
  
  
  gdouble       scale_x;
  gdouble       scale_y;
  gboolean      printf_flag;
  
}  GapMorphWarpCoreAPI;

#endif

