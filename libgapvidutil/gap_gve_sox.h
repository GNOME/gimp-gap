/* gap_gve_sox.h
 *    Headers for Audio resampling Modules
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

#ifndef GAP_GVE_SOX_H
#define GAP_GVE_SOX_H

#include <config.h>

/* GIMP includes */
#include "gtk/gtk.h"
#include "gap-intl.h"
#include "libgimp/gimp.h"

#include "gap_story_sox.h"

#include "gap_gvetypes.h"

void   gap_gve_sox_save_config(GapGveCommonValues *cval);
void   gap_gve_sox_init_config(GapGveCommonValues *cval);
void   gap_gve_sox_init_default(GapGveCommonValues *cval);
gint   gap_gve_sox_chk_and_resample(GapGveCommonValues *cval);

#endif
