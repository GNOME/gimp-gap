/* vid_api_mp_util.c
 *
 * GAP Video read API multiprocessor support utility procedures.
 *
 * 2010.11.21   hof created
 *
 */




#define GVA_MAX_MEMCPD_THREADS 16


typedef struct GapMultiPocessorCopyOrDelaceData {  /* memcpd */
    GVA_RgbPixelBuffer *rgbBuffer;
    guchar             *src_data;        /* source buffer data at same size and bpp as described by rgbBuffer */
    gint                startCol;        /* relevant for deinterlacing copy */
    gint                colWidth;        /* relevant for deinterlacing copy */
    gint                memRow;          /* relevant for simple memcpy */
    gint                memHeightInRows; /* relevant for simple memcpy */
    gint                cpuId;
  
    GapTimmRecord       memcpyStats;
    GapTimmRecord       delaceStats;
    
    
    gint                isFinished;
    
} GapMultiPocessorCopyOrDelaceData;




/* ---------------------------
 * GVA_fcache_mutex_lock
 * ---------------------------
 * lock the fcache_mutex if present (i.e. is NOT NULL)
 * Note: the fcache_mutex is NULL per default.
 *       In case an application wants to use the GVA api fcache in multithread environment,
 *       it must provide a mutex.
 *       example how to provide the mutex:
 *       (i.e gvahand->fcache_mutex = g_mutex_new() )
 */
void
GVA_fcache_mutex_lock(t_GVA_Handle  *gvahand)
{
  if(gvahand->fcache_mutex)
  {
    GAP_TIMM_START_RECORD(&gvahand->fcacheMutexLockStats);

    g_mutex_lock (gvahand->fcache_mutex);

    GAP_TIMM_STOP_RECORD(&gvahand->fcacheMutexLockStats);
  }

}  /* end GVA_fcache_mutex_lock */


/* ---------------------------
 * GVA_fcache_mutex_trylock
 * ---------------------------
 * lock the fcache_mutex if present (i.e. is NOT NULL)
 * return immediate FALSE in case the mutex is locked by another thread
 * return TRUE in case the mutex was locked successfully (may sleep until other threads unlock the mutex)
 *        TRUE will be immediatly returned in case
 *        a) thread system is not initialized, i.e g_thread_init was not yet called
 *        b) in case the gvahand->fcache_mutex is NULL (which is default after opening a videohandle)
 */
gboolean
GVA_fcache_mutex_trylock(t_GVA_Handle  *gvahand)
{
  gboolean isSuccessful;

  if(gvahand->fcache_mutex)
  {
    GAP_TIMM_START_RECORD(&gvahand->fcacheMutexLockStats);

    isSuccessful = g_mutex_trylock (gvahand->fcache_mutex);

    GAP_TIMM_STOP_RECORD(&gvahand->fcacheMutexLockStats);
  }
  else
  {
    /* not really locked, because no mutex is available.
     * but in this case behave same as g_mutex_trylock
     * when thread system is not initialized, i.e g_thread_init was not yet called
     */
    isSuccessful = TRUE;
  }

  return(isSuccessful);
  
}  /* end GVA_fcache_mutex_trylock */


/* ---------------------------
 * GVA_fcache_mutex_unlock
 * ---------------------------
 * unlock the fcache_mutex if present (i.e. is NOT NULL)
 */
void
GVA_fcache_mutex_unlock(t_GVA_Handle  *gvahand)
{
  if(gvahand->fcache_mutex)
  {
    g_mutex_unlock (gvahand->fcache_mutex);
  }
}  /* end GVA_fcache_mutex_unlock */



/* ------------------------------------------
 * p_copyAndDeinterlaceIntoRgbBuffer
 * ------------------------------------------
 * make a deinterlaced copy of the source buffer rectangular area 
 * starting at startCol
 * Note that the source buffer is allocated at the full frame size and must
 * match with the full size of the drawable that holds the currently processed tile.
 *
 * RESTRICTION: dst drawable and srcBuff must have the same width, height and bpp.
 */
static void
p_copyAndDeinterlaceRgbBufferToPixelRegion (const guchar *src_data
                    , GVA_RgbPixelBuffer *rgbBuffer
                    , gint32 startCol, gint32 stripeWidthInCols)
{
  guint          row;
  const guchar*  src;
  guchar*        dest;
  gint32         stripeWidthInBytes;
  gint32         startOffestInBytes;

  gint32  l_interpolate_flag;
  gint32  l_mix_threshold;

  l_interpolate_flag = gva_delace_calculate_interpolate_flag(rgbBuffer->deinterlace);
  l_mix_threshold = gva_delace_calculate_mix_threshold(rgbBuffer->threshold);
  
  stripeWidthInBytes = (stripeWidthInCols * rgbBuffer->bpp);
  startOffestInBytes = startCol * rgbBuffer->bpp;
  
  src = src_data + startOffestInBytes;
  dest = rgbBuffer->data + startOffestInBytes;


  for (row = 0; row < rgbBuffer->height; row++)
  {
     if ((row & 1) == l_interpolate_flag)
     {
       if(row == 0)
       {
         /* we have no prvious row, so we just copy the next row */
         memcpy(dest, src + rgbBuffer->rowstride, stripeWidthInBytes);
       }
       else if (row == rgbBuffer->height -1 )
       {
         /* we have no next row, so we just copy the prvious row */
         memcpy(dest, src - rgbBuffer->rowstride, stripeWidthInBytes);
       }
       else
       {
         /* we have both prev and next row within valid range
          * and can calculate an interpolated tile row
          */
         gva_delace_mix_rows ( stripeWidthInCols
                       , rgbBuffer->bpp
                       , rgbBuffer->rowstride
                       , l_mix_threshold
                       , src - rgbBuffer->rowstride   /* prev_row */
                       , src + rgbBuffer->rowstride   /* next_row */
                       , dest                       /* mixed_row (to be filled) */
                       );
       }
     }
     else
     {
       /* copy original row */
       memcpy(dest, src, stripeWidthInBytes);
     }
     
     src  += rgbBuffer->rowstride;
     dest += rgbBuffer->rowstride;
  }
  
}  /* end p_copyAndDeinterlaceRgbBufferToPixelRegion */



/* --------------------------------------------
 * p_memcpy_or_delace_WorkerThreadFunction
 * --------------------------------------------
 * this function runs in concurrent parallel worker threads.
 * each one of the parallel running threads processes another portion of the frame memory
 * when deinterlacing is requird the portions are column stripes starting at startCol and are colWidth wide. 
 * For simple memcpy the portions are memory blocks (row stripes starting at memRow Height)
 *
 * this procedure records runtime values using GAP_TIMM_ macros 
 *  (this debug feature is only available in case runtime recording was configured at compiletime)
 */
static void
p_memcpy_or_delace_WorkerThreadFunction(GapMultiPocessorCopyOrDelaceData *memcpd)
{
//  if(gap_debug)
//  {
//    printf("p_memcpy_or_delace_WorkerThreadFunction: START cpu[%d]\n"
//      ,(int)memcpd->cpuId
//      );
//  }  
     
  if (memcpd->rgbBuffer->deinterlace != 0)
  {
    GAP_TIMM_START_RECORD(&memcpd->delaceStats);
    
    p_copyAndDeinterlaceRgbBufferToPixelRegion (memcpd->src_data
                        , memcpd->rgbBuffer
                        , memcpd->startCol
                        , memcpd->colWidth
                        );
    GAP_TIMM_STOP_RECORD(&memcpd->delaceStats);
  }
  else
  {
    guchar*  src;
    guchar*  dest;
    gint32   startOffestInBytes;

    GAP_TIMM_START_RECORD(&memcpd->memcpyStats);
  
    startOffestInBytes = memcpd->memRow * memcpd->rgbBuffer->rowstride;
  
    src = memcpd->src_data + startOffestInBytes;
    dest = memcpd->rgbBuffer->data + startOffestInBytes;
    
    memcpy(dest, src, memcpd->memHeightInRows * memcpd->rgbBuffer->rowstride);

    GAP_TIMM_STOP_RECORD(&memcpd->memcpyStats);
  }

//  if(gap_debug)
//  {
//    printf("p_memcpy_or_delace_WorkerThreadFunction: DONE cpu[%d]\n"
//      ,(int)memcpd->cpuId
//      );
//  }  


  memcpd->isFinished = TRUE;
  
   
}  /* end p_memcpy_or_delace_WorkerThreadFunction */




/* ----------------------------------------------------
 * GVA_copy_or_deinterlace_fcache_data_to_rgbBuffer
 * ----------------------------------------------------
 * copy (or deinterlaced copy) the specified srcFrameData into the specified
 * rgbBuffer.
 * in case the specified numProcessors > 1 this procedure uses a thread pool
 * to spread the work to the given number of processors.
 *
 * calling this procedure with rgbBuffer == NULL triggers logging
 * of captured runtime statistic values per thread
 * (in case gimp-gap was comiled with runtime recording configuration)
 * (this is a debug feature for development and analyse purpose only)
 */
void
GVA_copy_or_deinterlace_fcache_data_to_rgbBuffer(GVA_RgbPixelBuffer *rgbBuffer
                                                , guchar *srcFrameData
                                                , gint32 numProcessors)
{
  static GapMultiPocessorCopyOrDelaceData  memcpdArray[GVA_MAX_MEMCPD_THREADS];

  static GThreadPool  *threadPool = NULL;
  static gulong        usleepTime = 10;
  static gint          numThreadsMax         = 1;
  gboolean             isMultithreadEnabled;
  gint                 numThreads;
  gint                 colsPerCpu;
  gint                 rowsPerCpu;
  gint                 retry;
  gint                 startCol;
  gint                 colWidth;
  gint                 startRow;
  gint                 rowHeight;
  gint                 ii;
  GapMultiPocessorCopyOrDelaceData *memcpd;
  GError *error;

  static gint32 funcId = -1;
  static gint32 funcIdPush = -1;
  static gint32 funcIdSingle = -1;
  static gint32 funcIdMainWait = -1;

  GAP_TIMM_GET_FUNCTION_ID(funcId, "GVA_copy_or_deinterlace_fcache_data_to_rgbBuffer");
  GAP_TIMM_GET_FUNCTION_ID(funcIdPush, "GVA_copy_or_deinterlace_fcache_data_to_rgbBuffer.pushToReActivateThreads");
  GAP_TIMM_GET_FUNCTION_ID(funcIdSingle, "GVA_copy_or_deinterlace_fcache_data_to_rgbBuffer.singlecall");
  GAP_TIMM_GET_FUNCTION_ID(funcIdMainWait, "GVA_copy_or_deinterlace_fcache_data_to_rgbBuffer.main (Wait)");

  error = NULL;
  ii = 0;

  if(gap_debug)
  {
    printf("GVA_copy_or_deinterlace_fcache_data_to_rgbBuffer START numProcessors:%d\n"
      ,(int)numProcessors
      );
  }
  
  /* rgbBuffer NULL pointer triggers a debug feature that logs the recorded runtime statistics to stdout.
   * (but only in gimp-gap was configured for runtime recording at compiletime)
   */
  if(rgbBuffer == NULL)
  {
    for(ii=0; ii < numThreadsMax; ii++)
    {
      GapMultiPocessorCopyOrDelaceData *memcpd;
      memcpd = &memcpdArray[ii];
      
      GAP_TIMM_PRINT_RECORD(&memcpd->memcpyStats,   "... Thrd GVA_fcache_to_drawable_multithread.memcpyStats");
      GAP_TIMM_PRINT_RECORD(&memcpd->delaceStats, "... Thrd GVA_fcache_to_drawable_multithread.delaceStats");
    }
    
    return;
  }
   

  GAP_TIMM_START_FUNCTION(funcId);

  numThreads = MIN(numProcessors, GVA_MAX_MEMCPD_THREADS);
  numThreadsMax = MAX(numThreadsMax, numThreads);
  
  colsPerCpu = (rgbBuffer->width + (numThreads -1)) / numThreads;
  rowsPerCpu = (rgbBuffer->height + (numThreads -1)) / numThreads;

  /* check and init thread system */
  if(numThreads > 1)
  {
    isMultithreadEnabled = gap_base_thread_init();
  }
  else
  {
    isMultithreadEnabled = FALSE;
  }
  
  if((isMultithreadEnabled != TRUE)
  || (colsPerCpu < 16)
  || (rowsPerCpu < 16))
  {
    GAP_TIMM_START_FUNCTION(funcIdSingle);
    /* singleproceesor variant calls the worker thread once with setup
     * to process full buffer in one synchron call
     * (no locks are set in this case and no condition will be sent)
     */
    memcpd = &memcpdArray[0];
    memcpd->src_data = srcFrameData;
    memcpd->rgbBuffer = rgbBuffer;
    
    memcpd->startCol        = 0;
    memcpd->colWidth        = rgbBuffer->width;
    memcpd->memRow          = 0;
    memcpd->memHeightInRows = rgbBuffer->height;
    memcpd->cpuId = ii;
    memcpd->isFinished = FALSE;
    
    p_memcpy_or_delace_WorkerThreadFunction(memcpd);
    
    GAP_TIMM_STOP_FUNCTION(funcIdSingle);
    GAP_TIMM_STOP_FUNCTION(funcId);
    return;
  }


    
  if (threadPool == NULL)
  {
    usleepTime = gap_base_get_gimprc_int_value("video-api-mp-copy-or-delace-usleep"
                  , 10
                  , 1
                  , 10000);
    
    /* init the treadPool at first multiprocessing call
     * (and keep the threads until end of main process..)
     */
    threadPool = g_thread_pool_new((GFunc) p_memcpy_or_delace_WorkerThreadFunction
                                         ,NULL        /* user data */
                                         ,GVA_MAX_MEMCPD_THREADS          /* max_threads */
                                         ,TRUE        /* exclusive */
                                         ,&error      /* GError **error */
                                         );

    for(ii=0; ii < GVA_MAX_MEMCPD_THREADS; ii++)
    {
      GapMultiPocessorCopyOrDelaceData *memcpd;
      memcpd = &memcpdArray[ii];
      
      GAP_TIMM_INIT_RECORD(&memcpd->memcpyStats);
      GAP_TIMM_INIT_RECORD(&memcpd->delaceStats);
    }
  }

  
  if(gap_debug)
  {
    printf("GVA_copy_or_deinterlace_fcache_data_to_rgbBuffer size:%d x %d numThreads:%d colsPerCpu:%d rowsPerCpu:%d\n"
      ,(int)rgbBuffer->width
      ,(int)rgbBuffer->height
      ,(int)numThreads
      ,(int)colsPerCpu
      ,(int)rowsPerCpu
      );
  }

  startCol = 0;
  startRow = 0;
  colWidth = colsPerCpu;
  rowHeight = rowsPerCpu;
  

  GAP_TIMM_START_FUNCTION(funcIdPush);
 
  /* build work packet-stripes and re-start one thread per stripe */
  for(ii=0; ii < numThreads; ii++)
  {
    GapMultiPocessorCopyOrDelaceData *memcpd;

    if(gap_debug)
    {
      printf("GVA_copy_or_deinterlace.. Cpu[%d] startCol:%d colWidth:%d startRow:%d rowHeight:%d delace:%d\n"
        ,(int)ii
        ,(int)startCol
        ,(int)colWidth
        ,(int)startRow
        ,(int)rowHeight
        ,(int)rgbBuffer->deinterlace
        );
    }
    
    memcpd = &memcpdArray[ii];
    memcpd->src_data = srcFrameData;
    memcpd->rgbBuffer = rgbBuffer;
    
    memcpd->startCol        = startCol;
    memcpd->colWidth        = colWidth;
    memcpd->memRow          = startRow;
    memcpd->memHeightInRows = rowHeight;
    memcpd->cpuId = ii;
    memcpd->isFinished = FALSE;
    
    /* (re)activate next thread */
    g_thread_pool_push (threadPool
                       , memcpd    /* user Data for the worker thread*/
                       , &error
                       );
    startCol += colsPerCpu;
    if((startCol + colsPerCpu) >  rgbBuffer->width)
    {
      /* the last thread handles a vertical stripe with the remaining columns
       */
      colWidth = rgbBuffer->width - startCol;
    }
    startRow += rowsPerCpu;
    if((startRow + rowsPerCpu) >  rgbBuffer->height)
    {
      /* the last thread handles a horizontal stripe with the remaining rows
       */
      colWidth = rgbBuffer->height - startRow;
    }
  }

  GAP_TIMM_STOP_FUNCTION(funcIdPush);


  /* now wait until all worker threads have finished thier tasks */
  retry = 0;
  while(TRUE)
  {
    gboolean isAllWorkDone;

    if(gap_debug)
    {
      printf("GVA_copy_or_deinterlace_fcache_data_to_rgbBuffer: WAIT retry :%d\n"
        ,(int)retry
        );
    }
  
    GAP_TIMM_START_FUNCTION(funcIdMainWait);

    /* sleep a very short time before next check
     * Note that longer sleep leaves more cpu resources for the prallel running threads
     * but increases the raster for overall performance time.
     * varaints using a mutex and wakeup cond did not perform better than this,
     * because the parallel running threads had additonal mutex locking overhead
     * (that is not necessary here where each tread operates on its own memory range
     * of the frame data)
     */
    g_usleep(usleepTime);
    
    GAP_TIMM_STOP_FUNCTION(funcIdMainWait);

    if(gap_debug)
    {
      printf("GVA_copy_or_deinterlace_fcache_data_to_rgbBuffer: WAKE-UP retry :%d\n"
        , (int)retry
        );
    }


    /* assume all threads finished work (this may already be the case after the first retry) */
    isAllWorkDone = TRUE;
    
    for(ii=0; ii < numThreads; ii++)
    {
      memcpd = &memcpdArray[ii];
      if(gap_debug)
      {
        printf("GVA_fcache_to_drawable_multithread: STATUS Cpu[%d] retry :%d  isFinished:%d\n"
          ,(int)ii
          ,(int)retry
          ,(int)memcpd->isFinished
          );
      }
      if(memcpd->isFinished != TRUE)
      {
        isAllWorkDone = FALSE;
        break;
      }
    }

    if(isAllWorkDone == TRUE)
    {
      break;
    }

    retry++;
  }

  GAP_TIMM_STOP_FUNCTION(funcId);

  if(gap_debug)
  {
    printf("GVA_copy_or_deinterlace_fcache_data_to_rgbBuffer: DONE retry :%d\n"
        , (int)retry
        );
  }
  
}  /* end GVA_copy_or_deinterlace_fcache_data_to_rgbBuffer */


/* ----------------------------------------------------
 * GVA_copy_or_delace_print_statistics
 * ----------------------------------------------------
 */
void
GVA_copy_or_delace_print_statistics()
{
  GVA_copy_or_deinterlace_fcache_data_to_rgbBuffer(NULL, NULL, 0);
}

