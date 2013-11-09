/* gap_audio_extract.c
 *
 *  GAP extract audio from videofile procedures
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
 * along with this program; if not, write to the Free Software
 * <http://www.gnu.org/licenses/>.
 */
 
/* 2008.06.24 hof  created (moved audio extract parts of gap_vex_exec.c to this module)
 */

/* SYTEM (UNIX) includes */
#include <stdio.h>
#include <stdlib.h>
#include <glib/gstdio.h>

/* GIMP includes */
#include "gtk/gtk.h"
#include "libgimp/gimp.h"


/* GAP includes */
#include "gap_audio_util.h"
#include "gap_audio_wav.h"
#include "gap_audio_extract.h"



extern      int gap_debug; /* ==0  ... dont print debug infos */



#ifdef GAP_ENABLE_VIDEOAPI_SUPPORT
/* ---------------------
 * p_init_progress
 * ---------------------
 */
static void
p_init_progress(const char *progressText
  ,gboolean do_progress
  ,GtkWidget *progressBar
  )
{
  if (do_progress)
  {
    if (progressBar == NULL)
    {
      gimp_progress_init (progressText);
    }
    else
    {
      gtk_progress_bar_set_text(GTK_PROGRESS_BAR(progressBar), progressText);
      gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(progressBar), 0);
    }
  }
}  /* end p_init_progress */  
#endif


#ifdef GAP_ENABLE_VIDEOAPI_SUPPORT
/* ---------------------
 * p_do_progress
 * ---------------------
 */
static void
p_do_progress(gdouble progressValue
  ,gboolean do_progress
  ,GtkWidget *progressBar   // use NULL for gimp_progress
  ,t_GVA_Handle   *gvahand
  ,gpointer user_data
  )
{
  if (do_progress)
  {
    if (progressBar == NULL)
    {
      gimp_progress_update (progressValue);
    }
    else
    {
      gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(progressBar), progressValue);
    }
  }
  if(gvahand->fptr_progress_callback)
  {
    gvahand->cancel_operation = (*gvahand->fptr_progress_callback)(progressValue, user_data);
  }

}  /* end p_do_progress */  
#endif


#ifdef GAP_ENABLE_VIDEOAPI_SUPPORT
/* ----------------------------------
 * p_audio_extract_rewrite_wav_header
 * ----------------------------------
 *
 */
static void
p_audio_extract_rewrite_wav_header(FILE *fp_wav
  ,gint32  samples_written_to_file
  ,int audio_channels
  ,int sample_rate
  ,gint32 bytes_per_sample
  )
{
  if(fp_wav == NULL)
  {
    return;
  }
  fseek(fp_wav, 0, SEEK_SET);

  /* re-write the header */
  gap_audio_wav_write_header(fp_wav
                      , samples_written_to_file
                      , audio_channels            /* cannels 1 or 2 */
                      , sample_rate
                      , bytes_per_sample
                      , 16                          /* 16 bit sample resolution */
                      );

}  /* end p_audio_extract_rewrite_wav_header */
#endif

#ifdef GAP_ENABLE_VIDEOAPI_SUPPORT
/* -------------------------
 * gap_audio_extract_as_wav
 * -------------------------
 * extract specified number of samples at current
 * position of the specified (already opened) videohandle.
 * and optional save extracted audiodata as RIFF WAVE file 
 * (set wav_save to FALSE to skip writing to wav file,
 *  this is typical used to perform dummy read for 
 *  advancing current position in the videohandle)
 */
void
gap_audio_extract_as_wav(const char *audiofile
   ,  t_GVA_Handle   *gvahand
   ,  gdouble samples_to_read
   ,  gboolean wav_save
   
   ,  gboolean do_progress
   ,  GtkWidget *progressBar   // use NULL for gimp_progress
   ,  t_GVA_progress_callback_fptr fptr_progress_callback
   ,  gpointer user_data
   )
{
   int l_audio_channels;
   int l_sample_rate;
   long l_audio_samples;
   unsigned short *left_ptr;
   unsigned short *right_ptr;
   unsigned short *l_lptr;
   unsigned short *l_rptr;
   long   l_to_read;
   gint64 l_left_to_read;
   long   l_block_read;
   gdouble l_progress;
   FILE  *fp_wav;

   l_audio_channels = gvahand->audio_cannels;
   l_sample_rate    = gvahand->samplerate;
   l_audio_samples  = gvahand->total_aud_samples;

   if(gap_debug)
   {
     printf("Channels:%d samplerate:%d samples:%d  samples_to_read: %.0f\n"
          , (int)l_audio_channels
          , (int)l_sample_rate
          , (int)l_audio_samples
          , (float)samples_to_read
          );
   }
   
  fp_wav = NULL;
  if(wav_save) 
  {
    fp_wav = g_fopen(audiofile, "wb");
  }
  
  if((fp_wav) || (!wav_save))
  {
     gint32 l_bytes_per_sample;
     gint32 l_ii;

     if(l_audio_channels == 1) { l_bytes_per_sample = 2;}  /* mono */
     else                      { l_bytes_per_sample = 4;}  /* stereo */

     if(wav_save) 
     {
       /* write the header */
       gap_audio_wav_write_header(fp_wav
                      , (gint32)samples_to_read
                      , l_audio_channels            /* cannels 1 or 2 */
                      , l_sample_rate
                      , l_bytes_per_sample
                      , 16                          /* 16 bit sample resolution */
                      );
    }
    if(gap_debug) 
    {
      printf("samples_to_read:%d\n", (int)samples_to_read);
    }

    /* audio block read (blocksize covers playbacktime for 250 frames */
    l_left_to_read = samples_to_read;
    l_block_read = (double)(250.0) / (double)gvahand->framerate * (double)l_sample_rate;
    l_to_read = MIN(l_left_to_read, l_block_read);

    /* allocate audio buffers */
    left_ptr = g_malloc0((sizeof(short) * l_block_read) + 16);
    right_ptr = g_malloc0((sizeof(short) * l_block_read) + 16);

    while(l_to_read > 0)
    {
       l_lptr = left_ptr;
       l_rptr = right_ptr;
       /* read the audio data of channel 0 (left or mono) */
       GVA_get_audio(gvahand
                 ,l_lptr                /* Pointer to pre-allocated buffer if int16's */
                 ,1                     /* Channel to decode */
                 ,(gdouble)l_to_read    /* Number of samples to decode */
                 ,GVA_AMOD_CUR_AUDIO    /* read from current audio position (and advance) */
                 );
      if((l_audio_channels > 1) && (wav_save))
      {
         /* read the audio data of channel 2 (right)
          * NOTE: GVA_get_audio has advanced the stream position,
          *       so we have to set GVA_AMOD_REREAD to read from
          *       the same startposition as for channel 1 (left).
          */
         GVA_get_audio(gvahand
                   ,l_rptr                /* Pointer to pre-allocated buffer if int16's */
                   ,2                     /* Channel to decode */
                   ,l_to_read             /* Number of samples to decode */
                   ,GVA_AMOD_REREAD       /* read from */
                 );
      }
      l_left_to_read -= l_to_read;

      if(wav_save) 
      {
        /* write 16 bit wave datasamples 
         * sequence mono:    (lo, hi)
         * sequence stereo:  (lo_left, hi_left, lo_right, hi_right)
         */
        for(l_ii=0; l_ii < l_to_read; l_ii++)
        {
           gap_audio_wav_write_gint16(fp_wav, *l_lptr);
           l_lptr++;
           if(l_audio_channels > 1)
           {
             gap_audio_wav_write_gint16(fp_wav, *l_rptr);
             l_rptr++;
           }
         }
      }


      l_to_read = MIN(l_left_to_read, l_block_read);

      /* calculate progress */
      l_progress = (gdouble)(samples_to_read - l_left_to_read) / ((gdouble)samples_to_read + 1.0);
      if(gap_debug)
      {
        printf("l_progress:%f\n", (float)l_progress);
      }
      
      p_do_progress(l_progress, do_progress, progressBar, gvahand, user_data);
      if(gvahand->cancel_operation)
      {
        if(wav_save)
        {
          long l_samples_written_to_file;
          
          l_samples_written_to_file = samples_to_read - l_left_to_read;

          /* rewrite header (to produce valid wave file in case we were cancelled) */
          p_audio_extract_rewrite_wav_header(fp_wav
             , l_samples_written_to_file
             , l_audio_channels
             , l_sample_rate
             , l_bytes_per_sample
             );
        }
        printf("Audio extract was cancelled.\n");
        break;
      }
    }

    if(wav_save) 
    {
      /* close wavfile */
      fclose(fp_wav);
    }

    /* free audio buffers */
    g_free(left_ptr);
    g_free(right_ptr);
  }
  return;
}  /* end gap_audio_extract_as_wav */
#endif


#ifdef GAP_ENABLE_VIDEOAPI_SUPPORT
/* ---------------------------------
 * gap_audio_extract_from_videofile
 * ---------------------------------
 * extract the specified audiotrack to WAVE file. (name specified via audiofile)
 * starting at position (specified by l_pos and l_pos_unit)
 * in length of extracted_frames (if number of frames is exactly known)
 * or in length expected_frames (is a guess if l_extracted_frames < 1)
 * use do_progress flag value TRUE for progress feedback on the specified
 * progressBar. 
 * (if progressBar = NULL gimp progress is used
 * this usually refers to the progress bar in the image window)
 * Note:
 *   this feature is not present if compiled without GAP_ENABLE_VIDEOAPI_SUPPORT
 */
void
gap_audio_extract_from_videofile(const char *videoname
  , const char *audiofile
  , gint32 audiotrack
  , const char *preferred_decoder
  , gint        exact_seek
  , t_GVA_PosUnit  pos_unit
  , gdouble        pos
  , gdouble        extracted_frames
  , gdouble        expected_frames
  , gboolean do_progress
  , GtkWidget *progressBar
  , t_GVA_progress_callback_fptr fptr_progress_callback
  , gpointer user_data
  )
{
  t_GVA_Handle   *gvahand;

  /* --------- OPEN the videofile --------------- */
  gvahand = GVA_open_read_pref(videoname
                              ,1 /* videotrack (not relevant for audio) */
                              ,audiotrack
                              ,preferred_decoder
                              , FALSE  /* use MMX if available (disable_mmx == FALSE) */
                              );
  if(gvahand == NULL)
  {
    printf("Could not open videofile:%s\n", videoname);
    return;
  }

  
  gvahand->image_id = -1;   /* prenvent API from deleting that image at close */
  gvahand->progress_cb_user_data = user_data;
  gvahand->fptr_progress_callback = fptr_progress_callback;

  /* ------ extract Audio ---------- */
  if(gvahand->atracks > 0)
  {
     if (audiotrack > 0)
     {
        gdouble l_samples_to_read;
        gdouble extracted_frames;

        if(gap_debug)
        {
          printf("EXTRACTING audio, writing to file %s\n", audiofile);
        }

        if(gvahand->audio_cannels > 0)
        {
          /* seek needed only if extract starts not at pos 1 */
          if(pos > 1)
          {
            p_init_progress(_("Seek Audio Position..."), do_progress, progressBar);

            /* check for exact frame_seek */
            if (exact_seek != 0)
            {
               gint32 l_seek_framenumber;
               
               
               l_seek_framenumber = pos;
               if(pos_unit == GVA_UPOS_PRECENTAGE)
               {
                 l_seek_framenumber = gvahand->total_frames * pos;
               }
               
               l_samples_to_read = (gdouble)(l_seek_framenumber) 
                                / (gdouble)gvahand->framerate * (gdouble)gvahand->samplerate;

               /* extract just for exact positioning (without save to wav file) */
               if(gap_debug)
               {
                 printf("extract just for exact positioning (without save to wav file)\n");
               }
               gap_audio_extract_as_wav(audiofile
                    , gvahand
                    , l_samples_to_read
                    , FALSE             /* wav_save */
                    , do_progress
                    , progressBar
                    , fptr_progress_callback
                    , user_data
                    );
            }
            else
            {
              /* audio pos 1 frame before video pos
                * example: extract frame 1 upto 2
                * results in audio range 0 upto 2
                * this way we can get the audioduration of frame 1 
                */
               GVA_seek_audio(gvahand, pos -1, pos_unit);
            }
          }
          if(gvahand->cancel_operation)
          {
             /* stop if we were cancelled (via request from fptr_progress_callback) */
             return;
          }

          p_init_progress(_("Extracting Audio..."), do_progress, progressBar);

          
          if(extracted_frames > 1)
          {
            l_samples_to_read = (gdouble)(extracted_frames +1.0)
                              / (gdouble)gvahand->framerate 
                        * (gdouble)gvahand->samplerate;
            if(gap_debug)
            {
              printf("A: l_samples_to_read %.0f extracted_frames:%d\n"
                          , (float)l_samples_to_read
                    ,(int)extracted_frames
                   );
            }
          }
          else
          {
            l_samples_to_read = (gdouble)(expected_frames +1.0) 
                              / (gdouble)gvahand->framerate 
                        * (gdouble)gvahand->samplerate;
            if(gap_debug)
            {
              printf("B: l_samples_to_read %.0f extracted_frames:%d expected_frames:%d\n"
                                ,(float)l_samples_to_read
                          ,(int)extracted_frames
                          ,(int)expected_frames
                          );
            }
          }

          /* extract and save to wav file */
          if(gap_debug)
          {
            printf("extract (with save to wav file)\n");
          }

          gap_audio_extract_as_wav(audiofile
                    , gvahand
                    , l_samples_to_read
                    , TRUE             /* wav_save */
                    , do_progress
                    , progressBar
                    , fptr_progress_callback
                    , user_data
                    );
        }
        
     }
  }
  return;
}  /* end gap_audio_extract_from_videofile */
#endif
