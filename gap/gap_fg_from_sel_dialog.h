/* gap_fg_from_sel_dialog.h
 * 2011.11.15 hof (Wolfgang Hofer)
 *
 * GAP ... Gimp Animation Plugins
 *
 * This Module contains:
 * - stuff for the GUI dialog
 *   of the foreground extraction from current selection via alpha matting plug-in.
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
 * along with this program; if not, see
 * <http://www.gnu.org/licenses/>.
 */

#ifndef GAP_FG_FROM_SEL_H
#define GAP_FG_FROM_SEL_H

#include "libgimp/gimp.h"

/*
 * DIALOG and callback stuff
 */
gboolean  gap_fg_from_sel_dialog(GapFgSelectValues *fsVals);
void      gap_fg_from_sel_init_default_vals(GapFgSelectValues *fsVals);


#endif /* GAP_FG_FROM_SEL_H */
