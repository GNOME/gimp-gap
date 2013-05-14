/* gap_vid_api.c
 *
 * This API (GAP Video Api) provides basic READ functions to access
 * Videoframes of some sopported Videoformats.
 *
 * ------------------------
 * API READ movie frames
 * ------------------------
 *
 * video decoder libraries can be turned on (conditional compile)
 * by define the following:
 *
 *  ENABLE_GVA_LIBMPEG3
 *  ENABLE_GVA_LIBQUICKTIME
 *  ENABLE_GVA_LIBAVFORMAT
 *  ENABLE_GVA_GIMP
 *
 * this is done via options passed to the configure script in the
 * gap main directory. the configure script saaves the
 * selected configuration as #define statements in the config.h file
 *
 * ---   API master Procedures
 * gap_api_vid.c
 *
 * --- the library dependent wrapper modules
 * ---  (the modules are included and compiled
 * ---   as one unit with this main api sourcefile.
 *       this helps to keep the number of external symbols small)
 *        
 * gap_api_vid_quicktime.h
 * gap_api_vid_quicktime.c
 * gap_api_vid_mpeg3.h
 * gap_api_vid_mpeg3.c
 * gap_api_vid_avi.h
 * gap_api_vid_avi.c
 * ---------------------------------------------
 */


/* API access for GIMP-GAP frame sequences needs no external
 * libraries and is always enabled
 * (there is no configuration parameter for this "decoder" implementation)
 */
#define ENABLE_GVA_GIMP 1

/* ------------------------------------------------
 * revision history
 *
 * 2010.11.20     (hof)  added multiprocessor support.
 * 2004.04.25     (hof)  integration into gimp-gap, using config.h
 * 2004.02.28     (hof)  added procedures GVA_frame_to_buffer, GVA_delace_frame
 */

#include <config.h>

#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <math.h>

#include <glib/gstdio.h>
#include <gtk/gtk.h>

#include <libgimp/gimp.h>
#include <gap_vid_api-intl.h>

/* includes for UNIX fork-based workarond for the libmpeg3 crash on close bug */
#ifndef G_OS_WIN32
#include <unistd.h>          /* for fork */
#include <sys/wait.h>
#include <signal.h> 
#endif

extern      int gap_debug; /* ==0  ... dont print debug infos */

#include "gap_vid_api.h"
#include "gap_base.h"


static inline void   gva_delace_mix_rows( gint32 width
                        , gint32 bpp
                        , gint32 row_bytewidth
                        , gint32 mix_threshold   /* 0 <= mix_threshold <= 33554432 (256*256*256*2) */
                        , const guchar *prev_row
                        , const guchar *next_row
                        , guchar *mixed_row
                        );
static gint32        gva_delace_calculate_mix_threshold(gdouble threshold);
static gint32        gva_delace_calculate_interpolate_flag(gint32 deinterlace);

#include "gap_vid_api_util.c"
#include "gap_vid_api_vidindex.c"
#include "gap_vid_api_mp_util.c"
#include "gap_libgapbase.h"

t_GVA_DecoderElem  *GVA_global_decoder_list = NULL;



/* max threshold for row mix algorithm (used for deinterlacing frames)
 * (510*510) + (256+256+256)
 */
#define MIX_MAX_THRESHOLD  260865



static void                      p_alloc_rowpointers(t_GVA_Handle *gvahand, t_GVA_Frame_Cache_Elem  *fc_ptr);
static t_GVA_Frame_Cache_Elem *  p_new_frame_cache_elem(t_GVA_Handle *gvahand);
static void                      p_drop_next_frame_cache_elem(t_GVA_Frame_Cache *fcache);
static void                      p_drop_frame_cache(t_GVA_Handle *gvahand);
static gint32                    p_build_frame_cache(t_GVA_Handle *gvahand, gint32 frames_to_keep_cahed);
static gdouble                   p_guess_total_frames(t_GVA_Handle *gvahand);

static void                      p_register_all_decoders(void);
static void                      p_gva_worker_close(t_GVA_Handle  *gvahand);
static t_GVA_RetCode             p_gva_worker_get_next_frame(t_GVA_Handle  *gvahand);
static t_GVA_RetCode             p_gva_worker_seek_frame(t_GVA_Handle  *gvahand, gdouble pos, t_GVA_PosUnit pos_unit);
static t_GVA_RetCode             p_gva_worker_seek_audio(t_GVA_Handle  *gvahand, gdouble pos, t_GVA_PosUnit pos_unit);
static t_GVA_RetCode             p_gva_worker_get_audio(t_GVA_Handle  *gvahand
                                    ,gint16 *output_i            /* preallocated buffer large enough for samples * siezeof gint16 */
                                    ,gint32 channel              /* audiochannel 1 upto n */
                                    ,gdouble samples             /* number of samples to read */
                                    ,t_GVA_AudPos mode_flag      /* specify the position where to start reading audio from */
                                    );
static t_GVA_RetCode             p_gva_worker_count_frames(t_GVA_Handle  *gvahand);
static t_GVA_RetCode             p_gva_worker_get_video_chunk(t_GVA_Handle  *gvahand
                                    , gint32 frame_nr
                                    , unsigned char *chunk
                                    , gint32 *size
                                    , gint32 max_size);
static t_GVA_Handle *            p_gva_worker_open_read(const char *filename, gint32 vid_track, gint32 aud_track
                                    ,const char *preferred_decoder
                                    ,gboolean disable_mmx
                                    );


/* ---------------------------
 * GVA_percent_2_frame
 * ---------------------------
 * 0.0%     returns frame #1
 * 100.0 %  returns frame #total_frames
 */
gint32
GVA_percent_2_frame(gint32 total_frames, gdouble percent)
{
  gint32 framenr;
  gdouble ffrnr;
  ffrnr = 1.5 + (percent / 100.0) * MAX(((gdouble)total_frames -1), 0);
  framenr = (gint32)ffrnr;

  if(gap_debug) printf("GVA_percent_2_frame  %f  #:%d\n", (float)percent, (int)framenr );

  return(framenr);
}  /* end GVA_percent_2_frame */

/* ---------------------------
 * GVA_frame_2_percent
 * ---------------------------
 */
gdouble
GVA_frame_2_percent(gint32 total_frames, gdouble framenr)
{
  gdouble percent;

  percent = 100.0 * ((gdouble)MAX((framenr -1),0) /  MAX((gdouble)(total_frames -1) ,1.0));

  if(gap_debug) printf("GVA_frame_2_percent  %f  #:%d\n", (float)percent, (int)framenr );

  return(CLAMP(percent, 0.0, 100.0));
}  /* end GVA_frame_2_percent */


/* ---------------------------
 * GVA_frame_2_secs
 * ---------------------------
 */
gdouble
GVA_frame_2_secs(gdouble framerate, gint32 framenr)
{
  gdouble secs;

  secs = ((gdouble)framenr  /  MAX(framerate, 1.0) );

  if(gap_debug) printf("GVA_frame_2_secs  %f  #:%d\n", (float)secs, (int)framenr );

  return(secs);
}  /* end GVA_frame_2_secs */


/* ---------------------------
 * GVA_frame_2_samples
 * ---------------------------
 */
gdouble
GVA_frame_2_samples(gdouble framerate, gint32 samplerate, gint32 framenr)
{
  gdouble samples;

  samples = ((gdouble)framenr  /  MAX(framerate, 1.0) ) * (gdouble)samplerate;

  if(gap_debug) printf("GVA_frame_2_samples  %f  #:%d\n", (float)samples, (int)framenr );

  return(samples);
}  /* end GVA_frame_2_samples */



/* XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX BEGIN fcache procedures */

/* ----------------------------------------------------
 * GVA_debug_print_fcache
 * ----------------------------------------------------
 */
void
GVA_debug_print_fcache(t_GVA_Handle *gvahand)
{
  t_GVA_Frame_Cache *fcache;
  t_GVA_Frame_Cache_Elem  *fc_ptr;


  printf("GVA_debug_print_fcache: START\n" );


  fcache = &gvahand->fcache;
  if(fcache->fc_current)
  {
    gint ii;

    printf("frame_cache_size: %d\n", (int)fcache->frame_cache_size);

    fc_ptr = (t_GVA_Frame_Cache_Elem  *)fcache->fc_current;
    for(ii=0; ii < fcache->frame_cache_size; ii++)
    {
       printf("  [%d]  ID:%d framenumber: %d  (my_adr: %d  next:%d  prev:%d)\n"
             , (int)ii
             , (int)fc_ptr->id
             , (int)fc_ptr->framenumber
             , (int)fc_ptr
             , (int)fc_ptr->next
             , (int)fc_ptr->prev
             );

      fc_ptr = (t_GVA_Frame_Cache_Elem  *)fc_ptr->prev;

      if(fcache->fc_current == fc_ptr)
      {
        printf("STOP, we are back at startpoint of the ringlist\n\n");
        return;
      }
      if(fc_ptr == NULL)
      {
        printf("internal error, ringlist is broken !!!!!!!!!\n\n");
        return;
      }
    }
    printf("internal error, too many elements found (maybe ringlist is not linked properly !!\n\n");
    return;
  }

  printf("fcache is empty (no current element found)\n");

}  /* end GVA_debug_print_fcache */


/* ------------------------------
 * p_alloc_rowpointers
 * ------------------------------
 * allocate frame_data buffer (for RGB or RGBA imagedata)
 * and  row pointers for each row
 * We must allocate 4 extra bytes in the last output_row.
 * This is scratch area for the MMX routines used by some decoder libs.
 */
static void
p_alloc_rowpointers(t_GVA_Handle *gvahand, t_GVA_Frame_Cache_Elem  *fc_ptr)
{
    int ii, wwidth, wheight, bpp;

    wwidth  = MAX(gvahand->width, 2);
    wheight = MAX(gvahand->height, 2);
    bpp = gvahand->frame_bpp;

    fc_ptr->frame_data = g_malloc(wwidth * wheight * bpp + 4);
    fc_ptr->row_pointers = g_malloc(sizeof(unsigned char*) * wheight);

    /* init row pointers */
    for(ii = 0; ii < wheight; ii++)
    {
      fc_ptr->row_pointers[ii] = &fc_ptr->frame_data[ii * wwidth * bpp];
    }
}  /* end p_alloc_rowpointers */


/* ----------------------------------------------------
 * p_new_frame_cache_elem
 * ----------------------------------------------------
 */
static t_GVA_Frame_Cache_Elem *
p_new_frame_cache_elem(t_GVA_Handle *gvahand)
{
  t_GVA_Frame_Cache_Elem  *fc_ptr;


  fc_ptr = g_malloc0(sizeof(t_GVA_Frame_Cache_Elem));
  gvahand->fcache.max_fcache_id++;
  fc_ptr->id = gvahand->fcache.max_fcache_id;
  fc_ptr->framenumber = -1;    /* marker for unused element, framedata is allocated but not initialized */
  fc_ptr->prev = fc_ptr;
  fc_ptr->next = fc_ptr;

  p_alloc_rowpointers(gvahand, fc_ptr);

  return (fc_ptr);
}  /* end p_new_frame_cache_elem */



/* ----------------------------------------------------
 * p_drop_next_frame_cache_elem
 * ----------------------------------------------------
 * drop the next (== oldest) element of the frame cache list
 */
static void
p_drop_next_frame_cache_elem(t_GVA_Frame_Cache *fcache)
{
  t_GVA_Frame_Cache_Elem  *fc_ptr;

  if(fcache)
  {
    if(fcache->fc_current)
    {
      fc_ptr = (t_GVA_Frame_Cache_Elem  *)fcache->fc_current->next;
      if(fc_ptr)
      {
        if(gap_debug) printf("p_drop_next_frame_cache_elem framenumber:%d\n", (int)fc_ptr->framenumber);

        if(fc_ptr != fcache->fc_current)
        {
          t_GVA_Frame_Cache_Elem  *fc_nxt;

          /* the ring still has 2 or more elements
           * we must update prev pointer of the next element
           * and next pointer of the previous element
           */
          fc_nxt = (t_GVA_Frame_Cache_Elem  *)fc_ptr->next;
          fcache->fc_current->next = fc_nxt;
          fc_nxt->prev = fcache->fc_current;
        }
        else
        {
          /* we are dropping the last element, set fc_current pointer to NULL */
          fcache->fc_current = NULL;
        }

        g_free(fc_ptr->frame_data);
        g_free(fc_ptr->row_pointers);
        g_free(fc_ptr);
      }
    }
  }
}  /* end p_drop_next_frame_cache_elem */


/* ----------------------------------------------------
 * p_drop_frame_cache
 * ----------------------------------------------------
 */
static void
p_drop_frame_cache(t_GVA_Handle *gvahand)
{
  t_GVA_Frame_Cache *fcache;

  if(gap_debug)  printf("p_drop_frame_cache START\n");
  fcache = &gvahand->fcache;
  if(fcache)
  {
    while(fcache->fc_current)
    {
      p_drop_next_frame_cache_elem(fcache);
    }
  }
  if(gap_debug) printf("p_drop_frame_cache END\n");

}  /* end p_drop_frame_cache */


/* ----------------------------------------------------
 * p_build_frame_cache
 * ----------------------------------------------------
 * the frame cache is a double linked ringlist.
 * this procedure creates such a list (if we have none)
 * or changes the (existing) ringlist to the desired number of elements.
 * this is done by dropping the oldest elements (if we already have to much)
 * or adding new elements (if we have not enough elements)
 *
 * if the fcache does not exist (if fcache gvahand->fcache.fc_current == NULL)
 * it will be created.
 * the pointers
 *    gvahand->frame_data
 *    gvahand->row_pointers
 * are set to point at the 1.st (current) fcache element in that case.
 */
static gint32
p_build_frame_cache(t_GVA_Handle *gvahand, gint32 frames_to_keep_cahed)
{
  t_GVA_Frame_Cache *fcache;
  t_GVA_Frame_Cache_Elem  *fc_ptr;

  fcache = &gvahand->fcache;
  fcache->frame_cache_size = 0;
  if(fcache->fc_current)
  {
    fcache->frame_cache_size = 1;
    fc_ptr = (t_GVA_Frame_Cache_Elem  *)fcache->fc_current->next;
    while(fcache->fc_current != fc_ptr)
    {
      fcache->frame_cache_size++;
      fc_ptr = (t_GVA_Frame_Cache_Elem  *)fc_ptr->next;
      if(fc_ptr == NULL)
      {
         printf("API internal ERROR (frame cache is not linked as ring)\n");
         exit(1);
      }
    }

    /* if cache is greater than wanted size drop the oldest (next)
     * elements until we have desired number of elements
     */
    while(fcache->frame_cache_size > frames_to_keep_cahed)
    {
      p_drop_next_frame_cache_elem(fcache);
      fcache->frame_cache_size--;
    }
  }
  else
  {
    /* create the 1st element, ring-linked to itself */
    fc_ptr = p_new_frame_cache_elem(gvahand);
    fc_ptr->prev = fc_ptr;
    fc_ptr->next = fc_ptr;
    fcache->fc_current = fc_ptr;
    gvahand->frame_data = fc_ptr->frame_data;
    gvahand->row_pointers = fc_ptr->row_pointers;
    fcache->frame_cache_size = 1;
  }

  /* if current cache is smaller than requested, add the missing elements */
  while(fcache->frame_cache_size < frames_to_keep_cahed)
  {
    t_GVA_Frame_Cache_Elem  *fc_nxt;

    /* add a new element after the current element in the pointerring
     */
    fc_nxt = (t_GVA_Frame_Cache_Elem  *)fcache->fc_current->next;

    fc_ptr = p_new_frame_cache_elem(gvahand);
    fc_ptr->prev = fcache->fc_current;
    fc_ptr->next = fc_nxt;
    fc_nxt->prev = fc_ptr;
    fcache->fc_current->next = fc_ptr;

    fcache->frame_cache_size++;
  }

  return(fcache->frame_cache_size);

}  /* end p_build_frame_cache */


/* ------------------------------------
 * GVA_set_fcache_size
 * ------------------------------------
 */
void
GVA_set_fcache_size(t_GVA_Handle *gvahand
                 ,gint32 frames_to_keep_cahed
                 )
{
  if(gvahand->fcache.fcache_locked)
  {
    printf("GVA_set_fcache_size: IGNORED "
           "because fcache is locked by running SEEK_FRAME or GET_NEXT FRAME)\n");
    return;  /* dont touch the fcache while locked */
  }
  
  if ((frames_to_keep_cahed > 0)
  &&  (frames_to_keep_cahed <= GVA_MAX_FCACHE_SIZE))
  {
      GVA_fcache_mutex_lock (gvahand);

      /* re-adjust fcache size as desired by calling program */
      p_build_frame_cache(gvahand, frames_to_keep_cahed);

      GVA_fcache_mutex_unlock (gvahand);
  }
  else
  {
    printf("GVA_set_fcache_size: size must be an integer > 0 and <= %d (value %d is ignored)\n"
          , (int)GVA_MAX_FCACHE_SIZE
          , (int)frames_to_keep_cahed);
  }
}  /* end GVA_set_fcache_size */


/* -------------------------------
 * GVA_get_fcache_size_in_elements
 * -------------------------------
 * return internal frame cache allocation size in number of elements (e.g. frames)
 * note that the fcache is already fully allocated at open time, therefore
 * the number of actual cached frames may be smaller than the returned number.
 */
gint32 
GVA_get_fcache_size_in_elements(t_GVA_Handle *gvahand)
{
  t_GVA_Frame_Cache *fcache;
  gint32 frame_cache_size;
  
  fcache = &gvahand->fcache;
  frame_cache_size = fcache->frame_cache_size;
 
  return (frame_cache_size);

}  /* end GVA_get_fcache_size_in_elements */


/* ------------------------------
 * GVA_get_fcache_size_in_bytes
 * ------------------------------
 * return internal frame cache allocation size in bytes
 * note that the fcache is already fully allocated at open time, therefore
 * the returned number is an indicator for memory usage but not for actually cached frames.
 */
gint32 
GVA_get_fcache_size_in_bytes(t_GVA_Handle *gvahand)
{
  int wwidth, wheight, bpp;
  gint32 bytesUsedPerElem;
  gint32 bytesUsedSummary;

  wwidth  = MAX(gvahand->width, 2);
  wheight = MAX(gvahand->height, 2);
  bpp = gvahand->frame_bpp;

  bytesUsedPerElem = (wwidth * wheight * bpp + 4);          /* data size per cache element */
  bytesUsedPerElem += (sizeof(unsigned char*) * wheight);   /* rowpointers per cache element */


  bytesUsedSummary = bytesUsedPerElem * GVA_get_fcache_size_in_elements(gvahand);
  return(bytesUsedSummary);

}  /* end GVA_get_fcache_size_in_bytes */


/* ------------------------------------
 * GVA_search_fcache
 * ------------------------------------
 * search the frame cache for given framenumber
 * and set the pointers
 *  gvahand->fc_frame_data
 *  gvahand->fc_row_pointers
 *
 * to point to the disired frame in the frame cache.
 * please note: if framenumber is not found,
 *              the pointers are set to the current frame
 *
 * RETURN: GVA_RET_OK (0) if OK,
 *         GVA_RET_EOF (1) if framenumber not found in fcache, or fcache LOCKED
 *         GVA_RET_ERROR on other errors
 */
t_GVA_RetCode
GVA_search_fcache(t_GVA_Handle *gvahand
                 ,gint32 framenumber
                 )
{
  t_GVA_Frame_Cache *fcache;
  t_GVA_Frame_Cache_Elem  *fc_ptr;
  static gint32 funcId = -1;
  
  GAP_TIMM_GET_FUNCTION_ID(funcId, "GVA_search_fcache");


  if(gap_debug)
  {
    printf("GVA_search_fcache: search for framenumber: %d\n", (int)framenumber );
  }
  if(gvahand->fcache.fcache_locked)
  {
    return(GVA_RET_EOF);  /* dont touch the fcache while locked */
  }

  GVA_fcache_mutex_lock (gvahand);
  GAP_TIMM_START_FUNCTION(funcId);

  /* init with framedata of current frame
   * (for the case that framenumber not available in fcache)
   */
  gvahand->fc_frame_data = gvahand->frame_data;
  gvahand->fc_row_pointers = gvahand->row_pointers;

  fcache = &gvahand->fcache;
  if(fcache->fc_current)
  {
    fc_ptr = (t_GVA_Frame_Cache_Elem  *)fcache->fc_current;
    while(1 == 1)
    {
      if(framenumber == fc_ptr->framenumber)
      {
        if(fc_ptr->framenumber >= 0)
        {
          gvahand->fc_frame_data = fc_ptr->frame_data;  /* framedata of cached frame */
          gvahand->fc_row_pointers = fc_ptr->row_pointers;


          GAP_TIMM_STOP_FUNCTION(funcId);
          GVA_fcache_mutex_unlock (gvahand);

          return(GVA_RET_OK);  /* OK */
        }
      }

      /* try to get framedata from fcache ringlist,
       * by stepping backwards the frames that were read before
       */
      fc_ptr = (t_GVA_Frame_Cache_Elem  *)fc_ptr->prev;

      if(fcache->fc_current == fc_ptr)
      {
        GAP_TIMM_STOP_FUNCTION(funcId);

        GVA_fcache_mutex_unlock (gvahand);

        return (GVA_RET_EOF);  /* STOP, we are back at startpoint of the ringlist */
      }
      if(fc_ptr == NULL)
      {
        GAP_TIMM_STOP_FUNCTION(funcId);
        GVA_fcache_mutex_unlock (gvahand);

        return (GVA_RET_ERROR);  /* internal error, ringlist is broken */
      }
    }
  }

  GAP_TIMM_STOP_FUNCTION(funcId);
  GVA_fcache_mutex_unlock (gvahand);

  /* ringlist not found */
  return (GVA_RET_ERROR);

}  /* end GVA_search_fcache */


/* ------------------------------------
 * GVA_search_fcache_by_index
 * ------------------------------------
 * search the frame cache backwards by given index
 * where index 0 is the current frame,
 *       index 1 is the prev handled frame
 *       (dont use negative indexes !!)
 * and set the pointers
 *  gvahand->fc_frame_data
 *  gvahand->fc_row_pointers
 *
 * to point to the disired frame in the frame cache.
 * please note: if framenumber is not found,
 *              the pointers are set to the current frame
 *
 * RETURN: GVA_RET_OK (0) if OK,
 *         GVA_RET_EOF (1) if framenumber not found in fcache, or fcache LOCKED
 *         GVA_RET_ERROR on other errors
 */
t_GVA_RetCode
GVA_search_fcache_by_index(t_GVA_Handle *gvahand
                 ,gint32 index
                 ,gint32 *framenumber
                 )
{
  t_GVA_Frame_Cache *fcache;
  t_GVA_Frame_Cache_Elem  *fc_ptr;


  if(gap_debug) printf("GVA_search_fcache_by_index: search for INDEX: %d\n", (int)index );
  if(gvahand->fcache.fcache_locked)
  {
    return(GVA_RET_EOF);  /* dont touch the fcache while locked */
  }

  GVA_fcache_mutex_lock (gvahand);

  /* init with framedata of current frame
   * (for the case that framenumber not available in fcache)
   */
  *framenumber = -1;
  gvahand->fc_frame_data = gvahand->frame_data;
  gvahand->fc_row_pointers = gvahand->row_pointers;

  fcache = &gvahand->fcache;
  if(fcache->fc_current)
  {
    gint32 ii;

    fc_ptr = (t_GVA_Frame_Cache_Elem  *)fcache->fc_current;
    for(ii=0; 1==1; ii++)
    {
      if(index == ii)
      {
        *framenumber = fc_ptr->framenumber;
        if(fc_ptr->framenumber < 0)
        {
          if(gap_debug)
          {
            printf("GVA_search_fcache_by_index: INDEX: %d  NOT FOUND (fnum < 0) ###########\n", (int)index );
          }

          GVA_fcache_mutex_unlock (gvahand);

          return (GVA_RET_EOF);
        }
        gvahand->fc_frame_data = fc_ptr->frame_data;  /* framedata of cached frame */
        gvahand->fc_row_pointers = fc_ptr->row_pointers;

        if(gap_debug)
        {
          printf("GVA_search_fcache_by_index: fnum; %d INDEX: %d  FOUND ;;;;;;;;;;;;;;;;;;\n", (int)*framenumber, (int)index );
        }
        GVA_fcache_mutex_unlock (gvahand);

        return(GVA_RET_OK);  /* OK */
      }

      /* try to get framedata from fcache ringlist,
       * by stepping backwards the frames that were read before
       */
      fc_ptr = (t_GVA_Frame_Cache_Elem  *)fc_ptr->prev;

      if(fcache->fc_current == fc_ptr)
      {
        if(gap_debug)
        {
          printf("GVA_search_fcache_by_index: INDEX: %d  NOT FOUND (ring done) ************\n", (int)index );
        }
        GVA_fcache_mutex_unlock (gvahand);

        return (GVA_RET_EOF);  /* STOP, we are back at startpoint of the ringlist */
      }
      if(fc_ptr == NULL)
      {
        GVA_fcache_mutex_unlock (gvahand);

        return (GVA_RET_ERROR);  /* internal error, ringlist is broken */
      }
    }
  }

  GVA_fcache_mutex_unlock (gvahand);

  /* ringlist not found */
  return (GVA_RET_ERROR);

}  /* end GVA_search_fcache_by_index */



/* ---------------------------------
 * p_copyRgbBufferToPixelRegion
 * ---------------------------------
 * tile based copy from rgbBuffer to PixelRegion 
 */
static inline void
p_copyRgbBufferToPixelRegion (const GimpPixelRgn *dstPR
                    ,const GVA_RgbPixelBuffer *srcBuff)
{
  guint    row;
  guchar*  src;
  guchar*  dest;
   
  dest  = dstPR->data;
  src = srcBuff->data 
       + (dstPR->y * srcBuff->rowstride)
       + (dstPR->x * srcBuff->bpp);

  for (row = 0; row < dstPR->h; row++)
  {
     memcpy(dest, src, dstPR->rowstride);
     src  += srcBuff->rowstride;
     dest += dstPR->rowstride;
  }
  
}  /* end p_copyRgbBufferToPixelRegion */

/* -------------------------------------------------------
 * GVA_search_fcache_and_get_frame_as_gimp_layer_or_rgb888    procedure instrumented for PERFTEST
 * -------------------------------------------------------
 * search the frame cache for given framenumber
 * and deliver result into the specified GVA_fcache_fetch_result struct.
 * The attribute isRgb888Result in the GVA_fcache_fetch_result struct
 * controls the type of result where 
 *    isRgb888Result == TRUE   will deliver a uchar buffer (optional deinterlaced) 
 *                             that is copy of the fcache in RGB888 colormodel
 *
 *    isRgb888Result == FALSE  will deliver a gimp layer
 *                             that is copy of the fcache (optional deinterlaced)
 *                             and is the only layer in a newly created gimp image ( without display attached).
 *
 *
 * fetchResult->isFrameAvailable == FALSE  indicates that the required framenumber
 * was not available in the fcache.
 *
 * This procedure also set fcache internal rowpointers.
 *  gvahand->fc_frame_data
 *  gvahand->fc_row_pointers
 *
 *
 * Notes:
 * o)  this procedure does not set image aspect for performance reasons.
 *     in case aspect is required the calling programm has to perform
 *     the additional call like this:
 *       GVA_search_fcache_and_get_frame_as_gimp_layer(gvahand, ....);
 *       GVA_image_set_aspect(gvahand, gimp_drawable_get_image(fetchResult->layer_id));
 */
void
GVA_search_fcache_and_get_frame_as_gimp_layer_or_rgb888(t_GVA_Handle *gvahand
                 , gint32   framenumber
                 , gint32   deinterlace
                 , gdouble  threshold
                 , gint32   numProcessors
                 , GVA_fcache_fetch_result *fetchResult
                 )
{
  t_GVA_Frame_Cache *fcache;
  t_GVA_Frame_Cache_Elem  *fc_ptr;
  gint32                   l_threshold;
  gint32                   l_mix_threshold;

  static gint32 funcId = -1;
  static gint32 funcIdMemcpy = -1;
  static gint32 funcIdToDrawableRect = -1;
  static gint32 funcIdToDrawableTile = -1;
  static gint32 funcIdDrawableFlush = -1;
  static gint32 funcIdDrawableDetach = -1;
  
  GAP_TIMM_GET_FUNCTION_ID(funcId, "GVA_search_fcache_and_get_frame");
  GAP_TIMM_GET_FUNCTION_ID(funcIdMemcpy, "GVA_search_fcache_and_get_frame.memcpy");
  GAP_TIMM_GET_FUNCTION_ID(funcIdToDrawableRect, "GVA_search_fcache_and_get_frame.toDrawable (rgn_set_rect)");
  GAP_TIMM_GET_FUNCTION_ID(funcIdToDrawableTile, "GVA_search_fcache_and_get_frame.toDrawable (tilebased)");
  GAP_TIMM_GET_FUNCTION_ID(funcIdDrawableFlush, "GVA_search_fcache_and_get_frame.gimp_drawable_flush");
  GAP_TIMM_GET_FUNCTION_ID(funcIdDrawableDetach, "GVA_search_fcache_and_get_frame.gimp_drawable_detach");

  fetchResult->isFrameAvailable = FALSE;
  fetchResult->layer_id = -1;
  fetchResult->image_id = -1;

  if(gap_debug)
  {
    printf("GVA_search_fcache_and_get_frame_as_gimp_layer: search for framenumber: %d\n", (int)framenumber );
  }
  if(gvahand->fcache.fcache_locked)
  {
    return;  /* dont touch the fcache while locked */
  }

  GAP_TIMM_START_FUNCTION(funcId);


  /* expand threshold range from 0.0-1.0  to 0 - MIX_MAX_THRESHOLD */
  threshold = CLAMP(threshold, 0.0, 1.0);
  l_threshold = (gdouble)MIX_MAX_THRESHOLD * (threshold * threshold * threshold);
  l_mix_threshold = CLAMP((gint32)l_threshold, 0, MIX_MAX_THRESHOLD);

  GVA_fcache_mutex_lock (gvahand);


  /* init with framedata of current frame
   * (for the case that framenumber not available in fcache)
   */
  gvahand->fc_frame_data = gvahand->frame_data;
  gvahand->fc_row_pointers = gvahand->row_pointers;

  fcache = &gvahand->fcache;
  if(fcache->fc_current)
  {
    fc_ptr = (t_GVA_Frame_Cache_Elem  *)fcache->fc_current;
    while(1 == 1)
    {
      if(framenumber == fc_ptr->framenumber)
      {
        if(fc_ptr->framenumber >= 0)
        {
          /* FCACHE HIT */
          static gboolean           isPerftestInitialized = FALSE;          
          static gboolean           isPerftestApiTilesDefault;
          static gboolean           isPerftestApiTiles;          /* copy tile-by-tile versus gimp_pixel_rgn_set_rect all at once */
          static gboolean           isPerftestApiMemcpyMP;       /* memcopy versus multithreade memcopy in rowStipres */

          GVA_RgbPixelBuffer  rgbBufferLocal;
          GVA_RgbPixelBuffer *rgbBuffer;
          guchar            *frameData;
          GimpDrawable      *drawable;
          GimpPixelRgn       pixel_rgn;
          gboolean           isEarlyUnlockPossible;
          
          
          gvahand->fc_frame_data = fc_ptr->frame_data;  /* framedata of cached frame */
          gvahand->fc_row_pointers = fc_ptr->row_pointers;
          
          
          if(gvahand->frame_bpp != 3)
          {
            /* force fetch as drawable in case video data is not of type rgb888
             */
            fetchResult->isRgb888Result = FALSE;
          }
          
          if (fetchResult->isRgb888Result == TRUE)
          {
            rgbBuffer = &fetchResult->rgbBuffer;
          }
          else
          {
            /* in case fetch result is gimp layer, use a local buffer for delace purpose */
            rgbBuffer = &rgbBufferLocal;
            rgbBuffer->data = NULL;
          }
          
          rgbBuffer->width = gvahand->width;
          rgbBuffer->height = gvahand->height;
          rgbBuffer->bpp = gvahand->frame_bpp;
          rgbBuffer->rowstride = gvahand->width * gvahand->frame_bpp;     /* bytes per pixel row */
          rgbBuffer->deinterlace = deinterlace;
          rgbBuffer->threshold = threshold;
          frameData = NULL;
          isEarlyUnlockPossible = TRUE;
          
          /* PERFTEST configuration values to test performance of various strategies on multiprocessor machines */
          if (isPerftestInitialized != TRUE)
          {
            isPerftestInitialized = TRUE;

            if(numProcessors > 1)
            {
              /* copy full size as one big rectangle gives the gimp core the chance to process with more than one thread */
              isPerftestApiTilesDefault = FALSE;
            }
            else
            {
              /* tile based copy was a little bit faster in tests where gimp-core used only one CPU */
              isPerftestApiTilesDefault = TRUE;
            }
            isPerftestApiTiles = gap_base_get_gimprc_gboolean_value("isPerftestApiTiles", isPerftestApiTilesDefault);
            isPerftestApiMemcpyMP = gap_base_get_gimprc_gboolean_value("isPerftestApiMemcpyMP", TRUE);
          }
          
          
          if (deinterlace != 0)
          {
            if(rgbBuffer->data == NULL)
            {
              rgbBuffer->data = g_malloc(rgbBuffer->rowstride * rgbBuffer->height);
              if(fetchResult->isRgb888Result != TRUE)
              {
                frameData = rgbBuffer->data;  /* frameData will be freed after convert to drawable */
              }
            }
            GVA_copy_or_deinterlace_fcache_data_to_rgbBuffer(rgbBuffer
                                                          , gvahand->fc_frame_data
                                                          , numProcessors
                                                          );
          }
          else
          {
            if (fetchResult->isRgb888Result == TRUE)
            {
              /* it is required to make an 1:1 copy of the rgb888 fcache data 
               * to the rgbBuffer.
               * allocate the buffer in case the caller has supplied just a NULL data pointer.
               * otherwise use the supplied buffer.
               */
              if(rgbBuffer->data == NULL)
              {
                rgbBuffer->data = g_malloc(rgbBuffer->rowstride * rgbBuffer->height);
              }

              if(isPerftestApiMemcpyMP)
              {
                GVA_copy_or_deinterlace_fcache_data_to_rgbBuffer(rgbBuffer
                                                          , gvahand->fc_frame_data
                                                          , numProcessors
                                                          );
              }
              else
              {
                GAP_TIMM_START_FUNCTION(funcIdMemcpy);
                
                memcpy(rgbBuffer->data, gvahand->fc_frame_data, (rgbBuffer->rowstride * rgbBuffer->height));
                
                GAP_TIMM_STOP_FUNCTION(funcIdMemcpy);
              }
            }
            else
            {
              /* setup rgbBuffer->data to point direct to the fcache frame data
               * No additional frameData buffer is allocated in this case and no extra memcpy is required,
               * but the fcache mutex must stay in locked state until data is completely transfered
               * to the drawable.
               */
              rgbBuffer->data = gvahand->fc_frame_data;
              isEarlyUnlockPossible = FALSE;
            }
          }
          
          
          
          if(isEarlyUnlockPossible == TRUE)
          {
           /* at this point the frame data is already copied to the
            * rgbBuffer or there is no need to convert to drawable at all.
            * therefore we can already unlock the mutex
            * so that other threads already can continue using the fcache
            */
            GVA_fcache_mutex_unlock (gvahand);
          }
          
          if (fetchResult->isRgb888Result == TRUE)
          {
            fetchResult->isFrameAvailable = TRUE; /* OK frame available in fcache and was copied to rgbBuffer */
            GAP_TIMM_STOP_FUNCTION(funcId);
            return;
          }
          
          fetchResult->image_id = gimp_image_new (rgbBuffer->width, rgbBuffer->height, GIMP_RGB);
          if (gimp_image_undo_is_enabled(fetchResult->image_id))
          {
            gimp_image_undo_disable(fetchResult->image_id);
          }
          
          if(rgbBuffer->bpp == 4)
          {
            fetchResult->layer_id = gimp_layer_new (fetchResult->image_id
                                            , "layername"
                                            , rgbBuffer->width
                                            , rgbBuffer->height
                                            , GIMP_RGBA_IMAGE
                                            , 100.0, GIMP_NORMAL_MODE);
          }
          else
          {
            fetchResult->layer_id = gimp_layer_new (fetchResult->image_id
                                            , "layername"
                                            , rgbBuffer->width
                                            , rgbBuffer->height
                                            , GIMP_RGB_IMAGE
                                            , 100.0, GIMP_NORMAL_MODE);
          }

          drawable = gimp_drawable_get (fetchResult->layer_id);
          
          
          if(isPerftestApiTiles)
          {
            gpointer pr;
            GAP_TIMM_START_FUNCTION(funcIdToDrawableTile);

            gimp_pixel_rgn_init (&pixel_rgn, drawable, 0, 0
                           , drawable->width, drawable->height
                           , TRUE      /* dirty */
                           , FALSE     /* shadow */
                           );

            for (pr = gimp_pixel_rgns_register (1, &pixel_rgn);
                 pr != NULL;
                 pr = gimp_pixel_rgns_process (pr))
            {
              p_copyRgbBufferToPixelRegion (&pixel_rgn, rgbBuffer);
            }

            GAP_TIMM_STOP_FUNCTION(funcIdToDrawableTile);
          }
          else
          {
            GAP_TIMM_START_FUNCTION(funcIdToDrawableRect);

            gimp_pixel_rgn_init (&pixel_rgn, drawable, 0, 0
                           , drawable->width, drawable->height
                           , TRUE      /* dirty */
                           , FALSE     /* shadow */
                           );
            gimp_pixel_rgn_set_rect (&pixel_rgn, rgbBuffer->data
                           , 0
                           , 0
                           , drawable->width
                           , drawable->height
                           );

            GAP_TIMM_STOP_FUNCTION(funcIdToDrawableRect);
          }
          
          if(isEarlyUnlockPossible != TRUE)
          {
            /* isEarlyUnlockPossible == FALSE indicates the case where the image was directly filled from fcache
             * in this scenario the mutex must be unlocked at this later time 
             * after the fcache data is already transfered to the drawable
             */
             GVA_fcache_mutex_unlock (gvahand);
          }


          GAP_TIMM_START_FUNCTION(funcIdDrawableFlush);
          gimp_drawable_flush (drawable);
          GAP_TIMM_STOP_FUNCTION(funcIdDrawableFlush);

          GAP_TIMM_START_FUNCTION(funcIdDrawableDetach);
          gimp_drawable_detach(drawable);
          GAP_TIMM_STOP_FUNCTION(funcIdDrawableDetach);

          /*
           * gimp_drawable_merge_shadow (drawable->id, TRUE);
           */

          /* add new layer on top of the layerstack */
          gimp_image_insert_layer (fetchResult->image_id, fetchResult->layer_id, 0, 0);
          gimp_drawable_set_visible(fetchResult->layer_id, TRUE);

          /* clear undo stack */
          if (gimp_image_undo_is_enabled(fetchResult->image_id))
          {
            gimp_image_undo_disable(fetchResult->image_id);
          }

          if(frameData != NULL)
          {
            g_free(frameData);
            frameData = NULL;
          }

          fetchResult->isFrameAvailable = TRUE; /* OK  frame available in fcache and was converted to drawable */
          GAP_TIMM_STOP_FUNCTION(funcId);
          return;
        }
      }

      /* try to get framedata from fcache ringlist,
       * by stepping backwards the frames that were read before
       */
      fc_ptr = (t_GVA_Frame_Cache_Elem  *)fc_ptr->prev;

      if(fcache->fc_current == fc_ptr)
      {
        break;  /* STOP, we are back at startpoint of the ringlist */
      }
      if(fc_ptr == NULL)
      {
        printf("** ERROR in GVA_search_fcache_and_get_frame_as_gimp_layer ringlist broken \n");
        break;  /* internal error, ringlist is broken */
      }
    }
  }

  GVA_fcache_mutex_unlock (gvahand);
  GAP_TIMM_STOP_FUNCTION(funcId);


}  /* end GVA_search_fcache_and_get_frame_as_gimp_layer_or_rgb888 */


// /* ---------------------------------------------
//  * GVA_search_fcache_and_get_frame_as_gimp_layer    procedure instrumented for PERFTEST
//  * ---------------------------------------------
//  * search the frame cache for given framenumber
//  * and return a copy (or deinterlaced copy) as gimp layer (in a newly created image)
//  * in case framenumber was NOT found in the fcache return -1
//  *
//  * This procedure also set fcache internal rowpointers.
//  *  gvahand->fc_frame_data
//  *  gvahand->fc_row_pointers
//  *
//  *
//  * RETURN: layerId (a positive integer)
//  *         -1 if framenumber not found in fcache, or errors occured
//  *
//  * Notes:
//  * o)  this procedure does not set image aspect for performance reasons.
//  *     in case aspect is required the calling programm has to perform
//  *     the additional call like this:
//  *       layer_id = GVA_search_fcache_and_get_frame_as_gimp_layer(gvahand, ....);
//  *       GVA_image_set_aspect(gvahand, gimp_drawable_get_image(layer_id));
//  */
// gint32
// GVA_search_fcache_and_get_frame_as_gimp_layer(t_GVA_Handle *gvahand
//                  , gint32 framenumber
//                  , gint32   deinterlace
//                  , gdouble  threshold
//                  , gint32   numProcessors
//                  )
// {
//   t_GVA_Frame_Cache *fcache;
//   t_GVA_Frame_Cache_Elem  *fc_ptr;
//   gint32                   l_threshold;
//   gint32                   l_mix_threshold;
//   gint32                   l_new_layer_id;
// 
//   static gint32 funcId = -1;
//   static gint32 funcIdMemcpy = -1;
//   static gint32 funcIdToDrawableRect = -1;
//   static gint32 funcIdToDrawableTile = -1;
//   static gint32 funcIdDrawableFlush = -1;
//   static gint32 funcIdDrawableDetach = -1;
//   
//   GAP_TIMM_GET_FUNCTION_ID(funcId, "GVA_search_fcache_and_get_frame");
//   GAP_TIMM_GET_FUNCTION_ID(funcIdMemcpy, "GVA_search_fcache_and_get_frame.memcpy");
//   GAP_TIMM_GET_FUNCTION_ID(funcIdToDrawableRect, "GVA_search_fcache_and_get_frame.toDrawable (rgn_set_rect)");
//   GAP_TIMM_GET_FUNCTION_ID(funcIdToDrawableTile, "GVA_search_fcache_and_get_frame.toDrawable (tilebased)");
//   GAP_TIMM_GET_FUNCTION_ID(funcIdDrawableFlush, "GVA_search_fcache_and_get_frame.gimp_drawable_flush");
//   GAP_TIMM_GET_FUNCTION_ID(funcIdDrawableDetach, "GVA_search_fcache_and_get_frame.gimp_drawable_detach");
// 
//   l_new_layer_id = -1;
// 
//   if(gap_debug)
//   {
//     printf("GVA_search_fcache_and_get_frame_as_gimp_layer: search for framenumber: %d\n", (int)framenumber );
//   }
//   if(gvahand->fcache.fcache_locked)
//   {
//     return(l_new_layer_id);  /* dont touch the fcache while locked */
//   }
// 
//   GAP_TIMM_START_FUNCTION(funcId);
// 
// 
//   /* expand threshold range from 0.0-1.0  to 0 - MIX_MAX_THRESHOLD */
//   threshold = CLAMP(threshold, 0.0, 1.0);
//   l_threshold = (gdouble)MIX_MAX_THRESHOLD * (threshold * threshold * threshold);
//   l_mix_threshold = CLAMP((gint32)l_threshold, 0, MIX_MAX_THRESHOLD);
// 
//   GVA_fcache_mutex_lock (gvahand);
// 
// 
//   /* init with framedata of current frame
//    * (for the case that framenumber not available in fcache)
//    */
//   gvahand->fc_frame_data = gvahand->frame_data;
//   gvahand->fc_row_pointers = gvahand->row_pointers;
// 
//   fcache = &gvahand->fcache;
//   if(fcache->fc_current)
//   {
//     fc_ptr = (t_GVA_Frame_Cache_Elem  *)fcache->fc_current;
//     while(1 == 1)
//     {
//       if(framenumber == fc_ptr->framenumber)
//       {
//         if(fc_ptr->framenumber >= 0)
//         {
//           /* FCACHE HIT */
//           static gboolean           isPerftestInitialized = FALSE;          
//           static gboolean           isPerftestApiTilesDefault;
//           static gboolean           isPerftestApiTiles;          /* copy tile-by-tile versus gimp_pixel_rgn_set_rect all at once */
//           static gboolean           isPerftestApiMemcpyMP;       /* memcopy versus multithreade memcopy in rowStipres */
//           static gboolean           isPerftestEarlyFcacheUnlock; /* TRUE: 1:1 copy the fcache to frameData buffer even if delace not required 
//                                                            *       to enable early unlock of the fcache mutex.
//                                                            *       (prefetch thread in the storyboard processor
//                                                            *        may benefit from the early unlock
//                                                            *        and have a chance for better overall performance)
//                                                            */
// 
//           GVA_RgbPixelBuffer rgbBuffer;
//           guchar            *frameData;
//           gint32             image_id;
//           GimpDrawable      *drawable;
//           GimpPixelRgn       pixel_rgn;
//           
//           
//           gvahand->fc_frame_data = fc_ptr->frame_data;  /* framedata of cached frame */
//           gvahand->fc_row_pointers = fc_ptr->row_pointers;
//           
//           rgbBuffer.width = gvahand->width;
//           rgbBuffer.height = gvahand->height;
//           rgbBuffer.bpp = gvahand->frame_bpp;
//           rgbBuffer.rowstride = gvahand->width * gvahand->frame_bpp;     /* bytes per pixel row */
//           rgbBuffer.deinterlace = deinterlace;
//           rgbBuffer.threshold = threshold;
//           frameData = NULL;
//           
//           /* PERFTEST configuration values to test performance of various strategies on multiprocessor machines */
//           if (isPerftestInitialized != TRUE)
//           {
//             isPerftestInitialized = TRUE;
// 
//             if(numProcessors > 1)
//             {
//               /* copy full size as one big rectangle gives the gimp core the chance to process with more than one thread */
//               isPerftestApiTilesDefault = FALSE;
//             }
//             else
//             {
//               /* tile based copy was a little bit faster in tests where gimp-core used only one CPU */
//               isPerftestApiTilesDefault = TRUE;
//             }
//             isPerftestApiTiles = gap_base_get_gimprc_gboolean_value("isPerftestApiTiles", isPerftestApiTilesDefault);
//             isPerftestApiMemcpyMP = gap_base_get_gimprc_gboolean_value("isPerftestApiMemcpyMP", TRUE);
//             isPerftestEarlyFcacheUnlock = gap_base_get_gimprc_gboolean_value("isPerftestEarlyFcacheUnlock", FALSE);
//           }
//           
//           if (deinterlace != 0)
//           {
//             frameData = g_malloc(rgbBuffer.rowstride * rgbBuffer.height);
//             rgbBuffer.data = frameData;
//             GVA_copy_or_deinterlace_fcache_data_to_rgbBuffer(&rgbBuffer
//                                                           , gvahand->fc_frame_data
//                                                           , numProcessors
//                                                           );
//           }
//           else
//           {
//             if (isPerftestEarlyFcacheUnlock)
//             {
//               /* for early unlock startegy it is required to make a copy of the fcache data */
// 
//               frameData = g_malloc(rgbBuffer.rowstride * rgbBuffer.height);
//               rgbBuffer.data = frameData;
//               if(isPerftestApiMemcpyMP)
//               {
//                 GVA_copy_or_deinterlace_fcache_data_to_rgbBuffer(&rgbBuffer
//                                                           , gvahand->fc_frame_data
//                                                           , numProcessors
//                                                           );
//               }
//               else
//               {
//                 GAP_TIMM_START_FUNCTION(funcIdMemcpy);
//                 
//                 memcpy(rgbBuffer.data, gvahand->fc_frame_data, (rgbBuffer.rowstride * rgbBuffer.height));
//                 
//                 GAP_TIMM_STOP_FUNCTION(funcIdMemcpy);
//               }
//             }
//             else
//             {
//               /* setup rgbBuffer.data to point direct to the fcache frame data
//                * No additional frameData buffer is allocated in this case and no extra memcpy is required,
//                * but the fcache mutex must stay in locked state until data is completely transfered
//                * to the drawable. This can have performance impact on parallel running prefetch threads
//                */
//               rgbBuffer.data = gvahand->fc_frame_data;
//             }
//           }
//           
//           
//           
//           if(frameData != TRUE)
//           {
//            /* at this point the frame data is already copied to newly allocated
//             * frameData buffer (that is owned exclusive by the current thread)
//             * therefore we can already unlock the mutex
//             * so that other threads already can continue using the fcache
//             */
//             GVA_fcache_mutex_unlock (gvahand);
//           }
//           
//          
//           image_id = gimp_image_new (rgbBuffer.width, rgbBuffer.height, GIMP_RGB);
//           if (gimp_image_undo_is_enabled(image_id))
//           {
//             gimp_image_undo_disable(image_id);
//           }
//           
//           if(rgbBuffer.bpp == 4)
//           {
//             l_new_layer_id = gimp_layer_new (image_id
//                                               , "layername"
//                                               , rgbBuffer.width
//                                               , rgbBuffer.height
//                                               , GIMP_RGBA_IMAGE
//                                               , 100.0, GIMP_NORMAL_MODE);
//           }
//           else
//           {
//             l_new_layer_id = gimp_layer_new (image_id
//                                               , "layername"
//                                               , rgbBuffer.width
//                                               , rgbBuffer.height
//                                               , GIMP_RGB_IMAGE
//                                               , 100.0, GIMP_NORMAL_MODE);
//           }
// 
// 
//           
//           drawable = gimp_drawable_get (l_new_layer_id);
//           
//           
//           if(isPerftestApiTiles)
//           {
//             gpointer pr;
//             GAP_TIMM_START_FUNCTION(funcIdToDrawableTile);
// 
//             gimp_pixel_rgn_init (&pixel_rgn, drawable, 0, 0
//                              , drawable->width, drawable->height
//                              , TRUE      /* dirty */
//                              , FALSE     /* shadow */
//                              );
// 
//             for (pr = gimp_pixel_rgns_register (1, &pixel_rgn);
//                  pr != NULL;
//                  pr = gimp_pixel_rgns_process (pr))
//             {
//               p_copyRgbBufferToPixelRegion (&pixel_rgn, &rgbBuffer);
//             }
// 
//             GAP_TIMM_STOP_FUNCTION(funcIdToDrawableTile);
//           }
//           else
//           {
//             GAP_TIMM_START_FUNCTION(funcIdToDrawableRect);
// 
//             gimp_pixel_rgn_init (&pixel_rgn, drawable, 0, 0
//                              , drawable->width, drawable->height
//                              , TRUE      /* dirty */
//                              , FALSE     /* shadow */
//                              );
//             gimp_pixel_rgn_set_rect (&pixel_rgn, rgbBuffer.data
//                              , 0
//                              , 0
//                              , drawable->width
//                              , drawable->height
//                              );
// 
//             GAP_TIMM_STOP_FUNCTION(funcIdToDrawableRect);
//           }
//           
//           if(frameData == NULL)
//           {
//             /* frameData == NULL indicates the case where the image was directly filled from fcache
//              * in this scenario the mutex must be unlocked at this later time 
//              * after the fcache data is already transfered to the drawable
//              */
//              GVA_fcache_mutex_unlock (gvahand);
//           }
// 
// 
//           GAP_TIMM_START_FUNCTION(funcIdDrawableFlush);
//           gimp_drawable_flush (drawable);
//           GAP_TIMM_STOP_FUNCTION(funcIdDrawableFlush);
// 
//           GAP_TIMM_START_FUNCTION(funcIdDrawableDetach);
//           gimp_drawable_detach(drawable);
//           GAP_TIMM_STOP_FUNCTION(funcIdDrawableDetach);
// 
//           /*
//            * gimp_drawable_merge_shadow (drawable->id, TRUE);
//            */
// 
//           /* add new layer on top of the layerstack */
//           gimp_image_insert_layer (image_id, l_new_layer_id, 0, 0);
//           gimp_drawable_set_visible(l_new_layer_id, TRUE);
// 
//           /* clear undo stack */
//           if (gimp_image_undo_is_enabled(image_id))
//           {
//             gimp_image_undo_disable(image_id);
//           }
// 
//           if(frameData != NULL)
//           {
//             g_free(frameData);
//             frameData = NULL;
//           }
// 
//           GAP_TIMM_STOP_FUNCTION(funcId);
//           return (l_new_layer_id); /* OK  frame available in fcache */
//         }
//       }
// 
//       /* try to get framedata from fcache ringlist,
//        * by stepping backwards the frames that were read before
//        */
//       fc_ptr = (t_GVA_Frame_Cache_Elem  *)fc_ptr->prev;
// 
//       if(fcache->fc_current == fc_ptr)
//       {
//         break;  /* STOP, we are back at startpoint of the ringlist */
//       }
//       if(fc_ptr == NULL)
//       {
//         printf("** ERROR in GVA_search_fcache_and_get_frame_as_gimp_layer ringlist broken \n");
//         break;  /* internal error, ringlist is broken */
//       }
//     }
//   }
// 
//   GVA_fcache_mutex_unlock (gvahand);
//   GAP_TIMM_STOP_FUNCTION(funcId);
// 
//   return (l_new_layer_id);
// 
// }  /* end GVA_search_fcache_and_get_frame_as_gimp_layer */


/* --------------------
 * p_guess_total_frames
 * --------------------
 */
static gdouble
p_guess_total_frames(t_GVA_Handle *gvahand)
{
  gdouble l_guess;
  gdouble l_guess_framesize;
  gint    l_len;

  if(gap_debug) printf ("p_guess_total_frames START\n");
  
  if(gvahand->filename == NULL)
  {
    if(gap_debug) printf ("p_guess_total_frames gvahand->filename: IS NULL\n");
    return(1);
  }

  if(gap_debug) printf ("p_guess_total_frames gvahand->filename:%s\n", gvahand->filename);
  l_len = strlen(gvahand->filename);
  if(l_len > 4)
  {
    const char *suffix;
    
    suffix = gvahand->filename + (l_len - 4);
    if((strcmp(suffix, ".ifo") == 0)
    || (strcmp(suffix, ".IFO") == 0))
    {
      /* for .ifo files it is not a good idea to guess the number
       * of frames from the filesize.
       * .ifo are typically used on DVD to referre to a set of .vob files
       * that makes up the whole movie.
       * without parsing the .ifo file we can not guess the number of
       * total frames of the whole movie.
       * This primitive workaround assumes a constant number of frames
       */
      if(gap_debug) printf ("p_guess_total_frames .ifo using fix total_frames 30000\n");
      return (30000.0);
    }
  }

  /* very unprecise guess total frames by checking the filesize
   * and assume compression 1/50 per compressed frame.
   * - compression rate 1/40 is a common value, but may differ heavily depending
   *   depending on the used codec and its quality settings.
   * - audiotracks are ignored for this guess
   * - we need a guess to update the percentage_done used for progress indication)
   */
  l_guess_framesize = (gvahand->width * gvahand->height * 3) / 50.0;
  l_guess = (gdouble)p_get_filesize(gvahand->filename) / (l_guess_framesize * MAX(gvahand->vtracks, 1.0)) ;

  if(gap_debug) printf ("p_guess_total_frames GUESS total_frames: %d\n", (int)l_guess);
  return(l_guess);

}  /* end p_guess_total_frames */

/* --------------------------------------------------------------------------------------
 * --------------------------------------- FILE(S) gap_api_vid_DECODERTYPE.c Starts here
 * --------------------------------------------------------------------------------------
 */



/* ----------------------------------------------
 * WRAPPER PROCEDURES for built in Video Decoders
 * ----------------------------------------------
 */


/* ================================================ FFMPEG
 * FFMPEG (libavformat libavcodec)                  FFMPEG
 * ================================================ FFMPEG
 * ================================================ FFMPEG
 */
#ifdef ENABLE_GVA_LIBAVFORMAT
#include "avformat.h"
#include "gap_vid_api_ffmpeg.c"
#endif  /* ENABLE_GVA_LIBAVFORMAT */

/* ================================================ GIMP-GAP
 * gimp (singleframe access via gap/gimp)           GIMP-GAP
 * ================================================ GIMP-GAP
 * ================================================ GIMP-GAP
 */
#ifdef ENABLE_GVA_GIMP
#include "gap_vid_api_gimp.c"
#endif  /* ENABLE_GVA_GIMP */

/* ================================================ QUICKTIME
 * quicktime                                        QUICKTIME
 * ================================================ QUICKTIME
 * ================================================ QUICKTIME
 */
#ifdef ENABLE_GVA_LIBQUICKTIME
#include "gap_vid_api_quicktime.c"
#endif  /* ENABLE_GVA_LIBQUICKTIME */



/* ================================================  LIBMEG3
 * libmpeg3                                          LIBMEG3
 * ================================================  LIBMEG3
 * ================================================  LIBMEG3
 */
 
#ifdef ENABLE_GVA_LIBMPEG3
#include "gap_vid_api_mpeg3toc.c"
#include "gap_vid_api_mpeg3.c"
#endif   /* ENABLE_GVA_LIBMPEG3 */





/* ###############################################################################
 * ###############################################################################
 * ###############################################################################
 * ###############################################################################
 *
 *
 * --------------------------------------------------------------------------------------
 * ---------------------------- API.c starts here
 * --------------------------------------------------------------------------------------
 */


/* ---------------------------
 * p_register_all_decoders
 * ---------------------------
 * build the GVA_global_decoder_list
 * with one element for each available decoder.
 *
 * in case the caller does not specifiy a prefered_decoder
 * the order of registered decoders is relevant, because
 * the 1st one that can handle the video is picked at open.
 *
 * This is typical the last one that is added to begin of the list
 * (currently ffmpeg)
 */
static void
p_register_all_decoders(void)
{
  t_GVA_DecoderElem  *dec_elem;

  /* register all internal decoders */
  GVA_global_decoder_list = NULL;

#ifdef ENABLE_GVA_GIMP
  dec_elem = p_gimp_new_dec_elem();
  if(dec_elem)
  {
     dec_elem->next = GVA_global_decoder_list;
     GVA_global_decoder_list = dec_elem;
  }
#endif

#ifdef ENABLE_GVA_LIBQUICKTIME
  dec_elem = p_quicktime_new_dec_elem();
  if(dec_elem)
  {
     dec_elem->next = GVA_global_decoder_list;
     GVA_global_decoder_list = dec_elem;
  }
#endif

#ifdef ENABLE_GVA_LIBMPEG3
  dec_elem = p_mpeg3_new_dec_elem();
  if(dec_elem)
  {
     dec_elem->next = GVA_global_decoder_list;
     GVA_global_decoder_list = dec_elem;
  }
#endif

#ifdef ENABLE_GVA_LIBAVFORMAT
  p_ffmpeg_init();
  dec_elem = p_ffmpeg_new_dec_elem();
  if(dec_elem)
  {
     dec_elem->next = GVA_global_decoder_list;
     GVA_global_decoder_list = dec_elem;
  }
#endif

}  /* end p_register_all_decoders */

/* ---------------------------
 * p_gva_worker_xxx Procedures
 * ---------------------------
 *
 * call decoder specific methodes
 *  for
 *   - close a videofile.
 *   - seek position of a frame
 *   - get frame (into pre allocated frame_data) and advance pos to next frame
 *
 */

/* --------------------------
 * p_gva_worker_close
 * --------------------------
 */
static void
p_gva_worker_close(t_GVA_Handle  *gvahand)
{
  if(gvahand)
  {
    t_GVA_DecoderElem *dec_elem;




    dec_elem = (t_GVA_DecoderElem *)gvahand->dec_elem;

    if(dec_elem)
    {
      char *nameMutexLockStats;
      
      if(gap_debug)
      {
        printf("GVA: gvahand:%d p_gva_worker_close: before CLOSE %s with decoder:%s\n"
           , (int)gvahand
           , gvahand->filename
           , dec_elem->decoder_name
           );
      }

      /* log mutex wait statistics (only in case compiled with runtime recording) */
      nameMutexLockStats = g_strdup_printf("... close gvahand:%d fcacheMutexLockStatistic ", (int)gvahand);
      GVA_copy_or_delace_print_statistics();
      GAP_TIMM_PRINT_RECORD(&gvahand->fcacheMutexLockStats, nameMutexLockStats);
      g_free(nameMutexLockStats);
      
      (*dec_elem->fptr_close)(gvahand);

      if(gap_debug)
      {
        printf("GVA: gvahand:%d p_gva_worker_close: after CLOSE %s with decoder:%s\n"
           , (int)gvahand
           , gvahand->filename
           , dec_elem->decoder_name
           );
      }

      if(gvahand->filename)
      {
         g_free(gvahand->filename);
         gvahand->filename = NULL;
      }

      /* free image buffer and row_pointers */
      p_drop_frame_cache(gvahand);
    }
  }

  if(gap_debug) printf("GVA: p_gva_worker_close: END\n");
}  /* end p_gva_worker_close */


/* ---------------------------
 * p_gva_worker_get_next_frame
 * ---------------------------
 */
static t_GVA_RetCode
p_gva_worker_get_next_frame(t_GVA_Handle  *gvahand)
{
  t_GVA_Frame_Cache *fcache;
  t_GVA_RetCode l_rc;

  l_rc = GVA_RET_ERROR;
  if(gvahand)
  {
    t_GVA_DecoderElem *dec_elem;

    dec_elem = (t_GVA_DecoderElem *)gvahand->dec_elem;

    if(dec_elem)
    {
      if(dec_elem->fptr_get_next_frame == NULL)
      {
         printf("p_gva_worker_get_next_frame: Method not implemented in decoder %s\n", dec_elem->decoder_name);
         return(GVA_RET_ERROR);
      }

      fcache = &gvahand->fcache;
      GVA_fcache_mutex_lock (gvahand);
      fcache->fcache_locked = TRUE;
      if(fcache->fc_current)
      {
        t_GVA_Frame_Cache_Elem *fc_current;

        /* if fcache framenumber is negative, we can reuse
         * that EMPTY element without advance
         */
        if(fcache->fc_current->framenumber >= 0)
        {
          /* advance current write position to next element in the fcache ringlist */
          fcache->fc_current = fcache->fc_current->next;
          fcache->fc_current->framenumber = -1;
          gvahand->frame_data = fcache->fc_current->frame_data;
          gvahand->row_pointers = fcache->fc_current->row_pointers;
        }
        
        fc_current = fcache->fc_current;

        GVA_fcache_mutex_unlock (gvahand);

        /* CALL decoder specific implementation of GET_NEXT_FRAME procedure */
        l_rc = (*dec_elem->fptr_get_next_frame)(gvahand);

        GVA_fcache_mutex_lock (gvahand);

        if (l_rc == GVA_RET_OK)
        {
          fc_current->framenumber = gvahand->current_frame_nr;
        }
      }
      fcache->fcache_locked = FALSE;
      GVA_fcache_mutex_unlock (gvahand);
    }
  }
  return(l_rc);
}  /* end p_gva_worker_get_next_frame */


/* --------------------------
 * p_gva_worker_seek_frame
 * --------------------------
 */
static t_GVA_RetCode
p_gva_worker_seek_frame(t_GVA_Handle  *gvahand, gdouble pos, t_GVA_PosUnit pos_unit)
{
  t_GVA_Frame_Cache *fcache;
  t_GVA_RetCode l_rc;

  l_rc = GVA_RET_ERROR;
  if(gvahand)
  {
    t_GVA_DecoderElem *dec_elem;

    dec_elem = (t_GVA_DecoderElem *)gvahand->dec_elem;

    if(dec_elem)
    {
      if(dec_elem->fptr_seek_frame == NULL)
      {
         printf("p_gva_worker_seek_frame: Method not implemented in decoder %s\n", dec_elem->decoder_name);
         return(GVA_RET_ERROR);
      }
      fcache = &gvahand->fcache;
      fcache->fcache_locked = TRUE;

      GVA_fcache_mutex_lock (gvahand);

      if(fcache->fc_current)
      {
        /* if fcache framenumber is negative, we can reuse
         * that EMPTY element without advance
         */
        if(fcache->fc_current->framenumber >= 0)
        {
          /* advance current write position to next element in the fcache ringlist
           * Some of the seek procedure implementations do dummy reads
           * therefore we provide a fcache element, but leave the
           * framenumber -1 because this element is invalid in most cases
           */
          fcache->fc_current = fcache->fc_current->next;
          fcache->fc_current->framenumber = -1;
          gvahand->frame_data = fcache->fc_current->frame_data;
          gvahand->row_pointers = fcache->fc_current->row_pointers;
        }
      }

      GVA_fcache_mutex_unlock (gvahand);
        
      /* CALL decoder specific implementation of SEEK_FRAME procedure */
      l_rc = (*dec_elem->fptr_seek_frame)(gvahand, pos, pos_unit);
      fcache->fcache_locked = FALSE;
    }
  }
  return(l_rc);
}  /* end p_gva_worker_seek_frame */


/* --------------------------
 * p_gva_worker_seek_audio
 * --------------------------
 */
static t_GVA_RetCode
p_gva_worker_seek_audio(t_GVA_Handle  *gvahand, gdouble pos, t_GVA_PosUnit pos_unit)
{
  t_GVA_RetCode l_rc;

  l_rc = GVA_RET_ERROR;
  if(gvahand)
  {
    t_GVA_DecoderElem *dec_elem;

    dec_elem = (t_GVA_DecoderElem *)gvahand->dec_elem;

    if(dec_elem)
    {
      if(dec_elem->fptr_seek_audio == NULL)
      {
         printf("p_gva_worker_seek_audio: Method not implemented in decoder %s\n", dec_elem->decoder_name);
         return(GVA_RET_ERROR);
      }
      l_rc = (*dec_elem->fptr_seek_audio)(gvahand, pos, pos_unit);
    }
  }
  return(l_rc);
}  /* end p_gva_worker_seek_audio */


/* --------------------------
 * p_gva_worker_get_audio
 * --------------------------
 */
static t_GVA_RetCode
p_gva_worker_get_audio(t_GVA_Handle  *gvahand
             ,gint16 *output_i            /* preallocated buffer large enough for samples * siezeof gint16 */
             ,gint32 channel              /* audiochannel 1 upto n */
             ,gdouble samples             /* number of samples to read */
             ,t_GVA_AudPos mode_flag      /* specify the position where to start reading audio from */
             )
{
  t_GVA_RetCode l_rc;

  l_rc = GVA_RET_ERROR;
  if(gvahand)
  {
    t_GVA_DecoderElem *dec_elem;

    dec_elem = (t_GVA_DecoderElem *)gvahand->dec_elem;

    if(dec_elem)
    {
      if(dec_elem->fptr_get_audio == NULL)
      {
         printf("p_gva_worker_get_audio: Method not implemented in decoder %s\n", dec_elem->decoder_name);
         return(GVA_RET_ERROR);
      }
      l_rc = (*dec_elem->fptr_get_audio)(gvahand
                                 ,output_i
                                 ,channel
                                 ,samples
                                 ,mode_flag
                                 );
    }
  }
  return(l_rc);
}  /* end p_gva_worker_get_audio */


/* --------------------------
 * p_gva_worker_count_frames
 * --------------------------
 */
static t_GVA_RetCode
p_gva_worker_count_frames(t_GVA_Handle  *gvahand)
{
  t_GVA_RetCode l_rc;

  l_rc = GVA_RET_ERROR;
  if(gvahand)
  {
    t_GVA_DecoderElem *dec_elem;

    dec_elem = (t_GVA_DecoderElem *)gvahand->dec_elem;

    if(dec_elem)
    {
      if(dec_elem->fptr_count_frames == NULL)
      {
         printf("p_gva_worker_count_frames: Method not implemented in decoder %s\n", dec_elem->decoder_name);
         return(GVA_RET_ERROR);
      }
      l_rc = (*dec_elem->fptr_count_frames)(gvahand);
    }
  }
  return(l_rc);
}  /* end p_gva_worker_count_frames */


/* -------------------------------
 * p_gva_worker_check_seek_support
 * -------------------------------
 */
static t_GVA_SeekSupport
p_gva_worker_check_seek_support(t_GVA_Handle  *gvahand)
{
  t_GVA_SeekSupport l_rc;

  l_rc = GVA_SEEKSUPP_NONE;
  if(gvahand)
  {
    t_GVA_DecoderElem *dec_elem;

    dec_elem = (t_GVA_DecoderElem *)gvahand->dec_elem;

    if(dec_elem)
    {
      if(dec_elem->fptr_seek_support == NULL)
      {
         printf("p_gva_worker_check_seek_support: Method not implemented in decoder %s\n", dec_elem->decoder_name);
         return(GVA_SEEKSUPP_NONE);
      }
      l_rc = (*dec_elem->fptr_seek_support)(gvahand);
    }
  }
  return(l_rc);
}  /* end p_gva_worker_check_seek_support */


/* ----------------------------
 * p_gva_worker_get_video_chunk
 * ----------------------------
 */
static t_GVA_RetCode
p_gva_worker_get_video_chunk(t_GVA_Handle  *gvahand
                            , gint32 frame_nr
                            , unsigned char *chunk
                            , gint32 *size
                            , gint32 max_size)
{
  t_GVA_RetCode l_rc;

  l_rc = GVA_RET_ERROR;
  if(gvahand)
  {
    t_GVA_DecoderElem *dec_elem;

    dec_elem = (t_GVA_DecoderElem *)gvahand->dec_elem;

    if(dec_elem)
    {
      if(dec_elem->fptr_get_video_chunk == NULL)
      {
         printf("p_gva_worker_get_video_chunk: Method not implemented in decoder %s\n", dec_elem->decoder_name);
         return(GVA_RET_ERROR);
      }
      l_rc =  (*dec_elem->fptr_get_video_chunk)(gvahand
                                               , frame_nr
                                               , chunk
                                               , size
                                               , max_size
                                               );
    }
  }
  return(l_rc);
}  /* end p_gva_worker_get_video_chunk */


/* ----------------------------
 * p_gva_worker_get_codec_name
 * ----------------------------
 */
static char *
p_gva_worker_get_codec_name(t_GVA_Handle  *gvahand
                            ,t_GVA_CodecType codec_type
                            ,gint32 track_nr
                            )
{
  char *codec_name;

  codec_name = NULL;
  if(gvahand)
  {
    t_GVA_DecoderElem *dec_elem;

    dec_elem = (t_GVA_DecoderElem *)gvahand->dec_elem;

    if(dec_elem)
    {
      if(dec_elem->fptr_get_codec_name == NULL)
      {
         printf("p_gva_worker_get_codec_name: Method not implemented in decoder %s\n", dec_elem->decoder_name);
         return(NULL);
      }
      codec_name =  (*dec_elem->fptr_get_codec_name)(gvahand
                                               , codec_type
                                               , track_nr
                                               );
    }
  }
  return(codec_name);
}  /* end p_gva_worker_get_codec_name */

/* --------------------------
 * p_gva_worker_open_read
 * --------------------------
 *
 * open a videofile for reading.
 * this procedure tries to findout a compatible
 * decoder for the videofile.
 * if a compatible decoder is available,
 * a t_GVA_Handle Structure (with informations
 * about the video and the decoder) is returned.
 * NULL is returned, if none of the known decoders
 * can read the videofile.
 *
 * in case of successful open a buffer for one uncompressed RGBA frame
 * and row_pointers are allocated automatically.
 * (and will be freed automatically by the close procedure)
 */
static t_GVA_Handle *
p_gva_worker_open_read(const char *filename, gint32 vid_track, gint32 aud_track
                      ,const char *preferred_decoder
                      ,gboolean disable_mmx
                      )
{
  t_GVA_Handle       *gvahand;
  t_GVA_DecoderElem  *dec_elem;

  gvahand = g_malloc0(sizeof(t_GVA_Handle));
  gvahand->dec_elem = NULL;  /* init no decoder found */
  gvahand->decoder_handle = NULL;
  gvahand->vid_track = MAX(vid_track -1, 0);
  gvahand->aud_track = MAX(aud_track -1, 0);
  gvahand->aspect_ratio = 0.0;
  gvahand->vtracks = 0;
  gvahand->atracks = 0;
  gvahand->progress_cb_user_data = NULL;
  gvahand->fptr_progress_callback = NULL;
  gvahand->frame_data = NULL;
  gvahand->row_pointers = NULL;
  gvahand->fc_frame_data = NULL;
  gvahand->fc_row_pointers = NULL;
  gvahand->fcache.fc_current = NULL;
  gvahand->fcache.frame_cache_size = 0;
  gvahand->fcache.max_fcache_id = 0;
  gvahand->fcache.fcache_locked = FALSE;
  gvahand->image_id = -1;
  gvahand->layer_id = -1;
  gvahand->disable_mmx = disable_mmx;
  gvahand->dirty_seek = FALSE;
  gvahand->emulate_seek = FALSE;
  gvahand->create_vindex = FALSE;
  gvahand->vindex = NULL;
  gvahand->mtime = 0;

  gvahand->do_gimp_progress = FALSE;          /* WARNING: dont try to set this TRUE if you call the API from a pthread !! */
  gvahand->all_frames_counted = FALSE;
  gvahand->all_samples_counted = FALSE;
  gvahand->critical_timecodesteps_found = FALSE;
  gvahand->cancel_operation = FALSE;
  gvahand->filename = g_strdup(filename);
  gvahand->current_frame_nr = 0;
  gvahand->current_seek_nr = 1;
  gvahand->current_sample = 1.0;
  gvahand->reread_sample_pos = 1.0;
  gvahand->audio_playtime_sec = 0.0;
  gvahand->total_aud_samples = 0;
  gvahand->samplerate = 0;
  gvahand->audio_cannels = 0;
  gvahand->percentage_done = 0.0;
  gvahand->frame_counter = 0;
  gvahand->gva_thread_save = TRUE;  /* default for most decoder libs */
  gvahand->fcache_mutex = NULL;     /* per default do not use g_mutex_lock / g_mutex_unlock at fcache access */
  gvahand->user_data = NULL;        /* reserved for user data */
  
  GAP_TIMM_INIT_RECORD(&gvahand->fcacheMutexLockStats);


  if(GVA_global_decoder_list == NULL)
  {
    p_register_all_decoders();
  }

  if(preferred_decoder)
  {
    /* if the caller provided a preferred_decoder name
     * we check that decoder as 1.st one, before checking the full decoder list
     */
    for(dec_elem = GVA_global_decoder_list; dec_elem != NULL; dec_elem = dec_elem->next)
    {
      int l_flag;

      if(strcmp(preferred_decoder, dec_elem->decoder_name) == 0)
      {
        if(gap_debug) printf("GVA: check sig %s with preferred decoder:%s\n", filename, dec_elem->decoder_name);


        /* call the decoder specific check sig function */
        l_flag = (*dec_elem->fptr_check_sig)(gvahand->filename);
        if (l_flag == 1)
        {
           gvahand->dec_elem = dec_elem;
           break;
        }
      }
    }
  }

  if(gvahand->dec_elem == NULL)
  {
    /* try to find a decoder that can decode the videofile */
    for(dec_elem = GVA_global_decoder_list; dec_elem != NULL; dec_elem = dec_elem->next)
    {
      int l_flag;

      if(gap_debug) printf("GVA: check sig %s with decoder:%s\n", filename, dec_elem->decoder_name);


      /* call the decoder specific check sig function */
      l_flag = (*dec_elem->fptr_check_sig)(gvahand->filename);
      if (l_flag == 1)
      {
         gvahand->dec_elem = dec_elem;
         break;
      }
    }
  }

  if(gvahand->dec_elem == NULL)
  {
      printf("GVA: File %s is NOT a supported Videoformat\n", filename);
      g_free(gvahand);
      return NULL;
  }

  /* call decoder specific wrapper for open_read Procedure */
  dec_elem = (t_GVA_DecoderElem *)gvahand->dec_elem;

  if(gap_debug) printf("GVA: p_gva_worker_open_read: before OPEN %s with decoder:%s\n", filename,  dec_elem->decoder_name);

  (*dec_elem->fptr_open_read)(gvahand->filename, gvahand);

  if(gap_debug) printf("GVA: p_gva_worker_open_read: after OPEN %s with decoder:%s\n", filename,  dec_elem->decoder_name);


  if (gvahand->decoder_handle == NULL)
  {
      printf("Open Videofile %s FAILED\n", filename);
      g_free(gvahand);
      return NULL;
  }

  /* allocate buffer for one frame (use minimal size 2x2 if no videotrack is present) */
  if(gvahand)
  {
    /* allocate frame_data and row_pointers for one frame (minimal cachesize 1 == no chaching)
     * (calling apps may request bigger cache after successful open)
     */
    p_build_frame_cache(gvahand, 1);
  }

  gvahand->vid_track = CLAMP(gvahand->vid_track, 0, gvahand->vtracks-1);
  gvahand->aud_track = CLAMP(gvahand->aud_track, 0, gvahand->atracks-1);

  if(gap_debug) printf("END OF p_gva_worker_open_read: vtracks:%d atracks:%d\n", (int)gvahand->vtracks, (int)gvahand->atracks );

  return (gvahand);
}   /* end p_gva_worker_open_read */



/* ------------------------------------
 * Public GVA API functions
 * ------------------------------------
 */
t_GVA_Handle *
GVA_open_read_pref(const char *filename, gint32 vid_track, gint32 aud_track
                 ,const char *preferred_decoder
                 ,gboolean disable_mmx
                 )
{
  t_GVA_Handle *handle;

  if(gap_debug) printf("GVA_open_read_pref: START\n");
  if((gap_debug) && (preferred_decoder)) printf("GVA_open_read_pref: preferred_decoder: %s\n", preferred_decoder);

  handle = p_gva_worker_open_read(filename, vid_track, aud_track
                                 ,preferred_decoder
                                 ,disable_mmx
                                 );

  if(gap_debug) printf("GVA_open_read_pref: END handle:%d\n", (int)handle);

  return(handle);
}

t_GVA_Handle *
GVA_open_read(const char *filename, gint32 vid_track, gint32 aud_track)
{
  t_GVA_Handle *handle;
  char   *l_env_preferred_decoder;

  if(gap_debug) printf("GVA_open_read: START\n");

  l_env_preferred_decoder = g_strdup(g_getenv("GVA_PREFERRED_DECODER"));

  handle = p_gva_worker_open_read(filename, vid_track, aud_track
                                 ,l_env_preferred_decoder    /* NULL or preferred_decoder */
                                 ,FALSE   /* disable_mmx==FALSE (use MMX if available) */
                                 );
  if(l_env_preferred_decoder)
  {
    g_free(l_env_preferred_decoder);
  }

  if(gap_debug) printf("GVA_open_read: END handle:%d\n", (int)handle);

  return(handle);
}

void
GVA_close(t_GVA_Handle  *gvahand)
{
  if(gap_debug) printf("GVA_close: START handle:%d\n", (int)gvahand);
  p_gva_worker_close(gvahand);
  if(gap_debug) printf("GVA_close: END\n");
}

t_GVA_RetCode
GVA_get_next_frame(t_GVA_Handle  *gvahand)
{
  t_GVA_RetCode l_rc;

  static gint32 funcId = -1;
  
  GAP_TIMM_GET_FUNCTION_ID(funcId, "GVA_get_next_frame");
  GAP_TIMM_START_FUNCTION(funcId);

  if(gap_debug)
  {
    printf("GVA_get_next_frame: START handle:%d\n", (int)gvahand);
  }

  l_rc = p_gva_worker_get_next_frame(gvahand);

  if(gap_debug)
  {
    printf("GVA_get_next_frame: END rc:%d\n", (int)l_rc);
  }


  GAP_TIMM_STOP_FUNCTION(funcId);

  return(l_rc);
}

t_GVA_RetCode
GVA_seek_frame(t_GVA_Handle  *gvahand, gdouble pos, t_GVA_PosUnit pos_unit)
{
  t_GVA_RetCode l_rc;

  static gint32 funcId = -1;
  
  GAP_TIMM_GET_FUNCTION_ID(funcId, "GVA_seek_frame");
  GAP_TIMM_START_FUNCTION(funcId);

  if(gap_debug)
  {
    printf("GVA_seek_frame: START handle:%d, pos%.4f unit:%d\n"
      , (int)gvahand, (float)pos, (int)pos_unit);
  }

  l_rc = p_gva_worker_seek_frame(gvahand, pos, pos_unit);

  if(gap_debug)
  {
    printf("GVA_seek_frame: END rc:%d\n"
      , (int)l_rc);
  }

  GAP_TIMM_STOP_FUNCTION(funcId);

  return(l_rc);
}

t_GVA_RetCode
GVA_seek_audio(t_GVA_Handle  *gvahand, gdouble pos, t_GVA_PosUnit pos_unit)
{
  t_GVA_RetCode l_rc;

  if(gap_debug) printf("GVA_seek_audio: START handle:%d\n", (int)gvahand);
  l_rc = p_gva_worker_seek_audio(gvahand, pos, pos_unit);
  if(gap_debug) printf("GVA_seek_audio: END rc:%d\n", (int)l_rc);

  return(l_rc);
}

t_GVA_RetCode
GVA_get_audio(t_GVA_Handle  *gvahand
             ,gint16 *output_i            /* preallocated buffer large enough for samples * siezeof gint16 */
             ,gint32 channel              /* audiochannel 1 upto n */
             ,gdouble samples             /* number of samples to read */
             ,t_GVA_AudPos mode_flag      /* specify the position where to start reading audio from */
             )
{
  t_GVA_RetCode l_rc;

  if(gap_debug) printf("GVA_get_audio: START handle:%d, ch:%d samples:%d mod:%d\n", (int)gvahand, (int)channel, (int)samples, (int)mode_flag);
  l_rc = p_gva_worker_get_audio(gvahand
                               ,output_i
                               ,channel
                               ,samples
                               ,mode_flag
                               );
  if(gap_debug) printf("GVA_get_audio: END rc:%d\n", (int)l_rc);

  return(l_rc);
}

t_GVA_RetCode
GVA_count_frames(t_GVA_Handle  *gvahand)
{
  t_GVA_RetCode l_rc;

  if(gap_debug) printf("GVA_count_frames: START handle:%d\n", (int)gvahand);
  l_rc = p_gva_worker_count_frames(gvahand);
  if(gap_debug) printf("GVA_count_frames: END rc:%d\n", (int)l_rc);

  return(l_rc);
}

t_GVA_SeekSupport
GVA_check_seek_support(t_GVA_Handle  *gvahand)
{
  t_GVA_SeekSupport l_rc;

  if(gap_debug) printf("GVA_check_seek_support: START handle:%d\n", (int)gvahand);
  l_rc = p_gva_worker_check_seek_support(gvahand);
  if(gap_debug) printf("GVA_check_seek_support: END rc:%d\n", (int)l_rc);

  return(l_rc);
}

t_GVA_RetCode
GVA_get_video_chunk(t_GVA_Handle  *gvahand
                   , gint32 frame_nr
                   , unsigned char *chunk
                   , gint32 *size
                   , gint32 max_size)
{
  t_GVA_RetCode l_rc;

  if(gap_debug) printf("GVA_get_video_chunk: START handle:%d, chunk addr:%d (max_size:%d) frame_nr:%d\n"
                      , (int)gvahand
                      , (int)chunk
                      , (int)max_size
                      , (int)frame_nr
                      );
  l_rc = p_gva_worker_get_video_chunk(gvahand, frame_nr, chunk, size, max_size);
  if(gap_debug) printf("GVA_get_video_chunk: END rc:%d size:%d\n", (int)l_rc, (int)*size);

  return(l_rc);
}


gboolean
GVA_has_video_chunk_proc(t_GVA_Handle  *gvahand)
{
  gboolean l_rc;

  if(gap_debug) printf("GVA_has_video_chunk_proc: START handle:%d\n", (int)gvahand);

  l_rc = FALSE;
  if(gvahand)
  {
    t_GVA_DecoderElem *dec_elem;

    dec_elem = (t_GVA_DecoderElem *)gvahand->dec_elem;

    if(dec_elem)
    {
      if(dec_elem->fptr_get_video_chunk != NULL)
      {
         l_rc = TRUE;
      }
    }
  }
  if(gap_debug) printf("GVA_has_video_chunk_proc: END rc:%d\n", (int)l_rc);

  return(l_rc);
}


char *
GVA_get_codec_name(t_GVA_Handle  *gvahand
                  ,t_GVA_CodecType codec_type
                  ,gint32 track_nr
                  )
{
  char *codec_name;

  if(gap_debug)
  {
    printf("GVA_get_video_chunk: START handle:%d, codec_type:%d track_nr:%d\n"
                      , (int)gvahand
                      , (int)codec_type
                      , (int)track_nr
                      );
  }

  codec_name = p_gva_worker_get_codec_name(gvahand, codec_type, track_nr);
  if(gap_debug)
  {
    printf("GVA_get_codec_name: END codec_name:");
    if (codec_name)
    {
      printf("%s", codec_name);
    }
    else
    {
      printf("NULL");
    }
    printf("\n");
  }

  return(codec_name);
}

/* -------------------------------------------------------------------------
 * Converter Procedures (Framebuffer <--> GIMP
 * -------------------------------------------------------------------------
 */


/* ----------------------------------
 * GVA_gimp_image_to_rowbuffer
 * ----------------------------------
 * transfer a gimp image to GVA rowbuffer (gvahand->frame_data)
 * please note that the image will be
 * - flattened
 * - converted to RGB
 * - scaled to GVA framesize
 * if needed.
 */
t_GVA_RetCode
GVA_gimp_image_to_rowbuffer(t_GVA_Handle *gvahand, gint32 image_id)
{
  GimpPixelRgn pixel_rgn;
  GimpDrawable *drawable;
  gint32 l_layer_id;
  gint          l_nlayers;
  gint32       *l_layers_list;

  if(gap_debug) printf("GVA_gimp_image_to_rowbuffer image_id: %d\n", (int)image_id);

  l_layer_id = -1;
  l_layers_list = gimp_image_get_layers(image_id, &l_nlayers);
  if(l_layers_list != NULL)
  {
    l_layer_id = l_layers_list[0];
    g_free (l_layers_list);
  }
  else
  {
    printf("GVA_gimp_image_to_rowbuffer: No Layer found, image_id:%d\n", (int)image_id);
    return(GVA_RET_ERROR);
  }

  drawable = gimp_drawable_get (l_layer_id);

  /* flatten image if needed (more layers or layer with alpha channel) */
  if((l_nlayers > 1 ) || (drawable->bpp == 4)  || (drawable->bpp ==2))
  {
     if(gap_debug) printf("GVA_gimp_image_to_rowbuffer: FLATTEN Image\n");

     l_layer_id = gimp_image_flatten(image_id);
     gimp_drawable_detach (drawable);
     drawable = gimp_drawable_get (l_layer_id);
  }

  /* convert TO RGB if needed */
  if(gimp_image_base_type(image_id) != GIMP_RGB)
  {
     if(gap_debug) printf("GVA_gimp_image_to_rowbuffer: convert TO_RGB\n");
     gimp_image_convert_rgb(image_id);
  }

  /* ensure unique imagesize for all frames */
  if((gimp_image_width(image_id) != gvahand->width)
  ||  (gimp_image_height(image_id) != gvahand->height))
  {
    if(gap_debug) printf("GVA_gimp_image_to_rowbuffer: SCALING Image\n");
    gimp_image_scale(image_id, gvahand->width, gvahand->height);
  }

  /* now we have an image with one RGB layer
   * and copy all pixelrows to gvahand->frame_data at once.
   */
  gimp_pixel_rgn_init (&pixel_rgn, drawable, 0, 0
                      , drawable->width, drawable->height
                      , FALSE     /* dirty */
                      , FALSE     /* shadow */
                       );
  gimp_pixel_rgn_get_rect (&pixel_rgn, gvahand->frame_data
                          , 0
                          , 0
                          , gvahand->width
                          , gvahand->height);

  gimp_drawable_detach(drawable);
  
  if (gap_debug)  printf("DEBUG: after copy data rows \n");


  return(GVA_RET_OK); /* return the newly created layer_id (-1 on error) */
}  /* end GVA_gimp_image_to_rowbuffer */




/* ------------------------------------
 * p_check_image_is_alive
 * ------------------------------------
 * return TRUE  if OK (image is still valid)
 * return FALSE if image is NOT valid
 */
static gboolean
p_check_image_is_alive(gint32 image_id)
{
  gint32 *images;
  gint    nimages;
  gint    l_idi;
  gint    l_found;

  if(image_id < 0)
  {
     return FALSE;
  }

  images = gimp_image_list(&nimages);
  l_idi = nimages -1;
  l_found = FALSE;
  while((l_idi >= 0) && images)
  {
    if(image_id == images[l_idi])
    {
          l_found = TRUE;
          break;
    }
    l_idi--;
  }
  if(images) g_free(images);
  if(l_found)
  {
    return TRUE;  /* OK */
  }

  if(gap_debug)
  {
    printf("p_check_image_is_alive: image_id %d is not VALID\n", (int)image_id);
  }
  return FALSE ;   /* INVALID image id */
}  /* end p_check_image_is_alive */


/* ------------------------------------
 * gva_delace_mix_rows
 * ------------------------------------
 * mix 2 input pixelrows (prev_row, next_row)
 * to one resulting pixelrow (mixed_row)
 * All pixelrows must have same width and bpp
 */
static inline void
gva_delace_mix_rows( gint32 width
          , gint32 bpp
          , gint32 row_bytewidth
          , gint32 mix_threshold   /* 0 <= mix_threshold <= 33554432 (256*256*256*2) */
          , const guchar *prev_row
          , const guchar *next_row
          , guchar *mixed_row
          )
{
  if((bpp <3)  || (mix_threshold >= MIX_MAX_THRESHOLD))
  {
    gint32 l_idx;

    /* simple mix all bytes */
    for(l_idx=0; l_idx < row_bytewidth; l_idx++)
    {
      mixed_row[l_idx] = (prev_row[l_idx] + next_row[l_idx]) / 2;
    }
  }
  else
  {
    gint32 l_col;

    /* color threshold mix */
    for(l_col=0; l_col < width; l_col++)
    {
      gint16 r1, g1, b1, a1;
      gint16 r2, g2, b2, a2;
      gint16 dr, db;
      gint32 fhue, fval;

      r1 = prev_row[0];
      g1 = prev_row[1];
      b1 = prev_row[2];

      r2 = next_row[0];
      g2 = next_row[1];
      b2 = next_row[2];

      dr = abs((r1 - g1) - (r2 - g2));
      db = abs((g1 - b1) - (g2 - b2));
      fval = abs(r1 - r2) + abs(g1 - g2) + abs(b1 - b2);    /* brightness difference */
      fhue = dr *  db;

      /* check hue failure and brightness failure against threshold */
      if((fhue + fval) < mix_threshold)
      {
        /* smooth mix */
        mixed_row[0] = (r1 + r2) / 2;
        mixed_row[1] = (g1 + g2) / 2;
        mixed_row[2] = (b1 + b2) / 2;
      }
      else
      {
        /* hard, no mix */
        mixed_row[0] = r1;
        mixed_row[1] = g1;
        mixed_row[2] = b1;
      }

      if(bpp == 4)
      {
        a1   = prev_row[3];
        a2   = next_row[3];
        mixed_row[4] = (a1 + a2) / 2;
      }

      prev_row += bpp;
      next_row += bpp;
      mixed_row += bpp;
    }
  }
}  /* end gva_delace_mix_rows */



/* ------------------------------------
 * gva_delace_calculate_mix_threshold
 * ------------------------------------
 */
static gint32
gva_delace_calculate_mix_threshold(gdouble threshold)
{
  gint32  l_threshold;
  gint32  l_mix_threshold;

  /* expand threshold range from 0.0-1.0  to 0 - MIX_MAX_THRESHOLD */
  threshold = CLAMP(threshold, 0.0, 1.0);
  l_threshold = (gdouble)MIX_MAX_THRESHOLD * (threshold * threshold * threshold);
  l_mix_threshold = CLAMP((gint32)l_threshold, 0, MIX_MAX_THRESHOLD);
  return l_mix_threshold;
}


/* ------------------------------------
 * gva_delace_calculate_interpolate_flag
 * ------------------------------------
 */
static gint32
gva_delace_calculate_interpolate_flag(gint32 deinterlace)
{
  gint32  l_interpolate_flag;
 
  l_interpolate_flag = 0;
  if (deinterlace == 1)
  {
    l_interpolate_flag = 1;
  }
  return l_interpolate_flag;  
}




/* ------------------------------------
 * GVA_delace_frame
 * ------------------------------------
 * create a deinterlaced copy of the current frame
 * (the one where gvahand->fc_row_pointers are referring to)
 *
 * IN: gvahand  videohandle
 * IN: do_scale  FALSE: deliver frame at original size (ignore bpp, width and height parameters)
 *               TRUE: deliver frame at size specified by width, height, bpp
 *                     scaling is done fast in low quality 
 * IN: deinterlace   0: no deinterlace, 1 pick odd lines, 2 pick even lines
 * IN: threshold     0.0 hard, 1.0 smooth interpolation at deinterlacing
 *                   threshold is ignored if do_scaing == TRUE
 *
 * return databuffer, Pixels are stored in the RGB colormdel
 *                    the data buffer must be g_free'd by the calling program
 */
guchar *
GVA_delace_frame(t_GVA_Handle *gvahand
                , gint32 deinterlace
                , gdouble threshold
                )
{
  guchar *l_framedata_copy;
  guchar *l_row_ptr_dest;
  gint32  l_row;
  gint32  l_interpolate_flag;
  gint32  l_row_bytewidth;
  gint32  l_mix_threshold;

  if(gvahand->fc_row_pointers == NULL)
  {
    return (NULL);
  }
  l_row_bytewidth = gvahand->width * gvahand->frame_bpp;
  l_framedata_copy = g_malloc(l_row_bytewidth * gvahand->height);


  l_interpolate_flag = gva_delace_calculate_interpolate_flag(deinterlace);
  l_mix_threshold = gva_delace_calculate_mix_threshold(threshold);

  l_row_ptr_dest = l_framedata_copy;
  for(l_row = 0; l_row < gvahand->height; l_row++)
  {
    if ((l_row & 1) == l_interpolate_flag)
    {
      if(l_row == 0)
      {
         /* we have no prvious row, so we just copy the next row */
         memcpy(l_row_ptr_dest, gvahand->fc_row_pointers[1],  l_row_bytewidth);
      }
      else
      {
        if(l_row == gvahand->height -1)
        {
          /* we have no next row, so we just copy the prvious row */
          memcpy(l_row_ptr_dest, gvahand->fc_row_pointers[gvahand->height -2],  l_row_bytewidth);
        }
        else
        {
          /* we have both prev and next row within valid range
           * and can calculate an interpolated row
           */
          gva_delace_mix_rows ( gvahand->width
                       , gvahand->frame_bpp
                       , l_row_bytewidth
                       , l_mix_threshold
                       , gvahand->fc_row_pointers[l_row -1]
                       , gvahand->fc_row_pointers[l_row +1]
                       , l_row_ptr_dest
                       );
        }
      }
    }
    else
    {
      /* copy original row */
      memcpy(l_row_ptr_dest, gvahand->fc_row_pointers[l_row],  l_row_bytewidth);
    }
    l_row_ptr_dest += l_row_bytewidth;

  }

  return(l_framedata_copy);
}  /* end GVA_delace_frame */


/* ------------------------------------
 * p_gva_deinterlace_drawable
 * ------------------------------------
 */
static void
p_gva_deinterlace_drawable (GimpDrawable *drawable, gint32 deinterlace, gdouble threshold)
{
  GimpPixelRgn  srcPR, destPR;
  guchar       *dest;
  guchar       *dest_buffer = NULL;
  guchar       *upper;
  guchar       *lower;
  gint          row, col;
  gint          x, y;
  gint          width, height;
  gint          bytes;
  gint x2, y2;
  gint32  l_row_bytewidth;
  gint32  l_interpolate_flag;
  gint32  l_mix_threshold;


  l_interpolate_flag = gva_delace_calculate_interpolate_flag(deinterlace);
  l_mix_threshold = gva_delace_calculate_mix_threshold(threshold);

  bytes = drawable->bpp;

  gimp_drawable_mask_bounds (drawable->drawable_id, &x, &y, &x2, &y2);
  width  = x2 - x;
  height = y2 - y;
  l_row_bytewidth = width * drawable->bpp;
  dest = g_new (guchar, l_row_bytewidth);

  gimp_pixel_rgn_init (&destPR, drawable, x, y, width, height, TRUE, TRUE);

  gimp_pixel_rgn_init (&srcPR, drawable,
                       x, MAX (y - 1, 0),
                       width, MIN (height + 1, drawable->height),
                       FALSE, FALSE);

  /*  allocate buffers for upper and lower row  */
  upper = g_new (guchar, l_row_bytewidth);
  lower = g_new (guchar, l_row_bytewidth);

  for (row = y; row < y + height; row++)
  {
    /*  Only do interpolation if the row:
     *  (1) Isn't one we want to keep
     *  (2) Has both an upper and a lower row
     *  Otherwise, just duplicate the source row
     */
    if (((row & 1) == l_interpolate_flag) &&
        (row - 1 >= 0) && (row + 1 < drawable->height))
    {
      gimp_pixel_rgn_get_row (&srcPR, upper, x, row - 1, width);
      gimp_pixel_rgn_get_row (&srcPR, lower, x, row + 1, width);
 
      gva_delace_mix_rows ( width
                 , drawable->bpp
                 , l_row_bytewidth
                 , l_mix_threshold
                 , upper
                 , lower
                 , dest
                 );

    }
    else
    {
      /* copy current row 1:1 */
      gimp_pixel_rgn_get_row (&srcPR, dest, x, row, width);
    }

    gimp_pixel_rgn_set_row (&destPR, dest, x, row, width);
  }


  /*  update the deinterlaced region  */
  gimp_drawable_flush (drawable);
  gimp_drawable_merge_shadow (drawable->drawable_id, TRUE);
  gimp_drawable_update (drawable->drawable_id, x, y, width, height);

  g_free (lower);
  g_free (upper);
  g_free (dest);

}  /* end p_gva_deinterlace_drawable */



/* ------------------------------------
 * GVA_delace_drawable
 * ------------------------------------
 * deinterlaced the specified drawable.
 *
 * IN: drawable_id     the drawable to be deinterlaced.
 * IN: do_scale  FALSE: deliver frame at original size (ignore bpp, width and height parameters)
 *               TRUE: deliver frame at size specified by width, height, bpp
 *                     scaling is done fast in low quality 
 * IN: deinterlace   0: no deinterlace (NOP), 1 pick odd lines, 2 pick even lines
 * IN: threshold     0.0 hard, 1.0 smooth interpolation at deinterlacing
 *                   threshold is ignored if do_scaing == TRUE
 *
 */
void
GVA_delace_drawable(gint32 drawable_id
                , gint32 deinterlace
                , gdouble threshold
                )
{
  GimpDrawable      *drawable;

  if (deinterlace != 0)
  {
    drawable = gimp_drawable_get(drawable_id);
    if (drawable)
    {
      p_gva_deinterlace_drawable (drawable, deinterlace, threshold);
      gimp_drawable_detach(drawable);
    }
  }
}  /* end GVA_delace_drawable */


/* ------------------------------------
 * GVA_image_set_aspect
 * ------------------------------------
 * if aspect_ratio is known convert the aspect
 * to image X and Y resolution in DPI 
 * this allows GIMP to scale the display to correct aspect
 * when View option "Dot for Dot" is turned off.
 * Note that the resolution is just a guess based on typical 
 * monitor resolution in DPI. 
 * (videos usually have no DPI resoulution information, just aspect ratio)
 */
void
GVA_image_set_aspect(t_GVA_Handle *gvahand, gint32 image_id)
{

  if(gvahand->aspect_ratio != 0.0)
  {
    gdouble xresolution;
    gdouble yresolution;
    gdouble xresolutionMoni;
    gdouble yresolutionMoni;
    gdouble asymetric;

    asymetric = ((gdouble)gvahand->width / (gdouble)gvahand->height)
              / gvahand->aspect_ratio;

    gimp_get_monitor_resolution(&xresolutionMoni, &yresolutionMoni);
    
    if (yresolutionMoni > 0)
    {
       yresolution = yresolutionMoni;
    }
    else
    {
       yresolution = 72.0;
    }
    xresolution = yresolution * asymetric;

    /* set resolution in DPI according to aspect ratio
     * assuming a typical monitor resolution
     */
    gimp_image_set_unit (image_id, GIMP_UNIT_INCH);
    gimp_image_set_resolution (image_id, xresolution, yresolution);

    if(gap_debug)
    {
      printf("API: resolution x/y %f / %f\n"
        , (float)xresolution
        , (float)yresolution
        );
    }
  }
}  /* end GVA_image_set_aspect */


/* ------------------------------------
 * GVA_frame_to_gimp_layer_2
 * ------------------------------------
 * transfer frame imagedata from buffer   (gvahand->frame_data or fcache)
 * to gimp image.                         (gvahand->image_id)
 * IN/OUT:
 *   *image_id    ... pointer to image id
 * IN:
 *   old_layer_id ...  -1 dont care about old layer(s) of the image
 *                        (you may use -1 if the image has no layer)
 *   delete_mode  ...  TRUE: delete old_layer_id
 *                     FALSE: keep old_layer_id, but set invisible
 *                            (use this to build mulitlayer images)
 *   deinterlace  ...  0 ..NO, deliver image as it is
 *                     1 .. deinterlace using odd lines and interpolate the even lines
 *                     2 .. deinterlace using even lines and interpolate the odd lines
 *   threshold    ...  0.0 <= threshold <= 1.0
 *                          threshold is used only for interpolation when deinterlace != 0
 *                          - big thresholds do smooth mix
 *                          - small thresholds keep hard edges (does not mix different colors)
 *                          - threshold 0 does not mix at all and uses only one inputcolor
 * RETURN: layer_id of the newly created layer,
 *         -2 if framenumber not found in fcache
 *         -1 on other errors
 *
 */
gint32
GVA_frame_to_gimp_layer_2(t_GVA_Handle *gvahand
                , gint32 *image_id
                , gint32 old_layer_id
                , gboolean delete_mode
                , gint32 framenumber
                , gint32 deinterlace
                , gdouble threshold
                )
{
  gchar *layername;
  GimpPixelRgn pixel_rgn;
  GimpDrawable *drawable;
  gint32 l_new_layer_id;
  gint32 l_threshold;
  gint32 l_mix_threshold;

  static gchar *odd_even_tab[8] = {"\0", "_odd", "_even", "_odd", "_even", "\0", "\0", "\0" };


  if(gap_debug)
  {
    printf("GVA_frame_to_gimp_layer_2 framenumber: %d\n", (int)framenumber);
  }

  l_new_layer_id = -1;
  if (GVA_search_fcache(gvahand, framenumber) != GVA_RET_OK)
  {
     if(gap_debug)
     {
       printf("frame %d not found in fcache!  %d\n"
         , (int)framenumber
         , (int)gvahand->current_frame_nr );
     }

     return (-2);
  }


  GVA_fcache_mutex_lock (gvahand);

  /* expand threshold range from 0.0-1.0  to 0 - MIX_MAX_THRESHOLD */
  threshold = CLAMP(threshold, 0.0, 1.0);
  l_threshold = (gdouble)MIX_MAX_THRESHOLD * (threshold * threshold * threshold);
  l_mix_threshold = CLAMP((gint32)l_threshold, 0, MIX_MAX_THRESHOLD);


  if(p_check_image_is_alive(*image_id))
  {
     /* reuse existing image for next frame */
     if((gvahand->width  != gimp_image_width(*image_id))
     || (gvahand->height != gimp_image_height(*image_id)))
     {
       /* resize to image to framesize */
       gimp_image_resize(*image_id
                       , gvahand->width
                       , gvahand->height
                       , 0
                       , 0);
       GVA_image_set_aspect(gvahand, *image_id);
     }
  }
  else
  {
     *image_id = gimp_image_new (gvahand->width, gvahand->height, GIMP_RGB);
     if (gap_debug)  printf("DEBUG: after gimp_image_new\n");
     GVA_image_set_aspect(gvahand, *image_id);

     old_layer_id = -1;
  }

  if (gimp_image_undo_is_enabled(*image_id))
  {
    gimp_image_undo_disable(*image_id);
  }

  if(gvahand->framerate > 0)
  {
    gint   delay;

    delay = 1000 / gvahand->framerate;
    layername = g_strdup_printf("Frame%05d%s (%dms)", (int)framenumber, odd_even_tab[deinterlace & 7], (int)delay);
  }
  else
  {
    layername = g_strdup_printf("Frame%05d%s", (int)framenumber, odd_even_tab[deinterlace & 7]);
  }

  if(gvahand->frame_bpp == 4)
  {
    l_new_layer_id = gimp_layer_new (*image_id
                                    , layername
                                    , gvahand->width
                                    , gvahand->height
                                    , GIMP_RGBA_IMAGE
                                    , 100.0, GIMP_NORMAL_MODE);
  }
  else
  {
    l_new_layer_id = gimp_layer_new (*image_id
                                    , layername
                                    , gvahand->width
                                    , gvahand->height
                                    , GIMP_RGB_IMAGE
                                    , 100.0, GIMP_NORMAL_MODE);
  }
  g_free(layername);

  drawable = gimp_drawable_get (l_new_layer_id);

  /* copy data rows
   *  libmpeg3 data rows (retrieved in mode MPEG3_RGB888)
   *  can be used 1:1 by gimp_pixel_rgn_set_rect on layertypes GIMP_RGB_IMAGE
   *
   */
  if (gap_debug)
  {
     printf("DEBUG: before copy data rows gvahand->height=%d  gvahand->width=%d\n"
            , (int)gvahand->height
            ,(int)gvahand->width);
  }

  /* Fill in the alpha channel (for RGBA mode)
   * libmpeg3 uses 0 for opaque alpha (that is full transparent in gimp terms)
   *
   * for playback it is faster to run libmpeg3 with bpp=3
   * and do not use alpha channel at all.
   * (or add alpha channel later but only if we are building a multilayer image)
   */
  if(gvahand->frame_bpp == 4)
  {
     gint i;

      if (gap_debug)  printf("DEBUG: FILL In the ALPHA CHANNEL\n");
      for (i=(gvahand->width * gvahand->height)-1; i>=0; i--)
      {
         gvahand->fc_frame_data[3+(i*4)] = 255;
      }
  }


  gimp_pixel_rgn_init (&pixel_rgn, drawable, 0, 0
                      , drawable->width, drawable->height
                      , TRUE      /* dirty */
                      , FALSE     /* shadow */
                       );
  if ((deinterlace == 0) || (gvahand->height < 2))
  {
    gimp_pixel_rgn_set_rect (&pixel_rgn, gvahand->fc_frame_data
                          , 0
                          , 0
                          , gvahand->width
                          , gvahand->height);
  }
  else
  {
    guchar *l_framedata_copy;
    
    l_framedata_copy = GVA_delace_frame(gvahand
                                       ,deinterlace
                                       ,threshold
                                       );
    if(l_framedata_copy)
    {
      gimp_pixel_rgn_set_rect (&pixel_rgn, l_framedata_copy
                            , 0
                            , 0
                            , gvahand->width
                            , gvahand->height);
      g_free(l_framedata_copy);
    }
  }

  GVA_fcache_mutex_unlock (gvahand);

  if (gap_debug)
  {
    printf("DEBUG: after copy data rows (NO SHADOW)\n");
  }

  gimp_drawable_flush (drawable);
  gimp_drawable_detach(drawable);

/*
 * gimp_drawable_merge_shadow (drawable->id, TRUE);
 */

  /* what to do with old layer ? */
  if(old_layer_id >= 0)
  {
    if(delete_mode)
    {
      gimp_image_remove_layer(*image_id, old_layer_id);
    }
    else
    {
      /* we are collecting layers in one multilayer image,
       * so do not delete previous added layer, but
       * only set invisible
       */
      gimp_drawable_set_visible(old_layer_id, FALSE);
    }
  }


  /* add new layer on top of the layerstack */
  gimp_image_insert_layer (*image_id, l_new_layer_id, 0, 0);
  gimp_drawable_set_visible(l_new_layer_id, TRUE);

  /* clear undo stack */
  if (gimp_image_undo_is_enabled(*image_id))
  {
    gimp_image_undo_disable(*image_id);
  }

  /* debug code to display a copy of the image */
  /*
   * {
   *   gint32 dup_id;
   *
   *  dup_id = gimp_image_duplicate(*image_id);
   *  printf("API: DUP_IMAGE_ID: %d\n", (int) dup_id);
   *  gimp_display_new(dup_id);
   * }
   */

  if (gap_debug)
  {
    printf("END GVA_frame_to_gimp_layer_2: return value:%d\n", (int)l_new_layer_id);
  }

  return(l_new_layer_id); /* return the newly created layer_id (-1 on error) */
}  /* end GVA_frame_to_gimp_layer_2 */


/* ------------------------------------
 * GVA_fcache_to_gimp_image
 * ------------------------------------
 * copy the GVA framecache to one GIMP multilayer image
 * limited by min_framenumber, max_framenumber.
 * use limitvalue(s) -1 if you want all cached frames without limit.
 *
 * returns image_id of the new created gimp image (or -1 on errors)
 */
gint32
GVA_fcache_to_gimp_image(t_GVA_Handle *gvahand
                , gint32 min_framenumber
                , gint32 max_framenumber
                , gint32 deinterlace
                , gdouble threshold
                )
{
  t_GVA_Frame_Cache *fcache;
  t_GVA_Frame_Cache_Elem  *fc_ptr;
  t_GVA_Frame_Cache_Elem  *fc_minframe;
  gint32   image_id;
  gint32   layer_id;
  gboolean delete_mode;

  image_id = -1;
  layer_id = -1;
  delete_mode = TRUE;

  fcache = &gvahand->fcache;

  GVA_fcache_mutex_lock (gvahand);

  /* search the fcache element with smallest positve framenumber */
  fc_minframe = NULL;
  if(fcache->fc_current)
  {
    fc_ptr = (t_GVA_Frame_Cache_Elem  *)fcache->fc_current;
    while(1 == 1)
    {
      if(fc_ptr->framenumber >= 0)
      {
        if(fc_minframe)
        {
          if(fc_ptr->framenumber < fc_minframe->framenumber)
          {
            fc_minframe = fc_ptr;
          }
        }
        else
        {
            fc_minframe = fc_ptr;
        }
      }

      fc_ptr = (t_GVA_Frame_Cache_Elem  *)fc_ptr->prev;

      if(fcache->fc_current == fc_ptr)
      {
        break;  /* STOP, we are back at startpoint of the ringlist */
      }
      if(fc_ptr == NULL)
      {
        break;  /* internal error, ringlist is broken */
      }
    }
  }

  if(fc_minframe)
  {
    fc_ptr = (t_GVA_Frame_Cache_Elem  *)fc_minframe;
    while(1 == 1)
    {
      if((fc_ptr->framenumber >= min_framenumber)
      &&((fc_ptr->framenumber <= max_framenumber) || (max_framenumber < 0))
      && (fc_ptr->framenumber >= 0))
      {
          GVA_fcache_mutex_unlock (gvahand);

          GVA_frame_to_gimp_layer_2(gvahand
                  , &image_id
                  , layer_id
                  , delete_mode
                  , fc_ptr->framenumber
                  , deinterlace
                  , threshold
                  );
           delete_mode = FALSE;  /* keep old layers */

           GVA_fcache_mutex_lock (gvahand);
      }

      /* step from fc_minframe forward in the fcache ringlist,
       * until we are back at fc_minframe
       */
      fc_ptr = (t_GVA_Frame_Cache_Elem  *)fc_ptr->next;

      if(fc_minframe == fc_ptr)
      {
        GVA_fcache_mutex_unlock (gvahand);
        return (image_id);  /* STOP, we are back at startpoint of the ringlist */
      }
      if(fc_ptr == NULL)
      {
        GVA_fcache_mutex_unlock (gvahand);
        return (image_id);  /* internal error, ringlist is broken */
      }
    }
  }

  GVA_fcache_mutex_unlock (gvahand);

  return image_id;
}  /* end GVA_fcache_to_gimp_image */


/* ------------------------------------
 * GVA_frame_to_gimp_layer
 * ------------------------------------
 */
t_GVA_RetCode
GVA_frame_to_gimp_layer(t_GVA_Handle *gvahand
                , gboolean delete_mode
                , gint32 framenumber
                , gint32 deinterlace
                , gdouble threshold
                )
{
  t_GVA_RetCode l_rc;

  if(gap_debug)
  {
    printf("GVA_frame_to_gimp_layer: START framenumber:%d image_id:%d layer_id: %d\n"
            , (int)framenumber
            , (int)gvahand->image_id
            , (int)gvahand->layer_id
            );
  }
  gvahand->layer_id =
  GVA_frame_to_gimp_layer_2(gvahand
                , &gvahand->image_id
                , gvahand->layer_id
                , delete_mode
                , framenumber
                , deinterlace
                , threshold
                );
  if(gvahand->layer_id < 0)
  {
    l_rc = GVA_RET_ERROR;
  }
  else
  {
    l_rc = GVA_RET_OK;
  }

  if(gap_debug)
  {
    printf("GVA_frame_to_gimp_layer: END RC:%d framenumber:%d image_id:%d layer_id: %d\n"
            , (int)l_rc
            , (int)framenumber
            , (int)gvahand->image_id
            , (int)gvahand->layer_id
            );
  }


  return(l_rc);
}  /* end GVA_frame_to_gimp_layer */



/* ------------------------------------
 * GVA_frame_to_buffer
 * ------------------------------------
 *  HINT:
 *  for the calling program it is easier to call
 *      GVA_fetch_frame_to_buffer
 *
 * IN: gvahand  videohandle
 * IN: do_scale  FALSE: deliver frame at original size (ignore bpp, width and height parameters)
 *               TRUE: deliver frame at size specified by width, height, bpp
 *                     scaling is done fast in low quality 
 * IN: framenumber   The wanted framenumber
 *                   return NULL if the wanted framnumber is not in the fcache
 *                   In this case the calling program should fetch the wanted frame.
 *                   This can be done by calling:
 *                      GVA_seek_frame(gvahand, framenumber, GVA_UPOS_FRAMES);
 *                      GVA_get_next_frame(gvahand);
 *                   after successful fetch call GVA_frame_to_buffer once again.
 *                   The wanted framnumber should be found in the cache now.
 * IN: deinterlace   0: no deinterlace, 1 pick odd lines, 2 pick even lines
 * IN: threshold     0.0 hard, 1.0 smooth interpolation at deinterlacing
 *                   threshold is ignored if do_scaing == TRUE
 * IN/OUT: bpp       accept 3 or 4 (ignored if do_scale == FALSE)
 * IN/OUT: width     accept with >=1 and <= original video with (ignored if do_scale == FALSE)
 * IN/OUT: height    accept height >=1 and <= original video height (ignored if do_scale == FALSE)
 *
 * return databuffer, Pixels are stored in the RGB colormdel
 *                    the data buffer must be g_free'd by the calling program
 */
guchar *
GVA_frame_to_buffer(t_GVA_Handle *gvahand
                , gboolean do_scale
                , gint32 framenumber
                , gint32 deinterlace
                , gdouble threshold
                , gint32 *bpp
                , gint32 *width
                , gint32 *height
                )
{
  gint32  frame_size;
  guchar *frame_data;
  static gint32 funcIdDoScale = -1;
  static gint32 funcIdNoScale = -1;
  
  GAP_TIMM_GET_FUNCTION_ID(funcIdDoScale, "GVA_frame_to_buffer.do_scale");
  GAP_TIMM_GET_FUNCTION_ID(funcIdNoScale, "GVA_frame_to_buffer.no_scale");

  frame_data = NULL;
  
  if (GVA_search_fcache(gvahand, framenumber) != GVA_RET_OK)
  {
     if(gap_debug) printf("frame %d not found in fcache!  %d\n", (int)framenumber , (int)gvahand->current_frame_nr );

     return (NULL);
  }



  if(do_scale)
  {
    guchar       *src_row, *src, *dest;
    gint         row, col;
    gint32       deinterlace_mask;
    gint         *arr_src_col;


    GAP_TIMM_START_FUNCTION(funcIdDoScale);

    if(gap_debug) printf("GVA_frame_to_buffer: DO_SCALE\n");
    /* for safety: width and height must be set to useful values
     * (dont accept bigger values than video size or values less than 1 pixel)
     */
    if((*width < 1) || (*width > gvahand->width))
    {
      *width = gvahand->width;
    }
    if((*height < 1) || (*height > gvahand->height))
    {
      *height = gvahand->height;
    }

    *bpp = CLAMP(*bpp, 3, 4);
   
    frame_size = (*width) * (*bpp) * (*height);
    if(*bpp < 4)
    {
      /* add extra scratch byte, because the following loop
       * always writes 4 byte per pixel
       * (no check for bpp for performance reasons)
       */
      frame_size++;
    }
    frame_data = g_malloc(frame_size);
    if(frame_data == NULL)
    {
      return (NULL);
    }
    if(deinterlace == 0)
    {
      deinterlace_mask = 0xffffffff;
    }
    else
    {
      /* mask is applied on row number to eliminate odd rownumbers */
      deinterlace_mask = 0x7ffffffe;
    }

    /* array of column fetch indexes for quick access
     * to the source pixels. (The source row may be larger or smaller than pwidth)
     */
    arr_src_col = g_new ( gint, (*width) );                   /* column fetch indexes foreach preview col */
    if(arr_src_col)
    {
      for ( col = 0; col < (*width); col++ )
      {
        arr_src_col[ col ] = ( col * gvahand->width / (*width) ) * gvahand->frame_bpp;
      }

      dest = frame_data;
      /* copy row by row */
      for ( row = 0; row < *height; row++ )
      {
          src_row = gvahand->fc_row_pointers[(row * gvahand->height / (*height)) & deinterlace_mask];
          for ( col = 0; col < (*width); col++ )
          {
              src = &src_row[ arr_src_col[col] ];
              dest[0] = src[0];
              dest[1] = src[1];
              dest[2] = src[2];
              dest[3] = 255;
              dest += (*bpp);
          }
      }
      g_free(arr_src_col);
    }

    GAP_TIMM_STOP_FUNCTION(funcIdDoScale);

  }
  else
  {
    GAP_TIMM_START_FUNCTION(funcIdNoScale);

    *bpp = gvahand->frame_bpp;
    *width = gvahand->width;
    *height = gvahand->height;
    frame_size = (*width) * (*height) * (*bpp);
 


      
    if ((deinterlace == 0) || (gvahand->height < 2))
    {
      if(gap_debug) printf("GVA_frame_to_buffer: MEMCPY\n");
      frame_data = g_malloc(frame_size);
      if(frame_data == NULL)
      {
        GAP_TIMM_STOP_FUNCTION(funcIdNoScale);
        return (NULL);
      }
      memcpy(frame_data, gvahand->fc_frame_data, frame_size);
    }
    else
    {
      frame_data = GVA_delace_frame(gvahand
                                   ,deinterlace
                                   ,threshold
                                   );
      if(frame_data == NULL)
      {
        GAP_TIMM_STOP_FUNCTION(funcIdNoScale);
        return (NULL);
      }
    }

    /* Fill in the alpha channel (for RGBA mode)
     * libmpeg3 uses 0 for opaque alpha (that is full transparent in gimp terms)
     *
     * normally the libmpeg3 decoder is used with with RGB mode (bpp==3)
     * and does not use alpha channel at all.
     */
    if(*bpp == 4)
    {
       gint i;

        if (gap_debug)  printf("DEBUG: FILL In the ALPHA CHANNEL\n");
        for (i=((*width) * (*height))-1; i>=0; i--)
        {
           frame_data[3+(i*4)] = 255;
        }
    }

    GAP_TIMM_STOP_FUNCTION(funcIdNoScale);
  }

  return(frame_data);
  
}       /* end GVA_frame_to_buffer */


/* ------------------------------------
 * GVA_fetch_frame_to_buffer
 * ------------------------------------
 * This procedure does fetch the specified framenumber from a videofile
 * and returns a buffer of RGB (or RGBA) encoded pixeldata.
 * (it does first search the API internal framecache,
 *  then tries to read from the videofile (if not found in the framecahe)
 *
 * the returned data is a (optional downscaled) copy of the API internal frame data
 * and must be g_free'd by the calling program after use.
 *
 * IN: gvahand  videohandle
 * IN: do_scale  FALSE: deliver frame at original size (ignore bpp, width and height parameters)
 *               TRUE: deliver frame at size specified by width, height, bpp
 *                     scaling is done fast in low quality 
 * IN: isBackwards  FALSE: disable prefetch on backwards seek (prefetch typically slows down in case of forward playing clip).
 *                  TRUE: enable prefetch of some (10) frames that fills up fcache and gives better performance
 *                        on backwards playing clips
 * IN: framenumber   The wanted framenumber
 *                   return NULL if the wanted framnumber could not be read.
 * IN: deinterlace   0: no deinterlace, 1 pick odd lines, 2 pick even lines
 * IN: threshold     0.0 hard, 1.0 smooth interpolation at deinterlacing
 *                   threshold is ignored if do_scaing == TRUE
 *                   (this parameter is ignored if deinterlace == 0)
 * IN/OUT: bpp       accept 3 or 4 (ignored if do_scale == FALSE)
 * IN/OUT: width     accept with >=1 and <= original video with (ignored if do_scale == FALSE)
 * IN/OUT: height    accept height >=1 and <= original video height (ignored if do_scale == FALSE)
 *
 * return databuffer, Pixels are stored in the RGB colormdel
 *                   NULL is returned if the frame could not be fetched
 *                        (in case of ERRORS or End of File reasons)
 */
guchar *
GVA_fetch_frame_to_buffer(t_GVA_Handle *gvahand
                , gboolean do_scale
                , gboolean isBackwards
                , gint32 framenumber
                , gint32 deinterlace
                , gdouble threshold
                , gint32 *bpp
                , gint32 *width
                , gint32 *height
                )
{
#define GVA_NEAR_FRAME_DISTANCE 10
#define GVA_NEAR_FRAME_DISTANCE_BACK 30
  guchar *frame_data;
  t_GVA_RetCode  l_rc;

  /* check if the wanted framenr is available in the GVA framecache */
  frame_data = GVA_frame_to_buffer(gvahand
             , do_scale
             , framenumber
             , deinterlace
             , threshold
             , bpp
             , width
             , height
             );

  if(frame_data == NULL)
  {
    gint32 l_delta;
    gint32 l_readsteps;
    
    l_readsteps = 1;
    l_delta = framenumber - gvahand->current_frame_nr;
    l_rc = GVA_RET_OK;

    if((l_delta >= 1) && (l_delta <= GVA_NEAR_FRAME_DISTANCE))
    {
      /* target framenumber is very near to the current_frame_nr
       * in this case positioning via sequential read is faster than seek
       */
      l_readsteps = l_delta;
    }
    else if ((l_delta < 0) && (isBackwards))
    {
      /* backwards seek is done to a position upto GVA_NEAR_FRAME_DISTANCE_BACK
       * frames before the wanted framenumber to fill up the fcache
       */
      gdouble seekFrameNumber;
      gdouble prefetchFrames;
           
           
      prefetchFrames = MIN((GVA_get_fcache_size_in_elements(gvahand) -1), GVA_NEAR_FRAME_DISTANCE_BACK) ;
      seekFrameNumber = MAX((gdouble)framenumber - prefetchFrames, 1);
      l_readsteps = 1 + (framenumber - seekFrameNumber);
      GVA_seek_frame(gvahand, seekFrameNumber, GVA_UPOS_FRAMES);
      
    }
    else
    {
      l_rc = GVA_seek_frame(gvahand, framenumber, GVA_UPOS_FRAMES);
    }

    while ((l_readsteps > 0) && (l_rc == GVA_RET_OK))
    {
      l_rc = GVA_get_next_frame(gvahand);
      if(gap_debug)
      {
        printf("GVA_fetch_frame_to_buffer: l_readsteps:%d framenumber;%d curr:%d l_rc:%d delta:%d fcacheSize:%d\n"
          , (int)l_readsteps
          , (int)framenumber
          , (int)gvahand->current_frame_nr
          , (int)l_rc
          , (int)l_delta
          , (int)GVA_get_fcache_size_in_elements(gvahand)
          );
      }
      l_readsteps--;
    }

    if(l_rc == GVA_RET_OK)
    {
      frame_data = GVA_frame_to_buffer(gvahand
                 , do_scale
                 , framenumber
                 , deinterlace
                 , threshold
                 , bpp
                 , width
                 , height
                 );
    }
  }
  return(frame_data);

}  /* end GVA_fetch_frame_to_buffer */
