/* gap_wr_resynth.c
 *   provides automated animated apply support for the 3rd party resynthesizer plug-in.
 *   Useful to remove unwanted logos when processing video frames.
 * PRECONDITIONS:
 *   Requires resynthesizer plug-in.
 *  (resynthesizer-0.16.tar.gz is available in the gimp plug-in registry)
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
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
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



/***** Magic numbers *****/

#define SCALE_WIDTH  200
#define SPIN_BUTTON_WIDTH 60

/***** Types *****/


typedef struct {
  gint32  corpus_border_radius;
  gint32  alt_selection;
  gint32  seed;
} TransValues;

static TransValues glob_vals =
{
   20           /* corpus_border_radius */
,  -1           /* alt_selection (drawable id or -1 for using original selection) */
, 4711          /* seed for random number generator */
};




/***** Prototypes *****/

static void query (void);
static void run   (const gchar      *name,
                   gint              nparams,
                   const GimpParam  *param,
                   gint             *nreturn_vals,
                   GimpParam       **return_vals);

static gint p_selectionConstraintFunc (gint32   image_id,
               gint32   drawable_id,
               gpointer data);
static gboolean p_dialog(TransValues *val_ptr, gint32 drawable_id);
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
    { GIMP_PDB_DRAWABLE, "alt_selection",        "id of a drawable to replace the selection (use -1 to operate with selection of the input image)"     },
    { GIMP_PDB_INT32,    "seed",                 "seed for random numbers (use -1 to init with current time)"     }
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
    GIMP_LASTVALDEF_DRAWABLE        (GIMP_ITER_TRUE,  glob_vals.alt_selection,         "alt_selection"),
    GIMP_LASTVALDEF_GINT32          (GIMP_ITER_TRUE,  glob_vals.seed,                  "seed")
  };

  gimp_plugin_domain_register (GETTEXT_PACKAGE, LOCALEDIR);
  
  /* registration for last values buffer structure (useful for animated filter apply) */
  gimp_lastval_desc_register(PLUG_IN_PROC,
                             &glob_vals,
                             sizeof(glob_vals),
                             G_N_ELEMENTS (lastvals),
                             lastvals);


  gimp_install_procedure (PLUG_IN_PROC,
                          N_("Smart selection eraser."),
                          "Remove an object from an image by extending surrounding texture to cover it. "
                          "The object can be represented by the current selection  "
                          "or by an alternative selction (provided as parameter alt_selection) "
                          "If the image, that is refered by the alt_selction drawable_id has a selction "
                          "then the refred selection is used to identify the object. "
                          "otherwise a grayscale copy of the alt_selection drawable_id will be used "
                          "to identify the object that shall be replaced. "
                          "alt_selection value -1 indicates that the selection of the input image shall be used. "
                          "Requires resynthesizer plug-in. (available in the gimp plug-in registry) "
                          "The smart selection eraser wrapper provides ability to run in GIMP_GAP filtermacros "
                          "when processing video frames (typically for removing unwanted logos from video frames)."
                          "(using the same seed value for all frames is recommanded) ",
                          "Wolfgang Hofer",
                          "Wolfgang Hofer",
                          PLUG_IN_VERSION,
                          N_("Smart selection eraser..."),
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
      glob_vals.alt_selection = -1;
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
             glob_vals.alt_selection = param[4].data.d_int32;
             glob_vals.seed = param[5].data.d_int32;
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


/* --------------------------
 * p_dialog
 * --------------------------
 */
static gboolean
p_dialog (TransValues *val_ptr, gint32 drawable_id)
{
  GtkWidget *dialog;
  GtkWidget *main_vbox;
  GtkWidget *label;
  GtkWidget *table;
  GtkWidget *combo;
  GtkObject *adj;
  gint       row;
  gboolean   run;
  gboolean   isResynthesizerInstalled;

  gboolean foundResynth;
  gboolean foundResynthS;

  foundResynthS = gap_pdb_procedure_name_available(PLUG_IN_RESYNTHESIZER_WITH_SEED);
  foundResynth = gap_pdb_procedure_name_available(PLUG_IN_RESYNTHESIZER);
  isResynthesizerInstalled = ((foundResynthS) || (foundResynth));
  val_ptr->alt_selection = -1;

  gimp_ui_init (PLUG_IN_BINARY, TRUE);

  if (isResynthesizerInstalled)
  {
    dialog = gimp_dialog_new (_("Smart selection eraser"), PLUG_IN_BINARY,
                            NULL, 0,
                            gimp_standard_help_func, PLUG_IN_PROC,

                            GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                            GTK_STOCK_OK,     GTK_RESPONSE_OK,

                            NULL);

  }
  else
  {
    dialog = gimp_dialog_new (_("Smart selection eraser"), PLUG_IN_BINARY,
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

    gimp_int_combo_box_connect (GIMP_INT_COMBO_BOX (combo), drawable_id,
                                G_CALLBACK (p_selectionComboCallback),
                                NULL);

    gtk_table_attach (GTK_TABLE (table), combo, 1, 3, row, row + 1,
                      GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);
    gtk_widget_show (combo);
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
 */
static gboolean
p_pdb_call_resynthesizer(gint32 image_id, gint32 layer_id, gint32 corpus_layer_id, gint32 seed)
{
   char            *l_called_proc;
   GimpParam       *return_vals;
   int              nreturn_vals;
   gint             nparams_resynth_s;
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
                                 GIMP_PDB_INT32,     1,                 /* Dont change border pixels */
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
  gint32 dup_image_id;
  gint32 channel_id;
  gint32 channel_2_id;
  GimpRGB  bck_color;
  GimpRGB  white_opaque_color;
  gboolean has_selection;
  gboolean non_empty;
  gint     x1, y1, x2, y2;
  gint32   active_layer_stackposition;
  gint32   active_dup_layer_id;


  active_layer_stackposition = gap_layer_get_stackposition(image_id, drawable_id);

  dup_image_id = gimp_image_duplicate(image_id);

  channel_id = gimp_selection_save(dup_image_id);
  gimp_selection_grow(dup_image_id, val_ptr->corpus_border_radius);
  gimp_selection_invert(dup_image_id);

  gimp_context_get_background(&bck_color);
  channel_2_id = gimp_selection_save(dup_image_id);

  gimp_image_select_item(dup_image_id, GIMP_CHANNEL_OP_REPLACE, channel_id);

  gimp_rgba_set_uchar (&white_opaque_color, 255, 255, 255, 255);
  gimp_context_set_background(&white_opaque_color);
  gimp_edit_clear(channel_2_id);


  gimp_context_set_background(&bck_color);  /* restore original background color */

  gimp_selection_load(channel_2_id);

  gimp_selection_invert(dup_image_id);

  has_selection  = gimp_selection_bounds(dup_image_id, &non_empty, &x1, &y1, &x2, &y2);
  gimp_image_crop(dup_image_id, (x2 - x1), (y2 - y1), x1, y1);

  gimp_selection_invert(dup_image_id);
  active_dup_layer_id = gap_layer_get_id_by_stackposition(dup_image_id, active_layer_stackposition);

  if (1==0)
  {
    /* debug code shows the duplicate image by adding a display */
    gimp_display_new(dup_image_id);
  }
  return (active_dup_layer_id);

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
    printf("alt_selection: %d\n", (int)val_ptr->alt_selection);
    printf("seed: %d\n", (int)val_ptr->seed);
  }

  gimp_image_undo_group_start(image_id);


  trans_drawable_id = -1;
  alt_selection_success = FALSE;

  if(val_ptr->alt_selection >= 0)
  {
    if(gap_debug)
    {
      printf("creating alt_selection: %d\n", (int)val_ptr->alt_selection);
    }
    alt_selection_success = p_set_alt_selection(image_id, drawable_id, val_ptr);
  }

  has_selection  = gimp_selection_bounds(image_id, &non_empty, &x1, &y1, &x2, &y2);

  /* here the action starts, we create the corpus_layer_id that builds the reference pattern
   * (the corpus is created in a spearate image and has an expanded selection
   * that excludes the unwanted parts)
   * then start the resynthesizer plug-in to replace selcted (e.g. unwanted parts) of the
   * processed layer (e.g. drawable_id)
   */
  if (non_empty)
  {
    gint32 corpus_layer_id;
    gint32 corpus_image_id;

    trans_drawable_id = drawable_id;

    corpus_layer_id = p_create_corpus_layer(image_id, drawable_id, val_ptr);

    p_pdb_call_resynthesizer(image_id, drawable_id, corpus_layer_id, val_ptr->seed);

    /* delete the temporary working duplicate */
    corpus_image_id = gimp_drawable_get_image(corpus_layer_id);
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

