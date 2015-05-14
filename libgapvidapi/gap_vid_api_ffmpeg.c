/* gap_vid_api_ffmpeg.c
 *
 * GAP Video read API implementation of libavformat/lbavcodec (also known as FFMPEG)
 * based wrappers to read various videofile formats
 *
 * 2010.07.31   update to support both ffmpeg-0.5 and ffmpeg-0.6
 * 2007.11.04   update to ffmpeg svn snapshot 2007.10.31
 *                bugfix: selftest sometimes did not detect variable timecodes.
 * 2007.04.04   update to ffmpeg svn snapshot 2007.04.04,
 *               droped support for older ffmpeg versions
 * 2007.03.21   support fmpeg native timecode based seek
 * 2007.03.12   update to ffmpeg svn snapshot 2007.03.12,
 * 2005.02.05   update to ffmpeg-0.4.9 (basically works now)
 * 2004.10.24   workaround initial frameread and reopen for detection of yuv_buff_pix_fmt
 *              of the active codec. (works only after the 1.st frame was read)
 * 2004.04.12   vindex bugfix seek high framnumbers sometimes used wrong (last index)
 * 2004.03.07   vindex
 * 2004.02.29   hof fptr_progress_callback
 * 2003.05.09   hof created
 *
 */


/* ================================================ FFMPEG
 * FFMPEG (libavformat libavcodec)                  FFMPEG
 * ================================================ FFMPEG
 * ================================================ FFMPEG
 */
#ifdef ENABLE_GVA_LIBAVFORMAT
#include "avformat.h"
#include "swscale.h"
#include "stdlib.h"
#include "gap_val_file.h"
#include "gap_base.h"
#include "audioconvert.h"
#include "imgconvert.h"


/* start ffmpeg 0.5 / 0.6 support */
#if LIBAVCODEC_VERSION_MAJOR < 52
#define GAP_USES_OLD_FFMPEG_0_5
#endif
#if LIBAVCODEC_VERSION_MAJOR == 52
#if LIBAVCODEC_VERSION_MINOR <= 20
#define GAP_USES_OLD_FFMPEG_0_5
#endif
#endif


#ifdef GAP_USES_OLD_FFMPEG_0_5
/* defines to use older ffmpeg-0.5 compatible types */
#define AVMEDIA_TYPE_UNKNOWN  CODEC_TYPE_UNKNOWN
#define AVMEDIA_TYPE_VIDEO    CODEC_TYPE_VIDEO
#define AVMEDIA_TYPE_AUDIO    CODEC_TYPE_AUDIO
#define AV_PKT_FLAG_KEY       PKT_FLAG_KEY
  static const char *GAP_FFMPEG_VERSION_STRING  = "0.5";
  static const char *PROCNAME_AVCODEC_DECODE_VIDEO = "avcodec_decode_video";
#else
  static const char *GAP_FFMPEG_VERSION_STRING  = "0.6";
  static const char *PROCNAME_AVCODEC_DECODE_VIDEO = "avcodec_decode_video2";
#endif

/* end ffmpeg 0.5 / 0.6 support */



#define READSTEPS_PROBE_TIMECODE 32

#define DEFAULT_NAT_SEEK_GOPSIZE 24
#define MAX_NAT_SEEK_GOPSIZE 8192

/* some defines to enable theme specific debug printf's
 * those defines are currently not configurable.
 */
#undef  GAP_DEBUG_FF_NATIVE_SEEK



#define MAX_TRIES_NATIVE_SEEK 3
#define GIMPRC_PERSISTENT_ANALYSE "video-gva-libavformat-video-analyse-persistent"
#define GIMPRC_CONTINUE_AFTER_READ_ERRORS "video-gva-libavformat-continue-after-read_errors"
#define ANALYSE_DEFAULT TRUE

/* MAX_PREV_OFFSET defines how to record defered url_offest frames of previous frames for byte positions in video index
 * in tests the byte based seek takes us to n frames after the wanted frame. Therefore video index creation
 * tries to compensate this by recording the offsets of the nth pervious frame.
 *
 * tuning: values 1 upto 12 did not compensate enough.
 * this resulted in more than 1 SYNC LOOP when positioning vio byte position and makes video index slower.
 *
 */
#define MAX_PREV_OFFSET 14


/* -------------------------
 * API READ extension FFMPEG
 * -------------------------
 */


/* samples buffer for ca. 500 Audioframes at 48000 Hz Samplerate and 30 fps
 * if someone tries to get a bigger audio segment than this at once
 * the call will fail !!
 * (shall be defined >= AVCODEC_MAX_AUDIO_FRAME_SIZE (192000)
 */

#define GVA_SAMPLES_BUFFER_SIZE  2400000

#define GVA_FFMPEG_DECODER_NAME "libavformat"


/* structure with FFMPEG specific file and codec handle pointers */
typedef struct t_GVA_ffmpeg
{
 AVFormatContext *vid_input_context;
 AVFormatContext *aud_input_context;

 AVCodecContext  *vid_codec_context;
 AVCodecContext  *aud_codec_context;
 AVCodec         *vcodec;
 AVCodec         *acodec;

 AVStream       *vid_stream;  /* pointer to to vid_input_context->streams[vid_track] */
 AVStream       *aud_stream;  /* pointer to to aud_input_context->streams[aud_track] */

 AVPacket        vid_pkt;         /* the current packet (read from the video stream) */
 AVPacket        aud_pkt;         /* the current packet (read from the audio stream) */
 AVAudioConvert *reformat_ctx;    /* for audioformat conversion */

 AVFrame         big_picture_rgb;
 AVFrame         big_picture_yuv;
 AVPicture      *picture_rgb;
 AVPicture      *picture_yuv;
 enum PixelFormat yuv_buff_pix_fmt;
 /*UNIT8*/  guchar        *yuv_buffer;
 /*UNIT8*/  guchar        *inbuf_ptr;
 int             inbuf_len;
 /*UNIT8*/  guchar        *abuf_ptr;
 int             abuf_len;

  guchar        *output_samples_ptr;    /* current write position (points somewhere into samples_buffer) */

  gint           bf_idx;               /* 0 or 1 (index of the active samples_buffer) */

  gint32        samples_buffer_size;
  guchar        *samples_buffer[2];    /* Buffer for decoded samples from samples_base_a until read position */
  int16_t       *av_samples;           /* aligned buffer for decoding audio samples (MMX requires aligned data) */
  int            av_samples_size;

  gint32        samples_base[2];       /* start offset of the samples_buffers (unit is samples) */

  gint32        bytes_filled[2];       /* how many bytes are filled into samples_buffer(s) */

  gint32        biggest_data_size;     /* the biggest uncompressed audio packet found (min 4096) */

  gdouble        samples_read;         /* how many (decoded) samples already read into samples_buffer
                                        * since (RE)open the audio context
                                        */

 gboolean        dummy_read;      /* FALSE: read YUV + convert to RGB,  TRUE: dont convert RGB */
 gboolean        capture_offset;  /* TRUE: capture url_offsets to vindex while reading next frame  */
 gint32          max_frame_len;
 guint16         got_frame_length16;   /* 16 lower bits of the length */
 gint64          prev_url_offset[MAX_PREV_OFFSET];
 gint32          prev_key_seek_nr;
 gdouble         guess_gop_size;

 int             vid_stream_index;
 int             aud_stream_index;
 int             vid_codec_id;
 int             aud_codec_id;
 gboolean        key_frame_detection_works;
 gboolean        timecode_proberead_done;
 gint64          timecode_offset_frame1;     /* dts timecode offset of the 1st frame (value -1 for not known) */
 gint32          timecode_step_avg;          /* average timecode step per frame (value -1 for not known)  */
 gint32          timecode_step_abs_min;      /* absolute value of smallest detected stepsize  */
 gint32          timecode_steps_sum;         /* average timecode step per frame (value -1 for not known)  */
 gint32          count_timecode_steps;
 gint32          timecode_steps[READSTEPS_PROBE_TIMECODE];
 gint32          video_libavformat_seek_gopsize;    /* gimprc parameter controls native seek 0==DISABLE */
 gint32          native_timecode_seek_failcount;    /* increased when expected timecode cant be found (seek after EOF is not counted) */
 gint64          eof_timecode;                      /* timecode after the last frame */
 gboolean        self_test_detected_seek_bug;
 gint32          max_tries_native_seek;
 gint32          readsteps_probe_timecode;
 gint32          timestamp;                         /* videofile last modification utc time */

 gboolean        prefere_native_seek;               /* prefere native seek if both vindex and native seek available */
 gboolean        all_timecodes_verified;
 gboolean        critical_timecodesteps_found;

 unsigned char *  chunk_ptr;
 gint32           chunk_len;

 struct SwsContext *img_convert_ctx;

 gboolean           continueAfterReadErrors;   /* default TRUE try to to continue reading next frame after read errors */
 gint32             libavcodec_version_int;    /* the ffmpeg libs version that was used to analyze the current video as integer LIBAVCODEC_VERSION_INT */
 gint64             pkt1_dts;                  /* dts timecode offset of the 1st package of the current frame */

} t_GVA_ffmpeg;


static void      p_ffmpeg_vid_reopen_read(t_GVA_ffmpeg *handle, t_GVA_Handle *gvahand);
static void      p_ffmpeg_aud_reopen_read(t_GVA_ffmpeg *handle, t_GVA_Handle *gvahand);
static gboolean  p_ff_open_input(char *filename, t_GVA_Handle *gvahand, t_GVA_ffmpeg*  handle, gboolean vid_open);
static void      p_set_aspect_ratio(t_GVA_Handle *gvahand, t_GVA_ffmpeg*  handle);

static void      p_reset_proberead_results(t_GVA_ffmpeg*  handle);
static gboolean  p_seek_timecode_reliability_self_test(t_GVA_Handle *gvahand);
static void      p_detect_total_frames_via_native_seek(t_GVA_Handle *gvahand);
static void      p_inc_native_timecode_seek_failcount(t_GVA_Handle *gvahand);
static void      p_clear_inbuf_and_vid_packet(t_GVA_ffmpeg *handle);

static t_GVA_RetCode p_seek_private(t_GVA_Handle *gvahand
                               , gdouble pos
                               , t_GVA_PosUnit pos_unit
                               );

static t_GVA_RetCode p_seek_native_timcode_based(t_GVA_Handle *gvahand
                              , gint32 target_frame);

static int       p_audio_convert_to_s16(t_GVA_ffmpeg *handle
                     , int data_size
                     );

static gint32    p_pick_channel( t_GVA_ffmpeg *handle
                               , t_GVA_Handle *gvahand
                               , gint16 *output_i
                               , gint32 sample_idx
                               , gint32 samples
                               , gint32 channel
                               );
static int       p_read_audio_packets(t_GVA_ffmpeg *handle
                               , t_GVA_Handle *gvahand
                               , gint32 max_sample_pos);

static void      p_vindex_add_url_offest(t_GVA_Handle *gvahand
                         , t_GVA_ffmpeg *handle
                         , t_GVA_Videoindex *vindex
                         , gint32 seek_nr
                         , gint64 url_offset
                         , guint16 frame_length
                         , guint16 checksum
                         , gint64 timecode_dts
                         );
static guint16  p_gva_checksum(AVPicture *picture_yuv, gint32 height);
static t_GVA_RetCode p_wrapper_ffmpeg_get_next_frame(t_GVA_Handle *gvahand);

static FILE *  p_init_timecode_log(t_GVA_Handle   *copy_gvahand);
static void    p_timecode_check_and_log(FILE *fp_timecode_log, gint32 framenr
                             , t_GVA_ffmpeg *handle
                             , t_GVA_ffmpeg *master_handle
                             , t_GVA_Handle *gvahand);



static void    p_set_analysefile_master_keywords(GapValKeyList *keylist
                       , t_GVA_Handle *gvahand
                       , gint32 count_timecode_steps);
static void     p_save_video_analyse_results(t_GVA_Handle *gvahand);
static gboolean p_get_video_analyse_results(t_GVA_Handle *gvahand);
static char*    p_create_analysefile_name(t_GVA_Handle *gvahand);
static gint32   p_timecode_to_frame_nr(t_GVA_ffmpeg *handle, int64_t timecode);
static int64_t  p_frame_nr_to_timecode(t_GVA_ffmpeg *handle, gint32 frame_nr);
static void     p_analyze_stepsize_pattern(gint max_idx, t_GVA_Handle *gvahand);
static void     p_probe_timecode_offset(t_GVA_Handle *gvahand);

static          t_GVA_RetCode   p_wrapper_ffmpeg_seek_frame(t_GVA_Handle *gvahand, gdouble pos, t_GVA_PosUnit pos_unit);
static          t_GVA_RetCode   p_wrapper_ffmpeg_get_next_frame(t_GVA_Handle *gvahand);
static          t_GVA_RetCode   p_private_ffmpeg_get_next_frame(t_GVA_Handle *gvahand, gboolean do_copy_raw_chunk_data);

/* -----------------------------
 * p_wrapper_ffmpeg_check_sig
 * -----------------------------
 */
static inline gboolean
p_areTimecodesEqualWithTolerance(gint64 timecode1, gint64 timecode2)
{
#define TIMECODE_EQ_TOLERANCE 2
  return (abs(timecode1 - timecode2) <= TIMECODE_EQ_TOLERANCE);
}

/* -----------------------------
 * p_wrapper_ffmpeg_check_sig
 * -----------------------------
 * TODO: should also check supported codec (for vid_track and aud_track but need this info as calling parameter !!)
 */
gboolean
p_wrapper_ffmpeg_check_sig(char *filename)
{
  AVFormatContext *ic;
  int err, ret;

  if(gap_debug) printf("p_wrapper_ffmpeg_check_sig: START filename:%s\n", filename);

  /* ------------ File Format  -------   */
  /* open the input file with generic libav function
   * (do not force a specific format)
   */
  err = av_open_input_file(&ic, filename, NULL, 0, NULL);
  if (err < 0)
  {
     if(gap_debug) printf("p_wrapper_ffmpeg_check_sig: av_open_input_file FAILED: %d\n", (int)err);
     return(FALSE);
  }

  /* If not enough info to get the stream parameters, we decode the
   * first frames to get it. (used in mpeg case for example)
   */
  ret = av_find_stream_info(ic);
  if (ret < 0)
  {
     if(gap_debug) printf("p_wrapper_ffmpeg_check_sig:%s: could not find codec parameters\n", filename);
     return(FALSE);
  }

  av_close_input_file(ic);

  if(gap_debug) printf("p_wrapper_ffmpeg_check_sig: compatible is TRUE\n");

  return (TRUE);
}  /* end p_wrapper_ffmpeg_check_sig */


/* -----------------------------
 * p_wrapper_ffmpeg_open_read
 * -----------------------------
 *  Open the fileformat
 *  Open the needed decoder codecs (video and audio)
 *  - set the CodecContext
 *  - do we need AVStream vid_stream, aud_stream and howto init and use that ?
 *  - init picture (rowpointers)
 * TODO:
 *  - howto findout total_frames (and total_samples)
 */
void
p_wrapper_ffmpeg_open_read(char *filename, t_GVA_Handle *gvahand)
{
  t_GVA_ffmpeg*  handle;
  AVInputFormat *iformat;


  if(gap_debug) printf("p_wrapper_ffmpeg_open_read: START filename:%s\n", filename);

  if(gvahand->filename == NULL)
  {
    gvahand->filename = g_strdup(filename);
  }


  gvahand->decoder_handle = (void *)NULL;
  gvahand->vtracks = 0;
  gvahand->atracks = 0;
  gvahand->frame_bpp = 3;              /* RGB pixelformat */

  handle = g_malloc0(sizeof(t_GVA_ffmpeg));
  handle->continueAfterReadErrors = gap_base_get_gimprc_gboolean_value(GIMPRC_CONTINUE_AFTER_READ_ERRORS, TRUE);
  handle->libavcodec_version_int = 0;
  handle->pkt1_dts = AV_NOPTS_VALUE;
  handle->dummy_read = FALSE;
  handle->capture_offset = FALSE;
  handle->guess_gop_size = 0;
  {
    gint ii;
    for(ii=0; ii < MAX_PREV_OFFSET; ii++)
    {
      handle->prev_url_offset[ii] = 0;
    }
  }
  handle->prev_key_seek_nr = 1;
  handle->max_frame_len = 0;
  handle->got_frame_length16 = 0;
  handle->vid_input_context = NULL;     /* set later (if has video) */
  handle->aud_input_context = NULL;     /* set later (if has audio) */
  handle->vid_codec_context = NULL;     /* set later (if has video) */
  handle->aud_codec_context = NULL;     /* set later (if has audio) */
  handle->vcodec = NULL;                /* set later (if has video and vid_track > 0) */
  handle->acodec = NULL;                /* set later (if has audio and aud_track > 0) */
  handle->vid_stream_index = -1;
  handle->aud_stream_index = -1;
  handle->vid_codec_id = 0;
  handle->aud_codec_id = 0;
  handle->vid_pkt.size = 0;             /* start with empty packet */
  handle->vid_pkt.data = NULL;          /* start with empty packet */
  handle->aud_pkt.size = 0;             /* start with empty packet */
  handle->aud_pkt.data = NULL;          /* start with empty packet */
  handle->samples_buffer[0] = NULL;        /* set later (at 1.st audio packet read) */
  handle->samples_buffer[1] = NULL;        /* set later (at 1.st audio packet read) */
  handle->av_samples = NULL;               /* set later (at 1.st audio packet read) */
  handle->av_samples_size = 0;
  handle->samples_base[0] = 0;
  handle->samples_base[1] = 0;
  handle->bytes_filled[0] = 0;
  handle->bytes_filled[1] = 0;
  handle->biggest_data_size = 4096;
  handle->output_samples_ptr = NULL;    /* current write position (points somewhere into samples_buffer) */
  handle->reformat_ctx = NULL;          /* context for audio conversion (only in case input is NOT SAMPLE_FMT_S16) */
  handle->samples_buffer_size = 0;
  handle->samples_read = 0;
  handle->key_frame_detection_works = FALSE;  /* assume a Codec with non working detection */
  handle->chunk_len = 0;
  handle->chunk_ptr = NULL;
  handle->img_convert_ctx = NULL;          /* context for img_convert via sws_scale */


  p_reset_proberead_results(handle);

  handle->prefere_native_seek = FALSE;
  handle->all_timecodes_verified = FALSE;
  handle->critical_timecodesteps_found = FALSE;

  /* open for the VIDEO part */
  if(FALSE == p_ff_open_input(filename, gvahand, handle, TRUE))
  {
    g_free(handle);
    if(gap_debug) printf("p_wrapper_ffmpeg_open_read: open Videopart FAILED\n");
    return;
  }

  if(gvahand->aud_track >= 0)
  {
    /* open a separate input context for the AUDIO part */
    if(FALSE == p_ff_open_input(filename, gvahand, handle, FALSE))
    {
      g_free(handle);
      if(gap_debug) printf("p_wrapper_ffmpeg_open_read: open Audiopart FAILED\n");
      return;
    }
  }


  if((handle->vid_stream_index == -1) && (handle->aud_stream_index == -1))
  {
     g_free(handle);
     if(gap_debug) printf("p_wrapper_ffmpeg_open_read: neither video nor audio codec found\n");
     return;    /* give up if both video_codec and audio_codec are not supported */
  }


  /* set detailed decoder description */
  iformat = handle->vid_input_context->iformat;
  if(iformat)
  {
    gchar *vcodec_name;
    gchar *acodec_name;
    t_GVA_DecoderElem  *dec_elem;

    dec_elem = (t_GVA_DecoderElem *)gvahand->dec_elem;

    if(dec_elem)
    {
      if(dec_elem->decoder_description)
      {
        g_free(dec_elem->decoder_description);
      }

      if(handle->vcodec)  { vcodec_name = g_strdup(handle->vcodec->name); }
      else                { vcodec_name = g_strdup("** no video codec **"); }

      if(handle->acodec)  { acodec_name = g_strdup(handle->acodec->name); }
      else                { acodec_name = g_strdup("** no audio codec **"); }

      dec_elem->decoder_description =
        g_strdup_printf("FFMPEG Decoder (Decoder for MPEG1,MPEG4(DivX),RealVideo)\n"
                        " (EXT: .mpg,.vob,.avi,.rm,.mpeg)\n"
                        "active FORMAT: %s %s\n"
                        "active CODEC(S): %s  %s"
                        , iformat->name
                        , iformat->long_name
                        , vcodec_name
                        , acodec_name
                       );

      g_free(vcodec_name);
      g_free(acodec_name);
    }
  }



  handle->inbuf_len = 0;            /* start with empty buffer */
  handle->inbuf_ptr = NULL;         /* start with empty buffer, after 1.st av_read_frame: pointer to pkt.data read pos */

  /* yuv_buffer big enough for all supported PixelFormats
   * (the vid_codec_context->pix_fmt may change later (?)
   *  but i want to use one all purpose yuv_buffer all the time
   *  while the video is open for performance reasons)
   */
  handle->yuv_buffer = g_malloc0(gvahand->width * gvahand->height * 4);


  /* total_frames and total_aud_samples are just a guess, based on framesize and filesize
   * It seems that libavformat does not provide a method to get the exact value.
   * (maybe due to the support of streaming where the size is not yet konwn at open time)
   */
  gvahand->total_frames = p_guess_total_frames(gvahand);
  gvahand->total_aud_samples = 0;
  if((gvahand->atracks > 0) && (handle->aud_stream_index >= 0))
  {
    gvahand->total_aud_samples = (gint32)GVA_frame_2_samples(gvahand->framerate, gvahand->samplerate, gvahand->total_frames);
  }

  /* Picture field are filled with 'ptr' addresses */

  handle->picture_rgb = (AVPicture *)&handle->big_picture_rgb;
  handle->picture_yuv = (AVPicture *)&handle->big_picture_yuv;

  /* allocate frame_data and row_pointers for one frame (minimal cachesize 1 == no chaching)
   * (calling apps may request bigger cache after successful open)
   */
  p_build_frame_cache(gvahand, 1);

  avpicture_fill(handle->picture_rgb
                ,gvahand->frame_data
                ,PIX_FMT_RGB24      /* PIX_FMT_RGB24, PIX_FMT_RGBA32, PIX_FMT_BGRA32 */
                ,gvahand->width
                ,gvahand->height
                );


  /*
   * vid_codec_context->pix_fmt may change later, and is sometimes
   * not recognized until the 1.st frame has been decoded.
   * therefore the follwing avpicture_fill is just a 1.st guess
   * and must be repeated later to be sure to know the codec's native pix_fmt
   * But this does decode the the 1.st frame with the wrong PIX_FMT settings
   *  - most MPEG codecs (VCD, DVD..) use the default PIX_FMT_YUV420P
   *    and work fine without problems
   *  - Quicktime codec (codec_id == 10)  uses PIX_FMT_YUV422P ( Olympus C4040Z Digicam MJPEG .mov files)
   */
  handle->yuv_buff_pix_fmt = handle->vid_codec_context->pix_fmt;   /* PIX_FMT_YUV420P */

  avpicture_fill(handle->picture_yuv
                ,handle->yuv_buffer
                ,handle->yuv_buff_pix_fmt
                ,gvahand->width
                ,gvahand->height
                );



  gvahand->decoder_handle = (void *)handle;


  /* workaround: initial frameread and reopen for detection of yuv_buff_pix_fmt */
  {
    if(gap_debug)
    {
      printf("INITIAL call p_wrapper_ffmpeg_get_next_frame\n");
    }
    p_wrapper_ffmpeg_get_next_frame(gvahand);
    p_set_aspect_ratio(gvahand, handle);

    /* reset seek position */
    p_ffmpeg_vid_reopen_read(handle, gvahand);
    gvahand->current_frame_nr = 0;
    gvahand->current_seek_nr = 1;

  }


  if(gap_debug)
  {
     printf("gvahand->width: %d  gvahand->height: %d aspect_ratio:%f\n", (int)gvahand->width , (int)gvahand->height, (float)gvahand->aspect_ratio);
     printf("picture_rgb: data[0]: %d linesize[0]: %d\n", (int)handle->picture_rgb->data[0], (int)handle->picture_rgb->linesize[0]);
     printf("picture_rgb: data[1]: %d linesize[1]: %d\n", (int)handle->picture_rgb->data[1], (int)handle->picture_rgb->linesize[1]);
     printf("picture_rgb: data[2]: %d linesize[2]: %d\n", (int)handle->picture_rgb->data[2], (int)handle->picture_rgb->linesize[2]);
     printf("picture_rgb: data[3]: %d linesize[3]: %d\n", (int)handle->picture_rgb->data[3], (int)handle->picture_rgb->linesize[3]);

     printf("picture_yuv: data[0]: %d linesize[0]: %d\n", (int)handle->picture_yuv->data[0], (int)handle->picture_yuv->linesize[0]);
     printf("picture_yuv: data[1]: %d linesize[1]: %d\n", (int)handle->picture_yuv->data[1], (int)handle->picture_yuv->linesize[1]);
     printf("picture_yuv: data[2]: %d linesize[2]: %d\n", (int)handle->picture_yuv->data[2], (int)handle->picture_yuv->linesize[2]);
     printf("picture_yuv: data[3]: %d linesize[3]: %d\n", (int)handle->picture_yuv->data[3], (int)handle->picture_yuv->linesize[3]);
     printf("handle->yuv_buff_pix_fmt: %d\n", (int)handle->yuv_buff_pix_fmt);
  }


  if(gvahand->vindex == NULL)
  {
    gvahand->vindex = GVA_load_videoindex(gvahand->filename, gvahand->vid_track, GVA_FFMPEG_DECODER_NAME);
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

  if(gap_debug)
  {
    if(gvahand->vindex)
    {
      if(gvahand->vindex->videoindex_filename)
      {
        printf("IDX: p_wrapper_ffmpeg_open_read: vindex->videoindex_filename %s\n"
               "  vindex->total_frames:%d\n"
              , gvahand->vindex->videoindex_filename
              , gvahand->vindex->total_frames
              );
      }
      else
      {
        printf("IDX: p_wrapper_ffmpeg_open_read: vindex->videoindex_filename is NULL\n");
      }
    }
    else
    {
      printf("IDX: p_wrapper_ffmpeg_open_read: vindex is NULL\n");
    }

    printf("p_wrapper_ffmpeg_open_read: END, OK\n");
  }

}  /* end p_wrapper_ffmpeg_open_read */


/* -----------------------------
 * p_wrapper_ffmpeg_close
 * -----------------------------
 * TODO:
 *  - howto close codec, codec ontext ... and whatever ...
 */
void
p_wrapper_ffmpeg_close(t_GVA_Handle *gvahand)
{
  t_GVA_ffmpeg *handle;

  if(gap_debug) printf("p_wrapper_ffmpeg_close: START\n");

  handle = (t_GVA_ffmpeg *)gvahand->decoder_handle;

  if(handle->av_samples)
  {
    av_free(handle->av_samples);
    handle->av_samples = NULL;
    handle->av_samples_size = 0;
  }

  if(handle->samples_buffer[0])
  {
    if(gap_debug) printf("p_wrapper_ffmpeg_close: FREE audio samples_buffer\n");
    g_free(handle->samples_buffer[0]);
    handle->samples_buffer[0] = NULL;
    g_free(handle->samples_buffer[1]);
    handle->samples_buffer[1] = NULL;
  }

  if(handle->yuv_buffer)
  {
    if(gap_debug) printf("p_wrapper_ffmpeg_close: FREE YUV_BUFFER\n");
    g_free(handle->yuv_buffer);
    handle->yuv_buffer = NULL;
  }

  if((handle->vid_codec_context) && (handle->vcodec))
  {
    if(gap_debug) printf("p_wrapper_ffmpeg_close: avcodec_close VIDEO_CODEC: %s\n", handle->vcodec->name);

    avcodec_close(handle->vid_codec_context);
    handle->vid_codec_context = NULL;
    /* ?????????? do not attempt to free handle->vid_codec_context (it points to &handle->vid_stream.codec) */
  }

  if((handle->aud_codec_context) && (handle->acodec))
  {
    if(gap_debug) printf("p_wrapper_ffmpeg_close: avcodec_close AUDIO_CODEC: %s\n", handle->acodec->name);

    avcodec_close(handle->aud_codec_context);
    handle->aud_codec_context = NULL;
    /* ??????? do not attempt to free handle->aud_codec_context (it points to &handle->aud_stream.codec) */
  }

  if (handle->img_convert_ctx != NULL)
  {
    sws_freeContext(handle->img_convert_ctx);
    handle->img_convert_ctx = NULL;
  }

  /* Close a media file (but not its codecs) */
  if(gap_debug) printf("p_wrapper_ffmpeg_close: av_close_input_file\n");
  if(handle->vid_input_context) av_close_input_file(handle->vid_input_context);
  if(handle->aud_input_context) av_close_input_file(handle->aud_input_context);

  if(gap_debug) printf("p_wrapper_ffmpeg_close: END\n");

}  /* end p_wrapper_ffmpeg_close */


/* ----------------------------------
 * p_wrapper_ffmpeg_get_codec_name
 * ----------------------------------
 * read one frame as raw data from the video_track (stream)
 * note that raw data is not yet decoded and contains
 * videofileformat specific frame header information.
 * this procedure is intended to 1:1 (lossless) copy of frames
 */
char *
p_wrapper_ffmpeg_get_codec_name(t_GVA_Handle  *gvahand
                             ,t_GVA_CodecType codec_type
                             ,gint32 track_nr
                             )
{
  t_GVA_ffmpeg *handle;

  handle = (t_GVA_ffmpeg *)gvahand->decoder_handle;
  if(handle == NULL)
  {
    if(gap_debug)
    {
      printf("p_wrapper_ffmpeg_get_codec_name  handle == NULL\n");
    }
    return(NULL);
  }

  if (codec_type == GVA_VIDEO_CODEC)
  {
    if(handle->vcodec)
    {
      if(gap_debug)
      {
        printf("p_wrapper_ffmpeg_get_codec_name  handle->vcodec == %s\n", handle->vcodec->name);
      }
      return(g_strdup(handle->vcodec->name));
    }
  }

  if (codec_type == GVA_AUDIO_CODEC)
  {
    if(handle->acodec)
    {
      if(gap_debug)
      {
        printf("p_wrapper_ffmpeg_get_codec_name  handle->acodec == %s\n", handle->acodec->name);
      }
      return(g_strdup(handle->acodec->name));
    }
  }

  if(gap_debug)
  {
    printf("p_wrapper_ffmpeg_get_codec_name  codec is NULL\n");
  }
  return(NULL);

}  /* end p_wrapper_ffmpeg_get_codec_name */


/* ----------------------------------
 * p_wrapper_ffmpeg_get_video_chunk
 * ----------------------------------
 * read one frame as raw data from the video_track (stream)
 * note that raw data is not yet decoded and contains
 * videofileformat specific frame header information.
 * this procedure is intended to 1:1 (lossless) copy of frames
 */
t_GVA_RetCode
p_wrapper_ffmpeg_get_video_chunk(t_GVA_Handle  *gvahand
                            , gint32 frame_nr
                            , unsigned char *chunk
                            , gint32 *size
                            , gint32 max_size)
{
  t_GVA_RetCode l_rc;
  t_GVA_ffmpeg *handle;

  if(frame_nr < 1)
  {
    /* illegal frame_nr (first frame starts at Nr 1 */
    return(GVA_RET_ERROR);
  }

  handle = (t_GVA_ffmpeg *)gvahand->decoder_handle;
  if(handle == NULL)
  {
    return(GVA_RET_ERROR);
  }

  /* check if current position is before the wanted frame */
  if (frame_nr != gvahand->current_seek_nr)
  {
    gdouble pos;

    pos = frame_nr;
    l_rc = p_wrapper_ffmpeg_seek_frame(gvahand, pos, GVA_UPOS_FRAMES);
    if (l_rc != GVA_RET_OK)
    {
      return (l_rc);
    }
  }


  l_rc = p_private_ffmpeg_get_next_frame(gvahand, TRUE);
  if(handle->chunk_len > max_size)
  {
    printf("CALLING ERROR p_wrapper_ffmpeg_get_video_chunk chunk_len:%d is greater than sepcified max_size:%d\n"
      ,(int)handle->chunk_len
      ,(int)max_size
      );
    return(GVA_RET_ERROR);
  }
  if(handle->chunk_ptr == NULL)
  {
    printf("CALLING ERROR p_wrapper_ffmpeg_get_video_chunk fetch raw frame failed, chunk_ptr is NULL\n");
    return(GVA_RET_ERROR);
  }

  if(gap_debug)
  {
    char *vcodec_name;

    vcodec_name = "NULL";
    if(handle->vcodec)
    {
      if(handle->vcodec->name)
      {
        vcodec_name = handle->vcodec->name;
      }
    }

    printf("p_wrapper_ffmpeg_get_video_chunk: chunk:%d chunk_ptr:%d, chunk_len:%d vcodec_name:%s\n"
      ,(int)chunk
      ,(int)handle->chunk_ptr
      ,(int)handle->chunk_len
      ,vcodec_name
      );
  }

  *size =  handle->chunk_len;
  memcpy(chunk, handle->chunk_ptr, handle->chunk_len);
  return (l_rc);
}  /* end p_wrapper_ffmpeg_get_video_chunk */


/* ----------------------------------
 * p_wrapper_ffmpeg_get_next_frame
 * ----------------------------------
 * read one frame from the video_track (stream)
 * and decode the frame
 * when EOF reached: update total_frames and close the stream)
 */
static t_GVA_RetCode
p_wrapper_ffmpeg_get_next_frame(t_GVA_Handle *gvahand)
{
  return (p_private_ffmpeg_get_next_frame(gvahand, FALSE));
}  /* end  p_wrapper_ffmpeg_get_next_frame*/



/* ----------------------------------
 * p_url_ftell
 * ----------------------------------
 * GIMP-GAP private variant of url_ftell procedure
 * (original implementation see see libacformat/aviobuf.c)
 *
 * restrictions:
 *  only the part for input strams is supported.
 *  (e.g. write_flag is ignored here)
 *  includes some chacks for inplausible results.
 *
 * the internal calculation  looks like this:
 *   pos = s->pos - (s->write_flag ? 0 : (s->buf_end - s->buffer));
 *   return  pos + (s->buf_ptr - s->buffer)
 *
 */
inline static int64_t
p_url_ftell(ByteIOContext *s)
{
  int64_t currentOffsetInStream;
  int64_t bytesReadInBuffer;
  int64_t bytesAlreadyHandledInBuffer;
  int64_t bufferStartOffsetInStream;

  if(s == NULL)
  {
      return (0);
  }

  /* ignore negative buffer size (in such cases the buffer is rather empty or not initialized properly) */
  bytesReadInBuffer = MAX(0, (s->buf_end - s->buffer));

  /* ignore negative buffer size (in such cases the buffer is rather empty or not initialized properly) */
  bytesAlreadyHandledInBuffer = 0;
  if((s->buf_ptr >= s->buffer) && (s->buf_ptr <= s->buf_end))
  {
    bytesAlreadyHandledInBuffer = MAX(0, (s->buf_ptr - s->buffer));
  }
  else
  {
    printf(" ** ERROR current buf_ptr:%d  points outside of the buffer (start: %d   end:%d)\n"
       ,(int)s->buf_ptr
       ,(int)s->buffer
       ,(int)s->buf_end
       );
  }


  /* ignore inplausible staart offsets */
  bufferStartOffsetInStream = 0;
  if(s->pos > bytesReadInBuffer)
  {
    bufferStartOffsetInStream = s->pos - bytesReadInBuffer;
  }

  currentOffsetInStream = bufferStartOffsetInStream + bytesAlreadyHandledInBuffer;

  printf("P_URL_FTELL: url64:%lld  pos: %lld, wr_flag:%d, buffer:%d buf_end:%d buf_ptr:%d\n"
         "  bufferStartOffsetInStream:%lld, bytesReadInBuffer:%lld bytesAlreadyHandledInBuffer:%lld\n\n"
         ,currentOffsetInStream
         ,s->pos
         ,(int)s->write_flag
         ,(int)s->buffer
         ,(int)s->buf_end
         ,(int)s->buf_ptr
         ,bufferStartOffsetInStream
         ,bytesReadInBuffer
         ,bytesAlreadyHandledInBuffer
         );

  return (currentOffsetInStream);
}

/* ----------------------------------
 * p_private_ffmpeg_get_next_frame
 * ----------------------------------
 * read one frame from the video_track (stream)
 * and decode the frame
 * when EOF reached: update total_frames and close the stream)
 * if recording of videoindex is enabled (in the handle)
 * then record positions of keyframes in the videoindex.
 * the flag do_copy_raw_chunk_data triggers copying the frame as raw data
 * chunk (useful for lossless video cut purpose)
 */
static t_GVA_RetCode
p_private_ffmpeg_get_next_frame(t_GVA_Handle *gvahand, gboolean do_copy_raw_chunk_data)
{
  static int ls_callNumber = 0;
  t_GVA_ffmpeg *handle;
  int       l_got_picture;
  int       l_rc;
  gint64    l_curr_url_offset;
  gint64    l_record_url_offset;
  gint32    l_url_seek_nr;
  gint32    l_len;
  gint32    l_pkt_size;
  guint16   l_frame_len;   /* 16 lower bits of the length */
  guint16   l_checksum;
  gboolean  l_potential_index_frame;
  gboolean  l_key_frame_detected;

  static gint32 funcId = -1;
  static gint32 funcIdReadPacket = -1;
  static gint32 funcIdDecode = -1;
  static gint32 funcIdSwScale = -1;
  
  GAP_TIMM_GET_FUNCTION_ID(funcId,           "p_private_ffmpeg_get_next_frame");
  GAP_TIMM_GET_FUNCTION_ID(funcIdReadPacket, "p_private_ffmpeg_get_next_frame.readAndDecodePacket");
  GAP_TIMM_GET_FUNCTION_ID(funcIdDecode,     "p_private_ffmpeg_get_next_frame.decode");
  GAP_TIMM_GET_FUNCTION_ID(funcIdSwScale,    "p_private_ffmpeg_get_next_frame.swScale");

  GAP_TIMM_START_FUNCTION(funcId);
  GAP_TIMM_START_FUNCTION(funcIdReadPacket);


  handle = (t_GVA_ffmpeg *)gvahand->decoder_handle;

  /* redirect picture_rgb pointers to current fcache element */
  avpicture_fill(handle->picture_rgb
                ,gvahand->frame_data
                ,PIX_FMT_RGB24      /* PIX_FMT_RGB24, PIX_FMT_RGBA32, PIX_FMT_BGRA32 */
                ,gvahand->width
                ,gvahand->height
                );

  ls_callNumber++;
  l_got_picture = 0;
  l_rc = 0;
  l_len = 0;
  l_frame_len = 0;
  l_pkt_size = 0;

  l_record_url_offset = -1;
  l_curr_url_offset = -1;
  l_url_seek_nr = gvahand->current_seek_nr;
  l_key_frame_detected = FALSE;
  handle->pkt1_dts = AV_NOPTS_VALUE;

  while(l_got_picture == 0)
  {
    int l_pktlen;

    /* if inbuf is empty we must read the next packet */
    if((handle->inbuf_len  < 1) || (handle->inbuf_ptr == NULL))
    {
      /* query current position in the file url_ftell
       * Note that url_ftell operates on the ByteIOContext  handle->vid_input_context->pb
       * and calculates correct file position by taking both the current (buffered) read position
       * and buffer position into account.
       */
      //l_curr_url_offset = p_url_ftell(handle->vid_input_context->pb);
      l_curr_url_offset = url_ftell(handle->vid_input_context->pb);

      /* if (gap_debug) printf("p_wrapper_ffmpeg_get_next_frame: before av_read_frame\n"); */
      /**
        * Read a packet from a media file
        * @param s media file handle
        * @param pkt is filled
        * @return 0 if OK. AVERROR_xxx(negative) if error.
        */
      l_pktlen = av_read_frame(handle->vid_input_context, &handle->vid_pkt);
      if(l_pktlen < 0)
      {
         if (l_pktlen == AVERROR_EOF)
         {
           /* EOF reached */
           if (gap_debug)
           {
             printf("get_next_frame: EOF reached"
                    "  (old)total_frames:%d current_frame_nr:%d all_frames_counted:%d\n"
                ,(int) gvahand->total_frames
                ,(int) gvahand->current_frame_nr
                ,(int) gvahand->all_frames_counted
                );
           }
         }
         else
         {
           /* EOF reached */
           if (gap_debug)
           {
             printf("get_next_frame: ERROR:%d (assuming EOF)"
                    "  (old)total_frames:%d current_frame_nr:%d all_frames_counted:%d\n"
                ,(int) l_pktlen
                ,(int) gvahand->total_frames
                ,(int) gvahand->current_frame_nr
                ,(int) gvahand->all_frames_counted
                );
           }
         }
         l_record_url_offset = -1;

         if (!gvahand->all_frames_counted)
         {
           gvahand->total_frames = gvahand->current_frame_nr;
           gvahand->all_frames_counted = TRUE;
         }
         if(handle->aud_stream_index >= 0)
         {
            /* calculate number of audio samples
             * (assuming that audio track has same duration as video track)
             */
            gvahand->total_aud_samples = (gint32)GVA_frame_2_samples(gvahand->framerate, gvahand->samplerate, gvahand->total_frames);
         }

         p_clear_inbuf_and_vid_packet(handle);

         l_rc = 1;
         break;
      }

      /* if (gap_debug)  printf("vid_stream:%d pkt.stream_index #%d, pkt.size: %d\n", handle->vid_stream_index, handle->vid_pkt.stream_index, handle->vid_pkt.size); */


      /* check if packet belongs to the selected video stream */
      if(handle->vid_pkt.stream_index != handle->vid_stream_index)
      {
         /* discard packet */
         /* if (gap_debug)  printf("DISCARD Packet\n"); */
         av_free_packet(&handle->vid_pkt);
         l_record_url_offset = -1;
         continue;
      }

      /* packet is part of the selected video stream, use that packet */
      handle->inbuf_ptr = handle->vid_pkt.data;
      handle->inbuf_len = handle->vid_pkt.size;

      l_pkt_size = handle->vid_pkt.size;
      if((handle->pkt1_dts == AV_NOPTS_VALUE) 
      && (handle->vid_pkt.dts != AV_NOPTS_VALUE))
      {
        handle->pkt1_dts = handle->vid_pkt.dts;
      }


      if(gap_debug)
      {  printf("using Packet data:%d size:%d  dts:%lld  pts:%lld  AV_NOPTS_VALUE:%lld\n"
                                 ,(int)handle->vid_pkt.data
                                 ,(int)handle->vid_pkt.size
                                 , handle->vid_pkt.dts
                                 , handle->vid_pkt.pts
                                 , AV_NOPTS_VALUE
                                 );
      }
    }

    if (gap_debug)
    {
      printf("before %s: inbuf_ptr:%d inbuf_len:%d  USING FFMPEG-%s\n"
             , PROCNAME_AVCODEC_DECODE_VIDEO
             , (int)handle->inbuf_ptr
             , (int)handle->inbuf_len
             , GAP_FFMPEG_VERSION_STRING
             );
    }


    /* --------- START potential CHUNK ----- */
    /* make a copy of the raw data packages for one video frame.
     * (we do not yet know if the raw data chunk is complete until
     *  avcodec_decode_video indicates a complete frame by setting the
     *  got_picture flag to TRUE)
     */
    if (do_copy_raw_chunk_data == TRUE)
    {
      if (handle->chunk_ptr != NULL)
      {
        g_free(handle->chunk_ptr);
        handle->chunk_ptr = NULL;
        handle->chunk_len = 0;
      }
      handle->chunk_len = handle->inbuf_len;
      if (handle->chunk_len > 0)
      {
        handle->chunk_ptr = g_malloc(handle->chunk_len);
        memcpy(handle->chunk_ptr, handle->inbuf_ptr, handle->chunk_len);
        if (gap_debug)
        {
          printf("copy potential raw chunk: chunk_ptr:%d chunk_len:%d\n",
                 (int)handle->chunk_ptr,
                 (int)handle->chunk_len);
        }
      }

    }
    /* --------- END potential CHUNK ----- */

    avcodec_get_frame_defaults(&handle->big_picture_yuv);

    GAP_TIMM_START_FUNCTION(funcIdDecode);

    /* decode a frame. return -1 on error, otherwise return the number of
     * bytes used. If no frame could be decompressed, *got_picture_ptr is
     * zero. Otherwise, it is non zero.
     */
#ifdef GAP_USES_OLD_FFMPEG_0_5
    l_len = avcodec_decode_video(handle->vid_codec_context  /* AVCodecContext * */
                               ,&handle->big_picture_yuv
                               ,&l_got_picture
                               ,handle->inbuf_ptr
                               ,handle->inbuf_len
                               );
#else
    l_len = avcodec_decode_video2(handle->vid_codec_context  /* AVCodecContext * */
                               ,&handle->big_picture_yuv
                               ,&l_got_picture
                               ,&handle->vid_pkt
                               );
#endif

    GAP_TIMM_STOP_FUNCTION(funcIdDecode);

    if (gap_debug) 
    {
      
      printf("get_next_frame(call:%d): "
             "curr_frame_nr:%d Pkt data:%d size:%d  dts:%lld pts:%lld l_len:%d got_pic:%d\n"
         ,(int)ls_callNumber
         ,(int) gvahand->current_frame_nr
         ,(int)handle->vid_pkt.data
         ,(int)handle->vid_pkt.size
         , handle->vid_pkt.dts
         , handle->vid_pkt.pts
         , (int)l_len
         , (int)l_got_picture
         );
    }

    if(handle->yuv_buff_pix_fmt != handle->vid_codec_context->pix_fmt)
    {
      if(gap_debug)
      {
        printf("$$ pix_fmt: old:%d new from codec:%d (PIX_FMT_YUV420P:%d PIX_FMT_YUV422P:%d)\n"
            , (int)handle->yuv_buff_pix_fmt
            , (int)handle->vid_codec_context->pix_fmt
            , (int)PIX_FMT_YUV420P
            , (int)PIX_FMT_YUV422P
            );
      }

      handle->yuv_buff_pix_fmt = handle->vid_codec_context->pix_fmt;   /* PIX_FMT_YUV420P */
      avpicture_fill(handle->picture_yuv
                ,handle->yuv_buffer
                ,handle->yuv_buff_pix_fmt
                ,gvahand->width
                ,gvahand->height
                );
    }

    if(l_len < 0)
    {
       if(handle->continueAfterReadErrors == FALSE)
       {
         printf("get_next_frame: %s returned ERROR configured to break at read errors)\n"
                ,PROCNAME_AVCODEC_DECODE_VIDEO
                );
         l_rc = 2;
         break;
       }
       printf("get_next_frame: %s returned ERROR, "
              "discarding packet dts:%lld and continueAfterReadErrors)\n"
           , PROCNAME_AVCODEC_DECODE_VIDEO
           , handle->vid_pkt.dts
           );
   
       handle->vid_pkt.size = 0;             /* set empty packet status */
       av_free_packet(&handle->vid_pkt);
       handle->inbuf_len = 0;
       handle->pkt1_dts = AV_NOPTS_VALUE;
       continue;
    }
    
#ifndef GAP_USES_OLD_FFMPEG_0_5
    /* fix for ffmpeg-0.6 MJPEG problem */
    if(l_len > 0)
    {
      /* since ffmpeg-0.6  the length (l_len) returned from avcodec_decode_video2
       * does not reliable indicate the number of handled input bytes 
       * (at least when mjpeg codec is used)
       * therefore assume that the decoded frame size is the full packet length
       * (note that packet is already read with av_read_frame and that shall already
       * collect all data for one frame in the packet,
       * The  case where multiple frames are included in one packet can not be handled after this fix
       * (i have no testvideos where this rare case occurs, and there is no way to implement
       * such a solution without reliable length information from the codec)
       */
      if(l_len != handle->inbuf_len)
      {
        if(gap_debug)
        {
          printf("WARNING: (call %d) current_frame_nr:%d decoded length:%d differs from packaet length:%d\n"
            ,(int)ls_callNumber
            ,(int)gvahand->current_frame_nr
            ,(int)l_len
            ,(int)handle->inbuf_len
            );
        }
        l_len = handle->inbuf_len;
      }
    }
#endif
    

    handle->inbuf_ptr += l_len;
    handle->inbuf_len -= l_len;
    if(l_got_picture)
    {
      l_frame_len = (l_len & 0xffff);
      l_record_url_offset = l_curr_url_offset;
      l_key_frame_detected = ((handle->vid_pkt.flags & AV_PKT_FLAG_KEY) != 0);

      if(gap_debug)
      {
        /* log information that could be relevant for redesign of VINDEX creation */
        printf("GOT PICTURE current_seek_nr:%06d   pp_prev_offset:%lld url_offset:%lld keyflag:%d dts:%lld dts1:%lld flen16:%d len:%d\n"
                  , (int)gvahand->current_seek_nr
                  , handle->prev_url_offset[MAX_PREV_OFFSET -1]
                  , l_record_url_offset
                  ,  (handle->vid_pkt.flags & AV_PKT_FLAG_KEY)
                  , handle->vid_pkt.dts
                  , handle->pkt1_dts
                  ,(int)l_frame_len
                  ,(int)l_len
                  );
      }
    }

    /* length of video packet was completely processed, we can free that packet now */
    if(handle->inbuf_len < 1)
    {
      /* if (gap_debug)  printf("FREE Packet\n"); */
      av_free_packet(&handle->vid_pkt);
      handle->vid_pkt.size = 0;             /* set empty packet status */
      handle->vid_pkt.data = NULL;          /* set empty packet status */
    }

  }  /* end while packet_read and decode frame loop */

  GAP_TIMM_STOP_FUNCTION(funcIdReadPacket);



  if((l_rc == 0)  && (l_got_picture))
  {
    GAP_TIMM_START_FUNCTION(funcIdSwScale);

    if(gvahand->current_seek_nr > 1)
    {
      /* 1.st frame_len may contain headers (size may be too large) */
      handle->max_frame_len = MAX(handle->max_frame_len, l_len);
    }

    handle->got_frame_length16 = l_frame_len;

    if(gap_debug)
    {
      printf("GOT PIC: current_seek_nr:%06d l_frame_len:%d got_pic:%d key:%d\n"
                       , (int)gvahand->current_seek_nr
                       , (int)handle->got_frame_length16
                     , (int)l_got_picture
                     , (int)handle->big_picture_yuv.key_frame
                     );
    }

    if(handle->dummy_read == FALSE)
    {
      /* reuse the img_convert_ctx or create a new one (in case ctx is NULL or params have changed)
       *
       */
      handle->img_convert_ctx = sws_getCachedContext(handle->img_convert_ctx
                                         , gvahand->width
                                         , gvahand->height
                                         , handle->yuv_buff_pix_fmt    /* src pixelformat */
                                         , gvahand->width
                                         , gvahand->height
                                         , PIX_FMT_RGB24               /* dest pixelformat */
                                         , SWS_BICUBIC                 /* int sws_flags */
                                         , NULL, NULL, NULL
                                         );
      if (handle->img_convert_ctx == NULL)
      {
         printf("Cannot initialize the conversion context (sws_getCachedContext delivered NULL pointer)\n");
         exit(1);
      }

      if (gap_debug)
      {
        printf("get_next_frame: before img_convert via sws_scale\n");
      }
      sws_scale(handle->img_convert_ctx
               , handle->picture_yuv->data      /* srcSlice */
               , handle->picture_yuv->linesize  /* srcStride the array containing the strides for each plane */
               , 0                              /* srcSliceY starting at 0 */
               , gvahand->height                /* srcSliceH the height of the source slice */
               , handle->picture_rgb->data      /* dst */
               , handle->picture_rgb->linesize  /* dstStride the array containing the strides for each plane */
               );

      if (gap_debug)
      {
        printf("get_next_frame: after img_convert via sws_scale\n");
      }

    }

#define GVA_LOW_GOP_LIMIT 24
#define GVA_HIGH_GOP_LIMIT 100


    l_potential_index_frame = FALSE;
    if((handle->big_picture_yuv.key_frame == 1) || (l_key_frame_detected == TRUE))
    {
      l_potential_index_frame = TRUE;
      handle->key_frame_detection_works = TRUE;
    }
    else
    {
      if(handle->key_frame_detection_works == FALSE)
      {
        if(l_url_seek_nr >= handle->prev_key_seek_nr + GVA_HIGH_GOP_LIMIT)
        {
          /* some codecs do not deliver key_frame information even if all frames are stored
           * as keyframes.
           * this would result in empty videoindexes for videoreads using such codecs
           * and seek ops would be done as sequential reads as if we had no videoindexes at all.
           * the GVA_HIGH_GOP_LIMIT  allows creation of videoindexes
           * to store offesets of frames that are NOT explicite marked as keyframes.
           * but never use non-keyframes for codecs with working key_frame detection.
           * (dont set GVA_HIGH_GOP_LIMIT lower than 100)
           */
          l_potential_index_frame = TRUE;
        }
      }
    }

    if(gap_debug)
    {
      if( (gvahand->vindex)
          && (handle->capture_offset)
          )
      {
        printf("KeyFrame: %d  works:%d\n"
          ,(int)handle->big_picture_yuv.key_frame
          ,(int)handle->key_frame_detection_works
          );
      }
    }

    if((gvahand->vindex)
    && (handle->capture_offset)
    && (l_potential_index_frame)
    )
    {
        l_checksum = p_gva_checksum(handle->picture_yuv, gvahand->height);

        /* the automatic GOP detection has a lower LIMIT of 24 frames
         * GOP values less than the limit can make the videoindex
         * slow, because the packet reads in libavformat are buffered,
         * and the index based search starting at the recorded seek offset (in the index)
         * may not find the wanted keyframe in the first Synchronisation Loop
         * if its 1.st packet starts before the recorded seek offset.
         * 24 frames shold be enough to catch an offest before the start of the wanted
         * packet.
         * Increasing the GVA_LOW_GOP_LIMIT would slow down the index based seek
         * ops.
         */

        /* printf("GUESS_GOP: prev_key_seek_nr:%d  l_url_seek_nr:%d\n"
         *       , (int)handle->prev_key_seek_nr
         *       , (int)l_url_seek_nr);
         */
        /* try to guess the GOP size (but use at least  GOPSIZE of 4 frames) */
        if(l_url_seek_nr > handle->prev_key_seek_nr)
        {
           gdouble l_gopsize;

           l_gopsize = l_url_seek_nr - handle->prev_key_seek_nr;
           if(handle->guess_gop_size == 0)
           {
             handle->guess_gop_size = l_gopsize;
           }
           else
           {
             handle->guess_gop_size = MAX(GVA_LOW_GOP_LIMIT, ((handle->guess_gop_size + l_gopsize) / 2));
           }
        }
        if (l_url_seek_nr >= handle->prev_key_seek_nr + GVA_LOW_GOP_LIMIT)
        {
            /* record the url_offset of 2 frames before. this is done because positioning to current
             * frame via url_fseek will typically take us to frame number +2
             */
            p_vindex_add_url_offest(gvahand
                         , handle
                         , gvahand->vindex
                         , l_url_seek_nr
                         , handle->prev_url_offset[MAX_PREV_OFFSET -1]
                         , handle->got_frame_length16
                         , l_checksum
                         , handle->vid_pkt.dts
                         );
            handle->prev_key_seek_nr = l_url_seek_nr;
        }

    }

    if(handle->capture_offset)
    {
        gint ii;

        /* save history of the last captured url_offsets in the handle
         */
        for(ii = MAX_PREV_OFFSET -1; ii > 0; ii--)
        {
          handle->prev_url_offset[ii] = handle->prev_url_offset[ii -1];
        }
        handle->prev_url_offset[0] = l_record_url_offset;
    }

    gvahand->current_frame_nr = gvahand->current_seek_nr;
    gvahand->current_seek_nr++;

    if (gap_debug) printf("get_next_frame: current_frame_nr :%d\n"
                         , (int)gvahand->current_frame_nr);

    /* if we found more frames, increase total_frames */
    gvahand->total_frames = MAX(gvahand->total_frames, gvahand->current_frame_nr);

    if(gap_debug)
    {
      printf("AVFORMAT: get_next_frame timecode: %lld current_frame_nr:%d\n"
                  , handle->vid_pkt.dts
                  , gvahand->current_frame_nr
                  );
    }
    GAP_TIMM_STOP_FUNCTION(funcIdSwScale);
    GAP_TIMM_STOP_FUNCTION(funcId);
    return(GVA_RET_OK);

  }


  GAP_TIMM_STOP_FUNCTION(funcId);

  if(l_rc == 1)
  {
    if(gap_debug)
    {
      printf("p_private_ffmpeg_get_next_frame: returning %d GVA_RET_EOF\n", (int)GVA_RET_EOF);
    } 
    return(GVA_RET_EOF);
  }

  if(gap_debug)
  {
    printf("p_private_ffmpeg_get_next_frame: returning %d GVA_RET_ERROR\n", (int)GVA_RET_ERROR);
  } 
  return(GVA_RET_ERROR);
}  /* end p_private_ffmpeg_get_next_frame */


/* ------------------------------
 * p_wrapper_ffmpeg_seek_frame
 * ------------------------------
 */
t_GVA_RetCode
p_wrapper_ffmpeg_seek_frame(t_GVA_Handle *gvahand, gdouble pos, t_GVA_PosUnit pos_unit)
{
  t_GVA_ffmpeg*  master_handle;

  master_handle = (t_GVA_ffmpeg*)gvahand->decoder_handle;

  if (master_handle->timecode_proberead_done != TRUE)
  {
    if(gvahand->vindex == NULL)
    {
      /* this video has no explicite gva index file.
       * try if native timecode bases seek is supported and reliable.
       */
      p_seek_timecode_reliability_self_test(gvahand);
    }
    else
    {
      /* this video has a gva index file.
       * but get analyse results (if availabe) that may contains the flag
       * (prefere_native_seek yes)
       * if this flag is set to yes we use native seek and ignore the valid videoindex.
       */
      if(gap_base_get_gimprc_gboolean_value(GIMPRC_PERSISTENT_ANALYSE, ANALYSE_DEFAULT))
      {
        p_get_video_analyse_results(gvahand);
      }
    }
  }

  return (p_seek_private(gvahand, pos, pos_unit));
}  /* end p_wrapper_ffmpeg_seek_frame */


/* -------------------------------------
 * p_reset_proberead_results
 * -------------------------------------
 */
static void p_reset_proberead_results(t_GVA_ffmpeg*  handle)
{
  handle->timecode_proberead_done = FALSE;
  handle->timecode_offset_frame1 = -1;
  handle->timecode_step_avg = -1;
  handle->timecode_step_abs_min = 0;
  handle->timecode_steps_sum = -1;
  handle->count_timecode_steps = 0;
  handle->native_timecode_seek_failcount = 0;
  handle->max_tries_native_seek = MAX_TRIES_NATIVE_SEEK;
  handle->self_test_detected_seek_bug = FALSE;
  handle->eof_timecode = AV_NOPTS_VALUE;
  handle->video_libavformat_seek_gopsize
    = gap_base_get_gimprc_int_value("video-libavformat-seek-gopsize", DEFAULT_NAT_SEEK_GOPSIZE, 0, MAX_NAT_SEEK_GOPSIZE);
}  /* end p_reset_proberead_results */


/* -------------------------------------
 * p_seek_timecode_reliability_self_test
 * -------------------------------------
 * this test analyzes the required settings
 * for native timebased seek.
 * further it tries to findout if this fast seek
 * method works (e.g. is able to perform frame exact positioning
 * that may fail for videofiles with variable frametiming
 * or have read errors).
 * Note that this check is no 100% guarantee but is able
 * to detetect such videos fast in most cases.
 *
 * this test also includes fast detection of total_frames (via seek).
 */
static gboolean
p_seek_timecode_reliability_self_test(t_GVA_Handle *gvahand)
{
  gdouble percent;
  gint32 bck_total_frames;
  t_GVA_ffmpeg*  master_handle;
  int gap_debug_local;
  gboolean persitent_analyse_available;


  gap_debug_local = gap_debug;

#ifdef GAP_DEBUG_FF_NATIVE_SEEK
  gap_debug_local = 1;
#endif /* GAP_DEBUG_FF_NATIVE_SEEK */

  master_handle = (t_GVA_ffmpeg*)gvahand->decoder_handle;

  persitent_analyse_available = FALSE;

  if(gap_base_get_gimprc_gboolean_value(GIMPRC_PERSISTENT_ANALYSE, ANALYSE_DEFAULT))
  {
    persitent_analyse_available =
      p_get_video_analyse_results(gvahand);
  }

  if(gap_debug_local)
  {
    printf("\n\n\n\n\n\n\n\n\n\n\n\n");
    if (persitent_analyse_available)
    {
      printf("SKIP p_seek_timecode_reliability_self_test PERSISTENT analyse available for file:%s\n"
        ,gvahand->filename
        );
    }
    else
    {
      printf("STARTING p_seek_timecode_reliability_self_test with probereads on file:%s\n"
        ,gvahand->filename
        );
    }
  }

  if(persitent_analyse_available != TRUE)
  {
    bck_total_frames = gvahand->total_frames;

    gvahand->cancel_operation = FALSE;   /* reset cancel flag */
    p_ffmpeg_vid_reopen_read(master_handle, gvahand);
    p_reset_proberead_results(master_handle);
    gvahand->current_frame_nr = 0;
    gvahand->current_seek_nr = 1;
    p_clear_inbuf_and_vid_packet(master_handle);

    /* limit native seek attempts to only one loop while running the selftest
     */
    master_handle->max_tries_native_seek = 1;


    /* perform some (few) sequential probe reads to analyze typical timecode
     * offsets and stepsize for this videfile.
     */
    p_probe_timecode_offset(gvahand);

    if ((master_handle->native_timecode_seek_failcount == 0)
    && (master_handle->video_libavformat_seek_gopsize > 0))
    {
      if(gap_debug_local)
      {
        printf("\nCONTINUE p_seek_timecode_reliability_self_test step: detect total_frames\n");
      }
      p_detect_total_frames_via_native_seek(gvahand);
      bck_total_frames = gvahand->total_frames;
    }

    if ((master_handle->native_timecode_seek_failcount == 0)
    && (master_handle->video_libavformat_seek_gopsize > 0))
    {
      if(gap_debug_local)
      {
        printf("\nCONTINUE p_seek_timecode_reliability_self_test step: perform seek tests\n");
      }

      /* now perform some (few!) seek operations with native timecode based
       * method and check if this method works reliable
       */
      for(percent = 0.9; percent > 0.1; percent-= 0.2)
      {
        gint32 framenr;
        t_GVA_RetCode l_rc_rd;

        framenr = gvahand->total_frames * percent;
        l_rc_rd = p_seek_native_timcode_based(gvahand, framenr);

        /* for releiable native seek both retcode and the
         * seek failcount must be OK.
         */
        if ((master_handle->native_timecode_seek_failcount > 0)
        || (l_rc_rd != GVA_RET_OK))
        {
          if (master_handle->native_timecode_seek_failcount == 0)
          {
            master_handle->self_test_detected_seek_bug = TRUE;
            printf("\n################################\n");
            printf("# ERROR on the video: %s\n", gvahand->filename);
            printf("# seek does not work.\n");
            printf("# possible reasons may be:\n");
            printf("#  - libavformat or gimp-gap api bug or\n");
            printf("#  - corrupted videofile\n");
            printf("# only slow sequential read will deliver frames\n");
            printf("#\n");
            printf("# Developer note: while testing this happened only with some MPEG1 videos\n");
            printf("# the bug seems to be a libavcodec/libavformat problem.\n");
            printf("################################\n");

          }

          /* disable native timecode seek in case of failures */
          master_handle->video_libavformat_seek_gopsize = 0;
          gvahand->total_frames = bck_total_frames;
          break;
        }
      }

    }


    if(gap_debug_local)
    {
      printf("\n\n");
      printf("\nDONE p_seek_timecode_reliability_self_test failcount:%d video_libavformat_seek_gopsize:%d"
        , (int)master_handle->native_timecode_seek_failcount
        , (int)master_handle->video_libavformat_seek_gopsize
        );
      if (master_handle->video_libavformat_seek_gopsize > 0)
      {
        printf(" (NATIVE seek ENABLED)");
      }
      else
      {
        printf(" (NATIVE seek ** DISABLED **)");
      }
      printf(" for file:%s\n", gvahand->filename);
      printf("\n\n\n\n\n");
    }

    if(gap_base_get_gimprc_gboolean_value(GIMPRC_PERSISTENT_ANALYSE, ANALYSE_DEFAULT))
    {
      p_save_video_analyse_results(gvahand);
      persitent_analyse_available = TRUE;
    }
    master_handle->max_tries_native_seek = MAX_TRIES_NATIVE_SEEK;

    p_ffmpeg_vid_reopen_read(master_handle, gvahand);
  }
  return (persitent_analyse_available);
}  /* end p_seek_timecode_reliability_self_test */


/* -------------------------------------
 * p_detect_total_frames_via_native_seek
 * -------------------------------------
 * findout total number of frames via native seek attempts
 * using binary search method.
 * further check if timecode based seek operates reliable
 * (native_timecode_seek_failcount must be 0)
 *
 * unfortunately i dont found a way to check if frame read attempts
 * failed due to read after EOF or failed due to errors.
 * some (typical but not all MPEG1 encoded) vidofiles deliver errors
 * when frames were read after any seek operations.
 * In such a case detection of total_frames is not possible
 * and furthermore native timecode based seek operations are not reliable
 * and must be disabled.
 *
 * it seems that libavformat sets the current timecode
 * to timecode after last frame
 * when seeking to positions after EOF (and in case of errors after native seek).
 * This (undocumented ?) effect enables quick detection of the toatal_frames.
 */
static void
p_detect_total_frames_via_native_seek(t_GVA_Handle *gvahand)
{
  gint32 frameNrLo;
  gint32 frameNrHi;
  gint32 frameNrMed;
  gint32 totalGuess;
  gint32 lastFrameNr;
  gint32 totalPlausible;
  gint32 totalDetected;
  t_GVA_RetCode l_rc;
  t_GVA_ffmpeg*  master_handle;
  int gap_debug_local;

  gap_debug_local = gap_debug;

#ifdef GAP_DEBUG_FF_NATIVE_SEEK
  gap_debug_local = 1;
#endif /* GAP_DEBUG_FF_NATIVE_SEEK */

  master_handle = (t_GVA_ffmpeg*)gvahand->decoder_handle;
  master_handle->eof_timecode = AV_NOPTS_VALUE;
  totalGuess = p_guess_total_frames(gvahand);
  lastFrameNr = totalGuess;

  totalPlausible = MAX((totalGuess / 4), 256);
  if(gap_debug_local)
  {
    printf("START: p_detect_total_frames_via_native_seek total_guess:%d\n"
      , (int)totalGuess
      );
  }

  /* first loop trys to seek after EOF until seek fails
   * seek to 150 % until seek fails
   */
  totalDetected = 0;
  frameNrLo = 1;
  frameNrHi = MAX(totalGuess, 256); /* start with guess value */
  while(1==1)
  {
    gdouble increase;
    increase = (gdouble)frameNrHi * 0.5;
    frameNrHi = frameNrHi + increase;
    l_rc = p_seek_native_timcode_based(gvahand, frameNrHi);
    if (l_rc != GVA_RET_OK)
    {
      if ((master_handle->eof_timecode != AV_NOPTS_VALUE)
      && (master_handle->eof_timecode > 0))
      {
        gdouble tolerance;
        /* probably already got eof_timecode from libavformat
         * therefore set Lo/Hi near that value to speed up the binary search.
         */
         lastFrameNr = p_timecode_to_frame_nr(master_handle, master_handle->eof_timecode) -1;
         tolerance = 32;
         frameNrLo = MAX(1, (lastFrameNr - tolerance));
         frameNrHi = lastFrameNr + tolerance;
         totalPlausible = frameNrLo;
      }
      break;
    }
    frameNrLo = MAX(frameNrLo, frameNrHi);
  }

  /* 2nd loop for binary search between Lo and Hi */
  while(1==1)
  {
    gdouble delta;

    delta = (gdouble)(frameNrHi - frameNrLo) / 2.0;
    frameNrMed = frameNrLo + delta;
    if(gap_debug_local)
    {
      printf("Lo:%d Hi:%d Med:%d delta:%.3f\n"
            ,(int)frameNrLo
            ,(int)frameNrHi
            ,(int)frameNrMed
            ,(float)delta
            );
    }
    if(delta < 1.0)
    {
       totalDetected = frameNrLo;
       gvahand->all_frames_counted = TRUE;
       break;
    }

    l_rc = p_seek_native_timcode_based(gvahand, frameNrMed);
    if (master_handle->native_timecode_seek_failcount > 0)
    {
      /* disable native timecode seek */
      master_handle->video_libavformat_seek_gopsize = 0;
      break;
    }
    if (l_rc != GVA_RET_OK)
    {
      frameNrHi = MIN(frameNrMed, frameNrHi);
    }
    else
    {
      frameNrLo = MAX(frameNrMed, frameNrLo);
    }

  }

  if(totalDetected >= totalPlausible)
  {
    l_rc = p_seek_native_timcode_based(gvahand, totalDetected);
    if (l_rc == GVA_RET_OK)
    {
      l_rc = p_wrapper_ffmpeg_get_next_frame(gvahand);
    }
    if (l_rc != GVA_RET_OK)
    {
      totalDetected--;
    }
    gvahand->total_frames = totalDetected;
    gvahand->all_frames_counted = TRUE;
  }
  else
  {
    /* disable native timecode seek
     * we typically get here for video files
     * where libavformat reads after seek always fail.
     */
    master_handle->video_libavformat_seek_gopsize = 0;
    gvahand->total_frames = lastFrameNr;
    gvahand->all_frames_counted = FALSE;
  }


  if(gap_debug_local)
  {
    printf("END: p_detect_total_frames_via_native_seek plausiblity limit:%d detected:%d total_frames:%d \n"
       , (int)totalPlausible
       , (int)totalDetected
       , (int)gvahand->total_frames
       );
  }
}  /* end p_detect_total_frames_via_native_seek */



/* ------------------------------------
 * p_inc_native_timecode_seek_failcount
 * ------------------------------------
 */
static void
p_inc_native_timecode_seek_failcount(t_GVA_Handle *gvahand)
{
  t_GVA_ffmpeg*  master_handle;

  master_handle = (t_GVA_ffmpeg*)gvahand->decoder_handle;
  if (master_handle->native_timecode_seek_failcount <= 0)
  {
    printf("\n##############################\n");
    printf("# WARNING video %s probably has variable frame timing\n"
          , gvahand->filename
          );
    printf("#  positioning to exact framenumber is not guaranteed\n");
    printf("#  you may create a videoindex to enable positioning to exact frame numbers\n");
    printf("##############################\n");

  }
  master_handle->native_timecode_seek_failcount++;
}  /* end p_inc_native_timecode_seek_failcount */

/* ------------------------------------
 * p_clear_inbuf_and_vid_packet
 * ------------------------------------
 */
static void
p_clear_inbuf_and_vid_packet(t_GVA_ffmpeg *handle)
{
  av_free_packet(&handle->vid_pkt);
  handle->vid_pkt.size = 0;             /* set empty packet status */
  handle->vid_pkt.data = NULL;          /* set empty packet status */
  handle->inbuf_len = 0;
  handle->inbuf_ptr = NULL;
}  /* end p_clear_inbuf_and_vid_packet */




/* ------------------------------
 * p_seek_private
 * ------------------------------
 * this procedure seeks to specified position.
 * one of 3 alternative methods will be used.
 *  - Native timebased seek via av_seek_frame
 *     This method is fastest, but may not be available for some videofile formats.
 *     This method may deliver
 *     wrong results in case the video has unrelible timecode,
 *     or has frames with individual duration, or streams with
 *     different framerates.
 *     Note that GAP usually works framenumber oriented, therefore
 *     native timebased seeking may delvier wrong results, depending on the videofile.
 *     native seek by timestamps can be generally disabled by setting gimprc parameter
 *     (video-libavformat-seek-gopsize "0")
 *     Note that native seek will also be disabled automatically, if the self test
 *     detects that native seek does not work properly on the current video.
 *
 *  - The videoindex is an external solution outside libavformat
 *    and enables seek in the videofile in case where native seek is not available.
 *    The videoindex is slower than native but much faster than sequential reads.
 *    a videoindex has stored offsets of the keyframes
 *    that were optional created by the count_frames procedure.
 *    The videoindex based seek modifies the libavformat internal straem seek position
 *    by an explicte call seek call (either to byte position or to timecode target)
 *    The index also has information about the (compressed) frame length and a checksum.
 *    In the inner Sync Loop(s) we try to find exactly the frame that is described in the index.
 *    The outer Loop (l_nloops) repeats the sync loop attempt by using a lower seek offset
 *    from the previous video index entry and uses more read operations (l_synctries)
 *    trying to find the target frame. (This is necessary because byte oriented
 *    seek operations do not work exact and may position the video stream
 *    AFTER the target frame.
 *
 *    The postioning to last Group of pictures is done
 *    by sequential read (l_readsteps). it should contain at least one keyframe (I-frame)
 *    that enables clean decoding of the following P and B frames
 *
 *                        video index table                              inner (synctries) and outer loop
 *
 *                       |-------------------------------------------|
 *                       | [10] seek_nr:000100  offset: len16:  DTS: |
 *                       |-------------------------------------------|
 *
 *
 *                                                                                     (only in case 1st attempt failed
 *                                                                                      to seek the video stream
 *                                                                                      to exact target position)
 *                       |-------------------------------------------|                 l_nloops = 2
 *                       | [11] seek_nr:000110  offset: len16:  DTS: |                      |  1  l_synctries
 *                       |-------------------------------------------|                      |  2
 *                                                                                          |  3
 *                                                                                          |  4
 *                                                                                          |  5
 *                                                                                          |  6
 *                                                                                          |  7
 *                                                                                          |  8
 *                                                                                          |  9
 *                       |-------------------------------------------|  l_nloops = 1        |  10
 *  l_idx_target ------> | [12] seek_nr:000120  offset: len16:  DTS: |   |  1 l_synctries   |  11
 *                       |-------------------------------------------|   |  2               |  12
 *                                                                       |  3               |  13
 *                                                                       |  4               |  14
 *                                                                       |  5               |  15
 *                                                                       |  6               |  16
 *                                                                       |  7               |  17
 *                                                                       |  8               |  18
 *                       |-------------------------------------------|   |  9               |  19
 *  l_idx_too_far -----> | [13] seek_nr:000130  offset: len16:  DTS: |   V  10              V  20
 *                       |-------------------------------------------|
 *
 *
 *  - the fallback method emulates
 *    seek by pos-1 sequential read ops.
 *    seek backwards must always reopen to reset stream position to start.
 *    this is very slow, creating a videoindex is recommanded for GUI-based applications
 *
 *    seek operation to the start (the first few frames) always use sequential read
 *
 *
 */
#define GVA_IDX_SYNC_STEPSIZE 1
#define GVA_FRAME_NEAR_START 24
static t_GVA_RetCode
p_seek_private(t_GVA_Handle *gvahand, gdouble pos, t_GVA_PosUnit pos_unit)
{
  t_GVA_ffmpeg *handle;
  t_GVA_RetCode l_rc_rd;
  gint32   l_frame_pos;
  gint32   l_url_frame_pos;
  gint32   l_readsteps;
  gint32   l_summary_read_ops;
  gdouble  l_progress_step;
  gboolean l_vindex_is_usable;
  t_GVA_Videoindex    *vindex;

  handle = (t_GVA_ffmpeg *)gvahand->decoder_handle;
  vindex = gvahand->vindex;

  gvahand->cancel_operation = FALSE;   /* reset cancel flag */

  switch(pos_unit)
  {
    case GVA_UPOS_FRAMES:
      l_frame_pos = (gint32)pos;
      break;
    case GVA_UPOS_SECS:
      l_frame_pos = (gint32)round(pos * gvahand->framerate);
      break;
    case GVA_UPOS_PRECENTAGE:
      /* is not reliable until all_frames_counted == TRUE */
      l_frame_pos = (gint32)GVA_percent_2_frame(gvahand->total_frames, pos);
      break;
    default:
      l_frame_pos = (gint32)pos;
      break;
  }

  if(gvahand->all_frames_counted)
  {
    /* in case total number of frames is exactly known
     * constraint seek position
     */
    l_frame_pos = MIN(l_frame_pos, gvahand->total_frames);
  }

  gvahand->percentage_done = 0.0;

  if(gap_debug)
  {
    printf("p_wrapper_ffmpeg_seek_frame: start: l_frame_pos: %d cur_seek:%d cur_frame:%d  (prefere_native:%d gopsize:%d) video:%s\n"
                           , (int)l_frame_pos
                           , (int)gvahand->current_seek_nr
                           , (int)gvahand->current_frame_nr
                           , (int)handle->prefere_native_seek
                           , (int)handle->video_libavformat_seek_gopsize
                           , gvahand->filename
                           );
  }


  handle->dummy_read = TRUE;
  l_url_frame_pos = 0;
  l_vindex_is_usable = FALSE;
  l_summary_read_ops = 0;

  if(l_frame_pos > GVA_FRAME_NEAR_START)
  {
    /* check if we have a usable video index */
    if(vindex != NULL)
    {
       if(vindex->tabsize_used > 0)
       {
         gint32   l_max_seek;  /* seek to frames above this numer via video index will be slow */

         l_max_seek = vindex->ofs_tab[vindex->tabsize_used-1].seek_nr + (2 * vindex->stepsize);
         if(l_frame_pos <= l_max_seek)
         {
           l_vindex_is_usable = TRUE;
           if(gap_debug)
           {
              printf("VINDEX flag l_vindex_is_usable is TRUE l_max_seek:%d (vindex->tabsize_used:%d)\n"
                   , (int)l_max_seek
                   , (int)vindex->tabsize_used
                   );
           }
         }
         else
         {
            if(gap_debug)
            {
              p_debug_print_videoindex(vindex);
            }
            printf("WARNING VINDEX flag l_vindex_is_usable is FALSE, the video Index is NOT COMPLETE ! for video:%s\n"
                   " l_frame_pos:%d l_max_seek:%d (vindex->tabsize_used:%d) total_frames:%d\n"
                   , gvahand->filename
                   , (int)l_frame_pos
                   , (int)l_max_seek
                   , (int)vindex->tabsize_used
                   , (int)gvahand->total_frames
                   );
         }
       }
    }


    /* check conditions and try native timecode based seek if conditions permit native seek.
     */
    if((l_vindex_is_usable != TRUE) || (handle->prefere_native_seek))
    {
      /* try native av_seek_frame if enabled by gimprc configuration
       * (native seek is disabled if video_libavformat_seek_gopsize == 0)
       */
      if (handle->video_libavformat_seek_gopsize > 0)
      {
        l_rc_rd = p_seek_native_timcode_based(gvahand, l_frame_pos);
        if(l_rc_rd == GVA_RET_OK)
        {
          if(gap_debug)
          {
            printf("NATIVE SEEK performed for videofile:%s\n"
              , gvahand->filename
              );
          }
          return (l_rc_rd);
        }
      }
    }

  }

  /* use video index where possible */
  if((vindex != NULL) && (l_frame_pos > GVA_FRAME_NEAR_START))
  {
     gint64  seek_pos;
     gint32  l_idx;

     if(gap_debug)
     {
       //p_debug_print_videoindex(vindex);
       printf("VIDEO INDEX is available for videofile:%s vindex->tabsize_used:%d\n"
         , gvahand->filename
         , (int)vindex->tabsize_used
         );
     }


     if(vindex->tabsize_used < 1)
     {
       printf("SEEK: index is >>> EMPTY <<<: vindex->tabsize_used: %d cannot use index!\n", (int)vindex->tabsize_used);
     }
     else
     {
       l_idx = ((l_frame_pos -2) / vindex->stepsize);

       /* make sure that table access limited to used tablesize
        * (this allows usage of incomplete indexes)
        * Note that the last index entry is not used,
        * because positioning after the last keyframe does not work properly
        */
       if(l_idx > vindex->tabsize_used -1)
       {
         if(gap_debug)
         {
           printf("SEEK: overflow l_idx: %d limit:%d\n"
                         , (int)l_idx
                         , (int) vindex->tabsize_used -1
                         );
         }
         l_idx = vindex->tabsize_used -1;
       }

       /* lower index adjustment (dont start seek with positions much smaller than l_frame_pos) */
       if(l_idx > 0)
       {
         gint32 l_pos_min;

         l_pos_min = l_frame_pos - (MAX(64, (2* vindex->stepsize)));

         while(vindex->ofs_tab[l_idx].seek_nr < l_pos_min)
         {
           l_idx++;
           if(l_idx > vindex->tabsize_used -1)
           {
             l_idx = vindex->tabsize_used -1;
             printf("WARNING: videoindex is NOT complete ! Seek to frame numers > seek_nr:%d will be VERY SLOW!\n"
                           , (int)vindex->ofs_tab[l_idx].seek_nr
                           );
             break;
           }
           if(gap_debug)
           {
             printf("SEEK: lower idx adjustment l_idx: %d l_pos_min:%d seek_nr[%d]:%d\n"
                           , (int)l_idx
                           , (int)l_pos_min
                           , (int)vindex->ofs_tab[l_idx].seek_nr
                           , (int)l_idx
                           );
           }
         }
       }

       /* upper index adjustment (dont start seek with positions already graeter than l_frame_pos)*/
       if(l_idx > 0)
       {
         while(vindex->ofs_tab[l_idx].seek_nr >= l_frame_pos)
         {
           l_idx--;
           if(gap_debug)
           {
             printf("SEEK: upper idx adjustment l_idx: %d l_frame_pos:%d seek_nr[%d]:%d\n\n"
                           , (int)l_idx
                           , (int)l_frame_pos
                           , (int)vindex->ofs_tab[l_idx].seek_nr
                           , (int)l_idx
                           );
           }
           if(l_idx < 1)
           {
             break;
           }
         }
       }

       if(l_idx > 0)
       {
         gint32  l_idx_target;
         gint32  l_idx_too_far;    /* next index position after the target
                                    * when this position is reached we are already too far
                                    * and must retry with a lower position at next attempt
                                    */
         gboolean l_target_found;

         l_target_found = FALSE;
         l_idx_target = l_idx;
         l_idx_too_far = l_idx_target + 1;
         if (l_idx_too_far > vindex->tabsize_used -1)
         {
           l_idx_too_far = -1;  /* mark as invalid */
         }

         l_readsteps = l_frame_pos - gvahand->current_seek_nr;
         if((l_readsteps > vindex->stepsize)
         || (l_readsteps < 0))
         {
           gint32  l_nloops;     /* number of seek attempts with different recorded videoindex entries
                                  * (outer loop)
                                  */
           gint32  l_synctries;  /* number of frame read attempts until
                                  * the frame read from video stream matches
                                  * the ident data of the recorded frame in the videoindex[l_idx]
                                  * (inner loop counter)
                                  */

           l_nloops = 1;
           while((l_idx >= 0) && (l_nloops < 12))
           {
             gboolean l_dts_timecode_usable;

             l_dts_timecode_usable = FALSE;

             if((vindex->ofs_tab[l_idx].timecode_dts != AV_NOPTS_VALUE)
             && (l_nloops == 1)
             && (vindex->ofs_tab[l_idx_target].timecode_dts != AV_NOPTS_VALUE)
             && (vindex->ofs_tab[0].timecode_dts != AV_NOPTS_VALUE))
             {
               l_dts_timecode_usable = TRUE;
             }

             if(gap_debug)
             {
               printf("SEEK: USING_INDEX: ofs_tab[%d]: ofs64: %lld seek_nr:%d flen:%d chk:%d dts:%lld DTS_USABLE:%d NLOOPS:%d\n"
               , (int)l_idx
               , vindex->ofs_tab[l_idx].uni.offset_gint64
               , (int)vindex->ofs_tab[l_idx].seek_nr
               , (int)vindex->ofs_tab[l_idx].frame_length
               , (int)vindex->ofs_tab[l_idx].checksum
               , vindex->ofs_tab[l_idx].timecode_dts
               , l_dts_timecode_usable
               , (int)l_nloops
               );
             }

             l_synctries = 4 + MAX_PREV_OFFSET + (vindex->stepsize * GVA_IDX_SYNC_STEPSIZE * l_nloops);

             /* SYNC READ loop
              * seek to offest found in the index table
              * then read until we are at the KEYFRAME that matches
              * in length and checksum with the one from the index table
              *
              * Note that Byte offest based seek is not frame exact. Seek may take us
              * to positions after the wanted framenumber.
              * (therefore we make more attempts l_nloops > 1 with previous index entries)
              *
              * Timecode based seek shall take us to the wanted frame already in the 1st attempt
              * but will fail (probably in all attempts) when the video has corrupted timecodes.
              * therefore switch seek strategy fo byte offset based seek in further attempts (l_nloops != 1)
              *
              * for some videofiles dts timecodes are available but not for all frames.
              * for those videos seek via dts does NOT work.
              * such videos are marked with AV_NOPTS_VALUE in entry at index 0
              * (vindex->ofs_tab[0].timecode_dts == AV_NOPTS_VALUE)
              *
              */
             p_ffmpeg_vid_reopen_read(handle, gvahand);
             if(l_dts_timecode_usable)
             {
               int      ret_av_seek_frame;

               l_synctries = 1;

               if(gap_debug)
               {
                 printf("USING DTS timecode based av_seek_frame\n");
               }

               /* timecode based seek */
               ret_av_seek_frame = av_seek_frame(handle->vid_input_context, handle->vid_stream_index
                   , vindex->ofs_tab[l_idx].timecode_dts, AVSEEK_FLAG_BACKWARD);
             }
             else
             {
               int      ret_av_seek_frame;
               /* seek based on url_fseek (for support of old video indexes without timecode) */
               seek_pos = vindex->ofs_tab[l_idx].uni.offset_gint64;

               /* byte position based seek  AVSEEK_FLAG_BACKWARD AVSEEK_FLAG_ANY*/
               ret_av_seek_frame = av_seek_frame(handle->vid_input_context, handle->vid_stream_index
                    ,seek_pos, AVSEEK_FLAG_BACKWARD | AVSEEK_FLAG_BYTE);

             }
             p_clear_inbuf_and_vid_packet(handle);

             gvahand->current_seek_nr = vindex->ofs_tab[vindex->tabsize_used].uni.offset_gint64;

             while(l_synctries > 0)
             {
               gboolean l_potentialCanditate;

               l_potentialCanditate = FALSE;
               l_rc_rd = p_wrapper_ffmpeg_get_next_frame(gvahand);
               l_summary_read_ops++;

               if(l_rc_rd != GVA_RET_OK)
               {
                 l_synctries = -1;
                 if(gap_debug)
                 {
                   printf("EOF or ERROR while SEEK_SYNC_LOOP\n");
                 }
                 break;
               }

//
//               printf("SEEK_SYNC_LOOP: idx_frame_len:%d  got_frame_length16:%d  Key:%d dts:%lld\n"
//                    , (int)vindex->ofs_tab[l_idx_target].frame_length
//                    , (int)handle->got_frame_length16
//                    , (int)handle->big_picture_yuv.key_frame
//                    , handle->vid_pkt.dts
//                    );

               if(l_dts_timecode_usable == TRUE)
               {
                 /* handling for index format with usable timecode
                  * for videos where DTS timecodes are consistent
                  * the 1st seek attempt shall take us already to the wanted position.
                  * (an additional length check would fail in this situation due to the buggy
                  * length information)
                  */
                 if(TRUE == p_areTimecodesEqualWithTolerance(vindex->ofs_tab[l_idx_target].timecode_dts, handle->vid_pkt.dts))
                 {
                   l_potentialCanditate = TRUE;
                 }
               }
               else
               {
                 /* handling for index without usable timecode */
                 if(vindex->ofs_tab[l_idx_target].frame_length == handle->got_frame_length16)
                 {
                   l_potentialCanditate = TRUE;
                 }
               }

               if (l_potentialCanditate == TRUE)
               {
                 guint16 l_checksum;

                 l_checksum = p_gva_checksum(handle->picture_yuv, gvahand->height);
                 if(l_checksum == vindex->ofs_tab[l_idx_target].checksum)
                 {
                   /* we have found the wanted (key) frame */
                   l_target_found = TRUE;
                   break;
                 }
                 else if ((vindex->ofs_tab[l_idx_target].timecode_dts != AV_NOPTS_VALUE)
                      &&  (TRUE == p_areTimecodesEqualWithTolerance(vindex->ofs_tab[l_idx_target].timecode_dts, handle->vid_pkt.dts)))
                 {
                   /* ignore the checksum missmatch when DTS timecode is matching
                    */
                   if(gap_debug)
                   {
                     printf("SEEK_SYNC_LOOP: CHKSUM_MISSMATCH: CHECKSUM[%d]:%d  l_checksum:%d but DTS equal:%lld \n"
                     , (int)l_idx_target
                     , (int)vindex->ofs_tab[l_idx_target].checksum
                     , (int)l_checksum
                     , handle->vid_pkt.dts
                     );
                   }
                   l_target_found = TRUE;
                   break;
                 }
                 else
                 {
                   /* checksum missmatch is non critical in most cases.
                    * a) there may be frames where the packed length is equal to the
                    *    wanted frame (NOT CRITICAL)
                    * b) after seek to a non-keyframe and subsequent reads
                    *    the resulting frame may be the wanted frame but is corrupted
                    *    (due to some missing delta information that was not yet read)
                    *    in this case the frame delivers wrong checksum. (CRITICAL)
                    * this code assumes case a) and
                    * continues searching for an exactly matching frame
                    * but also respects case b) in the next outer loop attempt.
                    * (with l_nloops == 2 where we start from a lower seek position)
                    * this shall increase the chance to perfectly decode the frame
                    * and deliver the correct checksum.
                    */
                   if(gap_debug)
                   {
                     printf("SEEK_SYNC_LOOP: CHKSUM_MISSMATCH: CHECKSUM[%d]:%d  l_checksum:%d\n"
                     , (int)l_idx_target
                     , (int)vindex->ofs_tab[l_idx_target].checksum
                     , (int)l_checksum
                     );
                   }
                 }
               }
               else
               {
                  if (l_idx_too_far > 0)  /* check for valid index */
                  {
                    /* Check if we are already AFTER the wanted position */
                    if(vindex->ofs_tab[l_idx_too_far].frame_length == handle->got_frame_length16)
                    {
                      guint16 l_checksum;

                      l_checksum = p_gva_checksum(handle->picture_yuv, gvahand->height);
                      if(l_checksum == vindex->ofs_tab[l_idx_too_far].checksum)
                      {
                        l_synctries = -1;
                        if(gap_debug)
                        {
                          printf("SEEK_SYNC_LOOP position is already after wanted target\n");
                        }
                        break;
                      }
                    }
                  }
               }

               l_synctries--;
             }                     /* end while */


             if(l_target_found != TRUE)
             {
               /* index sync search failed, try with previous index in the table */
               if(l_idx < GVA_IDX_SYNC_STEPSIZE)
               {
                 /* index sync search failed, must search slow from start */
                 p_ffmpeg_vid_reopen_read(handle, gvahand);
                 gvahand->current_seek_nr = 1;
                 l_url_frame_pos = 0;
               }
             }
             else
             {
               /* index sync search had success */

               l_url_frame_pos = 1 + vindex->ofs_tab[l_idx_target].seek_nr;
               if(gap_debug)
               {
                 printf("SEEK: url_fseek seek_pos: %d  l_idx_target:%d l_url_frame_pos:%d\n"
                                        , (int)seek_pos
                                        , (int)l_idx_target
                                        , (int)l_url_frame_pos
                                        );
               }
               gvahand->current_seek_nr = l_url_frame_pos;
               l_readsteps = l_frame_pos - gvahand->current_seek_nr;
               if(gap_debug)
               {
                 printf("SEEK: sync loop success remaining l_readsteps: %d\n", (int)l_readsteps);
               }

               break;  /* OK, escape from outer loop, we are now at read position of the wanted keyframe */
             }

             if(l_dts_timecode_usable == TRUE)
             {
               /* if seek via dts timecode failed, switch to byte offset based seek
                * but this must be restarted with the same index [l_idx]
                * (we must not decrement l_idx because timecode based seek has only checked one frame.
                * if the wanted frame is in the search range of current l_idx it will never be found
                * if we advance at this point, which results in very slow sequential search afterwards)
                */
               l_dts_timecode_usable = FALSE;
             }
             else
             {
               /* try another search with previous index table entry */
               l_idx -= GVA_IDX_SYNC_STEPSIZE;
             }
             l_nloops++;
           }                 /* end outer loop (l_nloops) */

           if (l_target_found != TRUE)
           {
             p_ffmpeg_vid_reopen_read(handle, gvahand);
             gvahand->current_seek_nr = 1;
             l_url_frame_pos = 0;
             if(gap_debug)
             {
                printf("SEEK: target frame not found, fallback to slow sequential read from begin\n");
             }

           }

         }
       }
     }
  }


  /* fallback to sequential read */

  if(l_url_frame_pos == 0)
  {
    /* emulate seek with N steps of read ops */
    if((l_frame_pos < gvahand->current_seek_nr)   /* position backwards ? */
    || (gvahand->current_seek_nr < 0))            /* inplausible current position ? */
    {
      /* reopen and read from start */
      p_ffmpeg_vid_reopen_read(handle, gvahand);
      l_readsteps = l_frame_pos -1;
    }
    else
    {
      /* continue read from current position */
      l_readsteps = l_frame_pos - gvahand->current_seek_nr;
    }
  }

  if(gap_debug)
  {
    printf("p_wrapper_ffmpeg_seek_frame: l_readsteps: %d\n", (int)l_readsteps);
  }

  l_progress_step =  1.0 / MAX((gdouble)l_readsteps, 1.0);
  l_rc_rd = GVA_RET_OK;
  while((l_readsteps > 0)
  &&    (l_rc_rd == GVA_RET_OK))
  {
    l_rc_rd = p_wrapper_ffmpeg_get_next_frame(gvahand);
    l_summary_read_ops++;
    gvahand->percentage_done = CLAMP(gvahand->percentage_done + l_progress_step, 0.0, 1.0);

    if(gvahand->do_gimp_progress) {  gimp_progress_update (gvahand->percentage_done); }
    if(gvahand->fptr_progress_callback)
    {
      gvahand->cancel_operation = (*gvahand->fptr_progress_callback)(gvahand->percentage_done, gvahand->progress_cb_user_data);
    }

    l_readsteps--;

    if(gvahand->cancel_operation)
    {
       l_rc_rd = GVA_RET_ERROR;             /* cancel by user request reults in seek error */
       gvahand->cancel_operation = FALSE;   /* reset cancel flag */
    }
  }
  handle->dummy_read = FALSE;

  if(l_rc_rd == GVA_RET_OK)
  {
    if(gap_debug)
    {
       printf("p_wrapper_ffmpeg_seek_frame: SEEK OK: l_frame_pos: %d cur_seek:%d cur_frame:%d l_summary_read_ops:%d video:%s\n"
                            , (int)l_frame_pos
                            , (int)gvahand->current_seek_nr
                            , (int)gvahand->current_frame_nr
                            , (int)l_summary_read_ops
                            , gvahand->filename
                            );
    }

    gvahand->current_seek_nr = (gint32)l_frame_pos;

    return(GVA_RET_OK);
  }

  if(l_rc_rd == GVA_RET_EOF)  { return(GVA_RET_EOF); }

  return(GVA_RET_ERROR);
}  /* end p_seek_private */


/* ------------------------------
 * p_seek_native_timcode_based
 * ------------------------------
 * positioning of the video stream before target_frame position.
 *
 * the native timecode based seek operates on
 * timecode steps sample pattern that will be measured
 * by some initial probereads (p_probe_timecode_offset).
 *
 * Most Videofiles have either constant timing (stepsize equal for all frames)
 * or use a repeating pattern of individual stepsizes within a group of pictures.
 * In both cases this procedure can calculate the correct timecode that
 * is required for frame exact postioning.
 *
 * Some videos use variable timecode all over the video. For such videos
 * where no typical pattern can be detected, the native timecodebased seek
 * is not possible.
 * Some videoformats do not allow seek at all.
 * (all attempts to read a frame after seek operations fail for
 * those videofiles)
 *
 *
 * returns GVA_RET_OK if positionig was successful
 *         GVA_RET_ERROR if positioning failed
 *         note: this procedure reopens the handle after errors
 *         to allow clean sequential reads afterwards.
 * futher results are stored in the decoder specific handle (part of gvahand)
 *    handle->video_libavformat_seek_gopsize
 *            (is automatically increased when necessary)
 *    handle->eof_timecode
 *            (is set when detected the 1st time)
 *    handle->handle->native_timecode_seek_failcount
 *            (is increased when variable frametiming is detected
 *             or when frame could be read but timcode overflow occured
 *             at the last try with the highest pre_read setting)
 */
static t_GVA_RetCode
p_seek_native_timcode_based(t_GVA_Handle *gvahand, gint32 target_frame)
{
  t_GVA_ffmpeg *handle;
  t_GVA_RetCode l_retcode;
  t_GVA_RetCode l_rc_rd;
  gint32   l_readsteps;
  gdouble  l_progress_step;
  gint32   l_pre_read_frames;
  gint     l_tries;
  gboolean l_retry;

#define NATIVE_SEEK_GOP_INCREASE 24
#define NATIVE_SEEK_EXTRA_PREREADS_NEAR_END 22

  handle = (t_GVA_ffmpeg *)gvahand->decoder_handle;

  l_retcode = GVA_RET_ERROR;
  if (handle->video_libavformat_seek_gopsize <= 0)
  {
      return(l_retcode);
  }

  l_retry = TRUE;
  for(l_tries=0; l_tries < handle->max_tries_native_seek; l_tries++)
  {
    if (l_retry != TRUE)
    {
      return(l_retcode);
    }

    l_pre_read_frames =
       MAX(4,
          (handle->video_libavformat_seek_gopsize + (l_tries * NATIVE_SEEK_GOP_INCREASE))
          );

#ifdef GAP_DEBUG_FF_NATIVE_SEEK
    {
      printf("TRY (%d) native seek with l_pre_read_frames:%d total_frames:%d target:%d\n"
        ,(int)l_tries
        ,(int)l_pre_read_frames
        ,(int)gvahand->total_frames
        ,(int)target_frame
        );
    }
#endif /* GAP_DEBUG_FF_NATIVE_SEEK */



    if (target_frame > l_pre_read_frames)
    {
      int      ret_av_seek_frame;
      int      jj;
      int64_t  l_seek_timecode;
      gint32   seek_fnr;
      gint32   seek_fnr2;


      ret_av_seek_frame = -1;
      p_probe_timecode_offset(gvahand);

      /* determine seek timecode.
       * this is done in 3 attempts and selects the smallest l_seek_timecode.
       * (videos that have negative timecode steps
       * may pick a timecode too high when using only one attempt.
       * this can lead to seek after the wanted position or after EOF)
       * example:
       * video timcode sequence with negative stepsizes may look like this:
       *   seek_fnr:000557; seek_timecode:49306;
       *   seek_fnr:000558; seek_timecode:45557;
       *   seek_fnr:000559; seek_timecode:45558;
       *
       * in this situation it is required to pick seek_timecode:45557;
       */
      seek_fnr = target_frame - l_pre_read_frames;
      l_seek_timecode = p_frame_nr_to_timecode(handle, seek_fnr);

      seek_fnr2 = seek_fnr;
      for(jj=0; jj < 2; jj++)
      {
        int64_t  l_seek_timecode2;
        seek_fnr2++;
        l_seek_timecode2 = p_frame_nr_to_timecode(handle, seek_fnr2);
        if(l_seek_timecode2 < l_seek_timecode)
        {
          seek_fnr = seek_fnr2;
          l_seek_timecode = l_seek_timecode2;
        }
      }

      /* SEEK to preread position */
      gvahand->current_frame_nr = 0;
      gvahand->current_seek_nr = 1;

      if(1==1 /*l_tries == 0*/)
      {
        /* seek variant based on stream specific dts timecode
         */
        /* seek is done some (configurable number) frames less than wanted frame position.
         * it shall deliver position of nearest key frame
         * after seek, have to read/decode some few frames until
         * wanted frame is reached
         * in some rare cases (some older mpeg files) libavformat positioning may take us
         * to non-keyframes (even with AVSEEK_FLAG_BACKWARD set).
         * therefore read/decode some frames (l_pre_read_frames) shall ensure that
         * a keyframe is included, and enable proper decoding of the subsequent frames.
         */
        ret_av_seek_frame = av_seek_frame(handle->vid_input_context, handle->vid_stream_index
                   , l_seek_timecode, AVSEEK_FLAG_BACKWARD);
      }
      else
      {
        /* seek variant based on global (not stream specific) pts timecode
         */
        int64_t  target_pts;
        gdouble  secs;

        target_pts =
          av_rescale_q(l_seek_timecode, handle->vid_stream->time_base, AV_TIME_BASE_Q);

        ret_av_seek_frame = av_seek_frame(handle->vid_input_context, -1
                   , target_pts, AVSEEK_FLAG_BACKWARD);
#ifdef GAP_DEBUG_FF_NATIVE_SEEK
        printf("PTS-SEEK: framerate:%.3f target_pts:%lld l_seek_timecode:%lld seek_fnr:%d\n"
           , gvahand->framerate
           , target_pts
           , l_seek_timecode
           , (int)seek_fnr
              );
#endif /* GAP_DEBUG_FF_NATIVE_SEEK */
      }


#ifdef GAP_DEBUG_FF_NATIVE_SEEK
      if(1==0)
      {
        int64_t seek_target;
        int64_t target_pts;
        int64_t microsecs_start;
        gdouble secs;
        gdouble dframes;
        gint32 seek_fnr;

        seek_fnr = target_frame - l_pre_read_frames;
        secs = (gdouble)seek_fnr / gvahand->framerate;
        target_pts = secs * AV_TIME_BASE;
        microsecs_start = handle->vid_input_context->start_time * handle->vid_stream->time_base.num
                          / handle->vid_stream->time_base.den;

        seek_target= av_rescale_q(target_pts, AV_TIME_BASE_Q, handle->vid_stream->time_base);
        dframes = handle->vid_input_context->duration / (gdouble)AV_TIME_BASE * gvahand->framerate;

        printf("VINFOt: framerate:%.3f target_pts:%lld seek_target:%lld mstart:%lld seek_timecode:%lld seek_fnr:%d\n"
           , gvahand->framerate
           , target_pts
           , seek_target
           , microsecs_start
           , l_seek_timecode
           , (int)seek_fnr
           );


      }


      if(gap_debug)
      {
        printf("AVFORMAT NATIVE-SEEK: av_seek_frame retcode: %d seek_timecode:%lld seek_frame:%ld (target frame_pos:%ld)\n"
           , ret_av_seek_frame
           , l_seek_timecode
           , (long)(target_frame - l_pre_read_frames)
           , (long)target_frame
           );
      }
#endif /* GAP_DEBUG_FF_NATIVE_SEEK */

      /* clear inbuf and packet
       *(must remove old content after seek to enable clean reads)
       */
      p_clear_inbuf_and_vid_packet(handle);

      if (ret_av_seek_frame < 0)
      {
        printf("** ERROR AVFORMAT: av_seek_frame failed retcode: %d \n"
               , ret_av_seek_frame);
      }
      else
      {
         int64_t  l_wanted_timecode;  /* timecode one frame before target */
         int64_t  l_want_2_timecode;  /* timecode 2 frames before target */
         int64_t  l_want_3_timecode;  /* timecode 3 frames before target */
         int64_t  l_overflow_timecode;  /* timecode 2 frames after target */
         int64_t  l_very_small_absdiff;
         gint     l_ii;
         gint     l_read_err_count;
         int64_t  l_prev_timecode;
         int64_t  l_pprev_timecode;
         gchar    l_debug_msg[500];

         l_wanted_timecode = p_frame_nr_to_timecode(handle, target_frame -1);
         l_want_2_timecode = p_frame_nr_to_timecode(handle, target_frame -2);
         l_want_3_timecode = p_frame_nr_to_timecode(handle, target_frame -3);
         l_overflow_timecode = MAX(l_wanted_timecode, p_frame_nr_to_timecode(handle, target_frame));
         l_overflow_timecode = MAX(l_overflow_timecode, p_frame_nr_to_timecode(handle, target_frame +1));
         l_overflow_timecode = MAX(l_overflow_timecode, p_frame_nr_to_timecode(handle, target_frame +2));
         l_very_small_absdiff = CLAMP((handle->timecode_step_abs_min / 200), 0, 10);

         l_prev_timecode = AV_NOPTS_VALUE;
         l_pprev_timecode = AV_NOPTS_VALUE;
         l_read_err_count = 0;

         l_progress_step =  1.0 / MAX((gdouble)l_pre_read_frames, 1.0);
         gvahand->percentage_done = 0.0;
         /* native seek has (hopefully) set stream position to the nearest keyframe
          * (before the wanted frame)
          * ??? For some video formats the native seek may set position AFTER the wanted
          *     frame in such a case it is worth to start a retry with larger
          *     l_pre_read_frames value.
          * now read/decode frames until wanted timecode position -1 is reached
          * Note: use pkt->dts because pkt->pts can be AV_NOPTS_VALUE if the video format
          * has B frames, so it is better to rely on pkt->dts
          */
         for (l_ii=0; l_ii <= MAX_NAT_SEEK_GOPSIZE; l_ii++)
         {
           int64_t l_curr_timecode;
           t_GVA_RetCode l_rc_timeread;

#ifdef GAP_DEBUG_FF_NATIVE_SEEK
           if(gap_debug)
           {
             printf(" [%d.%02d] START progress/cancel handler\n"
               , l_tries
               , l_ii
               );
             fflush(stdout);
           }
#endif /* GAP_DEBUG_FF_NATIVE_SEEK */

           /* progress and cancel handling */
           gvahand->percentage_done = CLAMP(gvahand->percentage_done + l_progress_step, 0.0, 1.0);

           if((l_ii % 3) == 0)
           {
             if(gvahand->do_gimp_progress) {  gimp_progress_update (gvahand->percentage_done); }
             if((gvahand->fptr_progress_callback)
             && (gvahand->progress_cb_user_data))
             {
               gvahand->cancel_operation = (*gvahand->fptr_progress_callback)(gvahand->percentage_done, gvahand->progress_cb_user_data);
             }
           }

           if(gvahand->cancel_operation)
           {
              l_retcode = GVA_RET_ERROR;           /* cancel by user request reults in seek error */
              l_retry = FALSE;                     /* no more attempts due to cancel */
              gvahand->cancel_operation = FALSE;   /* reset cancel flag */
              break;
           }

#ifdef GAP_DEBUG_FF_NATIVE_SEEK
           if(gap_debug)
           {
             printf(" [%d.%02d] END   progress/cancel handler\n"
               , l_tries
               , l_ii
               );
             fflush(stdout);
           }
#endif /* GAP_DEBUG_FF_NATIVE_SEEK */

           /* make a guess to set current_seek_nr while doing timebased frame serach loop */
           gvahand->current_seek_nr = (gint32)target_frame -1;

           /* READ FRAME from video stream at current position */
           handle->dummy_read = TRUE;

#ifdef GAP_DEBUG_FF_NATIVE_SEEK
           if(gap_debug)
           {
             gint64 url_pos;
             url_pos = url_ftell(handle->vid_input_context->pb);
             printf(" [%d.%02d] START nat-seek READ FRAME url_pos:%lld\n"
                , l_tries
                , l_ii
                , url_pos
                );
             fflush(stdout);
           }
#endif /* GAP_DEBUG_FF_NATIVE_SEEK */

           l_rc_timeread = p_wrapper_ffmpeg_get_next_frame(gvahand);
           l_curr_timecode = handle->vid_pkt.dts;

#ifdef GAP_DEBUG_FF_NATIVE_SEEK
           {
             gint64 url_pos;
             url_pos = url_ftell(handle->vid_input_context->pb);
             printf(" [%d.%02d] END   nat-seek READ FRAME url_pos:%lld curr_timecode:%lld rc:%d\n"
                 , l_tries
                 , l_ii
                 , url_pos
                 , l_curr_timecode
                 , l_rc_timeread
                 );
             fflush(stdout);
           }
#endif /* GAP_DEBUG_FF_NATIVE_SEEK */
           handle->dummy_read = FALSE;

           if(l_rc_timeread != GVA_RET_OK)
           {
             gint32 l_fnr;

             l_fnr = p_timecode_to_frame_nr(handle, l_curr_timecode);
             if(handle->eof_timecode == AV_NOPTS_VALUE)
             {
               handle->eof_timecode = l_curr_timecode;
             }

             l_read_err_count++;

#ifdef GAP_DEBUG_FF_NATIVE_SEEK
             printf("** ERROR AVFORMAT: (%d) errcount:%d timbased seek failed to read frame after native seek rc:%d\n"
                    " * target_frame:%d cur_timecode:%lld, l_fnr:%d, total_frames:%d, all_counted:%d\n"
                , (int)l_ii
                , (int)l_read_err_count
                , (int)l_rc_timeread
                , (int)target_frame
                , l_curr_timecode
                , (int)l_fnr
                , (int)gvahand->total_frames
                , (int)gvahand->all_frames_counted
                );
#endif /* GAP_DEBUG_FF_NATIVE_SEEK */

             if (1==1)  /* (l_read_err_count > 1) */
             {
               l_retry = FALSE;
             }
             else
             {
               l_pprev_timecode = l_prev_timecode;
               l_prev_timecode = AV_NOPTS_VALUE;
             }
             break;

           }

           l_debug_msg[0] = "\n";
           l_debug_msg[1] = "\0";
#ifdef GAP_DEBUG_FF_NATIVE_SEEK
           {
             g_snprintf(&l_debug_msg[0], sizeof(l_debug_msg)
                  , "AVFORMAT: try(%d) readcount(%d) debug read frame timecode: %lld "
                    "(wanted framenr:%d timecode:%lld)\n"
                  , l_tries
                  , l_ii
                  , l_curr_timecode
                  , target_frame -1
                  , l_wanted_timecode
                  );
           }
#endif /* GAP_DEBUG_FF_NATIVE_SEEK */

           /* tolerant check if wanted timecode is reached
            * it is sufficient if 2 out of the last 3 timecodes read do match.
            */
           {
              gint match_count;
              gint must_match;
              gchar debug_match_string[4];
              int64_t l_curr_absdiff;
              int64_t l_prev_absdiff;
              int64_t l_pprev_absdiff;


              match_count = 0;
              must_match = 2;
              debug_match_string[0] = '-';
              debug_match_string[1] = '-';
              debug_match_string[2] = '-';
              debug_match_string[3] = '\0';

              l_curr_absdiff  = llabs(l_curr_timecode  - l_wanted_timecode);
              l_prev_absdiff  = llabs(l_prev_timecode  - l_want_2_timecode);
              l_pprev_absdiff = llabs(l_pprev_timecode - l_want_3_timecode);


              if (l_curr_absdiff <= l_very_small_absdiff)
              {
                debug_match_string[0] = 'x';
                match_count++;
              }
              if (l_prev_absdiff <= l_very_small_absdiff)
              {
                debug_match_string[1] = 'x';
                match_count++;
              }
              if (l_pprev_absdiff <= l_very_small_absdiff)
              {
                debug_match_string[2] = 'x';
                match_count++;
              }

#ifdef GAP_DEBUG_FF_NATIVE_SEEK
              {
                printf("    match_string:[%s]\n"
                      , debug_match_string
                      );
                fflush(stdout);
              }
#endif /* GAP_DEBUG_FF_NATIVE_SEEK */

              if(match_count >= must_match)
              {
                /* set attributes of gvahand to refelect wanted seek position reached. */
                gvahand->current_seek_nr = target_frame;
                gvahand->current_frame_nr = target_frame -1;

                /* if this is not the first try ( l_tries > 0)
                 * increase video_libavformat_seek_gopsize
                 * to the value where we had success with.
                 * (the next seek operation may be successful
                 * in the first attempt based on this value)
                 */
                handle->video_libavformat_seek_gopsize +=
                   (l_tries * NATIVE_SEEK_GOP_INCREASE);

#ifdef GAP_DEBUG_FF_NATIVE_SEEK
                {
                  printf("%sABSDIFF: match_string:[%s] curr_absdiff: %lld prev_absdiff:%lld pprev_absdiff:%lld very_small:%d\n\n"
                     , l_debug_msg
                     , debug_match_string
                     , l_curr_absdiff
                     , l_prev_absdiff
                     , l_pprev_absdiff
                     , l_very_small_absdiff
                     );
                  printf("p_wrapper_ffmpeg_seek_frame: NATIVE SEEK OK: target_frame: %d cur_seek:%d cur_frame:%d seek_gopsize:%d\n"
                     , (int)target_frame
                     , (int)gvahand->current_seek_nr
                     , (int)gvahand->current_frame_nr
                     , (int)handle->video_libavformat_seek_gopsize
                     );
                }
#endif /* GAP_DEBUG_FF_NATIVE_SEEK */
                return (GVA_RET_OK);
              }
           } /* END of tolerant check if wanted timecode is reached */


           /* timecode overflow check */
           if ((l_curr_timecode != AV_NOPTS_VALUE)
           && (l_curr_timecode > l_overflow_timecode))
           {
               /* if the video has variable timing it is possible that
                * the wanted timecode can not be found, in such a case we get an overflow
                * when reading frames with higher timecode than the wanted timecode.
                * (check for overflow compares timecode greater than 2 frames after the
                *  wanted frame, because some videos have time code step patterns
                *  that include negative stepsizes)
                *
                * The overflow may also happen on seek attempts after the last
                * frame of the video.
                */
               if (l_tries >= handle->max_tries_native_seek -1)
               {
                 l_retry = FALSE;
                 p_inc_native_timecode_seek_failcount(gvahand);
               }
               if(gap_debug)
               {
                 printf("%s**   Timecode OVERFLOW: curr: %lld oflow:%lld  prev:%lld pprev:%lld (wanted:%lld)\n"
                  , l_debug_msg
                  , l_curr_timecode
                  , l_overflow_timecode
                  , l_prev_timecode
                  , l_pprev_timecode
                  , l_wanted_timecode
                  );
               }
               l_retcode = GVA_RET_ERROR;
               break;
           }

           l_pprev_timecode = l_prev_timecode;
           l_prev_timecode = l_curr_timecode;


         }

      }

      if (l_retry != TRUE)
      {
        printf("** AVFORMAT: native seek failed (seek behind EOF, Seek Error, or Read error)\n");
      }

#ifdef GAP_DEBUG_FF_NATIVE_SEEK
      {
        printf("** now reopening (reset seek position for next try or alternative seek)\n");
      }
#endif /* GAP_DEBUG_FF_NATIVE_SEEK */

      p_ffmpeg_vid_reopen_read(handle, gvahand);
      handle = (t_GVA_ffmpeg *)gvahand->decoder_handle;
      gvahand->current_frame_nr = 0;
      gvahand->current_seek_nr = 1;
    }


  }  /* end for tries loop */

  return (l_retcode);

}  /* end p_seek_native_timcode_based */


/* ------------------------------
 * p_wrapper_ffmpeg_seek_audio
 * ------------------------------
 * this procedure just sets the current_sample position
 * for access of the samples_buffer
 */
t_GVA_RetCode
p_wrapper_ffmpeg_seek_audio(t_GVA_Handle *gvahand, gdouble pos, t_GVA_PosUnit pos_unit)
{
  t_GVA_ffmpeg *handle;
  gint32   l_sample_pos;
  gint32   l_frame_pos;

  handle = (t_GVA_ffmpeg *)gvahand->decoder_handle;

  switch(pos_unit)
  {
    case GVA_UPOS_FRAMES:
      l_sample_pos = (gint32)GVA_frame_2_samples(gvahand->framerate, gvahand->samplerate, pos -1.0);
      break;
    case GVA_UPOS_SECS:
      l_sample_pos = (gint32)round(pos / MAX(gvahand->samplerate, 1.0));
      break;
    case GVA_UPOS_PRECENTAGE:
      l_frame_pos= (gint32)GVA_percent_2_frame(gvahand->total_frames, pos);
      l_sample_pos = (gint32)GVA_frame_2_samples(gvahand->framerate, gvahand->samplerate, l_frame_pos -1);
      break;
    default:
      l_sample_pos = 0;
      break;
  }


  if(gap_debug) printf("p_wrapper_ffmpeg_seek_audio: l_sample_pos:%d\n", (int)l_sample_pos);

  gvahand->current_sample = (gint32)l_sample_pos;
  return(GVA_RET_OK);

}  /* end p_wrapper_ffmpeg_seek_audio */


/* ------------------------------
 * p_wrapper_ffmpeg_get_audio
 * ------------------------------
 */
t_GVA_RetCode
p_wrapper_ffmpeg_get_audio(t_GVA_Handle *gvahand
             ,gint16 *output_i
             ,gint32 channel
             ,gdouble samples
             ,t_GVA_AudPos mode_flag
             )
{
  t_GVA_ffmpeg *handle;
  int      l_rc;
  gint32   l_sample_idx;
  gint32   l_low_sample_idx;
  gint32   l_samples_picked;

  handle = (t_GVA_ffmpeg *)gvahand->decoder_handle;


  if(mode_flag == GVA_AMOD_FRAME)
  {
      p_wrapper_ffmpeg_seek_audio(gvahand
                                    , (gdouble) gvahand->current_frame_nr
                                    , GVA_UPOS_FRAMES
                                    );
  }

  if(mode_flag == GVA_AMOD_REREAD)
  {
    l_sample_idx = gvahand->reread_sample_pos;
  }
  else
  {
    gvahand->reread_sample_pos = gvahand->current_sample;  /* store current position for reread operations */
    l_sample_idx = gvahand->current_sample;
  }

  if(gap_debug) 
  {
    printf("p_wrapper_ffmpeg_get_audio  samples: %d l_sample_idx:%d\n", (int)samples, (int)l_sample_idx);
  }

  l_low_sample_idx = 0;
  if (handle->samples_base[0] > 0)
  {
    l_low_sample_idx = handle->samples_base[0];
  }
  if ((handle->samples_base[1] > 0)
  &&  (handle->samples_base[1] < l_low_sample_idx))
  {
    l_low_sample_idx = handle->samples_base[1];
  }

  if(l_sample_idx < l_low_sample_idx)
  {
    /* the requested sample_idx is to old
     * (is not chached in the sample_buffers any more)
     * we have to reset audio read, and restart reading from the begin
     */
    if(gap_debug)
    {
      printf("ffmpeg_get_audio l_sample_idx:%d  l_low_sample_idx:%d (calling reopen)\n"
        , (int)l_sample_idx
        , (int)l_low_sample_idx
	);
    }
    p_ffmpeg_aud_reopen_read(handle, gvahand);
  }

  /* read and decode audio packets into sample_buffer */
  l_rc = p_read_audio_packets(handle, gvahand, (gdouble)(l_sample_idx + samples));

  /* copy desired range number of samples for the desired channel
   * from sample_buffer to output_i (16bit) sampledata
   */
  l_samples_picked = p_pick_channel( handle
                , gvahand
                , output_i
                , l_sample_idx
                , samples
                , channel
                );

  if(gap_debug) 
  {
    printf("p_wrapper_ffmpeg_get_audio  DONE: l_rc:%d\n", (int)l_rc);
  }

  if(l_rc == 0)
  {
    if(mode_flag != GVA_AMOD_REREAD)
    {
       gvahand->current_sample += l_samples_picked;        /* advance current audio read position */
    }
    return(GVA_RET_OK);
  }
  if(l_rc == 1)  { return(GVA_RET_EOF); }

  return(GVA_RET_ERROR);
}  /* end p_wrapper_ffmpeg_get_audio */



/* ----------------------------------
 * p_wrapper_ffmpeg_count_frames
 * ----------------------------------
 * (re)open a separated handle for counting
 * to ensure that stream positions are not affected by the count.
 * This procedure optionally creates a videoindex
 * for faster seek access
 */
t_GVA_RetCode
p_wrapper_ffmpeg_count_frames(t_GVA_Handle *gvahand)
{
  t_GVA_ffmpeg*  master_handle;
  t_GVA_ffmpeg*  handle;
  t_GVA_RetCode  l_rc;
  t_GVA_Handle   *copy_gvahand;
  gint32         l_total_frames;
  t_GVA_Videoindex    *vindex;
  gboolean persitent_analyse_available;

  if(gap_debug)
  {
    printf("p_wrapper_ffmpeg_count_frames: START\n");
  }
  gvahand->percentage_done = 0.0;
  persitent_analyse_available = FALSE;

  if(gap_base_get_gimprc_gboolean_value(GIMPRC_PERSISTENT_ANALYSE, ANALYSE_DEFAULT))
  {
      persitent_analyse_available = p_seek_timecode_reliability_self_test(gvahand);
      gvahand->percentage_done = 0.0;
  }

  if(gvahand->vindex == NULL)
  {
    gvahand->vindex = GVA_load_videoindex(gvahand->filename, gvahand->vid_track, GVA_FFMPEG_DECODER_NAME);
  }

  if(gvahand->vindex)
  {
     if((gvahand->vindex->total_frames > 0)
     && (gvahand->vindex->track == gvahand->vid_track))
     {
       /* we already have a valid videoindex that contains the real total number */
       if(gap_debug)
       {
         printf("p_wrapper_ffmpeg_count_frames: have already valid vindex\n");
       }
       gvahand->total_frames = gvahand->vindex->total_frames;
       gvahand->all_frames_counted = TRUE;
       return(GVA_RET_OK);
     }
     /* throw away existing videoindex (because it is unusable) */
     if(gap_debug)
     {
       printf("p_wrapper_ffmpeg_count_frames: throw away existing videoindex\n");
     }
     GVA_free_videoindex(&gvahand->vindex);
  }

  vindex = NULL;
  if(gvahand->create_vindex)
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

  copy_gvahand = g_malloc0(sizeof(t_GVA_Handle));
  if(gap_debug)
  {
    printf("p_wrapper_ffmpeg_count_frames: ## before open ## handle:%s\n", gvahand->filename);
  }
  p_wrapper_ffmpeg_open_read(gvahand->filename
                            ,copy_gvahand
                            );
  if(gap_debug)
  {
    printf("p_wrapper_ffmpeg_count_frames: after p_wrapper_ffmpeg_open_read\n");
  }
  copy_gvahand->current_frame_nr = 0;
  copy_gvahand->current_seek_nr = 1;
  copy_gvahand->vindex = vindex;
  handle = (t_GVA_ffmpeg*)copy_gvahand->decoder_handle;
  master_handle = (t_GVA_ffmpeg*)gvahand->decoder_handle;

  if(handle)
  {
    /* TIMCODE LOG INIT */
    FILE *fp_timecode_log;
    fp_timecode_log = p_init_timecode_log(gvahand);

    if(vindex)
    {
      handle->capture_offset = TRUE;
    }
    handle->dummy_read = TRUE;

    /* percentage_done is just a guess, because we dont know
     * the exact total_frames number before.
     */
    if(gvahand->all_frames_counted)
    {
       l_total_frames = gvahand->total_frames;
    }
    else
    {
       /* we need a guess to update the percentage_done used for progress indication)
        */
       l_total_frames = p_guess_total_frames(gvahand);
    }
    if(gap_debug)
    {
      printf("p_wrapper_ffmpeg_count_frames: begin Counting\n");
    }
    gvahand->frame_counter = 0;
    while(!gvahand->cancel_operation)
    {
       /* READ FRAME */
       l_rc = p_wrapper_ffmpeg_get_next_frame(copy_gvahand);
       if(gap_debug)
       {
         printf("p_wrapper_ffmpeg_count_frames: get_next l_rc:%d\n", (int)l_rc);
       }
       if(l_rc != GVA_RET_OK)
       {
         break;  /* eof, or fetch error */
       }
       gvahand->frame_counter++;
       l_total_frames = MAX(l_total_frames, gvahand->frame_counter);


       /* TIMCODE check and LOG WHILE COUNTING */
       p_timecode_check_and_log(fp_timecode_log, gvahand->frame_counter, handle, master_handle, gvahand);

       if(gvahand->all_frames_counted)
       {
         gvahand->percentage_done = CLAMP(((gdouble)gvahand->frame_counter / (gdouble)l_total_frames), 0.0, 1.0);
       }
       else
       {
         /* assume we have 5 % frames more to go than total_frames */
         gvahand->percentage_done = CLAMP(((gdouble)gvahand->frame_counter / MAX(((gdouble)l_total_frames * 1.05),1)), 0.0, 1.0);
         gvahand->total_frames = l_total_frames;
       }

       if(gvahand->do_gimp_progress) {  gimp_progress_update (gvahand->percentage_done); }
       if(gvahand->fptr_progress_callback)
       {
         gvahand->cancel_operation = (*gvahand->fptr_progress_callback)(gvahand->percentage_done, gvahand->progress_cb_user_data);
       }

    }
    if(gap_debug)
    {
      printf("p_wrapper_ffmpeg_count_frames: stop Counting\n");
    }

    vindex->stepsize = handle->guess_gop_size;
    handle->capture_offset = FALSE;

    /* the copy_gvahand has used reference to the orinal gvahand->vindex
     * this reference must be set NULL before the close is done
     * (otherwise p_wrapper_ffmpeg_close would free the vindex)
     */
    copy_gvahand->vindex = NULL;

    /* close the extra handle (that was opened for counting only) */
    p_wrapper_ffmpeg_close(copy_gvahand);


    /* TIMCODE LOG FINALLY */
    if (fp_timecode_log)
    {
      fclose(fp_timecode_log);
    }


  }

  g_free(copy_gvahand);

  gvahand->percentage_done = 0.0;

  if(vindex)
  {
    if(gap_debug)
    {
      printf("p_wrapper_ffmpeg_count_frames: before GVA_save_videoindex\n");
      printf("p_wrapper_ffmpeg_count_frames: gvahand->filename:%s\n", gvahand->filename);
    }
    vindex->total_frames = gvahand->frame_counter;
    if(gvahand->cancel_operation)
    {
      /* because of cancel the total_frames is still unknown
       * (vindex->total_frames == 0 is the indicator for incomplete index)
       */
      vindex->total_frames = 0;
    }

    if(gap_debug)
    {
      printf("p_wrapper_ffmpeg_count_frames: before GVA_save_videoindex\n");
    }
    GVA_save_videoindex(vindex, gvahand->filename, GVA_FFMPEG_DECODER_NAME);


    if(gap_debug)
    {
      GVA_debug_print_videoindex(gvahand);
    }
  }

  if(!gvahand->cancel_operation)
  {
     gvahand->total_frames = gvahand->frame_counter;
     gvahand->all_frames_counted = TRUE;

     master_handle->all_timecodes_verified = TRUE;
  }
  else
  {
     gvahand->total_frames = MAX(gvahand->total_frames, gvahand->frame_counter);
     gvahand->cancel_operation = FALSE;  /* reset cancel flag */

     if(gap_debug)
     {
       printf("p_wrapper_ffmpeg_count_frames: RETURN ERROR\n");
     }
     return(GVA_RET_ERROR);
  }

  if ((master_handle->critical_timecodesteps_found == FALSE)
  && (master_handle->all_timecodes_verified == TRUE))
  {
    // TODO: the video index creator plug-in may offer
    // an option wheter prefere_native_seek set to TRUE or stay FALSE
    // when all timecodesteps are verified as OK.
    // on the other hand the faster native seek is to prefere
    // at this point, where the full frame scan has verified all timecodes.
    master_handle->prefere_native_seek = TRUE;
  }

  if(gap_debug)
  {
    printf("VINDEX done, critical_timecodesteps_found:%d\n"
           "             master_handle->all_timecodes_verified %d\n"
           "             master_handle->prefere_native_seek %d\n"
           "             gvahand->frame_counter: %d\n"
           "             gvahand->all_frames_counted: %d\n"
           "             gvahand->cancel_operation: %d\n"
           "             gvahand->total_frames: %d\n"
      , (int)master_handle->critical_timecodesteps_found
      , (int)master_handle->all_timecodes_verified
      , (int)master_handle->prefere_native_seek
      , (int)gvahand->frame_counter
      , (int)gvahand->all_frames_counted
      , (int)gvahand->cancel_operation
      , (int)gvahand->total_frames
      );
  }


  if ((master_handle->critical_timecodesteps_found)
  || (master_handle->all_timecodes_verified))
  {
    if(persitent_analyse_available)
    {
      /* update analyse result file */
      p_save_video_analyse_results(gvahand);
    }
  }

  if(gap_debug)
  {
    printf("p_wrapper_ffmpeg_count_frames: RETURN OK\n");
  }

  return(GVA_RET_OK);

}  /* end p_wrapper_ffmpeg_count_frames */


/* ----------------------------------
 * p_wrapper_ffmpeg_seek_support
 * ----------------------------------
 * NOTE: if native seek is disabled via gimprc setting "video-libavformat-seek-gopsize" 0
 * the self test is not performed (seek bugs are not detected in that case)
 */
t_GVA_SeekSupport
p_wrapper_ffmpeg_seek_support(t_GVA_Handle *gvahand)
{
  t_GVA_ffmpeg*  master_handle;

  master_handle = (t_GVA_ffmpeg*)gvahand->decoder_handle;

  if ((master_handle->timecode_proberead_done != TRUE)
  && (master_handle->video_libavformat_seek_gopsize > 0))
  {
    p_seek_timecode_reliability_self_test(gvahand);
  }

  if (master_handle->self_test_detected_seek_bug == TRUE)
  {
    return(GVA_SEEKSUPP_NONE);
  }

  if (master_handle->video_libavformat_seek_gopsize > 0)
  {
    return(GVA_SEEKSUPP_NATIVE);
  }

  return(GVA_SEEKSUPP_VINDEX);

}  /* end p_wrapper_ffmpeg_seek_support */



/* -----------------------------
 * p_ffmpeg_new_dec_elem
 * -----------------------------
 * create a new decoder element and init with
 * functionpointers referencing the QUICKTIME
 * specific Procedures
 */
t_GVA_DecoderElem  *
p_ffmpeg_new_dec_elem(void)
{
  t_GVA_DecoderElem  *dec_elem;

  dec_elem = g_malloc0(sizeof(t_GVA_DecoderElem));
  if(dec_elem)
  {
    dec_elem->decoder_name         = g_strdup(GVA_FFMPEG_DECODER_NAME);
    dec_elem->decoder_description  = g_strdup("FFMPEG Decoder (Decoder for MPEG1,MPEG4(DivX),RealVideo) (EXT: .mpg,.vob,.avi,.rm,.mpeg)");
    dec_elem->fptr_check_sig       = &p_wrapper_ffmpeg_check_sig;
    dec_elem->fptr_open_read       = &p_wrapper_ffmpeg_open_read;
    dec_elem->fptr_close           = &p_wrapper_ffmpeg_close;
    dec_elem->fptr_get_next_frame  = &p_wrapper_ffmpeg_get_next_frame;
    dec_elem->fptr_seek_frame      = &p_wrapper_ffmpeg_seek_frame;
    dec_elem->fptr_seek_audio      = &p_wrapper_ffmpeg_seek_audio;
    dec_elem->fptr_get_audio       = &p_wrapper_ffmpeg_get_audio;
    dec_elem->fptr_count_frames    = &p_wrapper_ffmpeg_count_frames;
    dec_elem->fptr_seek_support    = &p_wrapper_ffmpeg_seek_support;
    dec_elem->fptr_get_video_chunk = &p_wrapper_ffmpeg_get_video_chunk;
    dec_elem->fptr_get_codec_name  = &p_wrapper_ffmpeg_get_codec_name;
    dec_elem->next = NULL;
  }

  return (dec_elem);
}  /* end p_ffmpeg_new_dec_elem */




/* ===================================
 * private (static) helper  PROCEDURES
 * ===================================
 * ===================================
 */


static void
p_debug_codec_list(void)
{
  AVCodec *codec;

  for(codec = av_codec_next(NULL); codec != NULL; codec = av_codec_next(codec))
  {
     printf("\n-------------------------\n");
     printf("name: %s\n", codec->name);
     printf("type: %d\n", codec->type);
     printf("id: %d\n",   codec->id);
     printf("priv_data_size: %d\n",   codec->priv_data_size);
     printf("capabilities: %d\n",   codec->capabilities);
     printf("init fptr: %d\n",   (int)codec->init);
     printf("encode fptr: %d\n",   (int)codec->encode);
     printf("close fptr: %d\n",   (int)codec->close);
     printf("decode fptr: %d\n",   (int)codec->decode);
  }
}

/* -----------------------------
 * p_ffmpeg_init
 * -----------------------------
 */
static void
p_ffmpeg_init(void)
{
  if(gap_debug) printf("p_ffmpeg_init: before av_register_all \n");


 /**
  * Initialize libavcodec and register all the codecs and formats.
  * (does  avcodec_init(),  avcodec_register_all, protocols etc....
  */
  av_register_all();

  if(gap_debug)
  {
    p_debug_codec_list();
  }
}  /* end p_ffmpeg_init */


/* -----------------------------
 * p_ff_open_input
 * -----------------------------
 * open input context and codec for video or audio
 * depending on the input parameter vid_open.
 *
 * IN: filename (videofile to open for read)
 * INOUT: gvahand
 * INOUT: handle
 * IN: vid_open TRUE: open stream for video only
 *               FALSE: open stream for audio only
 */
static gboolean
p_ff_open_input(char *filename, t_GVA_Handle *gvahand, t_GVA_ffmpeg*  handle, gboolean vid_open)
{
  AVCodecContext *acc;
  AVFormatContext *ic;
  AVInputFormat *iformat;
  int err, ii, ret;
  int rfps, rfps_base;
  int thread_count;
  
  if(gap_debug) printf("p_ff_open_input: START  vid_open:%d\n", (int)vid_open);

  thread_count = 4;
  
//   gap_base_get_gimprc_int_value("num-processors"
//                                    , DEFAULT_WORKER_THREADS
//                                    , 1
//                                    , MAX_WORKER_THREADS
//                                    );

  /* open the input file with generic libav function
   * Opens a media file as input. The codec are not opened.
   * Only the file header (if present) is read.
   */
  err = av_open_input_file(&ic, filename, NULL, 0, NULL);
  if (err < 0)
  {
     if(gap_debug) printf("p_ff_open_input: av_open_input_file FAILED: %d\n", (int)err);
     return(FALSE);
  }

  iformat = ic->iformat;

  if(gap_debug)
  {
    printf("ic: iformat: %d\n", (int)iformat);
    if(iformat)
    {
      printf("iformat name: %s\n", iformat->name);
      printf("iformat long_name: %s\n", iformat->long_name);
      printf("iformat FPTR read_seek: %d\n", (int)iformat->read_seek);
    }
  }


  /* If not enough info to get the stream parameters, we decode the
   * first frames to get it. (used in mpeg case for example)
   */
  ret = av_find_stream_info(ic);
  if (ret < 0)
  {
     if(gap_debug) printf("p_ff_open_input:%s: could not find codec parameters\n", filename);
     return(FALSE);
  }



  /* --------------------------
   * check all input streams
   * and pick up infos of the desired streams
   * (specified by vid_track aud_track)
   */
  gvahand->vtracks = 0;
  gvahand->atracks = 0;
  for(ii=0; ii < ic->nb_streams; ii++)
  {
    acc = ic->streams[ii]->codec;
    //avcodec_thread_init(acc, thread_count);

    switch(acc->codec_type)
    {
        case AVMEDIA_TYPE_AUDIO:
            gvahand->atracks++;  /* count all audiostraems as audiotrack */
            if(gap_debug)
	    {
	      printf("\nInput Audio Track @ streamIndex:%d channels: %d\n"
	        , ii
		, acc->channels
		);
	    }
            if((gvahand->atracks == gvahand->aud_track)
            || (gvahand->atracks == 1))
            {
              if(!vid_open)
              {
                handle->aud_stream_index = ii;
                handle->aud_codec_id = acc->codec_id;
                handle->aud_codec_context = acc;

                handle->aud_stream = ic->streams[ii];
                //avcodec_thread_init(handle->aud_stream->codec, thread_count);
              }
              gvahand->audio_cannels = acc->channels;
              gvahand->samplerate = acc->sample_rate;
            }
            break;
        case AVMEDIA_TYPE_VIDEO:
            gvahand->vtracks++; /* count all videostraems as videotrack */
            if(gap_debug)
	    {
	      printf("\nInput Video Track @ streamIndex:%d\n"
	        , ii
		);
	    }
            if((gvahand->vtracks == gvahand->vid_track)
            || (gvahand->vtracks == 1))
            {
              gdouble containerFramerate;
              gdouble codecFramerate;

              if(vid_open)
              {
                handle->vid_stream_index = ii;
                handle->vid_codec_id = acc->codec_id;
                handle->vid_codec_context = acc;

                handle->vid_stream = ic->streams[ii];
                //avcodec_thread_init(handle->vid_stream->codec, thread_count);
                
              }
              gvahand->height = acc->height;
              gvahand->width = acc->width;

              /* Aspect Ratio handling */
              p_set_aspect_ratio(gvahand, handle);


              rfps      = ic->streams[ii]->r_frame_rate.num;
              rfps_base = ic->streams[ii]->r_frame_rate.den;

              acc->strict_std_compliance = FF_COMPLIANCE_NORMAL;
              acc->workaround_bugs = FF_BUG_AUTODETECT;
              acc->error_recognition = FF_ER_COMPLIANT;
              acc->error_concealment = FF_EC_DEBLOCK | FF_EC_GUESS_MVS;
              acc->idct_algo= 0;
              /*
               * if(acc->codec->capabilities & CODEC_CAP_TRUNCATED)
               *   acc->flags|= CODEC_FLAG_TRUNCATED;
               */
              if(/*acc->codec_id==CODEC_ID_MPEG4 || */acc->codec_id==CODEC_ID_MPEG1VIDEO)
              {
                  acc->flags|= CODEC_FLAG_TRUNCATED;
              }


              /* attempt to get the framerate */
              containerFramerate = 1.0;
              codecFramerate = 1.0;

              gvahand->framerate = 1.0;
              if (acc->time_base.num != 0)
              {
                 codecFramerate = (gdouble)acc->time_base.den / (gdouble)acc->time_base.num;

                if (acc->ticks_per_frame > 1)
                {
                  /* videos with interlaced frames typically deliver framerates at double speed
                   * because they refere to half-frames per second and have set  ticks_per_frame to value 2.
                   * therefoe divide by ticks_per_frame to deliver the framerate of full frame
                   */
                  codecFramerate /= (gdouble)acc->ticks_per_frame;
                }
              }

              if (rfps_base != 0)
              {
                containerFramerate = (gdouble)rfps / (gdouble)rfps_base;
              }

              gvahand->framerate = codecFramerate;
              if(containerFramerate != codecFramerate)
              {
                /* in case the framerate values of the codec and container are different
                 * pick the smaller value (but only if it is greater than plausibility threshold of 10 fps)
                 */

                if((containerFramerate > 10.0)
                && (containerFramerate < codecFramerate))
                {
                   gvahand->framerate = containerFramerate;
                }

                printf("\nSeems stream %d codec frame rate differs from container frame rate: %2.2f (%d/%d) -> %2.2f (%d/%d) ticksPerFrame:%d\n"
                         ,ii
                         , (float)acc->time_base.den /  (float)acc->time_base.num
                         , acc->time_base.den
                         , acc->time_base.num
                         , (float)rfps /  (float)rfps_base
                         , rfps
                         , rfps_base
                         , acc->ticks_per_frame
                         );
              }
            }
            break;
        default:
            break;  /* av_abort(); */
    }
  }



  /* open video codec (if needed) */
  if((gvahand->vtracks > 0)
  && (vid_open))
  {
    if(handle->vid_codec_context)
    {
      handle->vcodec = avcodec_find_decoder(handle->vid_codec_id);
    }

    if(handle->vcodec)
    {
      /* open codec  */
      if (avcodec_open(handle->vid_codec_context, handle->vcodec) < 0)
      {
         printf("Error while opening video codec %s\n", handle->vcodec->name);
         return(FALSE);
      }
      if(gap_debug) printf("p_wrapper_ffmpeg_open_read: open video codec : %s OK\n", handle->vcodec->name);
    }
    else
    {
      handle->vid_stream_index = -1;
      printf("cant decode video (no compatible codec found)\n");
      /* return(FALSE); */
    }

    if(gap_debug)
    {
      if(handle->vid_codec_context) printf("(2) CodecPointer  AVCodecContext->codec : %d\n", (int)handle->vid_codec_context->codec);
      if(handle->vid_codec_context->codec) printf("(2) Codec FunctionPointer  AVCodecContext->codec->decode : %d\n", (int)handle->vid_codec_context->codec->decode);
    }
  }



  /* open audio codec (if needed) */
  if((gvahand->atracks > 0)
  && (!vid_open))
  {
    if(handle->aud_codec_context)
    {
      handle->acodec = avcodec_find_decoder(handle->aud_codec_id);
    }
    if(handle->acodec)
    {
      if (avcodec_open(handle->aud_codec_context, handle->acodec) < 0)
      {
         printf("** Error while opening audio codec %s\n", handle->acodec->name);
         return(FALSE);
      }
      if(gap_debug) printf("p_wrapper_ffmpeg_open_read: open audio codec : %s OK\n", handle->acodec->name);
    }
    else
    {
      printf("cant decode audio (no compatible codec found)\n");
      handle->aud_stream_index = -1;
      /* return(FALSE); */
    }

    if(gap_debug)
    {
      if(handle->aud_codec_context) printf("(3) CodecPointer  AVCodecContext->codec : %d\n", (int)handle->aud_codec_context->codec);
      if(handle->aud_codec_context->codec) printf("(3) Codec FunctionPointer  AVCodecContext->codec->decode : %d\n", (int)handle->aud_codec_context->codec->decode);
    }
  }


  if(vid_open)
  {
    handle->vid_input_context = ic;

    if(gap_debug) printf("p_ff_open_input: END  vid_input_context:%d\n", (int)handle->vid_input_context);
  }
  else
  {
    handle->aud_input_context = ic;

    if(gap_debug) printf("p_ff_open_input: END  aud_input_context:%d\n", (int)handle->aud_input_context);
  }

  return(TRUE);
}  /* end p_ff_open_input */

/* -------------------------
 * p_set_aspect_ratio
 * -------------------------
 * set the gvahand->aspect_ratio variable to aspect ratio
 * typical values are
 * 1.777777  For 16:9 video
 * 1.333333  For 4:3  video
 *
 * Note that the gvahand->aspect_ratio variable describes the ratio
 * for the full image (and not the ratio of a single pixel)
 *
 * (code is based on example ffplay.c)
 */
static void
p_set_aspect_ratio(t_GVA_Handle *gvahand, t_GVA_ffmpeg*  handle)
{
  AVStream *video_st;

  video_st = handle->vid_stream;
  gvahand->aspect_ratio = 0.0;

  if (video_st->sample_aspect_ratio.num)
  {
    gvahand->aspect_ratio = av_q2d(video_st->sample_aspect_ratio);
  }
  else if (video_st->codec->sample_aspect_ratio.num)
  {
    gvahand->aspect_ratio = av_q2d(video_st->codec->sample_aspect_ratio);
  }

  if (gvahand->aspect_ratio <= 0.0)
  {
     gvahand->aspect_ratio = 1.0;
  }

  if(gvahand->aspect_ratio != 0.0)
  {
     gvahand->aspect_ratio *= (gdouble)video_st->codec->width / video_st->codec->height;
  }


#if 0
  if(gap_debug)
  {
    printf("#if 0  dtg_active_format=%d\n", video_st->codec->dtg_active_format);
  }


  /* dtg_active_format: aspect information may be available in some cases.
   * MPEG2(additional aspect ratio
   * information only used in DVB MPEG-2 transport streams)
   * 0 if not set.
   */
  switch(video_st->codec->dtg_active_format)
  {
     case FF_DTG_AFD_4_3:
         gvahand->aspect_ratio = 4.0 / 3.0;
         break;
     case FF_DTG_AFD_16_9:
         gvahand->aspect_ratio = 16.0 / 9.0;
         break;
     case FF_DTG_AFD_14_9:
         gvahand->aspect_ratio = 14.0 / 9.0;
         break;
     case FF_DTG_AFD_4_3_SP_14_9:
         gvahand->aspect_ratio = 14.0 / 9.0;
         break;
     case FF_DTG_AFD_16_9_SP_14_9:
         gvahand->aspect_ratio = 14.0 / 9.0;
         break;
     case FF_DTG_AFD_SP_4_3:
         gvahand->aspect_ratio = 4.0 / 3.0;
         break;
    case FF_DTG_AFD_SAME:
    default:
      break;
  }
#endif


  if(gap_debug)
  {
    printf("FF ASPECT: dtg_active_format:%d  num:%d den:%d\n"
          ,(int)video_st->codec->dtg_active_format
          ,(int)video_st->codec->sample_aspect_ratio.num
          ,(int)video_st->codec->sample_aspect_ratio.den
          );
    printf("FF ASPECT: dtected aspect_ratio: %f\n"
          ,(float)gvahand->aspect_ratio
          );
  }

}  /* end p_set_aspect_ratio */



/* ------------------------------
 * p_ffmpeg_vid_reopen_read
 * ------------------------------
 * internal procedure to reset read position
 * reopen the video context of a videofile for read
 * (that is already open for read)
 * the audio context (if there is one) is not affected !
 */
static void
p_ffmpeg_vid_reopen_read(t_GVA_ffmpeg *handle, t_GVA_Handle *gvahand)
{
    if(gap_debug) printf("p_ffmpeg_vid_reopen_read: REOPEN\n");

    /* CLOSE the video codec */
    if((handle->vid_codec_context) && (handle->vcodec))
    {
      avcodec_close(handle->vid_codec_context);
      handle->vid_codec_context = NULL;
    }

    /* Close a media file (just the video context) */
    if(handle->vid_input_context)
    {
      av_close_input_file(handle->vid_input_context);
      handle->vid_input_context = NULL;
    }

    if(handle->vid_pkt.data != NULL)
    {
      av_free_packet(&handle->vid_pkt);
    }
    {
      gint ii;
      for(ii=0; ii < MAX_PREV_OFFSET; ii++)
      {
        handle->prev_url_offset[ii] = 0;
      }
    }
    handle->vid_pkt.size = 0;             /* REstart with empty packet */
    handle->vid_pkt.data = NULL;          /* REstart with empty packet */
    handle->inbuf_len = 0;            /* start with empty buffer */
    handle->inbuf_ptr = NULL;         /* start with empty buffer, after 1.st av_read_frame: pointer to pkt.data read pos */

    /* RE-open for the VIDEO part */
    p_ff_open_input(gvahand->filename, gvahand, handle, TRUE);


    if(gap_debug)
    {
      printf("++## pix_fmt: keep:%d ignore from codec:%d (PIX_FMT_YUV420P:%d PIX_FMT_YUV422P:%d)\n"
          , (int)handle->yuv_buff_pix_fmt
          , (int)handle->vid_codec_context->pix_fmt
          , (int)PIX_FMT_YUV420P
          , (int)PIX_FMT_YUV422P
          );
    }

}  /* end p_ffmpeg_vid_reopen_read */


/* ------------------------------
 * p_ffmpeg_aud_reopen_read
 * ------------------------------
 * internal procedure to reset read position
 * reopen the audio context of a videofile for read
 * (that is already open for read)
 * the video context (if there is one) is not affected !
 */
static void
p_ffmpeg_aud_reopen_read(t_GVA_ffmpeg *handle, t_GVA_Handle *gvahand)
{
    if(gap_debug)
    {
      printf("p_ffmpeg_aud_reopen_read: REOPEN\n");
    }


    /* CLOSE the audio codec */
    if((handle->aud_codec_context) && (handle->acodec))
    {
      avcodec_close(handle->aud_codec_context);
      handle->aud_codec_context = NULL;
    }

    /* Close a media file (just the video context) */
    if(handle->aud_input_context)
    {
      av_close_input_file(handle->aud_input_context);
      handle->aud_input_context = NULL;
    }

    handle->vid_pkt.size = 0;             /* REstart with empty packet */
    handle->vid_pkt.data = NULL;          /* REstart with empty packet */
    handle->inbuf_len = 0;            /* start with empty buffer */
    handle->inbuf_ptr = NULL;         /* start with empty buffer, after 1.st av_read_frame: pointer to pkt.data read pos */

    handle->samples_read = 0.0;
    handle->bf_idx = 0;
    handle->samples_base[0] = 0;
    handle->samples_base[1] = 0;
    handle->bytes_filled[0] = 0;
    handle->bytes_filled[1] = 0;
    handle->output_samples_ptr = handle->samples_buffer[0];

    /* RE-open for the AUDIO part */
    p_ff_open_input(gvahand->filename, gvahand, handle, FALSE);

}  /* end p_ffmpeg_aud_reopen_read */


/* ------------------------------
 * p_audio_convert_to_s16
 * ------------------------------
 *
 * convert audio samples in specified data_length at handle->output_samples_ptr
 * to signed 16 bit little endian audio format.
 * The original content of handle->output_samples_ptr is replaced by the conversion result
 * that may also change data length.
 *
 * return converted_data_size
 */
static int
p_audio_convert_to_s16(t_GVA_ffmpeg *handle
                     , int data_size
                     )
{
  guchar         *audio_convert_in_buffer;
  AVCodecContext *dec;
  int             converted_data_size;

  dec = handle->aud_codec_context;
  converted_data_size = 0;
  if (handle->reformat_ctx == NULL)
  {
    handle->reformat_ctx = av_audio_convert_alloc(SAMPLE_FMT_S16, 1,
                                           dec->sample_fmt, 1, NULL, 0);
    if (!handle->reformat_ctx)
    {
      printf("ERROR: Cannot convert %s sample format to %s sample format\n",
          avcodec_get_sample_fmt_name(dec->sample_fmt),
          avcodec_get_sample_fmt_name(SAMPLE_FMT_S16));
      return (converted_data_size);
    }
  }

  audio_convert_in_buffer = g_malloc(data_size);
  if (audio_convert_in_buffer != NULL)
  {
    /* copy samples in a new buffer to be used as input for the conversion */
    memcpy(audio_convert_in_buffer, handle->output_samples_ptr, data_size);

    /* convert and write converted samples back to handle->output_samples_ptr */
    if (handle->reformat_ctx)
    {
       const void *ibuf[6]= {audio_convert_in_buffer};
       void *obuf[6]= {handle->output_samples_ptr};
       int istride[6]= {av_get_bits_per_sample_format(dec->sample_fmt)/8};
       int ostride[6]= {2};
       int len= data_size/istride[0];
       converted_data_size = len * ostride[0];
       if (av_audio_convert(handle->reformat_ctx, obuf, ostride, ibuf, istride, len) < 0)
       {
           printf("av_audio_convert() failed\n");
           converted_data_size = 0;
       }

    }
    g_free(audio_convert_in_buffer);
  }

  return (converted_data_size);

}  /* end p_audio_convert_to_s16 */



/* ------------------------------
 * p_pick_channel
 * ------------------------------
 * copy 16bit samples from samples_buffer[0/1] to output_i buffer
 * only the samples from the requested channel are copied.
 * copy starts at sample_idx and affects the given number of samples.
 * NOTE: the samples_buffer must have been filled up
 *       at least until the needed position (sample_idx + samples) before
 *       this routine is called.
 *  there will be no size checks !!
 *      output_i must be large enogh to hold samples * 2 bytes.
 */
static gint32
p_pick_channel( t_GVA_ffmpeg *handle
              , t_GVA_Handle *gvahand
              , gint16 *output_i
              , gint32 sample_idx
              , gint32 samples
              , gint32 channel
              )
{
  guchar *this_peek_ptr;
  guchar *prev_peek_ptr;
  guchar *poke_ptr;
  gint    bytes_per_sample;
  gint32  ii;
  gint32  l_samples;
  gint32  l_this_samples;
  gint32  l_prev_samples;
  gint32  peek_idx;
  gint32  l_samples_picked;
  gint32  this_idx;
  gint32  prev_idx;

  if(gap_debug)
  {
    printf("p_pick_channel: START channel:%d\n", (int)channel);
  }

  l_samples_picked = 0;
  bytes_per_sample =  2 * gvahand->audio_cannels;

  this_idx = handle->bf_idx ;
  prev_idx = (this_idx + 1) & 1;
  this_peek_ptr = NULL;
  prev_peek_ptr = NULL;
  l_prev_samples = 0;
  l_this_samples = 0;
  l_samples  = samples;

  if((samples + sample_idx) > handle->samples_read)
  {
      /* copy only as much as was read before.
       * (normally we dont get in here,
       * but if we did not read enough (? at EOF) l_samples gets 0 or negative
       * and prevents copying uninitiated data
       */
      l_this_samples = (gint32)  (handle->samples_read - sample_idx);

      if(gap_debug)
      {
        printf("p_pick_channel(2): l_this_samples:%d\n", (int)l_this_samples);
      }
  }

  if(sample_idx >= handle->samples_base[this_idx])
  {
    /* start sample is available in the current samples_buffer
     * (there is no need to check the prev buffer)
     */

    peek_idx = ((sample_idx - handle->samples_base[this_idx])  * bytes_per_sample) + (2* (channel -1));

    this_peek_ptr = handle->samples_buffer[this_idx];
    this_peek_ptr += peek_idx;
    l_this_samples = l_samples;
  }
  else
  {
    if(sample_idx >= handle->samples_base[prev_idx])
    {
      gint32 l_avail_samples;

      /* start sample is available in the prev samples_buffer
       * but the requested number of samples may be spread
       * from the other buffer to this (current) buffer
       * (there may be a need to read from both buffers)
       */
      peek_idx = ((sample_idx - handle->samples_base[prev_idx])  * bytes_per_sample) + (2* (channel -1));

      prev_peek_ptr = handle->samples_buffer[prev_idx];
      prev_peek_ptr += peek_idx;

      l_avail_samples = (handle->bytes_filled[prev_idx] - peek_idx) / bytes_per_sample;

      if(l_samples <= l_avail_samples)
      {
        l_prev_samples = l_samples;     /* peek everything from the prev buffer */
      }
      else
      {
        l_prev_samples = l_avail_samples;
        l_this_samples  = l_samples - l_avail_samples;  /* peek the rest from this buffer */
        this_peek_ptr = handle->samples_buffer[this_idx];
      }

    }
    else
    {
       printf("** ERROR could not fetch %d samples at once (max Buffersize is: %d)\n"
              , (int)samples
              , (int)GVA_SAMPLES_BUFFER_SIZE / 2
              );
       return(0);
    }
  }

  poke_ptr = (guchar *)output_i;

  if(gap_debug)
  {
    printf("p_pick_channel(1): sample_idx:%d ch:%d bytes_p_s:%d sam_read:%d peek_idx:%d prev_peek_ptr:%d  this_peek_ptr:%d poke_ptr:%d\n"
                        ,(int)sample_idx
                        ,(int)channel
                        ,(int)bytes_per_sample
                        ,(int)handle->samples_read
                        ,(int)peek_idx
                        ,(int)prev_peek_ptr
                        ,(int)this_peek_ptr
                        ,(int)poke_ptr
                        );
    printf("p_pick_channel(2): samples:%d   prev_samples:%d this_samples:%d\n"
                        ,(int)samples
                        ,(int)l_prev_samples
                        ,(int)l_this_samples
                        );
  }

  /* we may peek the start from the prev buffer */
  for(ii=0; ii < l_prev_samples; ii++)
  {
     /* if(ii%60 == 0) printf("\n%12d:", (int)peek_ptr);  */
     /* printf("%02x%02x ", (int)peek_ptr[0] ,(int)peek_ptr[1]); */

     *poke_ptr = prev_peek_ptr[0];
     poke_ptr++;
     *poke_ptr = prev_peek_ptr[1];
     poke_ptr++;
     prev_peek_ptr += bytes_per_sample;
     l_samples_picked++;
  }

  /* then we can peek the rest from the this (current) buffer */
  for(ii=0; ii < l_this_samples; ii++)
  {
     /* if(ii%60 == 0) printf("\n%12d:", (int)peek_ptr);  */
     /* printf("%02x%02x ", (int)peek_ptr[0] ,(int)peek_ptr[1]); */

     *poke_ptr = this_peek_ptr[0];
     poke_ptr++;
     *poke_ptr = this_peek_ptr[1];
     poke_ptr++;
     this_peek_ptr += bytes_per_sample;
     l_samples_picked++;
  }



  if(gap_debug)
  {
    printf("p_pick_channel END: l_samples_picked:%d l_samples:%d prev_peek_ptr:%d this_peek_ptr:%d poke_ptr%d\n"
                      ,(int)l_samples_picked
                      ,(int)l_samples
                      ,(int)prev_peek_ptr
                      ,(int)this_peek_ptr
                      ,(int)poke_ptr
                      );
  }

  return(l_samples_picked);

}  /* end p_pick_channel */


/* ------------------------------
 * p_read_audio_packets
 * ------------------------------
 * IN: max_sample_pos is the position of the last audiosample
 *     that is needed.
 *     this procedure does write to samples_buffer[0] and/or samples_buffer[1]
 *     by reading and decoding audio packets as long as the samples_buffer [0/1]
 *     are filled up to max_sample_pos (or eof is reached)
 */
static int
p_read_audio_packets( t_GVA_ffmpeg *handle, t_GVA_Handle *gvahand, gint32 max_sample_pos)
{
  int       data_size;
  int       l_rc;

  l_rc = 0;

   if(handle->samples_buffer[0] == NULL)
   {
      /* allocate 2 buffers, each is large enough to hold at least one uncompressed
       * audiopacket. those buffers are then filled with uncomressed 16bit audiosamples.
       * each time one of the buffers is full, the write pointer output_samples_ptr
       * switches to the other buffer
       */
      handle->samples_buffer_size = GVA_SAMPLES_BUFFER_SIZE * gvahand->audio_cannels * 2;
      handle->samples_buffer[0] = g_malloc0(handle->samples_buffer_size);
      handle->samples_buffer[1] = g_malloc0(handle->samples_buffer_size);
      handle->samples_read = 0.0;
      handle->bf_idx = 0;
      handle->samples_base[0] = 0;
      handle->samples_base[1] = 0;
      handle->bytes_filled[0] = 0;
      handle->bytes_filled[1] = 0;
      handle->biggest_data_size = 4096;

      gvahand->current_sample = 0.0;
      gvahand->reread_sample_pos = 0.0;
      handle->output_samples_ptr = handle->samples_buffer[0];
   }

   if (gap_debug)
   {
     printf("p_read_audio_packets: before WHILE max_sample_pos: %d\n", (int)max_sample_pos);
     printf("p_read_audio_packets: before WHILE samples_read: %d\n", (int)handle->samples_read);
     printf("samples_buffer[0]: %d samples_buffer[1]:%d output_samples_ptr:%d\n"
         , (int)handle->samples_buffer[0]
         , (int)handle->samples_buffer[1]
         , (int)handle->output_samples_ptr
	 );
   }

   while(handle->samples_read < max_sample_pos)
   {
     int l_len;
     int l_pktlen;

    /* if abuf is empty we must read the next packet */
    if((handle->abuf_len  < 1) || (handle->abuf_ptr == NULL))
    {
      /*if (gap_debug) printf("p_read_audio_packets: before av_read_frame aud_input_context:%d\n", (int)handle->aud_input_context);*/
      /**
        * Read a packet from a media file
        * @param s media file handle
        * @param pkt is filled
        * @return 0 if OK. AVERROR_xxx if error.
        */
      l_pktlen = av_read_frame(handle->aud_input_context, &handle->aud_pkt);
      if(l_pktlen < 0)
      {
         /* EOF reached */
         if (gap_debug)
	 {
	   printf("p_read_audio_packets: EOF reached (or read ERROR)\n");
	 }

         gvahand->total_aud_samples = handle->samples_read;
         gvahand->all_samples_counted = TRUE;

         l_rc = 1;
         break;
      }

      if (gap_debug)
      {
        printf("aud_stream:%d pkt.stream_index #%d, pkt.size: %d samples_read:%d\n"
	   , handle->aud_stream_index, handle->aud_pkt.stream_index
	   , handle->aud_pkt.size, (int)handle->samples_read);
      }


      /* check if packet belongs to the selected audio stream */
      if(handle->aud_pkt.stream_index != handle->aud_stream_index)
      {
         /* discard packet */
         /* if (gap_debug)  printf("DISCARD Packet\n"); */
         av_free_packet(&handle->aud_pkt);
         continue;
      }

      if (gap_debug)
      {
        printf("using Packet stream_index:%d data:%d size:%d\n"
	   ,(int)handle->aud_pkt.stream_index
	   ,(int)handle->aud_pkt.data
	   ,(int)handle->aud_pkt.size
	   );
      }

      /* packet is part of the selected video stream, use that packet */
      handle->abuf_ptr = handle->aud_pkt.data;
      handle->abuf_len = handle->aud_pkt.size;
    }


    /* decode a frame. return -1 if error, otherwise return the number of
     * bytes used. If no audio frame could be decompressed, data_size is
     * zero or negative.
     */
    data_size = handle->samples_buffer_size;
#ifdef GAP_USES_OLD_FFMPEG_0_5
    if (gap_debug)
    {
       printf("before avcodec_decode_audio2: abuf_ptr:%d abuf_len:%d\n"
         , (int)handle->abuf_ptr, (int)handle->abuf_len);
    }
    l_len = avcodec_decode_audio2(handle->aud_codec_context  /* AVCodecContext * */
                               ,(int16_t *)handle->output_samples_ptr
                               ,&data_size
                               ,handle->abuf_ptr
                               ,handle->abuf_len
                               );
#else
  {
    data_size = FFMAX(handle->aud_pkt.size * sizeof(int16_t), AVCODEC_MAX_AUDIO_FRAME_SIZE);
    if (data_size > handle->av_samples_size)
    {
      /* force re-allocation of the aligend buffer */
      if (handle->av_samples != NULL)
      {
        av_free(handle->av_samples);
	handle->av_samples = NULL;
      }
    
    }
    if (handle->av_samples == NULL)
    {
      handle->av_samples = av_malloc(data_size);
      handle->av_samples_size = data_size;
    }
    if(gap_debug)
    {
       printf("before avcodec_decode_audio3: av_samples:%d data_size:%d\n"
	 , (int)handle->av_samples
	 , (int)data_size
	 );
    }
    l_len = avcodec_decode_audio3(handle->aud_codec_context  /* AVCodecContext * */
                               ,handle->av_samples
                               ,&data_size
                               ,&handle->aud_pkt
                               );
    if(data_size > 0)
    {
      /* copy the decoded samples from the aligned av_samples buffer
       * to current position in the samples_buffer of the relevant channel
       * Note that calling avcodec_decode_audio3 procedure with handle->output_samples_ptr
       * as destination pointer for the 16bit samples would be faster, 
       * but will crash when ffmpeg is configured
       * with enabled MMX and this pointer is not aligned.
       */
      memcpy(handle->output_samples_ptr, handle->av_samples, data_size);
    }
  }
#endif


    if (gap_debug)
    {
      printf("after avcodec_decode_audioX: l_len:%d data_size:%d samples_read:%d \n"
             " sample_fmt:%d %s (expect:%d SAMPLE_FMT_S16)\n"
                         , (int)l_len
                         , (int)data_size
                         , (int)handle->samples_read
                         , (int)handle->aud_codec_context->sample_fmt
                         , avcodec_get_sample_fmt_name(handle->aud_codec_context->sample_fmt)
                         , (int)SAMPLE_FMT_S16
                         );
    }

    if(l_len < 0)
    {
       printf("p_read_audio_packets: avcodec_decode_audioX returned ERROR)\n"
             "abuf_len:%d AVCODEC_MAX_AUDIO_FRAME_SIZE:%d samples_buffer_size:%d data_size:%d\n"
             , (int)handle->abuf_len
             , (int)AVCODEC_MAX_AUDIO_FRAME_SIZE
             , (int)handle->samples_buffer_size
             , (int)data_size
             );
       l_rc = 2;
       break;
    }

    /* Some bug in mpeg audio decoder gives */
    /* data_size < 0, it seems they are overflows */
    if (data_size > 0)
    {
       gint    bytes_per_sample;
       int     converted_data_size;
       if (handle->aud_codec_context->sample_fmt != SAMPLE_FMT_S16)
       {
         /* convert audio */
         converted_data_size = p_audio_convert_to_s16(handle
                                     , data_size
                                     );
         if(converted_data_size <= 0)
         {
           l_rc = 2;
           break;
         }
       }
       else
       {
         converted_data_size = data_size;
       }

       /* debug code block: count not null bytes in the decoded data block */
       if(gap_debug)
       {
          gint32   ii;
          gint32   l_sum;

          l_sum = 0;
          for(ii=0; ii < converted_data_size; ii++)
          {
            /* if(ii%60 == 0) printf("\n%012d:", (int)&handle->output_samples_ptr[ii]); */
            /* printf("%02x", (int)handle->output_samples_ptr[ii]); */

            if(handle->output_samples_ptr[ii] != 0)
            {
              l_sum++;
            }
          }
          printf("\nSUM of NOT NULL bytes: %d output_samples_ptr:%d\n", (int)l_sum, (int)handle->output_samples_ptr);
       }

       /* check for the biggest uncompressed packet size */
       if (converted_data_size  > handle->biggest_data_size)
       {
         handle->biggest_data_size = converted_data_size;
       }
       /* add the decoded packet size to one of the samples_buffers
        * and advance write position
        */
       bytes_per_sample =  MAX((2 * gvahand->audio_cannels), 1);
       handle->bytes_filled[handle->bf_idx] += converted_data_size;
       handle->samples_read += (converted_data_size  / bytes_per_sample);

       /* check if we have enough space in the current samples_buffer (for the next packet) */
       if(handle->bytes_filled[handle->bf_idx] + handle->biggest_data_size  > GVA_SAMPLES_BUFFER_SIZE)
       {
         /* no more space in the current samples_buffer,
          * switch write pointer to the other buffer
          * (reset this other buffer now, and overwrite at next packet read)
          */
         handle->bf_idx = (handle->bf_idx +1) & 1;

         if(gap_debug) printf("WRITE SWITCH samples_buffer to %d\n", (int)handle->bf_idx);

         handle->output_samples_ptr = handle->samples_buffer[handle->bf_idx];
         handle->samples_base[handle->bf_idx] = handle->samples_read;
         handle->bytes_filled[handle->bf_idx] = 0;
       }
       else
       {
         /* enouch space, continue writing to the same buffer */
         handle->output_samples_ptr += converted_data_size;
       }

       /* if more samples found then update total_aud_samples (that was just a guess) */
       gvahand->total_aud_samples = MAX(handle->samples_read, gvahand->total_aud_samples);
    }

    handle->abuf_ptr += l_len;
    handle->abuf_len -= l_len;

    /* length of audio packet was completely processed, we can free that packet now */
    if(handle->abuf_len < 1)
    {
      /* if (gap_debug)  printf("FREE Packet\n"); */
      av_free_packet(&handle->aud_pkt);
      handle->aud_pkt.size = 0;             /* set empty packet status */
      handle->aud_pkt.data = NULL;          /* set empty packet status */
    }

  }  /* end while packet_read and decode audio frame loop */

  if(gap_debug)
  {
    printf("p_read_audio_packets: DONE return code:%d\n", (int)l_rc);
  }
  return(l_rc);
}  /* end p_read_audio_packets */


/* ----------------------------------
 * p_vindex_add_url_offest
 * ----------------------------------
 * add one entry to the videoindex.
 */
static void
p_vindex_add_url_offest(t_GVA_Handle *gvahand
                         , t_GVA_ffmpeg *handle
                         , t_GVA_Videoindex *vindex
                         , gint32 seek_nr
                         , gint64 url_offset
                         , guint16 frame_length
                         , guint16 checksum
                         , gint64 timecode_dts
                         )
{
  static gint64 s_last_seek_nr;

  if(vindex->tabsize_used > 0)
  {
    if(seek_nr <= s_last_seek_nr)
    {
      return;
    }
  }
  s_last_seek_nr = seek_nr;

  if(vindex->tabsize_used >= vindex->tabsize_allocated -1)
  {
    t_GVA_IndexElem *redim_ofs_tab;
    t_GVA_IndexElem *old_ofs_tab;
    gint32  new_size;

    /* on overflow redim the table by adding GVA_VIDINDEXTAB_BLOCK_SIZE elements */
    new_size = vindex->tabsize_allocated + GVA_VIDINDEXTAB_BLOCK_SIZE;
    redim_ofs_tab = g_new(t_GVA_IndexElem, new_size);
    if(redim_ofs_tab)
    {
      if(gap_debug)
      {
        printf("p_vindex_add_url_offest: REDIM:vindex->tabsize_allocated %d NEW:%d\n"
                          , (int)vindex->tabsize_allocated
                          , (int)new_size
                          );
      }
      old_ofs_tab = vindex->ofs_tab;
      memcpy(redim_ofs_tab, old_ofs_tab, (vindex->tabsize_allocated * sizeof(t_GVA_IndexElem)));
      vindex->ofs_tab = redim_ofs_tab;
      g_free(old_ofs_tab);
      vindex->tabsize_allocated = new_size;
    }
  }

  if(vindex->tabsize_used < vindex->tabsize_allocated -1)
  {
    vindex->ofs_tab[vindex->tabsize_used].uni.offset_gint64 = url_offset;
    vindex->ofs_tab[vindex->tabsize_used].seek_nr = seek_nr;
    vindex->ofs_tab[vindex->tabsize_used].frame_length = frame_length;
    vindex->ofs_tab[vindex->tabsize_used].checksum = checksum;
    vindex->ofs_tab[vindex->tabsize_used].timecode_dts = timecode_dts;

    if(gap_debug)
    {
      printf("p_vindex_add_url_offest: ofs_tab[%d]: ofs64: %lld seek_nr:%d flen:%d chk:%d dts:%lld\n"
                       , (int)vindex->tabsize_used
                       , vindex->ofs_tab[vindex->tabsize_used].uni.offset_gint64
                       , (int)vindex->ofs_tab[vindex->tabsize_used].seek_nr
                       , (int)vindex->ofs_tab[vindex->tabsize_used].frame_length
                       , (int)vindex->ofs_tab[vindex->tabsize_used].checksum
                       , vindex->ofs_tab[vindex->tabsize_used].timecode_dts
                       );
    }
    vindex->tabsize_used++;
  }

  if(timecode_dts == AV_NOPTS_VALUE)
  {
    /* at this point we detected that seek based on DTS timecode is not reliable for this video
     * therefore store AV_NOPTS_VALUE at index [0] to disables timecode based seek in the video index.
     */
    vindex->ofs_tab[0].timecode_dts = AV_NOPTS_VALUE;
  }
}  /* end p_vindex_add_url_offest */



/* -----------------------------
 * p_gva_checksum
 * -----------------------------
 * for performance reasons the checksum
 * counts only odd rows of the middle column
 */
static guint16
p_gva_checksum(AVPicture *picture_yuv, gint height)
{
  guint16 l_checksum;
  gint ii;

  l_checksum = 0;
  for(ii=0;ii<4;ii++)
  {
    if(picture_yuv->linesize[ii] > 0)
    {
      guchar *buf;
      gint row;

      buf = picture_yuv->data[ii];
      buf += picture_yuv->linesize[ii] / 2;
      for(row=0; row < height; row+=2)
      {
        l_checksum += *buf;
        buf += picture_yuv->linesize[ii];
      }
    }
  }
  return (l_checksum);
}  /* end p_gva_checksum */



/* ----------------------------------
 * p_init_timecode_log
 * ----------------------------------
 * if configured via gimprc parameter video-libavformat-timecodelog != 0
 * for writing timecode log
 * then open the logfile for writing and return filpointer to the logfile.
 * ELSE return NULL pointer.
 */
static FILE *
p_init_timecode_log(t_GVA_Handle   *gvahand)
{
  FILE *fp_timecode_log;
  gint32 gimprc_timecode_log;
  gchar *timecode_logfile_name;

  gimprc_timecode_log
    = gap_base_get_gimprc_int_value("video-libavformat-timecodelog", 0, 0, 1);
  fp_timecode_log = NULL;
  timecode_logfile_name = g_strdup("\0");

  if (gimprc_timecode_log != 0)
  {
    char *timecode_logfile_name;
    char *vindex_file;
    t_GVA_DecoderElem *dec_elem;

    /* probe read required for calculation of expected timecodes in the log
     */
    p_probe_timecode_offset(gvahand);

    dec_elem = (t_GVA_DecoderElem *)gvahand->dec_elem;
    vindex_file = GVA_build_videoindex_filename(gvahand->filename
                                               ,gvahand->vid_track +1
                                               ,dec_elem->decoder_name
                                               );

    timecode_logfile_name = g_strdup_printf("%s.timelog", vindex_file);
    g_free(vindex_file);
    printf("TIMECODE log for video: %s\n  ==> will be created as file: %s\n"
        , gvahand->filename
        , timecode_logfile_name
        );
    fp_timecode_log = g_fopen(timecode_logfile_name, "w");
    if(fp_timecode_log)
    {
      t_GVA_ffmpeg *master_handle;
      master_handle = (t_GVA_ffmpeg *)gvahand->decoder_handle;

      fprintf(fp_timecode_log, "# TIMECODE log for video: %s\n"
        , gvahand->filename
        );
      fprintf(fp_timecode_log, "# READSTEPS_PROBE_TIMECODE = %d\n"
        , READSTEPS_PROBE_TIMECODE
        );
      fprintf(fp_timecode_log, "# timecode offset for frame1: %lld\n"
        , master_handle->timecode_offset_frame1
        );
      fprintf(fp_timecode_log, "# count_timecode_steps:%d stepsizes summary:%d\n"
        , (int)master_handle->count_timecode_steps
        , (int)master_handle->timecode_steps_sum
        );
      fprintf(fp_timecode_log, "#\n");
    }
    g_free(timecode_logfile_name);
  }

  return (fp_timecode_log);
}  /* end p_init_timecode_log */



/* ----------------------------------
 * p_timecode_check_and_log
 * ----------------------------------
 * write log entry for current frame to timecode log file.
 * the master_handle is used read only
 * (for converting framenr to timecode based on
 * proberead statistical data)
 */
static void
p_timecode_check_and_log(FILE *fp_timecode_log, gint32 framenr, t_GVA_ffmpeg *handle, t_GVA_ffmpeg *master_handle
  , t_GVA_Handle *gvahand)
{
  static const char *ok_string = "";
  static const char *err_string = "; # CRITICAL exp - dts difference > 10";
  static int64_t old_pts = AV_NOPTS_VALUE;
  static int64_t old_dts = 0;
  const char *remark_ptr;
  int64_t expected_dts;
  int64_t diff_dts;

  expected_dts = p_frame_nr_to_timecode(master_handle, framenr);

  if (abs(expected_dts - handle->vid_pkt.dts) > 10)
  {
    master_handle->critical_timecodesteps_found = TRUE;
    handle->critical_timecodesteps_found = TRUE;
    gvahand->critical_timecodesteps_found = TRUE;
    remark_ptr = err_string;
  }
  else
  {
    remark_ptr = ok_string;
  }

  if (fp_timecode_log == NULL)
  {
    return;
  }

  fprintf(fp_timecode_log, "num:%06d; exp:%lld; dts:%lld; dts-olddts:%lld; exp-dts:%lld"
          , framenr
          , expected_dts
          , handle->vid_pkt.dts
          , handle->vid_pkt.dts - old_dts
          , expected_dts - handle->vid_pkt.dts
          );

  old_dts = handle->vid_pkt.dts;

  if(handle->vid_pkt.pts != AV_NOPTS_VALUE)
  {
    fprintf(fp_timecode_log, "; pts:%lld; exp-pts:%lld; pts-dts:%lld"
          , handle->vid_pkt.pts
          , expected_dts - handle->vid_pkt.pts
          , handle->vid_pkt.pts - handle->vid_pkt.dts
          );

    if (old_pts != AV_NOPTS_VALUE)
    {
      fprintf(fp_timecode_log, "; pts-oldpts:%lld"
          , handle->vid_pkt.pts - old_pts
          );
    }
    old_pts = handle->vid_pkt.pts;
  }
  else
  {
    fprintf(fp_timecode_log, "; pts:NOPTS_VAL"
          );

  }

  fprintf(fp_timecode_log, "%s\n"
          , remark_ptr
          );

}  /* end p_timecode_check_and_log */



/* ----------------------------------
 * p_set_analysefile_master_keywords
 * ----------------------------------
 */
static void
p_set_analysefile_master_keywords(GapValKeyList *keylist
  , t_GVA_Handle *gvahand, gint32 count_timecode_steps)
{
   t_GVA_ffmpeg *master_handle;
   master_handle = (t_GVA_ffmpeg *)gvahand->decoder_handle;
   int ii;

   gap_val_set_keyword(keylist, "(libavcodec_version_int ", &master_handle->libavcodec_version_int, GAP_VAL_GINT32, 0, "\0");
   gap_val_set_keyword(keylist, "(READSTEPS_PROBE_TIMECODE ", &master_handle->readsteps_probe_timecode, GAP_VAL_GINT32, 0, "\0");
   gap_val_set_keyword(keylist, "(timestamp ", &master_handle->timestamp, GAP_VAL_GINT32, 0, "\0");
   gap_val_set_keyword(keylist, "(video_libavformat_seek_gopsize ", &master_handle->video_libavformat_seek_gopsize, GAP_VAL_GINT32, 0, "\0");
   gap_val_set_keyword(keylist, "(self_test_detected_seek_bug ", &master_handle->self_test_detected_seek_bug, GAP_VAL_GBOOLEAN, 0, "\0");
   gap_val_set_keyword(keylist, "(timecode_proberead_done ", &master_handle->timecode_proberead_done, GAP_VAL_GBOOLEAN, 0, "\0");
   gap_val_set_keyword(keylist, "(all_frames_counted ", &gvahand->all_frames_counted, GAP_VAL_GBOOLEAN, 0, "\0");
   gap_val_set_keyword(keylist, "(total_frames ", &gvahand->total_frames, GAP_VAL_GINT32, 0, "\0");
   gap_val_set_keyword(keylist, "(eof_timecode ", &master_handle->eof_timecode, GAP_VAL_GINT64, 0, "\0");
   gap_val_set_keyword(keylist, "(timecode_step_avg ", &master_handle->timecode_step_avg, GAP_VAL_GINT32, 0, "\0");
   gap_val_set_keyword(keylist, "(timecode_step_abs_min ", &master_handle->timecode_step_abs_min, GAP_VAL_GINT32, 0, "\0");
   gap_val_set_keyword(keylist, "(timecode_offset_frame1 ", &master_handle->timecode_offset_frame1, GAP_VAL_GINT64, 0, "\0");
   gap_val_set_keyword(keylist, "(timecode_steps_sum ", &master_handle->timecode_steps_sum, GAP_VAL_GINT32, 0, "\0");
   gap_val_set_keyword(keylist, "(count_timecode_steps ", &master_handle->count_timecode_steps, GAP_VAL_GINT32, 0, "\0");

   gap_val_set_keyword(keylist, "(prefere_native_seek ", &master_handle->prefere_native_seek, GAP_VAL_GBOOLEAN, 0, "\0");
   gap_val_set_keyword(keylist, "(all_timecodes_verified ", &master_handle->all_timecodes_verified, GAP_VAL_GBOOLEAN, 0, "\0");
   gap_val_set_keyword(keylist, "(critical_timecodesteps_found ", &master_handle->critical_timecodesteps_found, GAP_VAL_GBOOLEAN, 0, "\0");

   for (ii=0; ii < count_timecode_steps; ii++)
   {
      char l_keyword[100];
      g_snprintf(&l_keyword[0], sizeof(l_keyword), "(timecode_steps[%d] ", ii);
      gap_val_set_keyword(keylist, &l_keyword[0], &master_handle->timecode_steps[ii], GAP_VAL_GINT32, 0, "\0");
   }

}  /* end p_set_analysefile_master_keywords */


/* ----------------------------
 * p_save_video_analyse_results
 * ----------------------------
 */
static void
p_save_video_analyse_results(t_GVA_Handle *gvahand)
{
  GapValKeyList *keylist;
  t_GVA_ffmpeg *master_handle;
  FILE *fp_analyse;
  gint  ii;
  char *analysefile_name;

  master_handle = (t_GVA_ffmpeg *)gvahand->decoder_handle;
  analysefile_name = p_create_analysefile_name(gvahand);

  /* current videofile timestamp */
  master_handle->timestamp = gap_file_get_mtime(gvahand->filename);
  master_handle->readsteps_probe_timecode = READSTEPS_PROBE_TIMECODE;


  fp_analyse = g_fopen(analysefile_name, "w");
  if(fp_analyse)
  {
    /* overwrite file with header block (containing general comments on the video) */
    fprintf(fp_analyse, "GVA_SEEK_SUPPORT_");

    if (master_handle->self_test_detected_seek_bug == TRUE)
    {
        fprintf(fp_analyse, "NONE");
    }
    else if (master_handle->video_libavformat_seek_gopsize > 0)
    {
        fprintf(fp_analyse, "NATIVE");
    }
    else
    {
        fprintf(fp_analyse, "VINDEX");
    }
    fprintf(fp_analyse, "\n");
    fprintf(fp_analyse, "# GAP-FFMPEG video analyse results for file: %s\n"
      , gvahand->filename
      );

    {
        int64_t  duration;
        int64_t  duration3;
        int64_t  stt;
        duration = master_handle->vid_input_context->duration;
        duration3 =
          av_rescale_q(duration, AV_TIME_BASE_Q, master_handle->vid_stream->time_base);

        stt = master_handle->vid_input_context->start_time;
        /* scale stat_time from global AV_TIME_BASE to stream specific timecode */
        stt =
          av_rescale_q(stt, AV_TIME_BASE_Q, master_handle->vid_stream->time_base);

        fprintf(fp_analyse
           , "# VINFOs:\n"
             "#  vid_stream->time_base num:%d den:%d\n"
             "#  vid_input_context->start_time:%lld\n"
             "#  vid_input_context->duration:%lld\n"
             "#  (start+duration converted to frames:%d)\n"
             "#  (eof_timecode converted to frames:%d)\n"
             "#  (video-libavformat-seek-gopsize config:%d actual:%d)\n"
             "#  (ffmpeg-libs-version:%s)\n"
           , master_handle->vid_stream->time_base.num
           , master_handle->vid_stream->time_base.den
           , master_handle->vid_input_context->start_time
           , master_handle->vid_input_context->duration
           , p_timecode_to_frame_nr(master_handle, stt+duration3)
           , p_timecode_to_frame_nr(master_handle, master_handle->eof_timecode)
           , gap_base_get_gimprc_int_value("video-libavformat-seek-gopsize", DEFAULT_NAT_SEEK_GOPSIZE, 0, MAX_NAT_SEEK_GOPSIZE)
           , master_handle->video_libavformat_seek_gopsize
           , GAP_FFMPEG_VERSION_STRING
           );
    }
    fclose(fp_analyse);

    keylist = gap_val_new_keylist();

    /* setup key/value descriptions */
    master_handle->libavcodec_version_int = LIBAVCODEC_VERSION_INT;
    p_set_analysefile_master_keywords(keylist, gvahand, master_handle->count_timecode_steps);
 

    /* save key/value data */
    gap_val_rewrite_file(keylist, analysefile_name
        , NULL /* const char *hdr_text */
        , ")"  /* const char *term_str */
        );

    gap_val_free_keylist(keylist);

  }
  g_free(analysefile_name);

}  /* end p_save_video_analyse_results */


/* ----------------------------
 * p_get_video_analyse_results
 * ----------------------------
 * return
 *   TRUE  persitent analyse results are availabe (caller can skip the selftest)
 *   FALSE persitent analyse results are NOT availabe or no longer valid
 *        (the caller must perform the selftest)
 */
static gboolean
p_get_video_analyse_results(t_GVA_Handle *gvahand)
{
#define UNDEFINED_TIMECODE_STEP_VALUE -44444444
  int scanned_items;
  int min_expected_items;
  GapValKeyList *keylist;
  t_GVA_ffmpeg *master_handle;
  char *analysefile_name;
  gint32 curr_mtime;
  gint ii;
  gboolean ret;


  analysefile_name = p_create_analysefile_name(gvahand);
  master_handle = (t_GVA_ffmpeg *)gvahand->decoder_handle;
  keylist = gap_val_new_keylist();

  /* init verion with 0
   * when loading older analyze files that do not contain such version information
   * the version will keep the inital 0 value after loading.
   */
  master_handle->libavcodec_version_int = 0;
  p_set_analysefile_master_keywords(keylist, gvahand, READSTEPS_PROBE_TIMECODE);

  /* init structures with some non-plausible values
   * (will be overwritten on sucessful fetch)
   */
  for(ii=0; ii < READSTEPS_PROBE_TIMECODE; ii++)
  {
    master_handle->timecode_steps[ii] = UNDEFINED_TIMECODE_STEP_VALUE;
  }

  min_expected_items = 10;
  master_handle->count_timecode_steps = -1;

  scanned_items = gap_val_scann_filevalues(keylist, analysefile_name);
  gap_val_free_keylist(keylist);
  curr_mtime = gap_file_get_mtime(gvahand->filename);

  ret = TRUE;

  /* check if we got plausible values from persitent storage that are still valid */
  if ((scanned_items <  min_expected_items)
  || (master_handle->count_timecode_steps <= 0)
  || (master_handle->count_timecode_steps > READSTEPS_PROBE_TIMECODE)
  || (p_equal_mtime(master_handle->timestamp, curr_mtime) != TRUE))
  {
    /* perform analyse */
    ret = FALSE;
  }

  if (ret == TRUE)
  {
    if (master_handle->timecode_steps[master_handle->count_timecode_steps -1] == UNDEFINED_TIMECODE_STEP_VALUE)
    {
      ret = FALSE;
    }
  }

  if(gap_debug)
  {
    printf("p_get_video_analyse_results:\n");
    printf("  analysefile:%s"
           " min_expected_items:%d  scanned_items:%d\n"
           " master_handle->timestamp:%d  curr_mtime:%d\n"
      ,analysefile_name
      ,(int)min_expected_items
      ,(int)scanned_items
      ,(int)master_handle->timestamp
      ,(int)curr_mtime
      );
  }

  g_free(analysefile_name);
  return (ret);

}  /* end p_get_video_analyse_results */


/* ----------------------------
 * p_create_analysefile_name
 * ----------------------------
 * the caller is responsible to g_free the returned string
 */
static char*
p_create_analysefile_name(t_GVA_Handle *gvahand)
{
  char *vindex_name;

  vindex_name = p_build_gvaidx_filename(gvahand->filename
                                       , gvahand->vid_track
                                       , GVA_FFMPEG_DECODER_NAME
                                       , "analyze");
  return (vindex_name);
}  /* end p_create_analysefile_name */

/* ------------------------------
 * p_timecode_to_frame_nr
 * ------------------------------
 */
static gint32
p_timecode_to_frame_nr(t_GVA_ffmpeg *handle, int64_t timecode)
{
  int64_t framenr;
  int64_t l_timecode;
  int64_t l_group_timecode;

  framenr = 0;
  l_group_timecode = 0;
  l_timecode = timecode - handle->timecode_offset_frame1;

  if (handle->timecode_steps_sum > 0)
  {
    int ii;

    framenr = l_timecode / (int64_t)handle->timecode_steps_sum;
    l_group_timecode = framenr * (int64_t)handle->timecode_steps_sum;
    framenr *= handle->count_timecode_steps;

    l_timecode -= l_group_timecode;

    for(ii=0; ii < handle->count_timecode_steps; ii++)
    {
      if (l_timecode > 0)
      {
        framenr++;
        l_timecode -= handle->timecode_steps[ii];
      }
    }
  }

  framenr++;

  if(gap_debug)
  {
    printf("p_timecode_to_frame_nr: framenr:%lld  timecode:%lld offset_frame1:%lld\n"
         , framenr
         , timecode
         , handle->timecode_offset_frame1
         );
  }
  return (framenr);
}  /* end p_timecode_to_frame_nr */


/* ------------------------------
 * p_frame_nr_to_timecode
 * ------------------------------
 */
static int64_t
p_frame_nr_to_timecode(t_GVA_ffmpeg *handle, gint32 frame_nr)
{
  int64_t  timecode;

  if ((handle->count_timecode_steps <= 1)
  ||  (frame_nr <= 1))
  {
    /* videofile with constant frametiming */
    timecode = (handle->timecode_offset_frame1
          + ((int64_t)(frame_nr -1) * handle->timecode_steps[0])
          );
  }
  else
  {
    int64_t  frame_groups;
    int64_t  frame_idx;
    int ii;
    /* videofile with varying frametiming pattern (as detected by proberead)
     * a typical example uses alternating frametiming of 3003, 4505
     * with a pattern size value of 2 (handle->count_timecode_steps == 2)
     *
     * frame_nr   timecode  remarks
     * 1.         100000    timecode_offset_frame1
     * 2.         103003    (+3003)
     * 3.         107508    (+4505)
     * 4.         110511    (+3003)
     * 5.         115016    (+4505)
     */

    frame_groups = (frame_nr -2) / handle->count_timecode_steps;
    frame_idx = (frame_nr -2) % handle->count_timecode_steps;

    timecode = handle->timecode_offset_frame1
            + (frame_groups * (int64_t)handle->timecode_steps_sum);
    for(ii=0; ii <= frame_idx; ii++)
    {
      timecode += handle->timecode_steps[ii];
    }
  }

  if(gap_debug)
  {
    printf("FRAMENR %ld  to TIMECODE:%lld  offset for frame1:%lld step[0]:%d steps:%d steps_sum:%d\n"
      , frame_nr
      , timecode
      , handle->timecode_offset_frame1
      , (int)handle->timecode_steps[0]
      , (int)handle->count_timecode_steps
      , (int)handle->timecode_steps_sum
      );
  }

  return(timecode);

}  /* end p_frame_nr_to_timecode */

/* ------------------------------
 * p_analyze_stepsize_pattern
 * ------------------------------
 * try to findout repeating stepsize pattern,
 *  based on the timecode_steps[] array that must be already filled
 *  up to max_idx elements.
 * Result: set the count_timecode_steps
 *
 */
static void
p_analyze_stepsize_pattern(gint max_idx, t_GVA_Handle *gvahand)
{
  gint ii;
  gint jj;
  t_GVA_ffmpeg *master_handle;
  int gap_debug_local;

  gap_debug_local = gap_debug;

#ifdef GAP_DEBUG_FF_NATIVE_SEEK
  gap_debug_local = 1;
#endif /* GAP_DEBUG_FF_NATIVE_SEEK */

  master_handle = (t_GVA_ffmpeg *)gvahand->decoder_handle;
  master_handle->count_timecode_steps = 1;
  ii=1;
  while(ii < max_idx)
  {
    if(gap_debug)
    {
      printf("loop ii:%d\n", ii);
    }
    for(jj=0; jj < ii; jj++)
    {
      if ((ii + jj) >= max_idx)
      {
        break;
      }
      if(master_handle->timecode_steps[jj] != master_handle->timecode_steps[ii + jj])
      {
        master_handle->count_timecode_steps = ii + 1;
        if(gap_debug)
        {
          printf("ii:%d  steps[jj: %d]:%d   steps[ii+jj: %d]:%d\n"
           ,ii
           ,jj
           ,master_handle->timecode_steps[jj]
           ,ii+jj
           ,master_handle->timecode_steps[ii + jj]
           );
        }
        break;
      }
    }
    if((jj == ii) || (jj+ii == max_idx))
    {
      ii += jj;
    }
    else
    {
      ii++;
    }
  }

  if (master_handle->count_timecode_steps == max_idx)
  {
    printf("WARNING: p_analyze_stepsize_pattern video:%s has individual frame timing\n"
          , gvahand->filename
          );
    printf("   native timecode based seek will not find exact position by frame number\n");
  }

  master_handle->timecode_steps_sum = 0;
  for(ii=0; ii < master_handle->count_timecode_steps; ii++)
  {
    master_handle->timecode_steps_sum += master_handle->timecode_steps[ii];
  }


  if(gap_debug_local)
  {
    printf("PROBEREAD: p_analyze_stepsize_pattern  video:%s max_idx.%d count_timecode_steps:%d sum:%d\n"
          , gvahand->filename
          , max_idx
          , master_handle->count_timecode_steps
          , master_handle->timecode_steps_sum
          );
  }

}  /* end p_analyze_stepsize_pattern */



/* ------------------------------
 * p_probe_timecode_offset
 * ------------------------------
 * if timecode offset for the 1st frame and typical stepsize(s) are not yet known
 * determine those values by probe reading some video frames (at least 2).
 *
 * Note that duration of frames may not be constant for all videofiles.
 * for videos with very individual frame durations it is not possible
 * to calculate correct timecode for a given frame number.
 * Typically most (DVD) videos use individual frame timing
 * in cyclic pattern manner. This probe read analyzes the type of pattern
 * and enables correct frame to timestamp conversion
 * based on the measured resulting array timecode_steps[]
 * (this is required for exact positioning via native seek operations)
 *
 * this procedure opens its own copy handle for the probe read
 * (the stream position of the orignal gvahand are not affected)
 * but writes the results of the probe read to the attributes
 * of the master handle.
 */
static void
p_probe_timecode_offset(t_GVA_Handle *master_gvahand)
{
  t_GVA_ffmpeg *master_handle;

  master_handle = (t_GVA_ffmpeg *)master_gvahand->decoder_handle;


  if (master_handle->timecode_proberead_done != TRUE)
  {
    gint l_readsteps;
    t_GVA_RetCode l_rc_rd;
    t_GVA_Handle   *copy_gvahand;
    t_GVA_ffmpeg   *copy_handle;
    gdouble avg_fstepsize;
    int64_t prev_timecode;
    gint32 l_countValidTimecodes;
    gint32 l_countZeroTimecodes;

    l_countValidTimecodes = 0;
    l_countZeroTimecodes = 0;
    master_handle->timecode_proberead_done = TRUE;
    master_handle->timecode_step_abs_min = 99999999;

    /* calculate average timecode step per frame via framerate */
    avg_fstepsize = 100000.0 / master_gvahand->framerate;
    master_handle->timecode_steps[0] = avg_fstepsize;

    if(gap_debug)
    {
      printf("p_probe_timecode_offset: %d Probe Reads to detect timecode offset\n"
            , READSTEPS_PROBE_TIMECODE
            );
    }

    /* open an extra handle for the probe read */
    copy_gvahand = g_malloc0(sizeof(t_GVA_Handle));
    p_wrapper_ffmpeg_open_read(master_gvahand->filename
                              , copy_gvahand
                              );
    copy_handle = (t_GVA_ffmpeg *)copy_gvahand->decoder_handle;
    copy_gvahand->current_frame_nr = 0;
    copy_gvahand->current_seek_nr = 1;
    copy_gvahand->vindex = NULL;
    copy_handle->dummy_read = TRUE;

    l_rc_rd = p_wrapper_ffmpeg_get_next_frame(copy_gvahand);
    
    master_handle->timecode_offset_frame1 = copy_handle->vid_pkt.dts;
//     if(copy_handle->pkt1_dts != AV_NOPTS_VALUE)
//     {
//       master_handle->timecode_offset_frame1 = copy_handle->pkt1_dts;
//     }
    
    if(gap_debug)
    {
       printf("GOT master_handle->timecode_offset_frame1:%lld copy_handle->vid_pkt.dts:%lld dts1:%lld\n"
                  , master_handle->timecode_offset_frame1
                  , copy_handle->vid_pkt.dts
                  , copy_handle->pkt1_dts
                  );
    }

    prev_timecode = copy_handle->vid_pkt.dts;

    l_readsteps = 0;
    while(l_readsteps < READSTEPS_PROBE_TIMECODE)
    {
      l_rc_rd = p_wrapper_ffmpeg_get_next_frame(copy_gvahand);
      if (l_rc_rd != GVA_RET_OK)
      {
        break;
      }

      if (copy_handle->vid_pkt.dts != AV_NOPTS_VALUE)
      {
        l_countValidTimecodes++;
      }
      if (copy_handle->vid_pkt.dts == 0)
      {
        l_countZeroTimecodes++;
      }
      master_handle->timecode_steps[l_readsteps] = copy_handle->vid_pkt.dts - prev_timecode;
      master_handle->timecode_step_abs_min =
         MIN(abs(master_handle->timecode_steps[l_readsteps])
             , master_handle->timecode_step_abs_min);
      l_readsteps++;
      master_handle->timecode_step_avg =
             (copy_handle->vid_pkt.dts - master_handle->timecode_offset_frame1) / l_readsteps;

      if(gap_debug)
      {
        printf("p_probe_timecode_offset: step: (%d) timecode offset: %lld, stepsize:%ld (avg_measured: %ld avg: %.3f)\n"
          , (int)l_readsteps
          , master_handle->timecode_offset_frame1
          , (long)master_handle->timecode_steps[l_readsteps -1]
          , (long)master_handle->timecode_step_avg
          , (float)avg_fstepsize
          );
      }
      prev_timecode = copy_handle->vid_pkt.dts;
    }

    /* close the extra handle (that was opened for counting only) */
    p_wrapper_ffmpeg_close(copy_gvahand);

    if ((l_countValidTimecodes > 0) 
    && (l_countZeroTimecodes < 2))
    {
        p_analyze_stepsize_pattern(l_readsteps, master_gvahand);
    }
    else
    {
       /* some older ffmpeg versions did always deliver valid dts timecodes,
        * even if not present in the video.
        * but unfortunately recent ffmpeg snapshots deliver AV_NOPTS_VALUE as dts
        * for such videos. In this case native seek must be disabled.
        *
        * (but timecode 0 is not OK for framenumbers > 1)
        */
       master_handle->timecode_steps_sum = 0;
       master_handle->count_timecode_steps = 1;
       master_handle->video_libavformat_seek_gopsize = 0; /* DISABLE natvie seek */
       master_handle->prefere_native_seek = FALSE;

       printf("WARNING: p_probe_timecode_offset no valid timecode found in video:%s\n"
          , master_gvahand->filename
          );
       printf("   native timecode based seek not possible\n");

    }

  }

}  /* end p_probe_timecode_offset */


#endif  /* ENABLE_GVA_LIBAVFORMAT */
