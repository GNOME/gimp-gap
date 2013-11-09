/*  gap_fg_regions.h
 *    This module contains gimp pixel region processing utilities
 *    intended as helper procedures for preprocessing and postprocessing
 *    for the foreground sextract plug-in.
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
 * along with this program; if not, write to the Free Software
 * <http://www.gnu.org/licenses/>.
 */

/* Revision history
 *  (2011/10/05)  2.7.0       hof: created
 */
#ifndef GAP_FG_REGIONS_H
#define GAP_FG_REGIONS_H

#include <gtk/gtk.h>
#include <libgimp/gimp.h>
 


void       gap_fg_rgn_copy_alpha_channel(GimpDrawable *srcDrawable
                   ,GimpDrawable *resultDrawable
                   );

void       gap_fg_rgn_copy_rgb_channels(GimpDrawable *srcDrawable
                   ,GimpDrawable *resultDrawable
                   );

void       gap_fg_rgn_tri_map_normalize(GimpDrawable *drawable
                   , gint32 maskId
                   );
void       gap_fg_rgn_normalize_alpha_channel(GimpDrawable *drawable);


#endif  /* GAP_FG_REGIONS_H  */
