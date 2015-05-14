/* gap_colormask_file.c
 *    by hof (Wolfgang Hofer)
 *    colormask filter parameter file handling procedures (load/save)
 *  2010/02/25
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
 * version 2.7.0;             hof: created
 */

/* SYTEM (UNIX) includes */ 
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

/* GIMP includes */
#include "gtk/gtk.h"
#include "libgimp/gimp.h"

/* GAP includes */
#include "config.h"
#include "gap_lib_common_defs.h"
#include "gap_libgimpgap.h"
#include "gap_colormask_file.h"

extern int gap_debug;


/* --------------------------
 * p_set_master_keywords
 * --------------------------
 * set keywords for the colordiff filter parameter file format
 * adresspointers to the relevant parameter, datatype (how to scan value when parameter are read from file)
 * and comment string (that is added to the parameter when saved to file)
 */
static void
p_set_master_keywords(GapValKeyList *keylist, GapColormaskValues *cmaskvals)
{
   gap_val_set_keyword(keylist, "(algorithm ",            &cmaskvals->algorithm,           GAP_VAL_GINT,    0, "# 0 simple algorithm");
   gap_val_set_keyword(keylist, "(loColorThreshold ",     &cmaskvals->loColorThreshold,    GAP_VAL_GDOUBLE, 0, "# lower color threshold (0.0 to 1.0)");
   gap_val_set_keyword(keylist, "(hiColorThreshold ",     &cmaskvals->hiColorThreshold,    GAP_VAL_GDOUBLE, 0, "# upper color threshold (0.0 to 1.0)");
   gap_val_set_keyword(keylist, "(colorSensitivity ",     &cmaskvals->colorSensitivity,    GAP_VAL_GDOUBLE, 0, "# gamma valid range is (1.0 to 2.0)");
   gap_val_set_keyword(keylist, "(lowerOpacity ",         &cmaskvals->lowerOpacity,        GAP_VAL_GDOUBLE, 0, "# color threshold (0.0 to 1.0)");
   gap_val_set_keyword(keylist, "(upperOpacity ",         &cmaskvals->upperOpacity,        GAP_VAL_GDOUBLE, 0, "# color threshold (0.0 to 1.0)");
   gap_val_set_keyword(keylist, "(triggerAlpha ",         &cmaskvals->triggerAlpha,        GAP_VAL_GDOUBLE, 0, "# opacity protection threshold (0.0 to 1.0)");
   gap_val_set_keyword(keylist, "(isleRadius ",           &cmaskvals->isleRadius,          GAP_VAL_GDOUBLE, 0, "# radius for isolated pixel removal. (0.0 to 10.0)");
   gap_val_set_keyword(keylist, "(isleAreaPixelLimit ",   &cmaskvals->isleAreaPixelLimit,  GAP_VAL_GDOUBLE, 0, "# size of area in pixels for remove isaolated pixel(area)s (0.0 to 100.0)");
   gap_val_set_keyword(keylist, "(featherRadius ",        &cmaskvals->featherRadius,       GAP_VAL_GDOUBLE, 0, "# radius for feathering edges (0.0 to 10.0)");

   gap_val_set_keyword(keylist, "(edgeColorThreshold ",   &cmaskvals->edgeColorThreshold,  GAP_VAL_GDOUBLE,  0, "# threshold for edge detection (relevant for algorithm 1)");
   gap_val_set_keyword(keylist, "(thresholdColorArea ",   &cmaskvals->thresholdColorArea,  GAP_VAL_GDOUBLE,  0, "# threshold for color difference colors belonging to same area.(algorithm 2)");
   gap_val_set_keyword(keylist, "(pixelDiagonal ",        &cmaskvals->pixelDiagonal,       GAP_VAL_GDOUBLE,  0, "# areas with smaller diagonale are considered small isles (algorithm 2, 0.0 to 100.0)");
   gap_val_set_keyword(keylist, "(pixelAreaLimit ",       &cmaskvals->pixelAreaLimit,      GAP_VAL_GDOUBLE,  0, "# areas with less pixels are considered as small isles (algorithm 2, 0.0 to 1000.0)");
   gap_val_set_keyword(keylist, "(connectByCorner ",      &cmaskvals->connectByCorner,     GAP_VAL_GBOOLEAN, 0, "# pixels touching only at corners are of the area. (algorithm 2, 0 to 1)");

   gap_val_set_keyword(keylist, "(enableKeyColorThreshold ",  &cmaskvals->enableKeyColorThreshold,     GAP_VAL_GBOOLEAN, 0, "# enable individual low threshold for key color");
   gap_val_set_keyword(keylist, "(loKeyColorThreshold ",   &cmaskvals->loKeyColorThreshold,  GAP_VAL_GDOUBLE,  0, "# the individual threshold for keycolor (relevant when enableKeyColorThreshold == 1)");
   gap_val_set_keyword(keylist, "(keyColorSensitivity ",   &cmaskvals->keyColorSensitivity,  GAP_VAL_GDOUBLE,  0, "# sensitivity for keycolor (relevant when enableKeyColorThreshold == 1 0.0 to 10.0)");
   gap_val_set_keyword(keylist, "(keyColor.r ",            &cmaskvals->keycolor.r,  GAP_VAL_GDOUBLE,  0, "# keycolor red channel (0.0 to 1.0)");
   gap_val_set_keyword(keylist, "(keyColor.g ",            &cmaskvals->keycolor.g,  GAP_VAL_GDOUBLE,  0, "# keycolor green channel (0.0 to 1.0)");
   gap_val_set_keyword(keylist, "(keyColor.b ",            &cmaskvals->keycolor.b,  GAP_VAL_GDOUBLE,  0, "# keycolor blue channel (0.0 to 1.0)");

}  /* end p_set_master_keywords */


/* --------------------------
 * gap_colormask_file_load
 * --------------------------
 * load parameters from parameter file.
 * return TRUE if ok, FALSE if not
 */
gboolean
gap_colormask_file_load (const char *filename, GapColormaskValues *cmaskvals)
{
  GapValKeyList    *keylist;
  gint              cnt_scanned_items;

  /* TODO maybe init with defaults the case where no value available ?
   */

  keylist = gap_val_new_keylist();
  p_set_master_keywords(keylist, cmaskvals);

  cnt_scanned_items = gap_val_scann_filevalues(keylist, filename);
  gap_val_free_keylist(keylist);

  //if(gap_debug)
  {
    printf("gap_colormask_file_load: params loaded: cmaskvals:%ld cnt_scanned_items:%d\n"
       , (long)cmaskvals
       , (int)cnt_scanned_items
       );
  }

  if(cnt_scanned_items <= 0)
  {
    g_message(_("Could not read colormask parameters from file:\n%s"), filename);
    return (FALSE);
  }

  return(TRUE);
  
}  /* end gap_colormask_file_load */


/* --------------------------
 * gap_colormask_file_save
 * --------------------------
 * (re)write the specified ffmpeg videoencode parameter file
 */
gboolean
gap_colormask_file_save (const char *filename, GapColormaskValues *cmaskvals)
{
  GapValKeyList    *keylist;
  int               l_rc;

  keylist = gap_val_new_keylist();

  //if(gap_debug)
  {
    printf("gap_colormask_file_save: now saving parameters: to file:%s\n", filename);
  }

  p_set_master_keywords(keylist, cmaskvals);
  l_rc = gap_val_rewrite_file(keylist
                          , filename
                          , "# GIMP / GAP COLORMASK parameter file"   /*  hdr_text */
                          , ")"                                       /* terminate char */
                          );

  gap_val_free_keylist(keylist);

  if(l_rc != 0)
  {
    gint l_errno;

    l_errno = errno;
    g_message(_("Could not save colormask parameterfile:"
               "'%s'"
               "%s")
               ,filename
               ,g_strerror (l_errno) );
    return (FALSE);
  }

  return(TRUE);
}  /* end gap_colormask_file_save */
