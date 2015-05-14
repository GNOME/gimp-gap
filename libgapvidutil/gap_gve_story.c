/* gap_gve_story.c
 *
 *  GAP video encoder tool procedures for STORYBOARD file based video encoding
 *  This module is the Storyboard processor that reads instructions from the storyboard
 *  file, fetches input frames, and renders composite video frames
 *  according to the instructions in the storyboard file.
 *
 *  The storyboard processor is typically used to:
 *   - check storyboard syntax and deliver information (number of total frames)
 *     for the master encoder dialog.
 *   - render the composite video frame at specified master frame number
 *     (is called as frame fetching utility by all encoders)
 *
 *   The storyboard processor provides fuctionality to mix audiodata,
 *   this is usually done in the master encoder dialog (before starting the selected encoder)
 *
 */

/*
 * 2006.04.10  hof  - use procedure gap_story_file_calculate_render_attributes
 *                    p_transform_and_add_layer
 * 2005.01.12  hof  - audio processing has new flag: create_audio_tmp_files
 *                  - no more checks and constraints for valid video/audio ranges
 *                    adressing illegal range delivers black frames (or audio silence)
 * 2004.09.25  hof  - bugfix: made gap_gve_story_fetch_composite_image_or_chunk
 *                    basically work when dont_recode_flag is set (for lossles MPEG cut)
 * 2004.09.12  hof  - replaced parser by the newer one from libgapstory.a
 *                    (the new parser supports named parameters)
 * 2004.07.24  hof  - added step_density parameter
 * 2004.05.17  hof  - integration into gimp-gap project
 *                    Info: gimp-gap has already another storyboard parser module
 *                     gap/gap_story_file.c, but that module can not handle
 *                     level 2 features (multiple tracks, and transition effects,
 *                     and audio processing)
 *                    In the future there should be only one common parser ...
 *
 * 2004.02.25  hof  - extended Storyboard Syntax  "VID_PREFERRED_DECODER"
 * 2003.05.29  hof  - gap_gve_story_fetch_composite_image_or_chunk (for dont_recode_flag)
 * 2003.04.20  hof  - Support deinterlace and exact_seek features for VID_PLAY_MOVIE
 * 2003.04.15  hof  - minor bugfixes
 * 2003.04.05  hof  - use GVA framecahe
 * 2003.03.08  hof  - buffered audio processing
 * 2003.03.04  hof  - added Videofile (MOVIE) audio support
 * 2003.02.04  hof  - added Videofile (MOVIE) frames support
 * 2003.01.09  hof  - support for video encoding of single multilayer images
 * 2002.11.16  hof  - created procedures for storyboard file processing
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

#include <config.h>

static int dummy;  /* code was moved to gap/gap_story_render_processor.c */
