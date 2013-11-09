/* gap_story_render_lossless.c
 *
 *
 *  GAP storyboard pseudo rendering for lossless video cut.
 *  (includes checks if frames in the referenced video source
 *   can be provided 1:1 as raw data chunks to the calling encoder
 *   and performs the raw data chunk fetch where possible
 *   and requested by the calling encoder)
 *
 * Copyright (C) 2008 Wolfgang Hofer <hof@gimp.org>
 *
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

/*
 * 2008.06.11  hof  - created (moved stuff from the former gap_gve_story_render_processor to this  new module)
 */

#ifdef GAP_ENABLE_VIDEOAPI_SUPPORT

/* ----------------------------------------------------
 * p_debug_print_vcodec_missmatch
 * ----------------------------------------------------
 */
static void
p_debug_print_vcodec_missmatch(const char *videofile
  ,GapCodecNameElem *vcodec_list
  ,const char *vcodec_name_chunk
)
{
   printf("chunk_fetch not possible, video codec missmatch. ");
   printf("videofile:");
   if(videofile)
   {
     printf("%s", videofile);
   }
   else
   {
     printf("NULL");
   }

   printf(", vcodec_list:");
   if(vcodec_list)
   {
     GapCodecNameElem *codec_elem;
     for(codec_elem = vcodec_list; codec_elem != NULL; codec_elem = (GapCodecNameElem *) codec_elem->next)
     {
       if(codec_elem->codec_name)
       {
         printf("%s ", codec_elem->codec_name);
       }
     }
   }
   else
   {
     printf("NULL");
   }


   printf(", vcodec_name_chunk:");
   if(vcodec_name_chunk)
   {
     printf("%s", vcodec_name_chunk);
   }
   else
   {
     printf("NULL");
   }
   
   printf("\n");
   
}  /* end p_debug_print_vcodec_missmatch */

/* ----------------------------------------------------
 * p_check_vcodec_name
 * ----------------------------------------------------
 * return TRUE if vcodec_name_chunk is found in the
 *             specified list of compatible codecs names
 *             (The vcodec_list is provided by the calling encoder
 *              and includes a list of compatible codec names for lossles video cut purpose)
 *        FALSE if vcodec check condition failed.
 *
 * Note: vcodec_name_chunk NULL (unknown) is never compatible,
 *       an empty list (vcodec_list == NULL) matches to
 *       all codec names (but not to unknown codec)
 */
static gboolean
p_check_vcodec_name(gint32 check_flags
    , GapCodecNameElem *vcodec_list, const char *vcodec_name_chunk)
{
  if (check_flags & GAP_VID_CHCHK_FLAG_VCODEC_NAME)
  {
    if(vcodec_name_chunk == NULL)
    {
      return (FALSE);
    }

    if(vcodec_list == NULL)
    {
      return (TRUE);
    }
    else
    {
      GapCodecNameElem *codec_elem;
      for(codec_elem = vcodec_list; codec_elem != NULL; codec_elem = (GapCodecNameElem *) codec_elem->next)
      {
        if(codec_elem->codec_name)
        {
          if(strcmp(codec_elem->codec_name, vcodec_name_chunk) == 0)
          {
            return (TRUE);
          }
        }
      }
    }
    return (FALSE);
  }
  
  return (TRUE);
}  /* end p_check_vcodec_name */


/* ----------------------------------------------------
 * p_check_flags_for_matching_vcodec
 * ----------------------------------------------------
 * return TRUE if vcodec names fit to the specified check_flags conditions,
 *        FALSE if vcodec check condition failed.
 *
 * Note: vcodec_name_encoder value * (or NULL pointer) matches always.
 */
static void
p_check_flags_for_matching_vcodec(gint32 check_flags, gint32 *check_flags_result
   , const char *videofile, GapCodecNameElem *vcodec_list, const char *vcodec_name_chunk)
{
  if (TRUE == p_check_vcodec_name(check_flags
                  ,vcodec_list
                  ,vcodec_name_chunk))
  {
    *check_flags_result |= (check_flags & GAP_VID_CHCHK_FLAG_VCODEC_NAME);
  }
  else
  {
    if(gap_debug)
    {
       p_debug_print_vcodec_missmatch(videofile, vcodec_list, vcodec_name_chunk);
     
    }
  }

}  /* end p_check_flags_for_matching_vcodec */



/* ----------------------------------------------------
 * p_chunk_fetch_from_single_image
 * ----------------------------------------------------
 * This procedure fetches frame images from disc
 *
 * if possible and if the check conditions according to specified check_flags
 * are fulfilled.
 *
 * TODO:  GAP_VID_CHCHK_FLAG_SIZE 
 *       (need to parsing image or load as image and let gimp do the parsing
 *        such a check can significantly reduce performance)
 *
 * return FALSE if fetch not possible or failed.
 */
static gboolean
p_chunk_fetch_from_single_image(const char *videofile
                          , unsigned char *video_frame_chunk_data  // IN/OUT
                          , gint32 video_frame_chunk_maxsize       // IN
                          , gint32 *video_frame_chunk_hdr_size     // OUT
                          , gint32 *video_frame_chunk_size         // OUT
                          , GapCodecNameElem *vcodec_list          // IN
                          , gint32 check_flags                     // IN
                          )
{
  const char *vcodec_name_chunk;
  gint32 fileSize;
  gint32 bytesRead;
  
  *video_frame_chunk_size = 0;
  *video_frame_chunk_hdr_size = 0;
  vcodec_name_chunk = NULL;
  
  /* no need to check GAP_VID_CHCHK_FLAG_FULL_FRAME
   * (single image fetch is always a full frame) 
   */
  
  if (check_flags & GAP_VID_CHCHK_FLAG_MPEG_INTEGRITY)
  {
    if(gap_debug)
    {
      printf("p_chunk_fetch_from_single_image: single image never fits MPEG_INTEGRITY (file:%s)\n"
          ,videofile
          );
    }
    return (FALSE);
  }
  
  fileSize = gap_file_get_filesize(videofile);
  if (fileSize > video_frame_chunk_maxsize)
  {
    if(gap_debug)
    {
      printf("p_chunk_fetch_from_single_image: fileSize:%d biiger than max chunk buffer:%d (file:%s)\n"
          ,(int)fileSize
          ,(int)video_frame_chunk_maxsize
          ,videofile
          );
    }
    return (FALSE);
  }

  if (check_flags & GAP_VID_CHCHK_FLAG_JPG)
  {
    
    bytesRead = gap_file_load_file_segment(videofile
                      ,video_frame_chunk_data
                      ,0            /* seek_index, start byte of datasegment in file */
                      ,16           /* segment size in byets (must be a multiple of 4) */
                      );
    if(TRUE != GVA_util_check_jpg_picture( video_frame_chunk_data
                                         , 32  /* video_frame_chunk_size */
                                         , 0  /* max_check_size  */
                                         , video_frame_chunk_hdr_size))
    {
      if(gap_debug)
      {
        printf("p_chunk_fetch_from_single_image: not a JPEG file (file:%s)\n"
            ,videofile
            );
      }
      return (FALSE);
    }
    vcodec_name_chunk = "JPEG";
  }

  if (check_flags & GAP_VID_CHCHK_FLAG_PNG)
  {
    
    bytesRead = gap_file_load_file_segment(videofile
                      ,video_frame_chunk_data
                      ,0            /* seek_index, start byte of datasegment in file */
                      ,16           /* segment size in byets (must be a multiple of 4) */
                      );
    if(TRUE != GVA_util_check_png_picture( video_frame_chunk_data
                                         , 32  /* video_frame_chunk_size */
                                         , 0  /* max_check_size  */
                                         , video_frame_chunk_hdr_size))
    {
      if(gap_debug)
      {
        printf("p_chunk_fetch_from_single_image: not a PNG file (file:%s)\n"
            ,videofile
            );
      }
      return (FALSE);
    }
    vcodec_name_chunk = "PNG ";
  }
  
  if (vcodec_name_chunk == NULL)
  {
    if (check_flags & GAP_VID_CHCHK_FLAG_VCODEC_NAME)
    {
       vcodec_name_chunk = NULL; // TODO p_get_vcodec_name_by_magic_number ....
    }
  }

  if (TRUE != p_check_vcodec_name(check_flags
                  ,vcodec_list
                  ,vcodec_name_chunk))
  {
    if(gap_debug)
    {
      p_debug_print_vcodec_missmatch(videofile, vcodec_list, vcodec_name_chunk);
    }
    return (FALSE);
  }
  
  bytesRead = gap_file_load_file_segment(videofile
                      ,video_frame_chunk_data
                      ,0            /* seek_index, start byte of datasegment in file */
                      ,fileSize           /* segment size in byets (must be a multiple of 4) */
                      );
  if (bytesRead != fileSize)
  {
    printf("p_chunk_fetch_from_single_image: ERROR failed reading %d bytes from file. (bytesRead:%d) (file:%s)\n"
            ,(int)fileSize
            ,(int)bytesRead
            ,videofile
            );
    return (FALSE);
  }
  
  *video_frame_chunk_size = bytesRead;
  return (TRUE);
  
}  /* end p_chunk_fetch_from_single_image */





/* ----------------------------------------------------
 * p_check_chunk_fetch_possible
 * ----------------------------------------------------
 * This procedure checks the preconditions for a possible
 * fetch of an already compressed frame chunk.
 * (a frame chunk can be one raw frame chunk fetched from a videofile
 *  or a single image frame file that shall be loaded 1:1 into memory)
 * - there is only 1 videoinput track at this master_frame_nr
 * - the videoframe must match 1:1 in size
 * - there are no transformations (opacity, offsets ....)
 *
 * return the name of the input videofile if preconditions are OK,
 *        or NULL if not.
 */
static char *
p_check_chunk_fetch_possible(GapStoryRenderVidHandle *vidhand
                    , gint32 master_frame_nr  /* starts at 1 */
                    , gint32  vid_width       /* desired Video Width in pixels */
                    , gint32  vid_height      /* desired Video Height in pixels */
                    , gint32 *video_frame_nr  /* OUT: corresponding frame number in the input video */
                    , GapStoryRenderFrameRangeElem **frn_elem  /* OUT: pointer to relevant frame range element */
                    )
{
  GapStbFetchData gapStbFetchData;
  GapStbFetchData *gfd;
  gint32    l_track;
  gchar  *l_framename;
  gchar  *l_videofile;

  gint    l_cnt_active_tracks;

  gfd = &gapStbFetchData;

  *video_frame_nr   = -1;
  *frn_elem = NULL;

  l_videofile = NULL;
  l_cnt_active_tracks = 0;

  /* findout if there is just one input track from type videofile
   * (that possibly could be fetched as comressed videoframe_chunk
   *  and passed 1:1 to the calling encoder)
   */
  for(l_track = vidhand->maxVidTrack; l_track >= vidhand->minVidTrack; l_track--)
  {
    l_framename = p_fetch_framename(vidhand->frn_list
                 , master_frame_nr /* starts at 1 */
                 , l_track
                 , gfd
                 );

     if(gap_debug)
     {
       printf("l_track:%d  gfd->frn_type:%d\n", (int)l_track, (int)gfd->frn_type);
     }

     if(gfd->frn_type != GAP_FRN_SILENCE)
     {
       l_cnt_active_tracks++;
     }

     if((l_framename) || (gfd->frn_type == GAP_FRN_COLOR))
     {
       if(l_framename)
       {
         if((gfd->frn_type == GAP_FRN_MOVIE)
         || (gfd->frn_type == GAP_FRN_IMAGE)
         || (gfd->frn_type == GAP_FRN_FRAMES))
         {
           if(l_cnt_active_tracks == 1)
           {
             /* check for transformations */
             if((gfd->opacity == 1.0)
             && (gfd->rotate == 0.0)
             && (gfd->scale_x == 1.0)
             && (gfd->scale_y == 1.0)
             && (gfd->move_x == 0.0)
             && (gfd->move_y == 0.0)
             && (gfd->fit_width)
             && (gfd->fit_height)
             && (!gfd->keep_proportions)
             && (gfd->frn_elem->flip_request == GAP_STB_FLIP_NONE)
             && (gfd->frn_elem->mask_name == NULL)
             && (gfd->trak_filtermacro_file == NULL)
             && (gfd->movepath_file_xml == NULL))
             {
               if(gap_debug)
               {
                 printf("gap_story_render_fetch_composite_image_or_chunk:  video:%s\n", l_framename);
               }
               l_videofile = g_strdup(l_framename);
               *video_frame_nr = gfd->localframe_index;
               *frn_elem = gfd->frn_elem;
             }
             else
             {
               if(gap_debug)
               {
                 printf("gap_story_render_fetch_composite_image_or_chunk:  there are transformations\n");
               }
               /* there are transformations, cant use compressed frame */
               l_videofile = NULL;
               break;
             }
           }
           else
           {
             if(gap_debug)
             {
               printf("gap_story_render_fetch_composite_image_or_chunk:  2 or more videotracks found\n");
             }
             l_videofile = NULL;
             break;
           }
         }
         else
         {
             l_videofile = NULL;
             break;
         }

         g_free(l_framename);
       }
       else
       {
             l_videofile = NULL;
             break;
       }
     }
     /* else: (vid track not used) continue  */

  }       /* end for loop over all video tracks */

  return(l_videofile);
}  /* end p_check_chunk_fetch_possible */



/* ----------------------------------------------------
 * p_check_basic_chunk_fetch_conditions
 * ----------------------------------------------------
 * check some basic conditions for raw frame chunk fetching.
 * return FALSE if fetch not possible.
 */
static gboolean
p_check_basic_chunk_fetch_conditions(gint32 check_flags
  , gint32 vid_width
  , gint32 vid_height
  , GapStoryRenderFrameRangeElem *frn_elem)
{
  if(GVA_has_video_chunk_proc(frn_elem->gvahand) != TRUE)
  {
    if(gap_debug)
    {
      printf("p_check_basic_chunk_fetch_conditions: Decoder does not support raw chunk fetching\n");
    }
    /* chunk fetch not possible because the used decoder implementation
     * does not provide the required support.
     */
    return (FALSE);
  }

  if((check_flags & GAP_VID_CHCHK_FLAG_SIZE) != 0)
  {
    if((frn_elem->gvahand->width != vid_width)
    || (frn_elem->gvahand->height != vid_height) )
    {
      if(gap_debug)
      {
        printf("p_check_basic_chunk_fetch_conditions: size (%d x %d) does not match required size (%d x %d)\n"
          , (int)frn_elem->gvahand->width
          , (int)frn_elem->gvahand->height
          , (int)vid_width
          , (int)vid_height
          );
      }
      return (FALSE);
    }
  }

  return (TRUE);
}  /* end p_check_basic_chunk_fetch_conditions */




/* ----------------------------------------------------
 * p_debug_dump_chunk_to_file
 * ----------------------------------------------------
 *
 */
 static void
 p_debug_dump_chunk_to_file(const unsigned char *video_frame_chunk_data
    , gint32 video_frame_chunk_size
    , gint32 video_frame_nr
    , gint32 master_frame_nr
    )
 {
   FILE *fp;
   char *fname;
   const char *l_env;
   gint32 l_dump_chunk_frames;

   l_dump_chunk_frames = 0;
   l_env = g_getenv("GAP_DUMP_FRAME_CHUNKS");
   if(l_env)
   {
     l_dump_chunk_frames = atol(l_env);
   }

   if (master_frame_nr > l_dump_chunk_frames)
   {
     return;
   }

   fname = g_strdup_printf("zz_chunk_data_%06d.dmp", (int)video_frame_nr);

   printf("DEBUG: SAVING fetched raw chunk to file:%s\n", fname);

   fp = g_fopen(fname, "wb");
   if(fp)
   {
      fwrite(video_frame_chunk_data, video_frame_chunk_size, 1, fp);
      fclose(fp);
   }

   g_free(fname);
 }  /* end p_debug_dump_chunk_to_file */



/* ----------------------------------------------------
 * p_story_attempt_fetch_chunk
 * ----------------------------------------------------
 * fetch the frame as uncompressed chunk into the buffer
 * that is provide by the caller 
 *   (via parameter video_frame_chunk_data at size video_frame_chunk_maxsize)
 * if not possible return NULL.
 * else return the name of the referenced videofile.
 */
static gchar*
p_story_attempt_fetch_chunk(GapStoryRenderVidHandle *vidhand
                    , gint32 master_frame_nr  /* starts at 1 */
                    , gint32  vid_width       /* desired Video Width in pixels */
                    , gint32  vid_height      /* desired Video Height in pixels */
                    , GapCodecNameElem *vcodec_list            /* IN: list of video_codec names that are compatible to the calling encoder program */
                    , unsigned char *video_frame_chunk_data    /* OUT: */
                    , gint32 *video_frame_chunk_size             /* OUT:  total size of frame (may include a videoformat specific frameheader)*/
                    , gint32 video_frame_chunk_maxsize           /* IN:  sizelimit (larger chunks are not fetched) */
                    , gdouble master_framerate
                    , gint32  max_master_frame_nr               /* the number of frames that will be encode in total */
                    , gint32 *video_frame_chunk_hdr_size       /* OUT: size of videoformat specific frameheader (0 if has no hdr) */
                    , gint32 check_flags                       /* IN: combination of GAP_VID_CHCHK_FLAG_* flag values */

                    , gboolean   *last_fetch_was_compressed_chunk
                    , const char *last_videofile

                 )
{
#define GAP_MPEG_ASSUMED_REFERENCE_DISTANCE 3
  static gint32     last_video_frame_nr = -1;

  gchar  *l_framename;
  gchar  *l_videofile;
  GapStoryRenderFrameRangeElem *l_frn_elem;
  GapStoryRenderFrameRangeElem *l_frn_elem_2;
  GapStoryRenderFrameType l_curr_frn_type;

  gint32        l_video_frame_nr;


  l_frn_elem        = NULL;
  *video_frame_chunk_size = 0;
  *video_frame_chunk_hdr_size = 0;  /* assume chunk contains no frame header */


  l_videofile = NULL;     /* NULL: also used as flag for "MUST fetch regular uncompressed frame" */
  l_framename = NULL;
  l_video_frame_nr = 1;


  l_curr_frn_type = GAP_FRN_SILENCE;
  
  l_videofile = p_check_chunk_fetch_possible(vidhand
                    , master_frame_nr
                    , vid_width
                    , vid_height
                    , &l_video_frame_nr
                    , &l_frn_elem
                    );
  
  if((l_videofile) && (l_frn_elem) )
  {
    l_curr_frn_type = l_frn_elem->frn_type;
    if (l_curr_frn_type != GAP_FRN_MOVIE)
    {
      gboolean l_singleFetchOk;
      if(gap_debug)
      {
        printf("p_story_attempt_fetch_chunk: MASTER_FRAME_NR: %d refers to imagefile :%s \n"
           , (int)master_frame_nr
           , l_videofile
           );
      }
      last_video_frame_nr = -1;
      l_singleFetchOk = p_chunk_fetch_from_single_image(l_videofile
                        , video_frame_chunk_data
                        , video_frame_chunk_maxsize
                        , video_frame_chunk_hdr_size
                        , video_frame_chunk_size
                        , vcodec_list
                        , check_flags
                        );
      if (l_singleFetchOk == TRUE)
      {
        /* passed all requested checks */
        return(l_videofile);
      }
      g_free(l_videofile);
      l_videofile = NULL;
      return (NULL);
    }
  }      


  if(l_curr_frn_type == GAP_FRN_MOVIE)
  {
    if(gap_debug)
    {
        printf("p_story_attempt_fetch_chunk: MASTER_FRAME_NR: %d refers to videofile :%s \n"
           , (int)master_frame_nr
           , l_videofile
           );
    }

    p_check_and_open_video_handle(l_frn_elem, vidhand, master_frame_nr, l_videofile);

    if(l_frn_elem->gvahand)
    {
      /* check if framesize matches 1:1 to output video size
       * and if the videodecoder does support a read procedure for compressed vodeo chunks
       * TODO: should also check for compatible vcodec_name
       *       (cannot check that, because libmpeg3 does not deliver vcodec_name information
       *        and there is no implementation to fetch uncompressed chunks in other decoders)
       */

      if (p_check_basic_chunk_fetch_conditions(check_flags, vid_width, vid_height, l_frn_elem) != TRUE)
      {
        if(gap_debug)
        {
          printf("p_story_attempt_fetch_chunk: MASTER_FRAME_NR: %d basic conditions NOT OK (no chunk fetch possible)\n"
             ,(int)master_frame_nr
             );
        }
      }
      else
      {
        t_GVA_RetCode  l_fcr;

        if(gap_debug)
        {
          printf("p_story_attempt_fetch_chunk: MASTER_FRAME_NR: %d  video_frame_nr:%d performing CHUNK fetch\n"
             ,(int)master_frame_nr
             ,(int)l_video_frame_nr
             );
        }

        /* FETCH compressed video chunk
         * (on successful fetch the chunk contains (at least) one frame, and may start with
         * MPEG typical sequence header and/or GOP header, Picture Header
         */
        l_fcr = GVA_get_video_chunk(l_frn_elem->gvahand
                                , l_video_frame_nr
                                , video_frame_chunk_data
                                , video_frame_chunk_size
                                , video_frame_chunk_maxsize
                                );

        if(gap_debug)
        {
          printf("p_story_attempt_fetch_chunk:  AFTER CHUNK fetch max:%d chunk_data:%d  chunk_size:%d\n"
                      ,(int)video_frame_chunk_maxsize
                      ,(int)video_frame_chunk_data
                      ,(int)*video_frame_chunk_size
                      );
        }

        if(l_fcr == GVA_RET_OK)
        {
          gint l_frame_type;
          gint32 check_flags_result;
          gint32 check_flags_mask;
          gboolean is_mpeg_integrity_check_done;
          char *vcodec_name_chunk;
        
          vcodec_name_chunk = GVA_get_codec_name(l_frn_elem->gvahand
                                                 ,GVA_VIDEO_CODEC
                                                 ,l_frn_elem->seltrack
                                                 );

          is_mpeg_integrity_check_done = FALSE;
          check_flags_result = check_flags & GAP_VID_CHCHK_FLAG_SIZE;
          
          p_check_flags_for_matching_vcodec(check_flags, &check_flags_result
                                          , l_videofile
                                          , vcodec_list
                                          , vcodec_name_chunk
                                          );

          if(vcodec_name_chunk)
          {
            g_free(vcodec_name_chunk);
            vcodec_name_chunk = NULL;
          }                                                 

          l_frame_type = GVA_util_check_mpg_frame_type(video_frame_chunk_data
                         ,*video_frame_chunk_size
                         );
          if(gap_debug)
          {
            printf("\nfetched CHUNK with l_frame_type %d(1=I,2=P,3=B)\n", (int)l_frame_type);
          }

          /* debug code: dump first video chunks to file(s) */
          p_debug_dump_chunk_to_file(video_frame_chunk_data
             , *video_frame_chunk_size
             , l_video_frame_nr
             , master_frame_nr
             );

          if (l_frame_type != GVA_MPGFRAME_UNKNOWN)
          {
            /* known MPEG specific framehaedr information is present
             * (typical when chunk was read via libmpeg3)
             * in this case try to fix timecode information in the header.
             */
            GVA_util_fix_mpg_timecode(video_frame_chunk_data
                           ,*video_frame_chunk_size
                           ,master_framerate
                           ,master_frame_nr
                           );
            *video_frame_chunk_hdr_size = GVA_util_calculate_mpeg_frameheader_size(video_frame_chunk_data
                       , *video_frame_chunk_size
                       );
          }

          check_flags_mask = check_flags & (GAP_VID_CHCHK_FLAG_MPEG_INTEGRITY | GAP_VID_CHCHK_FLAG_FULL_FRAME);
          if ((l_frame_type == GVA_MPGFRAME_I_TYPE)
          &&  (check_flags_mask))
          {
            is_mpeg_integrity_check_done = TRUE;
            /* intra frame has no dependencies to other frames
             * can use that frame type at any place in an MPEG stream
             * (or save it as JPEG)
             */
            *last_fetch_was_compressed_chunk = TRUE;

            if(gap_debug)
            {
              printf("\nReuse I-FRAME at %06d,", (int)master_frame_nr);
            }

            check_flags_result |= check_flags_mask;
          }

          check_flags_mask = check_flags & (GAP_VID_CHCHK_FLAG_JPG | GAP_VID_CHCHK_FLAG_FULL_FRAME | GAP_VID_CHCHK_FLAG_MPEG_INTEGRITY);
          if (check_flags_mask)
          {
             if(TRUE == GVA_util_check_jpg_picture( video_frame_chunk_data
                                                  , *video_frame_chunk_size
                                                  , 32 /* max_check_size  */
                                                  , video_frame_chunk_hdr_size))
             {
               check_flags_result |= check_flags_mask;
             }
          }

          check_flags_mask = check_flags & (GAP_VID_CHCHK_FLAG_PNG | GAP_VID_CHCHK_FLAG_FULL_FRAME);
          if (check_flags_mask)
          {
             if(TRUE == GVA_util_check_png_picture( video_frame_chunk_data
                                                  , *video_frame_chunk_size
                                                  , 32 /* max_check_size  */
                                                  , video_frame_chunk_hdr_size))
             {
               check_flags_result |= check_flags_mask;
             }
          }

          check_flags_mask = check_flags & GAP_VID_CHCHK_FLAG_MPEG_INTEGRITY;
          if ((l_frame_type == GVA_MPGFRAME_P_TYPE)
          &&  (check_flags_mask))
          {
            is_mpeg_integrity_check_done = TRUE;
            /* predicted frame has dependencies to the previous intra frame
             * can use that frame if fetch sequence contains previous i frame
             */
            if(last_videofile)
            {
              /* check if frame is the next in sequence in the same videofile */
              if((strcmp(l_videofile, last_videofile) == 0)
              && (l_video_frame_nr == last_video_frame_nr +1))
              {
                *last_fetch_was_compressed_chunk = TRUE;

                if(gap_debug)
                {
                  printf("P,");
                  // printf(" Reuse P-FRAME Chunk  at %06d\n", (int)master_frame_nr);
                }
                check_flags_result |= check_flags_mask;
              }
            }
          }

          check_flags_mask = check_flags & GAP_VID_CHCHK_FLAG_MPEG_INTEGRITY;
          if (((l_frame_type == GVA_MPGFRAME_B_TYPE)
          ||  (l_frame_type == GVA_MPGFRAME_P_TYPE))
          && (is_mpeg_integrity_check_done != TRUE)
          && (check_flags_mask))
          {
            is_mpeg_integrity_check_done = TRUE;
            /* bi-directional predicted frame has dependencies both to
             * the previous intra frame or p-frame and to the following i or p-frame.
             *
             * can use that frame if fetch sequence contains previous i frame
             * and fetch will continue until the next i or p frame.
             *
             * we do a simplified check if the next few (say 3) frames in storyboard sequence
             * will fetch the next (3) frames in videofile sequence from the same videofile.
             * this is just a guess, but maybe sufficient in most cases.
             */
            if(last_videofile)
            {
              gboolean l_bframe_ok;

              l_bframe_ok = FALSE;

              /* check if frame is the next in sequence in the same videofile */
              if((strcmp(l_videofile, last_videofile) == 0)
              && (l_video_frame_nr == last_video_frame_nr +1))
              {
                /* B-frame are not reused at the last few frames in the output video.
                 * (unresolved references to following p or i frames of the
                 *  input video could be the result)
                 */
                if(master_frame_nr + GAP_MPEG_ASSUMED_REFERENCE_DISTANCE <= max_master_frame_nr)
                {
                  gint ii;
                  gint32 l_next_video_frame_nr;
                  char  *l_next_videofile;

                  l_bframe_ok = TRUE;  /* now assume that B-frame may be used */
                  
                  /* look ahead if the next few fetches in storyboard sequence
                   * will deliver the next frames from the same inputvideo
                   * in ascending input_video sequence at stepsize 1
                   * (it is assumed that the referenced P or I frame
                   *  will be fetched in later calls then)
                   */
                  for(ii=1; ii <= GAP_MPEG_ASSUMED_REFERENCE_DISTANCE; ii++)
                  {
                    gboolean is_next_video_the_same;
                    
                    is_next_video_the_same = FALSE;
                    l_next_videofile = p_check_chunk_fetch_possible(vidhand
                                  , (master_frame_nr + ii)
                                  , vid_width
                                  , vid_height
                                  , &l_next_video_frame_nr
                                  , &l_frn_elem_2
                                  );
                    if(l_next_videofile)
                    {
                      if (strcmp(l_next_videofile, l_videofile) == 0)
                      {
                        is_next_video_the_same = TRUE;
                      }
                      g_free(l_next_videofile);
                    }
                    if((is_next_video_the_same == TRUE) && (l_frn_elem_2))
                    {
                      if((l_next_video_frame_nr != l_video_frame_nr +ii)
                      || (l_frn_elem_2->frn_type != GAP_FRN_MOVIE))
                      {
                        l_bframe_ok = FALSE;
                      }
                    }
                    else
                    {
                      l_bframe_ok = FALSE;
                    }
                    if(!l_bframe_ok)
                    {
                      break;
                    }

                  }  /* end for loop (look ahed next few frames in storyboard sequence) */
                }

                if(gap_debug)
                {
                  if(l_bframe_ok) printf("Look Ahead B-FRAME OK to copy\n");
                  else            printf("Look Ahead B-FRAME dont USE\n");
                }

                if(l_bframe_ok)
                {
                  *last_fetch_was_compressed_chunk = TRUE;

                  if(gap_debug)
                  {
                    if (l_frame_type == GVA_MPGFRAME_B_TYPE)
                    {
                      printf("B,");
                    }
                    else
                    {
                      printf("p,");
                    }
                    printf(" Reuse B-FRAME Chunk  at %06d\n", (int)master_frame_nr);
                  }
                  check_flags_result |= check_flags_mask;
                }
              }
            }
          }

          last_video_frame_nr = l_video_frame_nr;

          if(check_flags_result == check_flags)
          {
            if(gap_debug)
            {
              printf("oo OK, Reuse of fetched CHUNK type: %d (1=I/2=P/3=B)  masterFrameNr:%d frame_nr:%d (last_frame_nr:%d) \n"
                     "   check_flags_result:%d (requested check_flags:%d)\n"
                    ,(int)l_frame_type
                    ,(int)master_frame_nr
                    ,(int)l_video_frame_nr
                    ,(int)last_video_frame_nr
                    ,(int)check_flags_result
                    ,(int)check_flags
                    );
            }
            /* passed all requested checks */
            return(l_videofile);
          }

          if(gap_debug)
          {
            printf("** sorry, no reuse of fetched CHUNK type: %d (1=I/2=P/3=B) masterFrameNr:%d  frame_nr:%d (last_frame_nr:%d) \n"
                   "   check_flags_result:%d (requeste) check_flags:%d\n"
                  ,(int)l_frame_type
                  ,(int)master_frame_nr
                  ,(int)l_video_frame_nr
                  ,(int)last_video_frame_nr
                  ,(int)check_flags_result
                  ,(int)check_flags
                  );
          }

        }
        else
        {
          last_video_frame_nr = -1;
          if(gap_debug)
          {
            printf("**# sorry, no reuse fetch failed  frame_nr:%d (last_frame_nr:%d)\n"
                  ,(int)l_video_frame_nr
                  ,(int)last_video_frame_nr
                  );
          }
        }
      }
    }
  }

  *last_fetch_was_compressed_chunk = FALSE;
  *video_frame_chunk_size = 0;
  
  if (l_videofile != NULL)
  {
    g_free(l_videofile);
    l_videofile = NULL;
  }
  return (NULL);   /* Chunk fetch Not possible */

} /* end p_story_attempt_fetch_chunk */


#endif


/* ----------------------------------------------------
 * gap_story_render_fetch_composite_image_or_chunk         DEPRECATED
 * ----------------------------------------------------
 *
 * fetch composite VIDEO Image at a given master_frame_nr
 * within a storyboard framerange list
 *
 * if desired (and possible) try directly fetch the (already compressed) Frame chunk from
 * an input videofile for the master_frame_nr.
 *
 * This procedure is typically used in encoders that support lossless video cut.
 *
 * the compressed fetch depends on following conditions:
 * - dont_recode_flag == TRUE
 * - there is only 1 videoinput track at this master_frame_nr
 * - the videodecoder must support a read_video_chunk procedure
 *   (libmpeg3 has this support, for the libavformat the support is available vie the gap video api)
 *    TODO: for future releases should also check for the same vcodec_name)
 * - the videoframe must match 1:1 in size
 * - there are no transformations (opacity, offsets ....)
 * - there are no filtermacros to perform on the fetched frame
 *
 * check_flags:
 *   force checks if corresponding bit value is set. Supportet Bit values are:
 *      GAP_VID_CHCHK_FLAG_SIZE               check if width and height are equal
 *      GAP_VID_CHCHK_FLAG_MPEG_INTEGRITY     checks for MPEG P an B frames if the sequence of fetched frames
 *                                                   also includes the refered I frame (before or after the current
 *                                                   handled frame)
 *      GAP_VID_CHCHK_FLAG_JPG                check if fetched cunk is a jpeg encoded frame.
 *                                                  (typical for MPEG I frames)
 *      GAP_VID_CHCHK_FLAG_VCODEC_NAME        check for a compatible vcodec_name
 *
 *
 * RETURN TRUE on success, FALSE on ERRORS
 *    if an already compressed video_frame_chunk was fetched then return the size of the chunk
 *        in the *video_frame_chunk_size OUT Parameter.
 *        (both *image_id an *layer_id will deliver -1 in that case)
 *    if a composite image was fetched deliver its id in the *image_id OUT parameter
 *        and the id of the only layer in the *layer_id OUT Parameter
 *        the *force_keyframe OUT parameter tells the calling encoder to write an I-Frame
 *        (*video_frame_chunk_size will deliver 0 in that case)
 */
gboolean
gap_story_render_fetch_composite_image_or_chunk(GapStoryRenderVidHandle *vidhand
                    , gint32 master_frame_nr  /* starts at 1 */
                    , gint32  vid_width       /* desired Video Width in pixels */
                    , gint32  vid_height      /* desired Video Height in pixels */
                    , char *filtermacro_file  /* NULL if no filtermacro is used */
                    , gint32 *layer_id        /* output: Id of the only layer in the composite image */

                    , gint32 *image_id        /* output: Id of the only layer in the composite image */
                    , gboolean dont_recode_flag                /* IN: TRUE try to fetch comressed chunk if possible */
                    , GapCodecNameElem *vcodec_list            /* IN: list of video_codec names that are compatible to the calling encoder program */
                    , gboolean *force_keyframe                 /* OUT: the calling encoder should encode an I-Frame */
                    , unsigned char *video_frame_chunk_data    /* OUT: */
                    , gint32 *video_frame_chunk_size             /* OUT:  total size of frame (may include a videoformat specific frameheader)*/
                    , gint32 video_frame_chunk_maxsize           /* IN:  sizelimit (larger chunks are not fetched) */
                    , gdouble master_framerate
                    , gint32  max_master_frame_nr               /* the number of frames that will be encode in total */
                    , gint32 *video_frame_chunk_hdr_size       /* OUT: size of videoformat specific frameheader (0 if has no hdr) */
                    , gint32 check_flags                       /* IN: combination of GAP_VID_CHCHK_FLAG_* flag values */
                 )
{
#define GAP_MPEG_ASSUMED_REFERENCE_DISTANCE 3
  static char      *last_videofile = NULL;
  static gboolean   last_fetch_was_compressed_chunk = FALSE;

  gchar  *l_videofile;
  GapStoryRenderFrameRangeElem *l_frn_elem;

  gboolean      l_enable_chunk_fetch;


  *image_id         = -1;
  *layer_id         = -1;
  *force_keyframe   = FALSE;
  l_frn_elem        = NULL;
  *video_frame_chunk_size = 0;
  *video_frame_chunk_hdr_size = 0;  /* assume chunk contains no frame header */
  l_enable_chunk_fetch = dont_recode_flag;

  if(gap_debug)
  {
    printf("gap_story_render_fetch_composite_image_or_chunk START  master_frame_nr:%d  %dx%d dont_recode:%d\n"
                       , (int)master_frame_nr
                       , (int)vid_width
                       , (int)vid_height
                       , (int)dont_recode_flag
                       );
  }

  l_videofile = NULL;     /* NULL: also used as flag for "MUST fetch regular uncompressed frame" */


#ifdef GAP_ENABLE_VIDEOAPI_SUPPORT

  if(p_isFiltermacroActive(filtermacro_file))
  {
    if(gap_debug)
    {
      printf("chunk fetch disabled due to filtermacro procesing\n");
    }
    /* if a filtermacro_file is force disable chunk fetching */
    l_enable_chunk_fetch = FALSE;  
  }

  if (l_enable_chunk_fetch)
  {
     if(gap_debug)
     {
        printf("start check if chunk fetch is possible\n");
     }

     l_videofile = p_story_attempt_fetch_chunk(vidhand
                         , master_frame_nr
                         , vid_width
                         , vid_height
                         , vcodec_list
                         , video_frame_chunk_data
                         , video_frame_chunk_size
                         , video_frame_chunk_maxsize
                         , master_framerate
                         , max_master_frame_nr
                         , video_frame_chunk_hdr_size
                         , check_flags
     
                         , &last_fetch_was_compressed_chunk
                         , last_videofile
                      );
  }

  if(last_fetch_was_compressed_chunk)
  {
    *force_keyframe = TRUE;
  }

  /* keep the videofile name for the next call
   * (for MPEG INTEGRITY checks that require continous sequence 
   *  in the same referenced source video
   */
  if(last_videofile)
  {
      g_free(last_videofile);
  }
  last_videofile = l_videofile;

#endif

  if(l_videofile != NULL)
  {
     /* chunk fetch was successful */
     if(gap_debug)
     {
        printf("gap_story_render_fetch_composite_image_or_chunk:  CHUNK fetch succsessful\n");
     }
     return(TRUE);
  }
  else
  {
    last_fetch_was_compressed_chunk = FALSE;
    if(last_videofile)
    {
      g_free(last_videofile);
    }
    last_videofile = l_videofile;


    if(gap_debug)
    {
       printf("gap_story_render_fetch_composite_image_or_chunk:  CHUNK fetch not possible (doing frame fetch instead)\n");
    }

    *video_frame_chunk_size = 0;
    *image_id = gap_story_render_fetch_composite_image(vidhand
                    , master_frame_nr  /* starts at 1 */
                    , vid_width       /* desired Video Width in pixels */
                    , vid_height      /* desired Video Height in pixels */
                    , filtermacro_file  /* NULL if no filtermacro is used */
                    , layer_id        /* output: Id of the only layer in the composite image */
                    );
    if(*image_id >= 0)
    {
      return(TRUE);
    }
  }

  return(FALSE);

} /* end gap_story_render_fetch_composite_image_or_chunk DEPRECATED */


/* ------------------------------------------------------------------------
 * gap_story_render_fetch_composite_image_or_buffer_or_chunk (extended API)
 * ------------------------------------------------------------------------
 *
 * fetch composite VIDEO frame at a given master_frame_nr
 * within a storyboard framerange list.
 *
 * on success the result can be delivered in one of those types:
 *   GAP_STORY_FETCH_RESULT_IS_IMAGE
 *   GAP_STORY_FETCH_RESULT_IS_RAW_RGB888
 *   GAP_STORY_FETCH_RESULT_IS_COMPRESSED_CHUNK
 *
 * The delivered data type depends on the flags:
 *   dont_recode_flag
 *   enable_rgb888_flag
 *
 * In case all of those flags are FALSE, the caller can always expect
 * a gimp image (GAP_STORY_FETCH_RESULT_IS_IMAGE) as result on success.
 *
 * Encoders that can handle RGB888 colormdel can set the enable_rgb888_flag
 *
 *   If the enable_rgb888_flag is TRUE and the refered frame can be copied
 *   without render transitions from only one input video clip
 *   then the render engine is bypassed, and the result will be of type 
 *   GAP_STORY_FETCH_RESULT_IS_RAW_RGB888 for this frame.
 *   (this speeds up encoding of simple 1:1 copied video clip frames
 *   because the converting from rgb88 to gimp drawable and back to rgb88
 *   can be skipped in this special case)
 *   
 *
 * Encoders that support lossless video cut can set the dont_recode_flag.
 *
 *   if the dont_recode_flag is TRUE, the render engine is also bypassed where
 *   a direct fetch of the (already compressed) Frame chunk from an input videofile
 *   is possible for the master_frame_nr.
 *   (in case there are any transitions or mix with other input channels
 *   or in case the input is not an mpeg encoded video file it is not possible to 
 *   make a lossless copy of the input frame data)
 *
 *   Restriction: current implementation provided lossless cut only for MPEG1 and MPEG2
 *
 *
 * the compressed fetch depends on following conditions:
 * - dont_recode_flag == TRUE
 * - there is only 1 videoinput track at this master_frame_nr
 * - the videodecoder must support a read_video_chunk procedure
 *   (libmpeg3 has this support, for the libavformat the support is available vie the gap video api)
 *    TODO: for future releases should also check for the same vcodec_name)
 * - the videoframe must match 1:1 in size
 * - there are no transformations (opacity, offsets ....)
 * - there are no filtermacros to perform on the fetched frame
 *
 * check_flags:
 *   force checks if corresponding bit value is set. Supportet Bit values are:
 *      GAP_VID_CHCHK_FLAG_SIZE               check if width and height are equal
 *      GAP_VID_CHCHK_FLAG_MPEG_INTEGRITY     checks for MPEG P an B frames if the sequence of fetched frames
 *                                                   also includes the refered I frame (before or after the current
 *                                                   handled frame)
 *      GAP_VID_CHCHK_FLAG_JPG                check if fetched cunk is a jpeg encoded frame.
 *                                                  (typical for MPEG I frames)
 *      GAP_VID_CHCHK_FLAG_VCODEC_NAME        check for a compatible vcodec_name
 *
 *
 * The resulting frame is deliverd into the GapStoryFetchResult struct.
 *
 *   Note that the caller of the fetch procedure can already provide
 *   allocated memory for the buffers  raw_rgb_data and video_frame_chunk_data.
 *   (in this case the caler is responsible to allocate the buffers large enough
 *   to hold one uncompressed frame in rgb888 colormodel representation)
 *
 *   in case raw_rgb_data or video_frame_chunk_data is NULL the buffer is automatically
 *   allocated in correct size when needed.
 */
void
gap_story_render_fetch_composite_image_or_buffer_or_chunk(GapStoryRenderVidHandle *vidhand
                    , gint32 master_frame_nr  /* starts at 1 */
                    , gint32  vid_width       /* desired Video Width in pixels */
                    , gint32  vid_height      /* desired Video Height in pixels */
                    , char *filtermacro_file  /* NULL if no filtermacro is used */
                    , gboolean dont_recode_flag                /* IN: TRUE try to fetch comressed chunk if possible */
                    , gboolean enable_rgb888_flag              /* IN: TRUE deliver result already converted to rgb buffer */
                    , GapCodecNameElem *vcodec_list            /* IN: list of video_codec names that are compatible to the calling encoder program */
                    , gint32 video_frame_chunk_maxsize         /* IN: sizelimit (larger chunks are not fetched) */
                    , gdouble master_framerate
                    , gint32  max_master_frame_nr              /* the number of frames that will be encoded in total */
                    , gint32  check_flags                      /* IN: combination of GAP_VID_CHCHK_FLAG_* flag values */
                    , GapStoryFetchResult *gapStoryFetchResult
                 )
{
#define GAP_MPEG_ASSUMED_REFERENCE_DISTANCE 3
  static char      *last_videofile = NULL;
  static gboolean   last_fetch_was_compressed_chunk = FALSE;

  gchar  *l_videofile;
  GapStoryRenderFrameRangeElem *l_frn_elem;

  gboolean      l_enable_chunk_fetch;

  /* init result record */
  gapStoryFetchResult->resultEnum = GAP_STORY_FETCH_RESULT_IS_ERROR;
  gapStoryFetchResult->image_id                   = -1;
  gapStoryFetchResult->layer_id                   = -1;
  gapStoryFetchResult->force_keyframe             = FALSE;
  gapStoryFetchResult->video_frame_chunk_size     = 0;
  gapStoryFetchResult->video_frame_chunk_hdr_size = 0;     /* assume chunk contains no frame header */
  
  l_frn_elem        = NULL;
  l_enable_chunk_fetch = dont_recode_flag;
  
  if ((gapStoryFetchResult->video_frame_chunk_data == NULL)
  && (l_enable_chunk_fetch == TRUE))
  {
    gapStoryFetchResult->video_frame_chunk_data = g_malloc(vid_width * vid_height * 4);
  }

  if(gap_debug)
  {
    printf("gap_story_render_fetch_composite_image_or_buffer_or_chunk START  master_frame_nr:%d  %dx%d dont_recode:%d\n"
                       , (int)master_frame_nr
                       , (int)vid_width
                       , (int)vid_height
                       , (int)dont_recode_flag
                       );
  }

  l_videofile = NULL;     /* NULL: also used as flag for "MUST fetch regular uncompressed frame" */


#ifdef GAP_ENABLE_VIDEOAPI_SUPPORT

  if(filtermacro_file)
  {
     if(*filtermacro_file != '\0')
     {
       if(gap_debug)
       {
         printf("chunk fetch disabled due to filtermacro procesing\n");
       }
       /* if a filtermacro_file is force disable chunk fetching */
       l_enable_chunk_fetch = FALSE;  
     }
  }

  if (l_enable_chunk_fetch)
  {
     if(gap_debug)
     {
        printf("start check if chunk fetch is possible\n");
     }

     l_videofile = p_story_attempt_fetch_chunk(vidhand
                         , master_frame_nr
                         , vid_width
                         , vid_height
                         , vcodec_list
                         , &gapStoryFetchResult->video_frame_chunk_data
                         , &gapStoryFetchResult->video_frame_chunk_size
                         , video_frame_chunk_maxsize
                         , master_framerate
                         , max_master_frame_nr
                         , &gapStoryFetchResult->video_frame_chunk_hdr_size
                         , check_flags
     
                         , &last_fetch_was_compressed_chunk
                         , last_videofile
                      );
  }

  if(last_fetch_was_compressed_chunk)
  {
    gapStoryFetchResult->force_keyframe = TRUE;
  }

  /* keep the videofile name for the next call
   * (for MPEG INTEGRITY checks that require continous sequence 
   *  in the same referenced source video
   */
  if(last_videofile)
  {
      g_free(last_videofile);
  }
  last_videofile = l_videofile;

#endif

  if(l_videofile != NULL)
  {
     /* chunk fetch was successful */
     if(gap_debug)
     {
        printf("gap_story_render_fetch_composite_image_or_buffer_or_chunk:  CHUNK fetch succsessful\n");
     }
     gapStoryFetchResult->resultEnum = GAP_STORY_FETCH_RESULT_IS_COMPRESSED_CHUNK;
     return;
  }
  else
  {
    last_fetch_was_compressed_chunk = FALSE;
    if(last_videofile)
    {
      g_free(last_videofile);
    }
    last_videofile = l_videofile;



    gapStoryFetchResult->video_frame_chunk_size = 0;
 
    if(gap_debug)
    {
      printf("gap_story_render_fetch_composite_image_or_buffer_or_chunk:  "
             "CHUNK fetch not possible (doing frame or rgb888 fetch instead) enable_rgb888_flag:%d\n"
             ,(int)enable_rgb888_flag
             );
    }
      
    p_story_render_fetch_composite_image_or_buffer(vidhand
                                                   ,master_frame_nr
                                                   ,vid_width
                                                   ,vid_height
                                                   ,filtermacro_file
                                                   ,&gapStoryFetchResult->layer_id
                                                   ,enable_rgb888_flag
                                                   ,gapStoryFetchResult  /* IN/OUT: fetched data is stored in this struct */
                                                   );
  }


} /* end gap_story_render_fetch_composite_image_or_buffer_or_chunk */

