/*  gap_accel_da.h
 *
 *  This module handles GAP acceleration characteristics drawing area.
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
 * version 2.7.0; 2010/02/06  hof: created
 */

#ifndef _GAP_ACCEL_DA_H
#define _GAP_ACCEL_DA_H

#include <gtk/gtk.h>
#include <libgimp/gimp.h>
#include <libgimp/gimpui.h>


typedef struct GapAccelWidget
{
  GtkObject *adj;
  GtkWidget *da_widget;                    /* the graph drawing_area */
  gint32     accelerationCharacteristic;
  gint       width;
  gint       height;
  gint       pixWidth;
  gint       pixHeight;
  gboolean   isButton1Pressed;
  
} GapAccelWidget;

GapAccelWidget   *gap_accel_new(gint width, gint height, gint32 accelerationCharacteristic);
GapAccelWidget   *gap_accel_new_with_adj(gint width, gint height, gint32 accelerationCharacteristic, GtkObject *adj);
void              gap_accel_render (GapAccelWidget *accel_wgt, gint32 accelerationCharacteristic);

#endif
