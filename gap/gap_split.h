/* gap_split.h
 * 1997.11.01 hof (Wolfgang Hofer)
 *
 * GAP ... Gimp Animation Plugins
 *
 * Split image to video frames (separate Images on disk)
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
 * 1.3.14a; 2003/05/25   hof: added digits parameter
 * 0.96.00; 1998/07/03   hof: 1.st releas
 */

#ifndef _GAP_SPLIT_H
#define _GAP_SPLIT_H

#include "libgimp/gimp.h"



int gap_split_image(GimpRunMode run_mode,
                             gint32     image_id,
                             gint32     inverse_order,
                             gint32     no_alpha,
                             char      *extension,
                             gint32     only_visible,
                             gint32     copy_properties,
                             gint32     digits
                             );


#endif


