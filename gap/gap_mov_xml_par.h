/* gap_mov_xml_par.h
 * 2011.03.09 hof (Wolfgang Hofer)
 *
 * GAP ... Gimp Animation Plugins
 *
 * Move Path : XML parameterfile load and save procedures.
 * The XML parameterfile contains full information of all parameters
 * available in the move path feature including:
 *  version
 *  frame description, moving object description
 *  tweens, trace layer, bluebox settings and controlpoints.
 *
 * Note that the old pointfile format is still supported
 * but not handled here.
 * (the old pointfile format is limited to information about the controlpoints that build the path
 * see gap_mov_exec module for load/save support of the old pointfile format)
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

/* revision history:
 * 2011.03.09  hof: created.
 */
#ifndef _GAP_MOV_XML_PAR_H
#define _GAP_MOV_XML_PAR_H

#include "libgimp/gimp.h"
#include "gap_mov_dialog.h"


gboolean  gap_mov_xml_par_load(const char *filename, GapMovValues *productiveValues
                              ,gint32 actualFrameWidth, gint32 actualFrameHeight);
gint      gap_mov_xml_par_save(char *filename, GapMovValues *pvals);

#endif

