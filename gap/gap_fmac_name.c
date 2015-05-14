/* gap_fmac_name.c
 * 2006.12.19 hof (Wolfgang Hofer)
 *
 * GAP ... Gimp Animation Plugins
 *
 * This Module contains:
 * - filtermacro name definitions and filename convention procedures.
 *
 * WARNING:
 * filtermacros are a temporary solution, useful for animations
 * but do not expect support for filtermacros in future releases of GIMP-GAP
 * because GIMP may have real makro features in the future ...
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

/* SYTEM (UNIX) includes */
#include <string.h>
#include <stdlib.h>
#include <errno.h>

#include <glib/gstdio.h>

/* GIMP includes */
#include "gtk/gtk.h"
#include "libgimp/gimp.h"
#include "libgimp/gimpui.h"


/* GAP includes */
#include "config.h"
#include "gap_fmac_name.h"



extern      int gap_debug; /* ==0  ... dont print debug infos */



/* -----------------------------
 * gap_fmac_chk_filtermacro_file
 * -----------------------------
 * return TRUE if specified file exists starts with a correct FILTERMACRO FILE header
 */
gboolean
gap_fmac_chk_filtermacro_file(const char *filtermacro_file)
{
  FILE *l_fp;
  char         l_buf[400];
  gboolean l_rc;

  l_buf[0] = '\0';
  l_rc = FALSE;
  l_fp = g_fopen(filtermacro_file, "r");
  if (l_fp)
  {
    /* file exists, check for header */
    fgets(l_buf, sizeof(l_buf)-1, l_fp);
    if (strncmp(l_buf, "# FILTERMACRO FILE", strlen("# FILTERMACRO FILE")) == 0)
    {
      l_rc = TRUE;
    }
    fclose(l_fp);
  }

  return l_rc;
}  /* end gap_fmac_chk_filtermacro_file */


/* ----------------------------
 * gap_fmac_get_alternate_name
 * ----------------------------
 * in:   filename.abc.xxx.fmac
 * out:  filename.abc.xxx.VARYING.fmac
 */
char *
gap_fmac_get_alternate_name(const char *filtermacro_file)
{
  char *alt_name;
  char *master_name;
  char *master_ext;
  gint ii;
  
  if(filtermacro_file == NULL)
  {
    return (NULL);
  }
  
  master_name = g_strdup(filtermacro_file);
  master_ext = g_strdup("\0");
  
  for(ii=strlen(master_name) -1; ii >= 0; ii--)
  {
    if(master_name[ii] == '.')
    {
      g_free(master_ext);
      master_ext = g_strdup(&master_name[ii]);
      master_name[ii] = '\0';
      break;
    }
  }

  alt_name = g_strdup_printf("%s.VARYING%s"
                 , master_name
                 , master_ext
                 );
  g_free(master_name);
  g_free(master_ext);
  
  return(alt_name);

}  /* end gap_fmac_get_alternate_name */
