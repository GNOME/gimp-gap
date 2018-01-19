/* gap_base_ops.c
 * 1997.11.18 hof (Wolfgang Hofer)
 *
 * GAP ... Gimp Animation Plugins
 *
 * basic Videomenu functions:
 *   Delete, Duplicate, Density, Exchange, Shift
 *   Next, Prev, First, Last, Goto
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
 * along with this program; if not, see
 * <http://www.gnu.org/licenses/>.
 */

/* revision history:
 * 2.8.xx;  2017/04/04    hof: added gap_base_rename
 * 1.3.17b; 2003/07/31   hof: message text fixes for translators (# 118392)
 * 1.3.16b; 2003/07/04   hof: added gap_density, confirm dialog for frame deleting operations
 * 1.3.15a  2003/06/21   hof: textspacing
 * 1.3.14a  2003/05/27   hof: created (module was splitted off from gap_lib)
 */

#include "config.h"

/* SYSTEM (UNIX) includes */
#include <stdlib.h>

#include <glib/gstdio.h>

/* GIMP includes */
#include "gtk/gtk.h"
#include "gap-intl.h"
#include "libgimp/gimp.h"


/* GAP includes */
#include "gap_layer_copy.h"
#include "gap_lib.h"
#include "gap_base_ops.h"
#include "gap_pdb_calls.h"
#include "gap_arr_dialog.h"
#include "gap_lock.h"
#include "gap_thumbnail.h"


extern      int gap_debug; /* ==0  ... dont print debug infos */

#define GAP_HELP_ID_DUPLICATE         "plug-in-gap-dup"
#define GAP_HELP_ID_DELETE            "plug-in-gap-del"
#define GAP_HELP_ID_DENSITY           "plug-in-gap-density"
#define GAP_HELP_ID_EXCHANGE          "plug-in-gap-exchg"
#define GAP_HELP_ID_RENUMBER          "plug-in-gap-renumber"
#define GAP_HELP_ID_RENAME            "plug-in-gap-rename"
#define GAP_HELP_ID_SHIFT             "plug-in-gap-shift"
#define GAP_HELP_ID_REVERSE           "plug-in-gap-reverse"


/* ------------------------
 * p_density_shrink
 * ------------------------
 * shrink framedensity in the selected range (range_from - range_to)
 * by density_factor (is used as divisor here).
 * a value of 2.0 results in deleting half of the frames
 * (by deleting every 2.nd frame in the affected range)
 * the range will playback with double speed when using the original framerate.
 */
static gint32
p_density_shrink(GapAnimInfo *ainfo_ptr
         , gdouble density_factor
         , long range_from, long range_to)
{
   gint32  l_lo, l_hi;
   gint32  l_framenr;
   gint32  l_last_kept_nr;
   gdouble    l_percentage, l_percentage_step;


   if(ainfo_ptr->run_mode == GIMP_RUN_INTERACTIVE)
   {
     gimp_progress_init(_("Decreasing density by deleting frames..."));
   }

   l_percentage = 0.0;
   l_percentage_step = 1.0 / (gdouble)(ainfo_ptr->last_frame_nr - range_from);

   l_framenr = range_from;
   l_last_kept_nr = -1;
   /* rename and delete loop */
   for(l_hi = range_from; l_hi <= range_to; l_hi++)
   {
     gdouble l_fnr;

     /* l_lo is the source framenumber in the (time) shrinked variant */
     l_fnr = ((gdouble)(l_hi - range_from) / density_factor);
     l_lo = range_from + (gint32)l_fnr;

     if(gap_debug) printf("p_density_shrink: (ren/dup loop) l_lo: %d  l_hi:%d last_kept:%d framenr:%d\n"
                         , (int)l_lo
                         , (int)l_hi
                         , (int)l_last_kept_nr
                         , (int)l_framenr
                         );

     if(l_lo == l_last_kept_nr)
     {
        /* 2 or more frames would result in the the same framenr
         * (in the shrinked range)
         * in this case we must delete this frame(s)
         */
        gap_lib_delete_frame(ainfo_ptr, l_hi);
     }
     else
     {
       if(l_hi != l_framenr)
       {
         if(0 != gap_lib_rename_frame(ainfo_ptr, l_hi, l_framenr))
         {
           gchar *tmp_errtxt;
           tmp_errtxt = g_strdup_printf(_("Error: could not rename frame %d to %d"), (int)l_hi, (int)l_framenr);
           gap_arr_msg_win(ainfo_ptr->run_mode, tmp_errtxt);
           g_free(tmp_errtxt);
           return -1;
         }
       }
       l_last_kept_nr = l_framenr;
       l_framenr++;
     }

     /* update progress for INTERACTIVE processing */
     if(ainfo_ptr->run_mode == GIMP_RUN_INTERACTIVE)
     {
        l_percentage += l_percentage_step;
        gimp_progress_update (l_percentage);
     }
   }

   /* down_shift rename loop
    * (framenumber higher than range_to are renamed with lower numbers
    *  to close up the leak in the numberspace that was
    *  left in the 1.loop when frames were deleted.)
    */
   l_lo = l_framenr;
   l_hi = range_to +1;
   while(l_hi <= ainfo_ptr->last_frame_nr  )
   {
     if(0 != gap_lib_rename_frame(ainfo_ptr, l_hi, l_lo))
     {
        gchar *tmp_errtxt;
        tmp_errtxt = g_strdup_printf(_("Error: could not rename frame %d to %d"), (int)l_hi, (int)l_lo);
        gap_arr_msg_win(ainfo_ptr->run_mode, tmp_errtxt);
        g_free(tmp_errtxt);
        return -1;
     }

     if(ainfo_ptr->run_mode == GIMP_RUN_INTERACTIVE)
     {
        l_percentage += l_percentage_step;
        gimp_progress_update (l_percentage);
     }

     l_hi++;
     l_lo++;
   }

   l_lo--;  /* this is the new last framenumber */

   /* recalculate last and total frames */
   ainfo_ptr->frame_cnt -= (ainfo_ptr->last_frame_nr - l_lo);
   ainfo_ptr->last_frame_nr = l_lo;

   return 0;  /* OK */

}  /* end p_density_shrink */


/* ------------------------
 * p_density_grow
 * ------------------------
 * grow framedensity in the selected range (range_from - range_to)
 * by density_factor.
 * a value of 2.0 results in duplicating
 * all frames within the range.
 * the range will playback with half speed when using the original framerate.
 */
static gint32
p_density_grow(GapAnimInfo *ainfo_ptr
         , gdouble density_factor
         , long range_from, long range_to)
{
   gint32  l_lo, l_hi;
   gint32  l_alias_lo;
   gint32  l_alias_nr;
   gint32  src_range_size;
   gint32  copied_frames;
   gdouble target_range_fsize;
   gdouble    l_percentage, l_percentage_step;

   if(ainfo_ptr->run_mode == GIMP_RUN_INTERACTIVE)
   {
     gimp_progress_init(_("Density duplicating frames..."));
   }

   src_range_size = 1 + (range_to - range_from);
   target_range_fsize = (gdouble)(src_range_size) * density_factor;
   copied_frames = (gint)(target_range_fsize) - src_range_size;

   l_percentage = 0.0;
   l_percentage_step = 1.0 / (gdouble)((gint32)target_range_fsize
                                      + (ainfo_ptr->last_frame_nr - range_to));

   /* up_shift rename loop
    * (framenumber higher than range_to are renamed with higher numbers
    *  to get free numberspace for the duplicates that will be created
    *  in the 2.nd loop)
    */
   l_lo   = ainfo_ptr->last_frame_nr;
   while(l_lo > range_to )
   {
     l_hi   = l_lo + copied_frames;
     if(0 != gap_lib_rename_frame(ainfo_ptr, l_lo, l_hi))
     {
        gchar *tmp_errtxt;
        tmp_errtxt = g_strdup_printf(_("Error: could not rename frame %d to %d"), (int)l_lo, (int)l_hi);
        gap_arr_msg_win(ainfo_ptr->run_mode, tmp_errtxt);
        g_free(tmp_errtxt);
        return -1;
     }

     if(ainfo_ptr->run_mode == GIMP_RUN_INTERACTIVE)
     {
        l_percentage += l_percentage_step;
        gimp_progress_update (l_percentage);
     }

     l_lo--;
   }


   l_alias_lo = -1;
   l_alias_nr = -1;

   /* rename and duplicate loop */
   for(l_hi = range_to + copied_frames; l_hi >= range_from; l_hi--)
   {
     gdouble l_flo;

     /* pick the source framenumber (l_lo) */
     l_flo = ((gdouble)(l_hi - range_from) / density_factor);
     l_lo = range_from + (gint32)l_flo;

     if(gap_debug) printf("p_density_grow: (ren/dup loop) l_lo: %d  l_hi:%d\n", (int)l_lo, (int)l_hi);

     if(l_lo == l_alias_lo)
     {
        gchar *l_alias_name;
        gchar *l_dup_name;

        /* the source framenumber (l_lo) was already renamed
         * to l_alias_nr. we must create a copy in that case
         * where the new framenumber (l_alias_nr) is the source framenumber
         */
        l_alias_name = gap_lib_alloc_fname(ainfo_ptr->basename, l_alias_nr, ainfo_ptr->extension);
        l_dup_name = gap_lib_alloc_fname(ainfo_ptr->basename, l_hi, ainfo_ptr->extension);
        if((l_dup_name != NULL) && (l_alias_name != NULL))
        {
           gap_lib_image_file_copy(l_alias_name, l_dup_name);
           g_free(l_dup_name);
           g_free(l_alias_name);
        }
     }
     else
     {
       if(l_lo != l_hi)
       {
         if(0 != gap_lib_rename_frame(ainfo_ptr, l_lo, l_hi))
         {
           gchar *tmp_errtxt;
           tmp_errtxt = g_strdup_printf(_("Error: could not rename frame %d to %d"), (int)l_lo, (int)l_hi);
           gap_arr_msg_win(ainfo_ptr->run_mode, tmp_errtxt);
           g_free(tmp_errtxt);
           return -1;
         }
       }
       l_alias_lo = l_lo;
       l_alias_nr = l_hi;
     }

     /* update progress for INTERACTIVE processing */
     if(ainfo_ptr->run_mode == GIMP_RUN_INTERACTIVE)
     {
        l_percentage += l_percentage_step;
        gimp_progress_update (l_percentage);
     }
   }

   /* recalculate last and total frames */
   ainfo_ptr->frame_cnt += copied_frames;
   ainfo_ptr->last_frame_nr += copied_frames;

   return 0;  /* OK */

}  /* end p_density_grow */



/* ------------------------
 * p_density
 * ------------------------
 * change the frame density by duplicating all frames within
 * the selected range (density_factor times)
 * or by deleting 100/density_factor Percent of the frames.
 * Depending on the density_grow flag.
 *
 * all following frames are renamed (renumbered up by cnt)
 * current frame is duplicated (cnt) times
 *
 * return image_id (of the new loaded frame) on success
 *        or -1 on errors
 */
static gint32
p_density(GapAnimInfo *ainfo_ptr
         , long range_from
         , long range_to
         , gdouble density_factor
         , gboolean density_grow
         )
{
   gint32 l_rc;
   char *l_curr_name;

   l_rc = -1;
   if(gap_debug) printf("p_density from:%d to:%d density: %.4f grow:%d extension:%s: basename:%s frame_cnt:%d\n"
                         , (int)range_from
                         , (int)range_to
                         , (float)density_factor
                         , (int)density_grow
                         , ainfo_ptr->extension
                         , ainfo_ptr->basename
                         , (int)ainfo_ptr->frame_cnt);

   if(density_factor <= 1.0) return -1;

   l_curr_name = gap_lib_alloc_fname(ainfo_ptr->basename, ainfo_ptr->curr_frame_nr, ainfo_ptr->extension);
   if(l_curr_name == NULL)
   {
     return -1;
   }

   if(gap_lib_gap_check_save_needed(ainfo_ptr->image_id))
   {
     /* save current frame  */
     if(gap_lib_save_named_frame(ainfo_ptr->image_id, l_curr_name) < 0)
     {
       gchar *tmp_errtxt;
       tmp_errtxt = g_strdup_printf(_("Error: could not save frame %s"), l_curr_name);
       gap_arr_msg_win(ainfo_ptr->run_mode, tmp_errtxt);
       g_free(tmp_errtxt);
       g_free(l_curr_name);
       return -1;
     }
   }

   /* use a new name (000001.xcf Konvention) */
   gimp_image_set_filename (ainfo_ptr->image_id, l_curr_name);
   g_free(l_curr_name);

   /* clip range */
   if(range_from > ainfo_ptr->last_frame_nr)  range_from = ainfo_ptr->last_frame_nr;
   if(range_to   > ainfo_ptr->last_frame_nr)  range_to   = ainfo_ptr->last_frame_nr;
   if(range_from < ainfo_ptr->first_frame_nr) range_from = ainfo_ptr->first_frame_nr;
   if(range_to   < ainfo_ptr->first_frame_nr) range_to   = ainfo_ptr->first_frame_nr;


   if(density_grow)
   {
     l_rc = p_density_grow(ainfo_ptr
                            , density_factor
                            , range_from
                            , range_to
                            );
   }
   else
   {
     l_rc = p_density_shrink(ainfo_ptr
                            , density_factor
                            , range_from
                            , range_to
                            );
   }

   if(l_rc < 0)
   {
     return(-1);
   }

   /* load from the "new" current frame */
   ainfo_ptr->curr_frame_nr = range_from;
   if(ainfo_ptr->new_filename != NULL) g_free(ainfo_ptr->new_filename);
   ainfo_ptr->new_filename = gap_lib_alloc_fname(ainfo_ptr->basename,
                                      ainfo_ptr->curr_frame_nr,
                                      ainfo_ptr->extension);
   return (gap_lib_load_named_frame(ainfo_ptr->image_id, ainfo_ptr->new_filename));
}        /* end p_density */



/* ============================================================================
 * p_del
 *
 * delete cnt frames starting at current
 * all following frames are renamed (renumbered down by cnt)
 *
 * return image_id (of the new loaded frame) on success
 *        or -1 on errors
 * ============================================================================
 */
static gint32
p_del(GapAnimInfo *ainfo_ptr, long cnt)
{
   long  l_lo, l_hi, l_curr, l_idx;
   gboolean l_hi_frame_found;

   if(gap_debug) fprintf(stderr, "DEBUG  p_del\n");

   if(cnt < 1) return -1;

   l_curr =  ainfo_ptr->curr_frame_nr;
   if((1 + ainfo_ptr->last_frame_nr - l_curr) < cnt)
   {
      /* limt cnt to last existing frame */
      cnt = 1 + ainfo_ptr->frame_cnt - l_curr;
   }

   if(cnt >= ainfo_ptr->frame_cnt)
   {
      /* dont want to delete all frames
       * so we have to leave a rest of one frame
       */
      cnt--;
   }


   l_idx   = l_curr;
   while(l_idx < (l_curr + cnt))
   {
      gap_lib_delete_frame(ainfo_ptr, l_idx);
      l_idx++;
   }

   /* rename (renumber) all frames with number greater than current
    */
   l_lo   = l_curr;
   l_hi   = l_curr + cnt;
   l_hi_frame_found = gap_lib_framefile_with_framenr_exists(ainfo_ptr, l_hi);
   while(l_hi <= ainfo_ptr->last_frame_nr)
   {
     l_lo = l_hi - cnt;
     if(l_hi_frame_found)
     {
      if(0 != gap_lib_rename_frame(ainfo_ptr, l_hi, l_lo))
      {
         gchar *tmp_errtxt;

         tmp_errtxt = g_strdup_printf(_("Error: could not rename frame %ld to %ld") ,l_hi, l_lo);
         gap_arr_msg_win(ainfo_ptr->run_mode, tmp_errtxt);
         g_free(tmp_errtxt);
         return -1;
       }
     }
     /* advance l_hi to the next available frame number 
      * (normally to l_hi += 1; sometimes to higher number when frames are missing) 
      */
     l_hi = gap_lib_get_next_available_frame_number(l_hi, 1
                           , ainfo_ptr->basename, ainfo_ptr->extension, &l_hi_frame_found);

   }

   /* calculate how much frames are left */
   ainfo_ptr->frame_cnt -= cnt;
   ainfo_ptr->last_frame_nr = ainfo_ptr->first_frame_nr + ainfo_ptr->frame_cnt -1;

   /* set current position to previous frame (if there is one) */
   if(l_curr > ainfo_ptr->first_frame_nr)
   {
     ainfo_ptr->frame_nr = l_curr -1;
   }
   else
   {
      ainfo_ptr->frame_nr = ainfo_ptr->first_frame_nr;
   }

   /* make filename, then load the new current frame */
   if(ainfo_ptr->new_filename != NULL) g_free(ainfo_ptr->new_filename);
   ainfo_ptr->new_filename = gap_lib_alloc_fname(ainfo_ptr->basename,
                                      ainfo_ptr->frame_nr,
                                      ainfo_ptr->extension);

   if(ainfo_ptr->new_filename == NULL)
      return -1;

   return (gap_lib_load_named_frame(ainfo_ptr->image_id, ainfo_ptr->new_filename));

}        /* end p_del */

/* ============================================================================
 * p_dup
 *
 * all following frames are renamed (renumbered up by cnt)
 * current frame is duplicated (cnt) times
 *
 * return image_id (of the new loaded frame) on success
 *        or -1 on errors
 * ============================================================================
 */
gint32
p_dup(GapAnimInfo *ainfo_ptr, long cnt, long range_from, long range_to)
{
   long  l_lo, l_hi;
   long  l_cnt2;
   long  l_step;
   long  l_src_nr, l_src_nr_min, l_src_nr_max;
   char  *l_dup_name;
   char  *l_curr_name;
   gdouble    l_percentage, l_percentage_step;
   gboolean   l_lo_frame_found;

   if(gap_debug) fprintf(stderr, "DEBUG  p_dup fr:%d to:%d cnt:%d extension:%s: basename:%s frame_cnt:%d\n",
                         (int)range_from, (int)range_to, (int)cnt, ainfo_ptr->extension, ainfo_ptr->basename, (int)ainfo_ptr->frame_cnt);

   if(cnt < 1) return -1;

   l_curr_name = gap_lib_alloc_fname(ainfo_ptr->basename, ainfo_ptr->curr_frame_nr, ainfo_ptr->extension);

   if(gap_lib_gap_check_save_needed(ainfo_ptr->image_id))
   {
     /* save current frame  */
     if(gap_lib_save_named_frame(ainfo_ptr->image_id, l_curr_name) < 0)
     {
       gchar *tmp_errtxt;
       tmp_errtxt = g_strdup_printf(_("Error: could not save frame %s"), l_curr_name);
       gap_arr_msg_win(ainfo_ptr->run_mode, tmp_errtxt);
       g_free(tmp_errtxt);
       return -1;
     }
   }

   /* use a new name (000001.xcf Konvention) */
   gimp_image_set_filename (ainfo_ptr->image_id, l_curr_name);
   g_free(l_curr_name);


   if((range_from <0 ) && (range_to < 0 ))
   {
      /* set range to one single current frame
       * (used for the old non_interactive PDB-interface without range params)
       */
      range_from = ainfo_ptr->curr_frame_nr;
      range_to   = ainfo_ptr->curr_frame_nr;
   }

   /* clip range */
   if(range_from > ainfo_ptr->last_frame_nr)  range_from = ainfo_ptr->last_frame_nr;
   if(range_to   > ainfo_ptr->last_frame_nr)  range_to   = ainfo_ptr->last_frame_nr;
   if(range_from < ainfo_ptr->first_frame_nr) range_from = ainfo_ptr->first_frame_nr;
   if(range_to   < ainfo_ptr->first_frame_nr) range_to   = ainfo_ptr->first_frame_nr;

   if(range_to < range_from)
   {
       /* invers range */
      l_cnt2 = ((range_from - range_to ) + 1) * cnt;
      l_step = -1;
      l_src_nr_max = range_from;
      l_src_nr_min = range_to;
   }
   else
   {
      l_cnt2 = ((range_to - range_from ) + 1) * cnt;
      l_step = 1;
      l_src_nr_max = range_to;
      l_src_nr_min = range_from;
   }

   if(gap_debug) fprintf(stderr, "DEBUG  p_dup fr:%d to:%d cnt2:%d l_src_nr_max:%d\n",
                         (int)range_from, (int)range_to, (int)l_cnt2, (int)l_src_nr_max);


   l_percentage = 0.0;
   if(ainfo_ptr->run_mode == GIMP_RUN_INTERACTIVE)
   {
     gimp_progress_init( _("Duplicating frames..."));
   }

   /* rename (renumber) all frames with number greater than current
    */
   l_lo   = ainfo_ptr->last_frame_nr;
   l_hi   = l_lo + l_cnt2;
   l_lo_frame_found = gap_lib_framefile_with_framenr_exists(ainfo_ptr, l_hi);
   while(l_lo > l_src_nr_max)
   {
     l_hi   = l_lo + l_cnt2;
     if (l_lo_frame_found)
     {
      if(0 != gap_lib_rename_frame(ainfo_ptr, l_lo, l_hi))
      {
         gchar *tmp_errtxt;
         tmp_errtxt = g_strdup_printf(_("Error: could not rename frame %ld to %ld"), l_lo, l_hi);
         gap_arr_msg_win(ainfo_ptr->run_mode, tmp_errtxt);
         g_free(tmp_errtxt);
         return -1;
       }
     }
     /* advance l_lo to the previous available frame number 
      * (normally to l_lo -= 1; sometimes to lower number when frames are missing) 
      */
     l_lo = gap_lib_get_next_available_frame_number(l_lo, -1
               , ainfo_ptr->basename, ainfo_ptr->extension, &l_lo_frame_found);
   }


   l_percentage_step = 1.0 / ((1.0 + l_hi) - l_src_nr_max);
   if(ainfo_ptr->run_mode == GIMP_RUN_INTERACTIVE)
   {
      l_percentage += l_percentage_step;
      gimp_progress_update (l_percentage);
   }

   /* copy cnt duplicates */
   l_src_nr = range_to;
   while(l_hi > l_src_nr_max)
   {
      l_curr_name = gap_lib_alloc_fname(ainfo_ptr->basename, l_src_nr, ainfo_ptr->extension);
      l_dup_name = gap_lib_alloc_fname(ainfo_ptr->basename, l_hi, ainfo_ptr->extension);
      if((l_dup_name != NULL) && (l_curr_name != NULL))
      {
         if (g_file_test(l_curr_name, G_FILE_TEST_EXISTS))
         {
           gap_lib_image_file_copy(l_curr_name, l_dup_name);
         }
         g_free(l_dup_name);
         g_free(l_curr_name);
      }
      if(ainfo_ptr->run_mode == GIMP_RUN_INTERACTIVE)
      {
         l_percentage += l_percentage_step;
         gimp_progress_update (l_percentage);
      }


      l_src_nr -= l_step;
      if(l_src_nr < l_src_nr_min) l_src_nr = l_src_nr_max;
      if(l_src_nr > l_src_nr_max) l_src_nr = l_src_nr_min;

      l_hi--;
   }

   /* restore current position */
   ainfo_ptr->frame_cnt += l_cnt2;
   ainfo_ptr->last_frame_nr = ainfo_ptr->first_frame_nr + ainfo_ptr->frame_cnt -1;

   /* load from the "new" current frame */
   /* hof: the current frame stays the same after duplicating frames.
    * therefore we can skip the reload here.
    * (reload failed at the gimp_displays_reconnect call, when invoked from script-fu
    *  in NONINTERACTIVE mode, dont know why ?)
    */

   /* if(ainfo_ptr->new_filename != NULL) g_free(ainfo_ptr->new_filename);
    *ainfo_ptr->new_filename = gap_lib_alloc_fname(ainfo_ptr->basename,
    *                                   ainfo_ptr->curr_frame_nr,
    *                                   ainfo_ptr->extension);
    * return (gap_lib_load_named_frame(ainfo_ptr->image_id, ainfo_ptr->new_filename));
    */
   return (ainfo_ptr->image_id);
}        /* end p_dup */

/* ============================================================================
 * p_exchg
 *
 * save current frame, exchange its name with destination frame on disk
 * and reload current frame (now has contents of dest. and vice versa)
 *
 * return image_id (of the new loaded frame) on success
 *        or -1 on errors
 * ============================================================================
 */
gint32
p_exchg(GapAnimInfo *ainfo_ptr, long dest)
{
   long  l_tmp_nr;
   gchar *tmp_errtxt;

   l_tmp_nr = ainfo_ptr->last_frame_nr + 4;  /* use a free frame_nr for temp name */

   if((dest < 1) || (dest == ainfo_ptr->curr_frame_nr))
      return -1;

   if(gap_lib_gap_check_save_needed(ainfo_ptr->image_id))
   {
     if(gap_lib_save_named_frame(ainfo_ptr->image_id, ainfo_ptr->old_filename) < 0)
        return -1;
   }

   /* rename (renumber) frames */
   if(0 != gap_lib_rename_frame(ainfo_ptr, dest, l_tmp_nr))
   {
        tmp_errtxt = g_strdup_printf(_("Error: could not rename frame %ld to %ld"), dest, l_tmp_nr);
        gap_arr_msg_win(ainfo_ptr->run_mode, tmp_errtxt);
        g_free(tmp_errtxt);
        return -1;
   }
   if(0 != gap_lib_rename_frame(ainfo_ptr, ainfo_ptr->curr_frame_nr, dest))
   {
        tmp_errtxt = g_strdup_printf(_("Error: could not rename frame %ld to %ld"), ainfo_ptr->curr_frame_nr, dest);
        gap_arr_msg_win(ainfo_ptr->run_mode, tmp_errtxt);
        g_free(tmp_errtxt);
        return -1;
   }
   if(0 != gap_lib_rename_frame(ainfo_ptr, l_tmp_nr, ainfo_ptr->curr_frame_nr))
   {
        tmp_errtxt = g_strdup_printf(_("Error: could not rename frame %ld to %ld"), l_tmp_nr, ainfo_ptr->curr_frame_nr);
        gap_arr_msg_win(ainfo_ptr->run_mode, tmp_errtxt);
        g_free(tmp_errtxt);
        return -1;
   }

   /* load from the "new" current frame */
   if(ainfo_ptr->new_filename != NULL) g_free(ainfo_ptr->new_filename);
   ainfo_ptr->new_filename = gap_lib_alloc_fname(ainfo_ptr->basename,
                                      ainfo_ptr->curr_frame_nr,
                                      ainfo_ptr->extension);
   return(gap_lib_load_named_frame(ainfo_ptr->image_id, ainfo_ptr->new_filename));
}        /* end p_exchg */

/* ============================================================================
 * p_shift
 *
 * all frmaes in the given range are renumbered (shifted)
 * according to cnt:
 *  example:  cnt == 1 :  range before 3, 4, 5, 6, 7
 *                        range after  4, 5, 6, 7, 3
 *
 * return image_id (of the new loaded frame) on success
 *        or -1 on errors
 * ============================================================================
 */
static gint32
p_shift(GapAnimInfo *ainfo_ptr, long cnt, long range_from, long range_to)
{
   long  l_lo, l_hi, l_curr, l_dst;
   long  l_upper;
   long  l_shift;
   gchar *l_curr_name;
   gchar *tmp_errtxt;
   gboolean l_frame_found;

   gdouble    l_percentage, l_percentage_step;

   if(gap_debug) fprintf(stderr, "DEBUG  p_shift fr:%d to:%d cnt:%d\n",
                         (int)range_from, (int)range_to, (int)cnt);

   if(range_from == range_to) return -1;

   /* clip range */
   if(range_from > ainfo_ptr->last_frame_nr)  range_from = ainfo_ptr->last_frame_nr;
   if(range_to   > ainfo_ptr->last_frame_nr)  range_to   = ainfo_ptr->last_frame_nr;
   if(range_from < ainfo_ptr->first_frame_nr) range_from = ainfo_ptr->first_frame_nr;
   if(range_to   < ainfo_ptr->first_frame_nr) range_to   = ainfo_ptr->first_frame_nr;

   if(range_to < range_from)
   {
      l_lo = range_to;
      l_hi = range_from;
   }
   else
   {
      l_lo = range_from;
      l_hi = range_to;
   }

   /* limit shift  amount to number of frames in range */
   l_shift = cnt % (l_hi - l_lo);
   if(gap_debug) fprintf(stderr, "DEBUG  p_shift shift:%d\n",
                         (int)l_shift);
   if(l_shift == 0) return -1;

   l_curr_name = gap_lib_alloc_fname(ainfo_ptr->basename, ainfo_ptr->curr_frame_nr, ainfo_ptr->extension);
   if(gap_lib_gap_check_save_needed(ainfo_ptr->image_id))
   {
     /* save current frame  */
     gap_lib_save_named_frame(ainfo_ptr->image_id, l_curr_name);
   }
   g_free(l_curr_name);

   l_percentage = 0.0;
   if(ainfo_ptr->run_mode == GIMP_RUN_INTERACTIVE)
   {
     gimp_progress_init( _("Renumber frame sequence..."));
   }

   /* rename (renumber) all frames 
    * (using higher numbers than last available frame number)
    */

   l_upper = ainfo_ptr->last_frame_nr +100;
   l_percentage_step = 0.5 / ((1.0 + l_lo) - l_hi);
   l_curr = l_lo;
   while (l_curr <= l_hi)
   {
     l_frame_found = gap_lib_framefile_with_framenr_exists(ainfo_ptr, l_curr);
     if (l_frame_found)
     {
       if(0 != gap_lib_rename_frame(ainfo_ptr, l_curr, l_curr + l_upper))
       {
         tmp_errtxt = g_strdup_printf(_("Error: could not rename frame %ld to %ld"), l_lo, l_hi);
         gap_arr_msg_win(ainfo_ptr->run_mode, tmp_errtxt);
         g_free(tmp_errtxt);
         return -1;
       }
     }
     if(ainfo_ptr->run_mode == GIMP_RUN_INTERACTIVE)
     {
       l_percentage += l_percentage_step;
       gimp_progress_update (l_percentage);
     }
     /* advance l_curr to the next available frame number 
      * (normally to l_curr += 1; sometimes to higher number when frames are missing) 
      */
     l_curr = gap_lib_get_next_available_frame_number(l_curr, 1
                           , ainfo_ptr->basename, ainfo_ptr->extension, &l_frame_found);
   }

   /* rename (renumber) all frames (using desired destination numbers)
    */
   l_dst = l_lo + l_shift;
   if (l_dst > l_hi) { l_dst -= (l_lo -1); }
   if (l_dst < l_lo) { l_dst += ((l_hi - l_lo) +1); }
   for(l_curr = l_upper + l_lo; l_curr <= l_upper + l_hi; l_curr++)
   {
     if (l_dst > l_hi) 
     {
       l_dst = l_lo; 
     }
     l_frame_found = gap_lib_framefile_with_framenr_exists(ainfo_ptr, l_curr);
     if (l_frame_found)
     {
       if(0 != gap_lib_rename_frame(ainfo_ptr, l_curr, l_dst))
       {
         tmp_errtxt = g_strdup_printf(_("Error: could not rename frame %ld to %ld"), l_lo, l_hi);
         gap_arr_msg_win(ainfo_ptr->run_mode, tmp_errtxt);
         g_free(tmp_errtxt);
         return -1;
       }
     }
     if(ainfo_ptr->run_mode == GIMP_RUN_INTERACTIVE)
     {
       l_percentage += l_percentage_step;
       gimp_progress_update (l_percentage);
     }

     l_dst ++;
   }


   /* load from the "new" current frame */
   if(ainfo_ptr->new_filename != NULL) g_free(ainfo_ptr->new_filename);
   ainfo_ptr->new_filename = gap_lib_alloc_fname(ainfo_ptr->basename,
                                      ainfo_ptr->curr_frame_nr,
                                      ainfo_ptr->extension);
   return(gap_lib_load_named_frame(ainfo_ptr->image_id, ainfo_ptr->new_filename));
}  /* end p_shift */


/* ============================================================================
 * p_reverse
 *
 * all frames in the given range are renamed to reverse the order of the frame sequence range.
 * Note that frame numbers in the range will be the same before and after this procdure,
 * but content of the frame images is swapped.
 *
 *  examples:
 *      A,B,C  .... image content with FrameNumber (n)
 *   range before A(3), B(4), C(5), D(6), E(7)
 *   range after  E(3), D(4), C(5), B(6), A(7)
 *
 *   range before A(3), B(4), C(7), D(21), E(22), F(51)
 *   range after  F(3), E(4), D(7), C(21), B(22), A(51)
 *
 * return image_id (of the new loaded frame) on success
 *        or -1 on errors
 * ============================================================================
 */
static gint32
p_reverse(GapAnimInfo *ainfo_ptr, long range_from, long range_to)
{
   long  l_tmp_nr;
   long  l_lo, l_hi, l_curr;
   long  l_swap;
   gchar *tmp_errtxt;
   gdouble    l_percentage;

   l_tmp_nr = ainfo_ptr->last_frame_nr + 4;  /* use a free frame_nr for temp name */

   if(gap_debug) fprintf(stderr, "DEBUG  p_reverse fr:%d to:%d\n",
                         (int)range_from, (int)range_to);

   if(range_from == range_to) return -1;

   /* clip range */
   if(range_from > ainfo_ptr->last_frame_nr)  range_from = ainfo_ptr->last_frame_nr;
   if(range_to   > ainfo_ptr->last_frame_nr)  range_to   = ainfo_ptr->last_frame_nr;
   if(range_from < ainfo_ptr->first_frame_nr) range_from = ainfo_ptr->first_frame_nr;
   if(range_to   < ainfo_ptr->first_frame_nr) range_to   = ainfo_ptr->first_frame_nr;

   if(range_to < range_from)
   {
      l_lo = range_to;
      l_hi = range_from;
   }
   else
   {
      l_lo = range_from;
      l_hi = range_to;
   }

   if(l_hi <= l_lo) return -1;

   if(gap_lib_gap_check_save_needed(ainfo_ptr->image_id))
   {
     if(gap_lib_save_named_frame(ainfo_ptr->image_id, ainfo_ptr->old_filename) < 0)
        return -1;
   }

   l_percentage = 0.0;
   if(ainfo_ptr->run_mode == GIMP_RUN_INTERACTIVE)
   {
     gimp_progress_init( _("Renumber frame sequence..."));
   }


   /* swap lo with high for each of the (first half of the) frames
    * to reverse the frame sequence.
    */
   l_curr = l_lo;
   l_swap = l_hi - (l_curr - l_lo);
   while(1)
   {
     gboolean l_cur_frame_found;
     gboolean l_swap_frame_found;
     
     l_cur_frame_found = gap_lib_framefile_with_framenr_exists(ainfo_ptr, l_curr);
     if(l_cur_frame_found != TRUE)
     {
       /* advance l_curr to the next available frame number 
        * (normally to l_curr += 1; 
        * sometimes to higher number when frames are missing) 
        */
       l_curr = gap_lib_get_next_available_frame_number(l_curr, 1
                           , ainfo_ptr->basename, ainfo_ptr->extension, &l_cur_frame_found);
     }
     l_swap_frame_found = gap_lib_framefile_with_framenr_exists(ainfo_ptr, l_swap);
     if(l_swap_frame_found != TRUE)
     {
       /* advance l_swap to the previous available frame number 
        * (normally to l_swap -= 1; 
        * sometimes to lower number when frames are missing) 
        */
       l_swap = gap_lib_get_next_available_frame_number(l_swap, -1
                           , ainfo_ptr->basename, ainfo_ptr->extension, &l_swap_frame_found);
     }
     
     if ((l_cur_frame_found != TRUE) 
     ||  (l_swap_frame_found != TRUE)
     ||  (l_swap <= l_curr))
     {
        /* stop when no more frames found for swapping 
         * or all framenames in the range are already swapped.
         */
        break;  
     }

     /* rename hi (l_swap) to temp */
     if(0 != gap_lib_rename_frame(ainfo_ptr, l_swap, l_tmp_nr))
     {
       tmp_errtxt = g_strdup_printf(_("Error: could not rename frame %ld to %ld"), l_swap, l_tmp_nr);
       gap_arr_msg_win(ainfo_ptr->run_mode, tmp_errtxt);
       g_free(tmp_errtxt);
       return -1;
     }
     /* rename lo to hi */
     if(0 != gap_lib_rename_frame(ainfo_ptr, l_curr, l_swap))
     {
       tmp_errtxt = g_strdup_printf(_("Error: could not rename frame %ld to %ld"), ainfo_ptr->curr_frame_nr, l_swap);
       gap_arr_msg_win(ainfo_ptr->run_mode, tmp_errtxt);
       g_free(tmp_errtxt);
       return -1;
     }
     /* rename temp to lo */
     if(0 != gap_lib_rename_frame(ainfo_ptr, l_tmp_nr, l_curr))
     {
       tmp_errtxt = g_strdup_printf(_("Error: could not rename frame %ld to %ld"), l_tmp_nr, ainfo_ptr->curr_frame_nr);
       gap_arr_msg_win(ainfo_ptr->run_mode, tmp_errtxt);
       g_free(tmp_errtxt);
       return -1;
     }
     if (ainfo_ptr->run_mode == GIMP_RUN_INTERACTIVE)
     {
       l_percentage = (double)((l_curr - l_lo + 1) / ((l_hi - l_lo + 1) / 2.0 ));
       if (l_percentage > 1.0) l_percentage = 1.0;
       gimp_progress_update (l_percentage);
     }
     l_curr++;
     l_swap--;
   }

   /* load from the "new" current frame */
   if(ainfo_ptr->new_filename != NULL) g_free(ainfo_ptr->new_filename);
   ainfo_ptr->new_filename = gap_lib_alloc_fname(ainfo_ptr->basename,
                                      ainfo_ptr->curr_frame_nr,
                                      ainfo_ptr->extension);
   return(gap_lib_load_named_frame(ainfo_ptr->image_id, ainfo_ptr->new_filename));
}  /* end p_reverse */

/* ============================================================================
 * gap_base_next gap_base_prev
 *
 * store the current Gimp Image to the current video frame
 * and load it from the next/prev video frame on disk.
 *
 * return image_id (of the new loaded frame) on success
 *        or -1 on errors
 * ============================================================================
 */
gint32
gap_base_next(GimpRunMode run_mode, gint32 image_id)
{
  int rc;
  GapAnimInfo *ainfo_ptr;

  rc = -1;
  ainfo_ptr = gap_lib_alloc_ainfo(image_id, run_mode);
  if(ainfo_ptr != NULL)
  {
    gboolean l_frame_found;
 
    ainfo_ptr->frame_nr = 
          gap_lib_get_next_available_frame_number(ainfo_ptr->curr_frame_nr, 1
                      , ainfo_ptr->basename, ainfo_ptr->extension
                      , &l_frame_found
                      );
    if (l_frame_found)
    {
      rc = gap_lib_replace_image(ainfo_ptr);
    }

    gap_lib_free_ainfo(&ainfo_ptr);
  }

  return(rc);
}       /* end gap_base_next */

gint32
gap_base_prev(GimpRunMode run_mode, gint32 image_id)
{
  int rc;
  GapAnimInfo *ainfo_ptr;

  rc = -1;
  ainfo_ptr = gap_lib_alloc_ainfo(image_id, run_mode);
  if(ainfo_ptr != NULL)
  {
    gboolean l_frame_found;
    ainfo_ptr->frame_nr = 
          gap_lib_get_next_available_frame_number(ainfo_ptr->curr_frame_nr, -1
                      , ainfo_ptr->basename, ainfo_ptr->extension
                      , &l_frame_found
                      );
    if (l_frame_found)
    {
      rc = gap_lib_replace_image(ainfo_ptr);
    }

    gap_lib_free_ainfo(&ainfo_ptr);
  }

  return(rc);
}       /* end gap_base_prev */

/* ============================================================================
 * gap_base_first  gap_base_last
 *
 * store the current Gimp Image to the current video frame
 * and load it from the first/last video frame on disk.
 * ============================================================================
 */

gint32
gap_base_first(GimpRunMode run_mode, gint32 image_id)
{
  int rc;
  GapAnimInfo *ainfo_ptr;

  rc = -1;
  ainfo_ptr = gap_lib_alloc_ainfo(image_id, run_mode);
  if(ainfo_ptr != NULL)
  {
    if (0 == gap_lib_dir_ainfo(ainfo_ptr))
    {
      ainfo_ptr->frame_nr = ainfo_ptr->first_frame_nr;
      rc = gap_lib_replace_image(ainfo_ptr);
    }
    gap_lib_free_ainfo(&ainfo_ptr);
  }

  return(rc);
}       /* end gap_base_first */

gint32
gap_base_last(GimpRunMode run_mode, gint32 image_id)
{
  int rc;
  GapAnimInfo *ainfo_ptr;

  rc = -1;
  ainfo_ptr = gap_lib_alloc_ainfo(image_id, run_mode);
  if(ainfo_ptr != NULL)
  {
    if (0 == gap_lib_dir_ainfo(ainfo_ptr))
    {
      ainfo_ptr->frame_nr = ainfo_ptr->last_frame_nr;
      rc = gap_lib_replace_image(ainfo_ptr);
    }
    gap_lib_free_ainfo(&ainfo_ptr);
  }

  return(rc);
}       /* end gap_base_last */

/* ============================================================================
 * gap_base_goto
 *
 * store the current Gimp Image to disk
 * and load it from the video frame on disk that has the specified frame Nr.
 * GIMP_RUN_INTERACTIVE:
 *    show dialogwindow where user can enter the destination frame Nr.
 * ============================================================================
 */
gint32
gap_base_goto(GimpRunMode run_mode, gint32 image_id, int nr)
{
  int rc;
  GapAnimInfo *ainfo_ptr;

  long            l_dest;
  gchar          *l_hline;
  gchar          *l_title;

  rc = -1;
  ainfo_ptr = gap_lib_alloc_ainfo(image_id, run_mode);
  if(ainfo_ptr != NULL)
  {
    if (0 == gap_lib_dir_ainfo(ainfo_ptr))
    {
      if(0 != gap_lib_chk_framerange(ainfo_ptr))   return -1;

      if(run_mode == GIMP_RUN_INTERACTIVE)
      {
        l_title = g_strdup_printf (_("Go To Frame (%ld/%ld)")
                                   , ainfo_ptr->curr_frame_nr
                                   , ainfo_ptr->frame_cnt);
        l_hline =  g_strdup_printf (_("Destination Frame Number (%ld - %ld)")
                                    , ainfo_ptr->first_frame_nr
                                    , ainfo_ptr->last_frame_nr);

        l_dest = gap_arr_slider_dialog(l_title, l_hline,
                 _("Number:")
                ,_("Go to this frame number")                 /* tooltip */
                , ainfo_ptr->first_frame_nr
                , ainfo_ptr->last_frame_nr
                , ainfo_ptr->curr_frame_nr
                , TRUE
                , NULL  /* help_id NULL: has no help button */
                );

        g_free (l_title);
        g_free (l_hline);

        if(l_dest < 0)
        {
           /* Cancel button: go back to current frame */
           l_dest = ainfo_ptr->curr_frame_nr;
        }
        if(0 != gap_lib_chk_framechange(ainfo_ptr))
        {
           l_dest = -1;
        }
      }
      else
      {
        l_dest = nr;
      }

      if(l_dest >= 0)
      {
        ainfo_ptr->frame_nr = l_dest;
        rc = gap_lib_replace_image(ainfo_ptr);
      }

    }
    gap_lib_free_ainfo(&ainfo_ptr);
  }

  return(rc);
}       /* end gap_base_goto */


/* ------------------------
 * p_delete_confirm_dialog
 * ------------------------
 */
static gboolean
p_delete_confirm_dialog(GapAnimInfo *ainfo_ptr, long range_from, long range_to)
{
  gchar *msg_txt;
  gboolean l_rc;

  msg_txt = g_strdup_printf(_("Frames %d - %d will be deleted. "
                    "There will be no undo for this operation.")
                   , (int)range_from
                   , (int)range_to
                 );
  l_rc = gap_arr_confirm_dialog(msg_txt
                         , _("Confirm Frame Delete")   /* title_txt */
                         , _("Confirm Frame Delete")   /* frame_txt */
                         );
  g_free(msg_txt);

  return (l_rc);

}  /* end p_delete_confirm_dialog */


/* ============================================================================
 * gap_base_del
 * ============================================================================
 */
gint32
gap_base_del(GimpRunMode run_mode, gint32 image_id, int nr)
{
  int rc;
  GapAnimInfo *ainfo_ptr;

  long           l_cnt;
  long           l_delete_to_frame_nr;
  long           l_max;
  gchar         *l_hline;
  gchar         *l_title;
  gchar         *l_tooltip;

  rc = -1;
  l_cnt = -1;
  ainfo_ptr = gap_lib_alloc_ainfo(image_id, run_mode);
  if(ainfo_ptr != NULL)
  {
    if (0 == gap_lib_dir_ainfo(ainfo_ptr))
    {
      if(0 != gap_lib_chk_framerange(ainfo_ptr))   return -1;

      if(run_mode == GIMP_RUN_INTERACTIVE)
      {
        l_title = g_strdup_printf (_("Delete Frames (%ld/%ld)")
                                   , ainfo_ptr->curr_frame_nr
                                   , ainfo_ptr->frame_cnt);
        l_hline = g_strdup_printf (_("Delete frames from %ld to (number)")
                                   , ainfo_ptr->curr_frame_nr);

        l_max = ainfo_ptr->last_frame_nr;
        if(l_max == ainfo_ptr->curr_frame_nr)
        {
          /* bugfix: the slider needs a maximum > minimum
           *         (if not an error is printed, and
           *          a default range 0 to 100 is displayed)
           */
          l_max++;
        }

        l_tooltip = g_strdup_printf(_("Delete frames starting at current number %d "
                                      "up to this number (inclusive)")
                    , (int)ainfo_ptr->curr_frame_nr );
        l_delete_to_frame_nr = gap_arr_slider_dialog(l_title, l_hline
              , _("Number:")
              , l_tooltip
              , ainfo_ptr->curr_frame_nr
              , l_max
              , ainfo_ptr->curr_frame_nr
              , TRUE
              , GAP_HELP_ID_DELETE);

        g_free (l_tooltip);
        g_free (l_title);
        g_free (l_hline);

        if(l_delete_to_frame_nr >= 0)
        {
           l_cnt = 1 + l_delete_to_frame_nr - ainfo_ptr->curr_frame_nr;

           /* ask the user to confirm delete (there is no undo) */
           if(!p_delete_confirm_dialog(ainfo_ptr
                                     ,ainfo_ptr->curr_frame_nr
                                     ,ainfo_ptr->curr_frame_nr + (l_cnt -1)))
           {
              l_cnt = -1;  /* Canceled by confirm dialog */
           }
        }
        if(0 != gap_lib_chk_framechange(ainfo_ptr))
        {
           l_cnt = -1;
        }
      }
      else
      {
        l_cnt = nr;
      }

      if(l_cnt >= 0)
      {
         /* delete l_cnt number of frames (-1 CANCEL button) */

         rc = p_del(ainfo_ptr, l_cnt);
      }


    }
    gap_lib_free_ainfo(&ainfo_ptr);
  }

  return(rc);

}       /* end gap_base_del */


/* ------------------------
 * p_density_confirm_dialog
 * ------------------------
 */
static gboolean
p_density_confirm_dialog(GapAnimInfo *ainfo_ptr, long range_from, long range_to
                , gdouble density_factor, gboolean density_grow)
{
  gchar *msg_txt;
  gdouble new_total_frames;
  gdouble l_src_range_size;
  gdouble l_target_range_size;
  gboolean l_rc;

  msg_txt = NULL;
  l_src_range_size = 1+ range_to - range_from;

  if(density_grow)
  {
    l_target_range_size = l_src_range_size * density_factor;
    new_total_frames = (gdouble)ainfo_ptr->frame_cnt
                     - l_src_range_size
                     + l_target_range_size
                     ;

    msg_txt =
    g_strdup_printf(_("Frames in the range: %d - %d will be duplicated %.4f times.\n"
                      "This will increase the total number of frames from %d up to %d.\n"
                      "There will be no undo for this operation\n")
                   , (int)range_from
                   , (int)range_to
                   , (float)density_factor
                   , (int)ainfo_ptr->frame_cnt
                   , (int)new_total_frames
                    );
  }
  else
  {
    l_target_range_size = l_src_range_size / density_factor;
    new_total_frames = (gdouble)ainfo_ptr->frame_cnt
                     - l_src_range_size
                     + l_target_range_size
                     ;
    msg_txt =
    g_strdup_printf(_("%.04f percent of the frames in the range: %d - %d\n"
                      "will be deleted.\n"
                      "This will decrease the total number of frames from %d down to %d\n"
                      "There will be no undo for this operation\n")
                   , (float)100.0 - (100.0 / l_src_range_size * l_target_range_size)
                   , (int)range_from
                   , (int)range_to
                   , (int)ainfo_ptr->frame_cnt
                   , (int)new_total_frames
                    );
  }

  l_rc = gap_arr_confirm_dialog(msg_txt
                         , _("Confirm Frame Density Change")   /* title_txt */
                         , _("Confirm Frame Density Change")   /* frame_txt */
                         );
  g_free(msg_txt);

  return (l_rc);

}  /* end p_density_confirm_dialog */


/* -----------------
 * p_density_dialog
 * -----------------
 */
static gboolean
p_density_dialog(GapAnimInfo *ainfo_ptr, long *range_from, long *range_to
                , gdouble *density_factor, gboolean *density_grow)
{
  static GapArrArg  argv[5];
  gchar            *l_title;
  gint              l_rci;
  gboolean          l_rc;

  l_title = g_strdup_printf (_("Change Frame Density"));

  gap_arr_arg_init(&argv[0], GAP_ARR_WGT_INT_PAIR);
  argv[0].label_txt = _("From Frame:");
  argv[0].constraint = TRUE;
  argv[0].int_min   = (gint)ainfo_ptr->first_frame_nr;
  argv[0].int_max   = (gint)ainfo_ptr->last_frame_nr;
  argv[0].int_ret   = (gint)ainfo_ptr->curr_frame_nr;
  argv[0].help_txt  = _("Affected range starts at this framenumber");

  gap_arr_arg_init(&argv[1], GAP_ARR_WGT_INT_PAIR);
  argv[1].label_txt = _("To Frame:");
  argv[1].constraint = TRUE;
  argv[1].int_min   = (gint)ainfo_ptr->first_frame_nr;
  argv[1].int_max   = (gint)ainfo_ptr->last_frame_nr;
  argv[1].int_ret   = (gint)ainfo_ptr->last_frame_nr;
  argv[1].help_txt  = _("Affected range ends at this framenumber");

  gap_arr_arg_init(&argv[2], GAP_ARR_WGT_FLT_PAIR);
  argv[2].label_txt = _("Density:");
  argv[2].constraint = FALSE;
  argv[2].flt_min   = 1.0;
  argv[2].flt_max   = 10.0;
  argv[2].flt_step  = 0.1;
  argv[2].pagestep  = 1.0;
  argv[2].flt_ret   = 2;
  argv[2].flt_digits = 4;
  argv[2].umin      = 1.0;
  argv[2].umax      = 100.0;
  argv[2].help_txt  = _("Factor to increase the frame density (acts as divisor if checkbutton increase density is off)");

  gap_arr_arg_init(&argv[3], GAP_ARR_WGT_TOGGLE);
  argv[3].label_txt = _("Increase Density");
  argv[3].help_txt  = _("ON: Duplicate frames to get a target rate that is density * original_rate..\n"
                        "OFF: Delete frames to get a target rate that is original_rate/density.");
  argv[3].int_ret   = 1;

  gap_arr_arg_init(&argv[4], GAP_ARR_WGT_HELP_BUTTON);
  argv[4].help_id = GAP_HELP_ID_DENSITY;

  l_rci = gap_arr_ok_cancel_dialog(l_title, _("Change Frames Density"),  5, argv);
  g_free (l_title);

  if(l_rci)
  {
    *range_from = (long)MIN(argv[0].int_ret, argv[1].int_ret);
    *range_to   = (long)MAX(argv[1].int_ret, argv[0].int_ret);
    *density_factor = (gdouble)(argv[2].flt_ret);
    *density_grow = (gboolean)(argv[3].int_ret);

    if ((*density_factor > 1.0) && (ainfo_ptr->run_mode != GIMP_RUN_NONINTERACTIVE))
    {
      l_rc = p_density_confirm_dialog(ainfo_ptr
                                        , *range_from
                                        , *range_to
                                        , *density_factor
                                        , *density_grow
                                        );
       return l_rc;
    }
    return TRUE;
  }

  return FALSE;

}       /* end p_density_dialog */


/* ============================================================================
 * gap_base_density
 * ============================================================================
 */
gint32
gap_base_density(GimpRunMode run_mode, gint32 image_id
           , long range_from, long range_to
           , gdouble density_factor, gboolean density_grow)
{
  int l_rci;
  GapAnimInfo *ainfo_ptr;

  long           l_from, l_to;
  gdouble        l_density_factor;
  gboolean       l_density_grow;
  gboolean       l_rc;

  l_rci = -1;
  l_rc = FALSE;
  ainfo_ptr = gap_lib_alloc_ainfo(image_id, run_mode);
  if(ainfo_ptr != NULL)
  {
    if (0 == gap_lib_dir_ainfo(ainfo_ptr))
    {
      if(run_mode == GIMP_RUN_INTERACTIVE)
      {
         if(0 != gap_lib_chk_framechange(ainfo_ptr)) { l_density_factor = -1.0; }
         else
         {
           if(*ainfo_ptr->extension == '\0' && ainfo_ptr->frame_cnt == 0)
           {
             /* density was called on a frame without extension and without framenumer in its name
              * (typical for new created images named like 'Untitled' (or 'Unbenannt' for german GUI or .. in other languages)
              */
               gap_arr_msg_win(ainfo_ptr->run_mode,
                       _("Operation cancelled.\n"
                         "GAP video plug-ins only work with filenames\n"
                         "that end in numbers like _000001.xcf.\n"
                         "==> Rename your image, then try again."));
               return -1;
           }
           l_rc = p_density_dialog(ainfo_ptr, &l_from, &l_to, &l_density_factor, &l_density_grow);
         }

         if((0 != gap_lib_chk_framechange(ainfo_ptr)) || (!l_rc))
         {
            l_density_factor = -1.0;
         }

      }
      else
      {
        l_from = range_from;
        l_to   = range_to;
        l_density_factor = density_factor;
        l_density_grow = density_grow;
      }

      if(l_density_factor > 1.0)
      {
         /* change density by duplicating or deleting frames (on disk) */
         l_rci = p_density(ainfo_ptr, l_from, l_to, l_density_factor, l_density_grow);
      }

    }
    gap_lib_free_ainfo(&ainfo_ptr);
  }

  return(l_rci);

}       /* end gap_base_density */


/* ============================================================================
 * p_dup_dialog
 *
 * ============================================================================
 */
gint32
p_dup_dialog(GapAnimInfo *ainfo_ptr, long *range_from, long *range_to)
{
  static GapArrArg  argv[4];
  gchar            *l_title;

  l_title = g_strdup_printf (_("Duplicate Frames (%ld/%ld)")
                             , ainfo_ptr->curr_frame_nr
                             , ainfo_ptr->frame_cnt);

  gap_arr_arg_init(&argv[0], GAP_ARR_WGT_INT_PAIR);
  argv[0].label_txt = _("From Frame:");
  argv[0].constraint = TRUE;
  argv[0].int_min   = (gint)ainfo_ptr->first_frame_nr;
  argv[0].int_max   = (gint)ainfo_ptr->last_frame_nr;
  argv[0].int_ret   = (gint)ainfo_ptr->curr_frame_nr;
  argv[0].help_txt  = _("Source range starts at this framenumber");

  gap_arr_arg_init(&argv[1], GAP_ARR_WGT_INT_PAIR);
  argv[1].label_txt = _("To Frame:");
  argv[1].constraint = TRUE;
  argv[1].int_min   = (gint)ainfo_ptr->first_frame_nr;
  argv[1].int_max   = (gint)ainfo_ptr->last_frame_nr;
  argv[1].int_ret   = (gint)ainfo_ptr->curr_frame_nr;
  argv[1].help_txt  = _("Source range ends at this framenumber");

  gap_arr_arg_init(&argv[2], GAP_ARR_WGT_INT_PAIR);
  argv[2].label_txt = _("N times:");
  argv[2].constraint = FALSE;
  argv[2].int_min   = 1;
  argv[2].int_max   = 99;
  argv[2].int_ret   = 1;
  argv[2].umin      = 1;
  argv[2].umax      = 9999;
  argv[2].help_txt  = _("Copy selected range n-times (you may type in values > 99)");

  gap_arr_arg_init(&argv[3], GAP_ARR_WGT_HELP_BUTTON);
  argv[3].help_id = GAP_HELP_ID_DUPLICATE;

  if(TRUE == gap_arr_ok_cancel_dialog(l_title, _("Make Duplicates of Frame Range"),  4, argv))
  {
    g_free (l_title);
    *range_from = (long)(argv[0].int_ret);
    *range_to   = (long)(argv[1].int_ret);
       return (int)(argv[2].int_ret);
  }
  else
  {
    g_free (l_title);
    return -1;
  }


}       /* end p_dup_dialog */


/* ============================================================================
 * gap_base_dup
 * ============================================================================
 */
gint32
gap_base_dup(GimpRunMode run_mode, gint32 image_id, int nr,
            long range_from, long range_to)
{
  int rc;
  GapAnimInfo *ainfo_ptr;

  long           l_cnt, l_from, l_to;

  rc = -1;
  ainfo_ptr = gap_lib_alloc_ainfo(image_id, run_mode);
  if(ainfo_ptr != NULL)
  {
    if (0 == gap_lib_dir_ainfo(ainfo_ptr))
    {
      if(run_mode == GIMP_RUN_INTERACTIVE)
      {
         if(0 != gap_lib_chk_framechange(ainfo_ptr)) { l_cnt = -1; }
         else
         {
           if(*ainfo_ptr->extension == '\0' && ainfo_ptr->frame_cnt == 0)
           {
             /* duplicate was called on a frame without extension and without framenumer in its name
              * (typical for new created images named like 'Untitled' (or 'Unbenannt' for german GUI or .. in other languages)
              */
               gap_arr_msg_win(ainfo_ptr->run_mode,
                       _("Operation cancelled.\n"
                         "GAP video plug-ins only work with filenames\n"
                         "that end in numbers like _000001.xcf.\n"
                         "==> Rename your image, then try again."));
               return -1;
           }
           l_cnt = p_dup_dialog(ainfo_ptr, &l_from, &l_to);
         }

         if((0 != gap_lib_chk_framechange(ainfo_ptr)) || (l_cnt < 1))
         {
            l_cnt = -1;
         }

      }
      else
      {
        l_cnt  = nr;
        l_from = range_from;
        l_to   = range_to;
      }

      if(l_cnt > 0)
      {
         /* make l_cnt duplicate frames (on disk) */
         rc = p_dup(ainfo_ptr, l_cnt, l_from, l_to);
      }


    }
    gap_lib_free_ainfo(&ainfo_ptr);
  }

  return(rc);

}       /* end gap_base_dup */


/* ============================================================================
 * gap_base_exchg
 * ============================================================================
 */

gint32
gap_base_exchg(GimpRunMode run_mode, gint32 image_id, int nr)
{
  int rc;
  GapAnimInfo *ainfo_ptr;

  long           l_dest;
  long           l_initial;
  gchar         *l_title;
  gchar         *l_tooltip;

  rc = -1;
  l_initial = 1;
  ainfo_ptr = gap_lib_alloc_ainfo(image_id, run_mode);
  if(ainfo_ptr != NULL)
  {
    if (0 == gap_lib_dir_ainfo(ainfo_ptr))
    {
      if(0 != gap_lib_chk_framerange(ainfo_ptr))   return -1;

      if(run_mode == GIMP_RUN_INTERACTIVE)
      {
         if(ainfo_ptr->curr_frame_nr < ainfo_ptr->last_frame_nr)
         {
           l_initial = ainfo_ptr->curr_frame_nr + 1;
         }
         else
         {
           l_initial = ainfo_ptr->last_frame_nr;
         }
         l_title = g_strdup_printf (_("Exchange Current Frame (%ld)")
                                    , ainfo_ptr->curr_frame_nr);

         l_tooltip = g_strdup_printf(_("Exchange the current frame %d "
                                      "with the frame that has the number entered here")
                    , (int)ainfo_ptr->curr_frame_nr );
         l_dest = gap_arr_slider_dialog(l_title,
                                  _("Exchange with Frame"),
                                  _("Number:")
                                  , l_tooltip
                                  , ainfo_ptr->first_frame_nr
                                  , ainfo_ptr->last_frame_nr
                                  , l_initial
                                  , TRUE
                                  , GAP_HELP_ID_EXCHANGE);
         g_free (l_tooltip);
         g_free (l_title);

         if(0 != gap_lib_chk_framechange(ainfo_ptr))
         {
            l_dest = -1;
         }
      }
      else
      {
        l_dest = nr;
      }

      if((l_dest >= ainfo_ptr->first_frame_nr ) && (l_dest <= ainfo_ptr->last_frame_nr ))
      {
         /* excange current frames with destination frame (on disk) */
         rc = p_exchg(ainfo_ptr, l_dest);
      }


    }
    gap_lib_free_ainfo(&ainfo_ptr);
  }

  return(rc);
}       /* end gap_base_exchg */

/* ============================================================================
 * p_shift_dialog
 *
 * ============================================================================
 */
gint32
p_shift_dialog(GapAnimInfo *ainfo_ptr, long *range_from, long *range_to)
{
  static GapArrArg  argv[4];
  gchar            *l_title;

  l_title = g_strdup_printf (_("Frame Sequence Shift (%ld/%ld)")
                             , ainfo_ptr->curr_frame_nr
                             , ainfo_ptr->frame_cnt);

  gap_arr_arg_init(&argv[0], GAP_ARR_WGT_INT_PAIR);
  argv[0].label_txt = _("From Frame:");
  argv[0].constraint = TRUE;
  argv[0].int_min   = (gint)ainfo_ptr->first_frame_nr;
  argv[0].int_max   = (gint)ainfo_ptr->last_frame_nr;
  argv[0].int_ret   = (gint)ainfo_ptr->curr_frame_nr;
  argv[0].help_txt  = _("Affected range starts at this framenumber");

  gap_arr_arg_init(&argv[1], GAP_ARR_WGT_INT_PAIR);
  argv[1].label_txt = _("To Frame:");
  argv[1].constraint = TRUE;
  argv[1].int_min   = (gint)ainfo_ptr->first_frame_nr;
  argv[1].int_max   = (gint)ainfo_ptr->last_frame_nr;
  argv[1].int_ret   = (gint)ainfo_ptr->last_frame_nr;
  argv[1].help_txt  = _("Affected range ends at this framenumber");

  gap_arr_arg_init(&argv[2], GAP_ARR_WGT_INT_PAIR);
  argv[2].label_txt = _("N-Shift:");
  argv[2].constraint = TRUE;
  argv[2].int_min   = -1 * (gint)ainfo_ptr->last_frame_nr;
  argv[2].int_max   = (gint)ainfo_ptr->last_frame_nr;
  argv[2].int_ret   = 1;
  argv[2].help_txt  = _("Renumber the affected frame sequence (numbers are shifted in circle by N steps)");

  gap_arr_arg_init(&argv[3], GAP_ARR_WGT_HELP_BUTTON);
  argv[3].help_id = GAP_HELP_ID_SHIFT;

  if(TRUE == gap_arr_ok_cancel_dialog(l_title, _("Frame Sequence Shift"),  4, argv))
  {
    g_free (l_title);
    *range_from = (long)(argv[0].int_ret);
    *range_to   = (long)(argv[1].int_ret);
    return (int)(argv[2].int_ret);
  }
  else
  {
    g_free (l_title);
    return 0;
  }


} /* end p_shift_dialog */


/* ============================================================================
 * p_reverse_dialog
 * returns range_to - range_from
 * ============================================================================
 */
gint32
p_reverse_dialog(GapAnimInfo *ainfo_ptr, long *range_from, long *range_to)
{
  static GapArrArg  argv[3];
  gchar            *l_title;

  l_title = g_strdup_printf (_("Frame Sequence reverse (%ld/%ld)")
           , ainfo_ptr->curr_frame_nr
           , ainfo_ptr->frame_cnt);

  gap_arr_arg_init(&argv[0], GAP_ARR_WGT_INT_PAIR);
  argv[0].label_txt = _("From Frame:");
  argv[0].constraint = TRUE;
  argv[0].int_min   = (gint)ainfo_ptr->first_frame_nr;
  argv[0].int_max   = (gint)ainfo_ptr->last_frame_nr;
  argv[0].int_ret   = (gint)ainfo_ptr->curr_frame_nr;
  argv[0].help_txt  = _("Affected range starts at this framenumber");

  gap_arr_arg_init(&argv[1], GAP_ARR_WGT_INT_PAIR);
  argv[1].label_txt = _("To Frame:");
  argv[1].constraint = TRUE;
  argv[1].int_min   = (gint)ainfo_ptr->first_frame_nr;
  argv[1].int_max   = (gint)ainfo_ptr->last_frame_nr;
  argv[1].int_ret   = (gint)ainfo_ptr->last_frame_nr;
  argv[1].help_txt  = _("Affected range ends at this framenumber");

  gap_arr_arg_init(&argv[2], GAP_ARR_WGT_HELP_BUTTON);
  argv[2].help_id = GAP_HELP_ID_REVERSE;

  if(TRUE == gap_arr_ok_cancel_dialog(l_title, _("Frame Sequence Reverse"),  3, argv))
  {
    g_free (l_title);
    *range_from = (long)(argv[0].int_ret);
    *range_to   = (long)(argv[1].int_ret);
    return (int)((long)(argv[1].int_ret) - (long)(argv[0].int_ret));
  }
  else
  {
    g_free (l_title);
    return 0;
  }


} /* end p_reverse_dialog */


/* ============================================================================
 * gap_base_shift
 * ============================================================================
 */
gint32
gap_base_shift(GimpRunMode run_mode, gint32 image_id, int nr,
            long range_from, long range_to)
{
  int rc;
  GapAnimInfo *ainfo_ptr;

  long           l_cnt, l_from, l_to;

  rc = -1;
  ainfo_ptr = gap_lib_alloc_ainfo(image_id, run_mode);
  if(ainfo_ptr != NULL)
  {
    if (0 == gap_lib_dir_ainfo(ainfo_ptr))
    {
      if(run_mode == GIMP_RUN_INTERACTIVE)
      {
         l_cnt = 1;
         if(0 != gap_lib_chk_framechange(ainfo_ptr)) { l_cnt = 0; }
         else { l_cnt = p_shift_dialog(ainfo_ptr, &l_from, &l_to); }

         if((0 != gap_lib_chk_framechange(ainfo_ptr)) || (l_cnt == 0))
         {
            l_cnt = 0;
         }

      }
      else
      {
        l_cnt  = nr;
        l_from = range_from;
        l_to   = range_to;
      }

      if(l_cnt != 0)
      {
         /* shift framesquence by l_cnt frames
          * (rename all frames in the given range on disk)
          */
         rc = p_shift(ainfo_ptr, l_cnt, l_from, l_to);
      }


    }
    gap_lib_free_ainfo(&ainfo_ptr);
  }

  return(rc);


} /* end gap_base_shift */

/* ============================================================================
 * gap_base_reverse
 * ============================================================================
 */
gint32
gap_base_reverse(GimpRunMode run_mode, gint32 image_id,
            long range_from, long range_to)
{
  int rc;
  GapAnimInfo *ainfo_ptr;

  long           l_cnt, l_from, l_to;

  rc = -1;
  ainfo_ptr = gap_lib_alloc_ainfo(image_id, run_mode);
  if(ainfo_ptr != NULL)
  {
    if (0 == gap_lib_dir_ainfo(ainfo_ptr))
    {
      if(run_mode == GIMP_RUN_INTERACTIVE)
      {
         l_cnt = 1;
         if(0 != gap_lib_chk_framechange(ainfo_ptr)) { l_cnt = 0; }
         else { l_cnt = p_reverse_dialog(ainfo_ptr, &l_from, &l_to); }
         /* note: 'p_reverse_dialog' returns 'to' minus 'from' */

         if((0 != gap_lib_chk_framechange(ainfo_ptr)) || (l_cnt == 0))
         {
            l_cnt = 0;
         }

      }
      else
      {
        l_from = range_from;
        l_to   = range_to;
        l_cnt  = l_to - l_from;
      }

      if(l_cnt != 0)
      {
         /* reverse framesquence
          * (renames all frames in the given range on disk)
          */
         rc = p_reverse(ainfo_ptr, l_from, l_to);
      }


    }
    gap_lib_free_ainfo(&ainfo_ptr);
  }

  return(rc);

} /* end gap_base_reverse */

/* --------------------------------
 * p_renumber_dialog
 * --------------------------------
 */
static int
p_renumber_dialog(GapAnimInfo *ainfo_ptr, long *start_frame_nr, long *digits)
{
  static GapArrArg  argv[3];
  gchar            *l_title;

  l_title = g_strdup_printf (_("Renumber Frames (%ld)")
                             , ainfo_ptr->frame_cnt);

  gap_arr_arg_init(&argv[0], GAP_ARR_WGT_INT_PAIR);
  argv[0].label_txt = _("First Frame Number:");
  argv[0].constraint = TRUE;
  argv[0].int_min   = (gint)0;
  argv[0].int_max   = (gint)99999999;
  argv[0].int_ret   = (gint)1;
  argv[0].help_txt  = _("New framenumber for the first frame");

  gap_arr_arg_init(&argv[1], GAP_ARR_WGT_INT_PAIR);
  argv[1].label_txt = _("Digits:");
  argv[1].constraint = TRUE;
  argv[1].int_min   = 1;
  argv[1].int_max   = GAP_LIB_MAX_DIGITS;
  argv[1].int_ret   = GAP_LIB_DEFAULT_DIGITS;
  argv[1].help_txt  = _("How many digits to use for the framenumber in the filename");

  gap_arr_arg_init(&argv[2], GAP_ARR_WGT_HELP_BUTTON);
  argv[2].help_id = GAP_HELP_ID_RENUMBER;


  if(TRUE == gap_arr_ok_cancel_dialog(l_title, _("Renumber Frames"),  3, argv))
  {
    g_free (l_title);
    *start_frame_nr = (long)(argv[0].int_ret);
    *digits   = (long)(argv[1].int_ret);
    return (0);
  }
  else
  {
    g_free (l_title);
    return -1;
  }

}       /* end p_renumber_dialog */


/* ------------------------
 * gap_lib_rename_frame_digits
 * ------------------------
 * rename framename. the source framename number part must match with from_nr
 * the resulting name has a number part with to_nr, filled up with leading zeroes
 * to the specified number of digits.
 * if the current frame is renumbered, we also set the image filename,
 * and keep track of the new current number
 */
int
gap_lib_rename_frame_digits(GapAnimInfo *ainfo_ptr, long from_nr, long to_nr, long from_digits, long to_digits)
{
   char          *l_from_fname;
   char          *l_to_fname;
   int            l_rc;

   l_from_fname = gap_lib_alloc_fname_fixed_digits(ainfo_ptr->basename, from_nr, ainfo_ptr->extension, from_digits);
   if(l_from_fname == NULL) { return(1); }

   l_to_fname = gap_lib_alloc_fname_fixed_digits(ainfo_ptr->basename, to_nr, ainfo_ptr->extension, to_digits);
   if(l_to_fname == NULL) { g_free(l_from_fname); return(1); }

   if(gap_debug) printf("DEBUG gap_lib_rename_frame_digits: %s ..to.. %s\n", l_from_fname, l_to_fname);
   l_rc = g_rename(l_from_fname, l_to_fname);

   gap_thumb_file_rename_thumbnail(l_from_fname, l_to_fname);


   if (from_nr == ainfo_ptr->curr_frame_nr)
   {
      ainfo_ptr->curr_frame_nr = to_nr;
      gimp_image_set_filename(ainfo_ptr->image_id, l_to_fname);
   }

   g_free(l_from_fname);
   g_free(l_to_fname);

   return(l_rc);

}    /* end gap_lib_rename_frame_digits */


/* ----------------------------------
 * p_renumber_frames
 * ----------------------------------
 * rename all frames with framenumbers starting at start_frame_nr
 * using frame numbers with at least n digits (leading zeroes)
 * and make sure that resulting framenumbers are continous
 *  example1:  digits == 6, start_frame_nr == 2
 *
 *     Old filenames               New Filenames
 *   -----------------------------------------------
 *     frame_7.xcf                 frame_000002.xcf
 *     frame_8.xcf                 frame_000003.xcf
 *     frame_14.xcf                frame_000004.xcf
 *     frame_16.xcf                frame_000005.xcf
 *
 *  example2:  digits == 4, start_frame_nr == 8
 *
 *     Old filenames               New Filenames
 *   -----------------------------------------------
 *     frame_7.xcf                 frame_0008.xcf
 *     frame_8.xcf                 frame_0009.xcf
 *     frame_14.xcf                frame_0010.xcf
 *     frame_16.xcf                frame_0011.xcf
 *
 * the 2nd example is processed in 2 loops, where the 1.st loop
 * steps down through all frames and makes continous numbers  0017, 0016, 0015, 0014.
 * the 2nd (up stepping) pass makes the final numbers.
 *
 *  example3:  digits == 4, start_frame_nr == 8
 *
 *     Old filenames               New Filenames
 *   -----------------------------------------------
 *     frame_2.xcf                 frame_0008.xcf
 *     frame_3.xcf                 frame_0009.xcf
 *     frame_4.xcf                 frame_0010.xcf
 *     frame_5.xcf                 frame_0011.xcf
 *
 * the 3nd example does not need a 2nd pass, because the 1.st downstepping loop
 * can make the requested numbers.
 */
static gint32
p_renumber_frames(GapAnimInfo *ainfo_ptr, long start_frame_nr, long digits)
{
  long l_from;
  long l_to;
  long l_has_digits;
  gboolean two_pass;


  if(ainfo_ptr->run_mode == GIMP_RUN_INTERACTIVE)
  {
    gimp_progress_init(_("Renumber Frames"));
  }
  two_pass = FALSE;
  if(start_frame_nr > ainfo_ptr->first_frame_nr)
  {
    long l_cnt;

    l_from = ainfo_ptr->last_frame_nr;
    l_to = ainfo_ptr->last_frame_nr + (start_frame_nr - ainfo_ptr->first_frame_nr);

    l_cnt=0;
    while (l_cnt < ainfo_ptr->frame_cnt)
    {

      if (gap_debug) printf("p_renumber_frames: DOWNSTEP l_cnt:%d l_from:%d l_to:%d\n", (int)l_cnt, (int)l_to, (int)l_from);
      if( gap_lib_exists_frame_nr(ainfo_ptr, l_from, &l_has_digits) )
      {
        if((l_from != l_to)
        || (l_has_digits != digits))
        {
          if (0 != gap_lib_rename_frame_digits(ainfo_ptr, l_from, l_to, l_has_digits, digits))
          {
            return -1;
          }
        }
        l_to--;
        l_cnt++;
      }
      /* advance l_from to the previous available frame number 
       * (normally to l_from -= 1; sometimes to lower number when frames are missing) 
       */
      l_from = gap_lib_get_next_available_frame_number(l_from, -1
               , ainfo_ptr->basename, ainfo_ptr->extension, NULL);

      if(ainfo_ptr->run_mode == GIMP_RUN_INTERACTIVE)
      {
        gimp_progress_update( (gdouble)(l_cnt)
                            / (gdouble)(ainfo_ptr->frame_cnt) );
      }
    }
    if(start_frame_nr !=  (l_to +1))
    {
      two_pass = TRUE;
      if(ainfo_ptr->run_mode == GIMP_RUN_INTERACTIVE)
      {
        gimp_progress_init(_("Renumber Frames 2nd Pass"));
      }
    }
  }

  if((start_frame_nr <= ainfo_ptr->first_frame_nr)
  || (two_pass))
  {
    l_from = ainfo_ptr->first_frame_nr;
    l_to = start_frame_nr;
    while (l_to < start_frame_nr + ainfo_ptr->frame_cnt)
    {

      if (gap_debug) printf("p_renumber_frames: UPSTEP l_from:%d l_to:%d\n", (int)l_to, (int)l_from);

      if( gap_lib_exists_frame_nr(ainfo_ptr, l_from, &l_has_digits) )
      {
        if((l_from != l_to)
        || (l_has_digits != digits))
        {
          if(0 != gap_lib_rename_frame_digits(ainfo_ptr, l_from, l_to, l_has_digits, digits))
          {
            return -1;
          }
        }
        l_to++;
      }
      l_from++;

      if(ainfo_ptr->run_mode == GIMP_RUN_INTERACTIVE)
      {
        gimp_progress_update( (gdouble)(l_from - ainfo_ptr->first_frame_nr)
                            / (gdouble)(1+ (ainfo_ptr->last_frame_nr - ainfo_ptr->first_frame_nr)) );
      }
    }
  }

  return 0; /* OK */
}  /* end p_renumber_frames */


/* ============================================================================
 * gap_base_renumber
 * ============================================================================
 */
gint32
gap_base_renumber(GimpRunMode run_mode, gint32 image_id,
            long start_frame_nr, long digits)
{
  gint32 rc;
  GapAnimInfo *ainfo_ptr;

  long           l_cnt, l_start_frame_nr, l_digits;


  rc = -1;
  l_cnt = 0;
  ainfo_ptr = gap_lib_alloc_ainfo(image_id, run_mode);
  if(ainfo_ptr != NULL)
  {
    if (0 == gap_lib_dir_ainfo(ainfo_ptr))
    {
      if(run_mode == GIMP_RUN_INTERACTIVE)
      {
         if(0 != gap_lib_chk_framechange(ainfo_ptr)) { l_cnt = -1; }
         else
         {
           l_cnt = p_renumber_dialog(ainfo_ptr, &l_start_frame_nr, &l_digits);
         }

         if(0 != gap_lib_chk_framechange(ainfo_ptr))
         {
            l_cnt = -1;
         }

      }
      else
      {
        l_start_frame_nr = start_frame_nr;
        l_digits   = digits;
      }

      if(gap_debug) printf("gap_base_renumber: l_cnt:%d l_start_frame_nr:%d l_digits:%d\n", (int)l_cnt, (int)l_start_frame_nr, (int)l_digits);

      if(l_cnt >= 0)
      {
         /* rename all frames (on disk) */
         rc = p_renumber_frames(ainfo_ptr, l_start_frame_nr, l_digits);
         if(rc >= 0)
         {
           rc = image_id;  /* if OK, return current image id */
         }
      }

    }
    gap_lib_free_ainfo(&ainfo_ptr);
  }

  return(rc);
}       /* end gap_base_renumber */



/* --------------------------------
 * p_getbasenameWithoutDirpartPtr
 * --------------------------------
 * returns a pointer to the start of the filename part within basenamePtr.
 * the caller MUST NOT free the resulting pointer !
 */
static char *
p_getbasenameWithoutDirpartPtr(char *basenamePtr)
{
  char             *retPtr;
  char             *ptr;
  
  /* retPtr is set after the last
   * occurance of directory separators (check for both UNIX and Windows separator characters)
   */
  retPtr = basenamePtr;
  ptr = basenamePtr;
  for(ptr = basenamePtr; ptr != NULL; ptr++)
  {
    if (*ptr == '\0')
    {
      break;
    }
    if ((*ptr == ':') || (*ptr == '/') || (*ptr == '\\'))
    {
      retPtr = ptr;
      retPtr++;
    }
  }
  
  return (retPtr);
  
}  /* end p_getbasenameWithoutDirpartPtr */


/* ----------------------------------
 * p_rename_frames
 * ----------------------------------
 * rename all frames within the same directory to newBasenamePtr
 * if the doRename flag is not TRUE just check if any of the new names already exist.
 *
 *     Old filenames               New Filenames
 *   -----------------------------------------------
 *     frame_000002.xcf            newname_000002.xcf
 *     frame_000003.xcf            newname_000003.xcf
 *     frame_000004.xcf            newname_000004.xcf
 *     frame_000005.xcf            newname_000005.xcf
 *
 */
static gint32
p_rename_frames(GapAnimInfo *ainfo_ptr, char *newBasenamePtr, gboolean doRename)
{
  long l_fnr;
  gint l_errcount;


  if(ainfo_ptr->run_mode == GIMP_RUN_INTERACTIVE)
  {
    if (doRename == TRUE)
    {
      gimp_progress_init(_("Rename Frames"));
    }
    else
    {
      gimp_progress_init(_("Check Framenames"));
    }
  }

  l_errcount = 0;
  l_fnr = ainfo_ptr->first_frame_nr;
  while (l_fnr <= ainfo_ptr->last_frame_nr)
  {
    char *l_curr_name;
    
    if (gap_debug)
    {
      printf("p_rename_frames: STEP l_fnr:%d ainfo_ptr->basename:%s\n", (int)l_fnr, ainfo_ptr->basename);
    }

    l_curr_name = gap_lib_alloc_fname(ainfo_ptr->basename, l_fnr, ainfo_ptr->extension);

    /* check if frame file with old name exists
     */
    if( gap_lib_file_exists(l_curr_name) )
    {
      char *l_new_name;
      char *l_new_basename;
      char *l_dir_path;
      char *l_filepartPtr;   /* points into l_dir_path dont g_free this */
      
      l_dir_path = g_strdup(ainfo_ptr->basename);
      l_filepartPtr = p_getbasenameWithoutDirpartPtr(l_dir_path);
      if (l_filepartPtr != NULL)
      {
        *l_filepartPtr = '\0';  /* cut off filename part, if no dir present cut can be at position 0 */
      }
      if (*l_dir_path == '\0')
      {
        l_new_basename = g_strdup(newBasenamePtr);
      }
      else
      {
        /* build basename from dirpath (that already ends up with a separator) and
         * the newBasename (that was entered in the dialog and is already without dirpath)
         */
        l_new_basename = g_strdup_printf("%s%s", l_dir_path, newBasenamePtr);
      }
      
      if(gap_debug)
      {
        printf("l_dir_path:%s\n", l_dir_path);
      }
      
      l_new_name = gap_lib_alloc_fname(l_new_basename, l_fnr, ainfo_ptr->extension);
      if (gap_lib_file_exists(l_new_name))
      {
        l_errcount++;
      }
      else
      {
        if (doRename == TRUE)
        {
           gint l_rc;
           
           l_rc = g_rename(l_curr_name, l_new_name);
	   gap_thumb_file_rename_thumbnail(l_curr_name, l_new_name);
	   
           if (l_fnr == ainfo_ptr->curr_frame_nr)
           {
             gimp_image_set_filename(ainfo_ptr->image_id, l_new_name);
           }
	   
	   if (l_rc != 0)
	   {
	     l_errcount++;
	   }
        }
      }
      g_free(l_new_name);
      g_free(l_new_basename);
      g_free(l_dir_path);
    }
    l_fnr++;
    g_free(l_curr_name);

    if(ainfo_ptr->run_mode == GIMP_RUN_INTERACTIVE)
    {
      gimp_progress_update( (gdouble)(l_fnr - ainfo_ptr->first_frame_nr)
                          / (gdouble)(1+ (ainfo_ptr->last_frame_nr - ainfo_ptr->first_frame_nr)) );
    }
    if (l_errcount > 0)
    {
      break;
    }
  }
  
  if (l_errcount > 0)
  {
    return (-1);
  }
  return (0); /* OK */
  
}  /* end p_rename_frames */





/* --------------------------------
 * p_rename_dialog
 * --------------------------------
 */
static int
p_rename_dialog(GapAnimInfo *ainfo_ptr, char *newFrameName,  gint len_newFrameName) /// long *start_frame_nr, long *digits)
{
#define ENTRY_WIDTH 400
  static GapArrArg  argv[3];
  gchar            *l_title;
  gchar            *l_oldFrameName;
  gboolean          l_rc;


  l_title = g_strdup_printf (_("Rename Frames (%ld)")
                             , ainfo_ptr->frame_cnt);
  l_oldFrameName = g_strdup_printf (_("Old FrameName: %s")
                             , p_getbasenameWithoutDirpartPtr(ainfo_ptr->basename));

  gap_arr_arg_init(&argv[0], GAP_ARR_WGT_LABEL_LEFT);
  argv[0].label_txt = l_oldFrameName;

  gap_arr_arg_init(&argv[1], GAP_ARR_WGT_TEXT);
  argv[1].label_txt = _("New FrameName");
  argv[1].entry_width = ENTRY_WIDTH;
  argv[1].help_txt  = _("New FrameName for all frames (must be entered without number part, extension and directory path)");
  argv[1].text_buf_len = len_newFrameName;
  argv[1].text_buf_ret = newFrameName;


  gap_arr_arg_init(&argv[2], GAP_ARR_WGT_HELP_BUTTON);
  argv[2].help_id = GAP_HELP_ID_RENAME;

  l_rc = gap_arr_ok_cancel_dialog(l_title, _("Rename Frames"),  3, argv);
  g_free (l_title);
  g_free (l_oldFrameName);

  if(TRUE == l_rc)
  {
    return (0);
  }
  else
  {
    return -1;
  }

}       /* end p_rename_dialog */



/* ============================================================================
 * gap_base_rename
 * ============================================================================
 */
gint32
gap_base_rename(GimpRunMode run_mode, gint32 image_id,
            char *newFrameName,  gint len_newFrameName)
{
  gint32 rc;
  GapAnimInfo *ainfo_ptr;
  char        *newBasenamePtr;

  long           l_cnt;

  rc = -1;
  l_cnt = 0;
  ainfo_ptr = gap_lib_alloc_ainfo(image_id, run_mode);
  if(ainfo_ptr != NULL)
  {
    if (0 == gap_lib_dir_ainfo(ainfo_ptr))
    {
      if(run_mode != GIMP_RUN_NONINTERACTIVE)
      {
         if(0 != gap_lib_chk_framechange(ainfo_ptr)) { l_cnt = -1; }
         else
         {
           strncpy(newFrameName, "newname_", len_newFrameName -1);
           l_cnt = p_rename_dialog(ainfo_ptr, newFrameName, len_newFrameName);
         }

         if(0 != gap_lib_chk_framechange(ainfo_ptr))
         {
            l_cnt = -1;
         }

      }
      
      newBasenamePtr = p_getbasenameWithoutDirpartPtr(newFrameName);

      /* check for directory path (that is not supported in this rename implementation) */
      if ((newBasenamePtr != newFrameName) && (newBasenamePtr != NULL))
      {
        gap_arr_msg_win(ainfo_ptr->run_mode,
	         _("Rename Frames cancelled.\n"
                   "new Framename MUST NOT contain directory path."));
        l_cnt = -1;
      }

      /* check for equal names (rename is not required in this case..) */
      if (strcmp(p_getbasenameWithoutDirpartPtr(ainfo_ptr->basename), newBasenamePtr) == 0)
      {
        gap_arr_msg_win(ainfo_ptr->run_mode,
	         _("Rename Frames cancelled.\n"
                   "new Framename is equal to old Framename."));
        l_cnt = -1;
      }

      if(gap_debug) 
      {
        printf("gap_base_rename: l_cnt:%d newFrameName:%s newBasenamePtr:%s\n"
             , (int)l_cnt
             , newFrameName
             , newBasenamePtr
             );
      }

      if(l_cnt >= 0)
      {
         /* check for all frames (on disk) if the newName already exists */
         rc = p_rename_frames(ainfo_ptr, newBasenamePtr, FALSE);
         if(rc < 0)
         {
           gap_arr_msg_win(ainfo_ptr->run_mode,
	         _("Rename Frames cancelled.\n"
                   "one or more new Framename(s) already exits."));
         }
         else
         {
           /* rename all frames (on disk) */
           rc = p_rename_frames(ainfo_ptr, newBasenamePtr, TRUE);
           if(rc < 0)
           {
             gap_arr_msg_win(ainfo_ptr->run_mode,
	         _("Rename Frames failed.\n"
                   "one or more new Framename(s) could not be renamed."));
           }
           else
           {
             rc = image_id;  /* if OK, return current image id */
           }
         }
      }

    }
    gap_lib_free_ainfo(&ainfo_ptr);
  }

  return(rc);
}       /* end gap_base_rename */

