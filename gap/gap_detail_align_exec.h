/*  gap_detail_align_exec.h
 *    This filter locates the position of a small 
 *    outside the selction into the selected area.
 *    It was implemented for fixing small pixel defects of my video camera sensor
 *    and is intended to be used as filter when processing video frames
 *    that are shot by such faulty cameras and typically runs
 *    as filtermacro. Therefore the selection can be provided via an external image
 *    or as path vectors via an extrernal SVG file.
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

#ifndef _GAP_DETAIL_ALIGN_EXEC_H
#define _GAP_DETAIL_ALIGN_EXEC_H


#include "config.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <gtk/gtk.h>
#include <libgimp/gimp.h>
#include <libgimp/gimpui.h>

#include "gap_libgapbase.h"
#include "gap_image.h"
#include "gap_layer_copy.h"
#include "gap_arr_dialog.h"

#include "gap-intl.h"


#define GAP_DETAIL_TRACKING_XML_ALIGNER_PLUG_IN_NAME      "gap-detail-tracking-xml-aligner"
#define GAP_EXACT_ALIGNER_PLUG_IN_NAME                    "gap-exact-aligner"
#define GAP_EXACT_ALIGNER_PLUG_IN_NAME_BINARY             "gap_exact_aligner"

#define GAP_EXACT_ALIGNER_TARGET_PATH_NAME                "TARGET"
#define GAP_EXACT_ALIGNER_SRC_PATH_NAME                   "SRC"
#define GAP_EXACT_ALIGNER_RE_TRACED_PATH_NAME             "ReTraced"  /* used for debug fine tuning */
#define GAP_EXACT_ALIGNER_USRC_PATH_NAME                  "USRC"      /* untuned src for debug */



#define GAP_EXACT_ALIGNER_REF_LAYER_NAME                  "REF"


#define POINT_ORDER_MODE_31_42   0
#define POINT_ORDER_MODE_21_43   1
#define POINT_ORDER_MODE_1234_1234   2


typedef struct XmlAlignValues {
   gint32     framePhase;
   gdouble    transformPrecision;           /* precision in pixels for calculating the perspective transformation matrix (0.0 to 1.0) */
   gdouble    transformPrecisionThreshold;  /* for fine tuning purpose (use perespective coords with precision lower than threshold) */
   char       moveLogFile[1600];
} XmlAlignValues;



gint32      gap_detail_xml_align(gint32 drawableId, XmlAlignValues *xaVals);
void        gap_detail_xml_align_get_values(XmlAlignValues *xaVals);
gboolean    gap_detail_xml_align_dialog(XmlAlignValues *xaVals);

gint32      gap_detail_exact_align_via_4point_path(gint32 image_id, gint32 activeDrawableId
                 ,gint32 pointOrder, GimpRunMode runMode);



#endif
