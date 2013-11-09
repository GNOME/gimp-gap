/* gap_wr_color_levels.c
 * 2002.Oct.27 hof (Wolfgang Hofer)
 *
 *  Wrapper Plugin for GIMP Color Levels tool
 *
 * Warning: This is just a QUICK HACK to enable
 *          Animated Filterapply in conjunction with the
 *          GIMP Color Levels Tool.
 *
 *  It provides a primitive Dialog Interface where you
 *  can call the Color Levels Tool
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
 * along with this program; if not, write to the Free Software
 * <http://www.gnu.org/licenses/>.
 */

/* Revision history
 *  (2004/01/15)  v1.3.24a  hof: adapted for gimp-1.3.x and gtk+2.2 API
 *  (2002/10/27)  v1.03     hof: - created
 */

#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <gtk/gtk.h>

#include <libgimp/gimp.h>
#include <libgimp/gimpui.h>

#include "gap-intl.h"

/* Defines */
#define PLUG_IN_NAME        "plug-in-wr-color-levels"
#define PLUG_IN_IMAGE_TYPES "RGB*, GRAY*"
#define PLUG_IN_AUTHOR      "Wolfgang Hofer (hof@gimp.org)"
#define PLUG_IN_COPYRIGHT   "Wolfgang Hofer"
#define PLUG_IN_DESCRIPTION "Wrapper call for GIMP Levels Color Tool"

#define PLUG_IN_ITER_NAME       "plug-in-wr-color-levels-Iterator"
#define PLUG_IN_DATA_ITER_FROM  "plug-in-wr-color-levels-ITER-FROM"
#define PLUG_IN_DATA_ITER_TO    "plug-in-wr-color-levels-ITER-TO"
#define PLUG_IN_HELP_ID         "plug-in-wr-color-levels"



typedef struct
{
  gint32    channel;
  gint32    low_input;
  gint32    high_input;
  gdouble   gamma;
  gint32    low_output;
  gint32    high_output;
} wr_levels_val_t;


typedef struct _WrDialog WrDialog;

struct _WrDialog
{
  gint          run;
  gint          show_progress;
  GtkWidget       *shell;
  GtkWidget       *radio_master;
  GtkWidget       *radio_red;
  GtkWidget       *radio_green;
  GtkWidget       *radio_blue;
  GtkWidget       *radio_alpha;
  wr_levels_val_t *vals;
};


WrDialog *do_dialog (wr_levels_val_t *);
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



/*
 * Delta Calculations for the iterator
 */
static void
p_delta_gdouble (gdouble  *val,
                 gdouble   val_from,
                 gdouble   val_to,
                 gint32   total_steps,
                 gdouble  current_step)
{
    gdouble     delta;

    if(total_steps < 1) return;

    delta = ((gdouble)(val_to - val_from) / (gdouble)total_steps) * ((gdouble)total_steps - current_step);
    *val  = val_from + delta;
}

static void
p_delta_gint32 (gint32  *val,
                 gint32   val_from,
                 gint32   val_to,
                 gint32   total_steps,
                 gdouble  current_step)
{
    gdouble     delta;

    if(total_steps < 1) return;

    delta = ((gdouble)(val_to - val_from) / (gdouble)total_steps) * ((gdouble)total_steps - current_step);
    *val  = val_from + delta;
}

/* ============================================================================
 * p_gimp_levels
 *
 * ============================================================================
 */

gint32
p_gimp_levels (gint32 drawable_id,
               gint32 channel,
               gint32 low_input,
               gint32 high_input,
               gdouble gamma,
               gint32 low_output,
               gint32 high_output
                   )
{
   static char     *l_procname = "gimp_levels";
   GimpParam          *return_vals;
   int              nreturn_vals;

   return_vals = gimp_run_procedure (l_procname,
                                 &nreturn_vals,
                                 GIMP_PDB_DRAWABLE,  drawable_id,
                                 GIMP_PDB_INT32,     channel,
                                 GIMP_PDB_INT32,     low_input,
                                 GIMP_PDB_INT32,     high_input,
                                 GIMP_PDB_FLOAT,     gamma,
                                 GIMP_PDB_INT32,     low_output,
                                 GIMP_PDB_INT32,     high_output,
                                  GIMP_PDB_END);

   if (return_vals[0].data.d_status == GIMP_PDB_SUCCESS)
   {
      gimp_destroy_params(return_vals, nreturn_vals);
      return (0);
   }
   gimp_destroy_params(return_vals, nreturn_vals);
   printf("Error: PDB call of %s failed status:%d\n", l_procname, (int)return_vals[0].data.d_status);
   return(-1);
}       /* end p_gimp_levels */

static void
p_run_levels_tool(gint32 drawable_id, wr_levels_val_t *cuvals)
{
  if(gap_debug)
  {
     printf("p_run_levels_tool: drawable_id :%d\n", (int)drawable_id);
     printf("p_run_levels_tool:  channel:%d\n", (int)cuvals->channel);
     printf("p_run_levels_tool:  low_input:%d\n", (int)cuvals->low_input);
     printf("p_run_levels_tool:  high_input:%d\n", (int)cuvals->high_input);
     printf("p_run_levels_tool:  gamma:%f\n", (float)cuvals->gamma);
     printf("p_run_levels_tool:  low_output:%d\n", (int)cuvals->low_output);
     printf("p_run_levels_tool:  high_output:%d\n", (int)cuvals->high_output);
  }
  p_gimp_levels(drawable_id
                       , cuvals->channel
                       , cuvals->low_input
                       , cuvals->high_input
                       , cuvals->gamma
                       , cuvals->low_output
                       , cuvals->high_output
                       );

}

MAIN ()

static void
query (void)
{
  static GimpParamDef args[] = {
                  { GIMP_PDB_INT32,      "run_mode", "Interactive, non-interactive"},
                  { GIMP_PDB_IMAGE,      "image", "Input image" },
                  { GIMP_PDB_DRAWABLE,   "drawable", "Input drawable (must be a layer without layermask)"},
                  { GIMP_PDB_INT32,      "channel", " affected channel: VALUE_LUT (0), RED_LUT(1), GREENLUT(2), BLUE_LUT(3), ALPHA_LUT(4)"},
                  { GIMP_PDB_INT32,      "low_input", "Intensity of lowest input (0 <= low_input <= 255) "},
                  { GIMP_PDB_INT32,      "high_input", "Intensity of highest input (0 <= highest_input <= 255) "},
                  { GIMP_PDB_FLOAT,      "gamma", "Gamma correction factor (0.1 <= gamma >= 10"},
                  { GIMP_PDB_INT32,      "low_output", "Intensity of lowest output (0 <= low_output <= 255) "},
                  { GIMP_PDB_INT32,      "high_output", "Intensity of highest output (0 <= highest_output <= 255) "},
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

  /* the actual installation of the bend plugin */
  gimp_install_procedure (PLUG_IN_NAME,
                          PLUG_IN_DESCRIPTION,
                         "This Plugin is a wrapper to call the GIMP Levels Color Tool (gimp_levels)"
                         " it has a simplified Dialog (without preview) where you can enter the parameters"
                         " this wrapper is useful for animated filtercalls and provides "
                         " a PDB interface that runs in GIMP_RUN_WITH_LAST_VALUES mode"
                         " and also provides an Iterator Procedure for animated calls"
                          ,
                          PLUG_IN_AUTHOR,
                          PLUG_IN_COPYRIGHT,
                          GAP_VERSION_WITH_DATE,
                          N_("Levels..."),
                          PLUG_IN_IMAGE_TYPES,
                          GIMP_PLUGIN,
                          nargs,
                          nreturn_vals,
                          args,
                          return_vals);


  /* the installation of the Iterator extension for the bend plugin */
  gimp_install_procedure (PLUG_IN_ITER_NAME,
                          "This extension calculates the modified values for one iterationstep for the call of plug_in_curve_bend",
                          "",
                          PLUG_IN_AUTHOR,
                          PLUG_IN_COPYRIGHT,
                          GAP_VERSION_WITH_DATE,
                          NULL,    /* do not appear in menus */
                          NULL,
                          GIMP_PLUGIN,
                          nargs_iter, nreturn_iter,
                          args_iter, return_iter);
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
  wr_levels_val_t l_cuvals;
  WrDialog   *wcd = NULL;

  gint32    l_image_id = -1;
  gint32    l_drawable_id = -1;
  gint32    l_handled_drawable_id = -1;

  /* Get the runmode from the in-parameters */
  GimpRunMode run_mode = param[0].data.d_int32;


  if(gap_debug)
  {
    printf("START plug-in-wr-color-levels\n");
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

  /* the Iterator Stuff */
  if (strcmp (name, PLUG_IN_ITER_NAME) == 0)
  {
     gint32  len_struct;
     gint32  total_steps;
     gdouble current_step;
     wr_levels_val_t   cval;                  /* current values while iterating */
     wr_levels_val_t   cval_from, cval_to;    /* start and end values */

     /* Iterator procedure for animated calls is usually called from
      * "plug_in_gap_layers_run_animfilter"
      * (always run noninteractive)
      */
     if ((run_mode == GIMP_RUN_NONINTERACTIVE) && (nparams == 4))
     {
       total_steps  =  param[1].data.d_int32;
       current_step =  param[2].data.d_float;
       len_struct   =  param[3].data.d_int32;

       if(len_struct == sizeof(cval))
       {
         /* get _FROM and _TO data,
          * This data was stored by plug_in_gap_layers_run_animfilter
          */
          gimp_get_data(PLUG_IN_DATA_ITER_FROM, &cval_from);
          gimp_get_data(PLUG_IN_DATA_ITER_TO,   &cval_to);
          memcpy(&cval, &cval_from, sizeof(cval));

          /* do not iterate channel ! (makes no sense) */
          p_delta_gint32 (&cval.low_input,  cval_from.low_input,  cval_to.low_input,  total_steps, current_step);
          p_delta_gint32 (&cval.high_input,  cval_from.high_input,  cval_to.high_input,  total_steps, current_step);
          p_delta_gdouble(&cval.gamma, cval_from.gamma, cval_to.gamma, total_steps, current_step);
          p_delta_gint32 (&cval.low_output,  cval_from.low_output,  cval_to.low_output,  total_steps, current_step);
          p_delta_gint32 (&cval.high_output,  cval_from.high_output,  cval_to.high_output,  total_steps, current_step);

          gimp_set_data(PLUG_IN_NAME, &cval, sizeof(cval));
       }
       else status = GIMP_PDB_CALLING_ERROR;
     }
     else status = GIMP_PDB_CALLING_ERROR;

     values[0].data.d_status = status;
     return;
  }



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
        l_cuvals.channel = 0;
        l_cuvals.low_input = 0;
        l_cuvals.high_input = 255;
        l_cuvals.gamma = 1.0;
        l_cuvals.low_output = 0;
        l_cuvals.high_output = 255;

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

           l_cuvals.channel  = param[3].data.d_int32;
           l_cuvals.low_input  = param[4].data.d_int32;
           l_cuvals.high_input  = param[5].data.d_int32;
           l_cuvals.gamma = param[6].data.d_float;
           l_cuvals.low_output  = param[7].data.d_int32;
           l_cuvals.high_output  = param[8].data.d_int32;
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
        p_run_levels_tool(l_drawable_id, &l_cuvals);
        l_handled_drawable_id = l_drawable_id;
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
       if(wgt == wcd->radio_alpha)    { wcd->vals->channel = 4; }
       if(wgt == wcd->radio_blue)     { wcd->vals->channel = 3; }
       if(wgt == wcd->radio_green)    { wcd->vals->channel = 2; }
       if(wgt == wcd->radio_red)      { wcd->vals->channel = 1; }
       if(wgt == wcd->radio_master)   { wcd->vals->channel = 0; }

       if(gap_debug) printf("radio_callback: value: %d\n", (int)wcd->vals->channel);
    }
  }
}


/* ---------------------------------
 * wr_levels_response
 * ---------------------------------
 */
static void
wr_levels_response (GtkWidget *widget,
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
}  /* end wr_levels_response */


WrDialog *
do_dialog (wr_levels_val_t *cuvals)
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
  GtkWidget *radiobutton4;
  GtkWidget *radiobutton5;
  GtkWidget *table1;
  GtkWidget *label2;
  GtkWidget *label3;
  GtkWidget *label4;
  GtkWidget *label5;
  GtkWidget *label6;
  GtkObject *spinbutton_low_input_adj;
  GtkWidget *spinbutton_low_input;
  GtkObject *spinbutton_high_input_adj;
  GtkWidget *spinbutton_high_input;
  GtkObject *spinbutton_gamma_adj;
  GtkWidget *spinbutton_gamma;
  GtkObject *spinbutton_low_output_adj;
  GtkWidget *spinbutton_low_output;
  GtkObject *spinbutton_high_output_adj;
  GtkWidget *spinbutton_high_output;
  GtkWidget *dialog_action_area1;


  /* Init UI  */
  gimp_ui_init ("wr_curves", FALSE);

  /*  The dialog1  */
  wcd = g_malloc (sizeof (WrDialog));
  wcd->run = FALSE;
  wcd->vals = cuvals;

  /*  The dialog1 and main vbox  */
  dialog1 = gimp_dialog_new (_("Color Levels"), "levels_wrapper",
                               NULL, 0,
                               gimp_standard_help_func, PLUG_IN_HELP_ID,

                               GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                               GTK_STOCK_OK,     GTK_RESPONSE_OK,
                               NULL);
  wcd->shell = dialog1;


  g_signal_connect (G_OBJECT (dialog1), "response",
                    G_CALLBACK (wr_levels_response),
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
  frame1 = gimp_frame_new (_("Color Levels  Adjustments "));
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


  /* Channel the label */
  label1 = gtk_label_new (_("Channel:"));
  gtk_widget_show (label1);
  gtk_box_pack_start (GTK_BOX (vbox1), label1, FALSE, FALSE, 0);
  gtk_misc_set_alignment (GTK_MISC (label1), 0, 0.5);


  /* Channel the radio buttons */
  radiobutton1 = gtk_radio_button_new_with_label (vbox1_group, _("Master"));
  vbox1_group = gtk_radio_button_get_group (GTK_RADIO_BUTTON (radiobutton1));
  gtk_widget_show (radiobutton1);
  gtk_box_pack_start (GTK_BOX (vbox1), radiobutton1, FALSE, FALSE, 0);

  radiobutton2 = gtk_radio_button_new_with_label (vbox1_group, _("Red"));
  vbox1_group = gtk_radio_button_get_group (GTK_RADIO_BUTTON (radiobutton2));
  gtk_widget_show (radiobutton2);
  gtk_box_pack_start (GTK_BOX (vbox1), radiobutton2, FALSE, FALSE, 0);

  radiobutton3 = gtk_radio_button_new_with_label (vbox1_group, _("Green"));
  vbox1_group = gtk_radio_button_get_group (GTK_RADIO_BUTTON (radiobutton3));
  gtk_widget_show (radiobutton3);
  gtk_box_pack_start (GTK_BOX (vbox1), radiobutton3, FALSE, FALSE, 0);

  radiobutton4 = gtk_radio_button_new_with_label (vbox1_group, _("Blue"));
  vbox1_group = gtk_radio_button_get_group (GTK_RADIO_BUTTON (radiobutton4));
  gtk_widget_show (radiobutton4);
  gtk_box_pack_start (GTK_BOX (vbox1), radiobutton4, FALSE, FALSE, 0);

  radiobutton5 = gtk_radio_button_new_with_label (vbox1_group, _("Alpha"));
  vbox1_group = gtk_radio_button_get_group (GTK_RADIO_BUTTON (radiobutton5));
  gtk_widget_show (radiobutton5);
  gtk_box_pack_start (GTK_BOX (vbox1), radiobutton5, FALSE, FALSE, 0);


  /* table1 for spinbuttons  */
  table1 = gtk_table_new (6, 2, FALSE);
  gtk_widget_show (table1);
  gtk_box_pack_start (GTK_BOX (hbox1), table1, TRUE, TRUE, 0);
  gtk_container_set_border_width (GTK_CONTAINER (table1), 4);

  label2 = gtk_label_new (_("Low Input:"));
  gtk_widget_show (label2);
  gtk_table_attach (GTK_TABLE (table1), label2, 0, 1, 1, 2,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);
  gtk_misc_set_alignment (GTK_MISC (label2), 0, 0.5);

  label3 = gtk_label_new (_("High Input:"));
  gtk_widget_show (label3);
  gtk_table_attach (GTK_TABLE (table1), label3, 0, 1, 2, 3,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);
  gtk_misc_set_alignment (GTK_MISC (label3), 0, 0.5);

  label4 = gtk_label_new (_("Gamma:"));
  gtk_widget_show (label4);
  gtk_table_attach (GTK_TABLE (table1), label4, 0, 1, 3, 4,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);
  gtk_misc_set_alignment (GTK_MISC (label4), 0, 0.5);

  label5 = gtk_label_new (_("Low Output:"));
  gtk_widget_show (label5);
  gtk_table_attach (GTK_TABLE (table1), label5, 0, 1, 4, 5,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);
  gtk_misc_set_alignment (GTK_MISC (label5), 0, 0.5);

  label6 = gtk_label_new (_("High Output:"));
  gtk_widget_show (label6);
  gtk_table_attach (GTK_TABLE (table1), label6, 0, 1, 5, 6,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);
  gtk_misc_set_alignment (GTK_MISC (label6), 0, 0.5);


  /* SPINBUTTONS */
  spinbutton_low_input_adj = gtk_adjustment_new (0, 0, 255, 1, 10, 0);
  spinbutton_low_input = gtk_spin_button_new (GTK_ADJUSTMENT (spinbutton_low_input_adj), 1, 0);
  gtk_widget_show (spinbutton_low_input);
  gtk_table_attach (GTK_TABLE (table1), spinbutton_low_input, 1, 2, 1, 2,
                    (GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);

  spinbutton_high_input_adj = gtk_adjustment_new (255, 0, 255, 1, 10, 0);
  spinbutton_high_input = gtk_spin_button_new (GTK_ADJUSTMENT (spinbutton_high_input_adj), 1, 0);
  gtk_widget_show (spinbutton_high_input);
  gtk_table_attach (GTK_TABLE (table1), spinbutton_high_input, 1, 2, 2, 3,
                    (GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);

  spinbutton_gamma_adj = gtk_adjustment_new (1, 0.1, 10, 0.1, 1, 0);
  spinbutton_gamma = gtk_spin_button_new (GTK_ADJUSTMENT (spinbutton_gamma_adj), 0.1, 2);
  gtk_widget_show (spinbutton_gamma);
  gtk_table_attach (GTK_TABLE (table1), spinbutton_gamma, 1, 2, 3, 4,
                    (GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);


  spinbutton_low_output_adj = gtk_adjustment_new (0, 0, 255, 1, 10, 0);
  spinbutton_low_output = gtk_spin_button_new (GTK_ADJUSTMENT (spinbutton_low_output_adj), 1, 0);
  gtk_widget_show (spinbutton_low_output);
  gtk_table_attach (GTK_TABLE (table1), spinbutton_low_output, 1, 2, 4, 5,
                    (GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);

  spinbutton_high_output_adj = gtk_adjustment_new (255, 0, 255, 1, 10, 0);
  spinbutton_high_output = gtk_spin_button_new (GTK_ADJUSTMENT (spinbutton_high_output_adj), 1, 0);
  gtk_widget_show (spinbutton_high_output);
  gtk_table_attach (GTK_TABLE (table1), spinbutton_high_output, 1, 2, 5, 6,
                    (GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);




  dialog_action_area1 = GTK_DIALOG (dialog1)->action_area;
  gtk_widget_show (dialog_action_area1);
  gtk_container_set_border_width (GTK_CONTAINER (dialog_action_area1), 10);



  wcd->radio_master  = radiobutton1;
  wcd->radio_red     = radiobutton2;
  wcd->radio_green   = radiobutton3;
  wcd->radio_blue    = radiobutton4;
  wcd->radio_alpha   = radiobutton5;


  /* signals */
  g_signal_connect (G_OBJECT (wcd->radio_master),  "clicked",  G_CALLBACK (radio_callback), wcd);
  g_signal_connect (G_OBJECT (wcd->radio_red),     "clicked",  G_CALLBACK (radio_callback), wcd);
  g_signal_connect (G_OBJECT (wcd->radio_green),   "clicked",  G_CALLBACK (radio_callback), wcd);
  g_signal_connect (G_OBJECT (wcd->radio_blue),    "clicked",  G_CALLBACK (radio_callback), wcd);
  g_signal_connect (G_OBJECT (wcd->radio_alpha),   "clicked",  G_CALLBACK (radio_callback), wcd);

  g_signal_connect (G_OBJECT (spinbutton_gamma_adj),       "value_changed",  G_CALLBACK (gimp_double_adjustment_update), &cuvals->gamma);

  g_signal_connect (G_OBJECT (spinbutton_high_input_adj),  "value_changed",  G_CALLBACK (gimp_int_adjustment_update), &cuvals->high_input);
  g_signal_connect (G_OBJECT (spinbutton_low_input_adj),   "value_changed",  G_CALLBACK (gimp_int_adjustment_update), &cuvals->low_input);
  g_signal_connect (G_OBJECT (spinbutton_high_output_adj), "value_changed",  G_CALLBACK (gimp_int_adjustment_update), &cuvals->high_output);
  g_signal_connect (G_OBJECT (spinbutton_low_output_adj),  "value_changed",  G_CALLBACK (gimp_int_adjustment_update), &cuvals->low_output);

  gtk_widget_show (dialog1);

  gtk_main ();
  gdk_flush ();


  return wcd;
}
