/* gap_libgapvidutil.h
 *    Common Types for GIMP/GAP Video Encoders
 */

/*
 * Changelog:
 * 2004/05/05 v2.1.0a:  created
 */

/*
 * Copyright
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

#ifndef GAP_LIBVIDUTIL_H
#define GAP_LIBVIDUTIL_H

#include <config.h>

/* SYTEM (UNIX) includes */
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <gap-intl.h>

#include <gtk/gtk.h>
#include <libgimp/gimp.h>
#include <libgimp/gimpui.h>




/* GAP Endcoder types */
#include "gap_gvetypes.h"

#include "gap_libgimpgap.h"

#include "gap_gve_raw.h"
#include "gap_gve_jpeg.h"
#include "gap_gve_xvid.h"

#include "gap_gve_misc_util.h"
#include "gap_gve_story.h"
#include "gap_gve_sox.h"

#endif
