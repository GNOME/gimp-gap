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


/* 
   gap_dbbrowser_utils.h (original code dbbrowser_utils.h by Thomas NOEL <thomas@minet.net>
   
   09. dec .1998   hof: update for GIMP 1.1
   20. dec .1997   hof: GAP variant of DBbrowser
                        removed apply_callback
                        added constraint_procedure, 
                        added 2 buttons
                        added return type

   0.08  26th sept 97  by Thomas NOEL <thomas@minet.net> 
*/

#ifndef _GAP_DB_BROWSER_UTILS_H
#define _GAP_DB_BROWSER_UTILS_H

#include "libgimp/gimp.h"

typedef struct {
   char   selected_proc_name[256];
   int    button_nr;               /* -1 on cancel, 0 .. n */
   gint32 accelCharacteristic;
} GapDbBrowserResult;

/* proc to check if to add or not to add the procedure to the browsers listbox
 * retcode:
 * 0 ... do not add
 * 1 ... add the procedure to the browsers listbox
 */
typedef int (*t_constraint_func) (gchar *proc_name, gint32 image_id);

int
gap_db_browser_dialog (char *title_txt,
                       char *button_1_txt,
                       gboolean                 showAccelerationCharacteristic,
                       t_constraint_func        constraint_func,
                       t_constraint_func        constraint_func_sel1,
                       t_constraint_func        constraint_func_sel2,
                       GapDbBrowserResult      *result,
                       gint32                   image_id,
                       const char              *help_id
                       );

gchar*
gap_db_get_plugin_menupath        (const gchar *name);


#endif
