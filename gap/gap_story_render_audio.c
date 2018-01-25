/* gap_story_render_audio.c
 *
 *
 *  GAP storyboard audio rendering.
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

/*
 * 2006.06.25  hof  - created (moved stuff from the former gap_gve_story modules to this  new module)
 *
 */

#include <config.h>

/* SYTEM (UNIX) includes */
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <math.h>
#include <errno.h>

#include <dirent.h>


#include <glib/gstdio.h>



/* GIMP includes */
#include "gtk/gtk.h"
#include "gap-intl.h"
#include "libgimp/gimp.h"


#include "gap_libgapbase.h"
#include "gap_libgimpgap.h"
#include "gap_audio_util.h"
#include "gap_audio_wav.h"
#include "gap_story_sox.h"

#ifdef GAP_ENABLE_VIDEOAPI_SUPPORT
#include "gap_vid_api.h"
#else
#ifndef GAP_STUBTYPE_GVA_HANDLE
typedef gpointer t_GVA_Handle;
#define GAP_STUBTYPE_GVA_HANDLE
#endif
#endif

#include "gap_story_file.h"
#include "gap_layer_copy.h"
#include "gap_story_render_processor.h"
#include "gap_lib_common_defs.h"


/* if G_BYTE_ORDER == G_BIG_ENDIAN */
#if G_BYTE_ORDER == G_LITTLE_ENDIAN

typedef union t_wav_16bit_int
{
  gint16  value;
  guint16 uvalue;
  struct
  {
    guchar lsb;
    guchar msb;
  } bytes;
} t_wav_16bit_int;
#else

typedef union t_wav_16bit_int
{
  gint16 value;
  struct
  {
    guchar msb;
    guchar lsb;
  } bytes;
} t_wav_16bit_int;

#endif


#define AUDIO_SEGMENT_SIZE 4000000
#define MAX_AUD_CACHE_ELEMENTS 999


extern int gap_debug;  /* 1 == print debug infos , 0 dont print debug infos */

static GapStoryRenderAudioCache *global_audcache = NULL;


static void     p_drop_audio_cache_elem1(GapStoryRenderAudioCache *audcache);
static GapStoryRenderAudioCacheElem  *p_load_cache_audio(gint32 audio_track, char* filename, gint32 *audio_id, gint32 *aud_bytelength, gint32 seek_idx);
static void     p_find_min_max_aud_tracknumbers(GapStoryRenderAudioRangeElem *aud_list
                              , gint32 *lowest_tracknr
                              , gint32 *highest_tracknr);
static void     p_get_audio_sample(GapStoryRenderVidHandle *vidhand        /* IN  */
                  ,gint32 track                 /* IN  */
                  ,gint32 master_sample_idx     /* IN  */
                  ,gdouble *sample_left    /* OUT (prescaled at local volume) */
                  ,gdouble *sample_right   /* OUT  (prescaled at local volume)*/
                  );
static void     p_mix_audio(FILE *fp                  /* IN: NULL: dont write to file */
                  ,gint16 *bufferl           /* IN: NULL: dont write to buffer */
                  ,gint16 *bufferr           /* IN: NULL: dont write to buffer */
                  ,GapStoryRenderVidHandle *vidhand
                  ,gdouble aud_total_sec
                  ,gdouble *mix_scale         /* OUT */
                  );
static void     p_check_audio_peaks(GapStoryRenderVidHandle *vidhand
                 ,gdouble aud_total_sec
                 ,gdouble *mix_scale         /* OUT */
                 );

#ifdef GAP_ENABLE_VIDEOAPI_SUPPORT
static void     p_extract_audioblock(t_GVA_Handle *gvahand
                                     , FILE  *fp_wav
                                     , gdouble samples_to_extract
                                     , int audio_channels
                                     , int sample_rate
                                     , GapStoryRenderVidHandle *vidhand  /* for progress */
                                     );
static void     p_extract_audiopart(t_GVA_Handle *gvahand
                                   , char *wavfilename
                                   , gdouble min_play_sec
                                   , gdouble max_play_sec
                                   , GapStoryRenderVidHandle *vidhand  /* for progress */
                                   );
#endif



/* ----------------------------------------------------
 * p_drop_audio_cache_elem1
 * ----------------------------------------------------
 */
static void
p_drop_audio_cache_elem1(GapStoryRenderAudioCache *audcache)
{
  GapStoryRenderAudioCacheElem  *ac_ptr;

  if(audcache)
  {
    ac_ptr = audcache->ac_list;
    if(ac_ptr)
    {
      if(gap_debug) printf("p_drop_audio_cache_elem1 delete:%s (audio_id:%d)\n", ac_ptr->filename, (int)ac_ptr->audio_id);
      g_free(ac_ptr->aud_data);
      g_free(ac_ptr->filename);
      audcache->ac_list = (GapStoryRenderAudioCacheElem  *)ac_ptr->next;
      g_free(ac_ptr);
    }
  }
}  /* end p_drop_audio_cache_elem1 */


/* ----------------------------------------------------
 * gap_story_render_drop_audio_cache
 * ----------------------------------------------------
 */
void
gap_story_render_drop_audio_cache(void)
{
  GapStoryRenderAudioCache *audcache;

  if(gap_debug)  printf("gap_story_render_drop_audio_cache START\n");
  audcache = global_audcache;
  if(audcache)
  {
    while(audcache->ac_list)
    {
      p_drop_audio_cache_elem1(audcache);
    }
  }
  if(gap_debug) printf("gap_story_render_drop_audio_cache END\n");

}  /* end gap_story_render_drop_audio_cache */


/* ----------------------------------------------------
 * gap_story_render_remove_tmp_audiofiles
 * ----------------------------------------------------
 */
void
gap_story_render_remove_tmp_audiofiles(GapStoryRenderVidHandle *vidhand)
{
  GapStoryRenderAudioRangeElem *aud_elem;

  if(gap_debug)
  {
    printf("gap_story_render_remove_tmp_audiofiles START\n");
  }

  for(aud_elem = vidhand->aud_list; aud_elem != NULL; aud_elem = (GapStoryRenderAudioRangeElem *)aud_elem->next)
  {
    if(aud_elem->tmp_audiofile)
    {
      if(gap_debug)
      {
        printf("gap_story_render_remove_tmp_audiofiles tmp_audiofile: %s\n"
              , aud_elem->tmp_audiofile);
      }

      /* delete tmp_audiofile if still exists
       * (more aud_elements may refere to the same tmp_audiofile)
       */
      if(g_file_test(aud_elem->tmp_audiofile, G_FILE_TEST_EXISTS))
      {
        if(gap_debug)
        {
          printf("gap_story_render_remove_tmp_audiofiles removing: %s\n"
                , aud_elem->tmp_audiofile);
        }
        g_remove(aud_elem->tmp_audiofile);
      }
      g_free(aud_elem->tmp_audiofile);
      aud_elem->tmp_audiofile = NULL;
    }
  }

}  /* end gap_story_render_remove_tmp_audiofiles */

/* ----------------------------------------------------
 * p_load_cache_audio
 * ----------------------------------------------------
 */
static GapStoryRenderAudioCacheElem  *
p_load_cache_audio(gint32 audio_track, char* filename, gint32 *audio_id, gint32 *aud_bytelength, gint32 seek_idx)
{
  gint32 l_idx;
  gint32 l_audio_id;
  GapStoryRenderAudioCacheElem  *ac_ptr;
  GapStoryRenderAudioCacheElem  *ac_last;
  GapStoryRenderAudioCacheElem  *ac_new;
  GapStoryRenderAudioCache  *audcache;
  guchar        *aud_data;


  if(filename == NULL)
  {
    printf("p_load_cache_audio: ** ERROR cant load filename == NULL!\n");
    return NULL;
  }


  if(global_audcache == NULL)
  {
    /* init the global_mage cache */
    global_audcache = g_malloc0(sizeof(GapStoryRenderAudioCache));
    global_audcache->ac_list = NULL;
    global_audcache->nextval_audio_id = 0;
    global_audcache->max_aud_cache = MAX_AUD_CACHE_ELEMENTS;
  }

  audcache = global_audcache;
  ac_last = audcache->ac_list;

  l_idx = 0;
  for(ac_ptr = audcache->ac_list; ac_ptr != NULL; ac_ptr = (GapStoryRenderAudioCacheElem *)ac_ptr->next)
  {
    l_idx++;
    if (audio_track == ac_ptr->owner_track)
    {
      if(strcmp(filename, ac_ptr->filename) == 0)
      {
        /* audio found in cache, can skip load */
        *audio_id       = ac_ptr->audio_id;
        *aud_bytelength = ac_ptr->aud_bytelength;

        return(ac_ptr);
      }
    }
    ac_last = ac_ptr;
  }

  aud_data = g_malloc(AUDIO_SEGMENT_SIZE);
  *aud_bytelength = gap_file_get_filesize(filename);

  ac_new = NULL;
  if(aud_data != NULL)
  {
    l_audio_id = global_audcache->nextval_audio_id;
    global_audcache->nextval_audio_id++;

    *audio_id = l_audio_id;
    ac_new = g_malloc0(sizeof(GapStoryRenderAudioCacheElem));
    ac_new->filename = g_strdup(filename);
    
    ac_new->owner_track = audio_track;
    ac_new->audio_id = l_audio_id;
    ac_new->aud_data = aud_data;
    ac_new->aud_bytelength = *aud_bytelength;

    ac_new->segment_startoffset = seek_idx;
    ac_new->segment_bytelength = gap_file_load_file_segment(filename
                                                    ,ac_new->aud_data
                                                    ,seek_idx
                                                    ,AUDIO_SEGMENT_SIZE
                                                    );



    if(audcache->ac_list == NULL)
    {
      audcache->ac_list = ac_new;   /* 1.st elem starts the list */
    }
    else
    {
      ac_last->next = (GapStoryRenderAudioCacheElem *)ac_new;  /* add new elem at end of the cache list */
    }

    if(l_idx > audcache->max_aud_cache)
    {
      /* chache list has more elements than desired,
       * drop the 1.st (oldest) entry in the chache list
       */
      /* p_drop_audio_cache_elem1(audcache); */  /* keep all audio files cached */
    }
  }

  return(ac_new);
}  /* end p_load_cache_audio */



/* ----------------------------------------------------
 * p_find_min_max_aud_tracknumbers
 * ----------------------------------------------------
 * findout the lowest and highest audio track number used
 * in the audiorange list
 */
static void
p_find_min_max_aud_tracknumbers(GapStoryRenderAudioRangeElem *aud_list
                              , gint32 *lowest_tracknr
                              , gint32 *highest_tracknr)
{
  GapStoryRenderAudioRangeElem *aud_elem;

  *lowest_tracknr = GAP_STB_MAX_AUD_TRACKS;
  *highest_tracknr = -1;

  for(aud_elem = aud_list; aud_elem != NULL; aud_elem = (GapStoryRenderAudioRangeElem *)aud_elem->next)
  {
    if (aud_elem->track > *highest_tracknr)
    {
      *highest_tracknr = aud_elem->track;
    }
    if (aud_elem->track < *lowest_tracknr)
    {
      *lowest_tracknr = aud_elem->track;
    }
  }

  if(gap_debug) printf("p_find_min_aud_vid_tracknumbers: min:%d max:%d\n", (int)*lowest_tracknr, (int)*highest_tracknr);

}  /* end p_find_min_max_aud_tracknumbers */



/* ---------------------------------
 * p_get_audio_sample
 * ---------------------------------
 * - scan the aud_list to findout
 *   audiofile that is played at desired track
 *   at desired sample_index position.
 * - return the sample (amplitude) at desired sample_index position
 *   both for left and reight stereo channel.
 *   the sample is already scaled to local input track
 *   volume settings (and loacl fade_in, fade_out effects)
 */
static void
p_get_audio_sample(GapStoryRenderVidHandle *vidhand        /* IN  */
                  ,gint32 track                 /* IN  */
                  ,gint32 master_sample_idx     /* IN  */
                  ,gdouble *sample_left    /* OUT (prescaled at local volume) */
                  ,gdouble *sample_right   /* OUT  (prescaled at local volume)*/
                  )
{
  GapStoryRenderAudioRangeElem *aud_elem;
  GapStoryRenderAudioCacheElem     *ac_elem;
  gint32  l_group_samples;
  gint32  l_byte_idx;
  gint32  l_samp_idx;               /* local track sepcific sample index */

  t_wav_16bit_int l_left;
  t_wav_16bit_int l_right;
  gdouble l_vol;
  gint32  l_range_samples;




  l_group_samples = 0;

  /* we are using master_sample_idx to access all other samples.
   * (this is faster than using time specificaton in secs (gdouble position)
   *  but requires that all samples are using the same samplerate.)
   */

  for(aud_elem = vidhand->aud_list; aud_elem != NULL; aud_elem = (GapStoryRenderAudioRangeElem *)aud_elem->next)
  {
    if(aud_elem->track == track)
    {
      l_range_samples = aud_elem->range_samples
                      + MAX(0, aud_elem->wait_until_samples - l_group_samples);

      if (master_sample_idx < l_group_samples + l_range_samples)
      {
        if(aud_elem->aud_type == GAP_AUT_SILENCE)
        {
          *sample_left = 0.0;
          *sample_right = 0.0;
          return;
        }
        /* found the range that contains the sample at the requested index and track */
        l_samp_idx = master_sample_idx - l_group_samples;



        /* fetch sample (and convert to 16bit stereo) */
        l_byte_idx = aud_elem->byteoffset_rangestart + (l_samp_idx * aud_elem->bytes_per_sample);

        /* check for audio data (if not already there then load to memory) */
        if(aud_elem->aud_data == NULL)
        {
           char *l_audiofile;
           gint32  l_seek_idx;

           /* make sure segment start is header offset + a multiple of 4 */
           l_seek_idx = (l_byte_idx - aud_elem->byteoffset_data) / 4;
           l_seek_idx = aud_elem->byteoffset_data + (l_seek_idx * 4);

           l_audiofile = aud_elem->audiofile;
           if(aud_elem->tmp_audiofile)
           {
              l_audiofile = aud_elem->tmp_audiofile;
           }

           if(gap_debug) printf("BEFORE p_load_cache_audio  %s\n", l_audiofile);
           ac_elem = p_load_cache_audio(aud_elem->track
                                       , l_audiofile
                                       , &aud_elem->audio_id
                                       , &aud_elem->aud_bytelength
                                       , l_seek_idx
                                       );

           aud_elem->ac_elem = ac_elem;
           aud_elem->aud_data = ac_elem->aud_data;

           if(aud_elem->aud_data == NULL)
           {
              char *l_errtxt;

              l_errtxt = g_strdup_printf(_("cant load:  %s to memory"), l_audiofile);
              gap_story_render_set_stb_error(vidhand->sterr, l_errtxt);
              g_free(l_errtxt);

              /* ERROR , audiofile was not loaded ! */
              *sample_left = 0.0;
              *sample_right = 0.0;
              return;
           }
        }

        /* check if byte_index is in the current segment */
        ac_elem = aud_elem->ac_elem;
        if(ac_elem == NULL)
        {
          printf("p_get_audio_sample: ERROR no audiosegement loaded for %s\n", aud_elem->audiofile);
          return;
        }

        if((l_byte_idx < ac_elem->segment_startoffset)
        || (l_byte_idx >= ac_elem->segment_startoffset + ac_elem->segment_bytelength))
        {
          /* the requested byte_index is outside of the currently loaded segment
           * we have to load the matching audio segment of the file
           */

          gint32  l_seek_idx;

          if(gap_debug) printf("SEGM_RELOAD l_byte_idx:%d   startoffset:%d  segm_size:%d\n", (int)l_byte_idx, (int)ac_elem->segment_startoffset ,(int)ac_elem->segment_bytelength );


          /* make sure segment start is header offset + a multiple of 4 */
          l_seek_idx = (l_byte_idx - aud_elem->byteoffset_data) / 4;
          l_seek_idx = aud_elem->byteoffset_data + (l_seek_idx * 4);

          ac_elem->segment_startoffset = l_seek_idx;
          ac_elem->segment_bytelength = gap_file_load_file_segment(ac_elem->filename
                                                           ,ac_elem->aud_data
                                                           ,l_seek_idx
                                                           ,AUDIO_SEGMENT_SIZE
                                                           );
        }


        /* reduce byte_index before accessing audio segmentdata */
        l_byte_idx -= ac_elem->segment_startoffset;

        if((l_byte_idx < 0) || (l_byte_idx >= ac_elem->segment_bytelength  /*aud_elem->aud_bytelength */ ))
        {
          printf("p_get_audio_sample: **ERROR INDEX OVERFLOW: %d (segment_bytelength: %d aud_bytelength: %d)\n"
                 "  file:%s\n"
                 , (int)l_byte_idx
                 , (int)ac_elem->segment_bytelength
                 , (int)aud_elem->aud_bytelength
                 , ac_elem->filename
                 );
          return;
        }


        if(aud_elem->channels == 2)  /* STEREO */
        {
          if(aud_elem->bytes_per_sample == 4)
          {
            /* fetch 16bit stereosample  (byteorder lLrR lLrR) */
            l_left.bytes.lsb = aud_elem->aud_data[l_byte_idx];
            l_left.bytes.msb = aud_elem->aud_data[l_byte_idx +1];
            l_right.bytes.lsb = aud_elem->aud_data[l_byte_idx +2];
            l_right.bytes.msb = aud_elem->aud_data[l_byte_idx +3];
          }
          else
          {
            /* fetch 8bit stereosample  (byteorder LR LR) */
            gap_audio_util_dbl_sample_8_to_16(aud_elem->aud_data[l_byte_idx]
                                ,&l_left.bytes.lsb
                                ,&l_left.bytes.msb
                                );
            gap_audio_util_dbl_sample_8_to_16(aud_elem->aud_data[l_byte_idx +1]
                                ,&l_right.bytes.lsb
                                ,&l_right.bytes.msb
                                );
          }
        }
        else                         /* MONO */
        {
          if(aud_elem->bytes_per_sample == 2)
          {

            /* printf("l_byte_idx: %d   %02x %02x\n", (int)l_byte_idx
             *             ,(int)aud_elem->aud_data[l_byte_idx]
             *             ,(int)aud_elem->aud_data[l_byte_idx +1]);
             */

            /* fetch 16bit momosample  (byteorder lL lL) */
            l_left.bytes.lsb = aud_elem->aud_data[l_byte_idx];
            l_left.bytes.msb = aud_elem->aud_data[l_byte_idx +1];
          }
          else
          {
            /* fetch 8bit monosample */
            gap_audio_util_dbl_sample_8_to_16(aud_elem->aud_data[l_byte_idx]
                                ,&l_left.bytes.lsb
                                ,&l_left.bytes.msb
                                );
          }
          l_right.value = l_left.value;
        }

        /* set volume, respecting fade effects */
        l_vol = aud_elem->volume;

        if( aud_elem->fade_in_samples > 0)
        {
          if(l_samp_idx < aud_elem->fade_in_samples)
          {
            l_vol = ((l_vol - aud_elem->volume_start) * ((gdouble)l_samp_idx / (gdouble)aud_elem->fade_in_samples))
                  + aud_elem->volume_start;
          }
        }

        if( aud_elem->fade_out_samples > 0)
        {
          if(l_samp_idx > (l_range_samples - aud_elem->fade_out_samples))
          {
            l_vol = ((l_vol - aud_elem->volume_end) * ((gdouble)(l_range_samples - l_samp_idx) / (gdouble)aud_elem->fade_out_samples))
                  + aud_elem->volume_end;
          }
        }

        *sample_left = l_left.value * l_vol;
        *sample_right = l_right.value * l_vol;
        return;
      }
      l_group_samples += l_range_samples;
    }
  }

  /* the requested position is larger than the length of the requested track.
   * return silence in this case.
   */
  *sample_left = 0.0;
  *sample_right = 0.0;
  return;
}   /* end p_get_audio_sample */


/* ---------------------------------
 * p_mix_audio
 * ---------------------------------
 * mix all audiotracks to one
 * composite audio track,
 * and calculate mix scale (to fit into 16bit int)
 * optional write the mixed audio to wav file
 *  (bytesrquence LLRRLLRR)
 * or write to separate buffers for left and right stereo channel
 * or write nothing at all.
 */
static void
p_mix_audio(FILE *fp                  /* IN: NULL: dont write to file */
           ,gint16 *bufferl           /* IN: NULL: dont write to buffer */
           ,gint16 *bufferr           /* IN: NULL: dont write to buffer */
           ,GapStoryRenderVidHandle *vidhand
           ,gdouble aud_total_sec
           ,gdouble *mix_scale         /* OUT */
           )
{
  gint32 l_track;
  gint32 l_min_track;
  gint32 l_max_track;

  gdouble l_max_peak;
  gdouble l_peak_posl;
  gdouble l_peak_posr;
  gdouble l_peak_negl;
  gdouble l_peak_negr;
  gdouble l_mixl;
  gdouble l_mixr;
  gdouble l_sample_left;
  gdouble l_sample_right;
  gdouble l_master_scale;
  gint32  l_master_sample_idx;
  gint32  l_max_sample_idx;


  l_peak_posl = 0.0;
  l_peak_posr = 0.0;
  l_peak_negl = 0.0;
  l_peak_negr = 0.0;
  l_sample_left = 0.0;
  l_sample_right = 0.0;

  l_master_scale = vidhand->master_volume;

  if((fp) || (bufferl) || (bufferr))
  {
    l_master_scale = *mix_scale * vidhand->master_volume;
  }

  l_max_sample_idx = aud_total_sec * vidhand->master_samplerate;

  p_find_min_max_aud_tracknumbers(vidhand->aud_list, &l_min_track, &l_max_track);

  for(l_master_sample_idx = 0; l_master_sample_idx < l_max_sample_idx; l_master_sample_idx++)
  {
    l_mixl = 0.0;
    l_mixr = 0.0;

    *vidhand->progress = (gdouble)l_master_sample_idx / (gdouble)l_max_sample_idx;

    /* mix samples of all tracks at current index
     * (track specific volume scaling is already done in p_get_audio_sample)
     */
    for(l_track=l_min_track; l_track <= l_max_track; l_track++)
    {
      p_get_audio_sample(vidhand
                        ,l_track
                        ,l_master_sample_idx
                        ,&l_sample_left
                        ,&l_sample_right
                        );
      l_mixl += l_sample_left;
      l_mixr += l_sample_right;
    }
    l_mixl *= l_master_scale;
    l_mixr *= l_master_scale;

    if(fp)
    {
       /* wav databyte sequence is LLRR LLRR LLRR ... */
       gap_audio_wav_write_gint16(fp, (gint16)l_mixl);
       gap_audio_wav_write_gint16(fp, (gint16)l_mixr);
    }

    if(bufferl)
    {
       bufferl[l_master_sample_idx] = l_mixl;
    }
    if(bufferr)
    {
       bufferr[l_master_sample_idx] = l_mixr;
    }


    l_peak_posl = MAX(l_mixl,l_peak_posl);
    l_peak_posr = MAX(l_mixr,l_peak_posr);
    l_peak_negl = MIN(l_mixl,l_peak_negl);
    l_peak_negr = MIN(l_mixr,l_peak_negr);
  }

  l_max_peak = MAX(l_peak_posl, l_peak_posr);
  l_max_peak = MAX(l_max_peak, (-1.0 * l_peak_negl));
  l_max_peak = MAX(l_max_peak, (-1.0 * l_peak_negr));

  if (l_max_peak <= 32767)
  {
    /* max peak fits into 16 bit integer,
     * (we dont need scale down)
     */
    *mix_scale = 1.0;
  }
  else
  {
    /* must scale down ALL sample amplitudes
     * to fit into 16 bit integer
     */
    *mix_scale = 32767 / l_max_peak;
  }

}   /* end p_mix_audio */

/* ---------------------------------
 * p_check_audio_peaks
 * ---------------------------------
 */
static void
p_check_audio_peaks(GapStoryRenderVidHandle *vidhand
           ,gdouble aud_total_sec
           ,gdouble *mix_scale         /* OUT */
           )
{
  /* perform mix without any output,
   * to findout peaks and calculate
   * mix_scale (how to scale to fit the result into 16 bit int)
   */
  p_mix_audio(NULL, NULL, NULL, vidhand, aud_total_sec, mix_scale);
}  /* end p_check_audio_peaks */




#ifdef GAP_ENABLE_VIDEOAPI_SUPPORT
/* ----------------------------------------------------
 * p_extract_audioblock
 * ----------------------------------------------------
 * extract an audio block in size of samples_to_extract of a videofile
 * if fp_wav is not NULL write the extracted data as 16bit samples to the file
 */
static void
p_extract_audioblock(t_GVA_Handle *gvahand
                   , FILE  *fp_wav
                   , gdouble samples_to_extract
                   , int audio_channels
                   , int sample_rate
                   , GapStoryRenderVidHandle *vidhand
                   )
{
  static unsigned short *left_ptr;
  static unsigned short *right_ptr;
  unsigned short *l_lptr;
  unsigned short *l_rptr;
  long   l_to_read;
  long   l_left_to_read;
  long   l_block_read;
  gint32 l_ii;


  if(gap_debug) printf("p_extract_audioblock samples_to_extract:%d\n", (int)samples_to_extract);


  /* audio block read (blocksize covers playbacktime for 250 frames */
  l_left_to_read = (long)samples_to_extract;
  l_block_read = (double)(250.0) / (double)gvahand->framerate * (double)sample_rate;
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
    if(audio_channels > 1)
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

    if(fp_wav)
    {
      /* write 16 bit wave datasamples
       * sequence mono:    (lo, hi)
       * sequence stereo:  (lo_left, hi_left, lo_right, hi_right)
       */
      for(l_ii=0; l_ii < l_to_read; l_ii++)
      {
         gap_audio_wav_write_gint16(fp_wav, *l_lptr);
         l_lptr++;
         if(audio_channels > 1)
         {
           gap_audio_wav_write_gint16(fp_wav, *l_rptr);
           l_rptr++;
         }
       }
    }

    l_to_read = MIN(l_left_to_read, l_block_read);

    /* handle progress */
    *vidhand->progress = (gdouble) (MAX((samples_to_extract - l_left_to_read), 0))
                       / (gdouble) (MAX(samples_to_extract, 1)) ;

    if(vidhand->status_msg)
    {
      if(fp_wav)
      {
        g_snprintf(vidhand->status_msg, vidhand->status_msg_len
                  , _("extracting audio to tmp audiofile"));
      }
      else
      {
        g_snprintf(vidhand->status_msg, vidhand->status_msg_len
                  , _("seeking audio"));
      }
    }

  }

  /* free audio buffers */
  g_free(left_ptr);
  g_free(right_ptr);


  if(gap_debug) printf("p_extract_audioblock: END\n");

}  /* end p_extract_audioblock */
#endif


#ifdef GAP_ENABLE_VIDEOAPI_SUPPORT
/* ----------------------------------------------------
 * p_extract_audiopart
 * ----------------------------------------------------
 * extract the audiopart of a videofile as RIFF .WAV file
 * the extracted audio range is limited by min_play_sec and max_play_sec
 *
 *  call this procedure with t_GVA_Handle that is NOT NULL (after successful open)
 */
static void
p_extract_audiopart(t_GVA_Handle *gvahand
                   , char *wavfilename
                   , gdouble min_play_sec
                   , gdouble max_play_sec
                   , GapStoryRenderVidHandle *vidhand  /* for progress */
                   )
{
  static int l_audio_channels;
  static int l_sample_rate;
  gdouble samples_to_extract;
  gdouble samples_to_skip;


  if(gap_debug)
  {
    printf("p_extract_audiopart: START\n");
  }

  l_audio_channels = gvahand->audio_cannels;
  l_sample_rate    = gvahand->samplerate;

  samples_to_extract = (max_play_sec - min_play_sec) * (gdouble)l_sample_rate;
  if(samples_to_extract <= 0)
  {
    return;
  }

  samples_to_skip = min_play_sec * (gdouble)l_sample_rate;

  GVA_seek_audio(gvahand, 0.0, GVA_UPOS_SECS);


  if(l_audio_channels > 0)
  {
    FILE  *fp_wav;

    fp_wav = g_fopen(wavfilename, "wb");
    if(fp_wav)
    {
       gint32 l_bytes_per_sample;

       if(l_audio_channels == 1) { l_bytes_per_sample = 2;}  /* mono */
       else                      { l_bytes_per_sample = 4;}  /* stereo */

       /* write the header */
       gap_audio_wav_write_header(fp_wav
                      , (gint32)(samples_to_extract)
                      , l_audio_channels            /* cannels 1 or 2 */
                      , l_sample_rate
                      , l_bytes_per_sample
                      , 16                          /* 16 bit sample resolution */
                      );

      if(gap_debug)
      {
        printf("samples_to_skip:%d\n", (int)samples_to_skip);
      }
      if(samples_to_skip > 0)
      {
        p_extract_audioblock(gvahand
                            ,NULL      /* dont write to file */
                            ,samples_to_skip
                            ,l_audio_channels
                            ,l_sample_rate
                            ,vidhand
                            );
      }
      if(gap_debug)
      {
        printf("samples_to_extract:%d\n", (int)samples_to_extract);
      }

      p_extract_audioblock(gvahand
                          ,fp_wav      /* now write to file */
                          ,samples_to_extract
                          ,l_audio_channels
                          ,l_sample_rate
                          ,vidhand
                          );

      /* close wavfile */
      fclose(fp_wav);

    }
  }

  if(gap_debug)
  {
    printf("p_extract_audiopart: END\n");
  }

}  /* end p_extract_audiopart */

#endif





/* -----------------------------------
 * gap_story_render_audio_add_aud_list
 * -----------------------------------
 */
void
gap_story_render_audio_add_aud_list(GapStoryRenderVidHandle *vidhand, GapStoryRenderAudioRangeElem *aud_elem)
{
  GapStoryRenderAudioRangeElem *aud_listend;


  if((vidhand)  && (aud_elem))
  {
     if(vidhand->parsing_section == NULL)
     {
       printf("** INTERNAL ERROR parsing_section is NULL\n");
       return;
     }
     aud_listend = vidhand->aud_list;
     if (vidhand->parsing_section->aud_list == NULL)
     {
       /* 1. element (or returned list) starts aud_list */
       vidhand->parsing_section->aud_list = aud_elem;
       vidhand->aud_list = aud_elem;
     }
     else
     {
       /* link aud_elem (that can be a single ement or list) to the end of aud_list */
       aud_listend = vidhand->parsing_section->aud_list;
       while(aud_listend->next != NULL)
       {
          aud_listend = (GapStoryRenderAudioRangeElem *)aud_listend->next;
       }
       aud_listend->next = (GapStoryRenderAudioRangeElem *)aud_elem;
     }
  }
}  /* end gap_story_render_audio_add_aud_list */





/* ----------------------------------------------------
 * gap_story_render_audio_new_audiorange_element
 * ----------------------------------------------------
 * allocate a new GapStoryRenderAudioRangeElem for storyboard processing
 * - check audiofile
 * if create_audio_tmp_files == TRUE do audio extract from videofiles
 * and perform samplerate conversions where the results will be
 * temporary RIFF Wav format audiofiles.
 */
GapStoryRenderAudioRangeElem *
gap_story_render_audio_new_audiorange_element(GapStoryRenderAudioType  aud_type
                      ,gint32 track
                      ,const char *audiofile
                      ,gint32  master_samplerate
                      ,gdouble play_from_sec
                      ,gdouble play_to_sec
                      ,gdouble volume_start
                      ,gdouble volume
                      ,gdouble volume_end
                      ,gdouble fade_in_sec
                      ,gdouble fade_out_sec
                      ,char *util_sox
                      ,char *util_sox_options
                      ,const char *storyboard_file  /* IN: NULL if no storyboard file is used */
                      ,const char *preferred_decoder
                      ,GapStoryRenderAudioRangeElem *known_aud_list /* NULL or list of already known ranges */
                      ,GapStoryRenderErrors *sterr           /* element to store Error/Warning report */
                      ,gint32 seltrack      /* IN: select audiotrack number 1 upto 99 for GAP_AUT_MOVIE */
                      ,gboolean create_audio_tmp_files
                      ,gdouble min_play_sec
                      ,gdouble max_play_sec
                      ,GapStoryRenderVidHandle *vidhand  /* for progress */
                      )
{
  GapStoryRenderAudioRangeElem *aud_known;
  GapStoryRenderAudioRangeElem *aud_elem;
  gboolean l_audscan_required;


  if(gap_debug)
  {
     printf("\ngap_story_render_audio_new_audiorange_element: START aud_type:%d\n", (int)aud_type);
     printf("  track:%d:\n", (int)track);
     printf("  create_audio_tmp_files:%d:\n", (int)create_audio_tmp_files);
     printf("  play_from_sec:%.4f:\n", (float)play_from_sec);
     printf("  play_to_sec:%.4f:\n", (float)play_to_sec);
     printf("  volume_start:%.4f:\n", (float)volume_start);
     printf("  volume:%.4f:\n", (float)volume);
     printf("  volume_end:%.4f:\n", (float)volume_end);
     printf("  fade_in_sec:%.4f:\n", (float)fade_in_sec);
     printf("  fade_out_sec:%.4f:\n", (float)fade_out_sec);

     printf("  min_play_sec:%.4f:\n", (float)min_play_sec);
     printf("  max_play_sec:%.4f:\n", (float)max_play_sec);

     if(audiofile)         printf("  audiofile:%s:\n", audiofile);
     if(storyboard_file)   printf("  storyboard_file:%s:\n", storyboard_file);
     if(preferred_decoder) printf("  preferred_decoder:%s:\n", preferred_decoder);
  }

  l_audscan_required = TRUE;

  aud_elem = g_malloc0(sizeof(GapStoryRenderAudioRangeElem));
  aud_elem->aud_type           = aud_type;
  aud_elem->track              = track;
  aud_elem->tmp_audiofile      = NULL;
  aud_elem->audiofile          = NULL;
  aud_elem->samplerate         = master_samplerate;
  aud_elem->channels           = 2;
  aud_elem->bytes_per_sample   = 4;
  aud_elem->samples            = master_samplerate * 9999;
  aud_elem->max_playtime_sec   = 0.0;
  aud_elem->wait_untiltime_sec = 0.0;
  aud_elem->wait_until_samples = 0.0;
  aud_elem->range_playtime_sec = 0.0;
  aud_elem->play_from_sec      = play_from_sec;
  aud_elem->play_to_sec        = play_to_sec;
  aud_elem->volume_start       = volume_start;
  aud_elem->volume             = volume;
  aud_elem->volume_end         = volume_end;
  aud_elem->fade_in_sec        = fade_in_sec;
  aud_elem->fade_out_sec       = fade_out_sec;

  aud_elem->audio_id           = -1;
  aud_elem->aud_data           = NULL;
  aud_elem->ac_elem            = NULL;
  aud_elem->aud_bytelength        = 0;
  aud_elem->range_samples         = 0;
  aud_elem->fade_in_samples       = 0;
  aud_elem->fade_out_samples      = 0;
  aud_elem->byteoffset_rangestart = 0;
  aud_elem->byteoffset_data       = 44;   /* default value, will fit for most WAV files */
  aud_elem->gvahand            = NULL;
  aud_elem->seltrack           = seltrack;

  aud_elem->next               = NULL;

  if(audiofile)
  {
     aud_elem->audiofile = gap_file_make_abspath_filename(audiofile, storyboard_file);

     if(!g_file_test(aud_elem->audiofile, G_FILE_TEST_EXISTS)) /* check for regular file */
     {
        char *l_errtxt;

        l_errtxt = g_strdup_printf(_("file not found:  %s for audioinput"), aud_elem->audiofile);
        gap_story_render_set_stb_error(sterr, l_errtxt);
        g_free(l_errtxt);
        return(NULL);
     }

     if (known_aud_list == NULL)
     {
       if(gap_debug)
       {
         printf("gap_story_render_audio_new_audiorange_element: list of known audiofiles is empty (NULL)\n");
       }
     }

     /* check the list for known audioranges
      * if audiofile is already known, we can skip the file scan
      * (that can save lot of time when the file must be converted or resampled)
      */
     for(aud_known = known_aud_list; aud_known != NULL; aud_known = (GapStoryRenderAudioRangeElem *)aud_known->next)
     {
        if(aud_known->audiofile)
        {
          if(gap_debug)
          {
            printf("gap_story_render_audio_new_audiorange_element: check known audio_id:%d audiofile:%s\n"
               , aud_elem->audio_id
               , aud_known->audiofile
               );
          }

          if((strcmp(aud_elem->audiofile, aud_known->audiofile) == 0)
          && (aud_elem->aud_type == aud_type))
          {
            if(gap_debug)
            {
              printf("gap_story_render_audio_new_audiorange_element: Range is already known audiofile:%s\n", aud_known->audiofile);
            }
            aud_elem->tmp_audiofile     = g_strdup(aud_known->tmp_audiofile);
            aud_elem->samplerate        = aud_known->samplerate;
            aud_elem->channels          = aud_known->channels;
            aud_elem->bytes_per_sample  = aud_known->bytes_per_sample;
            aud_elem->samples           = aud_known->samples;
            aud_elem->max_playtime_sec  = aud_known->max_playtime_sec;
            aud_elem->byteoffset_data   = aud_known->byteoffset_data;

            l_audscan_required = FALSE;
            break;
          }
        }
        else
        {
          if(gap_debug)
          {
            printf("gap_story_render_audio_new_audiorange_element: check known audio_id:%d audiofile: is NULL\n"
              ,aud_elem->audio_id
              );
          }
        }
     }
     if(l_audscan_required)
     {
        gboolean movieIsExtractedAudio = FALSE;

        if(aud_type == GAP_AUT_MOVIE)
        {
             long samplerate;
             long channels;
             long bytes_per_sample;
             long bits;
             long samples;
             int      l_rc;

             l_rc = gap_audio_wav_file_check(aud_elem->audiofile
                     , &samplerate, &channels
                     , &bytes_per_sample, &bits, &samples);
             if (l_rc == 0)
             {
               /* the movie filename refers to an already extracted RIFF WAVE audiofile.
                * Assume that this external audiotrack has full videolength.
                * in this case force audio processing without offset (e.g. set min_play_sec to 0).
                * Note that this special case is a workarond in case of unsupported audio codec
                * where the extraction my be done manually via other tools (that supports the codec)
                * and allows to access the extracted (external) audiotrack
                * in the storyboard by frame numbers just like a movie internal audiotrack.
                */
               movieIsExtractedAudio = TRUE;
               min_play_sec = 0;
             }
        }
#ifdef GAP_ENABLE_VIDEOAPI_SUPPORT
        if((aud_type == GAP_AUT_MOVIE) && (movieIsExtractedAudio == FALSE))
        {
           t_GVA_Handle *gvahand;

           if(preferred_decoder)
           {
             gvahand = GVA_open_read_pref(aud_elem->audiofile
                                         , 1 /*videotrack */
                                         , seltrack
                                         , preferred_decoder
                                         , FALSE  /* use MMX if available (disable_mmx == FALSE) */
                                         );
           }
           else
           {
             gvahand = GVA_open_read(aud_elem->audiofile, 1 /*videotrack */, seltrack);
           }
           if(gvahand)
           {
             /* set smallest fcache size 1 because fcache is not relevant for audio processing */
             GVA_set_fcache_size(gvahand, 1);
             aud_elem->samplerate        = gvahand->samplerate;
             aud_elem->channels          = gvahand->audio_cannels;
             aud_elem->bytes_per_sample  = gvahand->audio_cannels * 2;  /* API operates with 16 bit per sample */
             aud_elem->samples           = gvahand->total_aud_samples;  /* sometimes this is not an exact value, but just a guess */
             aud_elem->byteoffset_data   = 0;                           /* no headeroffset for audiopart loaded from videofiles */

             if(gap_debug)
             {
               printf("AFTER GVA_open_read: %s\n", aud_elem->audiofile);
               printf("   samplerate:      %d\n", (int)aud_elem->samplerate );
               printf("   channels:        %d\n", (int)aud_elem->channels );
               printf("   bytes_per_sample:%d\n", (int)aud_elem->bytes_per_sample );
               printf("   samples:         %d\n", (int)aud_elem->samples );
             }

             if(create_audio_tmp_files)
             {
                     if ((aud_elem->samplerate == master_samplerate)
                     && ((aud_elem->channels == 2) || (aud_elem->channels == 1)))
                     {
                       /* audio part of the video is OK, extract 1:1 as wavfile */
                       aud_elem->tmp_audiofile = gimp_temp_name("tmp.wav");

                       if(gap_debug)
                       {
                         printf("Extracting Audiopart as file:%s\n", aud_elem->tmp_audiofile);
                       }
                       p_extract_audiopart(gvahand
                                          , aud_elem->tmp_audiofile
                                          , min_play_sec
                                          , max_play_sec
                                          , vidhand
                                          );
                       GVA_close(gvahand);
                     }
                     else
                     {
                       char *l_wavfilename;
                       long samplerate;
                       long channels;
                       long bytes_per_sample;
                       long bits;
                       long samples;
                       int      l_rc;

                       /* extract the audiopart for resample */
                       l_wavfilename = gimp_temp_name("tmp.wav");
                       aud_elem->tmp_audiofile = gimp_temp_name("tmp.wav");

                       if(gap_debug)
                       {
                         printf("Resample Audiopart in file:%s outfile: %s\n"
                            , l_wavfilename
                            , aud_elem->tmp_audiofile
                            );
                       }
                       p_extract_audiopart(gvahand
                                          , l_wavfilename
                                          , min_play_sec
                                          , max_play_sec
                                          , vidhand
                                          );
                       GVA_close(gvahand);


                       if(vidhand->status_msg)
                       {
                         g_snprintf(vidhand->status_msg, vidhand->status_msg_len
                                   , _("converting audio (via external program)")
                                   );
                       }
                       /* fake some dummy progress */
                       *vidhand->progress = 0.05;

                       if(gap_debug)
                       {
                         printf("calling externeal Resample program: %s %s\n"
                            , util_sox
                            , util_sox_options
                            );
                       }
                       gap_story_sox_exec_resample(l_wavfilename
                                       ,aud_elem->tmp_audiofile
                                       ,master_samplerate
                                       ,util_sox               /* the extern converter program */
                                       ,util_sox_options       /* options for the converter */
                                       );

                        /* check again (with the resampled copy in tmp_audiofile
                         * that should have been created by gap_story_sox_exec_resample
                         * (if convert was OK)
                         */
                        l_rc = gap_audio_wav_file_check(aud_elem->tmp_audiofile
                                , &samplerate, &channels
                                , &bytes_per_sample, &bits, &samples);

                        /* delete the extracted wavfile after resampling (keep just the resampled variant) */
                        g_remove(l_wavfilename);
                        g_free(l_wavfilename);

                        if((l_rc == 0)
                        && ((bits == 16)    || (bits == 8))
                        && ((channels == 2) || (channels == 1))
                        && (samplerate == master_samplerate))
                        {
                          /* audio file is OK */
                           aud_elem->samplerate        = samplerate;
                           aud_elem->channels          = channels;
                           aud_elem->bytes_per_sample  = bytes_per_sample;
                           aud_elem->samples           = samples;
                           aud_elem->byteoffset_data   = 44;
                           /* TODO: gap_audio_wav_file_check should return the real offset
                            * of the 1.st audio_sample databyte in the wav file.
                            */
                        }
                        else
                        {
                          char *l_errtxt;
                          /* conversion failed, cant use that file, delete tmp_audiofile now */

                          if(aud_elem->tmp_audiofile)
                          {
                             g_remove(aud_elem->tmp_audiofile);
                             g_free(aud_elem->tmp_audiofile);
                             aud_elem->tmp_audiofile = NULL;
                          }
                          if(gap_debug)
                          {
                            printf("AUD 1  channels:%d samplerate:%d master_samplerate:%d\n"
                                   ,(int)channels
                                   ,(int)samplerate
                                   ,(int)master_samplerate
                                   );
                          }
                          l_errtxt = g_strdup_printf(_("cant use file:  %s as audioinput"), aud_elem->audiofile);
                          gap_story_render_set_stb_error(sterr, l_errtxt);
                          g_free(l_errtxt);
                          return(NULL);
                        }

                     }
             }
             else
             {
               GVA_close(gvahand);
             }  /* end if(create_audio_tmp_files) */
           }
           else
           {
               char *l_errtxt;

               l_errtxt = g_strdup_printf(_("ERROR file: %s is not a supported videoformat"), aud_elem->audiofile);
               gap_story_render_set_stb_error(sterr, l_errtxt);
               g_free(l_errtxt);
           }
        }
#endif

        if((aud_type == GAP_AUT_AUDIOFILE) || (movieIsExtractedAudio == TRUE))
        {
          if(g_file_test(aud_elem->audiofile, G_FILE_TEST_EXISTS))
          {
             long samplerate;
             long channels;
             long bytes_per_sample;
             long bits;
             long samples;
             int      l_rc;

             l_rc = gap_audio_wav_file_check(aud_elem->audiofile
                     , &samplerate, &channels
                     , &bytes_per_sample, &bits, &samples);

             if(gap_debug)
             {
               printf("AFTER gap_audio_wav_file_check: %s\n", aud_elem->audiofile);
               printf("   samplerate:      %d\n", (int)samplerate );
               printf("   channels:        %d\n", (int)channels );
               printf("   bytes_per_sample:%d\n", (int)bytes_per_sample );
               printf("   bits:            %d\n", (int) bits);
               printf("   samples:         %d\n", (int)samples );
             }

             if(l_rc == 0)
             {
                aud_elem->samplerate        = samplerate;
                aud_elem->channels          = channels;
                aud_elem->bytes_per_sample  = bytes_per_sample;
                aud_elem->samples           = samples;
                aud_elem->byteoffset_data   = 44;
             }

             if((l_rc == 0)
             && ((bits == 16)    || (bits == 8))
             && ((channels == 2) || (channels == 1))
             && (samplerate == master_samplerate))
             {
               /* audio file is OK */
                aud_elem->samplerate        = samplerate;
                aud_elem->channels          = channels;
                aud_elem->bytes_per_sample  = bytes_per_sample;
                aud_elem->samples           = samples;
                aud_elem->byteoffset_data   = 44;
                /* TODO: gap_audio_wav_file_check should return the real offset
                 * of the 1.st audio_sample databyte in the wav file.
                 */
             }
             else
             {
               if(create_audio_tmp_files)
               {

                  if(vidhand->status_msg)
                  {
                    g_snprintf(vidhand->status_msg, vidhand->status_msg_len
                              , _("converting audio (via external program)")
                              );
                  }
                  /* fake some dummy progress */
                  *vidhand->progress = 0.05;

                  /* aud_elem->tmp_audiofile = g_strdup_printf("%s.tmp.wav", aud_elem->audiofile); */
                  aud_elem->tmp_audiofile = gimp_temp_name("tmp.wav");
                  gap_story_sox_exec_resample(aud_elem->audiofile
                                 ,aud_elem->tmp_audiofile
                                 ,master_samplerate
                                 ,util_sox               /* the extern converter program */
                                 ,util_sox_options       /* options for the converter */
                                 );
                  /* check again (with the resampled copy in tmp_audiofile
                   * that should have been created by gap_story_sox_exec_resample
                   * (if convert was OK)
                   */
                  l_rc = gap_audio_wav_file_check(aud_elem->tmp_audiofile
                          , &samplerate, &channels
                          , &bytes_per_sample, &bits, &samples);

                  if((l_rc == 0)
                  && ((bits == 16)    || (bits == 8))
                  && ((channels == 2) || (channels == 1))
                  && (samplerate == master_samplerate))
                  {
                    /* audio file is OK */
                     aud_elem->samplerate        = samplerate;
                     aud_elem->channels          = channels;
                     aud_elem->bytes_per_sample  = bytes_per_sample;
                     aud_elem->samples           = samples;
                     aud_elem->byteoffset_data   = 44;
                     /* TODO: gap_audio_wav_file_check should return the real offset
                      * of the 1.st audio_sample databyte in the wav file.
                      */
                  }
                  else
                  {
                    char *l_errtxt;
                    /* conversion failed, cant use that file, delete tmp_audiofile now */

                    if(aud_elem->tmp_audiofile)
                    {
                       g_remove(aud_elem->tmp_audiofile);
                       g_free(aud_elem->tmp_audiofile);
                       aud_elem->tmp_audiofile = NULL;
                    }

                    l_errtxt = g_strdup_printf(_("cant use file:  %s as audioinput\n"
                      "(external converter %s FAILED to resample as WAV format at target samplerate %d)")
                      , aud_elem->audiofile
                      , util_sox
                      , master_samplerate);
                    gap_story_render_set_stb_error(sterr, l_errtxt);
                    g_free(l_errtxt);
                    if(gap_debug)
                    {
                      printf("gap_story_render_audio_new_audiorange_element CONVERSION FAILED.\n");
                    }
                    return(NULL);
                  }
               }

             }
          }
        }
     }
  }

  if((aud_elem->samplerate > 0) && (aud_elem->channels > 0))
  {
    /* calculate playtime of the full audiofile */
    aud_elem->max_playtime_sec = (((gdouble)aud_elem->samples / (gdouble)aud_elem->channels) / (gdouble)aud_elem->samplerate);
  }
  else
  {
    aud_elem->max_playtime_sec = 0.0;
  }

  /* from video extracted audio clips are not extracted in full videolength
   * (the start-part in length of min_play_sec is truncated at extract)
   * therefore we have to reduce the play range parameters by min_play_sec
   * to transform for reference in the extracted temp audiofile
   */
  if(min_play_sec > 0.0)
  {
    aud_elem->play_from_sec -= min_play_sec;
    aud_elem->play_to_sec   -= min_play_sec;
  }


  /* constraint range times to max_playtime_sec (full length of the audiofile in secs) */
  /* NO more constraint to max_playtime_sec */
  /* aud_elem->play_to_sec   = CLAMP(aud_elem->play_to_sec,    0.0, aud_elem->max_playtime_sec);*/
  aud_elem->play_from_sec = CLAMP(aud_elem->play_from_sec,  0.0, aud_elem->play_to_sec);

  aud_elem->range_playtime_sec = aud_elem->play_to_sec - aud_elem->play_from_sec;

  aud_elem->fade_in_sec   = CLAMP(aud_elem->fade_in_sec,    0.0, aud_elem->range_playtime_sec);
  aud_elem->fade_out_sec  = CLAMP(aud_elem->fade_out_sec,   0.0, aud_elem->range_playtime_sec);


  aud_elem->range_samples         = aud_elem->range_playtime_sec * aud_elem->samplerate;
  aud_elem->fade_in_samples       = aud_elem->fade_in_sec        * aud_elem->samplerate;
  aud_elem->fade_out_samples      = aud_elem->fade_out_sec       * aud_elem->samplerate;

  {
    gint32 boff;

    /* byte offset truncated to bytes_per_sample boundary */
    boff = (aud_elem->play_from_sec  * aud_elem->samplerate * aud_elem->bytes_per_sample) / aud_elem->bytes_per_sample;

    aud_elem->byteoffset_rangestart = (boff * aud_elem->bytes_per_sample)
                                    + aud_elem->byteoffset_data;
  }


  if(gap_debug)
  {
    printf("gap_story_render_audio_new_audiorange_element NORMAL END\n");
  }

  return(aud_elem);

}  /* end gap_story_render_audio_new_audiorange_element */



/* -----------------------------------------
 * gap_story_render_audio_calculate_playtime
 * -----------------------------------------
 * - check all audio tracks for audio playtime
 *   set *aud_total_sec to the playtime of the
 *   logest audio track playtime.
 */
void
gap_story_render_audio_calculate_playtime(GapStoryRenderVidHandle *vidhand, gdouble *aud_total_sec)
{
  GapStoryRenderAudioRangeElem *aud_elem;
  gint32 l_track;
  gint32 l_min_track;
  gint32 l_max_track;
  gdouble l_tracktime;
  gdouble l_aud_total_sec;

  l_aud_total_sec = 0;
  p_find_min_max_aud_tracknumbers(vidhand->aud_list, &l_min_track, &l_max_track);

  for(l_track=l_min_track; l_track <= l_max_track; l_track++)
  {
    l_tracktime = 0;
    for(aud_elem = vidhand->aud_list; aud_elem != NULL; aud_elem = aud_elem->next)
    {
      if(aud_elem->track == l_track)
      {
        if(aud_elem->wait_untiltime_sec > 0)
        {
          l_tracktime = MAX(l_tracktime, aud_elem->wait_untiltime_sec);
        }
        l_tracktime += aud_elem->range_playtime_sec;
      }
    }
    l_aud_total_sec = MAX(l_aud_total_sec, l_tracktime);
  }
  *aud_total_sec = l_aud_total_sec;
}  /* gap_story_render_audio_calculate_playtime */


/* -------------------------------------------------
 * gap_story_render_audio_create_composite_audiofile
 * -------------------------------------------------
 * create the composite audio as mix of all audio channels
 * and write result to file in WAV format.
 */
gboolean
gap_story_render_audio_create_composite_audiofile(GapStoryRenderVidHandle *vidhand
                            , char *comp_audiofile
                            )
{
  gdouble l_mix_scale;
  gdouble l_aud_total_sec;
  FILE   *l_fp;
  gboolean  l_retval;

  if(gap_debug)
  {
    printf("gap_story_render_audio_create_composite_audiofile START comp_audiofile: %s\n"
       , comp_audiofile);
  }
  l_retval = FALSE;
  gap_story_render_audio_calculate_playtime(vidhand, &l_aud_total_sec);

  if(vidhand->status_msg)
  {
    g_snprintf(vidhand->status_msg, vidhand->status_msg_len, _("checking audio peaks"));
  }
  *vidhand->progress = 0.0;

  if(gap_debug)
  {
    printf("gap_story_render_audio_create_composite_audiofile CHECKING AUDIO PEAKS %s\n"
       , comp_audiofile);
  }

  p_check_audio_peaks(vidhand, l_aud_total_sec, &l_mix_scale);

  if(gap_debug)
  {
    printf("gap_story_render_audio_create_composite_audiofile WRITE COMPOSITE AUDIO FILE %s\n"
       , comp_audiofile);
  }

  l_fp = g_fopen(comp_audiofile, "wb");
  if (l_fp)
  {
    gdouble l_nsamples;

    l_nsamples = l_aud_total_sec * (gdouble)vidhand->master_samplerate;
    gap_audio_wav_write_header(l_fp
                            , (gint32)l_nsamples
                            , 2                           /* cannels */
                            , vidhand->master_samplerate
                            , 4                           /* 4bytes for 16bit stereo */
                            , 16                          /* 16 bit sample resolution */
                            );

    if(vidhand->status_msg)
    {
      g_snprintf(vidhand->status_msg, vidhand->status_msg_len, _("writing composite audiofile"));
    }
    *vidhand->progress = 0.0;


    p_mix_audio(l_fp, NULL, NULL, vidhand, l_aud_total_sec, &l_mix_scale);

    fclose(l_fp);
    if(gap_debug)
    {
      printf("gap_story_render_audio_create_composite_audiofile COMPOSITE AUDIO FILE written OK file:%s\n"
         , comp_audiofile);
    }
    l_retval = TRUE;
  }
  else
  {
    char *l_errtext;

    l_errtext = g_strdup_printf(_("cant write audio to file: %s "), comp_audiofile);
    gap_story_render_set_stb_error(vidhand->sterr, l_errtext);

    printf("gap_story_render_audio_create_composite_audiofile failed to write COMPOSITE AUDIO FILE file:%s\n  **ERROR: %s\n"
         , comp_audiofile
         , l_errtext
         );
    g_free(l_errtext);
  }

  if(vidhand->status_msg)
  {
    g_snprintf(vidhand->status_msg, vidhand->status_msg_len, _("ready"));
  }
  return(l_retval);

}   /* end gap_story_render_audio_create_composite_audiofile */


