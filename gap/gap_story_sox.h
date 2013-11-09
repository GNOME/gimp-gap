/* gap_story_sox.h
 *    Headers for Audio resampling Modules
 */
/*
 * Copyright
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

#ifndef GAP_STORY_SOX_H
#define GAP_STORY_SOX_H

#include <config.h>

/* GIMP includes */
#include "gtk/gtk.h"
#include "gap-intl.h"
#include "libgimp/gimp.h"

#define GAP_STORY_SOX_DEFAULT_UTIL_SOX           "sox"
#define GAP_STORY_SOX_DEFAULT_UTIL_SOX_OPTIONS   " \"$IN\"  -t wav  \"$OUT\" rate -h $RATE "

void   gap_story_sox_exec_resample(char *in_audiofile
                      ,char *out_audiofile
                      ,gint32 samplerate
                      ,char *util_sox
                      ,char *util_sox_options
                      );

#endif
