/* gap_vid_api.h
 *
 * ------------------------
 * API READ movie frames
 * ------------------------
 */

/* ------------------------------------------------
 * 2003.05.09
 *
 * This API (GAP Video Api) provides basic READ functions to access
 * Videoframes of some sopported Videoformats.
 *
 * 2004.09.25     (hof)  added GVA_util_check_mpg_frame_type
 * 2004.04.25     (hof)  integration into gimp-gap
 * 2004.03.06     (hof)  videoindex
 * 2004.02.28     (hof)  added procedures GVA_frame_to_buffer, GVA_delace_frame
 */

#ifndef GAP_VID_API_H
#define GAP_VID_API_H

#include <gtk/gtk.h>
#include <libgimp/gimp.h>


#define GVA_MPGFRAME_UNKNOWN -1
#define GVA_MPGFRAME_I_TYPE 1
#define GVA_MPGFRAME_P_TYPE 2
#define GVA_MPGFRAME_B_TYPE 3

#define GVA_MPGHDR_PICTURE_START_CODE    0x00000100
#define GVA_MPGHDR_GOP_START_CODE        0x000001b8


/* -----------------------
 * TYPES
 * -----------------------
 */

typedef enum t_GVA_AudPos
{
  GVA_AMOD_FRAME         /* start audio reads from current frame pos */
 ,GVA_AMOD_CUR_AUDIO     /* start audio reads from current audio pos */
 ,GVA_AMOD_REREAD        /* start from same pos as last audioread */
} t_GVA_AudPos;


typedef enum t_GVA_PosUnit
{
  GVA_UPOS_FRAMES
 ,GVA_UPOS_SECS
 ,GVA_UPOS_PRECENTAGE         /* 0.0 upto 1.0 */
} t_GVA_PosUnit;


typedef enum t_GVA_RetCode
{
  GVA_RET_OK    = 0
 ,GVA_RET_EOF   = 1   /* not implemented yet */
 ,GVA_RET_ERROR = 2
} t_GVA_RetCode;

#define GVA_MAX_FCACHE_SIZE 1000

/*
 *  Frame Cache schema with at cachesize of 5 elements
 *
 *       +----+     +----+     +----+     +----+     +----+
 *  +----|prev|<----|prev|<----|prev|<----|prev|<----|prev|<----+
 *  | +->|next|---->|next|---->|next|---->|next|---->|next|---+ |
 *  | |  |frm_|     |frm_|     |    |     |    |     |    |   | |
 *  | |  |data|     |data|     |    |     |    |     |    |   | |
 *  | |  +----+     +----+     +----+     +----+     +----+   | |
 *  | |     |          |          |          |          |     | |
 *  | |   #####      #####      +---+      +---+      +---+   | |
 *  | |   #####      #####      |   |      |   |      |   |   | |
 *  | |   #####      #####      |   |      |   |      |   |   | |
 *  | |   #####      #####      +---+      +---+      +---+   | |
 *  | |                                                       | |
 *  | +-------------------------------------------------------+ |
 *  +-----------------------------------------------------------+
 *
 * The Frame cache is a list, linked to a ring.
 * all elements and frame_data pointers are created at open time,
 * the ringpointers are updated when the calling program
 * changes max_frame_cache.
 *
 * gvahand->frame_data and gvahand->row_pointers
 * are pointers to the current framecache, and are
 * advanced before each read_next_frame call.
 *
 */

typedef struct t_GVA_Frame_Cache_Elem
{
   gint32  id;             /* element identifier */
   gint32  framenumber;    /* -1 is the mark for unused elements */
   guchar *frame_data;     /* uncompressed framedata */
   guchar **row_pointers;  /* array of pointers to each row of the frame_data */
   void *prev;
   void *next;
} t_GVA_Frame_Cache_Elem;

typedef struct t_GVA_Frame_Cache
{
  t_GVA_Frame_Cache_Elem *fc_current;  /* pointer to 1.st element of frame chache list */
  gint32            frame_cache_size;  /* number of frames in the cache */
  gint32            max_fcache_id;
  gboolean          fcache_locked;     /* TRUE whilw SEEK_FRAME and GET_NEXT_FRAME operations in progress */
} t_GVA_Frame_Cache;


typedef  gboolean       (*t_GVA_progress_callback_fptr)(gdouble progress, gpointer user_data);


/* -----------------
 * vindex stuff
 * -----------------
 */

#define GVA_VIDINDEXTAB_BLOCK_SIZE 500
#define GVA_VIDINDEXTAB_DEFAULT_STEPSIZE 100

typedef struct t_GVA_VideoindexHdr
{
  char     key_identifier[15];
  char     key_type[5];
  char     val_type[10];
  char     key_step[5];
  char     val_step[10];
  char     key_size[5];
  char     val_size[10];
  char     key_trak[5];
  char     val_trak[5];
  char     key_ftot[5];
  char     val_ftot[10];
  char     key_deco[5];
  char     val_deco[15];
  char     key_mtim[5];
  char     val_mtim[15];
  char     key_flen[5];
  char     val_flen[10];
  
} t_GVA_VideoindexHdr;

typedef enum
{
  GVA_IDX_TT_GINT64
 ,GVA_IDX_TT_GDOUBLE
 ,GVA_IDX_TT_UNDEFINED
} t_GVA_IndexTabType;

typedef union
{
  gint64              offset_gint64;
  gdouble             offset_gdouble;
} t_GVA_UnionElem;

typedef struct t_GVA_IndexElem
{
  gint32  seek_nr;
  guint16 frame_length;
  guint16 checksum;

  t_GVA_UnionElem uni;
} t_GVA_IndexElem;

typedef struct t_GVA_Videoindex  /* nick: vindex */
{
  t_GVA_VideoindexHdr  hdr;
  t_GVA_IndexTabType   tabtype;
  char                *videoindex_filename;
  char                *videofile_uri;
  char                *tocfile;
  gint32               stepsize;
  gint32               tabsize_used;
  gint32               tabsize_allocated;
  gint32               track;
  gint32               total_frames;
  gint32               mtime;
  t_GVA_IndexElem     *ofs_tab;
} t_GVA_Videoindex;


/* ------------------------------------
 * GVA HANDLE Stucture
 * ------------------------------------
 */
typedef struct t_GVA_Handle  /* nickname: gvahand */
{
  gboolean cancel_operation;    /* */
  gboolean dirty_seek;          /* libmpeg3: FALSE: use native seek, TRUE: use gopseek workaround  */
  gboolean emulate_seek;        /* emulate seek ops by dummy read ops (for slow and exact positioning) */
  gboolean create_vindex;       /* TRUE: allow the fptr_count_frames procedure to create a videoindex file */
  t_GVA_Videoindex *vindex;
  gint32            mtime;
  
  gboolean disable_mmx;
  gboolean do_gimp_progress;    /* WARNING: dont try to set this TRUE if you call the API from a pthread !! */
  gboolean all_frames_counted;  /* TRUE: counted all frames, total_frames is an exact value
                                 * FALSE: total_frames may not tell the exact value.
                                 */
  gboolean all_samples_counted;
  gint32   frame_counter;

  gpointer progress_cb_user_data;
  t_GVA_progress_callback_fptr fptr_progress_callback; /* if != NULL: The API calls this user procedure with current progress
                                                        * if the procedure returns TRUE
							* the API does cancel current seek operations immediate.
							*/


  /* PUBLIC information (read only outside the API) */
  gint32  total_frames;         /* 0 no video */
  gdouble framerate;            /* frames per sec */
  gint32  current_frame_nr;     /* nr of the image in the internal frame_data buffer
                                 * (0 at open, 1 after reading 1.st frame)
                                 * will not be updated at seek operations
                                 */
  gint32  current_seek_nr;      /* 1 at open, 2 after reading 1.st frame
                                 * is updated on seek and read_next operations
                                 */
  gdouble current_sample;       /* 1 at open. Position of current audiosample
                                 * is updated on seek and read_next operations
                                 */
  gdouble reread_sample_pos;    /* last audioread pos (used in avlib ffmpeg only) */
  gint32  width;                /* width of the videoframes */
  gint32  height;               /* height of the videoframes */
  gdouble aspect_ratio;         /* 0 for unknown, or aspect_ratio width/heigth  */
  gint32  vtracks;              /* number of videotracks in the videofile */
  gint32  atracks;              /* number of audiotracks in the videofile */
  gdouble audio_playtime_sec;   /* 0 if no audio */
  gint32  total_aud_samples;    /* number of audio samples */
  gint32  samplerate;           /* audio samples per sec */
  gint32  audio_cannels;        /* number of channel (in the selected aud_track) */


  gdouble percentage_done;      /* 0.0 <= percentage_done <= 1.0 */

  /* Image and frame buffer */
  gint32 image_id;             /* -1 if there is no image */
  gint32 layer_id;

  /* frame buffer */
  guchar *frame_data;          /* the frame data buffer (colormodel RGBA or RGB is decoder dependent) */
  guchar **row_pointers;       /* array of pointers to each row of the frame_data */
  gint32 frame_bpp;            /* dont change this, is set by the decoder (Byte per pixel of frame_data buffer, delivered by decoder 4 or 3) */

  t_GVA_Frame_Cache fcache;    /* Frame Cache structure */

  /* frame cache buffer(use GVA_search_fcache to set the fc_xxx pointers)
   */
  guchar *fc_frame_data;       /* cached frame data buffer  */
  guchar **fc_row_pointers;    /* array of pointers to each row  */

  /* PRIVATE members (dont change this outside the API !) */
  void    *dec_elem;          /*  t_GVA_DecoderElem *   a Decoder that can handle the videofile */
  void    *decoder_handle;
  gint32  vid_track;
  gint32  aud_track;
  char   *filename;
  gboolean pthread_save;
} t_GVA_Handle;


/* Function Typedefs */
typedef  gboolean       (*t_check_sig_fptr)(char *filename);
typedef  void           (*t_open_read_fptr)(char *filename, t_GVA_Handle *gvahand);
typedef  void           (*t_close_fptr)(t_GVA_Handle *gvahand);
typedef  t_GVA_RetCode  (*t_get_next_frame_fptr)(t_GVA_Handle *gvahand);
typedef  t_GVA_RetCode  (*t_seek_frame_fptr)(t_GVA_Handle *gvahand, gdouble pos, t_GVA_PosUnit pos_unit);

typedef  t_GVA_RetCode  (*t_seek_audio_fptr)(t_GVA_Handle *gvahand, gdouble pos, t_GVA_PosUnit pos_unit);
typedef  t_GVA_RetCode  (*t_get_audio_fptr)(t_GVA_Handle *gvahand
                             ,gint16 *output_i
                             ,gint32 channel
                             ,gdouble samples
                             ,t_GVA_AudPos mode_flag
                        );
typedef  t_GVA_RetCode  (*t_count_frames_fptr)(t_GVA_Handle *gvahand);
typedef  t_GVA_RetCode  (*t_get_video_chunk_fptr)(t_GVA_Handle *gvahand
                             ,gint32 frame_nr
                             ,unsigned char *chunk
                             ,gint32 *sizes
                             ,gint32 max_size
                        );



/* List Element Description for a Decoder */
typedef struct t_GVA_DecoderElem
{
  char                 *decoder_name;
  char                 *decoder_description;
  void                 *next;

  /* functionpointers to decoder specific procedures */
  t_check_sig_fptr              fptr_check_sig;
  t_open_read_fptr              fptr_open_read;
  t_close_fptr                  fptr_close;
  t_get_next_frame_fptr         fptr_get_next_frame;
  t_seek_frame_fptr             fptr_seek_frame;

  t_seek_audio_fptr             fptr_seek_audio;
  t_get_audio_fptr              fptr_get_audio;
  t_count_frames_fptr           fptr_count_frames;
  t_get_video_chunk_fptr        fptr_get_video_chunk;
} t_GVA_DecoderElem;



/* ------------------------------------
 * Declare Public API functions
 * ------------------------------------
 * Tracknumbers (vid_track, aud_track) start at 1 for the API user
 * (API internal the first track has number 0)
 */
t_GVA_Handle *
GVA_open_read_pref(const char *filename, gint32 vid_track, gint32 aud_track
                 ,const char *preferred_decoder
                 ,gboolean disable_mmx
                 );
t_GVA_Handle*   GVA_open_read(const char *filename, gint32 vid_track, gint32 aud_track);
t_GVA_RetCode   GVA_get_next_frame(t_GVA_Handle  *gvahand);
t_GVA_RetCode   GVA_seek_frame(t_GVA_Handle  *gvahand, gdouble pos, t_GVA_PosUnit pos_unit);
void            GVA_close(t_GVA_Handle  *gvahand);

t_GVA_RetCode   GVA_seek_audio(t_GVA_Handle  *gvahand, gdouble pos, t_GVA_PosUnit pos_unit);
t_GVA_RetCode   GVA_get_audio(t_GVA_Handle  *gvahand
                             ,gint16 *output_i            /* preallocated buffer large enough for samples * siezeof gint16 */
                             ,gint32 channel              /* audiochannel 1 upto n */
                             ,gdouble samples             /* number of samples to read */
                             ,t_GVA_AudPos mode_flag      /* specify the position where to start reading audio from */
                             );

t_GVA_RetCode   GVA_count_frames(t_GVA_Handle  *gvahand);

void            GVA_set_fcache_size(t_GVA_Handle *gvahand
                 ,gint32 frames_to_keep_cahed
                 );

t_GVA_RetCode   GVA_search_fcache(t_GVA_Handle *gvahand
                 ,gint32 framenumber
                 );

t_GVA_RetCode   GVA_search_fcache_by_index(t_GVA_Handle *gvahand
                 ,gint32 index
                 ,gint32 *framenumber
                 );
void            GVA_debug_print_fcache(t_GVA_Handle *gvahand);

t_GVA_RetCode   GVA_gimp_image_to_rowbuffer(t_GVA_Handle *gvahand, gint32 image_id);

t_GVA_RetCode   GVA_frame_to_gimp_layer(t_GVA_Handle *gvahand
                , gboolean delete_mode
                , gint32 framenumber
                , gint32 deinterlace
                , gdouble threshold     /* 0.0 <= threshold <= 1.0 */
                );
gint32          GVA_frame_to_gimp_layer_2(t_GVA_Handle *gvahand
                , gint32 *image_id
                , gint32 old_layer_id
                , gboolean delete_mode
                , gint32 framenumber
                , gint32 deinterlace
                , gdouble threshold     /* 0.0 <= threshold <= 1.0 */
                );
gint32         GVA_fcache_to_gimp_image(t_GVA_Handle *gvahand
                , gint32 min_framenumber
                , gint32 max_framenumber
                , gint32 deinterlace
                , gdouble threshold
                );

guchar *       GVA_frame_to_buffer(t_GVA_Handle *gvahand
                , gboolean do_scale
                , gint32 framenumber
                , gint32 deinterlace
                , gdouble threshold
		, gint32 *bpp
		, gint32 *width
		, gint32 *height
                );
guchar *       GVA_fetch_frame_to_buffer(t_GVA_Handle *gvahand
                , gboolean do_scale
                , gint32 framenumber
                , gint32 deinterlace
                , gdouble threshold
		, gint32 *bpp
		, gint32 *width
		, gint32 *height
                );
guchar *       GVA_delace_frame(t_GVA_Handle *gvahand
                , gint32 deinterlace
                , gdouble threshold
		);
		
gint32          GVA_percent_2_frame(gint32 total_frames, gdouble percent);
gdouble         GVA_frame_2_percent(gint32 total_frames, gdouble framenr);

void            GVA_debug_print_videoindex(t_GVA_Handle *gvahand);
char *          GVA_build_videoindex_filename(const char *filename, gint32 track, const char *decoder_name);
char *          GVA_build_video_toc_filename(const char *filename, const char *decoder_name);

gboolean        GVA_has_video_chunk_proc(t_GVA_Handle  *gvahand);
t_GVA_RetCode   GVA_get_video_chunk(t_GVA_Handle  *gvahand
                   , gint32 frame_nr
                   , unsigned char *chunk
                   , gint32 *size
                   , gint32 max_size);
gint           GVA_util_check_mpg_frame_type(unsigned char *buffer, gint32 buf_size);
void           GVA_util_fix_mpg_timecode(unsigned char *buffer
                         ,gint32 buf_size
                         ,gdouble master_framerate
                         ,gint32  master_frame_nr
                         );

#endif