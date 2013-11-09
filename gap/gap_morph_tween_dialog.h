/*  gap_morph_tween_dialog.h
 *
 *  This module handles the GAP morph tween dialog
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

/* revision history:
 * version 2.1.0a; 2004/03/31  hof: created
 */

#ifndef _gap_morph_tween_dialog_H
#define _gap_morph_tween_dialog_H

#include "libgimp/gimp.h"
#include "gap_libgimpgap.h"
#include "gap_morph_main.h"
#include "gap_locate.h"
#include "gap_libgimpgap.h"

#define GAP_MORPH_ONE_TWEEN_PLUGIN_NAME    "plug_in_gap_morph_one_tween"
#define GAP_MORPH_ONE_TWEEN_HELP_ID        "plug-in-gap-morph-one-tween"

#define GAP_MORPH_TWEEN_PLUGIN_NAME    "plug_in_gap_morph_tween"
#define GAP_MORPH_TWEEN_HELP_ID        "plug-in-gap-morph-tween"

#define GAP_MORPH_WORKPOINTS_PLUGIN_NAME   "plug_in_gap_morph_workpoints"
#define GAP_MORPH_WORKPOINTS_HELP_ID       "plug-in-gap-morph-workpoints"

gboolean   gap_morph_one_tween_dialog(GapMorphGlobalParams *mgpp);
gint32     gap_morph_frame_tweens_dialog(GapAnimInfo *ainfo_ptr, GapMorphGlobalParams *mgpp);
gint32     gap_morph_generate_frame_tween_workpoints_dialog(GapAnimInfo *ainfo_ptr
                   , GapMorphGlobalParams *mgpp);
#endif
