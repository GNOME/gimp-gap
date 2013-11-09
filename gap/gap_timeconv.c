/*  gap_timeconv.c
 *
 *  This module handles conversions framenumber/Rate  --> timestring (mm:ss:msec)
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
 * version 1.3.19a; 2003/09/06  hof: added more converter procedures for audio
 * version 1.3.14c; 2003/06/14  hof: created
 */

#include "config.h"

#include <stdio.h>

#include "libgimp/gimp.h"
#include "gap_timeconv.h"
#include "gap-intl.h"

/* -------------------------------
 * gap_timeconv_msecs_to_timestr
 * -------------------------------
 * convert Input milliseconds value
 * into time string formated as mm:ss:msec
 * (minutes:seconds:milliseconds)
 *
 * the caller must provide a pointer to txt buffer
 * where the output is written into at txt_size.
 *
 * the txt buffer size should be at last 12 bytes
 */
void
gap_timeconv_msecs_to_timestr(gint32 tmsec, gchar *txt, gint txt_size)
{
  gint32   tms;
  gint32   tsec;
  gint32   tmin;

  tms = tmsec % 1000;
  tsec = (tmsec / 1000) % 60;
  tmin = tmsec / 60000;

  g_snprintf(txt, txt_size,"%02d:%02d:%03d"
            , (int)tmin, (int)tsec, (int)tms);
}  /* end gap_timeconv_msecs_to_timestr */

/* -------------------------------
 * gap_timeconv_framenr_to_timestr
 * -------------------------------
 * convert Input framenumber and framerate unit frames/sec)
 * into time string formated as mm:ss:msec
 * (minutes:seconds:milliseconds)
 *
 * the caller must provide a pointer to txt buffer
 * where the output is written into at txt_size.
 *
 * the txt buffer size should be at last 12 bytes
 */
void
gap_timeconv_framenr_to_timestr( gint32 framenr, gdouble framerate, gchar *txt, gint txt_size)
{
  gdouble  msec_per_frame;
  gint32   tmsec;

  if(framerate > 0)
  {
    msec_per_frame = 1000.0 / framerate;
  }
  else
  {
    msec_per_frame = 1000.0;
  } 

  if(framenr >= 0)
  {
    tmsec = framenr * msec_per_frame;
    gap_timeconv_msecs_to_timestr (tmsec, txt, txt_size);
  }
  else
  { g_snprintf(txt, txt_size, "??:??:???");
  }

}  /* end gap_timeconv_framenr_to_timestr */


/* -------------------------------
 * gap_timeconv_samples_to_timestr
 * -------------------------------
 * convert Input samples at given samplerate (unit is samples/sec)
 * into time string formated as mm:ss:msec
 * (minutes:seconds:milliseconds)
 *
 * the caller must provide a pointer to txt buffer
 * where the output is written into at txt_size.
 *
 * the txt buffer size should be at last 12 bytes
 */
void
gap_timeconv_samples_to_timestr( gint32 samples, gdouble samplerate, gchar *txt, gint txt_size)
{
  gdouble  tmsec;

  if(samplerate > 0)
  {
    tmsec = (1000.0 * (gdouble)samples) / samplerate;
  }
  else
  {
    tmsec = (1000.0 * (gdouble)samples) / 44100;
  } 

  gap_timeconv_msecs_to_timestr ((gint32)tmsec, txt, txt_size);
}  /* end gap_timeconv_samples_to_timestr */


/* -------------------------------
 * gap_timeconv_samples_to_frames
 * -------------------------------
 * convert Input samples at given samplerate (unit is samples/sec)
 *  and given framerate unit frames/sec)
 *  to audioframes. (== number of frames that have to be played at framerate
 *                      to get the same duration as samples played at samplerate)
 * returns the number of audioframes
 */
gdouble
gap_timeconv_samples_to_frames( gint32 samples, gdouble samplerate, gdouble framerate)
{
  gdouble  secs;
  gdouble  frames;

  if((samplerate < 1) || (framerate < 1))
  {
    return (0);
  }

  secs = (gdouble)samples / samplerate;
  frames = secs * framerate;
  
  return(frames);
}  /* end gap_timeconv_samples_to_frames */

