/* gap_vid_api_mpeg3.c
 *
 *
 * GAP Video read API implementation of libmpeg3
 * based wrappers to read various MPEG encoded videofile formats
 *
 * 2004.04.12   hof added libmpeg3 specific TOC file support
 * 2004.04.03   hof upgrade to libmpeg3-1.5.4
 *                 - mpeg3_seek_percentage no longer supported
 *                 - MMX support sometimes delivers stripes of noise when frames are read
 *                   so we dont use MMX at all any more.
 * 2003.05.09   hof created
 *
 *
 */

/* ================================================  MPEG3
 * libmpeg3                                          MPEG3
 * ================================================  MPEG3
 * ================================================  MPEG3
 */

#ifdef ENABLE_GVA_LIBMPEG3
#include "libmpeg3.h"

#define GVA_LIBMPEG3_DECODER_NAME "libmpeg3"

typedef struct t_GVA_mpeg3
{
 mpeg3_t *main_handle;
 mpeg3_t *raw_handle;

 gint32  raw_pos;
 unsigned long prev_code;  /* previous code for raw chunk reads */

} t_GVA_mpeg3;


static int p_mpeg3_gopseek(mpeg3_t*  handle, gint32 seekto_frame, t_GVA_Handle *gvahand);
static int p_mpeg3_emulate_seek(mpeg3_t*  handle, gint32 seekto_frame, t_GVA_Handle *gvahand);
static gboolean p_check_libmpeg3_toc_file(const char *filename);

#ifndef G_OS_WIN32
static gboolean  p_mpeg3_sig_workaround_fork(const char *filename);
#endif


#define GVA_PROCESS_EXIT_OK       0
#define GVA_PROCESS_EXIT_FAILURE  1


#ifndef G_OS_WIN32
/* -----------------------------
 * p_mpeg3_sig_handler
 * -----------------------------
 */
static void
p_mpeg3_sig_handler(int sig_num)
{
  printf("GVA_MP3: CILD SIG HANDLER workaround libmpeg3 close crash captured\n");
  exit (GVA_PROCESS_EXIT_FAILURE);
}  /* end p_mpeg3_sig_handler */

 
/* -----------------------------
 * p_mpeg3_sig_workaround_fork
 * -----------------------------
 * call libmpeg3 open/close sequence as own
 * process, and deliver FALSE when the process has crashed
 */
static gboolean
p_mpeg3_sig_workaround_fork(const char *filename)
{
  pid_t l_pid;
  int   l_status;

  l_pid = fork();
  if(l_pid == 0)
  {
     mpeg3_t *tmp_handle;
     gint     l_rc;
     char    *l_filename;
     int      l_error_return;

     /* here we are in the forked child process
      * where we install our own signal handlers
      */
     l_rc = GVA_PROCESS_EXIT_FAILURE;
     l_filename = g_strdup(filename);
     
     signal(SIGBUS,  p_mpeg3_sig_handler);
     signal(SIGSEGV, p_mpeg3_sig_handler);
     signal(SIGFPE,  p_mpeg3_sig_handler);

     if(gap_debug) printf("GVA_MP3: CILD process before mpeg3_open\n");
     tmp_handle = mpeg3_open(l_filename, &l_error_return);
     if(gap_debug) printf("GVA_MP3: CILD process after mpeg3_open\n");
     if(tmp_handle)
     {
       if(gap_debug) printf("GVA_MP3: CILD process before mpeg3_close\n");
       mpeg3_close(tmp_handle);  /* this call causes the crash (sometimes) */ 
       if(gap_debug) printf("GVA_MP3: CILD process after mpeg3_close\n");
       
       if (l_error_return == 0)
       {
          l_rc = GVA_PROCESS_EXIT_OK;  /* retcode for OK */
       }
      }
     g_free(l_filename);
     exit (l_rc);
  }

  if(l_pid < 0)
  {
    if(gap_debug) printf("GVA_MP3: PARENT process fork failed\n");
    /* negative pid indicates error to fork a child process */
    return (FALSE);
  }

  if(gap_debug) printf("GVA_MP3: PARENT before waitpid(%d)\n", (int)l_pid);
  waitpid(l_pid, &l_status, 0);
  if(gap_debug) printf("GVA_MP3: PARENT after waitpid(%d) status:%d\n", (int)l_pid, (int)l_status);
   
  if(l_status == 0)
  {
    if(gap_debug) printf("GVA_MP3: PARENT child normal end(%d)\n", (int)l_pid);
    return(TRUE);
  }

  if(gap_debug) printf("GVA_MP3: PARENT child crashed (%d)\n", (int)l_pid);
  return(FALSE);
   
}  /* end p_mpeg3_sig_workaround_fork */
#endif





/* -----------------------------
 * p_wrapper_mpeg3_check_sig
 * -----------------------------
 */
gboolean
p_wrapper_mpeg3_check_sig(char *filename)
{
  int major;
  int minor;
  int release;
  gboolean version_ok;
  gboolean sig_ok;
  
  major = mpeg3_major();
  minor = mpeg3_minor();
  release = mpeg3_release();


  version_ok  = TRUE;
  if(major < 1) 
  { 
    version_ok = FALSE;
  }
  else
  {
    if(major == 1)
    {
      if(minor < 5)
      {
        version_ok = FALSE;
      }
      else
      {
        if((minor == 5) && (release < 4))
        {
          version_ok = FALSE;
        }
      }
    }
  }
  
  if(version_ok)
  {
    if(gap_debug)
    {
      printf("GVA info: the GAP Video API was linked with "
           " libmpeg3 %d.%d.%d (reqired version is 1.5.4 or better)\n"
           ,major
           ,minor
           ,release
           );
    }
  }
  else
  {
    printf("## GVA WARNING: the GAP Video API was linked with "
           " an old version of libmpeg3 %d.%d.%d (reqired version is 1.5.4 or better)\n"
           " (the libmpeg3 based decoder is not available)"
           ,major
           ,minor
           ,release
          );
    return (FALSE);
  }
  
  /* mpeg3_check_sig returns 1 if the file is compatible (MPEG1 or 2) */
  sig_ok = mpeg3_check_sig(filename);

#ifndef G_OS_WIN32
  if(sig_ok)
  {
    gchar   *libmpeg3_workaround;
    gboolean libmpeg3_workaround_enabled;

    libmpeg3_workaround_enabled = TRUE;  /* enable per default if nothing configured in gimprc */
    libmpeg3_workaround = gimp_gimprc_query("video-workaround-for-libmpeg3-close-bug");
    if(libmpeg3_workaround)
    {
      libmpeg3_workaround_enabled = FALSE;
      if ((*libmpeg3_workaround == 'y')
      ||  (*libmpeg3_workaround == 'Y'))
      {
        libmpeg3_workaround_enabled = TRUE;
      }
      g_free(libmpeg3_workaround); 
    }
    
    if(libmpeg3_workaround_enabled)
    {
      /* workaround: (available for UNIX systems only)
       * libmpeg3 does sometimes crash on close.
       * (some mpeg1 files encoded by ffmpeg)
       * this code tries open close sequence in a separated process
       *
       * to catch those crashes, and deliver sig_ok FALSE
       * in case of crash)
       */
      sig_ok = p_mpeg3_sig_workaround_fork(filename);
    }
  }
#endif
  return (sig_ok);
}  /* end p_wrapper_mpeg3_check_sig */


/* -----------------------------
 * p_wrapper_mpeg3_open_read
 * -----------------------------
 * - fill t_GVA_Handle with informations (Audio + Video)
 * - libmpeg3 typical tocfiles for fast seek access are supported.
 */
void
p_wrapper_mpeg3_open_read(char *in_filename, t_GVA_Handle *gvahand)
{
  t_GVA_mpeg3*  handle;
  gint          repeat_count;
  char         *filename;
  char         *tocfile;

  if(gap_debug) printf("p_wrapper_mpeg3_open_read: START in_filename: %s\n", in_filename);

  filename = in_filename;

  /* test TOCFILE and if frame exact positioning available */
  {
     if(p_check_libmpeg3_toc_file(in_filename))
     {
       if(gap_debug) printf("OPEN: tocfile %s  OK (fast seek via mpeg3_set_frame is supported)\n", in_filename);
       gvahand->emulate_seek = FALSE;
     }
     else
     {
       /* check if there is a matching TOCFILE */
       tocfile =  GVA_build_video_toc_filename(in_filename, GVA_LIBMPEG3_DECODER_NAME);
       if(p_check_libmpeg3_toc_file(tocfile))
       {
         if(gap_debug) printf("OPEN: additional tocfile: %s  AVAILABLE (fast seek via mpeg3_set_frame is supported)\n", tocfile);
         gvahand->emulate_seek = FALSE;
         filename = tocfile;
       }
       else
       {
         if(gap_debug) printf("OPEN: %s is no tocfile (mpeg3_set_frame seek to frame NOT supported)\n", in_filename);
         gvahand->emulate_seek = TRUE;
       }
     }
  }


  handle = g_malloc0(sizeof(t_GVA_mpeg3));
  handle->main_handle = NULL;
  handle->raw_handle = NULL;     /* for reading compressed video chunks only */
  handle->raw_pos = 0;           /* start position is before frame 1 */

  /* workaround:
   *  sometimes open succeeds, but libmpeg3 does not recognize any video or audio channel
   *  dont know whats wrong here.
   *  as workaround this API tries repeart the open for 3 times (only if no tracks can be found)
   * hof: this happened in older libmpeg3 versions
   *      
   */
  for(repeat_count = 0; repeat_count < 3; repeat_count++)
  {
    int      l_error_return;
 
    handle->main_handle = mpeg3_open(filename, &l_error_return);

    if(gap_debug)
    {
      printf("GVA: libmpeg3 OPEN handle->main_handle:%ld l_error_return:%d\n"
            , (long)handle->main_handle
            , (int)l_error_return
            );
    }

    if((handle->main_handle) && (l_error_return == 0))
    {
      gvahand->decoder_handle = (void *)handle;
      gvahand->frame_bpp = 3;

      /* never use MMX, it sometimes delivers trashed frames
       * and has no advantages on modern CPU's at all
       * procedure mpeg3_set_mmx was removed since libmpeg3-1.8
       */
      /// mpeg3_set_mmx(handle->main_handle, FALSE);

      if(mpeg3_has_video(handle->main_handle))
      {
        gvahand->vtracks          = mpeg3_total_vstreams(handle->main_handle);
        gvahand->vid_track        = CLAMP(gvahand->vid_track, 0, gvahand->vtracks -1);

        gvahand->total_frames     = mpeg3_video_frames(handle->main_handle, (int)gvahand->vid_track);
        gvahand->framerate        = mpeg3_frame_rate(handle->main_handle, (int)gvahand->vid_track);
        gvahand->width            = mpeg3_video_width(handle->main_handle, (int)gvahand->vid_track);
        gvahand->height           = mpeg3_video_height(handle->main_handle, (int)gvahand->vid_track);
        gvahand->aspect_ratio     = mpeg3_aspect_ratio(handle->main_handle, (int)gvahand->vid_track);

        /* libmpeg3-1.5.4 has a bug and did always deliver aspect ratio value 0.0
         * the aspect was not recognized when it was there 
         * tested with some DVD and MPEG1 videofiles
         */
        /* printf("ASPECT RATIO: %f\n", (float)gvahand->aspect_ratio); */

        if(gvahand->total_frames <= 1)
        {
           /* For DVD files, total_frames is 1 indicating that the number is unknown
            * We try to figure the number of frames using a guess based on filesize
            * as workaround
            */

          gvahand->total_frames = p_guess_total_frames(gvahand);

          if(gap_debug) printf("GUESS TOTAL FRAMES %d\n", (int)gvahand->total_frames );

        }
        

      }
      else
      {
        gvahand->total_frames     = 0;
        gvahand->framerate        = 1.0;
        gvahand->width            = 0;
        gvahand->height           = 0;
      }
      if(mpeg3_has_audio(handle->main_handle))
      {
        gvahand->atracks            = mpeg3_total_astreams(handle->main_handle);
        gvahand->aud_track          = CLAMP(gvahand->aud_track, 0, gvahand->atracks -1);

        gvahand->samplerate         = mpeg3_sample_rate(handle->main_handle, (int)gvahand->aud_track);
        gvahand->audio_cannels      = mpeg3_audio_channels(handle->main_handle, (int)gvahand->aud_track);
        gvahand->total_aud_samples  = mpeg3_audio_samples(handle->main_handle, (int)gvahand->aud_track);
        gvahand->audio_playtime_sec = (gdouble)gvahand->total_aud_samples / (gdouble)gvahand->samplerate;
      }

      if((gvahand->atracks > 0) || (gvahand->vtracks > 0))
      {
        break;                /* OK video and /or audio found */
      }
      else
      {
        if(gap_debug) printf("GVA: mpeg3 Open problems attempt:%d\n", (int)repeat_count);

        mpeg3_close(handle->main_handle);  /* workaround: close and try open another time */

        if(gap_debug) printf("GVA: mpeg3 after close attempt:%d\n", (int)repeat_count);

        handle->main_handle = NULL;
        mpeg3_check_sig(filename);
        mpeg3_check_sig("dummy");
        gvahand->decoder_handle = NULL;
      }
    }

  }    /* end for repeat_count */

  if(gvahand->vindex == NULL)
  {
    gvahand->vindex = GVA_load_videoindex(in_filename, gvahand->vid_track, GVA_LIBMPEG3_DECODER_NAME);
    if(gvahand->vindex)
    {
      if(gvahand->vindex->total_frames > 1)
      {
        /* we have a complete vindex and can
         * trust the total_frames in the vindex file
         */
        gvahand->total_frames = gvahand->vindex->total_frames;
        gvahand->all_frames_counted = TRUE;
      }
    }
  }


  if(gvahand->decoder_handle == NULL)
  {
    if(handle)
    {
       g_free(handle);
    }
  }

}  /* end p_wrapper_mpeg3_open_read */

/* -----------------------------
 * p_wrapper_mpeg3_close
 * -----------------------------
 */
void
p_wrapper_mpeg3_close(t_GVA_Handle *gvahand)
{
  t_GVA_mpeg3 *handle;

  handle = (t_GVA_mpeg3 *)gvahand->decoder_handle;
  
  if(handle)
  {
    if(handle->main_handle)
    {
      mpeg3_close(handle->main_handle);
    }
    if(handle->raw_handle) 
    { 
      mpeg3_close(handle->raw_handle);
    }
  }
}  /* end p_wrapper_mpeg3_close */


/* ----------------------------------
 * p_wrapper_mpeg3_get_next_frame
 * ----------------------------------
 *  TODO:
 *  - Retocde evaluation (what is OK, EOF or ERROR ?)
 */
t_GVA_RetCode
p_wrapper_mpeg3_get_next_frame(t_GVA_Handle *gvahand)
{
  t_GVA_mpeg3 *handle;
  int      l_rc;


  handle = (t_GVA_mpeg3 *)gvahand->decoder_handle;
  if(handle == NULL)
  {
    return(GVA_RET_ERROR);
  }
  if(handle->main_handle == NULL)
  {
    return(GVA_RET_ERROR);
  }

  /* Read a frame.  The dimensions of the input area and output frame must be supplied. */
  /* The frame is taken from the input area and scaled to fit the output frame in 1 step. */
  /* Stream defines the number of the multiplexed stream to read. */
  /* The last row of **output_rows must contain 4 extra bytes for scratch work. */
  l_rc = mpeg3_read_frame(handle->main_handle
                ,gvahand->row_pointers        /* Array of pointers to the start of each output row */
                ,0                            /* in_x  Location in input frame to take picture */
                ,0                            /* in_y */
                ,(int)gvahand->width
                ,(int)gvahand->height
                ,(int)gvahand->width          /* Dimensions of output_rows */
                ,(int)gvahand->height
                ,MPEG3_RGB888                 /* One of the color model #defines MPEG3_RGB888   MPEG3_RGBA8888  */
                ,(int)gvahand->vid_track      /* stream */
                );

  if(l_rc == 0)
  {
    gvahand->current_frame_nr = gvahand->current_seek_nr;
    gvahand->current_seek_nr++;
 
    /* if we found more frames, increase total_frames */
    gvahand->total_frames = MAX(gvahand->total_frames, gvahand->current_frame_nr);
    return(GVA_RET_OK);
  }
  if(l_rc == 1)  { return(GVA_RET_EOF); }

  return(GVA_RET_ERROR);
}  /* end p_wrapper_mpeg3_get_next_frame */


/* ------------------------------
 * p_wrapper_mpeg3_seek_frame
 * ------------------------------
 *  TODO: - Retocde evaluation (what is OK, EOF or ERROR ?)
 */
t_GVA_RetCode
p_wrapper_mpeg3_seek_frame(t_GVA_Handle *gvahand, gdouble pos, t_GVA_PosUnit pos_unit)
{
  t_GVA_mpeg3 *handle;
  int      l_rc;
  long     l_frame_pos;

  handle = (t_GVA_mpeg3 *)gvahand->decoder_handle;
  if(handle == NULL)
  {
    return(GVA_RET_ERROR);
  }
  if(handle->main_handle == NULL)
  {
    return(GVA_RET_ERROR);
  }

  switch(pos_unit)
  {
    case GVA_UPOS_FRAMES:
      l_frame_pos = (long)pos;
      break;
    case GVA_UPOS_SECS:
      l_frame_pos = (long)rint(pos * gvahand->framerate);
      break;
    case GVA_UPOS_PRECENTAGE:
      l_frame_pos = (long)GVA_percent_2_frame(gvahand->total_frames, pos);
      break;
    default:
      l_frame_pos = (long)pos;
      break;
  }

  if(l_frame_pos == gvahand->current_seek_nr)
  {
    /* dont need to seek */
    l_rc = 0;
    if(gap_debug) printf("MPEG3_SEEK_FRAME(F): current_seek_nr:%d new_seek_nr:%d (NO SEEK)\n", (int)gvahand->current_seek_nr ,(int)l_frame_pos);
  }
  else
  {
    if(gap_debug) printf("MPEG3_SEEK_FRAME(F): current_seek_nr:%d new_seek_nr:%d (SEEK NEEDED)\n", (int)gvahand->current_seek_nr ,(int)l_frame_pos);

    if(gvahand->emulate_seek)
    {
      l_rc = p_mpeg3_emulate_seek(handle->main_handle, l_frame_pos -1, gvahand);
    }
    else
    {
      if (1==1)  /* (gvahand->dirty_seek) */
      {
        /* fast seek
         * (older libmpeg3 versions had aProblem: 
         *  a following read may deliver more or less thrashed images
         *  until the next I frame is read in sequence)
         */
        l_rc = mpeg3_set_frame(handle->main_handle, l_frame_pos, (int)gvahand->vid_track);
      }
      else
      {
        /* this dead code once was needed with older libmpeg3 versions
         * libmpeg3-1.5.4 seems to handle seek operations clean
         * without returning trash frames when seek to B or P frames
         */
        l_rc = p_mpeg3_gopseek(handle->main_handle, l_frame_pos -1, gvahand);
      }
    }
  }

  if(l_rc == 0)
  {
    gvahand->current_seek_nr = l_frame_pos;
    return(GVA_RET_OK);
  }
  if(l_rc == 1)  { return(GVA_RET_EOF); }

  return(GVA_RET_ERROR);
}  /* end p_wrapper_mpeg3_seek_frame */



/* ------------------------------
 * p_wrapper_mpeg3_seek_audio
 * ------------------------------
 *  TODO: - Retocde evaluation (what is OK, EOF or ERROR ?)
 */
t_GVA_RetCode
p_wrapper_mpeg3_seek_audio(t_GVA_Handle *gvahand, gdouble pos, t_GVA_PosUnit pos_unit)
{
  t_GVA_mpeg3 *handle;
  int      l_rc;
  long     l_frame_pos;
  long     l_sample_pos;

  handle = (t_GVA_mpeg3 *)gvahand->decoder_handle;
  if(handle == NULL)
  {
    return(GVA_RET_ERROR);
  }
  if(handle->main_handle == NULL)
  {
    return(GVA_RET_ERROR);
  }

  switch(pos_unit)
  {
    case GVA_UPOS_FRAMES:
      l_sample_pos = (long)GVA_frame_2_samples(gvahand->framerate, gvahand->samplerate, pos);
      break;
    case GVA_UPOS_SECS:
      l_sample_pos = (long)rint(pos / MAX(gvahand->samplerate, 1.0));
      break;
    case GVA_UPOS_PRECENTAGE:
      l_frame_pos= (long)GVA_percent_2_frame(gvahand->total_frames, pos);
      l_sample_pos = (long)GVA_frame_2_samples(gvahand->framerate, gvahand->samplerate, l_frame_pos);
      break;
    default:
      l_sample_pos = 0;
      break;
  }

  /* seek to a sample in the stream gvahand->aud_track
   */
  l_rc = mpeg3_set_sample(handle->main_handle, l_sample_pos, (int)gvahand->aud_track);

  if(l_rc == 0)
  {
    gvahand->current_sample = (gint32)l_sample_pos;
    return(GVA_RET_OK);
  }
  if(l_rc == 1)  { return(GVA_RET_EOF); }

  return(GVA_RET_ERROR);
}  /* end p_wrapper_mpeg3_seek_audio */


/* ------------------------------
 * p_wrapper_mpeg3_get_audio
 * ------------------------------
 *  TODO: - Retocde evaluation (what is OK, EOF or ERROR ?)
 */
t_GVA_RetCode
p_wrapper_mpeg3_get_audio(t_GVA_Handle *gvahand
             ,gint16 *output_i
             ,gint32 channel
             ,gdouble samples
             ,t_GVA_AudPos mode_flag
             )
{
  t_GVA_mpeg3 *handle;
  int      l_rc;

  handle = (t_GVA_mpeg3 *)gvahand->decoder_handle;
  if(handle == NULL)
  {
    return(GVA_RET_ERROR);
  }
  if(handle->main_handle == NULL)
  {
    return(GVA_RET_ERROR);
  }

  if(mode_flag == GVA_AMOD_FRAME)
  {
      p_wrapper_mpeg3_seek_audio(gvahand
                                    , (gdouble) gvahand->current_frame_nr
                                    , GVA_UPOS_FRAMES
                                    );
  }

  if(mode_flag == GVA_AMOD_REREAD)
  {
    l_rc = mpeg3_reread_audio(handle->main_handle
                           , NULL                      /* UNUSED  Pointer to pre-allocated buffer of floats */
                           , (short *)output_i         /* Pointer to pre-allocated buffer of int16's */
                           , (int) channel             /* Channel to decode */
                           , (long) samples            /* Number of samples to decode */
                           , (int) gvahand->aud_track  /* Stream containing the channel */
                           );
  }
  else
  {
    l_rc = mpeg3_read_audio(handle->main_handle
                           , NULL                      /* UNUSED  Pointer to pre-allocated buffer of floats */
                           , (short *)output_i         /* Pointer to pre-allocated buffer of int16's */
                           , (int) channel             /* Channel to decode */
                           , (long) samples            /* Number of samples to decode */
                           , (int) gvahand->aud_track  /* Stream containing the channel */
                           );
  }

  if(l_rc == 0)
  {
    if(mode_flag != GVA_AMOD_REREAD)
    {
       gvahand->current_sample += samples; /* keep track of current sample position */
    }
    return(GVA_RET_OK);
  }
  if(l_rc == 1)  { return(GVA_RET_EOF); }

  return(GVA_RET_ERROR);
}  /* end p_wrapper_mpeg3_get_audio */



/* ----------------------------------
 * p_wrapper_mpeg3_count_frames
 * ----------------------------------
 */
t_GVA_RetCode
p_wrapper_mpeg3_count_frames(t_GVA_Handle *gvahand)
{
  gint32         l_total_frames;
  t_GVA_Videoindex    *vindex;

  gvahand->percentage_done = 0.0;
  gvahand->cancel_operation = FALSE;  /* reset cancel flag */

  if(p_check_libmpeg3_toc_file(gvahand->filename))
  {
    if(gap_debug) 
    {
      printf("gap_mpeg3_count_frames NOP: filename: %s\n"
          , gvahand->filename);
      printf("is already a libmpeg3 TOC file\n");
      fflush(stdout);
    }
    return(GVA_RET_OK);
  }
  l_total_frames = 0;

  if(gap_debug) 
  {
    printf("gap_mpeg3_count_frames filename: %s\n", gvahand->filename); 
    fflush(stdout);
  }
  
  if(gvahand->vindex == NULL)
  {
    gvahand->vindex = GVA_load_videoindex(gvahand->filename, gvahand->vid_track, GVA_LIBMPEG3_DECODER_NAME);
  }
  
  if(gvahand->vindex)
  {
     if((gvahand->vindex->total_frames > 0)
     && (gvahand->vindex->track == gvahand->vid_track))
     {
       /* we already have a valid videoindex that contains the real total number */
       gvahand->total_frames = gvahand->vindex->total_frames;
       gvahand->all_frames_counted = TRUE;

       /* for libmpeg3 implementation we need a 2.nd file,
        * the .toc file that does the job for fast seek access by libmeg3 internal methods
        * (the videoindex_file is just used to store the total_frames information,
        *  that is not available in the .toc file)
        */
       gvahand->vindex->tocfile = GVA_build_video_toc_filename(gvahand->filename, GVA_LIBMPEG3_DECODER_NAME);
       
       if(p_check_libmpeg3_toc_file(gvahand->vindex->tocfile))
       {
         /* close video and switch to the TOC file */
         if(gap_debug) 
         {
           printf("gap_mpeg3_count_frames REOPEN using tocfile: %s\n"
                 , gvahand->vindex->tocfile
                 );
           fflush(stdout);
         }
         p_wrapper_mpeg3_close(gvahand);
         p_wrapper_mpeg3_open_read(gvahand->vindex->tocfile, gvahand);
         return(GVA_RET_OK);
       }
     }
     /* throw away existing videoindex (because it is unusable) */
     GVA_free_videoindex(&gvahand->vindex);
  }

  if(gap_debug)
  {
    printf("gap_mpeg3_count_frames filename: CREATE index & TOC-file\n");
    fflush(stdout);
  }

  vindex = NULL;

  /* create_vindex */
  {
    gvahand->vindex = GVA_new_videoindex();
    vindex = gvahand->vindex;
    if(vindex)
    {
      vindex->tabtype = GVA_IDX_TT_GINT64;
      vindex->ofs_tab = g_new(t_GVA_IndexElem, GVA_VIDINDEXTAB_BLOCK_SIZE);
      vindex->track = gvahand->vid_track;
      if(vindex->ofs_tab)
      {
        vindex->tabsize_allocated = GVA_VIDINDEXTAB_BLOCK_SIZE;
      }
    }
  }
  
  /* create the libmpeg3 specific TOC file */
  if(vindex)
  {

    vindex->tocfile = GVA_build_video_toc_filename(gvahand->filename, GVA_LIBMPEG3_DECODER_NAME);

    /* check if writeable by creating an empty toc file first */
    {
      FILE *fp;
      gint l_errno;
      
      fp = g_fopen(vindex->tocfile, "wb");
      if(fp)
      {
        int argc;
        char *argv[3];

        fclose(fp);
        
        argc = 3;
        argv[0] = g_strdup("GVA_mpeg3toc");
        argv[1] = g_strdup(gvahand->filename);
        argv[2] = g_strdup(vindex->tocfile);
        
        GVA_create_libmpeg3_toc(argc, argv, gvahand, &l_total_frames);
        
        g_free(argv[0]);
        g_free(argv[1]);
        g_free(argv[2]);
      }
      else
      {
        l_errno = errno;
        g_message(_("ERROR: Failed to write videoindex tocfile\n"
                  "tocfile: '%s'\n"
                  "%s")
                  , vindex->tocfile
                  , g_strerror (l_errno));
        gvahand->cancel_operation = TRUE;
      }
    }

    
    
    if(gvahand->cancel_operation)
    {
      /* delete incomplete TOC file
       * (dont know if libmpeg3 can use unfinished TOC files)
       */
      g_remove(vindex->tocfile);
    }
    else
    {
      if(gap_debug) printf("GOT total_frames: %d\n", (int)l_total_frames);
      vindex->total_frames = l_total_frames;

      /* close video and switch to the TOC file */
      p_wrapper_mpeg3_close(gvahand);
      p_wrapper_mpeg3_open_read(vindex->tocfile, gvahand);
    }
  }

  gvahand->percentage_done = 0.0;

  if(vindex)
  {
    if(gap_debug)
    {
      printf("p_wrapper_ffmpeg_count_frames: before GVA_save_videoindex\n");
      printf("p_wrapper_ffmpeg_count_frames: gvahand->filename:%s\n", gvahand->filename);
    }
    if(gvahand->cancel_operation)
    {
      /* because of cancel the total_frames is still unknown
       * (vindex->total_frames == 0 is the indicator for incomplete index)
       * For libmpeg3: videoindex is just used to store total_frames info
       *               so we dont save incomplete videoindex file
       */
      vindex->total_frames = 0;
    }
    else
    {
      GVA_save_videoindex(vindex, gvahand->filename, GVA_LIBMPEG3_DECODER_NAME);
    }
  }

  if(!gvahand->cancel_operation)
  {
     gvahand->total_frames = l_total_frames;
     gvahand->all_frames_counted = TRUE;
  }
  else
  {
     gvahand->total_frames = MAX(gvahand->total_frames, l_total_frames);
     gvahand->cancel_operation = FALSE;  /* reset cancel flag */
     return(GVA_RET_ERROR);
  }

  return(GVA_RET_OK);

}  /* end p_wrapper_mpeg3_count_frames */


/* ----------------------------------
 * p_wrapper_mpeg3_seek_support
 * ----------------------------------
 */
t_GVA_SeekSupport
p_wrapper_mpeg3_seek_support(t_GVA_Handle *gvahand)
{
  return(GVA_SEEKSUPP_VINDEX);
}  /* end p_wrapper_mpeg3_seek_support */


/* -------------------------------
 * p_wrapper_mpeg3_get_video_chunk
 * -------------------------------
 * read video_frame_chunk at frame_nr
 *   we use a separate handle (raw_handle) for mpeg3 file access
 *   to keep positions of the main_handle unaffected.
 *   further we do not use seek ops
 *   to get the exact frame numbers.
 */
t_GVA_RetCode
p_wrapper_mpeg3_get_video_chunk(t_GVA_Handle  *gvahand
                            , gint32 frame_nr
                            , unsigned char *chunk
                            , gint32 *size
                            , gint32 max_size)
{
  t_GVA_mpeg3 *handle;
  int      l_rc;
  long     l_size;
  unsigned long code;
  unsigned char *buffer;
  int      l_error_return;

  if(frame_nr < 1)
  {
    /* illegal frame_nr (first frame starts at Nr 1 */
    return(GVA_RET_ERROR);
  }

  handle = (t_GVA_mpeg3 *)gvahand->decoder_handle;
  if(handle == NULL)
  {
    return(GVA_RET_ERROR);
  }
  if(handle->main_handle == NULL)
  {
    return(GVA_RET_ERROR);
  }

  if(handle->raw_handle != NULL)
  {
    if(handle->raw_pos >= frame_nr)
    {
      mpeg3_close(handle->raw_handle);
      handle->raw_handle = NULL;
    }
  }

  if(handle->raw_handle == NULL)
  { 
     handle->raw_handle = mpeg3_open_copy(gvahand->filename, handle->main_handle, &l_error_return);
     if((handle->raw_handle == NULL) || (l_error_return != 0))
     {
       return(GVA_RET_ERROR);
     }
     
     handle->raw_pos = 0;
     handle->prev_code = 0;
  }

  buffer = g_malloc(max_size);
  while (TRUE)
  {
    /* Read the next compressed frame including headers. */
    /* Store the size in size and return a 1 if error. */
    /* Stream defines the number of the multiplexed stream to read. */
    l_rc = mpeg3_read_video_chunk(handle->raw_handle
                                 , buffer   /* out: unsigned char *output */
                                 , &l_size  /* out: size of the chunk */
                                 , (long) max_size
                                 ,(int)gvahand->vid_track      /* stream */
                                 );
  
    if(l_rc == 0)
    {
      handle->raw_pos++;
      
      code =  (unsigned long)buffer[l_size - 4] << 24;
      code |= (unsigned long)buffer[l_size - 3] << 16;
      code |= (unsigned long)buffer[l_size - 2] << 8;
      code |= (unsigned long)buffer[l_size - 1];
      
      /* check if we have reached the wanted frame */
      if(handle->raw_pos == frame_nr)
      {
        unsigned char *chunk_ptr;
        
        /* got start code of next frame at the end of the buffer ? */
        if(handle->prev_code == MPEG3_PICTURE_START_CODE)
        {
          l_size -= 4; 
        }
        
        *size = (gint32)l_size;
        chunk_ptr = chunk;
        
        if(handle->prev_code == MPEG3_PICTURE_START_CODE)
        {
          /* begin the chunk data with Picture start code 
           * (from the end of prev. buffer) 
           */
          *size += 4;
          chunk_ptr[0] = (handle->prev_code >> 24) & 0xff;
          chunk_ptr[1] = (handle->prev_code >> 16) & 0xff;
          chunk_ptr[2] = (handle->prev_code >> 8)  & 0xff;
          chunk_ptr[3] =  handle->prev_code        & 0xff;
          chunk_ptr += 4;
        }
        memcpy(chunk_ptr, buffer, l_size);
        handle->prev_code = code;
        
        g_free(buffer);
        
        return(GVA_RET_OK);
      }
      
      handle->prev_code = code;
    }
    else
    {
      g_free(buffer);
      if(l_rc == 1)  
      {
        return(GVA_RET_EOF); 
      }
      else
      {
        return(GVA_RET_ERROR);
      }
    }

    gvahand->percentage_done = (gdouble)handle->raw_pos / (gdouble)frame_nr;

    if(gvahand->do_gimp_progress) {  gimp_progress_update (gvahand->percentage_done); }
    if(gvahand->fptr_progress_callback)
    {
      gvahand->cancel_operation = (*gvahand->fptr_progress_callback)(gvahand->percentage_done, gvahand->progress_cb_user_data);
      if(gvahand->cancel_operation)
      {
        break;
      }
    }

  }

  g_free(buffer);
  return(GVA_RET_ERROR);

}  /* end p_wrapper_mpeg3_get_video_chunk */


/* -----------------------------
 * p_mpeg3_new_dec_elem
 * -----------------------------
 * create a new decoder element and init with
 * functionpointers referencing the
 * LIBMPEG3 specific Procedures
 */
t_GVA_DecoderElem  *
p_mpeg3_new_dec_elem(void)
{
  t_GVA_DecoderElem  *dec_elem;

  dec_elem = g_malloc0(sizeof(t_GVA_DecoderElem));
  if(dec_elem)
  {
    dec_elem->decoder_name         = g_strdup(GVA_LIBMPEG3_DECODER_NAME);
    dec_elem->decoder_description  = g_strdup("MPEG Decoder (MPEG1/2 video and system streams, DVD) (EXT: .mpg;.vob;.dat;.mpeg)");
    dec_elem->fptr_check_sig       = &p_wrapper_mpeg3_check_sig;
    dec_elem->fptr_open_read       = &p_wrapper_mpeg3_open_read;
    dec_elem->fptr_close           = &p_wrapper_mpeg3_close;
    dec_elem->fptr_get_next_frame  = &p_wrapper_mpeg3_get_next_frame;
    dec_elem->fptr_seek_frame      = &p_wrapper_mpeg3_seek_frame;
    dec_elem->fptr_seek_audio      = &p_wrapper_mpeg3_seek_audio;
    dec_elem->fptr_get_audio       = &p_wrapper_mpeg3_get_audio;
    dec_elem->fptr_count_frames    = &p_wrapper_mpeg3_count_frames;
    dec_elem->fptr_seek_support    = &p_wrapper_mpeg3_seek_support;
    dec_elem->fptr_get_video_chunk = &p_wrapper_mpeg3_get_video_chunk;
    dec_elem->next = NULL;
  }

  return (dec_elem);
}  /* end p_mpeg3_new_dec_elem */


/*
 * mpeg3 tool procedures
 * -------------------------------------
 * -------------------------------------
 */



/* -----------------------
 * p_mpeg3_emulate_seek
 * -----------------------
 * procedure to completely emulate frame seek by dummy reads
 * (is very slow but sets position to exact frame position
 *  this is needed if we have no TOC file)
 */
#define GVA_CLEANSEEKSIZE 48

static int
p_mpeg3_emulate_seek(mpeg3_t*  handle, gint32 seekto_frame, t_GVA_Handle *gvahand)
{
  gint32  l_clean_reads;
  gint32  l_dirty_reads;
  gint32  l_ii;
  int     l_rc;
  mpeg3_t*  seek_handle;
  char *dummy_y;
  char *dummy_u;
  char *dummy_v;
  int   l_error_return;

  l_rc = 0;
  l_dirty_reads = 0;

  gvahand->percentage_done = 0.0001;

  if (seekto_frame < gvahand->current_seek_nr)
  {
    t_GVA_mpeg3*  decoder_handle;

    l_clean_reads = seekto_frame;

    /* printf(" ++ 1 ++ p_mpeg3_emulate_seek before mpeg3_open_copy  %s\n", gvahand->filename); */

    /* bakward seek: reopen needed */
    seek_handle = mpeg3_open_copy(gvahand->filename, handle, &l_error_return);
    /* printf(" ++ 2 ++ p_mpeg3_emulate_seek after mpeg3_open_copy  %s\n", gvahand->filename); */

    decoder_handle = (t_GVA_mpeg3*)gvahand->decoder_handle;

    if((seek_handle) && (l_error_return == 0))
    {
      /* printf(" ++ 3 ++ p_mpeg3_emulate_seek before mpeg3_close\n"); */
      mpeg3_close(decoder_handle->main_handle);
      decoder_handle->main_handle = seek_handle;
    }
    else
    {
       printf("p_mpeg3_emulate_seek(E): REOPEN ERROR filename: %s\n", gvahand->filename);
    }

    if(gap_debug)
    {
      printf("p_mpeg3_emulate_seek(E): after REOPEN seek_handle: %ld  filename:%s\n"
          , (long)seek_handle
          , gvahand->filename
          );
    }

  }
  else
  {
    /* forward seek: continue reading
     * after open gvahand->current_seek_nr points to 1 (before the 1st frame)
     */
    seek_handle = handle;
    l_clean_reads = 1 + (seekto_frame - gvahand->current_seek_nr);

  }


  if(l_clean_reads > 0)
  {
    /* after (re)open we must start with a clean read
     * (never start with a dirty read (because it does not advance position as expected)
     */
     l_clean_reads--;

     l_rc = mpeg3_read_yuvframe_ptr(seek_handle
            , &dummy_y
            , &dummy_u
            , &dummy_v
            ,(int)gvahand->vid_track
            );
  }

  if (l_clean_reads > GVA_CLEANSEEKSIZE)
  {
    l_dirty_reads = l_clean_reads - GVA_CLEANSEEKSIZE;
    l_clean_reads = GVA_CLEANSEEKSIZE;
  }

  if(gap_debug)
  {
    printf("p_mpeg3_emulate_seek(E): current_seek_nr: %d    seekto_frame: %d  seek_handle:%ld l_clean_reads: %d l_dirty_reads: %d\n"
          , (int)gvahand->current_seek_nr
          , (int)seekto_frame
          , (long)seek_handle
          , (int)l_clean_reads
          , (int)l_dirty_reads
          );
  }
  /* dirty read ops without decoding at all */
  if(l_dirty_reads > 0)
  {
    guchar *dummy_buffer;
    long    dummy_buffer_size;
    long    chunk_size;

     /* buffer large enough for one uncompressed frame */
    dummy_buffer_size = mpeg3_video_width(seek_handle,  (int)gvahand->vid_track)
                      * mpeg3_video_height(seek_handle, (int)gvahand->vid_track)
                      * 3;
    dummy_buffer = g_malloc(dummy_buffer_size);

    for(l_ii = 0; l_ii < l_dirty_reads; l_ii++)
    {
      /* read one frame (we use the compressed chunk
       * because this is a dummy read to skip frames only)
       */
      l_rc = mpeg3_read_video_chunk(seek_handle,
                                 dummy_buffer,
                                 &chunk_size,
                                 dummy_buffer_size,
                                 gvahand->vid_track
                                );
       if(0 != l_rc)
       {
          break;  /* eof, or fetch error */
       }

      gvahand->percentage_done = (gdouble)l_ii / (l_clean_reads + l_dirty_reads);

      if(gvahand->do_gimp_progress) {  gimp_progress_update (gvahand->percentage_done); }
      if(gvahand->fptr_progress_callback)
      {
        gvahand->cancel_operation = (*gvahand->fptr_progress_callback)(gvahand->percentage_done, gvahand->progress_cb_user_data);
        if(gvahand->cancel_operation)
        {
          return(-1);
        }
      }
    }
    g_free(dummy_buffer);
  }

  /* clean read ops with native yuv decoding */
  for(l_ii = 0; l_ii < l_clean_reads; l_ii++)
  {
      /* Read a frame in the native color model used by the stream.  */
      /* The Y, U, and V planes are not copied but the _output pointers */
      /* are redirected to the frame buffer. */
      l_rc = mpeg3_read_yuvframe_ptr(seek_handle
                  , &dummy_y
                  , &dummy_u
                  , &dummy_v
                  ,(int)gvahand->vid_track
                  );
      if(l_rc != 0)
      {
        break;
      }

      gvahand->percentage_done = (gdouble)(l_ii + l_dirty_reads) / (l_clean_reads + l_dirty_reads);

      if(gvahand->do_gimp_progress) {  gimp_progress_update (gvahand->percentage_done); }
      if(gvahand->fptr_progress_callback)
      {
        gvahand->cancel_operation = (*gvahand->fptr_progress_callback)(gvahand->percentage_done, gvahand->progress_cb_user_data);
        if(gvahand->cancel_operation)
        {
          return(-1);
        }
      }
  }

  gvahand->percentage_done = 0.0;

  return(l_rc);
}  /* end p_mpeg3_emulate_seek */



/* -----------------------
 * p_mpeg3_gopseek   DEPRECATED
 * -----------------------
 * workaround procedure to perform frame seek
 * with a combination of seek and some dummy reads.
 * to avoid trashed images in subsequent read ops.
 *
 * hof: maybe i can drop that procedure
 *      if tests with the new libmpeg3-1.5.4 work
 *      without this workaround
 */
#define GVA_GOPSEEKSIZE 24

static int
p_mpeg3_gopseek(mpeg3_t*  handle, gint32 seekto_frame, t_GVA_Handle *gvahand)
{
  gint32  l_gopseek;
  gint32  l_ii;
  gint32  l_clean_read;
  int     l_rc;
  gdouble l_progress_step;
  mpeg3_t*  seek_handle;
  t_GVA_mpeg3*  decoder_handle;
  int      l_error_return;

  l_rc = 0;
  l_gopseek = MAX((seekto_frame - GVA_GOPSEEKSIZE), 1);

   if(gap_debug)
   {
     printf("p_mpeg3_gopseek(F): current_seek_nr: %d     seekto_frame: %d  l_gopseek: %d\n"
               , (int)gvahand->current_seek_nr
               , (int)seekto_frame
               , (int)l_gopseek
               );
   }

  gvahand->percentage_done = 0.0001;
  if(l_gopseek == 1)
  {
    seek_handle = mpeg3_open_copy(gvahand->filename, handle, &l_error_return);
    if((seek_handle) && (l_error_return == 0))
    {
      decoder_handle = (t_GVA_mpeg3*)gvahand->decoder_handle;
      mpeg3_close(decoder_handle->main_handle);
      decoder_handle->main_handle = seek_handle;
      mpeg3_set_frame(seek_handle, 1, gvahand->vid_track); /* Seek to 1.st frame */
    }
  }
  else
  {
    seek_handle = handle;
  }

  if(seekto_frame > 1)
  {
    l_progress_step =  1.0 / MAX((gdouble)seekto_frame - (gdouble)l_gopseek, 1.0);

    if ( (l_gopseek < gvahand->current_seek_nr)
    ||   ((gvahand->current_seek_nr + GVA_GOPSEEKSIZE) <= seekto_frame)  )
    {
       if(gap_debug) printf("p_mpeg3_gopseek(F): (GOPSEEK NEEDED)\n");
      /* perform (faster) seek if we have to do step back
       * or if we have to do a big step forward (greater than GVA_GOPSEEKSIZE)
       */
      l_rc = mpeg3_set_frame(seek_handle, l_gopseek +1, (int)gvahand->vid_track);
      gvahand->percentage_done += l_progress_step;

      if(gvahand->do_gimp_progress) {  gimp_progress_update (gvahand->percentage_done); }
      if(gvahand->fptr_progress_callback)
      {
        gvahand->cancel_operation = (*gvahand->fptr_progress_callback)(gvahand->percentage_done, gvahand->progress_cb_user_data);
        if(gvahand->cancel_operation)
        {
          return(-1);
        }
      }
    }

    /* advance position by dummy read ops, and (hopefully catching an I frame)
     * (the number of dummy Reads will be GVA_GOPSEEKSIZE
     *  or less if seekto_frame is near, or in the first few frames of the video)
     */

    l_clean_read = seekto_frame - l_gopseek;

    if (gap_debug) printf(" clean_reads: %d\n", (int) l_clean_read);

    for(l_ii = 0; l_ii < l_clean_read; l_ii++)
    {
      char *dummy_y;
      char *dummy_u;
      char *dummy_v;

      /* Read a frame in the native color model used by the stream.  */
      /* The Y, U, and V planes are not copied but the _output pointers */
      /* are redirected to the frame buffer. */
      l_rc = mpeg3_read_yuvframe_ptr(seek_handle
                  , &dummy_y
                  , &dummy_u
                  , &dummy_v
                  ,(int)gvahand->vid_track
                  );
      if(l_rc != 0)
      {
        break;
      }
      gvahand->percentage_done += l_progress_step;

      if(gvahand->do_gimp_progress) {  gimp_progress_update (gvahand->percentage_done); }
      if(gvahand->fptr_progress_callback)
      {
        gvahand->cancel_operation = (*gvahand->fptr_progress_callback)(gvahand->percentage_done, gvahand->progress_cb_user_data);
        if(gvahand->cancel_operation)
        {
          return(-1);
        }
      }
    }
  }
  gvahand->percentage_done = 0.0;
  return(l_rc);
}  /* end p_mpeg3_gopseek */


/* -------------------------
 * p_check_libmpeg3_toc_file
 * -------------------------
 * check libmpeg3 if file is libmpeg3 typical table of content file
 * starting with "TOC "
 * libmpeg3-1.5.4 require toc files for exact frame seek operations
 * the name of the toc file must then be passed to the 
 * the mpeg3_open procedure (rather than the original videofile)
 *
 * toc files can be created explicitly with the commandline tool mpeg3toc
 * (that is part of the libmpeg3 distribution)
 * or implicitly by the GVA video Api procedures.
 */
gboolean
p_check_libmpeg3_toc_file(const char *filename)
{
  FILE *fp;
  gboolean l_rc;
 
  l_rc = FALSE;
  fp = g_fopen(filename, "rb");
  if(fp)
  {
    char buff[5];
    
    buff[0] = '\0';
    fread(&buff[0], 1, 4, fp);
    buff[4] = '\0';
    if(strncmp(&buff[0], "TOC ", 4) == 0)
    {
      l_rc = TRUE;
    }
    
    fclose(fp);
  }
  return (l_rc);
}  /* end p_check_libmpeg3_toc_file */


#endif   /* ENABLE_GVA_LIBMPEG3 */
