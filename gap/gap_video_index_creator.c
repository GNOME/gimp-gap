/*  gap_video_index_creator.c
 *  utility plug-in for explicite creation of a video index for list of videofiles.
 *  plug-in by Wolfgang Hofer 2007/04/02
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
 *  (2007/04/02)  v1.0       hof: created
 */

#include "config.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <glib/gstdio.h>
#include <gtk/gtk.h>
#include <libgimp/gimp.h>
#include <libgimp/gimpui.h>

#include "gap_libgapbase.h"
#include "gap_arr_dialog.h"
#include "gap_story_file.h"
#include "gap_story_syntax.h"
#include "gap_libgimpgap.h"

#ifdef GAP_ENABLE_VIDEOAPI_SUPPORT
/* Includes for GAP Video API */
#include <gap_vid_api.h>
#endif

#include "gap-intl.h"

/* Defines */
#define PLUG_IN_NAME        "plug_in_gap_video_index_creator"
#define PLUG_IN_HELP_ID     "plug-in-gap-video-index-creator"
#define PLUG_IN_PRINT_NAME  "Video Index Creator"
#define PLUG_IN_AUTHOR      "Wolfgang Hofer (hof@gimp.org)"
#define PLUG_IN_COPYRIGHT   "Wolfgang Hofer"


int gap_debug = 0;  /* 1 == print debug infos , 0 dont print debug infos */


#define QICK_MODE     0
#define SMART_MODE    1
#define FULLSCAN_MODE 2

#define GAP_VINDEX_PROGRESS_PLUG_IN_PROC  "plug_in_gap_vindex_creation_progress"
#define PROCESSING_STATUS_STRING "@@@PROCESSING"
#define DEFAULT_SMART_PERCENTAGE 15.0

typedef struct {
  gint32  seltrack;
  gchar   videofile[4000];
  gchar   preferred_decoder[100];
  gint32  mode;
  gdouble percentage_smart_mode;   /* range 1.0 upto 100.0 percent */
} VindexValues;


typedef struct GapVideoIndexCreatorProgressParams {  /* nickname vipp */
  GtkWidget    *shell_window;
  GtkListStore     *store;
  GtkWidget        *tv;
  GtkTreeSelection *sel;

  GtkWidget    *progress_bar_master;
  GtkWidget    *progress_bar_sub;
  gboolean      cancel_video_api;
  gboolean      cancel_immedeiate_request;
  gboolean      cancel_enabled_smart;
  gboolean      processing_finished;
  VindexValues *val_ptr;
  GapStoryVideoFileRef  *vref;
  GapStoryVideoFileRef  *vref_list;
  gint32        timertag;
  t_GVA_Handle  *gvahand;
  GTimeVal      startTime;
  GTimeVal      endTime;
  
  gint32        numberOfVideos;
  gint32        numberOfValidVideos;
  gint32        countVideos;

  gdouble       breakPercentage;
  gint32        breakFrames;
  
} GapVideoIndexCreatorProgressParams;

static VindexValues glob_vindex_vals =
{
    1               /* seltrack */
 , "Test.mpg"       /* videofile */
 , "libavformat"    /* preferred_decoder*/
 , FULLSCAN_MODE    /* mode */
 , DEFAULT_SMART_PERCENTAGE             /* percentage_smart_mode */
};


static void  query (void);
static void  run (const gchar *name,          /* name of plugin */
     gint nparams,               /* number of in-paramters */
     const GimpParam * param,    /* in-parameters */
     gint *nreturn_vals,         /* number of out-parameters */
     GimpParam ** return_vals);  /* out-parameters */

static void      p_do_processing (GapVideoIndexCreatorProgressParams *vipp);
static gboolean  p_check_videofile(const char *filename, gint32 seltrack
                                     , const char *preferred_decoder);
static void      p_create_video_index(const char *filename, gint32 seltrack
                                     , const char *preferred_decoder
                                     , GapVideoIndexCreatorProgressParams *vipp);
static void      p_set_vref_userdata(GapStoryVideoFileRef  *vref, const char *userdata);
static void      p_set_userdata_processingstatus_check_videofile(GapStoryVideoFileRef  *vref
                                     , GapVideoIndexCreatorProgressParams *vipp);

static void      p_make_all_video_index(const char *filename, gint32 seltrack
                                     , const char *preferred_decoder
                                     ,GapVideoIndexCreatorProgressParams *vipp
                                     );

static gint      p_vindex_dialog (VindexValues *val_ptr);

static void      p_vipp_dlg_destroy (GtkWidget *widget,
                    GapVideoIndexCreatorProgressParams *vipp_1);
static void      p_cancel_button_cb (GtkWidget *w,
                    GapVideoIndexCreatorProgressParams *vipp);

static void      p_elapsedTimeToString (GapVideoIndexCreatorProgressParams *vipp, char *timeString, gint sizeOfTimeString);
static void      p_print_vref_list (GapVideoIndexCreatorProgressParams *vipp, GapStoryVideoFileRef  *vref_list);


static void      p_tree_fill (GapVideoIndexCreatorProgressParams *vipp, GapStoryVideoFileRef  *vref_list);
static void      p_create_video_list_widget(GapVideoIndexCreatorProgressParams *vipp, GtkWidget *vbox);
static void      p_create_progress_window(GapVideoIndexCreatorProgressParams *vipp);
static void      on_timer_start(GapVideoIndexCreatorProgressParams *vipp);
static gboolean  p_vid_progress_callback(gdouble progress
                                  ,gpointer user_data
                                  );



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
                  { GIMP_PDB_STRING,   "videofile", "name of videofile. this file can be a video or textfile containing"
                                                    " names of videofiles or a storyboard file containing references to videclips"},
                  { GIMP_PDB_INT32,    "seltrack", "selected video track number >= 1 (most videos have only one track)"},
                  { GIMP_PDB_STRING,   "preferred_decoder", "name of the decoder (libavformat or libmpeg3) "},
                  { GIMP_PDB_INT32,    "mode", "0 .. QICK_MODE, 1 .. SMART_MODE, 2 .. FULLSCAN_MODE"},
                  { GIMP_PDB_FLOAT,    "percentage_smart_mode", "1 to 100.0 perceent. stop verifying timecodes after the specidfeid percentage is scanned"
                                                   " and no critical errors were found so far. (only relevant in SMART_MODE)"}
  };


static gint global_number_in_args = G_N_ELEMENTS (in_args);


/* Functions */

MAIN ()

static void query (void)
{

  gimp_plugin_domain_register (GETTEXT_PACKAGE, LOCALEDIR);


  /* the actual installation of the adjust plugin */
  gimp_install_procedure (PLUG_IN_NAME,
                          "Create video index for videofile(s)",
                          "This plug-in creates a video index for the specified videofile, "
                          "index creation is skipped if a valid vide index already exists. "
                          "the specified videofilename may refere to a textfile that contains "
                          "a list of videofiles (one videofilename per line)."
                          "in this case all referred vidofiles are processed.",
                          PLUG_IN_AUTHOR,
                          PLUG_IN_COPYRIGHT,
                          GAP_VERSION_WITH_DATE,
                          N_("Video Index Creation..."),
                          NULL,
                          GIMP_PLUGIN,
                          global_number_in_args,
                          0,
                          in_args,
                          NULL);

  {
    /* Menu names */
    const char *menupath_toolbox_video_split = N_("<Toolbox>/Xtns/");

    //gimp_plugin_menu_branch_register("<Toolbox>", "Video");
    gimp_plugin_menu_register (PLUG_IN_NAME, menupath_toolbox_video_split);
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

  if(gap_debug) fprintf(stderr, "\n\nDEBUG: run %s\n", name);

  /* initialize the return of the status */
  values[0].type = GIMP_PDB_STATUS;
  values[0].data.d_status = status;
  values[1].type = GIMP_PDB_DRAWABLE;
  values[1].data.d_drawable = -1;
  *nreturn_vals = 1;
  *return_vals = values;


  /* how are we running today? */
  switch (run_mode)
  {
    case GIMP_RUN_INTERACTIVE:
      /* Possibly retrieve data from a previous run */
      gimp_get_data (PLUG_IN_NAME, &glob_vindex_vals);

      /* Get information from the dialog */
      if (p_vindex_dialog(&glob_vindex_vals) != 0)
                return;
      break;

    case GIMP_RUN_NONINTERACTIVE:
      /* check to see if invoked with the correct number of parameters */
      if (nparams == global_number_in_args)
      {

          if(param[1].data.d_string != NULL)
          {
            g_snprintf(glob_vindex_vals.videofile, sizeof(glob_vindex_vals.videofile)
                       , "%s"
                       , param[1].data.d_string
                       );
          }
          glob_vindex_vals.seltrack    = (gint) param[2].data.d_int32;
          if(param[3].data.d_string != NULL)
          {
            g_snprintf(glob_vindex_vals.preferred_decoder, sizeof(glob_vindex_vals.preferred_decoder)
                       , "%s"
                       , param[3].data.d_string
                       );
          }
          glob_vindex_vals.mode                     = (gint) param[4].data.d_int32;
          glob_vindex_vals.percentage_smart_mode    = (gint) param[5].data.d_float;
      }
      else
      {
        status = GIMP_PDB_CALLING_ERROR;
      }

      break;

    case GIMP_RUN_WITH_LAST_VALS:
      /* Possibly retrieve data from a previous run */
      gimp_get_data (PLUG_IN_NAME, &glob_vindex_vals);

      break;

    default:
      break;
  }

  if (status == GIMP_PDB_SUCCESS)
  {
    GapVideoIndexCreatorProgressParams vip_struct;
    GapVideoIndexCreatorProgressParams *vipp;
    
    vipp = &vip_struct;
    vipp->vref = NULL;
    vipp->vref_list = NULL;
    vipp->shell_window = NULL;
    vipp->gvahand = NULL;
    vipp->tv = NULL;
    vipp->progress_bar_master = NULL;
    vipp->progress_bar_sub = NULL;
    vipp->cancel_video_api = FALSE;
    vipp->timertag = -1;
    vipp->cancel_immedeiate_request = FALSE;

    vipp->val_ptr = &glob_vindex_vals;
    
    /* Store variable states for next run */
    if (run_mode == GIMP_RUN_INTERACTIVE)
    {
      gimp_set_data (PLUG_IN_NAME, &glob_vindex_vals, sizeof (VindexValues));
    }
    if (run_mode == GIMP_RUN_NONINTERACTIVE)
    {
      /* Run the main processing */
      p_do_processing(vipp);
    }
    else
    {
      /* creaate and open the progress dialog window
       + (dferred start main processing from the progres dialog)
       */
      p_create_progress_window(vipp);
    }
  }
  values[0].data.d_status = status;
}       /* end run */


/* ---------------
 * p_do_processing
 * ---------------
 */
static void
p_do_processing (GapVideoIndexCreatorProgressParams *vipp)
{
  p_make_all_video_index(vipp->val_ptr->videofile
                        ,vipp->val_ptr->seltrack
                        ,vipp->val_ptr->preferred_decoder
                        ,vipp
                        );
}       /* end p_do_processing */


/* --------------------------------
 * p_check_videofile
 * --------------------------------
 * check if specified filename is a videofile.
 *
 * return FALSE if specified video filename could not be opened as videofile.
 *
 */
static gboolean
p_check_videofile(const char *filename, gint32 seltrack
  , const char *preferred_decoder)
{
  t_GVA_Handle  *gvahand;

  /* first check the filename for known video extensions (.avi, .mpeg ...)
   * before attempt to open as video.
   * Note that the check_sig call of the gap_vid_api_gimp.c API implementation
   * allows access to anim frames via the videoapi.
   * But it does not make sense to detect anim frame images as video here, where
   * video index creation makes no sense and therefore is not supported.
   */

  if(gap_story_filename_is_videofile_by_ext(filename))
  {
    gvahand =  GVA_open_read_pref(filename
                                  , seltrack
                                  , 1 /* aud_track */
                                  , preferred_decoder
                                  , FALSE  /* use MMX if available (disable_mmx == FALSE) */
                                  );
    if(gvahand)
    {
      GVA_close(gvahand);
      return(TRUE);
    }
  }

  return(FALSE);

}  /* end p_check_videofile */


/* --------------------------------
 * p_is_valid_vindex_available
 * --------------------------------
 */
static gboolean
p_is_valid_vindex_available(t_GVA_Handle  *gvahand)
{
  gboolean    l_have_valid_vindex;

  l_have_valid_vindex = FALSE;
  if(gvahand->vindex)
  {
    if(gvahand->vindex->total_frames > 0)
    {
      l_have_valid_vindex = TRUE;
    }
  }
  
  return (l_have_valid_vindex);
}

/* --------------------------------
 * p_create_video_index
 * --------------------------------
 * check if specified filename is a videofile and if there is
 * a valid video index available for that videofile.
 * In case there is no valid video index, this procedure
 * will create the video index.
 *
 * return FALSE if specified video filename could not be opened as videofile.
 *
 */
static void
p_create_video_index(const char *filename, gint32 seltrack
  , const char *preferred_decoder,GapVideoIndexCreatorProgressParams *vipp)
{
  char *vindex_file;
  gboolean    l_have_valid_vindex;
  t_GVA_Handle  *gvahand;

  vipp->cancel_enabled_smart = FALSE;

  vindex_file = NULL;
  l_have_valid_vindex = FALSE;

  gvahand =  GVA_open_read_pref(filename
                                  , seltrack
                                  , 1 /* aud_track */
                                  , preferred_decoder
                                  , FALSE  /* use MMX if available (disable_mmx == FALSE) */
                                  );



  if(gvahand)
  {
    vipp->gvahand = gvahand;
    
    /* gvahand->emulate_seek = TRUE; */
    gvahand->do_gimp_progress = FALSE;

    gvahand->progress_cb_user_data = vipp;
    gvahand->fptr_progress_callback = p_vid_progress_callback;
    
    if ((vipp->val_ptr->mode == QICK_MODE)
    || (vipp->val_ptr->mode == SMART_MODE))
    {
      t_GVA_SeekSupport seekSupp;

      seekSupp = GVA_check_seek_support(gvahand);
      if (seekSupp == GVA_SEEKSUPP_NATIVE)
      {
        if(gap_debug)
        {
          printf("NATIVE SEEK seems to work for video:%s\n  SKIPPING video index creation due to QUICK mode.\n"
             , filename
             );
        }
        if (vipp->val_ptr->mode == QICK_MODE)
        {
          vipp->gvahand = NULL;
          GVA_close(gvahand);
          p_set_vref_userdata(vipp->vref, _("NO vindex created (QUICK)"));

          return;
        }
        vipp->cancel_enabled_smart = TRUE;
      }
    }
    
    /* get video index name */
    {
      t_GVA_DecoderElem *dec_elem;

      dec_elem = (t_GVA_DecoderElem *)gvahand->dec_elem;
      if(dec_elem->decoder_name)
      {
        vindex_file = GVA_build_videoindex_filename(filename
                                             ,seltrack  /* track */
                                             ,dec_elem->decoder_name
                                             );
      }
    }

    l_have_valid_vindex = p_is_valid_vindex_available(gvahand);

    if (vindex_file == NULL)
    {
      vindex_file = g_strdup("<null>");
    }

    if (l_have_valid_vindex)
    {
      p_set_vref_userdata(vipp->vref, _("vindex already OK"));
      if(gap_debug)
      {
        printf("VALID VIDEO INDEX found for video:%s\n  (index:%s)\n"
           , filename
           , vindex_file
           );
      }
    }
    else
    {
      if(gap_debug)
      {
        printf("CREATING VIDEO INDEX for video:%s\n  (index:%s)\n"
           , filename
           , vindex_file
           );
      }

      gvahand->create_vindex = TRUE;
      GVA_count_frames(gvahand);      /* here we CRREATE the vindex */

      if ((vipp->cancel_video_api != TRUE)
      && (TRUE == p_is_valid_vindex_available(gvahand)))
      {
        p_set_vref_userdata(vipp->vref, _("vindex created (FULLSCAN OK)"));
      }
      else
      {
        char *usrdata;
        if (vipp->val_ptr->mode == SMART_MODE)
        {
          if(g_file_test(vindex_file, G_FILE_TEST_EXISTS))
          {
            if(gap_debug)
            {
              printf("DELETE uncomplete vindex:%s\n", vindex_file);
            }
            g_remove(vindex_file);
          }

          usrdata = g_strdup_printf(_("NO vindex created (SMART %.1f%% %d frames)")
                                   ,(float)vipp->breakPercentage
                                   ,(int)vipp->breakFrames
                                   );
        }
        else
        {
          usrdata = g_strdup_printf(_("incomplete vindex created (%d frames)")
                                   ,(int)gvahand->frame_counter
                                   );
        }
        p_set_vref_userdata(vipp->vref, usrdata);
        g_free(usrdata);
      }
    }


    if (vindex_file != NULL)
    {
      g_free(vindex_file);
    }

    vipp->gvahand = NULL;
    GVA_close(gvahand);
  }


}  /* end p_create_video_index */

/* -----------------------------------------------
 * p_set_vref_userdata
 * -----------------------------------------------
 */
static void
p_set_vref_userdata(GapStoryVideoFileRef  *vref, const char *userdata)
{
  if(vref)
  {
    if(vref->userdata != NULL)
    {
      g_free(vref->userdata);
    }
    vref->userdata = NULL;
    if(userdata != NULL)
    {
      vref->userdata = g_strdup(userdata);
    }
  }

}  /* end p_set_vref_userdata */

/* -----------------------------------------------
 * p_set_userdata_processingstatus_check_videofile
 * -----------------------------------------------
 * set userdata to NULL for files that are NO videofiles
 * (vref elements with userdata NULL are skipped at further processing
 * but show up in the tv widget list)
 *
 * Videofiles that could be opened successfully get
 * the string "unprocessed" as userdata attribute.
 */
static void
p_set_userdata_processingstatus_check_videofile(GapStoryVideoFileRef  *vref_list, GapVideoIndexCreatorProgressParams *vipp)
{
  gboolean l_file_is_videofile;
  gint32  numberOfFiles;
  gint32  filesCount;
  GapStoryVideoFileRef  *vref;
  
  
  numberOfFiles = 0;
  vipp->numberOfVideos = 0;
  for(vref = vref_list; vref != NULL; vref = vref->next)
  {
    numberOfFiles++;
    vipp->numberOfVideos++;

  }

  filesCount = 0;
  for(vref = vref_list; vref != NULL; vref = vref->next)
  {
     if(gap_debug)
     {
        printf("filesCount:%d numberOfFiles:%d\n"
          ,(int)filesCount
          ,(int)numberOfFiles
          );
     }

    if(vipp->progress_bar_sub)
    {
      gtk_progress_bar_set_text(GTK_PROGRESS_BAR(vipp->progress_bar_sub), _("counting and checking videofiles"));
      gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(vipp->progress_bar_sub)
                                    , CLAMP(((gdouble)filesCount / (gdouble)numberOfFiles), 0.0, 1.0)
                                    );
      while(g_main_context_iteration(NULL, FALSE));
      
      
      if(vipp->cancel_immedeiate_request == TRUE)
      {
        p_vipp_dlg_destroy(NULL, vipp);
        return;
      }
      
    }
    filesCount++;
    
    l_file_is_videofile = FALSE;
    if(g_file_test(vref->videofile, G_FILE_TEST_EXISTS))
    {
      l_file_is_videofile = p_check_videofile(vref->videofile, vref->seltrack, vref->preferred_decoder);

      if(gap_debug)
      {
        printf("FILE_EXISTS:%s\n .. is_vieofile:%d\n"
          , vref->videofile
          , (int)l_file_is_videofile
          );
      }
    }

    if (l_file_is_videofile)
    {
      p_set_vref_userdata(vref, _("unprocessed"));
      vipp->numberOfValidVideos++;
    }
    else
    {
      p_set_vref_userdata(vref, NULL);
    }
  }


}  /* end p_set_userdata_processingstatus_check_videofile */



/* --------------------------------
 * p_make_all_video_index
 * --------------------------------
 * the specified filename shall be the name of a single videofile
 * or the name of a textfile containing names of videofiles
 * (one name per line)
 * This procedure processes all the specified videofilenames
 * by creating a videoindex for all videofiles that
 * do not yet have a valid videoindex.
 */
static void
p_make_all_video_index(const char *filename, gint32 seltrack
         , const char *preferred_decoder, GapVideoIndexCreatorProgressParams *vipp)
{
#define BUF_SIZE 4000
  FILE     *l_fp;
  char      l_buf[BUF_SIZE];
  gboolean  l_file_is_videofile;
  gint      l_video_count;
  
  GapStoryVideoFileRef  *vref_list;
  GapStoryVideoFileRef  *vref;

  vipp->numberOfVideos = 0;
  vipp->numberOfValidVideos = 0;
  vref_list = NULL;

 
  /* check if filename is a single viedeofile */
  l_file_is_videofile = p_check_videofile(filename, seltrack, preferred_decoder);
  if (l_file_is_videofile)
  {
    vipp->numberOfVideos = 1;
    vipp->numberOfValidVideos = 1;
    vref_list = p_new_GapStoryVideoFileRef(filename, seltrack, preferred_decoder
                                           , 1  /* max_ref_framenr */
                                           );
    p_set_vref_userdata(vref_list, _("unprocessed"));
  }
  else
  {
    /* 2nd attempt: if the filename does not directly refere to a videofile
     * it may be a textfile containing a list of videofile names.
     */
    l_fp = g_fopen(filename, "r");
    if(l_fp)
    {
      while(NULL != fgets(l_buf, BUF_SIZE-1, l_fp))
      {
        l_buf[BUF_SIZE-1] = '\0';


        /* chop trailing newline and whitespace */
        gap_file_chop_trailingspace_and_nl(&l_buf[0]);

        if((l_buf[0] == '\0')
        || (l_buf[0] == '#'))
        {
          continue;
        }

        if(gap_debug)
        {
          printf("LINE scanned:%s\n", &l_buf[0]);
        }
        
        if((strcmp(&l_buf[0], GAP_STBKEY_STB_HEADER) == 0)
        || (strcmp(&l_buf[0], GAP_STBKEY_CLIP_HEADER) == 0))
        {
          break;
        }

        vref = p_new_GapStoryVideoFileRef(&l_buf[0], seltrack, preferred_decoder
                                       , 1  /* max_ref_framenr */
                                       );
        /* add new elem at begin of list */
        vref->next =  vref_list;
        vref_list = vref;
      }

      fclose(l_fp);
      p_set_userdata_processingstatus_check_videofile(vref_list, vipp);
    }

  }

  
  if ((vref_list == NULL)
  || (vipp->numberOfValidVideos == 0))
  {
    GapStoryBoard *stb;
    
    /* 3rd attempt: check if file is a storyboard file that contains movie clip references
     * it may be a textfile containing a list of videofile names.
     */
    stb = gap_story_parse(filename);
    vref_list = gap_story_get_video_file_ref_list(stb);
    p_set_userdata_processingstatus_check_videofile(vref_list, vipp);
  }

  if (vipp->tv != NULL)
  {
    p_tree_fill (vipp, vref_list);
  }
  
  vipp->vref_list = vref_list;
  l_video_count = 0;
  for(vref = vref_list; (vref != NULL && vipp->numberOfVideos > 0); vref = vref->next)
  {
    vipp->cancel_video_api = FALSE;
    vipp->vref = vref;
    
    
    if(gap_debug)
    {
      printf("vref->videofile: %s\n  seltrack:%d preferred_decoder:%s"
             , vref->videofile
             , (int)vref->seltrack
             , vref->preferred_decoder
             );
    }


    if(vipp->progress_bar_master)
    {
      gchar  *message;
      gchar  *suffix;
      char timeString[20];

      gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(vipp->progress_bar_master)
                                    , CLAMP(((gdouble)l_video_count / (gdouble)vipp->numberOfVideos), 0.0, 1.0)
                                    );
      g_get_current_time(&vipp->endTime);
      p_elapsedTimeToString (vipp, &timeString[0], sizeof(timeString));
      
      l_video_count++;
      
      suffix = g_strdup_printf(_("  %s (%d of %d)")
         ,timeString
         ,(int)l_video_count
         ,(int)vipp->numberOfVideos
         );

      message = gap_base_shorten_filename(NULL   /* prefix */
                        ,vref->videofile        /* filenamepart */
                        ,suffix                 /* suffix */
                        ,90                     /* l_max_chars */
                        );

      
      gtk_progress_bar_set_text(GTK_PROGRESS_BAR(vipp->progress_bar_master), message);
      g_free(suffix);
      g_free(message);

    }

    if(vref->userdata != NULL)
    {
      p_set_vref_userdata(vref, PROCESSING_STATUS_STRING);
      if (vipp->tv != NULL)
      {
        p_tree_fill (vipp, vref_list);
      }
      p_create_video_index(vref->videofile, vref->seltrack, vref->preferred_decoder, vipp);
      if (vipp->tv != NULL)
      {
        p_tree_fill (vipp, vref_list);
      }
      if (vipp->cancel_immedeiate_request == TRUE)
      {
        if(gap_debug)
        {
          printf("CANCEL_IMMEDEIATE_REQUEST  vref->videofile: %s\n  seltrack:%d preferred_decoder:%s  userdata:%s"
                 , vref->videofile
                 , (int)vref->seltrack
                 , vref->preferred_decoder
                 , vref->userdata
                 );
        }
        vipp->processing_finished = TRUE;
        return;
      }
    }


    if(vipp->progress_bar_master)
    {
       gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(vipp->progress_bar_master)
                                    , CLAMP(((gdouble)l_video_count / (gdouble)vipp->numberOfVideos), 0.0, 1.0)
                                    );
    }
  }
  
}  /* end p_make_all_video_index */


/* ------------------
 * p_vindex_dialog
 * ------------------
 * initial dialog to specify parameters.
 *
 *   return  0 .. OK
 *          -1 .. in case of Error or cancel
 */
static gint
p_vindex_dialog(VindexValues *val_ptr)
{
#define VR_DIALOG_ARGC 7
#define VR_DECODERS_SIZE 2
#define MODE_SIZE 3
  static GapArrArg  argv[VR_DIALOG_ARGC];
  gint ii;
  gint ii_seltrack;
  gint ii_preferred_decoder;
  gint ii_mode;
  gint ii_percentage_smart_mode;
  static char *radio_modes[VR_DECODERS_SIZE]  = {"libavformat", "libmpeg3" };
  static char *mode_args[MODE_SIZE]    = { "QUICK", "SMART", "FULLSCAN" };
  static char *mode_help[MODE_SIZE]    = { N_("Conditional video index creation, "
                                              " based on a few quick timcode probereads.\n"
                                              "Skip index creation if native seek seems possible\n"
                                              "\nWARNING: positioning via native seek may not work exact in case critical "
                                              "timecode steps were not detected in the quick test)")
                                         , N_("Conditional video index creation, "
                                              "based on probereads for the specified percentage of frames.\n"
                                              "Skip index creation if native seek seems possible "
                                              "and no critical timecode steps are detected in the probereads so far.\n"
                                              "\nWARNING: positioning via native seek may not work exact in case critical "
                                              "timecode steps were not detected in the probereads.")
                                         , N_("Create video index. Requires unconditional full scan of all frames. "
                                              "Native seek is enabled only in case all timecodes are OK.")
                                         };

  gimp_ui_init ("vindex_progress", FALSE);
  
  
  ii=0;
  gap_arr_arg_init(&argv[ii], GAP_ARR_WGT_FILESEL);
  argv[ii].label_txt = _("Videofile:");
  argv[ii].help_txt  = _("Name of a videofile to create a videoindex for."
                        " You also can enter the name of a textfile containing a list"
                        " of videofile names (one name per line) to create all videoindexes at once."
                        " a video index enables fast and exact positioning in the videofile.");
  argv[ii].text_buf_len = sizeof(val_ptr->videofile);
  argv[ii].text_buf_ret = &val_ptr->videofile[0];
  argv[ii].entry_width = 400;



  ii++; gap_arr_arg_init(&argv[ii], GAP_ARR_WGT_OPT_ENTRY); ii_preferred_decoder = ii;
  argv[ii].label_txt = _("Decoder:");
  argv[ii].help_txt  = _("Create video index based on the specified decoder library");
  argv[ii].radio_argc  = VR_DECODERS_SIZE;
  argv[ii].radio_argv = radio_modes;
  argv[ii].radio_ret  = 0;
  argv[ii].has_default = TRUE;
  argv[ii].radio_default = 0;
  argv[ii].text_buf_ret = &val_ptr->preferred_decoder[0];
  argv[ii].text_buf_len = sizeof(val_ptr->preferred_decoder);
  argv[ii].text_buf_default = g_strdup("libavformat\0");


  ii++; gap_arr_arg_init(&argv[ii], GAP_ARR_WGT_INT); ii_seltrack = ii;
  argv[ii].constraint = TRUE;
  argv[ii].label_txt = _("Track:");
  argv[ii].help_txt  = _("Select video track");
  argv[ii].int_min   = (gint)1;
  argv[ii].int_max   = (gint)100;
  argv[ii].int_ret   = (gint)glob_vindex_vals.seltrack;
  argv[ii].entry_width = 60;
  argv[ii].has_default = TRUE;
  argv[ii].int_default = 1;


  ii++; gap_arr_arg_init(&argv[ii], GAP_ARR_WGT_RADIO); ii_mode = ii;
  argv[ii].constraint = TRUE;
  argv[ii].label_txt = _("Mode:");
  argv[ii].help_txt  = _("operation mode");
  argv[ii].radio_argc  = (gint)MODE_SIZE;
  argv[ii].radio_argv   = mode_args;
  argv[ii].radio_help_argv = mode_help;
  argv[ii].radio_ret   = (gint)glob_vindex_vals.mode;
  argv[ii].has_default = TRUE;
  argv[ii].radio_default  = FULLSCAN_MODE;

  ii++; gap_arr_arg_init(&argv[ii], GAP_ARR_WGT_FLT); ii_percentage_smart_mode = ii;
  argv[ii].constraint = TRUE;
  argv[ii].label_txt = _("Percentage:");
  argv[ii].help_txt  = _("stop scan after percentage reached and no unplausible timecode was detected so far (only relevant in smart mode)");
  argv[ii].flt_min   =  1.0;
  argv[ii].flt_max   = (gint)100.0;
  argv[ii].flt_step  =  1.0;
  argv[ii].flt_ret   = (gint)glob_vindex_vals.percentage_smart_mode;
  argv[ii].entry_width = 60;
  argv[ii].has_default = TRUE;
  argv[ii].flt_default = DEFAULT_SMART_PERCENTAGE;



  ii++; gap_arr_arg_init(&argv[ii], GAP_ARR_WGT_DEFAULT_BUTTON);
  argv[ii].label_txt = _("Default");
  argv[ii].help_txt  = _("Reset all parameters to default values");

  ii++; gap_arr_arg_init(&argv[ii], GAP_ARR_WGT_HELP_BUTTON);
  argv[ii].help_id = PLUG_IN_HELP_ID;

  if(TRUE == gap_arr_ok_cancel_dialog(_("Video Index Creation"),
                            _("Settings :"),
                            VR_DIALOG_ARGC, argv))
  {
      val_ptr->seltrack = (gint)(argv[ii_seltrack].int_ret);
      val_ptr->mode = (gint)(argv[ii_mode].int_ret);
      val_ptr->percentage_smart_mode = (gdouble)(argv[ii_percentage_smart_mode].flt_ret);
      
      if(gap_debug)
      {
        printf("p_vindex_dialog: PRAMETERS entered via dialog:\n");
        printf("  videofile: %s\n", &val_ptr->videofile[0]);
        printf("  decoder: %s\n", &val_ptr->preferred_decoder[0]);
        printf("  ii_preferred_decoder:%d\n", (int)ii_preferred_decoder);
        printf("  seltrack: %d\n", (int)val_ptr->seltrack);
        printf("  mode: %d\n", (int)val_ptr->mode);
        printf("  percentage_smart_mode: %f\n", (float)val_ptr->percentage_smart_mode);
      }
      return 0;
  }
  else
  {
      return -1;
  }
}               /* end p_vindex_dialog */


/* ========================================== progress dialog procedures ===================== */

/* ---------------------------------
 * p_vipp_dlg_destroy
 * ---------------------------------
 */
static void
p_vipp_dlg_destroy (GtkWidget *widget,
                 GapVideoIndexCreatorProgressParams *vipp_1)
{
  GtkWidget *dialog;
  GapVideoIndexCreatorProgressParams *vipp;

  dialog = NULL;
  
  if(vipp_1 != NULL)
  {
    vipp = vipp_1;
  }
  else
  {
    vipp = (GapVideoIndexCreatorProgressParams *)g_object_get_data(G_OBJECT(widget), "vipp");
  }
  
  
  if(vipp)
  {
    dialog = vipp->shell_window;
    if(dialog)
    {
      vipp->shell_window = NULL;
      vipp->tv = NULL;
      vipp->progress_bar_master = NULL;
      vipp->progress_bar_sub = NULL;
      vipp->cancel_video_api = TRUE;
      vipp->cancel_immedeiate_request = TRUE;
      gtk_widget_destroy (dialog);
    }
  }

  gtk_main_quit ();

}  /* end p_vipp_dlg_destroy */

/* -----------------------------
 * p_cancel_button_cb
 * -----------------------------
 */
static void
p_cancel_button_cb (GtkWidget *w,
                    GapVideoIndexCreatorProgressParams *vipp)
{

  if(gap_debug)
  {
    printf("VINDEX-PROGRESS-CANCEL BUTTON clicked\n");
  }

  if(vipp)
  {
    vipp->cancel_immedeiate_request = TRUE;
    if (vipp->processing_finished)
    {
      p_vipp_dlg_destroy(NULL, vipp);
    }
  }

}  /* end p_cancel_button_cb */


/* ----------------------------
 * p_elapsedTimeToString
 * ----------------------------
 */
static void
p_elapsedTimeToString (GapVideoIndexCreatorProgressParams *vipp, char *timeString, gint sizeOfTimeString)
{
  glong  secondsElapsed;
  glong  microSecondsElapsed;
  gint32 tmsec;

  secondsElapsed = vipp->endTime.tv_sec - vipp->startTime.tv_sec;
  microSecondsElapsed = vipp->endTime.tv_usec - vipp->startTime.tv_usec;
  tmsec = secondsElapsed * 1000 + ((microSecondsElapsed /1000) % 1000);
  gap_timeconv_msecs_to_timestr(tmsec, timeString, sizeOfTimeString);
}  /* end  p_elapsedTimeToString */


/* ----------------------------
 * p_print_vref_list
 * ----------------------------
 * print the vref_list to stdout
 */
static void
p_print_vref_list (GapVideoIndexCreatorProgressParams *vipp, GapStoryVideoFileRef  *vref_list)
{
  gint          count_elem;
  GapStoryVideoFileRef  *vref;

  printf("\n\n----------------------------------------------\n");
  printf("Video index creation processing results:\n");
  printf("----------------------------------------------\n");

  count_elem = 0;
  for(vref = vref_list; vref != NULL; vref = (GapStoryVideoFileRef *) vref->next)
  {
     gchar *label;
     gchar *video_filename;
     gchar *processing_status;

     label = g_strdup_printf("%3d.", (int)count_elem +1);
     video_filename = gap_base_shorten_filename(NULL   /* prefix */
                        ,vref->videofile        /* filenamepart */
                        ,NULL                   /* suffix */
                        ,90                     /* l_max_chars */
                        );
     if (vref->userdata == NULL)
     {
       processing_status = g_strdup(_(" ** no video **"));
     }
     else
     {
       if (strcmp(PROCESSING_STATUS_STRING, vref->userdata) == 0)
       {
         processing_status = g_strdup(_("processing not finished"));
        }
       else
       {
         processing_status = g_strdup(vref->userdata);
       }
     }

     printf("%3s %-90.90s %s\n"
            , label             /* visible number starting at 1 */
            , video_filename
            , processing_status
            );


     g_free (label);
     g_free (video_filename);
     g_free (processing_status);
     
     count_elem++;
  }

  {
    char timeString[20];

    p_elapsedTimeToString (vipp, &timeString[0], sizeof(timeString));
    
    printf("\nElapsed Time  %s\n", &timeString[0]);
    printf("----------------------------------------------\n");
  }
}  /* end p_print_vref_list */


/* ----------------------------
 * p_tree_fill
 * ----------------------------
 * fill videofile names into the treeview
 */
static void
p_tree_fill (GapVideoIndexCreatorProgressParams *vipp, GapStoryVideoFileRef  *vref_list)
{
  GtkTreeIter   iter;
  GtkTreePath  *treePathCurrent;
  GapStoryVideoFileRef  *vref;
  gint          count_elem;


  vipp->store = gtk_list_store_new (4, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING);
  gtk_tree_view_set_model (GTK_TREE_VIEW (vipp->tv)
                          ,GTK_TREE_MODEL (vipp->store)
                          );
  g_object_unref (vipp->store);

  count_elem = 0;
  treePathCurrent = NULL;
  for(vref = vref_list; vref != NULL; vref = (GapStoryVideoFileRef *) vref->next)
  {
     gchar *numtxt;
     gchar *label;
     gchar *video_filename;
     gchar *processing_status;
     gboolean      currentFlag;

     currentFlag = FALSE;

     label = g_strdup_printf("%3d.", (int)count_elem +1);
     numtxt = g_strdup_printf("%d", (int)count_elem);
     video_filename = gap_base_shorten_filename(NULL   /* prefix */
                        ,vref->videofile        /* filenamepart */
                        ,NULL                   /* suffix */
                        ,90                     /* l_max_chars */
                        );
     if (vref->userdata == NULL)
     {
       processing_status = g_strdup(_(" ** no video **"));
     }
     else
     {
       if (strcmp(PROCESSING_STATUS_STRING, vref->userdata) == 0)
       {
         processing_status = g_strdup(_("processing in progress"));
         currentFlag = TRUE;
        }
       else
       {
         processing_status = g_strdup(vref->userdata);
       }
     }

     gtk_list_store_append (vipp->store, &iter);
     gtk_list_store_set (vipp->store, &iter
                        ,0, numtxt            /* internal invisible number starting at 0 */
                        ,1, label             /* visible number starting at 1 */
                        ,2, video_filename
                        ,3, processing_status
                        ,-1);

     if(currentFlag == TRUE)
     {
         treePathCurrent = gtk_tree_model_get_path (GTK_TREE_MODEL (vipp->store), &iter);
     }

     g_free (numtxt);
     g_free (label);
     g_free (video_filename);
     g_free (processing_status);
     
     count_elem++;
  }

  if (count_elem == 0)
  {
    gtk_list_store_append (vipp->store, &iter);

    gtk_list_store_set (vipp->store, &iter
                       ,0, "-1"
                       ,1, " "
                       ,2, _("** Empty **")
                       ,3, " "
                       ,-1);
  }

  gtk_tree_model_get_iter_first (GTK_TREE_MODEL (vipp->store), &iter);
  gtk_tree_selection_select_iter (vipp->sel, &iter);
  
  gtk_tree_selection_unselect_all(vipp->sel);
  if(treePathCurrent != NULL)
  {
     gtk_tree_view_set_cursor(GTK_TREE_VIEW(vipp->tv)
                             ,treePathCurrent
                             ,NULL    /* focus_column */
                             ,FALSE   /* start_editing */
                             );

  }
}  /* end p_tree_fill */


/* -----------------------------
 * p_create_video_list_widget
 * -----------------------------
 */
void
p_create_video_list_widget(GapVideoIndexCreatorProgressParams *vipp, GtkWidget *vbox)
{
  GtkWidget       *scrolled_window;
  GtkCellRenderer *renderer;

  /* list : list in a scrolled_win */

  scrolled_window = gtk_scrolled_window_new (NULL, NULL);
  gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (scrolled_window),
                                       GTK_SHADOW_IN);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled_window),
                                  GTK_POLICY_AUTOMATIC,
                                  GTK_POLICY_ALWAYS);
  gtk_box_pack_start (GTK_BOX (vbox), scrolled_window, TRUE, TRUE, 0);
  gtk_widget_show (scrolled_window);

  vipp->tv = gtk_tree_view_new ();

  renderer = gtk_cell_renderer_text_new ();
  gtk_cell_renderer_text_set_fixed_height_from_font
    (GTK_CELL_RENDERER_TEXT (renderer), 1);

  gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (vipp->tv),
                                               -1, _("Nr"),
                                               renderer,
                                               "text", 1,
                                               NULL);

  gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (vipp->tv),
                                               -1, _("videofile"),
                                               renderer,
                                               "text", 2,
                                               NULL);

  gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (vipp->tv),
                                               -1, _("Status"),
                                               renderer,
                                               "text", 3,
                                               NULL);
  gtk_tree_view_set_headers_visible (GTK_TREE_VIEW (vipp->tv), TRUE);

  gtk_widget_set_size_request (vipp->tv, 840 /*WIDTH*/, 250 /*HEIGHT*/);
  gtk_container_add (GTK_CONTAINER (scrolled_window), vipp->tv);
  gtk_widget_show (vipp->tv);


  vipp->sel = gtk_tree_view_get_selection (GTK_TREE_VIEW (vipp->tv));
//   g_signal_connect (vipp->sel, "changed",
//                     G_CALLBACK (p_procedure_select_callback), vipp);


}  /* end p_create_video_list_widget */


/* -----------------------------
 * p_create_progress_window
 * -----------------------------
 */
static void
p_create_progress_window(GapVideoIndexCreatorProgressParams *vipp)
{
  GtkWidget *dialog;
  GtkWidget  *vbox;
  GtkWidget  *hbox;

  /* Init UI  */
  gimp_ui_init ("vindex_progress", FALSE);
  gap_stock_init();


  /*  The Storyboard dialog  */
  vipp->progress_bar_master = NULL;
  vipp->progress_bar_sub = NULL;



  /*  The dialog and main vbox  */
  /* the help_id is passed as NULL to avoid creation of the HELP button
   * (the Help Button would be the only button in the action area and results
   *  in creating an extra row
   *  additional note: the Storyboard dialog provides
   *  Help via Menu-Item
   */
  dialog = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_window_set_title (GTK_WINDOW (dialog), _("Video Index Creation Progress"));
  gtk_window_set_role (GTK_WINDOW (dialog), "vindex-creator-progress");

  g_object_set_data (G_OBJECT (dialog), "vipp"
                         , (gpointer)vipp);

  g_signal_connect (dialog, "destroy",
                    G_CALLBACK (p_vipp_dlg_destroy),
                    vipp);

  gimp_help_connect (dialog, gimp_standard_help_func, GAP_VINDEX_PROGRESS_PLUG_IN_PROC, NULL);

  /* the vbox */
  vbox = gtk_vbox_new (FALSE, 0);
  gtk_container_add (GTK_CONTAINER (dialog), vbox);
  gtk_widget_show (vbox);


  vipp->shell_window = dialog;

  p_create_video_list_widget(vipp, vbox);

  /* the hbox */
  hbox = gtk_hbox_new (FALSE, 2);
  gtk_container_set_border_width (GTK_CONTAINER (hbox), 2);
  gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 0);
  gtk_widget_show (hbox);

  /*  The Video ProgressBar */
  {
    GtkWidget *button;
    GtkWidget *progress_bar;
    GtkWidget *vbox_progress;

    vbox_progress = gtk_vbox_new (FALSE, 0);
    gtk_widget_show (vbox_progress);

    progress_bar = gtk_progress_bar_new ();
    vipp->progress_bar_master = progress_bar;
    gtk_progress_bar_set_text(GTK_PROGRESS_BAR(progress_bar), " ");
    gtk_widget_show (progress_bar);
    gtk_box_pack_start (GTK_BOX (vbox_progress), progress_bar, TRUE, TRUE, 0);

    progress_bar = gtk_progress_bar_new ();
    vipp->progress_bar_sub = progress_bar;
    gtk_progress_bar_set_text(GTK_PROGRESS_BAR(progress_bar), " ");
    gtk_widget_show (progress_bar);
    gtk_box_pack_start (GTK_BOX (vbox_progress), progress_bar, TRUE, TRUE, 0);


    gtk_box_pack_start (GTK_BOX (hbox), vbox_progress, TRUE, TRUE, 0);



    button = gtk_button_new_from_stock(GTK_STOCK_CANCEL);
    gtk_box_pack_start (GTK_BOX (hbox), button, FALSE, FALSE, 0);
    gtk_widget_show (button);
    gimp_help_set_help_data (button
                   , _("Cancel video access if in progress and disable automatic video thumbnails")
                   , NULL);
    g_signal_connect (G_OBJECT (button), "clicked",
                    G_CALLBACK (p_cancel_button_cb),
                    vipp);
  }



  vipp->timertag = (gint32) g_timeout_add(200  /* millisecs*/
                                     , (GtkFunction)on_timer_start, vipp);


  gtk_widget_show (dialog);


  gtk_main ();
  gdk_flush ();

}  /* end p_create_progress_window */


/* -----------------------------
 * on_timer_start
 * -----------------------------
 * This procedure triggers processing start
 * in interactive run modes.
 */
static void
on_timer_start(GapVideoIndexCreatorProgressParams *vipp)
{
  if(gap_debug)
  {
    printf("on_timer_start\n");
  }

  
  if(vipp)
  {
    if(vipp->timertag >= 0)
    {
      g_source_remove(vipp->timertag);
      vipp->timertag = -1;
    }
    vipp->processing_finished = FALSE;
    if (vipp->cancel_immedeiate_request == TRUE)
    {
      if(gap_debug)
      {
        printf("pending CANCEL_IMMEDEIATE_REQUEST\n");
      }
    }
    else
    {
      g_get_current_time(&vipp->startTime);
      p_do_processing(vipp);
      g_get_current_time(&vipp->endTime);
    }
    if(vipp->progress_bar_master)
    {
       if(vipp->cancel_immedeiate_request != TRUE)
       {
         gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(vipp->progress_bar_master), 1.0);
       }
    }
    if(vipp->progress_bar_sub)
    {
       if(vipp->cancel_immedeiate_request == TRUE)
       {
         gtk_progress_bar_set_text(GTK_PROGRESS_BAR(vipp->progress_bar_sub), _("processing cancelled"));
       }
       else
       {
         gtk_progress_bar_set_text(GTK_PROGRESS_BAR(vipp->progress_bar_sub), _("processing finished"));
         gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(vipp->progress_bar_sub), 1.0);
       }
    }
    vipp->processing_finished = TRUE;
    
    if(vipp->vref_list != NULL)
    {
      p_print_vref_list(vipp, vipp->vref_list);
    }
    
  }
}  /* end on_timer_start */

/* --------------------------------
 * p_vid_progress_callback
 * --------------------------------
 * return: TRUE: cancel videoapi immediate
 *         FALSE: continue
 */
static gboolean
p_vid_progress_callback(gdouble progress
                       ,gpointer user_data
                       )
{
  GapVideoIndexCreatorProgressParams *vipp;
  gboolean critical_timecode_found;
  gdouble currentPercentageLimit;
  
  

  vipp = (GapVideoIndexCreatorProgressParams *)user_data;
  if(vipp == NULL) { return (TRUE); }
  
  critical_timecode_found = FALSE;
  
  if (vipp->gvahand != NULL)
  {
    critical_timecode_found = vipp->gvahand->critical_timecodesteps_found;
  }
  
  if(vipp->progress_bar_sub != NULL)
  {
    char *message;
    
    if(gap_debug)
    {
      printf("p_vid_progress_callback progress:%f\n", progress);
    }
    
    gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(vipp->progress_bar_sub), CLAMP(progress, 0.0, 1.0));
    switch (vipp->val_ptr->mode)
    {
      case QICK_MODE:
        message = g_strdup_printf(_("Quick check %0.3f %%"), progress * 100.0);
        break;
      case SMART_MODE:
        if (critical_timecode_found == TRUE)
        {
          currentPercentageLimit = 100.0;
        }
        else
        {
          currentPercentageLimit = vipp->val_ptr->percentage_smart_mode;
        }
        message = g_strdup_printf(_("Smart check %0.3f %% (of %0.3f %%)")
                                , progress * 100.0
                                , currentPercentageLimit
                                );
        break;
      case FULLSCAN_MODE:
        if (vipp->gvahand == NULL)
        {
          message = g_strdup_printf(_("Creating video index %0.3f %%")
                                     , progress * 100.0
                                     );
        }
        else
        {
          message = g_strdup_printf(_("Creating video index %0.3f %% (%d)")
                                   , progress * 100.0
                                   , (int)vipp->gvahand->frame_counter
                                   );
        }
        break;
      default:
        message = g_strdup_printf("%0.3f %%", progress * 100.0);
        break;  
       
    }
    gtk_progress_bar_set_text(GTK_PROGRESS_BAR(vipp->progress_bar_sub), message);
    g_free(message);
  }
  
  if ((vipp->val_ptr->mode == SMART_MODE)
  && (vipp->cancel_enabled_smart == TRUE))
  {
    if ((progress * 100.0 > vipp->val_ptr->percentage_smart_mode)
    && (critical_timecode_found == FALSE))
    {
      if(gap_debug)
      {
        printf("SMART_MODE cancel at progress: %.3f, (limit: %.3f)\n"
          ,(float)progress
          ,(float)vipp->val_ptr->percentage_smart_mode
          );
      }
      vipp->cancel_video_api = TRUE;
      vipp->breakPercentage = progress * 100.0;
      vipp->breakFrames = vipp->gvahand->frame_counter;
    }
  }


  /* g_main_context_iteration makes sure that
   *  gtk does refresh widgets,  and react on events while the videoapi
   *  is busy with searching for the next frame.
   */
  while(g_main_context_iteration(NULL, FALSE));

  return(vipp->cancel_video_api || vipp->cancel_immedeiate_request);

  /* return (TRUE); */ /* cancel video api if playback was stopped */

}  /* end p_vid_progress_callback */

