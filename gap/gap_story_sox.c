/* gap_story_sox.c
 *    Audio resampling Modules based on calls to UNIX Utility Program sox
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

#include <config.h>

#include <stdlib.h>

#include <glib/gstdio.h>

#include "gap_story_sox.h"
#include "gap_audio_wav.h"
#include "gap-intl.h"


extern int gap_debug;  /* 1 == print debug infos , 0 dont print debug infos */

/* --------------------------------
 * gap_story_sox_exec_resample
 * --------------------------------
 */
void
gap_story_sox_exec_resample(char *in_audiofile
               ,char *out_audiofile
               ,gint32 samplerate
               ,char *util_sox           /* the resample program (default: sox) */
               ,char *util_sox_options
               )
{
  gchar *l_cmd;

  if(util_sox == NULL)
  {
    util_sox = GAP_STORY_SOX_DEFAULT_UTIL_SOX;
  }
  if(util_sox_options == NULL)
  {
    util_sox_options = GAP_STORY_SOX_DEFAULT_UTIL_SOX_OPTIONS;
  }

  /* the calling style requres UNIX Shell for Environment Variables
   * IN, OUT, RATE  that are used for Parameter substitution
   */

  l_cmd = g_strdup_printf("IN='%s';OUT='%s';RATE=%d;%s %s\n"
           , in_audiofile               /* input audio file */
           , out_audiofile              /* output audio file (tmp 16-bit wav file) */
           , (int)samplerate
           , util_sox
           , util_sox_options
           );

  if(gap_debug)
  {
    printf("Execute resample CMD:%s\n", l_cmd);
  }

  system(l_cmd);
  g_free(l_cmd);
}  /* end gap_story_sox_exec_resample */


