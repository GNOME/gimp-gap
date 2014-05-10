/* gap_arr_dialog.c
 * 1998.June.29 hof (Wolfgang Hofer)
 *
 * GAP ... Gimp Animation Plugins
 *
 * This is the common GTK Dialog for most of the GAP Functions.
 * (it replaces the older gap_sld_dialog module)
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
 * gimp    2.1.0a;  2005/01/15  hof: replaced global data (global_arrint) by dynamic allocated data
 *                                   to enable 2 or more simultan opened gap_arr_dialog based pop-up dialogs
 * gimp    2.1.0a;  2004/11/11  hof: added GAP_ARR_WGT_HELP_BUTTON
 * gimp    2.1.0a;  2004/11/05  hof: replaced deprecated option_menu by gimp_int_combo_box
 * gimp    2.1.0a;  2004/10/09  hof: bugfix GAP_ARR_WGT_OPT_ENTRY (entry height was set to 0)
 * gimp    2.1.0a;  2004/09/25  hof: gap_arr_create_vindex_permission
 * gimp    1.3.20d; 2003/10/04  hof: bugfix: added missing implementation for GAP_ARR_WGT_RADIO defaults
 * gimp    1.3.20a; 2003/09/29  hof: gap_arr_overwrite_file_dialog
 * gimp    1.3.20a; 2003/09/14  hof: extended function of GAP_ARR_WGT_LABEL GAP_ARR_WGT_LABEL_LEFT GAP_ARR_WGT_LABEL_RIGHT
 *                                   (caller can provide additional Text via text_buf_ret,
 *                                    to create 2 Labels)
 * gimp    1.3.18a; 2003/08/23  hof: gap_arr_slider_dialog increased entry_width from 45 to 80 (to show 6 digits)
 * gimp    1.3.17b; 2003/07/31  hof: message text fixes for translators (# 118392)
 * gimp    1.3.17a; 2003/07/28  hof: gimp_interactive_selection_font was renamed to:  gimp_font_select_new
 * gimp    1.3.16c; 2003/07/12  hof: new gap_arr_confirm_dialog
 * gimp    1.3.15a; 2003/06/21  hof: textspacing
 * gimp    1.3.14b; 2003/06/03  hof: added gap_stock_init
 * gimp    1.3.14a; 2003/05/19  hof: GUI standard (OK ist rightmost button)
 *                                   changed GAP_ARR_WGT_INT, and GAP_ARR_WGT_FLT from entry to spinbutton
 *                                   added GAP_ARR_WGT_FONTSEL
 * gimp    1.3.12a; 2003/05/03  hof: merge into CVS-gimp-gap project, replace gimp_help_init by _gimp_help_init
 * gimp    1.3.11a; 2003/01/18  hof: merged in changes of the gap_vid_enc project
 *                                   - added GAP_ARR_WGT_OPT_ENTRY (entry comined with Optionmenu) and GAP_ARR_WGT_DEFAULT_BUTTON
 * gimp    1.3.4a;  2002/03/12  hof: ported to gtk+-2.0.0
 *                                   radio_create_value (prevent fire callback to early, needed in new gtk)
 *                                   bugfix: optionmenu_create_value active menu entry now can have any position
 *                                   (dont change menuorder any longer, where active item was placed 1st)
 * gimp    1.1.17b; 2000/01/26  hof: bugfix gimp_help_init
 *                                   use gimp_scale_entry_new for GAP_ARR_WGT_FLT_PAIR, GAP_ARR_WGT_INT_PAIR
 * gimp    1.1.17a; 2000/02/20  hof: use gimp_help_set_help_data for tooltips
 * gimp    1.1.13b; 1999/12/04  hof: some cosmetic gtk fixes
 *                                   changed border_width spacing and Buttons in action area
 *                                   to same style as used in dialogs of the gimp 1.1.13 main dialogs
 * gimp    1.1.11b; 1999/11/20  hof: some cosmetic gtk fixes:
 *                                   - allow X-expansion (useful for the scale widgets)
 *                                   - use a hbox on GAP_ARR_WGT_INT_PAIR and GAP_ARR_WGT_FLT_PAIR
 *                                     (reduces the waste of horizontal space
 *                                      when used together with other widget types in the table)
 * gimp    1.1.5.1; 1999/05/08  hof: call fileselect in gtk+1.2 style 
 * version 0.96.03; 1998/08/15  hof: p_arr_gtk_init 
 * version 0.96.01; 1998/07/09  hof: Bugfix: gtk_init should be called only
 *                                           once in a plugin process 
 * version 0.96.00; 1998/07/09  hof: 1.st release 
 *                                   (re-implementation of gap_sld_dialog.c)
 */

#include "config.h"

#include <glib/gstdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>


/* GIMP includes */
#include "gtk/gtk.h"
#include "libgimp/gimp.h"
#include "libgimp/gimpui.h"

/* private includes */
#include "gap_libgapbase.h"
#include "gap_arr_dialog.h"
#include "gap_stock.h"
#include "gap-intl.h"

/* default alignment for labels 0.0 == left, 1.0 == right */
#define LABEL_DEFAULT_ALIGN_X 0.0
#define LABEL_DEFAULT_ALIGN_Y 0.5
#define GAP_ARR_INTERFACE_PTR  "gap_arr_interface_ptr"

typedef void (*t_entry_cb_func) (GtkWidget *widget, GapArrArg *arr_ptr);

typedef struct
{
  GapArrArg *arr_ptr;
  gint       radio_index;
  GtkWidget *radio_button;
  void      *first;
  void      *next;
} t_radio_arg;


typedef struct {
  GtkWidget *dlg;
  gint run;
} t_arr_interface;

typedef struct
{
  GapArrArg *argv;
  gint       argc;
}   t_all_arr_args;


extern      int gap_debug; /* ==0  ... dont print debug infos */

/* Declare local functions.
 */

static void   arr_close_callback        (GtkWidget *widget, gpointer data);
static void   but_array_callback        (GtkWidget *widget, gpointer data);
static void   arr_help_callback         (GtkWidget *widget, GapArrArg *arr_ptr);

static void   entry_create_value        (char *title, GtkTable *table, int row, 
                                         GapArrArg *arr_ptr, t_arr_interface *arrint_ptr,
                                         t_entry_cb_func entry_update_cb, char *init_txt);
static void   label_create_value         (char *title, GtkTable *table, int row,
                                          GapArrArg *arr_ptr, t_arr_interface *arrint_ptr,
                                          gfloat align);
static void   text_entry_update_cb       (GtkWidget *widget, GapArrArg *arr_ptr);
static void   text_create_value          (char *title, GtkTable *table, int row,
                                          GapArrArg *arr_ptr, t_arr_interface *arrint_ptr);
static void   filesel_close_cb           (GtkWidget *widget, GapArrArg *arr_ptr);
static void   filesel_ok_cb              (GtkWidget *widget, GapArrArg *arr_ptr);
static void   filesel_open_cb            (GtkWidget *widget, GapArrArg *arr_ptr);
static void   filesel_create_value       (char *title, GtkTable *table, int row,
                                          GapArrArg *arr_ptr, t_arr_interface *arrint_ptr);

static void   int_create_value           (char *title, GtkTable *table, int row,
                                          GapArrArg *arr_ptr, t_arr_interface *arrint_ptr);
static void   flt_create_value           (char *title, GtkTable *table, int row,
                                          GapArrArg *arr_ptr, t_arr_interface *arrint_ptr);

static void   toggle_update_cb           (GtkWidget *widget, GapArrArg *arr_ptr);
static void   toggle_create_value        (char *title, GtkTable *table, int row, 
                                          GapArrArg *arr_ptr, t_arr_interface *arrint_ptr);

static void   combo_update_cb            (GtkWidget *widget, GapArrArg *arr_ptr);

static void   radio_update_cb            (GtkWidget *widget, t_radio_arg *radio_ptr);
static void   radio_create_value         (char *title, GtkTable *table, int row, 
                                          GapArrArg *arr_ptr, t_arr_interface *arrint_ptr);
static void   combo_create_value         (char *title, GtkTable *table, int row,
                                          GapArrArg *arr_ptr, t_arr_interface *arrint_ptr);

static void   pair_int_create_value     (gchar *title, GtkTable *table, gint row,
                                         GapArrArg *arr_ptr, t_arr_interface *arrint_ptr);
static void   pair_flt_create_value     (gchar *title, GtkTable *table, gint row,
                                         GapArrArg *arr_ptr, t_arr_interface *arrint_ptr);

static void   default_button_cb           (GtkWidget *widget, t_all_arr_args *arr_all);
static void   default_button_create_value (char *title, GtkWidget *hbox, 
                                           GapArrArg *arr_ptr, t_arr_interface *arrint_ptr, 
                                           t_all_arr_args *arr_all);



static void
arr_close_callback (GtkWidget *widget,
                    gpointer   data)
{
  t_arr_interface *arrint_ptr;
  GtkWidget *dlg;
  
  arrint_ptr = NULL;
  dlg = NULL;

  if(widget)
  {
    arrint_ptr = (t_arr_interface *) g_object_get_data (G_OBJECT (widget)
                                                       , GAP_ARR_INTERFACE_PTR
                                                       );
  }

  if(arrint_ptr)
  {
    dlg = arrint_ptr->dlg;
    arrint_ptr->dlg = NULL;
  }

  if(dlg)
  {
    gtk_widget_destroy (GTK_WIDGET (dlg));  /* close & destroy dialog window */
    gtk_main_quit ();
  }
}

static void
arr_help_callback (GtkWidget *widget, GapArrArg *arr_ptr)
{
  t_arr_interface *arrint_ptr;
  
  arrint_ptr = NULL;
  if(widget)
  {
    arrint_ptr = (t_arr_interface *) g_object_get_data (G_OBJECT (widget)
                                                       , GAP_ARR_INTERFACE_PTR
                                                       );
  }
  
  if((arr_ptr)
  && (arrint_ptr))
  {
    if(arr_ptr->help_id)
    {
      if(arr_ptr->help_func)
      {
        /* call user help function */   
         arr_ptr->help_func (arr_ptr->help_id, arrint_ptr->dlg);
      }
      else
      {
        /* call gimp standard help function */
        gimp_standard_help_func(arr_ptr->help_id, arrint_ptr->dlg);
      }
    }
  }
}

static void
but_array_callback (GtkWidget *widget,
                    gpointer   data)
{
  t_arr_interface *arrint_ptr;

  arrint_ptr = (t_arr_interface *) g_object_get_data (G_OBJECT (widget), GAP_ARR_INTERFACE_PTR);
  if(arrint_ptr)
  {
    arrint_ptr->run = *((gint *)data);                  /* set returnvalue according to button */
    arr_close_callback(arrint_ptr->dlg, NULL);
    return;
  }
  else
  {
    arr_close_callback(widget, NULL);
  }
}


static void
entry_create_value(char *title, GtkTable *table, int row,
                   GapArrArg *arr_ptr, t_arr_interface *arrint_ptr,
                   t_entry_cb_func entry_update_cb, char *init_txt)
{
    GtkWidget *entry;
    GtkWidget *label;


    label = gtk_label_new(title);
    gtk_misc_set_alignment(GTK_MISC(label), LABEL_DEFAULT_ALIGN_X, LABEL_DEFAULT_ALIGN_Y);
    gtk_table_attach(table, label, 0, 1, row, row + 1, GTK_FILL, GTK_FILL, 0, 0);
    gtk_widget_show(label);

    entry = gtk_entry_new();
    g_object_set_data (G_OBJECT (entry), GAP_ARR_INTERFACE_PTR, (gpointer)arrint_ptr);
    gtk_widget_set_size_request(entry, arr_ptr->entry_width, -1);
    gtk_entry_set_text(GTK_ENTRY(entry), init_txt);
    gtk_table_attach(GTK_TABLE(table), entry, 1, 2, row, row + 1, GTK_FILL, GTK_FILL | GTK_EXPAND, 4, 0);
    if(arr_ptr->help_txt != NULL)
    { 
       gimp_help_set_help_data(entry, arr_ptr->help_txt,NULL);
    }
    gtk_widget_show(entry);
    g_signal_connect(G_OBJECT(entry), "changed",
                     G_CALLBACK (entry_update_cb),
                     arr_ptr);
    
    arr_ptr->text_entry = entry;
}

/* --------------------------
 * LABEL
 * --------------------------
 */

static void
label_create_value(char *title, GtkTable *table, int row,
                   GapArrArg *arr_ptr, t_arr_interface *arrint_ptr,
                   gfloat align)
{
    GtkWidget *label;
    GtkWidget *hbox;
    gint       from_col;

    from_col = 0;
    if(arr_ptr->text_buf_ret)
    {
      /* the caller provided both label and text
       * in this case 2 Labels are shown
       */
      label = gtk_label_new(title);
      gtk_misc_set_alignment(GTK_MISC(label), LABEL_DEFAULT_ALIGN_X, LABEL_DEFAULT_ALIGN_Y);
      gtk_table_attach(table, label, 0, 1, row, row + 1, GTK_FILL, GTK_FILL, 0, 0);
      gtk_widget_show(label);


      from_col = 1;
      label = gtk_label_new(arr_ptr->text_buf_ret);
    }
    else
    {
      /* the caller provided just the Label
       * in this case we show the label (spread accross all columns)
       */
     from_col = 0;
     label = gtk_label_new(title);
    }
    gtk_misc_set_alignment(GTK_MISC(label), align, LABEL_DEFAULT_ALIGN_Y);
    
    if(align != 0.5)
    {
      hbox = gtk_hbox_new (FALSE, 2);
      gtk_widget_show (hbox);
      gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, TRUE, 0);
      gtk_table_attach(table, hbox, from_col, 3, row, row + 1, GTK_FILL, GTK_FILL, 4, 0);
    }
    else
    {
      gtk_table_attach(table, label, from_col, 3, row, row + 1, GTK_FILL, GTK_FILL, 4, 0);
    }

    gtk_widget_show(label);
}

/* --------------------------
 * DEFAULT_BUTTON
 * --------------------------
 */

static void
default_button_create_value(char *title, GtkWidget *hbox,
                            GapArrArg *arr_ptr, t_arr_interface *arrint_ptr,
                            t_all_arr_args *arr_all)
{
    GtkWidget *button;

    button = gtk_button_new_from_stock (GIMP_STOCK_RESET);    /*button = gtk_button_new_with_label (title);*/
    g_object_set_data (G_OBJECT (button), GAP_ARR_INTERFACE_PTR, (gpointer)arrint_ptr);
    
    gtk_widget_show(button);
    g_signal_connect (G_OBJECT (button), "clicked",
                      G_CALLBACK(default_button_cb),
                      arr_all);
 
    gtk_box_pack_start (GTK_BOX (hbox), button, TRUE, TRUE, 0);

    /* gtk_table_attach(table, button, 0, 3, row, row + 1, GTK_FILL, 0, 0, 0); */
    if(arr_ptr->help_txt != NULL)
    { 
       gimp_help_set_help_data(button, arr_ptr->help_txt,NULL);
    }
}  /* end default_button_create_value */

static void
default_button_cb(GtkWidget *widget,  t_all_arr_args *arr_all)
{
  gint l_idx;
  GapArrArg  *arr_ptr;
  char       *buf;
  char       *fmt;

  if(arr_all == NULL)
  {
    return;
  }
  
  for(l_idx = 0; l_idx < arr_all->argc; l_idx++)
  {
     arr_ptr = &arr_all->argv[l_idx];
     buf = NULL;
 
     if(!arr_ptr->has_default)
     {
       continue;
     }
     
     switch(arr_ptr->widget_type)
     {
        case GAP_ARR_WGT_TEXT:
        case GAP_ARR_WGT_FONTSEL:
        case GAP_ARR_WGT_OPT_ENTRY:
          if((arr_ptr->text_buf_ret != NULL) && (arr_ptr->text_buf_default))
          {
             strncpy(arr_ptr->text_buf_ret, arr_ptr->text_buf_default, arr_ptr->text_buf_len -1);
             buf = g_strdup(arr_ptr->text_buf_default);
          }
          if(arr_ptr->widget_type == GAP_ARR_WGT_OPT_ENTRY)
          {
            arr_ptr->radio_ret = arr_ptr->radio_default;
            gimp_int_combo_box_set_active (GIMP_INT_COMBO_BOX (arr_ptr->combo), arr_ptr->radio_ret);
          }
          break;
        case GAP_ARR_WGT_FLT_PAIR:
        case GAP_ARR_WGT_FLT:  
          arr_ptr->flt_ret = arr_ptr->flt_default;
          fmt = g_strdup_printf("%%.%df", arr_ptr->flt_digits);
          buf = g_strdup_printf(fmt, arr_ptr->flt_ret);
          g_free(fmt);  
          if(arr_ptr->adjustment)
          {
            gtk_adjustment_set_value (GTK_ADJUSTMENT (arr_ptr->adjustment), (gfloat)arr_ptr->flt_default);
          }
          break;
        case GAP_ARR_WGT_INT_PAIR:
        case GAP_ARR_WGT_INT:
          arr_ptr->int_ret = arr_ptr->int_default;
          buf = g_strdup_printf("%d", arr_ptr->int_ret);
          if(arr_ptr->adjustment)
          {
            gtk_adjustment_set_value (GTK_ADJUSTMENT (arr_ptr->adjustment), (gfloat)arr_ptr->int_default);
          }
          break;
        case GAP_ARR_WGT_TOGGLE:
          arr_ptr->int_ret = arr_ptr->int_default;
          gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (arr_ptr->check_button),
                                arr_ptr->int_default);
          break;
        case GAP_ARR_WGT_RADIO:
          arr_ptr->radio_ret = arr_ptr->radio_default;

          {
            t_radio_arg *rgp_list;
            
            for(rgp_list = (t_radio_arg *)arr_ptr->radiogroup
               ;rgp_list != NULL
               ;rgp_list = (t_radio_arg *)rgp_list->next)
            {
              if(rgp_list->radio_index == arr_ptr->radio_ret)
              {
                gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (rgp_list->radio_button) 
                                             , TRUE);
                break;
              }
            }
          }
          break;
        case GAP_ARR_WGT_OPTIONMENU:
         arr_ptr->radio_ret = arr_ptr->radio_default;
         gimp_int_combo_box_set_active (GIMP_INT_COMBO_BOX (arr_ptr->combo), arr_ptr->radio_ret);
         break;
        default :
         break;
     }   /* end switch */

     if(buf != NULL)
     {
       if(arr_ptr->text_entry != NULL)
       {
         gtk_entry_set_text(GTK_ENTRY(arr_ptr->text_entry), buf);
       }
       g_free(buf);
     }
  }      /* end for */
}  /* end default_button_cb */


/* --------------------------
 * FONTSEL
 * --------------------------
 */

static void
p_font_callback   (gchar    *name
                  ,gboolean  dialog_closing
                  ,gpointer  user_data)
{
  GapArrArg *arr_ptr;
  
  if(gap_debug) printf("p_font_callback: %s  closing:%d\n", name, (int)dialog_closing);

  arr_ptr = (GapArrArg *)user_data;
  if(arr_ptr)
  {
    g_snprintf(arr_ptr->text_buf_ret, arr_ptr->text_buf_len, "%s", name);
    gtk_entry_set_text(GTK_ENTRY(arr_ptr->text_entry), arr_ptr->text_buf_ret);
  }
  
  if(dialog_closing)
  {
    arr_ptr->text_fontsel = NULL;
  }
}
                  

static void
fontsel_open_cb(GtkWidget *widget, GapArrArg *arr_ptr)
{
  const gchar *fontsel;

  if(arr_ptr->text_fontsel != NULL) return;  /* fontsel is already open */

  fontsel = gimp_font_select_new (arr_ptr->label_txt       /*    *dialogname */
                                 , arr_ptr->text_buf_ret   /*    *font_name */
                                 , (GimpRunFontCallback) p_font_callback
                                 , arr_ptr                 /* gpointer   data */
                                 );

  arr_ptr->text_fontsel = fontsel;
}


static void
fontsel_create_value(char *title, GtkTable *table, int row, GapArrArg *arr_ptr, t_arr_interface *arrint_ptr)
{
  GtkWidget *button;

  entry_create_value(title, table, row, arr_ptr, arrint_ptr, text_entry_update_cb, arr_ptr->text_buf_ret);
  gtk_widget_set_sensitive(arr_ptr->text_entry ,FALSE);
  arr_ptr->text_fontsel = NULL;
    
  /* Button  to invoke fontbrowser */  
  button = gtk_button_new_with_label ( _("Font Browser"));
  gtk_table_attach( GTK_TABLE(table), button, 2, 3, row, row +1,
                    0, 0, 0, 0 );
  if(arr_ptr->help_txt != NULL)
  { 
       gimp_help_set_help_data(button, arr_ptr->help_txt,NULL);
  }
  gtk_widget_show (button);
  g_signal_connect (G_OBJECT (button), "clicked",
                    G_CALLBACK (fontsel_open_cb),
                    arr_ptr);   
}


/* --------------------------
 * FILESEL
 * --------------------------
 */
static void
filesel_close_cb(GtkWidget *widget, GapArrArg *arr_ptr)
{
  if(arr_ptr->text_filesel == NULL) return;  /* filesel is already closed */

  gtk_widget_destroy(GTK_WIDGET(arr_ptr->text_filesel));
  arr_ptr->text_filesel = NULL;
}
static void
filesel_ok_cb(GtkWidget *widget, GapArrArg *arr_ptr)
{
  const gchar *filename;

  if(arr_ptr->text_filesel == NULL) return;  /* filesel is already closed */

  filename = gtk_file_selection_get_filename (GTK_FILE_SELECTION (arr_ptr->text_filesel));
  strncpy(arr_ptr->text_buf_ret, filename, arr_ptr->text_buf_len -1);

  gtk_entry_set_text(GTK_ENTRY(arr_ptr->text_entry), filename);

  filesel_close_cb(widget, arr_ptr);
}

static void
filesel_open_cb(GtkWidget *widget, GapArrArg *arr_ptr)
{
  GtkWidget *filesel;

  if(arr_ptr->text_filesel != NULL)
  {
    gtk_window_present(GTK_WINDOW(arr_ptr->text_filesel));
    return;  /* filesel is already open */
  }

  filesel = gtk_file_selection_new (arr_ptr->label_txt);
  arr_ptr->text_filesel = filesel;

  gtk_window_set_position (GTK_WINDOW (filesel), GTK_WIN_POS_MOUSE);

  gtk_file_selection_set_filename (GTK_FILE_SELECTION (filesel),
                                   arr_ptr->text_buf_ret);
  gtk_widget_show (filesel);

  g_signal_connect (G_OBJECT (filesel), "destroy",
                    G_CALLBACK (filesel_close_cb),
                    arr_ptr);

  g_signal_connect (G_OBJECT (GTK_FILE_SELECTION (filesel)->ok_button),
                   "clicked",
                    G_CALLBACK (filesel_ok_cb),
                    arr_ptr);
  g_signal_connect (G_OBJECT (GTK_FILE_SELECTION (filesel)->cancel_button),
                   "clicked",
                    G_CALLBACK (filesel_close_cb),
                    arr_ptr);
}


static void
filesel_create_value(char *title, GtkTable *table, int row, 
                     GapArrArg *arr_ptr, t_arr_interface *arrint_ptr)
{
  GtkWidget *button;

  entry_create_value(title, table, row, arr_ptr, arrint_ptr, text_entry_update_cb, arr_ptr->text_buf_ret);
  arr_ptr->text_filesel = NULL;
    
  /* Button  to invoke filebrowser */  
  button = gtk_button_new_with_label ( "..." );
  g_object_set_data (G_OBJECT (button), GAP_ARR_INTERFACE_PTR, (gpointer)arrint_ptr);
  gtk_table_attach( GTK_TABLE(table), button, 2, 3, row, row +1,
                    0, 0, 0, 0 );
  gtk_widget_show (button);
  g_signal_connect (G_OBJECT (button), "clicked",
                    G_CALLBACK (filesel_open_cb),
                    arr_ptr);
    
}

/* --------------------------
 * TEXT
 * --------------------------
 */
static void
text_entry_update_cb(GtkWidget *widget, GapArrArg *arr_ptr)
{
  if((arr_ptr->widget_type != GAP_ARR_WGT_TEXT)
  && (arr_ptr->widget_type != GAP_ARR_WGT_OPT_ENTRY)
  && (arr_ptr->widget_type != GAP_ARR_WGT_FONTSEL)
  && (arr_ptr->widget_type != GAP_ARR_WGT_FILESEL))
  {
    return;
  }
  
  strncpy(arr_ptr->text_buf_ret,
          gtk_entry_get_text(GTK_ENTRY(widget)), 
          arr_ptr->text_buf_len -1);
  arr_ptr->text_buf_ret[arr_ptr->text_buf_len -1] = '\0';
}

static void
text_create_value(char *title, GtkTable *table, int row, GapArrArg *arr_ptr, t_arr_interface *arrint_ptr)
{
    entry_create_value(title, table, row, arr_ptr, arrint_ptr, text_entry_update_cb, arr_ptr->text_buf_ret);
}


/* --------------------------
 * SPIN  (INT and FLT)
 * --------------------------
 */

static void
spin_create_value(char *title, GtkTable *table, int row
                , GapArrArg *arr_ptr, t_arr_interface *arrint_ptr
                , gboolean int_flag
                , gdouble un_min
                , gdouble un_max
                )
{
  GtkWidget *label;
  GtkWidget *spinbutton;
  GtkObject *adj;
  GtkWidget *hbox1;
  GtkWidget *label2;

  gdouble     umin, umax;
  gdouble     sstep;
  gdouble     initial_val;
  gint        l_digits;

  label = gtk_label_new(title);
  gtk_misc_set_alignment(GTK_MISC(label), LABEL_DEFAULT_ALIGN_X, LABEL_DEFAULT_ALIGN_Y);
  gtk_table_attach(table, label, 0, 1, row, row + 1, GTK_FILL, GTK_FILL, 0, 0);
  gtk_widget_show(label);

  if(int_flag)
  {
    sstep = arr_ptr->int_step;
    initial_val = (gdouble)   arr_ptr->int_ret;
    l_digits = 0;
  }
  else
  {
    sstep = arr_ptr->flt_step;
    initial_val = (gdouble)   arr_ptr->flt_ret;
    l_digits = arr_ptr->flt_digits;
  }
  
  if(arr_ptr->constraint)
  {
    umin = un_min;
    umax = un_max;
  }
  else
  {
    umin = arr_ptr->umin;
    umax = arr_ptr->umax;
  }

  hbox1 = gtk_hbox_new (FALSE, 0);
  gtk_widget_show (hbox1);

  spinbutton = gimp_spin_button_new (&adj,  /* return value */
                      initial_val,
                      umin,
                      umax,
                      sstep,
                      (gdouble)   arr_ptr->pagestep,
                      0.0,                 /* page_size */
                      1.0,                 /* climb_rate */
                      l_digits              /* digits */
                      );

  g_object_set_data (G_OBJECT (spinbutton), GAP_ARR_INTERFACE_PTR, (gpointer)arrint_ptr);
 
  gtk_widget_set_size_request(spinbutton, arr_ptr->entry_width, -1);
  gtk_widget_show (spinbutton);

  label2 = gtk_label_new(" ");
  gtk_widget_show(label2);

  gtk_box_pack_start (GTK_BOX (hbox1), spinbutton, FALSE, FALSE, 0);
  gtk_box_pack_start (GTK_BOX (hbox1), label2, TRUE, TRUE, 0);

  gtk_table_attach(GTK_TABLE(table), hbox1, 1, 2, row, row + 1, GTK_FILL, GTK_FILL, 4, 0);
  if(arr_ptr->help_txt != NULL)
  { 
     gimp_help_set_help_data(spinbutton, arr_ptr->help_txt,NULL);
  }
  gtk_widget_show(spinbutton);

  if(int_flag)
  {
    g_signal_connect
        (G_OBJECT (adj), "value_changed",
         G_CALLBACK (gimp_int_adjustment_update),
         &arr_ptr->int_ret);
  }
  else
  {
    g_signal_connect
        (G_OBJECT (adj), "value_changed",
         G_CALLBACK (gimp_double_adjustment_update),
         &arr_ptr->flt_ret);
  }
  
    
  arr_ptr->adjustment = adj;                
}  /* end spin_create_value */


/* --------------------------
 * INT
 * --------------------------
 */
static void
int_create_value(char *title, GtkTable *table, int row, 
                 GapArrArg *arr_ptr, t_arr_interface *arrint_ptr)
{
  spin_create_value(title, table, row, arr_ptr, arrint_ptr
                   , TRUE  /* int_flag */
                   , (gdouble) arr_ptr->int_min
                   , (gdouble) arr_ptr->int_max
                   );
}
/* --------------------------
 * FLOAT
 * --------------------------
 */

static void
flt_create_value(char *title, GtkTable *table, int row, 
                 GapArrArg *arr_ptr, t_arr_interface *arrint_ptr)
{
  spin_create_value(title, table, row, arr_ptr, arrint_ptr
                   , FALSE  /* int_flag */
                   , (gdouble) arr_ptr->flt_min
                   , (gdouble) arr_ptr->flt_max
                   );
}


/* --------------------------
 * TOGGLE
 * --------------------------
 */

static void
toggle_update_cb (GtkWidget *widget, GapArrArg *arr_ptr)
{
  if(arr_ptr->widget_type !=GAP_ARR_WGT_TOGGLE) return;

  if (GTK_TOGGLE_BUTTON (widget)->active)
  {
    arr_ptr->int_ret = 1;
  }
  else
  {
    arr_ptr->int_ret = 0;
  }
}

static void
toggle_create_value(char *title, GtkTable *table, int row, 
                    GapArrArg *arr_ptr, t_arr_interface *arrint_ptr)
{
  GtkWidget *check_button;
  GtkWidget *label;
  char      *l_togg_txt;


  label = gtk_label_new(title);
  gtk_misc_set_alignment(GTK_MISC(label), LABEL_DEFAULT_ALIGN_X, LABEL_DEFAULT_ALIGN_Y);
  gtk_table_attach(table, label, 0, 1, row, row + 1, GTK_FILL, GTK_FILL, 0, 0);
  gtk_widget_show(label);

  /* (make sure there is only 0 or 1) */
  if(arr_ptr->int_ret != 0) arr_ptr->int_ret = 1;

  if(arr_ptr->togg_label == NULL) l_togg_txt = " ";
  else                            l_togg_txt = arr_ptr->togg_label;
  /* check button */
  check_button = gtk_check_button_new_with_label (l_togg_txt);
  g_object_set_data (G_OBJECT (check_button), GAP_ARR_INTERFACE_PTR, (gpointer)arrint_ptr);
  gtk_table_attach ( GTK_TABLE (table), check_button, 1, 3, row, row+1, GTK_FILL, 0, 0, 0);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (check_button),
                                arr_ptr->int_ret);
  if(arr_ptr->help_txt != NULL)
  { 
     gimp_help_set_help_data(check_button, arr_ptr->help_txt,NULL);
  }
  gtk_widget_show (check_button);

  arr_ptr->check_button = check_button;

  g_signal_connect (G_OBJECT (check_button), "toggled",
                    G_CALLBACK (toggle_update_cb),
                    arr_ptr);
}


/* --------------------------
 * COMBO
 * --------------------------
 */
static void
combo_update_cb (GtkWidget *widget, GapArrArg *arr_ptr)
{
  gint   value;
  gint   radio_index;
  gint   l_idy;

  if(arr_ptr == NULL) return;
  if(arr_ptr->widget_locked) return;
  if((arr_ptr->widget_type != GAP_ARR_WGT_OPTIONMENU)
  && (arr_ptr->widget_type != GAP_ARR_WGT_OPT_ENTRY))
  {
    return;
  }

  radio_index = 0;
  
  gimp_int_combo_box_get_active (GIMP_INT_COMBO_BOX (widget), &value);
  
  arr_ptr->int_ret = value;
  arr_ptr->radio_ret = value;
  if(gap_debug) printf("radio_update_cb: radio_ret %d\n", (int)value);

  if((arr_ptr->widget_type == GAP_ARR_WGT_OPT_ENTRY)
  && (arr_ptr->radio_argv != NULL))
  {
    /* get the radio_index for the selected in value */
    for(l_idy=0; l_idy < arr_ptr->radio_argc; l_idy++)
    {
       if(value == l_idy) 
       {
         radio_index = l_idy;
         break;
       }
    }

    gtk_entry_set_text(GTK_ENTRY(arr_ptr->text_entry), arr_ptr->radio_argv[radio_index]);

    strncpy(arr_ptr->text_buf_ret,
          arr_ptr->radio_argv[radio_index],
          arr_ptr->text_buf_len -1);
    arr_ptr->text_buf_ret[arr_ptr->text_buf_len -1] = '\0';

  }
}

/* --------------------------
 * RADIO
 * --------------------------
 */

static void
radio_update_cb (GtkWidget *widget, t_radio_arg *radio_ptr)
{
  if(radio_ptr->arr_ptr == NULL) return;
  if(radio_ptr->arr_ptr->widget_locked) return;
  if(radio_ptr->arr_ptr->widget_type != GAP_ARR_WGT_RADIO)
  {
    return;
  }
  
  radio_ptr->arr_ptr->int_ret = radio_ptr->radio_index;
  radio_ptr->arr_ptr->radio_ret = radio_ptr->radio_index;
  if(gap_debug) printf("radio_update_cb: radio_ret %d\n", (int)radio_ptr->arr_ptr->radio_ret );

}

static void
radio_create_value(char *title, GtkTable *table, int row, 
                   GapArrArg *arr_ptr, t_arr_interface *arrint_ptr)
{
  GtkWidget *label;
  GtkWidget *radio_table;
  GtkWidget *radio_button;
  GSList    *radio_group = NULL;
  gint       l_idy;
  char       *l_radio_txt;
  const char *l_radio_help_txt;
  gint       l_radio_pressed;
  gint       l_int_ret_initial_value;


  t_radio_arg   *radio_ptr;
  t_radio_arg   *radio_list_root;
  t_radio_arg   *radio_list_prev;

  l_int_ret_initial_value = arr_ptr->radio_ret;
  
  label = gtk_label_new(title);
  gtk_misc_set_alignment(GTK_MISC(label), LABEL_DEFAULT_ALIGN_X, LABEL_DEFAULT_ALIGN_Y);
  gtk_table_attach( GTK_TABLE (table), label, 0, 1, row, row+1, GTK_FILL, GTK_FILL, 0, 0);
  gtk_widget_show(label);

  /* radio_table */
  radio_table = gtk_table_new (arr_ptr->radio_argc, 2, FALSE);

  radio_list_root = NULL;
  radio_list_prev = NULL;
  for(l_idy=0; l_idy < arr_ptr->radio_argc; l_idy++)
  {
     radio_ptr          = g_malloc0(sizeof(t_radio_arg));
     radio_ptr->arr_ptr = arr_ptr;
     radio_ptr->radio_index = l_idy;
     radio_ptr->next = NULL;
     if(l_idy==0)
     {
       radio_list_root = radio_ptr;
     }
     if(radio_list_prev)
     {
       radio_list_prev->next = radio_ptr;
     }
     radio_list_prev = radio_ptr;
     radio_ptr->first = radio_list_root;
     arr_ptr->radiogroup = (gpointer)radio_list_root;
     
     if(arr_ptr->radio_ret == l_idy) l_radio_pressed  = TRUE;
     else                            l_radio_pressed  = FALSE;

     l_radio_txt = "null";
     if (arr_ptr->radio_argv != NULL)
     {
        if (arr_ptr->radio_argv[l_idy] != NULL)
            l_radio_txt = arr_ptr->radio_argv[l_idy];
     }

     l_radio_help_txt = arr_ptr->help_txt;
     if (arr_ptr->radio_help_argv != NULL)
     {
         if (arr_ptr->radio_help_argv[l_idy] != NULL)
            l_radio_help_txt = arr_ptr->radio_help_argv[l_idy];
     }
 
     if(gap_debug) printf("radio_create_value: %02d %s\n", l_idy, l_radio_txt);
    
     radio_button = gtk_radio_button_new_with_label ( radio_group, l_radio_txt );
     g_object_set_data (G_OBJECT (radio_button), GAP_ARR_INTERFACE_PTR, (gpointer)arrint_ptr);
     radio_group = gtk_radio_button_get_group ( GTK_RADIO_BUTTON (radio_button) );  
     gtk_table_attach ( GTK_TABLE (radio_table), radio_button, 0, 2, l_idy, l_idy+1, GTK_FILL | GTK_EXPAND, 0, 0, 0);

     gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (radio_button), 
                                   l_radio_pressed);
     if(l_radio_help_txt != NULL)
     { 
       gimp_help_set_help_data(radio_button, l_radio_help_txt, NULL);
     }
     gtk_widget_show (radio_button);
     radio_ptr->radio_button = radio_button;

     g_signal_connect ( G_OBJECT (radio_button), "toggled",
                        G_CALLBACK (radio_update_cb),
                        radio_ptr);
  }

  
  /* attach radio_table */
  gtk_table_attach ( GTK_TABLE (table), radio_table, 1, 3, row, row+1, GTK_FILL | GTK_EXPAND, 0, 0, 0);
  gtk_widget_show (radio_table);

  /* the new gtk always calls the callback procedure of the 1st radio item,
   * if the 1.st item (with index 0) is not the selected one,
   * and the user makes no further selection
   * 0 would be returned as result (but this is wrong and not the expected value)
   * WORKAROUND:
   * copy initial value to int_ret 
   */
  arr_ptr->int_ret = l_int_ret_initial_value;
  arr_ptr->radio_ret = l_int_ret_initial_value;
}


/* ----------------------------------------------
 * COMBO BOX (replaces the deprecated OPTIONMENU)
 * ----------------------------------------------
 */

/* combo boxes share data structure with GAP_ARR_WGT_RADIO */
static void
combo_create_value(char *title, GtkTable *table, int row,
                   GapArrArg *arr_ptr, t_arr_interface *arrint_ptr)
{
  GtkWidget *label;
  GtkWidget *entry;
  GtkWidget  *combo;
  gint       l_idx;
  gint       l_idy;
  char      *l_radio_txt;
  gint       l_col;


  /* label */
  l_col = 0;
  label = gtk_label_new(title);
  gtk_misc_set_alignment(GTK_MISC(label), LABEL_DEFAULT_ALIGN_X, LABEL_DEFAULT_ALIGN_Y);
  gtk_table_attach( GTK_TABLE (table), label, l_col, l_col+1, row, row+1, GTK_FILL, GTK_FILL, 0, 0);
  gtk_widget_show(label);

  /* entry */
  if(arr_ptr->widget_type == GAP_ARR_WGT_OPT_ENTRY)
  {
    l_col++;
    entry = gtk_entry_new();
    g_object_set_data (G_OBJECT (entry), GAP_ARR_INTERFACE_PTR, (gpointer)arrint_ptr);
    gtk_widget_set_size_request(entry, arr_ptr->entry_width, -1);
    gtk_entry_set_text(GTK_ENTRY(entry), arr_ptr->text_buf_ret);
    gtk_table_attach(GTK_TABLE(table), entry, l_col, l_col+1, row, row + 1, GTK_FILL, GTK_FILL | GTK_EXPAND, 4, 0);
    if(arr_ptr->help_txt != NULL)
    { 
       gimp_help_set_help_data(entry, arr_ptr->help_txt,NULL);
    }
    gtk_widget_show(entry);
    g_signal_connect(G_OBJECT(entry), "changed",
                     G_CALLBACK(text_entry_update_cb),
                     arr_ptr);
    
    arr_ptr->text_entry = entry;
  }

  /* create an empty combo box */
  l_col++;
  combo = g_object_new (GIMP_TYPE_INT_COMBO_BOX, NULL);
  g_object_set_data (G_OBJECT (combo), GAP_ARR_INTERFACE_PTR, (gpointer)arrint_ptr);

  g_signal_connect (combo, "changed",
                    G_CALLBACK (combo_update_cb),
                    arr_ptr);
  
  gtk_table_attach(GTK_TABLE(table), combo, l_col, l_col+1, row, row +1,
                   GTK_FILL, GTK_FILL, 0, 0);

  if(arr_ptr->help_txt != NULL)
  { 
       gimp_help_set_help_data(combo, arr_ptr->help_txt, NULL);
  }

  /* fill the combo box liststore with value/label */
  l_idx=0;
  for(l_idy=0; l_idy < arr_ptr->radio_argc; l_idy++)
  {
     if(arr_ptr->radio_ret == l_idy) 
     {
       l_idx = l_idy; 
     }

     l_radio_txt = "null";
     if (arr_ptr->radio_argv != NULL)
     {
        if (arr_ptr->radio_argv[l_idy] != NULL)
            l_radio_txt = arr_ptr->radio_argv[l_idy];
     }

     gimp_int_combo_box_append (GIMP_INT_COMBO_BOX (combo),
                                 GIMP_INT_STORE_VALUE, l_idy,
                                 GIMP_INT_STORE_LABEL, l_radio_txt,
                                 -1);
  }

  gtk_widget_show(combo);
  gimp_int_combo_box_set_active (GIMP_INT_COMBO_BOX (combo), l_idx);
  
  arr_ptr->combo = combo;

}


/* --------------------------
 * FLT_PAIR
 * --------------------------
 */

static void
pair_flt_create_value(gchar *title, GtkTable *table, gint row,
                      GapArrArg *arr_ptr, t_arr_interface *arrint_ptr)
{
  GtkObject *adj;
  gfloat     umin, umax;
  
  if(arr_ptr->constraint)
  {
    umin = arr_ptr->flt_min;
    umax = arr_ptr->flt_max;
  }
  else
  {
    umin = arr_ptr->umin;
    umax = arr_ptr->umax;
  }
  

  adj = 
  gimp_scale_entry_new( GTK_TABLE (table), 0, row,        /* table col, row */
                        title,                            /* label text */
                        arr_ptr->scale_width,             /* scalesize */
                        arr_ptr->entry_width,             /* entrysize */
                       (gfloat)arr_ptr->flt_ret,          /* init value */
                       (gfloat)arr_ptr->flt_min,          /* lower,  */
                       (gfloat)arr_ptr->flt_max,          /* upper */
                        arr_ptr->flt_step,                /* step */
                        arr_ptr->pagestep,                /* pagestep */
                        arr_ptr->flt_digits,              /* digits */
                        arr_ptr->constraint,              /* constrain */
                        umin, umax,                       /* lower, upper (unconstrained) */
                        arr_ptr->help_txt,                /* tooltip */
                        NULL);                            /* privatetip */

  g_signal_connect (G_OBJECT (adj), "value_changed",
                    G_CALLBACK (gimp_double_adjustment_update),
                    &arr_ptr->flt_ret);
  arr_ptr->adjustment = adj;                
}

/* --------------------------
 * INT_PAIR
 * --------------------------
 */

static void
pair_int_create_value(gchar *title, GtkTable *table, gint row, 
                      GapArrArg *arr_ptr, t_arr_interface *arrint_ptr)
{
  GtkObject *adj;
  gfloat     umin, umax;
  
  if(arr_ptr->constraint)
  {
    umin = (gfloat)arr_ptr->int_min;
    umax = (gfloat)arr_ptr->int_max;
  }
  else
  {
    umin = arr_ptr->umin;
    umax = arr_ptr->umax;
  }

  adj = 
  gimp_scale_entry_new( GTK_TABLE (table), 0, row,        /* table col, row */
                        title,                            /* label text */
                        arr_ptr->scale_width,             /* scalesize */
                        arr_ptr->entry_width,             /* entrysize */
                       (gfloat)arr_ptr->int_ret,          /* init value */
                       (gfloat)arr_ptr->int_min,          /* lower,  */
                       (gfloat)arr_ptr->int_max,          /* upper */
                        arr_ptr->int_step,                /* step */
                        arr_ptr->pagestep,                /* pagestep */
                        0,                                /* digits */
                        arr_ptr->constraint,              /* constrain */
                        umin, umax,                       /* lower, upper (unconstrained) */
                        arr_ptr->help_txt,                /* tooltip */
                        NULL);                            /* privatetip */

  g_signal_connect (G_OBJECT (adj), "value_changed",
                    G_CALLBACK (gimp_int_adjustment_update),
                    &arr_ptr->int_ret);

  arr_ptr->adjustment = adj;                
}


/* ============================================================================
 * gap_arr_std_dialog
 *
 *   GTK dialog window that has argc rows.
 *   each row contains the widget(s) as defined in argv[row]
 *
 *   The Dialog has an Action Area with OK and CANCEL Buttons.
 * ============================================================================
 */
gint gap_arr_std_dialog(const char *title_txt,
                        const char *frame_txt,
                        int        argc,
                        GapArrArg  argv[],
                        int        b_argc,
                        GapArrButtonArg  b_argv[],
                        gint       b_def_val)
{
  GtkWidget *hbbox;
  GtkWidget *button;
  GtkWidget *frame;
  GtkWidget *table;
  gint    l_idx;
  gint    l_ok_value;
  char   *l_label_txt;
  GapArrArg  *arr_ptr;
  GapArrArg  *arr_help_ptr;
  GapArrArg  *arr_def_ptr;
  t_arr_interface arrint_struct;
  t_arr_interface *arrint_ptr;
    
  arr_def_ptr = NULL;
  arrint_ptr = &arrint_struct;

  arrint_ptr->run = b_def_val;           /* prepare default retcode (if window is closed without button) */
  l_ok_value = 0;
  table = NULL;
  arr_help_ptr = NULL;
  
  if((argc > 0) && (argv == NULL))
  {
    printf("gap_arr_std_dialog: calling error (widget array == NULL)\n");
    return (arrint_ptr->run);
  }
  if((b_argc > 0) && (b_argv == NULL))
  {
    printf("gap_arr_std_dialog: calling error (button array == NULL)\n");
    return (arrint_ptr->run);
  }

  gimp_ui_init ("gap_std_dialog", FALSE);
  gap_stock_init();
  
  /* dialog */
  arrint_ptr->dlg = gtk_dialog_new ();
  g_object_set_data (G_OBJECT (arrint_ptr->dlg), GAP_ARR_INTERFACE_PTR, (gpointer)arrint_ptr);
  gtk_window_set_title (GTK_WINDOW (arrint_ptr->dlg), title_txt);

  // hof: on my Gnome windowmanger (openSUSE 11.11) pop ups created with gtk_dialog_new
  // often open BEHIND other windows.
  //
  // As workaround i commented out placing the pop up on mouse position
  // (because this would completeley hide the popup behind the other window
  // when the popup is smaller)
  
  // gtk_window_set_position (GTK_WINDOW (arrint_ptr->dlg), GTK_WIN_POS_MOUSE);
  g_signal_connect (G_OBJECT (arrint_ptr->dlg), "destroy",
                    G_CALLBACK (arr_close_callback),
                    NULL);

  gtk_container_set_border_width (GTK_CONTAINER (GTK_DIALOG (arrint_ptr->dlg)->action_area), 2);
  gtk_box_set_homogeneous (GTK_BOX (GTK_DIALOG (arrint_ptr->dlg)->action_area), FALSE);

  hbbox = GTK_WIDGET(GTK_DIALOG (arrint_ptr->dlg)->action_area);


  /*  parameter settings  */
  if (frame_txt == NULL)   frame = gimp_frame_new ( _("Enter Values"));
  else                     frame = gimp_frame_new (frame_txt);
  gtk_frame_set_shadow_type (GTK_FRAME (frame), GTK_SHADOW_ETCHED_IN);
  gtk_container_set_border_width (GTK_CONTAINER (frame), 6);
  gtk_box_pack_start (GTK_BOX (GTK_DIALOG (arrint_ptr->dlg)->vbox), frame, TRUE, TRUE, 0);

  if(argc > 0)
  {
    /* table (one row per argv) */
    table = gtk_table_new (argc +1, 3, FALSE);
    gtk_table_set_col_spacings (GTK_TABLE (table), 4);
    gtk_table_set_row_spacings (GTK_TABLE (table), 2);
    gtk_container_set_border_width (GTK_CONTAINER (table), 4);
    gtk_container_add (GTK_CONTAINER (frame), table);

    for(l_idx = 0; l_idx < argc; l_idx++)
    {
       arr_ptr = &argv[l_idx];
       arr_ptr->widget_locked = TRUE;

       if(arr_ptr->label_txt == NULL)  l_label_txt = g_strdup(_("Value:"));
       else                            l_label_txt = g_strdup(arr_ptr->label_txt);
       
       switch(arr_ptr->widget_type)
       {
         case GAP_ARR_WGT_FLT_PAIR:
            pair_flt_create_value(l_label_txt, GTK_TABLE(table), (l_idx + 1), arr_ptr, arrint_ptr);
            break;
         case GAP_ARR_WGT_INT_PAIR:
            pair_int_create_value(l_label_txt, GTK_TABLE(table), (l_idx + 1), arr_ptr, arrint_ptr);
            break;
         case GAP_ARR_WGT_TOGGLE:
            toggle_create_value(l_label_txt, GTK_TABLE(table), (l_idx + 1),  arr_ptr, arrint_ptr);
            break;
         case GAP_ARR_WGT_RADIO:
            radio_create_value(l_label_txt, GTK_TABLE(table), (l_idx + 1),  arr_ptr, arrint_ptr);
            break;
         case GAP_ARR_WGT_OPTIONMENU:
         case GAP_ARR_WGT_OPT_ENTRY:
            combo_create_value(l_label_txt, GTK_TABLE(table), (l_idx + 1),  arr_ptr, arrint_ptr);
            break;
         case GAP_ARR_WGT_FILESEL:
            filesel_create_value(l_label_txt, GTK_TABLE(table), (l_idx + 1),  arr_ptr, arrint_ptr);
            break;
         case GAP_ARR_WGT_FONTSEL:
            fontsel_create_value(l_label_txt, GTK_TABLE(table), (l_idx + 1),  arr_ptr, arrint_ptr);
            break;
         case GAP_ARR_WGT_TEXT:
            text_create_value(l_label_txt, GTK_TABLE(table), (l_idx + 1),  arr_ptr, arrint_ptr);
            break;
         case GAP_ARR_WGT_INT:
            int_create_value(l_label_txt, GTK_TABLE(table), (l_idx + 1),  arr_ptr, arrint_ptr);
            break;
         case GAP_ARR_WGT_FLT:
            flt_create_value(l_label_txt, GTK_TABLE(table), (l_idx + 1),  arr_ptr, arrint_ptr);
            break;
         case GAP_ARR_WGT_LABEL:
            label_create_value(l_label_txt, GTK_TABLE(table), (l_idx + 1),  arr_ptr, arrint_ptr, 0.5);
            break;
         case GAP_ARR_WGT_LABEL_LEFT:
            label_create_value(l_label_txt, GTK_TABLE(table), (l_idx + 1),  arr_ptr, arrint_ptr, 0.0);
            break;
         case GAP_ARR_WGT_LABEL_RIGHT:
            label_create_value(l_label_txt, GTK_TABLE(table), (l_idx + 1),  arr_ptr, arrint_ptr, 1.0);
            break;
         case GAP_ARR_WGT_ACT_BUTTON:
            printf ("GAP_ARR_WGT_ACT_BUTTON not implemented yet, widget type ignored\n");
            break;
         case GAP_ARR_WGT_DEFAULT_BUTTON:
            arr_def_ptr = arr_ptr;
            break;
         case GAP_ARR_WGT_HELP_BUTTON:
            arr_help_ptr = arr_ptr;
            break;
         default:     /* undefined widget type */
            printf ("Unknown widget type %d ignored\n", arr_ptr->widget_type);
            break;

       }   /* end switch */
       arr_ptr->widget_locked = FALSE;
              
       g_free(l_label_txt);
    }      /* end for */
  }



  /*  Action area help-button (if desired) */
  if(arr_help_ptr && gimp_show_help_button ())
  {
    if(arr_help_ptr->help_id)
    {
      button = gtk_button_new_from_stock (GTK_STOCK_HELP);
      g_object_set_data (G_OBJECT (button), GAP_ARR_INTERFACE_PTR, (gpointer)arrint_ptr);
      gtk_box_pack_end (GTK_BOX (hbbox), button, FALSE, TRUE, 0); 
      gtk_button_box_set_child_secondary (GTK_BUTTON_BOX (hbbox),
                                          button, TRUE);
      gtk_widget_show (button);
      g_signal_connect (G_OBJECT (button), "clicked",
                        G_CALLBACK(arr_help_callback),
                        arr_help_ptr);
    }
  }
  
  if(arr_def_ptr)
  {
    t_all_arr_args arr_all;

    arr_all.argc = argc;
    arr_all.argv = argv;
    default_button_create_value(l_label_txt, hbbox, arr_def_ptr, arrint_ptr, &arr_all);
  }

  /* buttons in the action area */
  for(l_idx = 0; l_idx < b_argc; l_idx++)
  {

     if(b_argv[l_idx].but_txt == NULL)  
     {
       button = gtk_button_new_from_stock (GTK_STOCK_OK);
     }
     else
     {
       button = gtk_button_new_from_stock (b_argv[l_idx].but_txt);
     }
     g_object_set_data (G_OBJECT (button), GAP_ARR_INTERFACE_PTR, (gpointer)arrint_ptr);
     GTK_WIDGET_SET_FLAGS (button, GTK_CAN_DEFAULT);
     gtk_box_pack_start (GTK_BOX (hbbox), button, FALSE, FALSE, 0);
     if( b_argv[l_idx].but_val == b_def_val ) gtk_widget_grab_default (button);
     gtk_widget_show (button);
     g_signal_connect (G_OBJECT (button), "clicked",
                       G_CALLBACK (but_array_callback),
                       &b_argv[l_idx].but_val);
     
  }

  if(b_argc < 1)
  {
     /* if no buttons are specified use one CLOSE button per default */
     button = gtk_button_new_from_stock ( GTK_STOCK_CLOSE);
     g_object_set_data (G_OBJECT (button), GAP_ARR_INTERFACE_PTR, (gpointer)arrint_ptr);
     GTK_WIDGET_SET_FLAGS (button, GTK_CAN_DEFAULT);
     gtk_box_pack_start (GTK_BOX (hbbox), button, FALSE, FALSE, 0);
     gtk_widget_grab_default (button);
     gtk_widget_show (button);
     g_signal_connect (G_OBJECT (button), "clicked",
                       G_CALLBACK (but_array_callback),
                       &l_ok_value);
  }




  gtk_widget_show (frame);
  if(argc > 0)  {  gtk_widget_show (table); }
  gtk_widget_show (arrint_ptr->dlg);

  gtk_main ();
  gdk_flush ();

  if(gap_debug)
  {
     /* for debugging: print results to stdout */
     for(l_idx = 0; l_idx < argc; l_idx++)
     {
        arr_ptr = &argv[l_idx];

        if(arr_ptr->label_txt == NULL)  l_label_txt = g_strdup(_("Value:"));
        else                            l_label_txt = g_strdup(arr_ptr->label_txt);
        arr_ptr = &argv[l_idx];
        
        printf("%02d  ", l_idx);

        switch(arr_ptr->widget_type)
        {
          case GAP_ARR_WGT_FLT_PAIR:
          case GAP_ARR_WGT_FLT:
             printf("FLT  %s : %f\n",  l_label_txt, arr_ptr->flt_ret);
             break;
          case GAP_ARR_WGT_INT_PAIR:
          case GAP_ARR_WGT_INT:
          case GAP_ARR_WGT_TOGGLE:
             printf("INT  %s : %d\n",  l_label_txt, arr_ptr->int_ret);
             break;
          case GAP_ARR_WGT_TEXT:
          case GAP_ARR_WGT_OPT_ENTRY:
          case GAP_ARR_WGT_FILESEL:
          case GAP_ARR_WGT_FONTSEL:
             printf("TEXT  %s : %s\n",  l_label_txt, arr_ptr->text_buf_ret);
             break;
          case GAP_ARR_WGT_RADIO:
          case GAP_ARR_WGT_OPTIONMENU:
             printf("RADIO/OPTIONMENU  %s : %d\n",  l_label_txt, arr_ptr->radio_ret);
             break;
          default:
             printf("\n");
             break;

        }
        g_free(l_label_txt);
     }
  }
  
  return (arrint_ptr->run);
}       /* end gap_arr_std_dialog */



void
gap_arr_arg_init  (GapArrArg *arr_ptr,
                   gint       widget_type)
{
   arr_ptr->label_txt   = NULL;
   arr_ptr->help_txt    = NULL;
   arr_ptr->help_id     = NULL;
   arr_ptr->help_func   = NULL;
   arr_ptr->togg_label  = NULL;
   arr_ptr->entry_width = 80;
   arr_ptr->scale_width = 200;
   arr_ptr->constraint  = TRUE;
   arr_ptr->has_default = FALSE;
   arr_ptr->text_entry  = NULL;
   arr_ptr->text_buf_len     = 0;
   arr_ptr->text_buf_default = NULL;
   arr_ptr->text_buf_ret     = NULL;

   arr_ptr->text_fontsel = NULL;
   arr_ptr->text_filesel = NULL;
   arr_ptr->text_entry = NULL;
   arr_ptr->check_button = NULL;
   arr_ptr->combo = NULL;
   arr_ptr->adjustment = NULL;
   arr_ptr->radiogroup = NULL;

   switch(widget_type)
   {
     case GAP_ARR_WGT_LABEL:
     case GAP_ARR_WGT_LABEL_LEFT:
     case GAP_ARR_WGT_LABEL_RIGHT:
        arr_ptr->text_buf_len     = 0;
        arr_ptr->text_buf_default = NULL;
        arr_ptr->text_buf_ret     = NULL;
        arr_ptr->widget_type = widget_type;
        break;
     case GAP_ARR_WGT_INT_PAIR:
     case GAP_ARR_WGT_INT:
        arr_ptr->widget_type = widget_type;
        arr_ptr->umin        = (gfloat)G_MININT;
        arr_ptr->umax        = (gfloat)G_MAXINT;
        arr_ptr->int_min     = 0;
        arr_ptr->int_max     = 100;
        arr_ptr->int_step    = 1;
        arr_ptr->pagestep    = 10.0;
        arr_ptr->int_default = 0;
        arr_ptr->int_ret     = 0;
        break;
     case GAP_ARR_WGT_FLT_PAIR:
     case GAP_ARR_WGT_FLT:
        arr_ptr->widget_type = widget_type;
        arr_ptr->flt_digits  = 2;
        arr_ptr->umin        = G_MINFLOAT;
        arr_ptr->umax        = G_MAXFLOAT;
        arr_ptr->flt_min     = 0.0;
        arr_ptr->flt_max     = 100.0;
        arr_ptr->flt_step    = 0.1;
        arr_ptr->pagestep    = 10.0;
        arr_ptr->flt_default = 0.0;
        arr_ptr->flt_ret     = 0.0;
        break;
     case GAP_ARR_WGT_TOGGLE:
        arr_ptr->widget_type = widget_type;
        arr_ptr->int_default = 0;
        arr_ptr->int_ret     = 0;
        break;
     case GAP_ARR_WGT_RADIO:
     case GAP_ARR_WGT_OPTIONMENU:
     case GAP_ARR_WGT_OPT_ENTRY:
        arr_ptr->widget_type = widget_type;
        arr_ptr->radio_argc    = 0;
        arr_ptr->radio_default = 0;
        arr_ptr->radio_ret     = 0;
        arr_ptr->radio_argv    = NULL;
        arr_ptr->radio_help_argv = NULL;
        /* for GAP_ARR_WGT_OPT_ENTRY */
        arr_ptr->text_buf_len     = 0;
        arr_ptr->text_buf_default = NULL;
        arr_ptr->text_buf_ret     = NULL;
        break;
     case GAP_ARR_WGT_TEXT:
     case GAP_ARR_WGT_FONTSEL:
     case GAP_ARR_WGT_FILESEL:
        arr_ptr->widget_type = widget_type;
        arr_ptr->text_buf_len     = 0;
        arr_ptr->text_buf_default = NULL;
        arr_ptr->text_buf_ret     = NULL;
        arr_ptr->text_fontsel     = NULL;
        arr_ptr->text_filesel     = NULL;
        break;
     case GAP_ARR_WGT_ACT_BUTTON:
        arr_ptr->widget_type = widget_type;
        arr_ptr->action_functon = NULL;
        arr_ptr->action_data    = NULL;
        break;
     case GAP_ARR_WGT_DEFAULT_BUTTON:
     case GAP_ARR_WGT_HELP_BUTTON:
        arr_ptr->widget_type = widget_type;
        break;
     default:     /* Calling error: undefined widget type */
        arr_ptr->widget_type = GAP_ARR_WGT_LABEL;
        break;

   }
  
}       /* end gap_arr_arg_init */

/* ============================================================================
 *   simplified calls of gap_arr_std_dialog
 * ============================================================================
 */


gint gap_arr_ok_cancel_dialog(const char *title_txt,
                    const char *frame_txt,
                    int        argc,
                    GapArrArg  argv[])
{
    static GapArrButtonArg  b_argv[2];

    b_argv[1].but_txt  = GTK_STOCK_OK;
    b_argv[1].but_val  = TRUE;
    b_argv[0].but_txt  = GTK_STOCK_CANCEL;
    b_argv[0].but_val  = FALSE;
  
    return( gap_arr_std_dialog(title_txt,
                       frame_txt,
                       argc, argv,      /* widget array */
                       2,    b_argv,    /* button array */
                       FALSE)
           );          /* ret value for window close */
}

/* ============================================================================
 * gap_arr_buttons_dialog
 *   dialog window wit 1 upto n buttons
 *   return: the value aassigned with the pressed button.
 *           (If window closed by windowmanager return b_def_val)
 * ============================================================================
 */


gint gap_arr_buttons_dialog(const char *title_txt,
                         const char   *msg_txt,
                         int        b_argc,
                         GapArrButtonArg  b_argv[],
                         gint       b_def_val)
{
  static GapArrArg  argv[1];
  char   *frame_txt;

  if(b_argc == 1) frame_txt = _("Press Button");
  else            frame_txt = _("Select");
  
  gap_arr_arg_init(&argv[0], GAP_ARR_WGT_LABEL);
  argv[0].label_txt = msg_txt;

  return( gap_arr_std_dialog(title_txt,
                     frame_txt,
                     1, argv,
                     b_argc, b_argv,
                     b_def_val)
           );          /* ret value for window close */
                       
}       /* end gap_arr_buttons_dialog */



/* ============================================================================
 * gap_arr_slider_dialog
 *   simplified call of gap_arr_ok_cancel_dialog, using an array with one value.
 *
 *   return  the value of the (only) entryfield 
 *          or  -1 in case of Error or cancel
 * ============================================================================
 */

long gap_arr_slider_dialog(const char *title, const char *frame,
                     const char *label, const char *tooltip,
                     long min, long max, long curr, long constraint,
                     const char *help_id)
{
  static GapArrArg  argv[2];
  gboolean l_rc;
  gint argc;
  
  argc = 0;
  gap_arr_arg_init(&argv[argc], GAP_ARR_WGT_INT_PAIR);
  argv[argc].label_txt = label;
  argv[argc].help_txt = tooltip;
  argv[argc].constraint = constraint;
  argv[argc].entry_width = 80;
  argv[argc].scale_width = 130;
  argv[argc].int_min    = (gint)min;
  argv[argc].int_max    = (gint)max;
  argv[argc].int_step   = 1;
  argv[argc].int_ret    = (gint)curr;
  argc++;
  
  if(help_id)
  {
    gap_arr_arg_init(&argv[argc], GAP_ARR_WGT_HELP_BUTTON);
    argv[argc].help_id = help_id;
    argc++;
  }
  
  l_rc = gap_arr_ok_cancel_dialog(title, frame, argc, argv);
  
  if(l_rc == TRUE)
  {    return (long)(argv[0].int_ret);
  }
  else return -1;
}       /* end gap_arr_slider_dialog */


/* ------------------------
 * gap_arr_confirm_dialog
 * ------------------------
 */
gboolean
gap_arr_confirm_dialog(const char *msg_txt, const char *title_txt, const char *frame_txt)
{
  static GapArrButtonArg  l_but_argv[2];
  static GapArrArg  l_argv[1];
  gboolean          l_continue;
  gboolean          l_confirm;
  char *value_string;

  l_confirm = TRUE;
  value_string = gimp_gimprc_query("video-confirm-frame-delete");
  if(value_string)
  {

     if(gap_debug) printf("video-confirm-frame-delete: %s\n", value_string);

     /* check if gimprc preferences value no disables confirmation dialog */
     if((*value_string == 'n') || (*value_string == 'N'))
     {
       l_confirm = FALSE;
     }
     g_free(value_string);
  }
  else
  {
     if(gap_debug) printf("NOT found in gimprc: video-confirm-frame-delete:\n (CONFIRM default yes is used)");
  }

  if(!l_confirm)
  {
    /* automatic confirmed OK (dialog is disabled) */
    return TRUE;  
  }

  
  l_but_argv[0].but_txt  = GTK_STOCK_CANCEL;
  l_but_argv[0].but_val  = -1;
  l_but_argv[1].but_txt  = GTK_STOCK_OK;
  l_but_argv[1].but_val  = 0;


  gap_arr_arg_init(&l_argv[0], GAP_ARR_WGT_LABEL);
  l_argv[0].label_txt = msg_txt;

  l_continue = gap_arr_std_dialog (title_txt
                                  ,frame_txt
                                  ,G_N_ELEMENTS(l_argv),     l_argv
                                  ,G_N_ELEMENTS(l_but_argv), l_but_argv
                                  ,-1  /* default value is Cancel */
                                  );

  if(l_continue == 0)
  {
    return TRUE;  /* OK pressed */
  }
  
  return FALSE;

}  /* end gap_arr_confirm_dialog */


/* -----------------------------
 * gap_arr_overwrite_file_dialog
 * -----------------------------
 * if file exists ask for overwrite permission
 * return TRUE : caller may (over)write the file
 *        FALSE: do not write the file
 */
gboolean
gap_arr_overwrite_file_dialog(const char *filename)
{
  static  GapArrButtonArg  l_argv[2];
  static  GapArrArg  argv[1];

  if(g_file_test(filename, G_FILE_TEST_EXISTS))
  {
    gint    l_rc;
    gchar  *l_msg;
    gint    l_ii;
    
    l_ii = 0;
    if(g_file_test(filename, G_FILE_TEST_IS_DIR))
    {
      l_msg = g_strdup_printf(_("Directory '%s' already exists"), filename);
      /* filename is a directory, do not show the Overwrite Button */
    }
    else
    {
      l_msg = g_strdup_printf(_("File '%s' already exists"), filename);
      l_argv[l_ii].but_txt  = _("Overwrite");
      l_argv[l_ii].but_val  = 0;
      l_ii++;
    }

    l_argv[l_ii].but_txt  = GTK_STOCK_CANCEL;
    l_argv[l_ii].but_val  = -1;

    gap_arr_arg_init(&argv[0], GAP_ARR_WGT_LABEL);
    argv[0].label_txt = l_msg;
    
    l_rc =gap_arr_std_dialog ( _("GAP Question"),
                                  _("File Overwrite Warning"),
                                   1, argv,
                                   2, l_argv, -1);
    g_free(l_msg);
    if(l_rc < 0)
    {
      return (FALSE);   /* CANCEL was pressed, dont overwrite */
    }
  }
  return (TRUE);
  
}  /* end gap_arr_overwrite_file_dialog */


/* ----------------------------
 * gap_arr_msg_win
 * ----------------------------
 *  print a message both to stdout
 *  and to the gimp image window (only when run INTERACTIVE)
 *
 */
void
gap_arr_msg_win(GimpRunMode run_mode, const char *msg)
{
  if(msg)
  {
    if(*msg)
    {
       printf("%s\n", msg);

       if(run_mode == GIMP_RUN_INTERACTIVE)
       {
         g_message ("%s", msg);
       }
    }
  }
}    /* end  gap_arr_msg_win */

/* ----------------------------
 * gap_arr_msg_popup
 * ----------------------------
 *  print a message both to stdout
 *  and to a popup dialog window with OK button (only when run INTERACTIVE)
 *
 */
void gap_arr_msg_popup(GimpRunMode run_mode, const char *msg)
{
  static GapArrButtonArg  b_argv[1];
  static GapArrArg  argv[1];

  b_argv[1].but_txt  = GTK_STOCK_OK;
  b_argv[1].but_val  = TRUE;

  gap_arr_arg_init(&argv[0], GAP_ARR_WGT_LABEL);
  argv[0].label_txt = msg;
  

  if(msg)
  {
    if(*msg)
    {
       printf("%s\n", msg);

       if(run_mode == GIMP_RUN_INTERACTIVE)
       {
         gap_arr_std_dialog ( _("GAP Message")
                            ,""
                            ,G_N_ELEMENTS(argv),   argv
                            ,G_N_ELEMENTS(b_argv), b_argv
                            ,-1  /* default value is Cancel */
                            );
       }
    }
  }
}    /* end gap_arr_msg_popup  */

/* --------------------------------------
 * p_mkdir_from_file_if_not_exists
 * --------------------------------------
 * return TRUE if directory part of the specifed filename already exists
 *             or did not exist, but could be created successfully.
 * return FALSE if directory part does not exist and could not be created.
 */
int
p_mkdir_from_file_if_not_exists (const char *fname, int mode)
{
  gint   l_ii;
  char *l_dir;
  gboolean l_rc;

  l_rc = TRUE; /* assume success */

  if(fname != NULL)
  {
    /* build directory name from filename */
    l_dir = g_strdup(fname);
    for(l_ii = strlen(l_dir) -1; l_ii >= 0; l_ii--)
    {
      if(l_dir[l_ii] == G_DIR_SEPARATOR)
      {
        l_dir[l_ii] = '\0';
        break;
      }
      l_dir[l_ii] = '\0';
    }
    
    if(*l_dir != '\0')
    {
      /* check if directory already exists */
      if(!g_file_test(l_dir, G_FILE_TEST_IS_DIR))
      {
        gint l_errno;
        if(0 != gap_file_mkdir(l_dir, mode))
        {
          l_errno = errno;
          g_message(_("ERROR: could not create directory:"
                     "'%s'"
                     "%s")
                     ,l_dir
                     ,g_strerror (l_errno) );
          l_rc = FALSE;/* can not create vindex (invalid direcory path) */
        }
      }
    }

    g_free(l_dir);
  }

  return(l_rc);
}  /* end p_mkdir_from_file_if_not_exists */


/* --------------------------------
 * p_check_vindex_file
 * --------------------------------
 */
static gboolean
p_check_vindex_file(const char *vindex_file)
{
  gboolean l_rc;

  l_rc = TRUE; /* assume success */
  if(vindex_file)
  {
    l_rc = p_mkdir_from_file_if_not_exists(vindex_file, GAP_FILE_MKDIR_MODE);

    if(l_rc)
    {
      if(!g_file_test(vindex_file, G_FILE_TEST_EXISTS))
      {
        FILE *fp;
        gint l_errno;

        /* try to pre-create an empty videoindex file
         * this is done to check file creation errors immediate.
         * without this pre-create check the error would be detected very late
         * (after minutes minutes on large videos)
         * when the whole video was scanned and the index
         * should be written to file.
         */
        fp = g_fopen(vindex_file, "wb");
        if(fp)
        {
          fclose(fp);  /* OK, empty file written */
        }
        else
        {
          l_errno = errno;
          g_message(_("ERROR: Failed to write videoindex\n"
                    "file: '%s'\n"
                    "%s")
                    , vindex_file
                    , g_strerror (l_errno));
          l_rc = FALSE;
        }
      }
    }
    
  }
  return(l_rc);
}  /* end p_check_vindex_file */


/* --------------------------------
 * gap_arr_create_vindex_permission
 * --------------------------------
 * this procedure checks permission to create videoindex files
 * if permission is granted, but the videoindex name 
 * does not reside in a valid directory, the procedure tries to
 * create the directory automatically 
 * (but this is limited to a simple mkdir call and will create just one directory
 * not the full path if parent directories are invalid too)
 *
 * return TRUE : OK, permission to create videoindex
 *               (for already existing videoindex 
 *                TRUE is returned if gimprc settings
 *                permit videoindex creation)
 *        FALSE: user has cancelled, dont create videoindex
 */
gboolean
gap_arr_create_vindex_permission(const char *videofile, const char *vindex_file
 , gint32 seek_status)
{
  static GapArrArg  argv[6];
  gint   l_ii;
  char *value_string;
  char *l_videofile;
  char *l_vindex_file;
  char *l_info_msg;
  char *l_status_info;
  gboolean l_rc;
  gboolean l_ask_always;

#define SEEK_STATUS_SEEKSUPP_NONE   0
#define SEEK_STATUS_SEEKSUPP_VINDEX 1
#define SEEK_STATUS_SEEKSUPP_NATIVE 2

  l_ask_always = FALSE;
  l_videofile = NULL;
  l_vindex_file = NULL;
  value_string = gimp_gimprc_query("video-index-creation");
        
  if(value_string)
  {
    if((*value_string == 'n')
    || (*value_string == 'N'))
    {
      return(FALSE);  /* gimprc setting refuses permission to create videoindex */
    }
  }

  l_rc = p_mkdir_from_file_if_not_exists(vindex_file, GAP_FILE_MKDIR_MODE);
  if (l_rc != TRUE)
  {
    return(l_rc);
  }

        
  l_rc = FALSE;
  if(value_string)
  {
    /* check if gimprc setting grants general permission to create */
    if((*value_string == 'y')
    || (*value_string == 'Y'))
    {
      if ((seek_status == SEEK_STATUS_SEEKSUPP_NONE)
      || (seek_status == SEEK_STATUS_SEEKSUPP_NATIVE))
      {
        /* never automatically create videoindex for videos where
         * - seek is NOT possible (even with a video index)
         * - native seek support is available (without having video index)
         */
        return(FALSE);
      }
      
      l_rc = p_check_vindex_file(vindex_file);
      return(l_rc);
    }

    if (strcmp(value_string, "ask-always") == 0)
    {
      l_ask_always = TRUE;
    }
  }
  
  if (!l_ask_always)
  {
      if (seek_status == SEEK_STATUS_SEEKSUPP_NATIVE)
      {
        return(FALSE);
      }
  }

  l_info_msg = g_strdup_printf(_("Do you want to create a videoindex file ?\n"
                              "\n"
                              "If you want GIMP-GAP to create videoindex files automatically\n"
                              "when recommanded, whithout showing up this dialog again\n"
                              "then you should add the following line to\n"
                              "your gimprc file:\n"
                              "%s")
                              , "(video-index-creation \"yes\")"
                              );
  l_ii = 0;
  gap_arr_arg_init(&argv[l_ii], GAP_ARR_WGT_LABEL_LEFT);
  argv[l_ii].label_txt = " ";
  argv[l_ii].text_buf_ret = l_info_msg;

  l_ii++;
  gap_arr_arg_init(&argv[l_ii], GAP_ARR_WGT_LABEL_LEFT);
  argv[l_ii].label_txt = " ";
  argv[l_ii].text_buf_ret = " ";

  l_ii++;
  gap_arr_arg_init(&argv[l_ii], GAP_ARR_WGT_LABEL_LEFT);
  argv[l_ii].label_txt = " ";
  l_status_info = g_strdup("\0");
  switch (seek_status)
  {
    case SEEK_STATUS_SEEKSUPP_NONE:
      l_status_info = g_strdup_printf(_("WARNING:\nrandom positioning is not possible for this video.\n"
                                      "creating a video index is NOT recommanded\n"
                                      "(would not work)\n"));
      break;
    case SEEK_STATUS_SEEKSUPP_VINDEX:
      l_status_info = g_strdup_printf(_("TIP:\ncreating a video index on this video is recommanded.\n"
                                      "This will enable fast and random frame access.\n"
                                      "but requires one initial full scann.\n"
                                      "(this will take a while).\n"));
      break;
    case SEEK_STATUS_SEEKSUPP_NATIVE:
      l_status_info = g_strdup_printf(_("INFO:\nnative fast and exact random positioning works for this video.\n"
                                      "video index is not required, and should be cancelled.\n"));
      break;
  }
  argv[l_ii].text_buf_ret = l_status_info;

  if(videofile)
  {
    l_videofile = g_strdup(videofile);
    l_ii++;
    gap_arr_arg_init(&argv[l_ii], GAP_ARR_WGT_LABEL_LEFT);
    argv[l_ii].label_txt = _("Video:");
    argv[l_ii].text_buf_ret = l_videofile;
  }

  if(vindex_file)
  {
    l_vindex_file = g_strdup(vindex_file);
    l_ii++;
    gap_arr_arg_init(&argv[l_ii], GAP_ARR_WGT_LABEL_LEFT);
    argv[l_ii].label_txt = _("Index:");
    argv[l_ii].text_buf_ret = l_vindex_file;
  }
  l_ii++;
  
  l_rc = gap_arr_ok_cancel_dialog( _("Create Videoindex file"),
                                 " ", 
                                 l_ii, argv);

  if(l_rc)
  {
    l_rc = p_check_vindex_file(vindex_file);
  }
  
  if(l_videofile)
  {
    g_free(l_videofile);
  }
  if(l_vindex_file)
  {
    g_free(l_vindex_file);
  }
  g_free(l_info_msg);
  g_free(l_status_info);

  return (l_rc);
}  /* end gap_arr_create_vindex_permission */



/*---------------------------------- */
/*---------------------------------- */
/*---------------------------------- */

GtkWidget *
gap_dialog_add_button (GtkWidget  *dialog,
                        const gchar *button_text,
                        gint         response_id)
{
  GtkWidget *button;


  if(gap_debug)
  {
   printf("gap_dialog_add_button %s response_id:%d\n"
       ,button_text
       ,response_id
       );
  }


  button = gtk_dialog_add_button (GTK_DIALOG (dialog), button_text,
                                  response_id);
  gtk_widget_show (button);

  if (response_id == GTK_RESPONSE_OK)
    {
      gtk_dialog_set_default_response (GTK_DIALOG (dialog),
                                       GTK_RESPONSE_OK);
    }

  return button;
}

void
gap_dialog_add_buttons_valist (GtkWidget *dialog,
                                va_list     args)
{
  const gchar *button_text;
  gint         response_id;

  if(gap_debug)
  {
    printf("gap_dialog_add_buttons_valist START\n");
  }

  while ((button_text = va_arg (args, const gchar *)))
    {
      response_id = va_arg (args, gint);

      gap_dialog_add_button (dialog, button_text, response_id);
    }
}



static void
gap_dialog_help (GtkWidget *widget, GtkWidget *dialog)
{
  GimpHelpFunc    help_func;
  const gchar    *help_id;
 
  if(gap_debug)
  {
    printf("gap_dialog_help START\n");
  }

  help_func = (GimpHelpFunc) g_object_get_data (G_OBJECT (widget)
                                                       , "HelpFunc"
                                                       );
  help_id = (const gchar *) g_object_get_data (G_OBJECT (widget)
                                                       , "HelpId"
                                                       );

  if ((help_id != NULL) && (dialog != NULL))
  {
    if (help_func != NULL)
    {
      /* call user help function */   
      help_func (help_id, dialog);
    }
    else
    { 
      /* call gimp standard help function */
      gimp_standard_help_func(help_id, dialog);
    }
  }
}



GtkWidget *
gap_dialog_new_valist (const gchar    *title,
                        const gchar    *role,
                        GtkWidget      *parent,
                        GtkDialogFlags  flags,
                        GimpHelpFunc    help_func,
                        const gchar    *help_id,
                        va_list         args)
{
  GtkWidget *dlg;
  if(gap_debug)
  {
    printf("gap_dialog_new_valist START\n");
  }

  g_return_val_if_fail (title != NULL, NULL);
  g_return_val_if_fail (role != NULL, NULL);
  g_return_val_if_fail (parent == NULL || GTK_IS_WIDGET (parent), NULL);


  dlg = gtk_dialog_new ();
  gtk_window_set_type_hint (GTK_WINDOW (dlg), GDK_WINDOW_TYPE_HINT_NORMAL);
  if(gap_debug)
  {
    printf("gap_dialog_new_valist set title:%s\n", title);
  }
  gtk_window_set_title (GTK_WINDOW (dlg), title);
  
  gap_dialog_add_buttons_valist (dlg, args);


  if(help_id != NULL)
  {
      GtkWidget *action_area = gtk_dialog_get_action_area (dlg);
      GtkWidget *help_button = gtk_button_new_from_stock (GTK_STOCK_HELP);

      g_object_set_data (G_OBJECT (help_button), "HelpFunc", (gpointer)help_func);
      g_object_set_data (G_OBJECT (help_button), "HelpId", (gpointer)help_id);
      
      gtk_box_pack_end (GTK_BOX (action_area), help_button,
                        FALSE, TRUE, 0);
      gtk_button_box_set_child_secondary (GTK_BUTTON_BOX (action_area),
                                          help_button, TRUE);
      gtk_widget_show (help_button);

      g_signal_connect (G_OBJECT (help_button), "clicked",
                      G_CALLBACK(gap_dialog_help),
                      dlg);
  }
  else
  {
    if(gap_debug)
    {
      printf("gap_dialog_new_valist: help_id is NULL\n");
    }
  }
  return dlg;
}


/* ------------------------------
 * gap_dialog_new
 * ------------------------------
 * has same signature as gimp_dialog_new
 * but forces creation with hint GDK_WINDOW_TYPE_HINT_NORMAL
 * to behave like normal toplevel window
 * (e.g. has default decoreations of the Window manager
 * typical close, maximize, minimize widgets
 * and shall open in front of other windows..)
 */

GtkWidget *
gap_dialog_new (const gchar    *title,
                 const gchar    *role,
                 GtkWidget      *parent,
                 GtkDialogFlags  flags,
                 GimpHelpFunc    help_func,
                 const gchar    *help_id,
                 ...)
{
  GtkWidget *dialog;
  va_list    args;
  if(gap_debug)
  {
    printf("gap_dialog_new START\n");
  }

  g_return_val_if_fail (parent == NULL || GTK_IS_WIDGET (parent), NULL);
  g_return_val_if_fail (title != NULL, NULL);
  g_return_val_if_fail (role != NULL, NULL);

  if(gap_debug)
  {
    printf("gap_dialog_new title:%s role:%s parent:%d flags:%d\n"
       ,title
       ,role
       ,(int)parent
       ,(int)flags
       );
  }

  va_start (args, help_id);

  dialog = gap_dialog_new_valist (title, role,
                                  parent, flags,
                                  help_func, help_id,
                                  args);

  va_end (args);

  if(gap_debug)
  {
    printf("gap_dialog_new DONE, dialog:%d\n", (int)dialog);
  }

  return dialog;
}
