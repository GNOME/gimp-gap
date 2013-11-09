/* gap_audio_util.c
 *
 *  GAP common encoder audio utility procedures
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
 
/* 2002.01.05 hof  removed p_get_wavparams
 */

/* SYTEM (UNIX) includes */
#include <stdio.h>
#include <stdlib.h>

/* GIMP includes */
#include "gtk/gtk.h"
#include "libgimp/gimp.h"


/* GAP includes */
#include "gap_audio_util.h"

extern      int gap_debug; /* ==0  ... dont print debug infos */

/* --------------------------------
 * gap_audio_util_stereo_split16to16
 * --------------------------------
 */
void
gap_audio_util_stereo_split16to16(unsigned char *l_left_ptr, unsigned char *l_right_ptr, unsigned char *l_aud_ptr, long l_data_len)
{
  long l_idx;

  /* split stereo 16 bit data from wave data (sequence is  2 bytes left channel, 2 right, 2 left, 2 right ...)
   * into 2 separate datablocks for left and right channel of 16 bit per sample
   */
  l_idx = 0;
  while(l_idx < l_data_len)
  {
    *(l_left_ptr++)  = l_aud_ptr[l_idx++];
    *(l_left_ptr++)  = l_aud_ptr[l_idx++];
    *(l_right_ptr++) = l_aud_ptr[l_idx++];
    *(l_right_ptr++) = l_aud_ptr[l_idx++];
  }
}  /* end gap_audio_util_stereo_split16to16 */

/* --------------------------------
 * gap_audio_util_dbl_sample_8_to_16
 * --------------------------------
 */
void
gap_audio_util_dbl_sample_8_to_16(unsigned char insample8,
                     unsigned char *lsb_out,
                     unsigned char *msb_out)
{
  /* 16 bit audiodata is signed,
   *  8 bit audiodata is unsigned
   */

   /* this conversion uses only positive 16bit values
    */
   *msb_out = (insample8 >> 1);
   *lsb_out = ((insample8 << 7) &  0x80);
   return;


   /* this conversion makes use of negative 16bit values
    * and the full sample range
    * (somehow it sounds not as good as the other one)
    */
   *msb_out = (insample8 -64);
   *lsb_out = 0;
}  /* end gap_audio_util_dbl_sample_8_to_16 */

/* --------------------------------
 * gap_audio_util_stereo_split8to16
 * --------------------------------
 */
void
gap_audio_util_stereo_split8to16(unsigned char *l_left_ptr, unsigned char *l_right_ptr, unsigned char *l_aud_ptr, long l_data_len)
{
  long l_idx;
  unsigned char l_lsb, l_msb;

  /* split stereo 8 bit data from wave data (sequence is  2 bytes left channel, 2 right, 2 left, 2 right ...)
   * into 2 separate datablocks for left and right channel of 16 bit per sample
   */
  l_idx = 0;
  while(l_idx < l_data_len)
  {
    gap_audio_util_dbl_sample_8_to_16(l_aud_ptr[l_idx], &l_lsb, &l_msb);
    l_idx++;
    *(l_left_ptr++)  = l_lsb;
    *(l_left_ptr++)  = l_msb;

    gap_audio_util_dbl_sample_8_to_16(l_aud_ptr[l_idx], &l_lsb, &l_msb);
    l_idx++;
    *(l_left_ptr++)  = l_lsb;
    *(l_left_ptr++)  = l_msb;
  }
}  /* end gap_audio_util_stereo_split8to16 */





