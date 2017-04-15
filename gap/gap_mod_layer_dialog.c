/* gap_mod_layer_dialog.c
 * 2004.11.11 hof (Wolfgang Hofer)
 *
 * GAP ... Gimp Animation Plugins
 *
 * This Module contains:
 * modify Layer(s) in frames dialog
 * (perform actions (like raise, set visible, apply filter)
 *               - foreach selected layer
 *               - in each frame of the selected framerange)
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
 * version 2.1.00   2004.11.01  hof: - created module
 */

/* SYTEM (UNIX) includes */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

/* GIMP includes */
#include "gtk/gtk.h"
#include "config.h"
#include "libgimp/gimp.h"
#include <libgimp/gimpui.h>

/* GAP includes */
#include "gap-intl.h"
#include "gap_stock.h"
#include "gap_match.h"
#include "gap_lib.h"
#include "gap_mod_layer.h"
#include "gap_mod_layer_dialog.h"



#define GAP_MOD_FRAMES_PLUGIN_NAME "plug_in_gap_modify"
#define GAP_MOD_FRAMES_HELP_ID     "plug-in-gap-modify"
#define SCALE_WIDTH        180
#define SPIN_BUTTON_WIDTH   75
#define RADIO_ITEM_INDEX_KEY   "gap_radio_item_index_key"
#define MENU_ITEM_INDEX_KEY    "gap_menu_item_index_key"
#define MENU_ITEM_TITLE_KEY    "gap_menu_item_title_key"
#define MENU_ITEM_TIPTEXT_KEY  "gap_menu_item_tiptext_key"

#define GAP_MOD_FRAMES_RESPONSE_ACTIVE 1


extern      int gap_debug; /* ==0  ... dont print debug infos */

static void p_mod_frames_response (GtkWidget *widget,
                 gint       response_id,
                 GapModFramesGlobalParams *gmop);
static void p_upd_sensitivity(GapModFramesGlobalParams *gmop);
static void p_func_optionmenu_callback  (GtkWidget     *wgt_item,
                           GapModFramesGlobalParams *gmop);
static void p_make_func_menu_item(const char *title
                    , const char *tip_text
                    , gint32 action_mode
                    , GtkWidget *menu
                    , GapModFramesGlobalParams *gmop
                    );

static void p_make_func_menu_item_set_mode(const char *mode_name
                    , gint32 action_mode
                    , GtkWidget *menu
                    , GapModFramesGlobalParams *gmop
                    );


static void p_make_layer_attrinutes_submenu(GtkWidget *master_menu, GapModFramesGlobalParams *gmop);
static void p_make_layer_modes_submenu(GtkWidget *master_menu, GapModFramesGlobalParams *gmop);
static void p_make_layer_stackpositions_submenu(GtkWidget *master_menu, GapModFramesGlobalParams *gmop);
static void p_make_merge_layers_submenu(GtkWidget *master_menu, GapModFramesGlobalParams *gmop);
static void p_make_selection_submenu(GtkWidget *master_menu, GapModFramesGlobalParams *gmop);
static void p_make_layermask_submenu(GtkWidget *master_menu, GapModFramesGlobalParams *gmop);
static void p_make_toplevel_menu_items(GtkWidget *master_menu, GapModFramesGlobalParams *gmop);

static void p_case_sensitive_toggled_callback(GtkCheckButton *checkbutton, GapModFramesGlobalParams *gmop);
static void p_invert_selection_toggled_callback(GtkCheckButton *checkbutton, GapModFramesGlobalParams *gmop);
static void p_layer_pattern_entry_update_cb(GtkWidget *widget, GapModFramesGlobalParams *gmop);
static void p_new_layername_entry_update_cb(GtkWidget *widget, GapModFramesGlobalParams *gmop);
static void p_new_groupname_entry_update_cb(GtkWidget *widget, GapModFramesGlobalParams *gmop);
static void p_sel_groupname_entry_update_cb(GtkWidget *widget, GapModFramesGlobalParams *gmop);
static void p_delimiter_entry_update_cb(GtkWidget *widget, GapModFramesGlobalParams *gmop);


static void p_sel_mode_radio_callback(GtkWidget *widget, GapModFramesGlobalParams *gmop);
static void p_create_mod_frames_dialog(GapModFramesGlobalParams *gmop);
static void p_get_values_from_active_layer(GapModFramesGlobalParams *gmop);



/* ---------------------------------
 * p_mod_frames_response
 * ---------------------------------
 */
static void
p_mod_frames_response (GtkWidget *widget,
                 gint       response_id,
                 GapModFramesGlobalParams *gmop)
{
  if (response_id == GAP_MOD_FRAMES_RESPONSE_ACTIVE)
  {
    p_get_values_from_active_layer(gmop);
    return;
  }
  else if (response_id == GTK_RESPONSE_CANCEL)
  {
    gmop->retcode = -1;
    if(gmop->run_flag == TRUE)
    {
      gmop->run_flag = FALSE;
      return;
    }
  }
  else if (response_id == GTK_RESPONSE_OK)
  {
    if(gmop->run_flag == TRUE)
    {
      return;
    }
    gtk_widget_set_sensitive(gmop->main_vbox, FALSE);

    gmop->run_flag = TRUE;
    gmop->retcode = gap_mod_frames_modify(gmop->ainfo_ptr
                                        , gmop->range_from
                                        , gmop->range_to
                                        , gmop->action_mode
                                        , gmop->sel_mode
                                        , gmop->sel_case
                                        , gmop->sel_invert
                                        , gmop->sel_pattern
                                        , gmop->new_layername
                                        , gmop->new_position
                                        , gmop->new_groupname
                                        , gmop->sel_groupname
                                        , gmop->delimiter
                                        , gmop->progress_bar
                                        , &gmop->run_flag
                                        );
  }

  if(gmop->shell)
  {
    GtkWidget *l_shell;

    l_shell = gmop->shell;
    gmop->shell = NULL;

    gtk_widget_destroy(l_shell);
    gtk_main_quit();
  }
}  /* end p_mod_frames_response */


/* ---------------------------------
 * p_get_values_from_active_layer
 * ---------------------------------
 */
static void
p_get_values_from_active_layer(GapModFramesGlobalParams *gmop)
{
  gint32 l_image_id;
  gint32 l_active_layer_id;
  gint32 l_parent_layer_id;
  gchar *l_group_path;
  gchar *l_delimiter;
  gchar *l_name;
  gint32 l_position;
  
  if (gmop == NULL)
  {
    return;
  }
  if((gmop->ainfo_ptr == NULL)
  || (gmop->layer_pattern_entry == NULL)
  || (gmop->sel_groupname_entry == NULL))
  {
    return;
  }


  l_image_id = gmop->ainfo_ptr->image_id;
  l_active_layer_id = gimp_image_get_active_layer(l_image_id);
  if (l_active_layer_id < 0)
  {
    return;
  }
  
  l_group_path = g_strdup("\0");
  l_delimiter = g_strdup("\0");
  l_parent_layer_id = gimp_item_get_parent (l_active_layer_id);
  while(l_parent_layer_id > 0)
  {
    char *l_name;
    char *l_group_path_new;
    
    l_name = gimp_item_get_name(l_parent_layer_id);
    l_group_path_new = g_strdup_printf("%s%s%s", l_name, l_delimiter, l_group_path);
    
    
    
    g_free(l_delimiter);
    g_free(l_name);
    g_free(l_group_path);
    l_group_path = l_group_path_new;
    l_delimiter = g_strdup(&gmop->delimiter[0]);
    
    l_parent_layer_id = gimp_item_get_parent (l_parent_layer_id);

  }
  
  gtk_entry_set_text(GTK_ENTRY(gmop->sel_groupname_entry), l_group_path);
  g_free(l_group_path);

  switch(gmop->sel_mode)
  {
    case GAP_MTCH_EQUAL:
    case GAP_MTCH_START:
    case GAP_MTCH_END:
    case GAP_MTCH_ANYWHERE:
      l_name = gimp_item_get_name(l_active_layer_id);
      break;
    case GAP_MTCH_INV_NUMBERLIST:
      {
        gint        l_nlayers;
        gint32     *l_src_layers;
        l_src_layers = NULL;
        l_nlayers = 0;
        l_parent_layer_id = gimp_item_get_parent (l_active_layer_id);
        if (l_parent_layer_id <= 0)
        {
          l_src_layers = gimp_image_get_layers (l_image_id, &l_nlayers);
        }
        else
        {
          l_src_layers = gimp_item_get_children (l_parent_layer_id, &l_nlayers);
        }
        if (l_src_layers != NULL)
        {
          g_free(l_src_layers);
        }
        l_position = gimp_image_get_item_position (l_image_id, l_active_layer_id);
        l_name = g_strdup_printf("%d", (l_nlayers -1) - l_position);
      }
      break;
    default:
      l_position = gimp_image_get_item_position (l_image_id, l_active_layer_id);
      l_name = g_strdup_printf("%d", l_position);
      break;
  }
  gtk_entry_set_text(GTK_ENTRY(gmop->layer_pattern_entry), l_name);
  g_free(l_name);

 
  
}  /* end p_get_values_from_active_layer */



/* --------------------------------------
 * p_hide_or_show_widget
 * --------------------------------------
 */
static void
p_hide_or_show_widget(GtkWidget  *wgt, gboolean sensitive)
{
  if(wgt)
  {
    if(sensitive == TRUE)
    {
      gtk_widget_show(wgt);
    }
    else
    {
      gtk_widget_hide(wgt);
    }
  }

}  /* end p_hide_or_show_widget */


/* --------------------------------------
 * p_hide_or_show_attached_widget_by_name
 * --------------------------------------
 */
static void
p_hide_or_show_attached_widget_by_name(GtkObject *adj, const char *name, gboolean sensitive)
{
  GtkWidget  *wgt;

  if(adj == NULL)
  {
    return;
  }

  wgt = GTK_WIDGET(g_object_get_data (G_OBJECT (adj), name));
  p_hide_or_show_widget(wgt, sensitive);

}  /* end p_hide_or_show_attached_widget_by_name */


/* ---------------------------------
 * p_upd_sensitivity
 * ---------------------------------
 */
static void
p_upd_sensitivity(GapModFramesGlobalParams *gmop)
{
  GtkWidget  *wgt;
  gboolean    l_sensitive;
  gboolean    l_sensitive_frame;
  gboolean    l_sensitive_new_group;
  const char *l_label_name;

  l_sensitive = FALSE;
  l_sensitive_new_group = FALSE;
  switch(gmop->sel_mode)
  {
    case GAP_MTCH_EQUAL:
    case GAP_MTCH_START:
    case GAP_MTCH_END:
    case GAP_MTCH_ANYWHERE:
      /* insensitive for other select modes that are
       * lists of stacknumbers or all_visible layers
       */
      l_sensitive = TRUE;
      break;
    default:
      break;
  }
  wgt = gmop->case_sensitive_check_button;
  if(wgt)
  {
    gtk_widget_set_sensitive(wgt, l_sensitive);
  }

  l_sensitive = TRUE;
  if(gmop->sel_mode == GAP_MTCH_ALL_VISIBLE)
  {
     /* the pattern entry is insensitive if all_visible layers (6) is selected */
     l_sensitive = FALSE;
  }
  wgt = gmop->layer_pattern_entry;
  if(wgt)
  {
    gtk_widget_set_sensitive(wgt, l_sensitive);
  }



  l_sensitive = FALSE;
  l_sensitive_frame = TRUE;
  l_label_name = " ";
  switch(gmop->action_mode)
  {
    case GAP_MOD_ACM_NEW_LAYER_GROUP:
      l_sensitive_new_group = TRUE;
      break;
    case GAP_MOD_ACM_REORDER_LAYER:
      l_label_name = _("New Layer Name");
      l_sensitive = TRUE;
      l_sensitive_new_group = TRUE;
      break;
    case GAP_MOD_ACM_DUPLICATE:
    case GAP_MOD_ACM_RENAME:
      l_label_name = _("New Layer Name");
      l_sensitive = TRUE;
      break;
    case GAP_MOD_ACM_MERGE_EXPAND:
    case GAP_MOD_ACM_MERGE_IMG:
    case GAP_MOD_ACM_MERGE_BG:
      l_label_name = _("Merged Layer Name");
      l_sensitive = TRUE;
      break;
    case GAP_MOD_ACM_SEL_SAVE:
    case GAP_MOD_ACM_SEL_LOAD:
    case GAP_MOD_ACM_SEL_DELETE:
      l_sensitive_frame = FALSE;
      l_label_name = _("Channel Name");
      l_sensitive = TRUE;
      break;
    case GAP_MOD_ACM_SEL_REPLACE:
    case GAP_MOD_ACM_SEL_ADD:
    case GAP_MOD_ACM_SEL_SUBTRACT:
    case GAP_MOD_ACM_SEL_INTERSECT:
    case GAP_MOD_ACM_SEL_NONE:
    case GAP_MOD_ACM_SEL_ALL:
    case GAP_MOD_ACM_SEL_INVERT:
      l_sensitive_frame = FALSE;
      break;
    case GAP_MOD_ACM_RECORD_LAYER_OFFSETS:
      l_label_name = _("XML Filename");
      l_sensitive = TRUE;
      break;
    default:
      break;
  }

  wgt = gmop->new_layername_entry;
  if(wgt)
  {
    gtk_widget_set_sensitive(wgt, l_sensitive);
  }

  wgt = gmop->layer_selection_frame;
  if(wgt)
  {
    gtk_widget_set_sensitive(wgt, l_sensitive_frame);
  }

  wgt = gmop->new_layername_label;
  if(wgt)
  {
    gtk_label_set_text(GTK_LABEL(wgt), l_label_name);
  }

 
  p_hide_or_show_widget(gmop->new_groupname_label, l_sensitive_new_group);
  p_hide_or_show_widget(gmop->new_groupname_entry, l_sensitive_new_group);

  p_hide_or_show_attached_widget_by_name(gmop->new_position_adj, "label", l_sensitive_new_group);
  p_hide_or_show_attached_widget_by_name(gmop->new_position_adj, "scale", l_sensitive_new_group);
  p_hide_or_show_attached_widget_by_name(gmop->new_position_adj, "spinbutton", l_sensitive_new_group);
  

}  /* end p_upd_sensitivity */




/* ---------------------------------
 * p_func_optionmenu_callback
 * ---------------------------------
 */
static void
p_func_optionmenu_callback  (GtkWidget     *wgt_item,
                           GapModFramesGlobalParams *gmop)
{
  gint32       l_idx;
  const char *title;
  const char *tiptext;

 if(gap_debug)
 {
   printf("CB: p_func_optionmenu_callback\n");
 }

 if(gmop == NULL)
 {
   return;
 }

 l_idx = GPOINTER_TO_INT(g_object_get_data (G_OBJECT (wgt_item),
                                            MENU_ITEM_INDEX_KEY));
 title = (const char *)g_object_get_data (G_OBJECT (wgt_item), MENU_ITEM_TITLE_KEY);
 tiptext = (const char *)g_object_get_data (G_OBJECT (wgt_item), MENU_ITEM_TIPTEXT_KEY);

 if(gap_debug)
 {
    printf("CB: p_func_optionmenu_callback index: %d %s\n"
          , (int)l_idx
          , (tiptext == NULL) ? "<null>" : tiptext
          );
 }

 gmop->action_mode = l_idx;

 if(title)
 {
   if(gmop->func_info_label)
   {
     gtk_label_set_text(GTK_LABEL(gmop->func_info_label), title);
   }

 }


 /* update widget_sensitivity */
 p_upd_sensitivity(gmop);

}  /* end p_func_optionmenu_callback */



/* ---------------------------------
 * p_make_func_menu_item
 * ---------------------------------
 */
static void
p_make_func_menu_item(const char *title
                    , const char *tip_text
                    , gint32 action_mode
                    , GtkWidget *menu
                    , GapModFramesGlobalParams *gmop
                    )
{
  GtkWidget *menu_item;

  menu_item = gtk_menu_item_new_with_label (title);
        g_signal_connect (G_OBJECT (menu_item), "activate",
                          G_CALLBACK (p_func_optionmenu_callback),
                          (gpointer)gmop);
        g_object_set_data (G_OBJECT (menu_item), MENU_ITEM_INDEX_KEY
                           , GINT_TO_POINTER(action_mode));
        g_object_set_data_full (G_OBJECT (menu_item), MENU_ITEM_TITLE_KEY
                                , g_strdup(title), (GDestroyNotify) g_free);
        g_object_set_data_full (G_OBJECT (menu_item), MENU_ITEM_TIPTEXT_KEY
                                , g_strdup(tip_text), (GDestroyNotify) g_free);
  gtk_widget_show (menu_item);
  gtk_menu_shell_append (GTK_MENU_SHELL (menu), menu_item);

  if(action_mode == gmop->action_mode)
  {
    if(gmop->func_info_label)
    {
      gtk_label_set_text(GTK_LABEL(gmop->func_info_label), title);
    }
  }
}  /* end p_make_func_menu_item */



/* -------------------------------
 * p_make_layer_attrinutes_submenu
 * -------------------------------
 */
static void
p_make_layer_attrinutes_submenu(GtkWidget *master_menu, GapModFramesGlobalParams *gmop)
{
  GtkWidget *menu_item;
  GtkWidget *sub_menu;

  /* the Layer Attributes sub menu */
  menu_item = gtk_menu_item_new_with_label (_("Layer Attributes"));
  gtk_widget_show (menu_item);
  gtk_menu_shell_append (GTK_MENU_SHELL (master_menu), menu_item);

  sub_menu = gtk_menu_new ();
  gtk_menu_item_set_submenu (GTK_MENU_ITEM (menu_item), sub_menu);

  p_make_func_menu_item(_("Set layer(s) visible")
                       ,_("set all selected layers visible")
                       ,GAP_MOD_ACM_SET_VISIBLE
                       ,sub_menu
                       ,gmop
                       );
  p_make_func_menu_item(_("Set layer(s) invisible")
                       ,_("set all selected layers invisible")
                       ,GAP_MOD_ACM_SET_INVISIBLE
                       ,sub_menu
                       ,gmop
                       );
  p_make_func_menu_item(_("Set layer(s) linked")
                       ,_("set all selected layers linked")
                       ,GAP_MOD_ACM_SET_LINKED
                       ,sub_menu
                       ,gmop
                       );
  p_make_func_menu_item(_("Set layer(s) unlinked")
                       ,_("set all selected layers unlinked")
                       ,GAP_MOD_ACM_SET_UNLINKED
                       ,sub_menu
                       ,gmop
                       );


  p_make_func_menu_item(_("Set layer active")
                       ,_("set all selected layers unlinked")
                       ,GAP_MOD_ACM_SET_ACTIVE_LAYER
                       ,sub_menu
                       ,gmop
                       );
  p_make_func_menu_item(_("Set layermask active")
                       ,_("set all selected layers unlinked")
                       ,GAP_MOD_ACM_SET_ACTIVE_LAYERMASK
                       ,sub_menu
                       ,gmop
                       );
  p_make_func_menu_item(_("Record layer offsets (to xml file)")
                       ,_("set all selected layers unlinked")
                       ,GAP_MOD_ACM_RECORD_LAYER_OFFSETS
                       ,sub_menu
                       ,gmop
                       );

}  /* end p_make_layer_attrinutes_submenu */


/* ------------------------------
 * p_make_func_menu_item_set_mode
 * ------------------------------
 */
static void
p_make_func_menu_item_set_mode(const char *mode_name
                    , gint32 action_mode
                    , GtkWidget *menu
                    , GapModFramesGlobalParams *gmop)
{
  char *title;
  char *tip_text;

  title = g_strdup_printf(_("Set layer(s) mode: %s"), mode_name);
  tip_text = g_strdup_printf(_("Set all selected layers to mode: %s"), mode_name);

  p_make_func_menu_item(title
           ,tip_text
           ,action_mode
           ,menu
           ,gmop
           );
  g_free(title);
  g_free(tip_text);
}  /* end p_make_func_menu_item_set_mode */

/* ------------------------------
 * p_make_layer_modes_submenu
 * ------------------------------
 */
static void
p_make_layer_modes_submenu(GtkWidget *master_menu, GapModFramesGlobalParams *gmop)
{
  GtkWidget *menu_item;
  GtkWidget *sub_menu;
  /* the Layer Modes sub menu */
  menu_item = gtk_menu_item_new_with_label (_("Layer Modes"));
  gtk_widget_show (menu_item);
  gtk_menu_shell_append (GTK_MENU_SHELL (master_menu), menu_item);

  sub_menu = gtk_menu_new ();
  gtk_menu_item_set_submenu (GTK_MENU_ITEM (menu_item), sub_menu);


  p_make_func_menu_item_set_mode(_("Normal")
           ,GAP_MOD_ACM_SET_MODE_NORMAL
           ,sub_menu
           ,gmop
           );
  p_make_func_menu_item_set_mode(_("Dissolve")
           ,GAP_MOD_ACM_SET_MODE_DISSOLVE
           ,sub_menu
           ,gmop
           );
  p_make_func_menu_item_set_mode(_("Multiply")
           ,GAP_MOD_ACM_SET_MODE_MULTIPLY
           ,sub_menu
           ,gmop
           );
  p_make_func_menu_item_set_mode(_("Divide")
           ,GAP_MOD_ACM_SET_MODE_DIVIDE
           ,sub_menu
           ,gmop
           );
  p_make_func_menu_item_set_mode(_("Screen")
           ,GAP_MOD_ACM_SET_MODE_SCREEN
           ,sub_menu
           ,gmop
           );
  p_make_func_menu_item_set_mode(_("Overlay")
           ,GAP_MOD_ACM_SET_MODE_OVERLAY
           ,sub_menu
           ,gmop
           );

  p_make_func_menu_item_set_mode(_("Difference")
           ,GAP_MOD_ACM_SET_MODE_DIFFERENCE
           ,sub_menu
           ,gmop
           );
  p_make_func_menu_item_set_mode(_("Addition")
           ,GAP_MOD_ACM_SET_MODE_ADDITION
           ,sub_menu
           ,gmop
           );
  p_make_func_menu_item_set_mode(_("Subtract")
           ,GAP_MOD_ACM_SET_MODE_SUBTRACT
           ,sub_menu
           ,gmop
           );
  p_make_func_menu_item_set_mode(_("Darken only")
           ,GAP_MOD_ACM_SET_MODE_DARKEN_ONLY
           ,sub_menu
           ,gmop
           );
  p_make_func_menu_item_set_mode(_("Lighten only")
           ,GAP_MOD_ACM_SET_MODE_LIGHTEN_ONLY
           ,sub_menu
           ,gmop
           );

  p_make_func_menu_item_set_mode(_("Dodge")
           ,GAP_MOD_ACM_SET_MODE_DODGE
           ,sub_menu
           ,gmop
           );
  p_make_func_menu_item_set_mode(_("Burn")
           ,GAP_MOD_ACM_SET_MODE_BURN
           ,sub_menu
           ,gmop
           );
  p_make_func_menu_item_set_mode(_("Hardlight")
           ,GAP_MOD_ACM_SET_MODE_HARDLIGHT
           ,sub_menu
           ,gmop
           );
  p_make_func_menu_item_set_mode(_("Softlight")
           ,GAP_MOD_ACM_SET_MODE_SOFTLIGHT
           ,sub_menu
           ,gmop
           );
  p_make_func_menu_item_set_mode(_("Color erase")
           ,GAP_MOD_ACM_SET_MODE_COLOR_ERASE
           ,sub_menu
           ,gmop
           );
  p_make_func_menu_item_set_mode(_("Grain extract")
           ,GAP_MOD_ACM_SET_MODE_GRAIN_EXTRACT_MODE
           ,sub_menu
           ,gmop
           );
  p_make_func_menu_item_set_mode(_("Grain merge")
           ,GAP_MOD_ACM_SET_MODE_GRAIN_MERGE_MODE
           ,sub_menu
           ,gmop
           );
  p_make_func_menu_item_set_mode(_("Hue")
           ,GAP_MOD_ACM_SET_MODE_HUE_MODE
           ,sub_menu
           ,gmop
           );
  p_make_func_menu_item_set_mode(_("Saturation")
           ,GAP_MOD_ACM_SET_MODE_SATURATION_MODE
           ,sub_menu
           ,gmop
           );
  p_make_func_menu_item_set_mode(_("Color")
           ,GAP_MOD_ACM_SET_MODE_COLOR_MODE
           ,sub_menu
           ,gmop
           );
  p_make_func_menu_item_set_mode(_("Value")
           ,GAP_MOD_ACM_SET_MODE_VALUE_MODE
           ,sub_menu
           ,gmop
           );
}  /* end p_make_layer_modes_submenu */



/* -----------------------------------
 * p_make_layer_stackpositions_submenu
 * -----------------------------------
 */
static void
p_make_layer_stackpositions_submenu(GtkWidget *master_menu, GapModFramesGlobalParams *gmop)
{
  GtkWidget *menu_item;
  GtkWidget *sub_menu;

  /* the Layer Stackposition sub menu */
  menu_item = gtk_menu_item_new_with_label (_("Layer Stackposition"));
  gtk_widget_show (menu_item);
  gtk_menu_shell_append (GTK_MENU_SHELL (master_menu), menu_item);

  sub_menu = gtk_menu_new ();
  gtk_menu_item_set_submenu (GTK_MENU_ITEM (menu_item), sub_menu);

  p_make_func_menu_item(_("Raise layer(s) to top")
                       ,_("raise selected layer(s) to top")
                       ,GAP_MOD_ACM_RAISE_TOP
                       ,sub_menu
                       ,gmop
                       );
  p_make_func_menu_item(_("Raise layer(s)")
                       ,_("raise all selected layers")
                       ,GAP_MOD_ACM_RAISE
                       ,sub_menu
                       ,gmop
                       );
  p_make_func_menu_item(_("Lower layer(s)")
                       ,_("lower all selected layers")
                       ,GAP_MOD_ACM_LOWER
                       ,sub_menu
                       ,gmop
                       );
  p_make_func_menu_item(_("Lower layer(s) to bottom")
                       ,_("lower selected layer(s) to bottom")
                       ,GAP_MOD_ACM_LOWER_BOTTOM
                       ,sub_menu
                       ,gmop
                       );
  p_make_func_menu_item(_("Reorder layer(s)")
                       ,_("move the selected layer(s) to specified group and/or stack position")
                       ,GAP_MOD_ACM_REORDER_LAYER
                       ,sub_menu
                       ,gmop
                       );
}  /* end p_make_layer_stackpositions_submenu */


/* ------------------------------
 * p_make_merge_layers_submenu
 * ------------------------------
 */
static void
p_make_merge_layers_submenu(GtkWidget *master_menu, GapModFramesGlobalParams *gmop)
{
  GtkWidget *menu_item;
  GtkWidget *sub_menu;

  /* the Merge Layers sub menu */
  menu_item = gtk_menu_item_new_with_label (_("Merge Layers"));
  gtk_widget_show (menu_item);
  gtk_menu_shell_append (GTK_MENU_SHELL (master_menu), menu_item);

  sub_menu = gtk_menu_new ();
  gtk_menu_item_set_submenu (GTK_MENU_ITEM (menu_item), sub_menu);

  p_make_func_menu_item(_("Merge layer(s); expand as necessary")
                       ,_("merge selected layers and expand as necessary")
                       ,GAP_MOD_ACM_MERGE_EXPAND
                       ,sub_menu
                       ,gmop
                       );
  p_make_func_menu_item(_("Merge layer(s) clipped to image")
                       ,_("merge selected layers and clip to image")
                       ,GAP_MOD_ACM_MERGE_IMG
                       ,sub_menu
                       ,gmop
                       );
  p_make_func_menu_item(_("Merge layer(s) clipped to bg-layer")
                       ,_("merge selected layers and clip to bg-layer")
                       ,GAP_MOD_ACM_MERGE_BG
                       ,sub_menu
                       ,gmop
                       );

  p_make_func_menu_item(_("Merge down layer(s); expand as necessary")
                       ,_("merge selected layers with the layer below and expand as necessary")
                       ,GAP_MOD_ACM_MERGE_DOWN_EXPAND
                       ,sub_menu
                       ,gmop
                       );
  p_make_func_menu_item(_("Merge down layer(s); clipped to image")
                       ,_("merge selected layers with the layer below and clip to image size")
                       ,GAP_MOD_ACM_MERGE_DOWN_IMG
                       ,sub_menu
                       ,gmop
                       );
  p_make_func_menu_item(_("Merge down layer(s) clipped to bg-layer")
                       ,_("merge selected layers with the layer below and clip to bg-layer")
                       ,GAP_MOD_ACM_MERGE_DOWN_BG
                       ,sub_menu
                       ,gmop
                       );

}  /* end p_make_merge_layers_submenu */


/* ------------------------------
 * p_make_selection_submenu
 * ------------------------------
 */
static void
p_make_selection_submenu(GtkWidget *master_menu, GapModFramesGlobalParams *gmop)
{
  GtkWidget *menu_item;
  GtkWidget *sub_menu;

  /* the Selection sub menu */
  menu_item = gtk_menu_item_new_with_label (_("Selection"));
  gtk_widget_show (menu_item);
  gtk_menu_shell_append (GTK_MENU_SHELL (master_menu), menu_item);

  sub_menu = gtk_menu_new ();
  gtk_menu_item_set_submenu (GTK_MENU_ITEM (menu_item), sub_menu);

  p_make_func_menu_item(_("Replace selection (source is the active frame)")
                       ,_("Replace Selection by Selection of the invoking Frame Image")
                       ,GAP_MOD_ACM_SEL_REPLACE
                       ,sub_menu
                       ,gmop
                       );
  p_make_func_menu_item(_("Add selection (source is the active frame)")
                       ,NULL
                       ,GAP_MOD_ACM_SEL_ADD
                       ,sub_menu
                       ,gmop
                       );
  p_make_func_menu_item(_("Subtract selection (source is the active frame)")
                       ,NULL
                       ,GAP_MOD_ACM_SEL_SUBTRACT
                       ,sub_menu
                       ,gmop
                       );
  p_make_func_menu_item(_("Intersect selection (source is the active frame)")
                       ,NULL
                       ,GAP_MOD_ACM_SEL_INTERSECT
                       ,sub_menu
                       ,gmop
                       );
  p_make_func_menu_item(_("Selection none")
                       ,NULL
                       ,GAP_MOD_ACM_SEL_NONE
                       ,sub_menu
                       ,gmop
                       );
  p_make_func_menu_item(_("Selection all")
                       ,NULL
                       ,GAP_MOD_ACM_SEL_ALL
                       ,sub_menu
                       ,gmop
                       );
  p_make_func_menu_item(_("Selection invert")
                       ,NULL
                       ,GAP_MOD_ACM_SEL_INVERT
                       ,sub_menu
                       ,gmop
                       );
  p_make_func_menu_item(_("Selection from alpha channel (individual per frame)")
                       ,NULL
                       ,GAP_MOD_ACM_SEL_ALPHA
                       ,sub_menu
                       ,gmop
                       );
  p_make_func_menu_item(_("Save selection to channel (individual per frame)")
                       ,NULL
                       ,GAP_MOD_ACM_SEL_SAVE
                       ,sub_menu
                       ,gmop
                       );
  p_make_func_menu_item(_("Load selection from channel (individual per frame)")
                       ,NULL
                       ,GAP_MOD_ACM_SEL_LOAD
                       ,sub_menu
                       ,gmop
                       );
  p_make_func_menu_item(_("Delete channel (by name)")
                       ,NULL
                       ,GAP_MOD_ACM_SEL_DELETE
                       ,sub_menu
                       ,gmop
                       );
}  /* end p_make_selection_submenu */


/* ------------------------------
 * p_make_layermask_submenu
 * ------------------------------
 */
static void
p_make_layermask_submenu(GtkWidget *master_menu, GapModFramesGlobalParams *gmop)
{
  GtkWidget *menu_item;
  GtkWidget *sub_menu;

  /* the LayerMask sub menu */
  menu_item = gtk_menu_item_new_with_label (_("Layer Mask"));
  gtk_widget_show (menu_item);
  gtk_menu_shell_append (GTK_MENU_SHELL (master_menu), menu_item);

  sub_menu = gtk_menu_new ();
  gtk_menu_item_set_submenu (GTK_MENU_ITEM (menu_item), sub_menu);

  p_make_func_menu_item(_("Add white layermask (opaque)")
                       ,NULL
                       ,GAP_MOD_ACM_LMASK_WHITE
                       ,sub_menu
                       ,gmop
                       );
  p_make_func_menu_item(_("Add black layermask (transparent)")
                       ,NULL
                       ,GAP_MOD_ACM_LMASK_BLACK
                       ,sub_menu
                       ,gmop
                       );
  p_make_func_menu_item(_("Add layermask from alpha")
                       ,NULL
                       ,GAP_MOD_ACM_LMASK_ALPHA
                       ,sub_menu
                       ,gmop
                       );
  p_make_func_menu_item(_("Add layermask transfer from alpha")
                       ,NULL
                       ,GAP_MOD_ACM_LMASK_TALPHA
                       ,sub_menu
                       ,gmop
                       );
  p_make_func_menu_item(_("Add layermask from selection")
                       ,NULL
                       ,GAP_MOD_ACM_LMASK_SEL
                       ,sub_menu
                       ,gmop
                       );
  p_make_func_menu_item(_("Add layermask from bw copy")
                       ,NULL
                       ,GAP_MOD_ACM_LMASK_BWCOPY
                       ,sub_menu
                       ,gmop
                       );
  p_make_func_menu_item(_("Invert existing layermask")
                       ,NULL
                       ,GAP_MOD_ACM_LMASK_INVERT
                       ,sub_menu
                       ,gmop
                       );
  p_make_func_menu_item(_("Apply filter on layermask")
                       ,NULL
                       ,GAP_MOD_ACM_APPLY_FILTER_ON_LAYERMASK
                       ,sub_menu
                       ,gmop
                       );
  p_make_func_menu_item(_("Delete layermask")
                       ,NULL
                       ,GAP_MOD_ACM_LMASK_DELETE
                       ,sub_menu
                       ,gmop
                       );
  p_make_func_menu_item(_("Apply layermask")
                       ,NULL
                       ,GAP_MOD_ACM_LMASK_APPLY
                       ,sub_menu
                       ,gmop
                       );
  p_make_func_menu_item(_("Copy layermask from layer above")
                       ,NULL
                       ,GAP_MOD_ACM_LMASK_COPY_FROM_UPPER_LMASK
                       ,sub_menu
                       ,gmop
                       );
  p_make_func_menu_item(_("Copy layermask from layer below")
                       ,NULL
                       ,GAP_MOD_ACM_LMASK_COPY_FROM_LOWER_LMASK
                       ,sub_menu
                       ,gmop
                       );

  p_make_func_menu_item(_("Create mask layer representing the opacity (alpha+layermask merged)")
                       ,NULL
                       ,GAP_MOD_ACM_CREATE_LAYER_FROM_OPACITY
                       ,sub_menu
                       ,gmop
                       );
  p_make_func_menu_item(_("Create mask layer as copy of the layermask (ignore alpha)")
                       ,NULL
                       ,GAP_MOD_ACM_CREATE_LAYER_FROM_LMASK
                       ,sub_menu
                       ,gmop
                       );
  p_make_func_menu_item(_("Create mask layer as copy of the alpha channel (ignore layermask)")
                       ,NULL
                       ,GAP_MOD_ACM_CREATE_LAYER_FROM_ALPHA
                       ,sub_menu
                       ,gmop
                       );

}  /* end p_make_layermask_submenu */


/* ------------------------------
 * p_make_toplevel_menu_items
 * ------------------------------
 */
static void
p_make_toplevel_menu_items(GtkWidget *master_menu, GapModFramesGlobalParams *gmop)
{
  /* apply filter has no sub_menu */
  p_make_func_menu_item(_("Apply filter on layer(s)")
                       ,_("apply filter to all selected layers")
                       ,GAP_MOD_ACM_APPLY_FILTER
                       ,master_menu
                       ,gmop
                       );
  p_make_func_menu_item(_("Duplicate layer(s)")
                       ,NULL
                       ,GAP_MOD_ACM_DUPLICATE
                       ,master_menu
                       ,gmop
                       );
  p_make_func_menu_item(_("Delete layer(s)")
                       ,NULL
                       ,GAP_MOD_ACM_DELETE
                       ,master_menu
                       ,gmop
                       );
  p_make_func_menu_item(_("Rename layer(s)")
                       ,NULL
                       ,GAP_MOD_ACM_RENAME
                       ,master_menu
                       ,gmop
                       );

  p_make_func_menu_item(_("Resize layer(s) to image size")
                       ,_("Resize selected layer(s) to image size")
                      ,GAP_MOD_ACM_RESIZE_TO_IMG
                      ,master_menu
                      ,gmop
                      );


  p_make_func_menu_item(_("Resize layer(s) to selection (active frame)")
                       ,_("Resize selected layer(s) to selection bounds of the active frame")
                      ,GAP_MOD_ACM_RESIZE_TO_SELECTION_1
                      ,master_menu
                      ,gmop
                      );

  p_make_func_menu_item(_("Resize layer(s) to selection (individual per frame)")
                       ,_("Resize selected layer(s) to selection bounds using individual selection per frame")
                      ,GAP_MOD_ACM_RESIZE_TO_SELECTION_N
                      ,master_menu
                      ,gmop
                      );


  p_make_func_menu_item(_("Add alpha channel")
                       ,NULL
                       ,GAP_MOD_ACM_ADD_ALPHA
                       ,master_menu
                       ,gmop
                       );
  p_make_func_menu_item(_("Create Layergroup")
                       ,NULL
                       ,GAP_MOD_ACM_NEW_LAYER_GROUP
                       ,master_menu
                       ,gmop
                       );
}  /* end p_make_toplevel_menu_items */




/* ---------------------------------
 * p_case_sensitive_toggled_callback
 * ---------------------------------
 */
static void
p_case_sensitive_toggled_callback(GtkCheckButton *checkbutton, GapModFramesGlobalParams *gmop)
{
 if(gmop)
 {
    if (GTK_TOGGLE_BUTTON(checkbutton)->active)
    {
       gmop->sel_case = TRUE;
    }
    else
    {
       gmop->sel_case = FALSE;
    }
 }
}  /* end p_case_sensitive_toggled_callback */


/* ---------------------------------
 * p_invert_selection_toggled_callback
 * ---------------------------------
 */
static void
p_invert_selection_toggled_callback(GtkCheckButton *checkbutton, GapModFramesGlobalParams *gmop)
{
 if(gmop)
 {
    if (GTK_TOGGLE_BUTTON(checkbutton)->active)
    {
       gmop->sel_invert = TRUE;
    }
    else
    {
       gmop->sel_invert = FALSE;
    }
 }
}  /* end p_invert_selection_toggled_callback */

/* --------------------------
 * p_layer_pattern_entry_update_cb
 * --------------------------
 */
static void
p_layer_pattern_entry_update_cb(GtkWidget *widget, GapModFramesGlobalParams *gmop)
{
  if(gmop == NULL)
  {
    return;
  }

  g_snprintf(gmop->sel_pattern, sizeof(gmop->sel_pattern), "%s"
            , gtk_entry_get_text(GTK_ENTRY(widget))
            );
}  /* end p_layer_pattern_entry_update_cb */

/* --------------------------
 * p_new_layername_entry_update_cb
 * --------------------------
 */
static void
p_new_layername_entry_update_cb(GtkWidget *widget, GapModFramesGlobalParams *gmop)
{
  if(gmop == NULL)
  {
    return;
  }

  g_snprintf(gmop->new_layername, sizeof(gmop->new_layername), "%s"
            , gtk_entry_get_text(GTK_ENTRY(widget))
            );
}  /* end p_new_layername_entry_update_cb */

/* --------------------------
 * p_sel_groupname_entry_update_cb
 * --------------------------
 */
static void
p_sel_groupname_entry_update_cb(GtkWidget *widget, GapModFramesGlobalParams *gmop)
{
  if(gmop == NULL)
  {
    return;
  }

  g_snprintf(gmop->sel_groupname, sizeof(gmop->sel_groupname), "%s"
            , gtk_entry_get_text(GTK_ENTRY(widget))
            );
}  /* end p_sel_groupname_entry_update_cb */

/* --------------------------
 * p_new_groupname_entry_update_cb
 * --------------------------
 */
static void
p_new_groupname_entry_update_cb(GtkWidget *widget, GapModFramesGlobalParams *gmop)
{
  if(gmop == NULL)
  {
    return;
  }

  g_snprintf(gmop->new_groupname, sizeof(gmop->new_groupname), "%s"
            , gtk_entry_get_text(GTK_ENTRY(widget))
            );
}  /* end p_new_groupname_entry_update_cb */


/* --------------------------
 * p_delimiter_entry_update_cb
 * --------------------------
 */
static void
p_delimiter_entry_update_cb(GtkWidget *widget, GapModFramesGlobalParams *gmop)
{
  if(gmop == NULL)
  {
    return;
  }

  g_snprintf(gmop->delimiter, sizeof(gmop->delimiter), "%s"
            , gtk_entry_get_text(GTK_ENTRY(widget))
            );
}  /* end p_delimiter_entry_update_cb */


/* --------------------------
 * p_sel_mode_radio_callback
 * --------------------------
 */
static void
p_sel_mode_radio_callback(GtkWidget *widget, GapModFramesGlobalParams *gmop)
{
  gint32 l_sel_mode;

  l_sel_mode = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (widget)
                                                   , RADIO_ITEM_INDEX_KEY));


  if((gmop) && (GTK_TOGGLE_BUTTON (widget)->active))
  {
    gmop->sel_mode = l_sel_mode;

    /* update widget_sensitivity */
    p_upd_sensitivity(gmop);
  }
}  /*end p_sel_mode_radio_callback */


/* -----------------------------
 * p_create_mod_frames_dialog
 * -----------------------------
 */
static void
p_create_mod_frames_dialog(GapModFramesGlobalParams *gmop)
{
  GtkWidget *dlg;
  GtkWidget *main_vbox;
  GtkWidget *hbox;
  GtkWidget *frame;
  GtkWidget *entry;
  GtkWidget *table;
  GtkWidget *sel_table;
  GtkWidget *sel_sub_table;
  GtkWidget *range_table;
  GtkWidget *label;
  GtkWidget *check_button;
  GtkWidget *menu_bar;
  GtkWidget *menu_item;
  GtkWidget *master_menu;
  GtkWidget *progress_bar;
  gint       row;
  gint       subrow;
  GtkObject *adj;

  GtkWidget *radio_button;
  GSList    *radio_group = NULL;
  gboolean  l_radio_pressed;



  dlg = gimp_dialog_new (_("Frames Modify"), GAP_MOD_FRAMES_PLUGIN_NAME,
                         NULL, 0,
                         gimp_standard_help_func, GAP_MOD_FRAMES_HELP_ID,

                         _("Get Active Layer"), GAP_MOD_FRAMES_RESPONSE_ACTIVE,
                         GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                         GTK_STOCK_OK,     GTK_RESPONSE_OK,
                         NULL);

  gmop->shell = dlg;

  g_signal_connect (G_OBJECT (gmop->shell), "response",
                    G_CALLBACK (p_mod_frames_response),
                    gmop);


  main_vbox = gtk_vbox_new (FALSE, 4);
  gmop->main_vbox = main_vbox;
  gtk_container_set_border_width (GTK_CONTAINER (main_vbox), 6);
  gtk_container_add (GTK_CONTAINER (GTK_DIALOG (dlg)->vbox), main_vbox);

  /*  +++++++++++++++++++++++++  */
  /*  the function        frame  */
  /*  +++++++++++++++++++++++++  */

  frame = gimp_frame_new (_("Function"));
  gtk_box_pack_start (GTK_BOX (main_vbox), frame, FALSE, FALSE, 0);
  gtk_widget_show (frame);

  table = gtk_table_new (3, 2, FALSE);
  gtk_table_set_col_spacings (GTK_TABLE (table), 4);
  gtk_table_set_row_spacings (GTK_TABLE (table), 4);
  gtk_container_add (GTK_CONTAINER (frame), table);
  gtk_widget_show (table);

  row = 0;

  /* the Fuction label */
  label = gtk_label_new (_("Function:"));
  gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.0);
  gtk_widget_show (label);

  /* the hbox */
  hbox = gtk_hbox_new (FALSE, 4);
  gtk_widget_show (hbox);

  /* the function menu_bar */
  menu_bar = gtk_menu_bar_new();
  gtk_widget_show (menu_bar);

// the tooltip is drawn in front of the menu items
// and forces the user for blind selection
// as workaround dont show tooltip for the menu_bar at all:
//  gimp_help_set_help_data (menu_bar
//                          , _("Function to be performed on all selected layers "
//                            "in all frames of the selected frame range")
//                        , NULL);


  gtk_table_attach (GTK_TABLE(table), menu_bar, 0, 1, row, row+1
                    , GTK_FILL, 0, 0, 0);

  /* the function_info_label (some kind of tooltip for the selected menu_item) */
  label = gtk_label_new ("menu_item function description");
  gmop->func_info_label = label;
  gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
  gtk_widget_show (label);
  gtk_table_attach (GTK_TABLE(table), label, 1, 2, row, row+1
                    , GTK_FILL|GTK_EXPAND , 0, 0, 0);


  /* the master menu */
  master_menu = gtk_menu_new ();


  /* the toplevel menu_item (will be replaced on any selection) */
  menu_item = gtk_menu_item_new_with_label(_("Function:"));
  gtk_widget_show (menu_item);
  gtk_menu_shell_append(GTK_MENU_SHELL(menu_bar), menu_item);
  gtk_menu_item_set_submenu(GTK_MENU_ITEM(menu_item), master_menu);

  /* make submenus */
  p_make_layer_attrinutes_submenu(master_menu, gmop);
  p_make_layer_modes_submenu(master_menu, gmop);
  p_make_layer_stackpositions_submenu(master_menu, gmop);
  p_make_merge_layers_submenu(master_menu, gmop);
  p_make_selection_submenu(master_menu, gmop);
  p_make_layermask_submenu(master_menu, gmop);

  /* finally we have some menu items that are not grouped into sub-menus */
  p_make_toplevel_menu_items(master_menu, gmop);


  row++;

  /* the LayerName (or channel Name) label */
  label = gtk_label_new (_("Layer Name:"));
  gmop->new_layername_label = label;
  gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.0);
  gtk_table_attach (GTK_TABLE(table), label, 0, 1, row, row+1
                    , GTK_FILL, 0, 0, 0);
  gtk_widget_show (label);

  /* the LayerName (or channel Name) entry  */
  entry = gtk_entry_new();
  gmop->new_layername_entry = entry;
  gtk_entry_set_text(GTK_ENTRY(entry), gmop->new_layername);
  gtk_widget_show(entry);
  gtk_table_attach (GTK_TABLE(table), entry, 1, 2, row, row+1
                    ,GTK_FILL|GTK_EXPAND , 0, 0, 0);
  g_signal_connect(G_OBJECT(entry), "changed",
                   G_CALLBACK (p_new_layername_entry_update_cb),
                   gmop);
  gimp_help_set_help_data(entry
                         , _("Name for all handled layers (or channels),\n"
                             "where the string '[######]' is replaced by the frame number.")
                         , NULL);
  row++;

  /* the newGroupName label */
  label = gtk_label_new (_("New Group Name:"));
  gmop->new_groupname_label = label;
  gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.0);
  gtk_table_attach (GTK_TABLE(table), label, 0, 1, row, row+1
                    , GTK_FILL, 0, 0, 0);
  gtk_widget_show (label);

  /* the newGroupName entry  */
  entry = gtk_entry_new();
  gmop->new_groupname_entry = entry;
  gtk_entry_set_text(GTK_ENTRY(entry), gmop->new_groupname);
  gtk_widget_show(entry);
  gtk_table_attach (GTK_TABLE(table), entry, 1, 2, row, row+1
                    ,GTK_FILL|GTK_EXPAND , 0, 0, 0);
  g_signal_connect(G_OBJECT(entry), "changed",
                   G_CALLBACK (p_new_groupname_entry_update_cb),
                   gmop);
  gimp_help_set_help_data(entry
                         , _("Name for the new group to be created if not already present\n"
                             "(relevant in reorder layer and creat group function)")
                         , NULL);

  row++;
  /* the new_position scale entry */
  adj = gimp_scale_entry_new (GTK_TABLE (table), 0, row,
                              _("New Position:"), SCALE_WIDTH, SPIN_BUTTON_WIDTH,
                              (gdouble)gmop->new_position,
                              (gdouble)0,  /* lower */
                              (gdouble)999,   /* upper */
                              1, 10,          /* step, page */
                              0,              /* digits */
                              TRUE,           /* constrain */
                              (gdouble)gmop->ainfo_ptr->first_frame_nr,  /* lower unconstrained */
                              (gdouble)gmop->ainfo_ptr->last_frame_nr,   /* upper unconstrained */
                              _("New Stack position (relevant for reorder layer function)"), NULL);
  gmop->new_position_adj = adj;
  g_object_set_data(G_OBJECT(adj), "gmop", gmop);
  g_signal_connect (adj, "value_changed",
                    G_CALLBACK (gimp_int_adjustment_update),
                    &gmop->new_position);

  /*  +++++++++++++++++++++++++  */
  /*  the layer selection frame  */
  /*  +++++++++++++++++++++++++  */

  frame = gimp_frame_new (_("Layer Selection"));
  gmop->layer_selection_frame = frame;
  gtk_box_pack_start (GTK_BOX (main_vbox), frame, FALSE, FALSE, 0);
  gtk_widget_show (frame);

  table = gtk_table_new (8, 2, FALSE);
  sel_table = table;
  gtk_container_add (GTK_CONTAINER (frame), table);
  gtk_widget_show (table);

  row = 0;

  /* the radio button "Pattern is equal to layer name" */
  radio_button = gtk_radio_button_new_with_label ( radio_group, _("Pattern is equal to layer name") );
  radio_group = gtk_radio_button_get_group ( GTK_RADIO_BUTTON (radio_button) );
  gtk_table_attach (GTK_TABLE(sel_table), radio_button, 0, 1, row, row+1
                    , GTK_FILL, 0, 0, 0);

  l_radio_pressed = (gmop->sel_mode == GAP_MTCH_EQUAL);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (radio_button),
                                   l_radio_pressed);
  gimp_help_set_help_data(radio_button
                         , _("Select all layers where layer name is equal to pattern")
                         , NULL);

  gtk_widget_show (radio_button);
  g_object_set_data (G_OBJECT (radio_button), RADIO_ITEM_INDEX_KEY
                     , GINT_TO_POINTER(GAP_MTCH_EQUAL));
  g_signal_connect ( G_OBJECT (radio_button), "toggled",
                     G_CALLBACK (p_sel_mode_radio_callback),
                     gmop);

  /* the case sensitive  check_button */
  check_button = gtk_check_button_new_with_label (_("Case sensitive"));
  gmop->case_sensitive_check_button = check_button;
  gtk_widget_show (check_button);
  gtk_table_attach (GTK_TABLE (sel_table), check_button, 1, 2, row, row+1
                   ,(GtkAttachOptions) (GTK_FILL)
                   ,(GtkAttachOptions) (0), 0, 0);
  gimp_help_set_help_data (check_button
                          , _("Lowercase and uppercase letters are considered as different")
                          , NULL);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (check_button)
                                , gmop->sel_case);
  g_signal_connect ( G_OBJECT (check_button), "toggled",
                     G_CALLBACK (p_case_sensitive_toggled_callback),
                     gmop);

  row++;

  /* the radio button "Pattern is start of layer name"  */
  radio_button = gtk_radio_button_new_with_label ( radio_group, _("Pattern is start of layer name") );
  radio_group = gtk_radio_button_get_group ( GTK_RADIO_BUTTON (radio_button) );
  gtk_table_attach (GTK_TABLE(sel_table), radio_button, 0, 1, row, row+1
                    , GTK_FILL, 0, 0, 0);

  l_radio_pressed = (gmop->sel_mode == GAP_MTCH_START);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (radio_button),
                                   l_radio_pressed);
  gimp_help_set_help_data(radio_button
                         , _("Select all layers where layer name starts with pattern")
                         , NULL);

  gtk_widget_show (radio_button);
  g_object_set_data (G_OBJECT (radio_button), RADIO_ITEM_INDEX_KEY
                     , GINT_TO_POINTER(GAP_MTCH_START));
  g_signal_connect ( G_OBJECT (radio_button), "toggled",
                     G_CALLBACK (p_sel_mode_radio_callback),
                     gmop);

  /* the invert layer_selection  check_button */
  check_button = gtk_check_button_new_with_label (_("Invert Layer Selection"));
  gmop->invert_check_button = check_button;
  gtk_widget_show (check_button);
  gtk_table_attach (GTK_TABLE (sel_table), check_button, 1, 2, row, row+1
                    ,(GtkAttachOptions) (GTK_FILL)
                    ,(GtkAttachOptions) (0), 0, 0);
  gimp_help_set_help_data (check_button
                          , _("Perform actions on all unselected layers at same level in the layers tree (i.e. in the same group)")
                          , NULL);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (check_button)
                                , gmop->sel_invert);
  g_signal_connect ( G_OBJECT (check_button), "toggled",
                     G_CALLBACK (p_invert_selection_toggled_callback),
                     gmop);

  row++;

  /* the  radio button "Pattern is end of layer name" */
  radio_button = gtk_radio_button_new_with_label ( radio_group, _("Pattern is end of layer name") );
  radio_group = gtk_radio_button_get_group ( GTK_RADIO_BUTTON (radio_button) );
  gtk_table_attach (GTK_TABLE(sel_table), radio_button, 0, 1, row, row+1
                    , GTK_FILL, 0, 0, 0);

  l_radio_pressed = (gmop->sel_mode == GAP_MTCH_END);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (radio_button),
                                   l_radio_pressed);
  gimp_help_set_help_data(radio_button
                         , _("Select all layers where layer name ends up with pattern")
                         , NULL);

  gtk_widget_show (radio_button);
  g_object_set_data (G_OBJECT (radio_button), RADIO_ITEM_INDEX_KEY
                     , GINT_TO_POINTER(GAP_MTCH_END));
  g_signal_connect ( G_OBJECT (radio_button), "toggled",
                     G_CALLBACK (p_sel_mode_radio_callback),
                     gmop);

  row++;

  /* the  radio button "Pattern is a part of layer name" */
  radio_button = gtk_radio_button_new_with_label ( radio_group, _("Pattern is a part of layer name") );
  radio_group = gtk_radio_button_get_group ( GTK_RADIO_BUTTON (radio_button) );
  gtk_table_attach (GTK_TABLE(sel_table), radio_button, 0, 1, row, row+1
                    , GTK_FILL, 0, 0, 0);

  l_radio_pressed = (gmop->sel_mode == GAP_MTCH_ANYWHERE);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (radio_button),
                                   l_radio_pressed);
  gimp_help_set_help_data(radio_button
                         , _("Select all layers where layer name contains pattern")
                         , NULL);

  gtk_widget_show (radio_button);
  g_object_set_data (G_OBJECT (radio_button), RADIO_ITEM_INDEX_KEY
                     , GINT_TO_POINTER(GAP_MTCH_ANYWHERE));
  g_signal_connect ( G_OBJECT (radio_button), "toggled",
                     G_CALLBACK (p_sel_mode_radio_callback),
                     gmop);

  row++;

  /* the  radio button "Pattern is a list of layerstack numbers" */
  radio_button = gtk_radio_button_new_with_label ( radio_group, _("Pattern is a list of layerstack numbers") );
  radio_group = gtk_radio_button_get_group ( GTK_RADIO_BUTTON (radio_button) );
  gtk_table_attach (GTK_TABLE(sel_table), radio_button, 0, 1, row, row+1
                    , GTK_FILL, 0, 0, 0);

  l_radio_pressed = (gmop->sel_mode == GAP_MTCH_NUMBERLIST);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (radio_button),
                                   l_radio_pressed);
  gimp_help_set_help_data(radio_button
                         , _("Select layerstack positions where 0 is the top layer.\nExample: 0, 4-5, 8")
                         , NULL);

  gtk_widget_show (radio_button);
  g_object_set_data (G_OBJECT (radio_button), RADIO_ITEM_INDEX_KEY
                     , GINT_TO_POINTER(GAP_MTCH_NUMBERLIST));
  g_signal_connect ( G_OBJECT (radio_button), "toggled",
                     G_CALLBACK (p_sel_mode_radio_callback),
                     gmop);

  row++;

  /* the  radio button "Pattern is a list of reverse layerstack numbers" */
  radio_button = gtk_radio_button_new_with_label ( radio_group, _("Pattern is a list of reverse layerstack numbers") );
  radio_group = gtk_radio_button_get_group ( GTK_RADIO_BUTTON (radio_button) );
  gtk_table_attach (GTK_TABLE(sel_table), radio_button, 0, 1, row, row+1
                    , GTK_FILL, 0, 0, 0);

  l_radio_pressed = (gmop->sel_mode == GAP_MTCH_INV_NUMBERLIST);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (radio_button),
                                   l_radio_pressed);
  gimp_help_set_help_data(radio_button
                         , _("Select layerstack positions where 0 is the background layer.\nExample: 0, 4-5, 8")
                         , NULL);

  gtk_widget_show (radio_button);
  g_object_set_data (G_OBJECT (radio_button), RADIO_ITEM_INDEX_KEY
                     , GINT_TO_POINTER(GAP_MTCH_INV_NUMBERLIST));
  g_signal_connect ( G_OBJECT (radio_button), "toggled",
                     G_CALLBACK (p_sel_mode_radio_callback),
                     gmop);

  row++;

  /* the  radio button "All visible (ignore pattern)" */
  radio_button = gtk_radio_button_new_with_label ( radio_group, _("All visible (ignore pattern)") );
  radio_group = gtk_radio_button_get_group ( GTK_RADIO_BUTTON (radio_button) );
  gtk_table_attach (GTK_TABLE(sel_table), radio_button, 0, 1, row, row+1
                    , GTK_FILL, 0, 0, 0);

  l_radio_pressed = (gmop->sel_mode == GAP_MTCH_ALL_VISIBLE);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (radio_button),
                                   l_radio_pressed);
  gimp_help_set_help_data(radio_button
                         , _("Select all visible layers at same level in the layers tree (i.e. in the same group)")
                         , NULL);

  gtk_widget_show (radio_button);
  g_object_set_data (G_OBJECT (radio_button), RADIO_ITEM_INDEX_KEY
                     , GINT_TO_POINTER(GAP_MTCH_ALL_VISIBLE));
  g_signal_connect ( G_OBJECT (radio_button), "toggled",
                     G_CALLBACK (p_sel_mode_radio_callback),
                     gmop);


  row++;

  /* the sel_sub_table */
  sel_sub_table = gtk_table_new (2, 3, FALSE);
  gtk_widget_show (sel_sub_table);

  subrow = 0;


  /* the layer_pattern label */
  label = gtk_label_new (_("Layer Pattern:"));
  gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
  gtk_widget_show (label);
  gtk_table_attach (GTK_TABLE(sel_sub_table), label, 0, 1, subrow, subrow+1
                    , 0, 0, 0, 0);


  /* the layer_pattern entry */
  entry = gtk_entry_new();
  gmop->layer_pattern_entry = entry;
  gtk_entry_set_text(GTK_ENTRY(entry), gmop->sel_pattern);
  gimp_help_set_help_data(entry
                         , _("String to identify layer names or layerstack position numbers. Example: 0,3-5")
                         , NULL);
  gtk_widget_show(entry);
  g_signal_connect(G_OBJECT(entry), "changed",
                   G_CALLBACK (p_layer_pattern_entry_update_cb),
                   gmop);

  gtk_table_attach (GTK_TABLE(sel_sub_table), entry, 1, 3, subrow, subrow+1
                    , GTK_FILL | GTK_EXPAND, 0, 0, 0);


  subrow++;

  /* the sel_groupname label */
  label = gtk_label_new (_("Groupname:"));
  gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
  gtk_widget_show (label);
  gtk_table_attach (GTK_TABLE(sel_sub_table), label, 0, 1, subrow, subrow+1
                    , 0, 0, 0, 0);


  /* the sel_groupname entry */
  entry = gtk_entry_new();
  gmop->sel_groupname_entry = entry;
  gtk_entry_set_text(GTK_ENTRY(entry), gmop->sel_groupname);
  gimp_help_set_help_data(entry
                         , _("String to identify layer group/subgroup name "
                             "that builds the scope for the layer selection. "
                             "An empty string refers to the image toplevel")
                         , NULL);
  gtk_widget_show(entry);
  g_signal_connect(G_OBJECT(entry), "changed",
                   G_CALLBACK (p_sel_groupname_entry_update_cb),
                   gmop);

  gtk_table_attach (GTK_TABLE(sel_sub_table), entry, 1, 2, subrow, subrow+1
                    , GTK_FILL | GTK_EXPAND, 0, 0, 0);

  /* the delimiter entry */
  entry = gtk_entry_new();
  gmop->delimiter_entry = entry;
  gtk_entry_set_text(GTK_ENTRY(entry), gmop->delimiter);
  gtk_widget_set_size_request(entry, SPIN_BUTTON_WIDTH, -1);
  gimp_help_set_help_data(entry
                         , _("Delimiter string to split layer group/subgroup names.")
                         , NULL);
  gtk_widget_show(entry);
  g_signal_connect(G_OBJECT(entry), "changed",
                   G_CALLBACK (p_delimiter_entry_update_cb),
                   gmop);

  gtk_table_attach (GTK_TABLE(sel_sub_table), entry, 2, 3, subrow, subrow+1
                    , GTK_FILL, 0, 0, 0);





  gtk_table_attach (GTK_TABLE(sel_table), sel_sub_table, 0, 2, row, row+1
                    , GTK_FILL | GTK_EXPAND, 0, 0, 0);




  /*  +++++++++++++++++++++++++  */
  /*  the frame_range     frame  */
  /*  +++++++++++++++++++++++++  */

  frame = gimp_frame_new (_("Frame Range"));
  gtk_box_pack_start (GTK_BOX (main_vbox), frame, FALSE, FALSE, 0);
  gtk_widget_show (frame);

  table = gtk_table_new (2, 3, FALSE);
  range_table = table;
  gtk_container_add (GTK_CONTAINER (frame), table);
  gtk_widget_show (table);

  row = 0;

  /* the from_frame scale entry */
  adj = gimp_scale_entry_new (GTK_TABLE (range_table), 0, row,
                              _("From Frame:"), SCALE_WIDTH, SPIN_BUTTON_WIDTH,
                              (gdouble)gmop->ainfo_ptr->first_frame_nr,
                              (gdouble)gmop->ainfo_ptr->first_frame_nr,  /* lower */
                              (gdouble)gmop->ainfo_ptr->last_frame_nr,   /* upper */
                              1, 10,          /* step, page */
                              0,              /* digits */
                              TRUE,           /* constrain */
                              (gdouble)gmop->ainfo_ptr->first_frame_nr,  /* lower unconstrained */
                              (gdouble)gmop->ainfo_ptr->last_frame_nr,   /* upper unconstrained */
                              _("First handled frame"), NULL);
  gmop->frame_from_adj = adj;
  g_object_set_data(G_OBJECT(adj), "gmop", gmop);
  g_signal_connect (adj, "value_changed",
                    G_CALLBACK (gimp_int_adjustment_update),
                    &gmop->range_from);


  row++;


  /* the to_frame scale entry */
  adj = gimp_scale_entry_new (GTK_TABLE (range_table), 0, row,
                              _("To Frame:"), SCALE_WIDTH, SPIN_BUTTON_WIDTH,
                              (gdouble)gmop->ainfo_ptr->last_frame_nr,
                              (gdouble)gmop->ainfo_ptr->first_frame_nr,  /* lower */
                              (gdouble)gmop->ainfo_ptr->last_frame_nr,   /* upper */
                              1, 10,          /* step, page */
                              0,              /* digits */
                              TRUE,           /* constrain */
                              (gdouble)gmop->ainfo_ptr->first_frame_nr,  /* lower unconstrained */
                              (gdouble)gmop->ainfo_ptr->last_frame_nr,   /* upper unconstrained */
                              _("Last handled frame"), NULL);
  gmop->frame_to_adj = adj;
  g_object_set_data(G_OBJECT(adj), "gmop", gmop);
  g_signal_connect (adj, "value_changed",
                    G_CALLBACK (gimp_int_adjustment_update),
                    &gmop->range_to);

  row++;

  progress_bar = gtk_progress_bar_new ();
  gmop->progress_bar = progress_bar;
  gtk_progress_bar_set_text(GTK_PROGRESS_BAR(progress_bar), " ");
  gtk_widget_show (progress_bar);

  gtk_container_add (GTK_CONTAINER (GTK_DIALOG (dlg)->vbox), progress_bar);


  gtk_widget_show(main_vbox);

}  /* end p_create_mod_frames_dialog */


/* -----------------------------
 * gap_mod_frames_dialog
 * -----------------------------
 */
int
gap_mod_frames_dialog(GapAnimInfo *ainfo_ptr,
                   gint32 *range_from,  gint32 *range_to,
                   gint32 *action_mode, gint32 *sel_mode,
                   gint32 *sel_case,    gint32 *sel_invert,
                   char *sel_pattern,   char   *new_layername,
                   gint32 *new_position,
                   char *new_groupname, char *sel_groupname, char *delimiter)
{
  GapModFramesGlobalParams global_modify_params;
  GapModFramesGlobalParams *gmop;

  if(gap_debug) printf("\nSTART gap_mod_frames_dialog\n");

  gmop = &global_modify_params;
  gimp_ui_init ("gap_mod_frames", FALSE);
  gap_stock_init();

  gmop->run_flag = FALSE;
  gmop->retcode = -1;
  gmop->ainfo_ptr = ainfo_ptr;
  gmop->range_from = gmop->ainfo_ptr->first_frame_nr;
  gmop->range_to = gmop->ainfo_ptr->last_frame_nr;
  gmop->action_mode = 0;
  gmop->sel_mode = 4;   /* pattern is list of layerstack numbers */
  gmop->sel_case = TRUE;
  gmop->sel_invert = FALSE;
  gmop->sel_pattern[0] = '0';
  gmop->sel_pattern[1] = '\0';
  gmop->new_layername[0] = '\0';
  gmop->new_position = 0;
  gmop->new_groupname[0] = '\0';
  gmop->sel_groupname[0] = '\0';
  gmop->delimiter[0] = '/';
  gmop->delimiter[1] = '\0';

  gmop->case_sensitive_check_button = NULL;
  gmop->invert_check_button = NULL;
  gmop->layer_pattern_entry = NULL;
  gmop->new_layername_entry = NULL;
  gmop->new_layername_label = NULL;
  gmop->layer_selection_frame = NULL;

  gmop->new_groupname_label = NULL;
  gmop->new_groupname_entry = NULL;
  gmop->sel_groupname_label = NULL;
  gmop->sel_groupname_entry = NULL;
  gmop->delimiter_label = NULL;
  gmop->delimiter_entry = NULL;
  gmop->new_position_adj = NULL;

  p_create_mod_frames_dialog(gmop);
  p_upd_sensitivity(gmop);
  p_get_values_from_active_layer(gmop);
  gtk_widget_show (gmop->shell);


  if(gap_debug) printf("gap_mod_layer_dialog.c BEFORE  gtk_main\n");
  gtk_main ();
  gdk_flush ();

  if(gap_debug) printf("gap_mod_layer_dialog.c END run_flag: %d\n", (int)gmop->run_flag);


  *range_from   = gmop->range_from;
  *range_to     = gmop->range_to;
  *action_mode  = gmop->action_mode;
  *sel_mode     = gmop->sel_mode;
  *sel_case     = gmop->sel_case;
  *sel_invert   = gmop->sel_invert;
  *new_position = gmop->new_position;

  g_snprintf(sel_pattern, MAX_LAYERNAME, "%s", gmop->sel_pattern);
  g_snprintf(new_layername, MAX_LAYERNAME, "%s", gmop->new_layername);
  g_snprintf(new_groupname, MAX_LAYERNAME, "%s", gmop->new_groupname);
  g_snprintf(sel_groupname, MAX_LAYERNAME, "%s", gmop->sel_groupname);
  g_snprintf(delimiter, 32, "%s", gmop->delimiter);

  if(gmop->run_flag)
  {
    return 0;  /* OK, request to run the modify frames plug-in */
  }
  return -1;  /* for cancel or close dialog without run request */
}  /* end gap_mod_frames_dialog */
