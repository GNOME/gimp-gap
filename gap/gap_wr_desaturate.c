/* gap_wr_desaturate.c
 * 2017.03.07 hof (Wolfgang Hofer)
 *
 *  Wrapper Plugin for GIMP desaturate procedure
 *
 *  It provides a primitive Dialog Interface where you
 *  can call the gimp_drawable_desaturate procedure.
 *
 *  Further it has an Interface to 'Run_with_last_values'
 *  and an Iterator Procedure.
 *  (This enables the 'Animated Filter Call' from
 *   the GAP's Menu Filters->Filter all Layers)
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

/* Revision history
 *  (2017/03/07)  v2.8.xx   hof: - created
 */

#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <gtk/gtk.h>

#include <libgimp/gimp.h>
#include <libgimp/gimpui.h>

#include "gap_lastvaldesc.h"
#include "gap_lastvaldesc.h"
#include "gap_libgimpgap.h"

#include "gap-intl.h"

/* Defines */
#define PLUG_IN_NAME        "plug-in-wr-desaturate"
#define PLUG_IN_IMAGE_TYPES "RGB*, GRAY*"
#define PLUG_IN_AUTHOR      "Wolfgang Hofer (hof@gimp.org)"
#define PLUG_IN_COPYRIGHT   "Wolfgang Hofer"
#define PLUG_IN_DESCRIPTION "Wrapper call for GIMP desaturate procedure"

#define PLUG_IN_HELP_ID         "plug-in-wr-desaturate"



typedef struct
{
  gint32    desaturate_mode;
} wr_desaturate_val_t;


typedef struct _WrDialog WrDialog;

struct _WrDialog
{
  gint          run;
  gint          show_progress;
  GtkWidget       *shell;

  GtkWidget       *radio_lightess;
  GtkWidget       *radio_luminosity;
  GtkWidget       *radio_average;

  wr_desaturate_val_t *vals;
};

static wr_desaturate_val_t glob_vals =
{
   GIMP_DESATURATE_LIGHTNESS           /* desaturate_mode */
};




WrDialog *do_dialog (wr_desaturate_val_t *);
static void  query (void);
static void  run(const gchar *name
           , gint nparams
           , const GimpParam *param
           , gint *nreturn_vals
           , GimpParam **return_vals);

/* Global Variables */
GimpPlugInInfo PLUG_IN_INFO =
{
  NULL,   /* init_proc  */
  NULL,   /* quit_proc  */
  query,  /* query_proc */
  run     /* run_proc   */
};


gint  gap_debug = 0;  /* 0.. no debug, 1 .. print debug messages */

/* --------------
 * procedures
 * --------------
 */




static gint32
p_run_desaturate_procedure(gint32 drawable_id, wr_desaturate_val_t *cuvals)
{
  gint32 desaturatedDrawableId;
  if(gap_debug)
  {
     printf("p_run_desaturate_procedure: drawable_id :%d\n", (int)drawable_id);
     printf("p_run_desaturate_procedure:  desaturate_mode:%d\n", (int)cuvals->desaturate_mode);
  }
  
  /* for gimp-2.8.xx use the old name gimp_desaturate_full (that is deprected since gimp-2.9) */
  gimp_desaturate_full(drawable_id, cuvals->desaturate_mode);

// GIMP-2.9 gimp_desaturate_full is
//  gimp_drawable_desaturate(drawable_id, cuvals->desaturate_mode);


  desaturatedDrawableId = drawable_id;
  return (desaturatedDrawableId);                     

}

MAIN ()

static void
query (void)
{
  static GimpLastvalDef lastvals[] =
  {
    GIMP_LASTVALDEF_GINT32          (GIMP_ITER_FALSE,  glob_vals.desaturate_mode,  "desaturate_mode")
  };
  
  /* registration for last values buffer structure (useful for animated filter apply) */
  gimp_lastval_desc_register(PLUG_IN_NAME,
                             &glob_vals,
                             sizeof(glob_vals),
                             G_N_ELEMENTS (lastvals),
                             lastvals);

  static GimpParamDef args[] = {
                  { GIMP_PDB_INT32,      "run_mode", "Interactive, non-interactive"},
                  { GIMP_PDB_IMAGE,      "image", "Input image" },
                  { GIMP_PDB_DRAWABLE,   "drawable", "Input drawable (must be a layer without layermask)"},
                  { GIMP_PDB_INT32,      "desaturate_mode", " DESATURATE-LIGHTNESS (0), DESATURATE-LUMINOSITY (1), DESATURATE-AVERAGE (2)"}
  };
  static int nargs = sizeof(args) / sizeof(args[0]);

  static GimpParamDef return_vals[] =
  {
    { GIMP_PDB_DRAWABLE, "the_drawable", "the handled drawable" }
  };
  static int nreturn_vals = sizeof(return_vals) / sizeof(return_vals[0]);


  static GimpParamDef args_iter[] =
  {
    {GIMP_PDB_INT32, "run_mode", "non-interactive"},
    {GIMP_PDB_INT32, "total_steps", "total number of steps (# of layers-1 to apply the related plug-in)"},
    {GIMP_PDB_FLOAT, "current_step", "current (for linear iterations this is the layerstack position, otherwise some value inbetween)"},
    {GIMP_PDB_INT32, "len_struct", "length of stored data structure with id is equal to the plug_in  proc_name"},
  };
  static int nargs_iter = sizeof(args_iter) / sizeof(args_iter[0]);

  static GimpParamDef *return_iter = NULL;
  static int nreturn_iter = 0;

  gimp_plugin_domain_register (GETTEXT_PACKAGE, LOCALEDIR);

  /* the actual installation of the desaturate wrapper plugin */
  gimp_install_procedure (PLUG_IN_NAME,
                          PLUG_IN_DESCRIPTION,
                         "This Plugin is a wrapper to call the GIMP Desaturate procedure (gimp_drawable_desaturate)"
                         " it has a simplified Dialog (without preview) where you can enter the parameters"
                         " this wrapper is useful for animated filtercalls and provides "
                         " a PDB interface that runs in GIMP_RUN_WITH_LAST_VALUES mode"
                         " and also provides an Iterator Procedure for animated calls"
                          ,
                          PLUG_IN_AUTHOR,
                          PLUG_IN_COPYRIGHT,
                          GAP_VERSION_WITH_DATE,
                          N_("Desaturate..."),
                          PLUG_IN_IMAGE_TYPES,
                          GIMP_PLUGIN,
                          nargs,
                          nreturn_vals,
                          args,
                          return_vals);


  {
    /* Menu names */
    const char *menupath_image_video_layer_colors = N_("<Image>/Video/Layer/Colors/");

    //gimp_plugin_menu_branch_register("<Image>", "Video");
    //gimp_plugin_menu_branch_register("<Image>/Video", "Layer");
    //gimp_plugin_menu_branch_register("<Image>/Video/Layer", "Colors");

    gimp_plugin_menu_register (PLUG_IN_NAME, menupath_image_video_layer_colors);
  }
}


static void
run(const gchar *name
           , gint nparams
           , const GimpParam *param
           , gint *nreturn_vals
           , GimpParam **return_vals)
{
  wr_desaturate_val_t l_cuvals;
  WrDialog   *wcd = NULL;

  gint32    l_image_id = -1;
  gint32    l_drawable_id = -1;
  gint32    l_handled_drawable_id = -1;

  /* Get the runmode from the in-parameters */
  GimpRunMode run_mode = param[0].data.d_int32;


  if(gap_debug)
  {
    printf("START plug-in-wr-desaturate\n");
  }


  /* status variable, use it to check for errors in invocation usualy only
     during non-interactive calling */
  GimpPDBStatusType status = GIMP_PDB_SUCCESS;

  /*always return at least the status to the caller. */
  static GimpParam values[2];


  INIT_I18N();

  /* initialize the return of the status */
  values[0].type = GIMP_PDB_STATUS;
  values[0].data.d_status = status;
  values[1].type = GIMP_PDB_DRAWABLE;
  values[1].data.d_int32 = -1;
  *nreturn_vals = 2;
  *return_vals = values;


  /* get image and drawable */
  l_image_id = param[1].data.d_int32;
  l_drawable_id = param[2].data.d_drawable;

  if(status == GIMP_PDB_SUCCESS)
  {
    /* how are we running today? */
    switch (run_mode)
     {
      case GIMP_RUN_INTERACTIVE:
        /* Initial values */
        l_cuvals.desaturate_mode = GIMP_DESATURATE_LIGHTNESS;

        /* Get information from the dialog */
        wcd = do_dialog(&l_cuvals);
        wcd->show_progress = TRUE;
        break;

      case GIMP_RUN_NONINTERACTIVE:
        /* check to see if invoked with the correct number of parameters */
        if (nparams >= 9)
        {
           wcd = g_malloc (sizeof (WrDialog));
           wcd->run = TRUE;
           wcd->show_progress = FALSE;

           l_cuvals.desaturate_mode  = param[3].data.d_int32;
        }
        else
        {
          status = GIMP_PDB_CALLING_ERROR;
        }
        break;

      case GIMP_RUN_WITH_LAST_VALS:
        wcd = g_malloc (sizeof (WrDialog));
        wcd->run = TRUE;
        wcd->show_progress = TRUE;
        /* Possibly retrieve data from a previous run */
        gimp_get_data (PLUG_IN_NAME, &l_cuvals);
        break;

      default:
        break;
    }
  }

  if (wcd == NULL)
  {
    status = GIMP_PDB_EXECUTION_ERROR;
  }

  if (status == GIMP_PDB_SUCCESS)
  {
     /* Run the main function */
     if(wcd->run)
     {
        gimp_image_undo_group_start (l_image_id);
        l_handled_drawable_id = p_run_desaturate_procedure(l_drawable_id, &l_cuvals);
        gimp_image_undo_group_end (l_image_id);

        /* Store variable states for next run */
        if (run_mode == GIMP_RUN_INTERACTIVE)
        {
          gimp_set_data(PLUG_IN_NAME, &l_cuvals, sizeof(l_cuvals));
        }
     }
     else
     {
       status = GIMP_PDB_EXECUTION_ERROR;       /* dialog ended with cancel button */
     }

     /* If run mode is interactive, flush displays, else (script) don't
        do it, as the screen updates would make the scripts slow */
     if (run_mode != GIMP_RUN_NONINTERACTIVE)
     {
       gimp_displays_flush ();
     }
  }
  values[0].data.d_status = status;
  values[1].data.d_int32 = l_handled_drawable_id;   /* return the id of handled layer */

}       /* end run */


/*
 * DIALOG and callback stuff
 */


static void
radio_callback(GtkWidget *wgt, gpointer user_data)
{
  WrDialog *wcd;

  if(gap_debug) printf("radio_callback: START\n");
  wcd = (WrDialog*)user_data;
  if(wcd != NULL)
  {
    if(wcd->vals != NULL)
    {
       if(wgt == wcd->radio_lightess)    { wcd->vals->desaturate_mode = GIMP_DESATURATE_LIGHTNESS; }
       if(wgt == wcd->radio_luminosity)  { wcd->vals->desaturate_mode = GIMP_DESATURATE_LUMINOSITY; }
       if(wgt == wcd->radio_average)     { wcd->vals->desaturate_mode = GIMP_DESATURATE_AVERAGE; }

       if(gap_debug)
       {
         printf("radio_callback: value: %d\n", (int)wcd->vals->desaturate_mode);
       }
    }
  }
}


/* ---------------------------------
 * wr_desaturate_response
 * ---------------------------------
 */
static void
wr_desaturate_response (GtkWidget *widget,
                 gint       response_id,
                 WrDialog *wcd)
{
  GtkWidget *dialog;

  switch (response_id)
  {
    case GTK_RESPONSE_OK:
      if(wcd)
      {
        if (GTK_WIDGET_VISIBLE (wcd->shell))
          gtk_widget_hide (wcd->shell);

        wcd->run = TRUE;
      }

    default:
      dialog = NULL;
      if(wcd)
      {
        dialog = wcd->shell;
        if(dialog)
        {
          wcd->shell = NULL;
          gtk_widget_destroy (dialog);
        }
      }
      gtk_main_quit ();
      break;
  }
}  /* end wr_desaturate_response */


WrDialog *
do_dialog (wr_desaturate_val_t *cuvals)
{
  WrDialog *wcd;
  GtkWidget  *vbox;

  GtkWidget *dialog1;
  GtkWidget *dialog_vbox1;
  GtkWidget *frame1;
  GtkWidget *hbox1;
  GtkWidget *vbox1;
  GtkWidget *label1;
  GSList *vbox1_group = NULL;
  GtkWidget *radiobutton1;
  GtkWidget *radiobutton2;
  GtkWidget *radiobutton3;
  GtkWidget *table1;
  GtkWidget *dialog_action_area1;


  /* Init UI  */
  gimp_ui_init ("wr_desaturate", FALSE);

  /*  The dialog1  */
  wcd = g_malloc (sizeof (WrDialog));
  wcd->run = FALSE;
  wcd->vals = cuvals;

  /*  The dialog1 and main vbox  */
  dialog1 = gimp_dialog_new (_("Desaturate"), "desaturate_wrapper",
                               NULL, 0,
                               gimp_standard_help_func, PLUG_IN_HELP_ID,

                               GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                               GTK_STOCK_OK,     GTK_RESPONSE_OK,
                               NULL);
  wcd->shell = dialog1;


  g_signal_connect (G_OBJECT (dialog1), "response",
                    G_CALLBACK (wr_desaturate_response),
                    wcd);

  /* the vbox */
  vbox = gtk_vbox_new (FALSE, 2);
  gtk_container_set_border_width (GTK_CONTAINER (vbox), 2);
  gtk_box_pack_start (GTK_BOX (GTK_DIALOG (dialog1)->vbox), vbox,
                      TRUE, TRUE, 0);
  gtk_widget_show (vbox);

  dialog_vbox1 = GTK_DIALOG (dialog1)->vbox;
  gtk_widget_show (dialog_vbox1);



  /* the frame */
  frame1 = gimp_frame_new (_("Choose shade of gray based on:"));
  gtk_widget_show (frame1);
  gtk_box_pack_start (GTK_BOX (dialog_vbox1), frame1, TRUE, TRUE, 0);
  gtk_container_set_border_width (GTK_CONTAINER (frame1), 2);

  hbox1 = gtk_hbox_new (FALSE, 0);
  gtk_widget_show (hbox1);
  gtk_container_add (GTK_CONTAINER (frame1), hbox1);
  gtk_container_set_border_width (GTK_CONTAINER (hbox1), 4);

  vbox1 = gtk_vbox_new (FALSE, 0);
  gtk_widget_show (vbox1);
  gtk_box_pack_start (GTK_BOX (hbox1), vbox1, TRUE, TRUE, 0);


  /* Shades the label */
  label1 = gtk_label_new (_("Shades:"));
  gtk_widget_show (label1);
  gtk_box_pack_start (GTK_BOX (vbox1), label1, FALSE, FALSE, 0);
  gtk_misc_set_alignment (GTK_MISC (label1), 0, 0.5);


  /* Shades the radio buttons */
  radiobutton1 = gtk_radio_button_new_with_label (vbox1_group, _("Lightness"));
  vbox1_group = gtk_radio_button_get_group (GTK_RADIO_BUTTON (radiobutton1));
  gtk_widget_show (radiobutton1);
  gtk_box_pack_start (GTK_BOX (vbox1), radiobutton1, FALSE, FALSE, 0);

  radiobutton2 = gtk_radio_button_new_with_label (vbox1_group, _("Luminosity"));
  vbox1_group = gtk_radio_button_get_group (GTK_RADIO_BUTTON (radiobutton2));
  gtk_widget_show (radiobutton2);
  gtk_box_pack_start (GTK_BOX (vbox1), radiobutton2, FALSE, FALSE, 0);

  radiobutton3 = gtk_radio_button_new_with_label (vbox1_group, _("Average"));
  vbox1_group = gtk_radio_button_get_group (GTK_RADIO_BUTTON (radiobutton3));
  gtk_widget_show (radiobutton3);
  gtk_box_pack_start (GTK_BOX (vbox1), radiobutton3, FALSE, FALSE, 0);


  dialog_action_area1 = GTK_DIALOG (dialog1)->action_area;
  gtk_widget_show (dialog_action_area1);
  gtk_container_set_border_width (GTK_CONTAINER (dialog_action_area1), 10);



  wcd->radio_lightess  = radiobutton1;
  wcd->radio_luminosity = radiobutton2;
  wcd->radio_average   = radiobutton3;


  /* signals */
  g_signal_connect (G_OBJECT (wcd->radio_lightess),  "clicked",  G_CALLBACK (radio_callback), wcd);
  g_signal_connect (G_OBJECT (wcd->radio_luminosity),     "clicked",  G_CALLBACK (radio_callback), wcd);
  g_signal_connect (G_OBJECT (wcd->radio_average),   "clicked",  G_CALLBACK (radio_callback), wcd);


  gtk_widget_show (dialog1);

  gtk_main ();
  gdk_flush ();


  return wcd;
}
