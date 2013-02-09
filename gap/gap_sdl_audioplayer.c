/* gap_sdl_audioplayer.c
 *  implements audio wavefile playback via SDL medialibrary.
 *  (SDL is available for Linux, Windows and MacOS)
 *
 *  This player uses an interface that is compatible with
 *  the wavplay client wrapper, but adds support for playback of
 *  audio pcm data from a preloaded memory buffer.
 *
 * Note that the (older) wavplay server/client based audioplayback
 * may be configured at compiletime as alternative on Linux only.
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
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <SDL.h>
#include <SDL_audio.h>
#include <glib.h>
#include <glib/gstdio.h>

#include <gap_sdl_audioplayer.h>

#include <gap_audio_wav.h>


#define NUM_SOUNDSLOTS 1   /* number of sounds that can be played (mixed) at same time */
   /* Note that the current implementation is partly prepared for more than one sound at a time
    * but limited to 1 Sound (due to wavplay compatible API that does not require this feature)
    * Note that Playing from multiple slots also requires same audio parameters (channels, samplerate, format)
    * for audio data in all slots.
    */

#define AUDIO_BUFFER_SIZE_IN_SAMPLES 4096

/* The AUDIO_FILE_PACKET_SIZE shall be large enough to provide all requested samples (len)
 * in the mixaudio_callback with only one file read (fread) operation for optimal performance.
 * in tests on Linux systems a size of AUDIO_BUFFER_SIZE_IN_SAMPLES * 4 was sufficient
 * but tests on Windows7 showed that the mixaudio_callback requested a len value of 44112 bytes.
 * therfore the AUDIO_FILE_PACKET_SIZE was increased to AUDIO_BUFFER_SIZE_IN_SAMPLES * 32
 * that should work on most systems.
 * (smaller AUDIO_BUFFER_SIZE_IN_SAMPLES was also successfully tested on Windows7)
 *
 */
#define AUDIO_FILE_PACKET_SIZE  (AUDIO_BUFFER_SIZE_IN_SAMPLES * 32)


#define MAX_WAV_FILENAME_LENGTH 2048

typedef struct {
    Uint32            start_at_sample;  /* offset (where to start play) in number of audio frames */
    int               mix_volume;       /* 0 upto SDL_MIX_MAXVOLUME (Note this does not change hardware volume.) */

    /* preloaded memory slot (relevant in case data != NULL) */
    Uint8 *data;   /* the audio sample data */
    Uint32 dpos;   /* the current data position in bytes (points to rest of data that has not yet been played) */
    Uint32 dlen;   /* the length of audio sample data in bytes */

    /* file based audio slot */
    FILE             *fpWav;                       /* filehandle to read from audio wavefile */
    long              offset_to_first_sample;      /* fseek offset to the 1st sample data byte in the audio wavefile */
    Uint8             audio_buf[AUDIO_FILE_PACKET_SIZE];  /* buffer for fetching audio sample data from file */
    char              wav_filename[MAX_WAV_FILENAME_LENGTH];   /* the name of the audio file to be played  */
} AudioSlot;


typedef struct
{
  /* audio params */
  unsigned int      samplerate;
  unsigned int      channels;
  unsigned int      bits_per_sample;
  unsigned int      frame_size;       /* frame_size = (bits_per_sample / 8) * channels */
  unsigned int      format;           /* AUDIO_S16, AUDIO_U8, ... */

  /* private attribuites (managed by the playback_callback thread only) */
  SDL_AudioSpec     sdlAudioSpec;
  AudioSlot         sounds[NUM_SOUNDSLOTS];

} AudioPlaybackThreadUserData;


static AudioPlaybackThreadUserData *audioPlaybackThreadUserDataPtr = NULL;

extern int gap_debug;

/* ------------------------------------------------
 * call_errfunc
 * ------------------------------------------------
 * Error reporting function for this source module:
 */
static void
call_errfunc(GapSdlErrFunc v_erf, const char *format,...) {
  va_list ap;

  if ( v_erf == NULL )
  {
    return;                             /* Only report error if we have function */
  }
  va_start(ap,format);
  v_erf(format,ap);                     /* Use caller's supplied function */
  va_end(ap);
}

/* -------------------------------------
 * mixaudio_callback
 * -------------------------------------
 * this callback function will be called when the audio device is ready for more data.
 * It is passed a pointer to user specific data, the audio buffer, and the length in bytes of the audio buffer.
 * This function usually runs in a separate thread, and so access to data structures
 * must be protected by calling SDL_LockAudio and SDL_UnlockAudio.
 *
 * This callback fetches audio data from preloaded memory (at address usrPtr->sounds[i].data)
 * or tries to fetch from file
 *    in case address usrPtr->sounds[i].data is NULL and
 *    usrPtr->sounds[i].fpWav  refers to an audiofile opened for read access.
 *
 * it is assumed that the audiofile contains uncopressed PCM data
 * matching the current audio playback settings (channels, samplerate, bits_per_sample,...)
 * .. e.g no converting of different channels, samplerates is done in this function.
 */
void mixaudio_callback(void *userData, Uint8 *stream, int len)
{
  int ii;
  Uint32 amount;
  AudioPlaybackThreadUserData *usrPtr;


  usrPtr = (AudioPlaybackThreadUserData *) userData;
  if (usrPtr == NULL)
  {
    return;
  }

  SDL_LockAudio();

  for (ii=0; ii < NUM_SOUNDSLOTS; ii++)
  {
    if (usrPtr->sounds[ii].data == NULL)
    {
      int lenRest;
      int lenToDeliver;
      Uint8 *streamPos;

      lenToDeliver = len;
      streamPos = stream;

      while(lenToDeliver > 0)
      {
        lenRest = MIN(lenToDeliver, AUDIO_FILE_PACKET_SIZE);
        amount = AUDIO_FILE_PACKET_SIZE;
        if ( amount > lenRest )
        {
          amount = lenRest;
        }
        /* try fetch data packet direct from file */
        if ((usrPtr->sounds[ii].fpWav == NULL) || (amount <= 0))
        {
          /* file not available (already closed when plyaed until end */
          amount = 0;
        }
        else
        {
          int  datasize;
          datasize = fread(&usrPtr->sounds[ii].audio_buf[0]
                  , 1
                  , amount
                  , usrPtr->sounds[ii].fpWav
                  );
          if(gap_debug)
          {
            printf("mixaudio_callback fread amount:%d datasize:%d len:%d lenToDeliver:%d lenRest:%d mix_volume:%d\n"
                , amount
                , datasize
                , len
                , lenToDeliver
                , lenRest
                , (int)usrPtr->sounds[ii].mix_volume
                );
          }
          if (datasize != amount)
          {
            if (datasize > 0)
            {
              amount = datasize;
            }
            else
            {
              amount = 0;
            }
          }

        }
        SDL_MixAudio(streamPos
                , &usrPtr->sounds[ii].audio_buf[0]
                , amount
                , usrPtr->sounds[ii].mix_volume
                );
        if(amount < lenRest)
        {
          break;
        }
        lenToDeliver -= amount;
        streamPos += amount;

      }


    }
    else
    {
      /* fetch data from preloaded memory data buffer */
      amount = (usrPtr->sounds[ii].dlen - usrPtr->sounds[ii].dpos);
      if ( amount > len )
      {
        amount = len;
      }
      if(gap_debug)
      {
          printf("mixaudio_callback mix amount:%d len:%d mix_volume:%d\n"
                , amount
                , len
                , (int)usrPtr->sounds[ii].mix_volume
                );
      }
      SDL_MixAudio(stream
                , &usrPtr->sounds[ii].data[usrPtr->sounds[ii].dpos]
                , amount
                , usrPtr->sounds[ii].mix_volume
                );
      usrPtr->sounds[ii].dpos += amount;
    }
  }

  SDL_UnlockAudio();

}  /* end mixaudio_callback */



/* ------------------------------------------------
 * newAudioPlaybackThreadUserData
 * ------------------------------------------------
 * allocate a new AudioPlaybackThreadUserData structure
 * and initialize with default values.
 */
static AudioPlaybackThreadUserData *
newAudioPlaybackThreadUserData()
{
  int ii;
  AudioPlaybackThreadUserData  *usrPtr;

  /* init user data for the audio_playback_thread */
  usrPtr = g_new0 (AudioPlaybackThreadUserData, 1);

  /* prepared parameters (relevant for next playback start) */
  usrPtr->samplerate = 44100;
  usrPtr->channels = 2;
  usrPtr->bits_per_sample = 16;
  usrPtr->frame_size = (usrPtr->bits_per_sample / 8) * usrPtr->channels;
  usrPtr->format = AUDIO_S16;

  /* actual parameters (relevant for playback) */
  usrPtr->sdlAudioSpec.freq = 44100;
  usrPtr->sdlAudioSpec.format = AUDIO_S16;
  usrPtr->sdlAudioSpec.channels = 2;
  usrPtr->sdlAudioSpec.samples = AUDIO_BUFFER_SIZE_IN_SAMPLES;   /* 512 upto 8192 are recommanded values */
  usrPtr->sdlAudioSpec.callback = mixaudio_callback;
  usrPtr->sdlAudioSpec.userdata = usrPtr;


  /* init sound slots (relevant for filling audio buffer
   * at playback int mixaudio_callback thread)
   */
  for (ii=0; ii < NUM_SOUNDSLOTS; ii++)
  {
    usrPtr->sounds[ii].start_at_sample = 0;
    usrPtr->sounds[ii].mix_volume = SDL_MIX_MAXVOLUME;
    usrPtr->sounds[ii].data = NULL;
    usrPtr->sounds[ii].dpos = 0;
    usrPtr->sounds[ii].dlen = 0;

    usrPtr->sounds[ii].fpWav = NULL;
    usrPtr->sounds[ii].offset_to_first_sample = 0;
    usrPtr->sounds[ii].wav_filename[0] = '\0';
  }

  return (usrPtr);

}  /* end newAudioPlaybackThreadUserData */


/* ------------------------------------------------
 * getUsrPtr
 * ------------------------------------------------
 */
static AudioPlaybackThreadUserData *
getUsrPtr()
{
  AudioPlaybackThreadUserData  *usrPtr;
  if (audioPlaybackThreadUserDataPtr == NULL)
  {
    audioPlaybackThreadUserDataPtr = newAudioPlaybackThreadUserData();
  }

  usrPtr = audioPlaybackThreadUserDataPtr;
  return (usrPtr);

}  /* end getUsrPtr */

/* ------------------------------------------------
 * close_files
 * ------------------------------------------------
 */
static void
close_files()
{
  AudioPlaybackThreadUserData  *usrPtr;

  usrPtr = getUsrPtr();
  if (usrPtr != NULL)
  {
    int ii;

    SDL_LockAudio();
    for (ii=0; ii < NUM_SOUNDSLOTS; ii++)
    {
      if (usrPtr->sounds[ii].fpWav != NULL)
      {
        fclose(usrPtr->sounds[ii].fpWav);
        usrPtr->sounds[ii].fpWav = NULL;
      }
    }
    SDL_UnlockAudio();
  }

}  /* end close_files */


/* ------------------------------------------------
 * stop_audio
 * ------------------------------------------------
 */
static void
stop_audio()
{
  AudioPlaybackThreadUserData  *usrPtr;

  usrPtr = getUsrPtr();
  if (usrPtr != NULL)
  {
    SDL_LockAudio();

    if(SDL_GetAudioStatus() != SDL_AUDIO_STOPPED)
    {
      SDL_CloseAudio();
    }
    SDL_UnlockAudio();
  }

}  /* end stop_audio */


/* ------------------------------------------------
 * start_audio
 * ------------------------------------------------
 * init SDL audio with new (pending) setings and (re)start audio playback
 * from prepared position "start_at_sample"
 * (e.g. from begin in case gap_sdl_start_sample was not called before)
 *
 * in case audio is already playing stop it, and restart with the new settings.
 */
static int
start_audio()
{
  AudioPlaybackThreadUserData  *usrPtr;
  int ii;

  usrPtr = getUsrPtr();
  if (usrPtr == NULL)
  {
    return (1);
  }

  SDL_LockAudio();
  if(SDL_GetAudioStatus() != SDL_AUDIO_STOPPED)
  {
    SDL_CloseAudio();
  }

  for (ii=0; ii < NUM_SOUNDSLOTS; ii++)
  {
    if (usrPtr->sounds[ii].data == NULL)
    {
      if (usrPtr->sounds[ii].fpWav == NULL)
      {
        usrPtr->sounds[ii].fpWav = gap_audio_wav_open_seek_data(usrPtr->sounds[ii].wav_filename);
      }

      if (usrPtr->sounds[ii].fpWav != NULL)
      {
        long seekPosition;

        seekPosition = usrPtr->sounds[ii].offset_to_first_sample
                     + (usrPtr->sounds[ii].start_at_sample * usrPtr->frame_size);

        fseek(usrPtr->sounds[ii].fpWav, seekPosition, SEEK_SET);
      }
    }
    else
    {
      usrPtr->sounds[ii].dpos = usrPtr->sounds[ii].start_at_sample * usrPtr->frame_size;
      if (usrPtr->sounds[ii].dpos >= usrPtr->sounds[ii].dlen)
      {
        /* reset to begin on illegal start position */
        usrPtr->sounds[ii].dpos = 0;
      }
    }
  }


  /* load sdlAudioSpec (relevant for playback) from prepared values */
  usrPtr->sdlAudioSpec.freq = usrPtr->samplerate;
  usrPtr->sdlAudioSpec.format = usrPtr->format;
  usrPtr->sdlAudioSpec.channels = usrPtr->channels;
  usrPtr->sdlAudioSpec.samples = AUDIO_BUFFER_SIZE_IN_SAMPLES;        /* audio buffer size (512 upto 8192 recommanded) */


  SDL_UnlockAudio();
  if ( SDL_OpenAudio(&usrPtr->sdlAudioSpec, NULL) < 0 )
  {
    /* Unable to open audio use SDL_GetError() to query reason */
    return(2);
  }

  SDL_PauseAudio(0);  /* pause value 0 triggers playback start */

  return (0);  /* OK */

}  /* end start_audio */



/* -------------------------------------
 * gap_sdl_start
 * -------------------------------------
 * prepare the audio_playback.
 * (not mandatory in this SDL based implementation)
 */
int
gap_sdl_start(GapSdlErrFunc erf)
{
  AudioPlaybackThreadUserData  *usrPtr;

  usrPtr = getUsrPtr();
  if (usrPtr != NULL)
  {
    return (0);  /* OK */
  }

  call_errfunc(erf, "failed to prepare audio_playback.");
  return (1);

}  /* end gap_sdl_start */


/* -------------------------------------
 * gap_sdl_memory_buffer
 * -------------------------------------
 */
int
gap_sdl_memory_buffer(char *buffer, long buffer_len, GapSdlErrFunc erf)
{
  int rc = 1;
  int ii = 0;
  AudioPlaybackThreadUserData  *usrPtr;

  usrPtr = getUsrPtr();
  if (usrPtr != NULL)
  {
    stop_audio();
    usrPtr->sounds[ii].data = buffer;   /* point to memory provided by the caller */
    usrPtr->sounds[ii].dpos = 0;
    usrPtr->sounds[ii].dlen = buffer_len;

    if (usrPtr->sounds[ii].fpWav != NULL)
    {
      fclose(usrPtr->sounds[ii].fpWav);
      usrPtr->sounds[ii].fpWav = NULL;
    }
    rc = 0;  /* OK */
  }

  if ( (rc != 0) && (erf != NULL) )
  {
    call_errfunc(erf, "memory_buffer: %ld len: %ld not accepted", (long)&buffer, buffer_len);
  }

  return (rc);

}  /* end gap_sdl_memory_buffer */



/* -------------------------------------
 * gap_sdl_path
 * -------------------------------------
 *
 * Send a pathname to the audio_player.
 * (use flags =1 to keep current playback parameters)
 * typical call with flags == 0 does init the audio playback parameters
 * from the header information of the specified RIFF WAVE file (rfered by pathname)
 */
int
gap_sdl_path(const char *pathname,int flags,GapSdlErrFunc erf)
{
  int rc = 1;
  AudioPlaybackThreadUserData  *usrPtr;

  usrPtr = getUsrPtr();
  if (usrPtr != NULL)
  {
    int rcCheck;
    long sample_rate;
    long channels;
    long bytes_per_sample;
    long bits;
    long samples;

    rcCheck = gap_audio_wav_file_check(pathname, &sample_rate, &channels
                           , &bytes_per_sample, &bits, &samples);
    if (rcCheck == 0)
    {
      int ii;

      rc = 0;  /* OK */
      ii = 0;
      stop_audio();
      usrPtr->sounds[ii].data = NULL;   /* no preloded memory available */
      usrPtr->sounds[ii].dpos = 0;
      usrPtr->sounds[ii].dlen = 0;


      if (flags != 1)
      {
        /* set audio params from wavefile header */
        usrPtr->format = AUDIO_S16;

        usrPtr->samplerate = sample_rate;
        usrPtr->channels = channels;
        usrPtr->bits_per_sample = bits;
        usrPtr->frame_size = (usrPtr->bits_per_sample / 8) * usrPtr->channels;
        switch (bits)
        {
          case 8:
            usrPtr->format = AUDIO_U8;
            break;
          case 16:
            usrPtr->format = AUDIO_S16;  /* same as AUDIO_S16LSB */
            break;
          default:
            rc = 2;  /* unsupported bits_per_channel value */
            break;
        }
      }
      if (rc == 0)
      {
        if (usrPtr->sounds[ii].fpWav != NULL)
        {
          if (strcmp(usrPtr->sounds[ii].wav_filename, pathname) != 0)
          {
            /* file differs from currently opened wavfile, force close/reopen */
            fclose(usrPtr->sounds[ii].fpWav);
            usrPtr->sounds[ii].fpWav = NULL;
          }
          else
          {
            long seekPosition;

            /* current wavfile was specified again, just rewind existing filehandle */
            seekPosition = usrPtr->sounds[ii].offset_to_first_sample;
            fseek(usrPtr->sounds[ii].fpWav, seekPosition, SEEK_SET);
          }
        }

        if (usrPtr->sounds[ii].fpWav == NULL)
        {
          g_snprintf(usrPtr->sounds[ii].wav_filename, MAX_WAV_FILENAME_LENGTH -1, pathname);
          usrPtr->sounds[ii].wav_filename[MAX_WAV_FILENAME_LENGTH -1] = '\0';
          usrPtr->sounds[ii].offset_to_first_sample = 0;
          usrPtr->sounds[ii].fpWav = gap_audio_wav_open_seek_data(usrPtr->sounds[ii].wav_filename);
        }

        if(usrPtr->sounds[ii].fpWav == NULL)
        {
          rc = 3;  /* failed to open audio data file */
        }
        else
        {
          /* store current file offset (position of the 1st audio sample data byte) */
          usrPtr->sounds[ii].offset_to_first_sample = ftell(usrPtr->sounds[ii].fpWav);
        }
      }

    }
  }

  if ( (rc != 0) && (erf != NULL) )
  {
     if (pathname != NULL)
     {
       // TODO provide to_string for errorcodes rc
       call_errfunc(erf, "path: Not a valid RIFF WAVE file %s rc:%d (%s)", pathname, rc, " ");
     }
     else
     {
       call_errfunc(erf, "path: is NULL (name of RIFF WAVE file was expected).");
     }
  }

  return (rc);

}  /* end gap_sdl_path */


/* -------------------------------------
 * gap_sdl_volume
 * -------------------------------------
 * Tell audio_player to use mix volume (0.0 upto 1.0)
 * (Note: this does not set the hardware audio volume,
 *  values smaller than 1 may reduce the sound quality due to data loss while mix down)
 */
int
gap_sdl_volume(double volume, int flags, GapSdlErrFunc erf)
{
  int rc = 1;
  AudioPlaybackThreadUserData  *usrPtr;

  usrPtr = getUsrPtr();
  if (usrPtr != NULL)
  {
    if ((volume >= 0.0) && (volume <= 1.0))
    {
      int ii = 0;
      gdouble mixVolumeDouble;

      SDL_LockAudio();
      mixVolumeDouble = SDL_MIX_MAXVOLUME;
      mixVolumeDouble = mixVolumeDouble * volume;
      usrPtr->sounds[ii].mix_volume = (int)mixVolumeDouble;
      SDL_UnlockAudio();

      rc = 0;  /* OK */
    }
  }

  if ( (rc != 0) && (erf != NULL) )
  {
    call_errfunc(erf, "volume: value %f not accepted (valid range is 0.0 to 1.0)", (float)volume);
  }

  return (rc);

}  /* end gap_sdl_volume */


/* -------------------------------------
 * gap_sdl_channels
 * -------------------------------------
 *  Tell audio_player the number of channels
 */
int
gap_sdl_channels(int flags,GapSdlErrFunc erf, int channels)
{
  int rc = 1;
  AudioPlaybackThreadUserData  *usrPtr;

  usrPtr = getUsrPtr();
  if (usrPtr != NULL)
  {
    usrPtr->channels = channels;
    usrPtr->frame_size = (usrPtr->bits_per_sample / 8) * usrPtr->channels;
    rc = 0;  /* OK */
  }

  if ( (rc != 0) && (erf != NULL) )
  {
    call_errfunc(erf, "channels: %d not accepted", channels);
  }

  return (rc);

}  /* end gap_sdl_channels */


/* -------------------------------------
 * gap_sdl_bits
 * -------------------------------------
 * Tell audio_player about new data bits per sample and sample format setting:
 * flags: 0  signed sample   little endian (AUDIO_S8, AUDIO_S16LSB)
 * flags: 1  unsigned sample little endian (AUDIO_U8, AUDIO_U16LSB)
 * flags: 2  signed sample   big endian    (AUDIO_S16MSB)
 * flags: 3  unsigned sample big endian    (AUDIO_U16MSB)
 */
int
gap_sdl_bits(int flags,GapSdlErrFunc erf,int bits)
{
  int rc = 1;
  AudioPlaybackThreadUserData  *usrPtr;

  usrPtr = getUsrPtr();
  if (usrPtr != NULL)
  {
    rc = 0;  /* OK */
    switch (bits)
    {
      case 8:
        usrPtr->bits_per_sample = bits;
        if ((flags & 1) != 0) { usrPtr->format = AUDIO_U8; }
        else                  { usrPtr->format = AUDIO_S8; }
        break;
      case 16:
        usrPtr->bits_per_sample = bits;
        if      ((flags & 3) == 1) { usrPtr->format = AUDIO_U16LSB; }
        else if ((flags & 3) == 2) { usrPtr->format = AUDIO_S16MSB; }
        else if ((flags & 3) == 3) { usrPtr->format = AUDIO_U16MSB; }
        else                       { usrPtr->format = AUDIO_S16; }  /* same as AUDIO_S16LSB */
        break;
      default:
        rc = 2;  /* unsupported bits_per_channel value */
        break;
    }
    usrPtr->frame_size = (usrPtr->bits_per_sample / 8) * usrPtr->channels;
  }

  if ( (rc != 0) && (erf != NULL) )
  {
    call_errfunc(erf, "bits: %d not accepted", bits);
  }

  return (rc);

}  /* end gap_sdl_bits */




/* -------------------------------------
 * gap_sdl_sampling_rate
 * -------------------------------------
 * Tell audio_player to use new sampling rate:
 */
int
gap_sdl_sampling_rate(int flags,GapSdlErrFunc erf,Uint32 sampling_rate)
{
  int rc = 1;
  AudioPlaybackThreadUserData  *usrPtr;

  usrPtr = getUsrPtr();
  if (usrPtr != NULL)
  {
    usrPtr->samplerate = sampling_rate;
    rc = 0;  /* OK */
  }

  if ( (rc != 0) && (erf != NULL) )
  {
    call_errfunc(erf, "sampling_rate: %d not accepted", sampling_rate);
  }

  return (rc);

}  /* end gap_sdl_sampling_rate */




/* -------------------------------------
 * gap_sdl_start_sample
 * -------------------------------------
 * Tell audio_player to start at a new sample number:
 */
int
gap_sdl_start_sample(int flags, GapSdlErrFunc erf, Uint32 sample)
{
  int rc = 1;
  int ii = 0;
  AudioPlaybackThreadUserData  *usrPtr;

  usrPtr = getUsrPtr();
  if (usrPtr != NULL)
  {
    usrPtr->sounds[ii].start_at_sample = sample;
    rc = 0;  /* OK */
  }

  if ( (rc != 0) && (erf != NULL) )
  {
    call_errfunc(erf, "start_sample: %d not accepted", (int)sample);
  }

  return rc;

}  /* end gap_sdl_start_sample */



/* -------------------------------------
 * msg_name
 * -------------------------------------
 * convert int cmd to readable string representation.
 */
static const char *
msg_name(int cmd)
{
  switch(cmd)
  {
    case GAP_SDL_CMD_Bye:
      return ("GAP_SDL_CMD_Bye");
      break;
    case GAP_SDL_CMD_Play:
      return ("GAP_SDL_CMD_Play");
      break;
    case GAP_SDL_CMD_Pause:
      return ("GAP_SDL_CMD_Pause");
      break;
    case GAP_SDL_CMD_Stop:
      return ("GAP_SDL_CMD_Stop");
      break;
    case GAP_SDL_CMD_Restore:
      return ("GAP_SDL_CMD_Restore");
      break;
    case GAP_SDL_CMD_SemReset:
      return ("GAP_SDL_CMD_SemReset");
      break;
  }
  return ("unknown");

}  /* end msg_name */


/* -------------------------------------
 * gap_sdl_cmd
 * -------------------------------------
 * perform simple audio_player command .
 * Note: some of the commands do not make sense
 *       in this SDL based implementation
 *       and are just dummies for compatibility with the
 *       API (that was designed base on the older wavplay client functions)
 */
int
gap_sdl_cmd(int cmd,int flags,GapSdlErrFunc erf) {
  static char *sdl_no_error_available = "";
  char *sdl_error;
  int rc = 1;
  AudioPlaybackThreadUserData  *usrPtr;


  sdl_error = sdl_no_error_available;
  usrPtr = getUsrPtr();
  if (usrPtr != NULL)
  {
    if(gap_debug)
    {
      printf("gap_sdl_cmd cmd:%d (%s) flags:%d SdlAudioStatus:%d\n"
            , cmd
           , msg_name(cmd)
           , flags
           , (int)SDL_GetAudioStatus()
           );
    }
    switch(cmd)
    {
      case GAP_SDL_CMD_Bye:
        stop_audio();
        close_files();
        rc = 0;  /* OK */
        break;
      case GAP_SDL_CMD_Play:
        rc = start_audio();
        sdl_error = SDL_GetError();  /* SDL_GetError uses sttically allocated message that must NOT be freed */
        break;
      case GAP_SDL_CMD_Pause:
        stop_audio();
        rc = 0;  /* OK */
        break;
      case GAP_SDL_CMD_Stop:
        stop_audio();
        close_files();
        rc = 0;  /* OK */
        break;
      case GAP_SDL_CMD_Restore:
        stop_audio();
        rc = 0;  /* OK */
        break;
      case GAP_SDL_CMD_SemReset:
        rc = 0;  /* OK */
        break;
    }

  }


  if ((rc != 0) && (erf != NULL ))
  {
    call_errfunc(erf, "%s: Sending cmd%d to audio_player failed (rc:%d) err:%s",
                        msg_name(cmd),
                        cmd,
                        sdl_error,
                        rc);
  }
  if(gap_debug)
  {
    printf("gap_sdl_cmd cmd:%d (%s) flags:%d retcode:%d\n", cmd, msg_name(cmd), flags, rc);
  }
  return rc;  /* Zero indicates success */

}  /* end gap_sdl_cmd */
