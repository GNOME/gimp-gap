/* gap_fmac_main.c
 * 2003.11.08 hof (Wolfgang Hofer)
 *
 * GAP ... Gimp Animation Plugins
 *
 * This Module contains:
 * - filtermacros
 *
 * the GIMP-GAP filtermacro implementation introduces
 * LIMITED macro features into GIMP-1.3.x
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
#include "gap-intl.h"
#include "gap_lib.h"
#include "gap_val_file.h"
#include "gap_filter.h"
#include "gap_filter_pdb.h"
#include "gap_fmac_name.h"
#include "gap_fmac_base.h"
#include "gap_dbbrowser_utils.h"
#include "gap_lastvaldesc.h"
#include "gap_fmac_context.h"
#include "gap_accel_char.h"

/* revision history:
 * gimp   1.3.26b;  2004/02/29  hof: bugfix NONINTERACTIVE call did crash
 * gimp   1.3.22c;  2003/11/12  hof: button sensitivity
 * gimp   1.3.22b;  2003/11/09  hof: created (based on old unpublished patches for gimp-1.2)
 */


/* from gap_fmac_name.h: GAP_FMACNAME_PLUG_IN_NAME_FMAC */
#define HELP_ID_NAME_FMAC            "plug-in-filter-macro"
#define GAP_DB_BROWSER_FMAC_HELP_ID  "gap-filtermacro-db-browser"

#define FMAC_FILE_LENGTH  1500

/* ------------------------
 * global gap DEBUG switch
 * ------------------------
 */

/* int gap_debug = 1; */    /* print debug infos */
/* int gap_debug = 0; */    /* 0: dont print debug infos */

int gap_debug = 0;



/* ############################################################# */
typedef struct
{
  GtkWidget *dialog;

  GtkListStore     *store;
  GtkWidget        *tv;
  GtkTreeSelection *sel;
  gint              selected_number;
  GtkWidget        *file_entry;
  GtkWidget        *filesel;
  GtkWidget        *ok_button;
  GtkWidget        *add_button;
  GtkWidget        *delete_button;
  GtkWidget        *delete_all_button;

  gchar             filtermacro_file[FMAC_FILE_LENGTH];
  gint32            image_id;
  gint32            drawable_id;
  gboolean          run_flag;

}  fmac_globalparams_t;    /* gpp */


gint         gap_fmac_dialog(GimpRunMode run_mode, gint32 image_id, gint32 drawable_id);

static gchar * p_get_filtername(const char *line);
static void  p_procedure_select_callback (GtkTreeSelection *sel, fmac_globalparams_t *gpp);
static void  p_tree_fill (fmac_globalparams_t *gpp);
static void  p_close_callback  (GtkWidget *widget, fmac_globalparams_t *gpp);
static void  p_ok_callback     (GtkWidget *widget, fmac_globalparams_t *gpp);
static void  p_help_callback  (GtkWidget *widget, fmac_globalparams_t *gpp);
static void  p_add_callback    (GtkWidget *widget, fmac_globalparams_t *gpp);
static void  p_delete_callback (GtkWidget *widget, fmac_globalparams_t *gpp);
static void  p_delete_all_callback (GtkWidget *widget, fmac_globalparams_t *gpp);
static void  p_filebrowser_button_callback (GtkWidget *widget, fmac_globalparams_t *gpp);
static void  p_filesel_ok_callback(GtkWidget *widget, fmac_globalparams_t *gpp);
static void  p_file_entry_update_callback(GtkWidget *widget, fmac_globalparams_t *gpp);
static void  p_filesel_close_callback(GtkWidget *widget, fmac_globalparams_t *gpp);
static void  p_create_action_area_buttons(fmac_globalparams_t *gpp);
static void  p_setbutton_sensitivity(fmac_globalparams_t *gpp);

static gboolean  p_chk_filtermacro_file(const char *filtermacro_file);
static void      p_print_and_free_msg(char *msg, GimpRunMode run_mode);
static gchar *   p_get_gap_filter_data_string(const char *plugin_name);
static gchar *   p_get_mapped_gap_filter_data_string(const char *plugin_name, const char *filtermacro_file);
static gint      p_fmac_add_filter_to_file(const char *filtermacro_file, const char *plugin_name);
static gint      p_fmac_add_filter(const char *filtermacro_file, gint32 image_id);
static int       p_fmac_pdb_constraint_proc(gchar *proc_name, gint32 image_id);
static int       p_fmac_pdb_constraint_proc_sel1(gchar *proc_name, gint32 image_id);
static int       p_fmac_pdb_constraint_proc_sel2(gchar *proc_name, gint32 image_id);


/* ############################################################# */


static void query(void);
static void run(const gchar *name
              , gint n_params
              , const GimpParam *param
              , gint *nreturn_vals
              , GimpParam **return_vals);

GimpPlugInInfo PLUG_IN_INFO =
{
  NULL,  /* init_proc */
  NULL,  /* quit_proc */
  query, /* query_proc */
  run,   /* run_proc */
};

MAIN ()

static void
query ()
{
  static gchar             filtermacro_file[FMAC_FILE_LENGTH];

  static GimpLastvalDef lastvals[] =
  {
    GIMP_LASTVALDEF_ARRAY           (GIMP_ITER_FALSE,  filtermacro_file, "filtermacro_scriptname"),
    GIMP_LASTVALDEF_GCHAR           (GIMP_ITER_FALSE,  filtermacro_file[0], "filtermacro_scriptname"),
  };


  static GimpParamDef args_fmac_dialog[] =
  {
    {GIMP_PDB_INT32, "run_mode", "Interactive"},
    {GIMP_PDB_IMAGE, "image", "Input image"},
    {GIMP_PDB_DRAWABLE, "drawable", "Input drawable to be affected by the filtermacro"},
    {GIMP_PDB_STRING, "filtermacro_name", "Name of the filtermacro_file to execute on the input drawable)"},
  };

  static GimpParamDef *return_vals = NULL;
  static int nreturn_vals = 0;

  gimp_plugin_domain_register (GETTEXT_PACKAGE, LOCALEDIR);

  /* registration for last values buffer structure (useful for animated filter apply) */
  gimp_lastval_desc_register(GAP_FMACNAME_PLUG_IN_NAME_FMAC,
                             &filtermacro_file[0],
                             sizeof(filtermacro_file),
                             G_N_ELEMENTS (lastvals),
                             lastvals);

  gimp_install_procedure(GAP_FMACNAME_PLUG_IN_NAME_FMAC,
                         "This plug-in can create and execute filtermacro scriptfiles",
                         "This plug-in allows the user to pick one or more filters "
                         "that have already been called before in the current Gimp session. "
                         "The internal PDB-name of the picked filter is stored in a "
                         "filtermacro scriptfile, along with the parameter buffer that was "
                         "used at the last call.\n"
                         "You can execute a filtermacro scriptfile on the Input drawable "
                         "(in the current or in any further Gimp session). "
                         "The non-interactive API is limited to filtermacro script execution "
                         "and does not allow creation or modification of filtermacro scripts "
                         "WARNING: filtermacro scriptfiles are a temporary solution. "
                         "They are machine dependent. Support may be dropped in future gimp "
                         "versions.",
                         "Wolfgang Hofer (hof@gimp.org)",
                         "Wolfgang Hofer",
                         GAP_VERSION_WITH_DATE,
                         N_("Filtermacro..."),
                         "RGB*, INDEXED*, GRAY*",
                         GIMP_PLUGIN,
                         G_N_ELEMENTS (args_fmac_dialog), nreturn_vals,
                         args_fmac_dialog, return_vals);

  gimp_plugin_menu_register (GAP_FMACNAME_PLUG_IN_NAME_FMAC, N_("<Image>/Filters/"));
}  /* end query */


static void
run(const gchar *name
   , gint n_params
   , const GimpParam *param
   , gint *nreturn_vals
   , GimpParam **return_vals)
{
  static GimpParam values[1];
  GimpRunMode run_mode;
  GimpPDBStatusType status = GIMP_PDB_SUCCESS;
  gint32     image_id;
  gint32     drawable_id;
  char     *filtermacro_file;

  gint32      l_rc;
  const char *l_env;

  *nreturn_vals = 1;
  *return_vals = values;
  l_rc = 0;

  l_env = g_getenv("GAP_DEBUG");
  if(l_env != NULL)
  {
    if((*l_env != 'n') && (*l_env != 'N')) gap_debug = 1;
  }


  run_mode = param[0].data.d_int32;
  image_id = param[1].data.d_image;
  drawable_id = param[2].data.d_drawable;
  filtermacro_file = NULL;

  INIT_I18N ();


  if(gap_debug) fprintf(stderr, "\n\ngap_filter_main: debug name = %s\n", name);

  if (strcmp (name, GAP_FMACNAME_PLUG_IN_NAME_FMAC) == 0)
  {
      if (run_mode == GIMP_RUN_INTERACTIVE)
      {
        l_rc = gap_fmac_dialog(run_mode, image_id, drawable_id);
      }
      else
      {
        if (run_mode == GIMP_RUN_NONINTERACTIVE)
        {
          if(n_params == 4)
          {
            filtermacro_file = param[3].data.d_string;
            if(filtermacro_file == NULL)
            {
              status = GIMP_PDB_CALLING_ERROR;
            }
          }
          else
          {
            status = GIMP_PDB_CALLING_ERROR;
          }
        }
        else
        {
          gint l_len;

          l_len = gimp_get_data_size(GAP_FMACNAME_PLUG_IN_NAME_FMAC);
          if(l_len > 0)
          {
            filtermacro_file = g_malloc0(l_len);
            gimp_get_data(GAP_FMACNAME_PLUG_IN_NAME_FMAC, filtermacro_file);
          }
          else
          {
            filtermacro_file = g_strdup("\0");
          }
        }

        if(status == GIMP_PDB_SUCCESS)
        {
          l_rc = gap_fmac_execute(run_mode, image_id, drawable_id
                   , filtermacro_file
                   , NULL  /* filtermacro_file2 */
                   , 1.0   /* current_step */
                   , 1     /* total_steps */
                   );
        }
      }
  }
  else
  {
      status = GIMP_PDB_CALLING_ERROR;
  }

 if(l_rc < 0)
 {
    status = GIMP_PDB_EXECUTION_ERROR;
 }


 if (run_mode != GIMP_RUN_NONINTERACTIVE)
    gimp_displays_flush();

  values[0].type = GIMP_PDB_STATUS;
  values[0].data.d_status = status;

}  /* end run */



/* ----------------------
 * p_chk_filtermacro_file
 * ----------------------
 */
static gboolean
p_chk_filtermacro_file(const char *filtermacro_file)
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
}  /* end p_chk_filtermacro_file */



/* --------------------
 * p_print_and_free_msg
 * --------------------
 */
static void
p_print_and_free_msg(char *msg, GimpRunMode run_mode)
{
  if(run_mode == GIMP_RUN_INTERACTIVE)
  {
    g_message("%s", msg);
  }
  printf("%s\n", msg);
  g_free(msg);
}  /* end p_print_and_free_msg */


/* ----------------------------
 * p_get_gap_filter_data_string
 * ----------------------------
 *   return a textstring with the plugin_name
 *   and its values as textstring
 *   that has format like this:
 *      "plug_in_name" len hexbyte1 hexbyte2 .......\n
 *   example:
 *      "plug_in_sharpen"    4  0a 00 00 00
 *
 * return data_string or  NULL pointer if nothing was found.
 *        the returned data_string should be g_free'd by the caller (if it was not NULL)
 */
static gchar *
p_get_gap_filter_data_string(const char *plugin_name)
{
  gint plugin_data_len;
  int   l_byte;
  gint  l_idx;
  gchar *l_str;
  gchar *l_str_tmp;
  guchar *plugin_data;
  gchar  *data_string;

  data_string = NULL;
  plugin_data = NULL;


   if(gap_debug)
   {
     printf("p_get_gap_filter_data_string: plugin_name:%s:\n", plugin_name);
   }

   plugin_data_len = gimp_get_data_size (plugin_name);
   if (plugin_data_len > 0)
   {
        /* retrieve the data */
        plugin_data = g_malloc0(plugin_data_len);
        gimp_get_data(plugin_name, plugin_data);

        /* build the textstring, starting with quoted plug_in name and decimal datalength field
         */
        l_str_tmp = g_strdup_printf("\"%s\" %4d ", plugin_name, (int)plugin_data_len);

        /* add hex databytes */
        for (l_idx = 0; l_idx < plugin_data_len; l_idx++)
        {
          l_byte = plugin_data[l_idx];
          l_str = g_strdup_printf("%s %02x", l_str_tmp, l_byte);

          g_free(l_str_tmp);
          l_str_tmp = g_strdup(l_str);
          g_free(l_str);
        }

        /* add terminating newline character */
        l_str = g_strdup_printf("%s\n", l_str_tmp);
        g_free(l_str_tmp);

        if (gap_debug)
        {
          printf("p_get_gap_filter_data_string: %s", l_str);
        }

        data_string = l_str;
        g_free(plugin_data);

   }

   return (data_string);

}  /* end p_get_gap_filter_data_string */


/* -----------------------------------
 * p_get_mapped_gap_filter_data_string
 * -----------------------------------
 *   return a textstring with the plugin_name
 *   and its values as textstring
 *   that has format like this:
 *      "plug_in_name" len hexbyte1 hexbyte2 .......\n
 *   example:
 *      "plug_in_sharpen"    4  0a 00 00 00
 *
 *
 *  In case the plugin has at least one iterable drawable id within its last values parameters
 *  those values are mapped to persistent_drawable_id's with the help of a special
 *  iterator call with a filtermacro context and a corresponding .fmref file in recording mode.
 *
 * return data_string or  NULL pointer if nothing was found.
 *        the returned data_string should be g_free'd by the caller (if it was not NULL)
 * 
 */
static gchar *
p_get_mapped_gap_filter_data_string(const char *plugin_name, const char *filtermacro_file)
{
  static char l_key_from[512];
  static char l_key_to[512];
  gint plugin_data_len;
  guchar *plugin_data_bck;
  gchar  *data_string;
  const char *l_iteratorname;
  gint l_count;


  data_string = NULL;
 
  if(plugin_name == NULL)
  {
    return (NULL);
  }
  plugin_data_len = gimp_get_data_size (plugin_name);

  if(gap_debug)
  {
    printf("p_get_mapped_gap_filter_data_string: plugin_name:%s:  plugin_data_len:%d\n"
       , plugin_name
       , plugin_data_len
       );
  }

  /* assign the matching Iterator PluginProcedures (if there is any) */
  l_iteratorname = gap_filt_pdb_get_iterator_proc(plugin_name, &l_count);

  if ((plugin_data_len > 0) && (l_iteratorname != NULL))
  {
    GapFmacContext theFmacContext;
    GapFmacContext *fmacContext;

    fmacContext = &theFmacContext;

    if(gap_debug)
    {
      printf("p_get_mapped_gap_filter_data_string: l_iteratorname:%s:\n"
         , l_iteratorname
         );
    }
    gap_fmct_setup_GapFmacContext(fmacContext
                                 , TRUE  /* recording_mode */
                                 , filtermacro_file
                                 );


    /* retrieve the data (to backup original data) */
    plugin_data_bck = g_malloc0(plugin_data_len);
    gimp_get_data(plugin_name, plugin_data_bck);


    /* Set FROM and TO buffers. 
     * (in recording mode we use all the same data as the backup of the original buffer)
     * those buffers are not relevant in recording mode, but are required
     * for the iterator call interface.
     */
    g_snprintf(l_key_from, sizeof(l_key_from), "%s%s", plugin_name, GAP_ITER_TO_SUFFIX);
    gimp_set_data(l_key_from, plugin_data_bck, plugin_data_len);

    g_snprintf(l_key_to, sizeof(l_key_to), "%s%s", plugin_name, GAP_ITER_FROM_SUFFIX);
    gimp_set_data(l_key_to, plugin_data_bck, plugin_data_len);

    /* call the iterator in recording mode
     * this triggers the mapping of drawable ids in the persistent .fmref file.
     * (but only in case the called plugin has at least one an iterable drawable_id in its last_values paramters,
     * otherwise the las values buffer shall not change by the iteratorcall,
     * due to same settings for from and to values) 
     */
    gap_filter_iterator_call(l_iteratorname
       , 1       /* total_steps */
       , 1.0     /* current_step */
       , plugin_name
       , plugin_data_len
       );

    data_string = p_get_gap_filter_data_string(plugin_name);

    /* restore original data from backup buffer */
    gimp_set_data(plugin_name, plugin_data_bck, plugin_data_len);

    g_free(plugin_data_bck);
    
    /* disable the sessionwide filtermacro context */
    gap_fmct_disable_GapFmacContext();

  }

  return (data_string);

}  /* end p_get_mapped_gap_filter_data_string */


/* -------------------------
 * p_fmac_add_filter_to_file
 * -------------------------
 */
static gint
p_fmac_add_filter_to_file(const char *filtermacro_file, const char *plugin_name)
{
  FILE  *fp;
  gint  l_rc;

  l_rc = -1;
  if(gap_lib_file_exists(filtermacro_file) == 0 )
  {
    /*  file does not exist, or is empty */
    fp = g_fopen(filtermacro_file, "w");
    if(fp)
    {
      fprintf(fp, "# FILTERMACRO FILE (GIMP-GAP-1.3)\n");
      fprintf(fp, "# lineformat: \n");
      fprintf(fp, "# 1.st item plug-in PDB name in double quotes\n");
      fprintf(fp, "# 2.nd item decimal length of lastvalue data plug-in PDB name in double quotes\n");
      fprintf(fp, "# 3.rd until N items hex bytevalues of lastvalue data buffer\n");
      fprintf(fp, "#\n");
    }
  }
  else
  {
    fp = g_fopen(filtermacro_file, "a");
  }

  if (fp)
  {
    char *data_string;
    char *canonical_plugin_name;

    canonical_plugin_name = gimp_canonicalize_identifier(plugin_name);

    data_string = p_get_mapped_gap_filter_data_string(canonical_plugin_name, filtermacro_file);
    g_free(canonical_plugin_name);

    if(data_string)
    {
      fprintf(fp, "%s", data_string);
      l_rc = 0;  /* OK */
      g_free(data_string);
    }
    fclose(fp);
  }

  return (l_rc);
}  /* end p_fmac_add_filter_to_file */


/* -----------------
 * p_fmac_add_filter
 * -----------------
 *  pick a filter via GAP-DB-Browser dialog and add its name
 *  and its last_values buffer (parameter settings of last call in the
 *  current GIMP-session) to the end of thefiltermacro_file
 */

static gint
p_fmac_add_filter(const char *filtermacro_file, gint32 image_id)
{
  GapDbBrowserResult   l_browser_result;
  l_browser_result.accelCharacteristic = GAP_ACCEL_CHAR_NONE;

  if(gap_db_browser_dialog( _("Select Filtercalls of Current GIMP Session")
                          , _("Add Filter")
                          , FALSE                                /* FALSE disables acceleration characteristic */
                          , p_fmac_pdb_constraint_proc
                          , p_fmac_pdb_constraint_proc_sel1
                          , p_fmac_pdb_constraint_proc_sel2
                          , &l_browser_result
                          , image_id
                          , GAP_DB_BROWSER_FMAC_HELP_ID
                          )
    < 0)
  {
    if(gap_debug) printf("DEBUG: gap_db_browser_dialog cancelled\n");
    return -1;
  }

  return(p_fmac_add_filter_to_file(filtermacro_file, l_browser_result.selected_proc_name));

}  /* end p_fmac_add_filter */



/* ---------------
 * gap_fmac_dialog
 * ---------------
 *  create and perform the flitermacro dialog
 */
gint
gap_fmac_dialog(GimpRunMode run_mode, gint32 image_id, gint32 drawable_id)
{
  GtkWidget       *table;
  GtkWidget       *vbox;
  GtkWidget       *label;
  GtkWidget       *entry;
  GtkWidget       *button;
  GtkWidget       *scrolled_window;
  GtkCellRenderer *renderer;
  fmac_globalparams_t *gpp;
  gint  row;

  gimp_ui_init ("gap-filter-macro", FALSE);

  gpp = g_new0 (fmac_globalparams_t, 1);
  gpp->run_flag = FALSE;
  gpp->filtermacro_file[0] = '\0';
  gpp->selected_number = -1;

  /* probably get filtermacro_file (form a previous call in the same GIMP-session) */
  gimp_get_data(GAP_FMACNAME_PLUG_IN_NAME_FMAC, gpp->filtermacro_file);

  gpp->filesel = NULL;
  gpp->image_id = image_id;
  gpp->drawable_id = drawable_id;

  /* the dialog box */
  gpp->dialog = gtk_dialog_new ();

  gtk_window_set_title (GTK_WINDOW (gpp->dialog), _("Filter Macro Script"));
  gtk_window_set_position (GTK_WINDOW (gpp->dialog), GTK_WIN_POS_MOUSE);
  g_signal_connect (gpp->dialog, "destroy",
                    G_CALLBACK (p_close_callback), gpp);

  /* vbox : the list and the search entry */

  vbox = gtk_vbox_new (FALSE, 4);
  gtk_box_pack_start (GTK_BOX (GTK_DIALOG (gpp->dialog)->vbox),
                      vbox, TRUE, TRUE, 0);
  gtk_widget_show (vbox);

  /* table */
  table = gtk_table_new (1, 3, FALSE);
  gtk_table_set_col_spacings (GTK_TABLE (table), 4);
  gtk_table_set_row_spacings (GTK_TABLE (table), 2);
  gtk_container_set_border_width (GTK_CONTAINER (table), 4);

  gtk_box_pack_start (GTK_BOX (vbox), table, FALSE, FALSE, 0);
  gtk_widget_show (table);

  row = 0;

  /* label */
  label = gtk_label_new(_("Filename:"));
  gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
  gtk_table_attach(GTK_TABLE(table), label, 0, 1, row, row + 1, 0, 0, 0, 0);
  gtk_widget_show(label);

  /* entry */
  entry = gtk_entry_new();
  gpp->file_entry = entry;
  gtk_widget_set_size_request(entry, 300, -1);
  gtk_entry_set_text(GTK_ENTRY(entry), gpp->filtermacro_file);
  gtk_table_attach(GTK_TABLE(table), entry, 1, 2, row, row + 1, GTK_FILL | GTK_EXPAND, GTK_FILL, 4, 0);
  gimp_help_set_help_data(entry
                         , _("Name of the filtermacro scriptfile")
                         ,NULL);
  gtk_widget_show(entry);
  g_signal_connect(G_OBJECT(entry), "changed",
                   G_CALLBACK (p_file_entry_update_callback),
                   gpp);

  /* Button  to invoke filebrowser */
  button = gtk_button_new_with_label ("...");
  gimp_help_set_help_data(button
                         , _("Open filebrowser window to select a filename")
                         ,NULL);
  gtk_table_attach( GTK_TABLE(table), button, 2, 3, row, row +1,
                    0, 0, 0, 0 );
  gtk_widget_show (button);
  g_signal_connect (G_OBJECT (button), "clicked",
                    G_CALLBACK (p_filebrowser_button_callback),
                    gpp);

  /* list : list in a scrolled_win */

  scrolled_window = gtk_scrolled_window_new (NULL, NULL);
  gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (scrolled_window),
                                       GTK_SHADOW_IN);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled_window),
                                  GTK_POLICY_AUTOMATIC,
                                  GTK_POLICY_ALWAYS);
  gtk_box_pack_start (GTK_BOX (vbox), scrolled_window, TRUE, TRUE, 0);
  gtk_widget_show (scrolled_window);

  gpp->tv = gtk_tree_view_new ();

  renderer = gtk_cell_renderer_text_new ();
  gtk_cell_renderer_text_set_fixed_height_from_font
    (GTK_CELL_RENDERER_TEXT (renderer), 1);

  gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (gpp->tv),
                                               -1, _("Nr"),
                                               renderer,
                                               "text", 1,
                                               NULL);

  gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (gpp->tv),
                                               -1, _("PDB Name"),
                                               renderer,
                                               "text", 2,
                                               NULL);

  gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (gpp->tv),
                                               -1, _("Menu Path"),
                                               renderer,
                                               "text", 3,
                                               NULL);
  gtk_tree_view_set_headers_visible (GTK_TREE_VIEW (gpp->tv), TRUE);

  gtk_widget_set_size_request (gpp->tv, 320 /*WIDTH*/, 100 /*HEIGHT*/);
  gtk_container_add (GTK_CONTAINER (scrolled_window), gpp->tv);
  gtk_widget_show (gpp->tv);


  gpp->sel = gtk_tree_view_get_selection (GTK_TREE_VIEW (gpp->tv));
  g_signal_connect (gpp->sel, "changed",
                    G_CALLBACK (p_procedure_select_callback), gpp);

  /* buttons in action_aera */
  p_create_action_area_buttons(gpp);

  gtk_widget_show (gpp->dialog);

  /* now build the list */
  p_tree_fill (gpp);

  /* GTK Main Loop */
  gtk_main ();
  gdk_flush ();

  if(gpp->filtermacro_file[0] != '\0')
  {
    gimp_set_data(GAP_FMACNAME_PLUG_IN_NAME_FMAC, gpp->filtermacro_file, FMAC_FILE_LENGTH);

    if(gpp->run_flag)
    {
      if(gap_debug) printf("gap_fmac_dialog: RUN image_id:%d drawable_id:%d, filtermacro_file:%s\n"
                              ,(int)image_id
                              ,(int)drawable_id
                              ,gpp->filtermacro_file
                              );
      return(gap_fmac_execute(run_mode, image_id, drawable_id
              , gpp->filtermacro_file
              , NULL  /* filtermacro_file2 */
              , 1.0   /* current_step */
              , 1     /* total_steps */
              ));
    }
  }

  return 0;
} /* end gap_fmac_dialog */


/* ----------------------------
 * p_get_filtername
 * ----------------------------
 * input is a typical filtermacro file line
 *       if the line contains a filtername (starting with double quotes character)
 *       the return a copy of the extracted filtername (without quotes)
 *       else: return NULL
 */
static gchar *
p_get_filtername(const char *line)
{
  if(*line == '"')
  {
    gchar *filtername;
    gchar *ptr;

    filtername = g_strdup(&line[1]);
    ptr = filtername;
    while(ptr)
    {
      if((*ptr == '\n') || (*ptr == '\0') || (*ptr == '"'))
      {
        *ptr = '\0';
        break;
      }
      ptr++;
    }
    return(filtername);
  }
  return NULL;
}  /* end p_get_filtername */


/* ----------------------------
 * p_procedure_select_callback
 * ----------------------------
 * fill filternames into the treeview
 */
static void
p_procedure_select_callback (GtkTreeSelection *sel, fmac_globalparams_t *gpp)
{
  GtkTreeIter  iter;
  gchar       *numtxt;

  g_return_if_fail (sel != NULL);
  g_return_if_fail (gpp != NULL);

  if (gtk_tree_selection_get_selected (sel, NULL, &iter))
  {
    /* get column 0 (the invisible intenal number) from the store */
    gtk_tree_model_get (GTK_TREE_MODEL (gpp->store), &iter
                       ,0, &numtxt
                       , -1);
    if (gap_debug) printf("p_procedure_select_callback (3) numtxt:%s\n", numtxt);
    gpp->selected_number = atoi(numtxt);
    g_free (numtxt);
  }
}


/* ----------------------------
 * p_tree_fill
 * ----------------------------
 * fill filternames into the treeview
 */
static void
p_tree_fill (fmac_globalparams_t *gpp)
{
  GtkTreeIter   iter;
  gint          count_elem;
  gboolean      parse_file;

  GapValTextFileLines *txf_ptr;
  GapValTextFileLines *txf_ptr_root;


  gpp->store = gtk_list_store_new (4, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING);
  gtk_tree_view_set_model (GTK_TREE_VIEW (gpp->tv)
                          ,GTK_TREE_MODEL (gpp->store)
                          );
  g_object_unref (gpp->store);

  /* read filternames from filtermacro_file
   * and add them to the treeview widget
   */
  count_elem = 0;
  parse_file = FALSE;
  if(gpp->filtermacro_file[0] != '\0')
  {
    if(p_chk_filtermacro_file(gpp->filtermacro_file))
    {
      parse_file = TRUE;
    }
  }

  if(parse_file)
  {
    txf_ptr_root = gap_val_load_textfile(gpp->filtermacro_file);
    for(txf_ptr = txf_ptr_root; txf_ptr != NULL; txf_ptr = (GapValTextFileLines *) txf_ptr->next)
    {
       gchar *pdb_name;

       pdb_name = p_get_filtername(txf_ptr->line);
       if(pdb_name)
       {
         gchar *numtxt;
         gchar *label;
         gchar *menu_path;

         menu_path = gap_db_get_plugin_menupath(pdb_name);
         if(menu_path == NULL)
         {
           menu_path = g_strdup(_("** No menu path available **"));
         }
         label = g_strdup_printf("%3d.", (int)count_elem +1);
         numtxt = g_strdup_printf("%d", (int)count_elem);

         gtk_list_store_append (gpp->store, &iter);
         gtk_list_store_set (gpp->store, &iter
                            ,0, numtxt            /* internal invisible number starting at 0 */
                            ,1, label             /* visible number starting at 1 */
                            ,2, pdb_name
                            ,3, menu_path
                            ,-1);
         g_free (numtxt);
         g_free (label);
         g_free (menu_path);
         g_free (pdb_name);
         count_elem++;
       }
    }

    if(txf_ptr_root)
    {
      gap_val_free_textfile_lines(txf_ptr_root);
    }
  }

  if (count_elem == 0)
  {
    gtk_list_store_append (gpp->store, &iter);

    if((p_chk_filtermacro_file(gpp->filtermacro_file))
    || (gap_lib_file_exists(gpp->filtermacro_file) == 0 ))
    {
         gtk_list_store_set (gpp->store, &iter
                            ,0, "-1"
                            ,1, " "
                            ,2, _("** Empty **")
                            ,3, " "
                            ,-1);
    }
    else
    {
         gtk_list_store_set (gpp->store, &iter
                            ,0, "-1"
                            ,1, " "
                            ,2, _("** File is not a filtermacro **")
                            ,3, " "
                            ,-1);
    }
  }

  gtk_tree_model_get_iter_first (GTK_TREE_MODEL (gpp->store), &iter);
  gtk_tree_selection_select_iter (gpp->sel, &iter);

  p_setbutton_sensitivity  (gpp);
}  /* end p_tree_fill */


/* ----------------------------
 * p_create_action_area_buttons
 * ----------------------------
 *  create action area buttons for the flitermacro dialog
 */
static void
p_create_action_area_buttons(fmac_globalparams_t *gpp)
{
  GtkWidget       *button;
  GtkWidget       *hbox;

  hbox = gtk_hbutton_box_new ();
  gtk_box_set_spacing (GTK_BOX (hbox), 2);
  gtk_box_pack_end (GTK_BOX (GTK_DIALOG (gpp->dialog)->action_area), hbox, FALSE, FALSE, 0);
  gtk_widget_show (hbox);

  gtk_container_set_border_width (GTK_CONTAINER (GTK_DIALOG(gpp->dialog)->action_area), 0);


  /* Button HELP */
  if (gimp_show_help_button ())
    {
      button = gtk_button_new_from_stock ( GTK_STOCK_HELP);
      gtk_box_pack_start (GTK_BOX (hbox), button, TRUE, TRUE, 0);
      gimp_help_set_help_data(button
                              ,_("Show help page")
                              , NULL);
      g_signal_connect (G_OBJECT (button), "clicked"
                        ,G_CALLBACK (p_help_callback)
                        ,gpp
                        );
      gtk_widget_show (button);
    }

  /* Button Delete All */
  button = gtk_button_new_with_label (_("Delete All"));
  gpp->delete_all_button = button;
  gtk_box_pack_start (GTK_BOX (hbox), button, TRUE, TRUE, 0);
  gimp_help_set_help_data(button
                          ,_("Delete the filtermacro scriptfile")
                          , NULL);
  g_signal_connect (G_OBJECT (button), "clicked"
                   ,G_CALLBACK (p_delete_all_callback)
                   ,gpp
                   );
  gtk_widget_show (button);

  /* Button Delete */
  button = gtk_button_new_with_label (_("Delete"));
  gpp->delete_button = button;
  gtk_box_pack_start (GTK_BOX (hbox), button, TRUE, TRUE, 0);
  gimp_help_set_help_data(button
                          ,_("Delete the selected filtercall")
                          , NULL);
  g_signal_connect (G_OBJECT (button), "clicked"
                   ,G_CALLBACK (p_delete_callback)
                   ,gpp
                   );
  gtk_widget_show (button);

  /* Button Add */
  button = gtk_button_new_with_label (_("Add"));
  gpp->add_button = button;
  gtk_box_pack_start (GTK_BOX (hbox), button, TRUE, TRUE, 0);
  gimp_help_set_help_data(button
                          ,_("Open PDB-browser window to add a new filter "
                             "to the filtermacro scriptfile.\n"
                             "Important:\n"
                             "The PDB-browser shows only filters "
                             "that have already been used in the current session "
                             "and have setup the internal buffer with the "
                             "parameter settings of the last call")
                          , NULL);
  g_signal_connect (G_OBJECT (button), "clicked"
                   ,G_CALLBACK (p_add_callback)
                   ,gpp
                   );
  gtk_widget_show (button);


  /* Button Cancel */
  button = gtk_button_new_from_stock ( GTK_STOCK_CANCEL);
  GTK_WIDGET_SET_FLAGS (button, GTK_CAN_DEFAULT);
  gtk_box_pack_start (GTK_BOX (hbox), button, TRUE, TRUE, 0);
  gimp_help_set_help_data(button
                          ,_("Close window")
                          , NULL);
  g_signal_connect (G_OBJECT (button), "clicked"
                   ,G_CALLBACK (p_close_callback)
                   ,gpp
                   );
  gtk_widget_show (button);

  /* Button OK */
  button = gtk_button_new_from_stock ( GTK_STOCK_OK);
  gpp->ok_button = button;
  GTK_WIDGET_SET_FLAGS (button, GTK_CAN_DEFAULT);
  gtk_box_pack_start (GTK_BOX (hbox), button, TRUE, TRUE, 0);
  gimp_help_set_help_data(button
                          ,_("Apply filtermacro script on current drawable and close window")
                          , NULL);
  g_signal_connect (G_OBJECT (button), "clicked"
                   ,G_CALLBACK (p_ok_callback)
                   ,gpp
                   );
  gtk_widget_show (button);

  p_setbutton_sensitivity(gpp);

}  /* end p_create_action_area_buttons */


/* ----------------------------
 * p_setbutton_sensitivity
 * ----------------------------
 */
static void
p_setbutton_sensitivity(fmac_globalparams_t *gpp)
{
  gboolean sensitive_1;
  gboolean sensitive_2;

  sensitive_1 = FALSE;
  sensitive_2 = FALSE;

  if((gpp->filtermacro_file[0] != '\0')
  && (gpp->filtermacro_file[0] != ' '))
  {
    sensitive_2 = TRUE;
    if(p_chk_filtermacro_file(gpp->filtermacro_file))
    {
      sensitive_1 = TRUE;
    }
  }

  gtk_widget_set_sensitive(gpp->ok_button, sensitive_1);
  gtk_widget_set_sensitive(gpp->delete_all_button, sensitive_1);
  gtk_widget_set_sensitive(gpp->delete_button, sensitive_1);
  gtk_widget_set_sensitive(gpp->add_button, sensitive_2);

}  /* end p_setbutton_sensitivity */

/* ----------------------------
 * p_close_callback
 * ----------------------------
 */
static void
p_close_callback (GtkWidget *widget, fmac_globalparams_t *gpp)
{
  GtkWidget *dlg;

  if(gpp)
  {
    dlg = gpp->dialog;
    gpp->dialog = NULL;
    if(dlg)
    {
      gtk_widget_destroy (GTK_WIDGET (dlg));  /* close & destroy dialog window */
      gtk_main_quit ();
    }
  }
  else
  {
    gtk_main_quit ();
  }
}

/* ----------------------------
 * p_ok_callback
 * ----------------------------
 */
static void
p_ok_callback (GtkWidget *widget, fmac_globalparams_t *gpp)
{
  if(gpp)
  {
    gpp->run_flag = TRUE;
  }
  p_close_callback(NULL, gpp);
}

/* ----------------------------
 * p_help_callback
 * ----------------------------
 */
static void
p_help_callback (GtkWidget *widget, fmac_globalparams_t *gpp)
{
  if(gpp)
  {
    gimp_standard_help_func(HELP_ID_NAME_FMAC, gpp->dialog);
  }
}

/* ----------------------------
 * p_delete_callback
 * ----------------------------
 * delete the selected line in the filtermacro_file
 * the selcted_number counts only lines with filtercalls.
 * (comment lines are kept, but are not counted)
 *
 * negative values in  gpp->selected_number represent the
 * "** Empty **" Label (if no valid filtermacro file is present)
 * this line can not be deleted.
 */
static void
p_delete_callback (GtkWidget *widget, fmac_globalparams_t *gpp)
{
  if(gpp)
  {
    if(gap_lib_file_exists(gpp->filtermacro_file) != 0 )
    {

      if(gap_debug) printf("p_delete_callback: selected_number:%d\n", (int)gpp->selected_number);

      if(gpp->selected_number >= 0)
      {
        GapValTextFileLines *txf_ptr;
        GapValTextFileLines *txf_ptr_root;
        gint count_elem;
        gchar *label;
        FILE  *fp;
        gboolean copy_line;

        /* rewrite the filtermacro file
         * without the line that corresponds to gpp->selected_number
         */
        txf_ptr_root = gap_val_load_textfile(gpp->filtermacro_file);
        fp = g_fopen(gpp->filtermacro_file, "w");
        if(fp)
        {
          count_elem = 0;
          for(txf_ptr = txf_ptr_root; txf_ptr != NULL; txf_ptr = (GapValTextFileLines *) txf_ptr->next)
          {
            copy_line = TRUE;

            label = p_get_filtername(txf_ptr->line);
            if(label)
            {
              if(count_elem == gpp->selected_number)
              {
                copy_line = FALSE;
              }
              g_free(label);
              count_elem++;
            }

            if(gap_debug) printf("%4d  %s", (int)count_elem, txf_ptr->line);

            if(copy_line)
            {
              fprintf(fp, "%s", txf_ptr->line);
            }
          }
          fclose(fp);
        }
        if(txf_ptr_root)
        {
          gap_val_free_textfile_lines(txf_ptr_root);
        }

      }
    }
    p_tree_fill (gpp);
  }
}

/* ----------------------------
 * p_delete_all_callback
 * ----------------------------
 */
static void
p_delete_all_callback (GtkWidget *widget, fmac_globalparams_t *gpp)
{
  if(gpp)
  {
    if(p_chk_filtermacro_file(gpp->filtermacro_file))
    {
      g_remove(gpp->filtermacro_file);
    }
    p_tree_fill (gpp);
  }
}

/* ----------------------------
 * p_add_callback
 * ----------------------------
 */
static void
p_add_callback (GtkWidget *widget, fmac_globalparams_t *gpp)
{
  if(gpp)
  {
    if(gpp->filtermacro_file[0] != '\0')
    {
      gint l_rc;

      errno = 0;
      l_rc = p_fmac_add_filter(gpp->filtermacro_file, gpp->image_id);

      if((l_rc < 0) && (errno != 0))
      {
        g_message(_("ERROR: Could not write filtermacro script\n"
                "filename: '%s'\n%s")
               ,gpp->filtermacro_file, g_strerror (errno));
      }
    }
    p_tree_fill (gpp);
  }
}

/* -----------------------------
 * p_filebrowser_button_callback
 * -----------------------------
 */
static void
p_filebrowser_button_callback (GtkWidget *widget, fmac_globalparams_t *gpp)
{
  GtkWidget *filesel;

  if(gpp->filesel != NULL)
  {
    gtk_window_present(GTK_WINDOW(gpp->filesel));
    return;  /* filesel is already open */
  }

  filesel = gtk_file_selection_new ( _("Select Filtermacro Scriptfile"));
  gpp->filesel = filesel;

  gtk_window_set_position (GTK_WINDOW (filesel), GTK_WIN_POS_MOUSE);

  g_signal_connect (G_OBJECT (GTK_FILE_SELECTION (filesel)->ok_button),
                   "clicked",
                    G_CALLBACK (p_filesel_ok_callback),
                    gpp);

  g_signal_connect(G_OBJECT (GTK_FILE_SELECTION (filesel)->cancel_button),
                  "clicked",
                   G_CALLBACK (p_filesel_close_callback),
                   gpp);

  gtk_file_selection_set_filename (GTK_FILE_SELECTION (filesel),
                                   gpp->filtermacro_file);

  gtk_widget_show (filesel);
  /* "destroy" has to be the last signal,
   * (otherwise the other callbacks are never called)
   */
  g_signal_connect (G_OBJECT (filesel), "destroy",
                    G_CALLBACK (p_filesel_close_callback),
                    gpp);
}

/* ----------------------------
 * p_filesel_close_callback
 * ----------------------------
 */
static void
p_filesel_close_callback(GtkWidget *widget,
                   fmac_globalparams_t *gpp)
{
  GtkWidget        *filesel;

  if(gpp == NULL) return;

  filesel = gpp->filesel;
  if(filesel == NULL) return;

  gpp->filesel = NULL;   /* now filesel is closed */
  gtk_widget_destroy(GTK_WIDGET(filesel));
}

/* ----------------------------
 * p_filesel_ok_callback
 * ----------------------------
 */
static void
p_filesel_ok_callback(GtkWidget *widget,
                   fmac_globalparams_t *gpp)
{
  const gchar        *filename;

  if(gpp == NULL) return;

  filename = gtk_file_selection_get_filename (GTK_FILE_SELECTION (gpp->filesel));

  g_snprintf(&gpp->filtermacro_file[0], sizeof(gpp->filtermacro_file), "%s", filename);
  gtk_entry_set_text(GTK_ENTRY(gpp->file_entry), filename);

  if (gap_debug) printf("p_filesel_ok_callback: %s\n", gpp->filtermacro_file);

  p_filesel_close_callback(gpp->filesel, gpp);
}

/* ----------------------------
 * p_file_entry_update_callback
 * ----------------------------
 */
static void
p_file_entry_update_callback(GtkWidget *widget,
                   fmac_globalparams_t *gpp)
{
 if(gpp)
 {
   g_snprintf(gpp->filtermacro_file, sizeof(gpp->filtermacro_file), "%s"
             , gtk_entry_get_text(GTK_ENTRY(widget))
             );
   p_tree_fill (gpp);
 }
}


/* ---------------------------------
 * p_fmac_pdb_constraint_proc_sel1
 * ---------------------------------
 */
static int
p_fmac_pdb_constraint_proc(gchar *proc_name, gint32 image_id)
{
  int l_rc;

  if(strncmp(proc_name, "file", 4) == 0)
  {
     /* Do not add file Plugins (check if name starts with "file") */
     return 0;
  }

  if(strncmp(proc_name, "plug_in_gap_", 12) == 0)
  {
     /* Do not add GAP Plugins (check if name starts with "plug_in_gap_") */
     return 0;
  }

  if(strcmp(proc_name, GAP_FMACNAME_PLUG_IN_NAME_FMAC) == 0)
  {
     /* Do not add the filtermacro plugin itself */
     return 0;
  }

  l_rc = gap_filt_pdb_procedure_available(proc_name, GAP_PTYP_CAN_OPERATE_ON_DRAWABLE);

  if(l_rc < 0)
  {
     /* Do not add, Plug-in not available or wrong type */
     return 0;
  }


  return(p_fmac_pdb_constraint_proc_sel1(proc_name, image_id));
}

/* ---------------------------------
 * p_fmac_pdb_constraint_proc_sel1
 * ---------------------------------
 */
static int
p_fmac_pdb_constraint_proc_sel1(gchar *proc_name, gint32 image_id)
{
  char *data_string;

  data_string = p_get_gap_filter_data_string(proc_name);

  if(data_string)
  {
    g_free(data_string);
    return 1;            /* 1 .. set "Add Filter" Button sensitive */
  }

  return 0;              /* 0 .. set "Add Filter" Button insensitive */
}

/* ---------------------------------
 * p_fmac_pdb_constraint_proc_sel2
 * ---------------------------------
 */
static int
p_fmac_pdb_constraint_proc_sel2(gchar *proc_name, gint32 image_id)
{
  return (p_fmac_pdb_constraint_proc_sel1 (proc_name, image_id));
}

