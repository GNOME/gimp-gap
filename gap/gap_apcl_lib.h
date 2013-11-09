/* gap_apcl_lib.h
 *   Provides the GAP Audio Player Client support API functions.
 *
 * This module includes the relevant audio playback
 * implementation which is configured at compiletime.
 *
 * following implementations are available:
 * - SDL media library based audioplayback        GAP_ENABLE_AUDIO_SUPPORT_SDL
 * - wavplay client/server based audioplayback    GAP_ENABLE_AUDIO_SUPPORT_WAVPLAY
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
#ifndef _GAP_APCL_LIB_H
#define _GAP_APCL_LIB_H

#define WPC_FLAGS_BITS_SIGNED_LITTLE_ENDIAN_SAMPLE     0
#define WPC_FLAGS_BITS_UNSIGNED_LITTLE_ENDIAN_SAMPLE   1
#define WPC_FLAGS_BITS_SIGNED_BIG_ENDIAN_SAMPLES       2
#define WPC_FLAGS_BITS_UNSIGNED_BIG_ENDIAN_SAMPLE      3


#ifdef GAP_ENABLE_AUDIO_SUPPORT_SDL
#define GAP_ENABLE_AUDIO_SUPPORT 1
/* ------- START SDL based stuff ----------------------------------------------------------- */

/* use audio playback based on the SDL media library
 * (SDL works on multiple Operating systems including Linux and Windows and McOS)
 *
 * AudioPlayerCLient Wrapper Procedures
 * Implementation for SDL based audio player
 * can use MACRO definitions for most Procedures
 */

#include <gap_sdl_audioplayer.h>

#define APCL_ErrFunc GapSdlErrFunc

static int apcl_have_memory_playback(void) { return (1); }

/* Define MACROs to map the API functions ( apcl_* ) to SDL based player functions (gap_sdl_*) */
#define apcl_memory_buffer(buffer,len,erf) gap_sdl_memory_buffer(buffer,len,erf) /* Tell audio_player a buffer with PCM audiodata */
#define apcl_channels(flags,erf,channels) gap_sdl_channels(flags,erf,channels)   /*  Tell audio_player the number of channels */
#define apcl_bits(flags,erf,bits) gap_sdl_bits(flags,erf,bits)                   /*  Tell audio_player the number of bits per sample */
#define apcl_volume(volume,flags,erf) gap_sdl_volume(volume,flags,erf)           /*  Tell audio_player the mixvolume 0.0 to 1.0 */

#define apcl_bye(flags,erf) gap_sdl_cmd(GAP_SDL_CMD_Bye,flags,erf)	/* Tell audio_player to exit */
#define apcl_play(flags,erf) gap_sdl_cmd(GAP_SDL_CMD_Play,flags,erf)	/* Tell audio_player to play */
#define apcl_pause(flags,erf) gap_sdl_cmd(GAP_SDL_CMD_Pause,flags,erf) /* Tell audio_player to pause */
#define apcl_stop(flags,erf) gap_sdl_cmd(GAP_SDL_CMD_Stop,flags,erf)	/* Tell audio_player to stop */
#define apcl_restore(flags,erf) gap_sdl_cmd(GAP_SDL_CMD_Restore,flags,erf) /* Tell audio_player to restore settings */
#define apcl_semreset(flags,erf) gap_sdl_cmd(GAP_SDL_CMD_SemReset,flags,erf) /* No operation (audio_player has no semaphores) */

#define apcl_start(erf) gap_sdl_start(erf)  /* Tell audio_player to init a playback thread */
#define apcl_path(path,flags,erf) gap_sdl_path(path,flags,erf)	                   /* Tell audio_player a pathname */
#define apcl_sampling_rate(rate,flags,erf) gap_sdl_sampling_rate(flags,erf,rate)  /* Tell audio_player the samplingrate */
#define apcl_start_sample(offs,flags,erf) gap_sdl_start_sample(flags,erf,offs)    /* Tell audio_player where to start playback */



/* ------- END SDL based stuff ----------------------------------------------------------- */

#else
#ifdef GAP_ENABLE_AUDIO_SUPPORT_WAVPLAY
#define GAP_ENABLE_AUDIO_SUPPORT 1

/* ------- START waveplay client/server based stuff ----------------------------------------------------------- */
/* use wavplay as external audio server (available for Linux) */

#include <wavplay.h>
#include <wavfile.h>
#include <client.h>


/* AudioPlayerCLient Wrapper Procedures
 * Implementation for wavplay client
 * can use MACRO definitions for most Procedures
 * -- Note that this requires the wavplay server (available on Linux) installed
 * -- for working audio support at run time.
 */


#define APCL_ErrFunc ErrFunc

/* static dummy implementations for (newer) features that are NOT supported by wavplay */
static  int apcl_have_memory_playback(void) { return (0); }
static  int apcl_bits(int flags, PCL_ErrFunc erf,int bits) { return (1); }
static  int apcl_memory_buffer(char *buffer, long buffer_len, PCL_ErrFunc erf)  { return (1); }
static  int apcl_channels(int flags, PCL_ErrFunc erf, int channels) { return (1); }

/* apcl_volume: volume must be a value between 0.0 and 1.0 */
extern int   apcl_volume(double volume, int flags,APCL_ErrFunc erf);

/* Define MACROs to map the API functions ( apcl_* ) to wavplay client library functions (tosvr_*) */
#define apcl_bye(flags,erf) tosvr_cmd(ToSvr_Bye,flags,erf)	/* Tell server to exit */
#define apcl_play(flags,erf) tosvr_cmd(ToSvr_Play,flags,erf)	/* Tell server to play */
#define apcl_pause(flags,erf) tosvr_cmd(ToSvr_Pause,flags,erf) /* Tell server to pause */
#define apcl_stop(flags,erf) tosvr_cmd(ToSvr_Stop,flags,erf)	/* Tell server to stop */
#define apcl_restore(flags,erf) tosvr_cmd(ToSvr_Restore,flags,erf) /* Tell server to restore settings */
#define apcl_semreset(flags,erf) tosvr_cmd(ToSvr_SemReset,flags,erf) /* Tell server to reset semaphores */

#define apcl_start(erf) tosvr_start(erf)
#define apcl_path(path,flags,erf) tosvr_path(path,flags,erf)	/* Tell server a pathname */
#define apcl_sampling_rate(rate,flags,erf) tosvr_sampling_rate(flags,erf,rate)	/* Tell server a pathname */
#define apcl_start_sample(offs,flags,erf) tosvr_start_sample(flags,erf,offs)	/* Tell server a pathname */




/* ------- END waveplay client/server based stuff ----------------------------------------------------------- */

#endif  /* GAP_ENABLE_AUDIO_SUPPORT_WAVPLAY */
#endif  /* GAP_ENABLE_AUDIO_SUPPORT_SDL */
#endif  /* _GAP_APCL_LIB_H */
