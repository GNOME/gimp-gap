/*  gap_wr_layermode.c
 *    wrapper plugin to set Layermode by Wolfgang Hofer.
 *  2013/05/02
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
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

/* Revision history
 *  (2013/05/02)  v1.0       hof: created
 */

#include "config.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <gtk/gtk.h>
#include <libgimp/gimp.h>
#include <libgimp/gimpui.h>

#include "gap_lastvaldesc.h"

#include "gap-intl.h"

/* Defines */
#define PLUG_IN_NAME        "plug-in-wr-set-layermode"
#define PLUG_IN_BINARY      "wr_layermode"
#define PLUG_IN_PRINT_NAME  "Set Layermode"
#define PLUG_IN_IMAGE_TYPES "RGB*, INDEXED*, GRAY*"
#define PLUG_IN_AUTHOR      "Wolfgang Hofer (hof@gimp.org)"
#define PLUG_IN_COPYRIGHT   "Wolfgang Hofer"

int gap_debug = 0;  /* 1 == print debug infos , 0 dont print debug infos */


typedef struct {
  gint32  mode;
} LayermodeValues;



static LayermodeValues glob_vals =
{
  0           /* 0..GIMP_NORMAL_MODE */
};


static void  query (void);
static void  run (const gchar *name,          /* name of plugin */
     gint nparams,               /* number of in-paramters */
     const GimpParam * param,    /* in-parameters */
     gint *nreturn_vals,         /* number of out-parameters */
     GimpParam ** return_vals);  /* out-parameters */

static gint  p_layermode_run (gint32 image_id, gint32 drawable_id);
static gint  p_layermode_dialog (void);


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
                  { GIMP_PDB_DRAWABLE, "drawable", "Input drawable (must be a layer)"},
                  { GIMP_PDB_INT32,    "mode", "The new layer combination mode { NORMAL-MODE (0), DISSOLVE-MODE (1), BEHIND-MODE (2), MULTIPLY-MODE (3), SCREEN-MODE (4), OVERLAY-MODE (5), DIFFERENCE-MODE (6), ADDITION-MODE (7), SUBTRACT-MODE (8), DARKEN-ONLY-MODE (9), LIGHTEN-ONLY-MODE (10), HUE-MODE (11), SATURATION-MODE (12), COLOR-MODE (13), VALUE-MODE (14), DIVIDE-MODE (15), DODGE-MODE (16), BURN-MODE (17), HARDLIGHT-MODE (18), SOFTLIGHT-MODE (19), GRAIN-EXTRACT-MODE (20), GRAIN-MERGE-MODE (21), COLOR-ERASE-MODE (22), ERASE-MODE (23), REPLACE-MODE (24), ANTI-ERASE-MODE (25) }"},
  };

static GimpParamDef return_vals[] = {
    { GIMP_PDB_DRAWABLE, "the_drawable", "the handled drawable" }
};

static gint global_number_in_args = G_N_ELEMENTS (in_args);
static gint global_number_out_args = G_N_ELEMENTS (return_vals);


/* Functions */

MAIN ()

static void query (void)
{
  static GimpLastvalDef lastvals[] =
  {
    GIMP_LASTVALDEF_GINT32          (GIMP_ITER_FALSE,  glob_vals.mode,     "mode"),
  };



  gimp_plugin_domain_register (GETTEXT_PACKAGE, LOCALEDIR);

  /* registration for last values buffer structure (useful for animated filter apply) */
  gimp_lastval_desc_register(PLUG_IN_NAME,
                             &glob_vals,
                             sizeof(glob_vals),
                             G_N_ELEMENTS (lastvals),
                             lastvals);

  /* the actual installation of the plugin */
  gimp_install_procedure (PLUG_IN_NAME,
                          "Set Layer combination mode",
                          "This plug-in is a wrapper for gimp_layer_set_mode functionality, "
                          "and provides the typical PDB interface for calling it as "
                          "filter for the use with GIMP-GAP filtermacro and Video Frame manipulation",
                          PLUG_IN_AUTHOR,
                          PLUG_IN_COPYRIGHT,
                          GAP_VERSION_WITH_DATE,
                          N_("Set Layer Mode..."),
                          PLUG_IN_IMAGE_TYPES,
                          GIMP_PLUGIN,
                          global_number_in_args,
                          global_number_out_args,
                          in_args,
                          return_vals);
  {
    /* Menu names */
    const char *menupath_image_video_layer_attr = N_("<Image>/Video/Layer/Attributes/");

    gimp_plugin_menu_register (PLUG_IN_NAME, menupath_image_video_layer_attr);
  }

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

  l_env = g_getenv("GAP_DEBUG");
  if(l_env != NULL)
  {
    if((*l_env != 'n') && (*l_env != 'N')) gap_debug = 1;
  }

  if(gap_debug) printf("\n\nDEBUG: run %s\n", name);

  /* initialize the return of the status */
  values[0].type = GIMP_PDB_STATUS;
  values[0].data.d_status = status;
  values[1].type = GIMP_PDB_DRAWABLE;
  values[1].data.d_drawable = -1;
  *nreturn_vals = 2;
  *return_vals = values;


  /* get image and drawable */
  image_id = param[1].data.d_int32;


  /* how are we running today? */
  switch (run_mode)
  {
    case GIMP_RUN_INTERACTIVE:
      /* Possibly retrieve data from a previous run */
      gimp_get_data (PLUG_IN_NAME, &glob_vals);

      /* Get information from the dialog */
      if (!p_layermode_dialog())
      {
        return;
      }
      break;

    case GIMP_RUN_NONINTERACTIVE:
      /* check to see if invoked with the correct number of parameters */
      if (nparams == global_number_in_args)
      {
          glob_vals.mode        = (gint32)  param[3].data.d_int32;
      }
      else
      {
        status = GIMP_PDB_CALLING_ERROR;
      }

      break;

    case GIMP_RUN_WITH_LAST_VALS:
      /* Possibly retrieve data from a previous run */
      gimp_get_data (PLUG_IN_NAME, &glob_vals);

      break;

    default:
      break;
  }

  if (status == GIMP_PDB_SUCCESS)
  {
    /* Run the main function */
    values[1].data.d_drawable = p_layermode_run(image_id, param[2].data.d_drawable);
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
      gimp_set_data (PLUG_IN_NAME, &glob_vals, sizeof (LayermodeValues));
  }
  values[0].data.d_status = status;
}       /* end run */



/* ============================================================================
 * p_layermode_run
 *        The main function
 * ============================================================================
 */
static gint32
p_layermode_run (gint32 image_id, gint32 drawable_id)
{
  if(gap_debug)
  {
    printf("mode: %d\n", (int)glob_vals.mode);
  }

  gimp_layer_set_mode(drawable_id, glob_vals.mode);

  return (drawable_id);
}       /* end p_layermode_run */




/* ------------- Dialog stuff --------------- */



static void
p_layermode_menu_callback (GtkWidget *widget,  void *dataPtr)
{
  gint value;

  gimp_int_combo_box_get_active (GIMP_INT_COMBO_BOX (widget), &value);

  glob_vals.mode = value;
}

/* ------------------
 * p_layermode_dialog
 * ------------------
 *   return  TRUE (OK, run the processing)
 *           FALSE (in case of Error or cancel)
 */
static gboolean
p_layermode_dialog(void)
{
  GtkWidget *dialog;
  GtkWidget *main_vbox;
  GtkWidget *frame;
  GtkWidget *table;
  GtkWidget *label;
  GtkWidget *combo;
  gboolean   run;
  gint       rc;

  gimp_ui_init (PLUG_IN_BINARY, FALSE);

  dialog = gimp_dialog_new (_("LAYERMODE"), PLUG_IN_BINARY,
                            NULL, 0,
                            gimp_standard_help_func, PLUG_IN_NAME,

                            GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                            GTK_STOCK_OK,     GTK_RESPONSE_OK,

                            NULL);

  gtk_dialog_set_alternative_button_order (GTK_DIALOG (dialog),
                                           GTK_RESPONSE_OK,
                                           GTK_RESPONSE_CANCEL,
                                           -1);

  gimp_window_set_transient (GTK_WINDOW (dialog));

  /*  The main vbox  */
  main_vbox = gtk_vbox_new (FALSE, 12);
  gtk_container_set_border_width (GTK_CONTAINER (main_vbox), 12);
  gtk_container_add (GTK_CONTAINER (GTK_DIALOG (dialog)->vbox), main_vbox);
  gtk_widget_show (main_vbox);

  /*  Options section  */
  frame = gimp_frame_new ( _("Options"));
  gtk_widget_show (frame);
  gtk_box_pack_start (GTK_BOX (main_vbox), frame, FALSE, FALSE, 0);

  /* The table to hold the frame of options */
  table = gtk_table_new (2, 2, FALSE);
  gtk_table_set_col_spacings (GTK_TABLE (table), 12);
  gtk_table_set_row_spacings (GTK_TABLE (table), 12);
  gtk_container_add (GTK_CONTAINER (frame), table);
 

  /* Paintmode combo (menu) */

  label = gtk_label_new( _("Mode:"));
  gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
  gtk_table_attach(GTK_TABLE(table), label, 0, 1, 0, 1, GTK_FILL, 0, 4, 0);
  gtk_widget_show(label);

  combo = gimp_int_combo_box_new (_("Normal"),         GIMP_NORMAL_MODE,
                                   _("Dissolve"),       GIMP_DISSOLVE_MODE,
                                   _("Behind"),         GIMP_BEHIND_MODE,
                                   _("Multiply"),       GIMP_MULTIPLY_MODE,
                                   _("Divide"),         GIMP_DIVIDE_MODE,
                                   _("Screen"),         GIMP_SCREEN_MODE,
                                   _("Overlay"),        GIMP_OVERLAY_MODE,
                                   _("Dodge"),          GIMP_DODGE_MODE,
                                   _("Burn"),           GIMP_BURN_MODE,
                                   _("Hard Light"),     GIMP_HARDLIGHT_MODE,
                                   _("Soft Light"),     GIMP_SOFTLIGHT_MODE,
                                   _("Grain Extract"),  GIMP_GRAIN_EXTRACT_MODE,
                                   _("Grain Merge"),    GIMP_GRAIN_MERGE_MODE,
                                   _("Difference"),     GIMP_DIFFERENCE_MODE,
                                   _("Addition"),       GIMP_ADDITION_MODE,
                                   _("Subtract"),       GIMP_SUBTRACT_MODE,
                                   _("Darken Only"),    GIMP_DARKEN_ONLY_MODE,
                                   _("Lighten Only"),   GIMP_LIGHTEN_ONLY_MODE,
                                   _("Hue"),            GIMP_HUE_MODE,
                                   _("Saturation"),     GIMP_SATURATION_MODE,
                                   _("Color"),          GIMP_COLOR_MODE,
                                   _("Color Erase"),    GIMP_COLOR_ERASE_MODE,
                                   _("Value"),          GIMP_VALUE_MODE,
                                   NULL);

  {
    gint initialValue;
    initialValue = GIMP_NORMAL_MODE;


    gimp_int_combo_box_connect (GIMP_INT_COMBO_BOX (combo),
                              initialValue,
                              G_CALLBACK (p_layermode_menu_callback),
                              NULL);

  }

  gtk_table_attach(GTK_TABLE(table), combo, 1, 2, 0, 1,
                   GTK_EXPAND | GTK_FILL, 0, 0, 0);
  gimp_help_set_help_data(combo,
                       _("Paintmode")
                       , NULL);
  gtk_widget_show(combo);



  gtk_widget_show (frame);
  gtk_widget_show (table);
  gtk_widget_show (dialog);

  rc = gimp_dialog_run (GIMP_DIALOG (dialog));
  
  if(gap_debug)
  {
    printf("rc:%d GTK_RESPONSE_OK:%d\n", (int)rc, (int)GTK_RESPONSE_OK);
  }
  
  run = (rc == GTK_RESPONSE_OK);

  gtk_widget_destroy (dialog);

  return run;
}
