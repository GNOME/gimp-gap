/* gap_timm.c
 *    by hof (Wolfgang Hofer)
 *    simple runtime measuring procedures.
 *    Restrictions: 
 *     - current implementation does not support measuring of recursive procedure calls
 *     - measure results in multithread environment may be falsified
 *       due to wait cycles while locking the mutex.
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
 * along with this program; if not, write to the Free Software
 * <http://www.gnu.org/licenses/>.
 */

/* revision history:
 * version 2.7.0;             hof: created
 */

/* SYTEM (UNIX) includes */
#include "config.h"
#include "string.h"
/* GIMP includes */
/* GAP includes */
#include "gap_timm.h"
#include "gap_base.h"


#define GAP_TIMM_MAX_ELEMENTS 500
#define GAP_TIMM_MAX_THREADS  30
#define GAP_TIMM_MAX_FUNCNAME 65

typedef struct GapTimmElement
{
  gint32    funcId;
  char      funcName[GAP_TIMM_MAX_FUNCNAME];
  gint32    maxThreadIdx;
  gint64    funcThreadId[GAP_TIMM_MAX_THREADS];
  gboolean  isStartTimeRecorded[GAP_TIMM_MAX_THREADS];
  GTimeVal  startTime[GAP_TIMM_MAX_THREADS];
  guint32   numberOfCallsStarted;
  guint32   numberOfCallsFinished;
  gboolean  errorFlag;
  guint64   summaryDuration;
  guint64   minDuration;
  guint64   maxDuration;
  GMutex   *funcMutex;
 
} GapTimmElement;

typedef struct GapTimmData
{
  GMutex         *mutex;
  GapTimmElement *tab;
  gint32          tabSizeInElements;
  gint32          maxFuncId;
  gboolean        isMultiThreadSupport;
} GapTimmData;



extern      int gap_debug; /* ==0  ... dont print debug infos */

static GapTimmData timmData =
{
  NULL   /* GMutex  *mutex; */
, NULL   /* GapTimmElement *tab;  */
, 0      /* gint32          tabSizeInElements; */
, 0      /* gint32          maxFuncId; */

};


static void 
p_initGapTimmData()
{
  if (timmData.tab == NULL)
  {
    /* check and init thread system */
    timmData.isMultiThreadSupport = gap_base_thread_init();
    
    if(timmData.isMultiThreadSupport == TRUE)
    {
      timmData.mutex = g_mutex_new();
    }
    timmData.maxFuncId = -1;
    timmData.tabSizeInElements = GAP_TIMM_MAX_ELEMENTS;
    timmData.tab = g_malloc0(timmData.tabSizeInElements * sizeof(GapTimmElement));
  }
}

static guint64
p_timespecDiff(GTimeVal *startTimePtr, GTimeVal *endTimePtr)
{
  return ((endTimePtr->tv_sec * G_USEC_PER_SEC) + endTimePtr->tv_usec) -
           ((startTimePtr->tv_sec * G_USEC_PER_SEC) + startTimePtr->tv_usec);
}


/* ---------------------------------
 * p_tim_mutex_lock
 * ---------------------------------
 * lock the full timm table
 */
static void
p_tim_mutex_lock()
{
  if(timmData.mutex)
  {
    g_mutex_lock(timmData.mutex);
  }
}

/* ---------------------------------
 * p_tim_mutex_unlock
 * ---------------------------------
 * unlock the full timm table
 */
static void
p_tim_mutex_unlock()
{
  if(timmData.mutex)
  {
    g_mutex_unlock(timmData.mutex);
  }
}

/* ---------------------------------
 * p_get_threadIndex
 * ---------------------------------
 * get index for current funcId and current thread.
 * (tab entries  where 
 */
static gint
p_get_threadIndex(gint32 funcId)
{
  gint    idx;
  gint    firstFreeIdx;
  gint64  treadId;
  
  treadId = gap_base_get_thread_id();

  p_tim_mutex_lock();
  
  firstFreeIdx = -1;
  
  for(idx = 0; idx <= timmData.tab[funcId].maxThreadIdx; idx++)
  {
    if(timmData.tab[funcId].funcThreadId[idx] == treadId)
    {
      p_tim_mutex_unlock();
      return(idx);
    }
    if ((firstFreeIdx < 0)
    && (timmData.tab[funcId].isStartTimeRecorded[idx] == FALSE))
    {
      firstFreeIdx = idx;
    }
  }
  
  if (firstFreeIdx >= 0)
  {
    timmData.tab[funcId].funcThreadId[firstFreeIdx] = treadId;
    p_tim_mutex_unlock();
    return (firstFreeIdx);
  
  }
  
  firstFreeIdx = timmData.tab[funcId].maxThreadIdx +1;
  if(firstFreeIdx < GAP_TIMM_MAX_THREADS)
  {
    timmData.tab[funcId].maxThreadIdx = firstFreeIdx;
    timmData.tab[funcId].isStartTimeRecorded[firstFreeIdx] = FALSE;
    timmData.tab[funcId].funcThreadId[firstFreeIdx] = treadId;
    p_tim_mutex_unlock();
    return (firstFreeIdx);
  }
  
  printf("p_get_threadIndex: funcID:%d  ERROR more than %d parallel threads! (measured runtime results will wrong for this funcId..)\n"
     ,(int)funcId
     ,(int)GAP_TIMM_MAX_THREADS
     );
     
  p_tim_mutex_unlock();
  
  return (0);
  
}  /* end p_get_threadIndex */




/* ---------------------------------
 * gap_timm_get_function_id
 * ---------------------------------
 * returns a unique funcId for the specified function name.
 * note that this id is
 * (the returned id may differ for the same functionName 
 *  when this procedure is called in another session or another process)
 *
 */
gint32
gap_timm_get_function_id(const char *functionName)
{
  gint32 ii;

  
  p_initGapTimmData();

  if(timmData.mutex)
  {
    g_mutex_lock(timmData.mutex);
  }

  for(ii=0; ii <= timmData.maxFuncId; ii++)
  {
    if(strcmp(functionName, timmData.tab[ii].funcName) == 0)
    {
      if(timmData.mutex)
      {
        g_mutex_unlock(timmData.mutex);
      }
      return(timmData.tab[ii].funcId);
    }
  }
  if (timmData.maxFuncId < timmData.tabSizeInElements -1)
  {
    /* init element for the new funcId */
    timmData.tab[ii].funcThreadId[0] = gap_base_get_thread_id();
    timmData.tab[ii].maxThreadIdx = 0;

    timmData.maxFuncId++;
    g_snprintf(&timmData.tab[ii].funcName[0], GAP_TIMM_MAX_FUNCNAME -1
               ,"%s"
               ,functionName
            );
    timmData.tab[ii].isStartTimeRecorded[0] = FALSE;
    timmData.tab[ii].numberOfCallsStarted = 0;
    timmData.tab[ii].numberOfCallsFinished = 0;
    timmData.tab[ii].errorFlag = FALSE;
    timmData.tab[ii].summaryDuration = 0;
    timmData.tab[ii].minDuration = 0;
    timmData.tab[ii].maxDuration = 0;
    timmData.tab[ii].funcMutex = NULL;
    if(timmData.isMultiThreadSupport)
    {
      timmData.tab[ii].funcMutex = g_mutex_new();
    }
  }

  if(timmData.mutex)
  {
    g_mutex_unlock(timmData.mutex);
  }
  return (timmData.maxFuncId);
  
}  /* end gap_timm_get_function_id */


/* ---------------------------------
 * gap_timm_start_function
 * ---------------------------------
 * record start time for the function identified by funcId.
 */
void
gap_timm_start_function(gint32 funcId)
{
  p_initGapTimmData();
  if((funcId >= 0) && (funcId<=timmData.maxFuncId))
  {
    gint threadIdx;
    if(timmData.tab[funcId].funcMutex)
    {
      g_mutex_lock(timmData.tab[funcId].funcMutex);
    }
    
    
    threadIdx = p_get_threadIndex(funcId);
    
    timmData.tab[funcId].numberOfCallsStarted++;
    if(timmData.tab[funcId].isStartTimeRecorded[threadIdx])
    {
      timmData.tab[funcId].errorFlag = TRUE;
    }
    else
    {
      timmData.tab[funcId].isStartTimeRecorded[threadIdx] = TRUE;
    }
    g_get_current_time(&timmData.tab[funcId].startTime[threadIdx]);

    if(timmData.tab[funcId].funcMutex)
    {
      g_mutex_unlock(timmData.tab[funcId].funcMutex);
    }
  }
  else
  {
    printf("gap_timm_start_function: ERROR unsupported funcId:%d\n"
      ,(int)funcId
      );
  }
  
}  /* end gap_timm_start_function */


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
void
gap_timm_stop_function(gint32 funcId)
{
  p_initGapTimmData();
  if((funcId >= 0) && (funcId<=timmData.maxFuncId))
  {
    gint threadIdx;
    if(timmData.tab[funcId].funcMutex)
    {
      g_mutex_lock(timmData.tab[funcId].funcMutex);
    }
    
    
    threadIdx = p_get_threadIndex(funcId);
    
    if(timmData.tab[funcId].isStartTimeRecorded[threadIdx])
    {
       GTimeVal  stopTime;
       guint64   duration;
       
       g_get_current_time(&stopTime);
       duration = p_timespecDiff(&timmData.tab[funcId].startTime[threadIdx]
                                ,&stopTime
                                );
       timmData.tab[funcId].summaryDuration += duration;
       if(duration > timmData.tab[funcId].maxDuration)
       {
         timmData.tab[funcId].maxDuration = duration;
       }
       if ((duration < timmData.tab[funcId].minDuration)
       || (timmData.tab[funcId].numberOfCallsFinished == 0))
       {
         timmData.tab[funcId].minDuration = duration;
       }
       timmData.tab[funcId].numberOfCallsFinished++;
       timmData.tab[funcId].isStartTimeRecorded[threadIdx] = FALSE;
    }
    else
    {
      timmData.tab[funcId].errorFlag = TRUE;
      if(gap_debug)
      {
        printf("gap_timm_stop_function: ERROR no startTime was found for funcId:%d threadId:%d threadIdx:%d(%s)\n"
          ,(int)funcId
          ,(int)timmData.tab[funcId].funcThreadId[threadIdx]
          ,(int)threadIdx
          ,&timmData.tab[funcId].funcName[0]
          );
      }
    }
    
    if(timmData.tab[funcId].funcMutex)
    {
      g_mutex_unlock(timmData.tab[funcId].funcMutex);
    }
  }
  else
  {
    printf("gap_timm_stop_function: ERROR unsupported funcId:%d\n"
      ,(int)funcId
      );
  }
  
}  /* end gap_timm_stop_function */


/* ---------------------------------
 * gap_timm_print_statistics
 * ---------------------------------
 * print runtime statistics for all recorded funcId's.
 */
void
gap_timm_print_statistics()
{
  gint32 ii;
  
  p_initGapTimmData();
  p_tim_mutex_lock();
  
  printf("gap_timm_print_statistics runtime recording has %d entries:\n"
        ,(int)timmData.maxFuncId +1
        );
  
  for(ii=0; ii <= timmData.maxFuncId; ii++)
  {
    guint64 avgDuration;
    avgDuration = 0;
    if(timmData.tab[ii].numberOfCallsFinished > 0)
    {
      avgDuration = timmData.tab[ii].summaryDuration / timmData.tab[ii].numberOfCallsFinished;
    }
    
    printf("id:%03d %-65.65s calls:%06u sum:%llu min:%llu max:%llu avg:%llu"
      , (int) ii
      , &timmData.tab[ii].funcName[0]
      , (int) timmData.tab[ii].numberOfCallsFinished
      , timmData.tab[ii].summaryDuration
      , timmData.tab[ii].minDuration
      , timmData.tab[ii].maxDuration
      , avgDuration
      );
    if(timmData.tab[ii].errorFlag)
    {
      printf("(Err)");
    }
    if(timmData.tab[ii].numberOfCallsFinished != timmData.tab[ii].numberOfCallsStarted)
    {
      printf("(callsStarted:%d)"
        , (int)timmData.tab[ii].numberOfCallsStarted
        );
    }
    printf(" usecs\n");
    
  }
  fflush(stdout);
  p_tim_mutex_unlock();

}  /* end gap_timm_print_statistics */






/// recording features without function id
/// those functions do not syncronize

/* ---------------------------------
 * gap_timm_init_record
 * ---------------------------------
 * reset the specified record
 */
void
gap_timm_init_record(GapTimmRecord *timmp)
{
  if(timmp == NULL)
  {
    return;
  }
  timmp->numberOfCalls         = 0;
  timmp->isStartTimeRecorded   = FALSE;
  timmp->summaryDuration       = 0;
  timmp->minDuration           = 0;
  timmp->maxDuration           = 0;
  g_get_current_time(&timmp->startTime);

}  /* end gap_timm_init_record */

/* ---------------------------------
 * gap_timm_start_record
 * ---------------------------------
 * record start time for the function identified by funcId.
 */
void
gap_timm_start_record(GapTimmRecord *timmp)
{
  if(timmp == NULL)
  {
    return;
  }
  timmp->numberOfCalls++;
  timmp->isStartTimeRecorded = TRUE;
  g_get_current_time(&timmp->startTime);
}  /* end gap_timm_start_record */


/* ---------------------------------
 * gap_timm_stop_record
 * ---------------------------------
 * calculate the duration since start time recording
 */
void
gap_timm_stop_record(GapTimmRecord *timmp)
{
  GTimeVal  stopTime;
  guint64   duration;

  if(timmp == NULL)
  {
    return;
  }
  
  if(timmp->isStartTimeRecorded)
  {
    g_get_current_time(&stopTime);
    duration = p_timespecDiff(&timmp->startTime, &stopTime);
    timmp->summaryDuration += duration;
    if(duration > timmp->maxDuration)
    {
      timmp->maxDuration = duration;
    }
    if ((duration < timmp->minDuration)
    || (timmp->numberOfCalls == 0))
    {
      timmp->minDuration = duration;
    }
    timmp->isStartTimeRecorded = FALSE;
  }
  
}  /* end gap_timm_stop_function */


/* ---------------------------------
 * gap_timm_print_record
 * ---------------------------------
 * print runtime statistics for all recorded funcId's.
 */
void
gap_timm_print_record(GapTimmRecord *timmp, const char *functionName)
{
  guint64 avgDuration;

  if(timmp == NULL)
  {
    return;
  }


  avgDuration = 0;
  if(timmp->numberOfCalls > 0)
  {
    avgDuration = timmp->summaryDuration / timmp->numberOfCalls;
  }
    
  printf("tim: %-65.65s calls:%06u sum:%llu min:%llu max:%llu avg:%llu usecs\n"
      , functionName
      , (int) timmp->numberOfCalls
      , timmp->summaryDuration
      , timmp->minDuration
      , timmp->maxDuration
      , avgDuration
      );

}  /* end gap_timm_print_record */



