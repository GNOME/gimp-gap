/*
 * Copyright (C) 1995 Spencer Kimball and Peter Mattis
 *
 *
 * Copyright (C) 1997 Andy Thomas  <alt@picnic.demon.co.uk>
 *               2003 Sven Neumann  <sven@gimp.org>
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
 * gimp    1.3.20d; 2003/10/06  hof: added new icon gap_grab_points
 * gimp    1.3.14c; 2003/06/09  hof: bugfix: gap_stock_init must set initialized = TRUE; to avoid double init problems
 *                                   GAP_STOCK_PLAY_REVERSE, GAP_STOCK_PAUSE
 */
 
#include "config.h"

#include <gtk/gtk.h>

#include "gap_stock.h"
#include "gap-intl.h"

#include "images/gap-stock-pixbufs.h"

extern int gap_debug;  /* 1 == print debug infos , 0 dont print debug infos */

static GtkIconFactory *gap_icon_factory = NULL;

static GtkStockItem gap_stock_items[] =
{
  {  GAP_STOCK_ADD_POINT          , N_("Add Point"),         0, 0, NULL },
  {  GAP_STOCK_ANIM_PREVIEW       , N_("Anim Preview"),      0, 0, NULL },
  {  GAP_STOCK_DELETE_ALL_POINTS  , N_("Delete All Points"), 0, 0, NULL },
  {  GAP_STOCK_DELETE_POINT       , N_("Delete Point"),      0, 0, NULL },
  {  GAP_STOCK_FIRST_POINT        , N_("First Point"),       0, 0, NULL },
  {  GAP_STOCK_GRAB_POINTS        , N_("Grab Path"),         0, 0, NULL },
  {  GAP_STOCK_INSERT_POINT       , N_("Insert Point"),      0, 0, NULL },
  {  GAP_STOCK_LAST_POINT         , N_("Last Point"),        0, 0, NULL },
  {  GAP_STOCK_NEXT_POINT         , N_("Next Point"),        0, 0, NULL },
  {  GAP_STOCK_PAUSE              , NULL,                    0, 0, NULL },
  {  GAP_STOCK_PLAY               , NULL,                    0, 0, NULL },
  {  GAP_STOCK_PLAY_REVERSE       , NULL,                    0, 0, NULL },
  {  GAP_STOCK_PREV_POINT         , N_("Prev Point"),        0, 0, NULL },
  {  GAP_STOCK_RESET_ALL_POINTS   , N_("Reset All Points"),  0, 0, NULL },
  {  GAP_STOCK_RESET_POINT        , N_("Reset Point"),       0, 0, NULL },
  {  GAP_STOCK_ROTATE_FOLLOW      , N_("Rotate Follow"),     0, 0, NULL },
  {  GAP_STOCK_SOURCE_IMAGE       , NULL,                    0, 0, NULL },
  {  GAP_STOCK_STEPMODE           , NULL,                    0, 0, NULL },
  {  GAP_STOCK_UPDATE             , NULL,                    0, 0, NULL },
  {  GAP_STOCK_RANGE_END          , NULL,                    0, 0, NULL },
  {  GAP_STOCK_RANGE_START        , NULL,                    0, 0, NULL },
  {  GAP_STOCK_SET_RANGE_END      , NULL,                    0, 0, NULL },
  {  GAP_STOCK_SET_RANGE_START    , NULL,                    0, 0, NULL },
  {  GAP_STOCK_SPEED              , NULL,                    0, 0, NULL }
};


static void
add_stock_icon (const gchar  *stock_id,
                GtkIconSize   size,
                const guint8 *inline_data)
{
  GtkIconSource *source;
  GtkIconSet    *set;
  GdkPixbuf     *pixbuf;
  
  source = gtk_icon_source_new ();

  gtk_icon_source_set_size (source, size);
  gtk_icon_source_set_size_wildcarded (source, FALSE);

  pixbuf = gdk_pixbuf_new_from_inline (-1, inline_data, FALSE, NULL);

  if(gap_debug)
  {
    printf("add_stock_icon stock_id:%s size:%d inline_data:%ld pixbuf:%ld\n"
       ,stock_id
       ,(int)size
       ,(long)inline_data
       ,(long)pixbuf
       );
  }

  gtk_icon_source_set_pixbuf (source, pixbuf);
  g_object_unref (pixbuf);

  set = gtk_icon_set_new ();

  gtk_icon_set_add_source (set, source);
  gtk_icon_source_free (source);

  gtk_icon_factory_add (gap_icon_factory, stock_id, set);

  gtk_icon_set_unref (set);
}

void
gap_stock_init (void)
{
  static gboolean initialized = FALSE;

  if(gap_debug)
  {
    printf("gap_stock_init START\n");
  }

  if (initialized)
    return;

  gap_icon_factory = gtk_icon_factory_new ();
  
  add_stock_icon (GAP_STOCK_ADD_POINT          , GTK_ICON_SIZE_BUTTON, gap_add_point);
  add_stock_icon (GAP_STOCK_ANIM_PREVIEW       , GTK_ICON_SIZE_BUTTON, gap_anim_preview);
  add_stock_icon (GAP_STOCK_DELETE_ALL_POINTS  , GTK_ICON_SIZE_BUTTON, gap_delete_all_points);
  add_stock_icon (GAP_STOCK_DELETE_POINT       , GTK_ICON_SIZE_BUTTON, gap_delete_point);
  add_stock_icon (GAP_STOCK_FIRST_POINT        , GTK_ICON_SIZE_BUTTON, gap_first_point);
  add_stock_icon (GAP_STOCK_GRAB_POINTS        , GTK_ICON_SIZE_BUTTON, gap_grab_points);
  add_stock_icon (GAP_STOCK_INSERT_POINT       , GTK_ICON_SIZE_BUTTON, gap_insert_point);
  add_stock_icon (GAP_STOCK_LAST_POINT         , GTK_ICON_SIZE_BUTTON, gap_last_point);
  add_stock_icon (GAP_STOCK_NEXT_POINT         , GTK_ICON_SIZE_BUTTON, gap_next_point);
  add_stock_icon (GAP_STOCK_PAUSE              , GTK_ICON_SIZE_BUTTON, gap_pause);
  add_stock_icon (GAP_STOCK_PLAY               , GTK_ICON_SIZE_BUTTON, gap_play);
  add_stock_icon (GAP_STOCK_PLAY_REVERSE       , GTK_ICON_SIZE_BUTTON, gap_play_reverse);
  add_stock_icon (GAP_STOCK_PREV_POINT         , GTK_ICON_SIZE_BUTTON, gap_prev_point);
  add_stock_icon (GAP_STOCK_RESET_ALL_POINTS   , GTK_ICON_SIZE_BUTTON, gap_reset_all_points);
  add_stock_icon (GAP_STOCK_RESET_POINT        , GTK_ICON_SIZE_BUTTON, gap_reset_point);
  add_stock_icon (GAP_STOCK_ROTATE_FOLLOW      , GTK_ICON_SIZE_BUTTON, gap_rotate_follow);
  add_stock_icon (GAP_STOCK_SOURCE_IMAGE       , GTK_ICON_SIZE_BUTTON, gap_source_image);
  add_stock_icon (GAP_STOCK_STEPMODE           , GTK_ICON_SIZE_BUTTON, gap_stepmode);
  add_stock_icon (GAP_STOCK_UPDATE             , GTK_ICON_SIZE_BUTTON, gap_update);

  add_stock_icon ( GAP_STOCK_RANGE_END         , GTK_ICON_SIZE_BUTTON, gap_range_end);
  add_stock_icon ( GAP_STOCK_RANGE_START       , GTK_ICON_SIZE_BUTTON, gap_range_start);
  add_stock_icon ( GAP_STOCK_SET_RANGE_END     , GTK_ICON_SIZE_BUTTON, gap_set_range_end);
  add_stock_icon ( GAP_STOCK_SET_RANGE_START   , GTK_ICON_SIZE_BUTTON, gap_set_range_start);
  add_stock_icon ( GAP_STOCK_SPEED             , GTK_ICON_SIZE_BUTTON, gap_speed);

  gtk_icon_factory_add_default (gap_icon_factory);

  gtk_stock_add_static (gap_stock_items, G_N_ELEMENTS (gap_stock_items));

  initialized = TRUE;

  if(gap_debug)
  {
    printf("gap_stock_init DONE\n");
  }
}



GtkWidget *  
gap_stock_button_new_with_label(const char *stock_id, const char *optional_label)
{
  GtkWidget *button;
  GtkWidget *image;

  button = gtk_button_new ();

  image = gtk_image_new_from_stock (stock_id, GTK_ICON_SIZE_BUTTON);
  gtk_widget_show (image);
  

  if (optional_label != NULL)
  {
    GtkWidget *hbox;
    GtkWidget *label;

    hbox = gtk_hbox_new (FALSE, 2);
    gtk_widget_show (hbox);
  
    gtk_container_set_border_width (GTK_CONTAINER (hbox), 2);
    gtk_box_pack_start (GTK_BOX (hbox), image, FALSE, FALSE, 0);
    
    label = gtk_label_new (optional_label);
    gtk_widget_show (label);

    gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 4);

    gtk_container_add (GTK_CONTAINER (button), hbox);

  }
  else
  {
    gtk_container_add (GTK_CONTAINER (button), image);
  }

  return (button);
}

GtkWidget *  
gap_stock_button_new(const char *stock_id)
{
  return(gap_stock_button_new_with_label(stock_id, NULL));
}

