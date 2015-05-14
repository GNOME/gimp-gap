/*  gap_fg_matting_main.h
 *    foreground extraction based on alpha matting algorithm.
 *    Render transparancy for a layer based on a tri-map drawable (provided by the user).
 *    The tri-map defines what pixels are FOREGROUND (white) BACKGROUND (black) or UNDEFINED (gray).
 *    foreground extraction affects the UNDEFINED pixels and calculates transparency for those pixels.
 *
 *    This module provides main data structures of the plug-in
 *  2011/10/05
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
 *  (2011/10/05)  2.7.0       hof: created
 */
#ifndef GAP_FG_MATTING_MAIN_H
#define GAP_FG_MATTING_MAIN_H

#include <gtk/gtk.h>
#include <libgimp/gimp.h>
#include <libgimp/gimpui.h>


/* Defines */
#define PLUG_IN_NAME        "plug-in-foreground-extract-matting"
#define PLUG_IN_PRINT_NAME  "Foreground Extract"
#define PLUG_IN_IMAGE_TYPES "RGB*"
#define PLUG_IN_AUTHOR      "Wolfgang Hofer (hof@gimp.org)"
#define PLUG_IN_COPYRIGHT   "Jan Ruegg / Wolfgang Hofer"
#define PLUG_IN_HELP_ID     "plug-in-foreground-extract-matting"

#define PLUG_IN2_NAME        "plug-in-selection-to-foreground-layer"
#define PLUG_IN2_PRINT_NAME  "Selection To Foreground Extract"
#define PLUG_IN2_IMAGE_TYPES "RGB*"
#define PLUG_IN2_AUTHOR      "Wolfgang Hofer (hof@gimp.org)"
#define PLUG_IN2_COPYRIGHT   "Wolfgang Hofer"
#define PLUG_IN2_HELP_ID     "plug-in-selection-to-foreground-layer"



typedef struct GapFgExtractValues {  /* fgValPtr */
  gint32 input_drawable_id;
  gint32 tri_map_drawable_id;
  gboolean create_result;
  gboolean create_layermask;
  gboolean lock_color;
  gdouble  colordiff_threshold;

} GapFgExtractValues;



typedef struct GapFgSelectValues {  /* fsValPtr */
  gint32 input_drawable_id;
  gint32 tri_map_drawable_id;
  gint     innerRadius;
  gint     outerRadius;
  gboolean create_layermask;
  gboolean lock_color;
  gdouble  colordiff_threshold;

} GapFgSelectValues;


#endif /* GAP_FG_MATTING_MAIN_H */
