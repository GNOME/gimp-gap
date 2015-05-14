/*  gap_story_vthumb.h
 *
 *  This module handles GAP storyboard dialog video thumbnail (vthumb)
 *  processing. This includes videofile access via API and/or
 *  via storyboard render processor.
 *  video thumbnails are thumbnails of relevant frames (typically
 *  the first referenced framenumber in a video, section or anim image)
 *
 *  procedures for handling the list of video resources and video thumbnails
 *  are implemented in this module.
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
 * version 1.3.26a; 2007/10/06  hof: created
 */

#include "config.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>

#include <gtk/gtk.h>
#include <libgimp/gimp.h>
#include <libgimp/gimpui.h>
#include <gdk/gdkkeysyms.h>

#include "gap_libgapbase.h"
#include "gap_story_main.h"
#include "gap_story_dialog.h"
#include "gap_story_file.h"
#include "gap_arr_dialog.h"
#include "gap_story_render_processor.h"

#include "gap_story_vthumb.h"

#include "gap-intl.h"

extern int gap_debug;  /* 1 == print debug infos , 0 dont print debug infos */

/* global_stb_video_id is used to generate non-persistent unique video_id's per session */
static gint32 global_stb_video_id = 0;


static void                    p_debug_print_vthumbs_refering_video_id(
                                  FILE *fp
                                  , GapVThumbElem *vthumb_list
                                  , gint32 video_id
                                  );
static gboolean               p_vid_progress_callback(gdouble progress
                                  ,gpointer user_data
                                  );
static GapStoryVTResurceElem * p_new_velem(GapStoryVthumbEnum vt_type);
static GapVThumbElem *         p_new_vthumb(gint32 video_id
                                 ,gint32 framenr
                                 ,guchar *th_data
                                 ,gint32 th_width
                                 ,gint32 th_height
                                 ,gint32 th_bpp
                                 );

/* ---------------------------------------
 * p_debug_print_vthumbs_refering_video_id
 * ---------------------------------------
 * print the list of video elements
 * to stdout (typical used for logging and debug purpose)
 */
static void
p_debug_print_vthumbs_refering_video_id(FILE *fp, GapVThumbElem *vthumb_list, gint32 video_id)
{
  GapVThumbElem *vthumb_elem;

  fprintf(fp, "        vthumbs: [");  
  for(vthumb_elem = vthumb_list; vthumb_elem != NULL; vthumb_elem = vthumb_elem->next)
  {
    if(vthumb_elem->video_id == video_id)
    {
      fprintf(fp, " %06d", (int)vthumb_elem->framenr);
    }
  }
  fprintf(fp, "]\n");  
  
}  /* end p_debug_print_vthumbs_refering_video_id */


/* --------------------------------------
 * gap_story_vthumb_debug_fprint_videolist
 * --------------------------------------
 * print the list of video elements
 * to stdout (typical used for logging and debug purpose)
 */
void
gap_story_vthumb_debug_fprint_videolist(FILE *fp, GapStoryVTResurceElem *video_list, GapVThumbElem *vthumb_list)
{
  GapStoryVTResurceElem *velem;
  gint ii;
  
  fprintf(fp, "gap_story_vthumb_debug_fprint_videolist: START\n");
  ii = 0;
  for(velem=video_list; velem != NULL; velem = velem->next)
  {
    const char *filename;
    
    fprintf(fp, " [%03d] ", (int)ii);
    switch(velem->vt_type)
    {
      case GAP_STB_VLIST_MOVIE:
        fprintf(fp, "GAP_STB_VLIST_MOVIE:");
        break;
      case GAP_STB_VLIST_SECTION:
        fprintf(fp, "GAP_STB_VLIST_SECTION: section_id:%d"
             , (int)velem->section_id
             );
        break;
      case GAP_STB_VLIST_ANIM_IMAGE:
        fprintf(fp, "GAP_STB_VLIST_ANIM_IMAGE:");
        break;
      default:
        fprintf(fp, "** Type Unknown **:");
        break;
    }
    
    if (velem->video_filename != NULL)
    {
      filename = velem->video_filename;
    }
    else
    {
      filename = "(null)";
    }
    
    fprintf(fp, " total_frames:%d version:%d video_id:%d name:%s\n"
      , (int)velem->total_frames
      , (int)velem->version
      , (int)velem->video_id
      , filename
      );
    if (vthumb_list != NULL)
    {
      p_debug_print_vthumbs_refering_video_id(fp, vthumb_list, velem->video_id);
    }
    ii++;
  }
  fprintf(fp, "gap_story_vthumb_debug_fprint_videolist: END\n");
  
}  /* end gap_story_vthumb_debug_fprint_videolist */


/* --------------------------------
 * p_vid_progress_callback
 * --------------------------------
 * return: TRUE: cancel videoapi immediate
 *         FALSE: continue
 */
static gboolean
p_vid_progress_callback(gdouble progress
                       ,gpointer user_data
                       )
{
  GapStbMainGlobalParams *sgpp;

  sgpp = (GapStbMainGlobalParams *)user_data;
  if(sgpp == NULL) { return (TRUE); }
  if(sgpp->progress_bar_sub == NULL) { return (TRUE); }

  gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(sgpp->progress_bar_sub), progress);

  gap_story_vthumb_g_main_context_iteration(sgpp);

  return(sgpp->cancel_video_api);

  /* return (TRUE); */ /* cancel video api if playback was stopped */

}  /* end p_vid_progress_callback */



/* ---------------------------------------
 * gap_story_vthumb_g_main_context_iteration
 * ---------------------------------------
 * refresh widget processing
 * and lock video api during the refresh.
 * (to ensure the current video access is not interrupted
 * by any other video access attempts)
 */
void
gap_story_vthumb_g_main_context_iteration(GapStbMainGlobalParams *sgpp)
{
  gboolean old_gva_lock;
  
  old_gva_lock = sgpp->gva_lock;
  sgpp->gva_lock = TRUE;

  /* g_main_context_iteration makes sure that
   *  gtk does refresh widgets,  and react on events while the videoapi
   *  is busy with searching for the next frame.
   *  But hold gva_lock while processing.
   */
  while(g_main_context_iteration(NULL, FALSE));

  sgpp->gva_lock = old_gva_lock;
  
}  /* end gap_story_vthumb_g_main_context_iteration */



/* --------------------------------
 * gap_story_vthumb_close_videofile
 * --------------------------------
 */
void
gap_story_vthumb_close_videofile(GapStbMainGlobalParams *sgpp)
{
#ifdef GAP_ENABLE_VIDEOAPI_SUPPORT
  if(sgpp->gvahand)
  {
    if(sgpp->gva_videofile) g_free(sgpp->gva_videofile);
    GVA_close(sgpp->gvahand);

    sgpp->gvahand =  NULL;
    sgpp->gva_videofile = NULL;
  }
#endif
}  /* end gap_story_vthumb_close_videofile */


/* --------------------------------
 * gap_story_vthumb_open_videofile
 * --------------------------------
 */
void
gap_story_vthumb_open_videofile(GapStbMainGlobalParams *sgpp
                , const char *filename
                , gint32 seltrack
                , const char *preferred_decoder
                )
{
#ifdef GAP_ENABLE_VIDEOAPI_SUPPORT
  gboolean vindex_permission;
  gboolean l_have_valid_vindex;
  char *vindex_file;
  char *create_vindex_msg;

  if(gap_debug)
  {
    printf("gap_story_vthumb_open_videofile %s\n", filename);
  }

  gap_story_vthumb_close_videofile(sgpp);
  vindex_file = NULL;
  create_vindex_msg = NULL;

  sgpp->gvahand =  GVA_open_read_pref(filename
                                  , seltrack
                                  , 1 /* aud_track */
                                  , preferred_decoder
                                  , FALSE  /* use MMX if available (disable_mmx == FALSE) */
                                  );
  if(sgpp->gvahand)
  {
    sgpp->gva_videofile = g_strdup(filename);
    /* sgpp->gvahand->emulate_seek = TRUE; */
    sgpp->gvahand->do_gimp_progress = FALSE;

    sgpp->gvahand->progress_cb_user_data = sgpp;
    sgpp->gvahand->fptr_progress_callback = p_vid_progress_callback;

    /* set decoder name as progress idle text */
    {
      t_GVA_DecoderElem *dec_elem;

      dec_elem = (t_GVA_DecoderElem *)sgpp->gvahand->dec_elem;
      if(dec_elem->decoder_name)
      {
        create_vindex_msg = g_strdup_printf(_("Creating Index (decoder: %s)"), dec_elem->decoder_name);
        vindex_file = GVA_build_videoindex_filename(sgpp->gva_videofile
                                             ,1  /* track */
                                             ,dec_elem->decoder_name
                                             );
      }
      else
      {
        create_vindex_msg = g_strdup_printf(_("Creating Index"));
      }
    }

    sgpp->cancel_video_api = FALSE;

    l_have_valid_vindex = FALSE;
    vindex_permission = FALSE;

    if(sgpp->gvahand->vindex)
    {
      if(sgpp->gvahand->vindex->total_frames > 0)
      {
        l_have_valid_vindex = TRUE;
      }
    }


    if(l_have_valid_vindex == FALSE)
    {
      t_GVA_SeekSupport seekSupp;
  
      seekSupp = GVA_check_seek_support(sgpp->gvahand);

      /* check for permission to create a videoindex file */
      vindex_permission = gap_arr_create_vindex_permission(sgpp->gva_videofile
             , vindex_file
             , (gint32)seekSupp
             );
    }

    if((l_have_valid_vindex == FALSE)
    && (vindex_permission))
    {
      if(gap_debug)
      {
        printf("STORY: DEBUG: create vidindex start\n");
      }

      if(sgpp->progress_bar_sub)
      {
         gtk_progress_bar_set_text(GTK_PROGRESS_BAR(sgpp->progress_bar_sub), create_vindex_msg);
      }
      sgpp->gvahand->create_vindex = TRUE;
      GVA_count_frames(sgpp->gvahand);

      if(gap_debug)
      {
         printf("STORY: DEBUG: create vidindex done\n");
      }
    }

    if(sgpp->progress_bar_sub)
    {
       gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(sgpp->progress_bar_sub), 0.0);
       if(sgpp->cancel_video_api)
       {
         gtk_progress_bar_set_text(GTK_PROGRESS_BAR(sgpp->progress_bar_sub)
                                  ,_("Canceled"));
       }
       else
       {
         gtk_progress_bar_set_text(GTK_PROGRESS_BAR(sgpp->progress_bar_sub), " ");
       }
    }

    if(vindex_file)
    {
      g_free(vindex_file);
    }

    if(create_vindex_msg)
    {
      g_free(create_vindex_msg);
    }

  }
#endif

}  /* end gap_story_vthumb_open_videofile */





/* --------------------------------
 * p_new_velem
 * --------------------------------
 */
static GapStoryVTResurceElem *
p_new_velem(GapStoryVthumbEnum vt_type)
{
  GapStoryVTResurceElem *velem;
  
  velem = g_new(GapStoryVTResurceElem, 1);
  if(velem == NULL) { return (NULL); }
  
  velem->vt_type = vt_type;
  velem->version = 0;
  velem->section_id = -1;
  velem->video_id = global_stb_video_id++;
  velem->seltrack = 1;
  velem->video_filename = NULL;
  velem->total_frames = 0;
  
  velem->next = NULL;
  return (velem);
}  /* end p_new_velem */

/* --------------------------------
 * p_new_vthumb
 * --------------------------------
 */
GapVThumbElem *
p_new_vthumb(gint32 video_id
      ,gint32 framenr
      ,guchar *th_data
      ,gint32 th_width
      ,gint32 th_height
      ,gint32 th_bpp
      )
{
  GapVThumbElem     *vthumb;

  vthumb = g_new(GapVThumbElem ,1);
  if(vthumb)
  {
    vthumb->video_id = video_id;
    vthumb->th_data = th_data;
    vthumb->th_width = th_width;
    vthumb->th_height = th_height;
    vthumb->th_bpp = th_bpp;
    vthumb->framenr = framenr;
    vthumb->next = NULL;
  }
  return(vthumb);
}  /* end p_new_vthumb */

/* --------------------------------
 * gap_story_vthumb_add_vthumb
 * --------------------------------
 * create a new vthumb element with the specified
 * values and link it at begin of the sgpp->vthumb_list.
 * returns the newly created element.
 */
GapVThumbElem *
gap_story_vthumb_add_vthumb(GapStbMainGlobalParams *sgpp
                              ,gint32 framenr
                              ,guchar *th_data
                              ,gint32 th_width
                              ,gint32 th_height
                              ,gint32 th_bpp
                              ,gint32 video_id
                              )
{
  GapVThumbElem     *vthumb;
  
  vthumb = p_new_vthumb(video_id
      , framenr
      , th_data
      , th_width
      , th_height
      , th_bpp
      );
  if(vthumb)
  {
    /* link the new element as 1st into the list */
    vthumb->next = sgpp->vthumb_list;
    sgpp->vthumb_list = vthumb;
  }
  return(vthumb);
  
}  /* end gap_story_vthumb_add_vthumb */                              

/* --------------------------------
 * gap_story_vthumb_get_velem_movie
 * --------------------------------
 * search the Videofile List
 * for a matching Videofile element
 * IF FOUND: return pointer to that element
 * ELSE:     try to create a Videofile Element and return
 *           pointer to the newly created Element
 *           or NULL if no Element could be created.
 *
 * DO NOT g_free the returned Element !
 * it is just a reference to the original List
 * and should be kept until the program (The Storyboard Plugin) exits.
 */
GapStoryVTResurceElem *
gap_story_vthumb_get_velem_movie(GapStbMainGlobalParams *sgpp
              ,const char *video_filename
              ,gint32 seltrack
              ,const char *preferred_decoder
              )
{
  GapStoryVTResurceElem *velem;

  if(sgpp == NULL) { return (NULL); }

  /* check for known video_filenames */
  for(velem = sgpp->video_list; velem != NULL; velem = (GapStoryVTResurceElem *)velem->next)
  {
    if((strcmp(velem->video_filename, video_filename) == 0)
    && (seltrack == velem->seltrack)
    && (velem->vt_type == GAP_STB_VLIST_MOVIE))
    {
      return(velem);
    }
  }

#ifdef GAP_ENABLE_VIDEOAPI_SUPPORT
  if(!sgpp->auto_vthumb)
  {
    return(NULL);
  }

  if(sgpp->auto_vthumb_refresh_canceled)
  {
    return(NULL);
  }


  gap_story_vthumb_open_videofile(sgpp, video_filename, seltrack, preferred_decoder);
  if(sgpp->gvahand)
  {
    /* video_filename is new, add a new element */
    velem = p_new_velem(GAP_STB_VLIST_MOVIE);
    if(velem == NULL) { return (NULL); }

    velem->seltrack = seltrack;
    velem->video_filename = g_strdup(video_filename);
    velem->total_frames = sgpp->gvahand->total_frames;
    if(!sgpp->gvahand->all_frames_counted)
    {
         /* frames are not counted yet,
          * and the total_frames information is just a guess
          * in this case we assume 10 times more frames
          */
         velem->total_frames *= 10;
    }

    velem->next = sgpp->video_list;
    sgpp->video_list = velem;
  }

#endif
  return(velem);

}  /* end gap_story_vthumb_get_velem_movie */


/* -----------------------------------
 * gap_story_vthumb_get_velem_no_movie
 * -----------------------------------
 * search the Videofile List
 * for a matching Videofile element of types
 *   GAP_STB_VLIST_SECTION and
 *   GAP_STB_VLIST_ANIM_IMAGE
 *
 * Note that velem elements must match in name and version.
 *
 * IF FOUND: return pointer to that element
 * ELSE:     try to create a Videofile Element and return
 *           pointer to the newly created Element
 *           or NULL if no Element could be created.
 *
 * DO NOT g_free the returned Element !
 * it is just a reference to the original List
 * and should be kept until the program (The Storyboard Plugin) exits.
 */
GapStoryVTResurceElem *
gap_story_vthumb_get_velem_no_movie(GapStbMainGlobalParams *sgpp
              ,GapStoryBoard *stb
              ,GapStoryElem *stb_elem
              )
{
  GapStoryVTResurceElem *velem;
  GapStorySection *referenced_section;
  GapStoryVthumbEnum vt_type;
  gint32 section_id;
  gint32 version;
  const char *name;

  if((sgpp == NULL) || (stb_elem == NULL)) { return (NULL); }
  
  referenced_section = NULL;
  section_id = -1;
  switch(stb_elem->record_type)
  {
     case GAP_STBREC_VID_SECTION:
       vt_type = GAP_STB_VLIST_SECTION;
       name = stb_elem->orig_filename;   /* the referenced section_name */
       if (name == NULL)
       {
         /* no sub section_name reference available
          * no processing possible
          */
         return(NULL);
       }
       referenced_section =
         gap_story_find_section_by_name(stb, name);

       if (referenced_section == NULL)
       {
         /* sub section_name reference refers to invalid section
          * no processing possible
          */
         return(NULL);
       }
       version = referenced_section->version;
       section_id = referenced_section->section_id;
       /* check for known section respecting section_id and version
        */
       for(velem = sgpp->video_list; velem != NULL; velem = (GapStoryVTResurceElem *)velem->next)
       {
         if((section_id == velem->section_id)
         && (version == velem->version)
         && (velem->vt_type == GAP_STB_VLIST_SECTION))
         {
           return(velem);
         }
       }
       break;
     case GAP_STBREC_VID_ANIMIMAGE:
       vt_type = GAP_STB_VLIST_ANIM_IMAGE;
       name = stb_elem->orig_filename;  /* name of anim_image file */;
       version = gap_file_get_mtime(name);
       /* check for known anim images respecting filename and mtime version */
       for(velem = sgpp->video_list; velem != NULL; velem = (GapStoryVTResurceElem *)velem->next)
       {
         if((strcmp(velem->video_filename, name) == 0)
         && (version == velem->version)
         && (velem->vt_type == GAP_STB_VLIST_ANIM_IMAGE))
         {
           return(velem);
         }
       }
       break;
     default:
       return (NULL);
       break;
  }
  
  

  if(!sgpp->auto_vthumb)
  {
    return(NULL);
  }


  /* vt_type/name/version combination is new, add a new element */
  velem = p_new_velem(vt_type);
  if(velem == NULL) { return (NULL); }

  velem->version = version;
  velem->video_filename = g_strdup(name);
  velem->section_id = section_id;
  if(stb_elem->record_type == GAP_STBREC_VID_SECTION)
  {
    velem->total_frames = gap_story_count_total_frames_in_section(referenced_section);
  }
  else
  {
    velem->total_frames = stb_elem->nframes;
  }

  velem->next = sgpp->video_list;
  sgpp->video_list = velem;

  return(velem);

}  /* end gap_story_vthumb_get_velem_no_movie */


/* -------------------------------------
 * gap_story_vthumb_create_generic_vthumb
 * -------------------------------------
 * Create a thumbnail for the framenumber in the specified clip (via stb_elem)
 *
 *
 * return thdata pointer or NULL
 * create vthumb via storyboard render processor.
 * typical this is used only for storyboard element types:
 *  (stb_elem->record_type == GAP_STBREC_VID_ANIMIMAGE)
 *  (stb_elem->record_type == GAP_STBREC_VID_SECTION)
 *
 * Note that movie clips are handled separately
 * for performance reasons.
 * Single Images and frame images
 * use persistent thumbnails (open thumbnail standard),
 * and therefore are not present as vthumb at all.
 */
guchar *
gap_story_vthumb_create_generic_vthumb(GapStbMainGlobalParams *sgpp
                   , GapStoryBoard *stb
                   , GapStorySection *section
                   , GapStoryElem *stb_elem
                   , gint32 framenumber
                   , gint32 *th_bpp
                   , gint32 *th_width
                   , gint32 *th_height
                   , gboolean do_scale
                   )
{
  guchar *th_data;
  GapStoryBoard *stb_dup;
  GapStoryRenderVidHandle *stb_comp_vidhand;
  gint32 target_framenumber;
  gint32 l_total_framecount;

  th_data = NULL;
#ifdef GAP_ENABLE_VIDEOAPI_SUPPORT

  if(gap_story_elem_is_video(stb_elem) != TRUE)
  {
    return (NULL);
  }
  
  target_framenumber = 1 + (framenumber - stb_elem->from_frame);

  /* create a duplicate of the storyboard that has
   * just one element (with the specified story_id)
   * in its main section.
   */
  // TODO set vtrack number in the duplicated element to 1 ?
  // maybe not important because rendering of one element with another track number
  // shall give the same result.
  if(stb_elem->track == GAP_STB_MASK_TRACK_NUMBER)
  {
    /* we are rendering a mask_definition clip
     */
    stb_dup = gap_story_duplicate_one_elem(stb, stb->mask_section, stb_elem->story_id);
  }
  else
  {
    /* we are rendering a non-mask clip, therefore include
     * all potential mask referneces
     */
    stb_dup = gap_story_duplicate_one_elem_and_masks(stb, section, stb_elem->story_id);
  }
  
  if (stb_elem->record_type == GAP_STBREC_VID_SECTION)
  {
    /* to render a section clip we must include the refered subsection
     * (this implementation includes all subsections)
     */
    gap_story_copy_sub_sections(stb, stb_dup);
  }
  
  if(gap_debug)
  {
    printf("VTHUMB by RENDER_PROCESSOR START  gap_story_vthumb_create_generic_vthumb\n");
    printf(" framenumber: %d target_framenumber: %d\n"
       ,(int)framenumber
       ,(int)target_framenumber
       );
    printf("The CLIP to be rendered as vthumb:\n");
    gap_story_debug_print_elem(stb_elem);
       
    printf("\nThe final duplicated storyboard: stb_dup\n");
    gap_story_debug_print_list(stb_dup);
    printf("VTHUMB by RENDER_PROCESSOR END\n");
  }
  
  stb_comp_vidhand = gap_story_render_open_vid_handle_from_stb (stb_dup
                                       ,&l_total_framecount
                                       );
  if (stb_comp_vidhand != NULL)
  {
    gint32 vthumb_size;

    vthumb_size = 256;
    if(stb->master_width > stb->master_height)
    {
      *th_width = vthumb_size;
      *th_height = (stb->master_height * vthumb_size) / stb->master_width;
    }
    else
    {
      *th_height = vthumb_size;
      *th_width = (stb->master_width * vthumb_size) / stb->master_height;
    }
    stb_comp_vidhand->master_width = *th_width;
    stb_comp_vidhand->master_height = *th_height;
    *th_bpp = 3;
    th_data = gap_story_render_fetch_composite_vthumb(stb_comp_vidhand
               , target_framenumber
               , *th_width
               , *th_height
               );
    gap_story_render_close_vid_handle(stb_comp_vidhand);
  }
  
#endif

  return (th_data);
}  /* end gap_story_vthumb_create_generic_vthumb */



/* ---------------------------------
 * p_story_vthumb_elem_fetch
 * ---------------------------------
 * search the Video Thumbnail List
 * for a matching Thumbnail element.
 * supports videofile, anim-image and sub-section resources.
 *
 * IF FOUND: return pointer to that element
 * ELSE:     try to create a Thumbnail Element and return
 *           pointer to the newly created Element
 *           or NULL if no Element could be created.
 *
 * DO NOT g_free the returned Element !
 * it is just a reference to the original List
 * and should be kept until the program (The Storyboard Plugin) exits.
 */
static GapVThumbElem *
p_story_vthumb_elem_fetch(GapStbMainGlobalParams *sgpp
              ,GapStoryBoard *stb
              ,GapStoryElem *stb_elem
              ,gint32 framenr
              ,gint32 seltrack
              ,const char *preferred_decoder
              ,gboolean trigger_prefetch_restart_flag
              )
{
  GapStoryVTResurceElem *velem;
  GapVThumbElem     *vthumb;
  guchar *th_data;
  gint32 th_width;
  gint32 th_height;
  gint32 th_bpp;
  const char *video_filename;

  if(sgpp == NULL)
  {
    return (NULL);
  }

  if(trigger_prefetch_restart_flag == TRUE)
  {
    if(sgpp->vthumb_prefetch_in_progress != GAP_VTHUMB_PREFETCH_NOT_ACTIVE)
    {
      /* at this point an implicit cancel of video thumbnail prefetch
       * is detected.
       */
      if(gap_debug)
      {
        printf("p_story_vthumb_elem_fetch TRIGGER condition for vthumb prefetch restart occured\n");
      }
      sgpp->vthumb_prefetch_in_progress = GAP_VTHUMB_PREFETCH_RESTART_REQUEST;
      return (NULL);
    }
  }


  th_data = NULL;
  velem = NULL;
  video_filename = NULL;
  switch(stb_elem->record_type)
  {
     case GAP_STBREC_VID_MOVIE:
       video_filename = stb_elem->orig_filename;
       velem = gap_story_vthumb_get_velem_movie(sgpp
                  ,video_filename
                  ,seltrack
                  ,preferred_decoder
                  );
       break;
     case GAP_STBREC_VID_SECTION:
     case GAP_STBREC_VID_ANIMIMAGE:
       velem = gap_story_vthumb_get_velem_no_movie(sgpp
                  ,stb
                  ,stb_elem
                  );
        break;
     default:
         return (NULL);
         break;
  }


  if(velem == NULL) { return (NULL); }

  /* check for known viedo_thumbnails */
  for(vthumb = sgpp->vthumb_list; vthumb != NULL; vthumb = (GapVThumbElem *)vthumb->next)
  {
    if((velem->video_id == vthumb->video_id)
    && (framenr == vthumb->framenr))
    {
      /* we found the wanted video thumbnail */
      return(vthumb);
    }
  }


  if(!sgpp->auto_vthumb)
  {
    return(NULL);
  }

  if(sgpp->auto_vthumb_refresh_canceled)
  {
    return(NULL);
  }
  
  /* Video thumbnail not known yet,
   * we try to create it now
   */
  switch(velem->vt_type)
  {
    case GAP_STB_VLIST_MOVIE:
      /* Fetch the wanted Frame from the Videofile */
      th_data = gap_story_dlg_fetch_videoframe(sgpp
                      , video_filename
                      , framenr
                      , seltrack
                      , preferred_decoder
                      , 1.5               /* delace */
                      , &th_bpp
                      , &th_width
                      , &th_height
                      , TRUE  /* do scale */
                      );
      break;
    default:
      /* Fetch the wanted Frame by calling the storyboard render processor */
      if(framenr <= velem->total_frames)
      {
        th_data = gap_story_vthumb_create_generic_vthumb(sgpp
                      ,stb
                      ,stb->active_section
                      ,stb_elem
                      ,framenr
                      ,&th_bpp
                      ,&th_width
                      ,&th_height
                      , TRUE  /* do scale */
               );
      }
      break;
    
  }

  if(th_data == NULL ) { return (NULL); }

  /* add new vthumb elem to the global list of vthumb elements */
  vthumb = gap_story_vthumb_add_vthumb(sgpp
                          ,framenr
                          ,th_data
                          ,th_width
                          ,th_height
                          ,th_bpp
                          ,velem->video_id
                          );

  return(vthumb);

}  /* end p_story_vthumb_elem_fetch */


/* ---------------------------------
 * gap_story_vthumb_elem_fetch
 * ---------------------------------
 * search the Video Thumbnail List
 * for a matching Thumbnail element.
 * supports videofile, anim-image and sub-section resources.
 *
 * IF FOUND: return pointer to that element
 * ELSE:     try to create a Thumbnail Element and return
 *           pointer to the newly created Element
 *           or NULL if no Element could be created.
 *
 * DO NOT g_free the returned Element !
 * it is just a reference to the original List
 * and should be kept until the program (The Storyboard Plugin) exits.
 *
 * IMPORTANT: Note that this procedure is intended to use for vthumb prefetch purpose,
 * and shall not be used for fetching vthumbs in other situations,
 * because it does NOT retrigger prefetch restart, and may laed to wrong results and/or crash
 * when called while vthumb prefetch  is in progress.
 */
GapVThumbElem *
gap_story_vthumb_elem_fetch(GapStbMainGlobalParams *sgpp
              ,GapStoryBoard *stb
              ,GapStoryElem *stb_elem
              ,gint32 framenr
              ,gint32 seltrack
              ,const char *preferred_decoder
              )
{
  return(p_story_vthumb_elem_fetch(sgpp
              ,stb
              ,stb_elem
              ,framenr
              ,seltrack
              ,preferred_decoder
              ,FALSE  /* trigger_prefetch_restart_flag */
              ));
}  /* end gap_story_vthumb_elem_fetch */


/* ------------------------------
 * gap_story_vthumb_fetch_thdata
 * ------------------------------
 * RETURN a copy of the video thumbnail data
 *        or NULL if fetch was not successful
 *        the caller is responsible to g_free the returned data
 *        after usage.
 */
guchar *
gap_story_vthumb_fetch_thdata(GapStbMainGlobalParams *sgpp
              ,GapStoryBoard *stb
              ,GapStoryElem *stb_elem
              ,gint32 framenr
              ,gint32 seltrack
              ,const char *preferred_decoder
              , gint32 *th_bpp
              , gint32 *th_width
              , gint32 *th_height
              )
{
  GapVThumbElem *vthumb;
  guchar *th_data;


  th_data = NULL;
  vthumb = p_story_vthumb_elem_fetch(sgpp
              ,stb
              ,stb_elem
              ,framenr
              ,seltrack
              ,preferred_decoder
              ,TRUE  /* trigger_prefetch_restart_flag */
              );
  if(vthumb)
  {
    gint32 th_size;

    th_size = vthumb->th_width * vthumb->th_height * vthumb->th_bpp;
    th_data = g_malloc(th_size);
    if(th_data)
    {
      memcpy(th_data, vthumb->th_data, th_size);
      *th_width = vthumb->th_width;
      *th_height = vthumb->th_height;
      *th_bpp = vthumb->th_bpp;
    }
  }
  return(th_data);
}  /* end gap_story_vthumb_fetch_thdata */


/* --------------------------------------
 * gap_story_vthumb_fetch_thdata_no_store
 * --------------------------------------
 * RETURN a pointer of the video thumbnail data
 *        or thumbnail data read from file (indicated by file_read_flag = TRUE)
 * the caller must g_free the returned data if file_read_flag = TRUE
 * but MUST NOT g_free the returned data if file_read_flag = FALSE
 *
 * the video_id (identifier of the videofile in the video files list)
 * is passed in the output parameter video_id
 *
 * NOTE: this variant of vthumb fetching does NOT store
 * newly created vthumbs in the vthumb list.
 * it is intended for use in properties dialog at split scene
 * feature, where all frames of a (long) viedo clip are shown
 * in sequence and storing all of them would fill up the memory
 * with a lot of frames that are not wanted in the vthumb list.
 * (The vthumb lists main purpose is quick access to start frames
 * of all clips in the storyboard for quick rendering of thumbnails)
 */
guchar *
gap_story_vthumb_fetch_thdata_no_store(GapStbMainGlobalParams *sgpp
              ,GapStoryBoard *stb
              ,GapStoryElem *stb_elem
              ,gint32 framenr
              ,gint32 seltrack
              ,const char *preferred_decoder
              , gint32   *th_bpp
              , gint32   *th_width
              , gint32   *th_height
              , gboolean *file_read_flag
              , gint32   *video_id
              )
{
  GapStoryVTResurceElem *velem;
  GapVThumbElem     *vthumb;
  guchar *th_data;
  const char *video_filename;

  if(sgpp == NULL) { return (NULL); }

  video_filename = stb_elem->orig_filename;
  *video_id = -1;
  *file_read_flag = FALSE;
  velem = gap_story_vthumb_get_velem_movie(sgpp
                  ,video_filename
                  ,seltrack
                  ,preferred_decoder
                  );

  if(velem == NULL) { return (NULL); }

  *video_id = velem->video_id;

  /* check for known viedo_thumbnails */
  for(vthumb = sgpp->vthumb_list; vthumb != NULL; vthumb = (GapVThumbElem *)vthumb->next)
  {
    if((velem->video_id == vthumb->video_id)
    && (framenr == vthumb->framenr))
    {
      /* we found the wanted video thumbnail */
      *th_width = vthumb->th_width;
      *th_height = vthumb->th_height;
      *th_bpp = vthumb->th_bpp;
      return(vthumb->th_data);
    }
  }

  if(!sgpp->auto_vthumb)
  {
    return(NULL);
  }

  if(sgpp->vthumb_prefetch_in_progress != GAP_VTHUMB_PREFETCH_NOT_ACTIVE)
  {
      /* at this point an implicit cancel of video thumbnail prefetch
       * is detected.
       * - one option is to render default icon, and restart the prefetch 
       *   via GAP_VTHUMB_PREFETCH_RESTART_REQUEST
       *   (because the storyboard may have changed since prefetch was started
       *    note that prefetch will be very quick for all clips where vthumb is already present
       *    from the cancelled previous prefetch cycle)
       * - an other (not implemented) option is to cancel prefetch and implicitly turn off auto_vthumb mode
       */
      sgpp->vthumb_prefetch_in_progress = GAP_VTHUMB_PREFETCH_RESTART_REQUEST;
      return (NULL);
  }

  /* Video thumbnail not known yet,
   * we try to create it now
   * (but dont add it tho global vthumb list)
   */

  /* Fetch the wanted Frame from the Videofile */
  th_data = gap_story_dlg_fetch_videoframe(sgpp
                   , video_filename
                   , framenr
                   , seltrack
                   , preferred_decoder
                   , 1.5               /* delace */
                   , th_bpp
                   , th_width
                   , th_height
                   , TRUE              /* do_scale */
                   );

  *file_read_flag = TRUE;
  return(th_data);

}  /* end gap_story_vthumb_fetch_thdata_no_store */


