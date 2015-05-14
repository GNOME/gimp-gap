/*  gap_match.h
 *
 * GAP ... Gimp Animation Plugins
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
 * version 0.97.00  1998.10.14  hof: - created module 
 */
 

#ifndef _GAP_MATCH_H
#define _GAP_MATCH_H

#include "libgimp/gimp.h"

#define GAP_MTCH_EQUAL       0
#define GAP_MTCH_START       1
#define GAP_MTCH_END         2
#define GAP_MTCH_ANYWHERE    3
#define GAP_MTCH_NUMBERLIST  4
#define GAP_MTCH_INV_NUMBERLIST  5
#define GAP_MTCH_ALL_VISIBLE  6

int  gap_match_string_is_empty (const char *str);
void gap_match_substitute_framenr (char *buffer, int buff_len, char *new_layername, long curr);

int  gap_match_number(gint32 layer_id, const char *pattern);
int  gap_match_name(const char *layername, const char *pattern, gint32 mode, gint32 case_sensitive);
int  gap_match_layer(gint32 layer_idx, const char *layername, const char *pattern,
                  gint32 mode, gint32 case_sensitive, gint32 invert,
                  gint nlayers, gint32 layer_id);

#endif
