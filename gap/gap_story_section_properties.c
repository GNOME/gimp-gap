/*  gap_story_section_properties.c
 *
 *  This module handles GAP storyboard dialog section properties window.
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
 * version 1.3.26a; 2007/10/08  hof: created
 */


#include "config.h"

#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>

#include <glib/gstdio.h>

#include <gtk/gtk.h>
#include <libgimp/gimp.h>
#include <libgimp/gimpui.h>

#include "gap_story_main.h"
#include "gap_story_undo.h"
#include "gap_story_dialog.h"
#include "gap_story_file.h"
#include "gap_story_properties.h"
#include "gap_pview_da.h"
#include "gap_stock.h"
#include "gap_lib.h"
#include "gap_vin.h"
#include "gap_timeconv.h"
#include "gap_thumbnail.h"
#include "gap_fmac_base.h"
#include "gap_story_vthumb.h"


#include "images/gap-stock-pixbufs.h"

#include "gap-intl.h"


#define GAP_STORY_SECTION_PROP_HELP_ID  "plug-in-gap-storyboard-section-prop"

#define GAP_STORY_SECTION_RESPONSE_DELETE  2
#define GAP_STORY_SECTION_RESPONSE_CREATE  3
#define SPW_ENTRY_WIDTH        300

#define SPW_ICON_TYPE_WIDTH     48
#define SPW_ICON_TYPE_HEIGHT    35

  typedef enum
  {
     GAP_SPW_TYPE_MAIN_SECTION
    ,GAP_SPW_TYPE_MASK_SECTION        
    ,GAP_SPW_TYPE_SUB_SECTION        
    ,GAP_SPW_TYPE_UNKNOWN_SECTION        
  } GapStorySpwSectionType;


extern int gap_debug;  /* 1 == print debug infos , 0 dont print debug infos */

static void     p_spw_prop_response(GtkWidget *widget
                       , gint       response_id
                       , GapStbSecpropWidget *spw
                       );

static GapStorySpwSectionType p_get_section_type(GapStbSecpropWidget  *spw);

static void     p_section_name_entry_set_text(GapStbSecpropWidget *spw, const char *text);
static gboolean p_rename_section(GapStbSecpropWidget *spw);
static void     p_render_section_icon(GapStbSecpropWidget  *pv_ptr);

static void     p_switch_to_section(GapStbSecpropWidget *spw, GapStorySection *section);

static void     p_delete_sub_section_cb(GtkWidget *w, GapStbSecpropWidget *spw);
static void     p_create_new_section(GapStbSecpropWidget *spw);
static gboolean p_spw_icontype_preview_events_cb (GtkWidget *widget
                       , GdkEvent  *event
                       , GapStbSecpropWidget *spw);
static void     p_spw_section_name_entry_update_cb(GtkWidget *widget, GapStbSecpropWidget *spw);
static void     p_spw_update_info_labels(GapStbSecpropWidget *spw);
static void     p_update_widget_sensitivity(GapStbSecpropWidget *spw);



static GtkWidget *  p_spw_create_icontype_pv_container(GapStbSecpropWidget *spw);


/* ---------------------------------
 * p_spw_prop_response
 * ---------------------------------
 */
static void
p_spw_prop_response(GtkWidget *widget
                  , gint       response_id
                  , GapStbSecpropWidget *spw
                  )
{
  GtkWidget *dlg;
  GapStbMainGlobalParams  *sgpp;
  gboolean ok_flag;

  sgpp = spw->sgpp;
  switch (response_id)
  {
    case GAP_STORY_SECTION_RESPONSE_CREATE:
      p_create_new_section(spw);
      break;

    case GTK_RESPONSE_OK:
      ok_flag = p_rename_section(spw);
      if (ok_flag != TRUE)
      {
        return;
      }
      /* fall through and close the dialog on OK */
    case GTK_RESPONSE_CANCEL:
    case GAP_STORY_SECTION_RESPONSE_DELETE:
    default:
      gap_pview_reset(spw->typ_icon_pv_ptr);

      dlg = spw->spw_prop_dialog;
      if(dlg)
      {
        spw->spw_prop_dialog = NULL;
        gtk_widget_destroy(dlg);
      }
      break;
  }
}  /* end p_spw_prop_response */



/* ---------------------------------
 * p_get_section_type
 * ---------------------------------
 * returns the section type of the current 
 * section reference spw->section_refptr
 */
static GapStorySpwSectionType
p_get_section_type(GapStbSecpropWidget  *spw)
{
  GapStorySpwSectionType section_type;
  GapStorySection  *main_section;

  section_type = GAP_SPW_TYPE_UNKNOWN_SECTION;
  if (spw->section_refptr != NULL)
  {
    if (spw->stb_refptr != NULL)
    {
      if(spw->section_refptr == spw->stb_refptr->mask_section)
      {
        return (GAP_SPW_TYPE_MASK_SECTION);
      }
      main_section = gap_story_find_main_section(spw->stb_refptr);
      if (spw->section_refptr == main_section)
      {
        return(GAP_SPW_TYPE_MAIN_SECTION);
      }

      return(GAP_SPW_TYPE_SUB_SECTION);
      
    }
  }
  
  
  return (section_type);
  
}  /* end p_get_section_type */


/* ---------------------------------
 * p_section_name_entry_set_text
 * ---------------------------------
 */
static void
p_section_name_entry_set_text(GapStbSecpropWidget *spw, const char *text)
{
  if ((spw == NULL) || (text == NULL))
  {
    return;
  }
  if(spw->spw_section_name_entry == NULL)
  {
    return;
  }

  gtk_entry_set_text(GTK_ENTRY(spw->spw_section_name_entry), text);

}  /* end p_section_name_entry_set_text */


/* ---------------------------------
 * p_rename_section
 * ---------------------------------
 */
static gboolean
p_rename_section(GapStbSecpropWidget *spw)
{
  const guchar *section_name;
  if (spw == NULL) { return (TRUE); }
 
  if(spw->section_refptr == NULL)  { return(TRUE); }
  if(spw->stb_refptr == NULL)      { return(TRUE); }

  section_name = gtk_entry_get_text(GTK_ENTRY(spw->spw_section_name_entry));
  if (section_name == NULL)
  {
      return (TRUE);
  }


  if(spw->section_refptr->section_name != NULL)
  {
    if(strcmp(section_name, spw->section_refptr->section_name) == 0)
    {
      /* the name has not changed */
      return (TRUE);
    }

  
    if(gap_story_find_section_by_name(spw->stb_refptr, section_name) != NULL)
    {
      if(spw->spw_info_text_label != NULL)
      {
          gtk_label_set_text ( GTK_LABEL(spw->spw_info_text_label)
            , _("please enter a unique section name"));
      }
      return (FALSE);
    }
    
    gap_stb_undo_push((GapStbTabWidgets *)spw->tabw, GAP_STB_FEATURE_PROPERTIES_SECTION);
    
    spw->stb_refptr->unsaved_changes = TRUE;
    g_free(spw->section_refptr->section_name);
    spw->section_refptr->section_name = g_strdup(section_name);

    /* refresh the section_combo box to reflect new section_name
     */
    gap_story_dlg_spw_section_refresh(spw, spw->section_refptr);
  }
  
  return (TRUE);
  
}  /* end p_rename_section */

/* ----------------------------------
 * p_render_section_icon
 * ----------------------------------
 * TODO: have 3 different section type icons
 *  MAIN section
 *  Mask section
 *  sub-section
 */
static void
p_render_section_icon(GapStbSecpropWidget  *spw)
{
  GdkPixbuf *pixbuf;
  GapStorySpwSectionType l_section_type;
  
  pixbuf = NULL;
  if(spw == NULL)
  {
    return;
  }
  if((spw->typ_icon_pv_ptr == NULL) 
  || (spw->cliptype_label == NULL)
  || (spw->stb_refptr == NULL)
  || (spw->section_refptr == NULL))
  {
    return;
  }

  l_section_type = p_get_section_type(spw);
  
  switch(l_section_type)
  {
    case GAP_SPW_TYPE_MAIN_SECTION:
      // TODO create main section icon (dont use the blacksecton in this case)
      pixbuf = gdk_pixbuf_new_from_inline (-1, gap_story_icon_section_main, FALSE, NULL);
      gtk_label_set_text ( GTK_LABEL(spw->cliptype_label), _("MAIN Section"));
      break;
    case GAP_SPW_TYPE_MASK_SECTION:
      // TODO create mask section icon (dont use the blacksecton in this case)
      pixbuf = gdk_pixbuf_new_from_inline (-1, gap_story_icon_section_mask, FALSE, NULL);
      gtk_label_set_text ( GTK_LABEL(spw->cliptype_label), _("Mask Section"));
      break;
    case GAP_SPW_TYPE_SUB_SECTION:
      pixbuf = gdk_pixbuf_new_from_inline (-1, gap_story_icon_section, FALSE, NULL);
      gtk_label_set_text ( GTK_LABEL(spw->cliptype_label), _("Sub Section"));
      break;
    default:
      pixbuf = gdk_pixbuf_new_from_inline (-1, gap_story_icon_default, FALSE, NULL);
      gtk_label_set_text ( GTK_LABEL(spw->cliptype_label), _("Unknown Section"));
      break;
  }
  
  if(pixbuf)
  {
    gap_pview_render_from_pixbuf (spw->typ_icon_pv_ptr, pixbuf);
    g_object_unref(pixbuf);
  }
}  /* end p_render_section_icon */


/* ---------------------------------
 * gap_story_spw_switch_to_section
 * ---------------------------------
 * this procedure is typically calld
 * when the section combo box in the main
 * storyboard edit dialog has changed
 * and the section properties shall be updated to refect those change.
 */
void
gap_story_spw_switch_to_section(GapStbSecpropWidget *spw, GapStoryBoard *stb_dst)
{
  if ((spw == NULL) || (stb_dst == NULL))
  {
    return;
  }
  if ((spw->spw_prop_dialog == NULL)
  || (spw->spw_section_name_entry == NULL))
  {
    /* skip because dialog is not open or not initialised */
    return;
  }
  if(stb_dst->active_section == NULL)
  {
    printf("ERRPR: the active_section is NULL!\n");
    return;
  }

  spw->stb_refptr = stb_dst;
  spw->section_refptr = stb_dst->active_section;

  p_section_name_entry_set_text(spw, spw->section_refptr->section_name);

  p_update_widget_sensitivity(spw);
  p_render_section_icon(spw);

  p_update_widget_sensitivity(spw);
  p_spw_update_info_labels(spw);
  
}  /* end gap_story_spw_switch_to_section */


/* ---------------------------------
 * p_switch_to_section
 * ---------------------------------
 * this procedure handles the case when a change of the active_section
 * is requested by the section properties dialog window.
 * (after creating a new subsection, or after delete of the active subsection)
 * This requires refresh of the section Combo box in the 
 * main storyboard edit dialog and refresh of the thumbnails (tabw).
 */
static void
p_switch_to_section(GapStbSecpropWidget *spw, GapStorySection *target_section)
{
  if (spw == NULL)
  {
    return;
  }
  
  if(spw->spw_section_name_entry == NULL)
  {
    /* skip because init phase is in progress */
    return;
  }

  p_section_name_entry_set_text(spw, target_section->section_name);

  p_update_widget_sensitivity(spw);
  p_render_section_icon(spw);

  p_update_widget_sensitivity(spw);
  
  /* make the section the active section
   * make the section the active section, refresh the sections combo box
   * (this implicitly triggers thumbnail rendering)
   */
  gap_story_dlg_spw_section_refresh(spw, target_section);
  
  p_spw_update_info_labels(spw);
  
}  /* end p_switch_to_section */


/* ---------------------------------
 * p_delete_sub_section_cb
 * ---------------------------------
 * MAIN and Mask section can not be deleted.
 */
static void
p_delete_sub_section_cb(GtkWidget *w, GapStbSecpropWidget *spw)
{
  gboolean removeOK;
  
  if(spw == NULL) { return; }
  if(spw->stb_refptr == NULL) { return; }

  gap_stb_undo_push((GapStbTabWidgets *)spw->tabw, GAP_STB_FEATURE_DELETE_SECTION);

  /* remove section from the list */
  removeOK = gap_story_remove_section(spw->stb_refptr, spw->section_refptr);
  if (removeOK)
  {
     spw->section_refptr = gap_story_find_main_section(spw->stb_refptr);
     p_switch_to_section(spw, spw->section_refptr);
  }
  else
  {
    if(spw->spw_info_text_label != NULL)
    {
        gtk_label_set_text ( GTK_LABEL(spw->spw_info_text_label)
          , _("Could not delete current subsection"
              " because it is still used as Clip in the MAIN section"));
    }
  }
  
}  /* end p_delete_sub_section_cb */


/* ---------------------------------
 * p_create_new_section
 * ---------------------------------
 */
static void
p_create_new_section(GapStbSecpropWidget *spw)
{
  GapStorySection  *new_section; 
  gchar *new_section_name;
  
  if(spw == NULL) { return; }
  if(spw->stb_refptr == NULL) { return; }

  new_section = NULL;
  new_section_name = gap_story_generate_new_unique_section_name(spw->stb_refptr);
  if(new_section_name != NULL)
  {
    gap_stb_undo_push((GapStbTabWidgets *)spw->tabw, GAP_STB_FEATURE_CREATE_SECTION);
    
    /* create the section and add it to the end of section list in the storyboard */
    new_section =
      gap_story_create_or_find_section_by_name(spw->stb_refptr, new_section_name);

    if(new_section)
    {
      spw->section_refptr = new_section;
      
      p_switch_to_section(spw, new_section);
    }
    
    g_free(new_section_name);
  }

}  /* end p_create_new_section */


/* ---------------------------------
 * p_spw_icontype_preview_events_cb
 * ---------------------------------
 */
static gboolean
p_spw_icontype_preview_events_cb (GtkWidget *widget
                       , GdkEvent  *event
                       , GapStbSecpropWidget *spw)
{
  GdkEventExpose *eevent;
  GapStbMainGlobalParams  *sgpp;

  if ((spw->stb_refptr == NULL))
  {
    /* the widget is not initialized or it is just a dummy, no action needed */
    return FALSE;
  }
  sgpp = spw->sgpp;

  switch (event->type)
  {
    case GDK_BUTTON_PRESS:
      return FALSE;
      break;

    case GDK_EXPOSE:
      if(gap_debug)
      {
        printf("p_spw_icontype_preview_events_cb GDK_EXPOSE widget:%d  da_wgt:%d\n"
                              , (int)widget
                              , (int)spw->typ_icon_pv_ptr->da_widget
                              );
      }
      eevent = (GdkEventExpose *) event;

      if(spw->typ_icon_pv_ptr)
      {
        if(widget == spw->typ_icon_pv_ptr->da_widget)
        {
          p_render_section_icon(spw);
          gdk_flush ();
        }
      }

      break;

    default:
      break;
  }

  return FALSE;
}       /* end  p_spw_icontype_preview_events_cb */


/* -----------------------------------
 * p_spw_section_name_entry_update_cb
 * -----------------------------------
 */
static void
p_spw_section_name_entry_update_cb(GtkWidget *widget, GapStbSecpropWidget *spw)
{
  const guchar *section_name;
  gboolean unique_name_entered;
  GapStorySpwSectionType l_section_type;
  
  
  if(spw == NULL)  { return; }
  if(spw->stb_refptr == NULL)      { return; }
  if(spw->section_refptr == NULL)  { return; }

  section_name = gtk_entry_get_text(GTK_ENTRY(widget));

  l_section_type = p_get_section_type(spw);
//  if(l_section_type == GAP_SPW_TYPE_SUB_SECTION)
//  {
//    if(spw->section_refptr->section_name != NULL)
//    {
//      if(strcmp(spw->section_refptr, section_name) != 0)
//      {
//        if(gap_story_find_section_by_name(spw->stb_refptr, section_name) != NULL)
//        {
//          gtk_label_set_text ( GTK_LABEL(spw->spw_info_text_label)
//            , _("please enter a unique section name"));
//          return;
//
//        }
//      }
//      gtk_label_set_text ( GTK_LABEL(spw->spw_info_text_label)
//        , _("OK"));
//    }
//  }

}  /* end p_spw_section_name_entry_update_cb */


/* ---------------------------------
 * p_spw_update_info_labels
 * ---------------------------------
 */
static void
p_spw_update_info_labels(GapStbSecpropWidget *spw)
{
  char    txt_buf[100];
  gdouble l_speed_fps;
  gint32  l_nframes;

  if(spw == NULL) { return; }
  if(spw->stb_refptr == NULL) { return; }
  if(spw->section_refptr == NULL) { return; }

  l_nframes = gap_story_count_total_frames_in_section(spw->section_refptr);
  
  g_snprintf(txt_buf, sizeof(txt_buf), _("%d (frames)")
            ,(int)l_nframes
            );
  gtk_label_set_text ( GTK_LABEL(spw->dur_frames_label), txt_buf);

  l_speed_fps = GAP_STORY_DEFAULT_FRAMERATE;
  if(spw->stb_refptr)
  {
    if(spw->stb_refptr->master_framerate > 0)
    {
      l_speed_fps = spw->stb_refptr->master_framerate;
    }
  }
  gap_timeconv_framenr_to_timestr( l_nframes
                         , l_speed_fps
                         , txt_buf
                         , sizeof(txt_buf)
                         );
  gtk_label_set_text ( GTK_LABEL(spw->dur_time_label), txt_buf);
}  /* end p_spw_update_info_labels */


/* ---------------------------------
 * p_update_widget_sensitivity
 * ---------------------------------
 */
static void
p_update_widget_sensitivity(GapStbSecpropWidget *spw)
{
  GapStorySpwSectionType l_section_type;
  gboolean l_sensitive;
  
  if (spw == NULL) { return; }
  if ((spw->section_refptr == NULL)
  ||  (spw->stb_refptr == NULL))
  {
    return;
  }
  l_section_type = p_get_section_type(spw);

  l_sensitive = TRUE;
  if(spw->spw_info_text_label != NULL)
  {
    switch(l_section_type)
    {
      case GAP_SPW_TYPE_MAIN_SECTION:
        p_section_name_entry_set_text(spw, _("MAIN"));
        l_sensitive = FALSE;
        gtk_label_set_text ( GTK_LABEL(spw->spw_info_text_label)
          , _("Clips of the MAIN section are rendered in the output video"));
        break;
      case GAP_SPW_TYPE_MASK_SECTION:
        p_section_name_entry_set_text(spw, _("Mask"));
        l_sensitive = FALSE;
        gtk_label_set_text ( GTK_LABEL(spw->spw_info_text_label)
          , _("Clips in the Mask section have global scope in all other sections, "
              " and can be attached as (animated) masks to clips in all other "
              " sections to add transparency."
              " white pixels in the mask keep the full opacity"
              " black pixels makes the pixel fully transparent,"
              " all other colors in the mask result in more or less transparency"
              " depending on their brightness."));
        break;
      case GAP_SPW_TYPE_SUB_SECTION:
        l_sensitive = TRUE;
        gtk_label_set_text ( GTK_LABEL(spw->spw_info_text_label)
          , _("sub sections are some kind of repository."
              " Rendering of clips in sub sections depends on "
              " corresponding references in the MAIN section"
              " via clip type SECTION"));
        break;
      default:
        l_sensitive = TRUE;
        gtk_label_set_text ( GTK_LABEL(spw->spw_info_text_label), " ");
        break;
    }
    gtk_widget_set_sensitive(spw->spw_section_name_entry, l_sensitive);

    if(spw->spw_delete_button != NULL)
    {
      gtk_widget_set_sensitive(spw->spw_delete_button, l_sensitive);
    }
  }
  
}  /* end p_update_widget_sensitivity */


/* ----------------------------------
 * p_spw_create_icontype_pv_container
 * ----------------------------------
 */
static GtkWidget *
p_spw_create_icontype_pv_container(GapStbSecpropWidget *spw)
{
  GtkWidget *alignment;
  GtkWidget *event_box;
  GtkWidget *vbox_icontype;
  gint32     l_check_size;
  
  /* the vbox for displaying the clip  */
  vbox_icontype = gtk_vbox_new (FALSE, 1);

  /* Create an EventBox (container for the preview drawing_area)
   */
  event_box = gtk_event_box_new ();
  gtk_container_add (GTK_CONTAINER (event_box), vbox_icontype);
  gtk_widget_set_events (event_box, GDK_BUTTON_PRESS_MASK);
  g_signal_connect (G_OBJECT (event_box), "button_press_event",
                    G_CALLBACK (p_spw_icontype_preview_events_cb),
                    spw);


  /*  The thumbnail preview  */
  alignment = gtk_alignment_new (0.5, 0.5, 0.0, 0.0);
  gtk_box_pack_start (GTK_BOX (vbox_icontype), alignment, FALSE, FALSE, 1);
  gtk_widget_show (alignment);

  l_check_size = SPW_ICON_TYPE_WIDTH / 16;
  spw->typ_icon_pv_ptr = gap_pview_new( SPW_ICON_TYPE_WIDTH
                            , SPW_ICON_TYPE_HEIGHT
                            , l_check_size
                            , NULL   /* no aspect_frame is used */
                            );

  gtk_widget_set_events (spw->typ_icon_pv_ptr->da_widget, GDK_EXPOSURE_MASK);
  gtk_container_add (GTK_CONTAINER (alignment), spw->typ_icon_pv_ptr->da_widget);
  gtk_widget_show (spw->typ_icon_pv_ptr->da_widget);
  g_signal_connect (G_OBJECT (spw->typ_icon_pv_ptr->da_widget), "event",
                    G_CALLBACK (p_spw_icontype_preview_events_cb),
                    spw);

  gtk_widget_show (vbox_icontype);
  gtk_widget_show (event_box);

  p_render_section_icon(spw);

  
  return(event_box);
}  /* end p_spw_create_icontype_pv_container */


/* ---------------------------------------
 * gap_story_section_spw_properties_dialog
 * ---------------------------------------
 */
GtkWidget *
gap_story_section_spw_properties_dialog (GapStbSecpropWidget *spw)
{
  GtkWidget *dlg;
  GtkWidget *main_vbox;
  GtkWidget *frame;
  GtkWidget *table;
  GtkWidget *label;
  GtkWidget *button;
  GtkWidget *entry;
  gint       row;
  GapStbTabWidgets *tabw;
  GapStorySpwSectionType l_section_type;

  if(spw == NULL) { return (NULL); }
  if(spw->spw_prop_dialog != NULL) { return(NULL); }   /* is already open */

  tabw = (GapStbTabWidgets *)spw->tabw;
  if(tabw == NULL) { return (NULL); }

  l_section_type = p_get_section_type(spw);
  
  dlg = gimp_dialog_new (_("Section Properties"), "gap_story_section_properties"
                         ,NULL, 0
                         ,gimp_standard_help_func, GAP_STORY_SECTION_PROP_HELP_ID

                         ,GTK_STOCK_NEW, GAP_STORY_SECTION_RESPONSE_CREATE
                         ,GTK_STOCK_CANCEL,  GTK_RESPONSE_CANCEL
                         ,GTK_STOCK_OK,  GTK_RESPONSE_OK
                         ,NULL);

  spw->spw_prop_dialog = dlg;

  g_signal_connect (G_OBJECT (dlg), "response",
                    G_CALLBACK (p_spw_prop_response),
                    spw);


  main_vbox = gtk_vbox_new (FALSE, 4);
  gtk_container_set_border_width (GTK_CONTAINER (main_vbox), 6);
  gtk_container_add (GTK_CONTAINER (GTK_DIALOG (dlg)->vbox), main_vbox);

  /*  the frame  */
  frame = gimp_frame_new (_("Properties"));
  gtk_box_pack_start (GTK_BOX (main_vbox), frame, FALSE, FALSE, 0);
  gtk_widget_show (frame);

  table = gtk_table_new (17, 4, FALSE);
  spw->master_table = table;
  gtk_container_set_border_width (GTK_CONTAINER (table), 4);
  gtk_table_set_col_spacings (GTK_TABLE (table), 4);
  gtk_table_set_row_spacings (GTK_TABLE (table), 2);
  gtk_container_add (GTK_CONTAINER (frame), table);
  gtk_widget_show (table);

  row = 0;

  /* the Section Type: label */
  label = gtk_label_new (_("Type:"));
  gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
  gtk_table_attach_defaults (GTK_TABLE(table), label, 0, 1, row, row+1);
  gtk_widget_show (label);


  {
    GtkWidget *hbox_type;
    hbox_type = gtk_hbox_new (FALSE, 1);
    gtk_widget_show (hbox_type);
    gtk_table_attach_defaults (GTK_TABLE(table), hbox_type, 1, 2, row, row+1);

    /* the cliptype icon (pview widget) */
    gtk_box_pack_start (GTK_BOX (hbox_type)
         , p_spw_create_icontype_pv_container(spw)
         , FALSE, FALSE, 1);

    /* the cliptype label content */
    label = gtk_label_new ("");
    gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
    gtk_box_pack_start (GTK_BOX (hbox_type), label, FALSE, FALSE, 1);
    gtk_widget_show (label);
    spw->cliptype_label = label;

    /* dummy spacing label */
    label = gtk_label_new ("  ");
    gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
    gtk_box_pack_start (GTK_BOX (hbox_type), label, TRUE, TRUE, 1);
    gtk_widget_show (label);

    /* the duration label */
    label = gtk_label_new (_("Duration:"));
    gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
    gtk_box_pack_start (GTK_BOX (hbox_type), label, FALSE, FALSE, 1);
    gtk_widget_show (label);

    /* the frame duration label */
    label = gtk_label_new ("");
    gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
    gtk_box_pack_start (GTK_BOX (hbox_type), label, FALSE, FALSE, 1);
    gtk_widget_show (label);
    spw->dur_frames_label = label;

  }
  /* the time duration label  */
  label = gtk_label_new ("");
  gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
  gtk_table_attach_defaults (GTK_TABLE(table), label, 2, 3, row, row+1);
  gtk_widget_show (label);
  spw->dur_time_label = label;


  row++;

  /* the section_name label */
  label = gtk_label_new (_("Name:"));
  gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
  gtk_table_attach_defaults (GTK_TABLE(table), label, 0, 1, row, row+1);
  gtk_widget_show (label);

  /* the section_name entry */
  entry = gtk_entry_new ();
  spw->spw_section_name_entry = entry;
  gtk_widget_set_size_request(entry, SPW_ENTRY_WIDTH, -1);
  if(spw->section_refptr)
  {
    p_section_name_entry_set_text(spw, spw->section_refptr->section_name);
  }
  gtk_table_attach_defaults (GTK_TABLE(table), entry, 1, 2, row, row+1);
  g_signal_connect(G_OBJECT(entry), "changed",
                     G_CALLBACK(p_spw_section_name_entry_update_cb),
                     spw);
  gtk_widget_show (entry);

  /* the delete button  */
  button = gap_gtk_button_new_from_stock_icon (GTK_STOCK_DELETE);
  spw->spw_delete_button = button;
  gimp_help_set_help_data (button,
                             _("Delete storyboard section"),
                             NULL);
  gtk_table_attach_defaults (GTK_TABLE(table), button, 2, 3, row, row+1);
  g_signal_connect (G_OBJECT (button), "clicked",
                    G_CALLBACK (p_delete_sub_section_cb),
                    spw);
  gtk_widget_show (button);

  row++;

  /* the info label */
  label = gtk_label_new (_("Info:"));
  gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
  gtk_table_attach_defaults (GTK_TABLE(table), label, 0, 1, row, row+1);
  gtk_widget_show (label);

  /* the spw_info_text_label */
  label = gtk_label_new (" *** ");
  spw->spw_info_text_label = label;
  gtk_label_set_line_wrap(label, TRUE);
  gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
  gtk_table_attach_defaults (GTK_TABLE(table), label, 1, 2, row, row+1);
  gtk_widget_show (label);

  row++;

  /* the comment label */
  // label = gtk_label_new (_("Comment:"));
  // gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
  // gtk_table_attach_defaults (GTK_TABLE(table), label, 0, 1, row, row+1);
  // gtk_widget_show (label);

  p_update_widget_sensitivity(spw);
  
  /*  Show the main containers  */

  gtk_widget_show (main_vbox);

  return(dlg);
}  /* end gap_story_section_spw_properties_dialog */


/* -----------------------------------
 * gap_story_secprop_properties_dialog
 * -----------------------------------
 */
void
gap_story_secprop_properties_dialog (GapStoryBoard *stb_dst, GapStbTabWidgets *tabw)
{
  GapStbSecpropWidget *spw;

  if(stb_dst == NULL)
  {
    return;
  }

  spw = tabw->spw;
  /* check if already open */
  if (spw != NULL)
  {
      if(spw->spw_prop_dialog)
      {
        /* Properties for the selected element already open
         * bring the window to front
         */
        gtk_window_present(GTK_WINDOW(spw->spw_prop_dialog));
        return ;
      }
      /* we found a dead element (that is already closed)
       * reuse that element to open a new section properties dialog window
       */
  }

  if(spw==NULL)
  {
    spw = g_new(GapStbSecpropWidget ,1);
    tabw->spw = spw;
  }
  spw->spw_prop_dialog = NULL;
  spw->spw_section_name_entry = NULL;
  spw->cliptype_label = NULL;
  spw->spw_info_text_label = NULL;
  spw->spw_delete_button = NULL;
  spw->sgpp = tabw->sgpp;
  spw->tabw = tabw;
  if(stb_dst->active_section == NULL)
  {
    printf("ERRPR: active_section is NULL!\n");
    return;
  }
  spw->stb_refptr = stb_dst;
  spw->section_refptr = stb_dst->active_section;

  spw->spw_prop_dialog = gap_story_section_spw_properties_dialog(spw);
  if(spw->spw_prop_dialog)
  {
    gtk_widget_show(spw->spw_prop_dialog);
    p_spw_update_info_labels(spw);
  }
}  /* end gap_story_secprop_properties_dialog */

/* -------------------------------
 * gap_story_spw_properties_dialog
 * -------------------------------
 */
void
gap_story_spw_properties_dialog (GapStoryBoard *stb, GapStbTabWidgets *tabw)
{
  if((tabw == NULL) || (stb == NULL)) { return; }

  gap_story_secprop_properties_dialog(stb, tabw);
}  /* end gap_story_spw_properties_dialog */
