/*  gap_enc_ffmpeg_par.h
 *
 *  This module handles GAP ffmpeg videoencoder parameter files
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
 * version 2.1.0a;  2005/03/24  hof: created (based on procedures of the gap_vin module)
 */

#ifndef _GAP_ENC_FFMPEG_PAR_H
#define _GAP_ENC_FFMPEG_PAR_H

#include "libgimp/gimp.h"
#include "gap_enc_ffmpeg_main.h"

#define GAP_VIDEO_ENCODER_PRESET_DIR "video_encoder_presets"

int   gap_ffpar_set(const char *filename, GapGveFFMpegValues *ffpar_ptr);
void  gap_ffpar_get(const char *filename, GapGveFFMpegValues *ffpar_ptr);

gboolean  gap_ffpar_isValidPresetFile(const char *fullPresetFilename);
GapGveFFMpegValues * gap_ffpar_getPresetList();


#endif
