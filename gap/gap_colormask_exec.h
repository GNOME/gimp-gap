/* gap_colormask_exec.h
 *    by hof (Wolfgang Hofer)
 *    color mask filter worker procedures
 *    to set alpha channel for a layer according to matching colors
 *    of color mask (image)
 *  2010/02/21
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
 * version 2.7.0;             hof: created
 */

#ifndef _GAP_COLORMASK_EXEC_H
#define _GAP_COLORMASK_EXEC_H

/* SYTEM (UNIX) includes */
#include <stdio.h>
#include <stdlib.h>

/* GIMP includes */
#include "gtk/gtk.h"
#include "libgimp/gimp.h"

gint32   gap_colormask_apply_to_layer_of_same_size (gint32          dst_layer_id
                                             ,GapColormaskValues *cmaskvals
                                             ,gboolean            doProgress
                                             );
gint32   gap_colormask_apply_to_layer_of_same_size_from_file (gint32 dst_layer_id
                            , gint32                  colormask_id
                            , const char              *filename
                            , gboolean                keepLayerMask
                            , gboolean                doProgress
                            );

gint32   gap_create_or_replace_colormask_layer (gint32 orig_layer_id
                        , GapColormaskValues     *cmaskvals
                        , gboolean                doProgress
                        );
gint32   gap_colormask_apply_by_name (gint32 dst_layer_id
                        , GapColormaskValues     *cmaskvals
                        , gboolean                doProgress
                        );

#endif
