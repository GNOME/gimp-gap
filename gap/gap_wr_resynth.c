/* gap_wr_resynth.c
 *   provides automated animated apply support for the 3rd party resynthesizer plug-in.
 *   Useful to remove unwanted logos when processing video frames.
 * PRECONDITIONS:
 *   Requires resynthesizer plug-in.
 *  (resynthesizer-0.16.tar.gz is available in the gimp plug-in registry
 *   alternative sourcecode download from https://github.com/bootchk/resynthesizer  Latest commit on 29 May 2016
 *   was testest working OK on 2017.03.02 with this wrapper)
 *  NOTE this wrapper also supports an extended variant plug-in-resynthesizer-s
 *       that has an additional seed parameter.
 */

/* GIMP - The GNU Image Manipulation Program
 * Copyright (C) 1995 Spencer Kimball and Peter Mattis
 *
 * Copyright (C) 2008 Wolfgang Hofer
 * hof@gimp.org
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

#include "config.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <gtk/gtk.h>
#include <libgimp/gimp.h>
#include <libgimp/gimpui.h>

#include "gap_lastvaldesc.h"
#include "gap_lastvaldesc.h"
#include "gap_libgimpgap.h"

#include "gap-intl.h"

/***** Macro definitions  *****/
#define PLUG_IN_PROC        "plug-in-wr-resynth"
#define PLUG_IN_VERSION     "0.16"
#define PLUG_IN_AUTHOR      "Wolfgang Hofer (hof@gimp.org)"
#define PLUG_IN_COPYRIGHT   "Wolfgang Hofer"

#define PLUG_IN_BINARY  "gap_wr_resynth"


#define PLUG_IN_RESYNTHESIZER            "plug-in-resynthesizer"
#define PLUG_IN_RESYNTHESIZER_WITH_SEED  "plug-in-resynthesizer-s"  /* unpublished prvate version */

#define MAX_SVG_SIZE     1600
#define BUTTON_MIN_WIDTH     50


#define DIRECTION_ALL_AROUND      0
#define DIRECTION_SIDES           1
#define DIRECTION_ABOVE_AND_BELOW 2

#define FILL_ORDER_RANDOM                0
#define FILL_ORDER_INWARDS_TO_CENTER     1
#define FILL_ORDER_OUTWARDS_FROM_CENTER  2


#define SELECTION_FROM_VECTORS -2
#define SELECTION_FROM_SVG_FILE -3


/***** Magic numbers *****/

#define SCALE_WIDTH  200
#define SPIN_BUTTON_WIDTH 60

/***** Types *****/


typedef struct {
  gint32  corpus_border_radius;
  gint32  directionParam;
  gint32  orderParam;

  gint32  alt_selection;
  gint32  seed;
  gchar   selectionSVGFileName[MAX_SVG_SIZE]; /* contains small xml string or reference to SVG file */
} TransValues;

typedef struct GuiStuff {
  //gint32      imageId;
  //GtkWidget  *ok_button;
  //GtkWidget  *msg_label;
  GtkWidget  *svg_entry;
  GtkWidget  *svg_filesel;
  TransValues *valPtr;
} GuiStuff;


static TransValues glob_vals =
{
   20           /* corpus_border_radius */
,  DIRECTION_ALL_AROUND           /* directionParam 0 = AllAround, 1 = Sides, 2 = Above and Below */
,  FILL_ORDER_RANDOM              /* filling orderParam 0 = Random, 1 = Inwards To Center, 2 = Outwards from center */
,  -1           /* alt_selection (drawable id or -1 for using original selection) */
, 4711          /* seed for random number generator */
, "selection.svg"
};




/***** Prototypes *****/

static void query (void);
static void run   (const gchar      *name,
                   gint              nparams,
                   const GimpParam  *param,
                   gint             *nreturn_vals,
                   GimpParam       **return_vals);

static void p_set_selection_from_vectors_file(gint32 imageId, TransValues *val_ptr);
static gint p_selectionConstraintFunc (gint32   image_id,
               gint32   drawable_id,
               gpointer data);
static gboolean p_dialog(TransValues *val_ptr, gint32 drawable_id);
static void p_on_gint32_combo_callback  (GtkWidget     *widget, gint32 *value);
static void on_svg_entry_changed              (GtkEditable     *editable,
                                   TransValues *val_ptr);
static void on_filesel_button_clicked             (GtkButton *button,
                                       GuiStuff  *guiStuffPtr);
static GtkWidget* p_create_fileselection (GuiStuff *guiStuffPtr);

static gint32 p_process_layer(gint32 image_id, gint32 drawable_id, TransValues *val_ptr);

/***** Variables *****/

const GimpPlugInInfo PLUG_IN_INFO =
{
  NULL,   /* init_proc  */
  NULL,   /* quit_proc  */
  query,  /* query_proc */
  run     /* run_proc   */
};

static GimpParamDef in_args[] = {
    { GIMP_PDB_INT32,    "run-mode",             "Interactive, non-interactive" },
    { GIMP_PDB_IMAGE,    "image",                "Input image"                  },
    { GIMP_PDB_DRAWABLE, "drawable",             "The drawable (typically a layer)"               },
    { GIMP_PDB_INT32,    "corpus_border_radius", "Radius to take texture from"     },
    { GIMP_PDB_INT32,    "directionParam",       "0 = AllAround, 1 = Sides, 2 = Above and Below"     },
    { GIMP_PDB_INT32,    "orderParam",           "filling orderParam 0 = Random, 1 = Inwards To Center, 2 = Outwards from center"     },
    { GIMP_PDB_DRAWABLE, "alt_selection",        "id of a drawable to replace the selection (use -1 to operate with selection of the input image)"     },
    { GIMP_PDB_INT32,    "seed",                 "seed for random numbers (use -1 to init with current time)"     },
    { GIMP_PDB_STRING,   "selSVGFile",    "optional name of a file that contains the selection as vectors in SVG format. (set altSelection to -2)" }
  };

static GimpParamDef return_vals[] = {
    { GIMP_PDB_DRAWABLE, "the_drawable", "the handled drawable" }
};

static gint global_number_in_args = G_N_ELEMENTS (in_args);
static gint global_number_out_args = G_N_ELEMENTS (return_vals);

/* Global Variables */
int gap_debug = 0;  /* 1 == print debug infos , 0 dont print debug infos */


/***** Functions *****/

MAIN()

/* ------------------
 * query
 * ------------------
 */
static void
query (void)
{

  static GimpLastvalDef lastvals[] =
  {
    GIMP_LASTVALDEF_GINT32          (GIMP_ITER_TRUE,  glob_vals.corpus_border_radius,  "corpus_border_radius"),
    GIMP_LASTVALDEF_GINT32          (GIMP_ITER_FALSE, glob_vals.directionParam,        "directionParam"),
    GIMP_LASTVALDEF_GINT32          (GIMP_ITER_FALSE, glob_vals.orderParam,            "orderParam"),
    GIMP_LASTVALDEF_DRAWABLE        (GIMP_ITER_TRUE,  glob_vals.alt_selection,         "alt_selection"),
    GIMP_LASTVALDEF_GINT32          (GIMP_ITER_TRUE,  glob_vals.seed,                  "seed"),
    GIMP_LASTVALDEF_ARRAY           (GIMP_ITER_FALSE, glob_vals.selectionSVGFileName,    "svgFileNameArray"),
    GIMP_LASTVALDEF_GCHAR           (GIMP_ITER_FALSE, glob_vals.selectionSVGFileName[0], "svgFilenameNameChar")
  };

  gimp_plugin_domain_register (GETTEXT_PACKAGE, LOCALEDIR);
  
  /* registration for last values buffer structure (useful for animated filter apply) */
  gimp_lastval_desc_register(PLUG_IN_PROC,
                             &glob_vals,
                             sizeof(glob_vals),
                             G_N_ELEMENTS (lastvals),
                             lastvals);


  gimp_install_procedure (PLUG_IN_PROC,
                          N_("Heal Selection"),
                          "Remove an object from an image by extending surrounding texture to cover it. "
                          "The object can be represented by the current selection  "
                          "or by an alternative selection (provided as parameter alt_selection) "
                          "If the image, that is referred by the alt_selection drawable_id has a selection "
                          "then the referred selection is used to identify the object. "
                          "otherwise a grayscale copy of the alt_selection drawable_id will be used "
                          "to identify the object that shall be replaced. "
                          "alt_selection value -1 indicates that the selection of the input image shall be used. "
                          "Requires resynthesizer plug-in. (available in the gimp plug-in registry) "
                          "The Heal Selection wrapper provides ability to run in GIMP_GAP filtermacros "
                          "when processing video frames (typically for removing unwanted logos from video frames)."
                          "(using the same seed value for all frames is recommended) ",
                          "Wolfgang Hofer",
                          "Wolfgang Hofer",
                          PLUG_IN_VERSION,
                          N_("Heal Selection..."),
                          "RGB*, GRAY*",
                          GIMP_PLUGIN,
                          global_number_in_args, global_number_out_args,
                          in_args, return_vals);

  {
    /* Menu names */
    const char *menupath_image_video_layer = N_("<Image>/Video/Layer/Enhance/");

    gimp_plugin_menu_register (PLUG_IN_PROC, menupath_image_video_layer);
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
  gint32       drawable_id = -1;
  gint32       trans_drawable_id = -1;

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
  drawable_id = param[2].data.d_int32;

  if (strcmp (name, PLUG_IN_PROC) == 0)
  {
    if(gimp_drawable_is_layer(drawable_id))
    {
      gboolean run_flag;

      /* Initial values */
      glob_vals.corpus_border_radius = 20;
      glob_vals.directionParam = 0;
      glob_vals.orderParam = 0;
      glob_vals.alt_selection = -1;
      
      glob_vals.selectionSVGFileName[0] = '\0';
      g_snprintf(glob_vals.selectionSVGFileName
                   , sizeof(glob_vals.selectionSVGFileName), "%s"
             , _("selection.svg"));
      
      run_flag = TRUE;

      /* Possibly retrieve data from a previous run */
      gimp_get_data (name, &glob_vals);

      switch (run_mode)
       {
        case GIMP_RUN_INTERACTIVE:

          /* Get information from the dialog */
          run_flag = p_dialog(&glob_vals, drawable_id);
          break;

        case GIMP_RUN_NONINTERACTIVE:
          /* check to see if invoked with the correct number of parameters */
          if (nparams >= global_number_in_args)
          {
             glob_vals.corpus_border_radius = param[3].data.d_int32;
             glob_vals.directionParam = param[4].data.d_int32;
             glob_vals.orderParam = param[5].data.d_int32;
             glob_vals.alt_selection = param[6].data.d_int32;
             glob_vals.seed = param[7].data.d_int32;
          }
          else
          {
            status = GIMP_PDB_CALLING_ERROR;
          }
          break;

        case GIMP_RUN_WITH_LAST_VALS:
          break;

        default:
          break;
      }



      /* here the action starts, we transform the drawable */
      if(run_flag)
      {
        trans_drawable_id = p_process_layer(image_id
                                             , drawable_id
                                             , &glob_vals
                                             );
        if (trans_drawable_id < 0)
        {
           status = GIMP_PDB_CALLING_ERROR;
        }
        else
        {
           values[1].data.d_drawable = drawable_id;

           /* Store variable states for next run
            * (the parameters for the transform wrapper plugins are stored
            *  even if they contain just a dummy
            *  this is done to fullfill the GIMP-GAP LAST_VALUES conventions
            *  for filtermacro and animated calls)
            */
           if (run_mode == GIMP_RUN_INTERACTIVE)
           {
             gimp_set_data (name, &glob_vals, sizeof (TransValues));
           }
        }
      }
    }
    else
    {
       status = GIMP_PDB_CALLING_ERROR;
       if (run_mode == GIMP_RUN_INTERACTIVE)
       {
         g_message(_("The plug-in %s\noperates only on layers\n"
                     "(but was called on mask or channel)")
                  , name
                  );
       }
    }

  }
  else
  {
    status = GIMP_PDB_CALLING_ERROR;
  }


  if (status == GIMP_PDB_SUCCESS)
  {

    /* If run mode is interactive, flush displays, else (script) don't
     * do it, as the screen updates would make the scripts slow
     */
    if (run_mode != GIMP_RUN_NONINTERACTIVE)
      gimp_displays_flush ();

  }
  values[0].data.d_status = status;
}       /* end run */



/* ----------------------------------
 * p_set_selection_from_vectors_file
 * ----------------------------------
 * import selection from an SVG vectors file
 * and replace the current selection on success.
 * (on errors keep current selection)
 */
static void
p_set_selection_from_vectors_file(gint32 imageId, TransValues *val_ptr)
{
  gboolean vectorsOk;
  gint     num_vectors;
  gint32  *vectors_ids;

  vectorsOk = FALSE;
  if (val_ptr->selectionSVGFileName != '\0')
  {
    if(g_file_test(val_ptr->selectionSVGFileName, G_FILE_TEST_EXISTS))
    {
      vectorsOk = gimp_vectors_import_from_file   (imageId
                                                  ,val_ptr->selectionSVGFileName
                                                  , TRUE  /* Merge paths into a single vectors object. */
                                                  , TRUE  /* Scale the SVG to image dimensions. */
                                                  , &num_vectors
                                                  , &vectors_ids
                                                  );
    }
  }


  if ((vectorsOk) && (vectors_ids != NULL) && (num_vectors > 0))
  {
    gint32         vectorId;
    GimpChannelOps operation;

    vectorId = vectors_ids[0];
    operation = GIMP_CHANNEL_OP_REPLACE;
    gimp_image_select_item(imageId, operation, vectorId);
    gimp_image_remove_vectors(imageId, vectorId);
    // doClearSelection = TRUE;
  }

}  /* end p_set_selection_from_vectors_file */



/* ----------------------------
 * p_selectionConstraintFunc
 * ----------------------------
 *
 */
static gint
p_selectionConstraintFunc (gint32   image_id,
               gint32   drawable_id,
               gpointer data)
{
  if (image_id < 0)
    return FALSE;

  /* dont accept layers from indexed images */
  if (gimp_drawable_is_indexed (drawable_id))
    return FALSE;

  return TRUE;
}  /* end p_selectionConstraintFunc */




/* ----------------------------
 * p_selectionComboCallback
 * ----------------------------
 *
 */
static void
p_selectionComboCallback (GtkWidget *widget)
{
  gint idValue;

  if(gap_debug)
  {
    printf("p_selectionComboCallback START\n");
  }

  gimp_int_combo_box_get_active (GIMP_INT_COMBO_BOX (widget), &idValue);

  if(gap_debug)
  {
    printf("p_selectionComboCallback idValue:%d\n", idValue);
  }
  glob_vals.alt_selection = idValue;

}  /* end p_selectionComboCallback */

/* ----------------------------------------
 * p_on_gint32_combo_callback
 * ----------------------------------------
 */
static void
p_on_gint32_combo_callback  (GtkWidget     *widget,
                       gint32 *valuePtr)
{
  gint       value;

  if(gap_debug) printf("CB: p_on_gint32_combo_callback\n");

  if(valuePtr == NULL) return;

  gimp_int_combo_box_get_active (GIMP_INT_COMBO_BOX (widget), &value);

  if(gap_debug)
  {
    printf("CB: p_on_gint32_combo_callback value: %d\n", (int)value);
  }

  *valuePtr = value;

}  /* end p_on_gint32_combo_callback */

/* ---------------------------------
 * on_svg_entry_changed
 * ---------------------------------
 */
static void
on_svg_entry_changed              (GtkEditable     *editable,
                                   TransValues *val_ptr)
{
  GtkEntry *entry;

 if(gap_debug)
 {
   printf("CB: on_svg_entry_changed\n");
 }
 if(val_ptr == NULL)
 {
   return;
 }

 entry = GTK_ENTRY(editable);
 if(entry)
 {
    g_snprintf(val_ptr->selectionSVGFileName
              , sizeof(val_ptr->selectionSVGFileName), "%s"
              , gtk_entry_get_text(entry));
    // TODO p_check_exec_condition_and_set_ok_sesitivity(guiStuffPtr);
 }
}


/* ---------------------------------
 * on_filesel_button_clicked
 * ---------------------------------
 */
static void
on_filesel_button_clicked             (GtkButton *button,
                                       GuiStuff  *guiStuffPtr)
{
 if(gap_debug)
 {
   printf("CB: on_filesel_button_clicked\n");
 }
 if(guiStuffPtr == NULL)
 {
   return;
 }

 if(guiStuffPtr->svg_filesel == NULL)
 {
   guiStuffPtr->svg_filesel = p_create_fileselection(guiStuffPtr);
   gtk_file_selection_set_filename (GTK_FILE_SELECTION (guiStuffPtr->svg_filesel),
                                    guiStuffPtr->valPtr->selectionSVGFileName);

   gtk_widget_show (guiStuffPtr->svg_filesel);
 }

}  /* end on_filesel_button_clicked */


/* ---------------------------------
 * svg fileselct callbacks
 * ---------------------------------
 */

static void
on_svg_filesel_destroy          (GtkObject *object,
                                 GuiStuff  *guiStuffPtr)
{
 if(gap_debug) printf("CB: on_svg_filesel_destroy\n");
 if(guiStuffPtr == NULL) return;

 guiStuffPtr->svg_filesel = NULL;
}

static void
on_svg__button_cancel_clicked          (GtkButton *button,
                                        GuiStuff  *guiStuffPtr)
{
 if(gap_debug) printf("CB: on_svg__button_cancel_clicked\n");
 if(guiStuffPtr == NULL) return;

 if(guiStuffPtr->svg_filesel)
 {
   gtk_widget_destroy(guiStuffPtr->svg_filesel);
   guiStuffPtr->svg_filesel = NULL;
 }
}

static void
on_svg__button_OK_clicked       (GtkButton *button,
                                 GuiStuff  *guiStuffPtr)
{
  const gchar *filename;
  GtkEntry *entry;

 if(gap_debug) printf("CB: on_svg__button_OK_clicked\n");
 if(guiStuffPtr == NULL) return;

 if(guiStuffPtr->svg_filesel)
 {
   filename =  gtk_file_selection_get_filename (GTK_FILE_SELECTION (guiStuffPtr->svg_filesel));
   g_snprintf(guiStuffPtr->valPtr->selectionSVGFileName
             , sizeof(guiStuffPtr->valPtr->selectionSVGFileName), "%s"
             , filename);
   entry = GTK_ENTRY(guiStuffPtr->svg_entry);
   if(entry)
   {
      gtk_entry_set_text(entry, filename);
   }
   on_svg__button_cancel_clicked(NULL, (gpointer)guiStuffPtr);
 }
}


/* ----------------------------------------
 * p_create_fileselection
 * ----------------------------------------
 */
static GtkWidget*
p_create_fileselection (GuiStuff *guiStuffPtr)
{
  GtkWidget *svg_filesel;
  GtkWidget *svg__button_OK;
  GtkWidget *svg__button_cancel;

  svg_filesel = gtk_file_selection_new (_("Select vectorfile name"));
  gtk_container_set_border_width (GTK_CONTAINER (svg_filesel), 10);

  svg__button_OK = GTK_FILE_SELECTION (svg_filesel)->ok_button;
  gtk_widget_show (svg__button_OK);
  GTK_WIDGET_SET_FLAGS (svg__button_OK, GTK_CAN_DEFAULT);

  svg__button_cancel = GTK_FILE_SELECTION (svg_filesel)->cancel_button;
  gtk_widget_show (svg__button_cancel);
  GTK_WIDGET_SET_FLAGS (svg__button_cancel, GTK_CAN_DEFAULT);

  g_signal_connect (G_OBJECT (svg_filesel), "destroy",
                      G_CALLBACK (on_svg_filesel_destroy),
                      guiStuffPtr);
  g_signal_connect (G_OBJECT (svg__button_OK), "clicked",
                      G_CALLBACK (on_svg__button_OK_clicked),
                      guiStuffPtr);
  g_signal_connect (G_OBJECT (svg__button_cancel), "clicked",
                      G_CALLBACK (on_svg__button_cancel_clicked),
                      guiStuffPtr);

  gtk_widget_grab_default (svg__button_cancel);
  return svg_filesel;
}  /* end p_create_fileselection */


/* --------------------------
 * p_dialog
 * --------------------------
 */
static gboolean
p_dialog (TransValues *val_ptr, gint32 drawable_id)
{
  GuiStuff guiStuffRecord;
  GuiStuff *guiStuffPtr;
  GtkWidget *button;
  GtkWidget *dialog;
  GtkWidget *main_vbox;
  GtkWidget *label;
  GtkWidget *table;
  GtkWidget *combo;
  GtkWidget *entry;
  GtkObject *adj;
  gint       row;
  gboolean   run;
  gboolean   isResynthesizerInstalled;

  gboolean foundResynth;
  gboolean foundResynthS;


  guiStuffPtr = &guiStuffRecord;
  guiStuffPtr->valPtr = val_ptr;
  guiStuffPtr->svg_entry = NULL;
  guiStuffPtr->svg_filesel = NULL;



  foundResynthS = gap_pdb_procedure_name_available(PLUG_IN_RESYNTHESIZER_WITH_SEED);
  foundResynth = gap_pdb_procedure_name_available(PLUG_IN_RESYNTHESIZER);
  isResynthesizerInstalled = ((foundResynthS) || (foundResynth));
  val_ptr->alt_selection = -1;

  gimp_ui_init (PLUG_IN_BINARY, TRUE);

  if (isResynthesizerInstalled)
  {
    dialog = gimp_dialog_new (_("Heal Selection"), PLUG_IN_BINARY,
                            NULL, 0,
                            gimp_standard_help_func, PLUG_IN_PROC,

                            GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                            GTK_STOCK_OK,     GTK_RESPONSE_OK,

                            NULL);

  }
  else
  {
    dialog = gimp_dialog_new (_("Heal Selection"), PLUG_IN_BINARY,
                            NULL, 0,
                            gimp_standard_help_func, PLUG_IN_PROC,

                            GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,

                            NULL);

  }

  gtk_dialog_set_alternative_button_order (GTK_DIALOG (dialog),
                                           GTK_RESPONSE_OK,
                                           GTK_RESPONSE_CANCEL,
                                           -1);

  gimp_window_set_transient (GTK_WINDOW (dialog));

  main_vbox = gtk_vbox_new (FALSE, 12);
  gtk_container_set_border_width (GTK_CONTAINER (main_vbox), 12);
  gtk_container_add (GTK_CONTAINER (GTK_DIALOG (dialog)->vbox), main_vbox);
  gtk_widget_show (main_vbox);


  /* Controls */
  table = gtk_table_new (3, 3, FALSE);
  gtk_table_set_col_spacings (GTK_TABLE (table), 6);
  gtk_table_set_row_spacings (GTK_TABLE (table), 6);
  gtk_box_pack_start (GTK_BOX (main_vbox), table, FALSE, FALSE, 0);
  gtk_widget_show (table);

  row = 0;
  if (isResynthesizerInstalled != TRUE)
  {
    label = gtk_label_new (_("The Resynthesizer plug-in is required for this operation\n"
                             "But this 3rd party plug-in is not installed\n"
                             "Resynthesizer is available at the gimp plug-in registry"));
    gtk_misc_set_alignment (GTK_MISC (label), 0.5, 0.5);
    gtk_table_attach (GTK_TABLE (table), label, 0, 2, row, row + 1,
                      GTK_FILL, GTK_FILL, 4, 0);
    gtk_widget_show (label);

    row++;
  }
  else
  {
    adj = gimp_scale_entry_new (GTK_TABLE (table), 0, row,
                                _("Border Radius:"), SCALE_WIDTH, 7,
                                val_ptr->corpus_border_radius, 0.0, 1000.0, 1.0, 10.0, 0,
                                TRUE, 0, 0,
                                NULL, NULL);
    g_signal_connect (adj, "value-changed",
                      G_CALLBACK (gimp_int_adjustment_update),
                      &val_ptr->corpus_border_radius);

    row++;
    
    /* the directionParam label */
    label = gtk_label_new (_("Sample from:"));
    gtk_widget_show (label);
    gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
    gtk_table_attach (GTK_TABLE (table), label, 0, 1, row, row+1,
                        (GtkAttachOptions) (GTK_FILL),
                        (GtkAttachOptions) (0), 0, 0);
    
    
    /* the directionParam combo */
    combo = gimp_int_combo_box_new ("All around",      DIRECTION_ALL_AROUND,
                                    "Sides",           DIRECTION_SIDES,
                                    "Above and below", DIRECTION_ABOVE_AND_BELOW,
                                    NULL);
    
    gimp_int_combo_box_connect (GIMP_INT_COMBO_BOX (combo),
                                val_ptr->directionParam,  /* inital value */
                                G_CALLBACK (p_on_gint32_combo_callback),
                                &val_ptr->directionParam);
    
    gtk_widget_show (combo);
    gtk_table_attach (GTK_TABLE (table), combo, 1, 3, row, row+1,
                        (GtkAttachOptions) (GTK_FILL),
                        (GtkAttachOptions) (0), 0, 0);
    gimp_help_set_help_data (combo, _("Select direction from where to get sample pattern"), NULL);
    
    

    row++;

    /* the directionParam label */
    label = gtk_label_new (_("Filling order:"));
    gtk_widget_show (label);
    gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
    gtk_table_attach (GTK_TABLE (table), label, 0, 1, row, row+1,
                        (GtkAttachOptions) (GTK_FILL),
                        (GtkAttachOptions) (0), 0, 0);
    
    
    /* the directionParam combo */
    combo = gimp_int_combo_box_new ("Random",                 FILL_ORDER_RANDOM,
                                    "Inwards towards center", FILL_ORDER_INWARDS_TO_CENTER,
                                    "Outwards from center",   FILL_ORDER_OUTWARDS_FROM_CENTER,
                                    NULL);
    
    gimp_int_combo_box_connect (GIMP_INT_COMBO_BOX (combo),
                                val_ptr->orderParam,  /* inital value */
                                G_CALLBACK (p_on_gint32_combo_callback),
                                &val_ptr->orderParam);
    
    gtk_widget_show (combo);
    gtk_table_attach (GTK_TABLE (table), combo, 1, 3, row, row+1,
                        (GtkAttachOptions) (GTK_FILL),
                        (GtkAttachOptions) (0), 0, 0);
    gimp_help_set_help_data (combo, _("Select filling order"), NULL);
    

    row++;

    

    if (foundResynthS)
    {
      adj = gimp_scale_entry_new (GTK_TABLE (table), 0, row,
                                _("Seed:"), SCALE_WIDTH, 7,
                                val_ptr->seed, -1.0, 10000.0, 1.0, 10.0, 0,
                                TRUE, 0, 0,
                                NULL, NULL);
      g_signal_connect (adj, "value-changed",
                      G_CALLBACK (gimp_int_adjustment_update),
                      &val_ptr->seed);

      row++;
    }

    /* layer combo_box (alt_selection) */
    label = gtk_label_new (_("Set Selection:"));
    gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
    gtk_table_attach (GTK_TABLE (table), label, 0, 1, row, row + 1,
                      GTK_FILL, GTK_FILL, 4, 0);
    gtk_widget_show (label);

    /* layer combo_box (Sample from where to pick the alternative selection */
    combo = gimp_layer_combo_box_new (p_selectionConstraintFunc, NULL);

    gimp_int_combo_box_prepend (GIMP_INT_COMBO_BOX (combo),
                              GIMP_INT_STORE_VALUE,    SELECTION_FROM_SVG_FILE,
                              GIMP_INT_STORE_LABEL,    _("Selection From Vectors File"),
                              GIMP_INT_STORE_STOCK_ID, GIMP_STOCK_PATH,
                              -1);

    gimp_int_combo_box_connect (GIMP_INT_COMBO_BOX (combo), drawable_id,
                                G_CALLBACK (p_selectionComboCallback),
                                NULL);






    gtk_table_attach (GTK_TABLE (table), combo, 1, 3, row, row + 1,
                      GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);
    gtk_widget_show (combo);
    
    
    row++;

    /* the svg file label */
    label = gtk_label_new (_("Vectors (SVG) file:"));
    gtk_widget_show (label);
    gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
    gtk_table_attach (GTK_TABLE (table), label, 0, 1, row, row+1,
                        (GtkAttachOptions) (GTK_FILL),
                        (GtkAttachOptions) (0), 0, 0);
    
    /* the svg file name entry */
    entry = gtk_entry_new ();
    guiStuffPtr->svg_entry = entry;
    gtk_widget_show (entry);
    gtk_table_attach (GTK_TABLE (table), entry, 1, 2, row, row + 1,
                      GTK_FILL, GTK_FILL, 4, 0);
    gimp_help_set_help_data (entry, _("Name of SVG vector file from where to load selection"), NULL);
    if(strncmp("<?xml", val_ptr->selectionSVGFileName, 3) == 0)
    {
      g_snprintf(val_ptr->selectionSVGFileName
                 , sizeof(val_ptr->selectionSVGFileName), "%s"
                 , _("selection.svg"));
    }
    gtk_entry_set_text(GTK_ENTRY (entry), val_ptr->selectionSVGFileName);
    g_signal_connect (G_OBJECT (entry), "changed",
                      G_CALLBACK (on_svg_entry_changed),
                      val_ptr);


    button = gtk_button_new_with_label (_("..."));
    gtk_widget_set_size_request (button, BUTTON_MIN_WIDTH, -1);
    gtk_widget_show (button);
    gtk_table_attach (GTK_TABLE (table), button, 2, 3, row, row + 1,
                    GTK_FILL, GTK_FILL, 4, 0);

    gimp_help_set_help_data (button, _("Select output svg vector file via browser"), NULL);
    g_signal_connect (G_OBJECT (button), "clicked",
                      G_CALLBACK (on_filesel_button_clicked),
                      guiStuffPtr);

                      
                      
  }

  /* Done */

  gtk_widget_show (dialog);

  run = (gimp_dialog_run (GIMP_DIALOG (dialog)) == GTK_RESPONSE_OK);

  gtk_widget_destroy (dialog);

  return run;
}  /* end p_dialog */


/* --------------------------------
 * p_pdb_call_resynthesizer
 * --------------------------------
 * check if non official variant with additional seed parameter
 * is installed. if not use the official published resynthesizer 0.16
 * (API was still compatible to more recent src repository at https://github.com/bootchk/resynthesizer  Latest commit on 29 May 2016)
 */
static gboolean
p_pdb_call_resynthesizer(gint32 image_id, gint32 layer_id, gint32 corpus_layer_id, gint32 useContext, gint32 seed)
{
   char            *l_called_proc;
   GimpParam       *return_vals;
   int              nreturn_vals;
   gboolean         foundResynthS;

   l_called_proc = PLUG_IN_RESYNTHESIZER_WITH_SEED;
   foundResynthS = gap_pdb_procedure_name_available(l_called_proc);
   if (foundResynthS)
   {
     return_vals = gimp_run_procedure (l_called_proc,
                                 &nreturn_vals,
                                 GIMP_PDB_INT32,     GIMP_RUN_NONINTERACTIVE,
                                 GIMP_PDB_IMAGE,     image_id,
                                 GIMP_PDB_DRAWABLE,  layer_id,          /* input drawable (to be processed) */
                                 GIMP_PDB_INT32,     0,                 /* vtile Make tilable vertically */
                                 GIMP_PDB_INT32,     0,                 /* htile Make tilable horizontally */
                                 GIMP_PDB_INT32,     useContext,        /* useContext 1 =Dont change border pixels */
                                 GIMP_PDB_INT32,     corpus_layer_id,   /* corpus, Layer to use as corpus */
                                 GIMP_PDB_INT32,    -1,                 /* inmask Layer to use as input mask, -1 for none */
                                 GIMP_PDB_INT32,    -1,                 /* outmask Layer to use as output mask, -1 for none */
                                 GIMP_PDB_FLOAT,     0.0,               /* map_weight Weight to give to map, if map is used */
                                 GIMP_PDB_FLOAT,     0.117,             /* autism Sensitivity to outliers */
                                 GIMP_PDB_INT32,     16,                /* neighbourhood Neighbourhood size */
                                 GIMP_PDB_INT32,     500,               /* trys Search thoroughness */
                                 GIMP_PDB_INT32,     seed,              /* seed for random number generation */
                                 GIMP_PDB_END);
   }
   else
   {
     gboolean         foundResynth;
     
     l_called_proc = PLUG_IN_RESYNTHESIZER;
     foundResynth = gap_pdb_procedure_name_available(l_called_proc);
     
     if(!foundResynth)
     {
       printf("GAP: Error: PDB %s PDB plug-in is NOT installed\n"
          , l_called_proc
          );
       return(FALSE);
     }
     return_vals = gimp_run_procedure (l_called_proc,
                                 &nreturn_vals,
                                 GIMP_PDB_INT32,     GIMP_RUN_NONINTERACTIVE,
                                 GIMP_PDB_IMAGE,     image_id,
                                 GIMP_PDB_DRAWABLE,  layer_id,          /* input drawable (to be processed) */
                                 GIMP_PDB_INT32,     0,                 /* vtile Make tilable vertically */
                                 GIMP_PDB_INT32,     0,                 /* htile Make tilable horizontally */
                                 GIMP_PDB_INT32,     1,                 /* Dont change border pixels */
                                 GIMP_PDB_INT32,     corpus_layer_id,   /* corpus, Layer to use as corpus */
                                 GIMP_PDB_INT32,    -1,                 /* inmask Layer to use as input mask, -1 for none */
                                 GIMP_PDB_INT32,    -1,                 /* outmask Layer to use as output mask, -1 for none */
                                 GIMP_PDB_FLOAT,     0.0,               /* map_weight Weight to give to map, if map is used */
                                 GIMP_PDB_FLOAT,     0.117,             /* autism Sensitivity to outliers */
                                 GIMP_PDB_INT32,     16,                /* neighbourhood Neighbourhood size */
                                 GIMP_PDB_INT32,     500,               /* trys Search thoroughness */
                                 GIMP_PDB_END);
   }

   if (return_vals[0].data.d_status == GIMP_PDB_SUCCESS)
   {
      gimp_destroy_params(return_vals, nreturn_vals);
      return (TRUE);
   }

   g_message(_("The call of plug-in %s\nfailed.\n"
               "probably the 3rd party plug-in resynthesizer is not installed or is not compatible to version:%s")
              , l_called_proc
              , "resynthesizer-0.16"
            );
   gimp_destroy_params(return_vals, nreturn_vals);
   printf("GAP: Error: PDB call of %s failed (image_id:%d), d_status:%d\n"
          , l_called_proc
          , (int)image_id
          , (int)return_vals[0].data.d_status
          );
   return(FALSE);
}       /* end p_pdb_call_resynthesizer */

/* --------------------------
 * p_create_corpus_layer
 * --------------------------
 * create the corpus layer that builds the reference pattern
 * for the resynthesizer call. The reference pattern is built
 * as duplicate of the original image reduced to the area around the current selection.
 * (grown by corpus_border_radius)
 * Note that the duplicate image has a selection, that includes the gorwn area
 * around the orignal selection, but EXCLUDES the original selection
 * (that is the area holding the object that has to be replaced
 * by pattern of the surrounding area)
 * returns the layer_id of the reference pattern.
 */
static gint32
p_create_corpus_layer(gint32 image_id, gint32 drawable_id, TransValues *val_ptr)
{
// ## the following comment ist the coresponding part of the script plugin-heal-selection.py that ships with resynthesizer
// ## (this C codede wrapper to the resynthesizer engine shall provide same functionality)
//   targetBounds = tdrawable.mask_bounds
// 
//   # In duplicate image, create the sample (corpus).
//   # (I tried to use a temporary layer but found it easier to use duplicate image.)
//   tempImage = pdb.gimp_image_duplicate(timg)
//   if not tempImage:
//       raise RuntimeError, "Failed duplicate image"
//   
//   # !!! The drawable can be a mask (grayscale channel), don't restrict to layer.
//   work_drawable = pdb.gimp_image_get_active_drawable(tempImage)
//   if not work_drawable:
//       raise RuntimeError, "Failed get active drawable"
//       
//   '''
//   grow and punch hole, making a frisket iow stencil iow donut
//   
//   '''
//   orgSelection = pdb.gimp_selection_save(tempImage) # save for later use
//   pdb.gimp_selection_grow(tempImage, samplingRadiusParam)
//   # ??? returns None , docs say it returns SUCCESS
//   
//   # !!! Note that if selection is a bordering ring already, growing expanded it inwards.
//   # Which is what we want, to make a corpus inwards.
//   
//   grownSelection = pdb.gimp_selection_save(tempImage)
//   
//   # Cut hole where the original selection was, so we don't sample from it.
//   # !!! Note that gimp enums/constants are not prefixed with GIMP_
//   pdb.gimp_selection_combine(orgSelection, CHANNEL_OP_SUBTRACT)
//   
//   '''
//   Selection (to be the corpus) is donut or frisket around the original target T
//     xxx
//     xTx
//     xxx
//   '''
//   
//   # crop the temp image to size of selection to save memory and for directional healing!!
//   frisketBounds = grownSelection.mask_bounds
//   frisketLowerLeftX = frisketBounds[0]
//   frisketLowerLeftY = frisketBounds[1]
//   frisketUpperRightX = frisketBounds[2]
//   frisketUpperRightY = frisketBounds[3]
//   targetLowerLeftX = targetBounds[0]
//   targetLowerLeftY = targetBounds[1]
//   targetUpperRightX = targetBounds[2]
//   targetUpperRightY = targetBounds[3]
//   
//   frisketWidth = frisketUpperRightX - frisketLowerLeftX
//   frisketHeight = frisketUpperRightY - frisketLowerLeftY
//   
//   # User's choice of direction affects the corpus shape, and is also passed to resynthesizer plugin
//   if directionParam == 0: # all around
//       # Crop to the entire frisket
//       newWidth, newHeight, newLLX, newLLY = ( frisketWidth, frisketHeight, 
//         frisketLowerLeftX, frisketLowerLeftY )
//   elif directionParam == 1: # sides
//       # Crop to target height and frisket width:  XTX
//       newWidth, newHeight, newLLX, newLLY =  ( frisketWidth, targetUpperRightY-targetLowerLeftY, 
//         frisketLowerLeftX, targetLowerLeftY )
//   elif directionParam == 2: # above and below
//       # X Crop to target width and frisket height
//       # T
//       # X
//       newWidth, newHeight, newLLX, newLLY = ( targetUpperRightX-targetLowerLeftX, frisketHeight, 
//         targetLowerLeftX, frisketLowerLeftY )
//   # Restrict crop to image size (condition of gimp_image_crop) eg when off edge of image
//   newWidth = min(pdb.gimp_image_width(tempImage) - newLLX, newWidth)
//   newHeight = min(pdb.gimp_image_height(tempImage) - newLLY, newHeight)
//   pdb.gimp_image_crop(tempImage, newWidth, newHeight, newLLX, newLLY)
  
  
  
  
  gint32 tempImage;
  gint32 origSelectionChannelId;
  gint32 grownSelectionChannelId;
  gboolean non_empty;
  gint32   work_drawable; // the corpus layer
  gint targetLowerLeftX;  //= targetBounds[0]
  gint targetLowerLeftY;  //= targetBounds[1]
  gint targetUpperRightX; // = targetBounds[2]
  gint targetUpperRightY; // = targetBounds[3]
  gint frisketLowerLeftX; // = frisketBounds[0]
  gint frisketLowerLeftY; // = frisketBounds[1]
  gint frisketUpperRightX; // = frisketBounds[2]
  gint frisketUpperRightY; // = frisketBounds[3]
  gint frisketWidth;
  gint frisketHeight;
  gint newWidth;
  gint newHeight; 
  gint newLLX; 
  gint newLLY;

  //active_layer_stackposition = gap_layer_get_stackposition(image_id, drawable_id);

  tempImage = gimp_image_duplicate(image_id);
  work_drawable = gimp_image_get_active_drawable(tempImage); // gap_layer_get_id_by_stackposition(tempImage, active_layer_stackposition);

  /*   targetBounds = tdrawable.mask_bounds */
  gimp_selection_bounds(tempImage, &non_empty, &targetLowerLeftX, &targetLowerLeftY, &targetUpperRightX, &targetUpperRightY);


  /* grow and punch hole, making a frisket iow stencil iow donut */
 
  origSelectionChannelId = gimp_selection_save(tempImage);
  /* # !!! Note that if selection is a bordering ring already, growing expanded it inwards.
   * # Which is what we want, to make a corpus inwards. 
   */
  gimp_selection_grow(tempImage, val_ptr->corpus_border_radius);
 
  grownSelectionChannelId = gimp_selection_save(tempImage);
  
  /* # Cut hole where the original selection was, so we don't sample from it.  */
  //gimp_selection_combine(origSelectionChannelId, GIMP_CHANNEL_OP_SUBTRACT);
  gimp_image_select_item(tempImage, GIMP_CHANNEL_OP_SUBTRACT, origSelectionChannelId);
 
  /*   Selection (to be the corpus) is donut or frisket around the original target T
   *    xxx
   *    xTx
   *    xxx
   */
   
  /*   frisketBounds = grownSelection.mask_bounds */
  gimp_selection_bounds(tempImage, &non_empty, &frisketLowerLeftX, &frisketLowerLeftY, &frisketUpperRightX, &frisketUpperRightY);
   
  /* # crop the temp image to size of selection to save memory and for directional healing!! */
  frisketWidth = frisketUpperRightX - frisketLowerLeftX;
  frisketHeight = frisketUpperRightY - frisketLowerLeftY;
  
  /* default assume crop settings for "all around" */
  newWidth = frisketWidth;
  newHeight = frisketHeight; 
  newLLX = frisketLowerLeftX; 
  newLLY = frisketLowerLeftY;
  
  if(val_ptr->directionParam == DIRECTION_SIDES) /* # 1 sides */ 
  {
    /* # Crop to target height and frisket width:  XTX */
    newHeight = targetUpperRightY - targetLowerLeftY;
  }
  else if(val_ptr->directionParam == DIRECTION_ABOVE_AND_BELOW) /* # 2 above and below */
  {
    /* # X Crop to target width and frisket height
     * # T
     * # X      
     */
    newWidth = targetUpperRightX - targetLowerLeftX;
  }

  /*  # Restrict crop to image size (condition of gimp_image_crop) eg when off edge of image */
  newWidth = MIN(gimp_image_width(tempImage) - newLLX, newWidth);
  newHeight = MIN(gimp_image_height(tempImage) - newLLY, newHeight);
  gimp_image_crop(tempImage, newWidth, newHeight, newLLX, newLLY);
  

  if (1==0)
  {
    /* debug code shows the duplicate image by adding a display */
    gimp_display_new(tempImage);
  }
  return (work_drawable);

}  /* end p_create_corpus_layer */



/* --------------------------
 * p_set_alt_selection
 * --------------------------
 * create selection as Grayscale copy of the specified alt_selection layer
 *  - operates on a duplicate of the image references by alt_selection
 *  - this duplicate is scaled to same size as specified image_id
 *
 * - if alt_selection refers to an image that has a selction
 *   then use this selction instead of the layer itself.
 */
static gboolean
p_set_alt_selection(gint32 image_id, gint32 drawable_id, TransValues *val_ptr)
{
  if(gap_debug)
  {
    printf("p_set_alt_selection: drawable_id:%d  alt_selection:%d\n"
      ,(int)drawable_id
      ,(int)val_ptr->alt_selection
      );
  }

  if ((drawable_id == val_ptr->alt_selection) || (drawable_id < 0))
  {
    return (FALSE);
  }
  return (gap_image_set_selection_from_selection_or_drawable(image_id, val_ptr->alt_selection, FALSE));
}


/* --------------------------
 * p_process_layer
 * --------------------------
 */
static gint32
p_process_layer(gint32 image_id, gint32 drawable_id, TransValues *val_ptr)
{
  gboolean has_selection;
  gboolean non_empty;
  gboolean alt_selection_success;
  gint     x1, y1, x2, y2;
  gint32   trans_drawable_id;

  if(gap_debug)
  {
    printf("corpus_border_radius: %d\n", (int)val_ptr->corpus_border_radius);
    printf("directionParam: %d\n", (int)val_ptr->directionParam);
    printf("orderParam: %d\n", (int)val_ptr->orderParam);
    printf("alt_selection: %d\n", (int)val_ptr->alt_selection);
    printf("seed: %d\n", (int)val_ptr->seed);
  }

  gimp_image_undo_group_start(image_id);


  if(val_ptr->alt_selection == SELECTION_FROM_SVG_FILE)
  {
    p_set_selection_from_vectors_file(image_id, val_ptr);
  }


  trans_drawable_id = -1;
  alt_selection_success = FALSE;

  if(val_ptr->alt_selection >= 0)
  {
    if(gap_debug)
    {
      printf("p_process_layer creating alt_selection: %d\n", (int)val_ptr->alt_selection);
    }
    alt_selection_success = p_set_alt_selection(image_id, drawable_id, val_ptr);
  }

  has_selection  = gimp_selection_bounds(image_id, &non_empty, &x1, &y1, &x2, &y2);
  
  if (non_empty != TRUE)
  {
    /* in case of empty selection check if possible can load selection from SVG file (even if not explicite requested) */
    p_set_selection_from_vectors_file(image_id, val_ptr);
    has_selection  = gimp_selection_bounds(image_id, &non_empty, &x1, &y1, &x2, &y2);
  }
  
  
  if(gap_debug)
  {
    printf("p_process_layer has_selection: %d\n", (int)has_selection);
  }

  /* here the action starts, we create the corpus_layer_id that builds the reference pattern
   * (the corpus is created in a spearate image and has an expanded selection
   * that excludes the unwanted parts)
   * then start the resynthesizer plug-in to replace selected (i.e. unwanted parts) of the
   * processed layer (i.e. drawable_id)
   */
  if (non_empty)
  {
    gint32 corpus_layer_id;
    gint32 corpus_image_id;
    gint32 useContext;
    
    useContext = 1; /* default random filling */
    /* # Encode two script params into one resynthesizer param.
     * # use border 1 means fill target in random order
     * # use border 0 is for texture mapping operations, not used by this script
     */
    switch(val_ptr->orderParam)
    {
      case FILL_ORDER_RANDOM:
        useContext = 1; /* # 0:  User wants NO order, ie random filling */
        break;
      case FILL_ORDER_INWARDS_TO_CENTER:           /* # Inward to corpus.  2,3,4 */
        useContext = val_ptr->directionParam + 2;  /*  # !!! Offset by 2 to get past the original two boolean values */
        break;
      case FILL_ORDER_OUTWARDS_FROM_CENTER:           /* # Outward from image center.  */
        /* # Outward from image center.  
         * # 5+0=5 outward concentric
         * # 5+1=6 outward from sides
         * # 5+2=7 outward above and below
         */
        useContext = val_ptr->directionParam + 5;
        break;
    }


    trans_drawable_id = drawable_id;

    corpus_layer_id = p_create_corpus_layer(image_id, drawable_id, val_ptr);

    p_pdb_call_resynthesizer(image_id, drawable_id, corpus_layer_id, useContext, val_ptr->seed);

    /* delete the temporary working duplicate */
    corpus_image_id = gimp_item_get_image(corpus_layer_id);
    gimp_image_delete(corpus_image_id);
  }
  else
  {
    g_message("Please make a selection (cant operate on empty selection)");
  }

  if(alt_selection_success)
  {
    gimp_selection_none(image_id);
  }

  gimp_image_undo_group_end(image_id);

  return (trans_drawable_id);

}  /* end p_process_layer */
