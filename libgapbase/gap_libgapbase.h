/* gap_libgapbase.h
 * 2009.03.07 hof (Wolfgang Hofer)
 *
 * GAP ... Gimp Animation Plugins
 *
 * Master includefile for the library libgapbase
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
 * 2.5.0  2009.03.07   hof: created
 */

#ifndef _LIBGAPBASE_H
#define _LIBGAPBASE_H

#include "libgimp/gimp.h"

#include "gap_val_file.h"
#include "gap_file_util.h"
#include "gap_base.h"
#include "gap_timm.h"


/* GAP_BASE_MIX_VALUE  0.0 <= factor <= 1.0
 *  result is a  for factor 0.0
 *            b  for factor 1.0
 *            mix for factors inbetween
 */
#define GAP_BASE_MIX_VALUE(factor, a, b) ((a * (1.0 - factor)) +  (b * factor))


#endif
