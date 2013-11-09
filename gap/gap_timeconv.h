/*  gap_timeconv.h
 *
 *  This module handles conversions framenumber/Rate  --> timestring (mm:ss:msec)
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
 * version 1.3.19a; 2003/09/06  hof: added more converter procedures for audio
 * version 1.3.14c; 2003/06/14  hof: created
 */


#ifndef _GAP_TIMECONV_H
#define _GAP_TIMECONV_H

#include "libgimp/gimp.h"

void    gap_timeconv_msecs_to_timestr(gint32 tmsec, gchar *txt, gint txt_size);
void    gap_timeconv_framenr_to_timestr( gint32 framenr, gdouble framerate, gchar *txt, gint txt_size);
void    gap_timeconv_samples_to_timestr( gint32 samples, gdouble samplerate, gchar *txt, gint txt_size);
gdouble gap_timeconv_samples_to_frames( gint32 samples, gdouble samplerate, gdouble framerate);


#endif
