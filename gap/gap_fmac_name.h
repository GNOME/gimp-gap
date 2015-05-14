/* gap_fmac_name.h
 * 2006.12.19 hof (Wolfgang Hofer)
 *
 * GAP ... Gimp Animation Plugins
 *
 * This Module contains:
 * - filtermacro name definitions and filename convention procedures.
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

#ifndef GAP_FMAC_NAME_H
#define GAP_FMAC_NAME_H

#include "libgimp/gimp.h"

#define GAP_FMACNAME_PLUG_IN_NAME_FMAC_VARYING    "plug_in_filter_macro_varying"
#define GAP_FMACNAME_PLUG_IN_NAME_FMAC            "plug_in_filter_macro"

gboolean         gap_fmac_chk_filtermacro_file(const char *filtermacro_file);
char *           gap_fmac_get_alternate_name(const char *filtermacro_file);


#endif
