/* gap_sdl_audioplayer.h
 *  provides audio wavefile playback via SDL medialibrary.
 *  (SDL is available for Linux, Windows and MacOS)
 *
 *  This player uses an interface that is compatible with
 *  the wavplay client wrapper, but adds support for playback of
 *  audio pcm data from a preloaded memory buffer.
 *
 * Note that the (older) wavplay server/client based audioplayback
 * may be configured at compiletime as alternative on Linux only.
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

#ifndef _GAP_SDL_AUDIOPLAYER_H
#define _GAP_SDL_AUDIOPLAYER_H

#include <stdarg.h>
#include <sys/types.h>
#include <glib.h>
#include <glib/gstdio.h>
#include <SDL.h>
#include <SDL_audio.h>




typedef void (*GapSdlErrFunc)(const char *format,va_list ap);

#define GAP_SDL_CMD_Bye      0
#define GAP_SDL_CMD_Play     1
#define GAP_SDL_CMD_Pause    2
#define GAP_SDL_CMD_Stop     3
#define GAP_SDL_CMD_Restore  4
#define GAP_SDL_CMD_SemReset 5


/*
 * start the audio_playback thread.
 */
int  gap_sdl_start(GapSdlErrFunc erf);

/*
 * Tell audio_player a buffer with PCM audiodata
 * Fetch of audio data switches to the specified memory buffer
 * where following default playback settings are automatically established:
 *  samplerate: 44100 Hz.
 *  bits: 16 (SND_PCM_FORMAT_S16_LE)
 *  channels: 2
 *
 * IMPORTANT: the buffer MUST be aligned at short boundaries
 * (e.g do not allocate the memory as char that would lead to a crash)
 */
int  apcl_memory_buffer(char *buffer, long buffer_len, GapSdlErrFunc erf);

/*
 * Send simple command to audio_player:
 */
int  gap_sdl_cmd(int cmd, int flags, GapSdlErrFunc erf);

/*
 * Send a pathname to the audio_player:
 * The patrhname must refere to a RIFF WAVE fmt file.
 *
 * Note: to trigger audio playback the
 * audio_playback thread must be started { call gap_sdl_start(NULL) }
 * and playback must be turned on (call gap_sdl_cmd(GAP_SDL_CMD_Play,0,NULL)
 *
 * flags: 0 set audio hardware parameters (samplerate, channels,...) from the wavefile specified by path.
 * flags: 1 keep current audio hardware parameters.
 *          (you may use this in case your program has already explicite set
 *           the wanted samplerate, channels, ... )
 *
 * returns 0 OK, 1 if pathname is not readable as valid RIFF WAVE file.
 */
int  gap_sdl_path(const char *pathname, int flags, GapSdlErrFunc erf);

/*
 * Tell audio_player about new data bits per sample and sample format setting:
 * flags: 0  signed sample   little endian (AUDIO_S8, AUDIO_S16LSB)
 * flags: 1  unsigned sample little endian (AUDIO_U8, AUDIO_U16LSB)
 * flags: 2  signed sample   big endian    (AUDIO_S16MSB)
 * flags: 3  unsigned sample big endian    (AUDIO_U16MSB)
 */
int  gap_sdl_bits(int flags,GapSdlErrFunc erf,int bits);

/*
 * Tell audio_player to start at a new sample number:
 */
int  gap_sdl_start_sample(int flags, GapSdlErrFunc erf, Uint32 sample);

/*
 * Tell audio_player to use new sampling rate:
 */
int  gap_sdl_sampling_rate(int flags, GapSdlErrFunc erf, Uint32 sampling_rate);

/*
 * Tell audio_player to use new number of channels (1 for Mono, 2 for Stereo)
 */
int  gap_sdl_channels(int flags,GapSdlErrFunc erf, int channels);

/*
 * Tell audio_player to use mix volume (0.0 upto 1.0)
 *
 *  flags: 0  use mix volume. Note that this does not set the hardware audio volume,
 *            but downscales all processed samples.
 *            (Where values smaller than 1 may reduce the sound quality
 *             due to data loss while mix down)
 *  flags: 1  use master volume of the audio device (TODO: NOT YET supported)
 */
int  gap_sdl_volume(double volume, int flags, GapSdlErrFunc erf);

#endif
