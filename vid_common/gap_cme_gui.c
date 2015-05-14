/* gap_cme_gui.c
 * 2001.04.08 hof (Wolfgang Hofer)
 *
 * GAP ... Gimp Animation Plugins
 *
 * This Module contains GUI Procedures for common Video Encoder Dialog
 * it calls the galde GTK GUI modules and aditional GUI related stuff
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
 * version 2.2.0;   2005.10.08   hof: ported from pthread to gthread
 * version 2.1.0a;  2004.11.10   hof: reorganized dialog creation procedures
 *                                    and replaced the deprecated GtkOptionMenu widget by gimp_int_combo_box
 * version 2.1.0a;  2004.10.26   hof: added input_mode radiobuttons
 * version 2.1.0a;  2004.05.06   hof: integration into gimp-gap project
 * version 1.2.2b;  2003.03.08   hof: thread for storyboard_file processing
 * version 1.2.2b;  2002.11.23   hof: added filtermacro_file, storyboard_file
 * version 1.2.1a;  2001.06.30   hof: created
 */


/* if not defined GAP_GUI_USE_GTHREAD (from config.h) the gui can run without the gthread library
 * (but the main window refresh will not be done while the encoder
 * parameter dialog -- that is called via pdb -- is open)
 *
 * if not defined GAP_ENC_USE_GTHREAD (from config.h) the encoder call is NOT doen as thread
 * (tyou will not have progress feedback in the GUI in this case)
 *
 * Typically both GAP_GUI_USE_GTHREAD and GAP_ENC_USE_GTHREAD shall be enabled in Linux environment.
 * In Windows 64 bit environment
 *    you may  --disable-gui-thread-support (undef GAP_GUI_USE_GTHREAD )
 *    but      --enable-enc-thread-support  (define GAP_ENC_USE_GTHREAD )
 * as workaround for a (probably GTK related) bug that leads to crash on atttempt to move the dialog window.
 */

#include <config.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>

#include <glib/gstdio.h>
#include <gtk/gtk.h>

#include "gap-intl.h"

#include "gap_libgapbase.h"
#include "gap_cme_main.h"
#include "gap_cme_gui.h"
#include "gap_cme_callbacks.h"

#include "gap_gve_sox.h"
#include "gap_gve_story.h"
#include "gap_arr_dialog.h"
#include "gap_audio_wav.h"
#include "gap_stock.h"

#define RADIO_ITEM_INDEX_KEY "gap_radio_item_index_key"

typedef struct t_global_stb
{
  gdouble framerate;
  gdouble aud_total_sec;
  gchar  composite_audio[1000];
  gchar *errtext;
  gchar *errline;
  gchar *warntext;
  gchar *warnline;
  gint32 errline_nr;
  gint32 warnline_nr;
  gboolean vidhand_open_ok;
  gint32 first_frame_limit;
  gint32 last_frame_nr;
  gint32 master_width;
  gint32 master_height;

  gdouble progress;
  gchar   status_msg[65];

  gint32 total_stroyboard_frames;

  gint32 poll_timertag;
  gboolean stb_job_done;
  GapGveStoryVidHandle *vidhand;
} t_global_stb;

static t_global_stb  global_stb;

static void            p_gap_message(const char *msg);
static gint            p_overwrite_dialog(GapCmeGlobalParams *gpp, gchar *filename, gint overwrite_mode);
static GapGveEncList*  pdb_find_video_encoders(void);
static void            p_replace_combo_encodername(GapCmeGlobalParams *gpp);
static void            p_get_range_from_type (GapCmeGlobalParams *gpp
                           , GapLibTypeInputRange range_type
                           , gint32 *lower
                           , gint32 *upper
                           );
static void            p_get_range_and_type (GapCmeGlobalParams *gpp
                           , gint32 *lower
                           , gint32 *upper
                           , GapLibTypeInputRange *range_type);
static void            p_print_storyboard_text_label(GapCmeGlobalParams *gpp, char *msg);
static void            p_print_time_label( GtkLabel *lbl, gint32   tmsec);
static gint32          p_update_aud_info (GapCmeGlobalParams *gpp
                           , GtkLabel *lbl_info
                           , GtkLabel *lbl_time
                           , GtkLabel *lbl_time0
                           , char *audioname);
static void            p_range_widgets_set_limits(GapCmeGlobalParams *gpp
                           , gint32 lower_limit
                           , gint32 upper_limit
                           , GapLibTypeInputRange range_type);
static void            p_init_shell_window_widgets (GapCmeGlobalParams *gpp);
static void            p_status_progress(GapCmeGlobalParams *gpp, t_global_stb *gstb);
static void            p_storybord_job_finished(GapCmeGlobalParams *gpp, t_global_stb *gstb);
static void            on_timer_poll(gpointer   user_data);
static gpointer        p_thread_storyboard_file(gpointer data);


/* procedures to create the dialog windows */
static GtkWidget*      create_ow__dialog (GapCmeGlobalParams *gpp);
static void            p_input_mode_radio_callback(GtkWidget *widget, GapCmeGlobalParams *gpp);
static void            p_create_input_mode_widgets(GtkWidget *table, int row, int col, GapCmeGlobalParams *gpp);

static GtkWidget*      p_create_encoder_status_frame (GapCmeGlobalParams *gpp);
static GtkWidget*      p_create_encode_extras_frame (GapCmeGlobalParams *gpp);
static GtkWidget*      p_create_audiotool_cfg_frame (GapCmeGlobalParams *gpp);
static GtkWidget*      p_create_audio_options_frame (GapCmeGlobalParams *gpp);
static GtkWidget*      p_create_video_timing_table (GapCmeGlobalParams *gpp);
static GtkWidget*      p_create_video_options_frame (GapCmeGlobalParams *gpp);
static GtkWidget*      p_create_shell_window (GapCmeGlobalParams *gpp);

static gint            p_overwrite_dialog(GapCmeGlobalParams *gpp, gchar *filename, gint overwrite_mode);
static gint            p_call_encoder_procedure(GapCmeGlobalParams *gpp);

/* ----------------------------------------
 * p_gap_message
 * ----------------------------------------
 */
static void
p_gap_message(const char *msg)
{
  static GapArrButtonArg  l_argv[1];
  int               l_argc;

  l_argv[0].but_txt  = GTK_STOCK_OK;
  l_argv[0].but_val  = 0;
  l_argc             = 1;

  if(msg)
  {
    if(*msg)
    {
       if(gap_debug)
       {
         gap_file_printf("%s\n", msg);
       }
       gap_arr_buttons_dialog  (_("GAP Message"), msg, l_argc, l_argv, -1);
    }
  }
}  /* end p_gap_message */

/* ----------------------------------------
 * gap_cme_gui_check_gui_thread_is_active
 * ----------------------------------------
 */
gboolean
gap_cme_gui_check_gui_thread_is_active(GapCmeGlobalParams *gpp)
{
   static gboolean l_gap_message_open = FALSE;

   if(gpp->val.gui_proc_thread)
   {
     /* only one of the threads (Master or GUI thread) can use the PDB Interface (or call gimp_xxx procedures)
      * If 2 threads are talking to the gimp main app parallel it comes to crash.
      */
     if(gap_debug)
     {
       gap_file_printf("MASTER: GUI gui_proc_thread %ld is already active\n", (long)gpp->val.gui_proc_thread);
     }
     if(l_gap_message_open == FALSE)
     {
       l_gap_message_open = TRUE;
       p_gap_message(_("Encoder specific Parameter Window still open"));
       l_gap_message_open = FALSE;
     }
     return (TRUE);
   }
   return (FALSE);
}  /* end gap_cme_gui_check_gui_thread_is_active */


/* ----------------------------------------
 * gap_cme_gui_pdb_call_encoder_gui_plugin
 * ----------------------------------------
 */
gint
gap_cme_gui_pdb_call_encoder_gui_plugin(GapCmeGlobalParams *gpp)
{
  gboolean joinable;
  if(gpp->ecp == NULL)
  {
    return -1;
  }

#ifdef GAP_GUI_USE_GTHREAD
  if(gap_cme_gui_check_gui_thread_is_active(gpp)) return -1;

  /* start a thread for asynchron PDB call of the gui_ procedure
   */
  if(gap_debug)
  {
    gap_file_printf("MASTER: Before g_thread_create\n");
  }

  joinable = TRUE;
  gpp->val.gui_proc_thread =
      g_thread_create((GThreadFunc)gap_cme_gui_thread_async_pdb_call
                     , NULL  /* data */
                     , joinable
                     , NULL  /* GError **error (NULL dont report errors) */
                     );

  if(gap_debug)
  {
    gap_file_printf("MASTER: After g_thread_create\n");
  }
#else
  if(gap_debug)
  {
    gap_file_printf("MASTER: gap_cme_gui_pdb_call_encoder_gui_plugin (without threads)\n");
  }
  /* if threads are not used simply call the procedure
   * (the common GUI window is not refreshed until the called gui_proc ends)
   */
  gap_cme_gui_thread_async_pdb_call(NULL);

#endif

  return 0;
}  /* end gap_cme_gui_pdb_call_encoder_gui_plugin */


/* ----------------------------------------
 * gap_cme_gui_thread_async_pdb_call
 * ----------------------------------------
 */
gpointer
gap_cme_gui_thread_async_pdb_call(gpointer data)
{
   GapCmeGlobalParams *gpp;
   GimpParam       *l_ret_params;
   GimpParam       *l_argv;
   gint             l_retvals;
   gint             l_idx;
   gint             l_nparams;
   gint             l_nreturn_vals;
   GimpPDBProcType  l_proc_type;
   gchar           *l_proc_blurb;
   gchar           *l_proc_help;
   gchar           *l_proc_author;
   gchar           *l_proc_copyright;
   gchar           *l_proc_date;
   GimpParamDef    *l_params;
   GimpParamDef    *l_return_vals;
   gchar           *plugin_name;


  gpp = gap_cme_main_get_global_params();

#ifdef GAP_GUI_USE_GTHREAD
  if(gap_debug) 
  {
    gap_file_printf("THREAD: gap_cme_gui_thread_async_pdb_call &gpp: %ld\n", (long)gpp);
  }
#else
  if(gap_debug) 
  {
    gap_file_printf("non-thread call gap_cme_gui_thread_async_pdb_call &gpp: %ld\n", (long)gpp);
  }
#endif

  plugin_name = gpp->val.ecp_sel.gui_proc;

  /* query for plugin_name to get its argument types */
  if (!gimp_procedural_db_proc_info (plugin_name,
                                     &l_proc_blurb,
                                     &l_proc_help,
                                     &l_proc_author,
                                     &l_proc_copyright,
                                     &l_proc_date,
                                     &l_proc_type,
                                     &l_nparams,
                                     &l_nreturn_vals,
                                     &l_params,
                                     &l_return_vals))
  {
    gap_file_printf("ERROR: Plugin not available, Name was %s\n", plugin_name);

    if(gap_debug)
    {
      gap_file_printf("THREAD gui_proc_thread err TERMINATING: %ld\n", (long)gpp->val.gui_proc_thread);
    }

    gpp->val.gui_proc_thread = NULL;
    return (NULL);
  }

  /* construct the procedures arguments */
  l_argv = g_new (GimpParam, l_nparams);
  memset (l_argv, 0, (sizeof (GimpParam) * l_nparams));

  /* initialize the argument types */
  for (l_idx = 0; l_idx < l_nparams; l_idx++)
  {
    l_argv[l_idx].type = l_params[l_idx].type;
    switch(l_params[l_idx].type)
    {
      case GIMP_PDB_DISPLAY:
        l_argv[l_idx].data.d_display  = -1;
        break;
      case GIMP_PDB_DRAWABLE:
      case GIMP_PDB_LAYER:
      case GIMP_PDB_CHANNEL:
        l_argv[l_idx].data.d_drawable  = -1;
        break;
      case GIMP_PDB_IMAGE:
        l_argv[l_idx].data.d_image  = -1;
        break;
      case GIMP_PDB_INT32:
      case GIMP_PDB_INT16:
      case GIMP_PDB_INT8:
        l_argv[l_idx].data.d_int32  = 0;
        break;
      case GIMP_PDB_FLOAT:
        l_argv[l_idx].data.d_float  = 0.0;
        break;
      case GIMP_PDB_STRING:
        l_argv[l_idx].data.d_string  =  g_strdup("\0");
        break;
      default:
        l_argv[l_idx].data.d_int32  = 0;
        break;

    }
  }


  /* init the standard parameters, that should be common to all plugins */
  l_argv[0].data.d_int32     =  GIMP_RUN_INTERACTIVE;  /* run_mode */
  l_argv[1].data.d_string    =  g_strdup_printf("GAP_KEY_VIDENC_STDPAR_%d", (int)gpp->val.image_ID);

  gimp_set_data(l_argv[1].data.d_string, &gpp->val, sizeof(GapGveCommonValues));


  if(gap_debug)
  {
      gap_file_printf("THREAD Common GUI key: %s\n", l_argv[1].data.d_string);
      gap_file_printf("THREAD Common GUI rate: %f  w:%d h:%d\n", (float)gpp->val.framerate, (int)gpp->val.vid_width, (int)gpp->val.vid_height);
  }


  /* run the plug-in procedure */
  l_ret_params = gimp_run_procedure2 (plugin_name, &l_retvals, l_nparams, l_argv);

  /*  free up arguments and values  */
  gimp_destroy_params (l_argv, l_nparams);


  /*  free the query information  */
  g_free (l_proc_blurb);
  g_free (l_proc_help);
  g_free (l_proc_author);
  g_free (l_proc_copyright);
  g_free (l_proc_date);
  g_free (l_params);
  g_free (l_return_vals);



  if (l_ret_params[0].data.d_status != GIMP_PDB_SUCCESS)
  {
    gap_file_printf("THREAD ERROR: p_call_plugin %s failed.\n", plugin_name);
  }
  else
  {
    if(gap_debug)
    {
      gap_file_printf("THREAD DEBUG: p_call_plugin: %s successful.\n", plugin_name);
    }
  }

  /* the GUI of the encoder plugin might have changed the current video filename extension
   * (therefore we repeat the query for extension here)
   */
  gap_cme_gui_requery_vid_extension(gpp);

  if(gap_debug)
  {
    gap_file_printf("THREAD gui_proc_thread TERMINATING: %ld\n", (long)gpp->val.gui_proc_thread);
  }

  gpp->val.gui_proc_thread = NULL;

  return (NULL);
}  /* end gap_cme_gui_thread_async_pdb_call */



/* ----------------------------------------
 * pdb_find_video_encoders
 * ----------------------------------------
 */
static GapGveEncList*
pdb_find_video_encoders(void)
{
  GapGveEncList *l_ecp;
  GapGveEncList *list_ecp;
  char **proc_list;
  int num_procs;
  int i;
  int j;
  gint             l_nparams;
  gint             l_nreturn_vals;
  GimpPDBProcType   l_proc_type;
  gchar            *l_proc_blurb;
  gchar            *l_proc_help;
  gchar            *l_proc_author;
  gchar            *l_proc_copyright;
  gchar            *l_proc_date;
  GimpParamDef    *l_paramdef;
  GimpParamDef    *l_return_vals;


  if(gap_debug)
  {
    gap_file_printf("pdb_find_video_encoders: START\n");
  }

  list_ecp = NULL;
  l_ecp = NULL;

  gimp_procedural_db_query (GAP_WILDCARD_VIDEO_ENCODERS, ".*", ".*", ".*", ".*", ".*", ".*",
                           &num_procs, &proc_list);
  if(gap_debug)
  {
    gap_file_printf("pdb_find_video_encoders: num_procs:%d (matching the wildcard:%s)\n"
      ,(int)num_procs
      , GAP_WILDCARD_VIDEO_ENCODERS
      );
  }

  for (j = 0; j < num_procs; j++)
  {
     gboolean l_has_proc_info;

     i = (num_procs -1) - j;

     l_has_proc_info = gimp_procedural_db_proc_info (proc_list[i],
                             &l_proc_blurb,
                             &l_proc_help,
                             &l_proc_author,
                             &l_proc_copyright,
                             &l_proc_date,
                             &l_proc_type,
                             &l_nparams,
                             &l_nreturn_vals,
                             &l_paramdef,
                             &l_return_vals);

     if(gap_debug)
     {
       gap_file_printf("pdb_find_video_encoders: check proc:%s  has_proc_info:%d\n"
         , proc_list[i]
         , (int)l_has_proc_info
         );
     }

     if(l_has_proc_info == TRUE)
     {
        char *ecp_infoproc;

        if (l_nparams != GAP_VENC_NUM_STANDARD_PARAM)
        {
           if(gap_debug)
           {
             gap_file_printf("pdb_find_video_encoders: procedure %s is skipped (nparams %d != %d)\n"
                               , proc_list[i], (int)l_nparams, (int)GAP_VENC_NUM_STANDARD_PARAM );
           }
           continue;
        }

        l_ecp = g_malloc(sizeof(GapGveEncList));

        /* pepare default values for menu_name short_description and video_extensions */
        g_snprintf(l_ecp->menu_name, sizeof(l_ecp->menu_name), "%s", proc_list[i]);
        g_snprintf(l_ecp->video_extension, sizeof(l_ecp->video_extension), ".%d", (int)i);
        g_snprintf(l_ecp->short_description, sizeof(l_ecp->short_description), "%s", _("no description available"));

        /* get Menuname and video_extension via ecp_infoproc */
        ecp_infoproc = g_strdup_printf("%s%s", GAP_QUERY_PREFIX_VIDEO_ENCODERS, proc_list[i]);

        if(TRUE == gimp_procedural_db_proc_info (ecp_infoproc,
                             &l_proc_blurb,
                             &l_proc_help,
                             &l_proc_author,
                             &l_proc_copyright,
                             &l_proc_date,
                             &l_proc_type,
                             &l_nparams,
                             &l_nreturn_vals,
                             &l_paramdef,
                             &l_return_vals))
        {
           if(gap_debug)
           {
             gap_file_printf("pdb_find_video_encoders: run procedure %s nparams:%d nretrun_vals:%d\n"
                     , ecp_infoproc, (int)l_nparams, (int)l_nreturn_vals);
           }

           if(l_nreturn_vals >= 1)
           {
              GimpParam* l_params;
              gint       l_retvals;

              /* query for menu_name */
              l_params = gimp_run_procedure (ecp_infoproc,
                          &l_retvals,
                          GIMP_PDB_STRING, GAP_VENC_PAR_MENU_NAME,
                          GIMP_PDB_END);

              if((l_params[0].data.d_status == GIMP_PDB_SUCCESS) && (l_retvals >= 2))
              {
                if((l_params[1].data.d_string != NULL) && (l_params[1].type == GIMP_PDB_STRING))
                {
                  if(gap_debug)
                  {
                    gap_file_printf("[1].d_string %s\n", l_params[1].data.d_string);
                  }
                  g_snprintf(l_ecp->menu_name, sizeof(l_ecp->menu_name), "%s", l_params[1].data.d_string);
                }
              }
              g_free(l_params);

              /* query for video_extension */
              l_params = gimp_run_procedure (ecp_infoproc,
                          &l_retvals,
                          GIMP_PDB_STRING, GAP_VENC_PAR_VID_EXTENSION,
                          GIMP_PDB_END);

              if((l_params[0].data.d_status == GIMP_PDB_SUCCESS) && (l_retvals >= 2))
              {
                if((l_params[1].data.d_string) && (l_params[1].type == GIMP_PDB_STRING))
                {
                  if(gap_debug)
                  {
                    gap_file_printf("[1].d_string %s\n", l_params[1].data.d_string);
                  }
                  g_snprintf(l_ecp->video_extension, sizeof(l_ecp->video_extension), "%s",  l_params[1].data.d_string);
                }
              }
              g_free(l_params);

              /* query for short_description */
              l_params = gimp_run_procedure (ecp_infoproc,
                          &l_retvals,
                          GIMP_PDB_STRING, GAP_VENC_PAR_SHORT_DESCRIPTION,
                          GIMP_PDB_END);

              if((l_params[0].data.d_status == GIMP_PDB_SUCCESS) && (l_retvals >= 2))
              {
                if((l_params[1].data.d_string) && (l_params[1].type == GIMP_PDB_STRING))
                {
                  if(gap_debug)
                  {
                    gap_file_printf("[1].d_string %s\n", l_params[1].data.d_string);
                  }
                  g_snprintf(l_ecp->short_description, sizeof(l_ecp->short_description), "%s",  l_params[1].data.d_string);
                }
              }
              g_free(l_params);


              /* query for (optional) gui_proc (to set encoder soecific params) */
              l_params = gimp_run_procedure (ecp_infoproc,
                          &l_retvals,
                          GIMP_PDB_STRING, GAP_VENC_PAR_GUI_PROC,
                          GIMP_PDB_END);

              if((l_params[0].data.d_status == GIMP_PDB_SUCCESS) && (l_retvals >= 2))
              {
                if((l_params[1].data.d_string) && (l_params[1].type == GIMP_PDB_STRING))
                {
                  if(gap_debug)
                  {
                    gap_file_printf("[1].d_string %s\n", l_params[1].data.d_string);
                  }
                  g_snprintf(l_ecp->gui_proc, sizeof(l_ecp->gui_proc), "%s",  l_params[1].data.d_string);
                }
              }
              g_free(l_params);
           }
        }

        g_snprintf(l_ecp->vid_enc_plugin, sizeof(l_ecp->vid_enc_plugin), "%s", proc_list[i]);
        l_ecp->menu_nr = i+1;
        l_ecp->next = list_ecp;

        list_ecp = l_ecp;
     }
  }
  if(list_ecp == NULL)
  {
    list_ecp = g_malloc(sizeof(GapGveEncList));
    list_ecp->menu_nr = 0;
    list_ecp->vid_enc_plugin[0] = '\0';
    g_snprintf(list_ecp->menu_name, sizeof(list_ecp->menu_name), "none");
    g_snprintf(list_ecp->video_extension, sizeof(l_ecp->video_extension), ".0");
    list_ecp->next = NULL;
  }

  return(list_ecp);
}  /* end pdb_find_video_encoders */


/* ----------------------------------------
 * gap_cme_gui_requery_vid_extension
 * ----------------------------------------
 * some encoders do support more than 1 extension
 * therefore we do repeat the query for extension
 * to get the current one
 */
void
gap_cme_gui_requery_vid_extension(GapCmeGlobalParams *gpp)
{
  GapGveEncList *l_ecp;

  if(gap_debug)
  {
    gap_file_printf("gap_cme_gui_requery_vid_extension START\n");
  }

  for(l_ecp = gpp->ecp; l_ecp != NULL; l_ecp = (GapGveEncList *)l_ecp->next)
  {
    if(strcmp(l_ecp->vid_enc_plugin , gpp->val.ecp_sel.vid_enc_plugin) == 0)
    {
        char      *ecp_infoproc;
        GimpParam *l_params;
        gint       l_retvals;

        /* get video_extension via ecp_infoproc */
        ecp_infoproc = g_strdup_printf("%s%s", GAP_QUERY_PREFIX_VIDEO_ENCODERS, l_ecp->vid_enc_plugin);


        /* (re) query for video_extension */
        l_params = gimp_run_procedure (ecp_infoproc,
                    &l_retvals,
                    GIMP_PDB_STRING, GAP_VENC_PAR_VID_EXTENSION,
                    GIMP_PDB_END);

        if((l_params[0].data.d_status == GIMP_PDB_SUCCESS) && (l_retvals >= 2))
        {
          if((l_params[1].data.d_string) && (l_params[1].type == GIMP_PDB_STRING))
          {
            if(gap_debug)
            {
              gap_file_printf("gap_cme_gui_requery_vid_extension: [1].d_string %s\n"
                     , l_params[1].data.d_string);
            }
            g_snprintf(l_ecp->video_extension, sizeof(l_ecp->video_extension), "%s",  l_params[1].data.d_string);
            g_snprintf(gpp->val.ecp_sel.video_extension, sizeof(gpp->val.ecp_sel.video_extension), "%s",  l_params[1].data.d_string);
          }
        }
        g_free(l_params);
        g_free(ecp_infoproc);
        gap_cme_gui_upd_vid_extension(gpp);

        return;
    }
  }
}  /* end gap_cme_gui_requery_vid_extension */


/* ----------------------------------------
 * p_replace_combo_encodername
 * ----------------------------------------
 * replace encodername combo by dynamic menu we got from the PDB
 */
static void
p_replace_combo_encodername(GapCmeGlobalParams *gpp)
{
  GtkWidget *combo;
  GapGveEncList *l_ecp;
  gint  l_active_menu_nr;
  gint  l_idx;

  if(gap_debug)
  {
    gap_file_printf("p_replace_combo_encodername: START\n");
  }

  combo = gpp->cme__combo_encodername;
  if(combo == NULL)
  {
    return;
  }

  l_idx = 0;
  l_active_menu_nr = 0;
  l_ecp = gpp->ecp;
  while(l_ecp)
  {
     if(gap_debug)
     {
        gap_file_printf("p_replace_combo_encodername: %d, %s %s\n"
              , (int)l_ecp->menu_nr
              , l_ecp->menu_name
              , l_ecp->vid_enc_plugin);
     }


     gimp_int_combo_box_append (GIMP_INT_COMBO_BOX (combo),
                                GIMP_INT_STORE_VALUE, l_ecp->menu_nr,
                                GIMP_INT_STORE_LABEL, l_ecp->menu_name,
                                -1);

#ifdef ENABLE_GVA_LIBAVFORMAT
     /* set FFMPEG as default encoder (if this encoder is installed) */
     if(strcmp(l_ecp->vid_enc_plugin, GAP_PLUGIN_NAME_FFMPEG_ENCODE) == 0)
     {
         l_active_menu_nr = l_ecp->menu_nr;
     }
#else
     /* set AVI as default encoder (if this encoder is installed) */
     if(strcmp(l_ecp->vid_enc_plugin, GAP_PLUGIN_NAME_AVI_ENCODE) == 0)
     {
         l_active_menu_nr = l_ecp->menu_nr;
     }
#endif
     l_ecp =  (GapGveEncList *)l_ecp->next;
     l_idx++;
  }

  gimp_int_combo_box_connect (GIMP_INT_COMBO_BOX (combo)
                             , l_active_menu_nr,
                              G_CALLBACK (on_cme__combo_enocder),
                              gpp);
}  /* end p_replace_combo_encodername */


/* ----------------------------------------
 * p_get_range_from_type
 * ----------------------------------------
 */
static void
p_get_range_from_type (GapCmeGlobalParams *gpp
                           , GapLibTypeInputRange range_type
                           , gint32 *lower
                           , gint32 *upper
                           )
{
  switch(range_type)
  {
    case GAP_RNGTYPE_STORYBOARD:
      {
        t_global_stb  *gstb;

        *lower = 0;

        gstb = &global_stb;
        *upper = gstb->total_stroyboard_frames;
        if(*upper > 0)
        {
          *lower = 1;
        }
      }
      break;
    case GAP_RNGTYPE_LAYER:
      {
        gint          l_nlayers;
        gint32       *l_layers_list;


        l_layers_list = gimp_image_get_layers(gpp->val.image_ID, &l_nlayers);
        g_free(l_layers_list);
        *lower = 0;
        *upper = l_nlayers -1;
      }
      break;
    default:
      *lower = gpp->ainfo.first_frame_nr;
      *upper = gpp->ainfo.last_frame_nr;
      break;
  }
}  /* end p_get_range_from_type */


/* ----------------------------------------
 * p_get_range_and_type
 * ----------------------------------------
 */
static void
p_get_range_and_type (GapCmeGlobalParams *gpp, gint32 *lower, gint32 *upper, GapLibTypeInputRange *range_type)

{
  gint32 l_frame_cnt;

 /* Range limits for widgets "cme__spinbutton_from" and "cme__spinbutton_to"
  * If there is just one frame, we operate on layers
  */

  l_frame_cnt = abs(gpp->ainfo.last_frame_nr - gpp->ainfo.first_frame_nr);
  if(l_frame_cnt > 1)
  {
    *range_type = GAP_RNGTYPE_FRAMES;
  }
  else
  {
    *range_type = GAP_RNGTYPE_LAYER;
  }

  p_get_range_from_type(gpp, *range_type, lower, upper);

}   /* end p_get_range_and_type */


/* ----------------------------------------
 * p_print_storyboard_text_label
 * ----------------------------------------
 */
static void
p_print_storyboard_text_label(GapCmeGlobalParams *gpp, char *msg)
{
  GtkWidget  *lbl;

  lbl = gpp->cme__label_storyboard_helptext;
  if(lbl==NULL) { return; }


  if(msg)
  {
     gtk_label_set_text(GTK_LABEL(lbl), msg);
  }
  else
  {
     gtk_label_set_text(GTK_LABEL(lbl),
      _("Storyboardfiles are textfiles that are used to\n"
        "assemble a video from a list of single images,\n"
        "frameranges, videoclips, gif animations or audiofiles.\n"
        "the frames are organized in tracks,\n"
        "and allow fade, scale and move\n"
        "operations between the tracks.\n"
        "(see STORYBOARD_FILE_DOC.txt for details)")
        );
  }
}  /* end p_print_storyboard_text_label */


/* ----------------------------------------
 * p_print_time_label
 * ----------------------------------------
 */
static void
p_print_time_label( GtkLabel *lbl, gint32   tmsec)
{
  gint32   tms;
  gint32   tsec;
  gint32   tmin;
  gchar    txt[20];

  if(lbl == NULL)
  {
    return;
  }

  tms = tmsec % 1000;
  tsec = (tmsec / 1000) % 60;
  tmin = tmsec / 60000;

  g_snprintf(txt, sizeof(txt), "%02d:%02d:%03d"
                               , (int)tmin
                               , (int)tsec
                               , (int)tms
                               );
  gtk_label_set_text(lbl, txt);
}  /* end p_print_time_label */



/* ----------------------------------------
 * p_update_aud_info
 * ----------------------------------------
 */
static gint32
p_update_aud_info (GapCmeGlobalParams *gpp
                  , GtkLabel *lbl_info
                  , GtkLabel *lbl_time
                  , GtkLabel *lbl_time0
                  , char *audioname)
{
  gchar       txt[200];
  long        samplerate;
  long        disp_samplerate;   /* samplerate to display (rate after resampling by sox) */
  long        channels;
  long        bytes_per_sample;
  long        bits;
  long        samples;
  long        all_playlist_references;
  long        valid_playlist_references;
  int         l_rc;
  gint32      tmsec;        /* audioplaytime in milli secs */
  gboolean    l_audioname_empty;

  if(gap_debug)
  {
    gap_file_printf("p_update_aud_info: START lbl_info:%ld lbl_time:%ld lbl_time0:%ld\n"
       ,(long)lbl_info
       ,(long)lbl_time
       ,(long)lbl_time0
       );
  }

  if ((lbl_info == NULL) || (lbl_time == NULL) || (lbl_time0 == NULL))
  {
    return 0;
  }
  
  l_audioname_empty = TRUE;
  if(audioname != NULL)
  {
    if(*audioname != '\0')
    {
      l_audioname_empty = FALSE;
    }
  }
  
  
  if(l_audioname_empty == TRUE)
  {
     if(gap_debug)
     {
       gap_file_printf("p_update_aud_info: audioname is null or empty\n");
     }

     p_print_time_label(lbl_time, 0);
     p_print_time_label(lbl_time0, 0);
     return 0;
  }

  all_playlist_references = 0;
  valid_playlist_references = 0;
  g_snprintf(txt, sizeof(txt), "??:??:???");
  gtk_label_set_text(lbl_time, txt);
  gtk_label_set_text(lbl_time0, txt);

  samplerate = 0;
  disp_samplerate = gpp->val.samplerate;
  g_snprintf(txt, sizeof(txt), " ");

  if(gap_debug)
  {
    gap_file_printf("p_update_aud_info: audioname %s\n", audioname);
  }


  if(g_file_test(audioname, G_FILE_TEST_EXISTS))
  {
     tmsec = 0;

     /* check for WAV file or valid audio playlist, and get audio informations */
     l_rc = gap_audio_playlist_wav_file_check(audioname
                     , &samplerate
                     , &channels
                     , &bytes_per_sample
                     , &bits
                     , &samples
                     , &all_playlist_references
                     , &valid_playlist_references
                     , gpp->val.samplerate          /* desired_samplerate */
                     );

     if(gap_debug)
     {
       gap_file_printf("p_update_aud_info: l_rc:%d all_playlist_references:%d\n"
         ,(int)l_rc
         ,(int)all_playlist_references
         );
     }

     if((l_rc == 0)
     || (all_playlist_references >0))
     {
       if((samplerate > 0) && (channels > 0))
       {
         disp_samplerate = samplerate;
         /* have to calculate in gdouble, because samples * 1000
          * may not fit into gint32 datasize and brings negative results
          */
         tmsec = (gint32)((((gdouble)samples / (gdouble)channels) * 1000.0) / (gdouble)disp_samplerate);
       }

       if(all_playlist_references > 0)
       {
         /* audioname is a audio playlist with references to
          * audiofiles for multiple audio track encding
          * valid_playlist_references holds the number of valid tracks
          * (where samplerate matches the desired samplerate and bits == 16)
          */
         g_snprintf(txt, sizeof(txt), ngettext("List[%d] has [%d] valid track, Bit:%d Chan:%d Rate:%d"
                                             , "List[%d] has [%d] valid tracks, Bit:%d Chan:%d Rate:%d"
                                             , (int)valid_playlist_references )
                                  , (int)all_playlist_references
                                  , (int)valid_playlist_references
                                  , (int)bits
                                  , (int)channels
                                  , (int)samplerate
                                  );
       }
       else
       {
         g_snprintf(txt, sizeof(txt), _("%s, Bit:%d Chan:%d Rate:%d")
                                  , "WAV"
                                  , (int)bits
                                  , (int)channels
                                  , (int)samplerate
                                  );
       }

       p_print_time_label(lbl_time, tmsec);
       p_print_time_label(lbl_time0, tmsec);
     }
     else
     {
       g_snprintf(txt, sizeof(txt), _("UNKNOWN (using sox)"));
     }
  }

  gtk_label_set_text(lbl_info, txt);
  return (samplerate);
}  /* end p_update_aud_info */

/* ----------------------------------------
 * gap_cme_gui_upd_vid_extension
 * ----------------------------------------
 */
void
gap_cme_gui_upd_vid_extension (GapCmeGlobalParams *gpp)
{
  gchar *l_vid;
  gint   l_idx;
  char *videoname;

 l_vid = g_strdup(gpp->val.videoname);
 for(l_idx = strlen(l_vid) -1;  l_idx > 0; l_idx--)
 {
   if(l_vid[l_idx] == '.')
   {
      l_vid[l_idx] = '\0';
      break;
   }
 }
 videoname = g_strdup_printf("%s%s", l_vid, &gpp->val.ecp_sel.video_extension[0]);
 g_snprintf(gpp->val.videoname, sizeof(gpp->val.videoname), "%s", videoname);

 if(gpp->cme__entry_video != NULL)
 {
   gtk_entry_set_text(GTK_ENTRY(gpp->cme__entry_video), videoname);
 }
 
 if(gpp->cme__short_description != NULL)
 {
   gtk_label_set_text(GTK_LABEL(gpp->cme__short_description)
                  , &gpp->val.ecp_sel.short_description[0]);
 }

 g_free(l_vid);
 g_free(videoname);
}  /* end gap_cme_gui_upd_vid_extension */

/* ----------------------------------------
 * gap_cme_gui_upd_wgt_sensitivity
 * ----------------------------------------
 */
void
gap_cme_gui_upd_wgt_sensitivity (GapCmeGlobalParams *gpp)
{
  gboolean   sensitive;

  sensitive = FALSE;
  if(gpp->val.audioname1[0] != '\0')
  {
    if(g_file_test(gpp->val.audioname1, G_FILE_TEST_EXISTS))
    {
      sensitive = TRUE;
    }
  }

  gtk_widget_set_sensitive(gpp->cme__spinbutton_samplerate, sensitive);
  gtk_widget_set_sensitive(gpp->cme__button_gen_tmp_audfile, sensitive);

  sensitive = FALSE;
  if(gpp->val.ecp_sel.gui_proc[0] != '\0')
  {
      sensitive = TRUE;
  }

  gtk_widget_set_sensitive(gpp->cme__button_params, sensitive);

  sensitive = FALSE;
  if(gpp->val.storyboard_file[0] != '\0')
  {
    if(g_file_test(gpp->val.storyboard_file, G_FILE_TEST_EXISTS))
    {
      t_global_stb         *gstb;

      /* check if storyboard file is valid and has audio */
      gstb = &global_stb;
      if((gstb->aud_total_sec > 0.0)
      && (gstb->vidhand_open_ok))
      {
        sensitive = TRUE;
      }
    }
  }
  gtk_widget_set_sensitive(gpp->cme__button_stb_audio, sensitive);

}  /* end gap_cme_gui_upd_wgt_sensitivity */

/* ----------------------------------------
 * gap_cme_gui_update_aud_labels
 * ----------------------------------------
 */
void
gap_cme_gui_update_aud_labels (GapCmeGlobalParams *gpp)
{
  GtkLabel *lbl_tmp_audfile;
  GtkLabel *lbl_info;
  GtkLabel *lbl_info_tmp;
  GtkLabel *lbl_time;
  GtkLabel *lbl_time0;
  GtkLabel *lbl_time_tmp;

  lbl_info  = GTK_LABEL(gpp->cme__label_aud1_info);
  lbl_time  = GTK_LABEL(gpp->cme__label_aud1_time);
  lbl_time0 = GTK_LABEL(gpp->cme__label_aud0_time);
  lbl_time_tmp = GTK_LABEL(gpp->cme__label_aud_tmp_time);
  lbl_info_tmp = GTK_LABEL(gpp->cme__label_aud_tmp_info);
  lbl_tmp_audfile = GTK_LABEL(gpp->cme__label_tmp_audfile);

  if ((lbl_info != NULL)
  &&  (lbl_time != NULL)
  &&  (lbl_time0 != NULL)
  &&  (lbl_time_tmp != NULL)
  &&  (lbl_info_tmp != NULL)
  &&  (lbl_tmp_audfile != NULL))
  {
    gtk_label_set_text(lbl_info, " ");

    gpp->val.wav_samplerate1  =
    p_update_aud_info(gpp, lbl_info, lbl_time, lbl_time0, gpp->val.audioname1);
 

    gtk_label_set_text(lbl_time_tmp, " ");
    gtk_label_set_text(lbl_info_tmp, " ");
    gtk_label_set_text(lbl_tmp_audfile, " ");

    if(gpp->val.tmp_audfile[0] != '\0')
    {
      if(g_file_test(gpp->val.tmp_audfile, G_FILE_TEST_EXISTS))
      {
         gpp->val.wav_samplerate_tmp =
         p_update_aud_info(gpp, lbl_info_tmp, lbl_time_tmp, lbl_time0, gpp->val.tmp_audfile);
         gtk_label_set_text(lbl_tmp_audfile, gpp->val.tmp_audfile);
      }
    }
  }
  gap_cme_gui_upd_wgt_sensitivity (gpp);
}  /* end gap_cme_gui_update_aud_labels */


/* ----------------------------------------
 * gap_cme_gui_update_vid_labels
 * ----------------------------------------
 */
void
gap_cme_gui_update_vid_labels (GapCmeGlobalParams *gpp)
{
  GtkLabel   *lbl;
  gint32      tmsec;        /* audioplaytime in milli secs */
  gint32      first_offset;
  gint32      range_from;
  gint32      range_to;
  gdouble     msec_per_frame;

  if(gpp->val.framerate > 0)
  {
    msec_per_frame = 1000.0 / gpp->val.framerate;
  }
  else
  {
    msec_per_frame = 1000.0;
  }

  first_offset = gpp->ainfo.first_frame_nr;
  range_from = gpp->val.range_from;
  range_to = gpp->val.range_to;

  if(gpp->val.input_mode == GAP_RNGTYPE_STORYBOARD)
  {
    first_offset = MIN(1, MIN(range_from, range_to));
  }
  if(gpp->val.input_mode == GAP_RNGTYPE_LAYER)
  {
    range_from = gpp->val.range_to;
    range_to = gpp->val.range_from;
    first_offset = 0;
  }

  lbl = GTK_LABEL(gpp->cme__label_fromtime);
  tmsec = (range_from - first_offset) * msec_per_frame;
  p_print_time_label(lbl, tmsec);

  lbl = GTK_LABEL(gpp->cme__label_totime);
  tmsec = (range_to - first_offset) * msec_per_frame;
  p_print_time_label(lbl, tmsec);

  lbl = GTK_LABEL(gpp->cme__label_totaltime);
  tmsec = abs(gpp->val.range_to - gpp->val.range_from) * msec_per_frame;
  p_print_time_label(lbl, tmsec);
}  /* end gap_cme_gui_update_vid_labels */


/* ----------------------------------------
 * gap_cme_gui_util_sox_widgets
 * ----------------------------------------
 */
void
gap_cme_gui_util_sox_widgets (GapCmeGlobalParams *gpp)
{
  GtkEntry *entry;

  if(gap_debug)
  {
    gap_file_printf("gap_cme_gui_util_sox_widgets\n");
  }

  entry = GTK_ENTRY(gpp->cme__entry_sox);
  if(entry != NULL)
  {
    gtk_entry_set_text(entry, gpp->val.util_sox);
  }

  entry = GTK_ENTRY(gpp->cme__entry_sox_options);
  if(entry != NULL)
  {
    gtk_entry_set_text(entry, gpp->val.util_sox_options);
  }
}  /* end gap_cme_gui_util_sox_widgets */


/* ----------------------------------------
 * p_range_widgets_set_limits
 * ----------------------------------------
 */
static void
p_range_widgets_set_limits(GapCmeGlobalParams *gpp
                          , gint32 lower_limit
                          , gint32 upper_limit
                          , GapLibTypeInputRange range_type)
{
  GtkAdjustment *adj;
  gchar *lbl_text;
  gchar *range_text;

  if(gap_debug)
  {
    gap_file_printf("(++)p_range_widgets_set_limits lower_limit:%d upper_limit:%d input_mode:%d\n"
       , (int)lower_limit
       , (int)upper_limit
       , (int)range_type
       );
  }

  /* constraint range into lower_limit / upper_limit */
  /*
   * gpp->val.range_from = MAX(lower_limit, gpp->val.range_from);
   * gpp->val.range_from = MIN(upper_limit, gpp->val.range_from);
   * gpp->val.range_to = MAX(lower_limit, gpp->val.range_to);
   * gpp->val.range_to = MIN(upper_limit, gpp->val.range_to);
   */
  if(range_type == GAP_RNGTYPE_LAYER)
  {
    /* invers range for encoding Multilayer images
     * (top layer is usually the last frame but has index 0 in gimp images)
     */
    gpp->val.range_from = upper_limit;
    gpp->val.range_to = lower_limit;
  }
  else
  {
    gpp->val.range_from = lower_limit;
    gpp->val.range_to = upper_limit;
  }

  /* update spinbutton limits and values */
  adj = GTK_ADJUSTMENT(gpp->cme__spinbutton_from_adj);
  adj->lower = (gfloat) lower_limit;
  adj->upper = (gfloat) upper_limit;
  gtk_adjustment_set_value(adj, (gfloat)gpp->val.range_from);

  adj = GTK_ADJUSTMENT(gpp->cme__spinbutton_to_adj);
  adj->lower = (gfloat) lower_limit;
  adj->upper = (gfloat) upper_limit;
  gtk_adjustment_set_value(adj, (gfloat)gpp->val.range_to);

  switch(range_type)
  {
    case GAP_RNGTYPE_STORYBOARD:
      range_text = g_strdup(_("Storyframe"));
      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (gpp->cme__radio_button_storyboard), TRUE);
      break;
    case GAP_RNGTYPE_LAYER:
      range_text = g_strdup(_("Layer"));
      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (gpp->cme__radio_button_layer), TRUE);
      break;
    default:
      range_text = g_strdup(_("Frame"));
      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (gpp->cme__radio_button_frame), TRUE);
      break;
  }

  /* label changes dependent from rangetype "From Frame", "From Layer" or "From Storyframe" */
  if(gpp->cme__label_from != NULL)
  {
    lbl_text = g_strdup_printf(_("From %s:"),  range_text);
    gtk_label_set_text(GTK_LABEL(gpp->cme__label_from), lbl_text);
    g_free(lbl_text);
  }

  if(gpp->cme__label_to != NULL)
  {
    lbl_text = g_strdup_printf(_("To %s:"),  range_text);
    gtk_label_set_text(GTK_LABEL(gpp->cme__label_to), lbl_text);
    g_free(lbl_text);
  }

  g_free(range_text);

  gap_cme_gui_update_vid_labels (gpp);
  gap_cme_gui_update_aud_labels (gpp);
}       /* end p_range_widgets_set_limits */

/* ----------------------------------------
 * p_init_shell_window_widgets
 * ----------------------------------------
 */
static void
p_init_shell_window_widgets (GapCmeGlobalParams *gpp)
{
 if(gap_debug)
 {
   gap_file_printf("p_init_shell_window_widgets: Start INIT\n");
 }

 /* put initial values to the widgets */

 /* widgets "cme__spinbutton_from" and "cme__spinbutton_to" */
 {
   GapLibTypeInputRange l_rangetype;
   gint32  l_first_frame_limit;
   gint32  l_last_frame_nr;

   p_get_range_and_type (gpp, &l_first_frame_limit, &l_last_frame_nr, &l_rangetype);
   gpp->val.input_mode     = l_rangetype;

   p_range_widgets_set_limits(gpp
                             , l_first_frame_limit
                             , l_last_frame_nr
                             , l_rangetype
                             );
 }

 gtk_adjustment_set_value(GTK_ADJUSTMENT(gpp->cme__spinbutton_framerate_adj)
                         , (gfloat)gpp->val.framerate);
 gtk_adjustment_set_value(GTK_ADJUSTMENT(gpp->cme__spinbutton_samplerate_adj)
                         , (gfloat)gpp->val.samplerate);
 gtk_adjustment_set_value(GTK_ADJUSTMENT(gpp->cme__spinbutton_width_adj)
                         , (gfloat)gpp->val.vid_width);
 gtk_adjustment_set_value(GTK_ADJUSTMENT(gpp->cme__spinbutton_height_adj)
                         , (gfloat)gpp->val.vid_height);

 if(gpp->cme__entry_video != NULL)
 {
   gtk_entry_set_text(GTK_ENTRY(gpp->cme__entry_video), gpp->val.videoname);
 }
 if(gpp->cme__entry_audio1 != NULL)
 {
   gtk_entry_set_text(GTK_ENTRY(gpp->cme__entry_audio1), gpp->val.audioname1);
 }
 if (gpp->cme__entry_mac != NULL)
 {
   gtk_entry_set_text(GTK_ENTRY(gpp->cme__entry_mac), gpp->val.filtermacro_file);
 }

 gap_cme_gui_update_aud_labels (gpp);
 gap_cme_gui_update_vid_labels (gpp);

 /* dynamic combo for all encoders that are registered at runtime */
 p_replace_combo_encodername(gpp);
 gap_cme_gui_util_sox_widgets(gpp);
}    /* end p_init_shell_window_widgets */


/* ------------------
 * p_status_progress
 * ------------------
 */
static void
p_status_progress(GapCmeGlobalParams *gpp, t_global_stb *gstb)
{
  GtkWidget  *pbar;
  GtkWidget  *status_lbl;
  /* storyboard thread job still running
   * update status, and restart poll timer
   */

  status_lbl = gpp->cme__label_status;
  if(status_lbl)
  {
    gtk_label_set_text(GTK_LABEL(status_lbl), gstb->status_msg);
  }

  pbar = gpp->cme__progressbar_status;
  if(pbar)
  {
    gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR (pbar), gstb->progress);
    gtk_progress_bar_set_text(GTK_PROGRESS_BAR(pbar), gstb->status_msg);
  }

  if(gap_debug)
  {
    gap_file_printf("progress: %f\n", (float)gstb->progress );
  }
}  /* end p_status_progress */


/* ------------------------
 * p_storybord_job_finished
 * ------------------------
 * This is called from polling_timer
 * when the storyboard thread has finished
 *
 *  print storyboard parse report, update widgets
 *
 *   check if storyboard open was successful,
 *   if successful, set lower and upper limit of the range spinbutton widgets
 *   to 1 and the total number of frames found in the storyboard_file.
 *
 *   if no storyboard file exists then use the first/last frame_nr from the ainfo structure
 *   that describes the animation frames from the invoking image_frame.
 */
static void
p_storybord_job_finished(GapCmeGlobalParams *gpp, t_global_stb *gstb)
{
  char *l_msg;
  char *l_framerate_info;
  char *l_size_info;
  char *l_playtime_info;
  gdouble l_vid_total_sec;



  if(gap_debug)
  {
    gap_file_printf("p_storybord_job_finished: START\n");
  }

  gstb->progress = 1.0;
  g_snprintf(gstb->status_msg, sizeof(gstb->status_msg), _("ready"));
  p_status_progress(gpp, gstb);

#ifdef GAP_GUI_USE_GTHREAD
   /* is the encoder specific gui_thread still open ? */
   if(gpp->val.gui_proc_thread)
   {
      /* wait until thread exits */
      if(gap_debug)
      {
        gap_file_printf("p_storybord_job_finished: before g_thread_join\n");
      }

      g_thread_join(gpp->val.gui_proc_thread);

      if(gap_debug)
      {
        gap_file_printf("p_storybord_job_finished: after g_thread_join\n");
      }
      gpp->val.gui_proc_thread = NULL;
   }
#endif

  l_vid_total_sec = 0.0;
  if(gstb->vidhand_open_ok)
  {
    if(gstb->framerate != 0.0)
    {
       l_framerate_info = g_strdup_printf(_("using master_framerate %2.2f found in file")
                                         ,(float)gstb->framerate);
       gtk_adjustment_set_value(GTK_ADJUSTMENT(gpp->cme__spinbutton_framerate_adj)
                                , (gfloat)gstb->framerate);
       gpp->val.framerate = gstb->framerate;

    }
    else
    {
       l_framerate_info = g_strdup_printf(_("file has no master_framerate setting"));
    }
    if(gpp->val.framerate > 0)
    {
      l_vid_total_sec = gpp->val.storyboard_total_frames / gpp->val.framerate;
    }

    if(gstb->aud_total_sec > 0.0)
    {
      if(gstb->composite_audio[0] != '\0')
      {
         if(g_file_test(gstb->composite_audio, G_FILE_TEST_EXISTS))
         {
            GtkEntry *entry;

            g_snprintf(gpp->val.audioname1, sizeof(gpp->val.audioname1), "%s"
                      , gstb->composite_audio);
            gpp->val.audioname_ptr = &gpp->val.audioname1[0];
            entry = GTK_ENTRY(gpp->cme__entry_audio1);
            if(entry)
            {
              gtk_entry_set_text(entry, gstb->composite_audio);
            }

            gstb->composite_audio[0] = '\0';
         }
      }

      l_playtime_info = g_strdup_printf(_("composite video track playtime %.3fsec (%d frames)\ncomposite audiotrack playtime %.3f secs")
                                           , (float)l_vid_total_sec
                                           , (int) gpp->val.storyboard_total_frames
                                           , (float)gstb->aud_total_sec
                                           );
    }
    else
    {
      l_playtime_info = g_strdup_printf(_("composite video track playtime %.3fsec (%d frames)\nhas NO audiotracks")
                                           , (float)l_vid_total_sec
                                           , (int) gpp->val.storyboard_total_frames
                                           );
    }

    if(gstb->master_width != 0)
    {
       l_size_info = g_strdup_printf(_("using master_size %d x %d found in file")
                                         ,(int)gstb->master_width
                                         ,(int)gstb->master_height);
       gtk_adjustment_set_value(GTK_ADJUSTMENT(gpp->cme__spinbutton_width_adj)
                               , (gfloat)gstb->master_width);

       gtk_adjustment_set_value(GTK_ADJUSTMENT(gpp->cme__spinbutton_height_adj)
                               , (gfloat)gstb->master_height);
       gpp->val.vid_width = gstb->master_width;
       gpp->val.vid_height = gstb->master_height;
    }
    else
    {
       l_size_info = g_strdup_printf(_("file has no master_size setting"));
    }

   if(gstb->errtext)
   {
       if(gstb->warntext)
       {
         l_msg = g_strdup_printf(_("Storyboard file %s checkreport:\n\n%s\n%s\n%s\n\n%s\n[%d:] %s\n\n%s\n[%d:] %s")
                                 , " " /* gpp->val.storyboard_file */
                                 , l_playtime_info
                                 , l_size_info
                                 , l_framerate_info
                                 , gstb->errtext
                                 , (int)gstb->errline_nr
                                 , gstb->errline
                                 , gstb->warntext
                                 , (int)gstb->warnline_nr
                                 , gstb->warnline
                                 );
       }
       else
       {
         l_msg = g_strdup_printf(_("Storyboard file %s checkreport:\n\n%s\n%s\n%s\n\n%s\n[%d:] %s")
                                 , " " /* gpp->val.storyboard_file */
                                 , l_playtime_info
                                 , l_size_info
                                 , l_framerate_info
                                 , gstb->errtext
                                 , (int)gstb->errline_nr
                                 , gstb->errline
                                 );
       }
    }
    else
    {
       if(gstb->warntext)
       {
         l_msg = g_strdup_printf(_("Storyboard file %s checkreport:\n\n%s\n%s\n%s\n\n%s\n[%d:] %s")
                                 , " " /* gpp->val.storyboard_file */
                                 , l_playtime_info
                                 , l_size_info
                                 , l_framerate_info
                                 , gstb->warntext
                                 , (int)gstb->warnline_nr
                                 , gstb->warnline
                                 );
       }
       else
       {
         l_msg = g_strdup_printf(_("Storyboard file %s checkreport:\n\n%s\n%s\n%s\n\nno errors found, file is OK")
                                 ,  " " /* gpp->val.storyboard_file */
                                 , l_playtime_info
                                 , l_size_info
                                 , l_framerate_info
                                );
       }
    }
    g_free(l_size_info);
    g_free(l_framerate_info);
  }
  else
  {
     l_msg = g_strdup_printf(_("Storyboard file %s checkreport:\n\nSYNTAX check failed (internal error occurred)")
                               , gpp->val.storyboard_file
                                 );
  }

  /* info window of storyboard parsing report */
  /* g_message("%s", l_msg); */
  p_print_storyboard_text_label(gpp, l_msg);

  gstb->progress = 0.0;
  g_snprintf(gstb->status_msg, sizeof(gstb->status_msg), _("ready"));
  p_status_progress(gpp, gstb);

  if(gap_debug)
  {
    gap_file_printf("p_storybord_job_finished:\nMSG: %s\n", l_msg);
    gap_file_printf("(##) first:%d last:%d input_mode:%d\n"
       , (int)gstb->first_frame_limit
       , (int)gstb->last_frame_nr
       , (int)gpp->val.input_mode
       );
  }

  g_free(l_msg);

  p_range_widgets_set_limits(gpp
                             , gstb->first_frame_limit
                             , gstb->last_frame_nr
                             , gpp->val.input_mode
                             );

  gap_cme_gui_upd_wgt_sensitivity(gpp);

}  /* end p_storybord_job_finished */

/* -----------------------------
 * gap_cme_gui_remove_poll_timer
 * -----------------------------
 */
void
gap_cme_gui_remove_poll_timer(GapCmeGlobalParams *gpp)
{
  t_global_stb *gstb;

  gstb = &global_stb;

  if(gstb->poll_timertag >= 0)
  {
    g_source_remove(gstb->poll_timertag);
    gstb->poll_timertag = -1;
  }
}  /* end gap_cme_gui_remove_poll_timer */


/* ------------------
 * on_timer_poll
 * ------------------
 * This timer callback is called periodical
 * (The poll timer is restarted at each call with intervall of 1/4 sec)
 */
static void
on_timer_poll(gpointer   user_data)
{
  t_global_stb *gstb;
  GapCmeGlobalParams *gpp;

  if(gap_debug)
  {
    gap_file_printf("\non_timer_poll: START\n");
  }

  gstb = (t_global_stb *)user_data;
  gpp = gap_cme_main_get_global_params();

  if(gstb)
  {
     g_source_remove(gstb->poll_timertag);
     if(gstb->stb_job_done)
     {
       gstb->poll_timertag = -1;
       gstb->stb_job_done = FALSE;
       p_storybord_job_finished(gpp, gstb);
     }
     else
     {
       if(gstb->poll_timertag >= 0)
       {
          /* storyboard thread job still running
          * update status, and restart poll timer
          */
          p_status_progress(gpp, gstb);

          gstb->poll_timertag =
            (gint32) g_timeout_add (500, (GSourceFunc)on_timer_poll, gstb);
       }
     }
  }
}  /* end on_timer_poll */



/* ----------------------------------------
 * p_thread_storyboard_file
 * ----------------------------------------
 * p_thread_storyboard_file
 *   check if storyboard exists,
 *   if exists, set lower and upper limit of the range spinbutton widgets
 *   to 1 and the total number of frames found in the storyboard_file.
 *
 *   if no storyboard file exists then use the first/last frame_nr from the ainfo structure
 *   that describes the animation frames from the invoking image_frame.
 */
static gpointer
p_thread_storyboard_file(gpointer data)
{
  GapCmeGlobalParams   *gpp;
  t_global_stb         *gstb;
  GapGveStoryVidHandle *vidhand;
  gint32                l_first_frame_limit;
  gint32                l_last_frame_nr;
  GapLibTypeInputRange  l_rangetype;
  gdouble               l_aud_total_sec;
  gboolean              l_create_audio_tmp_files;

  gpp = gap_cme_main_get_global_params();

  if(gap_debug)
  {
    gap_file_printf("THREAD: p_thread_storyboard_file &gpp: %ld\n", (long)gpp);
  }

  gstb = &global_stb;
  gstb->total_stroyboard_frames = 0;

  p_get_range_and_type (gpp, &l_first_frame_limit, &l_last_frame_nr, &l_rangetype);


  gstb->vidhand_open_ok = FALSE;

  l_create_audio_tmp_files = FALSE;
  if(gpp->storyboard_create_composite_audio)
  {
    l_create_audio_tmp_files = TRUE;
  }

  vidhand = gap_story_render_open_extended_video_handle
           ( FALSE   /* dont ignore video */
           , FALSE   /* dont ignore audio */
           , l_create_audio_tmp_files
           , &gstb->progress
           , &gstb->status_msg[0]
           , sizeof(gstb->status_msg)
           , GAP_RNGTYPE_STORYBOARD
           , NULL      /* use no imagename */
           , gpp->val.storyboard_file
           , NULL      /* use no basename */
           , NULL      /* use no extension */
           , -1        /* frame_from */
           , 999999    /* frame_to */
           , &gpp->val.storyboard_total_frames
           , gpp->val.util_sox
           , gpp->val.util_sox_options
           );

  if(vidhand)
  {
    gstb->vidhand_open_ok = TRUE;

    if(vidhand->master_framerate != 0.0)
    {
        gpp->val.framerate = vidhand->master_framerate;
        gstb->framerate = vidhand->master_framerate;

    }


    gap_gve_story_calc_audio_playtime(vidhand, &l_aud_total_sec);
    gstb->aud_total_sec = l_aud_total_sec;

    if((gpp->storyboard_create_composite_audio)
    && (l_aud_total_sec > 0.0))
    {
      char *l_composite_audio;

      if(gap_debug)
      {
        gap_gve_story_debug_print_audiorange_list(vidhand->aud_list, -1);
      }

      /* name for the composite audio that should be created */
      l_composite_audio = g_strdup_printf("%s_composite_audio.wav", gpp->val.storyboard_file);


      gstb->composite_audio[0] = '\0';
      if(l_composite_audio)
      {
         gap_gve_story_create_composite_audiofile(vidhand, l_composite_audio);
         if(g_file_test(l_composite_audio, G_FILE_TEST_EXISTS))
         {
            g_snprintf(gstb->composite_audio, sizeof(gstb->composite_audio)
                      , "%s"
                      ,l_composite_audio
                      );

            if(gap_debug)
            {
              gap_gve_story_debug_print_audiorange_list(vidhand->aud_list, -1);
            }
            gap_gve_story_drop_audio_cache();
         }
         g_free(l_composite_audio);
      }
      gap_gve_story_remove_tmp_audiofiles(vidhand);

    }

    gstb->master_width = vidhand->master_width;
    gstb->master_height = vidhand->master_height;


    gstb->errtext = NULL;
    gstb->errline = NULL;
    gstb->warntext = NULL;
    gstb->warnline = NULL;

    if(vidhand->sterr->errtext)
    {
      gstb->errline_nr = vidhand->sterr->errline_nr;
      gstb->errtext = g_strdup(vidhand->sterr->errtext);
      gstb->errline = g_strdup(vidhand->sterr->errline);
    }
    if(vidhand->sterr->warntext)
    {
      gstb->warnline_nr = vidhand->sterr->warnline_nr;
      gstb->warntext = g_strdup(vidhand->sterr->warntext);
      gstb->warnline = g_strdup(vidhand->sterr->warnline);
    }

    if(vidhand->frn_list)
    {
        l_first_frame_limit = 1;
        l_last_frame_nr = gpp->val.storyboard_total_frames;
        l_rangetype = GAP_RNGTYPE_STORYBOARD;
        gstb->total_stroyboard_frames = gpp->val.storyboard_total_frames;
    }
    gap_gve_story_close_vid_handle(vidhand);
  }

  gstb->first_frame_limit = l_first_frame_limit;
  gstb->last_frame_nr     = l_last_frame_nr;
  gpp->val.input_mode     = l_rangetype;

  if(gap_debug)
  {
    gap_file_printf("THREAD storyboard TERMINATING: gui_proc_thread:%ld first:%d last:%d input_mode:%d\n"
       , (long)gpp->val.gui_proc_thread
       , (int)gstb->first_frame_limit
       , (int)gstb->last_frame_nr
       , (int)gpp->val.input_mode
       );
  }

  gstb->stb_job_done = TRUE;
  /* gpp->val.gui_proc_thread = NULL; */

  return (NULL);
} /* end p_thread_storyboard_file */



/* ----------------------------------------
 * gap_cme_gui_check_storyboard_file
 * ----------------------------------------
 */
void
gap_cme_gui_check_storyboard_file(GapCmeGlobalParams *gpp)
{
   gboolean joinable;
   t_global_stb    *gstb;
  gint32 l_first_frame_limit;
  gint32 l_last_frame_nr;
  GapLibTypeInputRange l_rangetype;

  gstb = &global_stb;
  p_get_range_and_type (gpp, &l_first_frame_limit, &l_last_frame_nr, &l_rangetype);

  gstb->total_stroyboard_frames = 0;
  gstb->first_frame_limit  = l_first_frame_limit;
  gstb->last_frame_nr      = l_last_frame_nr;
  gpp->val.input_mode      = l_rangetype;
  gstb->composite_audio[0] = '\0';
  gstb->aud_total_sec      = 0.0;

  p_range_widgets_set_limits(gpp
                             , l_first_frame_limit
                             , l_last_frame_nr
                             , l_rangetype
                             );

  if(gpp->ecp == NULL)
  {
    return;
  }


  if(gpp->val.storyboard_file[0] == '\0')
  {
    p_print_storyboard_text_label(gpp, NULL);
    return;
  }
  if(!g_file_test(gpp->val.storyboard_file, G_FILE_TEST_EXISTS))
  {
    p_print_storyboard_text_label(gpp, NULL);
     return;
  }



#ifdef GAP_GUI_USE_GTHREAD
  if(gap_cme_gui_check_gui_thread_is_active(gpp))  { return; }
  if(gstb->poll_timertag >= 0)           { return; }

  /* g_message(_("Go for checking storyboard file")); */
  p_print_storyboard_text_label(gpp, _("Checking Storyboard File") );

  gstb->progress = 0.0;
  g_snprintf(gstb->status_msg, sizeof(gstb->status_msg), _("Parsing Storyboardfile"));

  gstb->stb_job_done  = FALSE;

  /* start a thread for asynchron PDB call of the gui_ procedure
   */
  if(gap_debug)
  {
    gap_file_printf("MASTER: Before storyborad g_thread_create\n");
  }

  joinable = TRUE;
  gpp->val.gui_proc_thread =
      g_thread_create((GThreadFunc)p_thread_storyboard_file
                     , NULL  /* data */
                     , joinable
                     , NULL  /* GError **error (NULL dont report errors) */
                     );

  if(gap_debug)
  {
    gap_file_printf("MASTER: After storyborad g_thread_create\n");
  }

  /* start poll timer to update progress and notify when storyboard job finished */
  gstb->poll_timertag =
    (gint32) g_timeout_add(500, (GSourceFunc) on_timer_poll, gstb);


#else
  /* if threads are not used simply call the procedure
   * (the common GUI window is not refreshed until the called gui_proc ends)
   */
  {
    gboolean do_processing;

    do_processing = TRUE;

    /* if storyboard_create_composite_audio button was pressed
     * there is no need for extra pop-up dialog
     * before start processing
     */
    if (!gpp->storyboard_create_composite_audio)
    {
      static GapArrArg  argv[1];

      do_processing = FALSE;

      gap_arr_arg_init(&argv[0], GAP_ARR_WGT_LABEL);
      argv[0].label_txt = _("Go for checking storyboard file");

      if(gap_arr_ok_cancel_dialog(_("Storyboardfile Check")
                               ,_("Storyboardfile Check")
                               ,1
                               ,argv
                               ))
      {
        do_processing = TRUE;
      }
    }

    if(do_processing)
    {
      p_thread_storyboard_file(NULL);
      p_storybord_job_finished(gpp, gstb);
    }
  }
#endif


}  /* end gap_cme_gui_check_storyboard_file */



/* ----------------------------------------
 * gap_cme_gui_check_encode_OK
 * ----------------------------------------
 *
 *   check the (dialog) parameters if everything is OK
 *   this is done before we can start encoding
 *
 *   return  TRUE  .. OK
 *           FALSE .. in case of Error
 */
gboolean
gap_cme_gui_check_encode_OK (GapCmeGlobalParams *gpp)
{
  gchar       *l_msg;
  long        samplerate;
  long        channels;
  long        bytes_per_sample;
  long        bits;
  long        samples;
  gint         l_rc;
  gint32      tmsec;        /* audioplaytime in milli secs */

  samplerate = 0;
  bits = 16;

  if(gap_debug)
  {
    gap_file_printf("gap_cme_gui_check_encode_OK: Start\n");
  }

  if(gpp->val.gui_proc_thread)
  {
     p_gap_message(_("Encoder specific parameter window is still open" ));
     return (FALSE);
  }

  if(gpp->val.input_mode == GAP_RNGTYPE_STORYBOARD )
  {
    if((!g_file_test(gpp->val.storyboard_file, G_FILE_TEST_EXISTS))
    || (gpp->val.storyboard_total_frames < 1))
    {
      p_gap_message(_("ERROR: No valid storyboardfile was specified.\n"
                      "(a storyboard file can be specified in the extras tab)"));
      return (FALSE);
    }
  }

  if ((strcmp(gpp->val.ecp_sel.vid_enc_plugin, GAP_PLUGIN_NAME_MPG2_ENCODE) == 0)
  ||  (strcmp(gpp->val.ecp_sel.vid_enc_plugin, GAP_PLUGIN_NAME_MPG1_ENCODE) == 0))
  {
    if(((gpp->val.vid_width % 16)  != 0)
    || ((gpp->val.vid_height % 16)  != 0))
    {
      l_msg = g_strdup_printf(_("Error:\nfor MPEG width and height must be a multiple of 16\n"
                         "set Width to %d\n"
                         "set Height to %d")
                         , (int)(gpp->val.vid_width / 16) * 16
                         , (int)(gpp->val.vid_height / 16) * 16
                         );
      g_message("%s", l_msg);
      g_free(l_msg);
      return (FALSE);
    }
  }

  if(g_file_test(gpp->val.audioname1, G_FILE_TEST_EXISTS))
  {
     tmsec = 0;
     l_rc = gap_audio_wav_file_check(gpp->val.audioname1
                     , &samplerate, &channels
                     , &bytes_per_sample, &bits, &samples);

     if(l_rc == 0)
     {
       if((bits != 8) && (bits != 16))
       {
         l_msg = g_strdup_printf(_("Error: Unsupported Bit per Sample %d\n"
                            "file: %s\n"
                            "supported are 8 or 16 Bit")
                            , (int)bits
                            , gpp->val.audioname1
                            );
         g_message("%s", l_msg);
         g_free(l_msg);
         return (FALSE);
       }

       if((samplerate > 0) && (channels > 0))
       {
         tmsec = ((samples / channels) * 1000) / samplerate;
       }
     }
  }
  else
  {
    if(gpp->val.audioname1[0] != '\0')
    {
         l_msg = g_strdup_printf(_("Error: Audiofile not found\n"
                            "file: %s\n")
                            , gpp->val.audioname1
                            );
         g_message("%s", l_msg);
         g_free(l_msg);
         return (FALSE);
    }
  }

  if(gpp->val.audioname1[0] != '\0')
  {
    if (strcmp(gpp->val.ecp_sel.vid_enc_plugin, GAP_PLUGIN_NAME_MPG1_ENCODE) == 0)
    {
      switch(gpp->val.samplerate)
      {
         case 22050:
         case 24000:
         case 32000:
         case 44100:
         case 48000:
             break;
         default:
             l_msg = g_strdup_printf(_("Error: Unsupported Samplerate for MPEG1 Layer2 Audio Encoding\n"
                                "current rate: %d\n"
                                "supported rates: \n"
                                " 22050, 24000, 32000, 44100, 48000")
                                , (int)gpp->val.samplerate);
             g_message("%s", l_msg);
             g_free(l_msg);
             return (FALSE);
             break;
      }
    }


    if (strcmp(gpp->val.ecp_sel.vid_enc_plugin, GAP_PLUGIN_NAME_MPG2_ENCODE) == 0)
    {
      switch(gpp->val.samplerate)
      {
         case 8000:
         case 11025:
         case 12000:
         case 16000:
         case 22050:
         case 24000:
         case 32000:
         case 44100:
         case 48000:
             break;
         default:
             l_msg = g_strdup_printf(_("Error: Unsupported Samplerate for MPEG2 Layer3 Audio Encoding\n"
                                "current rate: %d\n"
                                "supported rates:\n"
                                " 8000, 11025, 12000, 16000, 22050, 24000, 32000, 44100, 48000")
                                , (int)gpp->val.samplerate);
             g_message("%s", l_msg);
             g_free(l_msg);
             return (FALSE);
             break;
      }
    }
  }

  if(g_file_test(gpp->val.videoname, G_FILE_TEST_EXISTS))
  {
    if(p_overwrite_dialog(gpp, gpp->val.videoname, 0) < 0)
    {
      return (FALSE);
    }
  }

  if(0 != gap_gve_sox_chk_and_resample(&gpp->val))
  {
     g_message(_("Can't process the audio input file."
                 " You should check audio options and audio tool configuration"));
     return (FALSE);
  }

  if(gap_debug)
  {
    gap_file_printf("gap_cme_gui_check_encode_OK: End OK tmsec:%s\n", (int)tmsec);
  }
  return (TRUE); /* OK */
}  /* end gap_cme_gui_check_encode_OK */


/* ----------------------------------------
 * gap_cme_gui_create_fss__fileselection
 * ----------------------------------------
 */
GtkWidget*
gap_cme_gui_create_fss__fileselection (GapCmeGlobalParams *gpp)
{
  GtkWidget *fss__fileselection;
  GtkWidget *fss__button_OK;
  GtkWidget *fss__button_cancel;

  fss__fileselection = gtk_file_selection_new (_("Select Storyboardfile"));
  gtk_container_set_border_width (GTK_CONTAINER (fss__fileselection), 10);

  fss__button_OK = GTK_FILE_SELECTION (fss__fileselection)->ok_button;
  gtk_widget_show (fss__button_OK);
  GTK_WIDGET_SET_FLAGS (fss__button_OK, GTK_CAN_DEFAULT);

  fss__button_cancel = GTK_FILE_SELECTION (fss__fileselection)->cancel_button;
  gtk_widget_show (fss__button_cancel);
  GTK_WIDGET_SET_FLAGS (fss__button_cancel, GTK_CAN_DEFAULT);

  g_signal_connect (G_OBJECT (fss__fileselection), "destroy",
                      G_CALLBACK (on_fss__fileselection_destroy),
                      gpp);
  g_signal_connect (G_OBJECT (fss__button_OK), "clicked",
                      G_CALLBACK (on_fss__button_OK_clicked),
                      gpp);
  g_signal_connect (G_OBJECT (fss__button_cancel), "clicked",
                      G_CALLBACK (on_fss__button_cancel_clicked),
                      gpp);

  return fss__fileselection;
}  /* end gap_cme_gui_create_fss__fileselection */


/* ----------------------------------------
 * gap_cme_gui_create_fsv__fileselection
 * ----------------------------------------
 */
GtkWidget*
gap_cme_gui_create_fsv__fileselection (GapCmeGlobalParams *gpp)
{
  GtkWidget *fsv__fileselection;
  GtkWidget *fsv__button_OK;
  GtkWidget *fsv__button_cancel;

  fsv__fileselection = gtk_file_selection_new (_("Select Videofile"));
  gtk_container_set_border_width (GTK_CONTAINER (fsv__fileselection), 10);

  fsv__button_OK = GTK_FILE_SELECTION (fsv__fileselection)->ok_button;
  gtk_widget_show (fsv__button_OK);
  GTK_WIDGET_SET_FLAGS (fsv__button_OK, GTK_CAN_DEFAULT);

  fsv__button_cancel = GTK_FILE_SELECTION (fsv__fileselection)->cancel_button;
  gtk_widget_show (fsv__button_cancel);
  GTK_WIDGET_SET_FLAGS (fsv__button_cancel, GTK_CAN_DEFAULT);

  g_signal_connect (G_OBJECT (fsv__fileselection), "destroy",
                      G_CALLBACK (on_fsv__fileselection_destroy),
                      gpp);
  g_signal_connect (G_OBJECT (fsv__button_OK), "clicked",
                      G_CALLBACK (on_fsv__button_OK_clicked),
                      gpp);
  g_signal_connect (G_OBJECT (fsv__button_cancel), "clicked",
                      G_CALLBACK (on_fsv__button_cancel_clicked),
                      gpp);

  gtk_widget_grab_default (fsv__button_cancel);
  return fsv__fileselection;
}  /* end gap_cme_gui_create_fsv__fileselection */


/* ----------------------------------------
 * gap_cme_gui_create_fsb__fileselection
 * ----------------------------------------
 */
GtkWidget*
gap_cme_gui_create_fsb__fileselection (GapCmeGlobalParams *gpp)
{
  GtkWidget *fsb__fileselection;
  GtkWidget *fsb__button_OK;
  GtkWidget *fsb__button_cancel;

  fsb__fileselection = gtk_file_selection_new (_("Select Macrofile"));
  gtk_container_set_border_width (GTK_CONTAINER (fsb__fileselection), 10);

  fsb__button_OK = GTK_FILE_SELECTION (fsb__fileselection)->ok_button;
  gtk_widget_show (fsb__button_OK);
  GTK_WIDGET_SET_FLAGS (fsb__button_OK, GTK_CAN_DEFAULT);

  fsb__button_cancel = GTK_FILE_SELECTION (fsb__fileselection)->cancel_button;
  gtk_widget_show (fsb__button_cancel);
  GTK_WIDGET_SET_FLAGS (fsb__button_cancel, GTK_CAN_DEFAULT);

  g_signal_connect (G_OBJECT (fsb__fileselection), "destroy",
                      G_CALLBACK (on_fsb__fileselection_destroy),
                      gpp);
  g_signal_connect (G_OBJECT (fsb__button_OK), "clicked",
                      G_CALLBACK (on_fsb__button_OK_clicked),
                      gpp);
  g_signal_connect (G_OBJECT (fsb__button_cancel), "clicked",
                      G_CALLBACK (on_fsb__button_cancel_clicked),
                      gpp);

  gtk_widget_grab_default (fsb__button_cancel);
  return fsb__fileselection;
}  /* end gap_cme_gui_create_fsb__fileselection */

/* ----------------------------------------
 * gap_cme_gui_create_fsa__fileselection
 * ----------------------------------------
 */
GtkWidget*
gap_cme_gui_create_fsa__fileselection (GapCmeGlobalParams *gpp)
{
  GtkWidget *fsa__fileselection;
  GtkWidget *fsa__button_OK;
  GtkWidget *fsa__button_cancel;

  fsa__fileselection = gtk_file_selection_new (_("Select Audiofilename"));
  gtk_container_set_border_width (GTK_CONTAINER (fsa__fileselection), 10);

  fsa__button_OK = GTK_FILE_SELECTION (fsa__fileselection)->ok_button;
  gtk_widget_show (fsa__button_OK);
  GTK_WIDGET_SET_FLAGS (fsa__button_OK, GTK_CAN_DEFAULT);

  fsa__button_cancel = GTK_FILE_SELECTION (fsa__fileselection)->cancel_button;
  gtk_widget_show (fsa__button_cancel);
  GTK_WIDGET_SET_FLAGS (fsa__button_cancel, GTK_CAN_DEFAULT);

  g_signal_connect (G_OBJECT (fsa__fileselection), "destroy",
                      G_CALLBACK (on_fsa__fileselection_destroy),
                      gpp);
  g_signal_connect (G_OBJECT (fsa__button_OK), "clicked",
                      G_CALLBACK (on_fsa__button_OK_clicked),
                      gpp);
  g_signal_connect (G_OBJECT (fsa__button_cancel), "clicked",
                      G_CALLBACK (on_fsa__button_cancel_clicked),
                      gpp);

  gtk_widget_grab_default (fsa__button_cancel);
  return fsa__fileselection;
}  /* end gap_cme_gui_create_fsa__fileselection */


/* ----------------------------------------
 * create_ow__dialog
 * ----------------------------------------
 */
static GtkWidget*
create_ow__dialog (GapCmeGlobalParams *gpp)
{
  GtkWidget *ow__dialog;
  GtkWidget *ow__dialog_vbox0;
  GtkWidget *ow__frame1;
  GtkWidget *ow__vbox1;
  GtkWidget *ow__hbox2;
  GtkWidget *ow__label1;
  GtkWidget *ow__hbox3;
  GtkWidget *ow__filename;
  GtkWidget *ow__action_area1;
  GtkWidget *ow__hbox1;
  GtkWidget *ow__label_dummy22;
  GtkWidget *ow__button_one;
  GtkWidget *ow__button_cancel;

  ow__dialog = gtk_dialog_new ();
  gtk_window_set_title (GTK_WINDOW (ow__dialog), _("Overwrite warning"));

  ow__dialog_vbox0 = GTK_DIALOG (ow__dialog)->vbox;
  gtk_widget_show (ow__dialog_vbox0);

  ow__frame1 = gimp_frame_new (NULL);
  gtk_widget_show (ow__frame1);
  gtk_box_pack_start (GTK_BOX (ow__dialog_vbox0), ow__frame1, TRUE, TRUE, 0);
  gtk_container_set_border_width (GTK_CONTAINER (ow__frame1), 5);

  ow__vbox1 = gtk_vbox_new (FALSE, 1);
  gtk_widget_show (ow__vbox1);
  gtk_container_add (GTK_CONTAINER (ow__frame1), ow__vbox1);
  gtk_container_set_border_width (GTK_CONTAINER (ow__vbox1), 5);

  ow__hbox2 = gtk_hbox_new (FALSE, 0);
  gtk_widget_show (ow__hbox2);
  gtk_box_pack_start (GTK_BOX (ow__vbox1), ow__hbox2, FALSE, FALSE, 0);

  ow__label1 = gtk_label_new (_("File already exists:"));
  gtk_widget_show (ow__label1);
  gtk_box_pack_start (GTK_BOX (ow__hbox2), ow__label1, FALSE, FALSE, 0);
  gtk_label_set_justify (GTK_LABEL (ow__label1), GTK_JUSTIFY_LEFT);

  ow__hbox3 = gtk_hbox_new (FALSE, 0);
  gtk_widget_show (ow__hbox3);
  gtk_box_pack_start (GTK_BOX (ow__vbox1), ow__hbox3, FALSE, FALSE, 0);

  ow__filename = gtk_label_new (_("filename"));
  gtk_widget_show (ow__filename);
  gtk_box_pack_start (GTK_BOX (ow__hbox3), ow__filename, FALSE, FALSE, 0);
  gtk_label_set_justify (GTK_LABEL (ow__filename), GTK_JUSTIFY_LEFT);

  ow__action_area1 = GTK_DIALOG (ow__dialog)->action_area;
  gtk_widget_show (ow__action_area1);
  gtk_container_set_border_width (GTK_CONTAINER (ow__action_area1), 10);

  ow__hbox1 = gtk_hbox_new (FALSE, 0);
  gtk_widget_show (ow__hbox1);
  gtk_box_pack_start (GTK_BOX (ow__action_area1), ow__hbox1, TRUE, TRUE, 0);

  ow__label_dummy22 = gtk_label_new (_("  "));
  gtk_widget_show (ow__label_dummy22);
  gtk_box_pack_start (GTK_BOX (ow__hbox1), ow__label_dummy22, TRUE, FALSE, 0);

  ow__button_one = gtk_button_new_with_label (_("Overwrite"));
  gtk_widget_show (ow__button_one);
  gtk_box_pack_start (GTK_BOX (ow__hbox1), ow__button_one, FALSE, FALSE, 0);
  gtk_container_set_border_width (GTK_CONTAINER (ow__button_one), 2);
  GTK_WIDGET_SET_FLAGS (ow__button_one, GTK_CAN_DEFAULT);

  ow__button_cancel = gtk_button_new_with_label (_("Cancel"));
  gtk_widget_show (ow__button_cancel);
  gtk_box_pack_start (GTK_BOX (ow__hbox1), ow__button_cancel, FALSE, FALSE, 0);
  gtk_container_set_border_width (GTK_CONTAINER (ow__button_cancel), 2);
  GTK_WIDGET_SET_FLAGS (ow__button_cancel, GTK_CAN_DEFAULT);

  g_signal_connect (G_OBJECT (ow__dialog), "destroy",
                      G_CALLBACK (on_ow__dialog_destroy),
                      gpp);
  g_signal_connect (G_OBJECT (ow__button_one), "clicked",
                      G_CALLBACK (on_ow__button_one_clicked),
                      gpp);
  g_signal_connect (G_OBJECT (ow__button_cancel), "clicked",
                      G_CALLBACK (on_ow__button_cancel_clicked),
                      gpp);

  gtk_widget_grab_default (ow__button_cancel);

  gpp->ow__filename = ow__filename;

  return ow__dialog;
}  /* end create_ow__dialog */


/* ---------------------------------
 * p_input_mode_radio_callback
 * ---------------------------------
 */
static void
p_input_mode_radio_callback(GtkWidget *widget, GapCmeGlobalParams *gpp)
{
  GapLibTypeInputRange l_rangetype;

  l_rangetype = (GapLibTypeInputRange) g_object_get_data (G_OBJECT (widget)
                                                        , RADIO_ITEM_INDEX_KEY);


  if((gpp) && (GTK_TOGGLE_BUTTON (widget)->active))
  {
    gint32  l_first_frame_limit;
    gint32  l_last_frame_nr;

    gpp->val.input_mode = l_rangetype;

    p_get_range_from_type (gpp, l_rangetype, &l_first_frame_limit, &l_last_frame_nr);

    /* update range limits and label texts */
    p_range_widgets_set_limits(gpp
                             , l_first_frame_limit
                             , l_last_frame_nr
                             , l_rangetype
                             );
  }
}  /* end p_input_mode_radio_callback */


/* ---------------------------------
 * p_create_input_mode_widgets
 * ---------------------------------
 */
static void
p_create_input_mode_widgets(GtkWidget *table, int row, int col, GapCmeGlobalParams *gpp)
{
  GtkWidget *label;
  GtkWidget *radio_table;
  GtkWidget *radio_button;
  GSList    *radio_group = NULL;
  gint      l_idx;
  gboolean  l_radio_pressed;

  label = gtk_label_new(_("Input Mode:"));
  gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.0);
  gtk_table_attach( GTK_TABLE (table), label, col, col+1, row, row+1, GTK_FILL, GTK_FILL, 0, 10);
  gtk_widget_show(label);

  /* radio_table */
  radio_table = gtk_table_new (1, 3, FALSE);

  l_idx = 0;
  /* radio button Frames input_mode */
  radio_button = gtk_radio_button_new_with_label ( radio_group, _("Frames") );
  gpp->cme__radio_button_frame = radio_button;
  radio_group = gtk_radio_button_get_group ( GTK_RADIO_BUTTON (radio_button) );
  gtk_table_attach ( GTK_TABLE (radio_table), radio_button, l_idx, l_idx+1
                   , 0, 1, GTK_FILL | GTK_EXPAND, 0, 0, 0);


  l_radio_pressed = (gpp->val.input_mode == GAP_RNGTYPE_FRAMES);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (radio_button),
                                   l_radio_pressed);
  gimp_help_set_help_data(radio_button, _("Input is a sequence of frame images"), NULL);

  gtk_widget_show (radio_button);
  g_object_set_data (G_OBJECT (radio_button), RADIO_ITEM_INDEX_KEY
                    , (gpointer)GAP_RNGTYPE_FRAMES
                    );
  g_signal_connect ( G_OBJECT (radio_button), "toggled",
                     G_CALLBACK (p_input_mode_radio_callback),
                     gpp);


  l_idx = 1;

  /* radio button Layers input_mode */
  radio_button = gtk_radio_button_new_with_label ( radio_group, _("Layers") );
  gpp->cme__radio_button_layer = radio_button;
  radio_group = gtk_radio_button_get_group ( GTK_RADIO_BUTTON (radio_button) );
  gtk_table_attach ( GTK_TABLE (radio_table), radio_button, l_idx, l_idx+1, 0, 1
                   , GTK_FILL | GTK_EXPAND, 0, 0, 0);

  l_radio_pressed = (gpp->val.input_mode == GAP_RNGTYPE_LAYER);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (radio_button),
                                   l_radio_pressed);
  gimp_help_set_help_data(radio_button, _("Input is all the layers of one image"), NULL);

  gtk_widget_show (radio_button);
  g_object_set_data (G_OBJECT (radio_button), RADIO_ITEM_INDEX_KEY
                    , (gpointer)GAP_RNGTYPE_LAYER
                    );
  g_signal_connect ( G_OBJECT (radio_button), "toggled",
                     G_CALLBACK (p_input_mode_radio_callback),
                     gpp);



  l_idx = 2;

  /* radio button Storyboard input_mode */
  radio_button = gtk_radio_button_new_with_label ( radio_group, _("Storyboard") );
  gpp->cme__radio_button_storyboard = radio_button;
  radio_group = gtk_radio_button_get_group ( GTK_RADIO_BUTTON (radio_button) );
  gtk_table_attach ( GTK_TABLE (radio_table), radio_button, l_idx, l_idx+1, 0, 1
                   , GTK_FILL | GTK_EXPAND, 0, 0, 0);

  l_radio_pressed = (gpp->val.input_mode == GAP_RNGTYPE_STORYBOARD);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (radio_button),
                                   l_radio_pressed);
  gimp_help_set_help_data(radio_button, _("Input is videoclips and frames, defined via storyboard file. "
                                          "(specify the storyboard filename in the extras tab)")
                                          , NULL);

  gtk_widget_show (radio_button);
  g_object_set_data (G_OBJECT (radio_button), RADIO_ITEM_INDEX_KEY
                    , (gpointer)GAP_RNGTYPE_STORYBOARD
                    );
  g_signal_connect ( G_OBJECT (radio_button), "toggled",
                     G_CALLBACK (p_input_mode_radio_callback),
                     gpp);


  /* attach radio_table */
  gtk_table_attach ( GTK_TABLE (table), radio_table, col+1, col+2, row, row+1, GTK_FILL | GTK_EXPAND, 0, 0, 0);
  gtk_widget_show (radio_table);

}  /* end p_create_input_mode_widgets */



/* ----------------------------------------
 * p_create_shell_window
 * ----------------------------------------
 */
static GtkWidget*
p_create_shell_window (GapCmeGlobalParams *gpp)
{
  GtkWidget *shell_window;
  GtkWidget *cme__dialog_vbox1;
  GtkWidget *cme__vbox_main;
  GtkWidget *notebook;
  GtkWidget *frame;
  GtkWidget *label;
  GtkWidget *hbox;
  GtkWidget *entry;
  GtkWidget *button;
  GtkWidget *cme__table_status;
  GtkWidget *progressbar;


  gpp->cme__spinbutton_width_adj = NULL;
  gpp->cme__spinbutton_height_adj = NULL;
  gpp->cme__spinbutton_framerate_adj = NULL;
  gpp->cme__spinbutton_samplerate_adj = NULL;

  shell_window = gap_dialog_new (_("Master Videoencoder"),
                         GAP_CME_PLUGIN_NAME_VID_ENCODE_MASTER,
                         NULL, 0,
                         gimp_standard_help_func, GAP_CME_PLUGIN_HELP_ID_VID_ENCODE_MASTER,

                         GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                         GTK_STOCK_OK,     GTK_RESPONSE_OK,
                         NULL);
  gtk_window_set_type_hint (GTK_WINDOW(shell_window), GDK_WINDOW_TYPE_HINT_NORMAL);
  
  g_signal_connect (G_OBJECT (shell_window), "response",
                    G_CALLBACK (on_cme__response),
                    gpp);


  cme__dialog_vbox1 = GTK_DIALOG (shell_window)->vbox;
  gtk_widget_show (cme__dialog_vbox1);

  cme__vbox_main = gtk_vbox_new (FALSE, 0);
  gpp->cme__vbox_main = cme__vbox_main;
  gtk_widget_show (cme__vbox_main);
  gtk_box_pack_start (GTK_BOX (cme__dialog_vbox1), cme__vbox_main, TRUE, TRUE, 0);


  /* the notebook with encoding options */
  notebook = gtk_notebook_new ();
  gpp->cme__notebook = notebook;
  gtk_widget_show (notebook);
  gtk_box_pack_start (GTK_BOX (cme__vbox_main), notebook, TRUE, TRUE, 0);


  /* the video options notebook tab */
  label = gtk_label_new (_("Video Options"));
  gtk_widget_show (label);
  frame = p_create_video_options_frame(gpp);
  gtk_widget_show (frame);
  gtk_container_add (GTK_CONTAINER (notebook), frame);
  gtk_container_set_border_width (GTK_CONTAINER (frame), 4);
  gtk_notebook_set_tab_label (GTK_NOTEBOOK (notebook)
                            , gtk_notebook_get_nth_page (GTK_NOTEBOOK (notebook), 0)
                            , label);


  /* the Audio Options notebook tab */
  label = gtk_label_new (_("Audio Options"));
  gtk_widget_show (label);
  frame = p_create_audio_options_frame (gpp);
  gtk_widget_show (frame);
  gtk_container_add (GTK_CONTAINER (notebook), frame);
  gtk_container_set_border_width (GTK_CONTAINER (frame), 4);
  gtk_notebook_set_tab_label (GTK_NOTEBOOK (notebook)
                             , gtk_notebook_get_nth_page (GTK_NOTEBOOK (notebook), 1)
                             , label);


  /* the Audio Tool notebook tab */
  label = gtk_label_new (_("Audio Tool Configuration"));
  gtk_widget_show (label);
  frame = p_create_audiotool_cfg_frame (gpp);
  gtk_widget_show (frame);
  gtk_container_add (GTK_CONTAINER (notebook), frame);
  gtk_container_set_border_width (GTK_CONTAINER (frame), 4);
  gtk_notebook_set_tab_label (GTK_NOTEBOOK (notebook)
                             , gtk_notebook_get_nth_page (GTK_NOTEBOOK (notebook), 2)
                             , label);



  /* the Extras notebook tab */
  label = gtk_label_new (_("Extras"));
  gtk_widget_show (label);
  frame = p_create_encode_extras_frame (gpp);
  gtk_widget_show (frame);
  gtk_container_add (GTK_CONTAINER (notebook), frame);
  gtk_container_set_border_width (GTK_CONTAINER (frame), 4);
  gtk_notebook_set_tab_label (GTK_NOTEBOOK (notebook)
                             , gtk_notebook_get_nth_page (GTK_NOTEBOOK (notebook), 3)
                             , label);

  /* add the Encoding notebook tab */
  label = gtk_label_new (_("Encoding"));
  gtk_widget_show (label);
  frame = p_create_encoder_status_frame(gpp);
  gpp->cme__encoder_status_frame = frame;
  gtk_widget_show (frame);
  gtk_container_add (GTK_CONTAINER (notebook), frame);
  gtk_container_set_border_width (GTK_CONTAINER (frame), 4);
  gtk_notebook_set_tab_label (GTK_NOTEBOOK (notebook)
                            , gtk_notebook_get_nth_page (GTK_NOTEBOOK (notebook), 4)
                            , label);


  /* the output frame */

  /* the hbox */
  frame = gimp_frame_new (_("Output"));
  gtk_widget_show (frame);
  gtk_box_pack_start (GTK_BOX (cme__vbox_main), frame, FALSE /* expand*/, TRUE /* fill */, 0);
  gtk_container_set_border_width (GTK_CONTAINER (frame), 4);


  hbox = gtk_hbox_new (FALSE, 0);
  gtk_widget_show (hbox);
  gtk_container_add (GTK_CONTAINER (frame), hbox);

  /* the (output) video label */
  label = gtk_label_new (_("Video :"));
  gtk_widget_show (label);
  gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 2);

  /* the (output) video entry */
  entry = gtk_entry_new ();
  gpp->cme__entry_video = entry;
  gtk_widget_show (entry);
  gtk_box_pack_start (GTK_BOX (hbox), entry, TRUE, TRUE, 0);
  gimp_help_set_help_data (entry, _("Name of output videofile"), NULL);
  g_signal_connect (G_OBJECT (entry), "changed",
                      G_CALLBACK (on_cme__entry_video_changed),
                      gpp);

  /* the (output) video filebrowser button */

  button = gtk_button_new_with_label (_("..."));
  gpp->cme__button_video_filesel = button;
  gtk_widget_show (button);
  gtk_box_pack_start (GTK_BOX (hbox), button, FALSE, FALSE, 0);
  gtk_widget_set_size_request (button, 80, -1);
  gimp_help_set_help_data (button, _("Select output videofile via browser"), NULL);
  g_signal_connect (G_OBJECT (button), "clicked",
                      G_CALLBACK (on_cme__button_video_clicked),
                      gpp);


  /* the Status frame */
  frame = gimp_frame_new (_("Status"));
  gtk_widget_show (frame);
  gtk_box_pack_start (GTK_BOX (cme__vbox_main), frame, TRUE, TRUE, 2);
  gtk_container_set_border_width (GTK_CONTAINER (frame), 4);

  /* the Status table */
  cme__table_status = gtk_table_new (2, 2, FALSE);
  gtk_widget_show (cme__table_status);
  gtk_container_add (GTK_CONTAINER (frame), cme__table_status);

  /* the Status label */
  label = gtk_label_new (_("READY"));
  gpp->cme__label_status = label;

  /* hide cme__label_status, because status is now displayed
   * via gtk_progress_bar_set_text
   */
  gtk_widget_hide (label);
  gtk_table_attach (GTK_TABLE (cme__table_status), label, 0, 2, 0, 1,
                    (GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);
  gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
  gtk_misc_set_padding (GTK_MISC (label), 2, 0);

  /* the Status progressbar */
  progressbar = gtk_progress_bar_new ();
  gpp->cme__progressbar_status = progressbar;
  gtk_widget_show (progressbar);
  gtk_table_attach (GTK_TABLE (cme__table_status), progressbar, 0, 2, 1, 2,
                    (GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
                    (GtkAttachOptions) (0), 2, 0);


  return shell_window;
}  /* end p_create_shell_window */


/* ----------------------------------------
 * p_overwrite_dialog
 * ----------------------------------------
 */
static gint
p_overwrite_dialog(GapCmeGlobalParams *gpp, gchar *filename, gint overwrite_mode)
{
  if(g_file_test(filename, G_FILE_TEST_EXISTS))
  {
    if (overwrite_mode < 1)
    {
       if(gpp->val.run_mode != GIMP_RUN_NONINTERACTIVE)
       {
         gpp->val.ow_mode = -1; /* prepare Cancel as default */
         gpp->ow__dialog_window = create_ow__dialog (gpp);

         gtk_label_set_text(GTK_LABEL(gpp->ow__filename), filename);
         gtk_widget_show (gpp->ow__dialog_window);
         gtk_main ();
         /* set ow_mode 0: Overwrite One
          *             1: Overwrite All
          *            -1: Cancel
          */
         gpp->ow__dialog_window = NULL;
       }
       return(gpp->val.ow_mode);
    }
  }
  return (overwrite_mode);
}  /* end p_overwrite_dialog */


/* ----------------------------------------
 * p_create_encoder_status_frame
 * ----------------------------------------
 */
static GtkWidget*
p_create_encoder_status_frame (GapCmeGlobalParams *gpp)
{
  GtkWidget *frame;
  GtkWidget *table;
  GtkWidget *label;
  gint       row;

  frame = gimp_frame_new (_("Video Encoder Status"));


  table = gtk_table_new (4, 2, FALSE);
  gtk_widget_show (table);
  gtk_container_add (GTK_CONTAINER (frame), table);
  gtk_table_set_row_spacings (GTK_TABLE (table), 2);
  gtk_table_set_col_spacings (GTK_TABLE (table), 2);


  row = 0;

  label = gtk_label_new (_("Active Encoder:"));
  gtk_widget_show (label);
  gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
  gtk_table_attach (GTK_TABLE (table), label, 0, 1, row, row+1,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);


  label = gtk_label_new ("#");
  gpp->cme__label_active_encoder_name          = label;
  gtk_widget_show (label);
  gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
  gtk_table_attach (GTK_TABLE (table), label, 1, 2, row, row+1,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);

  row++;

  label = gtk_label_new (" ");
  gtk_widget_show (label);
  gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
  gtk_table_attach (GTK_TABLE (table), label, 0, 1, row, row+1,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);
  row++;

  label = gtk_label_new (_("Total Frames:"));
  gtk_widget_show (label);
  gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
  gtk_table_attach (GTK_TABLE (table), label, 0, 1, row, row+1,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);


  label = gtk_label_new ("######");
  gpp->cme__label_enc_stat_frames_total          = label;
  gtk_widget_show (label);
  gtk_misc_set_alignment (GTK_MISC (label), 1.0, 0.5);
  gtk_table_attach (GTK_TABLE (table), label, 1, 2, row, row+1,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);

  row++;


  label = gtk_label_new (_("Frames Done:"));
  gtk_widget_show (label);
  gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
  gtk_table_attach (GTK_TABLE (table), label, 0, 1, row, row+1,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);


  label = gtk_label_new ("######");
  gpp->cme__label_enc_stat_frames_done          = label;
  gtk_widget_show (label);
  gtk_misc_set_alignment (GTK_MISC (label), 1.0, 0.5);
  gtk_table_attach (GTK_TABLE (table), label, 1, 2, row, row+1,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);

  row++;


  label = gtk_label_new (_("Frames Encoded:"));
  gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
  gtk_widget_show (label);
  gtk_table_attach (GTK_TABLE (table), label, 0, 1, row, row+1,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);


  label = gtk_label_new ("######");
  gpp->cme__label_enc_stat_frames_encoded          = label;
  gtk_widget_show (label);
  gtk_misc_set_alignment (GTK_MISC (label), 1.0, 0.5);
  gtk_table_attach (GTK_TABLE (table), label, 1, 2, row, row+1,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);

  row++;


  label = gtk_label_new (_("Frames Copied (lossless):"));
  gtk_widget_show (label);
  gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
  gtk_table_attach (GTK_TABLE (table), label, 0, 1, row, row+1,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);


  label = gtk_label_new ("######");
  gpp->cme__label_enc_stat_frames_copied_lossless          = label;
  gtk_widget_show (label);
  gtk_misc_set_alignment (GTK_MISC (label), 1.0, 0.5);
  gtk_table_attach (GTK_TABLE (table), label, 1, 2, row, row+1,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);

  row++;


  label = gtk_label_new (_("Encoding Time Elapsed:"));
  gtk_widget_show (label);
  gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
  gtk_table_attach (GTK_TABLE (table), label, 0, 1, row, row+1,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);


  label = gtk_label_new ("######");
  gpp->cme__label_enc_time_elapsed          = label;
  gtk_widget_show (label);
  gtk_misc_set_alignment (GTK_MISC (label), 1.0, 0.5);
  gtk_table_attach (GTK_TABLE (table), label, 1, 2, row, row+1,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);

  return(frame);
}  /* end p_create_encoder_status_frame */

/* ----------------------------------------
 * p_create_encode_extras_frame
 * ----------------------------------------
 */
static GtkWidget*
p_create_encode_extras_frame (GapCmeGlobalParams *gpp)
{
  GtkWidget *frame;
  GtkWidget *table1;
  GtkWidget *button;
  GtkWidget *entry;
  GtkWidget *label;
  GtkWidget *checkbutton;
  gint       row;

  frame = gimp_frame_new (_("Encoding Extras"));


  table1 = gtk_table_new (7, 3, FALSE);
  gtk_widget_show (table1);
  gtk_container_add (GTK_CONTAINER (frame), table1);

  row = 0;

  /* the Macrofile label */
  label = gtk_label_new (_("Macrofile:"));
  gtk_widget_show (label);
  gtk_table_attach (GTK_TABLE (table1), label, 0, 1, row, row+1,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);
  gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);


  /* the Macrofile entry */
  entry = gtk_entry_new ();
  gpp->cme__entry_mac = entry;
  gtk_widget_show (entry);
  gtk_table_attach (GTK_TABLE (table1), entry, 1, 2, row, row+1,
                    (GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);
  gimp_help_set_help_data (entry, _("optional filtermacro file to be performed on each handled frame "), NULL);
  g_signal_connect (G_OBJECT (entry), "changed",
                      G_CALLBACK (on_cme__entry_mac_changed),
                      gpp);


  /* the Macrofile filebrowser button */
  button = gtk_button_new_with_label (_("..."));
  gtk_widget_show (button);
  gtk_table_attach (GTK_TABLE (table1), button, 2, 3, row, row+1,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);
  gtk_widget_set_size_request (button, 70, -1);
  gimp_help_set_help_data (button, _("select macrofile via browser"), NULL);
  g_signal_connect (G_OBJECT (button), "clicked",
                      G_CALLBACK (on_cme__button_mac_clicked),
                      gpp);


  row++;

  /* the Storyboard label */
  label = gtk_label_new (_("Storyboard File:"));
  gtk_widget_show (label);
  gtk_table_attach (GTK_TABLE (table1), label, 0, 1, row, row+1,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);
  gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);

  /* the Storyboard entry */
  entry = gtk_entry_new ();
  gpp->cme__entry_stb = entry;
  gtk_widget_show (entry);
  gtk_table_attach (GTK_TABLE (table1), entry, 1, 2, row, row+1,
                    (GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);
  gimp_help_set_help_data (entry, _("optionally use a storyboard file to feed the encoder"), NULL);
  g_signal_connect (G_OBJECT (entry), "changed",
                      G_CALLBACK (on_cme__entry_stb_changed),
                      gpp);


  /* the Storyboard filebrowser button */
  button = gtk_button_new_with_label (_("..."));
  gtk_widget_show (button);
  gtk_table_attach (GTK_TABLE (table1), button, 2, 3, row, row+1,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);
  gimp_help_set_help_data (button, _("select storyboard file via browser"), NULL);
  g_signal_connect (G_OBJECT (button), "clicked",
                      G_CALLBACK (on_cme__button_stb_clicked),
                      gpp);
  row++;

  /* the Storyboard Audio */
  label = gtk_label_new (_("Storyboard Audio:"));
  gtk_widget_show (label);
  gtk_table_attach (GTK_TABLE (table1), label, 0, 1, row, row+1,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);
  gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);

  /* the Storyboard filebrowser button */
  button = gtk_button_new_with_label (_("Create Composite Audiofile"));
  gpp->cme__button_stb_audio = button;
  gtk_widget_show (button);
  gtk_table_attach (GTK_TABLE (table1), button, 1, 2, row, row+1,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);
  gimp_help_set_help_data (button
                          , _("create a composite audiofile "
                              "as mixdown of all audio tracks in the "
                              "storyboard file and use the created composite audiofile "
                              "as input for encoding")
                          , NULL);
  g_signal_connect (G_OBJECT (button), "clicked",
                      G_CALLBACK (on_cme__button_stb_audio_clicked),
                      gpp);

  row++;

  /* the  storyboard helptext & parsing report label */
  label = gtk_label_new (_("Storyboardfiles are textfiles that are used to\n"
                           "assemble a video from a list of single images,\n"
                           "frameranges, videoclips, gif animations or audiofiles.\n"
                           "(see STORYBOARD_FILE_DOC.txt for details)"));
  gpp->cme__label_storyboard_helptext   = label;
  gtk_widget_show (label);
  gtk_table_attach (GTK_TABLE (table1), label, 0, 3, row, row+1,
                    (GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
                    (GtkAttachOptions) (0), 2, 6);
  gtk_label_set_justify (GTK_LABEL (label), GTK_JUSTIFY_FILL);
  gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);

  row++;

  /* the Monitor label */
  label = gtk_label_new (_("Monitor"));
  gtk_widget_hide (label);
  gtk_table_attach (GTK_TABLE (table1), label, 0, 1, row, row+1,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);
  gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);

  /* the Monitor checkbutton */
  checkbutton = gtk_check_button_new_with_label (_("Monitor Frames while Encoding"));
  gtk_widget_show (checkbutton);
  gtk_table_attach (GTK_TABLE (table1), checkbutton, 1, 2, row, row+1,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);
  gimp_help_set_help_data (checkbutton, _("Show each frame before passed to encoder"), NULL);
  g_signal_connect (G_OBJECT (checkbutton), "toggled",
                      G_CALLBACK (on_cme__checkbutton_enc_monitor_toggled),
                      gpp);


  row++;

  /* the Debug Flat File label */
  label = gtk_label_new (_("Debug\nFlat File:"));
  gtk_widget_show (label);
  gtk_table_attach (GTK_TABLE (table1), label, 0, 1, row, row+1,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);
  gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);

  /* the Debug Flat File entry */
  entry = gtk_entry_new ();
  gpp->cme__entry_debug_flat = entry;
  gtk_widget_show (entry);
  gtk_table_attach (GTK_TABLE (table1), entry, 1, 2, row, row+1,
                    (GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);
  gimp_help_set_help_data (entry, _("optional Save each composite frame to JPEG file, before it is passed to the encoder"), NULL);
  g_signal_connect (G_OBJECT (entry), "changed",
                      G_CALLBACK (on_cme__entry_debug_flat_changed),
                      gpp);


  row++;

  /* the Debug Multilayer File label */
  label = gtk_label_new (_("Debug\nMultilayer File:"));
  gtk_widget_show (label);
  gtk_table_attach (GTK_TABLE (table1), label, 0, 1, row, row+1,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);
  gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);

  /* the Debug Multilayer File entry */
  entry = gtk_entry_new ();
  gpp->cme__entry_debug_multi = entry;
  gtk_widget_show (entry);
  gtk_table_attach (GTK_TABLE (table1), entry, 1, 2, row, row+1,
                    (GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);
  gimp_help_set_help_data (entry, _("optional save each composite multilayer frame to XCF file, before flattening and executing macro"), NULL);
  g_signal_connect (G_OBJECT (entry), "changed",
                      G_CALLBACK (on_cme__entry_debug_multi_changed),
                      gpp);



  return(frame);
}  /* end p_create_encode_extras_frame */


/* ----------------------------------------
 * p_create_audiotool_cfg_frame
 * ----------------------------------------
 */
static GtkWidget*
p_create_audiotool_cfg_frame (GapCmeGlobalParams *gpp)
{
  GtkWidget *frame;
  GtkWidget *table;
  GtkWidget *button;
  GtkWidget *entry;
  GtkWidget *label;
  GtkWidget *hbox;
  gint       row;

  frame = gimp_frame_new (_("Configuration of external audiotool program"));


  table = gtk_table_new (4, 2, FALSE);
  gtk_widget_show (table);
  gtk_container_add (GTK_CONTAINER (frame), table);

  row = 0;

  /* the audiotool (sox) label */
  label = gtk_label_new (_("Audiotool:"));
  gtk_widget_show (label);
  gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
  gtk_table_attach (GTK_TABLE (table), label, 0, 1, row, row+1,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);

  /* the audiotool (sox)  entry */
  entry = gtk_entry_new ();
  gpp->cme__entry_sox = entry;
  gtk_widget_show (entry);
  gtk_table_attach (GTK_TABLE (table), entry, 1, 2, row, row+1,
                    (GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
                    (GtkAttachOptions) (0), 2, 2);
  gimp_help_set_help_data (entry, _("name of audiotool (something like sox with or without path)"), NULL);
  g_signal_connect (G_OBJECT (entry), "changed",
                      G_CALLBACK (on_cme__entry_sox_changed),
                      gpp);



  row++;

  /* the audiotool options (sox options) label */
  label = gtk_label_new (_("Options:"));
  gtk_widget_show (label);
  gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
  gtk_table_attach (GTK_TABLE (table), label, 0, 1, row, row+1,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);


  /* the audiotool options (sox options) entry */
  entry = gtk_entry_new ();
  gpp->cme__entry_sox_options           = entry;
  gtk_widget_show (entry);
  gtk_table_attach (GTK_TABLE (table), entry, 1, 2, row, row+1,
                    (GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
                    (GtkAttachOptions) (0), 2, 2);
  gimp_help_set_help_data (entry, _("Options to call the audiotool ($IN, $OUT $RATE are replaced)"), NULL);
  g_signal_connect (G_OBJECT (entry), "changed",
                      G_CALLBACK (on_cme__entry_sox_options_changed),
                      gpp);


  row++;

  /* the multitextline information label
   * (explains how params are passed to audiotool)
   */
  label =
    gtk_label_new (_("Configuration of an audiotool (like sox on UNIX).\n\n"
                     "$IN .. is replaced by the input audiofile\n"
                     "$OUT .. is replaced by audiofile with suffix  _tmp.wav\n"
                     "$RATE .. is replaced by samplerate in byte/sec\n"
                     "\n"
                     "This tool is called for audio conversions  if\n"
                     "a) the input audiofile is not WAV 16Bit format\n"
                     "b) Desired Samplerate does not match the\n"
                     "     rate in the .wav file"));
  gtk_widget_show (label);
  gtk_table_attach (GTK_TABLE (table), label, 0, 2, row, row+1,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 2, 2);
  gtk_widget_set_size_request (label, 300, -1);
  gtk_label_set_line_wrap (GTK_LABEL (label), TRUE);
  gtk_misc_set_alignment (GTK_MISC (label), 0.5, 0.5);


  row++;

  /* the hbox for LOAD/SAVE/DEFAULT buttons */

  hbox = gtk_hbox_new (TRUE, 0);
  gtk_widget_show (hbox);
  gtk_table_attach (GTK_TABLE (table), hbox, 0, 2, row, row+1,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (GTK_EXPAND), 0, 2);

  /* the Save button */
  button = gtk_button_new_with_label (_("Save"));
  gtk_widget_show (button);
  gtk_box_pack_start (GTK_BOX (hbox), button, FALSE, TRUE, 2);
  gimp_help_set_help_data (button, _("Save audiotool configuration to gimprc"), NULL);
  g_signal_connect (G_OBJECT (button), "clicked",
                      G_CALLBACK (on_cme__button_sox_save_clicked),
                      gpp);

  /* the Load button */
  button = gtk_button_new_with_label (_("Load"));
  gtk_widget_show (button);
  gtk_box_pack_start (GTK_BOX (hbox), button, FALSE, TRUE, 2);
  gimp_help_set_help_data (button, _("Load audiotool configuration from gimprc"), NULL);
  g_signal_connect (G_OBJECT (button), "clicked",
                      G_CALLBACK (on_cme__button_sox_load_clicked),
                      gpp);

  /* the Default button */
  button = gtk_button_new_with_label (_("Default"));
  gtk_widget_show (button);
  gtk_box_pack_start (GTK_BOX (hbox), button, FALSE, TRUE, 2);
  gimp_help_set_help_data (button, _("Set default audiotool configuration "), NULL);
  g_signal_connect (G_OBJECT (button), "clicked",
                      G_CALLBACK (on_cme__button_sox_default_clicked),
                      gpp);


  return(frame);
}  /* end p_create_audiotool_cfg_frame */


/* ----------------------------------------
 * p_create_audio_options_frame
 * ----------------------------------------
 */
static GtkWidget*
p_create_audio_options_frame (GapCmeGlobalParams *gpp)
{
  GtkWidget *frame;
  GtkWidget *table;
  GtkWidget *label;
  GtkObject *adj;
  GtkWidget *spinbutton;
  GtkWidget *combo;
  GtkWidget *button;
  GtkWidget *entry;
  gint       row;

  frame = gimp_frame_new (_("Audio Input"));


  table = gtk_table_new (6, 3, FALSE);
  gtk_widget_show (table);
  gtk_container_add (GTK_CONTAINER (frame), table);
  gtk_table_set_row_spacings (GTK_TABLE (table), 2);
  gtk_table_set_col_spacings (GTK_TABLE (table), 2);


  row = 0;

  /* the Audiofile label */
  label = gtk_label_new (_("Audiofile:"));
  gtk_widget_show (label);
  gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
  gtk_table_attach (GTK_TABLE (table), label, 0, 1, row, row+1,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);

  /* the Audiofile entry */
  entry = gtk_entry_new ();
  gpp->cme__entry_audio1                = entry;
  gtk_widget_show (entry);
  gtk_table_attach (GTK_TABLE (table), entry, 1, 2, row, row+1,
                    (GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);
  gimp_help_set_help_data (entry
                   , _("Name of audiofile (.wav 16 bit mono or stereo samples preferred). "
                       "Optionally you may select a textfile that contains a list "
                       "of file names referring to audio files. "
                       "Each of those audio files will be encoded as a separate audio track.")
                   , NULL)
                   ;
  g_signal_connect (G_OBJECT (entry), "changed",
                      G_CALLBACK (on_cme__entry_audio1_changed),
                      gpp);

  /* the Audiofile filebrowser button */
  button = gtk_button_new_with_label (_("..."));
  gtk_widget_show (button);
  gtk_table_attach (GTK_TABLE (table), button, 2, 3, 0, 1,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);
  gtk_widget_set_size_request (button, 80, -1);
  gimp_help_set_help_data (button, _("Select input audiofile via browser"), NULL);
  g_signal_connect (G_OBJECT (button), "clicked",
                      G_CALLBACK (on_cme__button_audio1_clicked),
                      gpp);


  row++;

  /* the audiofile information label */
  label = gtk_label_new (_("WAV, 16 Bit stereo, rate: 44100"));
  gpp->cme__label_aud1_info             = label;
  gtk_widget_show (label);
  gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
  gtk_table_attach (GTK_TABLE (table), label, 1, 2, row, row+1,
                    (GtkAttachOptions) (0),
                    (GtkAttachOptions) (0), 0, 0);

  /* the audiofile total playtime information label */
  label = gtk_label_new (_("00:00:000"));
  gpp->cme__label_aud1_time             = label;
  gtk_widget_show (label);
  gtk_table_attach (GTK_TABLE (table), label, 2, 3, row, row+1,
                    (GtkAttachOptions) (0),
                    (GtkAttachOptions) (0), 0, 0);


  row++;

  /* the Samplerate label */
  label = gtk_label_new (_("Samplerate:"));
  gtk_widget_show (label);
  gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
  gtk_table_attach (GTK_TABLE (table), label, 0, 1, row, row+1,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);


  /* the Samplerate spinbutton */
  adj = gtk_adjustment_new (44100, 1000, 100000, 10, 100, 0);
  spinbutton = gtk_spin_button_new (GTK_ADJUSTMENT (adj), 1, 0);
  gpp->cme__spinbutton_samplerate_adj   = adj;
  gpp->cme__spinbutton_samplerate       = spinbutton;
  gtk_widget_show (spinbutton);
  gtk_table_attach (GTK_TABLE (table), spinbutton, 1, 2, row, row+1,
                    (GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);
  gimp_help_set_help_data (spinbutton, _("Output samplerate in samples/sec"), NULL);
  g_signal_connect (G_OBJECT (spinbutton), "value_changed",
                      G_CALLBACK (on_cme__spinbutton_samplerate_changed),
                      gpp);

  /* the Samplerate combo */
  combo = gimp_int_combo_box_new (_(" 8k Phone"),       8000,
                                  _("11.025k"),        11025,
                                  _("12k Voice"),      12000,
                                  _("16k FM"),         16000,
                                  _("22.05k"),         22050,
                                  _("24k Tape"),       24000,
                                  _("32k HiFi"),       32000,
                                  _("44.1k CD"),       44100,
                                  _("48 k Studio"),    48000,
                                  NULL);


  gpp->cme__combo_outsamplerate    = combo;
  gtk_widget_show (combo);
  gtk_table_attach (GTK_TABLE (table), combo, 2, 3, row, row+1,
                    (GtkAttachOptions) (GTK_EXPAND),
                    (GtkAttachOptions) (0), 0, 0);
  gimp_help_set_help_data (combo, _("Select a commonly-used samplerate"), NULL);
  gimp_int_combo_box_connect (GIMP_INT_COMBO_BOX (combo),
                              44100,  /* initial gint value */
                              G_CALLBACK (on_cme__combo_outsamplerate),
                              gpp);


  row++;

  /* the Tmp audiofile label */
  label = gtk_label_new (_("Tmpfile:"));
  gtk_widget_show (label);
  gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
  gtk_table_attach (GTK_TABLE (table), label, 0, 1, row, row+1,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 4);

  /* the Tmp audiofilefilename label */
  label = gtk_label_new ("");
  gpp->cme__label_tmp_audfile           = label;
  gtk_widget_show (label);
  gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
  gtk_table_attach (GTK_TABLE (table), label, 1, 2, row, row+1,
                    (GtkAttachOptions) (0),
                    (GtkAttachOptions) (0), 0, 0);

  /* the convert Tmp audiofilefile button */
  button = gtk_button_new_with_label (_("Audioconvert"));
  gpp->cme__button_gen_tmp_audfile      = button;
  gtk_widget_show (button);
  gtk_table_attach (GTK_TABLE (table), button, 2, 3, row, row+1,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 2, 0);
  gimp_help_set_help_data (button, _("Convert audio input file to a temporary file\n"
                                     "and feed the temporary file to the selected encoder\n"
                                     "(the temporary file is deleted when encoding is done)"), NULL);
  g_signal_connect (G_OBJECT (button), "clicked",
                      G_CALLBACK (on_cme__button_gen_tmp_audfile_clicked),
                      gpp);

  row++;

  /* the Tmp audioinformation  label */
  label = gtk_label_new (_("WAV, 16 Bit stereo, rate: 44100"));
  gpp->cme__label_aud_tmp_info = label;
  gtk_widget_show (label);
  gtk_table_attach (GTK_TABLE (table), label, 1, 2, row, row+1,
                    (GtkAttachOptions) (0),
                    (GtkAttachOptions) (0), 0, 0);


  /* the Tmp audio playtime information  label */
  label = gtk_label_new (_("00:00:000"));
  gpp->cme__label_aud_tmp_time          = label;
  gtk_widget_show (label);
  gtk_table_attach (GTK_TABLE (table), label, 2, 3, row, row+1,
                    (GtkAttachOptions) (0),
                    (GtkAttachOptions) (0), 0, 0);

  row++;

  /* the resample general information label */
  label = gtk_label_new (_("\nNote:\n"
                           "if you set samplerate lower than\n"
                           "rate of the WAV file, you lose sound quality,\n"
                           "but higher samplerates can not improve the\n"
                           "quality of the original sound.")
                         );
  gtk_widget_show (label);
  gtk_table_attach (GTK_TABLE (table), label, 1, 2, row, row+1,
                    (GtkAttachOptions) (0),
                    (GtkAttachOptions) (0), 0, 0);
  gtk_label_set_line_wrap (GTK_LABEL (label), TRUE);


  return(frame);
}  /* end p_create_audio_options_frame */


/* ----------------------------------------
 * p_create_video_timing_table
 * ----------------------------------------
 */
static GtkWidget*
p_create_video_timing_table (GapCmeGlobalParams *gpp)
{
  GtkWidget *cme__table_times;
  GtkWidget *label;


  /* table for the video timing info labels */
  cme__table_times = gtk_table_new (2, 3, FALSE);

  gtk_table_set_row_spacings (GTK_TABLE (cme__table_times), 10);

  /* the timestamp of the first frame */
  label = gtk_label_new ("00:00:000");
  gpp->cme__label_fromtime              = label;
  gtk_widget_show (label);
  gtk_table_attach (GTK_TABLE (cme__table_times), label, 0, 1, 0, 1,
                    (GtkAttachOptions) (0),
                    (GtkAttachOptions) (0), 0, 0);


  /* the video timing information labels */
  label = gtk_label_new (_("Videotime:"));
  gtk_widget_show (label);
  gtk_table_attach (GTK_TABLE (cme__table_times), label, 1, 2, 0, 1,
                    (GtkAttachOptions) (0),
                    (GtkAttachOptions) (0), 0, 0);
  gtk_misc_set_padding (GTK_MISC (label), 7, 0);


  label = gtk_label_new ("00:00:000");
  gpp->cme__label_totaltime = label;
  gtk_widget_show (label);
  gtk_table_attach (GTK_TABLE (cme__table_times), label, 2, 3, 0, 1,
                    (GtkAttachOptions) (0),
                    (GtkAttachOptions) (0), 0, 0);



  /* the timestamp of the last frame */
  label = gtk_label_new (_("00:00:000"));
  gpp->cme__label_totime = label;
  gtk_widget_show (label);
  gtk_table_attach (GTK_TABLE (cme__table_times), label, 0, 1, 1, 2,
                    (GtkAttachOptions) (0),
                    (GtkAttachOptions) (0), 0, 0);

  /* the audio timing information labels */

  label = gtk_label_new (_("Audiotime:"));
  gtk_widget_show (label);
  gtk_table_attach (GTK_TABLE (cme__table_times), label, 1, 2, 1, 2,
                    (GtkAttachOptions) (0),
                    (GtkAttachOptions) (0), 0, 0);

  label = gtk_label_new (_("00:00:000"));
  gpp->cme__label_aud0_time = label;
  gtk_widget_show (label);
  gtk_table_attach (GTK_TABLE (cme__table_times), label, 2, 3, 1, 2,
                    (GtkAttachOptions) (0),
                    (GtkAttachOptions) (0), 0, 0);


  return(cme__table_times);
}  /* end p_create_video_timing_table */


/* ----------------------------------------
 * p_create_video_options_frame
 * ----------------------------------------
 */
static GtkWidget*
p_create_video_options_frame (GapCmeGlobalParams *gpp)
{
  GtkWidget *frame;
  GtkWidget *cme__table1;
  GtkWidget *cme__table_times;
  GtkWidget *label;
  GtkObject *adj;
  GtkWidget *spinbutton;
  GtkWidget *combo;
  GtkWidget *button;
  gint       l_row;


  frame = gimp_frame_new (_("Video Encode Options"));



  cme__table1 = gtk_table_new (10, 3, FALSE);

  gtk_widget_show (cme__table1);
  gtk_container_add (GTK_CONTAINER (frame), cme__table1);
  gtk_container_set_border_width (GTK_CONTAINER (cme__table1), 5);
  gtk_table_set_row_spacings (GTK_TABLE (cme__table1), 2);
  gtk_table_set_col_spacings (GTK_TABLE (cme__table1), 2);


  l_row = 0;
  p_create_input_mode_widgets(cme__table1, l_row, 0, gpp);

  l_row++;

  /* the from_frame label */
  label = gtk_label_new (_("From Frame:"));
  gpp->cme__label_from = label;
  gtk_widget_show (label);
  gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
  gtk_table_attach (GTK_TABLE (cme__table1), label, 0, 1, l_row, l_row+1,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);


  /* the from_frame spinbutton */
  adj = gtk_adjustment_new (1, 0, 100000, 1, 10, 0);
  spinbutton = gtk_spin_button_new (GTK_ADJUSTMENT (adj), 1, 0);
  gpp->cme__spinbutton_from_adj         = adj;
  gpp->cme__spinbutton_from             = spinbutton;
  gtk_widget_show (spinbutton);
  gtk_table_attach (GTK_TABLE (cme__table1), spinbutton, 1, 2, l_row, l_row+1,
                    (GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);
  gimp_help_set_help_data (spinbutton, _("Start encoding at this frame"), NULL);
  g_signal_connect (G_OBJECT (spinbutton), "value_changed",
                      G_CALLBACK (on_cme__spinbutton_from_changed),
                      gpp);

  /* subtable for the video timing info labels (takes 2 rows) */
  cme__table_times = p_create_video_timing_table (gpp);
  gtk_widget_show (cme__table_times);
  gtk_table_attach (GTK_TABLE (cme__table1), cme__table_times, 2, 3, l_row, l_row+2,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (GTK_FILL), 0, 0);


  l_row++;

  /* the to_frame label */
  label = gtk_label_new (_("To Frame:"));
  gpp->cme__label_to                    = label;
  gtk_widget_show (label);
  gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
  gtk_table_attach (GTK_TABLE (cme__table1), label, 0, 1, l_row, l_row+1,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);

  /* the to_frame spinbutton */
  adj = gtk_adjustment_new (1, 0, 100000, 1, 10, 0);
  spinbutton = gtk_spin_button_new (GTK_ADJUSTMENT (adj), 1, 0);
  gpp->cme__spinbutton_to_adj           = adj;
  gpp->cme__spinbutton_to               = spinbutton;
  gtk_widget_show (spinbutton);
  gtk_table_attach (GTK_TABLE (cme__table1), spinbutton, 1, 2, l_row, l_row+1,
                    (GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);
  gimp_help_set_help_data (spinbutton, _("Stop encoding at this frame"), NULL);
  g_signal_connect (G_OBJECT (spinbutton), "value_changed",
                      G_CALLBACK (on_cme__spinbutton_to_changed),
                      gpp);


  l_row++;

  /* the width label */
  label = gtk_label_new (_("Width:"));
  gtk_widget_show (label);
  gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
  gtk_table_attach (GTK_TABLE (cme__table1), label, 0, 1, l_row, l_row+1,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);


  /* the width spinbutton */
  adj = gtk_adjustment_new (10, 10, 10000, 1, 10, 0);
  spinbutton = gtk_spin_button_new (GTK_ADJUSTMENT (adj), 1, 0);
  gpp->cme__spinbutton_width_adj        = adj;
  gpp->cme__spinbutton_width            = spinbutton;
  gtk_widget_show (spinbutton);
  gtk_table_attach (GTK_TABLE (cme__table1), spinbutton, 1, 2, l_row, l_row+1,
                    (GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);
  gimp_help_set_help_data (spinbutton, _("Width of the output video (pixels)"), NULL);
  g_signal_connect (G_OBJECT (spinbutton), "value_changed",
                      G_CALLBACK (on_cme__spinbutton_width_changed),
                      gpp);


  /* the Frame width/height scale combo (for picking common used video sizes) */
  combo = gimp_int_combo_box_new (_("keep"),                GAP_CME_STANDARD_SIZE_KEEP,
                                  _("Framesize (1:1)"),     GAP_CME_STANDARD_SIZE_IMAGE,
                                  _("320x240 NTSC"),        GAP_CME_STANDARD_SIZE_320x240,
                                  _("320x288 PAL"),         GAP_CME_STANDARD_SIZE_320x288,
                                  _("640x480"),             GAP_CME_STANDARD_SIZE_640x480,
                                  _("720x480 NTSC"),        GAP_CME_STANDARD_SIZE_720x480,
                                  _("720x576 PAL"),         GAP_CME_STANDARD_SIZE_720x576,
                                  _("1280x720 HD"),         GAP_CME_STANDARD_SIZE_1280x720,
                                  _("1920x1080 FullHD"),    GAP_CME_STANDARD_SIZE_1920x1080,
                                  _("1920x1088 EosHD"),     GAP_CME_STANDARD_SIZE_1920x1088,
                                  NULL);

  gpp->cme__combo_scale            = combo;
  gtk_widget_show (combo);
  gtk_table_attach (GTK_TABLE (cme__table1), combo, 2, 3, l_row, l_row+1,
                    (GtkAttachOptions) (0),
                    (GtkAttachOptions) (0), 0, 0);
  gtk_widget_set_size_request (combo, 160, -1);
  gimp_help_set_help_data (combo, _("Scale width/height to common size"), NULL);
  gimp_int_combo_box_connect (GIMP_INT_COMBO_BOX (combo),
                              GAP_CME_STANDARD_SIZE_IMAGE,  /* initial gint value */
                              G_CALLBACK (on_cme__combo_scale),
                              gpp);



  l_row++;

  /* the height label */
  label = gtk_label_new (_("Height:"));
  gtk_widget_show (label);
  gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
  gtk_table_attach (GTK_TABLE (cme__table1), label, 0, 1, l_row, l_row+1,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);

  /* the height spinbutton */
  adj = gtk_adjustment_new (10, 10, 10000, 1, 10, 0);
  spinbutton = gtk_spin_button_new (GTK_ADJUSTMENT (adj), 1, 0);
  gpp->cme__spinbutton_height_adj       = adj;
  gpp->cme__spinbutton_height           = spinbutton;
  gtk_widget_show (spinbutton);
  gtk_table_attach (GTK_TABLE (cme__table1), spinbutton, 1, 2, l_row, l_row+1,
                    (GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);
  gimp_help_set_help_data (spinbutton, _("Height of the output video (pixels)"), NULL);
  g_signal_connect (G_OBJECT (spinbutton), "value_changed",
                      G_CALLBACK (on_cme__spinbutton_height_changed),
                      gpp);



  l_row++;

  /* the Framerate lable */
  label = gtk_label_new (_("Framerate:"));
  gtk_widget_show (label);
  gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
  gtk_table_attach (GTK_TABLE (cme__table1), label, 0, 1, l_row, l_row+1,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);


  /* the framerate spinbutton */
  adj = gtk_adjustment_new (24, 1, 100, 0.1, 1, 0);
  spinbutton = gtk_spin_button_new (GTK_ADJUSTMENT (adj), 1, 2);
  gpp->cme__spinbutton_framerate_adj    = adj;
  gpp->cme__spinbutton_framerate        = spinbutton;
  gtk_widget_show (spinbutton);
  gtk_table_attach (GTK_TABLE (cme__table1), spinbutton, 1, 2, l_row, l_row+1,
                    (GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);
  gimp_help_set_help_data (spinbutton, _("Framerate of the output video (frames/sec)"), NULL);
  g_signal_connect (G_OBJECT (spinbutton), "value_changed",
                      G_CALLBACK (on_cme__spinbutton_framerate_changed),
                      gpp);


  /* the framerate combo (to select common used video framerates) */
  combo = gimp_int_combo_box_new (_("keep"),        GAP_CME_STANDARD_FRAMERATE_KEEP,
                                  _("original"),    GAP_CME_STANDARD_FRAMERATE_00_UNCHANGED,
                                  "23.98",          GAP_CME_STANDARD_FRAMERATE_01_23_98,
                                  "24",             GAP_CME_STANDARD_FRAMERATE_02_24,
                                  "25",             GAP_CME_STANDARD_FRAMERATE_03_25,
                                  "29.97",          GAP_CME_STANDARD_FRAMERATE_04_29_97,
                                  "30",             GAP_CME_STANDARD_FRAMERATE_05_30,
                                  "50",             GAP_CME_STANDARD_FRAMERATE_06_50,
                                  "59.94",          GAP_CME_STANDARD_FRAMERATE_07_59_94,
                                  "60",             GAP_CME_STANDARD_FRAMERATE_08_60,
                                  "1",              GAP_CME_STANDARD_FRAMERATE_09_1,
                                  "5",              GAP_CME_STANDARD_FRAMERATE_10_5,
                                  "10",             GAP_CME_STANDARD_FRAMERATE_11_10,
                                  "12",             GAP_CME_STANDARD_FRAMERATE_12_12,
                                  "15",             GAP_CME_STANDARD_FRAMERATE_13_15,
                                   NULL);

  gpp->cme__combo_framerate        = combo;
  gtk_widget_show (combo);
  gtk_table_attach (GTK_TABLE (cme__table1), combo, 2, 3, l_row, l_row+1,
                    (GtkAttachOptions) (0),
                    (GtkAttachOptions) (0), 0, 0);
  gtk_widget_set_size_request (combo, 160, -1);
  gimp_help_set_help_data (combo, _("Set framerate"), NULL);
  gimp_int_combo_box_connect (GIMP_INT_COMBO_BOX (combo),
                              GAP_CME_STANDARD_FRAMERATE_00_UNCHANGED,  /* initial gint value */
                              G_CALLBACK (on_cme__combo_framerate),
                              gpp);


  l_row++;


  /* the Videonorm label */
  label = gtk_label_new (_("Videonorm:"));
  gtk_widget_show (label);
  gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
  gtk_table_attach (GTK_TABLE (cme__table1), label, 0, 1, l_row, l_row+1,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);

  /* the Videonorm combo */
  combo = gimp_int_combo_box_new (_("NTSC"),       VID_FMT_NTSC,
                                  _("PAL"),        VID_FMT_PAL,
                                  _("SECAM"),      VID_FMT_SECAM,
                                  _("MAC"),        VID_FMT_MAC,
                                  _("COMP"),       VID_FMT_COMP,
                                  _("undefined"),  VID_FMT_UNDEF,
                                  NULL);


  gtk_widget_show (combo);
  gtk_table_attach (GTK_TABLE (cme__table1), combo, 2, 3, l_row, l_row+1,
                    (GtkAttachOptions) (0),
                    (GtkAttachOptions) (0), 0, 0);
  gtk_widget_set_size_request (combo, 160, -1);
  gimp_help_set_help_data (combo, _("Select videonorm"), NULL);
  gimp_int_combo_box_connect (GIMP_INT_COMBO_BOX (combo),
                              VID_FMT_NTSC,  /* initial gint value */
                              G_CALLBACK (on_cme__combo_vid_norm),
                              gpp);


  l_row++;

  /* the videoencoder label */
  label = gtk_label_new (_("Encoder:"));
  gtk_widget_show (label);
  gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
  gtk_table_attach (GTK_TABLE (cme__table1), label, 0, 1, l_row, l_row+1,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);

  /* the parameters button (invokes videoencoder specific GUI dialog) */
  button = gtk_button_new_with_label (_("Parameters"));
  gpp->cme__button_params               = button;
  gtk_widget_show (button);
  gtk_table_attach (GTK_TABLE (cme__table1), button, 1, 2, l_row, l_row+1,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);
  gimp_help_set_help_data (button, _("Edit encoder specific parameters"), NULL);
  g_signal_connect (G_OBJECT (button), "clicked",
                      G_CALLBACK (on_cme__button_params_clicked),
                      gpp);




  /* the videoencoder combo */
  combo = g_object_new (GIMP_TYPE_INT_COMBO_BOX, NULL);
  gpp->cme__combo_encodername      = combo;
  gtk_widget_show (combo);
  gtk_table_attach (GTK_TABLE (cme__table1), combo, 2, 3, l_row, l_row+1,
                    (GtkAttachOptions) (0),
                    (GtkAttachOptions) (0), 0, 0);
  gtk_widget_set_size_request (combo, 160, -1);
  gimp_help_set_help_data (combo, _("Select video encoder plugin"), NULL);

  /* the  videoencoder combo items are set later by query the PDB
   * for all registered video encoder plug-ins
   */


  l_row++;

  /* videoencoder short description label */
  label = gtk_label_new ("");
  gpp->cme__short_description           = label;
  gtk_widget_show (label);
  gtk_table_attach (GTK_TABLE (cme__table1), label, 1, 3, l_row, l_row+1,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);
  gtk_label_set_line_wrap (GTK_LABEL (label), TRUE);
  gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);


  return(frame);
}  /* end p_create_video_options_frame */


/* ----------------------------------------
 * p_call_encoder_procedure
 * ----------------------------------------
 */
static gint
p_call_encoder_procedure(GapCmeGlobalParams *gpp)
{
  GimpParam* l_params;
  gint   l_retvals;
  gint             l_nparams;
  gint             l_nreturn_vals;
  GimpPDBProcType   l_proc_type;
  gchar            *l_proc_blurb;
  gchar            *l_proc_help;
  gchar            *l_proc_author;
  gchar            *l_proc_copyright;
  gchar            *l_proc_date;
  GimpParamDef    *l_paramdef;
  GimpParamDef    *l_return_vals;
  char *l_msg;
  gint  l_use_encoderspecific_params;
  gint  l_rc;
  gchar            *l_16bit_wav_file;
  gint32           dummy_layer_id;

  l_rc = -1;

  l_use_encoderspecific_params = 0;  /* run with default values */


  if(gpp->val.ecp_sel.vid_enc_plugin[0] == '\0')
  {
     gap_file_printf("p_call_encoder_procedure: No encoder available (exit)\n");
     return -1;
  }

  if(gpp->val.ecp_sel.gui_proc[0] != '\0')
  {
    l_use_encoderspecific_params = 1;  /* run with encoder specific values */
  }

  if(gap_debug)
  {
     gap_file_printf("p_call_encoder_procedure %s: START\n", gpp->val.ecp_sel.vid_enc_plugin);
     gap_file_printf("  videoname: %s\n", gpp->val.videoname);
     gap_file_printf("  audioname1: %s\n", gpp->val.audioname1);
     gap_file_printf("  basename: %s\n", gpp->ainfo.basename);
     gap_file_printf("  extension: %s\n", gpp->ainfo.extension);
     gap_file_printf("  range_from: %d\n", (int)gpp->val.range_from);
     gap_file_printf("  range_to: %d\n", (int)gpp->val.range_to);
     gap_file_printf("  framerate: %f\n", (float)gpp->val.framerate);
     gap_file_printf("  samplerate: %d\n", (int)gpp->val.samplerate);
     gap_file_printf("  wav_samplerate: %d\n", (int)gpp->val.wav_samplerate1);
     gap_file_printf("  vid_width: %d\n", (int)gpp->val.vid_width);
     gap_file_printf("  vid_height: %d\n", (int)gpp->val.vid_height);
     gap_file_printf("  vid_format: %d\n", (int)gpp->val.vid_format);
     gap_file_printf("  image_ID: %d\n", (int)gpp->val.image_ID);
     gap_file_printf("  l_use_encoderspecific_params: %d\n", (int)l_use_encoderspecific_params);
     gap_file_printf("  filtermacro_file: %s\n", gpp->val.filtermacro_file);
     gap_file_printf("  storyboard_file: %s\n", gpp->val.storyboard_file);
     gap_file_printf("  input_mode: %d\n", gpp->val.input_mode);
  }

  if(FALSE == gimp_procedural_db_proc_info (gpp->val.ecp_sel.vid_enc_plugin,
                          &l_proc_blurb,
                          &l_proc_help,
                          &l_proc_author,
                          &l_proc_copyright,
                          &l_proc_date,
                          &l_proc_type,
                          &l_nparams,
                          &l_nreturn_vals,
                          &l_paramdef,
                          &l_return_vals))
  {
     l_msg = g_strdup_printf(_("Required Plugin %s not available"), gpp->val.ecp_sel.vid_enc_plugin);
     if(gpp->val.run_mode == GIMP_RUN_INTERACTIVE)
     {
       g_message("%s", l_msg);
     }
     g_free(l_msg);
     return -1;
  }

  l_16bit_wav_file = &gpp->val.audioname1[0];
  if(gpp->val.tmp_audfile[0] != '\0')
  {
     l_16bit_wav_file = &gpp->val.tmp_audfile[0];
  }


  /* generic call of GAP video encoder plugin */
  dummy_layer_id = gap_image_get_any_layer(gpp->val.image_ID);
  l_params = gimp_run_procedure (gpp->val.ecp_sel.vid_enc_plugin,
                     &l_retvals,
                     GIMP_PDB_INT32,  gpp->val.run_mode,
                     GIMP_PDB_IMAGE,  gpp->val.image_ID,
                     GIMP_PDB_DRAWABLE, dummy_layer_id,
                     GIMP_PDB_STRING, gpp->val.videoname,
                     GIMP_PDB_INT32,  gpp->val.range_from,
                     GIMP_PDB_INT32,  gpp->val.range_to,
                     GIMP_PDB_INT32,  gpp->val.vid_width,
                     GIMP_PDB_INT32,  gpp->val.vid_height,
                     GIMP_PDB_INT32,  gpp->val.vid_format,
                     GIMP_PDB_FLOAT,  gpp->val.framerate,
                     GIMP_PDB_INT32,  gpp->val.samplerate,
                     GIMP_PDB_STRING, l_16bit_wav_file,
                     GIMP_PDB_INT32,  l_use_encoderspecific_params,
                     GIMP_PDB_STRING, gpp->val.filtermacro_file,
                     GIMP_PDB_STRING, gpp->val.storyboard_file,
                     GIMP_PDB_INT32,  gpp->val.input_mode,
                     GIMP_PDB_INT32,  gpp->encStatus.master_encoder_id,
                     GIMP_PDB_END);
  if(l_params[0].data.d_status == GIMP_PDB_SUCCESS)
  {
    l_rc = 0;
  }
  g_free(l_params);
  
  if(gap_debug)
  {
     gap_file_printf("p_call_encoder_procedure %s: DONE\n", gpp->val.ecp_sel.vid_enc_plugin);
  }
  
  
  if(l_rc < 0)
  {
     l_msg = g_strdup_printf(_("Call of Required Plugin %s failed"), gpp->val.ecp_sel.vid_enc_plugin);
     if(gpp->val.run_mode == GIMP_RUN_INTERACTIVE)
     {
       g_message("%s", l_msg);
     }
     g_free(l_msg);
  }


  return (l_rc);
}  /* end p_call_encoder_procedure */



static void
p_set_label_to_numeric_value(GtkWidget *label, gint32 value)
{
  if(label != NULL)
  {
    char *buffer;
    buffer = g_strdup_printf("%6d", value);
    /* repeat the right alingnment of the label
     * (without this workaround my gtk version 2.10.14 shows just
     * the highest digit of the number, probably because the size at creation time
     * was only one character)
     *
     */
    gtk_misc_set_alignment (GTK_MISC (label), 1.0, 0.5);
    gtk_label_set_text(GTK_LABEL(label), buffer);
    gtk_misc_set_alignment (GTK_MISC (label), 1.0, 0.5);
    g_free(buffer);
  }
}

/* ----------------------------------------
 * gap_cme_gui_update_encoder_status
 * ----------------------------------------
 * called via polling timer while the encoder plug-in
 * is running (as separete process).
 * The displyed information is sent by the running encoder process
 * and received here (in the master videoencoder GUI process)
 * via procedure gap_gve_misc_get_master_encoder_progress
 */
void
gap_cme_gui_update_encoder_status(GapCmeGlobalParams *gpp)
{
  if(gap_debug)
  {
    gap_file_printf("  gap_cme_gui_update_encoder_status -- frames_processed:%d\n"
      , gpp->encStatus.frames_processed
      );
  }

  if (gpp)
  {
    GtkWidget  *pbar;
    GtkWidget *label;
    
    gap_gve_misc_get_master_encoder_progress(&gpp->encStatus);

    gtk_widget_show(gpp->cme__encoder_status_frame);

 
    p_set_label_to_numeric_value(gpp->cme__label_enc_stat_frames_total, gpp->encStatus.total_frames);
    p_set_label_to_numeric_value(gpp->cme__label_enc_stat_frames_done, gpp->encStatus.frames_processed);
    p_set_label_to_numeric_value(gpp->cme__label_enc_stat_frames_encoded, gpp->encStatus.frames_encoded);
    p_set_label_to_numeric_value(gpp->cme__label_enc_stat_frames_copied_lossless, gpp->encStatus.frames_copied_lossless);
    
    

    pbar = gpp->cme__progressbar_status;
    if(pbar)
    {
      gdouble l_progress;
      char *l_msg;
      
      switch (gpp->encStatus.current_pass)
      {
        case 1:
          l_progress = CLAMP(
                       (gdouble)gpp->encStatus.frames_processed 
                        / (gdouble)(MAX(1.0, 2.0 * gpp->encStatus.total_frames))
                      , 0.0
                      , 1.0
                      );
          l_msg = g_strdup_printf(_("Video encoding %d of %d frames done, PASS 1 of 2")
                             , gpp->encStatus.frames_processed
                             , gpp->encStatus.total_frames
                             );
          break;
        case 2:
          l_progress = CLAMP(
                     (gdouble)(gpp->encStatus.frames_processed + gpp->encStatus.total_frames)
                      / (gdouble)(MAX(1.0, 2.0 * gpp->encStatus.total_frames))
                      , 0.0
                      , 1.0
                      );
          l_msg = g_strdup_printf(_("Video encoding %d of %d frames done, PASS 2 of 2")
                             , gpp->encStatus.frames_processed
                             , gpp->encStatus.total_frames
                             );
          break;
        default: 
          /* current_pass is 0 for single pass encoders */
          l_progress = CLAMP(
                      (gdouble)gpp->encStatus.frames_processed
                      / (gdouble)(MAX(1.0, gpp->encStatus.total_frames))
                      , 0.0
                      , 1.0
                      );
          l_msg = g_strdup_printf(_("Video encoding %d of %d frames done")
                             , gpp->encStatus.frames_processed
                             , gpp->encStatus.total_frames
                             );
          break;
      }

      gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR (pbar), l_progress);
      gtk_progress_bar_set_text(GTK_PROGRESS_BAR(pbar), l_msg);
      g_free(l_msg);

      if (gpp->encStatus.pidOfRunningEncoder != 0)
      {
        if (!gap_base_is_pid_alive(gpp->encStatus.pidOfRunningEncoder))
        {
          if (gpp->video_encoder_run_state ==  GAP_CME_ENC_RUN_STATE_RUNNING)
          {
            gpp->video_encoder_run_state =  GAP_CME_ENC_RUN_STATE_FINISHED;
          }
          gtk_progress_bar_set_text(GTK_PROGRESS_BAR(pbar), _("ENCODER process has terminated"));
          return;
        }
      }
    }


    label = gpp->cme__label_enc_time_elapsed;
    if((label != NULL)
    && (gpp->video_encoder_run_state ==  GAP_CME_ENC_RUN_STATE_RUNNING))
    {
      time_t l_currentUtcTimeInSecs;
      gint32 l_secsElapsed;
      char *buffer;

      l_currentUtcTimeInSecs = time(NULL);
      l_secsElapsed = l_currentUtcTimeInSecs - gpp->encoder_started_on_utc_seconds;
      
      buffer = g_strdup_printf("%d:%02d:%02d"
                              , (int)l_secsElapsed / 3600
                              , (int)(l_secsElapsed / 60) % 60
                              , (int)l_secsElapsed % 60
                              );
      gtk_label_set_text(GTK_LABEL(label), buffer);
      g_free(buffer);
    }


//    if (gpp->video_encoder_run_state ==  GAP_CME_ENC_RUN_STATE_RUNNING)
//    {
//      /* detect if encoder has finished (required for asynchron calls since GIMP-2.6)  */
//      if(gpp->encStatus.frames_processed >= gpp->encStatus.total_frames)
//      {
//        if(gap_debug)
//        {
//          gap_file_printf("  gap_cme_gui_update_encoder_status -- detected encoder FINISHED via progress\n"
//            , gpp->encStatus.frames_processed
//            );
//        }
//        gpp->video_encoder_run_state =  GAP_CME_ENC_RUN_STATE_FINISHED;
//      }
//    }
  }
}  /* end gap_cme_gui_update_encoder_status */


/* ----------------------------------------
 * gap_cme_gui_start_video_encoder
 * ----------------------------------------
 * start the selected video encoder plug-in
 */
gint32
gap_cme_gui_start_video_encoder(GapCmeGlobalParams *gpp)
{
   gint32     l_rc;
   char *l_tmpname;
   gint32 l_tmp_image_id;

   l_tmpname = NULL;
   l_tmp_image_id = -1;

   /* cheks for encoding a multilayer image as video
    * (this may require save of a temporary imge before we can start the encoder plugin)
    */
   if((gpp->ainfo.last_frame_nr - gpp->ainfo.first_frame_nr == 0)
   && (gpp->val.storyboard_file[0] == '\0'))
   {
     char *l_current_name;

     if((strcmp(gpp->ainfo.extension, ".xcf") == 0)
     || (strcmp(gpp->ainfo.extension, ".xjt") == 0))
     {
       /* for xcf and xjt just save without making a temp copy */
       l_tmpname = gimp_image_get_filename(gpp->val.image_ID);
     }
     else
     {
       /* prepare encoder params to run the encoder on the (saved) duplicate of the image */
       g_snprintf(gpp->ainfo.basename, sizeof(gpp->ainfo.basename), "%s", l_tmpname);

       /* save a temporary copy of the image */
       l_tmp_image_id = gimp_image_duplicate(gpp->val.image_ID);

       /* l_tmpname = gimp_temp_name("xcf"); */
       l_current_name = gimp_image_get_filename(gpp->val.image_ID);
       l_tmpname = g_strdup_printf("%s_temp_copy.xcf", l_current_name);
       gimp_image_set_filename (l_tmp_image_id, l_tmpname);
       gpp->ainfo.extension[0] = '\0';
       gpp->val.image_ID = l_tmp_image_id;
       g_free(l_current_name);
     }

     gimp_file_save(GIMP_RUN_NONINTERACTIVE
                   , gpp->val.image_ID
                   , 1 /* dummy layer_id */
                   ,l_tmpname
                   ,l_tmpname
                   );

   }

   gap_gve_misc_initGapGveMasterEncoderStatus(&gpp->encStatus
       , gap_base_getpid() /* master_encoder_id */
       , abs(gpp->val.range_to - gpp->val.range_from) + 1   /* total_frames */
       );

   /* ------------------------------------------- HERE WE GO, start the selected Video Encoder ----
    * the encoder will run as separate process, started by the GIMP core.
    * note that any further calls of GIMP core procedures .. from another thread ...
    * will be blocked by the GIMP core while the encoder process is running.
    */
   l_rc = p_call_encoder_procedure(gpp);
   
   gpp->video_encoder_run_state =  GAP_CME_ENC_RUN_STATE_FINISHED;

   if(l_tmp_image_id >= 0)
   {
     gap_image_delete_immediate(l_tmp_image_id);
     g_remove(l_tmpname);
   }


   if(l_tmpname)
   {
     g_free(l_tmpname);
   }

   return (l_rc);
}  /* end gap_cme_gui_start_video_encoder */


/* ----------------------------------------
 * gap_cme_encoder_worker_thread
 * ----------------------------------------
 */
gpointer
gap_cme_encoder_worker_thread(gpointer data)
{
  GapCmeGlobalParams *gpp;

  gpp = (GapCmeGlobalParams *) data;

  if(gap_debug)
  {
    gap_file_printf("THREAD: gap_cme_encoder_worker_thread &gpp: %ld\n"
        , (long)gpp
        );
  }

  gap_cme_gui_start_video_encoder(gpp);

  if(gap_debug)
  {
    gap_file_printf("THREAD gap_cme_encoder_worker_thread TERMINATING: %ld\n"
        , (long)gpp->val.gui_proc_thread
        );
  }

  gpp->productive_encoder_thread = NULL;

  return (NULL);
}  /* end gap_cme_encoder_worker_thread */


/* ----------------------------------------
 * gap_cme_gui_start_video_encoder
 * ----------------------------------------
 * start the selected video encoder plug-in
 */
gint32
gap_cme_gui_start_video_encoder_as_thread(GapCmeGlobalParams *gpp)
{
  gboolean joinable;
  if(gpp->ecp == NULL)
  {
    return -1;
  }

#ifdef GAP_ENC_USE_GTHREAD
  if(gpp->productive_encoder_thread != NULL)
  {
    return -1;
  }

  /* start a thread for asynchron PDB call of the gui_ procedure
   */
  if(gap_debug)
  {
    gap_file_printf("MASTER: Before g_thread_create encode video worker\n");
  }

  joinable = TRUE;
  gpp->productive_encoder_thread =
      g_thread_create((GThreadFunc)gap_cme_encoder_worker_thread
                     , gpp  /* data */
                     , joinable
                     , NULL  /* GError **error (NULL dont report errors) */
                     );

  if(gap_debug)
  {
    gap_file_printf("MASTER: After g_thread_create encode video worker\n");
  }
#else
  /* if threads are not used simply call the procedure
   * (the common GUI window is not refreshed until the called gui_proc ends)
   */
  gap_cme_encoder_worker_thread(gpp);

#endif

  return 0;
}  /* end gap_cme_gui_start_video_encoder_as_thread */






/* ----------------------------------------
 * gap_cme_gui_master_encoder_dialog
 * ----------------------------------------
 * common GUI dialog
 *
 *   return  0 .. OK
 *          -1 .. in case of Error or cancel
 */
gint32
gap_cme_gui_master_encoder_dialog(GapCmeGlobalParams *gpp)
{
  t_global_stb    *gstb;
  GapLibTypeInputRange l_rangetype;

  if(gap_debug)
  {
    gap_file_printf("gap_cme_gui_master_encoder_dialog: Start\n");
  }

#ifdef GAP_GUI_USE_GTHREAD
  /* check and init thread system */
  gap_base_thread_init();
  gdk_threads_init ();
  gdk_threads_enter ();
#endif
  gstb = &global_stb;
  gstb->stb_job_done = FALSE;
  gstb->vidhand_open_ok = FALSE;
  gstb->poll_timertag = -1;
  gstb->total_stroyboard_frames = 0;
  gstb->aud_total_sec = 0.0;

  gimp_ui_init ("gap_master_video_encoder", FALSE);
  gap_stock_init();

  l_rangetype = gpp->val.input_mode;

  /* debug: disable debug save of encoded frames */
  gimp_set_data( GAP_VID_ENC_SAVE_MULTILAYER, "\0\0", 2);
  gimp_set_data( GAP_VID_ENC_SAVE_FLAT, "\0\0", 2);
  gimp_set_data( GAP_VID_ENC_MONITOR, "FALSE", strlen("FALSE") +1);


  /* ---------- dialog ----------*/
  gap_gve_misc_initGapGveMasterEncoderStatus(&gpp->encStatus
       , gap_base_getpid()  /* master_encoder_id */
       , 1         /* total_frames */
       );
  gap_gve_misc_set_master_encoder_cancel_request(&gpp->encStatus, FALSE);
  gpp->video_encoder_run_state = GAP_CME_ENC_RUN_STATE_READY;
  gpp->productive_encoder_thread = NULL;
  gpp->productive_encoder_timertag = -1;
  gpp->encoder_status_poll_timertag = -1;
  gpp->storyboard_create_composite_audio = FALSE;
  gpp->fsv__fileselection = NULL;
  gpp->fsb__fileselection = NULL;
  gpp->fsa__fileselection = NULL;
  gpp->fss__fileselection = NULL;
  gpp->ow__dialog_window = NULL;
  gpp->ecp = pdb_find_video_encoders();

  if(gap_debug)
  {
    gap_file_printf("gap_cme_gui_master_encoder_dialog: Before p_create_shell_window\n");
  }

  gpp->shell_window = p_create_shell_window (gpp);

  if(gap_debug)
  {
    gap_file_printf("gap_cme_gui_master_encoder_dialog: After p_create_shell_window\n");
  }

  p_init_shell_window_widgets(gpp);
  gtk_widget_show (gpp->shell_window);
  
  if(l_rangetype == GAP_RNGTYPE_STORYBOARD)
  {
    if((gpp->val.storyboard_file[0] != '\0')
    && (gpp->cme__entry_stb != NULL))
    {
      gtk_entry_set_text(GTK_ENTRY(gpp->cme__entry_stb), gpp->val.storyboard_file);
    }
  }

  gpp->val.run = 0;
  gtk_main ();
#ifdef GAP_GUI_USE_GTHREAD
  gdk_threads_leave ();
#endif

  if(gap_debug)
  {
    gap_file_printf("A F T E R gtk_main run:%d\n", (int)gpp->val.run);
  }

  gpp->shell_window = NULL;

  /* is the encoder specific gui_thread still open ? */
  if(gpp->val.gui_proc_thread != NULL)
  {
     /* wait until thread exits */
     g_thread_join(gpp->val.gui_proc_thread);
     gpp->val.gui_proc_thread = NULL;
  }
  if(gpp->productive_encoder_thread != NULL)
  {
     /* wait until thread exits */
     g_thread_join(gpp->productive_encoder_thread);
     gpp->productive_encoder_thread = NULL;
  }

  if(gpp->val.run)
  {
    return 0;
  }
  return -1;
}  /* end gap_cme_gui_master_encoder_dialog */
