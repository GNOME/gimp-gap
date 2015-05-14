/* gap_libgapstory.h
 * 1997.11.01 hof (Wolfgang Hofer)
 *
 * GAP ... Gimp Animation Plugins
 *
 * Master includefile for the library libgapstory (Storyboard SYNTAX AND  PARSER)
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
 * 2.1.0a  2004/09/12   hof: created
 */

#ifndef _LIBGAPSTORY_H
#define _LIBGAPSTORY_H

#include "libgimp/gimp.h"

/* libgapstory depends on libgimpgap */
#include "gap_libgimpgap.h"

#include "gap_story_file.h"
#include "gap_story_processor.h"
#include "gap_story_sox.h"
#include "gap_story_syntax.h"

#endif
