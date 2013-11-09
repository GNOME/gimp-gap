/* gap_onion_dialog.c
 * 2003.05.22 hof (Wolfgang Hofer)
 *
 * GAP ... Gimp Animation Plugins
 *
 * This Module contains GAP Onionskin GUI Procedures
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
 * version 2.1.0a;  2004/06/03   hof: added onionskin setting ref_mode
 * version 2.1.0a;  2004.04.10   hof: callbacks now directly use GapOnionMainGlobalParams *gpp
 *                                    rather than gpointer
 * version 1.3.16c; 2003.07.12   hof: bugfixes, added parameter asc_opacity
 * version 1.3.16b; 2003.07.06   hof: bugfixes, added parameter asc_opacity
 *                                    minor tooltip Helptexts changes
 * version 1.3.14a; 2003.05.24   hof: created
 */

#include "config.h"

/* SYTEM (UNIX) includes */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/* GIMP includes */
#include "gtk/gtk.h"
#include "libgimp/gimp.h"

/* GAP includes */
#include "gap_lib.h"
#include "gap_match.h"
#include "gap_onion_main.h"
#include "gap_onion_dialog.h"
#include "gap_onion_worker.h"

#include "gap-intl.h"


#define ENC_MENU_ITEM_INDEX_KEY "gap_enc_menu_item_index"
#define MAX_SELECT_MODE_ARRAY_ELEMENTS 7
#define MAX_REF_MODE_ARRAY_ELEMENTS 3


extern int gap_debug;

gint  gtab_select_modes[MAX_SELECT_MODE_ARRAY_ELEMENTS] =  { 0, 1, 2, 3, 4, 5, 6 };
gint  gtab_ref_modes[MAX_REF_MODE_ARRAY_ELEMENTS] =
  { GAP_ONION_REFMODE_NORMAL                /* 0 */
  , GAP_ONION_REFMODE_BIDRIECTIONAL_SINGLE  /* 1 */
  , GAP_ONION_REFMODE_BIDRIECTIONAL_DOUBLE  /* 2 */
  };


static void
p_init_main_dialog_widgets(GapOnionMainGlobalParams *gpp);


/* ----------------------------
 * Declare CALLBACK Procedures
 * ----------------------------
 */
static void
on_oni__spinbutton_range_from_changed  (GtkEditable     *editable,
                                        GapOnionMainGlobalParams *gpp);

static void
on_oni__spinbutton_range_to_changed    (GtkEditable     *editable,
                                        GapOnionMainGlobalParams *gpp);

static void
on_oni__spinbutton_num_olayers_changed (GtkEditable     *editable,
                                        GapOnionMainGlobalParams *gpp);

static void
on_oni__spinbutton_ref_delta_changed   (GtkEditable     *editable,
                                        GapOnionMainGlobalParams *gpp);

static void
on_oni__checkbutton_ref_cycle_toggled (GtkToggleButton *togglebutton,
                                       GapOnionMainGlobalParams *gpp);

static void
on_oni__spinbutton_stack_pos_changed   (GtkEditable     *editable,
                                        GapOnionMainGlobalParams *gpp);

static void
on_oni__checkbutton_stack_top_toggled  (GtkToggleButton *togglebutton,
                                        GapOnionMainGlobalParams *gpp);

static void
on_oni__checkbutton_asc_opacity_toggled  (GtkToggleButton *togglebutton,
                                          GapOnionMainGlobalParams *gpp);

static void
on_oni__spinbutton_opacity_changed     (GtkEditable     *editable,
                                        GapOnionMainGlobalParams *gpp);

static void
on_oni__spinbutton_opacity_delta_changed
                                        (GtkEditable     *editable,
                                         GapOnionMainGlobalParams *gpp);

static void
on_oni__spinbutton_ignore_botlayers_changed
                                        (GtkEditable     *editable,
                                         GapOnionMainGlobalParams *gpp);

static void
on_oni__entry_select_string_changed    (GtkEditable     *editable,
                                        GapOnionMainGlobalParams *gpp);

static void
on_oni__checkbutton_select_case_toggled
                                        (GtkToggleButton *togglebutton,
                                         GapOnionMainGlobalParams *gpp);

static void
on_oni__checkbutton_select_invert_toggled
                                        (GtkToggleButton *togglebutton,
                                         GapOnionMainGlobalParams *gpp);

static void
on_oni__button_default_clicked         (GtkButton       *button,
                                        GapOnionMainGlobalParams *gpp);

static void
on_oni__button_help_clicked            (GtkButton       *button,
                                        GapOnionMainGlobalParams *gpp);

static void
on_oni__button_cancel_clicked          (GtkButton       *button,
                                        GapOnionMainGlobalParams *gpp);

static void
on_oni__button_close_clicked          (GtkButton       *button,
                                       GapOnionMainGlobalParams *gpp);

static void
on_oni__button_set_clicked             (GtkButton       *button,
                                        GapOnionMainGlobalParams *gpp);

static void
on_oni__button_delete_clicked          (GtkButton       *button,
                                        GapOnionMainGlobalParams *gpp);

static void
on_oni__button_apply_clicked           (GtkButton       *button,
                                        GapOnionMainGlobalParams *gpp);

static void
on_oni__dialog_destroy                 (GtkObject       *object,
                                        GapOnionMainGlobalParams *gpp);



static void
on_oni__checkbutton_auto_replace_toggled  (GtkToggleButton *togglebutton,
                                        GapOnionMainGlobalParams *gpp);

static void
on_oni__checkbutton_auto_delete_toggled  (GtkToggleButton *togglebutton,
                                        GapOnionMainGlobalParams *gpp);




/* ------------------------------------------------------
 * CALLBACK Procedures Implementation
 * ------------------------------------------------------
 */


static void
p_init_sensitive(GapOnionMainGlobalParams *gpp)
{
  GtkWidget *wgt;
  gboolean  l_sensitive;

  l_sensitive = TRUE;
  if(gpp->vin.select_mode > 3)
  {
    /* insensitive for select modes (4,5,6) that are lists of stacknumbers or all_visible layers */
    l_sensitive = FALSE;
  }
  wgt = gpp->oni__checkbutton_select_case;
  if(wgt)
  {
    gtk_widget_set_sensitive(wgt, l_sensitive);
  }
  wgt = gpp->oni__checkbutton_select_invert;
  if(wgt)
  {
    gtk_widget_set_sensitive(wgt, l_sensitive);
  }

  l_sensitive = TRUE;
  if(gpp->vin.select_mode == 6)
  {
     /* the pattern entry is insensitive if all_visible layers (6) is selected */
     l_sensitive = FALSE;
  }
  wgt = gpp->oni__entry_select_string;
  if(wgt)
  {
    gtk_widget_set_sensitive(wgt, l_sensitive);
  }
}


static void
on_oni__combo_select_mode (GtkWidget     *widget,
                           GapOnionMainGlobalParams *gpp)
{
  gint       l_idx;
  gint       value;

  if(gap_debug) printf("CB: on_oni__combo_select_mode\n");

  if(gpp == NULL) return;

  gimp_int_combo_box_get_active (GIMP_INT_COMBO_BOX (widget), &value);
  l_idx = value;

  if(gap_debug) printf("CB: on_oni__combo_select_mode index: %d\n", (int)l_idx);
  if((l_idx >= MAX_SELECT_MODE_ARRAY_ELEMENTS) || (l_idx < 1))
  {
     l_idx = 0;
  }
  gpp->vin.select_mode = gtab_select_modes[l_idx];
  p_init_sensitive(gpp);
}


static void
on_oni__combo_ref_mode (GtkWidget     *widget,
                        GapOnionMainGlobalParams *gpp)
{
  gint       l_idx;
  gint       value;

  if(gap_debug) printf("CB: on_oni__combo_ref_mode\n");

  if(gpp == NULL) return;

  gimp_int_combo_box_get_active (GIMP_INT_COMBO_BOX (widget), &value);
  l_idx = value;

  if(gap_debug) printf("CB: on_oni__combo_ref_mode index: %d\n", (int)l_idx);
  if((l_idx >= MAX_REF_MODE_ARRAY_ELEMENTS) || (l_idx < 1))
  {
     l_idx = 0;
  }
  gpp->vin.ref_mode = gtab_ref_modes[l_idx];
}

static void
on_oni__spinbutton_range_from_changed  (GtkEditable     *editable,
                                        GapOnionMainGlobalParams *gpp)
{
 ;
  GtkAdjustment *adj;

  if(gap_debug) printf("CB: on_oni__spinbutton_range_from_changed\n");

  if(gpp == NULL) return;
  adj = GTK_ADJUSTMENT(gpp->oni__spinbutton_range_from_adj);

  if(gap_debug) printf("spin value: %f\n", (float)adj->value );

  if((gdouble)adj->value != gpp->range_from)
  {
    gpp->range_from = (gdouble)adj->value;
  }
}


static void
on_oni__spinbutton_range_to_changed    (GtkEditable     *editable,
                                        GapOnionMainGlobalParams *gpp)
{
  GtkAdjustment *adj;

  if(gap_debug) printf("CB: on_oni__spinbutton_range_to_changed\n");

  if(gpp == NULL) return;
  adj = GTK_ADJUSTMENT(gpp->oni__spinbutton_range_to_adj);

  if(gap_debug) printf("spin value: %f\n", (float)adj->value );

  if((gdouble)adj->value != gpp->range_to)
  {
    gpp->range_to = (gdouble)adj->value;
  }
}


static void
on_oni__spinbutton_num_olayers_changed (GtkEditable     *editable,
                                        GapOnionMainGlobalParams *gpp)
{
  GtkAdjustment *adj;

  if(gap_debug) printf("CB: on_oni__spinbutton_num_olayers_changed\n");

  if(gpp == NULL) return;
  adj = GTK_ADJUSTMENT(gpp->oni__spinbutton_num_olayers_adj);

  if(gap_debug) printf("spin value: %f\n", (float)adj->value );

  if((gdouble)adj->value != gpp->vin.num_olayers)
  {
    gpp->vin.num_olayers = (gdouble)adj->value;
  }
}


static void
on_oni__spinbutton_ref_delta_changed   (GtkEditable     *editable,
                                        GapOnionMainGlobalParams *gpp)
{
  GtkAdjustment *adj;

  if(gap_debug) printf("CB: on_oni__spinbutton_ref_delta_changed\n");

  if(gpp == NULL) return;
  adj = GTK_ADJUSTMENT(gpp->oni__spinbutton_ref_delta_adj);

  if(gap_debug) printf("spin value: %f\n", (float)adj->value );

  if((gdouble)adj->value != gpp->vin.ref_delta)
  {
    gpp->vin.ref_delta = (gdouble)adj->value;
  }
}


static void
on_oni__checkbutton_ref_cycle_toggled (GtkToggleButton *togglebutton,
                                       GapOnionMainGlobalParams *gpp)
{
 if(gap_debug) printf("CB: on_oni__checkbutton_ref_cycle_toggled\n");

 if(gpp)
 {
    if (togglebutton->active)
    {
       gpp->vin.ref_cycle = TRUE;
    }
    else
    {
       gpp->vin.ref_cycle = FALSE;
    }
 }
}


static void
on_oni__spinbutton_stack_pos_changed   (GtkEditable     *editable,
                                        GapOnionMainGlobalParams *gpp)
{
  GtkAdjustment *adj;

  if(gap_debug) printf("CB: on_oni__spinbutton_stack_pos_changed\n");

  if(gpp == NULL) return;
  adj = GTK_ADJUSTMENT(gpp->oni__spinbutton_stack_pos_adj);

  if(gap_debug) printf("spin value: %f\n", (float)adj->value );

  if((gdouble)adj->value != gpp->vin.stack_pos)
  {
    gpp->vin.stack_pos = (gdouble)adj->value;
  }
}


static void
on_oni__checkbutton_stack_top_toggled  (GtkToggleButton *togglebutton,
                                        GapOnionMainGlobalParams *gpp)
{
 if(gap_debug) printf("CB: on_oni__checkbutton_stack_top_toggled\n");

 if(gpp)
 {
    if (togglebutton->active)
    {
       gpp->vin.stack_top = TRUE;
    }
    else
    {
       gpp->vin.stack_top = FALSE;
    }
 }
}


static void
on_oni__checkbutton_asc_opacity_toggled  (GtkToggleButton *togglebutton,
                                          GapOnionMainGlobalParams *gpp)
{
 if(gap_debug) printf("CB: on_oni__checkbutton_asc_opacity_toggled\n");

 if(gpp)
 {
    if (togglebutton->active)
    {
       gpp->vin.asc_opacity = TRUE;
    }
    else
    {
       gpp->vin.asc_opacity = FALSE;
    }
 }
}

static void
on_oni__spinbutton_opacity_changed     (GtkEditable     *editable,
                                        GapOnionMainGlobalParams *gpp)
{
  GtkAdjustment *adj;

  if(gap_debug) printf("CB: on_oni__spinbutton_opacity_changed\n");

  if(gpp == NULL) return;

  adj = GTK_ADJUSTMENT(gpp->oni__spinbutton_opacity_adj);

  if(gap_debug) printf("spin value: %f\n", (float)adj->value );

  if((gdouble)adj->value != gpp->vin.opacity)
  {
    gpp->vin.opacity = (gdouble)adj->value;
  }
}


static void
on_oni__spinbutton_opacity_delta_changed
                                        (GtkEditable     *editable,
                                         GapOnionMainGlobalParams *gpp)
{
  GtkAdjustment *adj;

  if(gap_debug) printf("CB: on_oni__spinbutton_opacity_delta_changed\n");

  if(gpp == NULL) return;
  adj = GTK_ADJUSTMENT(gpp->oni__spinbutton_opacity_delta_adj);

  if(gap_debug) printf("spin value: %f\n", (float)adj->value );

  if((gdouble)adj->value != gpp->vin.opacity_delta)
  {
    gpp->vin.opacity_delta = (gdouble)adj->value;
  }
}


static void
on_oni__spinbutton_ignore_botlayers_changed
                                        (GtkEditable     *editable,
                                         GapOnionMainGlobalParams *gpp)
{
  GtkAdjustment *adj;

  if(gap_debug) printf("CB: on_oni__spinbutton_ignore_botlayers_changed\n");

  if(gpp == NULL) return;
  adj = GTK_ADJUSTMENT(gpp->oni__spinbutton_ignore_botlayers_adj);

  if(gap_debug) printf("spin value: %f\n", (float)adj->value );

  if((gdouble)adj->value != gpp->vin.ignore_botlayers)
  {
    gpp->vin.ignore_botlayers = (gdouble)adj->value;
  }
}


static void
on_oni__entry_select_string_changed    (GtkEditable     *editable,
                                        GapOnionMainGlobalParams *gpp)
{
if(gap_debug) printf("CB: on_oni__entry_select_string_changed\n");

 if(gpp)
 {
    g_snprintf(gpp->vin.select_string, sizeof(gpp->vin.select_string), "%s"
              , gtk_entry_get_text(GTK_ENTRY(editable)));
 }
}


static void
on_oni__checkbutton_select_case_toggled
                                        (GtkToggleButton *togglebutton,
                                         GapOnionMainGlobalParams *gpp)
{
 if(gap_debug) printf("CB: on_oni__checkbutton_select_case_toggled\n");

 if(gpp)
 {
    if (togglebutton->active)
    {
       gpp->vin.select_case = TRUE;
    }
    else
    {
       gpp->vin.select_case = FALSE;
    }
 }
}


static void
on_oni__checkbutton_select_invert_toggled
                                        (GtkToggleButton *togglebutton,
                                         GapOnionMainGlobalParams *gpp)
{
 if(gap_debug) printf("CB: on_oni__checkbutton_select_invert_toggled\n");

 if(gpp)
 {
    if (togglebutton->active)
    {
       gpp->vin.select_invert = TRUE;
    }
    else
    {
       gpp->vin.select_invert = FALSE;
    }
 }
}


static void
on_oni__button_default_clicked         (GtkButton       *button,
                                        GapOnionMainGlobalParams *gpp)
{
 if(gap_debug) printf("CB: on_oni__button_default_clicked gpp: %d\n", (int)gpp);

 if(gpp)
 {
   gap_onion_dlg_init_default_values(gpp);
   p_init_main_dialog_widgets(gpp);
 }
}

static void
on_oni__button_help_clicked         (GtkButton       *button,
                                     GapOnionMainGlobalParams *gpp)
{
 if(gap_debug) printf("CB: on_oni__button_help_clicked gpp: %d\n", (int)gpp);

 if(gpp)
 {
   gimp_standard_help_func(GAP_HELP_ID_ONION_CFG, gpp->main_dialog);
 }
}

static void
on_oni__button_cancel_clicked          (GtkButton       *button,
                                        GapOnionMainGlobalParams *gpp)
{
 if(gap_debug) printf("CB: on_oni__button_cancel_clicked\n");

 if(gpp)
 {
   gpp->run = GAP_ONION_RUN_CANCEL;
   gtk_widget_destroy (gpp->main_dialog);
 }
 gtk_main_quit ();
}

static void
on_oni__button_close_clicked             (GtkButton       *button,
                                         GapOnionMainGlobalParams *gpp)
{
 if(gap_debug) printf("CB: on_oni__button_close_clicked\n");

 if(gpp)
 {
   gap_onion_worker_set_data_onion_cfg(gpp, GAP_PLUGIN_NAME_ONION_CFG);
   gpp->run = GAP_ONION_RUN_CANCEL;
   gtk_widget_destroy (gpp->main_dialog);
 }
 gtk_main_quit ();
}

static void
on_oni__button_set_clicked             (GtkButton       *button,
                                        GapOnionMainGlobalParams *gpp)
{
 if(gap_debug) printf("CB: on_oni__button_set_clicked\n");

 if(gpp)
 {
   if(gap_debug) printf("  opacity:%f dlta:%f\n", (float)gpp->vin.opacity, (float)gpp->vin.opacity_delta );

   /* set does not close the dialog and is processed immediate now */
   gap_onion_worker_set_data_onion_cfg(gpp, GAP_PLUGIN_NAME_ONION_CFG);
 }
 /* gtk_main_quit (); */
}


static void
on_oni__button_delete_clicked          (GtkButton       *button,
                                        GapOnionMainGlobalParams *gpp)
{
 if(gap_debug) printf("CB: on_oni__button_delete_clicked\n");

 if(gpp)
 {
   gpp->run = GAP_ONION_RUN_DELETE;
   gtk_widget_destroy (gpp->main_dialog);
 }
 gtk_main_quit ();
}


static void
on_oni__button_apply_clicked           (GtkButton       *button,
                                        GapOnionMainGlobalParams *gpp)
{
 if(gap_debug) printf("CB: on_oni__button_apply_clicked\n");

 if(gpp)
 {
   gap_onion_worker_set_data_onion_cfg(gpp, GAP_PLUGIN_NAME_ONION_CFG);
   gpp->run = GAP_ONION_RUN_APPLY;
   gtk_widget_destroy (gpp->main_dialog);
 }
 gtk_main_quit ();
}


static void
on_oni__dialog_destroy                 (GtkObject       *object,
                                        GapOnionMainGlobalParams *gpp)
{
 if(gap_debug) printf("CB: on_oni__dialog_destroy\n");

 gtk_main_quit ();

}

static void
on_oni__checkbutton_auto_replace_toggled  (GtkToggleButton *togglebutton,
                                        GapOnionMainGlobalParams *gpp)
{
 if(gap_debug) printf("CB: on_oni__checkbutton_auto_replace_toggled\n");

 if(gpp)
 {
    if (togglebutton->active)
    {
       gpp->vin.auto_replace_after_load = TRUE;
    }
    else
    {
       gpp->vin.auto_replace_after_load = FALSE;
    }
 }
}


static void
on_oni__checkbutton_auto_delete_toggled  (GtkToggleButton *togglebutton,
                                        GapOnionMainGlobalParams *gpp)
{
 if(gap_debug) printf("CB: on_oni__checkbutton_auto_delete_toggled\n");

 if(gpp)
 {
    if (togglebutton->active)
    {
       gpp->vin.auto_delete_before_save = TRUE;
    }
    else
    {
       gpp->vin.auto_delete_before_save = FALSE;
    }
 }
}

static void
p_init_combo_actual_idx(GapOnionMainGlobalParams *gpp, GtkWidget *wgt, gint *gtab_ptr, gint val, gint maxidx)
{
  gint l_idx;

  for(l_idx = 0; l_idx < maxidx; l_idx++)
  {
    if(val == gtab_ptr[l_idx])
    {
      gimp_int_combo_box_set_active (GIMP_INT_COMBO_BOX (wgt), l_idx);
      return;
    }
  }
}


static void
p_init_combos(GapOnionMainGlobalParams *gpp)
{
 p_init_combo_actual_idx( gpp
                             , gpp->oni__combo_select_mode
                             , gtab_select_modes
                             , gpp->vin.select_mode
                             , MAX_SELECT_MODE_ARRAY_ELEMENTS
                             );
 p_init_combo_actual_idx( gpp
                             , gpp->oni__combo_ref_mode
                             , gtab_ref_modes
                             , gpp->vin.ref_mode
                             , MAX_REF_MODE_ARRAY_ELEMENTS
                             );
}


static void
p_init_entries(GapOnionMainGlobalParams *gpp)
{
  GtkEntry *entry;

  if(gap_debug) printf("p_init_utl_widgets\n");

  entry = GTK_ENTRY(gpp->oni__entry_select_string);
  gtk_entry_set_text(entry, gpp->vin.select_string);
}


static void
p_init_spinbuttons(GapOnionMainGlobalParams *gpp)
{
  GtkAdjustment *adj;

  if(gap_debug) printf("p_init_spinbuttons\n");

  adj = GTK_ADJUSTMENT(gpp->oni__spinbutton_range_from_adj);
  adj->lower = (gfloat) gpp->ainfo.first_frame_nr;
  adj->upper = (gfloat) gpp->ainfo.last_frame_nr;
  gtk_adjustment_set_value(adj, (gfloat)gpp->range_from);

  adj = GTK_ADJUSTMENT(gpp->oni__spinbutton_range_to_adj);
  adj->lower = (gfloat) gpp->ainfo.first_frame_nr;
  adj->upper = (gfloat) gpp->ainfo.last_frame_nr;
  gtk_adjustment_set_value(adj, (gfloat)gpp->range_to);


  adj = GTK_ADJUSTMENT(gpp->oni__spinbutton_num_olayers_adj);
  gtk_adjustment_set_value(adj, (gfloat)gpp->vin.num_olayers);

  adj = GTK_ADJUSTMENT(gpp->oni__spinbutton_ref_delta_adj);
  gtk_adjustment_set_value(adj, (gfloat)gpp->vin.ref_delta);

  adj = GTK_ADJUSTMENT(gpp->oni__spinbutton_stack_pos_adj);
  gtk_adjustment_set_value(adj, (gfloat)gpp->vin.stack_pos);

  adj = GTK_ADJUSTMENT(gpp->oni__spinbutton_opacity_adj);
  gtk_adjustment_set_value(adj, (gfloat)gpp->vin.opacity);

  adj = GTK_ADJUSTMENT(gpp->oni__spinbutton_opacity_delta_adj);
  gtk_adjustment_set_value(adj, (gfloat)gpp->vin.opacity_delta);

  adj = GTK_ADJUSTMENT(gpp->oni__spinbutton_ignore_botlayers_adj);
  gtk_adjustment_set_value(adj, (gfloat)gpp->vin.ignore_botlayers);
}

static void
p_init_togglebuttons(GapOnionMainGlobalParams *gpp)
{
  GtkWidget *wgt;

  wgt = gpp->oni__checkbutton_ref_cycle;
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (wgt), gpp->vin.ref_cycle);

  wgt = gpp->oni__checkbutton_stack_top;
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (wgt), gpp->vin.stack_top);

  wgt = gpp->oni__checkbutton_asc_opacity;
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (wgt), gpp->vin.asc_opacity);

  wgt = gpp->oni__checkbutton_select_case;
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (wgt), gpp->vin.select_case);

  wgt = gpp->oni__checkbutton_select_invert;
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (wgt), gpp->vin.select_invert);


  wgt = gpp->oni__checkbutton_auto_replace;
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (wgt), gpp->vin.auto_replace_after_load);

  wgt = gpp->oni__checkbutton_auto_delete;
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (wgt), gpp->vin.auto_delete_before_save);
}

static void
p_init_main_dialog_widgets(GapOnionMainGlobalParams *gpp)
{
  if(gap_debug) printf("p_init_main_dialog_widgets: Start INIT\n");

  /* put initial values to the widgets */

  p_init_spinbuttons(gpp);
  p_init_combos(gpp);
  p_init_entries(gpp);
  p_init_togglebuttons(gpp);
  p_init_sensitive(gpp);

}       /* end p_init_main_dialog_widgets */


/* ----------------------------
 * create_oni__dialog
 * ----------------------------
 */
static GtkWidget*
create_oni__dialog (GapOnionMainGlobalParams *gpp)
{
  GtkWidget *oni__dialog;
  GtkWidget *dialog_vbox1;
  GtkWidget *frame1;
  GtkWidget *frame2;
  GtkWidget *frame3;

  GtkWidget *hbox1;
  GtkWidget *hbox2;

  GtkWidget *table1;
  GtkWidget *table2;
  GtkWidget *table3;

  GtkWidget *label;
  GtkWidget *label2;
  GtkWidget *label3;
  GtkWidget *label4;
  GtkWidget *label5;
  GtkWidget *label6;
  GtkWidget *label7;
  GtkWidget *label8;
  GtkWidget *label9;
  GtkWidget *label10;

  GtkWidget *oni__combo_ref_mode;
  GtkWidget *oni__combo_select_mode;

  GtkWidget *oni__spinbutton_range_from;
  GtkWidget *oni__spinbutton_range_to;
  GtkWidget *oni__spinbutton_num_olayers;
  GtkWidget *oni__spinbutton_ref_delta;
  GtkWidget *oni__checkbutton_ref_cycle;
  GtkWidget *oni__spinbutton_stack_pos;
  GtkWidget *oni__checkbutton_stack_top;
  GtkWidget *oni__checkbutton_asc_opacity;
  GtkWidget *oni__spinbutton_opacity;
  GtkWidget *oni__spinbutton_opacity_delta;
  GtkWidget *oni__spinbutton_ignore_botlayers;
  GtkWidget *oni__checkbutton_select_case;
  GtkWidget *oni__checkbutton_select_invert;
  GtkWidget *oni__entry_select_string;

  GtkWidget *oni__checkbutton_auto_replace;
  GtkWidget *oni__checkbutton_auto_delete;


  GtkWidget *oni__button_default;
  GtkWidget *dialog_action_area1;
  GtkWidget *oni__button_cancel;
  GtkWidget *oni__button_set;
  GtkWidget *oni__button_close;
  GtkWidget *oni__button_delete;
  GtkWidget *oni__button_apply;
  GtkWidget *oni__button_help = NULL;

  gint tab1_row;



  oni__dialog = gtk_dialog_new ();
  gtk_container_set_border_width (GTK_CONTAINER (oni__dialog), 12);
  gtk_window_set_title (GTK_WINDOW (oni__dialog), _("Onionskin Configuration"));

  dialog_vbox1 = GTK_DIALOG (oni__dialog)->vbox;
  gtk_widget_show (dialog_vbox1);

  frame3 = gimp_frame_new (_("Frame Range"));
  gtk_widget_show (frame3);

  table3 = gtk_table_new (2, 2, FALSE);
  gtk_widget_show (table3);
  gtk_container_add (GTK_CONTAINER (frame3), table3);
  gtk_container_set_border_width (GTK_CONTAINER (table3), 4);
  gtk_table_set_row_spacings (GTK_TABLE (table3), 2);
  gtk_table_set_col_spacings (GTK_TABLE (table3), 2);

  label9 = gtk_label_new (_("From Frame:"));
  gtk_widget_show (label9);
  gtk_table_attach (GTK_TABLE (table3), label9, 0, 1, 0, 1,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);
  gtk_misc_set_alignment (GTK_MISC (label9), 0, 0.5);

  label10 = gtk_label_new (_("To Frame:"));
  gtk_widget_show (label10);
  gtk_table_attach (GTK_TABLE (table3), label10, 0, 1, 1, 2,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);
  gtk_misc_set_alignment (GTK_MISC (label10), 0, 0.5);

  oni__spinbutton_range_from = gimp_spin_button_new (&gpp->oni__spinbutton_range_from_adj,  /* return value (the adjustment) */
                      1,     /* initial_val */
                      0.0,                  /* umin */
                      99999.0,              /* umax */
                      1.0,                  /* sstep */
                      10.0,                /* pagestep */
                      0.0,                 /* page_size */
                      1.0,                 /* climb_rate */
                      0                    /* digits */
                      );
  gimp_help_set_help_data(oni__spinbutton_range_from
                         , _("First handled frame")
                         , NULL);
  gtk_widget_show (oni__spinbutton_range_from);
  gtk_table_attach (GTK_TABLE (table3), oni__spinbutton_range_from, 1, 2, 0, 1,
                    (GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
                    (GtkAttachOptions) (0), 2, 0);

  oni__spinbutton_range_to = gimp_spin_button_new (&gpp->oni__spinbutton_range_to_adj,  /* return value (the adjustment) */
                      1,     /* initial_val */
                      0.0,                  /* umin */
                      99999.0,              /* umax */
                      1.0,                  /* sstep */
                      10.0,                /* pagestep */
                      0.0,                 /* page_size */
                      1.0,                 /* climb_rate */
                      0                    /* digits */
                      );
  gimp_help_set_help_data(oni__spinbutton_range_to
                         , _("Last handled frame")
                         , NULL);
  gtk_widget_show (oni__spinbutton_range_to);
  gtk_table_attach (GTK_TABLE (table3), oni__spinbutton_range_to, 1, 2, 1, 2,
                    (GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
                    (GtkAttachOptions) (0), 2, 0);

  frame1 = gimp_frame_new (_("Onionskin Settings"));
  gtk_widget_show (frame1);
  gtk_box_pack_start (GTK_BOX (dialog_vbox1), frame1, TRUE, TRUE, 0);


  gtk_box_pack_start (GTK_BOX (dialog_vbox1), frame3, TRUE, TRUE, 0);


  table1 = gtk_table_new (8, 3, FALSE);
  gtk_widget_show (table1);
  gtk_container_add (GTK_CONTAINER (frame1), table1);
  gtk_container_set_border_width (GTK_CONTAINER (table1), 4);
  gtk_table_set_row_spacings (GTK_TABLE (table1), 2);
  gtk_table_set_col_spacings (GTK_TABLE (table1), 2);

  tab1_row = 0;

  label = gtk_label_new (_("Reference Mode:"));
  gtk_widget_show (label);
  gtk_table_attach (GTK_TABLE (table1), label, 0, 1, tab1_row, tab1_row+1,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);
  gtk_misc_set_alignment (GTK_MISC (label), 0, 0.5);



  /* the ref_mode combo box */
  oni__combo_ref_mode
    = gimp_int_combo_box_new (_("Normal +1,+2,+3,+4,+5,+6"),                   0,
                              _("Bidiriectional (single) +1,-2,+3,-4,+5,-6"),  1,
                              _("Bidiriectional (double) +1,-1,+2,-2,+3,-3"),  2,
                              NULL);
  gimp_int_combo_box_connect (GIMP_INT_COMBO_BOX (oni__combo_ref_mode),
                                 0,  /* initial gint value */
                                 G_CALLBACK (on_oni__combo_ref_mode),
                                 gpp);

  gtk_widget_show (oni__combo_ref_mode);
  gtk_table_attach (GTK_TABLE (table1), oni__combo_ref_mode, 1, 3, tab1_row, tab1_row+1,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);
  gimp_help_set_help_data(oni__combo_ref_mode
                         , _("Reference Modes determines stepsequence:\n"
                             " normal: +1,+2,+3,+4,+5,+6\n"
                             " bidirectional single: +1,-2,+3,-4,+5,-6\n"
                             " bidirectional double: +1,-1,+2,-2,+3,-3\n"
                             " (stepsize is set by frame reference parameter)")
                         , NULL);


  tab1_row++;
  label = gtk_label_new (_("Onionskin Layers:"));
  gtk_widget_show (label);
  gtk_table_attach (GTK_TABLE (table1), label, 0, 1, tab1_row, tab1_row+1,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);
  gtk_misc_set_alignment (GTK_MISC (label), 0, 0.5);




  oni__spinbutton_num_olayers = gimp_spin_button_new (&gpp->oni__spinbutton_num_olayers_adj,  /* return value (the adjustment) */
                      2,     /* initial_val */
                      1.0,                  /* umin */
                      20.0,                 /* umax */
                      1.0,                  /* sstep */
                      10.0,                 /* pagestep */
                      0.0,                 /* page_size */
                      1.0,                 /* climb_rate */
                      0                    /* digits */
                      );
  gtk_widget_show (oni__spinbutton_num_olayers);
  gtk_table_attach (GTK_TABLE (table1), oni__spinbutton_num_olayers, 1, 2, tab1_row, tab1_row+1,
                    (GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);
  gimp_help_set_help_data(oni__spinbutton_num_olayers
                         , _("Number of onionskin layers to create in the handled frame.")
                         , NULL);



  oni__checkbutton_asc_opacity = gtk_check_button_new_with_label (_("Ascending Opacity"));
  gtk_widget_show (oni__checkbutton_asc_opacity);
  gtk_table_attach (GTK_TABLE (table1), oni__checkbutton_asc_opacity, 2, 3, tab1_row, tab1_row+1,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);
  gimp_help_set_help_data(oni__checkbutton_asc_opacity
                         , _("ON: Far neighbour frames have the higher opacity.\n"
                             "OFF: Near neighbour frames have the higher opacity.")
                         , NULL);

  tab1_row++;

  label2 = gtk_label_new (_("Frame Reference:"));
  gtk_widget_show (label2);
  gtk_table_attach (GTK_TABLE (table1), label2, 0, 1, tab1_row, tab1_row+1,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);
  gtk_misc_set_alignment (GTK_MISC (label2), 0, 0.5);

  oni__spinbutton_ref_delta = gimp_spin_button_new (&gpp->oni__spinbutton_ref_delta_adj,  /* return value (the adjustment) */
                      -1,     /* initial_val */
                      -100.0,               /* umin */
                      100.0,                 /* umax */
                      1.0,                  /* sstep */
                      10.0,                 /* pagestep */
                      0.0,                 /* page_size */
                      1.0,                 /* climb_rate */
                      0                    /* digits */
                      );
  gtk_widget_show (oni__spinbutton_ref_delta);
  gtk_table_attach (GTK_TABLE (table1), oni__spinbutton_ref_delta, 1, 2, tab1_row, tab1_row+1,
                    (GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);
  gimp_help_set_help_data(oni__spinbutton_ref_delta
                         , _("reference stepsize where -1 is previous frame, +1 is next frame."
                             " (stepsequence is built together with reference mode parameter)")
                         , NULL);

  oni__checkbutton_ref_cycle = gtk_check_button_new_with_label (_("Cyclic"));
  gtk_widget_show (oni__checkbutton_ref_cycle);
  gtk_table_attach (GTK_TABLE (table1), oni__checkbutton_ref_cycle, 2, 3, tab1_row, tab1_row+1,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);
  gimp_help_set_help_data(oni__checkbutton_ref_cycle
                         , _("ON: Next frame of last is first and vice versa.")
                         , NULL);

  tab1_row++;

  label3 = gtk_label_new (_("Stackposition:"));
  gtk_widget_show (label3);
  gtk_table_attach (GTK_TABLE (table1), label3, 0, 1, tab1_row, tab1_row+1,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);
  gtk_misc_set_alignment (GTK_MISC (label3), 0, 0.5);

  oni__spinbutton_stack_pos = gimp_spin_button_new (&gpp->oni__spinbutton_stack_pos_adj,  /* return value (the adjustment) */
                      1,     /* initial_val */
                      0.0,                   /* umin */
                      500.0,                 /* umax */
                      1.0,                  /* sstep */
                      10.0,                 /* pagestep */
                      0.0,                 /* page_size */
                      1.0,                 /* climb_rate */
                      0                    /* digits */
                      );
  gtk_widget_show (oni__spinbutton_stack_pos);
  gtk_table_attach (GTK_TABLE (table1), oni__spinbutton_stack_pos, 1, 2, tab1_row, tab1_row+1,
                    (GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);
  gimp_help_set_help_data(oni__spinbutton_stack_pos
                         , _("Stackposition where to place onionskin layer(s)")
                         , NULL);

  oni__checkbutton_stack_top = gtk_check_button_new_with_label (_("From Top"));
  gtk_widget_show (oni__checkbutton_stack_top);
  gtk_table_attach (GTK_TABLE (table1), oni__checkbutton_stack_top, 2, 3, tab1_row, tab1_row+1,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);
  gimp_help_set_help_data(oni__checkbutton_stack_top
                         , _("ON: 0 is top of stack (in front).\n"
                             "OFF: 0 is bottom of stack (in background).")
                         , NULL);
  tab1_row++;

  label4 = gtk_label_new (_("Opacity:"));
  gtk_widget_show (label4);
  gtk_table_attach (GTK_TABLE (table1), label4, 0, 1, tab1_row, tab1_row+1,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);
  gtk_misc_set_alignment (GTK_MISC (label4), 0, 0.5);

  oni__spinbutton_opacity = gimp_spin_button_new (&gpp->oni__spinbutton_opacity_adj,  /* return value (the adjustment) */
                      80,     /* initial_val */
                      0.0,                   /* umin */
                      100.0,                 /* umax */
                      1.0,                  /* sstep */
                      10.0,                 /* pagestep */
                      0.0,                 /* page_size */
                      1.0,                 /* climb_rate */
                      1                    /* digits */
                      );
  gtk_widget_show (oni__spinbutton_opacity);
  gtk_table_attach (GTK_TABLE (table1), oni__spinbutton_opacity, 1, 2, tab1_row, tab1_row+1,
                    (GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);
  gimp_help_set_help_data(oni__spinbutton_opacity
                         , _("Opacity of first onionskin layer (0 is transparent, 100 full opaque)")
                         , NULL);

  oni__spinbutton_opacity_delta = gimp_spin_button_new (&gpp->oni__spinbutton_opacity_delta_adj,  /* return value (the adjustment) */
                      80,     /* initial_val */
                      0.0,                   /* umin */
                      100.0,                 /* umax */
                      1.0,                  /* sstep */
                      10.0,                 /* pagestep */
                      0.0,                 /* page_size */
                      1.0,                 /* climb_rate */
                      1                    /* digits */
                      );
  gtk_widget_show (oni__spinbutton_opacity_delta);
  gtk_table_attach (GTK_TABLE (table1), oni__spinbutton_opacity_delta, 2, 3, tab1_row, tab1_row+1,
                    (GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);
  gimp_help_set_help_data(oni__spinbutton_opacity_delta
                         , _("Descending opacity for 2.nd onionskin layer")
                         , NULL);

  tab1_row++;


  frame2 = gimp_frame_new (_("Layer Selection"));
  gtk_widget_show (frame2);
  gtk_table_attach (GTK_TABLE (table1), frame2, 0, 3, tab1_row, tab1_row+1,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (GTK_EXPAND | GTK_FILL), 0, 0);

  table2 = gtk_table_new (5, 2, FALSE);
  gtk_widget_show (table2);
  gtk_container_add (GTK_CONTAINER (frame2), table2);
  gtk_container_set_border_width (GTK_CONTAINER (table2), 4);

  label5 = gtk_label_new (_("Ignore BG-layer(s):"));
  gtk_widget_show (label5);
  gtk_table_attach (GTK_TABLE (table2), label5, 0, 1, 0, 1,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);
  gtk_misc_set_alignment (GTK_MISC (label5), 0, 0.5);

  oni__spinbutton_ignore_botlayers = gimp_spin_button_new (&gpp->oni__spinbutton_ignore_botlayers_adj,  /* return value (the adjustment) */
                      1,     /* initial_val */
                      0.0,                   /* umin */
                      100.0,                 /* umax */
                      1.0,                  /* sstep */
                      10.0,                 /* pagestep */
                      0.0,                 /* page_size */
                      1.0,                 /* climb_rate */
                      0                    /* digits */
                      );
  gtk_widget_show (oni__spinbutton_ignore_botlayers);
  gtk_table_attach (GTK_TABLE (table2), oni__spinbutton_ignore_botlayers, 1, 2, 0, 1,
                    (GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);
  gimp_help_set_help_data(oni__spinbutton_ignore_botlayers
                         , _("Exclude N background layers. Use 0 if you dont want to exclude any layer.")
                         , NULL);

  label7 = gtk_label_new (_("Select Mode:"));
  gtk_widget_show (label7);
  gtk_table_attach (GTK_TABLE (table2), label7, 0, 1, 2, 3,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);
  gtk_misc_set_alignment (GTK_MISC (label7), 0, 0.5);



  /* the ref_mode combo box */
  oni__combo_select_mode
    = gimp_int_combo_box_new (_("Pattern is equal to layername"),                   0,
                              _("Pattern is start of layername"),                   1,
                              _("Pattern is end of layername"),                     2,
                              _("Pattern is a part of layername"),                  3,
                              _("Pattern is a list of layerstack numbers"),         4,
                              _("Pattern is a list of reverse layerstack numbers"), 5,
                              _("All visible (ignore pattern)"),                    6,
                              NULL);
  gimp_int_combo_box_connect (GIMP_INT_COMBO_BOX (oni__combo_select_mode),
                                 0,  /* initial gint value */
                                 G_CALLBACK (on_oni__combo_select_mode),
                                 gpp);

  gtk_widget_show (oni__combo_select_mode);
  gtk_table_attach (GTK_TABLE (table2), oni__combo_select_mode, 1, 2, 2, 3,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);
  gimp_help_set_help_data(oni__combo_select_mode
                         , _("This mode determines how the selection pattern is interpreted.")
                         , NULL);




  /* the selct options label */
  label8 = gtk_label_new (_("Select Options:"));
  gtk_widget_show (label8);
  gtk_table_attach (GTK_TABLE (table2), label8, 0, 1, 3, 4,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);
  gtk_misc_set_alignment (GTK_MISC (label8), 0, 0.5);

  hbox2 = gtk_hbox_new (TRUE, 0);
  gtk_widget_show (hbox2);
  gtk_table_attach (GTK_TABLE (table2), hbox2, 1, 2, 3, 4,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (GTK_FILL), 0, 0);

  oni__checkbutton_select_case = gtk_check_button_new_with_label (_("Case sensitive"));
  gtk_widget_show (oni__checkbutton_select_case);
  gtk_box_pack_start (GTK_BOX (hbox2), oni__checkbutton_select_case, FALSE, TRUE, 0);
  gimp_help_set_help_data(oni__checkbutton_select_case
                         , _("ON: Case sensitive pattern.\n"
                             "OFF: Ignore case.")
                         , NULL);

  oni__checkbutton_select_invert = gtk_check_button_new_with_label (_("Invert Selection"));
  gtk_widget_show (oni__checkbutton_select_invert);
  gtk_box_pack_start (GTK_BOX (hbox2), oni__checkbutton_select_invert, FALSE, TRUE, 0);
  gimp_help_set_help_data(oni__checkbutton_select_invert
                         , _("ON: Select non-matching layers.\n"
                             "OFF: Select matching layers")
                         , NULL);

  label6 = gtk_label_new (_("Select Pattern:"));
  gtk_widget_show (label6);
  gtk_table_attach (GTK_TABLE (table2), label6, 0, 1, 4, 5,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);
  gtk_misc_set_alignment (GTK_MISC (label6), 0, 0.5);

  oni__entry_select_string = gtk_entry_new ();
  gtk_widget_show (oni__entry_select_string);
  gtk_table_attach (GTK_TABLE (table2), oni__entry_select_string, 1, 2, 4, 5,
                    (GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);
  gimp_help_set_help_data(oni__entry_select_string
                         , _("Select layernames by pattern (depends on mode and options)")
                         , NULL);


  tab1_row++;

  {
     char *lbl_txt;

     lbl_txt = g_strdup_printf(_("Set for: %s"), &gpp->ainfo.basename[0]);

     oni__button_set = gtk_button_new_with_label (lbl_txt);
     g_free(lbl_txt);
  }
  gtk_widget_show (oni__button_set);
  gtk_table_attach (GTK_TABLE (table1), oni__button_set, 0, 3, tab1_row, tab1_row+1,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 2, 0);
  gimp_help_set_help_data(oni__button_set
                         , _("Set onionskin parameters for the current video")
                         , NULL);

  tab1_row++;

  {
     GtkWidget *auto_table;

     auto_table = gtk_table_new (1, 2, TRUE);
     gtk_widget_show (auto_table);
     gtk_table_attach (GTK_TABLE (table1), auto_table, 0, 3, tab1_row, tab1_row+1,
                      (GtkAttachOptions) (GTK_FILL),
                      (GtkAttachOptions) (0), 2, 0);

     oni__checkbutton_auto_replace = gtk_check_button_new_with_label (_("Auto create after load"));
     gtk_widget_show (oni__checkbutton_auto_replace);
     gimp_help_set_help_data(oni__checkbutton_auto_replace
                         , _("ON: Automatic creation/replacement of onionskin layer(s). "
                             "Works on frame changes via 'VCR Navigator' and go to operations "
                             "in the video menu -- but not on explicite load from the file menu.")
                         , NULL);
     gtk_table_attach (GTK_TABLE (auto_table), oni__checkbutton_auto_replace,
                       0, 1, 0, 1,
                       (GtkAttachOptions) (GTK_FILL),
                       (GtkAttachOptions) (GTK_FILL), 0, 0);

     oni__checkbutton_auto_delete = gtk_check_button_new_with_label (_("Auto delete before save"));
     gtk_widget_show (oni__checkbutton_auto_delete);
     gimp_help_set_help_data(oni__checkbutton_auto_delete
                         , _("ON: Automatic delete of onionskin layer(s). "
                             "Works on framechanges via 'VCR Navigator' and go to operations "
                             "in the video menu -- but not on explicite save from the file menu. "
                             "Use this option if you dont want onionskin layers to appear in thumbnail files.")
                         , NULL);
     gtk_table_attach (GTK_TABLE (auto_table), oni__checkbutton_auto_delete,
                       1, 2, 0, 1,
                       (GtkAttachOptions) (GTK_FILL),
                       (GtkAttachOptions) (GTK_FILL), 0, 0);
  }


  dialog_action_area1 = GTK_DIALOG (oni__dialog)->action_area;
  gtk_widget_show (dialog_action_area1);
  gtk_container_set_border_width (GTK_CONTAINER (dialog_action_area1), 10);

  /*  hbox1 = gtk_hbox_new (FALSE, 0); */
  hbox1 = gtk_hbutton_box_new();
  gtk_widget_show (hbox1);
  gtk_box_pack_end (GTK_BOX (dialog_action_area1), hbox1, FALSE, TRUE, 0);


  if (gimp_show_help_button ())
    {
      oni__button_help = gtk_button_new_from_stock (GTK_STOCK_HELP);
      gtk_widget_show (oni__button_help);
      gtk_box_pack_end (GTK_BOX (hbox1), oni__button_help, FALSE, FALSE, 2);
      gimp_help_set_help_data(oni__button_help
                              , _("Show help page")
                              , NULL);
    }

  oni__button_default = gtk_button_new_from_stock (GIMP_STOCK_RESET);
  gtk_widget_show (oni__button_default);
  gtk_box_pack_end (GTK_BOX (hbox1), oni__button_default, FALSE, FALSE, 2);
  gimp_help_set_help_data(oni__button_default
                         , _("Reset to default settings")
                         , NULL);

  oni__button_cancel = gtk_button_new_from_stock (GTK_STOCK_CANCEL);
  gtk_widget_show (oni__button_cancel);
  gtk_box_pack_end (GTK_BOX (hbox1), oni__button_cancel, FALSE, FALSE, 2);
  gimp_help_set_help_data(oni__button_cancel
                         , _("Close window without any action")
                         , NULL);

  oni__button_close = gtk_button_new_from_stock (GTK_STOCK_CLOSE);
  gtk_widget_show (oni__button_close);
  gtk_box_pack_end (GTK_BOX (hbox1), oni__button_close, FALSE, FALSE, 2);
  gimp_help_set_help_data(oni__button_close
                         , _("Close window without creating or deleting any onionskin layers\n"
                             "but store current Settings")
                         , NULL);

  oni__button_delete = gtk_button_new_from_stock (GTK_STOCK_DELETE);
  gtk_widget_show (oni__button_delete);
  gtk_box_pack_end (GTK_BOX (hbox1), oni__button_delete, FALSE, FALSE, 2);
  gimp_help_set_help_data(oni__button_delete
                         , _("Remove all onionskin layers in all frames of the the selected frame range")
                         , NULL);


  oni__button_apply = gtk_button_new_from_stock (GTK_STOCK_OK);
  gtk_widget_set_size_request (oni__button_apply, 100, -1);
  gtk_widget_show (oni__button_apply);
  gtk_box_pack_end (GTK_BOX (hbox1), oni__button_apply, FALSE, FALSE, 2);
  gimp_help_set_help_data(oni__button_apply
                         , _("Create or replace onionskin layer(s) in all frames of the selected frame range")
                         , NULL);


  g_signal_connect (G_OBJECT (oni__dialog), "destroy",
                      G_CALLBACK (on_oni__dialog_destroy),
                      gpp);
  g_signal_connect (G_OBJECT (gpp->oni__spinbutton_range_from_adj), "value_changed",
                      G_CALLBACK (on_oni__spinbutton_range_from_changed),
                      gpp);
  g_signal_connect (G_OBJECT (gpp->oni__spinbutton_range_to_adj), "value_changed",
                      G_CALLBACK (on_oni__spinbutton_range_to_changed),
                      gpp);
  g_signal_connect (G_OBJECT (gpp->oni__spinbutton_num_olayers_adj), "value_changed",
                      G_CALLBACK (on_oni__spinbutton_num_olayers_changed),
                      gpp);
  g_signal_connect (G_OBJECT (gpp->oni__spinbutton_ref_delta_adj), "value_changed",
                      G_CALLBACK (on_oni__spinbutton_ref_delta_changed),
                      gpp);
  g_signal_connect (G_OBJECT (oni__checkbutton_ref_cycle), "toggled",
                      G_CALLBACK (on_oni__checkbutton_ref_cycle_toggled),
                      gpp);
  g_signal_connect (G_OBJECT (gpp->oni__spinbutton_stack_pos_adj), "value_changed",
                      G_CALLBACK (on_oni__spinbutton_stack_pos_changed),
                      gpp);
  g_signal_connect (G_OBJECT (oni__checkbutton_stack_top), "toggled",
                      G_CALLBACK (on_oni__checkbutton_stack_top_toggled),
                      gpp);
  g_signal_connect (G_OBJECT (oni__checkbutton_asc_opacity), "toggled",
                      G_CALLBACK (on_oni__checkbutton_asc_opacity_toggled),
                      gpp);
  g_signal_connect (G_OBJECT (gpp->oni__spinbutton_opacity_adj), "value_changed",
                      G_CALLBACK (on_oni__spinbutton_opacity_changed),
                      gpp);
  g_signal_connect (G_OBJECT (gpp->oni__spinbutton_opacity_delta_adj), "value_changed",
                      G_CALLBACK (on_oni__spinbutton_opacity_delta_changed),
                      gpp);
  g_signal_connect (G_OBJECT (gpp->oni__spinbutton_ignore_botlayers_adj), "value_changed",
                      G_CALLBACK (on_oni__spinbutton_ignore_botlayers_changed),
                      gpp);
  g_signal_connect (G_OBJECT (oni__checkbutton_select_case), "toggled",
                      G_CALLBACK (on_oni__checkbutton_select_case_toggled),
                      gpp);
  g_signal_connect (G_OBJECT (oni__checkbutton_select_invert), "toggled",
                      G_CALLBACK (on_oni__checkbutton_select_invert_toggled),
                      gpp);
  g_signal_connect (G_OBJECT (oni__entry_select_string), "changed",
                      G_CALLBACK (on_oni__entry_select_string_changed),
                      gpp);

  g_signal_connect (G_OBJECT (oni__checkbutton_auto_replace), "toggled",
                      G_CALLBACK (on_oni__checkbutton_auto_replace_toggled),
                      gpp);
  g_signal_connect (G_OBJECT (oni__checkbutton_auto_delete), "toggled",
                      G_CALLBACK (on_oni__checkbutton_auto_delete_toggled),
                      gpp);

  if (oni__button_help)
    g_signal_connect (G_OBJECT (oni__button_help), "clicked",
                      G_CALLBACK (on_oni__button_help_clicked),
                      gpp);
  g_signal_connect (G_OBJECT (oni__button_default), "clicked",
                      G_CALLBACK (on_oni__button_default_clicked),
                      gpp);
  g_signal_connect (G_OBJECT (oni__button_cancel), "clicked",
                      G_CALLBACK (on_oni__button_cancel_clicked),
                      gpp);
  g_signal_connect (G_OBJECT (oni__button_set), "clicked",
                      G_CALLBACK (on_oni__button_set_clicked),
                      gpp);
  g_signal_connect (G_OBJECT (oni__button_close), "clicked",
                      G_CALLBACK (on_oni__button_close_clicked),
                      gpp);
  g_signal_connect (G_OBJECT (oni__button_delete), "clicked",
                      G_CALLBACK (on_oni__button_delete_clicked),
                      gpp);
  g_signal_connect (G_OBJECT (oni__button_apply), "clicked",
                      G_CALLBACK (on_oni__button_apply_clicked),
                      gpp);



  /* copy widget pointers to global parameter
   * (for use in callbacks outside of this procedure)
   */
  gpp->oni__entry_select_string         = oni__entry_select_string;
  gpp->oni__combo_ref_mode              = oni__combo_ref_mode;
  gpp->oni__combo_select_mode           = oni__combo_select_mode;
  gpp->oni__spinbutton_ignore_botlayers = oni__spinbutton_ignore_botlayers;
  gpp->oni__spinbutton_num_olayers      = oni__spinbutton_num_olayers;
  gpp->oni__spinbutton_opacity          = oni__spinbutton_opacity;
  gpp->oni__spinbutton_opacity_delta    = oni__spinbutton_opacity_delta;
  gpp->oni__spinbutton_range_from       = oni__spinbutton_range_from;
  gpp->oni__spinbutton_range_to         = oni__spinbutton_range_to;
  gpp->oni__spinbutton_ref_delta        = oni__spinbutton_ref_delta;
  gpp->oni__spinbutton_stack_pos        = oni__spinbutton_stack_pos;
  gpp->oni__checkbutton_ref_cycle       = oni__checkbutton_ref_cycle;
  gpp->oni__checkbutton_select_case     = oni__checkbutton_select_case;
  gpp->oni__checkbutton_select_invert   = oni__checkbutton_select_invert;
  gpp->oni__checkbutton_stack_top       = oni__checkbutton_stack_top;
  gpp->oni__checkbutton_asc_opacity     = oni__checkbutton_asc_opacity;
  gpp->oni__checkbutton_auto_replace    = oni__checkbutton_auto_replace;
  gpp->oni__checkbutton_auto_delete     = oni__checkbutton_auto_delete;

  return oni__dialog;
}  /* end create_oni__dialog */


/* ============================================================================
 * gap_onion_dlg_init_default_values
 *    Set onion config default params.
 * ============================================================================
 */
void
gap_onion_dlg_init_default_values(GapOnionMainGlobalParams *gpp)
{
          gpp->vin.onionskin_auto_enable = TRUE;
          gpp->vin.auto_replace_after_load = FALSE;
          gpp->vin.auto_delete_before_save = FALSE;

          gpp->vin.num_olayers        = 2;
          gpp->vin.ref_mode           = gtab_ref_modes[0]; /* normal ref mode */
          gpp->vin.ref_delta          = -1;
          gpp->vin.ref_cycle          = FALSE;
          gpp->vin.stack_pos          = 1;
          gpp->vin.stack_top          = FALSE;
          gpp->vin.asc_opacity        = FALSE;
          gpp->vin.opacity            = 80.0;
          gpp->vin.opacity_delta      = 80.0;
          gpp->vin.ignore_botlayers   = 1;
          gpp->vin.select_mode        = GAP_MTCH_ALL_VISIBLE;  /* GAP_MTCH_ANYWHERE, GAP_MTCH_ALL_VISIBLE */
          gpp->vin.select_case        = 0;     /* 0 .. ignore case, 1..case sensitve */
          gpp->vin.select_invert      = 0;     /* 0 .. no invert, 1 ..invert */
          gpp->vin.select_string[0] = '\0';
/*
          g_snprintf(&gpp->vin.select_string[0]
                    , sizeof(gpp->vin.select_string)
                    ,"pasted"
                    );
*/
}       /* end gap_onion_dlg_init_default_values */


/* ============================================================================
 * gap_onion_dlg_onion_cfg_dialog
 *
 *   return  0 .. OK
 *          -1 .. in case of Error or cancel
 * ============================================================================
 */

gint
gap_onion_dlg_onion_cfg_dialog(GapOnionMainGlobalParams *gpp)
{
  if(gap_debug) printf("gap_onion_dlg_onion_cfg_dialog: Start\n");

  gimp_ui_init ("gap_onion_dialog", FALSE);

  /* ---------- dialog ----------*/
  gpp->oni__checkbutton_select_case = NULL;
  gpp->oni__checkbutton_select_invert = NULL;
  gpp->oni__entry_select_string = NULL;

  if(gap_debug) printf("gap_onion_dlg_onion_cfg_dialog: Before create_oni__dialog\n");

  {
    gint32 l_ref_mode;
    gint32 l_select_mode;

    l_ref_mode = gpp->vin.ref_mode;
    l_select_mode = gpp->vin.select_mode;

    gpp->main_dialog = create_oni__dialog(gpp);

    gpp->vin.ref_mode = l_ref_mode;
    gpp->vin.select_mode = l_select_mode;
  }

  if(gap_debug) printf("gap_onion_dlg_onion_cfg_dialog: After create_oni__dialog\n");

  p_init_main_dialog_widgets(gpp);
  gtk_widget_show (gpp->main_dialog);

  gpp->run = 0; /* GAP_ONION_RUN_CANCEL */
  gtk_main ();

  if(gap_debug) printf("gap_onion_dlg_onion_cfg_dialog: A F T E R gtk_main run:%d\n", (int)gpp->run);

  gpp->main_dialog = NULL;

  if(gpp->run)
  {
    return 0;
  }
  return -1;
}               /* end gap_onion_dlg_onion_cfg_dialog */
