/*  gap_fg_matting_exec.h
 *    foreground extraction based on alpha matting algorithm.
 *    Render transparancy for a layer based on a tri-map drawable (provided by the user).
 *    The tri-map defines what pixels are FOREGROUND (white) BACKGROUND (black) or UNDEFINED (gray).
 *    foreground extraction affects the UNDEFINED pixels and calculates transparency for those pixels.
 *
 *    This module provides the top level processing procedures
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
#ifndef GAP_FG_MATTING_EXEC_H
#define GAP_FG_MATTING_EXEC_H

#include <gtk/gtk.h>
#include <libgimp/gimp.h>
#include <libgimp/gimpui.h>

#include "gap_fg_matting_main.h"


gint
gap_fg_matting_exec_apply_run (gint32 image_id, gint32 drawable_id
                             , gboolean doProgress, gboolean doFlush
                             , GapFgExtractValues *fgValPtr);

gint
gap_fg_from_selection_exec_apply_run (gint32 image_id, gint32 drawable_id
                             , gboolean doProgress, gboolean doFlush
                             , GapFgSelectValues *fsValPtr);

#endif /* GAP_FG_MATTING_EXEC_H */
