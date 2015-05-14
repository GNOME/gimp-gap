/* gap_onion_main.h
 * 2001.12.10 hof (Wolfgang Hofer)
 *
 * GAP ... Gimp Animation Plugins
 *
 * This Module handles ONION Skin Layers in the GIMP Video Menu.
 * Onion Layer(s) usually do show previous (or next) frame(s)
 * of the video in the current frame.
 *
 *   Video/OnionSkin/Configuration        ... GUI to configure, create abd delete onionskin Layer(s) for framerange
 *   Video/OnionSkin/Create or Replace    ... create or replace onionskin Layer(s) and set visible.
 *   Video/OnionSkin/Delete               ... delete onionskin Layer(s)
 *   Video/OnionSkin/Toggle Visibility    ... show/hide onionskin layer(s)
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
 * 1.3.16c; 2003/07/09   hof: splitted off gap_onion_base.h
 * 1.3.14a; 2003/05/22   hof: integration into gimp-gap-1.3.14
 * 1.3.12a; 2003/05/03   hof: started port to gimp-1.3  /gtk+2.2
 * version 1.2.2a;  2001.12.10   hof: created
 */

#ifndef _GAP_ONION_MAIN_H
#define _GAP_ONION_MAIN_H
 
#include "config.h"

/* SYTEM (UNIX) includes */
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>

/* #include <locale.h> */
#include <gap-intl.h>

#include <gtk/gtk.h>
#include <libgimp/gimp.h>
#include <libgimp/gimpui.h>

#include <gap_match.h>
#include <gap_lib.h>
#include <gap_lock.h>
#include <gap_layer_copy.h>

#include <gap_onion_base.h>
#include <gap_vin.h>


#define GAP_PLUGIN_NAME_ONION_CFG   "plug_in_gap_onionskin_configuration"
#define GAP_HELP_ID_ONION_CFG       "plug-in-gap-onionskin-configuration"
#define GAP_PLUGIN_NAME_ONION_APPLY "plug_in_onionskin_create_or_replace"
#define GAP_PLUGIN_NAME_ONION_DEL   "plug_in_onionskin_del"
#define GAP_PLUGIN_NAME_ONION_VISI  "plug_in_onionskin_toggle_visibility"

#define GAP_ONION_RUN_CANCEL  0
#define GAP_ONION_RUN_SET     1
#define GAP_ONION_RUN_APPLY   2
#define GAP_ONION_RUN_DELETE  3

/* how many frames are kept in cache for range processing */
#define GAP_ONION_CACHE_SIZE  12


/* note: plugin mames starting with plug_in_gap_
 * cannot be used as filter in other gap functions (Video->Frames Modify)
 * that is the reason why MAKE and DEL names do not contain "gap"
 */

typedef struct GapOnionMainAinfo {
   long         first_frame_nr;
   long         last_frame_nr;
   long         curr_frame_nr;
   long         frame_cnt;
   char         basename[1024];    /* may include path */
   char         extension[50];
   gdouble      framerate;
} GapOnionMainAinfo;

typedef struct GapOnionMainImageCache {
   gint32       count;
   gint32       framenr[GAP_ONION_CACHE_SIZE];
   gint32       image_id[GAP_ONION_CACHE_SIZE];
   gint32       layer_id[GAP_ONION_CACHE_SIZE];
} GapOnionMainImageCache;



typedef struct {
  GapVinVideoInfo  vin;
  gint    run;

  GapOnionMainAinfo     ainfo;
  GapOnionMainImageCache cache;
  gint32  range_from;
  gint32  range_to;
  gint32  image_ID;        /* -1 if there is no valid current image */
  gint32  layer_ID;        /* -1 if there is no valid current image */
  GimpRunMode run_mode;
  GtkWidget *main_dialog;

  GtkWidget  *oni__entry_select_string;
  GtkWidget  *oni__combo_ref_mode;
  GtkWidget  *oni__combo_select_mode;
  GtkWidget  *oni__spinbutton_ignore_botlayers;
  GtkWidget  *oni__spinbutton_num_olayers;
  GtkWidget  *oni__spinbutton_opacity;
  GtkWidget  *oni__spinbutton_opacity_delta;
  GtkWidget  *oni__spinbutton_range_from;
  GtkWidget  *oni__spinbutton_range_to;
  GtkWidget  *oni__spinbutton_ref_delta;
  GtkWidget  *oni__spinbutton_stack_pos;
  GtkWidget  *oni__checkbutton_ref_cycle;
  GtkWidget  *oni__checkbutton_select_case;
  GtkWidget  *oni__checkbutton_select_invert;
  GtkWidget  *oni__checkbutton_stack_top;
  GtkWidget  *oni__checkbutton_asc_opacity;
  GtkWidget  *oni__checkbutton_auto_replace;
  GtkWidget  *oni__checkbutton_auto_delete;

  GtkObject  *oni__spinbutton_ignore_botlayers_adj;
  GtkObject  *oni__spinbutton_num_olayers_adj;
  GtkObject  *oni__spinbutton_opacity_adj;
  GtkObject  *oni__spinbutton_opacity_delta_adj;
  GtkObject  *oni__spinbutton_range_from_adj;
  GtkObject  *oni__spinbutton_range_to_adj;
  GtkObject  *oni__spinbutton_ref_delta_adj;
  GtkObject  *oni__spinbutton_stack_pos_adj;
} GapOnionMainGlobalParams;

extern GapOnionMainGlobalParams global_params;


#endif

