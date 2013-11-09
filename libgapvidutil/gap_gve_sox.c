/* gap_gve_sox.c
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
 * along with this program; if not, write to the Free Software
 * <http://www.gnu.org/licenses/>.
 */

#include <config.h>

#include <glib/gstdio.h>

#include "gap_gve_sox.h"
#include "gap_audio_wav.h"
#include "gap-intl.h"


/* --------------------------------
 * gap_gve_sox_init_default
 * --------------------------------
 */
void
gap_gve_sox_init_default(GapGveCommonValues *cval)
{
   g_snprintf(cval->util_sox, sizeof(cval->util_sox), "%s"
              , GAP_STORY_SOX_DEFAULT_UTIL_SOX);
   g_snprintf(cval->util_sox_options, sizeof(cval->util_sox_options), "%s"
              ,  GAP_STORY_SOX_DEFAULT_UTIL_SOX_OPTIONS);
}  /* end gap_gve_sox_init_default */

/* --------------------------------
 * gap_gve_sox_save_config
 * --------------------------------
 */
void
gap_gve_sox_save_config(GapGveCommonValues *cval)
{
  gimp_gimprc_set("video_encode_com_util_sox", cval->util_sox);
  gimp_gimprc_set("video_encode_com_util_sox_options", cval->util_sox_options);
}  /* end gap_gve_sox_save_config */

/* --------------------------------
 * gap_gve_sox_init_config
 * --------------------------------
 */
void
gap_gve_sox_init_config(GapGveCommonValues *cval)
{
   char *value_string;

   gap_gve_sox_init_default(cval);

   value_string = gimp_gimprc_query("video_encode_com_util_sox");
   if(value_string)
   {
      g_snprintf(cval->util_sox, sizeof(cval->util_sox), "%s", value_string);
      g_free(value_string);
   }

   value_string = gimp_gimprc_query("video_encode_com_util_sox_options");
   if(value_string)
   {
      g_snprintf(cval->util_sox_options, sizeof(cval->util_sox_options), "%s", value_string);
      g_free(value_string);
   }
}  /* end gap_gve_sox_init_config */


/* --------------------------------
 * gap_gve_sox_chk_and_resample
 * --------------------------------
 */
gint
gap_gve_sox_chk_and_resample(GapGveCommonValues *cval)
{
  long        samplerate;
  long        channels;
  long        bytes_per_sample;
  long        bits;
  long        samples;
  long        all_playlist_references;
  long        valid_playlist_references;
  int    l_rc;
  gchar *l_msg;

  if(gap_debug) printf("gap_gve_sox_chk_and_resample\n");

  all_playlist_references = 0;
  valid_playlist_references = 0;

  cval->tmp_audfile[0] = '\0';
  if(cval->audioname1[0] == '\0')
  {
    return 0;
  }

  /* check for WAV file or valid audio playlist, and get audio informations */
  l_rc = gap_audio_playlist_wav_file_check(cval->audioname1
                     , &samplerate
                     , &channels
                     , &bytes_per_sample
                     , &bits
                     , &samples
                     , &all_playlist_references
                     , &valid_playlist_references
                     , cval->samplerate
                     );

   if (valid_playlist_references > 0)
   {
     return 0;  /* OK,  valid audio playlist */
   }
   if (all_playlist_references > 0)
   {
     return -1; /*  non matching playlist */
   }

  if ((l_rc !=0)
  ||  (bits != 16)
  ||  (cval->samplerate != cval->wav_samplerate1))
  {
     if((cval->tmp_audfile[0] != '\0') && (cval->wav_samplerate_tmp == cval->samplerate))
     {
       if(g_file_test(cval->tmp_audfile, G_FILE_TEST_EXISTS))
       {
         if(gap_debug) printf("resample needed, tmpfile is already there %s\n", cval->tmp_audfile);
         return 0;
       }
     }

     g_snprintf(cval->tmp_audfile, sizeof(cval->tmp_audfile), "%s_tmp.wav", cval->audioname1);

     if(gap_debug) printf("resample needed, try to call sox to create tmpfile: %s\n", cval->tmp_audfile);

     /* delete tmp file */
     g_remove(cval->tmp_audfile);
     if(g_file_test(cval->tmp_audfile, G_FILE_TEST_EXISTS))
     {
        l_msg = g_strdup_printf(_("ERROR: Can't overwrite temporary workfile\nfile: %s")
                           , cval->tmp_audfile);
        if(cval->run_mode == GIMP_RUN_INTERACTIVE)
        {
          g_message("%s", l_msg);
            }
        return -1;
     }

     gap_story_sox_exec_resample( cval->audioname1
                    , cval-> tmp_audfile
                    , cval->samplerate
                    , cval->util_sox
                    , cval->util_sox_options
                    );

     if(!g_file_test(cval->tmp_audfile, G_FILE_TEST_EXISTS))
     {
       l_msg = g_strdup_printf(_("ERROR: Could not create resampled WAV workfile\n\n"
                               "1.) check write permission on \n  file:  %s\n"
                               "2.) check if SOX (version >= 12.16) is installed:\n  prog:  %s\n")
                       , cval->tmp_audfile
                       , cval->util_sox
                       );
       if(cval->run_mode == GIMP_RUN_INTERACTIVE)
       {
         g_message("%s", l_msg);
           }
       g_free(l_msg);
       return -1;
     }
  }
  return 0;
}  /* end gap_gve_sox_chk_and_resample */
