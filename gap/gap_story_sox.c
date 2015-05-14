/* gap_story_sox.c
 *    Audio resampling Modules based on calls to external audio converter program.
 *    Default for the external converter is the Utility Program sox
 *    (that is available on Linux, Windows, and MacOSX
 *     see http://sox.sourceforge.net/ )
 */
/*
 * Copyright
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

#include <config.h>

#include <stdlib.h>
#include <string.h>

#include <glib/gstdio.h>

#include "gap_story_sox.h"
#include "gap_audio_wav.h"
#include "gap-intl.h"


extern int gap_debug;  /* 1 == print debug infos , 0 dont print debug infos */


/* --------------------------------------------
 * p_allocate_and_substitute_calling_parameters
 * --------------------------------------------
 */
char *
p_allocate_and_substitute_calling_parameters(char *in_audiofile
               ,char *out_audiofile
               ,gint32 samplerate
               ,char *util_sox_options)
{
  char *optPtr;
  char *paramString;
  char *paramStringNew;

  paramString = g_strdup("");
  if (util_sox_options != NULL)
  {
    optPtr = util_sox_options;

    while(*optPtr != '\0')
    {
      if (strncmp("$IN", optPtr, strlen("$IN")) == 0)
      {
        paramStringNew = g_strdup_printf("%s%s"
                      , paramString
                      , in_audiofile               /* input audio file */
                      );
        g_free(paramString);
        paramString = paramStringNew;
        optPtr += strlen("$IN");
      }
      else if (strncmp("$OUT", optPtr, strlen("$OUT")) == 0)
      {
        paramStringNew = g_strdup_printf("%s%s"
                      , paramString
                      , out_audiofile              /* output audio file (tmp 16-bit wav file) */
                      );
        g_free(paramString);
        paramString = paramStringNew;
        optPtr += strlen("$OUT");
      }
      else if (strncmp("$RATE", optPtr, strlen("$RATE")) == 0)
      {
        paramStringNew = g_strdup_printf("%s%d"
                      , paramString
                      , (int)samplerate
                      );
        g_free(paramString);
        paramString = paramStringNew;
        optPtr += strlen("$RATE");
      }
      else
      {
        paramStringNew = g_strdup_printf("%s%c"
                      , paramString
                      , *optPtr
                      );
        g_free(paramString);
        paramString = paramStringNew;
        optPtr++;
      }
    }
  }

  return (paramString);

}  /* end p_allocate_and_substitute_calling_parameters */


/* --------------------------------
 * gap_story_sox_exec_resample
 * --------------------------------
 */
void
gap_story_sox_exec_resample(char *in_audiofile
               ,char *out_audiofile
               ,gint32 samplerate
               ,char *util_sox           /* the resample program (default: sox) */
               ,char *util_sox_options
               )
{
  gchar *l_cmd_argv[50];
  gchar *paramString;
  gchar *l_util_sox;
  gchar *l_util_sox_options;
  int    l_arg_idx;
  char  *l_ptr;
  char  *l_split_args_string;



  if(util_sox == NULL)
  {
     if(gap_debug)
     {
       printf("util_sox is NULL\n");
     }
     l_util_sox = gimp_gimprc_query("video_encode_com_util_sox");
     if(l_util_sox == NULL)
     {
       l_util_sox = g_strdup(GAP_STORY_SOX_DEFAULT_UTIL_SOX);
     }
  }
  else
  {
    l_util_sox = g_strdup(util_sox);
  }

  if(util_sox_options == NULL)
  {
    l_util_sox_options = gimp_gimprc_query("video_encode_com_util_sox_options");
    if(l_util_sox_options == NULL)
    {
      l_util_sox_options = g_strdup(GAP_STORY_SOX_DEFAULT_UTIL_SOX_OPTIONS);
    }
  }
  else
  {
    l_util_sox_options = g_strdup(util_sox_options);
  }


  /* build the parameter string for calling the external audio converter tool
   * IN, OUT, RATE  that are used for Parameter substitution
   */
  paramString = p_allocate_and_substitute_calling_parameters(in_audiofile
                   ,out_audiofile
                   ,samplerate
                   ,l_util_sox_options
                   );


  l_arg_idx = 0;
  l_cmd_argv[l_arg_idx++] = l_util_sox;  /* 1st param is name of called program */
  
  
  /* split the substituted parameter string in substrings with whitespae as separator
   * and put '\0' terminator characters wherever a substring ends.
   * The l_cmd_argv[] array is filled with pointers
   * to the substring start positions.
   */
  l_split_args_string = g_strdup(paramString);
  l_ptr = l_split_args_string;
  if (l_ptr != NULL)
  {
#define PARSE_STATUS_IS_SPACE     0
#define PARSE_STATUS_IS_SINGLE_QT 1
#define PARSE_STATUS_IS_DOUBLE_QT 2
#define PARSE_STATUS_IS_TEXT      3

    gint parseStatus = PARSE_STATUS_IS_SPACE;
    
    while(*l_ptr != '\0')
    {
      switch(parseStatus)
      {
        case PARSE_STATUS_IS_SINGLE_QT:
          switch(*l_ptr)
          {
            case '\'':
              parseStatus = PARSE_STATUS_IS_SPACE;
              *l_ptr = '\0';  /* set termintate marker for substring */
              break;
            default:
              break;
          }
          break;
        case PARSE_STATUS_IS_DOUBLE_QT:
          switch(*l_ptr)
          {
            case '"':
              parseStatus = PARSE_STATUS_IS_SPACE;
              *l_ptr = '\0';  /* set termintate marker for substring */
              break;
            default:
              break;
          }
          break;
        case PARSE_STATUS_IS_TEXT:
          switch(*l_ptr)
          {
            case ' ':
            case '\t':
            case '\n':
              parseStatus = PARSE_STATUS_IS_SPACE;
              *l_ptr = '\0';  /* set termintate marker for substring */
              break;    /* skip sequence of white space charactes */
            default:
              break;
          }
          break;
        case PARSE_STATUS_IS_SPACE:
          switch(*l_ptr)
          {
            case '\'':
              parseStatus = PARSE_STATUS_IS_SINGLE_QT;
              l_cmd_argv[l_arg_idx++] = &l_ptr[1];
              break;
            case '"':
              parseStatus = PARSE_STATUS_IS_DOUBLE_QT;
              l_cmd_argv[l_arg_idx++] = &l_ptr[1];
              break;
            case ' ':
            case '\t':
            case '\n':
              break;    /* skip sequence of white space charactes */
            default:
              parseStatus = PARSE_STATUS_IS_TEXT;
              l_cmd_argv[l_arg_idx++] = l_ptr;
              break;
          }
          break;
      }
      
      
      l_ptr++;  /* advance to next character */
    }
  }
 
  l_cmd_argv[l_arg_idx++] = NULL; /* (char *)0 indicates end of calling args */
  



  if(gap_debug)
  {
    printf("Execute-resample CMD:%s  args:%s\n", l_util_sox, paramString);
  }

  /* spawn external converter program (l_util_sox) */
  {
    gboolean spawnRc;
    GSpawnFlags spawnFlags = G_SPAWN_STDOUT_TO_DEV_NULL | G_SPAWN_STDERR_TO_DEV_NULL;
    gint exit_status;

    if(!g_file_test (l_util_sox, G_FILE_TEST_IS_EXECUTABLE) )
    {
      /* in case the external program was not configured with absolute path
       * enable the search Path flag
       */
      spawnFlags |= G_SPAWN_SEARCH_PATH;
    }
    spawnRc = g_spawn_sync(NULL,        /*  const gchar *working_directory  NULL: inherit from parent */
                          l_cmd_argv,   /*  argument vector for the external program */
                          NULL,         /*  gchar **envp NULL inherit from parent */
                          spawnFlags,
                          NULL,         /*  GSpawnChildSetupFunc child_setup function to run in the child just before exec() */
                          NULL,         /*  gpointer user_data */
                          NULL,         /*  gchar **standard_output */
                          NULL,         /*  gchar **standard_error */
                          &exit_status,
                          NULL          /* GError **spawnError */
                          );

    if(!spawnRc)
    {
      printf("Failed to spawn external audio converter program: %s %s"
        , l_util_sox
        , paramString
        );
    }
                          
  }

  g_free(l_util_sox);
  g_free(l_util_sox_options);

  g_free(l_split_args_string);
  g_free(paramString);


}  /* end gap_story_sox_exec_resample */
