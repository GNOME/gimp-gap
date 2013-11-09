/* gap_onion_dialog.h
 * 2003.05.22 hof (Wolfgang Hofer)
 *
 * GAP ... Gimp Animation Plugins
 *
 * This Module contains GAP Onionskin GUI Procedures
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
 * version 1.3.14a; 2003.05.22   hof: created
 */ 
 
#ifndef _GAP_ONION_DIALOG_H
#define _GAP_ONION_DIALOG_H

#include "gap_onion_main.h"

void   gap_onion_dlg_init_default_values(GapOnionMainGlobalParams *gpp);
gint   gap_onion_dlg_onion_cfg_dialog(GapOnionMainGlobalParams *gpp);

#endif
