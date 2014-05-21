/* gap_timm.h
 *    by hof (Wolfgang Hofer)
 *    runtime measuring procedures
 *    provides MACROS for runtime recording
 *       the xx_FUNCTION macros capture runtime results in a static table by functionId and threadId
 *                       (this requires a mutex that may affect the measured results due to synchronisation)
 *       the xx_RECORD macros capture values in the buffer provided by the caller (and no mutex locking is done)
 *
 *    Note that the timm proecures shall be called via the MACROS
 *
 *  2010/10/19
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
 * version 2.7.0;             hof: created
 */

#ifndef _GAP_TIMM_H
#define _GAP_TIMM_H

/* SYTEM (UNIX) includes */
#include <stdio.h>
#include <stdlib.h>

/* GIMP includes */
#include "gtk/gtk.h"
#include "libgimp/gimp.h"


typedef struct GapTimmRecord
{
  gboolean  isStartTimeRecorded;
  GTimeVal  startTime;
  guint32   numberOfCalls;
  guint64   summaryDuration;
  guint64   minDuration;
  guint64   maxDuration;
 
} GapTimmRecord;


/* macros to enable runtime recording function calls at compiletime.
 * in case GAP_RUNTIME_RECORDING_NOLOCK is not defined at compiletime
 * the macros expand to nothing, e.g the runtime recording function calls
 * are not compiled at all.
 */
#ifdef GAP_RUNTIME_RECORDING_NOLOCK

#define GAP_TIMM_INIT_RECORD(timmp)  gap_timm_init_record(timmp)
#define GAP_TIMM_START_RECORD(timmp) gap_timm_start_record(timmp)
#define GAP_TIMM_STOP_RECORD(timmp)  gap_timm_stop_record(timmp)
#define GAP_TIMM_PRINT_RECORD(timmp, functionName)  gap_timm_print_record(timmp, functionName)

#else

#define GAP_TIMM_INIT_RECORD(timmp)
#define GAP_TIMM_START_RECORD(timmp)
#define GAP_TIMM_STOP_RECORD(timmp)
#define GAP_TIMM_PRINT_RECORD(timmp, functionName)


#endif

/* macros to enable runtime recording function calls at compiletime.
 * in case GAP_RUNTIME_RECORDING_LOCK is not defined at compiletime
 * the macros expand to nothing, so that the runtime recording function calls
 * are not compiled at all.
 * 2014.03.19  GAP_TIMM_GET_FUNCTION_ID now expands to dummy code
 *             in case GAP_RUNTIME_RECORDING_LOCK is not defined
 *             (this eliminates compiler warnings)
 * Note that the xx_FUNCTION macros call g_mutex_lock
 */

#ifdef GAP_RUNTIME_RECORDING_LOCK

#define GAP_TIMM_GET_FUNCTION_ID(funcId, functionName)  if(funcId < 0) { funcId = gap_timm_get_function_id(functionName); }
#define GAP_TIMM_START_FUNCTION(funcId)                 gap_timm_start_function(funcId) 
#define GAP_TIMM_STOP_FUNCTION(funcId)                  gap_timm_stop_function(funcId)
#define GAP_TIMM_PRINT_FUNCTION_STATISTICS()            gap_timm_print_statistics()

#else

#define GAP_TIMM_GET_FUNCTION_ID(funcId, functionName)  if(funcId < 0) { funcId = 0; }
#define GAP_TIMM_START_FUNCTION(funcId)
#define GAP_TIMM_STOP_FUNCTION(funcId)
#define GAP_TIMM_PRINT_FUNCTION_STATISTICS()

#endif



/* ---------------------------------
 * gap_timm_get_function_id
 * ---------------------------------
 * returns a unique funcId for the specified function name.
 * note that this id is
 * (the returned id may differ for the same functionName 
 *  when this procedure is called in another session or another process)
 *
 */
gint32 gap_timm_get_function_id(const char *functionName);


/* ---------------------------------
 * gap_timm_start_function
 * ---------------------------------
 * record start time for the function identified by funcId.
 */
void   gap_timm_start_function(gint32 funcId);


/* ---------------------------------
 * gap_timm_stop_function
 * ---------------------------------
 * in case a starttime was recorded for the specified funcId
 * calculate the duration since start time recording
 * and remove the recorded starttime.
 *
 * typically gap_timm_start_function shall be called at begin
 * of a function to be measured and gap_timm_stop_function
 * is called after the 'to be measurded' processing was done.
 *
 * in case there is no recorded start time
 * an error message is printed and no calculation is done.
 */
void   gap_timm_stop_function(gint32 funcId);


/* ---------------------------------
 * gap_timm_print_statistics
 * ---------------------------------
 * print runtime statistics for all recorded funcId's.
 */
void   gap_timm_print_statistics();



/* ---------------------------------
 * gap_timm_init_record
 * ---------------------------------
 * reset the specified record
 */
void
gap_timm_init_record(GapTimmRecord *timmp);


/* ---------------------------------
 * gap_timm_start_record
 * ---------------------------------
 * record start time for the function identified by funcId.
 */
void
gap_timm_start_record(GapTimmRecord *timmp);


/* ---------------------------------
 * gap_timm_stop_record
 * ---------------------------------
 * calculate the duration since start time recording
 */
void
gap_timm_stop_record(GapTimmRecord *timmp);


/* ---------------------------------
 * gap_timm_print_record
 * ---------------------------------
 * print runtime statistics for all recorded funcId's.
 */
void
gap_timm_print_record(GapTimmRecord *timmp, const char *functionName);


#endif
