/* gap_gve_xvid.h by Wolfgang Hofer (hof@gimp.org)
 *
 * (GAP ... GIMP Animation Plugins, now also known as GIMP Video)
 *    Routines for MPEG4 (XVID) video encoding
 *    useable for AVI video.
 *    (GimpDrawable <--> MPEG4 Bitmap Data)
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


#ifndef GAP_GVE_XVID_H
#define GAP_GVE_XVID_H

#include <config.h>

/* GIMP includes */
#include "gtk/gtk.h"
#include "libgimp/gimp.h"

typedef struct GapGveXvidValues
{
  gint32  rc_bitrate;                  /* default 900000 */
  gint32  rc_reaction_delay_factor;    /* -1 force internal default */
  gint32  rc_averaging_period;         /* -1 force internal default */
  gint32  rc_buffer;                   /* -1 force internal default */
  gint32  max_quantizer;               /* upper limit for quantize Range 1 BEST Quality 31 Max Compression */
  gint32  min_quantizer;               /* lower limit for quantize Range 1 BEST Quality 31 Max Compression */
  gint32  max_key_interval;            /* max distance for keyframes (I-frames) */

  gint32  quality_preset;              /* -1 explicite set general and motion algorithm flags
                                        * 0..6 set general and motion algorithm flags
                                        *      according to presets, where 0==low quality(fast) 6==best(slow)
                                        */
  gint32  general;                     /* Choose general Encoding Algortihm Flags */
  gint32  motion;                      /* Choose motion Encoding Algortihm Flags */
} GapGveXvidValues;

#ifdef ENABLE_LIBXVIDCORE

/* XVIDlib includes */
#include "xvid.h"
#define GAP_XVID_MAX_ZONES   64



typedef struct GapGveXvidControl
{
 int num_zones;
 xvid_enc_zone_t zones[GAP_XVID_MAX_ZONES];
 xvid_plugin_single_t single;
 xvid_plugin_2pass1_t rc2pass1;
 xvid_plugin_2pass2_t rc2pass2;
 xvid_enc_plugin_t plugins[7];

 xvid_gbl_init_t xvid_gbl_init;
 xvid_enc_create_t xvid_enc_create;

 xvid_enc_frame_t xvid_enc_frame;
 xvid_enc_stats_t xvid_enc_stats;
} GapGveXvidControl;

/* ------------------------------------
 *  gap_gve_xvid_drawable_encode
 * ------------------------------------
 *  in: drawable: Describes the picture to be compressed in GIMP terms.
 *      xvid_encparam: an already initialized XVID_ENC_PARAM struct
 *                     (containing the encoder instance handle,
 *                      must be initialized for Encode -- use gap_gve_xvid_init to get such a struct)
 *      app0_buffer: if != NULL, the content of the APP0-marker to write.
 *      app0_length: the length of the APP0-marker.
 *  out:XVID_size: The size of the buffer that is returned.
 *      keyframe:  TRUE if the encoded frame was a keyframe (I-frame)
 *  returns: guchar *: A buffer, allocated by this routines, which contains
 *                     the compressed JPEG, NULL on error.
 */
guchar *             gap_gve_xvid_drawable_encode(GimpDrawable *drawable
                                         , gint32 *XVID_size
                                         , GapGveXvidControl  *xvid_control
                                         , int *keyframe
                                         , void *app0_buffer, gint32 app0_length
                                         );
void                 gap_gve_xvid_algorithm_preset(GapGveXvidValues *xvid_val);
GapGveXvidControl*   gap_gve_xvid_init(gint32 width, gint32 height, gdouble framerate, GapGveXvidValues *xvid_val);
void                 gap_gve_xvid_cleanup(GapGveXvidControl  *xvid_control);


#endif  /* ENABLE_LIBXVIDCORE */

#endif
