/* gap_base.h
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

#ifndef _GAP_BASE_H
#define _GAP_BASE_H

#include "libgimp/gimp.h"
#include "gap_timm.h"

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
                        );


/* -----------------------------
 * gap_base_strdup_add_underscore
 * -----------------------------
 * duplicates the specifed string and if last character is no underscore add one at end.
 * the caller is responsible to g_free the result after usage.
 */
char *
gap_base_strdup_add_underscore(char *name);


/* -----------------------------
 * gap_base_strdup_del_underscore
 * -----------------------------
 * duplicates the specifed string and delete the last character
 * if it is the underscore
 * the caller is responsible to g_free the result after usage.
 */
char *
gap_base_strdup_del_underscore(char *name);



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
gap_base_dup_filename_and_replace_extension_by_underscore(const char *filename);



/* --------------------------------
 * gap_base_gdouble_to_ascii_string
 * --------------------------------
 * convert the specified gdouble value to ascci string.
 * (always use "." as decimalpoint, independent of LOCALE language settings)
 * return the converted string. (the caller is responsible to g_free this string after usage)
 */
gchar *
gap_base_gdouble_to_ascii_string(gdouble value, gint precision_digits);


/* --------------------------------
 * gap_base_fprintf_gdouble
 * --------------------------------
 * print prefix and gdouble value to file
 * (always use "." as decimalpoint, independent of LOCALE language settings)
 */
void
gap_base_fprintf_gdouble(FILE *fp, gdouble value, gint digits, gint precision_digits, const char *pfx);


/* --------------------------------
 * gap_base_sscan_flt_numbers
 * --------------------------------
 * scan the blank separated buffer for 2 integer and 13 float numbers.
 * always use "." as decimalpoint in the float numbers regardless to LANGUAGE settings
 * return a counter that tells how many numbers were scanned successfully
 */
gint
gap_base_sscan_flt_numbers(gchar   *buf
                  , gdouble *farr
                  , gint     farr_max
                  );


/* --------------------------------
 * gap_base_check_tooltips
 * --------------------------------
 * check and enable/disable tooltips according to global gimprc settings
 */
gboolean
gap_base_check_tooltips(gboolean *old_state);

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
   , gdouble default_value, gdouble min_value, gdouble max_value);

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
   , gint32 default_value, gint32 min_value, gint32 max_value);

/* -----------------------------------------
 * gap_base_get_gimprc_gboolean_value
 * -----------------------------------------
 */
gboolean
gap_base_get_gimprc_gboolean_value (const char *gimprc_option_name
   , gboolean default_value);


/* --------------------------------
 * gap_base_getpid
 * --------------------------------
 * get process id of the current process
 */
gint32
gap_base_getpid(void);

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
gap_base_is_pid_alive(gint32 pid);


/* --------------------------------
 * gap_base_get_current_time
 * --------------------------------
 * get curent system time in utc timecode
 */
gint32
gap_base_get_current_time(void);



/* ------------------------------
 * gap_base_mix_value_exp
 * ------------------------------
 *  result is a  for factor 0.0
 *            b  for factor 1.0
 *            exponential mix for factors inbetween
 */
gdouble 
gap_base_mix_value_exp(gdouble factor, gdouble a, gdouble b);

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
gap_base_mix_value_exp_and_round(gdouble factor, gdouble a, gdouble b);


/* ---------------------------------
 * gap_base_get_numProcessors
 * ---------------------------------
 * get number of available processors.
 * This implementation uses the gimprc parameter of the gimp core application.
 * Therefore the returned number does not reflect hardware information, but
 * reprents the number of processors that are configured to be used by th gimp.
 */
gint
gap_base_get_numProcessors();

/* ---------------------------------
 * gap_base_thread_init
 * ---------------------------------
 * check if thread support and init thread support if true.
 * returns TRUE on successful initialisation of thread system
 *         FALSE in case thread support is not available.
 * Note: multiple calls are tolarated and shall always deliver the same result. 
 */
gboolean 
gap_base_thread_init();


/* ---------------------------------
 * gap_timm_get_thread_id
 * ---------------------------------
 * get id of the current thread.
 * gthread does not provide that feature.
 *
 * therefore use pthread implementation to get the current thread id.
 * In case pthread is not available at compiletime this procedure
 * will always return 0 and runtime.
 */
gint64
gap_base_get_thread_id();

/* ---------------------------------
 * gap_base_get_gimp_mutex
 * ---------------------------------
 * get the global mutex that is intended to synchronize calls to gimp core function
 * from within gap plug-ins when running in multithreaded environment.
 * the returned mutex is a singleton e.g. is the same static mutex at each call.
 * therefore the caller must not free the returned mutex.
 */
GStaticMutex *
gap_base_get_gimp_mutex();

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
gap_base_gimp_mutex_trylock(GapTimmRecord  *gimpMutexStats);


/* ---------------------------
 * gap_base_gimp_mutex_lock 
 * ---------------------------
 * lock the static gimpMutex singleton if present (e.g. is NOT NULL)
 */
void
gap_base_gimp_mutex_lock(GapTimmRecord  *gimpMutexStats);


/* ---------------------------
 * gap_base_gimp_mutex_unlock 
 * ---------------------------
 * lock the static gimpMutex singleton if present (e.g. is NOT NULL)
 */
void
gap_base_gimp_mutex_unlock(GapTimmRecord  *gimpMutexStats);





#endif
