/*  gap_vin.c
 *
 *  This module handles GAP video info files (_vin.gap)
 *  _vin.gap files store global informations about an animation
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
 * version 2.1.0a;  2004/06/03  hof: added onionskin setting ref_mode
 * version 1.3.25b; 2004/01/23  hof: bugfix: gap_val_load_textfile set correct line_nr
 * version 1.3.18b; 2003/08/23  hof: gap_vin_get_all: force timezoom value >= 1 (0 results in divison by zero)
 * version 1.3.18a; 2003/08/23  hof: bugfix: gap_vin_get_all must clear (g_malloc0) vin_ptr struct at creation
 * version 1.3.16c; 2003/07/12  hof: support onionskin settings in video_info files
 *                                   key/value/datatype is now managed by GapValKeyList
 * version 1.3.14b; 2003/06/03  hof: using setlocale independent float conversion procedures
 *                                   g_ascii_strtod() and g_ascii_dtostr()
 * version 1.3.14a; 2003/05/24  hof: created (splitted off from gap_pdb_calls module)
 *                              write now keeps unkown keyword lines in _vin.gap files
 */
#include <stdlib.h>
#include <string.h>
#include <locale.h>

#include <glib/gstdio.h>

/* GIMP includes */
#include "libgimp/gimp.h"

/* GAP includes */
#include "gap_val_file.h"
#include "gap_vin.h"
extern int gap_debug;


/* --------------------------
 * p_set_master_keywords
 * --------------------------
 */
static void
p_set_master_keywords(GapValKeyList *keylist, GapVinVideoInfo *vin_ptr)
{
   gap_val_set_keyword(keylist, "(framerate ", &vin_ptr->framerate, GAP_VAL_GDOUBLE, 0, "# 1.0 upto 100.0 frames per sec");
   gap_val_set_keyword(keylist, "(timezoom ", &vin_ptr->timezoom,   GAP_VAL_GINT32, 0, "# 1 upto 100 frames");
   gap_val_set_keyword(keylist, "(active_layer_tracking ", &vin_ptr->active_layer_tracking,   GAP_VAL_GINT32, 0, "# 0:OFF 1 by name 2 by stackpos");
}  /* end p_set_master_keywords */

/* --------------------------
 * p_set_onion_keywords
 * --------------------------
 */
static void
p_set_onion_keywords(GapValKeyList *keylist, GapVinVideoInfo *vin_ptr)
{
   gap_val_set_keyword(keylist, "(onion_auto_enable ", &vin_ptr->onionskin_auto_enable, GAP_VAL_GBOOLEAN, 0, "\0");
   gap_val_set_keyword(keylist, "(onion_auto_replace_after_load ", &vin_ptr->auto_replace_after_load, GAP_VAL_GBOOLEAN, 0, "\0");
   gap_val_set_keyword(keylist, "(onion_auto_delete_before_save ", &vin_ptr->auto_delete_before_save, GAP_VAL_GBOOLEAN, 0, "\0");
   gap_val_set_keyword(keylist, "(onion_number_of_layers ", &vin_ptr->num_olayers, GAP_VAL_GINT32, 0, "\0");
   gap_val_set_keyword(keylist, "(onion_ref_mode ", &vin_ptr->ref_mode, GAP_VAL_GINT32, 0, "\0");
   gap_val_set_keyword(keylist, "(onion_ref_delta ", &vin_ptr->ref_delta, GAP_VAL_GINT32, 0, "\0");
   gap_val_set_keyword(keylist, "(onion_ref_cycle ", &vin_ptr->ref_cycle, GAP_VAL_GBOOLEAN, 0, "\0");
   gap_val_set_keyword(keylist, "(onion_stack_pos ", &vin_ptr->stack_pos, GAP_VAL_GINT32, 0, "\0");
   gap_val_set_keyword(keylist, "(onion_stack_top ", &vin_ptr->stack_top, GAP_VAL_GBOOLEAN, 0, "\0");
   gap_val_set_keyword(keylist, "(onion_opacity_initial ", &vin_ptr->opacity, GAP_VAL_GDOUBLE, 0, "\0");
   gap_val_set_keyword(keylist, "(onion_opacity_delta ", &vin_ptr->opacity_delta, GAP_VAL_GDOUBLE, 0, "\0");
   gap_val_set_keyword(keylist, "(onion_ignore_botlayers ", &vin_ptr->ignore_botlayers, GAP_VAL_GINT32, 0, "\0");
   gap_val_set_keyword(keylist, "(onion_select_mode ", &vin_ptr->select_mode, GAP_VAL_GINT32, 0, "\0");
   gap_val_set_keyword(keylist, "(onion_select_casesensitive ", &vin_ptr->select_case, GAP_VAL_GBOOLEAN, 0, "\0");
   gap_val_set_keyword(keylist, "(onion_select_invert ", &vin_ptr->select_invert, GAP_VAL_GBOOLEAN, 0, "\0");
   gap_val_set_keyword(keylist, "(onion_select_string ", &vin_ptr->select_string[0], GAP_VAL_STRING, sizeof(vin_ptr->select_string), "\0");
   gap_val_set_keyword(keylist, "(onion_ascending_opacity ", &vin_ptr->asc_opacity, GAP_VAL_GBOOLEAN, 0, "\0");
}  /* end p_set_onion_keywords */


/* --------------------------
 * gap_vin_alloc_name
 * --------------------------
 * to get the name of the the video_info_file
 * (the caller should g_free the returned name after use)
 */
char *
gap_vin_alloc_name(char *basename)
{
  char *l_str;

  if(basename == NULL)
  {
    return(NULL);
  }

  l_str = g_strdup_printf("%svin.gap", basename);
  return(l_str);
}  /* end gap_vin_alloc_name */


/* --------------------------
 * gap_vin_set_common_keylist
 * --------------------------
 * (re)write the current video info to .vin file
 * only the values for the keywords in the passed keylist
 * are created (or replaced) in the file.
 * all other lines are left unchanged.
 * if the file is empty a header is added.
 */
static int
gap_vin_set_common_keylist(GapValKeyList *keylist, GapVinVideoInfo *vin_ptr, char *basename)
{
  char  *l_vin_filename;
  int   l_rc;
   
  l_rc = -1;
  l_vin_filename = gap_vin_alloc_name(basename);

  if(l_vin_filename)
  {
    l_rc = gap_val_rewrite_file(keylist
                          ,l_vin_filename
                          ,"# GIMP / GAP Videoinfo file"   /*  hdr_text */
                          ,")"                             /* terminate char */
                          );
    g_free(l_vin_filename);
  }

  return(l_rc);
}  /* end gap_vin_set_common_keylist */



/* --------------------------
 * gap_vin_get_all_keylist
 * --------------------------
 * get video info from .vin file
 */
static void
gap_vin_get_all_keylist(GapValKeyList *keylist, GapVinVideoInfo *vin_ptr, char *basename)
{
  char  *l_vin_filename;

  if (vin_ptr != NULL)
  {
    /* init wit defaults (for the case where no video_info file available) */
    vin_ptr->timezoom = 1;
    vin_ptr->framerate = 24.0;
    vin_ptr->active_layer_tracking = GAP_ACTIVE_LAYER_TRACKING_OFF;
  
    vin_ptr->onionskin_auto_enable = TRUE;
    vin_ptr->auto_replace_after_load = FALSE;
    vin_ptr->auto_delete_before_save = FALSE;

    vin_ptr->num_olayers        = 2;
    vin_ptr->ref_delta          = -1;
    vin_ptr->ref_cycle          = FALSE;
    vin_ptr->stack_pos          = 1;
    vin_ptr->stack_top          = FALSE;
    vin_ptr->asc_opacity        = FALSE;
    vin_ptr->opacity            = 80.0;
    vin_ptr->opacity_delta      = 80.0;
    vin_ptr->ignore_botlayers   = 1;
    vin_ptr->select_mode        = 6;     /* GAP_MTCH_ALL_VISIBLE */
    vin_ptr->select_case        = 0;     /* 0 .. ignore case, 1..case sensitve */
    vin_ptr->select_invert      = 0;     /* 0 .. no invert, 1 ..invert */
    vin_ptr->select_string[0] = '\0';
  }
  
  l_vin_filename = gap_vin_alloc_name(basename);
  if(l_vin_filename)
  {
      gap_val_scann_filevalues(keylist, l_vin_filename);
      g_free(l_vin_filename);
  }
}  /* end gap_vin_get_all_keylist */


/* --------------------------
 * gap_vin_get_all
 * --------------------------
 * get video info from .vin file
 * get does always fetch all values for all known keywords
 */
GapVinVideoInfo *
gap_vin_get_all(char *basename)
{
  GapVinVideoInfo *vin_ptr;
  GapValKeyList    *keylist;

  keylist = gap_val_new_keylist();
  vin_ptr = g_new0 (GapVinVideoInfo, 1);
  vin_ptr->timezoom = 1;
  p_set_master_keywords(keylist, vin_ptr);
  p_set_onion_keywords(keylist, vin_ptr);
  
  gap_vin_get_all_keylist(keylist, vin_ptr, basename);

  vin_ptr->timezoom = MAX(1,vin_ptr->timezoom);
  
  gap_val_free_keylist(keylist);

  if(gap_debug)
  {
     printf("gap_vin_get_all: RETURN with vin_ptr content:\n");
     printf("  num_olayers: %d\n",   (int)vin_ptr->num_olayers);
     printf("  ref_delta: %d\n",     (int)vin_ptr->ref_delta);
     printf("  ref_cycle: %d\n",     (int)vin_ptr->ref_cycle);
     printf("  stack_pos: %d\n",     (int)vin_ptr->stack_pos);
     printf("  stack_top: %d\n",     (int)vin_ptr->stack_top);
     printf("  opacity: %f\n",       (float)vin_ptr->opacity);
     printf("  opacity_delta: %f\n", (float)vin_ptr->opacity_delta);
     printf("  ignore_botlayers: %d\n", (int)vin_ptr->ignore_botlayers);
     printf("  asc_opacity: %d\n",   (int)vin_ptr->asc_opacity);
     printf("  onionskin_auto_enable: %d\n",   (int)vin_ptr->onionskin_auto_enable);
     printf("  auto_replace_after_load: %d\n",   (int)vin_ptr->auto_replace_after_load);
     printf("  auto_delete_before_save: %d\n",   (int)vin_ptr->auto_delete_before_save);
  }
  return(vin_ptr);
}  /* end gap_vin_get_all */


/* --------------------------
 * gap_vin_set_common
 * --------------------------
 * (re)write the current video info file  (_vin.gap file)
 * IMPORTANT: this Procedure affects only
 * the master values for framerate and timezoom.
 * (other settings like the onionskin specific Settings.
 *  are left unchanged)
 */
int
gap_vin_set_common(GapVinVideoInfo *vin_ptr, char *basename)
{
  GapValKeyList    *keylist;
  int          l_rc;

  keylist = gap_val_new_keylist();

  p_set_master_keywords(keylist, vin_ptr);
  l_rc = gap_vin_set_common_keylist(keylist, vin_ptr, basename);
  
  gap_val_free_keylist(keylist);

  return(l_rc);
}  /* end gap_vin_set_common */


/* --------------------------
 * gap_vin_set_common_onion
 * --------------------------
 * (re)write the onionskin specific values
 * to the video info file
 */
int
gap_vin_set_common_onion(GapVinVideoInfo *vin_ptr, char *basename)
{
  GapValKeyList    *keylist;
  int          l_rc;

  keylist = gap_val_new_keylist();

  p_set_onion_keywords(keylist, vin_ptr);
  l_rc = gap_vin_set_common_keylist(keylist, vin_ptr, basename);
  
  gap_val_free_keylist(keylist);

  return(l_rc);
}  /* end gap_vin_set_common_onion */
