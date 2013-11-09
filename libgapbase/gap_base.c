/* gap_base.c
 * 1997.11.01 hof (Wolfgang Hofer)
 *
 * GAP ... Gimp Animation Plugins
 *
 * basic GAP types and utility procedures
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
 * 2.5.0  2009.03.07   hof: created
 */

#include "config.h"

/* SYSTEM (UNIX) includes */
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <signal.h>           /* for kill */
#ifdef HAVE_SYS_TIMES_H
#include <sys/times.h>
#endif
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif


#include <glib/gstdio.h>

/* GIMP includes */
#include "gtk/gtk.h"
#include "libgimp/gimp.h"
#include <libgimpwidgets/gimpwidgets.h>

#ifdef G_OS_WIN32
#include <io.h>
#  ifndef S_ISDIR
#    define S_ISDIR(m) ((m) & _S_IFDIR)
#  endif
#  ifndef S_ISREG
#    define S_ISREG(m) ((m) & _S_IFREG)
#  endif
#endif


#ifdef G_OS_WIN32
#include <process.h>             /* For _getpid() */
#else
#ifdef GAP_HAVE_PTHREAD
#define USE_PTHREAD_FOR_LINUX_THREAD_ID 1
#include "pthread.h"             /* for pthread_self() */
#endif
#endif

/* GAP includes */
#include "gap_base.h"

extern      int gap_debug; /* ==0  ... dont print debug infos */


/* -----------------------------
 * gap_base_shorten_filename
 * -----------------------------
 * resulting string is built from prefix filename  and suffix
 *    filename will be shortened when
 *    prefix + " " + filename + " " + suffix 
 *    is longer then max_chars.
 * examples:
 *    gap_base_shorten_filenam("prefix", "this_is_a_very_long_filename", NULL, 20)
 *    returns:  "prefix ...g_filename"
 *
 *    gap_base_shorten_filenam("prefix", "shortname", NULL, 20)
 *    returns:  "prefix shortname"
 *
 * the caller is responsible to g_free the returned string
 */
char *
gap_base_shorten_filename(const char *prefix
                        ,const char *filename
                        ,const char *suffix
                        ,gint32 max_chars
                        )
{
  gint len_prefix;
  gint len_fname;
  const char *pfx;
  char       *fnam;
  char       *ret_string;
  
  len_prefix = 0;
  len_fname = 0;
  ret_string = NULL;
  pfx = prefix;
  if(prefix)
  {
    len_prefix = strlen(prefix);
    if((len_prefix + 10) > max_chars)
    {
      pfx = NULL;
      len_prefix = 0;
    }
    else
    {
      len_prefix++;  /* for the space between prefix and fname */
    }
  }

  
  if(filename)
  {
    fnam = NULL;
    
    if(suffix)
    {
      fnam = g_strdup_printf("%s %s", filename, suffix);
    }
    else
    {
      fnam = g_strdup(filename);
    }


    len_fname = strlen(fnam);
    if((len_fname + len_prefix) <= max_chars)
    {
      if(pfx)
      {
        ret_string = g_strdup_printf("%s %s"
                                    ,pfx
                                    ,fnam
                                    );
      }
      else
      {
        ret_string = g_strdup_printf("%s"
                                    ,fnam
                                    );
      }
    }
    else
    {
      gint fname_idx;
      gint len_rest;
      
      len_rest = (max_chars - len_prefix - 3);
      fname_idx = len_fname - len_rest;
      
      if(pfx)
      {
        ret_string = g_strdup_printf("%s ...%s"
                                    ,pfx
                                    ,&fnam[fname_idx]
                                    );
      }
      else
      {
        ret_string = g_strdup_printf("...%s"
                                    ,&fnam[fname_idx]
                                    );
      }
    }
 
    g_free(fnam);
    return (ret_string);
     
  }

  ret_string = g_strdup(prefix);
  return(ret_string);
}  /* end gap_base_shorten_filename */                  


/* -----------------------------
 * gap_base_strdup_add_underscore
 * -----------------------------
 * duplicates the specifed string and if last character is no underscore add one at end.
 * the caller is responsible to g_free the result after usage.
 */
char *
gap_base_strdup_add_underscore(char *name)
{
  int   l_len;
  char *l_str;
  if(name == NULL)
  {
    return(g_strdup("\0"));
  }

  l_len = strlen(name);
  l_str = g_malloc(l_len+2);
  strcpy(l_str, name);
  if(l_len > 0)
  {
    if (name[l_len-1] != '_')
    {
       l_str[l_len    ] = '_';
       l_str[l_len +1 ] = '\0';
    }

  }
  return(l_str);
}

/* -----------------------------
 * gap_base_strdup_del_underscore
 * -----------------------------
 * duplicates the specifed string and delete the last character
 * if it is the underscore
 * the caller is responsible to g_free the result after usage.
 */
char *
gap_base_strdup_del_underscore(char *name)
{
  int   l_len;
  char *l_str;
  if(name == NULL)
  {
    return(g_strdup("\0"));
  }

  l_len = strlen(name);
  l_str = g_strdup(name);
  if(l_len > 0)
  {
    if (l_str[l_len-1] == '_')
    {
       l_str[l_len -1 ] = '\0';
    }

  }
  return(l_str);
}


/* --------------------------------------------------------
 * gap_base_dup_filename_and_replace_extension_by_underscore
 * --------------------------------------------------------
 * returns a duplicate of the specified filename where the extension
 * (.xcf .jpg ...) is cut off and rplaced by the underscore character.
 * example: filename = "image_000001.xcf"
 *          returns    "image_000001_"
 *
 * the caller is responsible to g_free the result after usage.
 */
char *
gap_base_dup_filename_and_replace_extension_by_underscore(const char *filename)
{
  int l_len;
  int l_idx;
  char *l_str;
  char *l_nameWithUnderscore;

  if(filename == NULL)
  {
    return (g_strdup("_"));
  }

  l_len = strlen(filename);
  l_str = g_strdup(filename);

  /* cut off the trailing .extension */
  for(l_idx = l_len -1; l_idx >= 0; l_idx--)
  {
    if (l_str[l_idx] == '.')
    {
      l_str[l_idx] = '\0';
      break;
    }
  }

  /* add underscore (if not already there) */
  l_nameWithUnderscore = gap_base_strdup_add_underscore(l_str);
  
  g_free(l_str);
  
  return (l_nameWithUnderscore);

}  /* end gap_base_dup_filename_and_replace_extension_by_underscore */




/* --------------------------------
 * gap_base_gdouble_to_ascii_string
 * --------------------------------
 * convert the specified gdouble value to ascci string.
 * (always use "." as decimalpoint, independent of LOCALE language settings)
 * return the converted string. (the caller is responsible to g_free this string after usage)
 */
gchar *
gap_base_gdouble_to_ascii_string(gdouble value, gint precision_digits)
{
  gchar *retValueAsSting;
  
  gchar l_dbl_str[G_ASCII_DTOSTR_BUF_SIZE];
  gchar l_fmt_str[20];
  gint  l_len;

  g_snprintf(l_fmt_str, sizeof(l_fmt_str), "%%.%df", (int)precision_digits);
  g_ascii_formatd(l_dbl_str, sizeof(l_dbl_str), l_fmt_str, value);

  retValueAsSting = g_strdup_printf("%s", l_dbl_str);
  return (retValueAsSting);
}  /* end gap_base_gdouble_to_ascii_string */


/* --------------------------------
 * gap_base_fprintf_gdouble
 * --------------------------------
 * print prefix and gdouble value to file
 * (always use "." as decimalpoint, independent of LOCALE language settings)
 */
void
gap_base_fprintf_gdouble(FILE *fp, gdouble value, gint digits, gint precision_digits, const char *pfx)
{
  gchar l_dbl_str[G_ASCII_DTOSTR_BUF_SIZE];
  gchar l_fmt_str[20];
  gint  l_len;

  g_snprintf(l_fmt_str, sizeof(l_fmt_str), "%%.%df", (int)precision_digits);
  g_ascii_formatd(l_dbl_str, sizeof(l_dbl_str), l_fmt_str, value);

  fprintf(fp, "%s", pfx);

  /* make leading blanks */
  l_len = strlen(l_dbl_str) - (digits + 1 +  precision_digits);
  while(l_len < 0)
  {
    fprintf(fp, " ");
    l_len++;
  }
  fprintf(fp, "%s", l_dbl_str);
}  /* end gap_base_fprintf_gdouble */



/* ============================================================================
 * gap_base_sscan_flt_numbers
 * ============================================================================
 * scan the blank separated buffer for 2 integer and 13 float numbers.
 * always use "." as decimalpoint in the float numbers regardless to LANGUAGE settings
 * return a counter that tells how many numbers were scanned successfully
 */
gint
gap_base_sscan_flt_numbers(gchar   *buf
                  , gdouble *farr
                  , gint     farr_max
                  )
{
  gint  l_cnt;
  gchar *nptr;
  gchar *endptr;

  l_cnt =0;
  nptr  = buf;
  while(l_cnt < farr_max)
  {
    endptr = nptr;
    farr[l_cnt] = g_ascii_strtod(nptr, &endptr);
    if(nptr == endptr)
    {
      break;  /* pointer was not advanced because no valid floatnumber was scanned */
    }
    nptr = endptr;

    l_cnt++;  /* count successful scans */
  }

  return (l_cnt);
}  /* end gap_base_sscan_flt_numbers */


/* --------------------------------
 * gap_base_check_tooltips
 * --------------------------------
 * check and enable/disable tooltips according to global gimprc settings
 */
gboolean
gap_base_check_tooltips(gboolean *old_state)
{
  char *value_string;
  gboolean new_state;
  gboolean changed;

  new_state = TRUE;
  changed = TRUE;
  
  value_string = gimp_gimprc_query("show-tooltips");
  if(value_string != NULL)
  {
    if (strcmp(value_string, "no") == 0)
    {
       new_state = FALSE;
    }
  }
  
  if (old_state != NULL)
  {
    if(*old_state == new_state)
    {
      changed = FALSE;
    }
  }
  
  if (changed == TRUE)
  {
    if(new_state == TRUE)
    {
       gimp_help_enable_tooltips ();
    }
    else
    {
       gimp_help_disable_tooltips ();
    }
  }
  
  return (new_state);
  
}  /* end gap_base_check_tooltips */


/* -----------------------------------------
 * gap_base_get_gimprc_gdouble_value
 * -----------------------------------------
 * get gdouble configuration value for the keyname gimprc_option_name from the gimprc file.
 * returns the configure value in constraint to the specified range 
 * (between min_value and max_value)
 * the specified default_value is returned in case the gimprc
 * has no entry for the specified gimprc_option_name.
 */
gdouble
gap_base_get_gimprc_gdouble_value (const char *gimprc_option_name
   , gdouble default_value, gdouble min_value, gdouble max_value)
{
  char *value_string;
  gdouble value;

  value = default_value;

  value_string = gimp_gimprc_query(gimprc_option_name);
  if(value_string)
  {
     gchar *endptr;
     gchar *nptr;
     gdouble val;

     nptr  = value_string;
     val = g_ascii_strtod(nptr, &endptr);
     if(nptr != endptr)
     {
       value = val;
     }
     g_free(value_string);
  }
  return (CLAMP(value, min_value, max_value));

}  /* end gap_base_get_gimprc_gdouble_value */


/* -----------------------------------------
 * gap_base_get_gimprc_int_value
 * -----------------------------------------
 * get integer configuration value for the keyname gimprc_option_name from the gimprc file.
 * returns the configure value in constraint to the specified range 
 * (between min_value and max_value)
 * the specified default_value is returned in case the gimprc
 * has no entry for the specified gimprc_option_name.
 */
gint32
gap_base_get_gimprc_int_value (const char *gimprc_option_name
   , gint32 default_value, gint32 min_value, gint32 max_value)
{
  char *value_string;
  gint32 value;

  value = default_value;

  value_string = gimp_gimprc_query(gimprc_option_name);
  if(value_string)
  {
     value = atol(value_string);
     g_free(value_string);
  }
  return (CLAMP(value, min_value, max_value));

}  /* end p_get_gimprc_int_value */

/* -----------------------------------------
 * gap_base_get_gimprc_gboolean_value
 * -----------------------------------------
 */
gboolean
gap_base_get_gimprc_gboolean_value (const char *gimprc_option_name
   , gboolean default_value)
{
  char *value_string;
  gboolean value;

  value = default_value;

  value_string = gimp_gimprc_query(gimprc_option_name);
  if(value_string)
  {
     value = FALSE;
     if((*value_string == 'y') || (*value_string == 'Y'))
     {
       value = TRUE;
     }
     g_free(value_string);
  }
  return (value);

}  /* end gap_base_get_gimprc_gboolean_value */

/* --------------------------------
 * gap_base_getpid
 * --------------------------------
 * get process id of the current process
 */
gint32
gap_base_getpid(void)
{
  return ((gint32)getpid());
}

/* --------------------------------
 * gap_base_is_pid_alive
 * --------------------------------
 * return TRUE if the process with the specified pid
 * is alive.
 * WARNING:
 * there is no implementation for the WINDOWS operating system
 * where the return value is always TRUE.
 */
gboolean 
gap_base_is_pid_alive(gint32 pid)
{
#ifndef G_OS_WIN32
  /* for UNIX */

  /* kill  with signal 0 checks only if the process is alive (no signal is sent)
   *       returns 0 if alive, 1 if no process with given pid found.
   */
  if (0 == kill(pid, 0))
  {
    return(TRUE);
  }
  return (FALSE);
#else
  /* hof: dont know how to check on Windows
   *      assume that process is always alive
   *      (therefore on Windows locks will not be cleared 
   *       automatically after crashes of the locking process)
   */
  return(TRUE);
#endif
}


/* --------------------------------
 * gap_base_get_current_time
 * --------------------------------
 * get curent system time in utc timecode
 */
gint32
gap_base_get_current_time(void)
{
  return ((gint32)time(0));
}


/* ------------------------------
 * gap_base_mix_value_exp
 * ------------------------------
 *  result is a  for factor 0.0
 *            b  for factor 1.0
 *            exponential mix for factors inbetween
 */
gdouble 
gap_base_mix_value_exp(gdouble factor, gdouble a, gdouble b)
{
  gdouble minAB;
  gdouble offset;
  gdouble value;
  
  if((a > 0) && (b > 0))
  {
    return ((a) * exp((factor) * log((b) / (a))));
  }

  if(a == b)
  {
    return (a);
  }
  
  /* offset that makes both a and b positve values > 0
   * to perform the exponential mix calculation
   */
  
  minAB = (a < b ? a : b);
  offset = 1 - minAB;
  
  value = ((a + offset) * exp((factor) * log((b + offset) / (a + offset))));

  /* shift mixed value back to original range */
  return (value - offset);
}  /* end gap_base_mix_value_exp */


/* ---------------------------------
 * gap_base_mix_value_exp_and_round
 * ---------------------------------
 *  result is a  for factor 0.0
 *            b  for factor 1.0
 *            exponential mix for factors inbetween
 *            and rounded
 *            (0.5 is rounded to 1.0, -0.5 is rounded to -1.0
 */
gdouble 
gap_base_mix_value_exp_and_round(gdouble factor, gdouble a, gdouble b)
{
  return (ROUND(gap_base_mix_value_exp(factor, a, b)));
}



/* ---------------------------------
 * gap_base_get_numProcessors
 * ---------------------------------
 * get number of available processors.
 * This implementation uses the gimprc parameter of the gimp core application.
 * Therefore the returned number does not reflect hardware information, but
 * reprents the number of processors that are configured to be used by th gimp.
 */
gint
gap_base_get_numProcessors()
{
  gint      numProcessors;

  numProcessors = gap_base_get_gimprc_int_value("num-processors"
                               , 1  /* default */
                               , 1  /* min */
                               , 32 /* max */
                               );
  return (numProcessors);
}


/* ---------------------------------
 * gap_base_thread_init
 * ---------------------------------
 * check if thread support and init thread support if true.
 * returns TRUE on successful initialisation of thread system
 *         FALSE in case thread support is not available.
 * Note: multiple calls are tolarated and shall always deliver the same result. 
 */
gboolean 
gap_base_thread_init()
{
  static gboolean isFirstCall = TRUE;
  gboolean isThreadSupportOk;
  
  /* check if thread system is already initialized */
  if(isFirstCall == TRUE)
  {
    if(gap_debug)
    {
      printf("gap_base_thread_init: CALLING g_thread_init\n");
    }
    /* try to init thread system */
    g_thread_init(NULL);

    isFirstCall = FALSE;
  }

  isThreadSupportOk = g_thread_supported();

  if(gap_debug)
  {
    printf("gap_base_thread_init: isThreadSupportOk:%d\n"
      ,(int)isThreadSupportOk
      );
  }

  return(isThreadSupportOk);
}


/* ---------------------------------
 * gap_base_get_thread_id
 * ---------------------------------
 * get id of the current thread.
 * gthread does not provide that feature.
 *
 * therefore use pthread implementation to get the current thread id.
 * In case pthread is not available at compiletime this procedure
 * will always return 0 and runtime.
 *
 * WARNING: This procedure shall NOT be used in productive features,
 * because there is no working variant implemented on other OS than Linux.
 * The main purpose is for logging and performance measure purposes
 * while development on Linux
 */
gint64
gap_base_get_thread_id()
{
  gint64 threadId = 0;
  
//#ifdef HAVE_SYS_TYPES_H
//  threadId = (gint64)gettid();
//#endif

#ifdef USE_PTHREAD_FOR_LINUX_THREAD_ID
  threadId = pthread_self();
  if(gap_debug)
  {
    printf("pthread_self threadId:%lld\n", threadId);
  }
#endif

  return (threadId);
}


/* ---------------------------------
 * gap_base_get_gimp_mutex
 * ---------------------------------
 * get the global mutex that is intended to synchronize calls to gimp core function
 * from within gap plug-ins when running in multithreaded environment.
 * the returned mutex is a singleton e.g. is the same static mutex at each call.
 * therefore the caller must not free the returned mutex.
 */
GStaticMutex *
gap_base_get_gimp_mutex()
{
  static GStaticMutex gimpMutex = G_STATIC_MUTEX_INIT;
  
  return (&gimpMutex);

}



/* ---------------------------
 * gap_base_gimp_mutex_trylock 
 * ---------------------------
 * lock the static gimpMutex singleton if present (e.g. is NOT NULL)
 *
 * return immediate FALSE in case the mutex is locked by another thread
 * return TRUE in case the mutex was locked successfully (may sleep until other threads unlock the mutex)
 *        TRUE will be immediatly returned in case
 *        the thread system is not initialized, e.g g_thread_init was not yet called
 */
gboolean
gap_base_gimp_mutex_trylock(GapTimmRecord  *gimpMutexStats)
{
  gboolean isSuccessful;

  GStaticMutex        *gimpMutex;

  gimpMutex = gap_base_get_gimp_mutex();
  if(gimpMutex)
  {
    GAP_TIMM_START_RECORD(gimpMutexStats);

    isSuccessful = g_static_mutex_trylock (gimpMutex);

    GAP_TIMM_STOP_RECORD(gimpMutexStats);
  }
  else
  {
    isSuccessful = TRUE;
  }

  return(isSuccessful);

}  /* end gap_base_gimp_mutex_trylock */


/* ---------------------------
 * gap_base_gimp_mutex_lock 
 * ---------------------------
 * lock the static gimpMutex singleton if present (e.g. is NOT NULL)
 */
void
gap_base_gimp_mutex_lock(GapTimmRecord  *gimpMutexStats)
{
  GStaticMutex        *gimpMutex;

  gimpMutex = gap_base_get_gimp_mutex();
  if(gimpMutex)
  {
    GAP_TIMM_START_RECORD(gimpMutexStats);

    g_static_mutex_lock (gimpMutex);

    GAP_TIMM_STOP_RECORD(gimpMutexStats);
  }

}  /* end gap_base_gimp_mutex_lock */


/* ---------------------------
 * gap_base_gimp_mutex_unlock 
 * ---------------------------
 * lock the static gimpMutex singleton if present (e.g. is NOT NULL)
 */
void
gap_base_gimp_mutex_unlock(GapTimmRecord  *gimpMutexStats)
{
  GStaticMutex        *gimpMutex;

  gimpMutex = gap_base_get_gimp_mutex();
  if(gimpMutex)
  {
    GAP_TIMM_START_RECORD(gimpMutexStats);
    
    g_static_mutex_unlock (gimpMutex);
    
    GAP_TIMM_STOP_RECORD(gimpMutexStats);
  }

}  /* end gap_base_gimp_mutex_unlock */
