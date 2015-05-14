/* gap_audio_extract.h
 *
 *  GAP extract audio from videofile procedures
 *
 */
/* 2008.06.24 hof  created (moved audio extract parts of gap_vex_exec.c to this module)
 */

#ifndef GAP_AUDIO_EXTRACT_H
#define GAP_AUDIO_EXTRACT_H

#include "config.h"

/* GIMP includes */
#include "gtk/gtk.h"
#include "libgimp/gimp.h"

/* GAP includes */


#ifdef GAP_ENABLE_VIDEOAPI_SUPPORT
#include "gap_vid_api.h"
#else
#ifndef GAP_STUBTYPE_GVA_HANDLE
typedef gpointer t_GVA_Handle;
#define GAP_STUBTYPE_GVA_HANDLE
#endif
#endif


#include "gap-intl.h"


#ifdef GAP_ENABLE_VIDEOAPI_SUPPORT

/* -------------------------
 * gap_audio_extract_as_wav
 * -------------------------
 * extract specified number of samples at current
 * position of the specified (already opened) videohandle.
 * and optional save extracted audiodata as RIFF WAVE file 
 * (set wav_save to FALSE to skip writing to wav file,
 *  this is typically used to perform dummy read for 
 *  advancing current position in the videohandle)
 */
void 
gap_audio_extract_as_wav(const char *audiofile
   ,  t_GVA_Handle   *gvahand
   ,  gdouble samples_to_read
   ,  gboolean wav_save
   ,  gboolean do_progress
   ,  GtkWidget *progressBar
   ,  t_GVA_progress_callback_fptr fptr_progress_callback
   ,  gpointer user_data
   );

/* ---------------------------------
 * gap_audio_extract_from_videofile
 * ---------------------------------
 * extract the specified audiotrack to WAVE file. (name specified via audiofile)
 * starting at position (specified by l_pos and l_pos_unit)
 * in length of extracted_frames (if number of frames is exactly known)
 * or in length expected_frames (is a guess if l_extracted_frames < 1)
 * use do_progress flag value TRUE for progress feedback on the specified
 * progressBar. 
 * (if progressBar = NULL gimp progress is used
 * this usually refers to the progress bar in the image window)
 * Note:
 *   this feature is not present if compiled without GAP_ENABLE_VIDEOAPI_SUPPORT
 */
void
gap_audio_extract_from_videofile(const char *videoname
   , const char *audiofile
   , gint32 audiotrack
   , const char *preferred_decoder
   , gint        exact_seek
   , t_GVA_PosUnit  pos_unit
   , gdouble        pos
   , gdouble        extracted_frames
   , gdouble        expected_frames
   , gboolean do_progress
   , GtkWidget *progressBar
   , t_GVA_progress_callback_fptr fptr_progress_callback
   , gpointer user_data
   );

#endif          /*   GAP_ENABLE_VIDEOAPI_SUPPORT */
#endif          /* end  GAP_AUDIO_EXTRACT_H */
