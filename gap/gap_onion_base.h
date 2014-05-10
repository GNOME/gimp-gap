/* gap_onion_base.h
 * 2003.05.22 hof (Wolfgang Hofer)
 *
 * GAP ... Gimp Animation Plugins
 *
 * This Module contains ONION Skin Layers worker Procedures
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

/* revision history:
 * version 1.3.16c; 2003.07.09   hof: created (as extract of the gap_onion_worker.c module)
 * version 1.2.2a;  2001.12.10   hof: created
 */

#ifndef _GAP_ONION_BASE_H
#define _GAP_ONION_BASE_H


#include "config.h"

/* SYTEM (UNIX) includes */
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <time.h>

/* #include <locale.h> */
#include <gap-intl.h>

#include <gtk/gtk.h>
#include <libgimp/gimp.h>
#include <libgimp/gimpui.h>

#include <gap_vin.h>




#define GAP_ONION_PARASITE_NAME     "gap_onion_skin_layer"

#define GAP_ONION_VISI_FALSE   0
#define GAP_ONION_VISI_TRUE    1
#define GAP_ONION_VISI_TOGGLE  2

#define GAP_ONION_REFMODE_NORMAL   0
#define GAP_ONION_REFMODE_BIDRIECTIONAL_SINGLE   1
#define GAP_ONION_REFMODE_BIDRIECTIONAL_DOUBLE   2

typedef struct GapOnionBaseParasite_data {
   time_t       timestamp;      /* UTC timecode of creation time */
   gint32       tattoo;         /* unique tattoo */
} GapOnionBaseParasite_data;


/* Function Typedefs */
typedef  void    (*GapOnionBaseFptrAddImageToCache)(void *gpp_void
                                           , gint32 framenr
                                           , gint32 image_id
                                           , gint32 layer_id);
typedef  gint32  (*GapOnionBaseFptrFindFrameInImageCache)(void *gpp_void
                                                  , gint32 framenr
                                                  , gint32 *image_id
                                                  , gint32 *layer_id);


/* onion_base procedures */
void    gap_onion_base_mark_as_onionlayer(gint32 layer_id);
gint32  gap_onion_base_check_is_onion_layer(gint32 layer_id);
gint    gap_onion_base_onionskin_visibility(gint32 image_id, gint visi_mode);
gint    gap_onion_base_onionskin_delete(gint32 image_id);
gint    gap_onion_base_onionskin_apply(gpointer gpp
             , gint32 image_id
             , GapVinVideoInfo *vin_ptr
             , long   ainfo_curr_frame_nr
             , long   ainfo_first_frame_nr
             , long   ainfo_last_frame_nr
             , char  *ainfo_basename
             , char  *ainfo_extension
             , GapOnionBaseFptrAddImageToCache        fptr_add_img_to_cache
             , GapOnionBaseFptrFindFrameInImageCache fptr_find_frame_in_img_cache
             , gboolean use_cache
             );

gboolean gap_onion_image_has_oinonlayers(gint32 image_id, gboolean only_visible);
gint32   gap_onion_base_image_duplicate(gint32 image_id);

#endif

