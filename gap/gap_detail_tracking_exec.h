/*  gap_detail_tracking_exec.h
 *    This filter locates the position of one or 2 small areas
 *    of a reference layer within a target layer and logs the coordinates
 *    as XML file. It is intended to track details in a frame sequence.
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
 * along with this program; if not, write to the Free Software
 * <http://www.gnu.org/licenses/>.
 */

/* Revision history
 *  (2011/12/01)  2.7.0       hof: created
 */

#ifndef _GAP_DETAIL_TRACKING_EXEC_H
#define _GAP_DETAIL_TRACKING_EXEC_H


#include "config.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <gtk/gtk.h>
#include <libgimp/gimp.h>
#include <libgimp/gimpui.h>

#include "gap_libgapbase.h"
#include "gap_locate.h"
#include "gap_colordiff.h"
#include "gap_image.h"
#include "gap_layer_copy.h"
#include "gap_arr_dialog.h"

#include "gap-intl.h"



#define GAP_DETAIL_FRAME_HISTORY_INFO     "GAP_DETAIL_FRAME_HISTORY_INFO"
#define GAP_DETAIL_TRACKING_PLUG_IN_NAME  "gap-detail-tracking"

typedef struct FilterValues {
   gint32     refShapeRadius;
   gint32     targetMoveRadius;
   gdouble    loacteColodiffThreshold;

   gboolean   coordsRelToFrame1;   /* subtract coords of frame 1 when logging coords */
   gint32     offsX;               /* add this value when logging coords */
   gint32     offsY;
   gdouble    offsRotate;          /* additional rotation angle, to be added in all controlpoints */
   gboolean   enableScaling;       /* on: use rotation and scaling  off: roate only  */
   gboolean   bgLayerIsReference;
   gboolean   removeMidlayers;     /* on: keep 2 top layers and Bg layer, remove other layers  off: keep all layers  */
   char       moveLogFile[1600];
} FilterValues;

typedef struct PixelCoords
{
  gboolean  valid;
  gint32  px;
  gint32  py;
} PixelCoords;


typedef struct FrameHistInfo
{
  gint32       workImageId;
  gint32       frameNr;      /* last handled frameNr */
  PixelCoords  startCoords;  /* coords of first processed frame */
  PixelCoords  startCoords2; /* 2nd detail coords of first processed frame */

  PixelCoords  prevCoords;   /* coords of the previous processed frame */
  PixelCoords  prevCoords2;  /* 2nd detail coords of the previous processed frame */

  gint32       lostTraceCount;
} FrameHistInfo;



/* -----------------------------------
 * gap_track_detail_on_top_layers
 * -----------------------------------
 * This procedure is typically called
 * on the snapshot image created by the Player.
 * This image has one layer at the first snapshot
 * and each further snapshot adds one layer on top of the layerstack.
 *
 * The start is detected when the image has only one layer.
 * optionally the numer of layers can be limted
 * to 2 (or more) layers.
 */
gint32      gap_track_detail_on_top_layers(gint32 imageId, gboolean doProgress, FilterValues *valPtr);
void        gap_detail_tracking_get_values(FilterValues *fiVals);
gboolean    gap_detail_tracking_dialog(FilterValues *fiVals);


/* procedure variants intended for use in the player plug-in */
gint32      gap_track_detail_on_top_layers_lastvals(gint32 imageId);
gboolean    gap_detail_tracking_dialog_cfg_set_vals(gint32 imageId);



#endif
