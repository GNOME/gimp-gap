/* gap_audio_wav.c
 *
 *  GAP tool procedures
 *  for RIFF WAVE format audiofile handling.
 *
 */

/*
 * 2004.04.10  hof  - created
 *
 */
#include <config.h>
#include "sys/types.h"
#include "sys/stat.h"
#include "unistd.h"
#include <errno.h>
#include "stdlib.h"
#include <time.h>

#include <glib/gstdio.h>

#include "gap_libgapbase.h"
#include "libgimp/gimp.h"
#include "gap-intl.h"
#include "gap_audio_wav.h"

extern int gap_debug;


#define MAX_AUDIO_STREAMS  16

typedef enum t_Playlist_Check_Retcodes
{
  IS_VALID_PLAYLIST
 ,IS_ANY_PLAYLIST
 ,IS_NO_PLAYLIST
} t_Playlist_Check_Retcodes;


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

typedef union t_wav_32bit_int
{
  gint32  value;
  guint32 uvalue;
  struct
  {
    guchar b0;  /* lsb */
    guchar b1;
    guchar b2;
    guchar b3;  /* msb */
  } bytes;
} t_wav_32bit_int;

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

typedef union t_wav_32bit_int
{
  gint32 value;
  struct
  {
    guchar b3; /* msb */
    guchar b2;
    guchar b1;
    guchar b0; /* lsb */
  } bytes;
} t_wav_32bit_int;

#endif


void
p_check_errno()
{
  gint l_errno;

  l_errno = errno;
  if(l_errno != 0)
  {
    g_message(_("Problem while writing audiofile: %s")
               ,g_strerror (l_errno) );
    printf("gap_audio_wav errno:%d %s\n", l_errno, g_strerror (l_errno));
    exit (-1);
  }
}


void
gap_audio_wav_write_gint16(FILE *fp, gint16 val)
{
   t_wav_16bit_int l_val;

   l_val.value = val;
   errno = 0;

   /* write 16bit sample to file (filedata has always lsb first) */
   fputc((int)l_val.bytes.lsb, fp);
   fputc((int)l_val.bytes.msb, fp);
   p_check_errno();
}

void
gap_audio_wav_write_gint32(FILE *fp, gint32 val)
{
  t_wav_32bit_int l_val;

  l_val.value = val;
  errno = 0;

  /* write 32bit integer to file (lsb first) */
  fputc((int)l_val.bytes.b0, fp);
  fputc((int)l_val.bytes.b1, fp);
  fputc((int)l_val.bytes.b2, fp);
  fputc((int)l_val.bytes.b3, fp);
  p_check_errno();
}


/* ---------------------------------
 * gap_audio_wav_write_header
 * ---------------------------------
 */
void
gap_audio_wav_write_header(FILE *fp
                         , gint32 nsamples
                         , gint32 channels
                         , gint32 samplerate
                         , gint32 bytes_per_sample
                         , gint32 bits
                         )
{
  gint32 l_total_len;  /* is the total filesize - 8 bytes (for RIFF keyword and 4 bytes lengthfield) */
  gint32 l_data_len;
  gint32 l_fmt_len;
  gint16 l_format_tag;
  gint32 l_avg_bytes_per_sec;

  errno = 0;

  if(gap_debug)
  {
    printf("gap_audio_wav_write_header: START fp:%ld\n", (long)fp);
  }

  l_data_len  = nsamples * bytes_per_sample;
  l_total_len = l_data_len + 36;
  l_fmt_len = 16;      /* length of 'fmt ' chunk has fix length of 16 byte */

  l_format_tag = 1;     /* 1 is tag for PCM_WAVE_FORMAT   */
  l_avg_bytes_per_sec = samplerate * bytes_per_sample;

  /* 00 - 03 */  fprintf(fp, "RIFF");
  /* 04 - 07 */  gap_audio_wav_write_gint32(fp, l_total_len);
  /* 08 - 15 */  fprintf(fp, "WAVEfmt ");

  /* 16 - 19 */  gap_audio_wav_write_gint32(fp, (gint32)l_fmt_len);
  /* 20 - 21 */  gap_audio_wav_write_gint16(fp, (gint16)l_format_tag);
  /* 22 - 23 */  gap_audio_wav_write_gint16(fp, (gint16)channels);
  /* 24 - 27 */  gap_audio_wav_write_gint32(fp, samplerate);
  /* 28 - 31 */  gap_audio_wav_write_gint32(fp, l_avg_bytes_per_sec);
  /* 32 - 33 */  gap_audio_wav_write_gint16(fp, (gint16)bytes_per_sample);
  /* 34 - 35 */  gap_audio_wav_write_gint16(fp, (gint16)bits);

  /* 36 - 39 */  fprintf(fp, "data");
  /* 40 - 43 */  gap_audio_wav_write_gint32(fp, l_data_len);

  p_check_errno();
  if(gap_debug)
  {
    printf("gap_audio_wav_write_header: DONE fp:%ld\n", (long)fp);
  }
}  /* end gap_audio_wav_write_header */



/* ----------------------------
 * p_wav_open_seek_data_private
 * ----------------------------
 * Open a RIFF WAVE fmt file
 * check for RIFF WAVE header
 * and seek position to the 1.st data byte.
 *
 * return NULL   on ERROR
 *        FILE*  filepointer to the opened audiofile
 */
static FILE *
p_wav_open_seek_data_private(const char *filename, unsigned char *audata)
{
  FILE        *fp;
  GStatBuf     l_stat_buf;
  size_t       l_len_to_read;
  size_t       l_len_read;

  /* get File Length */
  if (0 != g_stat(filename, &l_stat_buf))
  {
    if(gap_debug)
    {
      printf("p_wav_open_seek_data_private file:%s:\n file does not exist or size 0\n"
            , filename
            );
    }
    /* stat error (file does not exist) */
    return(NULL);
  }

  if (l_stat_buf.st_size < 48)
  {
    if(gap_debug)
    {
      printf("p_wav_open_seek_data_private file: %s\n file size < 48 byte\n"
            , filename
            );
    }
    return(NULL);  /* too short for WAVE file */
  }

  fp = g_fopen(filename, "rb");
  if(fp == NULL)
  {
    if(gap_debug)
    {
      printf("p_wav_open_seek_data_private file: %s\n open read binary failed\n"
            , filename
            );
    }
    return(NULL);
  }

  l_len_to_read = 44;
  l_len_read = fread(&audata[0], 1, l_len_to_read, fp);

  if((audata[0]  == 'R')      /* check for RIFF identifier */
  && (audata[1]  == 'I')
  && (audata[2]  == 'F')
  && (audata[3]  == 'F')
  && (audata[8]  == 'W')      /* check for "WAVEfmt " chunk identifier */
  && (audata[9]  == 'A')
  && (audata[10] == 'V')
  && (audata[11] == 'E')
  && (audata[12] == 'f')
  && (audata[13] == 'm')
  && (audata[14] == 't')
  && (audata[15] == ' '))
  {
    if((audata[36] == 'd')
    && (audata[37] == 'a')
    && (audata[38] == 't')
    && (audata[39] == 'a'))
    {
      /* OK, standard header is immedate followed by data chunk
       * no further searches needed anymore
       */
      return(fp);
    }
  }
  else
  {
    if(gap_debug)
    {
      printf("check failed on file: %s\n The RIFFWAVEfmt  header string was not found\n"
            , filename
            );
    }
    fclose (fp);
    return(NULL);
  }


  /* searching for data chunk */
  while (l_len_to_read == l_len_read)
  {
    if(gap_debug)
    {
      printf("chunkID: %c%c%c%c\n", audata[36], audata[37], audata[38], audata[39]);
      printf("chunkID(hex): %x %x %x %x\n", audata[36], audata[37], audata[38], audata[39]);
    }
    if((audata[36] == 'd')
    && (audata[37] == 'a')
    && (audata[38] == 't')
    && (audata[39] == 'a'))
    {
      return(fp);  /* data chunk found, stop searching */
    }
    /* skip unknown chunk(s) */
    l_len_to_read = audata[40] + (256 * (long)audata[41]) + (65536 * (long)audata[42]) + (16777216 * (long)audata[43]);
    if(gap_debug) printf("skip length: %d\n", (int)l_len_to_read);
    if(l_len_to_read <= 0 || l_len_to_read > (size_t)l_stat_buf.st_size)
    {
      if(gap_debug)
      {
        printf("p_wav_open_seek_data_private file: %s\n l_len_to_read: %d not plausible\n"
              , filename
              , (int)l_len_to_read
              );
      }
      break;  /* chunk length contains nonsens, give up searching for data chunk */
    }
    fseek(fp, l_len_to_read, SEEK_CUR);

    /* read next chunk header and length (4+4 bytes) */
    l_len_to_read = 8;
    l_len_read = fread(&audata[36], 1, l_len_to_read, fp);
  }

  fclose(fp);
  return(NULL);
}   /* end p_wav_open_seek_data_private */

/* ----------------------------
 * gap_audio_wav_open_seek_data
 * ----------------------------
 */
FILE *
gap_audio_wav_open_seek_data(const char *filename)
{
  FILE *fp;
  unsigned char audata[60];

  fp = p_wav_open_seek_data_private(filename, &audata[0]);
  return(fp);
}  /* end gap_audio_wav_open_seek_data */


/* -----------------------
 * gap_audio_wav_file_check
 * -----------------------
 * check file if it is a RIFF WAVE fmt file
 * and obtain the header informatains
 * return -1 on ERROR
 *         0 if OK
 */
int
gap_audio_wav_file_check(const char *audfile, long *sample_rate, long *channels
                     , long *bytes_per_sample, long *bits, long *samples)
{
  /* note: this code is a quick hack and was inspired by a look at some
   *       .wav files. It will work on many .wav files.
   *       but it also may fail to identify some .wav files.
   */
  FILE        *fp;
  unsigned char audata[60];
  long  l_data_len;
  int          l_rc;

  l_rc = -1;  /* prepare retcode NOT OK */

  /* open and check for RIFFWAVEfmt header chunk and for data chunk */
  fp = p_wav_open_seek_data_private(audfile, &audata[0]);
  if(fp == NULL)
  {
    if(gap_debug)
    {
      printf ("p_audio_wav_file_check: open read or HDR and data chunk test failed on '%s'\n"
             , audfile
             );
    }
    return(l_rc);
  }
  fclose(fp);

  /* get informations from the header chunk
   * (fetch was done in p_wav_open_seek_data_private)
   */
  *sample_rate = audata[24] + (256 * (long)audata[25])+ (65536 * (long)audata[26]) + (16777216 * (long)audata[27]);
  *channels = audata[22] + (256 * (long)audata[23]);
  *bytes_per_sample = audata[32] + (256 * (long)audata[33]);
  *bits = audata[34] + (256 * (long)audata[35]);
  l_data_len = audata[40] + (256 * (long)audata[41]) + (65536 * (long)audata[42]) + (16777216 * (long)audata[43]);

  *samples = l_data_len / (*bits / 8);
  l_rc = 0; /* OK seems to be a WAVE file */

  if(gap_debug)
  {
    printf ("p_audio_wav_file_check: WAVE FILE: %s\n", audfile);
    printf (" sample_rate: %d\n", (int)*sample_rate );
    printf (" channels: %d\n", (int)*channels);
    printf (" bytes_per_sample: %d\n", (int)*bytes_per_sample );
    printf (" bits: %d\n", (int) *bits);
    printf (" summary samples (all channels): %d\n", (int)*samples);
    printf (" l_data_len: %d\n", (int)l_data_len);
  }

  return l_rc;
}   /* end gap_audio_wav_file_check */



/* -----------------------
 * gap_audio_wav_16bit_save
 * -----------------------
 * check file if it is a RIFF WAVE fmt file
 * and obtain the header informatains
 * return -1 on ERROR
 *         0 if OK
 */
int
gap_audio_wav_16bit_save(const char *wavfile
                    , int channels
                    , unsigned short *left_ptr
                    , unsigned short *right_ptr
                    , int samplerate
                    , long total_samples
                    )
{
  FILE *fp;
  gint32 l_bytes_per_sample;
  gint32 l_ii;

  if(channels == 1) { l_bytes_per_sample = 2;}  /* mono */
  else              { l_bytes_per_sample = 4; channels = 2; }  /* stereo */


  fp = g_fopen(wavfile, "wb");
  if (fp)
  {
    /* write the header */
    gap_audio_wav_write_header(fp
                            , (gint32)total_samples
                            , channels                           /* cannels 1 or 2 */
                            , samplerate
                            , l_bytes_per_sample
                            , 16                          /* 16 bit sample resolution */
                            );

    /* write 16 bit wave datasamples
     * sequence mono:    (lo, hi)
     * sequence stereo:  (lo_left, hi_left, lo_right, hi_right)
     */
    for(l_ii=0; l_ii < total_samples; l_ii++)
    {
       gap_audio_wav_write_gint16(fp, *left_ptr);
       left_ptr++;
       if(channels > 1)
       {
         gap_audio_wav_write_gint16(fp, *right_ptr);
         right_ptr++;
       }
     }

    fclose(fp);
  }
  return(0); /* OK */
}  /* end gap_audio_wav_16bit_save */



/* --------------------------------
 * p_check_for_valid_playlist
 * --------------------------------
 * check if the file specified via audfile
 * is a valid audio playlist. Valis playlists are textfiles
 * containing filenames of RIFF WAVE formated audiofiles
 * of desired sample rate and 16 bits per channel.
 * playlist syntax is:
 *  - one filename per line, without leading blanks
 *  - empty lines are allowd
 *  - lines starting with '#' are commentlines that are ignored
 */
static t_Playlist_Check_Retcodes
p_check_for_valid_playlist(const char *audfile, long *sample_rate, long *channels
                     , long *bytes_per_sample, long *bits, long *samples
                     , long *all_playlist_references
                     , long *valid_playlist_references
                     , long desired_samplerate
                     )
{
  t_Playlist_Check_Retcodes l_retval;
  long  l_bytes_per_sample = 4;
  long  l_sample_rate = 22050;
  long  l_channels = 2;
  long  l_bits = 16;
  long  l_samples = 0;
  FILE *l_fp;
  char  l_buf[4000];
  char  *referred_wavfile;
  gint  ii;

  l_retval = IS_NO_PLAYLIST;
  referred_wavfile = NULL;
  l_channels = 0;
  *all_playlist_references = 0;

  /* check if audfile
   * is a playlist referring to more than one inputfile
   * and try to open those input wavefiles
   * (but limited upto MAX_AUDIO_STREAMS input wavefiles)
   */

  if(gap_debug)
  {
    printf("p_check_for_valid_playlist: START checking file: %s\n", audfile);
  }

  ii = 0;
  l_fp = g_fopen(audfile, "r");
  if(l_fp)
  {
    while(NULL != fgets(l_buf, sizeof(l_buf)-1, l_fp))
    {
      if((l_buf[0] == '#') || (l_buf[0] == '\n') || (l_buf[0] == '\0'))
      {
        continue;  /* skip comment lines, and empty lines */
      }

      l_buf[sizeof(l_buf) -1] = '\0';  /* make sure we have a terminated string */
      gap_file_chop_trailingspace_and_nl(&l_buf[0]);

      if(ii < MAX_AUDIO_STREAMS)
      {
        int    l_rc;


        if(referred_wavfile)
        {
          g_free(referred_wavfile);
        }
        referred_wavfile = gap_file_make_abspath_filename(&l_buf[0], audfile);

        if(gap_debug)
        {
          printf("p_check_for_valid_playlist: checking reference file: %s\n", referred_wavfile);
        }

        l_rc = gap_audio_wav_file_check(referred_wavfile
                              , &l_sample_rate
                              , &l_channels
                              , &l_bytes_per_sample
                              , &l_bits
                              , &l_samples
                              );
        if(gap_debug)
        {
          printf("REF-file: %s bits:%d (expected:%d) samplerate:%d (expected: %d) l_rc: %d\n"
                ,referred_wavfile
                ,(int)l_bits
                ,(int)16
                ,(int)l_sample_rate
                ,(int)desired_samplerate
                ,(int)l_rc
                );
        }
        if(l_rc == 0)
        {
          (*all_playlist_references)++;

          /* use audio informations from the 1.st valid referenced file for output
           * (or pick information of any other referenced audiofile
           * if we have no matching audio file reference)
           */
          if(ii == 0)
          {
            *sample_rate = l_sample_rate;
            *channels = l_channels;
            *bytes_per_sample = l_bytes_per_sample;
            *bits = l_bits;
            *samples = l_samples;
          }

          if((l_bits == 16)
          && (l_sample_rate == desired_samplerate))
          {
            /* we have found at least one reference to an audiofile
             * with matching bits and samplerate
             * therefore we consider audfile as valid playlist file
             */
            ii++;
            l_retval = IS_VALID_PLAYLIST;
          }
          else
          {
            /* the file seems to be a play list
             * but the referenced file does not match desired parameters
             */
            if(l_retval != IS_VALID_PLAYLIST)
            {
              l_retval = IS_ANY_PLAYLIST;
            }
            break;
          }
        }
        else
        {
          /* report unexpected content as warning, but only when
           * we have already identified that file as playlist
           * (otherwise checks on audiofiles would also
           * report such a warning that would confusing)
           */
          if(ii > 0)
          {
            g_message(_("The file: %s\n"
                    "has unexpect content that will be ignored.\n"
                    "You should specify an audio file in RIFF WAVE fileformat,\n"
                    "or a textfile containing filenames of such audio files")
                   , audfile
                   );
          }
          break;
        }
      }
      else
      {
        g_message(_("The file: %s\n"
                    "contains too many audio-input tracks\n"
                    "(only %d tracks are used, the rest are ignored).")
                 , audfile
                 , (int) MAX_AUDIO_STREAMS
                 );
        break;
      }
      l_buf[0] = '\0';
    }
    fclose(l_fp);
  }


  if(l_retval == IS_ANY_PLAYLIST)
  {
    g_message(_("The file: %s\n"
                "is an audio playlist, but contains references to audiofiles that\n"
                "do not match the desired sample rate of %d Hz\n"
                "or do not have 16 bits per sample")
               , audfile
               , (int)desired_samplerate
               );
  }

  if(referred_wavfile)
  {
    g_free(referred_wavfile);
  }

  *valid_playlist_references = ii;
  return (l_retval);
}  /* end p_check_for_valid_playlist */




/* ---------------------------------
 * gap_audio_playlist_wav_file_check
 * ---------------------------------
 * check file if it is a RIFF WAVE fmt file
 * and obtain the header informatains
 * return -1 on ERROR  (file is neither valid wavfile nor valid audio playlist)
 * return -2 ERROR, File is an audio playlist, but not usable (non matching bits or samplerate)
 *         0 if OK  (valid WAV file or valid  playlist)
 */
int
gap_audio_playlist_wav_file_check(const char *audfile, long *sample_rate, long *channels
                     , long *bytes_per_sample, long *bits, long *samples
                     , long *all_playlist_references
                     , long *valid_playlist_references
                     , long desired_samplerate
                     )
{
  int    l_rc;

  /* check for WAV file, and get audio informations */
  l_rc = gap_audio_wav_file_check(audfile
                     , sample_rate
                     , channels
                     , bytes_per_sample
                     , bits
                     , samples
                     );

  if (l_rc ==0)
  {
    return 0; /* OK, audfile is a RIFF wavefile */
  }
  else
  {
    t_Playlist_Check_Retcodes l_rc_playlistcheck;

    l_rc_playlistcheck = p_check_for_valid_playlist(audfile
                                                  , sample_rate
                                                  , channels
                                                  , bytes_per_sample
                                                  , bits
                                                  , samples
                                                  , all_playlist_references
                                                  , valid_playlist_references
                                                  , desired_samplerate
                                                  );
    switch(l_rc_playlistcheck)
    {
      case  IS_VALID_PLAYLIST:
        if(gap_debug)
        {
          printf(": The file %s is a valid playlist\n", audfile);
        }
        return 0;
        break;
      case  IS_ANY_PLAYLIST:
        if(gap_debug)
        {
          printf(": The file %s is a  non matching playlist\n", audfile);
        }
        return -2;
        break;
      default:
        if(gap_debug)
        {
          printf(": The file %s is not a playlist\n", audfile);
        }
        break;
    }
  }

  return -1;
}  /* end gap_audio_playlist_wav_file_check */
