/* gap_fg_matting_dialog.h
 * 2011.10.05 hof (Wolfgang Hofer)
 *
 * GAP ... Gimp Animation Plugins
 *
 * This Module contains:
 * - stuff for the GUI dialog
 *   of the foreground selection via alpha matting plug-in.
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

#ifndef GAP_FG_MATTING_DIALOG_H
#define GAP_FG_MATTING_DIALOG_H

#include "libgimp/gimp.h"

/*
 * DIALOG and callback stuff
 */
gboolean  gap_fg_matting_dialog(GapFgExtractValues *fgVals);
void      gap_fg_matting_init_default_vals(GapFgExtractValues *fgVals);


#endif /* GAP_FG_MATTING_DIALOG_H */
