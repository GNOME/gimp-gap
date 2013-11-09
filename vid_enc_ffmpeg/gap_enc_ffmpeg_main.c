/*
- TODO:
  - dont_recode_flag  is still experimental code
    but now partly works, but sometimes does not produce clean output videos
    (need further fixes of framerate, pic_number and timecode informations
     in the copied uncompressed frame chunks)
  - p_ffmpeg_write_frame   :    l_force_keyframe
     still need a working methode to force the encoder to write a keyframe
     (in case after a sequence of copied uncompressed chunks that ends up in P or B frame)
     as workaround we can use the option "Intra Only"

- RealVideo fileformat: Video+Audio encoding does not work. (unusable results) playback  blocks.
                        (Video only seems OK)
*/

/* gap_enc_ffmpeg_main.c
 *  by hof (Wolfgang Hofer)
 *
 * GAP ... Gimp Animation Plugins
 *
 * This is the MAIN Module for the GAP FFMEG video encoder
 *  This encoder supports encoding of multiple Video/Audio formats and CODECS
 *  (such as AVI/DivX, MPEG1, MPEG2,..)
 *  it is based on the ffmpeg libraries.
 *
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

/* revision history:
 * version 2.1.0a;  2009.02.07   hof: update to ffmpeg snapshot 2009.01.31 (removed support for older ffmpeg versions)
 * version 2.1.0a;  2005.07.16   hof: base support for encoding of multiple tracks
 *                                    video is still limited to 1 track
 *                                    multiple audio tracks are possible
 *                                    when a playlist (file containing filenames of audiofiles)
 *                                    is passed rather than a single input audiofile
 * version 2.1.0a;  2005.03.27   hof: support ffmpeg  LIBAVCODEC_BUILD  >= 4744,
 *                                    changed Noninteractive PDB-API (use paramfile)
 *                                    changed presets (are still a wild guess)
 * version 2.1.0a;  2004.11.02   hof: added patch from pippin (allows compile with FFMPEG-0.4.0.9pre1)
 *                               support for the incompatible older (stable) FFMPEG 0.4.8 version is available
 * version 2.1.0a;  2004.06.10   hof: removed deinterlace option
 *                               delace was not implemented in the encoder module
 *                               and probably will never be implemented here for reasons a) and b)
 *                               a) the GIMP plug-in-deinterlace can be called via GAP-Filtermacros
 *                               b) for videoframe input the GVA Api provides already deinterlace while fetching frames
 *                                  (see delace options for STORYBOARD VID_FRAMES)
 * version 2.1.0a;  2004.06.05   hof: update params from ffmpeg 0.4.6 to 0.4.8
 * version 1.2.2b;  2003.05.29   hof: created
 */

#include <config.h>


#include <errno.h>

#include <glib/gstdio.h>

#include <gtk/gtk.h>
#include <libgimp/gimp.h>
#include <libgimp/gimpui.h>

#include "swscale.h"

#include "gap-intl.h"

#include "gap_file_util.h"
#include "gap_libgapvidutil.h"
#include "gap_libgimpgap.h"
#include "gap_enc_ffmpeg_main.h"
#include "gap_enc_ffmpeg_gui.h"
#include "gap_enc_ffmpeg_par.h"

#include "gap_audio_wav.h"
#include "gap_base.h"


/* FFMPEG defaults */
#define DEFAULT_PASS_LOGFILENAME "ffmpeg2pass"
#define MAX_AUDIO_PACKET_SIZE 16384

#ifndef DEFAULT_FRAME_RATE_BASE
#define DEFAULT_FRAME_RATE_BASE 1001000
#endif

/* define some Constant values
 * (needed just in case we are compiling with older ffmpeg releases)
 */
#ifdef FF_PROFILE_UNKNOWN
#define GAP_FF_PROFILE_UNKNOWN FF_PROFILE_UNKNOWN
#else
#define GAP_FF_PROFILE_UNKNOWN -99
#endif

#ifdef FF_LEVEL_UNKNOWN
#define GAP_FF_LEVEL_UNKNOWN FF_LEVEL_UNKNOWN
#else
#define GAP_FF_LEVEL_UNKNOWN -99
#endif

#ifdef FF_CMP_DCTMAX
#define GAP_FF_CMP_DCTMAX FF_CMP_DCTMAX
#else
#define GAP_FF_CMP_DCTMAX -99
#endif


#define MAX_VIDEO_STREAMS 1
#define MAX_AUDIO_STREAMS 16

#define ENCODER_QUEUE_RINGBUFFER_SIZE 4
 


typedef struct t_audio_work
{
  FILE *fp_inwav;
  char *referred_wavfile;   /* filename of the reffereed wavfile, extended to absolute path */
  gint32 wavsize; /* Data size of the wav file */
  long audio_margin; /*8192 The audio chunk size (8192) */
  long audio_filled_100;
  long audio_per_frame_x100;
  long frames_per_second_x100;
  gdouble frames_per_second_x100f;
  char *databuffer; /* 300000 dyn. allocated bytes for transferring audio data */

  long  sample_rate;
  long  channels;
  long  bytes_per_sample;
  long  bits;
  long  samples;

} t_audio_work;


typedef struct t_awk_array
{
  gint          audio_tracks;           /* == max_ast */
  t_audio_work  awk[MAX_AUDIO_STREAMS];
} t_awk_array;


typedef struct t_ffmpeg_video
{
 int              video_stream_index;
 AVStream        *vid_stream;
 AVCodecContext  *vid_codec_context;
 AVCodec         *vid_codec;
 AVFrame         *big_picture_codec;
 uint8_t         *video_buffer;
 int              video_buffer_size;
 uint8_t         *video_dummy_buffer;
 int              video_dummy_buffer_size;
 uint8_t         *yuv420_dummy_buffer;
 int              yuv420_dummy_buffer_size;
 FILE            *passlog_fp;
} t_ffmpeg_video;

typedef struct t_ffmpeg_audio
{
 int              audio_stream_index;
 AVStream        *aud_stream;
 AVCodecContext  *aud_codec_context;
 AVCodec         *aud_codec;
 uint8_t         *audio_buffer;
 int              audio_buffer_size;
} t_ffmpeg_audio;




typedef struct t_ffmpeg_handle         /* ffh */
{
 AVFormatContext *output_context;
 AVOutputFormat  *file_oformat;
 AVFormatParameters *ap;

 t_ffmpeg_video  vst[MAX_VIDEO_STREAMS];
 t_ffmpeg_audio  ast[MAX_AUDIO_STREAMS];

 int max_vst;   /* used video streams,  0 <= max_vst <= MAX_VIDEO_STREAMS */
 int max_ast;   /* used audio streams,  0 <= max_ast <= MAX_AUDIO_STREAMS */

 int frame_width;
 int frame_height;

 int audio_bit_rate;
 int audio_codec_id;

 int file_overwrite;

 gdouble pts_stepsize_per_frame;
 gint32 encode_frame_nr;          /* number of frame to encode always starts at 1 and inc by one (base for monotone timecode calculation) */

 struct SwsContext *img_convert_ctx;

 int countVideoFramesWritten;
 uint8_t  *convert_buffer;
 gint32    validEncodeFrameNr;
 
 gboolean      isMultithreadEnabled;

} t_ffmpeg_handle;


typedef enum
{
   EQELEM_STATUS_FREE
  ,EQELEM_STATUS_READY
  ,EQELEM_STATUS_LOCK        
} EncoderQueueElemStatusEnum;


typedef struct EncoderQueueElem  /* eq_elem */
{
  AVFrame                      *eq_big_picture_codec[MAX_VIDEO_STREAMS];   /* picture data to feed the encoder codec */
  gint32                        encode_frame_nr;
  gint                          vid_track;
  gboolean                      force_keyframe;
  
  EncoderQueueElemStatusEnum    status;
  GMutex                       *elemMutex;
  void                         *next;

} EncoderQueueElem;


typedef struct EncoderQueue    /* eque */
{
  gint                numberOfElements;
  EncoderQueueElem   *eq_root;           /* 1st element in the ringbuffer */
  EncoderQueueElem   *eq_write_ptr;      /* reserved for the frame fetcher thread to writes into the ringbuffer */
  EncoderQueueElem   *eq_read_ptr;       /* reserved for the encoder thread to read frames from the ringbuffer */

  t_ffmpeg_handle     *ffh;
  t_awk_array         *awp;
  gint                 runningThreadsCounter;
  
  GCond               *frameEncodedCond;  /* sent each time the encoder finished one frame */
  GMutex              *poolMutex;


  /* debug attributes reserved for runtime measuring in the main thread */
  GapTimmRecord       mainElemMutexWaits;
  GapTimmRecord       mainPoolMutexWaits;
  GapTimmRecord       mainEnqueueWaits;
  GapTimmRecord       mainPush2;
  GapTimmRecord       mainWriteFrame;
  GapTimmRecord       mainDrawableToRgb;
  GapTimmRecord       mainReadFrame;


  /* debug attributes reserved for runtime measuring in the encoder thread */
  GapTimmRecord       ethreadElemMutexWaits;
  GapTimmRecord       ethreadPoolMutexWaits;
  GapTimmRecord       ethreadEncodeFrame;

} EncoderQueue;


/* ------------------------
 * global gap DEBUG switch
 * ------------------------
 */

/* int gap_debug = 1; */    /* print debug infos */
/* int gap_debug = 0; */    /* 0: dont print debug infos */

int gap_debug = 0;

static GThreadPool         *encoderThreadPool = NULL;

GapGveFFMpegGlobalParams global_params;
int global_nargs_ffmpeg_enc_par;

static void query(void);
static void run(const gchar *name
              , gint n_params
              , const GimpParam *param
              , gint *nreturn_vals
              , GimpParam **return_vals);

static int    p_base_get_thread_id_as_int();
static void   p_debug_print_dump_AVCodecContext(AVCodecContext *codecContext);
static int    p_av_metadata_set(AVMetadata **pm, const char *key, const char *value, int flags);
static void   p_set_flag(gint32 value_bool32, int *flag_ptr, int maskbit);
static void   p_set_partition_flag(gint32 value_bool32, gint32 *flag_ptr, gint32 maskbit);


static void   p_gimp_get_data(const char *key, void *buffer, gint expected_size);
static void   p_ffmpeg_init_default_params(GapGveFFMpegValues *epp);

static void              p_init_audio_workdata(t_awk_array *awp);
static void              p_open_inwav_and_buffers(t_awk_array *awp
                                      , const char *wavefile
                                      , gint ii
                                      , long sample_rate
                                      , long channels
                                      , long bytes_per_sample
                                      , long bits
                                      , long samples
                                      );
static void              p_sound_precalculations(t_ffmpeg_handle *ffh
                                      , t_awk_array *awp
                                      , GapGveFFMpegGlobalParams *gpp
                                      );
static void              p_process_audio_frame(t_ffmpeg_handle *ffh, t_awk_array *awp);
static void              p_close_audio_input_files(t_awk_array *awp);
static void              p_open_audio_input_files(t_awk_array *awp, GapGveFFMpegGlobalParams *gpp);

static gint64            p_calculate_current_timecode(t_ffmpeg_handle *ffh);
static gint64            p_calculate_timecode(t_ffmpeg_handle *ffh, int frameNr);
static void              p_set_timebase_from_framerate(AVRational *time_base, gdouble framerate);

static void              p_ffmpeg_open_init(t_ffmpeg_handle *ffh, GapGveFFMpegGlobalParams *gpp);
static gboolean          p_init_video_codec(t_ffmpeg_handle *ffh
                                      , gint ii
                                      , GapGveFFMpegGlobalParams *gpp
                                      , gint32 current_pass);

static gboolean          p_init_and_open_audio_codec(t_ffmpeg_handle *ffh
                                      , gint ii
                                      , GapGveFFMpegGlobalParams *gpp
                                      , long audio_samplerate
                                      , long audio_channels
                                      , long bits
                                      );
static t_ffmpeg_handle * p_ffmpeg_open(GapGveFFMpegGlobalParams *gpp
                                      , gint32 current_pass
                                      , t_awk_array *awp
                                      , gint video_tracks
                                      );
static int    p_ffmpeg_write_frame_chunk(t_ffmpeg_handle *ffh, gint32 encoded_size, gint vid_track);
static void   p_convert_colormodel(t_ffmpeg_handle *ffh, AVPicture *picture_codec, guchar *rgb_buffer, gint vid_track);

static int    p_ffmpeg_encodeAndWriteVideoFrame(t_ffmpeg_handle *ffh, AVFrame *picture_codec
                     , gboolean force_keyframe, gint vid_track, gint32 encode_frame_nr);

static void   p_ffmpeg_convert_GapStoryFetchResult_to_AVFrame(t_ffmpeg_handle *ffh
                     , AVFrame *picture_codec
                     , GapStoryFetchResult *gapStoryFetchResult
                     , gint vid_track
                     );

static void   p_create_EncoderQueueRingbuffer(EncoderQueue *eque);
static EncoderQueue * p_init_EncoderQueueResources(t_ffmpeg_handle *ffh, t_awk_array *awp);
static void           p_debug_print_RingbufferStatus(EncoderQueue *eque);
static void           p_waitUntilEncoderQueIsProcessed(EncoderQueue *eque);
static void           p_free_EncoderQueueResources(EncoderQueue     *eque);

static void   p_fillQueueElem(EncoderQueue *eque, GapStoryFetchResult *gapStoryFetchResult, gboolean force_keyframe, gint vid_track);
static void   p_encodeCurrentQueueElem(EncoderQueue *eque);
static void   p_encoderWorkerThreadFunction (EncoderQueue *eque);
static int    p_ffmpeg_write_frame_and_audio_multithread(EncoderQueue *eque, GapStoryFetchResult *gapStoryFetchResult, gboolean force_keyframe, gint vid_track);




static int    p_ffmpeg_write_frame(t_ffmpeg_handle *ffh, GapStoryFetchResult *gapStoryFetchResult, gboolean force_keyframe, gint vid_track);
static int    p_ffmpeg_write_audioframe(t_ffmpeg_handle *ffh, guchar *audio_buf, int frame_bytes, gint aud_track);
static void   p_ffmpeg_close(t_ffmpeg_handle *ffh);
static gint   p_ffmpeg_encode(GapGveFFMpegGlobalParams *gpp);



GimpPlugInInfo PLUG_IN_INFO =
{
  NULL,  /* init_proc */
  NULL,  /* quit_proc */
  query, /* query_proc */
  run,   /* run_proc */
};


/* ------------------------
 * MAIN query and run
 * ------------------------
 */

MAIN ()

static void
query ()
{
  gchar      *l_ecp_key;

  /* video encoder standard parameters (same for each encoder)  */
  static GimpParamDef args_ffmpeg_enc[] =
  {
    {GIMP_PDB_INT32, "run_mode", "non-interactive"},
    {GIMP_PDB_IMAGE, "image", "Input image (must be one of the input frames, or is ignored if storyboard_file is used"},
    {GIMP_PDB_DRAWABLE, "drawable", "Input drawable (unused)"},
    {GIMP_PDB_STRING, "videofile", "filename of AVI or MPEG video (to write)"},
    {GIMP_PDB_INT32, "range_from", "number of first frame"},
    {GIMP_PDB_INT32, "range_to", "number of last frame"},
    {GIMP_PDB_INT32, "vid_width", "Width of resulting Video Frames (all Frames are scaled to this width)"},
    {GIMP_PDB_INT32, "vid_height", "Height of resulting Video Frames (all Frames are scaled to this height)"},
    {GIMP_PDB_INT32, "vid_format", "videoformat:  0=comp., 1=PAL, 2=NTSC, 3=SECAM, 4=MAC, 5=unspec"},
    {GIMP_PDB_FLOAT, "framerate", "framerate in frames per seconds"},
    {GIMP_PDB_INT32, "samplerate", "audio samplerate in samples per seconds (is ignored .wav files are used)"},
    {GIMP_PDB_STRING, "audfile", "optional audiodata file .wav must contain uncompressed 16 bit samples."
                                 " pass empty string if no audiodata should be included."
                                 " optional you can pass a textfile as audiofile. this textfile must contain "
                                 " one filename of a .wav file per line and allows encoding of "
                                 " multiple audio tracks because each of the specified audiofiles "
                                 " is encoded as ist own audiostream (works only if the selected multimedia fileformat "
                                 " has multiple audio track support. For example multilingual MPEG2 .vob files "
                                 " do support multiple audio streams)"},
    {GIMP_PDB_INT32, "use_rest", "0 == use default values for all encoder specific params, "
                                 "1 == use encoder specific params set by the parameter procedure:"
                                 GAP_PLUGIN_NAME_FFMPEG_PARAMS
                                 },
    {GIMP_PDB_STRING, "filtermacro_file", "macro to apply on each handled frame. (textfile with filter plugin names and LASTVALUE bufferdump"},
    {GIMP_PDB_STRING, "storyboard_file", "textfile with list of one or more framesequences"},
    {GIMP_PDB_INT32,  "input_mode", "0 ... image is one of the frames to encode, range_from/to params refere to numberpart of the other frameimages on disc. \n"
                                    "1 ... image is multilayer, range_from/to params refere to layer index. \n"
                                    "2 ... image is ignored, input is specified by storyboard_file parameter."},
    {GIMP_PDB_INT32, "master_encoder_id", "id of the master encoder that called this plug-in (typically the pid)"},
  };
  static int nargs_ffmpeg_enc = sizeof(args_ffmpeg_enc) / sizeof(args_ffmpeg_enc[0]);

  /* video encoder specific parameters */
  static GimpParamDef args_ffmpeg_enc_par[] =
  {
    {GIMP_PDB_INT32, "run_mode", "interactive, non-interactive"},
    {GIMP_PDB_STRING, "key_stdpar", "key to get standard video encoder params via gimp_get_data"},

    {GIMP_PDB_STRING, "ffmpeg_encode_parameter_file", "name of an ffmpeg encoder parameter file."
                                                      " Such a file can be created via save button in the"
                                                      " INTERACTIVE runmode"},


  };
  static int nargs_ffmpeg_enc_par = sizeof(args_ffmpeg_enc_par) / sizeof(args_ffmpeg_enc_par[0]);

  static GimpParamDef *return_vals = NULL;
  static int nreturn_vals = 0;

  /* video encoder standard query (same for each encoder) */
  static GimpParamDef args_in_ecp[] =
  {
    {GIMP_PDB_STRING, "param_name", "name of the parameter, supported: menu_name, video_extension"},
  };

  static GimpParamDef args_out_ecp[] =
  {
    {GIMP_PDB_STRING, "param_value", "parmeter value"},
  };

  static int nargs_in_enp = sizeof(args_in_ecp) / sizeof(args_in_ecp[0]);
  static int nargs_out_enp = (sizeof(args_out_ecp) / sizeof(args_out_ecp[0]));

  INIT_I18N();

  global_nargs_ffmpeg_enc_par = nargs_ffmpeg_enc_par;

  gimp_install_procedure(GAP_PLUGIN_NAME_FFMPEG_ENCODE,
                         _("ffmpeg video encoding for anim frames. Menu: @FFMPEG@"),
                         _("This plugin does video encoding of animframes based on libavformat."
                         " (also known as FFMPEG encoder)."
                         " The (optional) audiodata must be RIFF WAVEfmt (.wav) file."
                         " .wav files can be mono (1) or stereo (2channels) audiodata and must be 16bit uncompressed."
                         " IMPORTANT:  Non-interactive callers should first call "
                         "\"" GAP_PLUGIN_NAME_FFMPEG_PARAMS  "\""
                         " to set encoder specific paramters, then set the use_rest parameter to 1 to use them."),
                         "Wolfgang Hofer (hof@gimp.org)",
                         "Wolfgang Hofer",
                         GAP_VERSION_WITH_DATE,
                         NULL,                      /* has no Menu entry, just a PDB interface */
                         "RGB*, INDEXED*, GRAY*",
                         GIMP_PLUGIN,
                         nargs_ffmpeg_enc, nreturn_vals,
                         args_ffmpeg_enc, return_vals);



  gimp_install_procedure(GAP_PLUGIN_NAME_FFMPEG_PARAMS,
                         _("Set parameters for GAP ffmpeg video encoder Plugin"),
                         _("This plugin sets ffmpeg specific video encoding parameters."
                           " Non-interactive callers must provide a parameterfile,"
                           " Interactive calls provide a dialog window to specify and optionally save the parameters."),
                         "Wolfgang Hofer (hof@gimp.org)",
                         "Wolfgang Hofer",
                         GAP_VERSION_WITH_DATE,
                         NULL,                      /* has no Menu entry, just a PDB interface */
                         NULL,
                         GIMP_PLUGIN,
                         nargs_ffmpeg_enc_par, nreturn_vals,
                         args_ffmpeg_enc_par, return_vals);


  l_ecp_key = g_strdup_printf("%s%s", GAP_QUERY_PREFIX_VIDEO_ENCODERS, GAP_PLUGIN_NAME_FFMPEG_ENCODE);
  gimp_install_procedure(l_ecp_key,
                         _("Get GUI parameters for GAP ffmpeg video encoder"),
                         _("This plugin returns ffmpeg encoder specific parameters."),
                         "Wolfgang Hofer (hof@gimp.org)",
                         "Wolfgang Hofer",
                         GAP_VERSION_WITH_DATE,
                         NULL,                      /* has no Menu entry, just a PDB interface */
                         NULL,
                         GIMP_PLUGIN,
                         nargs_in_enp , nargs_out_enp,
                         args_in_ecp, args_out_ecp);


  g_free(l_ecp_key);
}       /* end query */


static void
run (const gchar      *name,
     gint              n_params,
     const GimpParam  *param,
     gint             *nreturn_vals,
     GimpParam       **return_vals)
{
  GapGveFFMpegValues *epp;
  GapGveFFMpegGlobalParams *gpp;

  static GimpParam values[2];
  gint32     l_rc;
  const char *l_env;
  char       *l_ecp_key1;
  char       *l_encoder_key;

  gpp = &global_params;
  epp = &gpp->evl;
  *nreturn_vals = 1;
  *return_vals = values;
  l_rc = 0;

  INIT_I18N();

  values[0].type = GIMP_PDB_STATUS;
  values[0].data.d_status = GIMP_PDB_SUCCESS;

  l_env = g_getenv("GAP_DEBUG_ENC");
  if(l_env != NULL)
  {
    if((*l_env != 'n') && (*l_env != 'N')) gap_debug = 1;
  }

  if(gap_debug) printf("\n\nSTART of PlugIn: %s\n", name);

  l_ecp_key1 = g_strdup_printf("%s%s", GAP_QUERY_PREFIX_VIDEO_ENCODERS, GAP_PLUGIN_NAME_FFMPEG_ENCODE);
  l_encoder_key = g_strdup(GAP_PLUGIN_NAME_FFMPEG_ENCODE);


  p_ffmpeg_init_default_params(epp);

  if (strcmp (name, l_ecp_key1) == 0)
  {
      /* this interface replies to the queries of the common encoder gui */
      gchar *param_name;

      param_name = param[0].data.d_string;
      if(gap_debug) printf("query for param_name: %s\n", param_name);
      *nreturn_vals = 2;

      values[1].type = GIMP_PDB_STRING;
      if(strcmp (param_name, GAP_VENC_PAR_MENU_NAME) == 0)
      {
        values[1].data.d_string = g_strdup("FFMPEG");
      }
      else if (strcmp (param_name, GAP_VENC_PAR_VID_EXTENSION) == 0)
      {
        /* check if current extension was already set
         * (in a previous call to the encoder GUI Procedure)
         */
        if(gimp_get_data_size(GAP_FFMPEG_CURRENT_VID_EXTENSION) == sizeof(gpp->evl.current_vid_extension))
        {
          gimp_get_data(GAP_FFMPEG_CURRENT_VID_EXTENSION, gpp->evl.current_vid_extension);
          values[1].data.d_string = g_strdup(gpp->evl.current_vid_extension);
        }
        else
        {
          /* if nothing was set use .avi as the default extension */
          values[1].data.d_string = g_strdup(".avi");
        }
      }
      else if (strcmp (param_name, GAP_VENC_PAR_SHORT_DESCRIPTION) == 0)
      {
        values[1].data.d_string =
          g_strdup(_("FFMPEG Encoder\n"
                     "writes AVI/DivX or MPEG1, MPEG2 (DVD) or MPEG4 encoded videos\n"
                     "based on FFMPEG by Fabrice Bellard"
                     )
                  );
      }
      else if (strcmp (param_name, GAP_VENC_PAR_GUI_PROC) == 0)
      {
        values[1].data.d_string = g_strdup(GAP_PLUGIN_NAME_FFMPEG_PARAMS);
      }
      else
      {
        values[1].data.d_string = g_strdup("\0");
      }
  }
  else if (strcmp (name, GAP_PLUGIN_NAME_FFMPEG_PARAMS) == 0)
  {
      /* this interface sets the encoder specific parameters */
      gint l_set_it;
      gchar  *l_key_stdpar;

      gpp->val.run_mode = param[0].data.d_int32;
      l_key_stdpar = param[1].data.d_string;
      gpp->val.vid_width = 320;
      gpp->val.vid_height = 200;
      p_gimp_get_data(l_key_stdpar, &gpp->val, sizeof(GapGveCommonValues));

      if(gap_debug)  printf("rate: %f  w:%d h:%d\n", (float)gpp->val.framerate, (int)gpp->val.vid_width, (int)gpp->val.vid_height);

      l_set_it = TRUE;
      if (gpp->val.run_mode == GIMP_RUN_NONINTERACTIVE)
      {
        /* set video encoder specific params */
        if (n_params != global_nargs_ffmpeg_enc_par)
        {
          values[0].data.d_status = GIMP_PDB_CALLING_ERROR;
          l_set_it = FALSE;
        }
        else
        {
           /* get parameters from file in NON_INTERACTIVE mode */
           if(param[2].data.d_string)
           {
             g_snprintf(gpp->ffpar_filename, sizeof(gpp->ffpar_filename), "%s", param[2].data.d_string);
             if(g_file_test(gpp->ffpar_filename, G_FILE_TEST_EXISTS))
             {
               gap_ffpar_get(gpp->ffpar_filename, &gpp->evl);
               l_set_it = TRUE;
             }
             else
             {
               values[0].data.d_status = GIMP_PDB_EXECUTION_ERROR;
               l_set_it = FALSE;
             }
           }
           else
           {
             values[0].data.d_status = GIMP_PDB_EXECUTION_ERROR;
             l_set_it = FALSE;
           }

        }
      }
      else
      {
        /* try to read encoder specific params */
        p_gimp_get_data(l_encoder_key, epp, sizeof(GapGveFFMpegValues));

        if(0 != gap_enc_ffgui_ffmpeg_encode_dialog(gpp))
        {
          l_set_it = FALSE;
        }
      }

      if(l_set_it)
      {
         if(gap_debug) printf("Setting Encoder specific Params\n");
         gimp_set_data(l_encoder_key, epp, sizeof(GapGveFFMpegValues));
         gimp_set_data(GAP_FFMPEG_CURRENT_VID_EXTENSION
                       , &gpp->evl.current_vid_extension[0]
                       , sizeof(gpp->evl.current_vid_extension));

      }

  }
  else   if (strcmp (name, GAP_PLUGIN_NAME_FFMPEG_ENCODE) == 0)
  {
      char *l_base;
      int   l_l;

      /* run the video encoder procedure */

      gpp->val.run_mode = param[0].data.d_int32;

      /* get image_ID and animinfo */
      gpp->val.image_ID    = param[1].data.d_image;
      gap_gve_misc_get_ainfo(gpp->val.image_ID, &gpp->ainfo);

      /* set initial (default) values */
      l_base = g_strdup(gpp->ainfo.basename);
      l_l = strlen(l_base);

      if (l_l > 0)
      {
         if(l_base[l_l -1] == '_')
         {
           l_base[l_l -1] = '\0';
         }
      }
      if(gap_debug) printf("Init Default parameters for %s base: %s\n", name, l_base);
      g_snprintf(gpp->val.videoname, sizeof(gpp->val.videoname), "%s.mpg", l_base);

      gpp->val.audioname1[0] = '\0';
      gpp->val.filtermacro_file[0] = '\0';
      gpp->val.storyboard_file[0] = '\0';
      gpp->val.framerate = gpp->ainfo.framerate;
      gpp->val.range_from = gpp->ainfo.curr_frame_nr;
      gpp->val.range_to   = gpp->ainfo.last_frame_nr;
      gpp->val.samplerate = 0;
      gpp->val.vid_width  = gimp_image_width(gpp->val.image_ID) - (gimp_image_width(gpp->val.image_ID) % 16);
      gpp->val.vid_height = gimp_image_height(gpp->val.image_ID) - (gimp_image_height(gpp->val.image_ID) % 16);
      gpp->val.vid_format = VID_FMT_NTSC;
      gpp->val.input_mode = GAP_RNGTYPE_FRAMES;

      g_free(l_base);

      if (n_params != GAP_VENC_NUM_STANDARD_PARAM)
      {
        values[0].data.d_status = GIMP_PDB_CALLING_ERROR;
      }
      else
      {
        if(gap_debug) printf("Reading Standard parameters for %s\n", name);

        if (param[3].data.d_string[0] != '\0') { g_snprintf(gpp->val.videoname, sizeof(gpp->val.videoname), "%s", param[3].data.d_string); }
        if (param[4].data.d_int32 >= 0) { gpp->val.range_from =    param[4].data.d_int32; }
        if (param[5].data.d_int32 >= 0) { gpp->val.range_to   =    param[5].data.d_int32; }
        if (param[6].data.d_int32 > 0)  { gpp->val.vid_width  =    param[6].data.d_int32; }
        if (param[7].data.d_int32 > 0)  { gpp->val.vid_height  =   param[7].data.d_int32; }
        if (param[8].data.d_int32 > 0)  { gpp->val.vid_format  =   param[8].data.d_int32; }
        gpp->val.framerate   = param[9].data.d_float;
        gpp->val.samplerate  = param[10].data.d_int32;
        g_snprintf(gpp->val.audioname1, sizeof(gpp->val.audioname1), "%s", param[11].data.d_string);

        /* use ffmpeg specific encoder parameters (0==run with default values) */
        if (param[12].data.d_int32 == 0)
        {
          if(gap_debug) printf("Running the Encoder %s with Default Values\n", name);
        }
        else
        {
          /* try to read encoder specific params */
          p_gimp_get_data(name, epp, sizeof(GapGveFFMpegValues));
        }
        if (param[13].data.d_string[0] != '\0') { g_snprintf(gpp->val.filtermacro_file, sizeof(gpp->val.filtermacro_file), "%s", param[13].data.d_string); }
        if (param[14].data.d_string[0] != '\0') { g_snprintf(gpp->val.storyboard_file, sizeof(gpp->val.storyboard_file), "%s", param[14].data.d_string); }
        if (param[15].data.d_int32 >= 0) { gpp->val.input_mode   =    param[15].data.d_int32; }

        gpp->val.master_encoder_id = param[16].data.d_int32;
      }

      if (values[0].data.d_status == GIMP_PDB_SUCCESS)
      {
         if (l_rc >= 0 )
         {
            l_rc = p_ffmpeg_encode(gpp);
            /* delete images in the cache
             * (the cache may have been filled while parsing
             * and processing a storyboard file)
             */
             gap_gve_story_drop_image_cache();
         }
      }
  }
  else
  {
      values[0].data.d_status = GIMP_PDB_CALLING_ERROR;
  }

 if(l_rc < 0)
 {
    values[0].data.d_status = GIMP_PDB_EXECUTION_ERROR;
 }

}       /* end run */

static guint64
p_timespecDiff(GTimeVal *startTimePtr, GTimeVal *endTimePtr)
{
  return ((endTimePtr->tv_sec * G_USEC_PER_SEC) + endTimePtr->tv_usec) -
           ((startTimePtr->tv_sec * G_USEC_PER_SEC) + startTimePtr->tv_usec);
}

static int
p_base_get_thread_id_as_int()
{
  int tid;
  
  tid = gap_base_get_thread_id();
  return (tid);
}

/* ---------------------------------------
 * p_debug_print_dump_AVCodecContext
 * ---------------------------------------
 * dump most parameters values
 * of the codec context to stdout
 * (some unused and deprecated parameters are skipped)
 */
static void
p_debug_print_dump_AVCodecContext(AVCodecContext *codecContext)
{
  printf("AVCodecContext Settings START\n");

  printf("(av_class (ptr AVClass*)      %d)\n", (int)    codecContext->av_class);
  printf("(bit_rate                     %d)\n", (int)    codecContext->bit_rate);
  printf("(bit_rate_tolerance           %d)\n", (int)    codecContext->bit_rate_tolerance);
  printf("(flags                        %d)\n", (int)    codecContext->flags);
  printf("(sub_id                       %d)\n", (int)    codecContext->sub_id);
  printf("(me_method                    %d)\n", (int)    codecContext->me_method);
  printf("(extradata (ptr uint8)        %d)\n", (int)    codecContext->extradata);
  printf("(extradata_size               %d)\n", (int)    codecContext->extradata_size);
  printf("(time_base.num                %d)\n", (int)    codecContext->time_base.num);
  printf("(time_base.den                %d)\n", (int)    codecContext->time_base.den);
  printf("(width                        %d)\n", (int)    codecContext->width);
  printf("(height                       %d)\n", (int)    codecContext->height);
  printf("(gop_size                     %d)\n", (int)    codecContext->gop_size);
  printf("(pix_fmt                      %d)\n", (int)    codecContext->pix_fmt);
  printf("(rate_emu                     %d)\n", (int)    codecContext->rate_emu);
  printf("(draw_horiz_band (fptr)       %d)\n", (int)    codecContext->draw_horiz_band);
  printf("(sample_rate                  %d)\n", (int)    codecContext->sample_rate);
  printf("(channels                     %d)\n", (int)    codecContext->channels);
  printf("(sample_fmt                   %d)\n", (int)    codecContext->sample_fmt);
  printf("(frame_size    (tabu)         %d)\n", (int)    codecContext->frame_size);
  printf("(frame_number  (tabu)         %d)\n", (int)    codecContext->frame_number);
  printf("(real_pict_num (tabu)         %d)\n", (int)    codecContext->real_pict_num);
  printf("(delay                        %d)\n", (int)    codecContext->delay);
  printf("(qcompress                    %f)\n", (float)  codecContext->qcompress);
  printf("(qblur                        %f)\n", (float)  codecContext->qblur);
  printf("(qmin                         %d)\n", (int)    codecContext->qmin);
  printf("(qmax                         %d)\n", (int)    codecContext->qmax);
  printf("(max_qdiff                    %d)\n", (int)    codecContext->max_qdiff);
  printf("(max_b_frames                 %d)\n", (int)    codecContext->max_b_frames);
  printf("(b_quant_factor               %f)\n", (float)  codecContext->b_quant_factor);
  printf("(rc_strategy  (OBSOLETE)      %d)\n", (int)    codecContext->rc_strategy);
  printf("(b_frame_strategy             %d)\n", (int)    codecContext->b_frame_strategy);
  printf("(hurry_up                     %d)\n", (int)    codecContext->hurry_up);
  printf("(codec (AVCodec *)            %d)\n", (int)    codecContext->codec);
  printf("(priv_data (void *)           %d)\n", (int)    codecContext->priv_data);
  printf("(rtp_payload_size             %d)\n", (int)    codecContext->rtp_payload_size);
  printf("(rtp_callback (fptr)          %d)\n", (int)    codecContext->rtp_callback);
  printf("(mv_bits                      %d)\n", (int)    codecContext->mv_bits);
  printf("(header_bits                  %d)\n", (int)    codecContext->header_bits);
  printf("(i_tex_bits                   %d)\n", (int)    codecContext->i_tex_bits);
  printf("(p_tex_bits                   %d)\n", (int)    codecContext->p_tex_bits);
  printf("(i_count                      %d)\n", (int)    codecContext->i_count);
  printf("(p_count                      %d)\n", (int)    codecContext->p_count);
  printf("(skip_count                   %d)\n", (int)    codecContext->skip_count);
  printf("(misc_bits                    %d)\n", (int)    codecContext->misc_bits);
  printf("(frame_bits                   %d)\n", (int)    codecContext->frame_bits);
  printf("(opaque (void *)              %d)\n", (int)    codecContext->opaque);
  printf("(codec_name                   %s)\n",         &codecContext->codec_name[0]);
  printf("(codec_type                   %d)\n", (int)    codecContext->codec_type);
  printf("(codec_id                     %d)\n", (int)    codecContext->codec_id);
  printf("(codec_tag (fourcc)           %d)\n", (int)    codecContext->codec_tag);
  printf("(workaround_bugs              %d)\n", (int)    codecContext->workaround_bugs);
  printf("(luma_elim_threshold          %d)\n", (int)    codecContext->luma_elim_threshold);
  printf("(chroma_elim_threshold        %d)\n", (int)    codecContext->chroma_elim_threshold);
  printf("(strict_std_compliance        %d)\n", (int)    codecContext->strict_std_compliance);
  printf("(b_quant_offset               %f)\n", (float)  codecContext->b_quant_offset);
  printf("(error_recognition            %d)\n", (int)    codecContext->error_recognition);
  printf("(get_buffer  (fptr)           %d)\n", (int)    codecContext->get_buffer);
  printf("(release_buffer (fptr)        %d)\n", (int)    codecContext->release_buffer);
  printf("(has_b_frames                 %d)\n", (int)    codecContext->has_b_frames);
  printf("(block_align                  %d)\n", (int)    codecContext->block_align);
  printf("(parse_only                   %d)\n", (int)    codecContext->parse_only);
  printf("(mpeg_quant                   %d)\n", (int)    codecContext->mpeg_quant);
  printf("(stats_out (char*)            %d)\n", (int)    codecContext->stats_out);
  printf("(stats_in (char*)             %d)\n", (int)    codecContext->stats_in);
  printf("(rc_qsquish                   %f)\n", (float)  codecContext->rc_qsquish);
  printf("(rc_qmod_amp                  %f)\n", (float)  codecContext->rc_qmod_amp);
  printf("(rc_qmod_freq                 %d)\n", (int)    codecContext->rc_qmod_freq);
  printf("(rc_override (RcOverride*)    %d)\n", (int)    codecContext->rc_override);
  printf("(rc_override_count            %d)\n", (int)    codecContext->rc_override_count);
  printf("(rc_eq (char *)               %d)\n", (int)    codecContext->rc_eq);
  printf("(rc_max_rate                  %d)\n", (int)    codecContext->rc_max_rate);
  printf("(rc_min_rate                  %d)\n", (int)    codecContext->rc_min_rate);
  printf("(rc_buffer_size               %d)\n", (int)    codecContext->rc_buffer_size);
  printf("(rc_buffer_aggressivity       %f)\n", (float)  codecContext->rc_buffer_aggressivity);
  printf("(i_quant_factor               %f)\n", (float)  codecContext->i_quant_factor);
  printf("(i_quant_offset               %f)\n", (float)  codecContext->i_quant_offset);
  printf("(rc_initial_cplx              %f)\n", (float)  codecContext->rc_initial_cplx);
  printf("(dct_algo                     %d)\n", (int)    codecContext->dct_algo);
  printf("(lumi_masking                 %f)\n", (float)  codecContext->lumi_masking);
  printf("(temporal_cplx_masking        %f)\n", (float)  codecContext->temporal_cplx_masking);
  printf("(spatial_cplx_masking         %f)\n", (float)  codecContext->spatial_cplx_masking);
  printf("(p_masking                    %f)\n", (float)  codecContext->p_masking);
  printf("(dark_masking                 %f)\n", (float)  codecContext->dark_masking);
  printf("(idct_algo                    %d)\n", (int)    codecContext->idct_algo);
  printf("(slice_count                  %d)\n", (int)    codecContext->slice_count);
  printf("(slice_offset (int *)         %d)\n", (int)    codecContext->slice_offset);
  printf("(error_concealment            %d)\n", (int)    codecContext->error_concealment);
  printf("(dsp_mask                     %d)\n", (int)    codecContext->dsp_mask);
  printf("(bits_per_coded_sample        %d)\n", (int)    codecContext->bits_per_coded_sample);
  printf("(prediction_method            %d)\n", (int)    codecContext->prediction_method);
  printf("(sample_aspect_ratio.num      %d)\n", (int)    codecContext->sample_aspect_ratio.num);
  printf("(sample_aspect_ratio.den      %d)\n", (int)    codecContext->sample_aspect_ratio.den);
  printf("(coded_frame (AVFrame*)       %d)\n", (int)    codecContext->coded_frame);
  printf("(debug                        %d)\n", (int)    codecContext->debug);
  printf("(debug_mv                     %d)\n", (int)    codecContext->debug_mv);
  printf("(error[0]                     %lld)\n",        codecContext->error[0]);
  printf("(error[1]                     %lld)\n",        codecContext->error[1]);
  printf("(error[2]                     %lld)\n",        codecContext->error[2]);
  printf("(error[3]                     %lld)\n",        codecContext->error[3]);
  printf("(mb_qmin                      %d)\n", (int)    codecContext->mb_qmin);
  printf("(mb_qmax                      %d)\n", (int)    codecContext->mb_qmax);
  printf("(me_cmp                       %d)\n", (int)    codecContext->me_cmp);
  printf("(me_sub_cmp                   %d)\n", (int)    codecContext->me_sub_cmp);
  printf("(mb_cmp                       %d)\n", (int)    codecContext->mb_cmp);
  printf("(ildct_cmp                    %d)\n", (int)    codecContext->ildct_cmp);
  printf("(dia_size                     %d)\n", (int)    codecContext->dia_size);
  printf("(last_predictor_count         %d)\n", (int)    codecContext->last_predictor_count);
  printf("(pre_me                       %d)\n", (int)    codecContext->pre_me);
  printf("(me_pre_cmp                   %d)\n", (int)    codecContext->me_pre_cmp);
  printf("(pre_dia_size                 %d)\n", (int)    codecContext->pre_dia_size);
  printf("(me_subpel_quality            %d)\n", (int)    codecContext->me_subpel_quality);
  printf("(get_format (fptr)            %d)\n", (int)    codecContext->get_format);
  printf("(dtg_active_format            %d)\n", (int)    codecContext->dtg_active_format);
  printf("(me_range                     %d)\n", (int)    codecContext->me_range);
  printf("(intra_quant_bias             %d)\n", (int)    codecContext->intra_quant_bias);
  printf("(inter_quant_bias             %d)\n", (int)    codecContext->inter_quant_bias);
  printf("(color_table_id               %d)\n", (int)    codecContext->color_table_id);
  printf("(internal_buffer_count        %d)\n", (int)    codecContext->internal_buffer_count);
  printf("(internal_buffer (void*)      %d)\n", (int)    codecContext->internal_buffer);
  printf("(global_quality               %d)\n", (int)    codecContext->global_quality);
  printf("(coder_type                   %d)\n", (int)    codecContext->coder_type);
  printf("(context_model                %d)\n", (int)    codecContext->context_model);
  printf("(slice_flags                  %d)\n", (int)    codecContext->slice_flags);
  printf("(xvmc_acceleration            %d)\n", (int)    codecContext->xvmc_acceleration);
  printf("(mb_decision                  %d)\n", (int)    codecContext->mb_decision);
  printf("(intra_matrix (uint16_t*)     %d)\n", (int)    codecContext->intra_matrix);
  printf("(inter_matrix (uint16_t*)     %d)\n", (int)    codecContext->inter_matrix);
  printf("(stream_codec_tag             %d)\n", (int)    codecContext->stream_codec_tag);
  printf("(scenechange_threshold        %d)\n", (int)    codecContext->scenechange_threshold);
  printf("(lmin                         %d)\n", (int)    codecContext->lmin);
  printf("(lmax                         %d)\n", (int)    codecContext->lmax);
  printf("(palctrl (AVPaletteControl*)  %d)\n", (int)    codecContext->palctrl);
  printf("(noise_reduction              %d)\n", (int)    codecContext->noise_reduction);
  printf("(reget_buffer (fptr)          %d)\n", (int)    codecContext->reget_buffer);
  printf("(rc_initial_buffer_occupancy  %d)\n", (int)    codecContext->rc_initial_buffer_occupancy);
  printf("(inter_threshold              %d)\n", (int)    codecContext->inter_threshold);
  printf("(flags2                       %d)\n", (int)    codecContext->flags2);
  printf("(error_rate                   %d)\n", (int)    codecContext->error_rate);
  printf("(antialias_algo               %d)\n", (int)    codecContext->antialias_algo);
  printf("(quantizer_noise_shaping      %d)\n", (int)    codecContext->quantizer_noise_shaping);
  printf("(thread_count                 %d)\n", (int)    codecContext->thread_count);
  printf("(execute (fptr)               %d)\n", (int)    codecContext->execute);
  printf("(thread_opaque (void*)        %d)\n", (int)    codecContext->thread_opaque);
  printf("(me_threshold                 %d)\n", (int)    codecContext->me_threshold);
  printf("(mb_threshold                 %d)\n", (int)    codecContext->mb_threshold);
  printf("(intra_dc_precision           %d)\n", (int)    codecContext->intra_dc_precision);
  printf("(nsse_weight                  %d)\n", (int)    codecContext->nsse_weight);
  printf("(skip_top                     %d)\n", (int)    codecContext->skip_top);
  printf("(skip_bottom                  %d)\n", (int)    codecContext->skip_bottom);
  printf("(profile                      %d)\n", (int)    codecContext->profile);
  printf("(level                        %d)\n", (int)    codecContext->level);
  printf("(lowres                       %d)\n", (int)    codecContext->lowres);
  printf("(coded_width                  %d)\n", (int)    codecContext->coded_width);
  printf("(coded_height                 %d)\n", (int)    codecContext->coded_height);
  printf("(frame_skip_threshold         %d)\n", (int)    codecContext->frame_skip_threshold);
  printf("(frame_skip_factor            %d)\n", (int)    codecContext->frame_skip_factor);
  printf("(frame_skip_exp               %d)\n", (int)    codecContext->frame_skip_exp);
  printf("(frame_skip_cmp               %d)\n", (int)    codecContext->frame_skip_cmp);
  printf("(border_masking               %f)\n", (float)  codecContext->border_masking);
  printf("(mb_lmin                      %d)\n", (int)    codecContext->mb_lmin);
  printf("(mb_lmax                      %d)\n", (int)    codecContext->mb_lmax);
  printf("(me_penalty_compensation      %d)\n", (int)    codecContext->me_penalty_compensation);
  printf("(skip_loop_filter             %d)\n", (int)    codecContext->skip_loop_filter);
  printf("(skip_idct                    %d)\n", (int)    codecContext->skip_idct);
  printf("(skip_frame                   %d)\n", (int)    codecContext->skip_frame);
  printf("(bidir_refine                 %d)\n", (int)    codecContext->bidir_refine);
  printf("(brd_scale                    %d)\n", (int)    codecContext->brd_scale);
  printf("(crf                          %f)\n", (float)  codecContext->crf);
  printf("(cqp                          %d)\n", (int)    codecContext->cqp);
  printf("(keyint_min                   %d)\n", (int)    codecContext->keyint_min);
  printf("(refs                         %d)\n", (int)    codecContext->refs);
  printf("(chromaoffset                 %d)\n", (int)    codecContext->chromaoffset);
  printf("(bframebias                   %d)\n", (int)    codecContext->bframebias);
  printf("(trellis                      %d)\n", (int)    codecContext->trellis);
  printf("(complexityblur               %f)\n", (float)  codecContext->complexityblur);
  printf("(deblockalpha                 %d)\n", (int)    codecContext->deblockalpha);
  printf("(deblockbeta                  %d)\n", (int)    codecContext->deblockbeta);
  printf("(partitions                   %d)\n", (int)    codecContext->partitions);
  printf("(directpred                   %d)\n", (int)    codecContext->directpred);
  printf("(cutoff                       %d)\n", (int)    codecContext->cutoff);
  printf("(scenechange_factor           %d)\n", (int)    codecContext->scenechange_factor);
  printf("(mv0_threshold                %d)\n", (int)    codecContext->mv0_threshold);
  printf("(b_sensitivity                %d)\n", (int)    codecContext->b_sensitivity);
  printf("(compression_level            %d)\n", (int)    codecContext->compression_level);
  printf("(use_lpc                      %d)\n", (int)    codecContext->use_lpc);
  printf("(lpc_coeff_precision          %d)\n", (int)    codecContext->lpc_coeff_precision);
  printf("(min_prediction_order         %d)\n", (int)    codecContext->min_prediction_order);
  printf("(max_prediction_order         %d)\n", (int)    codecContext->max_prediction_order);
  printf("(prediction_order_method      %d)\n", (int)    codecContext->prediction_order_method);
  printf("(min_partition_order          %d)\n", (int)    codecContext->min_partition_order);
  printf("(max_partition_order          %d)\n", (int)    codecContext->max_partition_order);
  printf("(timecode_frame_start         %lld)\n",        codecContext->timecode_frame_start);
  printf("(drc_scale                    %f)\n", (float)  codecContext->drc_scale);
  printf("(reordered_opaque             %lld)\n",        codecContext->reordered_opaque);
  printf("(bits_per_raw_sample          %d)\n", (int)    codecContext->bits_per_raw_sample);
  printf("(channel_layout               %lld)\n",        codecContext->channel_layout);
  printf("(request_channel_layout       %lld)\n",        codecContext->request_channel_layout);
  printf("(rc_max_available_vbv_use     %f)\n", (float)  codecContext->rc_max_available_vbv_use);
  printf("(rc_min_vbv_overflow_use      %f)\n", (float)  codecContext->rc_min_vbv_overflow_use);

#ifndef GAP_USES_OLD_FFMPEG_0_5

  printf("(hwaccel_context              %d)\n", (int)    codecContext->hwaccel_context);
  printf("(color_primaries              %d)\n", (int)    codecContext->color_primaries);
  printf("(color_trc                    %d)\n", (int)    codecContext->color_trc);
  printf("(colorspace                   %d)\n", (int)    codecContext->colorspace);
  printf("(color_range                  %d)\n", (int)    codecContext->color_range);
  printf("(chroma_sample_location       %d)\n", (int)    codecContext->chroma_sample_location);
  /* *func */
  printf("(weighted_p_pred              %d)\n", (int)    codecContext->weighted_p_pred);
  printf("(aq_mode                      %d)\n", (int)    codecContext->aq_mode);
  printf("(aq_strength                  %f)\n", (float)  codecContext->aq_strength);
  printf("(psy_rd                       %f)\n", (float)  codecContext->psy_rd);
  printf("(psy_trellis                  %f)\n", (float)  codecContext->psy_trellis);
  printf("(rc_lookahead                 %d)\n", (int)    codecContext->rc_lookahead);

#ifndef GAP_USES_OLD_FFMPEG_0_6
  printf("(crf_max                      %f)\n", (float)  codecContext->crf_max);
  printf("(log_level_offset             %d)\n", (int)    codecContext->log_level_offset);
  /* attribute_deprecated enum AVLPCType lpc_type; */
  /* attribute_deprecated int lpc_passes;          */
  printf("(slices                       %d)\n", (int)    codecContext->slices);
  /* uint8_t *subtitle_header; */
  /* int subtitle_header_size; */
  /* AVPacket *pkt */
  /* int is_copy;  */
  printf("(thread_type                  %d)\n", (int)    codecContext->thread_type);
  printf("(active_thread_type           %d)\n", (int)    codecContext->active_thread_type);
  printf("(thread_safe_callbacks        %d)\n", (int)    codecContext->thread_safe_callbacks);
  printf("(vbv_delay                    %lld)\n",        codecContext->vbv_delay);
  printf("(audio_service_type           %d)\n", (int)    codecContext->audio_service_type);
  /* enum AVSampleFormat request_sample_fmt */
  /* int64_t pts_correction_num_faulty_pts; */
  /* int64_t pts_correction_num_faulty_dts; */
  /* int64_t pts_correction_last_pts; */
  /* int64_t pts_correction_last_dts; */

#endif
#endif

  printf("AVCodecContext Settings END\n");

}  /* end p_debug_print_dump_AVCodecContext */


/* --------------------------------
 * p_av_metadata_set
 * --------------------------------
 */
static int
p_av_metadata_set(AVMetadata **pm, const char *key, const char *value, int flags)
{
  int ret;
  
#ifndef GAP_USES_OLD_FFMPEG_0_5
  ret = av_metadata_set2(pm, key, value, flags);
#else
  ret = av_metadata_set(pm, key, value);
#endif  
  
  return (ret);
}  /* end p_av_metadata_set */


/* --------------------------------
 * p_set_flag
 * --------------------------------
 */
static void
p_set_flag(gint32 value_bool32, int *flag_ptr, int maskbit)
{
  if(value_bool32)
  {
    *flag_ptr |= maskbit;
  }
}  /* end p_set_flag */


/* --------------------------------
 * p_set_partition_flag
 * --------------------------------
 */
static void
p_set_partition_flag(gint32 value_bool32, gint32 *flag_ptr, gint32 maskbit)
{
  if(value_bool32)
  {
    *flag_ptr |= maskbit;
  }
  else
  {
    gint32 clearmask;
    
    clearmask = ~maskbit;
    *flag_ptr &= clearmask;
  }

}  /* end p_set_partition_flag */


/* --------------------------------
 * p_gimp_get_data
 * --------------------------------
 */
static void
p_gimp_get_data(const char *key, void *buffer, gint expected_size)
{
  if(gimp_get_data_size(key) == expected_size)
  {
      if(gap_debug) printf("p_gimp_get_data: key:%s\n", key);
      gimp_get_data(key, buffer);
  }
  else
  {
     if(gap_debug)
     {
       printf("p_gimp_get_data key:%s failed\n", key);
       printf("gimp_get_data_size:%d  expected size:%d\n"
             , (int)gimp_get_data_size(key)
             , (int)expected_size);
     }
  }
}  /* end p_gimp_get_data */



/* ----------------------------------
 * p_initPresetFromPresetIndex
 * ----------------------------------
 * copy preset specified by presetId
 * to the preset buffer provided by epp.
 * Note: in case presetId not found epp is left unchanged.
 */
static void
p_initPresetFromPresetIndex(GapGveFFMpegValues *epp, gint presetId)
{
  GapGveFFMpegValues *eppAtId;
  GapGveFFMpegValues  eppBackup;
  GapGveFFMpegValues *eppBck;

  eppBck = &eppBackup;
  memcpy(eppBck, epp, sizeof(GapGveFFMpegValues));


  if(gap_debug)
  {
    printf("p_initPresetFromPresetIndex: name:%s title:%s bckTitle:%s size:%d\n"
      ,epp->presetName
      ,epp->title
      ,eppBck->title
      ,sizeof(epp->title)
      );
  }


  for(eppAtId = gap_ffpar_getPresetList(); eppAtId != NULL; eppAtId = eppAtId->next)
  {
    if (eppAtId->presetId == presetId)
    {
      break;
    }
  }
  
  if(eppAtId != NULL)
  {
    memcpy(epp, eppAtId, sizeof(GapGveFFMpegValues));

    /* restore original title, author, copyright and comment
     * where present.
     */  
    if(eppBck->title[0] != '\0')
    {
      memcpy(epp->title, eppBck->title, sizeof(epp->title));
    }
    if(eppBck->author[0] != '\0')
    {
      memcpy(epp->author, eppBck->author, sizeof(epp->author));
    }
    if(eppBck->copyright[0] != '\0')
    {
      memcpy(epp->copyright, eppBck->copyright, sizeof(epp->copyright));
    }
    if(eppBck->comment[0] != '\0')
    {
      memcpy(epp->comment, eppBck->comment, sizeof(epp->comment));
    }
  }

}  /* end p_initPresetFromPresetIndex */


/* --------------------------------------
 * gap_enc_ffmpeg_main_init_preset_params
 * --------------------------------------
 * Encoding parameter Presets (Preset Values are just a guess and are not OK, have to check ....)
 * ID
 * 0 .. DivX default (ffmpeg defaults, OK)
 * 1 .. DivX Best
 * 2 .. DivX Low
 * 3 .. DivX MSMPEG default
 * 4 .. MPEG1 VCD
 * 5 .. MPEG1 Best
 * 6 .. MPEG2 SVCD
 * 7 .. MPEG2 DVD
 * 8 .. Real Video default
 *
 * Higher IDs may be available when preset files are installed.
 *
 * NOTE: the hardcoded presets are not used since standard presets are 
 * now part of the installation.
 * BUT: preset with index 0 is still relevant to init default settings
 * for parameters that are NOT available in the preset files.
 * (this is important when updates to newer ffmpeg releases introduce new params
 * that are not present in private preset files created by users)
 */
void
gap_enc_ffmpeg_main_init_preset_params(GapGveFFMpegValues *epp, gint preset_idx)
{
#define GAP_FFPRESET_TABSIZE     10  /* GAP_GVE_FFMPEG_PRESET_MAX_ELEMENTS  */

  gint l_idx;
  /*                                                          DivX def      best       low   MS-def         VCD   MPGbest      SVCD       DVD      Real */
  static gint32 tab_ntsc_width[GAP_FFPRESET_TABSIZE]        =  {     0,        0,        0,       0,        352,        0,      480,      720,        0 };
  static gint32 tab_ntsc_height[GAP_FFPRESET_TABSIZE]       =  {     0,        0,        0,       0,        240,        0,      480,      480,        0 };

  static gint32 tab_pal_width[GAP_FFPRESET_TABSIZE]         =  {     0,        0,        0,       0,        352,        0,      480,      720,        0 };
  static gint32 tab_pal_height[GAP_FFPRESET_TABSIZE]        =  {     0,        0,        0,       0,        288,        0,      576,      576,        0 };

  static gint32 tab_audio_bitrate[GAP_FFPRESET_TABSIZE]     =  {   160,      192,       96,     160,        224,      192,      224,      192,      160 };
  static gint32 tab_video_bitrate[GAP_FFPRESET_TABSIZE]     =  {   200,     1500,      150,     200,       1150,     6000,     2040,     6000,     1200 };
  static gint32 tab_gop_size[GAP_FFPRESET_TABSIZE]          =  {    12,       12,       96,      12,         18,       12,       18,       18,       18 };
  static gint32 tab_intra[GAP_FFPRESET_TABSIZE]             =  {     0,        0,        0,       0,          0,        0,        0,        0,        0 };

  static float  tab_qscale[GAP_FFPRESET_TABSIZE]            =  {     2,        1,        6,       2,          1,        1,        1,        1,        2 };

  static gint32 tab_qmin[GAP_FFPRESET_TABSIZE]              =  {     2,        2,        8,       2,          2,        2,        2,        2,        2 };
  static gint32 tab_qmax[GAP_FFPRESET_TABSIZE]              =  {    31,        8,       31,      31,         31,        8,       16,        8,       31 };
  static gint32 tab_qdiff[GAP_FFPRESET_TABSIZE]             =  {     3,        2,        9,       3,          3,        2,        2,        2,        3 };

  static float  tab_qblur[GAP_FFPRESET_TABSIZE]             =  {   0.5,       0.5,      0.5,    0.5,        0.5,      0.5,      0.5,      0.5,      0.5 };
  static float  tab_qcomp[GAP_FFPRESET_TABSIZE]             =  {   0.5,       0.5,      0.5,    0.5,        0.5,      0.5,      0.5,      0.5,      0.5 };
  static float  tab_rc_init_cplx[GAP_FFPRESET_TABSIZE]      =  {   0.0,       0.0,      0.0,    0.0,        0.0,      0.0,      0.0,      0.0,      0.0 };
  static float  tab_b_qfactor[GAP_FFPRESET_TABSIZE]         =  {  1.25,      1.25,     1.25,   1.25,       1.25,     1.25,     1.25,     1.25,     1.25 };
  static float  tab_i_qfactor[GAP_FFPRESET_TABSIZE]         =  {  -0.8,      -0.8,     -0.8,   -0.8,       -0.8,     -0.8,     -0.8,     -0.8,     -0.8 };
  static float  tab_b_qoffset[GAP_FFPRESET_TABSIZE]         =  {  1.25,      1.25,     1.25,   1.25,       1.25,     1.25,     1.25,     1.25,     1.25 };
  static float  tab_i_qoffset[GAP_FFPRESET_TABSIZE]         =  {   0.0,       0.0,      0.0,    0.0,        0.0,      0.0,      0.0,      0.0,      0.0 };

  static gint32 tab_bitrate_tol[GAP_FFPRESET_TABSIZE]       =  {  4000,     6000,     2000,    4000,       1150,     4000,     2000,     2000,     5000 };
  static gint32 tab_maxrate_tol[GAP_FFPRESET_TABSIZE]       =  {     0,        0,        0,       0,       1150,        0,     2516,     9000,        0 };
  static gint32 tab_minrate_tol[GAP_FFPRESET_TABSIZE]       =  {     0,        0,        0,       0,       1150,        0,        0,        0,        0 };
  static gint32 tab_bufsize[GAP_FFPRESET_TABSIZE]           =  {     0,        0,        0,       0,         40,        0,      224,      224,        0 };

  static gint32 tab_motion_estimation[GAP_FFPRESET_TABSIZE] =  {     5,        5,        5,       5,          5,        5,        5,        5,        5 };

  static gint32 tab_dct_algo[GAP_FFPRESET_TABSIZE]          =  {     0,        0,        0,       0,          0,        0,        0,        0,        0 };
  static gint32 tab_idct_algo[GAP_FFPRESET_TABSIZE]         =  {     0,        0,        0,       0,          0,        0,        0,        0,        0 };
  static gint32 tab_strict[GAP_FFPRESET_TABSIZE]            =  {     0,        0,        0,       0,          0,        0,        0,        0,        0 };
  static gint32 tab_mb_qmin[GAP_FFPRESET_TABSIZE]           =  {     2,        2,        8,       2,          2,        2,        2,        2,        2 };
  static gint32 tab_mb_qmax[GAP_FFPRESET_TABSIZE]           =  {    31,       18,       31,      31,         31,       18,       31,       31,       31 };
  static gint32 tab_mb_decision[GAP_FFPRESET_TABSIZE]       =  {     0,        0,        0,       0,          0,        0,        0,        0,        0 };
  static gint32 tab_aic[GAP_FFPRESET_TABSIZE]               =  {     0,        0,        0,       0,          0,        0,        0,        0,        0 };
  static gint32 tab_umv[GAP_FFPRESET_TABSIZE]               =  {     0,        0,        0,       0,          0,        0,        0,        0,        0 };

  static gint32 tab_b_frames[GAP_FFPRESET_TABSIZE]          =  {     2,        2,        4,       0,          0,        0,        2,        2,        0 };
  static gint32 tab_mv4[GAP_FFPRESET_TABSIZE]               =  {     0,        0,        0,       0,          0,        0,        0,        0,        0 };
  static gint32 tab_partitioning[GAP_FFPRESET_TABSIZE]      =  {     0,        0,        0,       0,          0,        0,        0,        0,        0 };
  static gint32 tab_packet_size[GAP_FFPRESET_TABSIZE]       =  {     0,        0,        0,       0,          0,        0,        0,        0,        0 };
  static gint32 tab_bitexact[GAP_FFPRESET_TABSIZE]          =  {     0,        0,        0,       0,          0,        0,        0,        0,        0 };
  static gint32 tab_aspect[GAP_FFPRESET_TABSIZE]            =  {     1,        1,        1,       1,          1,        1,        1,        1,        1 };
  static gint32 tab_aspect_fact[GAP_FFPRESET_TABSIZE]       =  {   0.0,      0.0,      0.0,     0.0,        0.0,      0.0,      0.0,      0.0,      0.0 };

  static char*  tab_format_name[GAP_FFPRESET_TABSIZE]       =  { "avi",    "avi",    "avi",     "avi",     "vcd",   "mpeg",   "svcd",    "vob",     "rm" };
  static char*  tab_vcodec_name[GAP_FFPRESET_TABSIZE]       =  { "mpeg4", "mpeg4", "mpeg4", "msmpeg4", "mpeg1video", "mpeg1video", "mpeg2video", "mpeg2video", "rv10" };
  static char*  tab_acodec_name[GAP_FFPRESET_TABSIZE]       =  { "mp2",     "mp2",   "mp2",     "mp2",      "mp2",    "mp2",    "mp2",   "ac3",    "ac3" };

  static gint32 tab_mux_rate[GAP_FFPRESET_TABSIZE]          =  {     0,         0,        0,      0,     1411200,       0,       0,   10080000,        0 };
  static gint32 tab_mux_packet_size[GAP_FFPRESET_TABSIZE]   =  {     0,         0,        0,      0,        2324,    2324,    2324,       2048,        0 };
  static float  tab_mux_preload[GAP_FFPRESET_TABSIZE]       =  {   0.5,       0.5,      0.5,    0.5,        0.44,     0.5,     0.5,        0.5,      0.5 };
  static float  tab_mux_max_delay[GAP_FFPRESET_TABSIZE]     =  {   0.7,       0.7,      0.7,    0.7,         0.7,     0.7,     0.7,        0.7,      0.7 };
  static gint32 tab_use_scann_offset[GAP_FFPRESET_TABSIZE]  =  {     0,         0,        0,      0,           0,       0,       1,          0,        0 };

  static char*  tab_presetName[GAP_FFPRESET_TABSIZE] = {
      "DivX default preset"
     ,"DivX high quality preset"
     ,"DivX low quality preset"
     ,"DivX WINDOWS preset"
     ,"MPEG1 (VCD) preset"
     ,"MPEG1 high quality preset"
     ,"MPEG2 (SVCD) preset"
     ,"MPEG2 (DVD) preset"
     ,"REAL video preset"
  };

  l_idx = preset_idx -1;
  if ((preset_idx < 1) 
  || (preset_idx >= GAP_FFPRESET_TABSIZE)
  || (preset_idx >= GAP_GVE_FFMPEG_PRESET_MAX_ELEMENTS)
  )
  {
    /* use hardcoded preset 0 as base initialisation for non-hardcoded Id presets 
     */
    l_idx = 0;
  }
  else
  {
    g_snprintf(epp->presetName,  sizeof(epp->presetName),  tab_presetName[l_idx]);    /* "name of the preset */
  }

  if(gap_debug)
  {
    printf("gap_enc_ffmpeg_main_init_preset_params L_IDX:%d preset_idx:%d\n"
        , (int)l_idx
        , (int)preset_idx
        );
  }

  g_snprintf(epp->format_name, sizeof(epp->format_name), tab_format_name[l_idx]);   /* "avi" */
  g_snprintf(epp->vcodec_name, sizeof(epp->vcodec_name), tab_vcodec_name[l_idx]);   /* "msmpeg4" */
  g_snprintf(epp->acodec_name, sizeof(epp->acodec_name), tab_acodec_name[l_idx]);   /* "mp2" */


  epp->pass_nr             = 1;
  epp->twoPassFlag         = FALSE;
  epp->audio_bitrate       = tab_audio_bitrate[l_idx];   /* 160 kbit */
  epp->video_bitrate       = tab_video_bitrate[l_idx];   /* 5000 kbit */
  epp->gop_size            = tab_gop_size[l_idx];        /* 12 group of pictures size (frames) */

  epp->intra               = tab_intra[l_idx];           /* 0:FALSE 1:TRUE */
  epp->qscale              = tab_qscale[l_idx];          /* 0  0.01 .. 255 (0..31 in old fmpeg 0.4.8) */
  epp->qmin                = tab_qmin[l_idx];            /* 2  0..31 */
  epp->qmax                = tab_qmax[l_idx];            /* 31  0..31 */
  epp->qdiff               = tab_qdiff[l_idx];           /* 3 0..31 */

  epp->qblur               = tab_qblur[l_idx];           /* 0.5; */
  epp->qcomp               = tab_qcomp[l_idx];           /* 0.5; */
  epp->rc_init_cplx        = tab_rc_init_cplx[l_idx];    /* 0.0; */
  epp->b_qfactor           = tab_b_qfactor[l_idx];       /* 1.25; */
  epp->i_qfactor           = tab_i_qfactor[l_idx];       /* -0.8; */
  epp->b_qoffset           = tab_b_qoffset[l_idx];       /* 1.25; */
  epp->i_qoffset           = tab_i_qoffset[l_idx];       /* 0.0; */

  epp->bitrate_tol         = tab_bitrate_tol[l_idx];     /* 4000;  kbit */
  epp->maxrate_tol         = tab_maxrate_tol[l_idx];     /* 0;     kbit */
  epp->minrate_tol         = tab_minrate_tol[l_idx];     /* 0;     kbit */
  epp->bufsize             = tab_bufsize[l_idx];         /* 0;     kbit */
  epp->motion_estimation   = tab_motion_estimation[l_idx];  /* 5   1:zero(fastest), 2:full(best), 3:log, 4:phods, 5:epzs(recommanded), 6:x1 */
  epp->dct_algo            = tab_dct_algo[l_idx];        /* 0 */
  epp->idct_algo           = tab_idct_algo[l_idx];       /* 0 */
  epp->strict              = tab_strict[l_idx];          /* 0 */
  epp->mb_qmin             = tab_mb_qmin[l_idx];         /* 2 */
  epp->mb_qmax             = tab_mb_qmax[l_idx];         /* 31 */
  epp->mb_decision         = tab_mb_decision[l_idx];     /* 0:FF_MB_DECISION_SIMPLE  1:FF_MB_DECISION_BITS 2:FF_MB_DECISION_RD */
  epp->aic                 = tab_aic[l_idx];             /* 0:FALSE 1:TRUE */
  epp->umv                 = tab_umv[l_idx];             /* 0:FALSE 1:TRUE */
  epp->b_frames            = tab_b_frames[l_idx];        /* 0-8: number of B-frames per GOP */
  epp->mv4                 = tab_mv4[l_idx];             /* 0:FALSE 1:TRUE */
  epp->partitioning        = tab_partitioning[l_idx];    /* 0:FALSE 1:TRUE */
  epp->packet_size         = tab_packet_size[l_idx];
  epp->benchmark           = 0;      /* 0:FALSE 1:TRUE */
  epp->hex_dump            = 0;      /* 0:FALSE 1:TRUE */
  epp->psnr                = 0;      /* 0:FALSE 1:TRUE */
  epp->vstats              = 0;      /* 0:FALSE 1:TRUE */
  epp->bitexact            = tab_bitexact[l_idx];       /* 0:FALSE 1:TRUE */
  epp->set_aspect_ratio    = tab_aspect[l_idx];         /* 0:FALSE 1:TRUE */
  epp->factor_aspect_ratio = tab_aspect_fact[l_idx];    /* 0:auto, or width/height */


  /* new parms (added after ffmpeg 0.4.8)
   * TODO: findout valid value ranges and provide GUI widgets for those options
   */
  epp->thread_count                 = 1;
  epp->mb_cmp                       = GAP_GVE_FFMPEG_CMP_00_SAD;
  epp->ildct_cmp                    = GAP_GVE_FFMPEG_CMP_08_VSAD;
  epp->sub_cmp                      = GAP_GVE_FFMPEG_CMP_00_SAD;
  epp->cmp                          = GAP_GVE_FFMPEG_CMP_00_SAD;
  epp->pre_cmp                      = GAP_GVE_FFMPEG_CMP_00_SAD;
  epp->pre_me                       = 0;
  epp->lumi_mask                    = 0;
  epp->dark_mask                    = 0;
  epp->scplx_mask                   = 0;
  epp->tcplx_mask                   = 0;
  epp->p_mask                       = 0;
  epp->qns                          = 0;

  epp->use_ss                       = 0;
  epp->use_aiv                      = 0;
  epp->use_obmc                     = 0;
  epp->use_loop                     = 0;
  epp->use_alt_scan                 = 0;
  epp->use_mv0                      = 0;
  epp->do_normalize_aqp             = 0;
  epp->use_scan_offset              = tab_use_scann_offset[l_idx];
  epp->closed_gop                   = 0;
  epp->strict_gop                   = 0;
  epp->use_qpel                     = 0;
  epp->use_qprd                     = 0;
  epp->use_cbprd                    = 0;
  epp->do_interlace_dct             = 0;
  epp->do_interlace_me              = 0;
  epp->no_output                    = 0;
  epp->video_lmin                   = 2*GAP_GVE_FF_QP2LAMBDA;
  epp->video_lmax                   = 31*GAP_GVE_FF_QP2LAMBDA;
  epp->video_mb_lmin                = 2*GAP_GVE_FF_QP2LAMBDA;
  epp->video_mb_lmax                = 31*GAP_GVE_FF_QP2LAMBDA;
  epp->video_lelim                  = 0;
  epp->video_celim                  = 0;
  epp->video_intra_quant_bias       = FF_DEFAULT_QUANT_BIAS;
  epp->video_inter_quant_bias       = FF_DEFAULT_QUANT_BIAS;
  epp->me_threshold                 = 0;
  epp->mb_threshold                 = 0;
  epp->intra_dc_precision           = 8;


  epp->error_rate                   = 0;
  epp->noise_reduction              = 0;
  epp->sc_threshold                 = 0;
  epp->me_range                     = 0;
  epp->coder                        = 0;
  epp->context                      = 0;
  epp->predictor                    = 0;
  epp->video_profile                = GAP_FF_PROFILE_UNKNOWN;
  epp->video_level                  = GAP_FF_LEVEL_UNKNOWN;
  epp->nsse_weight                  = 8;
  epp->subpel_quality               = 8;
  epp->frame_skip_threshold         = 0;
  epp->frame_skip_factor            = 0;
  epp->frame_skip_exp               = 0;
  epp->frame_skip_cmp               = GAP_FF_CMP_DCTMAX;

  epp->mux_rate                     = tab_mux_rate[l_idx];
  epp->mux_packet_size              = tab_mux_packet_size[l_idx];
  epp->mux_preload                  = tab_mux_preload[l_idx];
  epp->mux_max_delay                = tab_mux_max_delay[l_idx];

  epp->ntsc_width                   = tab_ntsc_width[l_idx];
  epp->ntsc_height                  = tab_ntsc_height[l_idx];
  epp->pal_width                    = tab_pal_width[l_idx];
  epp->pal_height                   = tab_pal_height[l_idx];


  /* new parms 2009.01.31 */

  epp->b_frame_strategy              = 0;
  epp->workaround_bugs               = FF_BUG_AUTODETECT;
  epp->error_recognition             = FF_ER_CAREFUL;
  epp->mpeg_quant                    = 0;
  epp->rc_qsquish                    = 0.0;   /* 0 = clip, 1 = use differentiable function */
  epp->rc_qmod_amp                   = 0.0;
  epp->rc_qmod_freq                  = 0;
  epp->rc_eq[0]                      = '\0';  /* triggers use of use default equation */
  epp->rc_buffer_aggressivity        = 1.0;
  epp->dia_size                      = 0;
  epp->last_predictor_count          = 0;
  epp->pre_dia_size                  = 0;
  epp->inter_threshold               = 0;
  epp->border_masking                = 0.0;
  epp->me_penalty_compensation       = 256;
  epp->bidir_refine                  = 0;
  epp->brd_scale                     = 0;
  epp->crf                           = 0.0;
  epp->cqp                           = -1;
  epp->keyint_min                    = 25;   /* minimum GOP size */
  epp->refs                          = 1;
  epp->chromaoffset                  = 0;
  epp->bframebias                    = 0;
  epp->trellis                       = 0;
  epp->complexityblur                = 20.0;
  epp->deblockalpha                  = 0;
  epp->deblockbeta                   = 0;
  epp->partitions                    = 0;
  epp->directpred                    = 2;
  epp->cutoff                        = 0;
  epp->scenechange_factor            = 6;
  epp->mv0_threshold                 = 256;
  epp->b_sensitivity                 = 40;
  epp->compression_level             = -1;
  epp->use_lpc                       = -1;
  epp->lpc_coeff_precision           = 0;
  epp->min_prediction_order          = -1;
  epp->max_prediction_order          = -1;
  epp->prediction_order_method       = -1;
  epp->min_partition_order           = -1;
  epp->timecode_frame_start          = 0;
  epp->bits_per_raw_sample           = 0;
  epp->channel_layout                = 0;
  epp->rc_max_available_vbv_use      = 1.0 / 3.0;
  epp->rc_min_vbv_overflow_use       = 3.0;

  /* new params of ffmpeg-0.6 2010.07.31 */
  epp->color_primaries               =  0; // TODO  enum AVColorPrimaries color_primaries;
  epp->color_trc                     =  0; //  enum AVColorTransferCharacteristic color_trc;
  epp->colorspace                    =  0; //  enum AVColorSpace colorspace;
  epp->color_range                   =  0; //  enum AVColorRange color_range;
  epp->chroma_sample_location        =  0; //  enum AVChromaLocation chroma_sample_location;
  epp->weighted_p_pred               = 0;
  epp->aq_mode                       = 0;
  epp->aq_strength                   = 0.0; 
  epp->psy_rd                        = 0.0;
  epp->psy_trellis                   = 0.0;
  epp->rc_lookahead                  = 0;

  /* new flags/flags2 2009.01.31 */
  epp->codec_FLAG_GMC                   = 0; /* 0: FALSE */
  epp->codec_FLAG_INPUT_PRESERVED       = 0; /* 0: FALSE */
  epp->codec_FLAG_GRAY                  = 0; /* 0: FALSE */
  epp->codec_FLAG_EMU_EDGE              = 0; /* 0: FALSE */
  epp->codec_FLAG_TRUNCATED             = 0; /* 0: FALSE */

  epp->codec_FLAG2_FAST                 = 0; /* 0: FALSE */
  epp->codec_FLAG2_LOCAL_HEADER         = 0; /* 0: FALSE */
  epp->codec_FLAG2_BPYRAMID             = 0; /* 0: FALSE */
  epp->codec_FLAG2_WPRED                = 0; /* 0: FALSE */
  epp->codec_FLAG2_MIXED_REFS           = 0; /* 0: FALSE */
  epp->codec_FLAG2_8X8DCT               = 0; /* 0: FALSE */
  epp->codec_FLAG2_FASTPSKIP            = 0; /* 0: FALSE */
  epp->codec_FLAG2_AUD                  = 0; /* 0: FALSE */
  epp->codec_FLAG2_BRDO                 = 0; /* 0: FALSE */
  epp->codec_FLAG2_INTRA_VLC            = 0; /* 0: FALSE */
  epp->codec_FLAG2_MEMC_ONLY            = 0; /* 0: FALSE */
  epp->codec_FLAG2_DROP_FRAME_TIMECODE  = 0; /* 0: FALSE */
  epp->codec_FLAG2_SKIP_RD              = 0; /* 0: FALSE */
  epp->codec_FLAG2_CHUNKS               = 0; /* 0: FALSE */
  epp->codec_FLAG2_NON_LINEAR_QUANT     = 0; /* 0: FALSE */
  epp->codec_FLAG2_BIT_RESERVOIR        = 0; /* 0: FALSE */

  /* new flags/flags2 of ffmpeg-0.6 2010.07.31 */
  epp->codec_FLAG2_MBTREE               = 0; /* 0: FALSE */
  epp->codec_FLAG2_PSY                  = 0; /* 0: FALSE */
  epp->codec_FLAG2_SSIM                 = 0; /* 0: FALSE */
  /* new flags/flags2 of ffmpeg-0.7.11 2012.01.12 */
  epp->codec_FLAG2_INTRA_REFRESH        = 0; /* 0: FALSE */

  epp->partition_X264_PART_I4X4         = 0; /* 0: FALSE */
  epp->partition_X264_PART_I8X8         = 0; /* 0: FALSE */
  epp->partition_X264_PART_P8X8         = 0; /* 0: FALSE */
  epp->partition_X264_PART_P4X4         = 0; /* 0: FALSE */
  epp->partition_X264_PART_B8X8         = 0; /* 0: FALSE */


  /* new parameters of ffmpeg-0.7.11 2012.01.12 */
  epp->crf_max                       = 0.0;
  epp->log_level_offset              = 0;
  epp->slices                        = 0;
  epp->thread_type                   = 3;   /* FF_THREAD_FRAME   1  + FF_THREAD_SLICE   2 */
  epp->active_thread_type            = 0;
  epp->thread_safe_callbacks         = 0;
  epp->vbv_delay                     = 0;
  epp->audio_service_type            = 0;

  if (preset_idx >= GAP_GVE_FFMPEG_PRESET_MAX_ELEMENTS)
  {
    if(gap_debug)
    {
      printf("gap_enc_ffmpeg_main_init_preset_params before p_initPresetFromPresetIndex\n"
          );
    }
    p_initPresetFromPresetIndex(epp, preset_idx);
  }

  if(gap_debug)
  {
    printf("gap_enc_ffmpeg_main_init_preset_params DONE L_IDX:%d preset_idx:%d\n"
        , (int)l_idx
        , (int)preset_idx
        );
  }

}   /* end gap_enc_ffmpeg_main_init_preset_params */


/* --------------------------------
 * p_ffmpeg_init_default_params
 * --------------------------------
 */
static void
p_ffmpeg_init_default_params(GapGveFFMpegValues *epp)
{
  g_snprintf(epp->title, sizeof(epp->title), "%s", "\0");
  g_snprintf(epp->author, sizeof(epp->author), "%s", "\0");
  g_snprintf(epp->copyright, sizeof(epp->copyright), "%s", "\0");
  g_snprintf(epp->comment, sizeof(epp->comment), "%s", "\0");
  g_snprintf(epp->passlogfile, sizeof(epp->passlogfile), DEFAULT_PASS_LOGFILENAME);

  gap_enc_ffmpeg_main_init_preset_params(epp, 0);
  epp->dont_recode_flag    = 0;      /* 0:FALSE 1:TRUE */
}   /* end p_ffmpeg_init_default_params */




/* ---------------------
 * p_init_audio_workdata
 * ---------------------
 */
static void
p_init_audio_workdata(t_awk_array *awp)
{
  gint ii;

  awp->audio_tracks = 0;

  /* clear all elements of the audio stream and audio work tables */
  for (ii=0; ii < MAX_AUDIO_STREAMS; ii++)
  {
    awp->awk[ii].fp_inwav = NULL;
    awp->awk[ii].referred_wavfile = NULL;
    awp->awk[ii].wavsize = 0;
    awp->awk[ii].audio_margin = 8192;
    awp->awk[ii].audio_filled_100 = 0;
    awp->awk[ii].audio_per_frame_x100 = -1;
    awp->awk[ii].frames_per_second_x100 = 2500;
    awp->awk[ii].frames_per_second_x100f = 2500;
    awp->awk[ii].databuffer = NULL;
    awp->awk[ii].sample_rate = 22050;
    awp->awk[ii].channels = 2;
    awp->awk[ii].bytes_per_sample = 4;
    awp->awk[ii].bits = 16;
    awp->awk[ii].samples = 0;

  }
}  /* end p_init_audio_workdata */




/* ------------------------
 * p_open_inwav_and_buffers
 * ------------------------
 * open input wavefile for the audio stram specified by its index ii
 * allocate buffers and init audio relevant informations
 */
static void
p_open_inwav_and_buffers(t_awk_array *awp
                        , const char *wavefile
                        , gint ii
                        , long sample_rate
                        , long channels
                        , long bytes_per_sample
                        , long bits
                        , long samples
                        )
{
  FILE *fp_inwav;

  /* l_bytes_per_sample: 8bitmono: 1, 16bit_mono: 2, 8bitstereo: 2, 16bitstereo: 4 */
  awp->awk[ii].wavsize = samples * bytes_per_sample;
  awp->awk[ii].bytes_per_sample = bytes_per_sample;
  awp->awk[ii].samples = samples;
  awp->awk[ii].sample_rate = sample_rate;
  awp->awk[ii].channels = channels;
  awp->awk[ii].bits = bits;

  /* open WAVE file and set position to the 1.st audio databyte */
  fp_inwav = gap_audio_wav_open_seek_data(wavefile);

  awp->awk[ii].fp_inwav = fp_inwav;
  awp->awk[ii].audio_margin = 8192;
  awp->awk[ii].audio_filled_100 = 0;
  awp->awk[ii].audio_per_frame_x100 = -1;
  awp->awk[ii].frames_per_second_x100 = 2500;
  awp->awk[ii].frames_per_second_x100f = 2500;
  awp->awk[ii].databuffer = g_malloc(300000);

}  /* end p_open_inwav_and_buffers */



/* -----------------------
 * p_sound_precalculations
 * -----------------------
 */
static void
p_sound_precalculations(t_ffmpeg_handle *ffh
                      , t_awk_array *awp
                      , GapGveFFMpegGlobalParams *gpp)
{
  gint ii;

  for (ii=0; ii < MIN(ffh->max_ast, awp->audio_tracks); ii++)
  {
    /* Calculations for encoding the sound
     */
    awp->awk[ii].frames_per_second_x100f = (100.0 * gpp->val.framerate) + 0.5;
    awp->awk[ii].frames_per_second_x100 = awp->awk[ii].frames_per_second_x100f;
    awp->awk[ii].audio_per_frame_x100 = (100 * 100 * awp->awk[ii].sample_rate
                                         * awp->awk[ii].bytes_per_sample)
                                         / MAX(1,awp->awk[ii].frames_per_second_x100);

    /* ?? dont sure if to use fix audo_margin of 8192 or
     * if its ok to set audio_margin to audio_per_frame
     */
    /* audio_margin = audio_per_frame_x100 / 100; */
    if(ffh->ast[ii].aud_codec_context)
    {
       /* the audio codec gives us the correct audio frame size, in samples */
       if (gap_debug)
       {
         printf("Audio Codec context frame_size[%d]:%d  channels:%d\n"
                             , (int)ii
                             , (int)ffh->ast[ii].aud_codec_context->frame_size
                             , (int)ffh->ast[ii].aud_codec_context->channels
                             );
       }
       awp->awk[ii].audio_margin = ffh->ast[ii].aud_codec_context->frame_size * 2 * ffh->ast[ii].aud_codec_context->channels;
    }

    if(gap_debug)
    {
      printf("audio_margin[%d]: %d\n", (int)ii, (int)awp->awk[ii].audio_margin);
    }
  }


}  /* end p_sound_precalculations */


/* ----------------------
 * p_process_audio_frame
 * ----------------------
 * encode + write audio one frame (for each audio track)
 * requires an already opened multimedia output file context
 * and audio codecs initilalized and opened for each input
 * track. (ffh->ast)
 * Further all audio buffers have to be allocated
 * and all audio input file handles must be opened read
 *
 * The read position in those file will be advanced
 * when the content of the corresponding input buffers
 * falls down below the margin.
 */
static void
p_process_audio_frame(t_ffmpeg_handle *ffh, t_awk_array *awp)
{
  gint ii;
  gint32 datasize;

  /* foreach audio_track
   * (number of audioinput tracks should always match
   *  the max audio streams)
   */
  for (ii=0; ii < MIN(ffh->max_ast, awp->audio_tracks); ii++)
  {
      if (gap_debug)
      {
        printf("p_process_audio_frame: audio treack: ii=%d.\n", (int)ii);
      }
      /* encode AUDIO FRAME (audio packet for playbacktime of one frame) */
      /* As long as there is a video frame, "fill" the fake audio buffer */
      if (awp->awk[ii].fp_inwav)
      {
        awp->awk[ii].audio_filled_100 += awp->awk[ii].audio_per_frame_x100;
      }

      /* Now "empty" the audio buffer, until it goes under the margin again */
      while ((awp->awk[ii].audio_filled_100 >= (awp->awk[ii].audio_margin * 100)) && (awp->awk[ii].wavsize > 0))
      {
        if (gap_debug) printf("p_process_audio_frame: Audio buffer at %ld bytes.\n", awp->awk[ii].audio_filled_100);
        if (awp->awk[ii].wavsize >= awp->awk[ii].audio_margin)
        {
            datasize = fread(awp->awk[ii].databuffer
                           , 1
                           , awp->awk[ii].audio_margin
                           , awp->awk[ii].fp_inwav
                           );
            if (datasize != awp->awk[ii].audio_margin)
            {
              printf("Warning: Read from wav file failed. (non-critical)\n");
            }
            awp->awk[ii].wavsize -= awp->awk[ii].audio_margin;
        }
        else
        {
            datasize = fread(awp->awk[ii].databuffer
                           , 1
                           , awp->awk[ii].wavsize
                           , awp->awk[ii].fp_inwav
                           );
            if (datasize != awp->awk[ii].wavsize)
            {
              printf("Warning: Read from wav file failed. (non-critical)\n");
            }
            awp->awk[ii].wavsize = 0;
        }

        if (gap_debug) printf("Now encoding audio frame datasize:%d\n", (int)datasize);

        p_ffmpeg_write_audioframe(ffh, awp->awk[ii].databuffer, datasize, ii /* aud_track */);

        awp->awk[ii].audio_filled_100 -= awp->awk[ii].audio_margin * 100;
      }
  }


}  /* end p_process_audio_frame */


/* -------------------------
 * p_close_audio_input_files
 * -------------------------
 * close input files for all audio tracks
 * and free their databuffer
 */
static void
p_close_audio_input_files(t_awk_array *awp)
{
  gint ii;

  for (ii=0; ii < awp->audio_tracks; ii++)
  {
    if (awp->awk[ii].fp_inwav)
    {
      fclose(awp->awk[ii].fp_inwav);
      awp->awk[ii].fp_inwav = NULL;
    }

    if(awp->awk[ii].referred_wavfile)
    {
      g_free(awp->awk[ii].referred_wavfile);
      awp->awk[ii].referred_wavfile = NULL;
    }


    if(awp->awk[ii].databuffer)
    {
      g_free(awp->awk[ii].databuffer);
      awp->awk[ii].databuffer = NULL;
    }

  }
}  /* end p_close_audio_input_files */


/* -------------------------
 * p_open_audio_input_files
 * -------------------------
 *
 */
static void
p_open_audio_input_files(t_awk_array *awp, GapGveFFMpegGlobalParams *gpp)
{
  long  l_bytes_per_sample = 4;
  long  l_sample_rate = 22050;
  long  l_channels = 2;
  long  l_bits = 16;
  long  l_samples = 0;
  gint  ii = 0;

  /* check and open audio wave file */
  if(0 == gap_audio_wav_file_check( gpp->val.audioname1
                                , &l_sample_rate
                                , &l_channels
                                , &l_bytes_per_sample
                                , &l_bits
                                , &l_samples
                                ))
  {
    p_open_inwav_and_buffers(awp
                            , gpp->val.audioname1
                            , ii
                            , l_sample_rate
                            , l_channels
                            , l_bytes_per_sample
                            , l_bits
                            , l_samples
                            );
    awp->audio_tracks = 1;
  }
  else
  {
    FILE *l_fp;
    char         l_buf[4000];

    l_channels = 0;
    awp->audio_tracks = 0;

    /* check if gpp->val.audioname1
     * is a playlist referring to more than one inputfile
     * and try to open those input wavefiles
     * (but limited upto MAX_AUDIO_STREAMS input wavefiles)
     */
    l_fp = g_fopen(gpp->val.audioname1, "r");
    if(l_fp)
    {
      ii = 0;
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

          awp->awk[ii].referred_wavfile = gap_file_make_abspath_filename(&l_buf[0], gpp->val.audioname1);

          l_rc = gap_audio_wav_file_check(awp->awk[ii].referred_wavfile
                                , &l_sample_rate
                                , &l_channels
                                , &l_bytes_per_sample
                                , &l_bits
                                , &l_samples
                                );
          if(l_rc == 0)
          {
            p_open_inwav_and_buffers(awp
                            , awp->awk[ii].referred_wavfile
                            , ii
                            , l_sample_rate
                            , l_channels
                            , l_bytes_per_sample
                            , l_bits
                            , l_samples
                            );

            ii++;
            awp->audio_tracks = ii;
          }
          else
          {
            g_message(_("The file: %s\n"
                      "has unexpect content that will be ignored.\n"
                      "You should specify an audio file in RIFF WAVE fileformat,\n"
                      "or a textfile containing filenames of such audio files")
                     , gpp->val.audioname1
                     );
            break;
          }
        }
        else
        {
          g_message(ngettext("The file: %s\n"
                      "contains too many audio-input tracks\n"
                      "(only %d track is used, the rest is ignored)."
                      
                     ,"The file: %s\n"
                      "contains too many audio-input tracks\n"
                      "(only %d tracks are used, the rest is ignored)."
                      
                     , (int) MAX_AUDIO_STREAMS)
                   , gpp->val.audioname1
                   , (int) MAX_AUDIO_STREAMS
                   );
          break;
        }
        l_buf[0] = '\0';
      }

      fclose(l_fp);
    }
  }

}  /* end p_open_audio_input_files */

/* -----------------------------
 * p_calculate_timecode
 * -----------------------------
 * returns the timecode for the specified frameNr
 */
static gint64
p_calculate_timecode(t_ffmpeg_handle *ffh, int frameNr)
{
  gdouble dblTimecode;
  gint64  timecode64;

  /*
   * gdouble seconds;
   * seconds = (ffh->encode_frame_nr  / gpp->val.framerate);
   */

  dblTimecode = frameNr * ffh->pts_stepsize_per_frame;
  timecode64 = dblTimecode;

  return (timecode64);
}  /* end p_calculate_timecode */

/* -----------------------------
 * p_calculate_current_timecode
 * -----------------------------
 * returns the timecode for the current frame
 */
static gint64
p_calculate_current_timecode(t_ffmpeg_handle *ffh)
{
  return (p_calculate_timecode(ffh, ffh->encode_frame_nr));
}


/* -----------------------------
 * p_set_timebase_from_framerate
 * -----------------------------
 * set the specified framerate (frames per second)
 * in the ffmpeg typical AVRational struct.
 *
 * 25 frames per second shall be encoded ad num = 1, den = 25
 * typedef struct AVRational{
 *     int num;  numerator
 *     int den;  denominator
 * } AVRational;
 */
static void
p_set_timebase_from_framerate(AVRational *time_base, gdouble framerate)
{
  gdouble truncatedFramerate;

  time_base->num = 1;
  time_base->den = framerate;

  truncatedFramerate = time_base->den;
  if((framerate - truncatedFramerate) != 0.0)
  {
    time_base->num = DEFAULT_FRAME_RATE_BASE;
    time_base->den = framerate * DEFAULT_FRAME_RATE_BASE;
  }

  /* due to rounding errors, some typical framerates would be
   * rejected by newer libavcodec encoders.
   * therefore adjust those values to exact expected values of the codecs.
   */
  if ((framerate > 59.90) && (framerate < 59.99))
  {
    time_base->num = 1001;
    time_base->den = 60000;

  }
  if ((framerate > 29.90) && (framerate < 29.99))
  {
    time_base->num = 1001;
    time_base->den = 30000;

  }
  if ((framerate > 23.90) && (framerate < 23.99))
  {
    time_base->num = 1001;
    time_base->den = 24000;

  }

}  /* end p_set_timebase_from_framerate */


/* ------------------
 * p_ffmpeg_open_init
 * ------------------
 */
static void
p_ffmpeg_open_init(t_ffmpeg_handle *ffh, GapGveFFMpegGlobalParams *gpp)
{
  gint ii;
  GapGveFFMpegValues   *epp;

  epp = &gpp->evl;

  /* clear all elements of the video stream tables */
  for (ii=0; ii < MAX_VIDEO_STREAMS; ii++)
  {
    ffh->vst[ii].video_stream_index = 0;
    ffh->vst[ii].vid_stream = NULL;
    ffh->vst[ii].vid_codec_context = NULL;
    ffh->vst[ii].vid_codec = NULL;
    ffh->vst[ii].big_picture_codec = avcodec_alloc_frame();
    ffh->vst[ii].video_buffer = NULL;
    ffh->vst[ii].video_buffer_size = 0;
    ffh->vst[ii].video_dummy_buffer = NULL;
    ffh->vst[ii].video_dummy_buffer_size = 0;
    ffh->vst[ii].yuv420_dummy_buffer = NULL;
    ffh->vst[ii].yuv420_dummy_buffer_size = 0;
    ffh->vst[ii].passlog_fp = NULL;
  }


  /* clear all elements of the audio stream and audio work tables */
  for (ii=0; ii < MAX_AUDIO_STREAMS; ii++)
  {
    ffh->ast[ii].audio_stream_index = 0;
    ffh->ast[ii].aud_stream = NULL;
    ffh->ast[ii].aud_codec_context = NULL;
    ffh->ast[ii].aud_codec = NULL;
    ffh->ast[ii].audio_buffer = NULL;
    ffh->ast[ii].audio_buffer_size = 0;
  }

  {
    AVRational l_time_base;

    p_set_timebase_from_framerate(&l_time_base, gpp->val.framerate);
    ffh->pts_stepsize_per_frame = l_time_base.num;

    if(gap_debug)
    {
      printf("p_ffmpeg_open_init  time_base: num:%d den:%d  pts_stepsize_per_frame:%.4f\n"
        ,(int)l_time_base.num
        ,(int)l_time_base.den
        ,(float)ffh->pts_stepsize_per_frame
        );
    }
  }


  /* initialize common things */
  ffh->frame_width                  = (int)gpp->val.vid_width;
  ffh->frame_height                 = (int)gpp->val.vid_height;
  /* allocate buffer for image conversion large enough for for uncompressed RGBA32 colormodel */
  ffh->convert_buffer = g_malloc(4 * ffh->frame_width * ffh->frame_height);

  ffh->audio_bit_rate               = epp->audio_bitrate * 1000;     /* 64000; */
  ffh->audio_codec_id               = CODEC_ID_NONE;

  ffh->file_overwrite               = 0;
  ffh->countVideoFramesWritten      = 0;

}  /* end p_ffmpeg_open_init */


/* ------------------
 * p_choose_pixel_fmt
 * ------------------
 */
static void p_choose_pixel_fmt(AVStream *st, AVCodec *codec)
{
    if(codec && codec->pix_fmts){
        const enum PixelFormat *p= codec->pix_fmts;
        for(; *p!=-1; p++){
            if(*p == st->codec->pix_fmt)
                break;
        }
        if(*p == -1
           && !(   st->codec->codec_id==CODEC_ID_MJPEG
                && st->codec->strict_std_compliance <= FF_COMPLIANCE_INOFFICIAL
                && (   st->codec->pix_fmt == PIX_FMT_YUV420P
                    || st->codec->pix_fmt == PIX_FMT_YUV422P)))
            st->codec->pix_fmt = codec->pix_fmts[0];
    }
}


/* ------------------
 * p_init_video_codec
 * ------------------
 * add stream for stream index ii (ii=0 for the 1st video stream)
 * and init settings for the video codec to be used for encoding frames in the stream
 * (just prepare the video codec for opening, but dont open yet)
 */
static gboolean
p_init_video_codec(t_ffmpeg_handle *ffh
             , gint ii
             , GapGveFFMpegGlobalParams *gpp
             , gint32 current_pass)
{
  GapGveFFMpegValues   *epp;
  AVCodecContext *video_enc = NULL;

  epp = &gpp->evl;


  ffh->vst[ii].vid_codec = avcodec_find_encoder_by_name(epp->vcodec_name);
  if(!ffh->vst[ii].vid_codec)
  {
     printf("Unknown Video CODEC: %s\n", epp->vcodec_name);
     g_free(ffh);
     return(FALSE); /* error */
  }
  if(ffh->vst[ii].vid_codec->type != AVMEDIA_TYPE_VIDEO)
  {
     printf("CODEC: %s is no VIDEO CODEC!\n", epp->vcodec_name);
     g_free(ffh);
     return(FALSE); /* error */
  }


  /* set stream 0 (video stream) */

  ffh->vst[ii].vid_stream = av_new_stream(ffh->output_context, ii /* vid_stream_index */ );

  if(gap_debug)
  {
    printf("p_init_video_codec ffh->vst[%d].vid_stream: %d   ffh->output_context->streams[%d]: %d nb_streams:%d\n"
      ,(int)ii
      ,(int)ffh->vst[ii].vid_stream
      ,(int)ii
      ,(int)ffh->output_context->streams[ii]
      ,(int)ffh->output_context->nb_streams
      );
  }

  avcodec_get_context_defaults2(ffh->vst[ii].vid_stream->codec, AVMEDIA_TYPE_VIDEO);
  avcodec_thread_init(ffh->vst[ii].vid_stream->codec, epp->thread_count);

  /* set Video codec context in the video stream array (vst) */
  ffh->vst[ii].vid_codec_context = ffh->vst[ii].vid_stream->codec;


  /* some formats need to know about the coded_frame
   * and get the information from the AVCodecContext coded_frame Pointer
   * 2009.01.31 Not sure if this is still required in recent ffmpeg versions,
   * the coded_frame pointer seems to be dynamically allocated
   * (in avcodec_open and avcodec_encode_video calls)
   */
  ffh->vst[ii].vid_codec_context->coded_frame = ffh->vst[ii].big_picture_codec;


  video_enc = ffh->vst[ii].vid_codec_context;

  video_enc->codec_type = AVMEDIA_TYPE_VIDEO;
  video_enc->codec_id = ffh->vst[ii].vid_codec->id;

  video_enc->bit_rate = epp->video_bitrate * 1000;
  video_enc->bit_rate_tolerance = epp->bitrate_tol * 1000;

  p_set_timebase_from_framerate(&video_enc->time_base, gpp->val.framerate);

  video_enc->width = gpp->val.vid_width;
  video_enc->height = gpp->val.vid_height;

  if (gap_debug)
  {
    printf("VCODEC internal DEFAULTS: id:%d (%s) time_base.num :%d  time_base.den:%d    float:%f\n"
           "            DEFAULT_FRAME_RATE_BASE: %d\n"
      , video_enc->codec_id
      , epp->vcodec_name
      , video_enc->time_base.num
      , video_enc->time_base.den
      , (float)gpp->val.framerate
      , (int)DEFAULT_FRAME_RATE_BASE
      );

    /* dump all parameters (standard values reprsenting ffmpeg internal defaults) to stdout */
    p_debug_print_dump_AVCodecContext(video_enc);
  }


  video_enc->pix_fmt = PIX_FMT_YUV420P;     /* PIX_FMT_YUV444P;  PIX_FMT_YUV420P;  PIX_FMT_BGR24;  PIX_FMT_RGB24; */

  p_choose_pixel_fmt(ffh->vst[ii].vid_stream, ffh->vst[ii].vid_codec);


  /* some formats want stream headers to be separate */
  if(ffh->output_context->oformat->flags & AVFMT_GLOBALHEADER)
  {
    video_enc->flags |= CODEC_FLAG_GLOBAL_HEADER;
  }
  if(!strcmp(epp->format_name, "mp4")
  || !strcmp(epp->format_name, "mov")
  || !strcmp(epp->format_name, "3gp"))
  {
    video_enc->flags |= CODEC_FLAG_GLOBAL_HEADER;
  }

  /* some formats want stream headers to be separate */
  if(ffh->output_context->oformat->flags & AVFMT_GLOBALHEADER)
  {
    video_enc->flags |= CODEC_FLAG_GLOBAL_HEADER;
  }

  /* mb_decision changed in ffmpeg-0.4.8
   *  0: FF_MB_DECISION_SIMPLE
   *  1: FF_MB_DECISION_BITS
   *  2: FF_MB_DECISION_RD
   */
  video_enc->mb_decision = epp->mb_decision;
  if (video_enc->codec_id == CODEC_ID_MPEG1VIDEO)
  {
        /* Needed to avoid using macroblocks in which some coeffs overflow.
           This does not happen with normal video, it just happens here as
           the motion of the chroma plane does not match the luma plane. */
        video_enc->mb_decision=2;
  }

  if(epp->qscale)
  {
    video_enc->flags |= CODEC_FLAG_QSCALE;
    ffh->vst[ii].vid_stream->quality = FF_QP2LAMBDA * epp->qscale;   /* video_enc->quality = epp->qscale; */
    video_enc->global_quality =  FF_QP2LAMBDA * epp->qscale;         /* 2009.01.31 must init global_quality to avoid crash */
  }


  /**
   * AVRational sample aspect ratio (0 if unknown).
   * numerator and denominator must be relative prime and smaller then 256 for some video standards
   * - encoding: set by user.
   * - decoding: set by lavc.
   */
  video_enc->sample_aspect_ratio.num = 0;
  video_enc->sample_aspect_ratio.den = 1;
  if(epp->set_aspect_ratio)
  {
    gdouble l_aspect_factor;
    gdouble l_dbl;


    if(epp->factor_aspect_ratio == 0.0)
    {
      l_aspect_factor = (gdouble)gpp->val.vid_width / (gdouble)(MAX(1,gpp->val.vid_height));
    }
    else
    {
      l_aspect_factor = epp->factor_aspect_ratio;
    }

    /* dont understand why to multiply aspect by height and divide by width
     * but seen this in ffmpeg.c sourcecode that seems to work
     * A typical 720x576 frame
     *   with  4:3 format will result in NUM:16  DEN:15
     *   with 16:9 format will result in NUM: 64 DEN:45
     */
    l_dbl = l_aspect_factor * (gdouble)gpp->val.vid_height / (gdouble)MAX(1,gpp->val.vid_width);

    if(gap_debug)
    {
      printf("ASPECT ratio DEFAULT setting: NUM:%d  DEN:%d\n"
            ,(int)video_enc->sample_aspect_ratio.num
            ,(int)video_enc->sample_aspect_ratio.den
            );
    }
    video_enc->sample_aspect_ratio = av_d2q(l_dbl, 255);
    ffh->vst[ii].vid_stream->sample_aspect_ratio = video_enc->sample_aspect_ratio;  /* 2009.01.31 must init stream sample_aspect_ratio to avoid crash */

    if(gap_debug)
    {
      printf("ASPECT ratio (w/h): %f l_dbl:%f av_d2q NUM:%d  DEN:%d\n"
            ,(float)l_aspect_factor
            ,(float)l_dbl
            ,(int)video_enc->sample_aspect_ratio.num
            ,(int)video_enc->sample_aspect_ratio.den
            );
    }

  }

  /* the following options were added after ffmpeg 0.4.8 */

  video_enc->thread_count   = epp->thread_count;
  video_enc->mb_cmp         = epp->mb_cmp;
  video_enc->ildct_cmp      = epp->ildct_cmp;
  video_enc->me_sub_cmp     = epp->sub_cmp;
  video_enc->me_cmp         = epp->cmp;
  video_enc->me_pre_cmp     = epp->pre_cmp;
  video_enc->pre_me         = epp->pre_me;
  video_enc->lumi_masking   = epp->lumi_mask;
  video_enc->dark_masking   = epp->dark_mask;
  video_enc->spatial_cplx_masking    = epp->scplx_mask;
  video_enc->temporal_cplx_masking   = epp->tcplx_mask;
  video_enc->p_masking               = epp->p_mask;
  video_enc->quantizer_noise_shaping = epp->qns;

  if(!epp->intra)  { video_enc->gop_size = epp->gop_size;  }
  else             { video_enc->gop_size = 0;              }

  /* set codec flags */
  p_set_flag(epp->bitexact,         &video_enc->flags, CODEC_FLAG_BITEXACT);
  p_set_flag(epp->umv,              &video_enc->flags, CODEC_FLAG_H263P_UMV);
  p_set_flag(epp->use_ss,           &video_enc->flags, CODEC_FLAG_H263P_SLICE_STRUCT);
  p_set_flag(epp->use_aiv,          &video_enc->flags, CODEC_FLAG_H263P_AIV);
  p_set_flag(epp->use_obmc,         &video_enc->flags, CODEC_FLAG_OBMC);
  p_set_flag(epp->use_loop,         &video_enc->flags, CODEC_FLAG_LOOP_FILTER);
  p_set_flag(epp->use_alt_scan,     &video_enc->flags, CODEC_FLAG_ALT_SCAN);
  p_set_flag(epp->use_mv0,          &video_enc->flags, CODEC_FLAG_MV0);
  p_set_flag(epp->do_normalize_aqp, &video_enc->flags, CODEC_FLAG_NORMALIZE_AQP);
  p_set_flag(epp->use_scan_offset,  &video_enc->flags, CODEC_FLAG_SVCD_SCAN_OFFSET);
  p_set_flag(epp->closed_gop,       &video_enc->flags, CODEC_FLAG_CLOSED_GOP);
  p_set_flag(epp->use_qpel,         &video_enc->flags, CODEC_FLAG_QPEL);
  p_set_flag(epp->use_qprd,         &video_enc->flags, CODEC_FLAG_QP_RD);
  p_set_flag(epp->use_cbprd,        &video_enc->flags, CODEC_FLAG_CBP_RD);
  p_set_flag(epp->do_interlace_dct, &video_enc->flags, CODEC_FLAG_INTERLACED_DCT);
  p_set_flag(epp->do_interlace_me,  &video_enc->flags, CODEC_FLAG_INTERLACED_ME);
  p_set_flag(epp->aic,              &video_enc->flags, CODEC_FLAG_AC_PRED);
  p_set_flag(epp->mv4,              &video_enc->flags, CODEC_FLAG_4MV);
  p_set_flag(epp->partitioning,     &video_enc->flags, CODEC_FLAG_PART);

  p_set_flag(epp->codec_FLAG_GMC,             &video_enc->flags, CODEC_FLAG_GMC);
  p_set_flag(epp->codec_FLAG_INPUT_PRESERVED, &video_enc->flags, CODEC_FLAG_INPUT_PRESERVED);
  p_set_flag(epp->codec_FLAG_GRAY,            &video_enc->flags, CODEC_FLAG_GRAY);
  p_set_flag(epp->codec_FLAG_EMU_EDGE,        &video_enc->flags, CODEC_FLAG_EMU_EDGE);
  p_set_flag(epp->codec_FLAG_TRUNCATED,       &video_enc->flags, CODEC_FLAG_TRUNCATED);

  /* set codec flags2 */
  p_set_flag(epp->strict_gop,                      &video_enc->flags2, CODEC_FLAG2_STRICT_GOP);
  p_set_flag(epp->no_output,                       &video_enc->flags2, CODEC_FLAG2_NO_OUTPUT);
  p_set_flag(epp->codec_FLAG2_FAST,                &video_enc->flags2, CODEC_FLAG2_FAST);
  p_set_flag(epp->codec_FLAG2_LOCAL_HEADER,        &video_enc->flags2, CODEC_FLAG2_LOCAL_HEADER);
  p_set_flag(epp->codec_FLAG2_BPYRAMID,            &video_enc->flags2, CODEC_FLAG2_BPYRAMID);
  p_set_flag(epp->codec_FLAG2_WPRED,               &video_enc->flags2, CODEC_FLAG2_WPRED);
  p_set_flag(epp->codec_FLAG2_MIXED_REFS,          &video_enc->flags2, CODEC_FLAG2_MIXED_REFS);
  p_set_flag(epp->codec_FLAG2_8X8DCT,              &video_enc->flags2, CODEC_FLAG2_8X8DCT);
  p_set_flag(epp->codec_FLAG2_FASTPSKIP,           &video_enc->flags2, CODEC_FLAG2_FASTPSKIP);
  p_set_flag(epp->codec_FLAG2_AUD,                 &video_enc->flags2, CODEC_FLAG2_AUD);
  p_set_flag(epp->codec_FLAG2_BRDO,                &video_enc->flags2, CODEC_FLAG2_BRDO);
  p_set_flag(epp->codec_FLAG2_INTRA_VLC,           &video_enc->flags2, CODEC_FLAG2_INTRA_VLC);
  p_set_flag(epp->codec_FLAG2_MEMC_ONLY,           &video_enc->flags2, CODEC_FLAG2_MEMC_ONLY);
  p_set_flag(epp->codec_FLAG2_DROP_FRAME_TIMECODE, &video_enc->flags2, CODEC_FLAG2_DROP_FRAME_TIMECODE);
  p_set_flag(epp->codec_FLAG2_SKIP_RD,             &video_enc->flags2, CODEC_FLAG2_SKIP_RD);
  p_set_flag(epp->codec_FLAG2_CHUNKS,              &video_enc->flags2, CODEC_FLAG2_CHUNKS);
  p_set_flag(epp->codec_FLAG2_NON_LINEAR_QUANT,    &video_enc->flags2, CODEC_FLAG2_NON_LINEAR_QUANT);
  p_set_flag(epp->codec_FLAG2_BIT_RESERVOIR,       &video_enc->flags2, CODEC_FLAG2_BIT_RESERVOIR);

#ifndef GAP_USES_OLD_FFMPEG_0_5
  p_set_flag(epp->codec_FLAG2_MBTREE,              &video_enc->flags2, CODEC_FLAG2_MBTREE);
  p_set_flag(epp->codec_FLAG2_PSY,                 &video_enc->flags2, CODEC_FLAG2_PSY);
  p_set_flag(epp->codec_FLAG2_SSIM,                &video_enc->flags2, CODEC_FLAG2_SSIM);
#ifndef GAP_USES_OLD_FFMPEG_0_6
  p_set_flag(epp->codec_FLAG2_INTRA_REFRESH,       &video_enc->flags2, CODEC_FLAG2_INTRA_REFRESH);
#endif
#endif


  if ((epp->b_frames > 0) && (!epp->intra))
  {
    /* hof: TODO: here we should check if the selected codec
     * has the ability to encode B_frames
     * (dont know how to do yet)
     *
     * as workaround i do check for the MSMPEG4 codec ID's
     * because these codecs do crash (at least the version of the ffmpeg-0.4.8 release)
     * when b_frames setting > 0 is used
     */
    switch(video_enc->codec_id)
    {
      case CODEC_ID_MSMPEG4V1:
      case CODEC_ID_MSMPEG4V2:
      case CODEC_ID_MSMPEG4V3:
        if(gpp->val.run_mode == GIMP_RUN_NONINTERACTIVE)
        {
          printf("\n### Warning: B frames encoding not supported by selected codec. (option ignored)\n");
        }
        else
        {
          g_message("Warning: B frames encoding not supported by selected codec. (option ignored)\n");
        }
        break;
      default:
        video_enc->max_b_frames = epp->b_frames;
        video_enc->b_frame_strategy = epp->b_frame_strategy;  /* default 0 */
        break;
    }
  }

  video_enc->qmin      = epp->qmin;
  video_enc->qmax      = epp->qmax;
  video_enc->max_qdiff = epp->qdiff;
  video_enc->qblur     = (float)epp->qblur;
  video_enc->qcompress = (float)epp->qcomp;
#ifndef GAP_USES_OLD_FFMPEG_0_5
  if (epp->rc_eq[0] != '\0')
  {
    video_enc->rc_eq     = &epp->rc_eq[0];
  }
#else
  if (epp->rc_eq[0] != '\0')
  {
    video_enc->rc_eq     = &epp->rc_eq[0];
  }
  else
  {
    /* use default rate control equation */
    video_enc->rc_eq     = "tex^qComp";
  }
#endif

  video_enc->rc_override_count      =0;
  video_enc->inter_threshold        = epp->inter_threshold;
  video_enc->rc_max_rate            = epp->maxrate_tol * 1000;
  video_enc->rc_min_rate            = epp->minrate_tol * 1000;
  video_enc->rc_buffer_size         = epp->bufsize * 8*1024;
  video_enc->rc_buffer_aggressivity = (float)epp->rc_buffer_aggressivity;
  video_enc->rc_initial_cplx        = (float)epp->rc_init_cplx;
  video_enc->i_quant_factor         = (float)epp->i_qfactor;
  video_enc->b_quant_factor         = (float)epp->b_qfactor;
  video_enc->i_quant_offset         = (float)epp->i_qoffset;
  video_enc->b_quant_offset         = (float)epp->b_qoffset;
  video_enc->dct_algo               = epp->dct_algo;
  video_enc->idct_algo              = epp->idct_algo;
  video_enc->strict_std_compliance  = epp->strict;
  video_enc->workaround_bugs        = epp->workaround_bugs;
  video_enc->error_recognition      = epp->error_recognition;
  video_enc->mpeg_quant             = epp->mpeg_quant;


  video_enc->debug                  = 0;
  video_enc->mb_qmin                = epp->mb_qmin;
  video_enc->mb_qmax                = epp->mb_qmax;

  video_enc->rc_initial_buffer_occupancy = video_enc->rc_buffer_size * 3/4;
  video_enc->lmin                   = epp->video_lmin;
  video_enc->lmax                   = epp->video_lmax;
  video_enc->rc_qsquish             = epp->rc_qsquish;
  video_enc->rc_qmod_amp            = epp->rc_qmod_amp;
  video_enc->rc_qmod_freq           = epp->rc_qmod_freq;


  video_enc->luma_elim_threshold    = epp->video_lelim;
  video_enc->chroma_elim_threshold  = epp->video_celim;

  video_enc->intra_quant_bias       = epp->video_intra_quant_bias;
  video_enc->inter_quant_bias       = epp->video_inter_quant_bias;

  video_enc->me_threshold           = epp->me_threshold;
  if(epp->me_threshold)
  {
    video_enc->debug |= FF_DEBUG_MV;
  }

  video_enc->mb_threshold           = epp->mb_threshold;
  video_enc->intra_dc_precision     = epp->intra_dc_precision - 8;

  video_enc->error_rate             = epp->error_rate;
  video_enc->noise_reduction        = epp->noise_reduction;
  video_enc->scenechange_threshold  = epp->sc_threshold;
  video_enc->me_range               = epp->me_range;
  video_enc->coder_type             = epp->coder;
  video_enc->context_model          = epp->context;
  video_enc->prediction_method      = epp->predictor;

  video_enc->nsse_weight            = epp->nsse_weight;
  video_enc->me_subpel_quality      = epp->subpel_quality;

  video_enc->dia_size               = epp->dia_size;
  video_enc->last_predictor_count   = epp->last_predictor_count;
  video_enc->pre_dia_size           = epp->pre_dia_size;


  video_enc->mb_lmin                  = epp->video_mb_lmin;
  video_enc->mb_lmax                  = epp->video_mb_lmax;
  video_enc->profile                  = epp->video_profile;
  video_enc->level                    = epp->video_level;
  video_enc->frame_skip_threshold     = epp->frame_skip_threshold;
  video_enc->frame_skip_factor        = epp->frame_skip_factor;
  video_enc->frame_skip_exp           = epp->frame_skip_exp;
  video_enc->frame_skip_cmp           = epp->frame_skip_cmp;
  video_enc->border_masking           = (float)epp->border_masking;
  video_enc->me_penalty_compensation  = epp->me_penalty_compensation;
  video_enc->bidir_refine             = epp->bidir_refine;
  video_enc->brd_scale                = epp->brd_scale;
  video_enc->crf                      = (float)epp->crf;
  video_enc->cqp                      = epp->cqp;
  video_enc->keyint_min               = epp->keyint_min;
  video_enc->refs                     = epp->refs;
  video_enc->chromaoffset             = epp->chromaoffset;
  video_enc->bframebias               = epp->bframebias;
  video_enc->trellis                  = epp->trellis;
  video_enc->complexityblur           = (float) epp->complexityblur;
  video_enc->deblockalpha             = epp->deblockalpha;
  video_enc->deblockbeta              = epp->deblockbeta;
  video_enc->partitions               = epp->partitions;
  p_set_partition_flag(epp->partition_X264_PART_I4X4,        &video_enc->partitions, X264_PART_I4X4);
  p_set_partition_flag(epp->partition_X264_PART_I8X8,        &video_enc->partitions, X264_PART_I8X8);
  p_set_partition_flag(epp->partition_X264_PART_P8X8,        &video_enc->partitions, X264_PART_P8X8);
  p_set_partition_flag(epp->partition_X264_PART_P4X4,        &video_enc->partitions, X264_PART_P4X4);
  p_set_partition_flag(epp->partition_X264_PART_B8X8,        &video_enc->partitions, X264_PART_B8X8);
  
  video_enc->directpred               = epp->directpred;
  video_enc->scenechange_factor       = epp->scenechange_factor;
  video_enc->mv0_threshold            = epp->mv0_threshold;
  video_enc->b_sensitivity            = epp->b_sensitivity;
  video_enc->compression_level        = epp->compression_level;
  video_enc->use_lpc                  = epp->use_lpc;
  video_enc->lpc_coeff_precision      = epp->lpc_coeff_precision;
  video_enc->min_prediction_order     = epp->min_prediction_order;
  video_enc->max_prediction_order     = epp->max_prediction_order;
  video_enc->prediction_order_method  = epp->prediction_order_method;
  video_enc->min_partition_order      = epp->min_partition_order;
  video_enc->timecode_frame_start     = epp->timecode_frame_start;
  video_enc->bits_per_raw_sample      = epp->bits_per_raw_sample;
  video_enc->rc_max_available_vbv_use = (float)epp->rc_max_available_vbv_use;
  video_enc->rc_min_vbv_overflow_use  = (float)epp->rc_min_vbv_overflow_use;

#ifndef GAP_USES_OLD_FFMPEG_0_5
  //video_enc->hwaccel_context == NULL;
  video_enc->color_primaries = epp->color_primaries;
  video_enc->color_trc = epp->color_trc;
  video_enc->colorspace = epp->colorspace;
  video_enc->color_range = epp->color_range;
  video_enc->chroma_sample_location = epp->chroma_sample_location;
  //video_enc->func = NULL;
  video_enc->weighted_p_pred = (int)epp->weighted_p_pred;
  video_enc->aq_mode = (int)epp->aq_mode;
  video_enc->aq_strength = (float)epp->aq_strength;
  video_enc->psy_rd = (float)epp->psy_rd;
  video_enc->psy_trellis = (float)epp->psy_trellis;
  video_enc->rc_lookahead = (int)epp->rc_lookahead;
#ifndef GAP_USES_OLD_FFMPEG_0_6
  video_enc->crf_max = (float)epp->crf_max;
  video_enc->log_level_offset = (int)epp->log_level_offset;
  video_enc->slices = (int)epp->slices;
  video_enc->thread_type = (int)epp->thread_type;
  video_enc->active_thread_type = (int)epp->active_thread_type;
  video_enc->thread_safe_callbacks = (int)epp->thread_safe_callbacks;
  video_enc->vbv_delay = epp->vbv_delay;
  video_enc->audio_service_type = epp->audio_service_type;
#endif
#endif


  if(epp->packet_size)
  {
      video_enc->rtp_payload_size   = epp->packet_size;
  }

  if (epp->psnr) {  video_enc->flags |= CODEC_FLAG_PSNR; }

  video_enc->me_method = epp->motion_estimation;


  if (epp->title[0] != '\0')
  {
      p_av_metadata_set(&ffh->output_context->metadata, "title",  &epp->title[0], 0);
  }
  if (epp->author[0] != '\0')
  {
      p_av_metadata_set(&ffh->output_context->metadata, "author",  &epp->author[0], 0);
  }
  if (epp->copyright[0] != '\0')
  {
      p_av_metadata_set(&ffh->output_context->metadata, "copyright",  &epp->copyright[0], 0);
  }
  if (epp->comment[0] != '\0')
  {
      p_av_metadata_set(&ffh->output_context->metadata, "comment",  &epp->comment[0], 0);
  }


  /* ----------------- 2 pass handling ------------------------ */
  if (current_pass > 0)
  {
      if (current_pass == 1)
      {
          video_enc->flags |= CODEC_FLAG_PASS1;
      } else
      {
          video_enc->flags |= CODEC_FLAG_PASS2;
      }
  }
  ffh->vst[ii].passlog_fp = NULL;

  if (video_enc->flags & (CODEC_FLAG_PASS1 | CODEC_FLAG_PASS2))
  {
      char logfilename[1024];
      FILE *fp;
      int size;
      char *logbuffer;
      gint l_errno;

      snprintf(logfilename, sizeof(logfilename), "%s.log", epp->passlogfile);
      if (video_enc->flags & CODEC_FLAG_PASS1)
      {
          fp = g_fopen(logfilename, "w");
          l_errno = errno;
          if (!fp)
          {
              if(gpp->val.run_mode == GIMP_RUN_NONINTERACTIVE)
              {
                perror(logfilename);
              }
              else
              {
                g_message(_("Could not create pass logfile:"
                           "'%s'"
                           "%s")
                           ,logfilename
                           ,g_strerror (l_errno) );
              }
              return(FALSE);  /* error */
          }
          ffh->vst[ii].passlog_fp = fp;
      }
      else
      {
          /* read the log file */
          fp = g_fopen(logfilename, "r");
          l_errno = errno;
          if (!fp)
          {
              if(gpp->val.run_mode == GIMP_RUN_NONINTERACTIVE)
              {
                perror(logfilename);
              }
              else
              {
                g_message(_("Could not open pass logfile:"
                           "'%s'"
                           "%s")
                           ,logfilename
                           ,g_strerror (l_errno) );
              }
              return(FALSE);  /* error */
          }
          fseek(fp, 0, SEEK_END);
          size = ftell(fp);
          fseek(fp, 0, SEEK_SET);
          logbuffer = g_malloc(size + 1);

          fread(logbuffer, 1, size, fp);
          fclose(fp);
          logbuffer[size] = '\0';
          video_enc->stats_in = logbuffer;
      }
  }

  /* dont know why */
  /* *(ffh->vst[ii].vid_stream->codec) = *video_enc; */    /* XXX(#) copy codec context */

  if (gap_debug)
  {
    printf("VCODEC initialized: id:%d (%s) time_base.num :%d  time_base.den:%d    float:%f\n"
           "            DEFAULT_FRAME_RATE_BASE: %d\n"
      , video_enc->codec_id
      , epp->vcodec_name
      , video_enc->time_base.num
      , video_enc->time_base.den
      , (float)gpp->val.framerate
      , (int)DEFAULT_FRAME_RATE_BASE
      );

    /* dump all parameters (standard values reprsenting ffmpeg internal defaults) to stdout */
    p_debug_print_dump_AVCodecContext(video_enc);
  }

  return (TRUE); /* OK */

}  /* end p_init_video_codec */


/* ---------------------------
 * p_init_and_open_audio_codec
 * ---------------------------
 */
static gboolean
p_init_and_open_audio_codec(t_ffmpeg_handle *ffh
             , gint ii
             , GapGveFFMpegGlobalParams *gpp
             , long audio_samplerate
             , long audio_channels
             , long bits
             )
{
  GapGveFFMpegValues   *epp;
  AVCodecContext *audio_enc = NULL;
  gboolean audioOK;
  char *msg;

  epp = &gpp->evl;

  audioOK = TRUE;
  msg = NULL;

  /* ------------ Start Audio CODEC init -------   */
  if(audio_channels > 0)
  {
    if(gap_debug) printf("p_ffmpeg_open Audio CODEC start init\n");

    ffh->ast[ii].aud_codec = avcodec_find_encoder_by_name(epp->acodec_name);
    if(!ffh->ast[ii].aud_codec)
    {
       audioOK = FALSE;
       msg = g_strdup_printf(_("Unknown Audio CODEC: %s"), epp->acodec_name);
    }
    else
    {
      if(ffh->ast[ii].aud_codec->type != AVMEDIA_TYPE_AUDIO)
      {
         audioOK = FALSE;
         msg = g_strdup_printf(_("CODEC: %s is no AUDIO CODEC!"), epp->acodec_name);
         ffh->ast[ii].aud_codec = NULL;
      }
      else
      {
        gint ast_index;

        /* audio stream indexes start after last video stream */
        ast_index = ffh->max_vst + ii;

        /* set stream 1 (audio stream) */
        ffh->ast[ii].aud_stream = av_new_stream(ffh->output_context, ast_index /* aud_stream_index */ );
        ffh->ast[ii].aud_codec_context = ffh->ast[ii].aud_stream->codec;

        /* set codec context */
        /*  ffh->ast[ii].aud_codec_context = avcodec_alloc_context();*/
        audio_enc = ffh->ast[ii].aud_codec_context;

        audio_enc->codec_type = AVMEDIA_TYPE_AUDIO;
        audio_enc->codec_id = ffh->ast[ii].aud_codec->id;

        audio_enc->bit_rate = epp->audio_bitrate * 1000;
        audio_enc->sample_rate = audio_samplerate;  /* was read from wav file */
        audio_enc->channels = audio_channels;       /* 1=mono, 2 = stereo (from wav file) */


        /* the following options were added after ffmpeg 0.4.8 */
        audio_enc->thread_count = epp->thread_count;
        audio_enc->strict_std_compliance = epp->strict;
        audio_enc->workaround_bugs       = epp->workaround_bugs;
        audio_enc->error_recognition     = epp->error_recognition;
        audio_enc->cutoff                = epp->cutoff;

        if (bits == 16)
        {
          audio_enc->sample_fmt = SAMPLE_FMT_S16;
        }
        if (bits == 8)
        {
          audio_enc->sample_fmt = SAMPLE_FMT_U8;
        }


        switch (audio_channels)
        {
          case 1:
            audio_enc->channel_layout = CH_LAYOUT_MONO;
            break;
          case 2:
            audio_enc->channel_layout = CH_LAYOUT_STEREO;  /* CH_LAYOUT_STEREO_DOWNMIX ? */
            break;
          default:
            audio_enc->channel_layout = epp->channel_layout;
            break;

        }

        /* open audio codec */
        if (avcodec_open(ffh->ast[ii].aud_codec_context, ffh->ast[ii].aud_codec) < 0)
        {
          audioOK = FALSE;

          msg = g_strdup_printf(_("could not open audio codec: %s\n"
                      "at audio_samplerate:%d channels:%d bits per channel:%d\n"
                      "(try to convert to 48 KHz, 44.1KHz or 32 kHz samplerate\n"
                      "that is supported by most codecs)")
                     , epp->acodec_name
                     , (int)audio_samplerate
                     , (int)audio_channels
                     , (int)bits
                     );
          ffh->ast[ii].aud_codec = NULL;
        }

      }
    }
  }

  if (msg != NULL)
  {
    if(gpp->val.run_mode == GIMP_RUN_NONINTERACTIVE)
    {
      printf("** Error: %s\n", msg);
    }
    else
    {
       g_message(msg);
    }
    g_free(msg);
  }
  return (audioOK);

} /* end p_init_and_open_audio_codec */



/* ---------------
 * p_ffmpeg_open
 * ---------------
 * master open procedure
 */
static t_ffmpeg_handle *
p_ffmpeg_open(GapGveFFMpegGlobalParams *gpp
             , gint32 current_pass
             , t_awk_array *awp
             , gint video_tracks
             )
{
  t_ffmpeg_handle *ffh;
  GapGveFFMpegValues   *epp;
  gint ii;

  if(gap_debug) printf("\np_ffmpeg_open START\n");

  epp = &gpp->evl;

  if(((gpp->val.vid_width % 2) != 0) || ((gpp->val.vid_height % 2) != 0))
  {
    if(gpp->val.run_mode == GIMP_RUN_NONINTERACTIVE)
    {
      printf(_("Frame width and height must be a multiple of 2\n"));
    }
    else
    {
      g_message(_("Frame width and height must be a multiple of 2\n"));
    }
    return (NULL);
  }

  ffh = g_malloc0(sizeof(t_ffmpeg_handle));
  ffh->isMultithreadEnabled = FALSE;
  ffh->max_vst = MIN(MAX_VIDEO_STREAMS, video_tracks);
  ffh->max_ast = MIN(MAX_AUDIO_STREAMS, awp->audio_tracks);

  av_register_all();  /* register all fileformats and codecs before we can use the lib */

  p_ffmpeg_open_init(ffh, gpp);

  /* ------------ File Format  -------   */

#ifndef GAP_USES_OLD_FFMPEG_0_5
  ffh->file_oformat = av_guess_format(epp->format_name, gpp->val.videoname, NULL);
#else
  ffh->file_oformat = guess_format(epp->format_name, gpp->val.videoname, NULL);
#endif
  if (!ffh->file_oformat)
  {
     printf("Unknown output format: %s\n", epp->format_name);
     g_free(ffh);
     return(NULL);
  }

  if(gap_debug)
  {
    printf("AVOutputFormat ffh->file_oformat opened\n");
    printf("  name: %s\n", ffh->file_oformat->name);
    printf("  long_name: %s\n", ffh->file_oformat->long_name);
    printf("  extensions: %s\n", ffh->file_oformat->extensions);
    printf("  priv_data_size: %d\n", (int)ffh->file_oformat->priv_data_size);
    printf("  ID default audio_codec: %d\n", (int)ffh->file_oformat->audio_codec);
    printf("  ID default video_codec: %d\n", (int)ffh->file_oformat->video_codec);
    printf("  FPTR write_header: %d\n", (int)ffh->file_oformat->write_header);
    printf("  FPTR write_packet: %d\n", (int)ffh->file_oformat->write_packet);
    printf("  FPTR write_trailer: %d\n", (int)ffh->file_oformat->write_trailer);
    printf("  flags: %d\n", (int)ffh->file_oformat->flags);
    printf("\n");
  }


  ffh->output_context = avformat_alloc_context();
  ffh->output_context->oformat = ffh->file_oformat;
  g_snprintf(ffh->output_context->filename, sizeof(ffh->output_context->filename), "%s", &gpp->val.videoname[0]);


  ffh->output_context->nb_streams = 0;  /* number of streams */
  ffh->output_context->timestamp = 0;

  /* ------------ video codec -------------- */

  for (ii=0; ii < ffh->max_vst; ii++)
  {
    gboolean codec_ok;

    codec_ok = p_init_video_codec(ffh, ii, gpp, current_pass);
    if(!codec_ok)
    {
      g_free(ffh);
      return(NULL);
    }
  }


  /* open output FILE */
  g_snprintf(ffh->output_context->filename, sizeof(ffh->output_context->filename), "%s", &gpp->val.videoname[0]);
  if (url_fopen(&ffh->output_context->pb, gpp->val.videoname, URL_WRONLY) < 0)
  {
    gint l_errno;

    l_errno = errno;
    g_message(_("Could not create videofile:"
               "'%s'"
               "%s")
               ,gpp->val.videoname
               ,g_strerror (l_errno) );
    return(NULL);
  }



  /* open video codec(s) */
  for (ii=0; ii < ffh->max_vst; ii++)
  {
    if (avcodec_open(ffh->vst[ii].vid_codec_context, ffh->vst[ii].vid_codec) < 0)
    {
      if(gpp->val.run_mode == GIMP_RUN_NONINTERACTIVE)
      {
        printf("could not avcodec_open video-codec: %s\n", epp->vcodec_name);
      }
      else
      {
        g_message("could not open video codec: %s\n", epp->vcodec_name);
      }
      return(NULL);
    }
  }

  /* ------------ audio codec -------------- */

  for (ii=0; ii < MIN(ffh->max_ast, awp->audio_tracks); ii++)
  {
    gboolean codec_ok;
    codec_ok = p_init_and_open_audio_codec(ffh
                                         , ii
                                         , gpp
                                         , awp->awk[ii].sample_rate
                                         , awp->awk[ii].channels
                                         , awp->awk[ii].bits
                                         );
    if(!codec_ok)
    {
      g_free(ffh);
      return(NULL);
    }
  }

  /* media file format parameters */

  ffh->ap = g_malloc0(sizeof(AVFormatParameters));

  /* we use sample rate and channels from the 1.st audio track for
   * the overall format parameters
   * (dont know if ffmpeg allows different settings for the audio tracks
   *  and how to deal with this)
   */
  ffh->ap->sample_rate = awp->awk[0].sample_rate;
  ffh->ap->channels = awp->awk[0].channels;

  p_set_timebase_from_framerate(&ffh->ap->time_base, gpp->val.framerate);

  ffh->ap->width = gpp->val.vid_width;
  ffh->ap->height = gpp->val.vid_height;

  ffh->ap->pix_fmt = PIX_FMT_YUV420P;

  /* tv standard, NTSC, PAL, SECAM */
  switch(gpp->val.vid_format)
  {
   /* gpp->val.vid_format possible values are:
    *    0=comp., 1=PAL, 2=NTSC, 3=SECAM, 4=MAC, 5=unspec
    * FFMPEG supports PAL, NTSC and SECAM
    */
   case VID_FMT_PAL:
     ffh->ap->standard = "PAL";
     break;
   case VID_FMT_NTSC:
     ffh->ap->standard = "NTSC";
     break;
   case VID_FMT_SECAM:
     ffh->ap->standard = "SECAM";
     break;
   default:
     ffh->ap->standard = "NTSC";
     break;
  }

  if (av_set_parameters(ffh->output_context, ffh->ap) < 0)
  {
    printf("Invalid output format parameters\n");
    return(NULL);
  }


  /* ------------ media file output  -------   */
  av_write_header(ffh->output_context);

  /* ------------ buffers for video output  -------   */
  for (ii=0; ii < ffh->max_vst; ii++)
  {
    gint  size;
    size = gpp->val.vid_width * gpp->val.vid_height;

    /* allocate video_buffer (should be large enough for one ENCODED frame) */
    ffh->vst[ii].video_buffer_size =  size * 4;  /* (1024*1024); */
    ffh->vst[ii].video_buffer = g_malloc0(ffh->vst[ii].video_buffer_size);
    ffh->vst[ii].video_stream_index = ii;

    ffh->vst[ii].video_dummy_buffer_size =  size * 4;  /* (1024*1024); */
    ffh->vst[ii].video_dummy_buffer = g_malloc0(ffh->vst[ii].video_dummy_buffer_size);

    ffh->vst[ii].yuv420_dummy_buffer_size = size + (size / 2);
    ffh->vst[ii].yuv420_dummy_buffer = g_malloc0(ffh->vst[ii].yuv420_dummy_buffer_size);

    memset(ffh->vst[ii].yuv420_dummy_buffer, 0, ffh->vst[ii].yuv420_dummy_buffer_size);
  }

  /* ------------ buffers for audio output  -------   */
  for (ii=0; ii < ffh->max_ast; ii++)
  {
    ffh->ast[ii].audio_buffer_size =  4 * MAX_AUDIO_PACKET_SIZE;
    ffh->ast[ii].audio_buffer = g_malloc0(ffh->ast[ii].audio_buffer_size);
    /* audio stream indexes start after last video stream */
    ffh->ast[ii].audio_stream_index = ffh->max_vst + ii;
  }



  ffh->output_context->packet_size = epp->mux_packet_size;
  ffh->output_context->mux_rate    = epp->mux_rate;
  ffh->output_context->preload     = (int)(epp->mux_preload*AV_TIME_BASE);
  ffh->output_context->max_delay   = (int)(epp->mux_max_delay*AV_TIME_BASE);

  ffh->img_convert_ctx = NULL; /* will be allocated at first img conversion */

  if(gap_debug)
  {
    printf("\np_ffmpeg_open END\n");
  }

  return(ffh);

}  /* end p_ffmpeg_open */


/* --------------------------
 * p_ffmpeg_write_frame_chunk
 * --------------------------
 * write videoframe chunk 1:1 to the mediafile as packet.
 * (typically used for lossless video cut to copy already encoded frames)
 */
static int
p_ffmpeg_write_frame_chunk(t_ffmpeg_handle *ffh, gint32 encoded_size, gint vid_track)
{
  int ret;
  int encoded_dummy_size;
  int ii;

  ret = 0;
  ii = ffh->vst[vid_track].video_stream_index;


  if(gap_debug)
  {
     AVCodec  *codec;

     codec = ffh->vst[ii].vid_codec_context->codec;

     printf("p_ffmpeg_write_frame_chunk: START codec: %d  track:%d\n", (int)codec, (int)vid_track);
     printf("\n-------------------------\n");
     printf("name: %s\n", codec->name);
     printf("type: %d\n", codec->type);
     printf("id: %d\n",   codec->id);
  }

  if(ffh->output_context)
  {
    if(gap_debug)  printf("before av_write_frame  encoded_size:%d\n", (int)encoded_size );

    {
      AVPacket pkt;
      av_init_packet (&pkt);
      gint chunk_frame_type;

      /*
       * FFMPEG 0.4.9 and later versions require monotone ascending timestamps
       * therefore we encode a fully black dummy frame
       * this is done to force codec internal advance of timestamps (PTS and DTS)
       * and to let the codec know about the handled frame (internal picture_number count)
       * TODO:
       *   find a faster way to fix PTS
       */
      {
        AVFrame   *big_picture_yuv;
        AVPicture *picture_yuv;
        AVPicture *picture_codec;

        /* picture to feed the codec */
        picture_codec = (AVPicture *)ffh->vst[ii].big_picture_codec;

        /* source picture (YUV420) */
        big_picture_yuv = avcodec_alloc_frame();
        picture_yuv = (AVPicture *)big_picture_yuv;

        /* Feed the encoder with a (black) dummy frame
         */
        avpicture_fill(picture_codec
                      ,ffh->vst[ii].yuv420_dummy_buffer
                      ,PIX_FMT_YUV420P          /* PIX_FMT_RGB24, PIX_FMT_RGBA32, PIX_FMT_BGRA32 */
                      ,ffh->frame_width
                      ,ffh->frame_height
                      );

        ffh->vst[ii].big_picture_codec->quality = ffh->vst[ii].vid_stream->quality;
        ffh->vst[ii].big_picture_codec->key_frame = 1;

        /* some codecs just pass through pts information obtained from
         * the AVFrame struct (big_picture_codec)
         * therefore init the pts code in this structure.
         * (a valid pts is essential to get encoded frame results in the correct order)
         */
        //ffh->vst[ii].big_picture_codec->pts = p_calculate_current_timecode(ffh);
        ffh->vst[ii].big_picture_codec->pts = ffh->encode_frame_nr -1;

        encoded_dummy_size = avcodec_encode_video(ffh->vst[ii].vid_codec_context
                               ,ffh->vst[ii].video_dummy_buffer, ffh->vst[ii].video_dummy_buffer_size
                               ,ffh->vst[ii].big_picture_codec);

        g_free(big_picture_yuv);
      }

      if(encoded_dummy_size == 0)
      {
        /* on size 0 the encoder has buffered the dummy frame.
         * this unwanted frame might be added when there is a next non-dummy frame
         * to encode and to write. (dont know if there is a chance to clear
         * the encoder's internal buffer in this case.)
         * in my tests this case did not happen yet....
         * .. but display a warning to find out in further test
         */
         g_message(_("Black dummy frame was added"));
      }

      chunk_frame_type = GVA_util_check_mpg_frame_type(ffh->vst[ii].video_buffer, encoded_size);
      if(chunk_frame_type == 1)  /* check for intra frame type */
      {
        pkt.flags |= AV_PKT_FLAG_KEY;
      }


      if(gap_debug)
      {
        printf("CHUNK picture_number: enc_dummy_size:%d  frame_type:%d pic_number: %d %d PTS:%lld\n"
              , (int)encoded_dummy_size
              , (int)chunk_frame_type
              , (int)ffh->vst[ii].vid_codec_context->coded_frame->coded_picture_number
              , (int)ffh->vst[ii].vid_codec_context->coded_frame->display_picture_number
              , ffh->vst[ii].vid_codec_context->coded_frame->pts
              );
      }

      ///// pkt.pts = ffh->vst[ii].vid_codec_context->coded_frame->pts;  // OLD
      {
        AVCodecContext *c;

        c = ffh->vst[ii].vid_codec_context;
        if (c->coded_frame->pts != AV_NOPTS_VALUE)
        {
          pkt.pts= av_rescale_q(c->coded_frame->pts, c->time_base, ffh->vst[ii].vid_stream->time_base);
        }
      }

      pkt.dts = AV_NOPTS_VALUE;  /* let av_write_frame calculate the decompression timestamp */
      pkt.stream_index = ffh->vst[ii].video_stream_index;
      pkt.data = ffh->vst[ii].video_buffer;
      pkt.size = encoded_size;
      ret = av_write_frame(ffh->output_context, &pkt);

      ffh->countVideoFramesWritten++;
    }

    if(gap_debug)  printf("after av_write_frame  encoded_size:%d\n", (int)encoded_size );
  }

  return (ret);
}  /* end p_ffmpeg_write_frame_chunk */


/* -----------------------
 * p_convert_colormodel
 * -----------------------
 * convert video frame specified in the rgb_buffer
 * from PIX_FMT_RGB24 to the colormodel that is required
 * by the video codec.
 *
 * conversion is done based on ffmpegs img_convert procedure.
 */
static void
p_convert_colormodel(t_ffmpeg_handle *ffh, AVPicture *picture_codec, guchar *rgb_buffer, gint vid_track)
{
  AVFrame   *big_picture_rgb;
  AVPicture *picture_rgb;
  int        ii;

  ii = ffh->vst[vid_track].video_stream_index;

  /* source picture (RGB) */
  big_picture_rgb = avcodec_alloc_frame();
  picture_rgb = (AVPicture *)big_picture_rgb;


  if(gap_debug)
  {
    printf("HAVE TO convert  TO pix_fmt: %d\n"
            , (int)ffh->vst[ii].vid_codec_context->pix_fmt
            );
  }



  /* init destination picture structure (the codec context tells us what pix_fmt is needed)
   */
   avpicture_fill(picture_codec
                  ,ffh->convert_buffer
                  ,ffh->vst[ii].vid_codec_context->pix_fmt          /* PIX_FMT_RGB24, PIX_FMT_RGBA32, PIX_FMT_BGRA32 */
                  ,ffh->frame_width
                  ,ffh->frame_height
                  );
  /* source picture (has RGB24 colormodel) */
   avpicture_fill(picture_rgb
                  ,rgb_buffer
                  ,PIX_FMT_RGB24
                  ,ffh->frame_width
                  ,ffh->frame_height
                  );

  /* AVFrame is the new structure introduced in FFMPEG 0.4.6,
   * (same as AVPicture but with additional members at the end)
   */

  if(gap_debug)
  {
    printf("before colormodel convert  pix_fmt: %d  (YUV420:%d)\n"
        , (int)ffh->vst[ii].vid_codec_context->pix_fmt
        , (int)PIX_FMT_YUV420P);
  }

  /* reuse the img_convert_ctx or create a new one
   * (in case ctx is NULL or params have changed)
   */
  ffh->img_convert_ctx = sws_getCachedContext(ffh->img_convert_ctx
                                         , ffh->frame_width
                                         , ffh->frame_height
                                         , PIX_FMT_RGB24               /* src pixelformat */
                                         , ffh->frame_width
                                         , ffh->frame_height
                                         , ffh->vst[ii].vid_codec_context->pix_fmt    /* dest pixelformat */
                                         , SWS_BICUBIC                 /* int sws_flags */
                                         , NULL, NULL, NULL
                                         );
  if (ffh->img_convert_ctx == NULL)
  {
     printf("Cannot initialize the conversion context (sws_getCachedContext delivered NULL pointer)\n");
     exit(1);
  }

  /* convert from RGB to pix_fmt needed by the codec */
  sws_scale(ffh->img_convert_ctx
           , picture_rgb->data        /* srcSlice */
           , picture_rgb->linesize    /* srcStride the array containing the strides for each plane */
           , 0                        /* srcSliceY starting at 0 */
           , ffh->frame_height        /* srcSliceH the height of the source slice */
           , picture_codec->data      /* dst */
           , picture_codec->linesize  /* dstStride the array containing the strides for each plane */
           );

  // /* img_convert is no longer available since ffmpeg.0.6 */
  //l_rc = img_convert(picture_codec, ffh->vst[ii].vid_codec_context->pix_fmt  /* dst */
  //             ,picture_rgb, PIX_FMT_BGR24                    /* src */
  //             ,ffh->frame_width
  //             ,ffh->frame_height
  //             );

  if(gap_debug)
  {
    printf("after sws_scale:\n");
  }

  g_free(big_picture_rgb);


  if(gap_debug)
  {
    printf("DONE p_convert_colormodel\n");
  }

}  /* end p_convert_colormodel */


/* ---------------------------------
 * p_ffmpeg_encodeAndWriteVideoFrame
 * ---------------------------------
 * encode one videoframe using the selected codec and write
 * the encoded frame to the mediafile as packet.
 */
static int
p_ffmpeg_encodeAndWriteVideoFrame(t_ffmpeg_handle *ffh, AVFrame *picture_codec
   , gboolean force_keyframe, gint vid_track, gint32 encode_frame_nr)
{
  int encoded_size;
  int ret;
  int ii;

  ii = ffh->vst[vid_track].video_stream_index;
  ret = 0;

  /* AVFrame is the new structure introduced in FFMPEG 0.4.6,
   * (same as AVPicture but with additional members at the end)
   */
  picture_codec->quality = ffh->vst[ii].vid_stream->quality;

  if((force_keyframe)
  || (encode_frame_nr == 1))
  {
    /* TODO: howto force the encoder to write an I frame ??
     *   ffh->vst[ii].big_picture_codec->key_frame could be ignored
     *   because this seems to be just an information
     *   reported by the encoder.
     */
    /* ffh->vst[ii].vid_codec_context->key_frame = 1; */
    picture_codec->key_frame = 1;
  }
  else
  {
    picture_codec->key_frame = 0;
  }


  if(ffh->output_context)
  {
    if(gap_debug)
    {
      printf("p_ffmpeg_encodeAndWriteVideoFrame: TID:%d before avcodec_encode_video  picture_codec:%d\n"
         , p_base_get_thread_id_as_int()
         ,(int)picture_codec
         );
    }


    /* some codecs (x264) just pass through pts information obtained from
     * the AVFrame struct (picture_codec)
     * therefore init the pts code in this structure.
     * (a valid pts is essential to get encoded frame results in the correct order)
     */
    picture_codec->pts = encode_frame_nr -1;

    encoded_size = avcodec_encode_video(ffh->vst[ii].vid_codec_context
                           ,ffh->vst[ii].video_buffer, ffh->vst[ii].video_buffer_size
                           ,picture_codec);


    if(gap_debug)
    {
      printf("p_ffmpeg_encodeAndWriteVideoFrame: TID:%d after avcodec_encode_video  encoded_size:%d\n"
         , p_base_get_thread_id_as_int()
         ,(int)encoded_size
         );
    }

    /* if zero size, it means the image was buffered */
    if(encoded_size != 0)
    {
      {
        AVPacket pkt;
        AVCodecContext *c;

        av_init_packet (&pkt);
        c = ffh->vst[ii].vid_codec_context;


        ///// pkt.pts = ffh->vst[ii].vid_codec_context->coded_frame->pts; // OLD

        if (c->coded_frame->pts != AV_NOPTS_VALUE)
        {
          pkt.pts= av_rescale_q(c->coded_frame->pts, c->time_base, ffh->vst[ii].vid_stream->time_base);
        }


        if(c->coded_frame->key_frame)
        {
          pkt.flags |= AV_PKT_FLAG_KEY;
        }

        pkt.stream_index = ffh->vst[ii].video_stream_index;
        pkt.data = ffh->vst[ii].video_buffer;
        pkt.size = encoded_size;

        if(gap_debug)
        {
          AVStream *st;

          st = ffh->output_context->streams[pkt.stream_index];
          if (pkt.pts == AV_NOPTS_VALUE)
          {
            printf("p_ffmpeg_encodeAndWriteVideoFrame: TID:%d ** HOF: Codec delivered invalid pts  AV_NOPTS_VALUE !\n"
                  , p_base_get_thread_id_as_int()
                  );
          }

          printf("p_ffmpeg_encodeAndWriteVideoFrame: TID:%d  before av_interleaved_write_frame video encoded_size:%d\n"
                " pkt.stream_index:%d pkt.pts:%lld dts:%lld coded_frame->pts:%lld  c->time_base:%d den:%d\n"
                " st->pts.num:%lld, st->pts.den:%lld st->pts.val:%lld\n"
             , p_base_get_thread_id_as_int()
             , (int)encoded_size
             , pkt.stream_index
             , pkt.pts
             , pkt.dts
             , c->coded_frame->pts
             , c->time_base.num
             , c->time_base.den
             ,st->pts.num
             ,st->pts.den
             ,st->pts.val
             );

        }

        //ret = av_write_frame(ffh->output_context, &pkt);
        ret = av_interleaved_write_frame(ffh->output_context, &pkt);

        ffh->countVideoFramesWritten++;
        
        if(gap_debug)
        {
          printf("p_ffmpeg_encodeAndWriteVideoFrame: TID:%d after av_interleaved_write_frame  encoded_size:%d\n"
            , p_base_get_thread_id_as_int()
            , (int)encoded_size 
            );
        }
      }
    }
  }


  /* if we are in pass1 of a two pass encoding run, output log */
  if (ffh->vst[ii].passlog_fp && ffh->vst[ii].vid_codec_context->stats_out)
  {
    fprintf(ffh->vst[ii].passlog_fp, "%s", ffh->vst[ii].vid_codec_context->stats_out);
  }

  return(ret);
  
}  /* end p_ffmpeg_encodeAndWriteVideoFrame */

/* -----------------------------------------------
 * p_ffmpeg_convert_GapStoryFetchResult_to_AVFrame
 * -----------------------------------------------
 * convert the specified gapStoryFetchResult into the specified AVFrame (picture_codec)
 * the encoded frame to the mediafile as packet.
 *
 */
static void
p_ffmpeg_convert_GapStoryFetchResult_to_AVFrame(t_ffmpeg_handle *ffh
 , AVFrame *picture_codec
 , GapStoryFetchResult *gapStoryFetchResult
 , gint vid_track)
{
  int ii;
  GapRgbPixelBuffer  rgbBufferLocal;
  GapRgbPixelBuffer *rgbBuffer;
  guchar            *frameData;

  ii = ffh->vst[vid_track].video_stream_index;

  rgbBuffer = &rgbBufferLocal;
  frameData = NULL;
 
  if(gapStoryFetchResult->resultEnum == GAP_STORY_FETCH_RESULT_IS_RAW_RGB888)
  {
    if(gap_debug)
    {
      printf("p_ffmpeg_convert_GapStoryFetchResult_to_AVFrame: RESULT_IS_RAW_RGB888\n");
    }
    if(gapStoryFetchResult->raw_rgb_data == NULL)
    {
      printf("** ERROR p_ffmpeg_convert_GapStoryFetchResult_to_AVFrame  RGB88 raw_rgb_data is NULL!\n");
      return;
    }
    rgbBuffer->data = gapStoryFetchResult->raw_rgb_data;
  }
  else
  {
    GimpDrawable      *drawable;
    
    drawable = gimp_drawable_get (gapStoryFetchResult->layer_id);
    if(drawable->bpp != 3)
    {
      printf("** ERROR drawable bpp value %d is not supported (only bpp == 3 is supporeted!\n"
        ,(int)drawable->bpp
        );
    }
    gap_gve_init_GapRgbPixelBuffer(rgbBuffer, drawable->width, drawable->height);
 
    frameData = (guchar *)g_malloc0((rgbBuffer->height * rgbBuffer->rowstride));
    rgbBuffer->data = frameData;

    if(gap_debug)
    {
      printf("p_ffmpeg_convert_GapStoryFetchResult_to_AVFrame: before gap_gve_drawable_to_RgbBuffer rgb_buffer\n");
    }
 
    /* tests with framesize 720 x 480 on my 4 CPU development machine showed that
     *    gap_gve_drawable_to_RgbBuffer_multithread runs 1.5 times
     *    slower than the singleprocessor implementation  (1.25 times slower on larger frames 1440 x 960)
     * possible reasons may be
     * a) too much overhead to init multithread stuff
     * b) too much time spent waiting for unlocking the mutex.
     * TODO in case a ==> remove code for gap_gve_drawable_to_RgbBuffer_multithread
     *      in case b ==> further tuning to reduce wait cycles.
     */
    // gap_gve_drawable_to_RgbBuffer_multithread(drawable, rgbBuffer);
    gap_gve_drawable_to_RgbBuffer(drawable, rgbBuffer);
    gimp_drawable_detach (drawable);
 
    /* destroy the fetched (tmp) image */
    gimp_image_delete(gapStoryFetchResult->image_id);
  }

  if (ffh->vst[ii].vid_codec_context->pix_fmt == PIX_FMT_RGB24)
  {
    if(gap_debug)
    {
      printf("p_ffmpeg_convert_GapStoryFetchResult_to_AVFrame: USE PIX_FMT_RGB24 (no pix_fmt convert needed)\n");
    }
    avpicture_fill(picture_codec
                ,rgbBuffer->data
                ,PIX_FMT_RGB24          /* PIX_FMT_RGB24, PIX_FMT_BGR24, PIX_FMT_RGBA32, PIX_FMT_BGRA32 */
                ,ffh->frame_width
                ,ffh->frame_height
                );
  }
  else
  {
    if(gap_debug)
    {
      printf("p_ffmpeg_convert_GapStoryFetchResult_to_AVFrame: before p_convert_colormodel rgb_buffer\n");
    }
    p_convert_colormodel(ffh, picture_codec, rgbBuffer->data, vid_track);
  }


  if(frameData != NULL)
  {
    if(gap_debug)
    {
      printf("p_ffmpeg_convert_GapStoryFetchResult_to_AVFrame: before g_free frameData\n");
    }
    g_free(frameData);
  }

}  /* end p_ffmpeg_convert_GapStoryFetchResult_to_AVFrame */


/* -------------------------------------------
 * p_create_EncoderQueueRingbuffer
 * -------------------------------------------
 * this procedure allocates the encoder queue ringbuffer
 * for use in multiprocessor environment.
 * The Encoder queue ringbuffer is created 
 * with all AVFrame buffers for N elements allocated
 * and all elements have the initial status EQELEM_STATUS_FREE
 *
 * Initial                                                 After writing the 1st frame (main thread)
 *                                                       
 *                          +---------------------+		                          +------------------------+
 *  eque->eq_write_ptr  --->| EQELEM_STATUS_FREE  |<---+                                  | EQELEM_STATUS_FREE     |<---+  
 *  (main thread)           +---------------------+    |	                          +------------------------+    |
 *                                  |                  |	                                  |                     |
 *                                  V                  |	                                  V                     |
 *                          +---------------------+    |	  eque->eq_write_ptr  --->+------------------------+    |
 *  eque->eq_read_ptr   --->| EQELEM_STATUS_FREE  |    | 	  eque->eq_read_ptr   --->| EQELEM_STATUS_READY ** |    | 
 *  (encoder thread)        +---------------------+    |	                          +------------------------+    |
 *                                  |                  |	                                  |                     |
 *                                  V                  |	                                  V                     |
 *                          +---------------------+    |	                          +------------------------+    |
 *                          | EQELEM_STATUS_FREE  |    | 	                          | EQELEM_STATUS_FREE     |    | 
 *                          +---------------------+    |	                          +------------------------+    |
 *                                  |                  |	                                  |                     |
 *                                  V                  |	                                  V                     |
 *                          +---------------------+    |	                          +------------------------+    |
 *                          | EQELEM_STATUS_FREE  |    | 	                          | EQELEM_STATUS_FREE     |    | 
 *                          +---------------------+    |	                          +------------------------+    |
 *                                  |                  |	                                  |                     |
 *                                  +------------------+	                                  +---------------------+
 *
 */
static void
p_create_EncoderQueueRingbuffer(EncoderQueue *eque)
{
  gint ii;
  gint jj;
  EncoderQueueElem *eq_elem_one;
  EncoderQueueElem *eq_elem;
  
  eq_elem_one = NULL;
  eq_elem = NULL;
  for(jj=0; jj < ENCODER_QUEUE_RINGBUFFER_SIZE; jj++)
  {
    eq_elem = g_new(EncoderQueueElem, 1);
    eq_elem->encode_frame_nr = 0;
    eq_elem->vid_track = 1;
    eq_elem->force_keyframe = FALSE;
    eq_elem->status = EQELEM_STATUS_FREE;
    eq_elem->elemMutex = g_mutex_new();
    eq_elem->next = eque->eq_root;
    
    if(eq_elem_one == NULL)
    {
      eq_elem_one = eq_elem;
    }
    eque->eq_root = eq_elem;
    
    for (ii=0; ii < MAX_VIDEO_STREAMS; ii++)
    {
      eq_elem->eq_big_picture_codec[ii] = avcodec_alloc_frame();
    }
  }
  
  /* close the pointer ring */
  if(eq_elem_one)
  {
    eq_elem_one->next = eq_elem;
  }
  
  /* init read and write pointers
   * note that write access typically start with advancing the write pointer
   * followed by write data to eq_big_picture_codec.
   */
  eque->eq_write_ptr = eque->eq_root;
  eque->eq_read_ptr  = eque->eq_root->next;

}  /* end p_create_EncoderQueueRingbuffer */


/* -------------------------------------------
 * p_init_EncoderQueueResources
 * -------------------------------------------
 * this procedure creates a thread pool and an EncoderQueue ringbuffer in case 
 * the gimprc parameters are configured for multiprocessor support.
 * (otherwise those resources are not allocated and were initalized with NULL pointers)
 * 
 */
static EncoderQueue *
p_init_EncoderQueueResources(t_ffmpeg_handle *ffh, t_awk_array *awp)
{
  EncoderQueue     *eque;
  
  eque = g_new(EncoderQueue ,1);
  eque->numberOfElements = 0;
  eque->eq_root      = NULL;
  eque->eq_write_ptr = NULL;
  eque->eq_read_ptr  = NULL;
  eque->ffh          = ffh;
  eque->awp          = awp;
  eque->runningThreadsCounter = 0;
  eque->frameEncodedCond   = NULL;
  eque->poolMutex          = NULL;
  eque->frameEncodedCond   = NULL;

  GAP_TIMM_INIT_RECORD(&eque->mainElemMutexWaits);
  GAP_TIMM_INIT_RECORD(&eque->mainPoolMutexWaits);
  GAP_TIMM_INIT_RECORD(&eque->mainEnqueueWaits);
  GAP_TIMM_INIT_RECORD(&eque->mainPush2);
  GAP_TIMM_INIT_RECORD(&eque->mainWriteFrame);
  GAP_TIMM_INIT_RECORD(&eque->mainDrawableToRgb);
  GAP_TIMM_INIT_RECORD(&eque->mainReadFrame);

  GAP_TIMM_INIT_RECORD(&eque->ethreadElemMutexWaits);
  GAP_TIMM_INIT_RECORD(&eque->ethreadPoolMutexWaits);
  GAP_TIMM_INIT_RECORD(&eque->ethreadEncodeFrame);
  
  if (ffh->isMultithreadEnabled)
  {
    /* check and init thread system */
    ffh->isMultithreadEnabled = gap_base_thread_init();
    if(gap_debug)
    {
      printf("p_init_EncoderQueueResources: isMultithreadEnabled: %d\n"
        ,(int)ffh->isMultithreadEnabled
        );
    }
  }
  
  if (ffh->isMultithreadEnabled)
  {
    p_create_EncoderQueueRingbuffer(eque);
    eque->poolMutex          = g_mutex_new ();
    eque->frameEncodedCond   = g_cond_new ();
  }

  return (eque);
  
}  /* end p_init_EncoderQueueResources */


/* ------------------------------
 * p_debug_print_RingbufferStatus
 * ------------------------------
 * print status of all encoder queue ringbuffer elements
 * to stdout for debug purpose.
 * (all log lines are printed to a buffer and printed to stdout
 * at once to prevent mix up stdout with output from other threads)
 */
static void
p_debug_print_RingbufferStatus(EncoderQueue *eque)
{
  EncoderQueueElem    *eq_elem;
  EncoderQueueElem    *eq_elem_next;
  GString             *logString;
  gint tid;
  gint ii;
      
  tid = p_base_get_thread_id_as_int();
  
  logString = g_string_new(
    "--------------------------------\n"
    "p_debug_print_RingbufferStatus\n"
    "--------------------------------\n"
  );

  eq_elem_next = NULL;
  ii=0;
  for(eq_elem = eque->eq_root; eq_elem != NULL; eq_elem = eq_elem_next)
  {
    g_string_append_printf(logString, "TID:%d EQ_ELEM[%d]: %d  EQ_ELEM->next:%d STATUS:%d encode_frame_nr:%d"
          , (int)tid
          , (int)ii
          , (int)eq_elem
          , (int)eq_elem->next
          , (int)eq_elem->status
          , (int)eq_elem->encode_frame_nr
          );
    if(eq_elem == eque->eq_write_ptr)
    {
      g_string_append_printf(logString, " <-- eq_write_ptr");
    }
    
    if(eq_elem == eque->eq_read_ptr)
    {
      g_string_append_printf(logString, " <-- eq_read_ptr");
    }
    
    g_string_append_printf(logString, "\n");
    
    
    eq_elem_next = eq_elem->next;
    if(eq_elem_next == eque->eq_root)
    {
      /* this was the last element in the ringbuffer.
       */
      eq_elem_next = NULL;
    }
    ii++;
  }

  printf("%s\n", logString->str);
  
  g_string_free(logString, TRUE);

}  /* end p_debug_print_RingbufferStatus */

/* -----------------------------------------
 * p_waitUntilEncoderQueIsProcessed
 * -----------------------------------------
 * check if encoder thread is still running
 * if yes then wait until  finished (e.q. until
 * all enqued frames have been processed)
 * 
 */
static void
p_waitUntilEncoderQueIsProcessed(EncoderQueue *eque)
{
  gint retryCount;

  if(eque == NULL)
  {
    return;
  }

  if(gap_debug)
  {
    printf("p_waitUntilEncoderQueIsProcessed: MainTID:%d\n"
            ,p_base_get_thread_id_as_int()
            );
    p_debug_print_RingbufferStatus(eque);
  }

  retryCount = 0;
  g_mutex_lock (eque->poolMutex);
  while(eque->runningThreadsCounter > 0)
  {
    if(gap_debug)
    {
      printf("p_waitUntilEncoderQueIsProcessed: WAIT MainTID:%d until encoder thread finishes queue processing. eq_write_ptr:%d STATUS:%d retry:%d \n"
        , p_base_get_thread_id_as_int()
        , (int)eque->eq_write_ptr
        , (int)eque->eq_write_ptr->status
        , (int)retryCount
        );
      fflush(stdout);
    }

    g_cond_wait (eque->frameEncodedCond, eque->poolMutex);

    if(gap_debug)
    {
      printf("p_waitUntilEncoderQueIsProcessed: WAKE-UP MainTID:%d after encoder thread finished queue processing. eq_write_ptr:%d STATUS:%d retry:%d \n"
        , p_base_get_thread_id_as_int()
        , (int)eque->eq_write_ptr
        , (int)eque->eq_write_ptr->status
        , (int)retryCount
        );
      fflush(stdout);
    }

    retryCount++;
    if(retryCount >= 250)
    {
      printf("** WARNING encoder thread not yet finished after %d wait retries\n"
        ,(int)retryCount
        );
      break;
    }
  }
  g_mutex_unlock (eque->poolMutex);

}  /* end p_waitUntilEncoderQueIsProcessed */


/* -------------------------------------------
 * p_free_EncoderQueueResources
 * -------------------------------------------
 * this procedure frees the resources for the specified EncoderQueue.
 * Note: this does NOT include the ffh reference.
 * 
 */
static void
p_free_EncoderQueueResources(EncoderQueue     *eque)
{
  t_ffmpeg_handle     *ffh;
  EncoderQueueElem    *eq_elem;
  EncoderQueueElem    *eq_elem_next;
  gint                 ii;
  
  if(eque == NULL)
  {
    return;
  }

  if(eque->frameEncodedCond != NULL)
  {
    if(gap_debug)
    {
      printf("p_free_EncoderQueueResources: before g_cond_free(eque->frameEncodedCond)\n");
    }
    g_cond_free(eque->frameEncodedCond);
    eque->frameEncodedCond = NULL;
    if(gap_debug)
    {
      printf("p_free_EncoderQueueResources: after g_cond_free(eque->frameEncodedCond)\n");
    }
  }

  eq_elem_next = NULL;
  for(eq_elem = eque->eq_root; eq_elem != NULL; eq_elem = eq_elem_next)
  {
    for (ii=0; ii < MAX_VIDEO_STREAMS; ii++)
    {
      if(gap_debug)
      {
        printf("p_free_EncoderQueueResources: g_free big_picture[%d] of eq_elem:%d\n"
          ,(int)ii
          ,(int)eq_elem
          );
      }
      g_free(eq_elem->eq_big_picture_codec[ii]);
      if(gap_debug)
      {
        printf("p_free_EncoderQueueResources: g_mutex_free of eq_elem:%d\n"
          ,(int)eq_elem
          );
      }
      g_mutex_free(eq_elem->elemMutex);
    }
    if(gap_debug)
    {
      printf("p_free_EncoderQueueResources: g_free eq_elem:%d\n"
        ,(int)eq_elem
        );
    }
    eq_elem_next = eq_elem->next;
    if(eq_elem_next == eque->eq_root)
    {
      /* this was the last element in the ringbuffer that points to
       * the (already free'd root); it is time to break the loop now.
       */
      eq_elem_next = NULL;
    }
    g_free(eq_elem);
  }
  eque->eq_root      = NULL;
  eque->eq_write_ptr = NULL;
  eque->eq_read_ptr  = NULL;
  
}  /* end p_free_EncoderQueueResources */


/* -------------------------------------
 * p_fillQueueElem
 * -------------------------------------
 * fill element eque->eq_write_ptr with imag data and information
 * that is required for the encoder.
 */
static void
p_fillQueueElem(EncoderQueue *eque, GapStoryFetchResult *gapStoryFetchResult, gboolean force_keyframe, gint vid_track)
{
  EncoderQueueElem *eq_write_ptr;
  AVFrame *picture_codec;
  GimpDrawable *drawable;

  eq_write_ptr = eque->eq_write_ptr;
  eq_write_ptr->encode_frame_nr = eque->ffh->encode_frame_nr;
  eq_write_ptr->vid_track = vid_track;
  eq_write_ptr->force_keyframe = force_keyframe;
  picture_codec = eq_write_ptr->eq_big_picture_codec[vid_track];

  GAP_TIMM_START_RECORD(&eque->mainDrawableToRgb);
  if(gap_debug)
  {
    printf("p_fillQueueElem: START drawable:%d eq_write_ptr:%d picture_codec:%d vid_track:%d encode_frame_nr:%d\n"
      ,(int)drawable
      ,(int)eq_write_ptr
      ,(int)picture_codec
      ,(int)eq_write_ptr->vid_track
      ,(int)eq_write_ptr->encode_frame_nr
      );
  }
  
  /* fill the AVFrame data at eq_write_ptr */
  if(gapStoryFetchResult != NULL)
  {
    p_ffmpeg_convert_GapStoryFetchResult_to_AVFrame(eque->ffh
                 , picture_codec
                 , gapStoryFetchResult
                 , vid_track
                 );
    if(gap_debug)
    {
      printf("p_fillQueueElem: DONE drawable:%d drawableId:%d eq_write_ptr:%d picture_codec:%d vid_track:%d encode_frame_nr:%d\n"
        ,(int)drawable
        ,(int)drawable->drawable_id
        ,(int)eq_write_ptr
        ,(int)picture_codec
        ,(int)eq_write_ptr->vid_track
        ,(int)eq_write_ptr->encode_frame_nr
        );
    }
  }
  GAP_TIMM_STOP_RECORD(&eque->mainDrawableToRgb);
  
}  /* end p_fillQueueElem */


/* -------------------------------------
 * p_encodeCurrentQueueElem
 * -------------------------------------
 * Encode current queue element at eq_read_ptr
 * and write encoded videoframe to the mediafile.
 */
static void
p_encodeCurrentQueueElem(EncoderQueue *eque)
{
  EncoderQueueElem *eq_read_ptr;
  AVFrame *picture_codec;
  gint     vid_track;
  
  eq_read_ptr = eque->eq_read_ptr;
  
  vid_track = eq_read_ptr->vid_track;
  picture_codec = eq_read_ptr->eq_big_picture_codec[vid_track];

  if(gap_debug)
  {
    printf("p_encodeCurrentQueueElem: TID:%d START eq_read_ptr:%d STATUS:%d ffh:%d picture_codec:%d vid_track:%d encode_frame_nr:%d\n"
      , p_base_get_thread_id_as_int()
      ,(int)eq_read_ptr
      ,(int)eq_read_ptr->status
      ,(int)eque->ffh
      ,(int)picture_codec
      ,(int)eq_read_ptr->vid_track
      ,(int)eq_read_ptr->encode_frame_nr
      );
  }
  
  p_ffmpeg_encodeAndWriteVideoFrame(eque->ffh
                                  , picture_codec
                                  , eque->eq_read_ptr->force_keyframe
                                  , vid_track
                                  , eque->eq_read_ptr->encode_frame_nr
                                  );

  if(gap_debug)
  {
    printf("p_encodeCurrentQueueElem: TID:%d DONE eq_read_ptr:%d picture_codec:%d vid_track:%d encode_frame_nr:%d\n"
      , p_base_get_thread_id_as_int()
      ,(int)eq_read_ptr
      ,(int)picture_codec
      ,(int)eq_read_ptr->vid_track
      ,(int)eq_read_ptr->encode_frame_nr
      );
  }
}  /* end p_encodeCurrentQueueElem */


/* -------------------------------------------
 * p_encoderWorkerThreadFunction
 * -------------------------------------------
 * this procedure runs as thread pool function to encode video and audio
 * frames, Encoding is based on libavformat/libavcodec.
 * videoframe input is taken from the EncoderQueue ringbuffer
 *  (that is filled parallel by the main thread)
 * audioframe input is directly fetched from an input audifile.
 *
 * After encoding the first available frame this thread tries
 * to encode following frames when available.
 * In case the elemMutex can not be locked for those additional frames
 * it gives up immediate to avoid deadlocks. (such frames are handled in the next call
 * when the thread is restarted from the thread pool)
 * 
 *
 * The encoding is done with the selected codec, the compressed data is written
 * to the mediafile as packet.
 *
 * Note: the read pointer is reserved for exclusive use in this thread
 * therefore advance of this pointer can be done without locks.
 * but accessing the element data (status or buffer) requires locking at element level
 * because the main thread does acces the same data via the write pointer.
 *
 * 
 */
static void
p_encoderWorkerThreadFunction (EncoderQueue *eque)
{
  EncoderQueueElem     *nextElem;
  gint32                encoded_frame_nr;
  if(gap_debug)
  {
    printf("p_encoderWorkerThreadFunction: TID:%d before elemMutex eq_read_ptr:%d\n"
          , p_base_get_thread_id_as_int()
          , (int)eque->eq_read_ptr
          );
  }
  /* lock at element level */
  if(g_mutex_trylock (eque->eq_read_ptr->elemMutex) != TRUE)
  {
    GAP_TIMM_START_RECORD(&eque->ethreadElemMutexWaits);

    g_mutex_lock (eque->eq_read_ptr->elemMutex);
  
    GAP_TIMM_STOP_RECORD(&eque->ethreadElemMutexWaits);
  }
  
  
  
  if(gap_debug)
  {
    printf("p_encoderWorkerThreadFunction: TID:%d  after elemMutex lock eq_read_ptr:%d STATUS:%d encode_frame_nr:%d\n"
          , p_base_get_thread_id_as_int()
          , (int)eque->eq_read_ptr
          , (int)eque->eq_read_ptr->status
          , (int)eque->eq_read_ptr->encode_frame_nr
          );
    p_debug_print_RingbufferStatus(eque);
  }

ENCODER_LOOP:

  /* check next element is ready and advance read pointer if true.
   *
   * encoding of the last n-frames is typically triggerd
   * by setting the same frame n times to EQELEM_STATUS_READY
   * repeated without advance to the next element.
   * therefore the read pointer advance is done only in case
   * when the next element with EQELEM_STATUS_READY is already available.
   */
  if(eque->eq_read_ptr->status == EQELEM_STATUS_FREE)
  {
    nextElem = eque->eq_read_ptr->next;
    g_mutex_unlock (eque->eq_read_ptr->elemMutex);

    if(TRUE != g_mutex_trylock (nextElem->elemMutex))
    {
        goto ENCODER_STOP;
    }
    else
    {
      if(nextElem->status == EQELEM_STATUS_READY)
      {
        eque->eq_read_ptr = nextElem;
      }
      else
      {
        /* next element is not ready, 
         */
        g_mutex_unlock (nextElem->elemMutex);
        goto ENCODER_STOP;
      }
    }
  }

  if(eque->eq_read_ptr->status == EQELEM_STATUS_READY)
  {
    eque->eq_read_ptr->status = EQELEM_STATUS_LOCK;


    GAP_TIMM_START_RECORD(&eque->ethreadEncodeFrame);

    /* Encode and write the current element (e.g. videoframe) at eq_read_ptr */
    p_encodeCurrentQueueElem(eque);

    if(eque->ffh->countVideoFramesWritten > 0)
    {
      /* fetch, encode and write one audioframe */
      p_process_audio_frame(eque->ffh, eque->awp);
    }

    GAP_TIMM_STOP_RECORD(&eque->ethreadEncodeFrame);


    /* setting EQELEM_STATUS_FREE enables re-use (e.g. overwrite)
     * of this element's data buffers.
     */
    eque->eq_read_ptr->status = EQELEM_STATUS_FREE;
    encoded_frame_nr = eque->eq_read_ptr->encode_frame_nr;
    
    /* encoding of the last n-frames is typically triggerd
     * by setting the same frame n times to EQELEM_STATUS_READY
     * repeated without advance to the next element.
     * therefore the read pointer advance is done only in case
     * when the next element with EQELEM_STATUS_READY is already available.
     */
    nextElem = eque->eq_read_ptr->next;

    /* advance the lock to next element */
    g_mutex_unlock (eque->eq_read_ptr->elemMutex);

    if(TRUE != g_mutex_trylock (nextElem->elemMutex))
    {
        goto ENCODER_STOP;
    }
    else
    {
      if (nextElem->status == EQELEM_STATUS_READY)
      {
        eque->eq_read_ptr = nextElem;

        if(TRUE == g_mutex_trylock (eque->poolMutex))
        {
          g_cond_signal  (eque->frameEncodedCond);
          g_mutex_unlock (eque->poolMutex);
        }
        /* next frame already available, contine the encoder loop */ 
        goto ENCODER_LOOP;
      }
      else
      {
        /* unlock because next element is not ready, 
         */
        g_mutex_unlock (nextElem->elemMutex);
      }
    }

  }
  else
  {
    g_mutex_unlock (eque->eq_read_ptr->elemMutex);
  }
  

  /* no element in ready status available.
   * This can occure in followinc scenarios:
   * a) all frames are encoded
   *    in this case the main thread will free up resources and exit
   *    or
   * b) encoding was faster than fetching/rendering (in the main thread)
   *    in this case the main thread will reactivate this thread pool function
   *    when the next frame is enqued and reached the ready status.
   *
   * send signal to wake up main thread (even if nothing was actually encoded)
   */

ENCODER_STOP:

  /* lock at pool level */
  if(g_mutex_trylock (eque->poolMutex) != TRUE)
  {
    GAP_TIMM_START_RECORD(&eque->ethreadPoolMutexWaits);
    g_mutex_lock (eque->poolMutex);
    GAP_TIMM_STOP_RECORD(&eque->ethreadPoolMutexWaits);
  }

  if(gap_debug)
  {
    printf("p_encoderWorkerThreadFunction: TID:%d  send frameEncodedCond encoded_frame_nr:%d\n"
          , p_base_get_thread_id_as_int()
          , encoded_frame_nr
          );
  }
  eque->runningThreadsCounter--;
  g_cond_signal  (eque->frameEncodedCond);
  g_mutex_unlock (eque->poolMutex);

  if(gap_debug)
  {
    printf("p_encoderWorkerThreadFunction: TID:%d  DONE eq_read_ptr:%d encoded_frame_nr:%d\n"
          , p_base_get_thread_id_as_int()
          , (int)eque->eq_read_ptr
          , encoded_frame_nr
          );
  }

}  /* end p_encoderWorkerThreadFunction */


/* ------------------------------------------
 * p_ffmpeg_write_frame_and_audio_multithread
 * ------------------------------------------
 * trigger encoding one videoframe and one audioframe (in case audio is uesd)
 * The videoframe is enqueued and processed in parallel by the encoder worker thread.
 * Passing NULL as gapStoryFetchResult is used to flush one frame from the codecs internal buffer
 * (typically required after the last frame has been already feed to the codec)
 */
static int
p_ffmpeg_write_frame_and_audio_multithread(EncoderQueue *eque
   , GapStoryFetchResult *gapStoryFetchResult, gboolean force_keyframe, gint vid_track)
{
  GError *error = NULL;
  gint retryCount;

  GAP_TIMM_START_RECORD(&eque->mainWriteFrame);

  retryCount = 0;

  if(encoderThreadPool == NULL)
  {
    /* init thread pool for one encoder thread.
     * The current implementation does not support 2 or more concurrent queue writer threads
     * -- more threads would crash or produce unusable video --
     * But independent form this setup some of the codecs in libavcodec do support
     * multiple threads that are configuread with the treads encoder parameter.
     */
    encoderThreadPool = g_thread_pool_new((GFunc) p_encoderWorkerThreadFunction
                                         ,NULL        /* user data */
                                         ,1           /* max_threads */
                                         ,TRUE        /* exclusive */
                                         ,&error      /* GError **error */
                                         );
  }

  if(gapStoryFetchResult != NULL)
  {
    /* a new frame is availble as gimp drawable
     * and shall be enqueued at next position in the 
     * EncoderQueue ringbuffer. Therefore first advance
     * write position. 
     */
    eque->eq_write_ptr = eque->eq_write_ptr->next;
  }

  if(gap_debug)
  {
    printf("p_ffmpeg_write_frame_and_audio_multithread: before elemMutex lock eq_write_ptr:%d STATUS:%d retry:%d encode_frame_nr:%d\n"
          , (int)eque->eq_write_ptr
          , (int)eque->eq_write_ptr->status
          , (int)retryCount
          , (int)eque->ffh->encode_frame_nr
          );
    fflush(stdout);
  }
  
  /* lock at element level (until element is filled and has reached EQELEM_STATUS_READY) */
  if(g_mutex_trylock (eque->eq_write_ptr->elemMutex) != TRUE)
  {
    GAP_TIMM_START_RECORD(&eque->mainElemMutexWaits);

    g_mutex_lock (eque->eq_write_ptr->elemMutex);

    GAP_TIMM_STOP_RECORD(&eque->mainElemMutexWaits);
  }
  
  if(gap_debug)
  {
    printf("p_ffmpeg_write_frame_and_audio_multithread: after elemMutex lock eq_write_ptr:%d STATUS:%d retry:%d encode_frame_nr:%d\n"
          , (int)eque->eq_write_ptr
          , (int)eque->eq_write_ptr->status
          , (int)retryCount
          , (int)eque->ffh->encode_frame_nr
          );
  }
  
  while(eque->eq_write_ptr->status != EQELEM_STATUS_FREE)
  {
    GAP_TIMM_START_RECORD(&eque->mainEnqueueWaits);
    
    g_mutex_unlock (eque->eq_write_ptr->elemMutex);

    /* lock at pool level for checking numer of running encoder threads (0 or 1 expected) */
    if(g_mutex_trylock (eque->poolMutex) != TRUE)
    {
      GAP_TIMM_START_RECORD(&eque->mainPoolMutexWaits);
      g_mutex_lock (eque->poolMutex);
      GAP_TIMM_STOP_RECORD(&eque->mainPoolMutexWaits);
    }

    if(eque->runningThreadsCounter <= 0)
    {
      if(gap_debug)
      {
        printf("p_ffmpeg_write_frame_and_audio_multithread: PUSH-1 (re)start worker thread eq_write_ptr:%d STATUS:%d retry:%d encode_frame_nr:%d\n"
          , (int)eque->eq_write_ptr
          , (int)eque->eq_write_ptr->status
          , (int)retryCount
          , (int)eque->ffh->encode_frame_nr
          );
      }
      /* (re)activate encoder thread */
      eque->runningThreadsCounter++;
      g_thread_pool_push (encoderThreadPool
                         , eque    /* VideoPrefetchData */
                         , &error
                         );
    }

    /* ringbuffer queue is currently full, 
     * have to wait until encoder thread finished processing for at least one frame
     * and sets the status to EQELEM_STATUS_FREE.
     * g_cond_wait Waits until this thread is woken up on frameEncodedCond. 
     * The mutex is unlocked before falling asleep and locked again before resuming. 
     */
    if(gap_debug)
    {
      printf("p_ffmpeg_write_frame_and_audio_multithread: WAIT MainTID:%d retry:%d encode_frame_nr:%d\n"
        ,  p_base_get_thread_id_as_int()
        , (int)retryCount
        , (int)eque->ffh->encode_frame_nr
        );
    }
    g_cond_wait (eque->frameEncodedCond, eque->poolMutex);
    if(gap_debug)
    {
      printf("p_ffmpeg_write_frame_and_audio_multithread: WAKE-UP MainTID:%d retry:%d encode_frame_nr:%d\n"
        ,  p_base_get_thread_id_as_int()
        , (int)retryCount
        , (int)eque->ffh->encode_frame_nr
        );
    }
    g_mutex_unlock (eque->poolMutex);

    retryCount++;
    if(retryCount > 100)
    {
      printf("** INTERNAL ERROR: failed to enqueue frame data after %d reties!\n"
         ,(int)retryCount
         );
      exit (1);
    }

    /* lock at element level (until element is filled and has reached EQELEM_STATUS_READY) */
    g_mutex_lock (eque->eq_write_ptr->elemMutex);

    GAP_TIMM_STOP_RECORD(&eque->mainEnqueueWaits);

  }


  if(gap_debug)
  {
    printf("p_ffmpeg_write_frame_and_audio_multithread: FILL-QUEUE eq_write_ptr:%d STATUS:%d retry:%d encode_frame_nr:%d\n"
      , (int)eque->eq_write_ptr
      , (int)eque->eq_write_ptr->status
      , (int)retryCount
      , (int)eque->ffh->encode_frame_nr
      );
  }

  /* convert gapStoryFetchResult and put resulting data into element eque->eq_write_ptr */
  p_fillQueueElem(eque, gapStoryFetchResult, force_keyframe, vid_track);

  eque->eq_write_ptr->status = EQELEM_STATUS_READY;

  g_mutex_unlock (eque->eq_write_ptr->elemMutex);
  

  /* try lock at pool level to check if encoder thread is already running
   * and (re)start if this is not the case.
   * in case g_mutex_trylock returns FALSE, the mutex is already locked
   * and it can be assumed that the encoder Thread is the lock holder and already running
   */
  if(TRUE == g_mutex_trylock (eque->poolMutex))
  {
    if(eque->runningThreadsCounter <= 0)
    {
      GAP_TIMM_START_RECORD(&eque->mainPush2);
      
      if(gap_debug)
      {
        printf("p_ffmpeg_write_frame_and_audio_multithread: PUSH-2 (re)start worker thread eq_write_ptr:%d STATUS:%d retry:%d encode_frame_nr:%d\n"
          , (int)eque->eq_write_ptr
          , (int)eque->eq_write_ptr->status
          , (int)retryCount
          , (int)eque->ffh->encode_frame_nr
          );
      }

      /* (re)activate encoder thread */
      eque->runningThreadsCounter++;
      g_thread_pool_push (encoderThreadPool
                         , eque    /* VideoPrefetchData */
                         , &error
                         );

      GAP_TIMM_STOP_RECORD(&eque->mainPush2);
    }

    g_mutex_unlock (eque->poolMutex);

  }

  GAP_TIMM_STOP_RECORD(&eque->mainWriteFrame);

}  /* end p_ffmpeg_write_frame_and_audio_multithread */




/* --------------------
 * p_ffmpeg_write_frame  SINGLEPROCESSOR
 * --------------------
 * encode one videoframe using the selected codec and write
 * the encoded frame to the mediafile as packet.
 * Passing NULL as gapStoryFetchResult is used to flush one frame from the codecs internal buffer
 * (typically required after the last frame has been already feed to the codec)
 */
static int
p_ffmpeg_write_frame(t_ffmpeg_handle *ffh, GapStoryFetchResult *gapStoryFetchResult
  , gboolean force_keyframe, gint vid_track)
{
  AVFrame *picture_codec;
  int encoded_size;
  int ret;
  int ii;

  ii = ffh->vst[vid_track].video_stream_index;
  ret = 0;

  if(gap_debug)
  {
     AVCodec  *codec;

     codec = ffh->vst[ii].vid_codec_context->codec;

     printf("\n-------------------------\n");
     printf("p_ffmpeg_write_frame: START codec: %d track:%d countVideoFramesWritten:%d frame_nr:%d (validFrameNr:%d)\n"
           , (int)codec
           , (int)vid_track
           , (int)ffh->countVideoFramesWritten
           , (int)ffh->encode_frame_nr
           , (int)ffh->validEncodeFrameNr
           );
     printf("name: %s\n", codec->name);
     if(gap_debug)
     {
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

  /* picture to feed the codec */
  picture_codec = ffh->vst[ii].big_picture_codec;

  /* in case gapStoryFetchResult is NULL
   * we feed the previous handled picture (e.g the last of the input)
   * again and again to the codec
   * Note that this procedure typically is called with NULL  drawbale
   * until all frames in its internal buffer are writen to the output video.
   */

  if(gapStoryFetchResult != NULL)
  {
    p_ffmpeg_convert_GapStoryFetchResult_to_AVFrame(ffh
               , picture_codec
               , gapStoryFetchResult
               , vid_track
               );
  }


  ret = p_ffmpeg_encodeAndWriteVideoFrame(ffh, picture_codec, force_keyframe, vid_track, ffh->encode_frame_nr);

  return (ret);

}  /* end p_ffmpeg_write_frame */




/* -------------------------
 * p_ffmpeg_write_audioframe
 * -------------------------
 */
static int
p_ffmpeg_write_audioframe(t_ffmpeg_handle *ffh, guchar *audio_buf, int frame_bytes, gint aud_track)
{
  int encoded_size;
  int ret;
  int ii;

  ret = 0;
  ii = aud_track;

  if(ffh->output_context)
  {
    if(gap_debug)
    {
      printf("p_ffmpeg_write_audioframe: START audo track ii=%d\n", (int)ii);
      printf("ffh->ast[%d].audio_buffer: %d\n", (int)ii, (int)ffh->ast[ii].audio_buffer);
      printf("ffh->ast[%d].audio_buffer_size: %d\n", (int)ii, ffh->ast[ii].audio_buffer_size);
      printf("audio_buf: %d\n", (int)audio_buf);
    }
    encoded_size = avcodec_encode_audio(ffh->ast[ii].aud_codec_context
                           ,ffh->ast[ii].audio_buffer, ffh->ast[ii].audio_buffer_size
                           ,(short *)audio_buf);


    {
      AVPacket pkt;
      AVCodecContext *enc;

      av_init_packet (&pkt);
      enc = ffh->ast[ii].aud_codec_context;


//      pkt.pts = ffh->ast[ii].aud_codec_context->coded_frame->pts;  // OLD

      if(enc->coded_frame && enc->coded_frame->pts != AV_NOPTS_VALUE)
      {
        pkt.pts= av_rescale_q(enc->coded_frame->pts, enc->time_base, ffh->ast[ii].aud_stream->time_base);
      }
      pkt.flags |= AV_PKT_FLAG_KEY;


//       if (pkt.pts == AV_NOPTS_VALUE)
//       {
//         /* WORKAROND calculate pts timecode for the current frame
//          * because the codec did not deliver a valid timecode
//          */
//         pkt.pts = p_calculate_current_timecode(ffh);
//         //pkt.pts = p_calculate_timecode(ffh, ffh->countVideoFramesWritten);
//         pkt.dts = pkt.pts;
//         //if(gap_debug)
//         {
//           printf("WORKAROND calculated audio pts (because codec deliverd AV_NOPTS_VALUE\n");
//         }
//       }


      pkt.stream_index = ffh->ast[ii].audio_stream_index;
      pkt.data = ffh->ast[ii].audio_buffer;
      pkt.size = encoded_size;

      if(gap_debug)
      {
        printf("before av_interleaved_write_frame audio  encoded_size:%d pkt.pts:%lld dts:%lld\n"
          , (int)encoded_size
          , pkt.pts
          , pkt.dts
          );
      }

      //ret = av_write_frame(ffh->output_context, &pkt);
      ret = av_interleaved_write_frame(ffh->output_context, &pkt);  // seems not OK work pts/dts is invalid

      if(gap_debug)
      {
        printf("after av_interleaved_write_frame audio encoded_size:%d\n", (int)encoded_size );
      }
    }
  }
  return(ret);
}  /* end p_ffmpeg_write_audioframe */



/* ---------------
 * my_url_fclose
 * ---------------
 * this procedure is a workaround that fixes a crash that happens on attempt to call the original
 * url_fclose procedure
 * my private copy of (libavformat/aviobuf.c url_fclose)
 * just skips the crashing step "av_free(s);"  that frees up the ByteIOContext itself
 * (free(): invalid pointer: 0x0859f3b0)
 * The workaround is still required in ffmpeg snapshot from 2009.01.31
 */
int
my_url_fclose(ByteIOContext *s)
{
    URLContext *h = s->opaque;

    av_free(s->buffer);

//printf("before av_free s\n");
//    av_free(s);                  // ## crash happens here
//printf("after av_free s\n");

    return url_close(h);
}

/* ---------------
 * p_ffmpeg_close
 * ---------------
 * write multimeda format trailer, close all video and audio codecs
 * and free all related buffers
 */
static void
p_ffmpeg_close(t_ffmpeg_handle *ffh)
{
  gint ii;

  if(gap_debug)
  {
    printf("free big_picture_codec\n");
  }

  for (ii=0; ii < ffh->max_vst; ii++)
  {
    if(ffh->vst[ii].big_picture_codec)
    {
      g_free(ffh->vst[ii].big_picture_codec);
      ffh->vst[ii].big_picture_codec = NULL;
    }
  }
  if(gap_debug)
  {
    printf("Closing VIDEO stuff\n");
  }

  if(ffh->output_context)
  {
    /* write the trailer if needed and close file */
    av_write_trailer(ffh->output_context);
    if(gap_debug)
    {
      printf("after av_write_trailer\n");
    }
  }

  for (ii=0; ii < ffh->max_vst; ii++)
  {
    if(ffh->vst[ii].vid_codec_context)
    {
      if(gap_debug)
      {
        printf("[%d] before avcodec_close\n", ii);
      }
      avcodec_close(ffh->vst[ii].vid_codec_context);
      if(gap_debug)
      {
        printf("[%d] after avcodec_close\n", ii);
      }
      ffh->vst[ii].vid_codec_context = NULL;
      /* do not attempt to free ffh->vst[ii].vid_codec_context (it points to ffh->vst[ii].vid_stream->codec) */
    }

    if (ffh->vst[ii].passlog_fp)
    {
      fclose(ffh->vst[ii].passlog_fp);
      ffh->vst[ii].passlog_fp = NULL;
    }


    if(ffh->vst[ii].video_buffer)
    {
       if(gap_debug)
       {
         printf("[%d] before g_free video_buffer\n", ii);
       }
       g_free(ffh->vst[ii].video_buffer);
       ffh->vst[ii].video_buffer = NULL;
       if(gap_debug)
       {
         printf("[%d] after g_free video_buffer\n", ii);
       }
    }
    if(ffh->vst[ii].video_dummy_buffer)
    {
       if(gap_debug)
       {
         printf("[%d] before g_free video_dummy_buffer\n", ii);
       }
       g_free(ffh->vst[ii].video_dummy_buffer);
       ffh->vst[ii].video_dummy_buffer = NULL;
       if(gap_debug)
       {
         printf("[%d] after g_free video_dummy_buffer\n", ii);
       }
    }
    if(ffh->vst[ii].yuv420_dummy_buffer)
    {
       if(gap_debug)
       {
         printf("[%d] before g_free yuv420_dummy_buffer\n", ii);
       }
       g_free(ffh->vst[ii].yuv420_dummy_buffer);
       ffh->vst[ii].yuv420_dummy_buffer = NULL;
       if(gap_debug)
       {
         printf("[%d] after g_free yuv420_dummy_buffer\n", ii);
       }
    }

  }

  if(gap_debug)
  {
    printf("Closing AUDIO stuff\n");
  }

  for (ii=0; ii < ffh->max_ast; ii++)
  {
    if(ffh->ast[ii].aud_codec_context)
    {
      avcodec_close(ffh->ast[ii].aud_codec_context);
      ffh->ast[ii].aud_codec_context = NULL;
      /* do not attempt to free ffh->ast[ii].aud_codec_context (it points to ffh->ast[ii].aud_stream->codec) */
    }

    if(ffh->ast[ii].audio_buffer)
    {
       g_free(ffh->ast[ii].audio_buffer);
       ffh->ast[ii].audio_buffer = NULL;
    }
  }

  if(ffh->output_context)
  {
    if (!(ffh->output_context->oformat->flags & AVFMT_NOFILE))
    {
      if(gap_debug)
      {
        printf("before url_fclose\n");
      }
      my_url_fclose(&ffh->output_context->pb);

      if(gap_debug)
      {
        printf("after url_fclose\n");
      }
    }
    else
    {
        printf("SKIP url_fclose (AVFMT_NOFILE)\n");
    }
  }

  if(ffh->img_convert_ctx != NULL)
  {
    sws_freeContext(ffh->img_convert_ctx);
    ffh->img_convert_ctx = NULL;
  }
  if(ffh->convert_buffer != NULL)
  {
    g_free(ffh->convert_buffer);
    ffh->convert_buffer = NULL;
  }

}  /* end p_ffmpeg_close */

/* ---------------------------
 * p_add_vcodec_name
 * ---------------------------
 * add the specified name to as 1st element to the specified codec_list
 * optional followed by all compatible video codecs that can be used for lossless video cut feature.
 */
static void
p_add_vcodec_name(GapCodecNameElem **codec_list, const char *name)
{
  if(name)
  {
    GapCodecNameElem *codec_elem;

    codec_elem = g_malloc0(sizeof(GapCodecNameElem));
    if (codec_elem)
    {
      codec_elem->codec_name = g_strdup(name);
      codec_elem->next = *codec_list;
      *codec_list = codec_elem;
    }
  }
}  /* end p_add_vcodec_name */


/* ---------------------------
 * p_setup_check_flags
 * ---------------------------
 * Set up check_flags (conditions for lossless video cut)
 * and return a list of codec_names that shall be compatible to the
 * selected codec (epp->vcodec_name) for encoding.
 * NOTE:
 *  the compatibility list is in experimental state,
 *  (it is not based on format specifications)
 *
 */
static   GapCodecNameElem *
p_setup_check_flags(GapGveFFMpegValues *epp
   , gint32 *check_flags  /* OUT */
   )
{
  gint32 l_check_flags;
  GapCodecNameElem *compatible_vcodec_list;


  l_check_flags = GAP_VID_CHCHK_FLAG_SIZE;

  compatible_vcodec_list = NULL;

  if(strcmp(epp->vcodec_name, "mjpeg") == 0)
  {
    l_check_flags |= (GAP_VID_CHCHK_FLAG_JPG | GAP_VID_CHCHK_FLAG_FULL_FRAME);
    p_add_vcodec_name(&compatible_vcodec_list, "JPEG");
  }
  else if(strcmp(epp->vcodec_name, "png") == 0)
  {
    l_check_flags |= (GAP_VID_CHCHK_FLAG_JPG | GAP_VID_CHCHK_FLAG_FULL_FRAME);
    p_add_vcodec_name(&compatible_vcodec_list, "PNG");
    p_add_vcodec_name(&compatible_vcodec_list, "PNG ");
  }
  else if(strcmp(epp->vcodec_name, "mpeg1video") == 0)
  {
    l_check_flags |= (GAP_VID_CHCHK_FLAG_MPEG_INTEGRITY | GAP_VID_CHCHK_FLAG_VCODEC_NAME);
    p_add_vcodec_name(&compatible_vcodec_list, "mjpeg");
  }
  else if(strcmp(epp->vcodec_name, "mpeg2video") == 0)
  {
    l_check_flags |= (GAP_VID_CHCHK_FLAG_MPEG_INTEGRITY | GAP_VID_CHCHK_FLAG_VCODEC_NAME);
    p_add_vcodec_name(&compatible_vcodec_list, "mpeg1video");
    p_add_vcodec_name(&compatible_vcodec_list, "mpegvideo");
    p_add_vcodec_name(&compatible_vcodec_list, "mjpeg");
    p_add_vcodec_name(&compatible_vcodec_list, "JPEG");
  }
  else if(strcmp(epp->vcodec_name, "mpeg4") == 0)
  {
    l_check_flags |= (GAP_VID_CHCHK_FLAG_MPEG_INTEGRITY | GAP_VID_CHCHK_FLAG_VCODEC_NAME);
    p_add_vcodec_name(&compatible_vcodec_list, "mpeg1video");
    p_add_vcodec_name(&compatible_vcodec_list, "mpeg2video");
    p_add_vcodec_name(&compatible_vcodec_list, "mpegvideo");
    p_add_vcodec_name(&compatible_vcodec_list, "mjpeg");
    p_add_vcodec_name(&compatible_vcodec_list, "JPEG");
  }
  else if(strcmp(epp->vcodec_name, "msmpeg4") == 0)
  {
    l_check_flags |= (GAP_VID_CHCHK_FLAG_MPEG_INTEGRITY | GAP_VID_CHCHK_FLAG_VCODEC_NAME);
    p_add_vcodec_name(&compatible_vcodec_list, "mpeg1video");
    p_add_vcodec_name(&compatible_vcodec_list, "mpeg2video");
    p_add_vcodec_name(&compatible_vcodec_list, "mpegvideo");
    p_add_vcodec_name(&compatible_vcodec_list, "mjpeg");
    p_add_vcodec_name(&compatible_vcodec_list, "JPEG");
  }
  else if(strcmp(epp->vcodec_name, "msmpeg4v1") == 0)
  {
    l_check_flags |= (GAP_VID_CHCHK_FLAG_MPEG_INTEGRITY | GAP_VID_CHCHK_FLAG_VCODEC_NAME);
    p_add_vcodec_name(&compatible_vcodec_list, "mpeg1video");
    p_add_vcodec_name(&compatible_vcodec_list, "mpeg2video");
    p_add_vcodec_name(&compatible_vcodec_list, "mpegvideo");
    p_add_vcodec_name(&compatible_vcodec_list, "msmpeg4");
    p_add_vcodec_name(&compatible_vcodec_list, "mjpeg");
    p_add_vcodec_name(&compatible_vcodec_list, "JPEG");
  }
  else if(strcmp(epp->vcodec_name, "msmpeg4v2") == 0)
  {
    l_check_flags |= (GAP_VID_CHCHK_FLAG_MPEG_INTEGRITY | GAP_VID_CHCHK_FLAG_VCODEC_NAME);
    p_add_vcodec_name(&compatible_vcodec_list, "mpeg1video");
    p_add_vcodec_name(&compatible_vcodec_list, "mpeg2video");
    p_add_vcodec_name(&compatible_vcodec_list, "mpegvideo");
    p_add_vcodec_name(&compatible_vcodec_list, "msmpeg4");
    p_add_vcodec_name(&compatible_vcodec_list, "msmpeg4v1");
    p_add_vcodec_name(&compatible_vcodec_list, "mjpeg");
    p_add_vcodec_name(&compatible_vcodec_list, "JPEG");
  }
  else
  {
    l_check_flags |= (GAP_VID_CHCHK_FLAG_VCODEC_NAME);
  }


  *check_flags = l_check_flags;
  p_add_vcodec_name(&compatible_vcodec_list, epp->vcodec_name);


  return (compatible_vcodec_list);
}  /* end p_setup_check_flags */




/* ---------------------------
 * p_ffmpeg_encode_pass
 * ---------------------------
 *    The main "productive" routine
 *    ffmpeg encoding of anim frames, based on ffmpeg lib (by Fabrice Bellard)
 *    Audio encoding is Optional.
 *    (wav_audiofile must be provided in that case)
 *
 * returns   value >= 0 if ok
 *           (or -1 on error)
 */
static gint
p_ffmpeg_encode_pass(GapGveFFMpegGlobalParams *gpp, gint32 current_pass, GapGveMasterEncoderStatus *encStatusPtr)
{
#define GAP_FFENC_USE_YUV420P "GAP_FFENC_USE_YUV420P"
  GapGveFFMpegValues   *epp = NULL;
  t_ffmpeg_handle      *ffh = NULL;
  EncoderQueue         *eque = NULL;
  GapGveStoryVidHandle        *l_vidhand = NULL;
  long          l_master_frame_nr;
  long          l_step, l_begin, l_end;
  int           l_rc;
  gint32        l_max_master_frame_nr;
  gint32        l_cnt_encoded_frames;
  gint32        l_cnt_reused_frames;
  gint          l_video_tracks = 0;
  gint32        l_check_flags;
  t_awk_array   l_awk_arr;
  t_awk_array   *awp;
  GapCodecNameElem    *l_vcodec_list;
  const char          *l_env;
  GapStoryFetchResult  gapStoryFetchResultLocal;
  GapStoryFetchResult *gapStoryFetchResult;


  static gint32 funcId = -1;
  static gint32 funcIdVidFetch = -1;
  static gint32 funcIdVidEncode = -1;
  static gint32 funcIdVidCopy11 = -1;

  GAP_TIMM_GET_FUNCTION_ID(funcId,          "p_ffmpeg_encode_pass");
  GAP_TIMM_GET_FUNCTION_ID(funcIdVidFetch,  "p_ffmpeg_encode_pass.VideoFetch");
  GAP_TIMM_GET_FUNCTION_ID(funcIdVidEncode, "p_ffmpeg_encode_pass.VideoEncode");
  GAP_TIMM_GET_FUNCTION_ID(funcIdVidCopy11, "p_ffmpeg_encode_pass.VideoCopyLossless");

  GAP_TIMM_START_FUNCTION(funcId);

  epp = &gpp->evl;
  awp = &l_awk_arr;

  encStatusPtr->current_pass = current_pass;
  encStatusPtr->frames_processed = 0;

  l_cnt_encoded_frames = 0;
  l_cnt_reused_frames = 0;
  p_init_audio_workdata(awp);

  l_check_flags = GAP_VID_CHCHK_FLAG_SIZE;
  l_vcodec_list = p_setup_check_flags(epp, &l_check_flags);

  if(gap_debug)
  {
     printf("p_ffmpeg_encode: START\n");
     printf("  current_pass: %d (twoPassFlag:%d)\n", current_pass, epp->twoPassFlag);
     printf("  videoname: %s\n", gpp->val.videoname);
     printf("  audioname1: %s\n", gpp->val.audioname1);
     printf("  basename: %s\n", gpp->ainfo.basename);
     printf("  extension: %s\n", gpp->ainfo.extension);
     printf("  range_from: %d\n", (int)gpp->val.range_from);
     printf("  range_to: %d\n", (int)gpp->val.range_to);
     printf("  framerate: %f\n", (float)gpp->val.framerate);
     printf("  samplerate: %d\n", (int)gpp->val.samplerate);
     printf("  vid_width: %d\n", (int)gpp->val.vid_width);
     printf("  vid_height: %d\n", (int)gpp->val.vid_height);
     printf("  image_ID: %d\n", (int)gpp->val.image_ID);
     printf("  storyboard_file: %s\n\n", gpp->val.storyboard_file);
     printf("  input_mode: %d\n", gpp->val.input_mode);

     printf("  acodec_name: %s\n", epp->acodec_name);
     printf("  vcodec_name: %s\n", epp->vcodec_name);
     printf("  format_name: %s\n", epp->format_name);
     printf("  master_encoder_id:%d:\n", gpp->val.master_encoder_id);
  }



  l_rc = 0;

  /* make list of frameranges (input frames to feed the encoder) */
  {
    gint32 l_total_framecount;
    l_vidhand = gap_gve_story_open_vid_handle (gpp->val.input_mode
                                       ,gpp->val.image_ID
                                       ,gpp->val.storyboard_file
                                       ,gpp->ainfo.basename
                                       ,gpp->ainfo.extension
                                       ,gpp->val.range_from
                                       ,gpp->val.range_to
                                       ,&l_total_framecount
                                       );

    l_video_tracks = 1;
    l_vidhand->do_gimp_progress = FALSE;
  }

  /* TODO check for overwrite (in case we are called non-interactive)
   * overwrite check shall be done only if (current_pass < 2)
   */

  if (gap_debug) printf("Creating ffmpeg file.\n");


  /* check and open audio wave file(s) */
  p_open_audio_input_files(awp, gpp);


  /* OPEN the video file for write (create)
   *  successful open returns initialized FFMPEG handle structure
   */
  ffh = p_ffmpeg_open(gpp, current_pass, awp, l_video_tracks);
  if(ffh == NULL)
  {
    p_close_audio_input_files(awp);
    return(-1);     /* FFMPEG open Failed */
  }

  gapStoryFetchResult = &gapStoryFetchResultLocal;
  gapStoryFetchResult->raw_rgb_data = NULL;
  gapStoryFetchResult->video_frame_chunk_data = ffh->vst[0].video_buffer;


  /* Calculations for encoding the sound */
  p_sound_precalculations(ffh, awp, gpp);


  ffh->isMultithreadEnabled = gap_base_get_gimprc_gboolean_value(
                                 GAP_GIMPRC_VIDEO_ENCODER_FFMPEG_MULTIPROCESSOR_ENABLE
                               , FALSE  /* default */
                                 );
  if(gap_debug)
  {
    printf("pass: (1) isMultithreadEnabled: %d\n"
      ,(int)ffh->isMultithreadEnabled
      );
  }

  if((ffh->isMultithreadEnabled)
  && (epp->dont_recode_flag != TRUE))
  {
    eque = p_init_EncoderQueueResources(ffh, awp);
  }
  else
  {
    if(ffh->isMultithreadEnabled)
    {
      printf("WARNING: multiprocessor support is not implemented for lossless video encoding\n");
      printf("         therefore single cpu processing will be done due to the lossles render option\n");
    }
    /* lossless encoding is not supported in multiprocessor environment
     * (for 1:1 copy there is no benfit to be expected 
     * when parallel running encoder thread is used)
     */
    ffh->isMultithreadEnabled = FALSE;
  }


  if(gap_debug)
  {
    printf("pass: (2) isMultithreadEnabled: %d\n"
      ,(int)ffh->isMultithreadEnabled
      );
  }

  /* special setup (makes it possible to code sequences backwards)
   * (NOTE: Audio is NEVER played backwards)
   */
  if(gpp->val.range_from > gpp->val.range_to)
  {
    l_step  = -1;     /* operate in descending (reverse) order */
  }
  else
  {
    l_step  = 1;      /* operate in ascending order */
  }
  l_begin = gpp->val.range_from;
  l_end   = gpp->val.range_to;
  l_max_master_frame_nr = abs(l_end - l_begin) + 1;

  l_master_frame_nr = l_begin;
  ffh->encode_frame_nr = 1;
  ffh->countVideoFramesWritten = 0;
  while(l_rc >= 0)
  {
    gboolean l_force_keyframe;
    gint32   l_video_frame_chunk_size;
    gint32   l_video_frame_chunk_hdr_size;

    ffh->validEncodeFrameNr = ffh->encode_frame_nr;

    /* must fetch the frame into gimp_image  */
    /* load the current frame image, and transform (flatten, convert to RGB, scale, macro, etc..) */

    if(gap_debug)
    {
      printf("\nFFenc: before gap_story_render_fetch_composite_image_or_chunk\n");
    }

    GAP_TIMM_START_FUNCTION(funcIdVidFetch);
    if(eque)
    {
      GAP_TIMM_START_RECORD(&eque->mainReadFrame);
    }

    

    gap_story_render_fetch_composite_image_or_buffer_or_chunk(l_vidhand
                    , l_master_frame_nr  /* starts at 1 */
                    , (gint32)  gpp->val.vid_width       /* desired Video Width in pixels */
                    , (gint32)  gpp->val.vid_height      /* desired Video Height in pixels */
                    , gpp->val.filtermacro_file
                    , epp->dont_recode_flag              /* IN: TRUE try to fetch comressed chunk if possible */
                    , TRUE                               /* IN: enable_rgb888_flag TRUE rgb88 result where possible */
                    , l_vcodec_list                      /* IN: list of video_codec names that are compatible to the calling encoder program */
                    , ffh->vst[0].video_buffer_size      /* IN: video_frame_chunk_maxsize sizelimit (larger chunks are not fetched) */
                    , gpp->val.framerate
                    , l_max_master_frame_nr              /* the number of frames that will be encoded in total */
                    , l_check_flags                      /* IN: check_flags combination of GAP_VID_CHCHK_FLAG_* flag values */
                    , gapStoryFetchResult                /* OUT: struct with feth result */
                 );
                 
    l_force_keyframe = gapStoryFetchResult->force_keyframe;

    GAP_TIMM_STOP_FUNCTION(funcIdVidFetch);
    if(eque)
    {
      GAP_TIMM_STOP_RECORD(&eque->mainReadFrame);
    }

    if(gap_debug)
    {
      printf("\nFFenc: after gap_story_render_fetch_composite_image_or_chunk master_frame_nr:%d image_id:%d layer_id:%d resultEnum:%d\n"
        , (int) l_master_frame_nr
        , (int) gapStoryFetchResult->image_id
        , (int) gapStoryFetchResult->layer_id
        , (int) gapStoryFetchResult->resultEnum
        );
    }

    /* this block is done foreach handled video frame */
    if(gapStoryFetchResult->resultEnum != GAP_STORY_FETCH_RESULT_IS_ERROR)
    {

      if (gapStoryFetchResult->resultEnum == GAP_STORY_FETCH_RESULT_IS_COMPRESSED_CHUNK)
      {
        l_cnt_reused_frames++;
        if (gap_debug)
        {
          printf("DEBUG: 1:1 copy of frame %d\n", (int)l_master_frame_nr);
        }
        l_video_frame_chunk_size = gapStoryFetchResult->video_frame_chunk_size;
        l_video_frame_chunk_hdr_size = gapStoryFetchResult->video_frame_chunk_hdr_size;

        GAP_TIMM_START_FUNCTION(funcIdVidCopy11);

        /* dont recode, just copy video chunk to output videofile */
        p_ffmpeg_write_frame_chunk(ffh, l_video_frame_chunk_size, 0 /* vid_track */);

        GAP_TIMM_STOP_FUNCTION(funcIdVidCopy11);

        /* encode AUDIO FRAME (audio data for playbacktime of one frame) */
        if(ffh->countVideoFramesWritten > 0)
        {
          p_process_audio_frame(ffh, awp);
        }
      }
      else   /* encode one VIDEO FRAME */
      {
        l_cnt_encoded_frames++;

        if (gap_debug)
        {
          printf("DEBUG: %s encoding frame %d\n"
                , epp->vcodec_name
                , (int)l_master_frame_nr
                );
        }

        /* store the compressed video frame */
        if (gap_debug)
        {
          printf("GAP_FFMPEG: Writing frame nr. %d\n", (int)l_master_frame_nr);
        }


        GAP_TIMM_START_FUNCTION(funcIdVidEncode);

        if(ffh->isMultithreadEnabled)
        {
          p_ffmpeg_write_frame_and_audio_multithread(eque, gapStoryFetchResult, l_force_keyframe, 0 /* vid_track */ );
        }
        else
        {
          p_ffmpeg_write_frame(ffh, gapStoryFetchResult, l_force_keyframe, 0 /* vid_track */ );
          if(ffh->countVideoFramesWritten > 0)
          {
            p_process_audio_frame(ffh, awp);
          }
        }


        GAP_TIMM_STOP_FUNCTION(funcIdVidEncode);
      }



    }
    else  /* if fetch_ok */
    {
      l_rc = -1;
    }


    if(gpp->val.run_mode == GIMP_RUN_INTERACTIVE)
    {
      encStatusPtr->frames_processed++;
      encStatusPtr->frames_encoded = l_cnt_encoded_frames;
      encStatusPtr->frames_copied_lossless = l_cnt_reused_frames;

      gap_gve_misc_do_master_encoder_progress(encStatusPtr);
    }

    /* terminate on cancel reqeuset (CANCEL button was pressed in the master encoder dialog)
     * or in case of errors.
     */
    if((gap_gve_misc_is_master_encoder_cancel_request(encStatusPtr))
    || (l_rc < 0))
    {
       break;
    }

    /* detect regular end */
    if(l_master_frame_nr == l_end)
    {
       /* handle encoder latency (encoders typically hold some frames in internal buffers
        * that must be flushed after the last input frame was feed to the codec)
        * in case of codecs without frame latency
        * ffh->countVideoFramesWritten and ffh->encode_frame_nr
        * shall be already equal at this point. (where no flush is reuired)
        */
       int flushTries;
       int flushCount;

       if(ffh->isMultithreadEnabled)
       {
         /* wait until encoder thread has processed all enqued frames so far.
          * (otherwise the ffh->countVideoFramesWritten information will
          * not count the still unprocessed remaining frames in the queue)
          */
         p_waitUntilEncoderQueIsProcessed(eque);
       }

       flushTries = 2 + (ffh->validEncodeFrameNr - ffh->countVideoFramesWritten);
       
       for(flushCount = 0; flushCount < flushTries; flushCount++)
       {
         if(ffh->countVideoFramesWritten >= ffh->validEncodeFrameNr)
         {
           /* all frames are now written to the output video */
           break;
         }
         
         /* increment encode_frame_nr, because this is the base for pts timecode calculation
          * and some codecs (mpeg2video) complain about "non monotone timestamps"  otherwise.
          */
         ffh->encode_frame_nr++;

         GAP_TIMM_START_FUNCTION(funcIdVidEncode);

         if(ffh->isMultithreadEnabled)
         {
           p_ffmpeg_write_frame_and_audio_multithread(eque, NULL, l_force_keyframe, 0 /* vid_track */ );
           if(gap_debug)
           {
             printf("p_ffmpeg_encode_pass: Flush-Loop for Codec remaining frames MainTID:%d, flushCount:%d\n"
                ,p_base_get_thread_id_as_int()
                ,(int)flushCount
                );
             p_debug_print_RingbufferStatus(eque);
           }
         }
         else
         {
           p_ffmpeg_write_frame(ffh, NULL, l_force_keyframe, 0 /* vid_track */ );
           /* continue encode AUDIO FRAME (audio data for playbacktime of one frame)
            */
           if(ffh->countVideoFramesWritten > 0)
           {
             p_process_audio_frame(ffh, awp);
           }
         }

         GAP_TIMM_STOP_FUNCTION(funcIdVidEncode);
         
       }
       break;
    }
    /* advance to next frame */
    l_master_frame_nr += l_step;
    ffh->encode_frame_nr++;

  }  /* end loop foreach frame */

  if(ffh != NULL)
  {
    if(gap_debug)
    {
      printf("before: p_ffmpeg_close\n");
    }
    
    if(ffh->isMultithreadEnabled)
    {
      p_waitUntilEncoderQueIsProcessed(eque);
    }
    
    p_ffmpeg_close(ffh);
    if(gap_debug)
    {
      printf("after: p_ffmpeg_close\n");
    }
    g_free(ffh);
  }

  if(gap_debug)
  {
    printf("before: p_close_audio_input_files\n");
  }

  p_close_audio_input_files(awp);
  if(gap_debug)
  {
    printf("after: p_close_audio_input_files\n");
  }

  if(l_vidhand)
  {
    if(gap_debug)
    {
      gap_gve_story_debug_print_framerange_list(l_vidhand->frn_list, -1);
    }
    gap_gve_story_close_vid_handle(l_vidhand);
  }

  /* statistics */
  if(gap_debug)
  {
    printf("current_pass          %d\n", (int)current_pass);
    printf("encoded       frames: %d\n", (int)l_cnt_encoded_frames);
    printf("1:1 copied    frames: %d\n", (int)l_cnt_reused_frames);
    printf("total handled frames: %d\n", (int)l_cnt_encoded_frames + l_cnt_reused_frames);
  }


  GAP_TIMM_STOP_FUNCTION(funcId);
  GAP_TIMM_PRINT_FUNCTION_STATISTICS();

  if(eque)
  {
    g_usleep(30000);  /* sleep 0.3 seconds */
    
    /*  print MAIN THREAD runtime statistics */
    GAP_TIMM_PRINT_RECORD(&eque->mainReadFrame,         "... mainReadFrame");
    GAP_TIMM_PRINT_RECORD(&eque->mainWriteFrame,        "... mainWriteFrame");
    GAP_TIMM_PRINT_RECORD(&eque->mainDrawableToRgb,     "... mainWriteFrame.DrawableToRgb");
    GAP_TIMM_PRINT_RECORD(&eque->mainElemMutexWaits,    "... mainElemMutexWaits");
    GAP_TIMM_PRINT_RECORD(&eque->mainPoolMutexWaits,    "... mainPoolMutexWaits");
    GAP_TIMM_PRINT_RECORD(&eque->mainEnqueueWaits,      "... mainEnqueueWaits");
    GAP_TIMM_PRINT_RECORD(&eque->mainPush2,             "... mainPush2 (re-start times of encoder thread)");

    /*  print Encoder THREAD runtime statistics */
    GAP_TIMM_PRINT_RECORD(&eque->ethreadElemMutexWaits, "... ethreadElemMutexWaits");
    GAP_TIMM_PRINT_RECORD(&eque->ethreadPoolMutexWaits, "... ethreadPoolMutexWaits");
    GAP_TIMM_PRINT_RECORD(&eque->ethreadEncodeFrame,    "... ethreadEncodeFrame");

    p_free_EncoderQueueResources(eque);
    g_free(eque);
  }
 
  if (gapStoryFetchResult->raw_rgb_data != NULL)
  {
    /* finally free the rgb data that was optionally allocated in storyboard fetch calls.
     * The raw_rgb_data is typically allocated at first direct RGB888 fetch (if any)
     * and reused in all further direct RGB888 fetches.
     */
    g_free(gapStoryFetchResult->raw_rgb_data);
  }
  return l_rc;
}    /* end p_ffmpeg_encode_pass */


/* ---------------------------
 * p_ffmpeg_encode
 * ---------------------------
 *    The main "productive" routine
 *    ffmpeg encoding of anim frames, based on ffmpeg lib (by Fabrice Bellard)
 *    Audio encoding is Optional.
 *    (wav_audiofile must be provided in that case)
 *
 * returns   value >= 0 if ok
 *           (or -1 on error)
 */
static gint
p_ffmpeg_encode(GapGveFFMpegGlobalParams *gpp)
{
  GapGveFFMpegValues   *epp = NULL;
  gint32        l_current_pass = 0;  /* 0 for standard encoding, for 2-pass encoding loop from 1 to 2 */
  gint l_rc;
  GapGveMasterEncoderStatus encStatus;
  GapGveMasterEncoderStatus *encStatusPtr;

  epp = &gpp->evl;
  encStatusPtr = &encStatus;

  if(gpp->val.run_mode == GIMP_RUN_INTERACTIVE)
  {
    gap_gve_misc_initGapGveMasterEncoderStatus(encStatusPtr
       , gpp->val.master_encoder_id
       , abs(gpp->val.range_to - gpp->val.range_from) + 1   /* total_frames */
       );
  }


  if (epp->twoPassFlag == TRUE)
  {
    l_current_pass = 1;
    l_rc = p_ffmpeg_encode_pass(gpp, l_current_pass, encStatusPtr);
    if (l_rc >= 0)
    {
      l_current_pass = 2;
      l_rc = p_ffmpeg_encode_pass(gpp, l_current_pass, encStatusPtr);
    }
  }
  else
  {
    l_current_pass = 0;
    l_rc = p_ffmpeg_encode_pass(gpp, l_current_pass, encStatusPtr);
  }

  return l_rc;
}    /* end p_ffmpeg_encode */
