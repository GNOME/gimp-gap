/*  gap_filter_pdb.h
 *
 * GAP ... Gimp Animation Plugins
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

#ifndef _GAP_FILTER_PDB_H
#define _GAP_FILTER_PDB_H

#include "libgimp/gimp.h"

typedef enum
{  GAP_PTYP_ANY                     = 0,
   GAP_PTYP_ITERATOR                = 1,
   GAP_PTYP_CAN_OPERATE_ON_DRAWABLE = 2 
} GapFiltPdbProcType;



/* ------------------------
 * gap_filter_pdb.h
 * ------------------------
 */

gint gap_filt_pdb_call_plugin(char *plugin_name, gint32 image_id, gint32 layer_id, GimpRunMode run_mode);
int  gap_filt_pdb_save_xcf(gint32 image_id, char *sav_name);
gint gap_filt_pdb_get_data(char *key);
void gap_filt_pdb_set_data(char *key, gint plugin_data_len);
gint gap_filt_pdb_procedure_available(char  *proc_name, GapFiltPdbProcType ptype);
char * gap_filt_pdb_get_iterator_proc(const char *plugin_name, gint *count);

int gap_filt_pdb_constraint_proc_sel1(gchar *proc_name, gint32 image_id);
int gap_filt_pdb_constraint_proc_sel2(gchar *proc_name, gint32 image_id);
int gap_filt_pdb_constraint_proc(gchar *proc_name, gint32 image_id);

gboolean        gap_filter_iterator_call(const char *iteratorname
                        , gint32      total_steps
                        , gdouble     current_step
                        , const char *plugin_name
                        , gint32      plugin_data_len
                        );


#endif
