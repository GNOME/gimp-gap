/* vid_api_util.c
 *
 * GAP Video read API implementation of utility procedures.
 *
 * 2004.03.14   hof created
 *
 */

#include <glib/gstdio.h>


/* XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX END fcache procedures */


/* CCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCC START copies gap_lib procedures */

/* --------------------
 * p_get_filesize
 * --------------------
 *   get the filesize of a file.
 * in: fname: The filename of the file to check.
 * returns: The filesize.
 */
static gint32
p_get_filesize(char *fname)
{
  GStatBuf  stat_buf;

  if (0 != g_stat(fname, &stat_buf))
  {
    printf ("stat error on '%s'\n", fname);
    return(0);
  }

  return((gint32)(stat_buf.st_size));
}  /* end p_get_filesize */


/* ----------------------------------
 * p_alloc_fname
 * ----------------------------------
 * build the framname by concatenating basename, nr and extension.
 * the Number part has leading zeroes, depending
 * on filenames with the same basename and extension on disc.
 *
 * if no similar discfiles were found 6 digits (with leading zeroes)
 * are used per default.
 *
 * if a similar discfilename is found, the number of digits/leading zeroes
 * is set equal to the discfile found.
 * example:
 *   basename == "frame_", nr == 5, ext == ".xcf"
 *   - discfile was found with name:  "frame_00001.xcf"
 *     return ("frame_00001.xcf");
 *
 *   - discfile was found with name:  "frame_001.xcf"
 *     return ("frame_001.xcf");
 *
 * return the resulting framename string
 *   (the calling program should g_free this string
 *    after use)
 */
static char*
p_alloc_fname(char *basename, long nr, char *extension)
{
  gchar *l_fname;
  gint   l_digits_used;
  gint   l_len;
  long   l_nr_chk;

  if(basename == NULL) return (NULL);
  l_len = (strlen(basename)  + strlen(extension) + 10);
  l_fname = (char *)g_malloc(l_len);

    l_digits_used = 6;
    if(nr < 10000000)
    {
       /* try to figure out if the frame numbers are in
        * 4-digit style, with leading zeroes  "frame_0001.xcf"
        * or not                              "frame_1.xcf"
        */
       l_nr_chk = nr;

       while(l_nr_chk >= 0)
       {
         /* check if frame is on disk with 6-digit style framenumber */
         g_snprintf(l_fname, l_len, "%s%06ld%s", basename, l_nr_chk, extension);
         if (g_file_test(l_fname, G_FILE_TEST_EXISTS))
         {
            l_digits_used = 6;
            break;
         }

         /* check if frame is on disk with 8-digit style framenumber */
         g_snprintf(l_fname, l_len, "%s%08ld%s", basename, l_nr_chk, extension);
         if (g_file_test(l_fname, G_FILE_TEST_EXISTS))
         {
            l_digits_used = 8;
            break;
         }

         /* check if frame is on disk with 7-digit style framenumber */
         g_snprintf(l_fname, l_len, "%s%07ld%s", basename, l_nr_chk, extension);
         if (g_file_test(l_fname, G_FILE_TEST_EXISTS))
         {
            l_digits_used = 7;
            break;
         }

         /* check if frame is on disk with 5-digit style framenumber */
         g_snprintf(l_fname, l_len, "%s%05ld%s", basename, l_nr_chk, extension);
         if (g_file_test(l_fname, G_FILE_TEST_EXISTS))
         {
            l_digits_used = 5;
            break;
         }

         /* check if frame is on disk with 4-digit style framenumber */
         g_snprintf(l_fname, l_len, "%s%04ld%s", basename, l_nr_chk, extension);
         if (g_file_test(l_fname, G_FILE_TEST_EXISTS))
         {
            l_digits_used = 4;
            break;
         }

         /* check if frame is on disk with 3-digit style framenumber */
         g_snprintf(l_fname, l_len, "%s%03ld%s", basename, l_nr_chk, extension);
         if (g_file_test(l_fname, G_FILE_TEST_EXISTS))
         {
            l_digits_used = 3;
            break;
         }

         /* check if frame is on disk with 2-digit style framenumber */
         g_snprintf(l_fname, l_len, "%s%02ld%s", basename, l_nr_chk, extension);
         if (g_file_test(l_fname, G_FILE_TEST_EXISTS))
         {
            l_digits_used = 2;
            break;
         }



         /* now check for filename without leading zeroes in the framenumber */
         g_snprintf(l_fname, l_len, "%s%ld%s", basename, l_nr_chk, extension);
         if (g_file_test(l_fname, G_FILE_TEST_EXISTS))
         {
            l_digits_used = 1;
            break;
         }
         l_nr_chk--;

         /* if the frames nr and nr-1  were not found
          * try to check frames 1 and 0
          * to limit down the loop to max 4 cycles
          */
         if((l_nr_chk == nr -2) && (l_nr_chk > 1))
         {
           l_nr_chk = 1;
         }
      }
    }
    else
    {
      /* numbers > 10000000 have 9 digits or more */
      l_digits_used = 0;
    }

  g_free(l_fname);

  switch(l_digits_used)
  {
    case 6:  l_fname = g_strdup_printf("%s%06ld%s", basename, nr, extension);
             break;
    case 8:  l_fname = g_strdup_printf("%s%08ld%s", basename, nr, extension);
             break;
    case 7:  l_fname = g_strdup_printf("%s%07ld%s", basename, nr, extension);
             break;
    case 5:  l_fname = g_strdup_printf("%s%05ld%s", basename, nr, extension);
             break;
    case 4:  l_fname = g_strdup_printf("%s%04ld%s", basename, nr, extension);
             break;
    case 3:  l_fname = g_strdup_printf("%s%03ld%s", basename, nr, extension);
             break;
    case 2:  l_fname = g_strdup_printf("%s%02ld%s", basename, nr, extension);
             break;
    default: l_fname = g_strdup_printf("%s%ld%s", basename, nr, extension);
             break;
  }
  return(l_fname);
}    /* end p_alloc_fname */


/* ----------------------------------------------------
 * DELETE image
 * ----------------------------------------------------
 * this procedure deletes a frame image
 * (only from the GIMP allocated Memory)
 *
 * further it makes a Workaround for a memory leak Problem in gimp-1.2.2
 */
static void
p_gimp_image_delete(gint32 image_id)
{
    gimp_image_undo_disable(image_id); /* clear undo stack */
    gimp_image_scale(image_id, 2, 2);
    if(gap_debug) printf("SCALED down to 2x2 id = %d (workaround for gimp_image-delete problem)\n", (int)image_id);

    gimp_image_undo_enable(image_id); /* clear undo stack */
    gimp_image_delete(image_id);
}

/* CCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCC END copies of gap_lib procedures */



/* --------------------------------
 * GVA_md5_string
 * --------------------------------
 * name must point to a buffer of at least 32 Bytes
 * tht will be filled with the MD5 hashstring.
 */
void
GVA_md5_string(char *name, const char *uri)
{
  if(uri)
  {
    guchar       digest[16];
    guchar       n;
    GChecksum *checksum;
    gsize      len;
    gsize      i;

    len = sizeof (digest);
    checksum = g_checksum_new (G_CHECKSUM_MD5);
    g_checksum_update (checksum, (const guchar *) uri, -1);
    g_checksum_get_digest (checksum, digest, &len);
    g_checksum_free (checksum);

    for (i = 0; i < 16; i++)
    {
        n = (digest[i] >> 4) & 0xF;
        name[i * 2]     = (n > 9) ? 'a' + n - 10 : '0' + n;

        n = digest[i] & 0xF;
        name[i * 2 + 1] = (n > 9) ? 'a' + n - 10 : '0' + n;
    }

    name[32] = '\0';
  }  
}  /* end GVA_md5_string */


/* --------------------------------
 * GVA_filename_to_uri
 * --------------------------------
 */
gchar*
GVA_filename_to_uri(const char *filename)
{
  gchar *uri;
  gchar *absolute;

  if (! g_path_is_absolute (filename))
  {
      gchar *current;

      current = g_get_current_dir ();
      absolute = g_build_filename (current, filename, NULL);
      g_free (current);
  }
  else
  {
      absolute = g_strdup (filename);
  }

  uri = g_filename_to_uri (absolute, NULL, NULL);

  g_free (absolute);

  return uri;
}  /* end GVA_filename_to_uri */


/* --------------------------------
 * GVA_util_check_mpg_frame_type
 * --------------------------------
 * this procedure is typically used after the caller
 * has fetched uncompressed MPEG frames (using GVA_get_video_chunk)
 * to find out the type of the fetched frame.
 * in:     buffer that should contain (at least) one compressed MPEG frame
 *         that may be prefixed by Sequence Header or GOP header data
 * return: int frametype of the 1.st frame found in the buffer.
 *         where 1 == intra, 2 == predicted, 3 == bidrectional predicted
 *         -1 is returned if no frame was found.
 */
gint
GVA_util_check_mpg_frame_type(unsigned char *buffer, gint32 buf_size)
{
  unsigned long code;
  gint     l_idx;
  gint     l_frame_type;
  unsigned l_picture_number;
  gint32   l_max_check_size;
  
  l_max_check_size = buf_size;
  
  l_frame_type = GVA_MPGFRAME_UNKNOWN;
  l_picture_number = 0;
  code = 0;
  l_idx = 0;
  while(l_idx < l_max_check_size)
  {
    code <<= 8;
    code |= buffer[l_idx++];
    
    if(code == GVA_MPGHDR_PICTURE_START_CODE)
    {
      /* found a picture start code
       * get next 10 bits for picture_number
       */
      l_picture_number =(unsigned long)buffer[l_idx] << 2;
      l_picture_number |= (unsigned long)((buffer[l_idx +1] >> 6) & 0x3);

      /* get next 3 bits for frame_type information */
      l_frame_type = ((buffer[l_idx +1] >> 3) & 0x7);
      break;
    }
  }
  

  if(gap_debug)
  {
    printf("GVA_util_check_mpg_frame_type: Frame type:%d  local_picnum:%d l_idx:%d\n"
          ,(int)l_frame_type
          ,(int)l_picture_number
          ,(int)l_idx
          );
  }
  
  return(l_frame_type);
}  /* end GVA_util_check_mpg_frame_type */


/* -------------------------------
 * GVA_util_fix_mpg_timecode
 * -------------------------------
 * In: buffer (in length buf_size)
 *     the buffer should contain one compressed MPEG
 *     frame, optionally prefixed by Sequence Header, GOP Header ...
 * If the buffer contains a GOP header,
 * the timecode in the GOP Header is replaced with a new timecode,
 * matching master_frame_nr at playbackrate of master_framerate (fps)
 *
 * the timecode is not replaced if the old code is all zero (00:00:00:00)
 */
void
GVA_util_fix_mpg_timecode(unsigned char *buffer
                         ,gint32 buf_size
                         ,gdouble master_framerate
                         ,gint32  master_frame_nr
                         )
{
  unsigned long code;
  gint          l_idx;

  
  code = 0;
  l_idx = 0;
  while(l_idx < buf_size)
  {
    code <<= 8;
    code |= buffer[l_idx++];
    
    if(code == GVA_MPGHDR_GOP_START_CODE)
    {
      int hour, minute, second, frame;
      float carry;

      /* Get the time old time code (including the drop_frame flag in the 1.st byte) */
      code = (unsigned long)buffer[l_idx] << 24;
      code |= (unsigned long)buffer[l_idx + 1] << 16;
      code |= (unsigned long)buffer[l_idx + 2] << 8;
      code |= (unsigned long)buffer[l_idx + 3];

      hour = code >> 26 & 0x1f;
      minute = code >> 20 & 0x3f;
      second = code >> 13 & 0x3f;
      frame = code >> 7 & 0x3f;

      if(gap_debug)
      {
        printf("Timecode old: %02d:%02d:%02d:%02d ", hour, minute, second, frame);
      }

      if((hour == 0)
      && (minute == 0)
      && (second == 0)
      && (frame == 0))
      {
        if(gap_debug)
        {
          printf("\n");
        }
      }
      else
      {
        /* Write a new time code */
        hour = (long)((float)(master_frame_nr - 1) / master_framerate / 3600);
        carry = hour * 3600 * master_framerate;
        minute = (long)((float)(master_frame_nr - 1 - carry) / master_framerate / 60);
        carry += minute * 60 * master_framerate;
        second = (long)((float)(master_frame_nr - 1 - carry) / master_framerate);
        carry += second * master_framerate;
        frame = (master_frame_nr - 1 - carry);

        buffer[l_idx] = ((code >> 24) & 0x80) | (hour << 2) | (minute >> 4);
        buffer[l_idx + 1] = ((code >> 16) & 0x08) | ((minute & 0xf) << 4) | (second >> 3);
        buffer[l_idx + 2] = ((second & 0x7) << 5) | (frame >> 1);
        buffer[l_idx + 3] = (code & 0x7f) | ((frame & 0x1) << 7);

        if(gap_debug)
        {
          printf("new: %02d:%02d:%02d:%02d\n", hour, minute, second, frame);
        }
      }

      break;
    }
  }
}  /* end GVA_util_fix_mpg_timecode */


/* ----------------------------------------
 * GVA_util_calculate_mpeg_frameheader_size
 * ----------------------------------------
 * scan the buffer for the 1st Mpeg picture start code.
 * all information from start of the buffer inclusive the picuture header
 * are considered as MPG header information.
 * (TODO: )
 *
 * returns the size of frame/gop header or 0 if no header is present.
 */
gint32
GVA_util_calculate_mpeg_frameheader_size(unsigned char *buffer
                         ,gint32 buf_size
                         )
{
  unsigned long code;
  gint     l_idx;
  gint     l_frame_type;
  unsigned l_picture_number;
  gint32   l_max_check_size;
  gint32   l_hdr_size;
  
  l_max_check_size = buf_size;
  
  l_frame_type = GVA_MPGFRAME_UNKNOWN;
  l_picture_number = 0;
  l_hdr_size = 0;
  code = 0;
  l_idx = 0;
  while(l_idx < l_max_check_size)
  {
    code <<= 8;
    code |= buffer[l_idx++];
    
    if(code == GVA_MPGHDR_PICTURE_START_CODE)
    {
      /* found a picture start code
       * get next 10 bits for picture_number
       */
      l_picture_number =(unsigned long)buffer[l_idx] << 2;
      l_picture_number |= (unsigned long)((buffer[l_idx +1] >> 6) & 0x3);

      /* get next 3 bits for frame_type information */
      l_frame_type = ((buffer[l_idx +1] >> 3) & 0x7);
      
      l_hdr_size = l_idx + 2; // TODO dont know if there are more bytes in the picture header ???
      break;
    }
  }
  

  if(gap_debug)
  {
    printf("GVA_util_calculate_mpeg_frameheader_size: %d  l_picture_number:%d frame_type:%d (1=I,2=P,3=B)\n"
          ,(int)l_hdr_size
          ,(int)l_picture_number
          ,(int)l_frame_type
          );
  }
  
  return(l_hdr_size);
}  /* end GVA_util_calculate_mpeg_frameheader_size */


/* ----------------------------------------
 * GVA_util_check_jpg_picture
 * ----------------------------------------
 * scan the buffer for the 1st JPEG picture.
 * This implementation checks for the jpeg typical "magic number"
 *    ff d8 ff
 * TODO: if libjpeg is available (#ifdef HAVE_??LIBJPG)
 *       we colud try to a more sophisticated check
 *       via jpeg_read_header from memory...
 *       
 * returns TRUE if the specified buffer contains a JPEG image.
 */
gboolean
GVA_util_check_jpg_picture(unsigned char *buffer
                         ,gint32 buf_size
                         ,gint32 max_check_size
                         ,gint32 *hdr_size
                         )
{
  gint     l_idx;
  gint32   l_max_check_size;
  gboolean l_jpeg_magic_number_found;
  
  l_max_check_size = MAX(max_check_size, 1);
  l_jpeg_magic_number_found = FALSE;
  l_idx = 0;
  while(l_idx < l_max_check_size)
  {
    /* check magic number */
    if ((buffer[l_idx]    == 0xff) 
    &&  (buffer[l_idx +1] == 0xd8)
    &&  (buffer[l_idx +2] == 0xff))
    {
      *hdr_size = l_idx;
      l_jpeg_magic_number_found = TRUE;
      break;
    }
    l_idx++;
  }
  

  if(gap_debug)
  {
    printf("GVA_util_check_jpg_magic_number: l_jpeg_magic_number_found:%d at idx:%d hdr_size:%d\n"
          ,(int)l_jpeg_magic_number_found
          ,(int)l_idx
          ,(int)*hdr_size
          );
  }
  
  return(l_jpeg_magic_number_found);
}  /* end GVA_util_check_jpg_picture */


/* ----------------------------------------
 * GVA_util_check_png_picture
 * ----------------------------------------
 * scan the buffer for the 1st PNG picture.
 * This implementation checks for the PNG typical "magic number"
 *   89 50 4e 47  (.PNG)
 *       
 * returns TRUE if the specified buffer contains a PNG image.
 */
gboolean
GVA_util_check_png_picture(unsigned char *buffer
                         ,gint32 buf_size
                         ,gint32 max_check_size
                         ,gint32 *hdr_size
                         )
{
  gint     l_idx;
  gint32   l_max_check_size;
  gboolean l_png_magic_number_found;
  
  l_max_check_size = MAX(max_check_size, 1);
  l_png_magic_number_found = FALSE;
  l_idx = 0;
  while(l_idx < l_max_check_size)
  {
    /* check magic number */
    if ((buffer[l_idx]    == 0x89) 
    &&  (buffer[l_idx +1] == 0x50)    // 'P'
    &&  (buffer[l_idx +2] == 0x4e)    // 'N'
    &&  (buffer[l_idx +3] == 0x47))   // 'G'
    {
      *hdr_size = l_idx;
      l_png_magic_number_found = TRUE;
      break;
    }
    l_idx++;
  }
  

  if(gap_debug)
  {
    printf("GVA_util_check_png_picture: l_png_magic_number_found:%d at idx:%d hdr_size:%d\n"
          ,(int)l_png_magic_number_found
          ,(int)l_idx
          ,(int)*hdr_size
          );
  }
  
  return(l_png_magic_number_found);
}  /* end GVA_util_check_png_picture */
