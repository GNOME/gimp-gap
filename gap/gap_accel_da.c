/*  gap_accel_da.c
 *
 *  This module handles GAP acceleration characteristics drawing area.
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
 * version 2.7.0; 2010/02/06  hof: created
 */

#include "config.h"

#include <stdio.h>
#include <string.h>

#include <gtk/gtk.h>
#include <libgimp/gimp.h>
#include <libgimp/gimpui.h>
#include "gap_accel_char.h"
#include "gap_accel_da.h"
#include "gap-intl.h"
#include "gap_lib_common_defs.h"


extern int gap_debug;  /* 1 == print debug infos , 0 dont print debug infos */


#define MIX_CHANNEL(b, a, m)  (((a * m) + (b * (255 - m))) / 255)
#define PREVIEW_BG_GRAY1 80
#define PREVIEW_BG_GRAY2 180

#define PREVIEW_BG_GRAY1_GDK 0x505050
#define PREVIEW_BG_GRAY2_GDK 0xb4b4b4


#define GRAPH_RADIUS           3
#define GRAPH_MASK  GDK_EXPOSURE_MASK | \
                    GDK_POINTER_MOTION_MASK | \
                    GDK_POINTER_MOTION_HINT_MASK | \
                    GDK_ENTER_NOTIFY_MASK | \
                    GDK_BUTTON_PRESS_MASK | \
                    GDK_BUTTON_RELEASE_MASK | \
                    GDK_BUTTON1_MOTION_MASK


static void    p_accel_value_changed_cb(GtkObject *obj, GapAccelWidget *accel_wgt);
static gfloat  p_calculate_accelCharacteristic_from_Position(GapAccelWidget *accel_wgt);
static gint    p_accel_event_callback(GtkWidget *wgt, GdkEvent *evt,  GapAccelWidget *accel_wgt);
static gint    gap_accel_repaint (GtkWidget *, GdkEvent *,
                                GapAccelWidget *);

/* ------------------------------
 * gap_pview_new
 * ------------------------------
 * pv_check_size is checkboard size in pixels
 * (for transparent pixels)
 */
GapAccelWidget *
gap_accel_new(gint width, gint height, gint32 accelerationCharacteristic)
{
  return (gap_accel_new_with_adj(width, height, accelerationCharacteristic, NULL));
}  /* end gap_accel_new */

/* ------------------------------
 * gap_pview_new
 * ------------------------------
 * pv_check_size is checkboard size in pixels
 * (for transparent pixels)
 */
GapAccelWidget *
gap_accel_new_with_adj(gint width, gint height, gint32 accelerationCharacteristic, GtkObject *adj)
{
  GapAccelWidget *accel_wgt;
 
  accel_wgt = g_malloc0(sizeof(GapAccelWidget));

  accel_wgt->accelerationCharacteristic = accelerationCharacteristic;
  accel_wgt->width = width;
  accel_wgt->height = height;
  accel_wgt->pixWidth = accel_wgt->width + GRAPH_RADIUS * 4;
  accel_wgt->pixHeight = accel_wgt->height+ GRAPH_RADIUS * 4;
  accel_wgt->isButton1Pressed = FALSE;

  if (adj == NULL)
  {
    adj = gtk_adjustment_new ( accelerationCharacteristic
                           , GAP_ACCEL_CHAR_MIN
                           , GAP_ACCEL_CHAR_MAX
                           , 1
                           , 10
                           , 0
                           );
  }
  accel_wgt->adj = adj;

  accel_wgt->da_widget = gtk_drawing_area_new ();
  gtk_widget_set_size_request (accel_wgt->da_widget
                              , accel_wgt->pixWidth
                              , accel_wgt->pixHeight
                              );

  gtk_widget_set_events (accel_wgt->da_widget, GRAPH_MASK);
  gtk_widget_show (accel_wgt->da_widget);

  g_signal_connect (accel_wgt->da_widget, "event",
                    G_CALLBACK (p_accel_event_callback),
                    accel_wgt);

  g_signal_connect (G_OBJECT (adj), "value_changed",
                      G_CALLBACK (p_accel_value_changed_cb),
                      accel_wgt);

  return(accel_wgt);
}  /* end gap_pview_new */


/* ---------------------------------------------
 * p_calculate_accelCharacteristic_from_Position
 * ---------------------------------------------
 */
static gfloat
p_calculate_accelCharacteristic_from_Position(GapAccelWidget *accel_wgt)
{
  int tx, ty;
  gdouble  x, y;
  gfloat   accelValueFlt;

  /* init with current value as default */
  accelValueFlt = (gfloat)accel_wgt->accelerationCharacteristic;
  
  if(accel_wgt->da_widget != NULL)
  {
    if(accel_wgt->da_widget->window != NULL)
    {
        /*  get the pointer position  */
        gdk_window_get_pointer (accel_wgt->da_widget->window, &tx, &ty, NULL);

        x = CLAMP ((tx - GRAPH_RADIUS), 0, accel_wgt->width);
        y = CLAMP ((ty - GRAPH_RADIUS), 0, accel_wgt->height);


        accelValueFlt = y / (MAX(1.0, accel_wgt->height));
        accelValueFlt = (accelValueFlt * 200) - 100;
        if((accelValueFlt < 1.0) && (accelValueFlt > -1.0))
        {
          /* value 0 is replaced by 1 (because 0 turns off acceleration completely
           * and this shall not happen when picking the value)
           */
          accelValueFlt = 1.0;
        }
    }
  }

  
  return (accelValueFlt);
}  /* end p_calculate_accelCharacteristic_from_Position */


/* -------------------------------
 * p_accel_value_changed_cb
 * -------------------------------
 */
static void
p_accel_value_changed_cb(GtkObject *obj, GapAccelWidget *accel_wgt)
{
  gint32 accelerationCharacteristic;

  if(obj)
  {
    accelerationCharacteristic = (GTK_ADJUSTMENT(obj)->value);

    /* refresh the acceleration graph */
    if(accel_wgt == NULL)
    {
      if(gap_debug)
      {
        printf("p_accel_value_changed_cb: accel_wgt is NULL\n");
      }
    }
    else
    {
      gap_accel_render (accel_wgt, accelerationCharacteristic);
    }

  }
}  /* end p_accel_value_changed_cb */

   
    
/* ------------------------------
 * gap_accel_render
 * ------------------------------
 */
void
gap_accel_render (GapAccelWidget *accel_wgt, gint32 accelerationCharacteristic)
{
  accel_wgt->accelerationCharacteristic = accelerationCharacteristic;
  gap_accel_repaint(NULL, NULL, accel_wgt);
}

/* ------------------------------
 * p_accel_event_callback
 * ------------------------------
 */
static gint
p_accel_event_callback(GtkWidget *wgt, GdkEvent *event,
  GapAccelWidget *accel_wgt)
{
  GdkEventButton *bevent;

  switch (event->type)
  {
    case GDK_EXPOSE:
      gap_accel_repaint(wgt, event, accel_wgt);
      break;

    case GDK_BUTTON_PRESS:
      bevent = (GdkEventButton *) event;
      if ((bevent != NULL) && (accel_wgt->adj != NULL))
      {
        switch (bevent->button)
        {
          case 1:
            accel_wgt->isButton1Pressed = TRUE;
            gtk_adjustment_set_value(accel_wgt->adj, p_calculate_accelCharacteristic_from_Position(accel_wgt));
            break;
          case 2:
            gtk_adjustment_set_value(accel_wgt->adj, (gfloat)0.0);
            break;
          case 3:
            gtk_adjustment_set_value(accel_wgt->adj, (gfloat)1.0);
            break;
        }
      }
      break;

    case GDK_BUTTON_RELEASE:
      accel_wgt->isButton1Pressed = FALSE;
      break;

    case GDK_MOTION_NOTIFY:
      if(accel_wgt->isButton1Pressed == TRUE)
      {
        gtk_adjustment_set_value(accel_wgt->adj, p_calculate_accelCharacteristic_from_Position(accel_wgt));
      }
      break;

    default:
      break;
  }

  return FALSE;
}  /* end p_accel_event_callback */


/* ------------------------------
 * gap_accel_repaint
 * ------------------------------
 */
static gint
gap_accel_repaint(GtkWidget *wgt, GdkEvent *evt,
  GapAccelWidget *accel_wgt)
{
  GtkStyle *graph_style;
  gint32    accelerationCharacteristic;
  gint      i;

  if(accel_wgt->da_widget == NULL)
  {
     if(gap_debug)
     {
       printf("gap_accel_repaint: drawing area graph is NULL (render request ignored\n");
     }
     return (FALSE);
  }
  if(accel_wgt->da_widget->window == NULL)
  {
     if(gap_debug)
     {
       printf("gap_accel_repaint: window pointer is NULL (render request ignored\n");
     }
     return (FALSE);
  }

  accelerationCharacteristic = accel_wgt->accelerationCharacteristic;
  
  graph_style = gtk_widget_get_style (accel_wgt->da_widget);
  
  
  /*  Clear the pixmap  */
  gdk_draw_rectangle (accel_wgt->da_widget->window, graph_style->bg_gc[GTK_STATE_NORMAL],
                      TRUE, 0, 0, accel_wgt->pixWidth, accel_wgt->pixHeight);


  /*  Draw the grid lines (or just outline if acceleration is not active */
  for (i = 0; i < 5; i++)
  {
      if ((i == 0) || (i==4) || (accelerationCharacteristic != 0))
      {
        gdouble xf;
        gdouble yf;
        gint    xi;
        gint    yi;
        
        xf = (gdouble)i * ((gdouble)accel_wgt->width / 4.0);
        yf = (gdouble)i * ((gdouble)accel_wgt->height / 4.0);
        xi = xf;
        yi = yf;
        
        gdk_draw_line (accel_wgt->da_widget->window, graph_style->dark_gc[GTK_STATE_NORMAL],
                     GRAPH_RADIUS, yi + GRAPH_RADIUS,
                     accel_wgt->width + GRAPH_RADIUS, yi + GRAPH_RADIUS);
        gdk_draw_line (accel_wgt->da_widget->window, graph_style->dark_gc[GTK_STATE_NORMAL],
                     xi + GRAPH_RADIUS, GRAPH_RADIUS,
                     xi + GRAPH_RADIUS, accel_wgt->height + GRAPH_RADIUS);

      }
  }

 
  /*  Draw the the acceleration curve according to accelerationCharacteristic
   *  when acceleration is active 
   */
  if(accelerationCharacteristic != 0)
  {
       gdouble xFactor;
       gdouble yFactor;
       gdouble yF;
       gint px, py;
       
       px = 0;
       py = 0;
       /*  Draw the active curve  */
       for (i = 0; i < accel_wgt->width; i++)
       {
           gint cx, cy;
         
           xFactor = (gdouble)i / (MAX(1.0, (gdouble)accel_wgt->width));
           yFactor = gap_accelMixFactor(xFactor,  accelerationCharacteristic);
           yF = (gdouble)accel_wgt->height * yFactor;


           cx = i + GRAPH_RADIUS;
           cy = (accel_wgt->height) - yF  + GRAPH_RADIUS;
           
           if(gap_debug)
           {
             printf("point[%03d] x:%04d y:%04d  xf:%f yf:%f\n"
               ,(int)i
               ,(int)cx
               ,(int)cy
               ,(float)xFactor
               ,(float)yFactor
               );
           }
           if(i>0)
           {
              gdk_draw_line (accel_wgt->da_widget->window, graph_style->black_gc,
                     px, py,
                     cx, cy
                     );
           }
           px = cx;
           py = cy;
       }

  }

  return FALSE;

}  /* end gap_accel_repaint */
