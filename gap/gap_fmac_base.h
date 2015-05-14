/* gap_fmac_base.h
 * 2006.12.11 hof (Wolfgang Hofer)
 *
 * GAP ... Gimp Animation Plugins
 *
 * This Module contains:
 * - filtermacro execution backend procedures.
 *
 * WARNING:
 * filtermacros are a temporary solution, useful for animations
 * but do not expect support for filtermacros in future releases of GIMP-GAP
 * because GIMP may have real makro features in the future ...
 *
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

#ifndef GAP_FMAC_BASE_H
#define GAP_FMAC_BASE_H

#include "libgimp/gimp.h"

gint             gap_fmac_execute(GimpRunMode run_mode, gint32 image_id, gint32 drawable_id
                        , const char *filtermacro_file1
                        , const char *filtermacro_file2
                        , gdouble current_step
                        , gint32  total_steps
                        );


#endif
