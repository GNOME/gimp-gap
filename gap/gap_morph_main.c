/*  gap_morph_main.c
 *  creation of morphing animations (transform source image into des. image) by Wolfgang Hofer
 *  2004/02/11
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
 * version 1.3.26a; 2004/02/12  hof: created
 */

#include "config.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <gtk/gtk.h>
#include <libgimp/gimp.h>
#include <libgimp/gimpui.h>

#include "gap_morph_main.h"
#include "gap_morph_exec.h"
#include "gap_morph_shape.h"
#include "gap_morph_dialog.h"
#include "gap_morph_tween_dialog.h"
#include "gap_pview_da.h"

/* for pointfile loader (workaround) */
#include "gap_mov_dialog.h"
#include "gap_mov_exec.h"



#include "gap-intl.h"

/* Defines */
#define PLUG_IN_NAME            "plug_in_gap_morph_layers"
#define PLUG_IN_NAME_TWEEN      "plug_in_gap_morph_tween"      /* render missing tween(s) between frames */
#define PLUG_IN_NAME_ONE_TWEEN  "plug_in_gap_morph_one_tween"  /* single tween rendering  */
#define PLUG_IN_NAME_WORKPOINTS "plug_in_gap_morph_workpoints"   /* workpoint generation */
#define PLUG_IN_PRINT_NAME      "Morph Layers"
#define PLUG_IN_IMAGE_TYPES     "RGBA, GRAYA"
#define PLUG_IN_AUTHOR          "Wolfgang Hofer (hof@gimp.org)"
#define PLUG_IN_COPYRIGHT       "Wolfgang Hofer"

#define LOCATE_COLORDIFF_THRESHOLD 0.05;

int gap_debug = 0;  /* 1 == print debug infos , 0 dont print debug infos */

static GapMorphGlobalParams global_params =
{
  GIMP_RUN_INTERACTIVE
, -1          /* gint32  image_id */

, 1           /* gint32  tween_steps */
, -1          /* gint32  fdst_layer_id */
, -1          /* gint32  osrc_layer_id */
, NULL        /* GapMorphWorkPoint  *master_wp_list */

, "\0"        /* char workpoint_file_lower[1024] */
, "\0"        /* char workpoint_file_upper[1024] */
, TRUE        /* gboolean            create_tween_layers */
, FALSE       /* gboolean            have_workpointsets */
, FALSE       /* gboolean            use_quality_wp_selection */
, FALSE       /* gboolean            use_gravity */
, 2.0         /* gdouble             gravity_intensity */
, 100.0       /* gdouble             affect_radius */
, GAP_MORPH_RENDER_MODE_MORPH  /* gint32 render_mode */
, FALSE       /* gboolean            do_progress */
, 0.0         /* gdouble             master_progress */
, 0.0         /* gdouble             layer_progress_step */
, 0.0         /* gdouble             tween_mix_factor */
, -1          /*  long                range_from */
, -1          /*  long                range_to */
, FALSE       /* gboolean            overwrite_flag */
, FALSE       /* gboolean            append_flag */
, FALSE       /* gboolean            do_simple_fade */
, 0.08        /* gdouble             edgeColordiffThreshold */
, 0.05        /* gdouble             locateColordiffThreshold */
, 5           /* gint32              locateDetailShapeRadius */
, 70          /* gint32              locateDetailMoveRadius */
, 150         /* gint32              numWorkpoints */
, 32          /* gint32              numOutlinePoints */
, 1           /* gint32              master_tween_steps */
, TRUE        /* gboolean            create_tweens_in_subdir */
, "tween_frames\0"        /* char                tween_subdir[1024] */
, NULL        /* master_progress_callback_fptr */
, NULL        /* progress_callback_fptr */
, NULL        /* gpointer            callback_data_ptr */
};


static void  query (void);
static void  run (const gchar *name,
                  gint nparams,              /* number of parameters passed in */
                  const GimpParam * param,   /* parameters passed in */
                  gint *nreturn_vals,        /* number of parameters returned */
                  GimpParam ** return_vals); /* parameters to be returned */

static gint32   p_handle_tween_processing_pug_ins(GapMorphGlobalParams *mgpp, const char *name);

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
                  { GIMP_PDB_DRAWABLE, "src_drawable", "source drawable (usually a layer)"},
                  { GIMP_PDB_DRAWABLE, "dst_drawable", "destination drawable (usually a layer)"},
                  { GIMP_PDB_INT32,    "tween_steps", "number of layers to create (or modify) below the dst_layer"},
                  { GIMP_PDB_INT32,    "render_mode", "0: Do Morph transformation, 1: do only Warp transformation"},
                  { GIMP_PDB_INT32,    "create_tween_layers", "TRUE: Do create tween layers,  FALSE: operate on existing layers"},
                  { GIMP_PDB_STRING,   "workpoint_file_1", "Name of a Morph/Warp workpointfile"
                                                           "(create such file(s) with the save button in the GUI at INTERACTIVE runmode)"},
                  { GIMP_PDB_STRING,   "workpoint_file_2", "Name of an optional 2nd Morph/Warp workpointfile."
                                                           " (pass an empty string or the same name as the 1st file"
                                                           " if you want to operate with one workpoint file)"},
  };


static GimpParamDef in_tween_args[] = {
                  { GIMP_PDB_INT32,    "run_mode", "Interactive, non-interactive."},
                  { GIMP_PDB_IMAGE,    "start_image",    "from frame image (must be a frame with GIMP-GAP typical number part in the imagefilename)" },
                  { GIMP_PDB_DRAWABLE, "drawable", "ignored"},
                  { GIMP_PDB_INT32,    "to_frame_nr", "frame number of the next available frame"},
                  { GIMP_PDB_INT32,    "overwrite", "0 == do not overwrite, 1 == overrite existing frames"},
                  { GIMP_PDB_INT32,    "do_simple_fade", "0 == use morph algorithm, 1 == use simple fade operation (ignore the workpoint_file) "},
                  { GIMP_PDB_STRING,   "workpoint_file", "Name of a Morph/Warp workpointfile"
                                                           "(create such file(s) with the save button in the GUI "
                                                           "of the plug_in_gap_morph_layers, or specify an emty string"
                                                           "that starts with 0x00 to operate as simple video fade)"},
  };
static GimpParamDef out_tween_args[] = {
                  { GIMP_PDB_DRAWABLE, "tween_drawable", "the last one of the newly created tween(s) (a layer in a newly created frame image)"},
  };

static GimpParamDef in_one_tween_args[] = {
                  { GIMP_PDB_INT32,    "run_mode", "Interactive, non-interactive."},
                  { GIMP_PDB_IMAGE,    "image",    "(not relevant)" },
                  { GIMP_PDB_DRAWABLE, "src_drawable", "source drawable (usually a layer)"},
                  { GIMP_PDB_DRAWABLE, "dst_drawable", "destination drawable (usually a layer)"},
                  { GIMP_PDB_FLOAT,    "tween_mix_factor", "a value between 0.0 and 1.0 where 0 delivers a copy of the src layer"
                                        "1 delivers a copy of the destination layer,"
                                        "other value deliver a mix according to morph algortihm" },
                  { GIMP_PDB_INT32,    "do_simple_fade", "0 == use morph algorithm, 1 == use simple fade operation (ignore the workpoint_file) "},
                  { GIMP_PDB_STRING,   "workpoint_file", "Name of a Morph/Warp workpointfile"
                                                           "(create such file(s) with the save button in the GUI "
                                                           "of the plug_in_gap_morph_layers, or specify an emty string"
                                                           "that starts with 0x00 to operate as simple video fade)"},
  };
static GimpParamDef out_one_tween_args[] = {
                  { GIMP_PDB_DRAWABLE, "tween_drawable", "newly created tween (a layer in a newly created image)"},
  };

static GimpParamDef in_workpoint_args[] = {
                  { GIMP_PDB_INT32,    "run_mode", "Interactive, non-interactive."},
                  { GIMP_PDB_IMAGE,    "start_image",    "from frame image (must be a frame with GIMP-GAP typical number part in the imagefilename)" },
                  { GIMP_PDB_DRAWABLE, "drawable", "ignored"},
                  { GIMP_PDB_INT32,    "to_frame_nr", "frame number of the next available frame"},
                  { GIMP_PDB_INT32,    "numWorkpoints", "number of workpoints to be generated (per workpoint file)"},
                  { GIMP_PDB_INT32,    "numOutlinePoints", "number of additional workpoint on outline (per workpoint file)"},
                  { GIMP_PDB_INT32,    "locateDetailMoveRadius", "radius for locating corresponding coordinate"},
                  { GIMP_PDB_INT32,    "locateDetailShapeRadius", "shape radius in pixels (3 to 16) for locating corresponding coordinate"},
                  { GIMP_PDB_FLOAT,    "locateColordiffThreshold", "0.05 (0.01 - 0.1)  "},
                  { GIMP_PDB_FLOAT,    "edgeColordiffThreshold", "0.05 (0.0 - 1.0) edge detection threshold (the generated workpoints are picked at detected edges) "},
  };
static GimpParamDef out_workpoint_args[] = {
                  { GIMP_PDB_INT32, "num_workpointfiles", "number of successfully generated workpoint files"},
  };

MAIN ()

static void query (void)
{
  gimp_plugin_domain_register (GETTEXT_PACKAGE, LOCALEDIR);

  /* the actual installation of the plugin */
  gimp_install_procedure (PLUG_IN_NAME,
                          "Image Layer Morphing",
                          "This plug-in creates new layers by transforming the src_drawable to dst_drawable, "
                          "the transformation type depends on the render_mode parameter. "
                          "for MORPH render_mode (0) it is a combination of 2 warp deformation actions "
                          "and cross-blending, commonly known as morphing."
                          "The tween_steps parameter controls how much new layers to create. "
                          "(or how much layers to modify depending on the create_tween_layers parameter) "
                          "source and destination may differ in size and can be in different images. "
                          "for WARP render_mode (1) there will be just Move Deformation. "
                          "Deformation is controlled by workpoints. Workpoints are created and edited with the help "
                          "of the INTERACTIVE GUI and saved to file(s). "
                          "For the NON-INTERACTIVE runmode you must provide the filename of such a file. "
                          "Provide 2 filenames if you want to operate with multiple workpoint sets. "
                          "In that case your workpoint files can have a 2 digit numberpart. "
                          "This will implicitly select all filenames with numbers in between as well."
                          ,
                          PLUG_IN_AUTHOR,
                          PLUG_IN_COPYRIGHT,
                          GAP_VERSION_WITH_DATE,
                          N_("Morph..."),
                          PLUG_IN_IMAGE_TYPES,
                          GIMP_PLUGIN,
                          G_N_ELEMENTS (in_args),
                          0,        /* G_N_ELEMENTS (out_args) */
                          in_args,
                          NULL      /* out_args */
                          );

  /* the actual installation of the plugin */
  gimp_install_procedure (PLUG_IN_NAME_TWEEN,
                          "Render tween frames via morphing",
                          "This plug-in creates and saves image frames that are a mix of the specified image frame and the frame with to_frame_nr, "
                          "The typical usage is to create the frame images of missing frame numbers in a series of anim frame images. "
                          "the overwrite flag allows overwriting of already existing frames between the start frame image "
                          "and the frame with to_frame_nr"
                          "Morphing is controled by workpoints. A Workpoint file can be created and edited with the help "
                          "of the Morph feature in the Video menu. "
                          "Note: without workpoints the resulting tween is calculated as simple fade operation. "
                          ,
                          PLUG_IN_AUTHOR,
                          PLUG_IN_COPYRIGHT,
                          GAP_VERSION_WITH_DATE,
                          N_("Morph Tweenframes..."),
                          PLUG_IN_IMAGE_TYPES,
                          GIMP_PLUGIN,
                          G_N_ELEMENTS (in_tween_args),
                          G_N_ELEMENTS (out_tween_args),
                          in_tween_args,
                          out_tween_args
                          );


  /* the actual installation of the plugin */
  gimp_install_procedure (PLUG_IN_NAME_ONE_TWEEN,
                          "Render one tween via morphing",
                          "This plug-in creates a new image that is a mix of the specified src_drawable and dst_drawable, "
                          "the mixing is done based on a morphing transformation where the tween_mix_factor "
                          "determines how much the result looks like source or destination. "
                          "source and destination may differ in size and can be in different images. "
                          "Morphing is controlled by workpoints. A Workpoint file can be created and edited with the help "
                          "of the Morph feature in the Video menu. "
                          "Note: without workpoints the resulting tween is calculated as simple fade operation. "
                          ,
                          PLUG_IN_AUTHOR,
                          PLUG_IN_COPYRIGHT,
                          GAP_VERSION_WITH_DATE,
                          N_("Morph One Tween..."),
                          PLUG_IN_IMAGE_TYPES,
                          GIMP_PLUGIN,
                          G_N_ELEMENTS (in_one_tween_args),
                          G_N_ELEMENTS (out_one_tween_args),
                          in_one_tween_args,
                          out_one_tween_args
                          );


  /* the actual installation of the plugin */
  gimp_install_procedure (PLUG_IN_NAME_WORKPOINTS,
                          "Generate workpoint files for tween frame morphing",
                          "This plug-in generates workpoint files for the specifed frame range."
                          "The generated files are saved with the same name as the frame name(s) "
                          "but with extension .morphpoints "
                          ,
                          PLUG_IN_AUTHOR,
                          PLUG_IN_COPYRIGHT,
                          GAP_VERSION_WITH_DATE,
                          N_("Morph Workpoint Generator..."),
                          PLUG_IN_IMAGE_TYPES,
                          GIMP_PLUGIN,
                          G_N_ELEMENTS (in_workpoint_args),
                          G_N_ELEMENTS (out_workpoint_args),
                          in_workpoint_args,
                          out_workpoint_args
                          );

  {
    /* Menu names */
    const char *menupath_image_video_morph = N_("<Image>/Video/Morphing/");

    gimp_plugin_menu_register (PLUG_IN_NAME,           menupath_image_video_morph);
    gimp_plugin_menu_register (PLUG_IN_NAME_TWEEN,     menupath_image_video_morph);
    gimp_plugin_menu_register (PLUG_IN_NAME_ONE_TWEEN, menupath_image_video_morph);
    gimp_plugin_menu_register (PLUG_IN_NAME_WORKPOINTS, menupath_image_video_morph);
  }
}

/* ----------------------------------
 * p_handle_tween_processing_pug_ins
 * ----------------------------------
 *
 * return the newly created tween morphed layer
 *
 */
static gint32
p_handle_tween_processing_pug_ins(GapMorphGlobalParams *mgpp, const char *name)
{
  gint32 l_tween_layer_id;
  GapAnimInfo *ainfo_ptr;

  gboolean       l_run_flag;

  l_tween_layer_id = -1;
  l_run_flag = TRUE;

  if(gap_debug)
  {
    printf("p_handle_tween_processing_pug_ins name:%s\n", name);
  }

  ainfo_ptr = gap_lib_alloc_ainfo(mgpp->image_id, mgpp->run_mode);
  if(ainfo_ptr != NULL)
  {
    if (0 == gap_lib_dir_ainfo(ainfo_ptr))
    {
      mgpp->range_from = ainfo_ptr->curr_frame_nr;
      /* mgpp->range_to is already set at non-interactive call. for interactive calls this is set in the following dialog
       */
      if(mgpp->run_mode == GIMP_RUN_INTERACTIVE)
      {
         if(gap_lib_chk_framerange(ainfo_ptr) != 0)
         {
           return(-1);
         }
         
         if(0 != gap_lib_chk_framechange(ainfo_ptr)) { l_run_flag = FALSE; }
         else
         {
           if((*ainfo_ptr->extension == '\0') 
           || (ainfo_ptr->frame_cnt == 0))
           {
             /* plugin was called on a frame without extension and without framenumber in its name
              * (typical for new created images named like 'Untitled' 
              */
               g_message(_("Operation cancelled.\n"
                         "GAP video plug-ins only work with filenames\n"
                         "that end in numbers like _000001.xcf.\n"
                         "==> Rename your image, then try again."));
               return -1;
           }
           if (strcmp(name, PLUG_IN_NAME_TWEEN) == 0)
           {
             l_tween_layer_id = gap_morph_frame_tweens_dialog(ainfo_ptr, mgpp);
             /* this dialog also perform processing while dialog window is open
              * (no need to run non-interactive when dialog returns) */
             l_run_flag = FALSE;
           }
           else if (strcmp(name, PLUG_IN_NAME_WORKPOINTS) == 0)
           {
             l_tween_layer_id = gap_morph_generate_frame_tween_workpoints_dialog(ainfo_ptr, mgpp);
             /* this dialog also perform processing while dialog window is open
              * (no need to run non-interactive when dialog returns) */
             l_run_flag = FALSE;
           }
           else
           {
               return -1;
           }
           mgpp->do_progress = TRUE;
         }

         if(0 != gap_lib_chk_framechange(ainfo_ptr))
         {
            l_run_flag = FALSE;
         }

      }

      if(l_run_flag == TRUE)
      {
        gboolean cancelFlag;
             
        cancelFlag = FALSE;
        if(gap_debug)
        {
          printf("l_run_flag is TRUE, running...%s\n", name);
        }

        if (strcmp(name, PLUG_IN_NAME_TWEEN) == 0)
        {
          /* render tween frames and write them as framefiles to disc () */
          l_tween_layer_id = gap_morph_render_frame_tweens(ainfo_ptr
                                 , mgpp
                                 , &cancelFlag
                                 );
        }
        else if (strcmp(name, PLUG_IN_NAME_WORKPOINTS) == 0)
        {
          /* non-interactive workpoint generation (progressBar is NULL) */
          l_tween_layer_id = gap_morph_shape_generate_frame_tween_workpoints(ainfo_ptr
                                 , mgpp
                                 , NULL           /* masterProgressBar */
                                 , NULL           /* progressBar */
                                 , &cancelFlag
                                 );
        }
      }
      else
      {
        if(gap_debug)
        {
          printf("l_run_flag is FALSE, terminating...%s\n", name);
        }
      }

    }
    gap_lib_free_ainfo(&ainfo_ptr);
  }

  return(l_tween_layer_id);

}  /* end p_handle_tween_processing_pug_ins */

/* -------------------------------------
 * run
 * -------------------------------------
 */
static void
run (const gchar *name,          /* name of plugin */
     gint nparams,               /* number of in-parameters */
     const GimpParam * param,    /* in-parameters */
     gint *nreturn_vals,         /* number of out-parameters */
     GimpParam ** return_vals)   /* out-parameters */
{
  const gchar *l_env;
  gint32       image_id = -1;
  gint32       drawable_id = -1;
  GapMorphGlobalParams  *mgpp = &global_params;
  gboolean      run_flag;

  /* Get the runmode from the in-parameters */
  GimpRunMode run_mode = param[0].data.d_int32;

  /* status variable, use it to check for errors in invocation usually only
   * during non-interactive calling
   */
  GimpPDBStatusType status = GIMP_PDB_SUCCESS;

  /* always return at least the status to the caller. */
  static GimpParam values[2];

  INIT_I18N();

  l_env = g_getenv("GAP_DEBUG");
  if(l_env != NULL)
  {
    if((*l_env != 'n') && (*l_env != 'N')) gap_debug = 1;
  }

  if(gap_debug)
  {
    printf("\n\nDEBUG: run %s  RUN_MODE:%d\n", name, (int)run_mode);
  }

  run_flag = FALSE;
  if ((strcmp(name, PLUG_IN_NAME_TWEEN) == 0)
  ||  (strcmp(name, PLUG_IN_NAME_WORKPOINTS) == 0))
  {
    run_flag = TRUE;
  }
  /* initialize the return of the status */
  values[0].type = GIMP_PDB_STATUS;
  values[0].data.d_status = status;
  *nreturn_vals = 1;
  *return_vals = values;


  /* get image */
  image_id = param[1].data.d_int32;
  drawable_id = param[2].data.d_drawable;
  mgpp->do_progress = FALSE;
  mgpp->image_id = image_id;
  mgpp->run_mode = run_mode;



  switch (run_mode)
  {
    case GIMP_RUN_INTERACTIVE:
      if (strcmp(name, PLUG_IN_NAME) == 0)
      {
        /* Possibly retrieve data from a previous run */
        gimp_get_data (PLUG_IN_NAME, mgpp);
        mgpp->osrc_layer_id          = drawable_id;
        mgpp->fdst_layer_id          = gap_morph_exec_find_dst_drawable(image_id, drawable_id);
        run_flag = gap_morph_dialog(mgpp);
        mgpp->do_progress            = TRUE;
        mgpp->master_progress_callback_fptr     = NULL;
        mgpp->progress_callback_fptr            = NULL;
      }
      else if (strcmp(name, PLUG_IN_NAME_ONE_TWEEN) == 0)
      {
        mgpp->tween_mix_factor       = 0.5;
        /* Possibly retrieve data from a previous run */
        gimp_get_data (PLUG_IN_NAME_ONE_TWEEN, mgpp);
        mgpp->osrc_layer_id          = drawable_id;
        mgpp->fdst_layer_id          = gap_morph_exec_find_dst_drawable(image_id, drawable_id);
        run_flag = gap_morph_one_tween_dialog(mgpp);
        mgpp->do_progress            = TRUE;
        mgpp->master_progress_callback_fptr     = NULL;
        mgpp->progress_callback_fptr            = NULL;
      }
      /* dialog for other frame tween processing plug ins see 
       * p_handle_tween_processing_pug_ins
       */
      break;

    case GIMP_RUN_NONINTERACTIVE:
      /* check to see if invoked with the correct number of parameters */
      if ((nparams == G_N_ELEMENTS (in_args))
      && (strcmp(name, PLUG_IN_NAME) == 0))
      {
          mgpp->have_workpointsets     = TRUE;  /* use pointset(s) from file */
          
          /* set defaults for params that may be specified in the workpointfiles
           * (the defaults will take effect only if the file does not contain such settings)
           */
          mgpp->use_quality_wp_selection = FALSE;
          mgpp->use_gravity = FALSE;
          mgpp->gravity_intensity = 2.0;
          mgpp->affect_radius = 100.0;
          
          mgpp->osrc_layer_id          = param[2].data.d_drawable;
          mgpp->fdst_layer_id          = param[3].data.d_drawable;
          mgpp->tween_steps            = param[4].data.d_int32;
          mgpp->render_mode            = param[5].data.d_int32;
          mgpp->create_tween_layers    = param[6].data.d_int32;
          if(param[7].data.d_string != NULL)
          {
            if(param[7].data.d_string[0] != '\0')
            {
              g_snprintf(mgpp->workpoint_file_lower
                      , sizeof(mgpp->workpoint_file_lower)
                      , "%s", param[7].data.d_string);
              g_snprintf(mgpp->workpoint_file_upper
                      , sizeof(mgpp->workpoint_file_upper)
                      , "%s", param[7].data.d_string);
            }
            else
            {
              printf("%s: non-interactive call requires a not-empty workpoint_file_1 parameter\n"
                    , PLUG_IN_NAME
                    );
              status = GIMP_PDB_CALLING_ERROR;
            }
          }
          else
          {
            printf("%s: non-interactive call requires a not-NULL workpoint_file_1 parameter\n"
                    , PLUG_IN_NAME
                    );
            status = GIMP_PDB_CALLING_ERROR;
          }
          
          if(param[8].data.d_string != NULL)
          {
            if(param[8].data.d_string[0] != '\0')
            {
              g_snprintf(mgpp->workpoint_file_upper
                      , sizeof(mgpp->workpoint_file_upper)
                      , "%s", param[8].data.d_string);
            }
          }
          
          run_flag = TRUE;
      }
      else if ((nparams == G_N_ELEMENTS (in_one_tween_args))
      && (strcmp(name, PLUG_IN_NAME_ONE_TWEEN) == 0))
      {
          /* set defaults for params that may be specified in the workpointfiles
           * (the defaults will take effect only if the file does not contain such settings)
           */
          mgpp->use_quality_wp_selection = FALSE;
          mgpp->have_workpointsets     = FALSE;  /* operate with a single workpointfile */
          mgpp->use_gravity = FALSE;
          mgpp->gravity_intensity = 2.0;
          mgpp->affect_radius = 100.0;
          mgpp->tween_steps = 1;  /* (in this mode always render only one tween) */
          mgpp->render_mode = GAP_MORPH_RENDER_MODE_MORPH;
          mgpp->create_tween_layers = TRUE;
          
          mgpp->osrc_layer_id          = param[2].data.d_drawable;
          mgpp->fdst_layer_id          = param[3].data.d_drawable;
          mgpp->tween_mix_factor       = param[4].data.d_float;
          mgpp->do_simple_fade         = (param[5].data.d_int32 != 0);
          if(param[6].data.d_string != NULL)
          {
            if(param[6].data.d_string[0] != '\0')
            {
              g_snprintf(mgpp->workpoint_file_lower
                      , sizeof(mgpp->workpoint_file_lower)
                      , "%s", param[6].data.d_string);
              g_snprintf(mgpp->workpoint_file_upper
                      , sizeof(mgpp->workpoint_file_upper)
                      , "%s", param[6].data.d_string);
            }
            else
            {
              mgpp->have_workpointsets     = FALSE;  /* no pointset file available */
            }
            run_flag = TRUE;
          }
          else
          {
            printf("%s: non-interactive call requires a not-NULL workpoint_file parameter\n"
                    , PLUG_IN_NAME_ONE_TWEEN
                    );
            status = GIMP_PDB_CALLING_ERROR;
          }
          
          
      }
      else if ((nparams == G_N_ELEMENTS (in_one_tween_args))
      && (strcmp(name, PLUG_IN_NAME_TWEEN) == 0))
      {
          /* set defaults for params that may be specified in the workpointfiles
           * (the defaults will take effect only if the file does not contain such settings)
           */
          mgpp->use_quality_wp_selection = FALSE;
          mgpp->have_workpointsets     = FALSE;  /* operate with a single workpointfile */
          mgpp->use_gravity = FALSE;
          mgpp->gravity_intensity = 2.0;
          mgpp->affect_radius = 100.0;
          mgpp->tween_steps = 1;  /* (will be calculated later as difference of handled frame numbers) */
          mgpp->render_mode = GAP_MORPH_RENDER_MODE_MORPH;
          mgpp->create_tween_layers = TRUE;
          
          mgpp->osrc_layer_id = -1;  /* is created later as merged copy of the specified image */
          mgpp->fdst_layer_id = -1;   /* is created later as merged copy of the frame rfered by to_frame_nr parameter  */        
          mgpp->range_to = param[3].data.d_int32;
          mgpp->overwrite_flag = (param[4].data.d_int32 != 0);
          mgpp->do_simple_fade = (param[5].data.d_int32 != 0);
          mgpp->tween_mix_factor       = 1.0;  /* not relevant here */
          if(param[6].data.d_string != NULL)
          {
            if(param[6].data.d_string[0] != '\0')
            {
              g_snprintf(mgpp->workpoint_file_lower
                      , sizeof(mgpp->workpoint_file_lower)
                      , "%s", param[6].data.d_string);
              g_snprintf(mgpp->workpoint_file_upper
                      , sizeof(mgpp->workpoint_file_upper)
                      , "%s", param[6].data.d_string);
            }
            else
            {
              mgpp->have_workpointsets     = FALSE;  /* no pointset file available */
            }
            run_flag = TRUE;
          }
          else
          {
            printf("%s: non-interactive call requires a not-NULL workpoint_file parameter\n"
                    , PLUG_IN_NAME_TWEEN
                    );
            status = GIMP_PDB_CALLING_ERROR;
          }
          
      }
      else if ((nparams == G_N_ELEMENTS (in_workpoint_args))
      && (strcmp(name, PLUG_IN_NAME_WORKPOINTS) == 0))
      {
          mgpp->osrc_layer_id = -1;  /* is created later as merged copy of the specified image */
          mgpp->fdst_layer_id = -1;   /* is created later as merged copy of the frame rfered by to_frame_nr parameter  */        
          mgpp->range_to = param[3].data.d_int32;
          mgpp->numWorkpoints = param[4].data.d_int32;
          mgpp->numOutlinePoints = param[5].data.d_int32;
          mgpp->locateDetailMoveRadius = param[6].data.d_int32;
          mgpp->locateDetailShapeRadius = param[7].data.d_int32;
          mgpp->locateColordiffThreshold = param[8].data.d_float;
          mgpp->edgeColordiffThreshold = param[9].data.d_float;
          run_flag = TRUE;
      }
      else
      {
        printf("%s: non-interactive call wrong nuber %d of params were passed. (%d params are required)\n"
              , PLUG_IN_NAME
              , (int)nparams
              , (int)G_N_ELEMENTS (in_args)
              );
        status = GIMP_PDB_CALLING_ERROR;
      }
      break;

    case GIMP_RUN_WITH_LAST_VALS:
      if (strcmp(name, PLUG_IN_NAME) == 0)
      {
        /* Possibly retrieve data from a previous run */
        gimp_get_data (PLUG_IN_NAME, mgpp);
        mgpp->osrc_layer_id          = drawable_id;
        mgpp->fdst_layer_id          = gap_morph_exec_find_dst_drawable(image_id, drawable_id);
        run_flag = gap_morph_dialog(mgpp);
        mgpp->do_progress            = TRUE;
        mgpp->master_progress_callback_fptr     = NULL;
        mgpp->progress_callback_fptr            = NULL;
      }
      break;

    default:
      break;
  }

  if ((status == GIMP_PDB_SUCCESS)
  && (run_flag))
  {
    if (strcmp(name, PLUG_IN_NAME) == 0)
    {
      gap_morph_execute(mgpp);
      /* Store variable states for next run */
      if (run_mode == GIMP_RUN_INTERACTIVE)
      {
        gimp_set_data (PLUG_IN_NAME, mgpp, sizeof (GapMorphGlobalParams));
      }
    }
    else if ((strcmp(name, PLUG_IN_NAME_ONE_TWEEN) == 0) 
         || (strcmp(name, PLUG_IN_NAME_TWEEN) == 0)
         || (strcmp(name, PLUG_IN_NAME_WORKPOINTS) == 0))
    {
      gint32 tween_layer_id;
      
      if (strcmp(name, PLUG_IN_NAME_ONE_TWEEN) == 0)
      {
        tween_layer_id = gap_morph_render_one_tween(mgpp);
      }
      else
      {
        /* handle processing for PLUG_IN_NAME_TWEEN or PLUG_IN_NAME_WORKPOINTS */
        tween_layer_id = p_handle_tween_processing_pug_ins(mgpp, name);
      }


      values[1].type = GIMP_PDB_DRAWABLE;
      values[1].data.d_int32 = tween_layer_id;
      values[1].data.d_drawable = tween_layer_id;
      *nreturn_vals = 2;
      if (tween_layer_id < 0)
      {
        status = GIMP_PDB_EXECUTION_ERROR;
      }
      else
      {
         if (run_mode == GIMP_RUN_INTERACTIVE)
         {
           /* Store variable states for next run */
           gimp_set_data (name, mgpp, sizeof (GapMorphGlobalParams));
           if(strcmp(name, PLUG_IN_NAME_WORKPOINTS) != 0)
           {
             gimp_display_new(gimp_drawable_get_image(tween_layer_id));
           }
         }
      }
    }
  }

  values[0].data.d_status = status;
}       /* end run */


 
