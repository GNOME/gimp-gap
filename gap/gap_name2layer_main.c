/*  gap_name2layer_main.c
 *  create a textlayer from imagename (or ists number part) plug-in by Wolfgang Hofer
 *  2003/05/15
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
 * along with this program; if not, write to the Free Software
 * <http://www.gnu.org/licenses/>.
 */

/* Revision history
 *  (2003/10/10)  v1.3.20d   hof: sourcecode cleanup
 *  (2003/06/21)  v1.3.17a   hof: types in GimpPlugInInfo.run procedure
 *  (2003/06/21)  v1.3.15a   hof: bugfix: MenuPath is language dependent string
 *  (2003/05/15)  v1.0       hof: created
 */

#include "config.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <gtk/gtk.h>
#include <libgimp/gimp.h>
#include <libgimp/gimpui.h>

#include "gap_arr_dialog.h"
#include "gap_lastvaldesc.h"

#include "gap-intl.h"

/* Defines */
#define PLUG_IN_NAME        "plug_in_name2layer"
#define PLUG_IN_HELP_ID     "plug-in-name2layer"
#define PLUG_IN_PRINT_NAME  "Name to Layer"
#define PLUG_IN_IMAGE_TYPES "RGB*, INDEXED*, GRAY*"
#define PLUG_IN_AUTHOR      "Wolfgang Hofer (hof@gimp.org)"
#define PLUG_IN_COPYRIGHT   "Wolfgang Hofer"


int gap_debug = 0;  /* 1 == print debug infos , 0 dont print debug infos */


typedef struct {
  gint mode;
  gint fontsize;
  gchar fontname[500];
  gint  posx;
  gint  posy;
  gint  antialias;
  gint  create_new_layer;
} NamlValues;

typedef struct {
  gint run;
} NamlInterface;



static NamlValues glob_namlvals =
{
    0    /* 0 .. mode number only, 1 ..filename, 2..filename with path */
 , 48   /* 24 default fontsize (?? is ignored size form fontname is used ) */
 ,  "-*-Charter-*-r-*-*-48-*-*-*-p-*-*-*"   /* defaul fontname "-*-Charter-*-r-*-*-24-*-*-*-p-*-*-*" */
 , 15
 , 15
 , FALSE
 , 0     /* 0 render on input drawable, 1 create new layer */
};


static void  query (void);
static void  run (const gchar *name,          /* name of plugin */
     gint nparams,               /* number of in-paramters */
     const GimpParam * param,    /* in-parameters */
     gint *nreturn_vals,         /* number of out-parameters */
     GimpParam ** return_vals);  /* out-parameters */

static gint  p_Naml (gint32 image_id, gint32 drawable_id);
static gint  Naml_dialog (void);


/* Global Variables */
GimpPlugInInfo PLUG_IN_INFO =
{
  NULL,   /* init_proc  */
  NULL,   /* quit_proc  */
  query,  /* query_proc */
  run     /* run_proc   */
};

static GimpParamDef in_args[] = {
                  { GIMP_PDB_INT32,    "run_mode", "Interactive, non-interactive"},
                  { GIMP_PDB_IMAGE,    "image", "Input image" },
                  { GIMP_PDB_DRAWABLE, "drawable", "Input drawable ()"},
                  { GIMP_PDB_INT32,    "mode", "0 ... number only, 1 .. filename  2 .. filename with path"},
                  { GIMP_PDB_INT32,    "fontsize", "fontsize in pixel"},
                  { GIMP_PDB_STRING,   "font", "fontname "},
                  { GIMP_PDB_INT32,    "posx", "x offset in pixel"},
                  { GIMP_PDB_INT32,    "posy", "y offset in pixel"},
                  { GIMP_PDB_INT32,    "antialias", "0 .. OFF, 1 .. antialias on"},
                  { GIMP_PDB_INT32,    "create_new_layer", "0 .. render on input drawable, 1 .. create new layer"},
  };

static GimpParamDef out_args[] = {
    { GIMP_PDB_DRAWABLE, "affected_drawable", "the drawable where the name/number was rendered." }
  };


static gint global_number_in_args = G_N_ELEMENTS (in_args);
static gint global_number_out_args = G_N_ELEMENTS(out_args);


/* Functions */

MAIN ()

static void query (void)
{
  static GimpLastvalDef lastvals[] =
  {
    GIMP_LASTVALDEF_GINT            (GIMP_ITER_FALSE,  glob_namlvals.mode,  "mode"),
    GIMP_LASTVALDEF_GINT            (GIMP_ITER_TRUE,   glob_namlvals.fontsize,  "fontsize"),
    GIMP_LASTVALDEF_ARRAY           (GIMP_ITER_FALSE,  glob_namlvals.fontname, "fontname"),
    GIMP_LASTVALDEF_GCHAR           (GIMP_ITER_FALSE,  glob_namlvals.fontname[0], "fontname"),
    GIMP_LASTVALDEF_GINT            (GIMP_ITER_TRUE,   glob_namlvals.posx, "posx"),
    GIMP_LASTVALDEF_GINT            (GIMP_ITER_TRUE,   glob_namlvals.posy, "posy"),
    GIMP_LASTVALDEF_GINT            (GIMP_ITER_FALSE,  glob_namlvals.antialias, "antialias"),
    GIMP_LASTVALDEF_GINT            (GIMP_ITER_FALSE,  glob_namlvals.create_new_layer, "create_new_layer"),
  };



  gimp_plugin_domain_register (GETTEXT_PACKAGE, LOCALEDIR);

  /* registration for last values buffer structure (useful for animated filter apply) */
  gimp_lastval_desc_register(PLUG_IN_NAME,
                             &glob_namlvals,
                             sizeof(glob_namlvals),
                             G_N_ELEMENTS (lastvals),
                             lastvals);

  /* the actual installation of the adjust plugin */
  gimp_install_procedure (PLUG_IN_NAME,
                          "Render Filename to Layer",
                          "This plug-in renders the imagename, "
                          "or the numberpart of the imagename onto the image. "
                          "if the parameter create_new_layer is 0, the filename is rendered "
                          "to the input drawable, otherwise a new textlayer is created",
                          PLUG_IN_AUTHOR,
                          PLUG_IN_COPYRIGHT,
                          GAP_VERSION_WITH_DATE,
                          N_("Filename to Layer..."),
                          PLUG_IN_IMAGE_TYPES,
                          GIMP_PLUGIN,
                          global_number_in_args,
                          global_number_out_args,
                          in_args,
                          out_args);

  gimp_plugin_menu_register (PLUG_IN_NAME, N_("<Image>/Video/Layer/Render/"));
}  /* end query */

static void
run (const gchar *name,          /* name of plugin */
     gint nparams,               /* number of in-paramters */
     const GimpParam * param,    /* in-parameters */
     gint *nreturn_vals,         /* number of out-parameters */
     GimpParam ** return_vals)   /* out-parameters */
{
  const gchar *l_env;
  gint32       image_id = -1;


  /* Get the runmode from the in-parameters */
  GimpRunMode run_mode = param[0].data.d_int32;

  /* status variable, use it to check for errors in invocation usualy only
     during non-interactive calling */
  GimpPDBStatusType status = GIMP_PDB_SUCCESS;

  /* always return at least the status to the caller. */
  static GimpParam values[2];

  INIT_I18N();

  l_env = g_getenv("NUML_DEBUG");
  if(l_env != NULL)
  {
    if((*l_env != 'n') && (*l_env != 'N')) gap_debug = 1;
  }

  if(gap_debug) fprintf(stderr, "\n\nDEBUG: run %s\n", name);

  /* initialize the return of the status */
  values[0].type = GIMP_PDB_STATUS;
  values[0].data.d_status = status;
  values[1].type = GIMP_PDB_DRAWABLE;
  values[1].data.d_drawable = -1;
  *nreturn_vals = 1;
  *return_vals = values;


  /* get image and drawable */
  image_id = param[1].data.d_int32;


  /* how are we running today? */
  switch (run_mode)
  {
    case GIMP_RUN_INTERACTIVE:
      /* Possibly retrieve data from a previous run */
      gimp_get_data (PLUG_IN_NAME, &glob_namlvals);

      /* Get information from the dialog */
      if (Naml_dialog() != 0)
                return;
      break;

    case GIMP_RUN_NONINTERACTIVE:
      /* check to see if invoked with the correct number of parameters */
      if (nparams == global_number_in_args)
      {
          glob_namlvals.mode        = (gint) param[3].data.d_int32;
          glob_namlvals.fontsize    = (gint) param[4].data.d_int32;

          if(param[5].data.d_string != NULL)
          {
            g_snprintf(glob_namlvals.fontname, sizeof(glob_namlvals.fontname), "%s", param[5].data.d_string);
          }
          glob_namlvals.posx        = (gint) param[6].data.d_int32;
          glob_namlvals.posy        = (gint) param[7].data.d_int32;
          glob_namlvals.antialias   = (gint) param[8].data.d_int32;
          glob_namlvals.create_new_layer = (gint) param[9].data.d_int32;
      }
      else
      {
        status = GIMP_PDB_CALLING_ERROR;
      }

      break;

    case GIMP_RUN_WITH_LAST_VALS:
      /* Possibly retrieve data from a previous run */
      gimp_get_data (PLUG_IN_NAME, &glob_namlvals);

      break;

    default:
      break;
  }

  if (status == GIMP_PDB_SUCCESS)
  {
    /* Run the main function */
    values[1].data.d_drawable = p_Naml(image_id, param[2].data.d_drawable);
    if (values[1].data.d_drawable < 0)
    {
       status = GIMP_PDB_CALLING_ERROR;
    }

    /* If run mode is interactive, flush displays, else (script) don't
       do it, as the screen updates would make the scripts slow */
    if (run_mode != GIMP_RUN_NONINTERACTIVE)
      gimp_displays_flush ();

    /* Store variable states for next run */
    if (run_mode == GIMP_RUN_INTERACTIVE)
      gimp_set_data (PLUG_IN_NAME, &glob_namlvals, sizeof (NamlValues));
  }
  values[0].data.d_status = status;
}       /* end run */



/* ============================================================================
 * p_Naml
 *        The main function
 * ============================================================================
 */

static gint
p_Naml (gint32 image_id, gint32 drawable_id)
{
  gint32  l_new_layer_id;
  gint32  l_drawable_id;
  gchar *l_imagename;
  gchar *l_text;

  if(gap_debug) printf("p_Naml START\n");

  l_imagename = g_strdup(gimp_image_get_filename(image_id) );
  if(l_imagename == NULL)
  {
    l_imagename = g_strdup("<null> 0");
  }
  l_text = l_imagename;

  if(gap_debug) printf("p_Naml (1) l_imagename:%s\n", l_imagename);

  l_drawable_id = -1;
  if(glob_namlvals.create_new_layer == 0)
  {
    l_drawable_id = drawable_id;
  }


  if (glob_namlvals.mode == 0)
  {
    gchar *l_ext;
    gchar *l_ptr;
    gint  l_digits;


    l_ext = NULL;
    l_ptr = &l_imagename[strlen(l_imagename)-1];
    while(l_ptr != l_imagename)
    {
      if(*l_ptr == '.')
      {
        *l_ptr = '\0';   /* cut off extension part */
        l_ext = l_ptr;
        l_ext++;
        l_ptr--;
        break;
      }
      l_ptr--;
    }
    l_digits = 0;

    if(l_ptr == l_imagename)
    {
      /* the imagename had no extension, restart number search at end of string */
      l_ptr = &l_imagename[strlen(l_imagename)-1];
    }

    while(l_ptr != l_imagename)
    {

      if((*l_ptr >= '0') && (*l_ptr <= '9'))
      {
        l_ptr--;
        l_digits++;
      }
      else
      {
        l_text = l_ptr;   /* pointer to start of number part in the filename */
        if(l_digits > 0)
        {
          l_text++;
        }
        else
        {
          l_text = "<null>";   /* no digits found */
        }
        break;
      }
    }
  }
  else
  {
    if (glob_namlvals.mode == 1)
    {
      l_text = &l_imagename[strlen(l_imagename)-1];
      while(l_text != l_imagename)
      {
        if(*l_text == G_DIR_SEPARATOR)
        {
          l_text++;
          break;
        }
        l_text--;
      }
    }
  }

  gimp_image_undo_group_start(image_id);

  gimp_selection_none(image_id);

  l_new_layer_id = gimp_text_fontname( image_id
                                   , l_drawable_id          /* drawable -1 force create of new layer */
                                   , (float)glob_namlvals.posx           /* X */
                                   , (float)glob_namlvals.posy           /* Y */
                                   , l_text
                                   , 0             /* -1 <= border */
                                   , glob_namlvals.antialias
                                   , (float)glob_namlvals.fontsize
                                   , 0    /* 0 size in pixels, 1: in points */
                                   , glob_namlvals.fontname
                                   );

  if(l_drawable_id >= 0)
  {
     gimp_floating_sel_anchor(l_new_layer_id);
  }

  gimp_image_undo_group_end(image_id);

  if(gap_debug) printf("p_Naml END layer_id: %d\n", (int)l_new_layer_id);

  return l_new_layer_id;
}       /* end p_Naml */


/* ------------------
 * Naml_dialog
 * ------------------
 *   return  0 .. OK
 *          -1 .. in case of Error or cancel
 */
static gint
Naml_dialog(void)
{
#define VR_NAME2LAYER_DIALOG_ARGC 9
#define VR_MODELIST_SIZE 3
  static GapArrArg  argv[VR_NAME2LAYER_DIALOG_ARGC];
  gint ii;
  gint ii_mode;
  gint ii_fontsize;
  gint ii_posx;
  gint ii_posy;
  gint ii_antialias;
  gint ii_create_new_layer;
  static char *radio_modes[VR_MODELIST_SIZE]  = {"Number Only", "Filename", "Path/Filename" };

  ii=0; gap_arr_arg_init(&argv[ii], GAP_ARR_WGT_OPTIONMENU); ii_mode = ii;
  argv[ii].label_txt = _("Mode:");
  argv[ii].help_txt  = _("Modes to choose entire filename or just parts of the filename");
  argv[ii].radio_argc  = VR_MODELIST_SIZE;
  argv[ii].radio_argv = radio_modes;
  argv[ii].radio_ret  = 0;
  argv[ii].int_default = 0;
  argv[ii].has_default = TRUE;
  argv[ii].text_buf_default = g_strdup("\0");

  ii++; gap_arr_arg_init(&argv[ii], GAP_ARR_WGT_FONTSEL);
  argv[ii].label_txt = _("Fontname:");
  argv[ii].entry_width = 350;       /* pixel */
  argv[ii].help_txt  = _("Select Fontname");
  argv[ii].text_buf_len = sizeof(glob_namlvals.fontname);
  argv[ii].text_buf_ret = &glob_namlvals.fontname[0];
  argv[ii].has_default = TRUE;
  argv[ii].text_buf_default = g_strdup("-*-Charter-*-r-*-*-24-*-*-*-p-*-*-*");

  ii++; gap_arr_arg_init(&argv[ii], GAP_ARR_WGT_INT); ii_fontsize = ii;
  argv[ii].constraint = TRUE;
  argv[ii].label_txt = _("Fontsize:");
  argv[ii].help_txt  = _("Fontsize in pixels");
  argv[ii].int_min   = (gint)1;
  argv[ii].int_max   = (gint)1000;
  argv[ii].int_ret   = (gint)glob_namlvals.fontsize;
  argv[ii].entry_width = 60;
  argv[ii].has_default = TRUE;
  argv[ii].int_default = 24;

  ii++; gap_arr_arg_init(&argv[ii], GAP_ARR_WGT_INT); ii_posx = ii;
  argv[ii].constraint = TRUE;
  argv[ii].label_txt = _("X:");
  argv[ii].help_txt  = _("Position X-offset in pixels");
  argv[ii].int_min   = (gint)-1000;
  argv[ii].int_max   = (gint)10000;
  argv[ii].int_ret   = (gint)glob_namlvals.posx;
  argv[ii].entry_width = 60;
  argv[ii].has_default = TRUE;
  argv[ii].int_default = 15;

  ii++; gap_arr_arg_init(&argv[ii], GAP_ARR_WGT_INT); ii_posy = ii;
  argv[ii].constraint = TRUE;
  argv[ii].label_txt = _("Y:");
  argv[ii].help_txt  = _("Position Y-offset in pixels");
  argv[ii].int_min   = (gint)-1000;
  argv[ii].int_max   = (gint)10000;
  argv[ii].int_ret   = (gint)glob_namlvals.posy;
  argv[ii].entry_width = 60;
  argv[ii].has_default = TRUE;
  argv[ii].int_default = 15;

  ii++; gap_arr_arg_init(&argv[ii], GAP_ARR_WGT_TOGGLE); ii_antialias = ii;
  argv[ii].label_txt = _("Antialias:");
  argv[ii].help_txt  = _("Use antialias");
  argv[ii].int_ret   = (gint)glob_namlvals.antialias;
  argv[ii].has_default = TRUE;
  argv[ii].int_default = 0;

  ii++; gap_arr_arg_init(&argv[ii], GAP_ARR_WGT_TOGGLE); ii_create_new_layer = ii;
  argv[ii].label_txt = _("Create Layer:");
  argv[ii].help_txt  = _("ON: Create a new layer.\n"
                         "OFF: Render on active drawable.");
  argv[ii].int_ret   = (gint)glob_namlvals.antialias;
  argv[ii].has_default = TRUE;
  argv[ii].int_default = 0;


  ii++; gap_arr_arg_init(&argv[ii], GAP_ARR_WGT_DEFAULT_BUTTON);
  argv[ii].label_txt = _("Default");
  argv[ii].help_txt  = _("Reset all parameters to default values");

  ii++; gap_arr_arg_init(&argv[ii], GAP_ARR_WGT_HELP_BUTTON);
  argv[ii].help_id = PLUG_IN_HELP_ID;

  if(TRUE == gap_arr_ok_cancel_dialog(_("Render Filename to Layer"),
                            _("Settings :"),
                            VR_NAME2LAYER_DIALOG_ARGC, argv))
  {
      glob_namlvals.mode             = (gint)(argv[ii_mode].radio_ret);
      glob_namlvals.fontsize         = (gint)(argv[ii_fontsize].int_ret);
      glob_namlvals.posx             = (gint)(argv[ii_posx].int_ret);
      glob_namlvals.posy             = (gint)(argv[ii_posy].int_ret);
      glob_namlvals.antialias        = (gint)(argv[ii_antialias].int_ret);
      glob_namlvals.create_new_layer = (gint)(argv[ii_create_new_layer].int_ret);
      return 0;
  }
  else
  {
      return -1;
  }
}               /* end Naml_dialog */
