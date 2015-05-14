/*
 * gap_vex_exec.h
 * Video Extract worker procedures
 *   based on gap_vid_api (GVA)
 */

/*
 * Changelog:
 * 2003/02/11 v1.2.1a:  hof  created
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
 * along with this program; if not, see
 * <http://www.gnu.org/licenses/>.
 */

#ifndef GAP_VEX_EXEC
#define GAP_VEX_EXEC

#include "config.h"

/* SYTEM (UNIX) includes */
#include <stdio.h>
#include <stdlib.h>

/* GIMP includes */
#include "gtk/gtk.h"
#include "libgimp/gimp.h"


#include "gap_vex_main.h"


/* -------- PRODUCTIVE WORKER PROCEDURES -----------*/

void        gap_vex_exe_extract_videorange(GapVexMainGlobalParams *gpp);

#endif
