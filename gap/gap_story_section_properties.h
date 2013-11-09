/*  gap_story_section_properties.h
 *
 *  This module handles GAP storyboard dialog section properties window.
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

/* revision history:
 * version 1.3.26a; 2004/02/18  hof: created
 */

#ifndef _GAP_STORY_SECTION_PROPERTIES_H
#define _GAP_STORY_SECTION_PROPERTIES_H

#include "libgimp/gimp.h"
#include "gap_story_main.h"

void           gap_story_spw_properties_dialog (GapStoryBoard *stb, GapStbTabWidgets *tabw);

void           gap_story_spw_switch_to_section(GapStbSecpropWidget *spw, GapStoryBoard *stb_dst);

#endif
