/* gap_arr_dialog.h
 * 1998.May.23 hof (Wolfgang Hofer)
 *
 * GAP ... Gimp Animation Plugins (Standard array dialog)
 *
 * - gap_arr_ok_cancel_dialog   Dialog Window with one or more rows
 *                    each row can contain one of the following GAP widgets:
 *                       - float pair widget
 *                         (horizontal slidebar combined with a float input field)
 *                       - int pair widget
 *                         (horizontal slidebar combined with a int input field)
 *                       - Toggle Button widget
 *                       - Textentry widget
 *                       - Float entry widget
 *                       - Int entry widget
 * - gap_arr_slider_dialog
 *                         simplified call of p_pair_array_dialog,
 *                         using an array with one GAP_ARR_WGT_INT_PAIR.
 * - gap_arr_buttons_dialog
 *
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
 * gimp    2.1.0a;  2004/09/25  hof: gap_arr_create_vindex_permission
 * gimp    1.3.20a; 2003/09/29  hof: gap_arr_overwrite_file_dialog
 * gimp    1.3.16b; 2003/07/04  hof: new gap_arr_confirm_dialog
 * gimp    1.3.14a; 2003/05/15  hof: new GAP_ARR_WGT_FONTSEL
 * gimp    1.3.12a; 2003/05/01  hof: merge into CVS-gimp-gap project
 * gimp    1.3.11a; 2003/01/18  hof: merged in changes of the gap_vid_enc project
 *                                   - added GAP_ARR_WGT_OPT_ENTRY (entry comined with Optionmenu) and GAP_ARR_WGT_DEFAULT_BUTTON
 * gimp    1.3.4a;  2002/03/12  hof: ported to gtk+-2.0.0
 * gimp    1.1.17b; 2000/01/26  hof: 
 * version 0.96.03; 1998/08/15  hof: p_arr_gtk_init 
 * version 0.96.00; 1998/07/09  hof: 1.st release 
 *                                   (re-implementation of gap_sld_dialog.c)
 */

#ifndef _ARR_DIALOG_H
#define _ARR_DIALOG_H

/* GIMP includes */
#include "gtk/gtk.h"
#include "libgimp/gimp.h"
#include "libgimp/gimpui.h"

typedef enum
{
   GAP_ARR_WGT_LABEL        
  ,GAP_ARR_WGT_TEXT       
  ,GAP_ARR_WGT_INT        
  ,GAP_ARR_WGT_FLT        
  ,GAP_ARR_WGT_TOGGLE     
  ,GAP_ARR_WGT_RADIO      
  ,GAP_ARR_WGT_OPTIONMENU      
  ,GAP_ARR_WGT_FLT_PAIR   
  ,GAP_ARR_WGT_INT_PAIR   
  ,GAP_ARR_WGT_ACT_BUTTON 
  ,GAP_ARR_WGT_FILESEL
  ,GAP_ARR_WGT_LABEL_LEFT
  ,GAP_ARR_WGT_LABEL_RIGHT
  ,GAP_ARR_WGT_OPT_ENTRY
  ,GAP_ARR_WGT_DEFAULT_BUTTON
  ,GAP_ARR_WGT_FONTSEL       
  ,GAP_ARR_WGT_HELP_BUTTON
} GapArrWidget;

typedef int (*t_action_func) ( gpointer action_data);
/*
 * - If one of the Args has set 'has_default' to TRUE
 *   the action Area will contain an additional Button 'Default'
 *
 */

typedef struct {
  GapArrWidget widget_type;

  /* common fields for all widget types */
  const char    *label_txt;
  const char    *help_txt;  /* help for tooltips */
  const char    *help_id;   /* help_id for the help page that should be displayed via help button */
  GimpHelpFunc   help_func;
  gint     entry_width;  /* for all Widgets with  an entry */
  gint     scale_width;  /* for the Widgets with a scale */
  gint     constraint;   /* TRUE: check for min/max values */
  gint     has_default;  /* TRUE: default value available */
  
  /* flt_ fileds are used for GAP_ARR_WGT_FLT and GAP_ARR_WGT_FLT_PAIR */
  gint     flt_digits;    /* digits behind comma */
  gdouble  flt_min;
  gdouble  flt_max;
  gdouble  flt_step;
  gdouble  flt_default;
  gdouble  flt_ret;
  
  /* int_ fileds are used for GAP_ARR_WGT_INT and GAP_ARR_WGT_INT_PAIR GAP_ARR_WGT_TOGGLE */
  gint     int_min;
  gint     int_max;
  gint     int_step;
  gint     int_default;
  gint     int_ret;
  gint     int_ret_lim;  /* for private (arr_dialog.c) use only */

  /* unconstraint lower /upper limit for GAP_ARR_WGT_FLT_PAIR and GAP_ARR_WGT_INT_PAIR */
  gfloat   umin;
  gfloat   umax;
  gfloat   pagestep;


  /* togg_ field are used for GAP_ARR_WGT_TOGGLE */
  char    *togg_label;    /* extra label attached right to toggle button */
   
  /* radio_ fileds are used for GAP_ARR_WGT_RADIO and GAP_ARR_WGT_OPTIONMENU */
  gint     radio_argc;
  gint     radio_default;
  gint     radio_ret;
  char   **radio_argv;
  char   **radio_help_argv;
  
  /* text_ fileds are used for GAP_ARR_WGT_TEXT */
  gint     text_buf_len;         /* common length for init, default and ret text_buffers */
  char    *text_buf_default;
  char    *text_buf_ret;
  const gchar  *text_fontsel; /* for private (arr_dialog.c) use only */
  GtkWidget  *text_filesel; /* for private (arr_dialog.c) use only */
  GtkWidget  *text_entry;   /* for private (arr_dialog.c) use only */
  GtkWidget  *check_button;   /* for private (arr_dialog.c) use only */
  GtkWidget  *combo;          /* for private (arr_dialog.c) use only */
  GtkObject  *adjustment;     /* for private (arr_dialog.c) use only */
  gpointer    radiogroup;     /* for private (arr_dialog.c) use only */

  /* action_ fileds are used for GAP_ARR_WGT_ACT_BUTTON */
  t_action_func action_functon;  
  gpointer      action_data;  

  
  /* flag is FALSE while the dialog is built
   * and goes to TRUE if all widgets are there and ready for user interaction
   * (used in some callbacks to prevent too to early fire)
   */
  gboolean  widget_locked;

} GapArrArg;


typedef struct {
  char      *but_txt;
  gint       but_val;
} GapArrButtonArg;

void     gap_arr_arg_init  (GapArrArg *arr_ptr,
                          gint       widget_type);
 
gint     gap_arr_ok_cancel_dialog  (const char     *title_txt,
                          const char     *frame_txt,
                          int       argc,
                          GapArrArg argv[]);

long     gap_arr_slider_dialog(const char *title_txt,
                         const char *frame_txt,
                         const char *label_txt,
                         const char *tooltip_txt,
                         long min, long max, long curr, long constraint,
                         const char *help_id);



gint     gap_arr_buttons_dialog (const char *title_txt,
                         const char *frame_txt,
                         int        b_argc,
                         GapArrButtonArg  b_argv[],
                         gint       b_def_val);


gint     gap_arr_std_dialog  (const char     *title_txt,
                          const char     *frame_txt,
                          int       argc,
                          GapArrArg argv[],
                          int       b_argc,
                          GapArrButtonArg b_argv[],
                          gint      b_def_val);

gboolean gap_arr_confirm_dialog(const char *msg_txt, const char *title_txt, const char *frame_txt);

gboolean gap_arr_overwrite_file_dialog(const char *filename);

void gap_arr_msg_win(GimpRunMode run_mode, const char *msg);
void gap_arr_msg_popup(GimpRunMode run_mode, const char *msg);

gboolean gap_arr_create_vindex_permission(const char *videofile
      , const char *vindex_file
      , gint32 seek_status
      );

#endif
