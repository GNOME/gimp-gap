/* gap_mod_layer_dialog.h
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

#ifndef _GAP_MOD_LAYER_DIALOG_H
#define _GAP_MOD_LAYER_DIALOG_H

/* GIMP includes */
#include "gtk/gtk.h"
#include "config.h"
#include "libgimp/gimp.h"

/* GAP includes */
#include "gap_lib.h"
#include "gap_mod_layer.h"


typedef struct GapModFramesGlobalParams {  /* nick: gmop */
  gboolean     run_flag;
  GimpRunMode  run_mode;

  GapAnimInfo *ainfo_ptr;
  gint32       range_from;
  gint32       range_to;
  gint32       action_mode;
  gint32       sel_mode;
  gint32       sel_case;
  gint32       sel_invert;
  char         sel_pattern[MAX_LAYERNAME];
  char         new_layername[MAX_LAYERNAME];


  /* GUI widget pointers */
  GtkWidget *shell;  
  GtkWidget *func_info_label;
  GtkWidget *new_layername_label;
  GtkWidget *new_layername_entry;
  GtkWidget *layer_pattern_entry;
  GtkWidget *case_sensitive_check_button;  
  GtkWidget *invert_check_button;
  GtkWidget *layer_selection_frame;
  GtkWidget *progress_bar;
  GtkWidget *main_vbox;  

  GtkObject *frame_from_adj;
  GtkObject *frame_to_adj;

  gint32       retcode;
  
} GapModFramesGlobalParams;




int gap_mod_frames_dialog(GapAnimInfo *ainfo_ptr,
                   gint32 *range_from,  gint32 *range_to,
                   gint32 *action_mode, gint32 *sel_mode,
                   gint32 *sel_case,    gint32 *sel_invert,
                   char *sel_pattern,   char   *new_layername);

#endif


















