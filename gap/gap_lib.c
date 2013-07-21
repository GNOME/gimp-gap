/* gap_lib.c
 * 1997.11.18 hof (Wolfgang Hofer)
 *
 * GAP ... Gimp Animation Plugins
 *
 * basic GAP types and utility procedures
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

/* revision history:
 * 2.1.0a   2005/03/10   hof: added active_layer_tracking feature
 * 2.1.0a   2004/12/04   hof: added gap_lib (base)_shorten_filename
 * 2.1.0a   2004/04/18   hof: added gap_lib (base)_fprintf_gdouble
 * 1.3.26a  2004/02/01   hof: added: gap_lib_alloc_ainfo_from_name
 * 1.3.25a  2004/01/20   hof: - removed xvpics support (gap_lib_get_video_paste_name)
 * 1.3.21e  2003/11/04   hof: - gimprc
 * 1.3.18a  2003/08/23   hof: - all gap_debug messages to stdout (do not mix with stderr)
 *                            - do not save thumbnails in p_decide_save_as because it saves
 *                              to a temp filename that is renamed later after successful save
 * 1.3.16c  2003/07/12   hof: - triggers for automatic onionskinlayer create and remove
 *                              bugfix gap_vid_edit_paste
 * 1.3.14a  2003/05/27   hof: - moved basic gap operations to new module gap_base_ops
 *                            - moved procedures for thumbnail handling to gap_thumbnail
 *                            - conditional save now depends on
 *                               GIMP standard gimprc keyword "trust-dirty-flag"
 *                               (removed support for old GAP private keyword "video-unconditional-frame-save")
 * 1.3.12a; 2003/05/03   hof: merge into CVS-gimp-gap project, added gap_renumber, gap_lib_alloc_fname 6 digit support
 * 1.3.11a; 2003/01/19   hof: conditional SAVE  (based on gimp_image_is_dirty), use gimp_directory
 * 1.3.9a;  2002/10/28   hof: minor cleanup (replace strcpy by g_strdup)
 * 1.3.5a;  2002/04/21   hof: gimp_palette_refresh changed name to: gimp_palettes_refresh
 *                            gap_locking (now moved to gap_lock.c)
 * 1.3.4b;  2002/03/15   hof: gap_lib_load_named_frame now uses gimp_displays_reconnect (removed gap_exchange_image.h)
 * 1.3.4a;  2002/03/11   hof: port to gimp-1.3.4
 * 1.2.2a;  2001/10/21   hof: bufix # 61677 (error in duplicate frames GUI)
 *                            and disable duplicate for Unsaved/untitled Images.
 *                            (creating frames from such images with a default name may cause problems
 *                             as unexpected overwriting frames or mixing animations with different sized frames)
 * 1.2.1a;  2001/07/07   hof: gap_lib_file_copy use binary modes in fopen (hope that fixes bug #52890 in video/duplicate)
 * 1.1.29a; 2000/11/23   hof: gap locking (changed to procedures and placed here)
 * 1.1.28a; 2000/11/05   hof: check for GIMP_PDB_SUCCESS (not for FALSE)
 * 1.1.20a; 2000/04/25   hof: new: gap_lib_get_video_paste_name gap_vid_edit_clear
 * 1.1.17b; 2000/02/27   hof: bug/style fixes
 * 1.1.14a; 1999/12/18   hof: handle .xvpics on fileops (copy, rename and delete)
 *                            new: gap_lib_get_frame_nr,
 * 1.1.9a;  1999/09/14   hof: handle frame filenames with framenumbers
 *                            that are not the 4digit style. (like frame1.xcf)
 * 1.1.8a;  1999/08/31   hof: for AnimFrame Filtypes != XCF:
 *                            p_decide_save_as does save INTERACTIVE at 1.st time
 *                            and uses GIMP_RUN_WITH_LAST_VALS for subsequent calls
 *                            (this enables to set Fileformat specific save-Parameters
 *                            at least at the 1.st call, using the save dialog
 *                            of the selected (by gimp_file_save) file_save procedure.
 *                            in NONINTERACTIVE mode we have no access to
 *                            the Fileformat specific save-Parameters
 *          1999/07/22   hof: accept anim framenames without underscore '_'
 *                            (suggested by Samuel Meder)
 * 0.99.00; 1999/03/15   hof: prepared for win/dos filename conventions
 * 0.98.00; 1998/11/30   hof: started Port to GIMP 1.1:
 *                               exchange of Images (by next frame) is now handled in the
 *                               new module: gap_exchange_image.c
 * 0.96.02; 1998/07/30   hof: extended gap_dup (duplicate range instead of singele frame)
 *                            added gap_shift
 * 0.96.00               hof: - now using gap_arr_dialog.h
 * 0.95.00               hof:  increased duplicate frames limit from 50 to 99
 * 0.93.01               hof: fixup bug when frames are not in the current directory
 * 0.90.00;              hof: 1.st (pre) release
 */
#include "config.h"

/* SYSTEM (UNIX) includes */
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <signal.h>           /* for kill */
#ifdef HAVE_SYS_TIMES_H
#include <sys/times.h>
#endif
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include <glib/gstdio.h>

/* GIMP includes */
#include "gtk/gtk.h"
#include "gap-intl.h"
#include "libgimp/gimp.h"

#ifdef G_OS_WIN32
#include <io.h>
#  ifndef S_ISDIR
#    define S_ISDIR(m) ((m) & _S_IFDIR)
#  endif
#  ifndef S_ISREG
#    define S_ISREG(m) ((m) & _S_IFREG)
#  endif
#endif


#ifdef G_OS_WIN32
#include <process.h>            /* For _getpid() */
#endif

/* GAP includes */
#include "gap_arr_dialog.h"
#include "gap_image.h"
#include "gap_layer_copy.h"
#include "gap_lib.h"
#include "gap_lock.h"
#include "gap_navi_activtable.h"
#include "gap_onion_base.h"
#include "gap_pdb_calls.h"
#include "gap_thumbnail.h"
#include "gap_vin.h"

typedef struct
{
  gdouble  quality;
  gdouble  smoothing;
  gboolean optimize;
  gboolean progressive;
  gboolean baseline;
  gint     subsmp;
  gint     restart;
  gint     dct;
  gboolean preview;
  gboolean save_exif;
  gboolean save_thumbnail;
  gboolean save_xmp;
  gboolean use_orig_quality;
} GAPJpegSaveVals;

extern      int gap_debug; /* ==0  ... dont print debug infos */

/* ------------------------------------------ */
/* forward  working procedures */
/* ------------------------------------------ */

static gint32       p_set_or_pick_active_layer_by_pos(gint32 image_id
                         ,gint32 ref_layer_stackpos
                         ,gboolean setActiveLayer
			 ,gboolean ignoreOnionLayers
                         );
static gint32       p_set_or_pick_active_layer_by_name(gint32 image_id
                          ,const gchar *ref_layer_name
                          ,gint32 ref_layer_stackpos
                          ,gboolean setActiveLayer
                          ,gboolean ignoreOnionLayers
                          );


static gchar *      p_get_active_layer_name(gint32 image_id
                       ,gint32 *active_layer
                       ,gint32 *stack_pos
                       );
static void         p_do_active_layer_tracking(gint32 image_id
                       ,GapVinVideoInfo *vin_ptr
                       ,gchar *ref_layer_name
                       ,gint32 ref_layer_stackpos
                       );

static int          p_save_old_frame(GapAnimInfo *ainfo_ptr, GapVinVideoInfo *vin_ptr);
static int          p_decide_save_as(gint32 image_id, const char *sav_name, const char *final_sav_name);
static gint32       p_lib_save_jpg_non_interactive(gint32 image_id, gint32 l_drawable_id,
                         const char *sav_name,
                         const GAPJpegSaveVals *save_vals);
static gint32       p_lib_save_named_image_1(gint32 image_id, const char *sav_name, GimpRunMode run_mode, gboolean enable_thumbnailsave
                         , const char *l_basename
                         , const char *l_extension
                         );
static gint32       p_lib_save_named_image2(gint32 image_id, const char *sav_name, GimpRunMode run_mode,
                        gboolean enable_thumbnailsave, const GAPJpegSaveVals *jpg_save_vals);
static char*        p_gzip (char *orig_name, char *new_name, char *zip);


long     gap_lib_dir_find_next_frame_number(long cur_frame_nr, long stepsize,
            char *basename, char *extension, gboolean *frame_found_ptr);
long     gap_lib_dir_find_next_frame_number_for_imagename(long stepsize,
            char *imagename, gboolean *frame_found_ptr);



/* ------------------------------------
 * gap_lib_layer_tracking
 * ------------------------------------
 * try to locate the specified reference layer within the image by name or stack position.
 * this procedure is typically used to find a corresponding layer while
 * processing a sequence of frames or when stepping to another frame with
 * the "Active Layer Tracking" feature enabled.
 *
 * setActiveLayer: TRUE set the corresponding layer as active layer (when present)
 *                 FALSE do not change the active layer
 *
 * ignoreOnionLayers: TRUE ignore onion skin layers when present (useful for AL tracking)
 *                    FALSE do not ccare about onion skin layers.
 *
 * return the corresponding layer id  (that is part of to the target image_id)
 *        or -1 in case no corresponding layer was found.
 */
gint32
gap_lib_layer_tracking(gint32 image_id
                    , gchar *ref_layer_name
                    , gint32 ref_layer_stackpos
                    , gboolean setActiveLayer
                    , gboolean ignoreOnionLayers
                    , gboolean trackByName
                    , gboolean trackByStackPosition
                    )
{
  gint32 l_matching_layer_id;

  l_matching_layer_id = -1;
  if (trackByName)
  {
    l_matching_layer_id = p_set_or_pick_active_layer_by_name(image_id
                            ,ref_layer_name
                            ,ref_layer_stackpos
                            ,setActiveLayer
                            ,ignoreOnionLayers
                            );
    if (l_matching_layer_id >= 0)
    {
      return(l_matching_layer_id);
    }
  }

  if (trackByStackPosition)
  {
    l_matching_layer_id = p_set_or_pick_active_layer_by_pos(image_id
                            ,ref_layer_stackpos
                            ,setActiveLayer
                            ,ignoreOnionLayers
                            );
  }

  return(l_matching_layer_id);

}  /* end gap_lib_layer_tracking */


/* ---------------------------------
 * p_set_or_pick_active_layer_by_pos
 * ---------------------------------
 * set the layer with same stackposition as ref_layer_stackpos
 * as the active layer, where stackpositions of onionskin layers are not counted.
 * if the image has less layers than ref_layer_stackpos
 * then pick the top-most layer, that is no onionskin layer
 */
static gint32
p_set_or_pick_active_layer_by_pos(gint32 image_id
                         ,gint32 ref_layer_stackpos
                         ,gboolean setActiveLayer
			 ,gboolean ignoreOnionLayers
                         )
{
  gint32     *l_layers_list;
  gint        l_nlayers;
  gint        l_idx;
  gboolean    l_is_layer_ignored;
  gint32      l_layer_id;
  gint32      l_matching_layer_id;
  gint        l_pos;

  if(gap_debug)
  {
    printf("p_set_or_pick_active_layer_by_pos START\n");
  }
  l_pos = 0;
  l_matching_layer_id = -1;
  l_layers_list = gimp_image_get_layers(image_id, &l_nlayers);
  if(l_layers_list)
  {
    for(l_idx=0;l_idx < l_nlayers;l_idx++)
    {
      l_layer_id = l_layers_list[l_idx];
      if (ignoreOnionLayers == TRUE)
      {
        l_is_layer_ignored = gap_onion_base_check_is_onion_layer(l_layer_id);
      }
      else
      {
        l_is_layer_ignored = FALSE;
      }

      if(!l_is_layer_ignored)
      {
        l_matching_layer_id = l_layer_id;
        if(l_pos == ref_layer_stackpos)
        {
          break;
        }
        l_pos++;
      }
    }

    g_free(l_layers_list);
  }

  if(l_matching_layer_id >= 0)
  {
    if(setActiveLayer == TRUE)
    {
      gimp_image_set_active_layer(image_id, l_matching_layer_id);
    }

    if(gap_debug)
    {
      char       *l_name;

      l_name = gimp_drawable_get_name(l_matching_layer_id);
      if(setActiveLayer == TRUE)
      {
        printf("p_set_or_pick_active_layer_by_pos SET layer_id %d '%s' as ACTIVE\n"
            , (int)l_matching_layer_id
            , l_name
            );
      }
      else
      {
        printf("p_set_or_pick_active_layer_by_pos layer_id %d '%s' was PICKED\n"
            , (int)l_matching_layer_id
            , l_name
            );
      }
      if(l_name)
      {
        g_free(l_name);
      }
    }
    return (l_matching_layer_id);
  }

  return (-1);

}  /* end p_set_or_pick_active_layer_by_pos */



/* ----------------------------------
 * p_set_or_pick_active_layer_by_name
 * ----------------------------------
 * set active layer by picking the layer
 * who's name matches best with ref_layer_name.
 * ref_layer_stackpos is the 2nd criterium
 * restriction: works only for utf8 compliant layernames.
 */
static gint32
p_set_or_pick_active_layer_by_name(gint32 image_id
                          ,const gchar *ref_layer_name
                          ,gint32 ref_layer_stackpos
                          ,gboolean setActiveLayer
                          ,gboolean ignoreOnionLayers
                          )
{
  gint32     *l_layers_list;
  gint        l_nlayers;
  gint        l_idx;
  gboolean    l_is_layer_ignored;
  gint32      l_layer_id;
  gint32      l_matching_layer_id;
  gint        l_pos;
  gint        l_score;
  gint        l_case_bonus;
  gint        l_max_score;
  glong       l_ref_len;
  glong       l_len;
  char       *l_layer_name;

  if(gap_debug)
  {
    printf("p_set_or_pick_active_layer_by_name START\n");
  }
  l_pos = 0;
  l_max_score = 0;
  l_matching_layer_id = -1;

  if(ref_layer_name == NULL)
  {
    return(p_set_or_pick_active_layer_by_pos(image_id, ref_layer_stackpos, setActiveLayer, ignoreOnionLayers));
  }

  if(!g_utf8_validate(ref_layer_name, -1, NULL))
  {
    return(p_set_or_pick_active_layer_by_pos(image_id, ref_layer_stackpos, setActiveLayer, ignoreOnionLayers));
  }

  l_ref_len = g_utf8_strlen(ref_layer_name, -1);
  l_layers_list = gimp_image_get_layers(image_id, &l_nlayers);
  if(l_layers_list)
  {
    for(l_idx=0;l_idx < l_nlayers;l_idx++)
    {
      l_score = 0;
      l_case_bonus = 0;
      l_layer_id = l_layers_list[l_idx];
      l_layer_name = gimp_drawable_get_name(l_layer_id);
      if(l_layer_name)
      {
        gint ii;

        if(g_utf8_validate(l_layer_name, -1, NULL))
        {
          const char *uni_ref_ptr;
          const char *uni_nam_ptr;

          uni_ref_ptr = ref_layer_name;
          uni_nam_ptr = l_layer_name;

          /* check how many characters of the name are equal */
          l_len = g_utf8_strlen(l_layer_name, -1);
          for(ii=0; ii < MIN(l_len, l_ref_len); ii++)
          {
            gunichar refname_char;
            gunichar layername_char;

            refname_char = g_utf8_get_char(uni_ref_ptr);
            layername_char = g_utf8_get_char(uni_nam_ptr);

            if(g_unichar_toupper(layername_char) == g_unichar_toupper(refname_char))
            {
              /* best score for matching character */
              l_score += 8;
              if(layername_char == refname_char)
              {
                if(ii==0)
                {
                  /* prepare bonus for matching in case sensitivity */
                  l_case_bonus = 4;
                }
              }
              else
              {
                /* lost the bonus for exact matching in case sensitivity */
                l_case_bonus = 0;
              }
            }
            else
            {
              break;
            }
            uni_ref_ptr = g_utf8_find_next_char(uni_ref_ptr, NULL);
            uni_nam_ptr = g_utf8_find_next_char(uni_nam_ptr, NULL);
          }

          l_score += l_case_bonus;
          if(l_len == l_ref_len)
          {
            /* extra score for equal length */
            l_score += 2;
          }
        }

        g_free(l_layer_name);
      }

      if(l_score == l_max_score)
      {
        /* stackposition is checked as secondary criterium
         * (score +1) in case there are more names matching
         * in the same number of characters
         */
        if (ignoreOnionLayers == TRUE)
        {
          l_is_layer_ignored = gap_onion_base_check_is_onion_layer(l_layer_id);
        }
        else
        {
          l_is_layer_ignored = FALSE;
        }


        if(!l_is_layer_ignored)
        {
          if(l_pos == ref_layer_stackpos)
          {
            l_score += 1;
          }
          l_pos++;
        }
      }
      if(l_score > l_max_score)
      {
        l_matching_layer_id = l_layer_id;
        l_max_score = l_score;
      }
    }

    g_free(l_layers_list);
  }

  if(l_matching_layer_id >= 0)
  {
    if (setActiveLayer == TRUE)
    {
      gimp_image_set_active_layer(image_id, l_matching_layer_id);
    }

    if(gap_debug)
    {
      char       *l_name;

      l_name = gimp_drawable_get_name(l_matching_layer_id);
      if (setActiveLayer == TRUE)
      {
        printf("p_set_or_pick_active_layer_by_name SET layer_id %d '%s' as ACTIVE\n"
            , (int)l_matching_layer_id
            , l_name
            );
      }
      else
      {
        printf("p_set_or_pick_active_layer_by_name layer_id %d '%s' was PICKED\n"
            , (int)l_matching_layer_id
            , l_name
            );
      }
      if(l_name)
      {
        g_free(l_name);
      }
    }
    return (l_matching_layer_id);
  }

  return (-1);
}  /* end p_set_or_pick_active_layer_by_name */


/* ---------------------------------
 * p_get_active_layer_name
 * ---------------------------------
 * get id, name and stackposition of the
 * active_layer in the specified image.
 * (stackpostitions are counted without onionskin layers
 *  stackposition 0 is the background layer)
 */
gchar *
p_get_active_layer_name(gint32 image_id
                       ,gint32 *active_layer
                       ,gint32 *stack_pos
                       )
{
  gchar      *layer_name;
  gint32     *l_layers_list;
  gint        l_nlayers;
  gint        l_idx;
  gint        l_pos;
  gboolean    l_is_onion;
  gint32      l_layer_id;

  layer_name = NULL;

  *stack_pos = -1;
  *active_layer = gimp_image_get_active_layer(image_id);

  if(*active_layer >= 0)
  {
    layer_name = gimp_drawable_get_name(*active_layer);

    /* findout stackposition of the active layer
     * (ignoring onionskin layer positions)
     */
    l_layers_list = gimp_image_get_layers(image_id, &l_nlayers);
    if(l_layers_list)
    {
      l_pos = 0;
      for(l_idx=0;l_idx < l_nlayers;l_idx++)
      {
        l_layer_id = l_layers_list[l_idx];
        if(l_layer_id == *active_layer)
        {
          *stack_pos = l_pos;
          break;
        }

        l_is_onion = gap_onion_base_check_is_onion_layer(l_layer_id);
        if(!l_is_onion)
        {
          l_pos++;
        }
      }

      g_free(l_layers_list);
    }
  }

  return (layer_name);
}  /* end p_get_active_layer_name */


/* ---------------------------------
 * p_do_active_layer_tracking
 * ---------------------------------
 * set the active layer in the specified image
 * due to settings of active layer tracking
 */
void
p_do_active_layer_tracking(gint32 image_id
                       ,GapVinVideoInfo *vin_ptr
                       ,gchar *ref_layer_name
                       ,gint32 ref_layer_stackpos
                       )
{
  switch(vin_ptr->active_layer_tracking)
  {
    case GAP_ACTIVE_LAYER_TRACKING_BY_NAME:
      gap_lib_layer_tracking(image_id, ref_layer_name, ref_layer_stackpos
                            , TRUE  /* setActiveLayer */
                            , TRUE  /* ignoreOnionLayers */
                            , TRUE  /* trackByName */
                            , TRUE  /* trackByStackPosition */
                            );
      break;
    case GAP_ACTIVE_LAYER_TRACKING_BY_STACKPOS:
      gap_lib_layer_tracking(image_id, ref_layer_name, ref_layer_stackpos
                            , TRUE  /* setActiveLayer */
                            , TRUE  /* ignoreOnionLayers */
                            , FALSE /* trackByName */
                            , TRUE  /* trackByStackPosition */
                            );
      break;
    default: /* GAP_ACTIVE_LAYER_TRACKING_OFF */
      break;
  }
}  /* end p_do_active_layer_tracking */


/* ============================================================================
 * gap_lib_file_exists
 *
 * return 0  ... file does not exist, or is not accessible by user,
 *               or is empty,
 *               or is not a regular file
 *        1  ... file exists
 * ============================================================================
 */
int
gap_lib_file_exists(const char *fname)
{
  struct stat  l_stat_buf;
  long         l_len;

  /* File Laenge ermitteln */
  if (0 != g_stat(fname, &l_stat_buf))
  {
    /* stat error (file does not exist) */
    return(0);
  }

  if(!S_ISREG(l_stat_buf.st_mode))
  {
    return(0);
  }

  l_len = (long)l_stat_buf.st_size;
  if(l_len < 1)
  {
    /* File is empty*/
    return(0);
  }

  return(1);
}       /* end gap_lib_file_exists */

/* ============================================================================
 * gap_lib_searchpath_for_exefile
 * -----------------------------
 * search for executable file with given exefile name in given PATH
 * return NULL if not found,
 * else return full name of the exefile including the directorypath
 * (the returned string should be g_free'ed by the caller after usage)
 * ============================================================================
 */
char *
gap_lib_searchpath_for_exefile(const char *exefile, const char *path)
{
  char *exe_fullname;
  char *path_copy;
  char *dirpath;
  char *pp;
  gboolean break_flag;


  path_copy = g_strdup(path);
  exe_fullname = NULL;
  break_flag = FALSE;
  pp = path_copy;
  dirpath = path_copy;

  while(pp && break_flag == FALSE)
  {
    if (*pp == '\0')
    {
      break_flag = TRUE;
    }

    if(*pp == G_SEARCHPATH_SEPARATOR)
    {
      *pp = '\0';  /* terminate dirpath string at separator */
    }

    if (*pp == '\0')
    {
      exe_fullname = g_build_filename(dirpath, exefile, NULL);
      if(g_file_test (exe_fullname, G_FILE_TEST_IS_EXECUTABLE) )
      {
        /* the executable was found at exe_fullname,
         * set break flag and keep that name as return value
         */
        break_flag = TRUE;
      }
      else
      {
        g_free(exe_fullname);
        exe_fullname = NULL;
      }
      dirpath = pp;
      dirpath++;       /* start of next directoryname in the path string */
    }
    pp++;
  }

  g_free(path_copy);

  if (gap_debug)
  {
    printf("gap_lib_searchpath_for_exefile: path: %s\n", path);
    printf("gap_lib_searchpath_for_exefile: exe: %s\n", exefile);
    if(exe_fullname)
    {
      printf("gap_lib_searchpath_for_exefile: RET: %s\n", exe_fullname);
    }
    else
    {
      printf("gap_lib_searchpath_for_exefile: RET: NULL (not found)\n");
    }
  }

  return(exe_fullname);

}  /* end gap_lib_searchpath_for_exefile */


/* ============================================================================
 * gap_lib_image_file_copy
 *    (copy the imagefile and its thumbnail)
 * ============================================================================
 */
int
gap_lib_image_file_copy(char *fname, char *fname_copy)
{
   int            l_rc;

   l_rc = gap_lib_file_copy(fname, fname_copy);
   gap_thumb_file_copy_thumbnail(fname, fname_copy);
   return(l_rc);
}

/* ============================================================================
 * gap_lib_file_copy
 * ============================================================================
 */
int
gap_lib_file_copy(char *fname, char *fname_copy)
{
  FILE        *l_fp;
  char                     *l_buffer;
  struct stat               l_stat_buf;
  long         l_len;

  if(gap_debug) printf("gap_lib_file_copy src:%s dst:%s\n", fname, fname_copy);

  /* File Laenge ermitteln */
  if (0 != g_stat(fname, &l_stat_buf))
  {
    fprintf (stderr, "stat error on '%s'\n", fname);
    return -1;
  }
  l_len = (long)l_stat_buf.st_size;

  /* Buffer allocate */
  l_buffer=(char *)g_malloc0((size_t)l_len+1);
  if(l_buffer == NULL)
  {
    fprintf(stderr, "file_copy: calloc error (%ld Bytes not available)\n", l_len);
    return -1;
  }

  /* load File into Buffer */
  l_fp = g_fopen(fname, "rb");              /* open read */
  if(l_fp == NULL)
  {
    fprintf (stderr, "open(read) error on '%s'\n", fname);
    g_free(l_buffer);
    return -1;
  }
  fread(l_buffer, 1, (size_t)l_len, l_fp);
  fclose(l_fp);

  l_fp = g_fopen(fname_copy, "wb");                 /* open write */
  if(l_fp == NULL)
  {
    fprintf (stderr, "file_copy: open(write) error on '%s' \n", fname_copy);
    g_free(l_buffer);
    return -1;
  }

  if(l_len > 0)
  {
     fwrite(l_buffer,  l_len, 1, l_fp);
  }

  fclose(l_fp);
  g_free(l_buffer);
  return 0;           /* all done OK */
}       /* end gap_lib_file_copy */


/* ============================================================================
 * gap_lib_delete_frame
 * ============================================================================
 */
int
gap_lib_delete_frame(GapAnimInfo *ainfo_ptr, long nr)
{
   char          *l_fname;
   int            l_rc;

   l_fname = gap_lib_alloc_fname(ainfo_ptr->basename, nr, ainfo_ptr->extension);
   if(l_fname == NULL) { return(1); }

   if(gap_debug) printf("\nDEBUG gap_lib_delete_frame: %s\n", l_fname);
   l_rc = g_remove(l_fname);

   gap_thumb_file_delete_thumbnail(l_fname);

   g_free(l_fname);

   return(l_rc);

}    /* end gap_lib_delete_frame */


/* ---------------------------------
 * gap_lib_count_framenumber_digits
 * ---------------------------------
 * count the number of digits used in the framenumber part of the imagename
 */
long
gap_lib_count_framenumber_digits(const char *imagename)
{
  char *l_fname;
  char *l_ptr;
  long  l_digits_used;

  if(imagename == NULL)
  {
    return (-1);
  }

  /* copy from imagename */
  l_fname = g_strdup(imagename);

  /* cut off extension */
  l_ptr = &l_fname[strlen(l_fname)];
  while(l_ptr != l_fname)
  {
    if((*l_ptr == G_DIR_SEPARATOR) || (*l_ptr == DIR_ROOT))  { break; }  /* dont run into dir part */
    if(*l_ptr == '.')                                        { *l_ptr = '\0'; break; }
    l_ptr--;
  }
  if(gap_debug)
  {
    printf("DEBUG gap_lib_count_framenumber_digits (ext_off): '%s'\n", l_fname);
  }

  /* count the digits at end of fname */
  l_ptr = &l_fname[strlen(l_fname)];
  if(l_ptr != l_fname)
  {
    l_ptr--;
  }
  l_digits_used = 0;
  while(TRUE)
  {
    if((*l_ptr >= '0') && (*l_ptr <= '9'))
    {
      l_digits_used++;
    }
    else
    {
       break;  /* stop when found a character that is n digit */
    }
    if(l_ptr == l_fname)
    {
      break;  /* stop when no more characters left to check */
    }

    l_ptr--;
  }

  if(gap_debug)
  {
    printf("DEBUG gap_lib_count_framenumber_digits  imagename:'%s' digits_used:%d\n"
      , l_fname
      , (int)l_digits_used
      );
  }


  g_free(l_fname);
  return(l_digits_used);

}    /* end gap_lib_count_framenumber_digits */



/* ----------------------------------
 * gap_lib_rename_frame
 * ----------------------------------
 * rename the frame imagefile with from_nr
 * by building a filename where the number part is replaced by specified to_nr.
 *
 * the number of digits in the new filename is set to same size as in the original
 * frame imagefile.
 */
int
gap_lib_rename_frame(GapAnimInfo *ainfo_ptr, long from_nr, long to_nr)
{
   char          *l_from_fname;
   char          *l_to_fname;
   int            l_rc;
   long           l_digits_used;

   l_from_fname = gap_lib_alloc_fname(ainfo_ptr->basename, from_nr, ainfo_ptr->extension);
   if(l_from_fname == NULL)
   {
     return(1);
   }

   l_digits_used = gap_lib_count_framenumber_digits(l_from_fname);
   if (l_digits_used > 0)
   {
     l_to_fname = gap_lib_alloc_fname6(ainfo_ptr->basename, to_nr, ainfo_ptr->extension, l_digits_used);
   }
   else
   {
     /* this should not occur when the frame imagfile with from_nr already exists */
     l_to_fname = gap_lib_alloc_fname(ainfo_ptr->basename, to_nr, ainfo_ptr->extension);
   }

   if(l_to_fname == NULL)
   {
     g_free(l_from_fname);
     return(1);
   }


   if(gap_debug) printf("\nDEBUG gap_lib_rename_frame: %s ..to.. %s\n", l_from_fname, l_to_fname);
   l_rc = g_rename(l_from_fname, l_to_fname);

   gap_thumb_file_rename_thumbnail(l_from_fname, l_to_fname);


   g_free(l_from_fname);
   g_free(l_to_fname);

   return(l_rc);

}    /* end gap_lib_rename_frame */


/* ============================================================================
 * gap_lib_alloc_basename
 *
 * build the basename from an images name
 * Extension and trailing digits ("0000.xcf") are cut off.
 * return name or NULL (if malloc fails)
 * Output: number contains the integer representation of the stripped off
 *         number String. (or 0 if there was none)
 * ============================================================================
 */
char *
gap_lib_alloc_basename(const char *imagename, long *number)
{
  char *l_fname;
  char *l_ptr;
  long  l_idx;

  *number = 0;
  if(imagename == NULL) return (NULL);

  /* copy from imagename */
  l_fname = g_strdup(imagename);

  if(gap_debug) printf("DEBUG gap_lib_alloc_basename  source: '%s'\n", l_fname);
  /* cut off extension */
  l_ptr = &l_fname[strlen(l_fname)];
  while(l_ptr != l_fname)
  {
    if((*l_ptr == G_DIR_SEPARATOR) || (*l_ptr == DIR_ROOT))  { break; }  /* dont run into dir part */
    if(*l_ptr == '.')                                        { *l_ptr = '\0'; break; }
    l_ptr--;
  }
  if(gap_debug) printf("DEBUG gap_lib_alloc_basename (ext_off): '%s'\n", l_fname);

  /* cut off trailing digits (0000) */
  l_ptr = &l_fname[strlen(l_fname)];
  if(l_ptr != l_fname) l_ptr--;
  l_idx = 1;
  while(TRUE)
  {
    if((*l_ptr >= '0') && (*l_ptr <= '9'))
    {
      *number += ((*l_ptr - '0') * l_idx);
       l_idx = l_idx * 10;
      *l_ptr = '\0';
    }
    else
    {
      /* do not cut off underscore any longer */
      /*
       * if(*l_ptr == '_')
       * {
       *    *l_ptr = '\0';
       * }
       */
       break;
    }
    if(l_ptr == l_fname)
    {
      break;
    }

    l_ptr--;
  }

  if(gap_debug) printf("DEBUG gap_lib_alloc_basename  result:'%s'\n", l_fname);

  return(l_fname);

}    /* end gap_lib_alloc_basename */



/* ============================================================================
 * gap_lib_alloc_extension
 *
 * input: a filename
 * returns: a copy of the filename extension (incl "." )
 *          if the filename has no extension, a pointer to
 *          an empty string is returned ("\0")
 *          NULL if allocate mem for extension failed.
 * ============================================================================
 */
char *
gap_lib_alloc_extension(const char *imagename)
{
  int   l_exlen;
  char *l_ext;
  const char *l_ptr;

  l_exlen = 0;
  l_ptr = &imagename[strlen(imagename)];
  while(l_ptr != imagename)
  {
    if((*l_ptr == G_DIR_SEPARATOR) || (*l_ptr == DIR_ROOT))  { break; }     /* dont run into dir part */
    if(*l_ptr == '.')                                        { l_exlen = strlen(l_ptr); break; }
    l_ptr--;
  }

  if(l_exlen > 0)
  {
    l_ext = g_strdup(l_ptr);
  }
  else
  {
    l_ext = g_strdup("\0");
  }

  return(l_ext);
}

/* -------------------------------------
 * gap_lib_build_basename_without_ext
 * -------------------------------------
 * return a duplicate of the basename part of the specified filename without extension.
 *        leading directory path and drive letter (for WinOS) is cut off
 * the caller is responsible to g_free the returned string.
 */
char *
gap_lib_build_basename_without_ext(const char *filename)
{
  char *basename;
  gint  idx;

  basename = g_filename_display_basename(filename);
  idx = strlen(basename) -1;
  while(idx > 0)
  {
    if(basename[idx] == '.')
    {
      basename[idx] = '\0';
      break;
    }
    if((basename[idx] == G_DIR_SEPARATOR) || (basename[idx] == DIR_ROOT))
    {
      break;    /* dont run into dir part */
    }

    idx--;
  }
  return(basename);
}  /* end gap_lib_build_basename_without_ext */



/* ----------------------------------
 * gap_lib_alloc_fname_fixed_digits
 * ----------------------------------
 * build the framname by concatenating basename, nr and extension.
 * the Number part has leading zeroes, depending
 * on the number of digits specified.
 */
char*
gap_lib_alloc_fname_fixed_digits(char *basename, long nr, char *extension, long digits)
{
  gchar *l_fname;
  gint   l_len;

  if(basename == NULL) return (NULL);
  l_len = (strlen(basename)  + strlen(extension) + 10);
  l_fname = (char *)g_malloc(l_len);

  switch(digits)
  {
    case 8:  l_fname = g_strdup_printf("%s%08ld%s", basename, nr, extension);
             break;
    case 7:  l_fname = g_strdup_printf("%s%07ld%s", basename, nr, extension);
             break;
    case 6:  l_fname = g_strdup_printf("%s%06ld%s", basename, nr, extension);
             break;
    case 5:  l_fname = g_strdup_printf("%s%05ld%s", basename, nr, extension);
             break;
    case 4:  l_fname = g_strdup_printf("%s%04ld%s", basename, nr, extension);
             break;
    case 3:  l_fname = g_strdup_printf("%s%03ld%s", basename, nr, extension);
             break;
    case 2:  l_fname = g_strdup_printf("%s%02ld%s", basename, nr, extension);
             break;
    default: l_fname = g_strdup_printf("%s%ld%s", basename, nr, extension);
             break;
  }
  return(l_fname);
}    /* end gap_lib_alloc_fname_fixed_digits */


/* ============================================================================
 * gap_lib_alloc_fname
 * ============================================================================
 * at 1st call check environment
 * to findout how many digits (leading zeroes) to use in framename numbers
 * per default.
 */
char*
gap_lib_alloc_fname(char *basename, long nr, char *extension)
{
  static long default_digits = -1;

  if (default_digits < 0)
  {
    const char   *l_env;

    default_digits = GAP_LIB_DEFAULT_DIGITS;

    l_env = g_getenv("GAP_FRAME_DIGITS");
    if(l_env != NULL)
    {
      default_digits = atol(l_env);
      default_digits = CLAMP(default_digits, 1 , GAP_LIB_MAX_DIGITS);
    }
  }

  return (gap_lib_alloc_fname6(basename, nr, extension, default_digits));
}    /* end gap_lib_alloc_fname */

/* ----------------------------------
 * gap_lib_alloc_fname6
 * ----------------------------------
 * build the framname by concatenating basename, nr and extension.
 * the Number part has leading zeroes, depending
 * on filenames with the same basename and extension on disc.
 *
 * if no similar discfiles were found default_digits (with leading zeroes)
 * are used per default.
 *
 * if a similar discfilename is found, the number of digits/leading zeroes
 * is set equal to the discfile found.
 * example:
 *   basename == "frame_", nr == 5, ext == ".xcf"
 *   - discfile was found with name:  "frame_00001.xcf"
 *     return ("frame_00005.xcf");
 *
 *   - discfile was found with name:  "frame_001.xcf"
 *     return ("frame_005.xcf");
 *
 * return the resulting framename string
 *   (the calling program should g_free this string
 *    after use)
 */
char*
gap_lib_alloc_fname6(char *basename, long nr, char *extension, long default_digits)
{
  gchar *l_fname;
  gint   l_digits_used;
  gint   l_len;
  long   l_nr_chk;

  if(basename == NULL) return (NULL);
  l_len = (strlen(basename)  + strlen(extension) + 10);
  l_fname = (char *)g_malloc(l_len);

    l_digits_used = default_digits;
    if(nr < 100000000)
    {
       /* try to figure out if the frame numbers are in
        * 6-digit style, with leading zeroes  "frame_000001.xcf"
        * 4-digit style, with leading zeroes  "frame_0001.xcf"
        * or not                              "frame_1.xcf"
        */
       l_nr_chk = nr;

       while(l_nr_chk >= 0)
       {
         /* check if frame is on disk with 6-digit style framenumber
          * (we check 6-digit style first because this is the GAP default style)
          */
         g_snprintf(l_fname, l_len, "%s%06ld%s", basename, l_nr_chk, extension);
         if (gap_lib_file_exists(l_fname))
         {
            l_digits_used = 6;
            break;
         }

         /* check if frame is on disk with 8-digit style framenumber */
         g_snprintf(l_fname, l_len, "%s%08ld%s", basename, l_nr_chk, extension);
         if (gap_lib_file_exists(l_fname))
         {
            l_digits_used = 8;
            break;
         }

         /* check if frame is on disk with 7-digit style framenumber */
         g_snprintf(l_fname, l_len, "%s%07ld%s", basename, l_nr_chk, extension);
         if (gap_lib_file_exists(l_fname))
         {
            l_digits_used = 7;
            break;
         }




         /* check if frame is on disk with 4-digit style framenumber */
         g_snprintf(l_fname, l_len, "%s%04ld%s", basename, l_nr_chk, extension);
         if (gap_lib_file_exists(l_fname))
         {
            l_digits_used = 4;
            break;
         }

         /* check if frame is on disk with 5-digit style framenumber */
         g_snprintf(l_fname, l_len, "%s%05ld%s", basename, l_nr_chk, extension);
         if (gap_lib_file_exists(l_fname))
         {
            l_digits_used = 5;
            break;
         }

         /* check if frame is on disk with 3-digit style framenumber */
         g_snprintf(l_fname, l_len, "%s%03ld%s", basename, l_nr_chk, extension);
         if (gap_lib_file_exists(l_fname))
         {
            l_digits_used = 3;
            break;
         }

         /* check if frame is on disk with 2-digit style framenumber */
         g_snprintf(l_fname, l_len, "%s%02ld%s", basename, l_nr_chk, extension);
         if (gap_lib_file_exists(l_fname))
         {
            l_digits_used = 2;
            break;
         }



         /* now check for filename without leading zeroes in the framenumber */
         g_snprintf(l_fname, l_len, "%s%ld%s", basename, l_nr_chk, extension);
         if (gap_lib_file_exists(l_fname))
         {
            l_digits_used = 1;
            break;
         }
         l_nr_chk--;

         /* if the frames nr and nr-1  were not found
          * try to check frames 1 and 0
          * to limit down the loop to max 4 cycles
          */
         if((l_nr_chk == nr -2) && (l_nr_chk > 1))
         {
           l_nr_chk = 1;
         }
      }
    }
    else
    {
      /* numbers > 100000000 have 9 digits or more */
      l_digits_used = 0;
    }

  g_free(l_fname);

  switch(l_digits_used)
  {
    case 6:  l_fname = g_strdup_printf("%s%06ld%s", basename, nr, extension);
             break;
    case 8:  l_fname = g_strdup_printf("%s%08ld%s", basename, nr, extension);
             break;
    case 7:  l_fname = g_strdup_printf("%s%07ld%s", basename, nr, extension);
             break;
    case 5:  l_fname = g_strdup_printf("%s%05ld%s", basename, nr, extension);
             break;
    case 4:  l_fname = g_strdup_printf("%s%04ld%s", basename, nr, extension);
             break;
    case 3:  l_fname = g_strdup_printf("%s%03ld%s", basename, nr, extension);
             break;
    case 2:  l_fname = g_strdup_printf("%s%02ld%s", basename, nr, extension);
             break;
    default: l_fname = g_strdup_printf("%s%ld%s", basename, nr, extension);
             break;
  }
  return(l_fname);
}    /* end gap_lib_alloc_fname6 */


/* ----------------------------------
 * gap_lib_exists_frame_nr
 * ----------------------------------
 * check if frame with nr does exist
 * and find out how much digits are used for the number part
 */
gboolean
gap_lib_exists_frame_nr(GapAnimInfo *ainfo_ptr, long nr, long *l_has_digits)
{
  gchar *l_fname;
  gint   l_digits_used;
  gint   l_len;
  long   l_nr_chk;
  gboolean l_exists;

  l_exists = FALSE;

  if(ainfo_ptr->basename == NULL) return (FALSE);
  l_len = (strlen(ainfo_ptr->basename)  + strlen(ainfo_ptr->extension) + 10);
  l_fname = (char *)g_malloc(l_len);

  l_digits_used = GAP_LIB_DEFAULT_DIGITS;
  l_nr_chk = nr;

  while(l_nr_chk >= 0)
  {
     /* check if frame is on disk with 6-digit style framenumber */
     g_snprintf(l_fname, l_len, "%s%06ld%s", ainfo_ptr->basename, l_nr_chk, ainfo_ptr->extension);
     if (gap_lib_file_exists(l_fname))
     {
        l_digits_used = 6;
        if(l_nr_chk == nr)
        {
          l_exists = TRUE;
        }
        break;
     }

     /* check if frame is on disk with 8-digit style framenumber */
     g_snprintf(l_fname, l_len, "%s%08ld%s", ainfo_ptr->basename, l_nr_chk, ainfo_ptr->extension);
     if (gap_lib_file_exists(l_fname))
     {
        l_digits_used = 8;
        if(l_nr_chk == nr)
        {
          l_exists = TRUE;
        }
        break;
     }

     /* check if frame is on disk with 7-digit style framenumber */
     g_snprintf(l_fname, l_len, "%s%07ld%s", ainfo_ptr->basename, l_nr_chk, ainfo_ptr->extension);
     if (gap_lib_file_exists(l_fname))
     {
        l_digits_used = 7;
        if(l_nr_chk == nr)
        {
          l_exists = TRUE;
        }
        break;
     }






     /* check if frame is on disk with 4-digit style framenumber */
     g_snprintf(l_fname, l_len, "%s%04ld%s", ainfo_ptr->basename, l_nr_chk, ainfo_ptr->extension);
     if (gap_lib_file_exists(l_fname))
     {
        l_digits_used = 4;
        if(l_nr_chk == nr)
        {
          l_exists = TRUE;
        }
        break;
     }

     /* check if frame is on disk with 5-digit style framenumber */
     g_snprintf(l_fname, l_len, "%s%05ld%s", ainfo_ptr->basename, l_nr_chk, ainfo_ptr->extension);
     if (gap_lib_file_exists(l_fname))
     {
        l_digits_used = 5;
        if(l_nr_chk == nr)
        {
          l_exists = TRUE;
        }
        break;
     }

     /* check if frame is on disk with 3-digit style framenumber */
     g_snprintf(l_fname, l_len, "%s%03ld%s", ainfo_ptr->basename, l_nr_chk, ainfo_ptr->extension);
     if (gap_lib_file_exists(l_fname))
     {
        l_digits_used = 3;
        if(l_nr_chk == nr)
        {
          l_exists = TRUE;
        }
        break;
     }

     /* check if frame is on disk with 2-digit style framenumber */
     g_snprintf(l_fname, l_len, "%s%02ld%s", ainfo_ptr->basename, l_nr_chk, ainfo_ptr->extension);
     if (gap_lib_file_exists(l_fname))
     {
        l_digits_used = 2;
        if(l_nr_chk == nr)
        {
          l_exists = TRUE;
        }
        break;
     }


     /* now check for filename without leading zeroes in the framenumber */
     g_snprintf(l_fname, l_len, "%s%ld%s", ainfo_ptr->basename, l_nr_chk, ainfo_ptr->extension);
     if (gap_lib_file_exists(l_fname))
     {
        l_digits_used = 1;
        if(l_nr_chk == nr)
        {
          l_exists = TRUE;
        }
        break;
     }
     l_nr_chk--;

     /* if the frames nr and nr-1  were not found
      * try to check frames 1 and 0
      * to limit down the loop to max 4 cycles
      */
     if((l_nr_chk == nr -2) && (l_nr_chk > 1))
     {
       l_nr_chk = 1;
     }
  }

  g_free(l_fname);

  *l_has_digits = l_digits_used;

  return(l_exists);
}    /* end gap_lib_exists_frame_nr */


/* ============================================================================
 * gap_lib_alloc_ainfo_from_name
 *
 * allocate and init an ainfo structure from the given imagename
 * (use this to get anim informations if none of the frame is loaded
 *  as image into the gimp
 *  and no image_id is available)
 * The ainfo_type is just a first guess.
 *  (check for videofiles GAP_AINFO_MOVIE is not supported here,
 *   because this would require an video-api open attempt that would slow down)
 * ============================================================================
 */
GapAnimInfo *
gap_lib_alloc_ainfo_from_name(const char *imagename, GimpRunMode run_mode)
{
   GapAnimInfo   *l_ainfo_ptr;

   if(imagename == NULL)
   {
     return (NULL);
   }

   l_ainfo_ptr = (GapAnimInfo*)g_malloc(sizeof(GapAnimInfo));
   if(l_ainfo_ptr == NULL) return(NULL);

   l_ainfo_ptr->basename = NULL;
   l_ainfo_ptr->new_filename = NULL;
   l_ainfo_ptr->extension = NULL;
   l_ainfo_ptr->image_id = -1;

   l_ainfo_ptr->old_filename = g_strdup(imagename);

   l_ainfo_ptr->basename = gap_lib_alloc_basename(l_ainfo_ptr->old_filename, &l_ainfo_ptr->frame_nr);
   if(NULL == l_ainfo_ptr->basename)
   {
       gap_lib_free_ainfo(&l_ainfo_ptr);
       return(NULL);
   }

   l_ainfo_ptr->ainfo_type = GAP_AINFO_IMAGE;
   if(l_ainfo_ptr->frame_nr > 0)
   {
     l_ainfo_ptr->ainfo_type = GAP_AINFO_FRAMES;
   }
   l_ainfo_ptr->extension = gap_lib_alloc_extension(l_ainfo_ptr->old_filename);

   l_ainfo_ptr->curr_frame_nr = l_ainfo_ptr->frame_nr;
   l_ainfo_ptr->first_frame_nr = -1;
   l_ainfo_ptr->last_frame_nr = -1;
   l_ainfo_ptr->frame_cnt = 0;
   l_ainfo_ptr->run_mode = run_mode;
   l_ainfo_ptr->frame_nr_before_curr_frame_nr = -1;   /* -1 if no frame found before curr_frame_nr */
   l_ainfo_ptr->frame_nr_after_curr_frame_nr = -1;    /* -1 if no frame found after curr_frame_nr */


   return(l_ainfo_ptr);

}    /* end gap_lib_alloc_ainfo_from_name */


/* ============================================================================
 * gap_lib_alloc_ainfo_unsaved_image
 *
 * allocate and init an ainfo structure from unsaved image.
 * ============================================================================
 */
GapAnimInfo *
gap_lib_alloc_ainfo_unsaved_image(gint32 image_id)
{
   GapAnimInfo   *l_ainfo_ptr;

   l_ainfo_ptr = (GapAnimInfo*)g_malloc(sizeof(GapAnimInfo));
   if(l_ainfo_ptr == NULL) return(NULL);

   l_ainfo_ptr->basename = NULL;
   l_ainfo_ptr->new_filename = NULL;
   l_ainfo_ptr->extension = NULL;
   l_ainfo_ptr->image_id = image_id;

   l_ainfo_ptr->old_filename = NULL;

   l_ainfo_ptr->ainfo_type = GAP_AINFO_IMAGE;
   l_ainfo_ptr->extension = NULL;

   l_ainfo_ptr->frame_nr = 0;
   l_ainfo_ptr->curr_frame_nr = 0;
   l_ainfo_ptr->first_frame_nr = -1;
   l_ainfo_ptr->last_frame_nr = -1;
   l_ainfo_ptr->frame_cnt = 0;
   l_ainfo_ptr->run_mode = GIMP_RUN_NONINTERACTIVE;
   l_ainfo_ptr->frame_nr_before_curr_frame_nr = -1;   /* -1 if no frame found before curr_frame_nr */
   l_ainfo_ptr->frame_nr_after_curr_frame_nr = -1;    /* -1 if no frame found after curr_frame_nr */


   return(l_ainfo_ptr);

}    /* end gap_lib_alloc_ainfo_unsaved_image */



/* ============================================================================
 * gap_lib_alloc_ainfo
 *
 * allocate and init an ainfo structure from the given image.
 * ============================================================================
 */
GapAnimInfo *
gap_lib_alloc_ainfo(gint32 image_id, GimpRunMode run_mode)
{
   GapAnimInfo   *l_ainfo_ptr;

   l_ainfo_ptr = (GapAnimInfo*)g_malloc(sizeof(GapAnimInfo));
   if(l_ainfo_ptr == NULL) return(NULL);

   l_ainfo_ptr->basename = NULL;
   l_ainfo_ptr->new_filename = NULL;
   l_ainfo_ptr->extension = NULL;
   l_ainfo_ptr->image_id = image_id;

   /* get current gimp images name  */
   l_ainfo_ptr->old_filename = gimp_image_get_filename(image_id);
   if(l_ainfo_ptr->old_filename == NULL)
   {
     /* note: some gimp versions > 1.2  and < 1.3.x had default filenames for new created images
      * gimp-1.3.14 delivers NULL for unnamed images again
      */
     l_ainfo_ptr->old_filename = gap_lib_alloc_fname("frame_", 1, ".xcf");    /* assign a defaultname */
     gimp_image_set_filename (image_id, l_ainfo_ptr->old_filename);
   }

   l_ainfo_ptr->basename = gap_lib_alloc_basename(l_ainfo_ptr->old_filename, &l_ainfo_ptr->frame_nr);
   if(NULL == l_ainfo_ptr->basename)
   {
       gap_lib_free_ainfo(&l_ainfo_ptr);
       return(NULL);
   }

   l_ainfo_ptr->ainfo_type = GAP_AINFO_IMAGE;
   if(l_ainfo_ptr->frame_nr > 0)
   {
     l_ainfo_ptr->ainfo_type = GAP_AINFO_FRAMES;
   }
   l_ainfo_ptr->extension = gap_lib_alloc_extension(l_ainfo_ptr->old_filename);

   l_ainfo_ptr->curr_frame_nr = l_ainfo_ptr->frame_nr;
   l_ainfo_ptr->first_frame_nr = -1;
   l_ainfo_ptr->last_frame_nr = -1;
   l_ainfo_ptr->frame_cnt = 0;
   l_ainfo_ptr->run_mode = run_mode;
   l_ainfo_ptr->frame_nr_before_curr_frame_nr = -1;   /* -1 if no frame found before curr_frame_nr */
   l_ainfo_ptr->frame_nr_after_curr_frame_nr = -1;    /* -1 if no frame found after curr_frame_nr */


   return(l_ainfo_ptr);

}    /* end gap_lib_alloc_ainfo */

/* ============================================================================
 * gap_lib_dir_ainfo
 *
 * fill in more items into the given ainfo structure.
 * - first_frame_nr
 * - last_frame_nr
 * - frame_cnt
 *
 * to get this information, the directory entries have to be checked
 * ============================================================================
 */
int
gap_lib_dir_ainfo(GapAnimInfo *ainfo_ptr)
{
   char          *l_dirname;
   char          *l_dirname_ptr;
   char          *l_ptr;
   const char    *l_exptr;
   char          *l_dummy;
   GDir          *l_dirp;
   const gchar   *l_entry;
   long           l_nr;
   long           l_maxnr;
   long           l_minnr;
   short          l_dirflag;
   char           dirname_buff[1024];

   ainfo_ptr->frame_cnt = 0;
   l_dirp = NULL;
   l_minnr = 99999999;
   l_maxnr = 0;
   l_dirname = g_strdup(ainfo_ptr->basename);

   l_ptr = &l_dirname[strlen(l_dirname)];
   while(l_ptr != l_dirname)
   {
      if ((*l_ptr == G_DIR_SEPARATOR) || (*l_ptr == DIR_ROOT))
      {
        *l_ptr = '\0';   /* split the string into dirpath 0 basename */
        l_ptr++;
        break;           /* stop at char behind the slash */
      }
      l_ptr--;
   }

   if(gap_debug) printf("DEBUG gap_lib_dir_ainfo: BASENAME:%s\n", l_ptr);

   if      (l_ptr == l_dirname)   { l_dirname_ptr = ".";  l_dirflag = 0; }
   else if (*l_dirname == '\0')   { l_dirname_ptr = G_DIR_SEPARATOR_S ; l_dirflag = 1; }
   else                           { l_dirname_ptr = l_dirname; l_dirflag = 2; }

   if(gap_debug) printf("DEBUG gap_lib_dir_ainfo: DIRNAME:%s\n", l_dirname_ptr);
   l_dirp = g_dir_open( l_dirname_ptr, 0, NULL );

   if(!l_dirp) fprintf(stderr, "ERROR gap_lib_dir_ainfo: can't read directory %s\n", l_dirname_ptr);
   else
   {
     while ( (l_entry = g_dir_read_name( l_dirp )) != NULL )
     {

       /* if(gap_debug) printf("DEBUG gap_lib_dir_ainfo: l_entry:%s\n", l_entry); */


       /* findout extension of the directory entry name */
       l_exptr = &l_entry[strlen(l_entry)];
       while(l_exptr != l_entry)
       {
         if(*l_exptr == G_DIR_SEPARATOR) { break; }                 /* dont run into dir part */
         if(*l_exptr == '.')       { break; }
         l_exptr--;
       }
       /* l_exptr now points to the "." of the direntry (or to its begin if has no ext) */
       /* now check for equal extension */
       if((*l_exptr == '.') && (0 == strcmp(l_exptr, ainfo_ptr->extension)))
       {
         /* build full pathname (to check if file exists) */
         switch(l_dirflag)
         {
           case 0:
            g_snprintf(dirname_buff, sizeof(dirname_buff), "%s", l_entry);
            break;
           case 1:
            g_snprintf(dirname_buff, sizeof(dirname_buff), "%c%s",  G_DIR_SEPARATOR, l_entry);
            break;
           default:
            /* UNIX:  "/dir/file"
             * DOS:   "drv:\dir\file"
             */
            g_snprintf(dirname_buff, sizeof(dirname_buff), "%s%c%s", l_dirname_ptr,  G_DIR_SEPARATOR,  l_entry);
            break;
         }

         if(1 == gap_lib_file_exists(dirname_buff)) /* check for regular file */
         {
           /* get basename and frame number of the directory entry */
           l_dummy = gap_lib_alloc_basename(l_entry, &l_nr);
           if(l_dummy != NULL)
           {
               /* check for files, with equal basename (frames)
                * (length must be greater than basepart+extension
                * because of the frame_nr part "0000")
                */
               if((0 == strcmp(l_ptr, l_dummy))
               && ( strlen(l_entry) > strlen(l_dummy) + strlen(l_exptr)  ))
               {
                 ainfo_ptr->frame_cnt++;


                 if(gap_debug)
                 {
                   printf("DEBUG gap_lib_dir_ainfo:  %s NR=%ld  Curr:%ld\n", l_entry, l_nr, ainfo_ptr->curr_frame_nr);
                 }

                 if (l_nr > l_maxnr)
                 {
                    l_maxnr = l_nr;
                 }
                 if (l_nr < l_minnr)
                 {
                    l_minnr = l_nr;
                 }

                 if ((l_nr < ainfo_ptr->curr_frame_nr) 
                 && (l_nr > ainfo_ptr->frame_nr_before_curr_frame_nr))
                 {
                   ainfo_ptr->frame_nr_before_curr_frame_nr = l_nr;
                 }
                 if (l_nr > ainfo_ptr->curr_frame_nr)
                 {
                   if ((ainfo_ptr->frame_nr_after_curr_frame_nr < 0)
                   || (l_nr < ainfo_ptr->frame_nr_after_curr_frame_nr))
                   {
                     ainfo_ptr->frame_nr_after_curr_frame_nr = l_nr;
                   }
                 }

               }

               g_free(l_dummy);
           }
         }
       }
     }
     g_dir_close( l_dirp );
   }

  g_free(l_dirname);

  /* set first_frame_nr and last_frame_nr (found as "_0099" in diskfile namepart) */
  ainfo_ptr->last_frame_nr = l_maxnr;
  ainfo_ptr->first_frame_nr = MIN(l_minnr, l_maxnr);

  return 0;           /* OK */
}       /* end gap_lib_dir_ainfo */


/* ============================================================================
 * gap_lib_free_ainfo
 *
 * free ainfo and its allocated stuff
 * ============================================================================
 */

void
gap_lib_free_ainfo(GapAnimInfo **ainfo)
{
  GapAnimInfo *aptr;

  if((aptr = *ainfo) == NULL)
     return;

  if(aptr->basename)
     g_free(aptr->basename);

  if(aptr->extension)
     g_free(aptr->extension);

  if(aptr->new_filename)
     g_free(aptr->new_filename);

  if(aptr->old_filename)
     g_free(aptr->old_filename);

  g_free(aptr);
}


/* ============================================================================
 * gap_lib_get_frame_nr
 * ============================================================================
 */
long
gap_lib_get_frame_nr_from_name(char *fname)
{
  long number;
  int  len;
  char *basename;
  if(fname == NULL) return(-1);

  basename = gap_lib_alloc_basename(fname, &number);
  if(basename == NULL) return(-1);

  len = strlen(basename);
  g_free(basename);

  if(number > 0)  return(number);

  if(fname[len]  == '0') return(number);
/*
 *   if(fname[len]  == '_')
 *   {
 *     if(fname[len+1]  == '0') return(TRUE);
 *   }
 */
  return(-1);
}

/* -------------------------------
 * gap_lib_get_frame_nr
 * -------------------------------
 * return -1 if the specified image is
 *           NOT a gimp-gap typical frame image (e.g. has no number part in its filename)
 * return the number part in case of valid frame image.
 */
long
gap_lib_get_frame_nr(gint32 image_id)
{
  char *fname;
  long  number;

  number = -1;
  if (gap_image_is_alive(image_id))
  {
    fname = gimp_image_get_filename(image_id);
    if(fname)
    {
       number = gap_lib_get_frame_nr_from_name(fname);
       g_free(fname);
    }
  }

  return (number);
}

/* ============================================================================
 * gap_lib_chk_framechange
 *
 * check if the current frame nr has changed.
 * useful after dialogs, because the user may have renamed the current image
 * (using save as)
 * while the gap-plugin's dialog was open.
 * return: 0 .. OK
 *        -1 .. Changed (or error occurred)
 * ============================================================================
 */
int
gap_lib_chk_framechange(GapAnimInfo *ainfo_ptr)
{
  int l_rc;
  GapAnimInfo *l_ainfo_ptr;

  l_rc = -1;
  l_ainfo_ptr = gap_lib_alloc_ainfo(ainfo_ptr->image_id, ainfo_ptr->run_mode);
  if(l_ainfo_ptr != NULL)
  {
    if(ainfo_ptr->curr_frame_nr == l_ainfo_ptr->curr_frame_nr )
    {
       l_rc = 0;
    }
    else
    {
       gap_arr_msg_win(ainfo_ptr->run_mode,
         _("Operation cancelled.\n"
           "Current frame was changed while dialog was open."));
    }
    gap_lib_free_ainfo(&l_ainfo_ptr);
  }

  return l_rc;
}       /* end gap_lib_chk_framechange */

/* ============================================================================
 * gap_lib_chk_framerange
 *
 * check ainfo struct if there is a framerange (of at least one frame)
 * return: 0 .. OK
 *        -1 .. No frames there (print error)
 * ============================================================================
 */
int
gap_lib_chk_framerange(GapAnimInfo *ainfo_ptr)
{
  if(ainfo_ptr->frame_cnt == 0)
  {
     gap_arr_msg_win(ainfo_ptr->run_mode,
               _("Operation cancelled.\n"
                 "GAP video plug-ins only work with filenames\n"
                 "that end in numbers like _000001.xcf.\n"
                 "==> Rename your image, then try again."));
     return -1;
  }
  return 0;
}       /* end gap_lib_chk_framerange */

/* ============================================================================
 * p_gzip
 *   gzip or gunzip the file to a temporary file.
 *   zip == "zip"    compress
 *   zip == "unzip"  decompress
 *   return a pointer to the temporary created (by gzip) file.
 *          NULL  in case of errors
 * ============================================================================
 */
char *
p_gzip (char *orig_name, char *new_name, char *zip)
{
  gchar*   l_cmd;
  gchar*   l_tmpname;
  gint     l_rc, l_rc2;

  if(zip == NULL) return NULL;

  l_cmd = NULL;
  l_tmpname = new_name;

  /* used gzip options:
   *     -c --stdout --to-stdout
   *          Write  output  on  standard  output
   *     -d --decompress --uncompress
   *          Decompress.
   *     -f --force
   *           Force compression or decompression even if the file
   */

  if(*zip == 'u')
  {
    l_cmd = g_strdup_printf("gzip -cfd <\"%s\"  >\"%s\"", orig_name, l_tmpname);
  }
  else
  {
    l_cmd = g_strdup_printf("gzip -cf  <\"%s\"  >\"%s\"", orig_name, l_tmpname);
  }

  if(gap_debug) printf("system: %s\n", l_cmd);

  l_rc = system(l_cmd);
  if(l_rc != 0)
  {
     /* Shift 8 Bits gets Retcode of the executed Program */
     l_rc2 = l_rc >> 8;
     fprintf(stderr, "ERROR system: %s\nreturncodes %d %d", l_cmd, l_rc, l_rc2);
     l_tmpname = NULL;
  }
  g_free(l_cmd);
  return l_tmpname;

}       /* end p_gzip */

/* ============================================================================
 * p_decide_save_as
 *   decide what to do on attempt to save a frame in any image format
 *  (other than xcf)
 *   Let the user decide if the frame is to save "as it is" or "flattened"
 *   ("as it is" will save only the backround layer in most fileformat types)
 *   The SAVE_AS_MODE is stored , and reused
 *   (without displaying the dialog again)
 *   on subsequent calls of the same frame-basename and extension
 *   in the same GIMP-session.
 *
 *   return -1  ... CANCEL (do not save)
 *           0  ... save the image (may be flattened)
 * ============================================================================
 */
int
p_decide_save_as(gint32 image_id, const char *sav_name, const char *final_sav_name)
{
  gchar *l_key_save_as_mode;
  gchar *l_extension;
  gchar *l_ext;
  gchar *l_basename;
  long  l_number;
  int   l_sav_rc;

  static GapArrButtonArg  l_argv[3];
  int               l_argc;
  int               l_save_as_mode;
  GimpRunMode      l_run_mode;


   /* check if there are SAVE_AS_MODE settings (from privious calls within one gimp session) */
  l_save_as_mode = -1;
  l_sav_rc = -1;

  l_extension = gap_lib_alloc_extension(final_sav_name);
  l_basename = gap_lib_alloc_basename(final_sav_name, &l_number);

  l_key_save_as_mode = g_strdup_printf("GIMP_GAP_SAVE_MODE_%s%s"
                       ,l_basename
                       ,l_extension
                       );

  gimp_get_data (l_key_save_as_mode, &l_save_as_mode);

  if(gap_debug)
  {
    printf("DEBUG: p_decide_save_as l_save_as_mode: %d\n"
          , l_save_as_mode
          );
  }

  if(l_save_as_mode == -1)
  {
    gchar *l_key_gimprc;
    gchar *l_val_gimprc;
    gboolean l_ask;

    /* no defined value found (this is the 1.st call for this animation in the current session)
     * check for gimprc configuration to decide how to continue:
     * open a dialog (if no configuration value was found,
     * or configuration says "ask" (== other value than "yes" or "no" )
     */
    l_ext = l_extension;
    if(*l_ext == '.')
    {
      l_ext++;
    }
    l_key_gimprc = g_strdup_printf("video-save-flattened-%s", l_ext);

    if(gap_debug)
    {
      printf("GIMPRC KEY:%s:\n", l_key_gimprc);
    }

    l_val_gimprc = gimp_gimprc_query(l_key_gimprc);
    l_ask = TRUE;

    if(l_val_gimprc)
    {
      if(gap_debug)
      {
        printf("GIMPRC VAL:%s:\n", l_val_gimprc);
      }

      if(strcmp(l_val_gimprc, "yes") == 0)
      {
        l_save_as_mode = 1;
        l_ask = FALSE;
      }
      if(strcmp(l_val_gimprc, "no") == 0)
      {
        l_save_as_mode = 0;
        l_ask = FALSE;
      }

      g_free(l_val_gimprc);
    }
    else
    {
      if(gap_debug)
      {
        printf("GIMPRC VAL:<NULL>\n");
      }
    }

    if(l_ask)
    {
      gchar *l_msg;

      l_argv[0].but_txt  = GTK_STOCK_CANCEL;
      l_argv[0].but_val  = -1;
      l_argv[1].but_txt  = _("Save Flattened");
      l_argv[1].but_val  = 1;
      l_argv[2].but_txt  = _("Save As Is");
      l_argv[2].but_val  = 0;
      l_argc             = 3;

      l_msg = g_strdup_printf(_("You are using another file format than xcf.\n"
                                "Save operations may result in loss of layer information.\n\n"
                                "To configure flattening for this fileformat\n"
                                "(permanent for all further sessions) please add the line:\n"
                                "(%s %s)\n"
                                "to your gimprc file.")
                             , l_key_gimprc
                             , "\"yes\""
                             );
      l_save_as_mode =  gap_arr_buttons_dialog (_("Fileformat Warning")
                                                ,l_msg
                                                , l_argc, l_argv, -1);
      g_free(l_msg);
    }

    g_free(l_key_gimprc);

    if(gap_debug)
    {
      printf("DEBUG: decide SAVE_AS_MODE %d\n", (int)l_save_as_mode);
    }

    l_run_mode = GIMP_RUN_INTERACTIVE;
  }
  else
  {
    l_run_mode = GIMP_RUN_WITH_LAST_VALS;
  }

  gimp_set_data (l_key_save_as_mode, &l_save_as_mode, sizeof(l_save_as_mode));

  g_free(l_key_save_as_mode);

  if(l_save_as_mode < 0)
  {
    l_sav_rc = -1;
  }
  else
  {
    if(l_save_as_mode == 1)
    {
        gimp_image_flatten (image_id);
    }


    l_sav_rc = p_lib_save_named_image_1(image_id
                             , sav_name
                             , l_run_mode
                             , FALSE      /* do not enable_thumbnailsave */
                             , l_basename
                             , l_extension
                             );
  }

  g_free(l_basename);
  g_free(l_extension);

  return l_sav_rc;
}       /* end p_decide_save_as */

/* ============================================================================
 * gap_lib_gap_check_save_needed
 * ============================================================================
 */
gint32
gap_lib_gap_check_save_needed(gint32 image_id)
{
  const char *value_string;

  if (gimp_image_is_dirty(image_id))
  {
    if(gap_debug) printf("gap_lib_gap_check_save_needed: GAP need save, caused by dirty image id: %d\n", (int)image_id);
    return(TRUE);
  }
  else
  {
    value_string = gimp_gimprc_query("trust-dirty-flag");
    if(value_string != NULL)
    {
      if(gap_debug) printf("gap_lib_gap_check_save_needed:GAP gimprc FOUND relevant LINE: trust-dirty-flag  %s\n", value_string);
      {
        if((*value_string != 'y') && (*value_string != 'Y'))
        {
           if(gap_debug) printf("gap_lib_gap_check_save_needed: GAP need save, forced by gimprc trust-dirty-flag %s\n", value_string);
           return(TRUE);
        }
      }
    }
  }

  if(gap_debug) printf("gap_lib_gap_check_save_needed: GAP can SKIP save\n");
  return(FALSE);
}  /* end gap_lib_gap_check_save_needed */


/* ------------------------------------------
 * p_lib_save_jpg_non_interactive
 * ------------------------------------------
 * this procedure handles NON-Interactive save of a frame
 * in the lossy but widly spread JPEG fileformat.
 * The quality settings have to be provided via jpg_save_parasite
 * paramteter that has the structure GAPJpegSaveVals.
 * NOTE: the structure GAPJpegSaveVals must match with
 * the structure JpegSaveVals (as defined by the jpeg file save plugin
 * in file jpeg-save.h of the GIMP distribution)
 */
static gint32
p_lib_save_jpg_non_interactive(gint32 image_id, gint32 l_drawable_id, const char *sav_name,
  const GAPJpegSaveVals *save_vals)
{
  gint32     l_rc;
  gint       l_retvals;

  l_rc   = FALSE;

  if(gap_debug)
  {
    printf("DEBUG: p_lib_save_jpg_non_interactive: '%s' imageId:%d, drawableID:%d\n"
           "  jpg quality:%f\n"
           "  jpg smoothing:%f\n"
           "  jpg optimize:%d\n"
           "  jpg progressive:%d\n"
           "  jpg subsmp:%d\n"
           "  jpg baseline:%d\n"
           "  jpg restart:%d\n"
           "  jpg dct:%d\n"
         , sav_name
         , (int)image_id
         , (int)l_drawable_id
         , (float)save_vals->quality
         , (float)save_vals->smoothing
         , (int)save_vals->optimize
         , (int)save_vals->progressive
         , (int)save_vals->subsmp
         , (int)save_vals->baseline
         , (int)save_vals->restart
         , (int)save_vals->dct
         );
  }

  /* save specified layer of current frame as jpg image
   */
  GimpParam *l_params;
  l_params = gimp_run_procedure ("file_jpeg_save",
                               &l_retvals,
                               GIMP_PDB_INT32,    GIMP_RUN_NONINTERACTIVE,
                               GIMP_PDB_IMAGE,    image_id,
                               GIMP_PDB_DRAWABLE, l_drawable_id,
                               GIMP_PDB_STRING, sav_name,
                               GIMP_PDB_STRING, sav_name, /* raw name ? */
                               GIMP_PDB_FLOAT,  save_vals->quality / 100.0,
                               GIMP_PDB_FLOAT,  save_vals->smoothing,
                               GIMP_PDB_INT32,  save_vals->optimize,
                               GIMP_PDB_INT32,  save_vals->progressive,
                               GIMP_PDB_STRING, "GIMP-GAP Frame",    /* comment */
                               GIMP_PDB_INT32,  save_vals->subsmp,
                               GIMP_PDB_INT32,  save_vals->baseline,
                               GIMP_PDB_INT32,  save_vals->restart,
                               GIMP_PDB_INT32,  save_vals->dct,
                               GIMP_PDB_END);
  if (l_params[0].data.d_status == GIMP_PDB_SUCCESS)
  {
     if(gap_debug)
     {
       printf("DEBUG: p_lib_save_jpg_non_interactive: GIMP_PDB_SUCCESS '%s\n"
          ,sav_name
          );
     }
     l_rc = TRUE;
  }
  gimp_destroy_params (l_params, l_retvals);

  return (l_rc);

}  /* end p_lib_save_jpg_non_interactive */


static gboolean
p_extension_is_jpeg(const char *extension)
{
  if (extension != NULL)
  {
    const char *ext;
    ext = extension;
    if (*ext == '.')
    {
      ext++;
    }
    if ((*ext == 'j') || (*ext == 'J'))
    {
      ext++;
      if ((*ext == 'p') || (*ext == 'P'))
      {
        return (TRUE);
      }
    }

  }
  return (FALSE);

}

/* -------------------------------------
 * p_lib_save_named_image_1
 * -------------------------------------
 * save non xcf frames/images.
 * This procedure handles special case if frames have to be
 * saved as JPEG files.
 */
static gint32
p_lib_save_named_image_1(gint32 image_id, const char *sav_name, GimpRunMode run_mode, gboolean enable_thumbnailsave
  , const char *l_basename
  , const char *l_extension
  )
{
  GAPJpegSaveVals   *jpg_save_vals;
  gint32 l_sav_rc;
  char *l_key_save_vals_jpg;


  l_sav_rc = -1;
  jpg_save_vals = NULL;

  l_key_save_vals_jpg = g_strdup_printf("GIMP_GAP_SAVE_VALS_JPG_%s%s"
                       ,l_basename
                       ,l_extension
                       );

  if(gap_debug)
  {
    printf("p_lib_save_named_image_1: runmode:%d  sav_name:%s\n .. l_key_save_vals_jpg:%s\n  .. base:%s\n  .. ext:%s\n"
      ,run_mode
      ,sav_name
      ,l_key_save_vals_jpg
      ,l_basename
      ,l_extension
      );
  }

  /* before non interactive save options check if we have already jpeg-save-options
   * for the handled frames with the same basename and extension
   */
  if (run_mode != GIMP_RUN_INTERACTIVE)
  {
    int jpg_parsize;

    jpg_parsize = gimp_get_data_size(l_key_save_vals_jpg);
    if(gap_debug)
    {
      printf("p_lib_save_named_image_1: jpg_parsize=%d\n"
        ,jpg_parsize
        );
    }

    if (jpg_parsize > 0)
    {
      jpg_save_vals = g_malloc(jpg_parsize);
      gimp_get_data (l_key_save_vals_jpg, jpg_save_vals);
      run_mode = GIMP_RUN_NONINTERACTIVE;
    }
  }

  l_sav_rc = p_lib_save_named_image2(image_id
                           , sav_name
                           , run_mode
                           , FALSE      /* do not enable_thumbnailsave */
                           , jpg_save_vals
                           );
  if (jpg_save_vals != NULL)
  {
    g_free(jpg_save_vals);
  }

  /* check for jpeg specific save options after INTERACTIVE save operation
   * (recent versions of JPEG save plugin shall store the save options
   * in parasite data.)
   */
  if ((run_mode == GIMP_RUN_INTERACTIVE) && (p_extension_is_jpeg(l_extension)))
  {
    GimpParasite      *jpg_save_parasite;

    jpg_save_parasite = gimp_image_parasite_find (image_id,
                                      "jpeg-save-options");
    if(gap_debug)
    {
      printf("DEBUG: jpg_save_parasite %d\n", (int)jpg_save_parasite);
    }
    if (jpg_save_parasite)
    {
      const GAPJpegSaveVals   *const_jpg_save_vals;
      const_jpg_save_vals = gimp_parasite_data (jpg_save_parasite);

      /* store the jpeg save options for the handled frame basename and extension in this session */
      gimp_set_data (l_key_save_vals_jpg, const_jpg_save_vals, sizeof(GAPJpegSaveVals));
      gimp_parasite_free (jpg_save_parasite);
    }
  }

  g_free(l_key_save_vals_jpg);

  return (l_sav_rc);
}  /* end p_lib_save_named_image_1 */

/* ============================================================================
 * gap_lib_save_named_image / 2
 * ============================================================================
 * this procedure is typically used to save frame images in other image formats than
 * gimp native XCF format.
 */
static gint32
p_lib_save_named_image2(gint32 image_id, const char *sav_name, GimpRunMode run_mode, gboolean enable_thumbnailsave
  ,const GAPJpegSaveVals *jpg_save_vals)
{
  gint32      l_drawable_id;
  gint        l_nlayers;
  gint32     *l_layers_list;
  gboolean    l_rc;

  if(gap_debug)
  {
    printf("DEBUG: before   p_lib_save_named_image2: '%s'\n", sav_name);
  }

  l_layers_list = gimp_image_get_layers(image_id, &l_nlayers);
  if(l_layers_list == NULL)
  {
     printf("ERROR: gap_lib.c.p_lib_save_named_image2: failed to save '%s' because has no layers\n"
         , sav_name
         );
     return -1;
  }

  l_drawable_id = l_layers_list[l_nlayers -1];  /* use the background layer */

  if ((jpg_save_vals != NULL)
  && (run_mode != GIMP_RUN_INTERACTIVE))
  {
    /* Special case: save JPG noninteractive
     * Since GIMP-2.4.x jpeg file save plugin changed behaviour:
     * when saved in GIMP_RUN_WITH_LAST_VALS mode it acts the same way
     * as interactive mode (e.q. opens a dialog)
     * this behaviour is not acceptable when saving lots of frames.
     * therefore GAP tries to figure out the jpeg save paramters that
     * are available as parasite data, and perform a non interactive
     * save operation.
     * (This way the GIMP_GAP code gets a dependency to all future
     * changes of JPEG file save parameter changes,
     * but at least works for GIMP-2.4.x again)
     */
     l_rc = p_lib_save_jpg_non_interactive(image_id,
                                    l_drawable_id,
                                    sav_name,
                                    jpg_save_vals
                                    );
  }
  else
  {
    l_rc = gimp_file_save(run_mode,
                   image_id,
                   l_drawable_id,
                   sav_name,
                   sav_name /* raw name ? */
                   );
  }


  if(gap_debug)
  {
    printf("DEBUG: after    p_lib_save_named_image2: '%s' nlayers=%d image=%d drawable_id=%d run_mode=%d\n"
        , sav_name
        , (int)l_nlayers
        , (int)image_id
        , (int)l_drawable_id
        , (int)run_mode
        );
  }

  if(enable_thumbnailsave)
  {
    gchar *l_sav_name;

    l_sav_name = g_strdup(sav_name);
    gap_thumb_cond_gimp_file_save_thumbnail(image_id, l_sav_name);
    g_free(l_sav_name);
  }

  if(gap_debug)
  {
    printf("DEBUG: after thumbmail save\n");
  }

  g_free (l_layers_list);

  if (l_rc != TRUE)
  {
    printf("ERROR: p_lib_save_named_image2  gimp_file_save failed '%s'  rc:%d\n"
        , sav_name
        ,(int)l_rc
        );
    return -1;
  }
  return image_id;

}       /* end p_lib_save_named_image2 */

gint32
gap_lib_save_named_image(gint32 image_id, const char *sav_name, GimpRunMode run_mode)
{
  gchar *l_extension;
  gchar *l_basename;
  gint32 l_rc;
  long  l_number;

  l_extension = gap_lib_alloc_extension(sav_name);
  l_basename = gap_lib_alloc_basename(sav_name, &l_number);

  l_rc = p_lib_save_named_image_1(image_id
                            , sav_name
                            , run_mode
                            , TRUE      /* enable_thumbnailsave */
                            , l_basename
                            , l_extension
                            );
  g_free(l_extension);
  g_free(l_basename);

  return (l_rc);
}


/* ============================================================================
 * gap_lib_save_named_frame
 *  save frame into temporary image,
 *  on success rename it to desired framename.
 *  (this is done, to avoid corrupted frames on disk in case of
 *   crash in one of the save procedures)
 * ============================================================================
 */
int
gap_lib_save_named_frame(gint32 image_id, char *sav_name)
{
  GimpParam *l_params;
  gchar     *l_ext;
  char      *l_tmpname;
  gint       l_retvals;
  int        l_gzip;
  int        l_xcf;
  int        l_rc;

  l_tmpname = NULL;
  l_rc   = -1;
  l_gzip = 0;          /* dont zip */
  l_xcf  = 0;          /* assume no xcf format */

  /* check extension to decide if savd file will be zipped */
  l_ext = gap_lib_alloc_extension(sav_name);
  if(l_ext == NULL)
  {
    printf("gap_lib_save_named_frame failed for file:%s  (because it has no extension)\n"
      ,sav_name
      );
    return -1;
  }

  gimp_image_set_filename(image_id, sav_name);

  if(0 == strcmp(l_ext, ".xcf"))
  {
    l_xcf  = 1;
  }
  else if(0 == strcmp(l_ext, ".xcfgz"))
  {
    l_gzip = 1;          /* zip it */
    l_xcf  = 1;
  }
  else if(0 == strcmp(l_ext, ".gz"))
  {
    l_gzip = 1;          /* zip it */
  }

  /* find a temp name
   * that resides on the same filesystem as sav_name
   * and has the same extension as the original sav_name
   */
  l_tmpname = g_strdup_printf("%s.gtmp%s", sav_name, l_ext);
  if(1 == gap_lib_file_exists(l_tmpname))
  {
      /* FILE exists: let gimp find another temp name */
      l_tmpname = gimp_temp_name(&l_ext[1]);
  }

  g_free(l_ext);


  if(gap_debug)
  {
    char *env_no_save;
    env_no_save = (gchar *) g_getenv("GAP_NO_SAVE");
    if(env_no_save != NULL)
    {
      printf("DEBUG: GAP_NO_SAVE is set: save is skipped: '%s'\n", l_tmpname);
      g_free(l_tmpname);  /* free if it was a temporary name */
      return 0;
    }
  }


  if(l_xcf != 0)
  {
    if(gap_debug)
    {
      printf("DEBUG: gap_lib_save_named_frame before gimp_xcf_save on file: '%s'\n"
            , l_tmpname
            );
    }

    /* save current frame as xcf image
     * xcf_save does operate on the complete image,
     * the drawable is ignored. (we can supply a dummy value)
     */
    l_params = gimp_run_procedure ("gimp_xcf_save",
                                 &l_retvals,
                                 GIMP_PDB_INT32,    GIMP_RUN_NONINTERACTIVE,
                                 GIMP_PDB_IMAGE,    image_id,
                                 GIMP_PDB_DRAWABLE, 0,
                                 GIMP_PDB_STRING, l_tmpname,
                                 GIMP_PDB_STRING, l_tmpname, /* raw name ? */
                                 GIMP_PDB_END);
    if(gap_debug)
    {
      printf("DEBUG: after   xcf  gap_lib_save_named_frame: '%s'\n", l_tmpname);
    }

    if (l_params[0].data.d_status == GIMP_PDB_SUCCESS)
    {
       l_rc = image_id;
    }
    gimp_destroy_params (l_params, l_retvals);
  }
  else
  {
     if(gap_debug)
     {
       printf("DEBUG: gap_lib_save_named_frame before save NON-XCF file: '%s'\n"
             , l_tmpname
             );
     }

     /* let gimp try to save (and detect filetype by extension)
      * Note: the most imagefileformats do not support multilayer
      *       images, and extra channels
      *       the result may not contain the whole imagedata
      *
      * To Do: Should warn the user at 1.st attempt to do this.
      */

     l_rc = p_decide_save_as(image_id, l_tmpname, sav_name);

     if(gap_debug)
     {
       printf("DEBUG: gap_lib_save_named_frame after save NON-XCF file: '%s'  rc:%d\n"
             , l_tmpname
             , (int)l_rc
             );
     }
  }

  if(l_rc < 0)
  {
     g_remove(l_tmpname);
     g_free(l_tmpname);  /* free temporary name */
     return l_rc;
  }

  if(l_gzip == 0)
  {
     /* remove sav_name, then rename tmpname ==> sav_name */
     g_remove(sav_name);
     if (0 != g_rename(l_tmpname, sav_name))
     {
        /* if tempname is located on another filesystem (errno == EXDEV)
         * rename will not work.
         * so lets try a  copy ; remove sequence
         */
         if(gap_debug)
         {
           printf("DEBUG: gap_lib_save_named_frame: RENAME 2nd try\n");
         }

         if(0 == gap_lib_file_copy(l_tmpname, sav_name))
         {
            g_remove(l_tmpname);
         }
         else
         {
            printf("ERROR in gap_lib_save_named_frame: can't rename %s to %s\n"
                  , l_tmpname
                  , sav_name
                  );
            return -1;
         }
     }
  }
  else
  {
    /* ZIP tmpname ==> sav_name */
    if(NULL != p_gzip(l_tmpname, sav_name, "zip"))
    {
       /* OK zip created compressed file named sav_name
        * now delete the uncompressed l_tempname
        */
       g_remove(l_tmpname);
    }
  }

  if(gap_debug)
  {
    printf("DEBUG: gap_lib_save_named_frame: before gap_thumb_cond_gimp_file_save_thumbnail\n");
  }

  gap_thumb_cond_gimp_file_save_thumbnail(image_id, sav_name);

  if(gap_debug)
  {
    printf("DEBUG: gap_lib_save_named_frame: after gap_thumb_cond_gimp_file_save_thumbnail\n");
  }

  g_free(l_tmpname);  /* free temporary name */

  return l_rc;

}       /* end gap_lib_save_named_frame */


/* ============================================================================
 * p_save_old_frame
 * ============================================================================
 */
int
p_save_old_frame(GapAnimInfo *ainfo_ptr, GapVinVideoInfo *vin_ptr)
{
  /* SAVE of old image image if it has unsaved changes
   * (or if Unconditional frame save is forced by gimprc setting)
   */
  if(gap_lib_gap_check_save_needed(ainfo_ptr->image_id))
  {
    if(gap_debug)
    {
       printf("p_save_old_frame Save required for file:%s\n"
             , ainfo_ptr->old_filename
             );
    }
    /* check and perform automatic onionskinlayer remove */
    if(vin_ptr)
    {
      if((vin_ptr->auto_delete_before_save) && (vin_ptr->onionskin_auto_enable))
      {
        gap_onion_base_onionskin_delete(ainfo_ptr->image_id);
      }
    }
    return (gap_lib_save_named_frame(ainfo_ptr->image_id, ainfo_ptr->old_filename));
  }
  else
  {
    if(gap_debug)
    {
       printf("p_save_old_frame No save needed for file:%s OK\n"
             , ainfo_ptr->old_filename
             );
    }
  }
  return 0;
}       /* end p_save_old_frame */



/* ============================================================================
 * gap_lib_load_image
 * load image of any type by filename, and return its image id
 * (or -1 in case of errors)
 * ============================================================================
 */
gint32
gap_lib_load_image (char *lod_name)
{
  char  *l_ext;
  char  *l_tmpname;
  gint32 l_tmp_image_id;
  int    l_rc;

  l_rc = 0;
  l_tmpname = NULL;
  l_ext = gap_lib_alloc_extension(lod_name);
  if(l_ext != NULL)
  {
    if((0 == strcmp(l_ext, ".xcfgz"))
    || (0 == strcmp(l_ext, ".gz")))
    {

      /* find a temp name and */
      /* try to unzip file, before loading it */
      l_tmpname = p_gzip(lod_name, gimp_temp_name(&l_ext[1]), "unzip");
    }
    else l_tmpname = lod_name;
    g_free(l_ext);
  }

  if(l_tmpname == NULL)
  {
    return -1;
  }


  if(gap_debug) printf("DEBUG: before   gap_lib_load_image: '%s'\n", l_tmpname);

  l_tmp_image_id = gimp_file_load(GIMP_RUN_NONINTERACTIVE,
                l_tmpname,
                l_tmpname  /* raw name ? */
                );

  if(gap_debug) printf("DEBUG: after    gap_lib_load_image: '%s' id=%d\n\n",
                               l_tmpname, (int)l_tmp_image_id);

  if(l_tmpname != lod_name)
  {
    g_remove(l_tmpname);
    g_free(l_tmpname);  /* free if it was a temporary name */
  }


  return l_tmp_image_id;

}       /* end gap_lib_load_image */



/* ============================================================================
 * gap_lib_load_named_frame
 *   load new frame,
 *   reconnect displays from old existing image to the new loaded frame
 *   and delete the old image.
 *  returns: new_image_id (or -1 on errors)
 * ============================================================================
 */
gint32
gap_lib_load_named_frame (gint32 old_image_id, char *lod_name)
{
  gint32 l_new_image_id;

  l_new_image_id = gap_lib_load_image(lod_name);

  if(gap_debug)
  {
    printf("DEBUG: after    gap_lib_load_named_frame: '%s' old_id=%d  new_id=%d\n\n",
                               lod_name, (int)old_image_id, (int)l_new_image_id);
  }

  if(l_new_image_id < 0)
  {
      return -1;
  }

  if (gap_pdb_gimp_displays_reconnect(old_image_id, l_new_image_id))
  {
      /* deleting the old image if it is still alive
       * (maybe still required gimp-2.2.x prior to gimp-2.2.11
       * gimp-2.2.11 still accepts the delete, but the old image becomes invalid after
       * reconnecting the display.
       * gimp-2.3.8 even complains and breaks gimp-gap if we attempt to delete
       * the old image. (see #339840)
       * GAP has no more chance of explicitly deleting the old image
       * (hope that this is already done implicitly by gimp_reconnect_displays ?)
       */

      if(gap_image_is_alive(old_image_id))
      {
        gimp_image_delete(old_image_id);
      }

      gimp_image_undo_disable(l_new_image_id);

      /* use the original lod_name */
      gimp_image_set_filename (l_new_image_id, lod_name);

      /* dont consider image dirty after load */
      gimp_image_clean_all(l_new_image_id);

      /* Update the active_image table for the Navigator Dialog
       * (TODO: replace the active-image table by real Event handling)
       */
      gap_navat_update_active_image(old_image_id, l_new_image_id);

      gimp_image_undo_enable(l_new_image_id);

      return l_new_image_id;
   }

   printf("GAP: Error: PDB call of gimp_displays_reconnect failed\n");
   return (-1);
}       /* end gap_lib_load_named_frame */



/* ============================================================================
 * gap_lib_replace_image
 *
 * make new_filename of next file to load, check if that file does exist on disk
 * then save current image and replace it, by loading the new_filename
 * this includes automatic onionskin layer handling and
 * active layer tracking
 *
 * return image_id (of the new loaded frame) on success
 *        or -1 on errors
 * ============================================================================
 */
gint32
gap_lib_replace_image(GapAnimInfo *ainfo_ptr)
{
  gint32 image_id;
  GapVinVideoInfo *vin_ptr;
  gboolean  do_onionskin_crate;
  gint32    ref_active_layer;
  gint32    ref_layer_stackpos;
  gchar    *ref_layer_name;
  int       save_rc;

  do_onionskin_crate  = FALSE;

  if(gap_debug)
  {
    printf("gap_lib_replace_image START\n");
  }

  ref_layer_name = p_get_active_layer_name(ainfo_ptr->image_id
                                          ,&ref_active_layer
                                          ,&ref_layer_stackpos
                                          );

  if(ainfo_ptr->new_filename != NULL)
  {
    g_free(ainfo_ptr->new_filename);
  }
  ainfo_ptr->new_filename = gap_lib_alloc_fname(ainfo_ptr->basename,
                                      ainfo_ptr->frame_nr,
                                      ainfo_ptr->extension);
  if(ainfo_ptr->new_filename == NULL)
  {
     if(gap_debug)
     {
       printf("gap_lib_replace_image (1) return because ainfo_ptr->new_filename == NULL\n");
     }
     return -1;
  }

  if(0 == gap_lib_file_exists(ainfo_ptr->new_filename ))
  {
     if(gap_debug)
     {
       printf("gap_lib_replace_image (2) return because %s does not exist (or is empty)\n"
             , ainfo_ptr->new_filename
             );
     }
     return -1;
  }

  if(gap_debug)
  {
     printf("gap_lib_replace_image (3) ainfo_ptr->new_filename:%s OK\n"
           , ainfo_ptr->new_filename
            );
  }

  vin_ptr = gap_vin_get_all(ainfo_ptr->basename);
  save_rc = p_save_old_frame(ainfo_ptr, vin_ptr);
  if(gap_debug)
  {
    printf("gap_lib_replace_image (4) automatic save: save_rc:%d, old_filename:%s\n"
           , (int)save_rc
           , ainfo_ptr->old_filename
            );
  }

  if(save_rc < 0)
  {
    if(vin_ptr)
    {
      g_free(vin_ptr);
    }
    printf("gap_lib_replace_image (5) automatic save failed: save_rc:%d, old_filename:%s\n"
           , (int)save_rc
           , ainfo_ptr->old_filename
            );
    return -1;
  }

  if((vin_ptr->auto_replace_after_load) && (vin_ptr->onionskin_auto_enable))
  {
    do_onionskin_crate  = TRUE;

    /* check if directoryscan is needed */
    if((ainfo_ptr->first_frame_nr < 0)
    || (ainfo_ptr->last_frame_nr < 0))
    {
       /* perform directoryscan to findout first_frame_nr and last_frame_nr
        * that is needed for onionskin creation
        */
       gap_lib_dir_ainfo(ainfo_ptr);
    }
  }

  image_id = gap_lib_load_named_frame(ainfo_ptr->image_id, ainfo_ptr->new_filename);

  /* check and peroform automatic onionskinlayer creation */
  if(vin_ptr)
  {
    if(do_onionskin_crate)
    {
       /* create onionskinlayers without keeping the handled images cached
        * (passing NULL pointers for the chaching structures and functions)
        */
       gap_onion_base_onionskin_apply(NULL         /* dummy pointer gpp */
             , image_id               /* apply on the newly loaded image_id */
             , vin_ptr
             , ainfo_ptr->frame_nr        /* the new current frame_nr */
             , ainfo_ptr->first_frame_nr
             , ainfo_ptr->last_frame_nr
             , ainfo_ptr->basename
             , ainfo_ptr->extension
             , NULL                    /* fptr_add_img_to_cache */
             , NULL                    /* fptr_find_frame_in_img_cache */
             , FALSE                   /* use_cache */
             );
    }

    p_do_active_layer_tracking(image_id
                       ,vin_ptr
                       ,ref_layer_name
                       ,ref_layer_stackpos
                       );

    g_free(vin_ptr);
  }

  if(ref_layer_name)
  {
    g_free(ref_layer_name);
  }

  if(gap_debug)
  {
    printf("gap_lib_replace_image ENDED regular\n");
  }

  return(image_id);
}       /* end gap_lib_replace_image */




/* XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX */
/* XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX */
/* XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX */

/* ============================================================================
 * gap_video_paste Buffer procedures
 * ============================================================================
 */

static gchar *
p_get_video_paste_basename(void)
{
  gchar *l_basename;

  l_basename = gimp_gimprc_query("video-paste-basename");
  if(l_basename == NULL)
  {
     l_basename = g_strdup("gap_video_pastebuffer_");
  }
  return(l_basename);
}

static gchar *
p_get_video_paste_dir(void)
{
  gchar *l_dir;
  gint   l_len;

  l_dir = gimp_gimprc_query("video-paste-dir");
  if(l_dir == NULL)
  {
     l_dir = g_strdup("/tmp");
  }

  /* if dir is configured with trailing dir seprator slash
   * then cut it off
   */
  l_len = strlen(l_dir);
  if((l_dir[l_len -1] == G_DIR_SEPARATOR) && (l_len > 1))
  {
     l_dir[l_len -1] = '\0';
  }
  return(l_dir);
}

gchar *
gap_lib_get_video_paste_name(void)
{
  gchar *l_dir;
  gchar *l_basename;
  gchar *l_video_name;

  l_dir = p_get_video_paste_dir();
  l_basename = p_get_video_paste_basename();
  l_video_name = g_strdup_printf("%s%s%s", l_dir, G_DIR_SEPARATOR_S, l_basename);

  g_free(l_dir);
  g_free(l_basename);

  if(gap_debug) printf("gap_lib_get_video_paste_name: %s\n", l_video_name);
  return(l_video_name);
}

static gint32
p_clear_or_count_video_paste(gint delete_flag)
{
  gchar *l_dir;
  gchar *l_basename;
  gchar *l_filename;
  gint   l_len;
  gint32 l_framecount;
  GDir          *l_dirp;
  const char    *l_entry;

  l_dir = p_get_video_paste_dir();
  l_dirp = g_dir_open(l_dir, 0, NULL);
  l_framecount = 0;

  if(!l_dirp)
  {
    printf("ERROR gap_vid_edit_clear: can't read directory %s\n", l_dir);
    l_framecount = -1;
  }
  else
  {
     l_basename = p_get_video_paste_basename();

     l_len = strlen(l_basename);
     while ( (l_entry = g_dir_read_name( l_dirp )) != NULL )
     {
       if(strncmp(l_basename, l_entry, l_len) == 0)
       {
          l_filename = g_strdup_printf("%s%s%s", l_dir, G_DIR_SEPARATOR_S, l_entry);
          if(1 == gap_lib_file_exists(l_filename)) /* check for regular file */
          {
             /* delete all files in the video paste directory
              * with names matching the basename part
              */
             l_framecount++;
             if(delete_flag)
             {
                if(gap_debug) printf("gap_vid_edit_clear: remove file %s\n", l_filename);
                g_remove(l_filename);

                /* also delete thumbnail */
                gap_thumb_file_delete_thumbnail(l_filename);
             }
          }
          g_free(l_filename);
       }
     }
     g_dir_close( l_dirp );
     g_free(l_basename);
   }
   g_free(l_dir);
   return(l_framecount);
}

gint32
gap_vid_edit_clear(void)
{
  return(p_clear_or_count_video_paste(TRUE)); /* delete frames */
}

gint32
gap_vid_edit_framecount()
{
  return (p_clear_or_count_video_paste(FALSE)); /* delete_flag is off, just count frames */
}


/* ============================================================================
 * gap_vid_edit_copy
 * ============================================================================
 * copy frames to the vid paste directory
 * (always using 6 digit framenumbers in the vid paste dir)
 */
gint
gap_vid_edit_copy(GimpRunMode run_mode, gint32 image_id, long range_from, long range_to)
{
  int rc;
  GapAnimInfo *ainfo_ptr;

  gchar *l_curr_name;
  gchar *l_fname ;
  gchar *l_basename;
  gint32 l_frame_nr;
  gint32 l_cnt2;
  gint32 l_tmp_image_id;
  long   l_cur_frame_nr;
  long   l_top_frame_nr;
  gboolean   l_frame_found;

  ainfo_ptr = gap_lib_alloc_ainfo(image_id, run_mode);
  if(ainfo_ptr == NULL)
  {
     return (-1);
  }
  rc = 0;

  if((ainfo_ptr->curr_frame_nr >= MIN(range_to, range_from))
  && (ainfo_ptr->curr_frame_nr <= MAX(range_to, range_from)))
  {
    /* current frame is in the affected range
     * so we have to save current frame to file
     */
    l_curr_name = gap_lib_alloc_fname(ainfo_ptr->basename, ainfo_ptr->curr_frame_nr, ainfo_ptr->extension);
    gap_lib_save_named_frame(ainfo_ptr->image_id, l_curr_name);
    g_free(l_curr_name);
  }

  l_basename = gap_lib_get_video_paste_name();
  l_cnt2 = gap_vid_edit_framecount();  /* count frames in the video paste buffer */
  l_frame_nr = 1 + l_cnt2;           /* start at one, or continue (append) at end +1 */



  l_top_frame_nr = MAX(range_to, range_from);
  l_cur_frame_nr = MIN(range_to, range_from);
  l_frame_found = TRUE;
  while(l_frame_found == TRUE)
  {
     if((rc < 0) || (l_cur_frame_nr > l_top_frame_nr))
     {
       break;
     }
     l_fname = gap_lib_alloc_fname(ainfo_ptr->basename,
                             l_cur_frame_nr,
                             ainfo_ptr->extension);
     if (g_file_test(l_fname, G_FILE_TEST_EXISTS))
     {
       gchar *l_fname_copy;

       l_fname_copy = g_strdup_printf("%s%06ld.xcf", l_basename, (long)l_frame_nr);

       if(strcmp(ainfo_ptr->extension, ".xcf") == 0)
       {
         rc = gap_lib_image_file_copy(l_fname, l_fname_copy);
       }
       else
       {
         /* convert other fileformats to xcf before saving to video paste buffer */
         l_tmp_image_id = gap_lib_load_image(l_fname);
         rc = gap_lib_save_named_frame(l_tmp_image_id, l_fname_copy);
         gimp_image_delete(l_tmp_image_id);
       }
       g_free(l_fname_copy);
       l_frame_nr++;
     }
     g_free(l_fname);

     /* advance l_cur_frame_nr to the next available frame number
      * (normally to l_cur_frame_nr += 1;
      * sometimes to higher number when frames are missing)
      */
     l_cur_frame_nr = gap_lib_get_next_available_frame_number(l_cur_frame_nr, 1
                                , ainfo_ptr->basename, ainfo_ptr->extension, &l_frame_found);


  }
  gap_lib_free_ainfo(&ainfo_ptr);
  return(rc);
}       /* end gap_vid_edit_copy */




/* ============================================================================
 * p_custom_palette_file
 *   write a gimp palette file
 * ============================================================================
 */

static gint
p_custom_palette_file(char *filename, guchar *rgb, gint count)
{
  FILE *l_fp;

  l_fp= g_fopen(filename, "w");
  if (l_fp == NULL)
  {
    return -1;
  }

  fprintf(l_fp, "GIMP Palette\n");
  fprintf(l_fp, "# this file will be overwritten each time when video frames are converted to INDEXED\n");

  while (count > 0)
  {
    fprintf(l_fp, "%d %d %d\tUnknown\n", rgb[0], rgb[1], rgb[2]);
    rgb+= 3;
    --count;
  }


  fclose (l_fp);
  return 0;
}       /* end p_custom_palette_file */


/* ============================================================================
 * gap_vid_edit_paste
 *
 * return image_id (of the new loaded current frame) on success
 *        or -1 on errors
 * ============================================================================
 * copy frames from the vid paste directory
 * (always using 6 digit framenumbers in the vid paste dir)
 */
gint32
gap_vid_edit_paste(GimpRunMode run_mode, gint32 image_id, long paste_mode)
{
#define CUSTOM_PALETTE_NAME "gap_cmap.gpl"
  gint32 rc;
  GapAnimInfo *ainfo_ptr;

  gchar *l_curr_name;
  gchar *l_fname ;
  gchar *l_fname_copy;
  gchar *l_basename;
  gint32 l_frame_nr;
  gint32 l_dst_frame_nr;
  gint32 l_cnt2;
  gint32 l_lo, l_hi;
  gint32 l_insert_frame_nr;
  gint32 l_tmp_image_id;
  gint       l_rc;
  GimpImageBaseType  l_orig_basetype;
  gboolean   l_frame_found;

  l_cnt2 = gap_vid_edit_framecount();
  if(gap_debug)
  {
    printf("gap_vid_edit_paste: paste_mode %d found %d frames to paste, image_id: %d\n"
           , (int)paste_mode
           , (int)l_cnt2
           , (int)image_id
           );
  }
  if (l_cnt2 < 1)
  {
    return(0);  /* video paste buffer is empty */
  }


  ainfo_ptr = gap_lib_alloc_ainfo(image_id, run_mode);
  if(ainfo_ptr == NULL)
  {
     return (-1);
  }
  if (0 != gap_lib_dir_ainfo(ainfo_ptr))
  {
     return (-1);
  }

  if(gap_debug)
  {
    printf("gap_vid_edit_paste: ainfo: basename: %s, extension:%s  curr:%d first:%d last: %d\n"
           , ainfo_ptr->basename
           , ainfo_ptr->extension
           , (int) ainfo_ptr->curr_frame_nr
           , (int) ainfo_ptr->first_frame_nr
           , (int) ainfo_ptr->last_frame_nr
           );
  }

  rc = 0;

  l_insert_frame_nr = ainfo_ptr->curr_frame_nr;

  if(paste_mode != GAP_VID_PASTE_INSERT_AFTER)
  {
    /* we have to save current frame to file */
    l_curr_name = gap_lib_alloc_fname(ainfo_ptr->basename, ainfo_ptr->curr_frame_nr, ainfo_ptr->extension);
    gap_lib_save_named_frame(ainfo_ptr->image_id, l_curr_name);
    g_free(l_curr_name);
  }

  if(paste_mode != GAP_VID_PASTE_REPLACE)
  {
     if(paste_mode == GAP_VID_PASTE_INSERT_AFTER)
     {
       l_insert_frame_nr = ainfo_ptr->curr_frame_nr +1;
     }

     /* rename (renumber) all frames with number greater (or greater equal)  than current
      */
     l_lo   = ainfo_ptr->last_frame_nr;
     l_hi   = l_lo + l_cnt2;

     if(gap_debug)
     {
       printf("gap_vid_edit_paste: l_insert_frame_nr %d l_lo:%d l_hi:%d\n"
           , (int)l_insert_frame_nr, (int)l_lo, (int)l_hi);
     }

     while(l_lo >= l_insert_frame_nr)
     {
       l_hi   = l_lo + l_cnt2;
       l_frame_found = gap_lib_framefile_with_framenr_exists(ainfo_ptr, l_lo);
       if (l_frame_found)
       {
         if(0 != gap_lib_rename_frame(ainfo_ptr, l_lo, l_hi))
         {
           gchar *tmp_errtxt;
           tmp_errtxt = g_strdup_printf(_("Error: could not rename frame %ld to %ld"), (long int)l_lo, (long int)l_hi);
           gap_arr_msg_win(ainfo_ptr->run_mode, tmp_errtxt);
           g_free(tmp_errtxt);
           return -1;
         }
       }
       /* advance l_lo to the previous available frame number
        * (normally to l_lo -= 1;
        * sometimes to lower number when frames are missing)
        */
       l_lo = gap_lib_get_next_available_frame_number(l_lo, -1
                              , ainfo_ptr->basename, ainfo_ptr->extension, &l_frame_found);
       if (!l_frame_found)
       {
         break;
       }

     }  /* end while */
  }

  l_basename = gap_lib_get_video_paste_name();
  l_dst_frame_nr = l_insert_frame_nr;
  for(l_frame_nr = 1; l_frame_nr <= l_cnt2; l_frame_nr++)
  {
     l_fname = gap_lib_alloc_fname(ainfo_ptr->basename,
                             l_dst_frame_nr,
                             ainfo_ptr->extension);
     l_fname_copy = g_strdup_printf("%s%06ld.xcf", l_basename, (long)l_frame_nr);

     l_tmp_image_id = gap_lib_load_image(l_fname_copy);

     /* delete onionskin layers (if there are any) before paste */
     gap_onion_base_onionskin_delete(l_tmp_image_id);

     /* check size and resize if needed */
     if((gimp_image_width(l_tmp_image_id) != gimp_image_width(image_id))
     || (gimp_image_height(l_tmp_image_id) != gimp_image_height(image_id)))
     {
         gint32      l_size_x, l_size_y;

         l_size_x = gimp_image_width(image_id);
         l_size_y = gimp_image_height(image_id);
         if(gap_debug) printf("DEBUG: scale to size %d %d\n", (int)l_size_x, (int)l_size_y);

         gimp_image_scale(l_tmp_image_id, l_size_x, l_size_y);
     }

     /* check basetype and convert if needed */
     l_orig_basetype = gimp_image_base_type(image_id);
     if(gimp_image_base_type(l_tmp_image_id) != l_orig_basetype)
     {
       switch(l_orig_basetype)
       {
           gchar      *l_palette_filename;
           guchar     *l_cmap;
           gint        l_ncolors;

           /* convert tmp image to dest type */
           case GIMP_INDEXED:
             l_cmap = gimp_image_get_colormap(image_id, &l_ncolors);
             if(gap_debug) printf("DEBUG: convert to INDEXED %d colors\n", (int)l_ncolors);

             l_palette_filename = g_strdup_printf("%s%spalettes%s%s"
                                                 , gimp_directory()
                                                 , G_DIR_SEPARATOR_S
                                                 , G_DIR_SEPARATOR_S
                                                 , CUSTOM_PALETTE_NAME);

             l_rc = p_custom_palette_file(l_palette_filename, l_cmap, l_ncolors);
             if(l_rc == 0)
             {
               gimp_palettes_refresh();
               gimp_image_convert_indexed(l_tmp_image_id,
                                          GIMP_FS_DITHER,           /* dither floyd-steinberg */
                                          GIMP_CUSTOM_PALETTE,
                                          l_ncolors,                /* number of colors */
                                          FALSE,                    /* No alpha_dither */
                                          FALSE,                    /* dont remove_unused */
                                          CUSTOM_PALETTE_NAME
                                          );
             }
             else
             {
               printf("ERROR: gap_vid_edit_paste: could not save custom palette %s\n", l_palette_filename);
             }
             g_free(l_cmap);
             g_free(l_palette_filename);
             break;
           case GIMP_GRAY:
             if(gap_debug) printf("DEBUG: convert to GRAY'\n");
             gimp_image_convert_grayscale(l_tmp_image_id);
             break;
           case GIMP_RGB:
             if(gap_debug) printf("DEBUG: convert to RGB'\n");
             gimp_image_convert_rgb(l_tmp_image_id);
             break;
           default:
             printf( "DEBUG: unknown image type\n");
             return -1;
             break;
        }
     }

     if(gap_debug) printf("DEBUG: before gap_lib_save_named_frame l_fname:%s l_tmp_image_id:%d'\n"
                      , l_fname
                      , (int) l_tmp_image_id);

     gimp_image_set_filename (l_tmp_image_id, l_fname);
     rc = gap_lib_save_named_frame(l_tmp_image_id, l_fname);

     gimp_image_delete(l_tmp_image_id);

     g_free(l_fname);
     g_free(l_fname_copy);

     l_dst_frame_nr++;
  }

  if(paste_mode == GAP_VID_PASTE_INSERT_AFTER)
  {
    /* we pasted successful after the current image,
     * keep the calling image_id as active image_id return value
     */
    if(rc >= 0)
    {
      rc = image_id;
    }
  }
  else
  {
    if(rc >= 0)
    {
      /* load from the "new" current frame */
      if(ainfo_ptr->new_filename != NULL) g_free(ainfo_ptr->new_filename);

      ainfo_ptr->new_filename = gap_lib_alloc_fname(ainfo_ptr->basename,
                                      ainfo_ptr->curr_frame_nr,
                                      ainfo_ptr->extension);

      if(gap_debug)  printf("gap_vid_edit_paste: before load: %s basename: %s, extension:%s  curr:%d first:%d last: %d\n"
           , ainfo_ptr->new_filename
           , ainfo_ptr->basename
           , ainfo_ptr->extension
           , (int) ainfo_ptr->curr_frame_nr
           , (int) ainfo_ptr->first_frame_nr
           , (int) ainfo_ptr->last_frame_nr
           );

      rc = gap_lib_load_named_frame(ainfo_ptr->image_id, ainfo_ptr->new_filename);
    }
  }

  if(rc < 0)
  {
      rc = -1;
  }

  if(gap_debug)  printf("gap_vid_edit_paste: rc: %d\n", (int)rc);

  gap_lib_free_ainfo(&ainfo_ptr);

  return(rc);
}       /* end gap_vid_edit_paste */


/* ---------------------------------------
 * gap_lib_get_next_available_frame_number
 * ---------------------------------------
 * returns the next available frame_number in sequence
 * (for ascending sequence use stepsize 1
 * for descending sequence use stepsize -1)
 * if the frame image file with the next expected frame number is not
 * found, the number is derived from the next available frame image file.
 *
 * if there are no more next frame images,
 *     *frame_found_ptr is set to FALSE (if it is not NULL)
 *     and cur_frame_nr + stepsize is returned
 * otherwise (when a next frame image was found)
 *     *frame_found_ptr is set to TRUE (if it is not NULL)
 *     and the frame number of the next matching frame image is returned.
 */
long
gap_lib_get_next_available_frame_number(long cur_frame_nr, long stepsize,
  char *basename, char *extension, gboolean *frame_found_ptr)
{
  long     l_next_frame_nr;
  gint     l_idx;
  gboolean l_frame_found;

  l_next_frame_nr = cur_frame_nr + stepsize;
  l_frame_found = FALSE;

  for(l_idx=1; l_idx < 10; l_idx++)
  {
    char *new_filename;
    long l_potential_next_frame_nr;

    l_potential_next_frame_nr =  cur_frame_nr + (l_idx * stepsize);
    new_filename = gap_lib_alloc_fname(basename,
                                       l_potential_next_frame_nr,
                                       extension);
    if (new_filename == NULL)
    {
      break;
    }
    else
    {
      if(g_file_test(new_filename, G_FILE_TEST_EXISTS))
      {
         l_next_frame_nr = l_potential_next_frame_nr;
         l_frame_found = TRUE;
      }
      g_free(new_filename);
    }
    if (l_frame_found)
    {
      break;
    }
  }

  if (l_frame_found != TRUE)
  {
     l_next_frame_nr = gap_lib_dir_find_next_frame_number(cur_frame_nr
                         , stepsize
                         , basename
                         , extension
                         , &l_frame_found
                         );
  }


  if (frame_found_ptr != NULL)
  {
    *frame_found_ptr = l_frame_found;
  }
  return (l_next_frame_nr);


}  /* end gap_lib_get_next_available_frame_number */



/* ----------------------------------
 * gap_lib_dir_find_next_frame_number
 * ----------------------------------
 * returns the next available frame_number in sequence
 * for the frame that is specified by basename, cur_frame_nr, extension.
 * (for ascending sequence use stepsize 1
 * for descending sequence use stepsize -1)
 * if the frame image file with the next expected frame number is not
 * found, the number is derived from the next available frame image file.
 *
 * NOTE that this variant performs an uncodintional directory scan
 * of the directory that contains the specified imagename.
 * For performance reasons gap_lib_get_next_available_frame_number
 * should be used where possible.
 *
 * if there are no more next frame images,
 *     *frame_found_ptr is set to FALSE (if it is not NULL)
 *     and cur_frame_nr + stepsize is returned
 * otherwise (when a next frame image was found)
 *     *frame_found_ptr is set to TRUE (if it is not NULL)
 *     and the frame number of the next matching frame image is returned.
 */
long
gap_lib_dir_find_next_frame_number(long cur_frame_nr, long stepsize,
  char *basename, char *extension, gboolean *frame_found_ptr)
{
  char        *imagename;
  long         l_next_frame_nr;

  l_next_frame_nr = cur_frame_nr + stepsize;

  imagename = gap_lib_alloc_fname(basename,
                                  cur_frame_nr,
                                  extension);
  l_next_frame_nr = gap_lib_dir_find_next_frame_number_for_imagename(
                      stepsize, imagename, frame_found_ptr);
  g_free(imagename);

  return (l_next_frame_nr);

}       /* end gap_lib_dir_find_next_frame_number */


/* -----------------------------------------------
 * gap_lib_dir_find_next_frame_number_for_imagename
 * ------------------------------------------------
 * returns the next available frame_number for the specified frame imganame in sequence
 * (for ascending sequence use stepsize 1
 * for descending sequence use stepsize -1)
 * if the frame image file with the next expected frame number is not
 * found, the number is derived from the next available frame image file.
 *
 * NOTE that this variant performs a directory scan of the directory
 * that contains the specified imagename.
 *
 *
 * if there are no more next frame images,
 *     *frame_found_ptr is set to FALSE (if it is not NULL)
 *     and cur_frame_nr + stepsize is returned
 * otherwise (when a next frame image was found)
 *     *frame_found_ptr is set to TRUE (if it is not NULL)
 *     and the frame number of the next matching frame image is returned.
 */
long
gap_lib_dir_find_next_frame_number_for_imagename(long stepsize,
  char *imagename, gboolean *frame_found_ptr)
{
  GapAnimInfo *ainfo_ptr;
  long         l_next_frame_nr;
  gboolean     l_frame_found;

  l_next_frame_nr = -1;
  l_frame_found = FALSE;

  ainfo_ptr = gap_lib_alloc_ainfo_from_name(imagename, GIMP_RUN_NONINTERACTIVE);
  if (ainfo_ptr == NULL)
  {
    printf("gap_lib_dir_find_next_frame_number_for_imagename: FAILED, ainfo_ptr is null for %s\n"
          , imagename);
  }
  else
  {
    l_next_frame_nr = ainfo_ptr->curr_frame_nr + stepsize;

    if (0 == gap_lib_dir_ainfo(ainfo_ptr))
    {
      if (stepsize >= 0)
      {
        if (ainfo_ptr->frame_nr_after_curr_frame_nr >= 0)
        {
          l_frame_found = TRUE;
          l_next_frame_nr = ainfo_ptr->frame_nr_after_curr_frame_nr;
        }
      }
      else
      {
        if (ainfo_ptr->frame_nr_before_curr_frame_nr >= 0)
        {
          l_frame_found = TRUE;
          l_next_frame_nr = ainfo_ptr->frame_nr_before_curr_frame_nr;
        }
      }

    }

    if(gap_debug)
    {
      printf("gap_lib_dir_find_next_frame_number_for_imagename: %s \n"
           "  l_next_frame_nr:%d found:%d TRUE:%d\n"
           "  curr_frame_nr:%d  frame_nr_after_curr_frame_nr:%d  frame_nr_before_curr_frame_nr:%d stepsize:%d \n"
          , imagename
          , (int)l_next_frame_nr
          , (int)l_frame_found
          , (int)TRUE
          , (int)ainfo_ptr->curr_frame_nr
          , (int)ainfo_ptr->frame_nr_after_curr_frame_nr
          , (int)ainfo_ptr->frame_nr_before_curr_frame_nr
          , (int)stepsize
          );
    }

    gap_lib_free_ainfo(&ainfo_ptr);
  }


  if (frame_found_ptr != NULL)
  {
    *frame_found_ptr = l_frame_found;
  }


  return (l_next_frame_nr);
}       /* end gap_lib_dir_find_next_frame_number_for_imagename */


/* -------------------------------------
 * gap_lib_framefile_with_framenr_exists
 * -------------------------------------
 *
 * return FALSE ... file does not exist
 * return TRUE  ... file exists
 */
gboolean
gap_lib_framefile_with_framenr_exists(GapAnimInfo *ainfo_ptr, long frame_nr)
{
  char *filename;
  gboolean l_rc;

  l_rc = FALSE;

  /* build the frame name */
  filename = gap_lib_alloc_fname(ainfo_ptr->basename,
                                 frame_nr,
                                 ainfo_ptr->extension);
  if(filename != NULL)
  {
    l_rc = g_file_test(filename, G_FILE_TEST_EXISTS);
    g_free(filename);
  }

  return (l_rc);

}       /* end gap_lib_framefile_with_framenr_exists */
